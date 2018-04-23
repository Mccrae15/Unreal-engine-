// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoviePlayer.h"

#include "Slate/SlateTextures.h"

#include <sceavplayer.h>

#include "GnmRHI.h"

// Audio playback configuration
#define NUM_PCM_SAMPLES			(1024)
#define PCM_SAMPLE_SIZE			(sizeof(int16_t))
#define SINGLE_PCM_BUFFER		(NUM_PCM_SAMPLES * PCM_SAMPLE_SIZE)

#define MAX_NUM_CHANNELS		(8)
#define DEFAULT_NUM_CHANNELS	(2)
#define BASE_SAMPLE_RATE		(48000)

// The actual streamer class
class FAVPlayerMovieStreamer : public IMovieStreamer
{
public:
	FAVPlayerMovieStreamer();
	~FAVPlayerMovieStreamer();

	virtual bool Init(const TArray<FString>& MoviePaths, TEnumAsByte<EMoviePlaybackType> inPlaybackType) override;
	virtual void ForceCompletion() override;
	virtual bool Tick(float DeltaTime) override;

	virtual TSharedPtr<class ISlateViewport> GetViewportInterface() override;
	virtual float GetAspectRatio() const override;
	virtual void Cleanup() override;

	virtual FString GetMovieName() override;
	virtual bool IsLastMovieInPlaylist() override;

	FOnCurrentMovieClipFinished OnCurrentMovieClipFinishedDelegate;
	virtual FOnCurrentMovieClipFinished& OnCurrentMovieClipFinished() override { return OnCurrentMovieClipFinishedDelegate; }

	virtual FTexture2DRHIRef GetTexture() override { return SlateVideoTexture.IsValid() ? SlateVideoTexture->GetRHIRef() : nullptr; }

private:
	//Theoretically only need 2 buffered textures, but we have extra to avoid needing to make a copy of the AvPlayer data to pass to an RHI thread command.  Instead, we buffer deeper and update the textures on the Render thread.
	static const int32 NumBufferedTextures = 4;

	/** Texture and viewport data for displaying to Slate.  SlateVideoTexture is always used by the viewport, but it's texture reference is swapped out when a new frame is available.
	 Method assumes 1 Tick() call per frame, and that the Streamer Tick comes before Slate rendering */
	TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> SlateVideoTexture;
	TSharedPtr<FSlateTexture2DRHIRef, ESPMode::ThreadSafe> BufferedVideoTextures[NumBufferedTextures];	
	int32 CurrentTexture;

	TSharedPtr<FMovieViewport> MovieViewport;

	// The list of pending movies
	TArray<FString>		MovieQueue;

	// Current language
	char                LanguageName[16];

	// The current AVPlayer instance
	SceAvPlayerHandle	SamplePlayer;
	bool				bIsPlayerLoaded;

	// Audio handling
	ScePthread			AudioThread;
	bool				bAudioThreadShouldRun;
	uint8				NumAudioChannels;
	int32				AudioPortType;
	int32				AudioSampleRate;
	int32				AudioHandle;
	uint32				PCMBufferSize;

	// Audio resampling support
	float				InputOffset;
	int32				NumLeadInSamples;
	float				ResampleData[MAX_NUM_CHANNELS][4];
	int32				NumOutputSamples;
	float				ResampleOutputBuffer[NUM_PCM_SAMPLES * MAX_NUM_CHANNELS];

	// Video handling
	bool				bWasActive;			    // used to detect active-to-inactive transition
	bool				bIsInTick;              // used to guard against a race condition between ForceCompletion() and Tick()

	int32				CurrentStreamWidth;
	int32				CurrentStreamHeight;

	//the decoder end up re-using the same textures, so keep them cached.
	TArray<FTextureRHIRef> DecodedLuma;
	TArray<FTextureRHIRef> DecodedChroma;

	class TickGuard
	{
	public:
		TickGuard(FAVPlayerMovieStreamer *MovieStreamer);
		~TickGuard();

	private:
		FAVPlayerMovieStreamer *Streamer;
	};
	
private:
	/**
	 * Thread function for handling audio playback
	 */
	static void* AudioThreadFunction(void* arg);

	/**
	 * Handler for events generated during video playback
	 */
	static void EventCallback(void* data, int32_t argEventId, int32_t argSourceId, void* argEventData);

	/**
	 * Check with the AV player library to determine if a video frame is ready to be processes
	 * Returns true if there was a video frame
	 */
	void CheckForNextVideoFrame();

	/**
	 * Find the cached texture or create a new one
	 */
	FTextureRHIRef FindOrCreateRHITexture( const Gnm::Texture& GnmTexture, bool bLuma );

	/**
	 * Sets up and starts playback on the next movie in the queue
	 */
	bool StartNextMovie();

	/**
	 * Initializes the av player and starts the video thread
	 */
	bool SetupPlayback();

	/**
	 * Terminates the video thread, shutsdown audio, and closes the player
	 */
	void TeardownPlayback();

	//
	// Audio methods
	//

	/**
	 * Sends audio to the output, resampling as needed
	 */
	bool SendAudio(uint8* argpData, uint32 argSampleRate, uint32 argChannelCount);

	/**
	 * Sends empty audio to the output, ensuring that there is silence
	 */
	bool SendAudioEnd();

	/**
	 * Opens the audio device and starts the audio processing thread
	 */
	bool SetupAudio();

	/**
	 * Closes the audio device and terminates the audio processing thread
	 */
	void TeardownAudio();

	/**
	 * Sends data to the audio output, resampling as needed
	 */
	bool SoundOutputResample(int16 *data, int32 numSamples, uint32 argSampleRate, uint32 argChannelCount);

	/**
	 * Sends data to the audio output, remapping to an 8 channel format
	 */
	bool SoundOutputTo8Channel(int16 *data, int32 numSamples, uint32 argSampleRate, uint32 argChannelCount);
};
