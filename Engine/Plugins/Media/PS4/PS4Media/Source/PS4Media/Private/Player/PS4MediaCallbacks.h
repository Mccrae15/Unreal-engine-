// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Queue.h"
// begin missing includes for GnmRHI.h
#include "RHI.h"
#include "RHIResources.h"
#include "ShaderCore.h"
// end missing includes for GnmRHI.h
#include "GnmRHI.h"
#include "IMediaEventSink.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"

#include <sceavplayer.h>

class FArchive;
class FMediaSamples;
class FPS4MediaAudioSamplePool;
class FPS4MediaTextureSamplePool;

struct SceAvPlayerFrameInfo;
struct SceAvPlayerFrameInfoEx;


/**
 * Implements a callback handler for PS4 media players.
 */
class FPS4MediaCallbacks
{
public:

	/** Default constructor. */
	FPS4MediaCallbacks(const TSharedRef<FMediaSamples, ESPMode::ThreadSafe>& InSamples);

	/** Destructor. */
	~FPS4MediaCallbacks();

public:

	/**
	 * Get the currently used archive, if any.
	 *
	 * @param The archive, or nullptr if no archive is used.
	 * @see GetEvents, GetHandle
	 */
	TSharedPtr<FArchive, ESPMode::ThreadSafe> GetArchive()
	{
		return Archive;
	}

	/**
	 * Gets all deferred player events.
	 *
	 * @param OutEvents Will contain the events.
	 * @see GetArchive, GetHandle
	 */
	void GetEvents(TArray<EMediaEvent>& OutEvents);

	/**
	 * Get the native AvPlayer handle.
	 *
	 * @return The handle.
	 * @see GetArchive, GetEvents
	 */
	SceAvPlayerHandle GetHandle()
	{
		return Handle;
	}

	/**
	 * Initialize this object.
	 *
	 * @param InArchive The archive to read media data from (optional).
	 * @param Url The media source URL.
	 * @param Precache Whether to precache media into RAM if InURL is a local file.
	 * @param NumVideoBuffers The number of video buffers to use for decoding.
	 * @param VideoBufferSizeMB The size of the video buffers (in megabytes).
	 */
	void Initialize(TSharedPtr<FArchive, ESPMode::ThreadSafe> InArchive, const FString& Url, bool Precache, int32 NumVideoBuffers, int32 VideoBufferSizeMB);

	/** Restart stream sampling. */
	void Restart();

	/**
	 * Tick audio sample processing.
	 *
	 * @param Rate The current playback rate.
	 * @param Time The current presentation time.
	 * @see TickVideo
	 */
	void TickAudio(float Rate, FTimespan Time);

	/**
	 * Tick video sample processing.
	 *
	 * @param Rate The current playback rate.
	 * @param Time The current presentation time.
	 * @note Must be called on render thread.
	 * @see TickAudio
	 */
	void TickVideo(float Rate, FTimespan Time);

protected:

	/**
	 * Find a cached RHI texture for the specified GNM texture.
	 *
	 * @param GnmTexture The GNM texture.
	 * @param Width The expected width of the texture (in pixels).
	 * @param Height The expected height of the texture (in pixels).
	 * @param Luma Whether this is the luma texture (or chroma otherwise).
	 * @return The corresponding RHI texture.
	 */
	TRefCountPtr<FRHITexture> FindOrCreateRHITexture(const sce::Gnm::Texture& GnmTexture, uint32 Width, uint32 Height, bool Luma);

	/**
	 * Initialize the native AvPlayer handle.
	 *
	 * @param NumVideoBuffers The number of video buffers to use for decoding.
	 * @param VideoBufferSizeMB The size of the video buffers (in megabytes).
	 * @return true on success, false otherwise.
	 */
	bool InitializeHandle(int32 NumVideoBuffers, int32 VideoBufferSizeMB);

	/**
	 * Initialize media source.
	 *
	 * @param InArchive The archive to read media data from (optional).
	 * @param Url The media source URL.
	 * @param Precache Whether to precache media into RAM if InURL is a local file.
	 * @return true on success, false otherwise.
	 */
	bool InitializeSource(TSharedPtr<FArchive, ESPMode::ThreadSafe> InArchive, const FString& Url, bool Precache);

	/**
	 * Process audio.
	 *
	 * @param FrameInfo The frame information containing the audio data.
	 * @see ProcessTimedText, ProcessVideo
	 */
	void ProcessAudio(SceAvPlayerFrameInfo& FrameInfo);

	/**
	 * Process an event from the native player.
	 *
	 * @param EventId The ID of the event to process.
	 * @param SourceId The source identifier.
	 * @param EventData Additional data associated with the event.
	 */
	void ProcessEvent(int32 EventId, int32 SourceId, void* EventData);

	/**
	 * Process timed text events.
	 *
	 * @param FrameInfo The frame information containing the timed text data.
	 * @see ProcessAudio, ProcessVideo
	 */
	void ProcessTimedText(SceAvPlayerFrameInfo& FrameInfo);

	/**
	 * Process video.
	 *
	 * @param FrameInfo The frame information containing the video data.
	 * @see ProcessAudio, ProcessTimedText
	 */
	void ProcessVideo(SceAvPlayerFrameInfoEx& FrameInfo);

private:

	/** Callback for closing any previously opened file. */
	static int CloseFile(void* P);

	/** Callback for processing AvPlayer events. */
	static void EventCallback(void* P, int32_t EventId, int32_t SourceId, void* EventData);

	/** Callback for getting the size of the currently opened file. */
	static uint64_t GetFileSize(void* P);

	/** Callback for opening a file. */
	static int OpenFile(void* P, const char* Filename);

	/** Callback for reading from a file. */
	static int ReadOffsetFile(void* P, uint8_t* Buffer, uint64_t Position, uint32_t Length);

private:

	/** The archive to read from, if any. */
	TSharedPtr<FArchive, ESPMode::ThreadSafe> Archive;

	/** Audio sample object pool. */
	FPS4MediaAudioSamplePool* AudioSamplePool;

	/** Time range of audio samples to be requested. */
	TRange<FTimespan> AudioSampleRange;

	/** RHI texture for the chroma channel. */
	TArray<TRefCountPtr<FRHITexture>> DecodedChroma;

	/** RHI texture for the chroma luma. */
	TArray<TRefCountPtr<FRHITexture>> DecodedLuma;

	/** Media events to be forwarded to main thread. */
	TQueue<EMediaEvent> DeferredEvents;

	/** Handle to the native AvPlayer object. */
	SceAvPlayerHandle Handle;

	/** Timestamp of the last processed audio sample. */
	FTimespan LastAudioSampleTime;

	/** Timestamp of the last processed video sample. */
	FTimespan LastVideoSampleTime;

	/** The media sample queue to push generated samples into. */
	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;

	/** Video sample object pool. */
	FPS4MediaTextureSamplePool* VideoSamplePool;

	/** Time range of video samples to be requested. */
	TRange<FTimespan> VideoSampleRange;
};
