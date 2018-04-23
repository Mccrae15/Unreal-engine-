// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "IMediaTracks.h"

#include <sceavplayer.h>

struct FTimespan;


/**
 * Manages available audio, caption and video tracks.
 */
class FPS4MediaTracks
	: public IMediaTracks
{
	typedef uint32 FStreamId;

	struct FTrack
	{
		FStreamId StreamIndex;

		SceAvPlayerAudio Audio;
		SceAvPlayerTimedText TimedText;
		SceAvPlayerVideo Video;
	};

public:

	/** Default constructor. */
	FPS4MediaTracks();

public:

	/**
	 * Clear the flag that indicates whether the track selection has changed.
	 *
	 * @see GetTrackSelectionChanged
	 */
	void ClearTrackSelectionChanged()
	{
		TrackSelectionChanged = false;
	}

	/**
	 * Get debug information about track collection.
	 *
	 * @return Information string.
	 */
	const FString& GetInfo() const;

	/**
	 * Check whether the track selection has changed.
	 *
	 * @return true if the selection changed, false otherwise.
	 * @see TrackSelectionChanged
	 */
	bool GetTrackSelectionChanged() const
	{
		return TrackSelectionChanged;
	}

	/**
	 * Initialize the track collection.
	 *
	 * @param PlayerHandle Handle to the native player.
	 * @param OutDuration Will contain the duration of the longest track.
	 * @see Shutdown
	 */
	void Initialize(SceAvPlayerHandle PlayerHandle, FTimespan& OutDuration);

	/**
	 * Shut down the track collection.
	 *
	 * @param ResetDesiredTracks Whether the desired track selections should be reset.
	 * @see Initialize
	 */
	void Shutdown(bool ResetDesiredTracks);

public:

	//~ IMediaTracks interface

	virtual bool GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const override;
	virtual int32 GetNumTracks(EMediaTrackType TrackType) const override;
	virtual int32 GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetSelectedTrack(EMediaTrackType TrackType) const override;
	virtual FText GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual int32 GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual FString GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const override;
	virtual bool GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const override;
	virtual bool SelectTrack(EMediaTrackType TrackType, int32 TrackIndex) override;
	virtual bool SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex) override;

private:

	/** The available audio tracks. */
	TArray<FTrack> AudioTracks;

	/** The available caption tracks. */
	TArray<FTrack> CaptionTracks;

	/** The available video tracks. */
	TArray<FTrack> VideoTracks;

private:

	/** Index of the desired audio track. */
	int32 DesiredAudioTrack;

	/** Index of the desired caption track. */
	int32 DesiredCaptionTrack;

	/** Index of the desired video track. */
	int32 DesiredVideoTrack;

	/** Track information string. */
	FString Info;

	/** Whether the track collection has been initialized. */
	bool Initialized;

	/** Index of the selected audio track. */
	int32 SelectedAudioTrack;

	/** Index of the selected caption track. */
	int32 SelectedCaptionTrack;

	/** Index of the selected video track. */
	int32 SelectedVideoTrack;

	/** Indicates whether the track selection has changed. */
	bool TrackSelectionChanged;
};
