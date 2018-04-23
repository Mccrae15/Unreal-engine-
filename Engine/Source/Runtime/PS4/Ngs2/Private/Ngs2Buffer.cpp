// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Ngs2Device.h"
#include "Ngs2Effects.h"
#include "Ngs2Support.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "IAudioFormat.h"


FNgs2SoundBuffer::FNgs2SoundBuffer(FAudioDevice* InAudioDevice, ESoundFormat InSoundFormat)
	: FSoundBuffer(InAudioDevice)
	, SoundFormat(InSoundFormat)
	, DecompressionState(nullptr)
	, RealtimeAsyncHeaderParseTask(nullptr)
	, NumWaveFormInfoBlocks(0)
	, MonoPCMBufferSize(0)
	, NumWaveFormInfoBlocksHeader(0)
{
	FMemory::Memzero(WaveFormInfoBlocks);
	FMemory::Memzero(WaveFormInfoBlocksHeader);
}

FNgs2SoundBuffer::~FNgs2SoundBuffer()
{
	check(RealtimeAsyncHeaderParseTask == nullptr);

	if (DecompressionState)
	{
		delete DecompressionState;
	}
}

int32 FNgs2SoundBuffer::GetSize()
{
	switch (SoundFormat)
	{
		case SoundFormat_PCM:
			return AT9Data.Num();

		case SoundFormat_PCMRT:
			return (DecompressionState ? DecompressionState->GetSourceBufferSize() : 0) + (NGS2_MONO_PCM_BUFFER_SIZE * NGS2_AUDIO_REALTIME_BUFFER_COUNT * NumChannels);

		case SoundFormat_Streaming:
			return NGS2_MONO_PCM_BUFFER_SIZE * NGS2_AUDIO_REALTIME_BUFFER_COUNT * NumChannels;
	}
	return 0;
}

int32 FNgs2SoundBuffer::GetCurrentChunkIndex() const
{
	if (DecompressionState)
	{
		return DecompressionState->GetCurrentChunkIndex();
	}
	return INDEX_NONE;
}

int32 FNgs2SoundBuffer::GetCurrentChunkOffset() const
{
	if (DecompressionState)
	{
		return DecompressionState->GetCurrentChunkOffset();
	}
	return INDEX_NONE;
}

bool FNgs2SoundBuffer::IsRealTimeSourceReady()
{
	// If we have a realtime async header parse task, then we check if its done
	if (RealtimeAsyncHeaderParseTask)
	{
		const bool bIsDone = RealtimeAsyncHeaderParseTask->IsDone();
		if (bIsDone)
		{
			delete RealtimeAsyncHeaderParseTask;
			RealtimeAsyncHeaderParseTask = nullptr;
		}
		return bIsDone;
	}
	// Otherwise, we weren't a real time decoding sound buffer (or we've already asked and it was ready)
	return true;
}

void FNgs2SoundBuffer::EnsureRealtimeTaskCompletion()
{
	if (RealtimeAsyncHeaderParseTask)
	{
		RealtimeAsyncHeaderParseTask->EnsureCompletion();
		delete RealtimeAsyncHeaderParseTask;
		RealtimeAsyncHeaderParseTask = nullptr;
	}
}

void FNgs2SoundBuffer::InitWaveformInfo(USoundWave* InSoundWave)
{
	// Setup the WaveformInfo format
	FMemory::Memzero(&WaveformInfo, sizeof(WaveformInfo));
	SceNgs2WaveformFormat& Format = WaveformInfo.format;
	Format.waveformType = SCE_NGS2_WAVEFORM_TYPE_PCM_I16L;
	Format.numChannels = InSoundWave->NumChannels;
	Format.sampleRate = 48000; // Our Ngs2 and AT9 files are all at 48k
	Format.configData = 0;
	Format.frameOffset = 0;
	Format.frameMargin = 0;
}

bool FNgs2SoundBuffer::ReadCompressedInfo(USoundWave* SoundWave)
{
	if (!DecompressionState)
	{
		UE_LOG(LogAudio, Warning, TEXT("Attempting to read compressed info without a compression state instance for resource '%s'"), *ResourceName);
		return false;
	}

	return DecompressionState->ReadCompressedInfo(SoundWave->ResourceData, SoundWave->ResourceSize, nullptr);
}

bool FNgs2SoundBuffer::ReadCompressedData(uint8* Destination, bool bLooping)
{
	if (!DecompressionState)
	{
		UE_LOG(LogAudio, Warning, TEXT("Attempting to read compressed data without a compression state instance for resource '%s'"), *ResourceName);
		return false;
	}

	const uint32 kPCMBufferSize = MonoPCMBufferSize * NumChannels;
	if (SoundFormat == SoundFormat_Streaming)
	{
		return DecompressionState->StreamCompressedData(Destination, bLooping, kPCMBufferSize);
	}
	else
	{
		return DecompressionState->ReadCompressedData(Destination, bLooping, kPCMBufferSize);
	}
}

void FNgs2SoundBuffer::Seek(const float SeekTime)
{
	// If we're doing real-time decompression, we need to use the compressor to seek
	if (DecompressionState)
	{
		DecompressionState->SeekToTime(SeekTime);
	}
	else
	{
		// Start time will only work if numblocks == 1, will this ever not be the case?
		check(WaveformInfo.numBlocks == 1);

		const float StartSampleFloat = SeekTime * WaveformInfo.format.sampleRate;
		const uint32 StartSample = FMath::TruncToInt(StartSampleFloat);

		const uint32 BlockNumSamples = WaveFormInfoBlocksHeader[0].numSamples;
		const uint32 BlockStartSample = (StartSample <= BlockNumSamples) ? StartSample : BlockNumSamples;
		const uint32 BlockTotalSamples = BlockNumSamples - BlockStartSample;

		// Save off initial offset so that we can restore it later
		const uint32 BlockInitialOffset = WaveFormInfoBlocksHeader[0].dataOffset;

		if (AudioDevice->ValidateAPICall(TEXT("sceNgs2CalcWaveformBlock"),
			sceNgs2CalcWaveformBlock(&WaveformInfo.format, BlockStartSample, BlockTotalSamples, &WaveFormInfoBlocks[0])))
		{
 			WaveFormInfoBlocks[0].dataOffset += BlockInitialOffset;
 			WaveFormInfoBlocks[0].dataSize += BlockInitialOffset;
		}
	}
}

FNgs2SoundBuffer* FNgs2SoundBuffer::CreateNativeBuffer(FAudioDevice* InAudioDevice, USoundWave* InSoundWave)
{
	// get the cooked data -- we don't need to decompress native buffers for ngs2
	FByteBulkData* AT9BulkData = InSoundWave->GetCompressedData(InAudioDevice->GetRuntimeFormat(InSoundWave));

	// If we found bulk data then we're good to go
	if (AT9BulkData)
	{
		FNgs2SoundBuffer* NewBuffer = new FNgs2SoundBuffer(InAudioDevice, SoundFormat_PCM);

		// Take ownership of the bulk cooked AT9 data
		const int32 BulkDataSize = AT9BulkData->GetBulkDataSize();
		NewBuffer->AT9Data.Reset(BulkDataSize);
		NewBuffer->AT9Data.SetNumUninitialized(BulkDataSize);

		uint8* DataPtr = NewBuffer->AT9Data.GetData();
		constexpr bool bDiscardInternalCopy = true;
		AT9BulkData->GetCopy((void**)&DataPtr, bDiscardInternalCopy);

		NewBuffer->NumChannels = InSoundWave->NumChannels;

		// load the waveform information (parses the header)
		InAudioDevice->ValidateAPICall(TEXT("sceNgs2ParseWaveformData"),
			sceNgs2ParseWaveformData(NewBuffer->AT9Data.GetData(), NewBuffer->AT9Data.Num(), &NewBuffer->WaveformInfo));

		FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
		check(AudioDeviceManager != nullptr);
		AudioDeviceManager->TrackResource(InSoundWave, (FSoundBuffer*)NewBuffer);

		InSoundWave->RemoveAudioResource();

		// Get a copy of the wave form info blocks for the source data. This will always be 1 for our case since we're not embedding loop points.
		NewBuffer->NumWaveFormInfoBlocksHeader = NewBuffer->WaveformInfo.numBlocks;
		FMemory::Memcpy(NewBuffer->WaveFormInfoBlocksHeader, NewBuffer->WaveformInfo.aBlock, sizeof(SceNgs2WaveformBlock) * SCE_NGS2_WAVEFORM_INFO_MAX_BLOCKS);

		return NewBuffer;
	}
	else
	{
		UE_LOG(LogAudio, Warning, TEXT("Failed to retreive cooked data for audio file."));
	}
	return nullptr;
}

FNgs2SoundBuffer* FNgs2SoundBuffer::CreateProceduralBuffer(FAudioDevice* InAudioDevice, USoundWave* InSoundWave)
{
	FNgs2SoundBuffer* NewBuffer = new FNgs2SoundBuffer(InAudioDevice, SoundFormat_PCMRT);
	NewBuffer->InitWaveformInfo(InSoundWave);

	NewBuffer->NumChannels = InSoundWave->NumChannels;
	NewBuffer->ResourceID = 0;

	InSoundWave->ResourceID = 0;

	return NewBuffer;
}

FNgs2SoundBuffer* FNgs2SoundBuffer::CreateStreamingBuffer(FAudioDevice* InAudioDevice, USoundWave* InSoundWave)
{
	FNgs2SoundBuffer* NewBuffer = new FNgs2SoundBuffer(InAudioDevice, SoundFormat_Streaming);

	// Prime the first two buffers and prepare the decompression
	FSoundQualityInfo QualityInfo = { 0 };

	check(NewBuffer->DecompressionState == nullptr);
	NewBuffer->DecompressionState = InAudioDevice->CreateCompressedAudioInfo(InSoundWave);

	if (NewBuffer->DecompressionState->StreamCompressedInfo(InSoundWave, &QualityInfo))
	{
		InSoundWave->SampleRate = QualityInfo.SampleRate;
		InSoundWave->NumChannels = QualityInfo.NumChannels;
		InSoundWave->RawPCMDataSize = QualityInfo.SampleDataSize;
		InSoundWave->Duration = QualityInfo.Duration;

		NewBuffer->NumChannels = QualityInfo.NumChannels;
		NewBuffer->InitWaveformInfo(InSoundWave);

		NewBuffer->MonoPCMBufferSize = NGS2_MONO_PCM_BUFFER_SIZE;
	}
	else
	{
		InSoundWave->DecompressionType = DTYPE_Invalid;
		InSoundWave->NumChannels = 0;
		InSoundWave->RemoveAudioResource();
	}

	return NewBuffer;
}

FNgs2SoundBuffer* FNgs2SoundBuffer::Init(FAudioDevice* InAudioDevice, USoundWave* InSoundWave)
{
	if (InSoundWave == nullptr || InSoundWave->NumChannels == 0)
	{
		return nullptr;
	}

	FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
	FNgs2Device* Ngs2Device = (FNgs2Device*)InAudioDevice;
	FNgs2SoundBuffer* Buffer = nullptr;

	// Allow the precache to happen if necessary
	EDecompressionType DecompressionType = InSoundWave->DecompressionType;

	switch (DecompressionType)
	{
		case DTYPE_Setup:
			// Has circumvented precache mechanism - precache now
			InAudioDevice->Precache(InSoundWave, true, false);

			// if it didn't change, we will recurse forever
			check(InSoundWave->DecompressionType != DTYPE_Setup);

			// Recall this function with new decompression type
			return Init(InAudioDevice, InSoundWave);

		//Native type means the Wave already contains all the data.
		case DTYPE_Native:
		{
			if (InSoundWave->ResourceID)
			{
				Buffer = (FNgs2SoundBuffer*)AudioDeviceManager->WaveBufferMap.FindRef(InSoundWave->ResourceID);
			}

			if (Buffer == nullptr)
			{
				Buffer = CreateNativeBuffer(InAudioDevice, InSoundWave);
			}
			break;
		}

		case DTYPE_Procedural:
		{
			Buffer = CreateProceduralBuffer(InAudioDevice, InSoundWave);
			break;
		}

		case DTYPE_Streaming:
		{
			Buffer = CreateStreamingBuffer(InAudioDevice, InSoundWave);
			break;
		}

		default:
			UE_LOG(LogAudio, Warning, TEXT("NSG2 Buffer type: %i not supported"), DecompressionType);
			break;
	}

	return Buffer;
}

