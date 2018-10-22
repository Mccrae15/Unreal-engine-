// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "HAL/CriticalSection.h"
#include "Templates/SharedPointer.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaSamples.h"
#include "IMediaView.h"

#include <sceavplayer.h>

class FMediaSamples;
class FPS4MediaCallbacks;
class FPS4MediaTracks;
class IMediaEventSink;


/*
 * Implement media playback using the PS4 MediaPlayer interface.
 */
class FPS4MediaPlayer
	: public IMediaPlayer
	, protected IMediaCache
	, protected IMediaControls
	, protected IMediaSamples
	, protected IMediaView
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InEventSink The object that receives media events from this player.
	 */
	FPS4MediaPlayer(IMediaEventSink& InEventSink);

	/** Virtual destructor. */
	virtual ~FPS4MediaPlayer();

public:

	//~ IMediaPlayer interface

	virtual void Close() override;
	virtual IMediaCache& GetCache() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FName GetPlayerName() const override;
	virtual IMediaSamples& GetSamples() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual IMediaView& GetView() override;
	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
	virtual bool Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options) override;
	virtual void TickAudio() override;
	virtual void TickFetch(FTimespan DeltaTime, FTimespan Timecode) override;
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override;

protected:

	/**
	 * Send the specified playback rate to the AvPlayer instance.
	 *
	 * @param Rate The rate to set.
	 * @see CommitLoop, CommitTime
	 */
	bool CommitRate(float Rate);

	/**
	 * Send the specified playback time to the AvPlayer instance.
	 *
	 * @param Time The playback time to set.
	 * @see CommitLoop, CommitRate
	 */
	bool CommitTime(FTimespan Time);

	/**
	 * Get the current AvPlayer handle.
	 *
	 * @return The handle, or nullptr if not initialized.
	 */
	SceAvPlayerHandle GetCurrentHandle() const;

	/**
	 * Initialize the native AvPlayer instance.
	 *
	 * @param Archive The archive being used as a media source (optional).
	 * @param Url The media URL being opened.
	 * @param Precache Whether to precache media into RAM if InURL is a local file.
	 * @return true on success, false otherwise.
	 */
	bool InitializePlayer(const TSharedPtr<FArchive, ESPMode::ThreadSafe>& Archive, const FString& Url, bool Precache);

	/** Get the latest characteristics from the current media source. */
	void UpdateCharacteristics();

protected:

	//~ IMediaControls interface

	virtual bool CanControl(EMediaControl Control) const override;
	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual EMediaStatus GetStatus() const override;
	virtual TRangeSet<float> GetSupportedRates(EMediaRateThinning Thinning) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;

protected:

	//~ IMediaSamples interface

	virtual bool FetchAudio(TRange<FTimespan> TimeRange, TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe>& OutSample) override;
	virtual bool FetchCaption(TRange<FTimespan> TimeRange, TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>& OutSample) override;
	virtual bool FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample) override;
	virtual void FlushSamples() override;

private:

	/** Whether the player is currently buffering data. */
	bool Buffering;

	/** The current callback handler. */
	TSharedPtr<FPS4MediaCallbacks, ESPMode::ThreadSafe> Callbacks;

	/** Length of the longest track. */
	FTimespan CurrentDuration;

	/** Critical section for synchronizing access to callback handler. */
	FCriticalSection CriticalSection;

	/** Current play rate. */
	float CurrentRate;

	/** The player's current state. */
	EMediaState CurrentState;

	/** The current playback time. */
	FTimespan CurrentTime;

	/** Currently opened media URL. */
	FString CurrentUrl;

	/** The media event handler. */
	IMediaEventSink& EventSink;

	/** If playback just restarted from the Stopped state. */
	bool PlaybackRestarted;

	/** The media sample queue. */
	TSharedPtr<FMediaSamples, ESPMode::ThreadSafe> Samples;

	/** Whether playback should be looped. */
	bool ShouldLoop;

	/** The thinned play rates supported by the current media source. */
	TRangeSet<float> ThinnedRates;

	/** Media tracks. */
	FPS4MediaTracks* Tracks;

	/** The unthinned play rates supported by the current media source. */
	TRangeSet<float> UnthinnedRates;

private:

	// AvPlayer currently does not support video track switching while playback
	// is in progress, so we create and restart a new AvPlayer instance instead.

	/** Play rate when restarting playback. */
	float RestartRate;

	/** Restart playback at this time if >= Zero. */
	FTimespan RestartTime;
};
