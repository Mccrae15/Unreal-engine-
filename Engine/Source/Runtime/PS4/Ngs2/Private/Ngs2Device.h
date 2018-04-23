// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioDevice.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include <ngs2.h>
#include <user_service.h>
#include "Ngs2A3d.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNgs2, Log, All);

//
// Forward declarations.
//
class FNgs2AudioRenderingThread;
class FNgs2SoundSource;
struct FPendingDecodeTaskCleanup;

/**
 * Host side audio debugging using Sulpha
 */
class FSulpha
{
public:
	FSulpha() : SulphaBuffer( NULL )
	{
	}

	void Init();
	void Shutdown();

private:
	void* SulphaBuffer;
};

namespace ENgs2Pathway
{
	enum Type
	{
		// Normal pathway for Ngs2
		TV,
		// Special pathway for A3D custom sampler rack
		TV_A3D,
		// Controller pathways for up to 4 controllers
		Controller,
		Controller0 = Controller,
		Controller1,
		Controller2,
		Controller3,
		MAX,
	};
}


class FNgs2Pathway
{
public:
	FNgs2Pathway(ENgs2Pathway::Type InPathwayName, class FNgs2Device* InAudioDevice, SceUserServiceUserId UserIndex, uint32 InChannelCount, uint32 InOutputDestination);
	virtual ~FNgs2Pathway();

	virtual void AudioThread_Render() = 0;
	virtual void SetupVoice(FNgs2SoundSource& SoundSource) = 0;

	/** The Ngs2 system object */
	SceNgs2Handle Ngs2;

	/** The output racks */
	SceNgs2Handle SamplerRack;
	SceNgs2Handle MasteringRack;
	/** Final output voice */
	SceNgs2Handle MasteringVoice;

	ENgs2Pathway::Type PathwayName;

protected:

	void CreateMasteringObjects();
	void DestroyMasteringObjects();

	uint32 ChannelCount;
	uint32 OutputDestination;	

	/** Double buffered renderer output */
	int32 BufferIndex = 0;
#if USE_7TH_CORE && 0
	float Buffer[2][(SCE_NGS2_MAX_GRAIN_SAMPLES * SCE_NGS2_MAX_VOICE_CHANNELS)*2];
#else
	float Buffer[2][SCE_NGS2_MAX_GRAIN_SAMPLES * SCE_NGS2_MAX_VOICE_CHANNELS];
#endif

	/** Render buffer descriptor */
	SceNgs2RenderBufferInfo BufferInfo;

	/** Cache the device */
	class FNgs2Device* AudioDevice;

};

/** Everything needed for a unique pathway (ie TV speakers vs controller speaker) */
/** 'Ambient' because raw NGS2 doesn't perform positional panning or gain operations. */
class FAmbientPathway : public FNgs2Pathway
{
public:
	FAmbientPathway(ENgs2Pathway::Type InPathwayName, class FNgs2Device* InAudioDevice, SceUserServiceUserId UserIndex, uint32 InChannelCount, uint32 InOutputDestination);
	virtual ~FAmbientPathway();

	/**
	 * Run the system and output to the final output device
	 */
	void AudioThread_Render();
	virtual void SetupVoice(FNgs2SoundSource& SoundSource) override;

private:
	
	/** AudioOut port ID */
	int32 PortID;	
};

#if A3D
class F3DPathway : public FNgs2Pathway
{
public:

	F3DPathway(ENgs2Pathway::Type InPathwayName, class FNgs2Device* InAudioDevice, SceUserServiceUserId UserIndex, uint32 InChannelCount, uint32 InOutputDestination);
	virtual ~F3DPathway();

	void AudioThread_Render();
	virtual void SetupVoice(FNgs2SoundSource& SoundSource) override;

private:

	/** Double buffered renderer output */
	int32 BufferIndex = 0;
#if USE_7TH_CORE
	float Buffer[2][(SCE_NGS2_MAX_GRAIN_SAMPLES * SCE_NGS2_MAX_VOICE_CHANNELS)*2];
#else
	float Buffer[2][SCE_NGS2_MAX_GRAIN_SAMPLES * SCE_NGS2_MAX_VOICE_CHANNELS];
#endif

	/** Audio 3D port ID */
	SceAudio3dPortId PortId;		

};
#endif

enum ESoundFormat
{
	SoundFormat_Invalid,		// Invalid sound format
	SoundFormat_PCM,			// Compressed AT9 files (native PS4 format)
	SoundFormat_PCMRT,			// Real-time decoded AT9 FIles
	SoundFormat_Streaming,		// Streamed, real-time decoded AT9 files
};


/**
 * Ngs2 implementation of an Unreal audio device.
 */
class FNgs2Device : public FAudioDevice
{
public:
	/** Starts up any platform specific hardware/APIs */
	virtual bool InitializeHardware() override;

	/** Shuts down any platform specific hardware/APIs */
	virtual void TeardownHardware() override;

	/** Lets the platform any tick actions */
	virtual void UpdateHardware() override;

	/** Creates a new platform specific sound source */
	virtual FAudioEffectsManager* CreateEffectsManager() override;

	/** Creates a new platform specific sound source */
	virtual FSoundSource* CreateSoundSource() override;

	virtual FName GetRuntimeFormat(USoundWave* SoundWave) override
	{
		static FName NAME_AT9(TEXT("AT9"));
		return NAME_AT9;
	}
	
	virtual bool HasCompressedAudioInfoClass(USoundWave* SoundWave) override
	{
		// only streaming sound waves have a compressed info class that isn't native
		return SoundWave->IsStreaming();
	}

	virtual bool SupportsRealtimeDecompression() const override
	{
		// Ngs2 internally performs real-time decompresses.
		return false;
	}

	virtual class ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* SoundWave) override;

	/** 
	 * Check for errors and output a human readable string 
	 */
	virtual bool ValidateAPICall(const TCHAR* Function, uint32 ErrorCode) override;

	/** Preloads the sound bulk data into memory */
	virtual void Precache(USoundWave* SoundWave, bool bSynchronous, bool bTrackMemory, bool bForceFullDecompression) override;

	/** Retrieve (and possibly create) the pathway for the given enum value */
	FNgs2Pathway* GetPathway(ENgs2Pathway::Type Pathway, int32 UserIndex);

	/** Retrieve data for computing panning */
	SceNgs2PanWork& GetPanningData() { return PanningData; }

	bool GetUseSurroundSound() { return bUseSurroundSound; }

#if A3D
	Audio3dService& GetAudio3DService() { return Audio3d; }
#endif

	SceNgs2BufferAllocator& GetNgs2Allocator() { return Ngs2Allocator; }
	
	// Enqueues an audio render thread command which will be executed on the audio render thread
	void AudioRenderThreadCommand(TFunction<void()> Command);

	// Adds a pending cleanup task to the audio device. Is cleaned up in the audio render thread update.
	void AddPendingCleanupTask(const FPendingDecodeTaskCleanup& PendingDecodeTaskCleanup);

	// Called at the end of the audio render thread, after updating Ngs2 system
	void AudioRenderThreadUpdate();

protected:

#if A3D
	Audio3dService Audio3d;
#endif
	
	bool bUseSurroundSound;

	/** Inverse listener transformation, used for spatialization */
	FMatrix								InverseTransform;

	SceNgs2PanWork						PanningData;

	/** All audio pathways */
	FNgs2Pathway* Pathways[ENgs2Pathway::MAX];

	/** The wrapper struct to route allocations to our allocators */
	SceNgs2BufferAllocator Ngs2Allocator;


	/** Cached handles set by the effects object if effects are setup (these are SCE_NGS2_HANDLE_INVALID if not set) */
	SceNgs2Handle ReverbVoice;
	SceNgs2Handle EQVoice;

	/** Audio rendering thread */
	FRunnableThread* AudioThread;
	FNgs2AudioRenderingThread* AudioRunnable;

	/** Host side audio debugging */
	FSulpha	Sulpha;

	// Queue of commands which will be excuted on the audio render thread
	TQueue<TFunction<void()>> AudioRenderThreadCommandQueue;

	// Transfer queue, used to queue up pending cleanup tasks
	TQueue<FPendingDecodeTaskCleanup> PendingAudioDecodeTasksQueue;
	
	// Array which is evaluated in audio render thread, tasks are queried if they're finished and then cleaned up.
	TArray<FPendingDecodeTaskCleanup> PendingAudioDecodeTasksArray;

	friend class FNgs2SoundBuffer;
	friend class FNgs2SoundSource;
	friend class FNgs2EffectsManager;
	friend class FNgs2AudioRenderingThread;
	friend class FNgs2Pathway;
};


/** The audio rendering thread object */
class FNgs2AudioRenderingThread : public FRunnable
{
public:

	/**
	 * Constructor
	 */
	FNgs2AudioRenderingThread(FNgs2Device* InAudioDevice)
		: AudioDevice(InAudioDevice)
		, bShouldStopThread(false)
	{

	}

	// FRunnable interface.
	virtual bool Init() override;

	virtual void Exit() override;

	virtual void Stop() override
	{
		bShouldStopThread = true;
	}

	virtual uint32 Run() override;

private:

	/** Cached device object */
	FNgs2Device* AudioDevice;

	/** Flag to tell thread to stop */
	bool bShouldStopThread;
};
