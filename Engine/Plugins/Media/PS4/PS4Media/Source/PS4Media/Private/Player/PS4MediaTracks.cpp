// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4MediaTracks.h"
#include "PS4MediaPrivate.h"

#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "MediaHelpers.h"
#include "Misc/Timespan.h"

#include "PS4MediaUtils.h"


#define LOCTEXT_NAMESPACE "FPS4MediaTracks"


/* FPS4MediaTracks structors
 *****************************************************************************/

FPS4MediaTracks::FPS4MediaTracks()
	: DesiredAudioTrack(0)
	, DesiredCaptionTrack(INDEX_NONE)
	, DesiredVideoTrack(0)
	, SelectedAudioTrack(INDEX_NONE)
	, SelectedCaptionTrack(INDEX_NONE)
	, SelectedVideoTrack(INDEX_NONE)
	, TrackSelectionChanged(false)
{ }


/* FPS4MediaTracks interface
 *****************************************************************************/

const FString& FPS4MediaTracks::GetInfo() const
{
	return Info;
}


void FPS4MediaTracks::Initialize(SceAvPlayerHandle PlayerHandle, FTimespan& OutDuration)
{
	Shutdown(false);

	OutDuration = FTimespan::Zero();

	if (PlayerHandle == nullptr)
	{
		return;
	}

	// create tracks for each media stream
	const int32 StreamCount = sceAvPlayerStreamCount(PlayerHandle);

	UE_LOG(LogPS4Media, Verbose, TEXT("Tracks %p: Found %i streams"), this, StreamCount);

	for (int32 StreamIndex = 0; StreamIndex < StreamCount; ++StreamIndex)
	{
		Info += FString::Printf(TEXT("Stream %i\n"), StreamIndex);

		// get stream info
		SceAvPlayerStreamInfo StreamInfo;
		{
			int32 Result = sceAvPlayerGetStreamInfo(PlayerHandle, StreamIndex, &StreamInfo);

			if (Result != 0)
			{
				UE_LOG(LogPS4Media, Warning, TEXT("Failed to get stream info for stream %i: %s"), StreamIndex, *PS4Media::ResultToString(Result));
				Info += TEXT("    failed to get stream info\n");

				continue;
			}
		}

		// create track
		if (StreamInfo.type == SCE_AVPLAYER_AUDIO)
		{
			const int32 AudioTrackIndex = AudioTracks.AddDefaulted();
			UE_LOG(LogPS4Media, VeryVerbose, TEXT("Tracks %p: Total number of audio tracks %i, added audio track: %i"), this, AudioTracks.Num(), AudioTrackIndex);

			FTrack& AudioTrack = AudioTracks[AudioTrackIndex];
			{
				AudioTrack.Audio = StreamInfo.details.audio;
				AudioTrack.StreamIndex = StreamIndex;
			}

			if (AudioTrackIndex == DesiredAudioTrack)
			{
				const int32 Result = sceAvPlayerEnableStream(PlayerHandle, StreamIndex);

				if (Result != 0)
				{
					UE_LOG(LogPS4Media, Warning, TEXT("Failed to enable stream %i: %s"), StreamIndex, *PS4Media::ResultToString(Result));
				}
				else
				{
					UE_LOG(LogPS4Media, Verbose, TEXT("Tracks %p: Enabled stream %i"), this, StreamIndex);
				}

				SelectedAudioTrack = DesiredAudioTrack;
			}
			else
			{
				const int32 Result = sceAvPlayerDisableStream(PlayerHandle, StreamIndex);

				if (Result != 0)
				{
					UE_LOG(LogPS4Media, Warning, TEXT("Failed to disable stream %i: %s"), StreamIndex, *PS4Media::ResultToString(Result));
				}
				else
				{
					UE_LOG(LogPS4Media, Verbose, TEXT("Tracks %p: Disabled stream %i"), this, StreamIndex);
				}
			}

			Info += TEXT("    Type: Audio\n");
			Info += FString::Printf(TEXT("    Duration: %s\n"), *FTimespan::FromMilliseconds(StreamInfo.duration).ToString());
			Info += FString::Printf(TEXT("    Language: %s\n"), *PS4Media::LanguageToString(AudioTrack.Audio.languageCode));
			Info += FString::Printf(TEXT("    Channels: %i\n"), AudioTrack.Audio.channelCount);
			Info += FString::Printf(TEXT("    Sample Rate: %i Hz\n"), AudioTrack.Audio.sampleRate);
		}
		else if (StreamInfo.type == SCE_AVPLAYER_TIMEDTEXT)
		{
			const int32 CaptionTrackIndex = CaptionTracks.AddDefaulted();
			FTrack& CaptionTrack = CaptionTracks[CaptionTrackIndex];
			{
				CaptionTrack.TimedText = StreamInfo.details.subs;
				CaptionTrack.StreamIndex = StreamIndex;
			}

			if (CaptionTrackIndex == DesiredCaptionTrack)
			{
				const int32 Result = sceAvPlayerEnableStream(PlayerHandle, StreamIndex);

				if (Result != 0)
				{
					UE_LOG(LogPS4Media, Warning, TEXT("Failed to enable stream %i: %s"), StreamIndex, *PS4Media::ResultToString(Result));
				}
				else
				{
					UE_LOG(LogPS4Media, Verbose, TEXT("Tracks %p: Enabled stream %i"), this, StreamIndex);
				}

				SelectedCaptionTrack = DesiredCaptionTrack;
			}
			else
			{
				const int32 Result = sceAvPlayerDisableStream(PlayerHandle, StreamIndex);

				if (Result != 0)
				{
					UE_LOG(LogPS4Media, Warning, TEXT("Failed to disable stream %i: %s"), StreamIndex, *PS4Media::ResultToString(Result));
				}
				else
				{
					UE_LOG(LogPS4Media, Verbose, TEXT("Tracks %p: Disabled stream %i"), this, StreamIndex);
				}
			}

			Info += TEXT("    Type: Caption\n");
			Info += FString::Printf(TEXT("    Duration: %s\n"), *FTimespan::FromMilliseconds(StreamInfo.duration).ToString());
			Info += FString::Printf(TEXT("    Language: %s\n"), *PS4Media::LanguageToString(CaptionTrack.TimedText.languageCode));
		}
		else if (StreamInfo.type == SCE_AVPLAYER_VIDEO)
		{
			const int32 VideoTrackIndex = VideoTracks.AddDefaulted();
			UE_LOG(LogPS4Media, VeryVerbose, TEXT("Tracks %p: Total number of video tracks %i, added video track: %i"), this, VideoTracks.Num(), VideoTrackIndex);

			FTrack& VideoTrack = VideoTracks[VideoTrackIndex];
			{
				VideoTrack.Video = StreamInfo.details.video;
				VideoTrack.StreamIndex = StreamIndex;
			}

			if (VideoTrackIndex == DesiredVideoTrack)
			{
				const int32 Result = sceAvPlayerEnableStream(PlayerHandle, StreamIndex);

				if (Result != 0)
				{
					UE_LOG(LogPS4Media, Warning, TEXT("Failed to enable stream %i: %s"), StreamIndex, *PS4Media::ResultToString(Result));
				}
				else
				{
					UE_LOG(LogPS4Media, Verbose, TEXT("Tracks %p: Enabled stream %i"), this, StreamIndex);
				}

				SelectedVideoTrack = DesiredVideoTrack;
			}
			else
			{
				const int32 Result = sceAvPlayerDisableStream(PlayerHandle, StreamIndex);

				if (Result != 0)
				{
					UE_LOG(LogPS4Media, Warning, TEXT("Failed to disable stream %i: %s"), StreamIndex, *PS4Media::ResultToString(Result));
				}
				else
				{
					UE_LOG(LogPS4Media, Verbose, TEXT("Tracks %p: Disabled stream %i"), this, StreamIndex);
				}
			}

			Info += TEXT("    Type: Video\n");
			Info += FString::Printf(TEXT("    Duration: %s\n"), *FTimespan::FromMilliseconds(StreamInfo.duration).ToString());
			Info += FString::Printf(TEXT("    Language: %s\n"), *PS4Media::LanguageToString(VideoTrack.Video.languageCode));
			Info += FString::Printf(TEXT("    Dimensions: %i x %i\n"), VideoTrack.Video.width, VideoTrack.Video.height);
		}

		// update media duration
		OutDuration = FMath::Max(OutDuration, FTimespan::FromMilliseconds(StreamInfo.duration));
		Info += TEXT("\n");
	}

	// verify stream selections
	if (SelectedAudioTrack != DesiredAudioTrack)
	{
		UE_LOG(LogPS4Media, Warning, TEXT("Failed to select desired audio track %i, resetting to: %i"), DesiredAudioTrack, SelectedAudioTrack);
		DesiredAudioTrack = SelectedAudioTrack;
	}

	if (SelectedCaptionTrack != DesiredCaptionTrack)
	{
		UE_LOG(LogPS4Media, Warning, TEXT("Failed to select desired caption track %i, resetting to: %i"), DesiredCaptionTrack, SelectedCaptionTrack);
		DesiredCaptionTrack = SelectedCaptionTrack;
	}

	if (SelectedVideoTrack != DesiredVideoTrack)
	{
		UE_LOG(LogPS4Media, Warning, TEXT("Failed to select desired video track %i, resetting to: %i"), DesiredVideoTrack, SelectedVideoTrack);
		DesiredVideoTrack = SelectedVideoTrack;
	}
	
	Initialized = true;
}


void FPS4MediaTracks::Shutdown(bool ResetDesiredTracks)
{
	if (ResetDesiredTracks)
	{
		DesiredAudioTrack = 0;
		DesiredCaptionTrack = INDEX_NONE;
		DesiredVideoTrack = 0;
	}

	SelectedAudioTrack = INDEX_NONE;
	SelectedCaptionTrack = INDEX_NONE;
	SelectedVideoTrack = INDEX_NONE;
	TrackSelectionChanged = false;

	AudioTracks.Empty();
	CaptionTracks.Empty();
	VideoTracks.Empty();
	Info.Empty();

	Initialized = false;
}


/* IMediaTracks interface
 *****************************************************************************/

bool FPS4MediaTracks::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	if ((FormatIndex != 0) || !AudioTracks.IsValidIndex(TrackIndex))
	{
		return false;
	}

	const SceAvPlayerAudio& TrackInfo = AudioTracks[TrackIndex].Audio;

	OutFormat.BitsPerSample = 16;
	OutFormat.NumChannels = TrackInfo.channelCount;
	OutFormat.SampleRate = TrackInfo.sampleRate;
	OutFormat.TypeName = TEXT("MPEG-4 AAC");

	return true;
}


int32 FPS4MediaTracks::GetNumTracks(EMediaTrackType TrackType) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return AudioTracks.Num();

	case EMediaTrackType::Caption:
		return CaptionTracks.Num();

	case EMediaTrackType::Video:
		return VideoTracks.Num();

	default:
		return 0;
	}
}


int32 FPS4MediaTracks::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return ((TrackIndex >= 0) && (TrackIndex < GetNumTracks(TrackType))) ? 1 : 0;
}


int32 FPS4MediaTracks::GetSelectedTrack(EMediaTrackType TrackType) const
{
	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return SelectedAudioTrack;

	case EMediaTrackType::Caption:
		return SelectedCaptionTrack;

	case EMediaTrackType::Video:
		return SelectedVideoTrack;

	default:
		return INDEX_NONE;
	}
}


FText FPS4MediaTracks::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if (((TrackType == EMediaTrackType::Audio) && !AudioTracks.IsValidIndex(TrackIndex)) ||
		((TrackType == EMediaTrackType::Caption) && !CaptionTracks.IsValidIndex(TrackIndex)) ||
		((TrackType == EMediaTrackType::Video) && !VideoTracks.IsValidIndex(TrackIndex)))
	{
		return FText::GetEmpty();
	}

	// AvPlayer doesn't provide stream names, so we make one up
	return FText::Format(LOCTEXT("StreamDisplayNameFormat", "Stream {0}"), FText::AsNumber(TrackIndex));
}


int32 FPS4MediaTracks::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return (GetSelectedTrack(TrackType) != INDEX_NONE) ? 0 : INDEX_NONE;
}


FString FPS4MediaTracks::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	uint8_t const* LanguageCode = nullptr;

	if (TrackType == EMediaTrackType::Audio)
	{
		if (AudioTracks.IsValidIndex(TrackIndex))
		{
			LanguageCode = AudioTracks[TrackIndex].Audio.languageCode;
		}
	}
	else if (TrackType == EMediaTrackType::Caption)
	{
		if (CaptionTracks.IsValidIndex(TrackIndex))
		{
			LanguageCode = CaptionTracks[TrackIndex].TimedText.languageCode;
		}
	}
	else if (TrackType == EMediaTrackType::Video)
	{
		if (VideoTracks.IsValidIndex(TrackIndex))
		{
			LanguageCode = VideoTracks[TrackIndex].Video.languageCode;
		}
	}

	return PS4Media::LanguageToString(LanguageCode);
}


FString FPS4MediaTracks::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FString(); // AvPlayer doesn't provide track names
}


bool FPS4MediaTracks::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	if ((FormatIndex != 0) || !VideoTracks.IsValidIndex(TrackIndex))
	{
		return false;
	}

	const SceAvPlayerVideo& TrackInfo = VideoTracks[TrackIndex].Video;

	OutFormat.Dim = FIntPoint(TrackInfo.width, TrackInfo.height);
	OutFormat.FrameRate = 30.0f;
	OutFormat.FrameRates = TRange<float>(30.0f);
	OutFormat.TypeName = TEXT("MPEG-4 AVC");

	return true;
}


bool FPS4MediaTracks::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if (!Initialized)
	{
		return false;
	}

	UE_LOG(LogPS4Media, Verbose, TEXT("Tracks %p: Selecting %s track %i"), this, *MediaUtils::TrackTypeToString(TrackType), TrackIndex);

	int32* DesiredTrack = nullptr;
	TArray<FTrack>* Tracks = nullptr;

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		DesiredTrack = &DesiredAudioTrack;
		Tracks = &AudioTracks;
		break;

	case EMediaTrackType::Caption:
		DesiredTrack = &DesiredCaptionTrack;
		Tracks = &CaptionTracks;
		break;

	case EMediaTrackType::Video:
		DesiredTrack = &DesiredVideoTrack;
		Tracks = &VideoTracks;
		break;

	default:
		return false; // unsupported track type
	}

	check(DesiredTrack != nullptr);
	check(Tracks != nullptr);

	if (TrackIndex == *DesiredTrack)
	{
		return true; // already selected
	}

	if ((TrackIndex != INDEX_NONE) && !Tracks->IsValidIndex(TrackIndex))
	{
		return false; // invalid track index
	}

	*DesiredTrack = TrackIndex;

	// AvPlayer currently does not support video track switching while
	// playback is in progress, so we just remember the desired tracks,
	// and the owner player will create a new AvPlayer instance instead.

	UE_LOG(LogPS4Media, Verbose, TEXT("Tracks %p: Selected %s track %i instead of %i (%i tracks)"), this, *MediaUtils::TrackTypeToString(TrackType), TrackIndex, SelectedAudioTrack, AudioTracks.Num());
	
	TrackSelectionChanged = true;

	return true;
}


bool FPS4MediaTracks::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	if (!Initialized || (FormatIndex != 0))
	{
		return false;
	}

	switch (TrackType)
	{
	case EMediaTrackType::Audio:
		return AudioTracks.IsValidIndex(TrackIndex);

	case EMediaTrackType::Caption:
		return CaptionTracks.IsValidIndex(TrackIndex);

	case EMediaTrackType::Video:
		return VideoTracks.IsValidIndex(TrackIndex);

	default:
		return false;
	}
}


#undef LOCTEXT_NAMESPACE
