// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Ngs2Device.h"
#include "Ngs2Effects.h"
#include "Ngs2Support.h"
#include "ActiveSound.h"
#include "ContentStreaming.h"

uint32 FNgs2SoundSource::NextVoiceId = 0;

static int32 MuteStreamingSounds = 0;
FAutoConsoleVariableRef CVarMuteStreamingSounds(
	TEXT("au.MuteStreamingSounds"),
	MuteStreamingSounds,
	TEXT("Mutes streaming sounds.\n")
	TEXT("0: Not Muted, 1: Muted"),
	ECVF_Default);

static int32 MuteNonStreamingSounds = 0;
FAutoConsoleVariableRef CVarMuteNonStreamingSounds(
	TEXT("au.MuteNonStreamingSounds"),
	MuteNonStreamingSounds,
	TEXT("Mutes non-streaming sounds.\n")
	TEXT("0: Not Muted, 1: Muted"),
	ECVF_Default);


/**
* Function used to get waveform block callbacks from playing Ngs2 voices, used for feeding more audio
* to playing ngs2 sound sources.
*/
static void FNgs2VoiceCallbackHandler(const SceNgs2VoiceCallbackInfo* CallbackInfo)
{
	// Get our sound buffer
	if (CallbackInfo && CallbackInfo->callbackData)
	{
		FNgs2SoundSource* SoundSource = (FNgs2SoundSource*)CallbackInfo->callbackData;
		SoundSource->OnBufferEnd((uint32)CallbackInfo->param.waveformBlock.userData);
	}
}

FNgs2SoundSource::FNgs2SoundSource(FAudioDevice* InAudioDevice, int32 InVoiceIndex)
	: FSoundSource(InAudioDevice)	
{
	AudioDevice = (FNgs2Device*)InAudioDevice;
	check(AudioDevice);
	Effects = (FNgs2EffectsManager*)AudioDevice->GetEffects();
	check(Effects);
	Voice = SCE_NGS2_HANDLE_INVALID;
	VoiceIndex = InVoiceIndex;
	bIsA3dSound = false;
	bVoiceSetup = false;
	RealtimeAsyncTask = nullptr;
	Ngs2Buffer = nullptr;
	bBuffersToFlush = false;
	bIsFinished = false;
	bLooped = false;
	bLoopCallback = false;
	bPlayedCachedBuffer = false;
	bResourcesNeedFreeing = false;
	CurrentBuffer = 0;
	VoiceId = 0;
}

FNgs2SoundSource::~FNgs2SoundSource()
{
	FreeResources();
}

void FNgs2SoundSource::FreeResources()
{
	Voice = SCE_NGS2_HANDLE_INVALID;
	VoiceId = 0;

	if (RealtimeAsyncTask)
	{
		check(Ngs2Buffer);
		check(bResourcesNeedFreeing);

		FPendingDecodeTaskCleanup TaskCleanup;
		TaskCleanup.RealtimeAsyncTask = RealtimeAsyncTask;
		TaskCleanup.Buffer = Ngs2Buffer;

		AudioDevice->AddPendingCleanupTask(TaskCleanup);
	}
	else if (bResourcesNeedFreeing)
	{
		delete Ngs2Buffer;
	}

	RealtimeAsyncTask = nullptr;
	Buffer = Ngs2Buffer = nullptr;
	CurrentBuffer = 0;
	bResourcesNeedFreeing = false;
}

void FNgs2SoundSource::PatchVoice(FNgs2Pathway* Pathway)
{
	ENgs2Pathway::Type PathwayName = Pathway->PathwayName;	

	// route this voice to the output through EQ if desired, otherwise directly to mastering rack
	if (PathwayName == ENgs2Pathway::TV && IsEQFilterApplied() && AudioDevice->EQVoice != SCE_NGS2_HANDLE_INVALID)
	{
		AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceSetPortVolume"),
			sceNgs2VoiceSetPortVolume(Voice, 2, 0.0));
		AudioDevice->ValidateAPICall(TEXT("sceNgs2VoicePatch"),
			sceNgs2VoicePatch(Voice, 2, AudioDevice->EQVoice, 0));
	}
	else
	{
		// if we're using libaudio3d the voices do not need to be patched because they get patched internally through the customsampler rack in
		// the UserFx2 module.
		if (!bIsA3dSound)
		{
			AudioDevice->ValidateAPICall(TEXT("sceNgs2VoicePatch"),
				sceNgs2VoicePatch(Voice, 0, Pathway->MasteringVoice, 0));
		}
	}

	// send to reverb if desired (this is in addition to EQ or master above)
	if (PathwayName == ENgs2Pathway::TV && bReverbApplied)
	{
		AudioDevice->ValidateAPICall(TEXT("sceNgs2VoicePatch"),
			sceNgs2VoicePatch(Voice, 1, AudioDevice->ReverbVoice, 0));
	}
}

void FNgs2SoundSource::OnBufferEnd(uint32 InVoiceId)
{
	FScopeLock Lock(&VoiceCallbackCriticalSection);

	// If we're still playing and if we still have a valid handle
	if (Playing && Voice != SCE_NGS2_HANDLE_INVALID && VoiceId == InVoiceId && WaveInstance != nullptr && Ngs2Buffer != nullptr)
	{
		// Get the voice state to determine if this sound source is done
		if (bBuffersToFlush)
		{
			bIsFinished = true;
			return;
		}
		
		if (RealtimeAsyncTask)
		{
			// Make sure the task finishes its job
			RealtimeAsyncTask->EnsureCompletion();

			switch (RealtimeAsyncTask->GetTask().GetTaskType())
			{
				case ERealtimeAudioTaskType::Decompress:
				{
					bLooped = RealtimeAsyncTask->GetTask().GetBufferLooped();

					USoundWave* WaveData = WaveInstance->WaveData;
					const int32 BytesWritten = Ngs2Buffer->GetMonoPCMBufferSize() * WaveData->NumChannels;
					RealtimeBufferData[CurrentBuffer].SetBytesWritten(BytesWritten);
				}
				break;

				case ERealtimeAudioTaskType::Procedural:
				{
					RealtimeBufferData[CurrentBuffer].SetBytesWritten(RealtimeAsyncTask->GetTask().GetBytesWritten());
				}
				break;
			}

			// Cleanup async decoding/generating task
			delete RealtimeAsyncTask;
			RealtimeAsyncTask = nullptr;

			// Write the results of the async task
			HandleRealTimeSource(bLooped);
		}

		// Update buffer index
		if (++CurrentBuffer > 2)
		{
			CurrentBuffer = 0;
		}

		// Figure out the data read mode based on if we've played the first cached buffer or if it's a streaming source
		EDataReadMode DataReadMode;
		if (bPlayedCachedBuffer)
		{
			bPlayedCachedBuffer = false;
			DataReadMode = EDataReadMode::AsynchronousSkipFirstFrame;
		}
		else
		{
			check(Ngs2Buffer);
			DataReadMode = EDataReadMode::Asynchronous;
		}

		bLooped = ReadMorePCMData(CurrentBuffer, DataReadMode, VoiceId);

		// If this was a synchronous read (i.e. no task was created), then immediate write the results!
		if (RealtimeAsyncTask == nullptr)
		{
			HandleRealTimeSource(bLooped);
		}
	}
}

bool FNgs2SoundSource::ReadMorePCMData(const int32 BufferIndex, const EDataReadMode DataReadMode, const int32 InVoiceId)
{
	FScopeLock Lock(&VoiceCallbackCriticalSection);

	if (!WaveInstance || !Ngs2Buffer || InVoiceId != VoiceId || Voice == SCE_NGS2_HANDLE_INVALID)
	{
		return false;
	}

	USoundWave* WaveData = WaveInstance->WaveData;
	if (WaveData && WaveData->bProcedural)
	{
		const int32 MaxSamples = (NGS2_MONO_PCM_BUFFER_SIZE * Buffer->NumChannels) / sizeof(int16);
		if (DataReadMode == EDataReadMode::Synchronous || WaveData->bCanProcessAsync == false)
		{
			const int32 BytesWritten = WaveData->GeneratePCMData(RealtimeBufferData[BufferIndex].AudioData.GetData(), MaxSamples);
			RealtimeBufferData[BufferIndex].SetBytesWritten(BytesWritten);
		}
		else
		{
			check(!RealtimeAsyncTask);
			RealtimeAsyncTask = new FAsyncRealtimeAudioTask(WaveData, RealtimeBufferData[BufferIndex].AudioData.GetData(), MaxSamples);
			RealtimeAsyncTask->StartBackgroundTask();
		}
		return false;
	}
	else
	{
		if (DataReadMode == EDataReadMode::Synchronous)
		{
			const bool bReadLooped = Ngs2Buffer->ReadCompressedData(RealtimeBufferData[BufferIndex].AudioData.GetData(), WaveInstance->LoopingMode != LOOP_Never);
			const int32 BytesWritten = Ngs2Buffer->GetMonoPCMBufferSize() * WaveData->NumChannels;
			RealtimeBufferData[BufferIndex].SetBytesWritten(BytesWritten);
			return bReadLooped;
		}
		else
		{
			check(!RealtimeAsyncTask);
			RealtimeAsyncTask = new FAsyncRealtimeAudioTask(Ngs2Buffer, RealtimeBufferData[BufferIndex].AudioData.GetData(), WaveInstance->LoopingMode != LOOP_Never, DataReadMode == EDataReadMode::AsynchronousSkipFirstFrame);
			RealtimeAsyncTask->StartBackgroundTask();
			return false;
		}
	}
}

void FNgs2SoundSource::SubmitPCMBuffers()
{
	AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceAddWaveformBlocks"),
		SubmitData(Voice, Ngs2Buffer->AT9Data.GetData(), Ngs2Buffer->WaveFormInfoBlocks, Ngs2Buffer->NumWaveFormInfoBlocks, 0));

	SetVoiceReady(true);
}

void FNgs2SoundSource::SubmitPCMRTBuffers()
{
	// Setup a ngs2 source voice callback for streamed sources
	// so we can get callbacks when more audio is needed for this source
	AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceSetCallback"),
		sceNgs2VoiceSetCallback(Voice, &FNgs2VoiceCallbackHandler, (uintptr_t)this, SCE_NGS2_VOICE_CALLBACK_FLAG_WAVEFORM_BLOCK_END));

	// Reset the buffer index
	CurrentBuffer = 0;

	const uint32 BufferSize = NGS2_MONO_PCM_BUFFER_SIZE * Ngs2Buffer->NumChannels;

	// Setup the RealtimeBufferData
	for (int32 i = 0; i < NGS2_AUDIO_REALTIME_BUFFER_COUNT; ++i)
	{
		RealtimeBufferData[i].AudioData.Reset();
		RealtimeBufferData[i].AudioData.AddZeroed(BufferSize);
		RealtimeBufferData[i].WaveformBlock.userData = VoiceId;
	}

	// Setup the first waveform block to hold the largest possible procedural buffer, and tell it to loop infinitely
	// The procedural voice will manage it's state on its own looping state (i.e. when it stops, etc).
	SceNgs2WaveformBlock FirstBlock;
	FirstBlock.dataOffset = 0;
	FirstBlock.dataSize = 0;
	FirstBlock.numSamples = MONO_PCM_BUFFER_SAMPLES * Buffer->NumChannels + 1;
	FirstBlock.numSkipSamples = 0;
	FirstBlock.numRepeats = 0;
	FirstBlock.userData = VoiceId;

	// Submit the first audio data buffer (just to have a ptr) even though the dataSize is set to 0. 
	// This sets up the voice for streamed audio. Subsequent waveform blocks will submit as data-only

	uint32 Flags = SCE_NGS2_WAVEFORM_BLOCK_FLAG_CONTINUOUS | SCE_NGS2_WAVEFORM_BLOCK_FLAG_NOJOINT;
	AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceAddWaveformBlocks"),
		SubmitData(Voice, (uint8*)RealtimeBufferData[0].AudioData.GetData(), &FirstBlock, 1, Flags));

	// Check if we need to copy the precached buffer for real-time decoded sources...
	bPlayedCachedBuffer = false;
	bool bIsSeeking = (WaveInstance->StartTime > 0.0f);
	if (WaveInstance->WaveData && WaveInstance->WaveData->CachedRealtimeFirstBuffer && !bIsSeeking)
	{
		bPlayedCachedBuffer = true;
		FMemory::Memcpy((uint8*)RealtimeBufferData[0].AudioData.GetData(), WaveInstance->WaveData->CachedRealtimeFirstBuffer, BufferSize);
		FMemory::Memcpy((uint8*)RealtimeBufferData[1].AudioData.GetData(), WaveInstance->WaveData->CachedRealtimeFirstBuffer + BufferSize, BufferSize);
	}
	else
	{
		// If we're not playing cached buffers (either streaming or procedural sources), the first two reads need to be synchronous.
		ReadMorePCMData(0, EDataReadMode::Synchronous, VoiceId);
		ReadMorePCMData(1, EDataReadMode::Synchronous, VoiceId);
	}

	// Immediately submit the first two buffers that were either cached or synchronously read.
	// The first buffer will start the voice processing buffers and trigger an OnBufferEnd callback, which will then 
	// trigger async tasks to generate more PCMRT buffers.

	AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceAddWaveformBlocks"),
		SubmitData(Voice, RealtimeBufferData[0].AudioData.GetData(), &RealtimeBufferData[0].WaveformBlock, 1, Flags));

	AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceAddWaveformBlocks"),
		SubmitData(Voice, RealtimeBufferData[1].AudioData.GetData(), &RealtimeBufferData[1].WaveformBlock, 1, Flags));

	CurrentBuffer = 2;

	bResourcesNeedFreeing = true;

	// Pass this voice's Id to the task so that if the voice id changes while the task is in-flight, we won't read more PCM data
	const int32 ThisVoiceId = VoiceId;
	AudioDevice->AudioRenderThreadCommand([this, ThisVoiceId]()
	{
		ReadMorePCMData(CurrentBuffer, EDataReadMode::Asynchronous, ThisVoiceId);
	});

	SetVoiceReady(true);
}

bool FNgs2SoundSource::PrepareForInitialization(FWaveInstance* InWaveInstance)
{
	// Flag that we're not initialized yet
	bInitialized = false;

	Ngs2Buffer = FNgs2SoundBuffer::Init(InWaveInstance->ActiveSound->AudioDevice, InWaveInstance->WaveData);
	if (Ngs2Buffer)
	{
		// If the realtime source is not ready, then we will need to free resources because this buffer is an async decoded buffer
		// and could be stopped before the header is finished being parsed.
		if (!Ngs2Buffer->IsRealTimeSourceReady() || InWaveInstance->IsStreaming())
		{
			bResourcesNeedFreeing = true;
		}

		Buffer = Ngs2Buffer;
		WaveInstance = InWaveInstance;

		LPFFrequency = MAX_FILTER_FREQUENCY;
		LastLPFFrequency = FLT_MAX;
		bIsFinished = false;

		// We succeed in preparing our xaudio2 buffer for initialization. We are technically not initialized yet.
		// If the buffer is asynchronously prepare the At9 file handle (in the case of a streaming or real-time decoded system), we may not yet be initiliaze the source.
		return true;
	}

	// Something went wrong when creating the ngs2 sound buffer
	return false;
}

bool FNgs2SoundSource::IsPreparedToInit()
{
	return Ngs2Buffer && Ngs2Buffer->IsRealTimeSourceReady();
}

bool FNgs2SoundSource::Init(FWaveInstance* InWaveInstance)
{
	check(Ngs2Buffer);
	check(Ngs2Buffer->IsRealTimeSourceReady());

	FSoundSource::InitCommon();

	VoiceId = ++NextVoiceId;

	// If Buffer is nullptr or the number of channels is 0, then it failed to be created
	if (Buffer && Buffer->NumChannels > 0)
	{
		SCOPE_CYCLE_COUNTER( STAT_AudioSourceInitTime );

		WaveInstance = InWaveInstance;

		// Copy the waveform info blocks header from the state that it was originally parsed
		Ngs2Buffer->NumWaveFormInfoBlocks = Ngs2Buffer->NumWaveFormInfoBlocksHeader;
		FMemory::Memcpy(Ngs2Buffer->WaveFormInfoBlocks, Ngs2Buffer->WaveFormInfoBlocksHeader, sizeof(SceNgs2WaveformBlock) * SCE_NGS2_WAVEFORM_INFO_MAX_BLOCKS);

		if (WaveInstance->StartTime > 0.0f)
		{
			Ngs2Buffer->Seek(WaveInstance->StartTime);
		}

		// Set whether to apply reverb
		SetReverbApplied((InWaveInstance->OutputTarget == EAudioOutputTarget::Speaker) && AudioDevice->ReverbVoice != SCE_NGS2_HANDLE_INVALID);

		// Create a new source if we haven't already
		if (CreateSource(InWaveInstance->bUseSpatialization))
		{
			// Updates the source which e.g. sets the pitch and volume.
			Update();

			bInitialized = true;

			// Initialization succeeded.
			return true;

		}
	}

	// Initialization failed.
	return false;
}

bool FNgs2SoundSource::CreateSource(const bool bUseSpatialization)
{
	check(WaveInstance);

	// Get the "pathway" name of the sound source
	ENgs2Pathway::Type PathwayName;

	bIsA3dSound = false;

	if (WaveInstance->OutputTarget == EAudioOutputTarget::Speaker)
	{
#if A3D && USING_A3D
		if (bUseSpatialization && Buffer->NumChannels == 1)
		{
			bIsA3dSound = true;
			PathwayName = ENgs2Pathway::TV_A3D;
		}
		else
#endif
		{
			PathwayName = ENgs2Pathway::TV;
		}
	}
	else
	{
		PathwayName = ENgs2Pathway::Controller;
	}

	// A3D enabled sounds (mono) only support decoding at a fixed "grainSize". 
	if (bIsA3dSound)
	{
		Ngs2Buffer->SetMonoPCMBufferSize(256);
	}
	else
	{
		Ngs2Buffer->SetMonoPCMBufferSize(NGS2_MONO_PCM_BUFFER_SIZE);
	}

	// And the pathway 
	FNgs2Pathway* Pathway = AudioDevice->GetPathway(PathwayName, WaveInstance->UserIndex);

	// Get the voice out of the sampler rack for this source
	AudioDevice->ValidateAPICall(TEXT("sceNgs2RackGetVoiceHandle_Sampler"),
		sceNgs2RackGetVoiceHandle(Pathway->SamplerRack, VoiceIndex, &Voice));

	if (Voice != SCE_NGS2_HANDLE_INVALID)
	{
		bBuffersToFlush = false;
		bIsFinished = false;
		bLooped = false;
		bLoopCallback = false;
		bPlayedCachedBuffer = false;

		// Make sure any previous lives of the voice handle are killed. In-flight stop events may not have completed so we need to force the voice to be free from previous data.
		AudioDevice->ValidateAPICall(TEXT("sceNgs2RackGetVoiceHandle_Sampler"),
			sceNgs2VoiceKickEvent(Voice, SCE_NGS2_VOICE_EVENT_KILL));

		// Setup looping
		if (WaveInstance->LoopingMode != LOOP_Never && Ngs2Buffer->SoundFormat == SoundFormat_PCM)
		{
			check(Ngs2Buffer->WaveformInfo.numBlocks == 1);

			// If start time is not at the beginning of the wave, we can't just loop the waveform block since it's
			// only playing the partial sound.  Instead we first play the partial sound and add another block to
			// loop the full length of the audio.
			if (WaveInstance->StartTime > 0.0f)
			{
				Ngs2Buffer->WaveFormInfoBlocks[0].numRepeats = 0;

				// Copy the parameters of the original full length sample and add as the next sample block to play that
				// will be the one to do the looping.
				FMemory::Memcpy(&Ngs2Buffer->WaveFormInfoBlocks[1], &Ngs2Buffer->WaveformInfo.aBlock[0], sizeof(SceNgs2WaveformBlock));
				Ngs2Buffer->WaveFormInfoBlocks[1].numRepeats = SCE_NGS2_WAVEFORM_BLOCK_REPEAT_INFINITE;

				// We now have 2 blocks to play.
				Ngs2Buffer->NumWaveFormInfoBlocks = 2;
			}
			else
			{
				Ngs2Buffer->WaveFormInfoBlocks[0].numRepeats = SCE_NGS2_WAVEFORM_BLOCK_REPEAT_INFINITE;
			}
			TotalLoopCount = 0;
		}

		Pathway->SetupVoice(*this);

		if (bVoiceSetup)
		{
			PatchVoice(Pathway);
		}

		return true;
	}

	return false;
}

void FNgs2SoundSource::Update()
{
	SCOPE_CYCLE_COUNTER( STAT_AudioUpdateSources );

	if (!WaveInstance || Paused || !Buffer)
	{	
		return;
	}

	FSoundSource::UpdateCommon();

	// Clamp to valid values in case the hardware doesn't like strange values
	float Volume = FMath::Clamp<float>(WaveInstance->GetActualVolume(), 0.0f, MAX_VOLUME);
	Volume *= AudioDevice->GetPlatformAudioHeadroom();

	bool bWasA3dSound = false;
	Volume = FSoundSource::GetDebugVolume(Volume);

#if !UE_BUILD_SHIPPING
	if (MuteStreamingSounds == 1 && Ngs2Buffer->GetFormat() == ESoundFormat::SoundFormat_Streaming)
	{
		Volume = 0.0f;
	}
	else if (MuteNonStreamingSounds == 1 && !(Ngs2Buffer->GetFormat() == ESoundFormat::SoundFormat_Streaming))
	{
		Volume = 0.0f;
	}
#endif

	FVector RelativeSoundPosition = FVector::ZeroVector;
	if (WaveInstance->bUseSpatialization)
	{		
		FSpatializationParams SpatializationParams = GetSpatializationParams();

		// PanParam.distance is the spread of a sound. 1.0 is no spread, 0.0 is full spread. We map our omni-radius to distance spread.
		float PanParamDist = 1.0f;
		if (SpatializationParams.Distance < WaveInstance->OmniRadius)
		{
			check(WaveInstance->OmniRadius > 0.0f);
			PanParamDist = SpatializationParams.Distance / WaveInstance->OmniRadius;
		}

		if (Buffer->NumChannels == 2)
		{
			UpdateStereoEmitterPositions();

			float Stereo3dVolumeMatrix[SCE_NGS2_PAN_MATRIX_FORMAT_7_1CH * 2];

			// Need a pan param for each stereo channel
			SceNgs2PanParam PanParams[2];

			// Listener coord space is a little strange.  Add an epsilon to avoid undefined atan2(0,0)
			float AngleToLeftChannel = FMath::Atan2(SpatializationParams.LeftChannelPosition.Y + 0.01f, -SpatializationParams.LeftChannelPosition.Z + 0.01f);
			AngleToLeftChannel = FMath::RadiansToDegrees(AngleToLeftChannel);

			float AngleToRightChannel = FMath::Atan2(SpatializationParams.RightChannelPosition.Y + 0.01f, -SpatializationParams.RightChannelPosition.Z + 0.01f);
			AngleToRightChannel = FMath::RadiansToDegrees(AngleToRightChannel);

			PanParams[0] = { AngleToLeftChannel, PanParamDist, 1.0f, 0.0f };
			PanParams[1] = { AngleToRightChannel, PanParamDist, 1.0f, 0.0f };

			AudioDevice->ValidateAPICall(TEXT("sceNgs2PanGetVolumeMatrix"),
				sceNgs2PanGetVolumeMatrix(&AudioDevice->GetPanningData(), PanParams, 2, SCE_NGS2_PAN_MATRIX_FORMAT_7_1CH, Stereo3dVolumeMatrix));

			AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceSetMatrixLevels"),
				sceNgs2VoiceSetMatrixLevels(Voice, 0, Stereo3dVolumeMatrix, SCE_NGS2_PAN_MATRIX_FORMAT_7_1CH * 2));

			AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceSetPortMatrix"),
				sceNgs2VoiceSetPortMatrix(Voice, 0, 0));
		}
		else
		{
#if A3D
			if (bIsA3dSound)
			{
				A3dParams.Priority = 0;

				if (WaveInstance->bUseSpatialization)
				{
					RelativeSoundPosition = AudioDevice->InverseTransform.TransformPosition(WaveInstance->Location);

					//rearrange so that position is in the A3D space.  (X : Right, Y : Up, Z : Forward
					//UU are cm, libaudio3d wants meters.
					const float UnrealUnitsToMeters = 0.01f;
					A3dParams.X = RelativeSoundPosition.Y * UnrealUnitsToMeters;
					A3dParams.Y = RelativeSoundPosition.X * UnrealUnitsToMeters;
					A3dParams.Z = -RelativeSoundPosition.Z * UnrealUnitsToMeters;
				}

				A3dParams.Spread = 0.0f;
				A3dParams.Gain = Volume;

				sceNgs2CustomVoiceSetUserFx2(Voice, 0, &A3dParams, sizeof(A3dParams));

				// Flag that this was spatialized through the A3D library... this is so we don't set volume later
				bWasA3dSound = true;
			}
			else
#endif

			{
				float Mono3dVolumeMatrix[SCE_NGS2_PAN_MATRIX_FORMAT_7_1CH];

				checkf(Buffer->NumChannels == 1, TEXT("Can only spatialize stereo or mono sound assets."));

				// Only need a matrix large enough
				SceNgs2PanParam PanParams;

				// Listener coord space is a little strange.  Add an epsilon to avoid undefined atan2(0,0)
				float AngleToSound = FMath::Atan2(SpatializationParams.EmitterPosition.Y + 0.01f, -SpatializationParams.EmitterPosition.Z + 0.01f);
				AngleToSound = FMath::RadiansToDegrees(AngleToSound);

				// calculate panning
				PanParams.angle = AngleToSound;	// Angle (-360.0f~0.0f~+360.0f) 
				PanParams.distance = PanParamDist;
				PanParams.fbwLevel = 1.0f;		// 100[%]
				PanParams.lfeLevel = 0.0f;		// Disable LFE (subwoofer)... why???

				AudioDevice->ValidateAPICall(TEXT("sceNgs2PanGetVolumeMatrix"),
					sceNgs2PanGetVolumeMatrix(&AudioDevice->GetPanningData(), &PanParams, 1, SCE_NGS2_PAN_MATRIX_FORMAT_7_1CH, Mono3dVolumeMatrix));


				AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceSetMatrixLevels"),
					sceNgs2VoiceSetMatrixLevels(Voice, 0, Mono3dVolumeMatrix, SCE_NGS2_PAN_MATRIX_FORMAT_7_1CH));

				AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceSetPortMatrix"),
					sceNgs2VoiceSetPortMatrix(Voice, 0, 0));
			}
		}
	}

	// If this sound was an A3D spatialized sound, no need to set volume since
	// it is set automatically when calling the A3D library above.
	if (bWasA3dSound)
	{	
		AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceSetPitch"),
			sceNgs2CustomSamplerVoiceSetPitch(Voice, Pitch));
	}
	else
	{
		AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceSetPitch"),
			sceNgs2SamplerVoiceSetPitch(Voice, Pitch));

		AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceSetPortVolume"),
			sceNgs2VoiceSetPortVolume(Voice, 0, Volume));
	}

	if (bReverbApplied)
	{
		AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceSetPortVolume"),
			sceNgs2VoiceSetPortVolume(Voice, 1, Volume));
	}

	// Unfortunately, we can't use the normal filter effect on A3D-spatialized sounds
	if (!bWasA3dSound)
	{
		// Set the low pass filter frequency value
		SetFilterFrequency();

		if (LastLPFFrequency != LPFFrequency)
		{
			// Set the per-voice filter effect
			float OneOverQ = AudioDevice->GetLowPassFilterResonance();
			check(OneOverQ > 0.0f);
			float Q = 1.0f / OneOverQ;

			AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceSetFilterByFcq"),
				sceNgs2SamplerVoiceSetFilterByFcq(Voice, 0, SCE_NGS2_SAMPLER_FILTER_LOCATION_REAR, SCE_NGS2_FILTER_TYPE_LOW_PASS, 0, LPFFrequency, Q, 1.0f));

			LastLPFFrequency = LPFFrequency;
		}
	}

	HandleLooping();

	FSoundSource::DrawDebugInfo();
}

void FNgs2SoundSource::HandleLooping()
{
	if (!WaveInstance)
	{
		return;
	}

	// If our sound format is native, we need to update voice state by querying voice flags
	if (Ngs2Buffer->SoundFormat == SoundFormat_PCM)
	{
		// If we're a one-shot, we need to query if the voice is still in-use. If it's not in-use then
		// it has been stopped.
		if (WaveInstance->LoopingMode == LOOP_Never)
		{
			// ask the API for the state
			uint32 Flags;
			AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceGetState"),
				sceNgs2VoiceGetStateFlags(Voice, &Flags));

			// if it's no longer in use, it's stopped
			if (!(Flags & SCE_NGS2_VOICE_STATE_FLAG_INUSE))
			{
				// Set the flag that we're finished. This is going to be queried in the IsFinished() function
				bIsFinished = true;
			}
		}
		else if (WaveInstance->LoopingMode == LOOP_WithNotification)
		{
			// Check if it has looped
			SceNgs2SamplerVoiceState SamplerVoiceState;
			AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceGetState"),
				sceNgs2SamplerVoiceGetState(Voice, &SamplerVoiceState));

			uint32 NumSamples = Ngs2Buffer->WaveFormInfoBlocks[0].numSamples;
			uint32 CurrentLoopCount = SamplerVoiceState.numDecodedSamples / NumSamples;

			if (WaveInstance->StartTime > 0.0f && Ngs2Buffer->NumWaveFormInfoBlocks > 1)
			{
				// Loop count is calculated differently if StartTime is not at the beginning of the wave since
				// we use the first sample block as the partial sound and the second block as the full looping sound.
				CurrentLoopCount = 0;
				uint64 NumDecodedSamples = SamplerVoiceState.numDecodedSamples;
				if (NumDecodedSamples > NumSamples)
				{
					// We have played the first sample block so it counts as the first loop.
					CurrentLoopCount += 1;
					// Deduct the num samples of the first block to the total decoded sample
					// so that the calculation for the full looping sound in the next block is correct.
					NumDecodedSamples -= NumSamples;

					NumSamples = Ngs2Buffer->WaveFormInfoBlocks[1].numSamples;
					CurrentLoopCount += NumDecodedSamples / NumSamples;
				}
			}

			// We have completed a loop, so set the flag to send a notification that we're looping
			if (CurrentLoopCount != TotalLoopCount)
			{
				TotalLoopCount = CurrentLoopCount;
				bLoopCallback = true;
			}
		}
	}
}

void FNgs2SoundSource::Play()
{
	if (WaveInstance)
	{
		if (!Playing && bVoiceSetup)
		{
			if (AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceKickEvent_SourcePlay"),
				sceNgs2VoiceKickEvent(Voice, SCE_NGS2_VOICE_EVENT_PLAY)))
			{
				Paused = false;
				Playing = true;
				bIsFinished = false;
			}

			if (WaveInstance->LoopingMode == LOOP_Never)
			{
				if (bIsA3dSound)
				{	
					AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceExitLoop"),
						sceNgs2CustomSamplerVoiceExitLoop(Voice));					
				}
				else
				{
					AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceExitLoop"),
						sceNgs2SamplerVoiceExitLoop(Voice));
				}
			}
		}
		else if (Paused)
		{
			if (AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceKickEvent_SourcePlay"),
				sceNgs2VoiceKickEvent(Voice, SCE_NGS2_VOICE_EVENT_RESUME)))
			{
				Paused = false;
				bIsFinished = false;
			}
		}
	}
}

void FNgs2SoundSource::Stop()
{
	IStreamingManager::Get().GetAudioStreamingManager().RemoveStreamingSoundSource(this);

	FScopeLock Lock(&VoiceCallbackCriticalSection);

	if (Playing && Voice != SCE_NGS2_HANDLE_INVALID)
	{
 		AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceKickEvent_SourceStop"),
 			sceNgs2VoiceKickEvent(Voice, SCE_NGS2_VOICE_EVENT_STOP));
	}

	Paused = false;
	Playing = false;
	bVoiceSetup = false;

	FreeResources();

	bBuffersToFlush = false;
	bLoopCallback = false;
	bResourcesNeedFreeing = false;

	FSoundSource::Stop();
}

void FNgs2SoundSource::Pause()
{
	if( WaveInstance )
	{
		AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceKickEvent_SourcePause"),
			sceNgs2VoiceKickEvent(Voice, SCE_NGS2_VOICE_EVENT_PAUSE));

		Paused = true;
	}
}

bool FNgs2SoundSource::IsFinished()
{
	// A paused source is not finished.
	if (Paused || !bInitialized)
	{
		// We could be still getting initialized
		return false;
	}

	if (!WaveInstance || Voice == SCE_NGS2_HANDLE_INVALID)
	{
		return true;
	}

	if (bIsFinished)
	{
		WaveInstance->NotifyFinished();
		return true;
	}

	if (bLoopCallback && WaveInstance->LoopingMode == LOOP_WithNotification)
	{
		WaveInstance->NotifyFinished();
		bLoopCallback = false;
	}

	return false;
}

void FNgs2SoundSource::HandleRealTimeSource(const bool bInLooped)
{
	if (bInLooped)
	{
		switch (WaveInstance->LoopingMode)
		{
		case LOOP_Never:
				bBuffersToFlush = true;
				break;

			case LOOP_WithNotification:
				// If we've looped here, we're programmatically looping, send notification.
				// This will trigger a WaveInstance->NotifyFinished() in the FNgs2SoundSource::IsFinished() function on main thread.
				bLoopCallback = true;
				break;

			case LOOP_Forever:
				// Let sound loop indefinitely
				break;
		}
	}

	// If we have actually generated audio, then write it to the voice
	if (RealtimeBufferData[CurrentBuffer].WaveformBlock.dataSize > 0)
	{
		int32 Result = 0;
		constexpr uint32 Flags = SCE_NGS2_WAVEFORM_BLOCK_FLAG_CONTINUOUS | SCE_NGS2_WAVEFORM_BLOCK_FLAG_NOJOINT;
		Result = SubmitData(Voice, RealtimeBufferData[CurrentBuffer].AudioData.GetData(), &RealtimeBufferData[CurrentBuffer].WaveformBlock, 1, Flags);

		if (Result < 0)
		{
			UE_LOG(LogAudio, Error, TEXT("Failed to submit waveform block to realtime Ngs2 voice: 0x%08x"), Result);
			bBuffersToFlush = true;
			bIsFinished = true;
		}
	}
}

int32 FNgs2SoundSource::SubmitData(SceNgs2Handle VoiceHandle, uint8* DataBuffer, const SceNgs2WaveformBlock* WaveformBlock, const int32 NumWaveFormInfoBlocks, const uint32 Flags)
{
#if A3D && USING_A3D
	if (bIsA3dSound)
	{
		return sceNgs2CustomSamplerVoiceAddWaveformBlocks(VoiceHandle, DataBuffer, WaveformBlock, NumWaveFormInfoBlocks, Flags);
	}
	else
#endif
	{
		return sceNgs2SamplerVoiceAddWaveformBlocks(VoiceHandle, DataBuffer, WaveformBlock, NumWaveFormInfoBlocks, Flags);
	}
}

FString FNgs2SoundSource::Describe(bool bUseLongName)
{
#if !UE_BUILD_SHIPPING
	FString Base = FSoundSource::Describe(bUseLongName);

	// get state of the sound
	uint32 Flags;
	AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceGetState"),
		sceNgs2VoiceGetStateFlags(Voice, &Flags));

	FString StateDesc;
	if (Flags & SCE_NGS2_VOICE_STATE_FLAG_INUSE)
	{
		StateDesc += FString(TEXT("INUSE "));
	}
	if (Flags & SCE_NGS2_VOICE_STATE_FLAG_PLAYING)
	{
		StateDesc += FString(TEXT("PLAYING "));
	}
	if (Flags & SCE_NGS2_VOICE_STATE_FLAG_PAUSED)
	{
		StateDesc += FString(TEXT("PAUSED "));
	}
	if (Flags & SCE_NGS2_VOICE_STATE_FLAG_STOPPED)
	{
		StateDesc += FString(TEXT("STOPPED "));
	}
	if (Flags & SCE_NGS2_VOICE_STATE_FLAG_ERROR)
	{
		StateDesc += FString(TEXT("ERROR "));
	}
	if (Flags & SCE_NGS2_VOICE_STATE_FLAG_EMPTY)
	{
		StateDesc += FString(TEXT("EMPTY "));
	}

	return Base + TEXT(", Ngs2 State: ") + StateDesc;
#else
	return TEXT("");
#endif
}
