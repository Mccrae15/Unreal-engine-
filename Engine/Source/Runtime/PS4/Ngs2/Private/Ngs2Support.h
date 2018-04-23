// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"

// Number of buffers used for realtime decoding
#define NGS2_AUDIO_REALTIME_BUFFER_COUNT (3)

#define NGS2_MONO_PCM_BUFFER_SIZE (MONO_PCM_BUFFER_SIZE)

/**
 * Ngs2 implementation of a FSoundBuffer, containing the wave data and format information.
 */
class FNgs2SoundBuffer : public FSoundBuffer
{
public:
	/** 
	 * Constructor
	 *
	 * @param AudioDevice	audio device this sound buffer is going to be attached to.
	 */
	FNgs2SoundBuffer(FAudioDevice* AudioDevice, ESoundFormat InSoundFormat);
	
	/**
	 * Destructor 
	 * 
	 * Frees wave data and detaches itself from audio device.
	 */
	virtual ~FNgs2SoundBuffer();

	//~ Begin FSoundBuffer interface
	int32 GetSize() override;
	int32 GetCurrentChunkIndex() const override;
	int32 GetCurrentChunkOffset() const override;
	bool IsRealTimeSourceReady() override;
	void EnsureRealtimeTaskCompletion() override;
	bool ReadCompressedInfo(USoundWave* SoundWave) override;
	bool ReadCompressedData(uint8* Destination, bool bLooping) override;
	void Seek(const float SeekTime) override;
	//~ End FSoundBuffer interface

	/**
	 * Static function used to create a buffer.
	 *
	 * @param InWave USoundNodeWave to use as template and wave source
	 * @param AudioDevice audio device to attach created buffer to
	 * @return FNgs2SoundBuffer pointer if buffer creation succeeded, NULL otherwise
	 */
	static FNgs2SoundBuffer* Init(FAudioDevice* InAudioDevice, USoundWave* InSoundWave);

	static FNgs2SoundBuffer* CreateNativeBuffer(FAudioDevice* InAudioDevice, USoundWave* InSoundWave);
	static FNgs2SoundBuffer* CreateProceduralBuffer(FAudioDevice* InAudioDevice, USoundWave* InSoundWave);
	static FNgs2SoundBuffer* CreateStreamingBuffer(FAudioDevice* InAudioDevice, USoundWave* InSoundWave);

	const ESoundFormat GetFormat() const { return SoundFormat; }

	void InitWaveformInfo(USoundWave* InSoundWave);

	void SetMonoPCMBufferSize(const uint32 InMonoPCMBufferSize) { MonoPCMBufferSize = InMonoPCMBufferSize; }

	uint32 GetMonoPCMBufferSize() const { return MonoPCMBufferSize; }

protected:

	/** Format of the sound referenced by this buffer */
	ESoundFormat SoundFormat;

	/** Waveform descriptor */
	SceNgs2WaveformInfo WaveformInfo;

	/** Wrapper to handle the decompression of audio codecs (where applicable) */
	class ICompressedAudioInfo* DecompressionState;

	/** Async task for parsing real-time decompressed compressed info headers */
	FAsyncRealtimeAudioTaskProxy<FNgs2SoundBuffer>* RealtimeAsyncHeaderParseTask;

	/** Indicates that the real-time decompressed header is ready */
	FThreadSafeBool bRealTimeSourceReady;

	/** In-memory copy of the sound (a TArray for auto-memory management ;)) */
	TArray<uint8> AT9Data;

	TArray<uint8> AudioData;

	/** Number of blocks in use in the WaveFormInfoBlocks array */
	uint32 NumWaveFormInfoBlocks;

	/** Number of samples to decode per callback if this sound buffer is a real-time decoded sound. */
	uint32 MonoPCMBufferSize;

	/** Local copy of the Waveform Info block from the asset so that this sound source can modify it */
	SceNgs2WaveformBlock WaveFormInfoBlocks[SCE_NGS2_WAVEFORM_INFO_MAX_BLOCKS];

	/** Initial number of WaveFormInfoBlocks. Parsed in Ngs2Buffer::Init. */
	uint32 NumWaveFormInfoBlocksHeader;

	/** Cached copy of the initial state of the waveform used for seeking and looping through the waveform. */
	SceNgs2WaveformBlock WaveFormInfoBlocksHeader[SCE_NGS2_WAVEFORM_INFO_MAX_BLOCKS];

	friend class FNgs2SoundSource;
	friend class FAmbientPathway;
	friend class F3DPathway;
};


typedef FAsyncRealtimeAudioTaskProxy<FNgs2SoundBuffer> FAsyncRealtimeAudioTask;

// Used to enqueue pending decode cleanup tasks. Allows cleanup within same thread as ngs2 render update.
struct FPendingDecodeTaskCleanup
{
	FAsyncRealtimeAudioTask* RealtimeAsyncTask;
	FNgs2SoundBuffer* Buffer;
};

/**
 * Ngs2 implementation of FSoundSource, the interface used to play, stop and update sources
 */
class FNgs2SoundSource : public FSoundSource
{
public:
	/**
	 * Constructor
	 *
	 * @param	InAudioDevice	audio device this source is attached to
	 * @param	VoiceIndex		the index for this source (can never change)
	 */
	FNgs2SoundSource(FAudioDevice* InAudioDevice, int32 InVoiceIndex);

	/**
	 * Destructor, cleaning up voice
	 */
	virtual ~FNgs2SoundSource();

	/**
	 * Frees existing resources. Called from destructor and therefore not virtual.
	 */
	void FreeResources();

	//~ Begin FSoundSource
	virtual bool PrepareForInitialization(FWaveInstance* InWaveInstance) override;
	virtual bool IsPreparedToInit() override;
	virtual bool Init(FWaveInstance* WaveInstance) override;
	virtual void Update() override;
	virtual void Play() override;
	virtual void Stop() override;
	virtual void Pause() override;
	virtual bool IsFinished() override;
	//~ End FSoundSource

	/**
	* Create a new source voice
	*/
	bool CreateSource(const bool bUseSpatialization);

	void HandleRealTimeSource(const bool bLooped);

	FSoundBuffer* GetBufferNonConst() const { return Buffer; }

	/**
	 * Describe the buffer (platform can override to add to the description, but should call the base class version)
	 * 
	 * @param bUseLongNames If TRUE, this will print out the full path of the sound resource, otherwise, it will show just the object name
	 */
	virtual FString Describe(bool bUseLongName) override;

	SceNgs2Handle GetVoice() const
	{
		return Voice;
	}

	void SetVoiceReady(bool bReady)
	{
		bVoiceSetup = bReady;
	}

	bool GetVoiceReady() const
	{
		return bVoiceSetup;
	}

	void PatchVoice(FNgs2Pathway* Pathway);	

	/** Called when a streamed source needs more audio. */
	void OnBufferEnd(uint32 InVoiceId);

	void SubmitPCMBuffers();
	void SubmitPCMRTBuffers();

protected:

	enum class EDataReadMode : uint8
	{
		Synchronous,
		Asynchronous,
		AsynchronousSkipFirstFrame
	};

	bool ReadMorePCMData(const int32 BufferIndex, const EDataReadMode DataReadMode, const int32 InVoiceId);
	int32 SubmitData(SceNgs2Handle Voice, uint8* DataBuffer, const SceNgs2WaveformBlock* WaveformBlock, const int32 NumWaveFormInfoBlocks, const uint32 Flags);
	void HandleLooping();

	/** Owning classes */
	FNgs2Device* AudioDevice;
	FNgs2EffectsManager* Effects;

	/** Cache the typecasted pointer */
	FNgs2SoundBuffer* Ngs2Buffer;

	/** This is the voice handle from the sampler rack for this sound source */
	SceNgs2Handle Voice;

	int32 VoiceIndex;

#if A3D
	Audio3dUtils::A3dModuleParam A3dParams;
#endif	

	/** Number of times this sound source has looped */
	uint32 TotalLoopCount;

	/** Asynchronous task for real time audio decoding */
	FAsyncRealtimeAudioTask* RealtimeAsyncTask;

	/** Whether or not the sound has finished playing */
	FThreadSafeBool bIsFinished;

	/** Set when we wish to let the buffers play themselves out */
	FThreadSafeBool bBuffersToFlush;

	/** Whether or not this source looped */
	FThreadSafeBool bLooped;

	/** Set to true when the loop end callback is hit */
	FThreadSafeBool bLoopCallback;

	/** Whether or not we played a cached buffer */
	FThreadSafeBool bPlayedCachedBuffer;

	/** Struct to wrap information about Ngs2 streamed buffers. */
	struct FRealtimeAudioData
	{
		/** Ngs2 data about the audio buffer */
		SceNgs2WaveformBlock WaveformBlock;

		/** Decoded/Generated PCM audio data. */
		TArray<uint8> AudioData;

		/** Constructor */
		FRealtimeAudioData()
		{
			FMemory::Memzero(&WaveformBlock, sizeof(WaveformBlock));
		}

		/** Helper function to set the waveform block to the number of bytes written */
		void SetBytesWritten(const int32 BytesWritten)
		{
			WaveformBlock.dataSize = BytesWritten;
			WaveformBlock.numSamples = BytesWritten / sizeof(int16);
			WaveformBlock.numSkipSamples = 0;
			WaveformBlock.dataOffset = 0;
		}
	};

	/** Triple buffer of audio data */
	FRealtimeAudioData RealtimeBufferData[NGS2_AUDIO_REALTIME_BUFFER_COUNT];
	/** Index to current buffer */
	int32 CurrentBuffer;
	/** Set to true when we've allocated resources that need to be freed */
	uint8 bResourcesNeedFreeing:1;
	/** Whether or not this source is using the A3D library (mono spatialized and A3D is enabled) */
	uint8 bIsA3dSound:1;
	/** Whether or not this source has been "setup" */
	uint8 bVoiceSetup:1;
	/** Critical section to protect from the OnBufferEnd callback and stopping a source voice */
	FCriticalSection VoiceCallbackCriticalSection;

	/** To avoid handling callbacks for submit requests done with this sound source before it was recycled for a new one, we pass through an Id to identify it should be the same sound. */
	uint32 VoiceId;
	static uint32 NextVoiceId;
};
