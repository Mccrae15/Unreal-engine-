// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Ngs2Device.h"
#include "AudioEffect.h"
#include "AudioDecompress.h"
#include "Ngs2Effects.h"
#include "Ngs2Support.h"
#include <audioout.h>
#include <user_service.h>
#include <libsysmodule.h>
#include "PS4Application.h"
#include <Sulpha.h>
#include "Atrac9Decoder.h"
#include "HAL/RunnableThread.h"
#include "HAL/LowLevelMemTracker.h"

#include "Sound/AudioSettings.h"

const int32 GrainSize = 256; // hardcoded in samples
const int32 SampleRate = 48000; // docs say to always use 48000


DEFINE_LOG_CATEGORY(LogNgs2);

class FNgs2DeviceModule : public IAudioDeviceModule
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new FNgs2Device;
	}
};

IMPLEMENT_MODULE(FNgs2DeviceModule, Ngs2);


void FSulpha::Init()
{
#if (ENABLE_SULPHA_DEBUGGER)
	int Result;

	SceSulphaConfig SulphaConfig;
	size_t          SulphaMemSize;
	
	Result = sceSysmoduleLoadModule( SCE_SYSMODULE_SULPHA );
	if( Result < SCE_OK )
	{
		return;
	}
				
	Result = sceSulphaGetDefaultConfig( &SulphaConfig );
	check( Result == SCE_OK );

	Result = sceSulphaGetNeededMemory( &SulphaConfig, &SulphaMemSize );
	check( Result == SCE_OK );

	SulphaBuffer = FMemory::Malloc( SulphaMemSize );
	if( SulphaBuffer == NULL )
	{
		return;
	}

	Result = sceSulphaInit( &SulphaConfig, SulphaBuffer, SulphaMemSize );
	check( Result == SCE_OK );
#endif
}

void FSulpha::Shutdown()
{
#if (ENABLE_SULPHA_DEBUGGER)
	sceSulphaShutdown();
	FMemory::Free( SulphaBuffer );
	SulphaBuffer = NULL;

	sceSysmoduleUnloadModule( SCE_SYSMODULE_SULPHA );
#endif
}

FNgs2Pathway::FNgs2Pathway(ENgs2Pathway::Type InPathwayName, FNgs2Device* InAudioDevice, SceUserServiceUserId UserIndex, uint32 InChannelCount, uint32 InOutputDestination)
	: AudioDevice(InAudioDevice)
{
	PathwayName = InPathwayName;
	check(InAudioDevice);
	AudioDevice = InAudioDevice;

	ChannelCount = InChannelCount;
	OutputDestination = InOutputDestination;

	// initialize Ngs2
	SceNgs2SystemOption SystemOption;
	sceNgs2SystemResetOption(&SystemOption);
	AudioDevice->ValidateAPICall(TEXT("sceNgs2SystemCreateWithAllocator"),
		sceNgs2SystemCreateWithAllocator(&SystemOption, &AudioDevice->Ngs2Allocator, &Ngs2));

	FMemory::Memzero(Buffer[0]);
	FMemory::Memzero(Buffer[1]);

	// set up the render descriptor
	BufferInfo.bufferSize = sizeof(Buffer[0]);
	BufferInfo.numChannels = ChannelCount;
	BufferInfo.waveformType = SCE_NGS2_WAVEFORM_TYPE_PCM_F32L;
}

void FNgs2Pathway::CreateMasteringObjects()
{
	// create mastering rack
	SceNgs2MasteringRackOption MasteringOption;
	sceNgs2MasteringRackResetOption(&MasteringOption);
	AudioDevice->ValidateAPICall(TEXT("sceNgs2SystemCreateWithAllocator_SCE_NGS2_RACK_ID_MASTERING"),
		sceNgs2RackCreateWithAllocator(Ngs2, SCE_NGS2_RACK_ID_MASTERING, &MasteringOption.rackOption, &AudioDevice->Ngs2Allocator, &MasteringRack));

	// get the mastering output voice
	AudioDevice->ValidateAPICall(TEXT("sceNgsRackGetVoiceHandle_MasteringRack"),
		sceNgs2RackGetVoiceHandle(MasteringRack, 0, &MasteringVoice));

	// set the number of channels
	AudioDevice->ValidateAPICall(TEXT("sceNgs2MasteringVoiceSetup"),
		sceNgs2MasteringVoiceSetup(MasteringVoice, ChannelCount, 0));

	// start outputting the master rack
	AudioDevice->ValidateAPICall(TEXT("sceNgs2VoiceKickEvent_MasteringPlay"),
		sceNgs2VoiceKickEvent(MasteringVoice, SCE_NGS2_VOICE_EVENT_PLAY));
}

void FNgs2Pathway::DestroyMasteringObjects()
{
	sceNgs2RackDestroy(MasteringRack, NULL);
}

FNgs2Pathway::~FNgs2Pathway()
{

	DestroyMasteringObjects();
	
	// shutdown the system
	sceNgs2SystemDestroy(Ngs2, NULL);
}

FAmbientPathway::FAmbientPathway(ENgs2Pathway::Type InPathwayName, FNgs2Device* InAudioDevice, SceUserServiceUserId UserIndex, uint32 InChannelCount, uint32 InOutputDestination)
	: FNgs2Pathway(InPathwayName, InAudioDevice, UserIndex, InChannelCount, InOutputDestination)
{	
	// create sampler rack
	SceNgs2SamplerRackOption SamplerOption;
	sceNgs2SamplerRackResetOption(&SamplerOption);

	// Make sure we can use the voice callbacks with the sampler rack, so we can feed more data to real-time decoded sources
	SamplerOption.rackOption.flags |= SCE_NGS2_RACK_OPTION_FLAG_CALLBACK;

	AudioDevice->ValidateAPICall(TEXT("sceNgs2RackCreateWithAllocator_SCE_NGS2_RACK_ID_SAMPLER"),
		sceNgs2RackCreateWithAllocator(Ngs2, SCE_NGS2_RACK_ID_SAMPLER, &SamplerOption.rackOption, &AudioDevice->GetNgs2Allocator(), &SamplerRack));

	// audio out parameters
	int32 ChannelsParam;
	int32 ChannelVolumeMask = 0;
	int32 NumChannels;

	// turn number of channels to an "enum"
	switch (ChannelCount)
	{
	case SCE_NGS2_CHANNELS_1_0CH:
		ChannelsParam = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_MONO;
		ChannelVolumeMask = SCE_AUDIO_VOLUME_FLAG_FL_CH;
		NumChannels = 1;
		break;
	case SCE_NGS2_CHANNELS_2_0CH:
		ChannelsParam = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_STEREO;
		ChannelVolumeMask = SCE_AUDIO_VOLUME_FLAG_FL_CH | SCE_AUDIO_VOLUME_FLAG_FR_CH;
		NumChannels = 2;
		break;
	case SCE_NGS2_CHANNELS_7_1CH:
		ChannelsParam = SCE_AUDIO_OUT_PARAM_FORMAT_FLOAT_8CH;
		ChannelVolumeMask =
			SCE_AUDIO_VOLUME_FLAG_FL_CH | SCE_AUDIO_VOLUME_FLAG_FR_CH |
			SCE_AUDIO_VOLUME_FLAG_CNT_CH | SCE_AUDIO_VOLUME_FLAG_LFE_CH |
			SCE_AUDIO_VOLUME_FLAG_RL_CH | SCE_AUDIO_VOLUME_FLAG_RR_CH |
			SCE_AUDIO_VOLUME_FLAG_BL_CH | SCE_AUDIO_VOLUME_FLAG_BR_CH;
		NumChannels = 8;
		break;
	default:
		UE_LOG(LogNgs2, Fatal, TEXT("Incorrect number of channels specified in FNgs2Pathway constructor"));
		return;
	}

	CreateMasteringObjects();
	
	// open up the final port
	PortID = sceAudioOutOpen(UserIndex, OutputDestination, 0, GrainSize, SampleRate, ChannelsParam);

	// check the return value
	if (PortID < 0)
	{
		AudioDevice->ValidateAPICall(TEXT("sceAudioOutOpenPort"), PortID);
	}	

	// For ambient pathway (2d sounds) we want to make sure there is no reverb added
// #if A3D
// 	bool bSuccessful = false;
// 	float ReverbLevel = 0.f;
// 	int err = sceAudio3dPortSetAttribute(PortID, SCE_AUDIO3D_PORT_ATTRIBUTE_LATE_REVERB_LEVEL, &ReverbLevel, sizeof(float));
// 	if (err == SCE_OK)
// 	{
// 		bSuccessful = true;
// 		sceAudio3dPortFlush(PortID);
// 		sceAudio3dPortAdvance(PortID);
// 		sceAudio3dPortPush(PortID, SCE_AUDIO3D_BLOCKING_ASYNC);
// 	}
// #endif

	// set the volume to full (per voice volume handled earlier)
	int32 Volumes[SCE_NGS2_MAX_VOICE_CHANNELS];
	for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
	{
		Volumes[ChannelIndex] = SCE_AUDIO_VOLUME_0dB;
	}
	AudioDevice->ValidateAPICall(TEXT("sceAudioOutSetVolume"),
		sceAudioOutSetVolume(PortID, ChannelVolumeMask, Volumes));
}

void FAmbientPathway::AudioThread_Render()
{
	// render the audio into an output buffer
	BufferInfo.buffer = Buffer[BufferIndex];
	int32 Res = sceNgs2SystemRender(Ngs2, &BufferInfo, 1);
	AudioDevice->ValidateAPICall(TEXT("sceNgs2SystemRender"),
		Res);

	// send it out
	Res = sceAudioOutOutput(PortID, Buffer[BufferIndex]);

	// Negative values indicate an error in this case, positive values indicate number of samples output
	if (Res < 0)
	{
		AudioDevice->ValidateAPICall(TEXT("sceAudioOutOutput"), Res);
	}

	// swap buffer
	BufferIndex = 1 - BufferIndex;
}

void FAmbientPathway::SetupVoice(FNgs2SoundSource& SoundSource)
{
	SceNgs2Handle VoiceHandle = SoundSource.GetVoice();
	FNgs2SoundBuffer* Buffer = (FNgs2SoundBuffer*)SoundSource.GetBufferNonConst();
	check(Buffer);

	const SceNgs2WaveformInfo& WaveformInfo = Buffer->WaveformInfo;

	if (WaveformInfo.format.numChannels > 0)
	{
		AudioDevice->ValidateAPICall(TEXT("sceNgs2SamplerVoiceSetup"),
			sceNgs2SamplerVoiceSetup(VoiceHandle, &WaveformInfo.format, 0));

		switch (Buffer->SoundFormat)
		{
			default:
			case SoundFormat_Invalid:
				checkf(false, TEXT("Unknown Ngs2 sound format"));
				break;

			case SoundFormat_PCM:
				SoundSource.SubmitPCMBuffers();
				break;

			case SoundFormat_PCMRT:
			case SoundFormat_Streaming:
				SoundSource.SubmitPCMRTBuffers();
				break;
		}
	}
}

FAmbientPathway::~FAmbientPathway()
{
	sceNgs2RackDestroy(SamplerRack, NULL);

	// shut down AudioOut
	sceAudioOutOutput(PortID, NULL);
	sceAudioOutClose(PortID);
}

#if A3D
F3DPathway::F3DPathway(ENgs2Pathway::Type InPathwayName, FNgs2Device* InAudioDevice, SceUserServiceUserId UserIndex, uint32 InChannelCount, uint32 InOutputDestination)
	: FNgs2Pathway(InPathwayName, InAudioDevice, UserIndex, InChannelCount, InOutputDestination)
{
	check(OutputDestination == SCE_AUDIO_OUT_PORT_TYPE_MAIN);

	SceNgs2BufferAllocator* Ngs2Allocator = &AudioDevice->GetNgs2Allocator();

	int32 Ret;

	// Create Audio 3D port	
	SceAudio3dOpenParameters Params;
	sceAudio3dGetDefaultOpenParameters(&Params);
	Params.eRate = SCE_AUDIO3D_RATE_48000;
	Params.uiGranularity = GrainSize;
	Params.uiMaxObjects = 512;
	Params.uiQueueDepth = 1;
	Params.eBufferMode = SCE_AUDIO3D_BUFFER_NO_ADVANCE;
	Params.szSizeThis = sizeof(Params);
	
	Ret = sceAudio3dPortOpen(
		SCE_USER_SERVICE_USER_ID_SYSTEM,
		&Params,
		&PortId);

	checkf(Ret == SCE_OK, TEXT("sceAudio3dPortOpenfailed: 0x%x\n"), Ret);

	// Turn off late reverb level
	float ReverbLevel = 0.0f;
	Ret = sceAudio3dPortSetAttribute(PortId, SCE_AUDIO3D_PORT_ATTRIBUTE_LATE_REVERB_LEVEL, &ReverbLevel, sizeof(float));
	checkf(Ret == SCE_OK, TEXT("sceAudio3dPortSetAttribute: 0x%x\n"), Ret);

	sceAudio3dPortFlush(PortId);
	sceAudio3dPortAdvance(PortId);
	sceAudio3dPortPush(PortId, SCE_AUDIO3D_BLOCKING_ASYNC);

	// Create NGS2 custom sampler UserFx2
	SceNgs2CustomSamplerRackOption CustomSamplerOption;
	SceNgs2CustomUserFx2ModuleOption A3dOptions;
	A3dOptions.customModuleOption.size = sizeof(A3dOptions);
	A3dOptions.workSize = sizeof(Audio3dUtils::A3dModuleWork);
	A3dOptions.paramSize = sizeof(Audio3dUtils::A3dModuleParam);
	A3dOptions.commonSize = 0;
	A3dOptions.userData = (uintptr_t)PortId;
	A3dOptions.processHandler = Audio3dUtils::A3dModuleProcess;
	A3dOptions.setupHandler = Audio3dUtils::A3dModuleSetup;
	A3dOptions.cleanupHandler = NULL;
	A3dOptions.controlHandler = NULL;

	sceNgs2CustomSamplerRackResetOption(&CustomSamplerOption);
	CustomSamplerOption.customRackOption.aModule[0].moduleId = SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2;
	CustomSamplerOption.customRackOption.aModule[0].stateOffset = 0;
	CustomSamplerOption.customRackOption.aModule[0].stateSize = 0;
	CustomSamplerOption.customRackOption.aModule[0].option = &A3dOptions.customModuleOption;
	CustomSamplerOption.customRackOption.numModules = 1;
	CustomSamplerOption.customRackOption.stateSize = sizeof(SceNgs2CustomSamplerVoiceState);

	// Make sure we can do voice callbacks for real-time decoded sounds
	CustomSamplerOption.customRackOption.rackOption.flags |= SCE_NGS2_RACK_OPTION_FLAG_CALLBACK;
 
	if (!AudioDevice->ValidateAPICall(TEXT("sceNgs2RackCreateWithAllocator"),
		sceNgs2RackCreateWithAllocator(Ngs2, SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &CustomSamplerOption.customRackOption.rackOption, Ngs2Allocator, &SamplerRack)))
	{
		checkf(false, TEXT("Sce Ngs2 create custom sampler for A3D failed! 0x%x\n"), Ret);
	}

	CreateMasteringObjects();
	
}

void F3DPathway::AudioThread_Render()
{
	// render the audio into an output buffer
	BufferInfo.buffer = Buffer[BufferIndex];

	// Rendering
	int32 Res = sceNgs2SystemRender(Ngs2, &BufferInfo, 1);
	AudioDevice->ValidateAPICall(TEXT("sceNgs2SystemRender"),
		Res);	
	
	// Write to Audio 3D bed
	// e.g. Ambient sounds.
	Res = sceAudio3dBedWrite(
		PortId,
		ChannelCount,
		SCE_AUDIO3D_FORMAT_FLOAT,
		BufferInfo.buffer,
		GrainSize);

	AudioDevice->ValidateAPICall(TEXT("sceAudio3dBedWrite"),
		Res);	
	
	// Flush Audio 3D port
	Res = sceAudio3dPortFlush(PortId);
	AudioDevice->ValidateAPICall(TEXT("sceAudio3dPortFlush"),
		Res);	

	// swap buffer
	BufferIndex = 1 - BufferIndex;
}

void F3DPathway::SetupVoice(FNgs2SoundSource& SoundSource)
{
	check(!SoundSource.GetVoiceReady());

	SceNgs2Handle VoiceHandle = SoundSource.GetVoice();
	FNgs2SoundBuffer* SoundBuffer = (FNgs2SoundBuffer*)SoundSource.GetBufferNonConst();
	check(Buffer);

	const SceNgs2WaveformInfo& WaveformInfo = SoundBuffer->WaveformInfo;

	if (WaveformInfo.format.numChannels > 0)
	{
		AudioDevice->ValidateAPICall(TEXT("sceNgs2CustomSamplerVoiceSetup"),
			sceNgs2CustomSamplerVoiceSetup(VoiceHandle, &WaveformInfo.format, 0));

		switch (SoundBuffer->SoundFormat)
		{
			default:
			case SoundFormat_Invalid:
				checkf(false, TEXT("Unknown Ngs2 sound format"));
				break;

			case SoundFormat_PCM:
				SoundSource.SubmitPCMBuffers();
				break;

			case SoundFormat_PCMRT:
			case SoundFormat_Streaming:
				SoundSource.SubmitPCMRTBuffers();
				break;
		}
	}
}


F3DPathway::~F3DPathway()
{	
	sceNgs2RackDestroy(SamplerRack, NULL);

	int32 Ret;

	// Close Audio 3D port
	if (PortId != SCE_AUDIO3D_PORT_INVALID)
	{
		Ret = sceAudio3dPortClose(PortId);
		checkf(Ret == SCE_OK, TEXT("sceAudio3dPortClose failed: 0x%x"), Ret);		
		PortId = SCE_AUDIO3D_PORT_INVALID;
	}
}
#endif

/*------------------------------------------------------------------------------------
	FAudioDevice Interface.
------------------------------------------------------------------------------------*/

/**
 * Route Ngs allocations through FMemory::Malloc
 */
static int32 NgsAlloc(SceNgs2ContextBufferInfo* BufferInfo)
{
	BufferInfo->hostBuffer = FMemory::Malloc(BufferInfo->hostBufferSize);
	if (BufferInfo->hostBuffer)	{
		FMemory::Memzero(BufferInfo->hostBuffer, BufferInfo->hostBufferSize);
	}
	return BufferInfo->hostBuffer ? SCE_OK : SCE_NGS2_ERROR_EMPTY_BUFFER;
}

/**
 * Route Ngs deallocations throuGetIOSgh FMemory::Free
 */
static int32_t NgsFree(SceNgs2ContextBufferInfo *BufferInfo)
{
	FMemory::Free(BufferInfo->hostBuffer);
	return SCE_OK;
}

bool FNgs2Device::InitializeHardware()
{
	sceSysmoduleLoadModule( SCE_SYSMODULE_NGS2 );
	FAtrac9DecoderModule::InitializeAjm();

	// Initialize Audio output, if it isn't already
	int32 Result = sceAudioOutInit();
	if (Result < 0 && Result != SCE_AUDIO_OUT_ERROR_ALREADY_INIT)
	{
		// We failed for any reason other than the output system has already been initialized
		checkf(false, TEXT("sceAudioOutInit failed in FNgs2Device::InitializeHardware(). Error code 0x%x"), Result);
		return false;
	}

	// try for surround, let the hardware downmix if necessary.
	bUseSurroundSound = true;

	// setup allocator wrapper
	Ngs2Allocator.allocHandler = NgsAlloc;
	Ngs2Allocator.freeHandler = NgsFree;
	Ngs2Allocator.userData = 0;


	// null out the pathway pointers
	FMemory::Memzero(Pathways, sizeof(Pathways));

	ValidateAPICall(TEXT("sceNgs2PanInit"),
		sceNgs2PanInit(&PanningData, NULL, SCE_NGS2_PAN_ANGLE_DEGREE, bUseSurroundSound ? SCE_NGS2_PAN_SPEAKER_7_0CH : SCE_NGS2_PAN_SPEAKER_2_0CH));

	// start the audio rendering thread
	AudioRunnable = new FNgs2AudioRenderingThread(this);
	AudioThread = FRunnableThread::Create(AudioRunnable, TEXT("Ngs2AudioRenderingThread"), 0, TPri_AboveNormal, FPlatformAffinity::GetAudioThreadMask());

#if A3D
	// Initialize Audio 3D service
	Audio3d.Init();
#endif

	// no effects by default
	ReverbVoice = SCE_NGS2_HANDLE_INVALID;
	EQVoice = SCE_NGS2_HANDLE_INVALID;

	// Initialize Sulpha for host side audio debugging
	Sulpha.Init();

	return true;
}

void FNgs2Device::TeardownHardware()
{
	// Shutdown the at9 decoder module
	FAtrac9DecoderModule::ShutdownAjm();

	Sulpha.Shutdown();

	// stop the rendering thread and wait for completion
	if (AudioThread)
	{
		AudioThread->Kill(true);
		delete AudioThread;
	}
	delete AudioRunnable;

	// destroy the pathways
	for (int32 PathwayIndex = 0; PathwayIndex < ENgs2Pathway::MAX; PathwayIndex++)
	{
		delete Pathways[PathwayIndex];
	}

}

void FNgs2Device::UpdateHardware()
{
	// Caches the matrix used to transform a sounds position into local space so we can just look
	// at the Y component after normalization to determine spatialization.
	const FListener& Listener = GetListeners()[0];
	const FVector Up = Listener.GetUp();
	const FVector Right = Listener.GetFront();
	InverseTransform = FMatrix( Up, Right, Up ^ Right, Listener.Transform.GetTranslation() ).InverseFast();
}

class ICompressedAudioInfo* FNgs2Device::CreateCompressedAudioInfo(USoundWave* SoundWave)
{
	check(SoundWave);

	// Return an opus compressed audio info for streaming sounds
	if (SoundWave->IsStreaming())
	{
		return FAtrac9DecoderModule::CreateCompressedAudioInfo();
	}

	// No need to create a compressed audio info for non-streaming AT9 files since Ngs2 handles realtime decoding automatically
	check(false);
	return nullptr;
}

FAudioEffectsManager* FNgs2Device::CreateEffectsManager()
{
	// Create the effects subsystem (reverb, EQ, etc.)
	return new FNgs2EffectsManager(this, bUseSurroundSound ? SCE_NGS2_CHANNELS_7_1CH : SCE_NGS2_CHANNELS_2_0CH);
}

FSoundSource* FNgs2Device::CreateSoundSource()
{
	static int32 NextVoiceIndex = 1;

	// create source source object
	return new FNgs2SoundSource(this, NextVoiceIndex++);
}

/**  
 * Check for errors and output a human readable string 
 */
bool FNgs2Device::ValidateAPICall(const TCHAR* Function, uint32 ErrorCode)
{
	// Ngs2 error codes are almost always positive!
	if (ErrorCode != SCE_OK)
	{
		UE_LOG(LogAudio, Warning, TEXT("%s error 0x%08x"), Function, ErrorCode);
		return false;
	}

	return true;
}

void FNgs2Device::Precache(USoundWave* SoundWave, bool bSynchronous, bool bTrackMemory, bool bForceFullDecompression)
{
	FAudioDevice::Precache(SoundWave, bSynchronous, bTrackMemory);

	// Do not preload streaming bulk data
	if (SoundWave->DecompressionType != DTYPE_Streaming && SoundWave->DecompressionType != DTYPE_Procedural)
	{
		// preload bulk data to avoid hitches
		FNgs2SoundBuffer::Init(this, SoundWave);
	}
}

FNgs2Pathway* FNgs2Device::GetPathway(ENgs2Pathway::Type Pathway, int32 UserIndex)
{
	checkf(Pathway < ENgs2Pathway::MAX, TEXT("Invalid pathway sent to FNgs2Device::GetPathway"));
	if (UserIndex >= SCE_USER_SERVICE_MAX_LOGIN_USERS)
	{
		UE_LOG(LogNgs2, Log, TEXT("Playing audio for invalid user through TV pathway."));
		UserIndex = 0;
		Pathway = ENgs2Pathway::TV;
	}
	int32 PathwayIndex = 0;
	switch (Pathway)
	{
	// Normal pathway (sampler rack) for Ngs2
	case ENgs2Pathway::TV:
		PathwayIndex = ENgs2Pathway::TV;
		break;
	// Special A3D pathway for the Audio3D library. Note: we create both pathways to support non-A3D spatialized sounds and features.
	case ENgs2Pathway::TV_A3D:
		PathwayIndex = ENgs2Pathway::TV_A3D;
		break;
	case ENgs2Pathway::Controller:
		check(UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS);
		PathwayIndex = ENgs2Pathway::Controller0 + UserIndex;
		break;
	default:
		check(false);
	}
	
	// initialize if needed
	if (Pathways[PathwayIndex] == NULL)
	{
		switch (Pathway)
		{
			case ENgs2Pathway::TV:
				Pathways[PathwayIndex] = new FAmbientPathway(Pathway, this, SCE_USER_SERVICE_USER_ID_SYSTEM, bUseSurroundSound ? SCE_NGS2_CHANNELS_7_1CH : SCE_NGS2_CHANNELS_2_0CH, SCE_AUDIO_OUT_PORT_TYPE_MAIN);
				break;

			case ENgs2Pathway::TV_A3D:
#if A3D && USING_A3D
				Pathways[PathwayIndex] = new F3DPathway(Pathway, this, SCE_USER_SERVICE_USER_ID_SYSTEM, bUseSurroundSound ? SCE_NGS2_CHANNELS_7_1CH : SCE_NGS2_CHANNELS_2_0CH, SCE_AUDIO_OUT_PORT_TYPE_MAIN);
#else
				check(false); // Shouldn't hit this if we're not compiled with A3D
#endif
				break;

			case ENgs2Pathway::Controller:
				{
					// PS4Application has the master mapping from UserIndex -> UserID.  You can't get it from the SceUserServiceLoginUserIdList
					// because it gets remapped behind the scenes as users log in/out.
					verify(UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS);
					const FPS4Application* PS4Application = FPS4Application::GetPS4Application();
					int32 UserID = PS4Application->GetUserID(UserIndex);
					
					Pathways[PathwayIndex] = new FAmbientPathway(Pathway, this, UserID, SCE_NGS2_CHANNELS_1_0CH, SCE_AUDIO_OUT_PORT_TYPE_PADSPK);
				}
				break;
		}
	}

	// now return it
	return Pathways[PathwayIndex];
}

void FNgs2Device::AudioRenderThreadCommand(TFunction<void()> Command)
{
	AudioRenderThreadCommandQueue.Enqueue(MoveTemp(Command));
}

void FNgs2Device::AddPendingCleanupTask(const FPendingDecodeTaskCleanup& PendingDecodeTaskCleanup)
{
	PendingAudioDecodeTasksQueue.Enqueue(PendingDecodeTaskCleanup);
}

void FNgs2Device::AudioRenderThreadUpdate()
{
	// Pump audio render thread command queue
	TFunction<void()> Command;
	while (AudioRenderThreadCommandQueue.Dequeue(Command))
	{
		Command();
	}

	// Pump the thread safe transfer queue
	FPendingDecodeTaskCleanup PendingDecodeTaskCleanup;
	while (PendingAudioDecodeTasksQueue.Dequeue(PendingDecodeTaskCleanup))
	{
		PendingAudioDecodeTasksArray.Add(PendingDecodeTaskCleanup);
	}

	// Iterate through pending decode tasks and find finished tasks. Don't force them to finish.
	for (int32 i = PendingAudioDecodeTasksArray.Num() - 1; i >= 0; --i)
	{
		// If done
		if (PendingAudioDecodeTasksArray[i].RealtimeAsyncTask->IsDone())
		{
			// cleanup
			delete PendingAudioDecodeTasksArray[i].RealtimeAsyncTask;
			delete PendingAudioDecodeTasksArray[i].Buffer;

			PendingAudioDecodeTasksArray.RemoveAtSwap(i, 1, false);
		}
	}
}

/*------------------------------------------------------------------------------------
	FNgs2AudioRenderingThread
------------------------------------------------------------------------------------*/

bool FNgs2AudioRenderingThread::Init()
{ 
	return true;
}

void FNgs2AudioRenderingThread::Exit()
{
}

uint32 FNgs2AudioRenderingThread::Run()
{
	LLM_SCOPE(ELLMTag::Audio);

	// loop until we are done
	while (!bShouldStopThread)
	{
		for (int32 PathwayIndex = 0; PathwayIndex < ENgs2Pathway::MAX; PathwayIndex++)
		{
			// if the pathway exists, we can use it
			FNgs2Pathway* Pathway = AudioDevice->Pathways[PathwayIndex];
			if (Pathway)
			{
				Pathway->AudioThread_Render();
			}
		}

		// After processing ngs2 render, call into audio device with an audio render thread update to allow us to do any render-thread tasks
		AudioDevice->AudioRenderThreadUpdate();
	}

	return 0;
}