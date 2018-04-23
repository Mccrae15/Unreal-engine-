// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4MediaPlayer.h"
#include "PS4MediaPrivate.h"

#include "Async/Async.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "MediaSamples.h"
#include "Misc/ScopeLock.h"
#include "RenderingThread.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#include "PS4MediaCallbacks.h"
#include "PS4MediaSettings.h"
#include "PS4MediaTracks.h"
#include "PS4MediaUtils.h"

#include <sceavplayer_ex.h>


#define LOCTEXT_NAMESPACE "FPS4MediaModule"


/* FPS4MediaPlayer structors
 *****************************************************************************/

FPS4MediaPlayer::FPS4MediaPlayer(IMediaEventSink& InEventSink)
	: Buffering(false)
	, CurrentDuration(0)
	, CurrentRate(0.0f)
	, CurrentState(EMediaState::Closed)
	, CurrentTime(FTimespan::Zero())
	, EventSink(InEventSink)
	, PlaybackRestarted(false)
	, Samples(MakeShared<FMediaSamples, ESPMode::ThreadSafe>())
	, ShouldLoop(false)
	, Tracks(new FPS4MediaTracks)
	, RestartRate(0.0f)
	, RestartTime(FTimespan::MinValue())
{ }


FPS4MediaPlayer::~FPS4MediaPlayer()
{
	Close();

	delete Tracks;
	Tracks = nullptr;
}


/* IMediaPlayer interface
 *****************************************************************************/

void FPS4MediaPlayer::Close()
{
	if (Callbacks.IsValid())
	{
		FScopeLock Lock(&CriticalSection);
		Callbacks.Reset();
	}
	else
	{
		return;
	}

	Tracks->Shutdown(true);

	Buffering = false;
	CurrentDuration = FTimespan::Zero();
	CurrentRate = 0.0f;
	CurrentTime = FTimespan::Zero();
	CurrentUrl.Empty();
	PlaybackRestarted = false;
	RestartRate = 0.0f;
	RestartTime = FTimespan::MinValue();
	ShouldLoop = false;
	ThinnedRates.Empty();
	UnthinnedRates.Empty();

	CurrentState = EMediaState::Closed;

	EventSink.ReceiveMediaEvent(EMediaEvent::TracksChanged);
	EventSink.ReceiveMediaEvent(EMediaEvent::MediaClosed);
}


IMediaCache& FPS4MediaPlayer::GetCache()
{
	return *this;
}


IMediaControls& FPS4MediaPlayer::GetControls()
{
	return *this;
}


FString FPS4MediaPlayer::GetInfo() const
{
	return Tracks->GetInfo();
}


FName FPS4MediaPlayer::GetPlayerName() const
{
	static FName PlayerName(TEXT("PS4Media"));
	return PlayerName;
}


IMediaSamples& FPS4MediaPlayer::GetSamples()
{
	return *this;
}


FString FPS4MediaPlayer::GetStats() const
{
	return TEXT("n/a");
}


IMediaTracks& FPS4MediaPlayer::GetTracks()
{
	return *Tracks;
}


FString FPS4MediaPlayer::GetUrl() const
{
	return CurrentUrl;
}


IMediaView& FPS4MediaPlayer::GetView()
{
	return *this;
}


bool FPS4MediaPlayer::Open(const FString& Url, const IMediaOptions* Options)
{
	Close();

	if ((Url.IsEmpty()))
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Cannot open media from URL (no URL provided)"), this);
		return false;
	}

	const bool Precache = (Options != nullptr) ? Options->GetMediaOption("PrecacheFile", false) : false;

	return InitializePlayer(nullptr, Url, Precache);
}


bool FPS4MediaPlayer::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* /*Options*/)
{
	Close();

	if (Archive->TotalSize() == 0)
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Cannot open media from archive (archive is empty)"), this);
		return false;
	}

	if (OriginalUrl.IsEmpty())
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Cannot open media from archive (no original URL provided)"), this);
		return false;
	}

	return InitializePlayer(Archive, OriginalUrl, false);
}


void FPS4MediaPlayer::TickAudio()
{
	FScopeLock Lock(&CriticalSection);

	if (Callbacks.IsValid())
	{
		Callbacks->TickAudio(CurrentRate, CurrentTime);
	}
}


void FPS4MediaPlayer::TickFetch(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	if (!Callbacks.IsValid())
	{
		return;
	}

	// AvPlayer currently does not support video track switching while playback
	// is in progress, so we create and restart a new AvPlayer instance instead.
	// See also https://ps4.scedev.net/forums/thread/187390/

	if (Tracks->GetTrackSelectionChanged())
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Re-initializing player for track selection change"), this);

		Tracks->ClearTrackSelectionChanged();

		if ((CurrentState == EMediaState::Paused) || (CurrentState == EMediaState::Playing))
		{
			RestartRate = CurrentRate;
			RestartTime = CurrentTime;
		}

		InitializePlayer(Callbacks->GetArchive(), CurrentUrl, false);
	}

	// process video samples on render thread
	struct FTickVideoParams
	{
		TWeakPtr<FPS4MediaCallbacks, ESPMode::ThreadSafe> CallbacksPtr;
		float Rate;
		FTimespan Time;
	}
	TickVideoParams = { Callbacks, CurrentRate, CurrentTime };

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(PS4MediaPlayerTickVideo,
		FTickVideoParams, Params, TickVideoParams,
		{
			auto PinnedCallbacks = Params.CallbacksPtr.Pin();

			if (PinnedCallbacks.IsValid())
			{
				PinnedCallbacks->TickVideo(Params.Rate, Params.Time);
			}
		});
}


void FPS4MediaPlayer::TickInput(FTimespan DeltaTime, FTimespan /*Timecode*/)
{
	const SceAvPlayerHandle PlayerHandle = GetCurrentHandle();

	if (PlayerHandle == nullptr)
	{
		return;
	}

	// process deferred events
	TArray<EMediaEvent> OutEvents;
	Callbacks->GetEvents(OutEvents);

	for (const auto& Event : OutEvents)
	{
		switch (Event)
		{
		case EMediaEvent::MediaBuffering:
			Buffering = true;
			break;

		case EMediaEvent::MediaOpened:
			CurrentState = EMediaState::Stopped;

			// AvPlayer currently does not support video track switching while playback
			// is in progress, so we create and restart a new AvPlayer instance instead.

			if (RestartTime >= FTimespan::Zero())
			{
				CommitRate(RestartRate);
				CommitTime(RestartTime);

				RestartRate = 0.0f;
				RestartTime = FTimespan::MinValue();

				continue; // do not forward event when track switching
			}
			break;

		case EMediaEvent::MediaOpenFailed:
			CurrentRate = 0.0f;
			CurrentState = EMediaState::Error;
			break;

		case EMediaEvent::PlaybackEndReached:
			if (!ShouldLoop)
			{
				CurrentRate = 0.0f;
				CurrentState = EMediaState::Stopped;
			}
			break;

		case EMediaEvent::PlaybackResumed:
			CurrentState = EMediaState::Playing;
			Buffering = false;
			break;

		case EMediaEvent::PlaybackSuspended:
			CurrentRate = 0.0f;
			CurrentState = EMediaState::Paused;
			Buffering = false;
			break;

		case EMediaEvent::SeekCompleted:
			CurrentTime = FTimespan(sceAvPlayerCurrentTime(PlayerHandle) * ETimespan::TicksPerMillisecond);
			break;

		case EMediaEvent::TracksChanged:
			Tracks->Initialize(PlayerHandle, CurrentDuration);
			UpdateCharacteristics();

			if (RestartTime >= FTimespan::Zero())
			{
				continue; // do not forward event when track switching
			}
			break;
		}

		EventSink.ReceiveMediaEvent(Event);
	}

	if (CurrentState != EMediaState::Playing)
	{
		return;
	}

	// update clock
	if (PlaybackRestarted)
	{
		PlaybackRestarted = false;
	}
	else if ((CurrentTime < CurrentDuration) && (CurrentTime >= FTimespan::Zero()))
	{
		CurrentTime += DeltaTime * CurrentRate;
	}

	// handle looping
	if ((CurrentTime >= CurrentDuration) || (CurrentTime < FTimespan::Zero()))
	{
		EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackEndReached);

		if (ShouldLoop)
		{
			FTimespan SeekTime;

			if (CurrentRate > 0.0f)
			{
				SeekTime = FTimespan::Zero();
			}
			else
			{
				SeekTime = CurrentDuration - FTimespan(1);
			}

/*			if ((CurrentRate != 0.0f) && (CurrentRate != 1.0f))
			{
				const int32 Result = sceAvPlayerSetTrickSpeed(PlayerHandle, SCE_AVPLAYER_SPEED_NORMAL);

				if (Result < 0)
				{
					UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to reset trick speed for looping: %s"), this, *PS4Media::ResultToString(Result));
				}
			}*/

			PlaybackRestarted = true;

			if (CommitTime(SeekTime) && CommitRate(CurrentRate))
			{
				sceAvPlayerResume(PlayerHandle);

				Callbacks->GetEvents(OutEvents); // discard all events
				Callbacks->Restart();
			}
			else
			{
				UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to loop playback"), this);
			}
		}
		else
		{
			if (sceAvPlayerIsActive(PlayerHandle))
			{
				sceAvPlayerStop(PlayerHandle);
			}
			else
			{
				CurrentRate = 0.0f;
				CurrentState = EMediaState::Stopped;
				CurrentTime = FTimespan::Zero();
			}

			EventSink.ReceiveMediaEvent(EMediaEvent::PlaybackSuspended);
		}
	}
}


/* FPS4MediaPlayer implementation
 *****************************************************************************/

bool FPS4MediaPlayer::CommitRate(float Rate)
{
	const SceAvPlayerHandle PlayerHandle = GetCurrentHandle();

	if (PlayerHandle == nullptr)
	{
		return false;
	}

	if (CurrentDuration == FTimespan::Zero())
	{
		return false; // nothing to play
	}

	UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Committing rate %f"), this, Rate);

	if (Rate == 0.0f)
	{
		// pausing is currently broken in AvPlayer if trick speed != normal
		// see also https://ps4.scedev.net/forums/thread/232864/

		int32 Result = sceAvPlayerSetTrickSpeed(PlayerHandle, SCE_AVPLAYER_SPEED_NORMAL);

		if (Result < 0)
		{
			UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to set trick speed to normal prior to pausing: %s"), this, *PS4Media::ResultToString(Result));
		}
		else
		{
			CurrentRate = Rate;
		}

		// pause playback
		Result = sceAvPlayerPause(PlayerHandle);

		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to pause playback: %s"), this, *PS4Media::ResultToString(Result));
			return false;
		}
	}
	else
	{
		// resume playback first
		if (CurrentState == EMediaState::Paused)
		{
			const int32 Result = sceAvPlayerResume(PlayerHandle);

			if (Result != SCE_OK)
			{
				UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to resume playback: %s"), this, *PS4Media::ResultToString(Result));
				return false;
			}
		}
		else if (CurrentState != EMediaState::Playing)
		{
			if (CurrentState == EMediaState::Stopped)
			{
				PlaybackRestarted = true;
			}

			const int32 Result = sceAvPlayerStart(PlayerHandle);

			if (Result != SCE_OK)
			{
				UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to resume playback: %s"), this, *PS4Media::ResultToString(Result));
				return false;
			}
		}

		// then loop around if needed
		if ((Rate > 0.0f) && (CurrentTime == CurrentDuration))
		{
			int32 Result = sceAvPlayerJumpToTime(PlayerHandle, 0);

			if (Result < 0)
			{
				UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to loop to beginning: %s)"), this, *PS4Media::ResultToString(Result));
				return false;
			}
		}
		else if ((Rate < 0.0f) && (CurrentTime == FTimespan::Zero()))
		{
			int32 Result = sceAvPlayerJumpToTime(PlayerHandle, CurrentDuration.GetTotalMilliseconds());

			if (Result < 0)
			{
				UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to loop to end: %s)"), this, *PS4Media::ResultToString(Result));
				return false;
			}
		}

		// then set trick speed if needed
		if (Rate != CurrentRate)
		{
			if (((CurrentRate != 0.0f) && (CurrentRate != 1.0f)) || ((Rate != 0.0f) && (Rate != 1.0f)))
			{
				const int32 TrickSpeed = (float)SCE_AVPLAYER_SPEED_NORMAL * Rate;
				const int32 Result = sceAvPlayerSetTrickSpeed(PlayerHandle, TrickSpeed);

				if (Result < 0)
				{
					UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to set trick speed to %i (Rate = %f): %s"), this, TrickSpeed, Rate, *PS4Media::ResultToString(Result));
					return false;
				}
			}

			CurrentRate = Rate;
		}
	}

	return true;
}


bool FPS4MediaPlayer::CommitTime(FTimespan Time)
{
	const SceAvPlayerHandle PlayerHandle = GetCurrentHandle();

	if (PlayerHandle == nullptr)
	{
		return false;
	}

	UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Committing time %s"), this, *Time.ToString());

	// cannot seek unless the video stream is active
	if (!sceAvPlayerIsActive(PlayerHandle))
	{
		int32 Result = sceAvPlayerStart(PlayerHandle);

		if (Result < 0)
		{
			UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to start player for seek: %s"), this, *PS4Media::ResultToString(Result));
			return false;
		}

		Result = sceAvPlayerPause(PlayerHandle);

		if (Result < 0)
		{
			UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to pause player for seek: %s"), this, *PS4Media::ResultToString(Result));
			return false;
		}
	}

	// jump to desired time
	uint64 JumpTime = Time.GetTicks() / ETimespan::TicksPerMillisecond;
	int32 Result = sceAvPlayerJumpToTime(PlayerHandle, JumpTime);

	if (Result < 0)
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to commit time %d: %s)"), this, Time.GetTicks(), *PS4Media::ResultToString(Result));
		return false;
	}

	CurrentTime = Time;

	return true;
}


SceAvPlayerHandle FPS4MediaPlayer::GetCurrentHandle() const
{
	if (!Callbacks.IsValid())
	{
		return nullptr;
	}

	return Callbacks->GetHandle();
}


bool FPS4MediaPlayer::InitializePlayer(const TSharedPtr<FArchive, ESPMode::ThreadSafe>& Archive, const FString& Url, bool Precache)
{
	// get settings
	auto Settings = GetDefault<UPS4MediaSettings>();

	if (Settings == nullptr)
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Failed to initialize player (settings not found)"), this);
		return false;
	}

	// initialize player
	{
		FScopeLock Lock(&CriticalSection);
		Callbacks = MakeShared<FPS4MediaCallbacks, ESPMode::ThreadSafe>(Samples.ToSharedRef());
	}

	Samples->FlushSamples();

	CurrentState = EMediaState::Preparing;
	CurrentRate = 0.0f;
	CurrentUrl = Url;

	// open the media source on a separate thread
	const int32 NumVideoBuffers = Settings->OutputVideoFrameBuffers;
	const int32 VideoBufferSizeMB = Archive.IsValid() || Url.StartsWith(TEXT("file://")) ? Settings->FileVideoBufferSizeMB : Settings->HlsVideoBufferSizeMB;
	const EAsyncExecution Execution = Precache ? EAsyncExecution::Thread : EAsyncExecution::ThreadPool;

	Async<void>(Execution, [Archive, CallbacksPtr = TWeakPtr<FPS4MediaCallbacks, ESPMode::ThreadSafe>(Callbacks), NumVideoBuffers, Precache, Url, VideoBufferSizeMB]()
	{
		TSharedPtr<FPS4MediaCallbacks, ESPMode::ThreadSafe> PinnedCallbacks = CallbacksPtr.Pin();

		if (PinnedCallbacks.IsValid())
		{
			PinnedCallbacks->Initialize(Archive, Url, Precache, NumVideoBuffers, VideoBufferSizeMB);
		}
	});

	return true;
}


void FPS4MediaPlayer::UpdateCharacteristics()
{
	if (CurrentDuration > FTimespan::Zero())
	{
		// Note: speeds less than 4x fast forward or 4x
		// rewind are not supported (even when thinned)

		ThinnedRates.Add(TRange<float>(0.0f));
		ThinnedRates.Add(TRange<float>(1.0f));
		ThinnedRates.Add(TRange<float>::Inclusive(-32.0f, -4.0f));
		ThinnedRates.Add(TRange<float>::Inclusive(4.0f, 32.0f));

		UnthinnedRates.Add(TRange<float>(0.0f));
		UnthinnedRates.Add(TRange<float>(1.0f));
	}
	else
	{
		ThinnedRates.Empty();
		UnthinnedRates.Empty();
	}
}


/* IMediaControls interface
 *****************************************************************************/

bool FPS4MediaPlayer::CanControl(EMediaControl Control) const
{
	if (Control == EMediaControl::Pause)
	{
		return (CurrentState == EMediaState::Playing);
	}

	if (Control == EMediaControl::Resume)
	{
		return ((CurrentState == EMediaState::Paused) || (CurrentState == EMediaState::Stopped));
	}

	if (Control == EMediaControl::Seek)
	{
		return ((CurrentState != EMediaState::Closed) && (CurrentState != EMediaState::Error));
	}

	return false;
}


FTimespan FPS4MediaPlayer::GetDuration() const
{
	return CurrentDuration;
}


float FPS4MediaPlayer::GetRate() const
{
	return CurrentRate;
}


EMediaState FPS4MediaPlayer::GetState() const
{
	return CurrentState;
}


EMediaStatus FPS4MediaPlayer::GetStatus() const
{
	return Buffering ? EMediaStatus::Buffering : EMediaStatus::None;
}


TRangeSet<float> FPS4MediaPlayer::GetSupportedRates(EMediaRateThinning Thinning) const
{
	return (Thinning == EMediaRateThinning::Thinned) ? ThinnedRates : UnthinnedRates;
}


FTimespan FPS4MediaPlayer::GetTime() const
{
	return CurrentTime;
}


bool FPS4MediaPlayer::IsLooping() const
{
	return ShouldLoop;
}


bool FPS4MediaPlayer::Seek(const FTimespan& Time)
{
	UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Seeking to %s"), this, *Time.ToString());

	// validate seek
	if ((CurrentState == EMediaState::Closed) ||
		(CurrentState == EMediaState::Error))
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Cannot seek while closed, preparing, or in error state"), this);
		return false;
	}

	if ((Time < FTimespan::Zero()) || (Time > CurrentDuration))
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Invalid seek time %s (media duration is %s)"), this, *Time.ToString(), *CurrentDuration.ToString());
		return false;
	}

	if (CurrentState == EMediaState::Preparing)
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Requesting seek after pending initialization"), this);

		RestartRate = CurrentRate;
		RestartTime = Time;

		return true;
	}

	return CommitTime(Time);
}


bool FPS4MediaPlayer::SetLooping(bool Looping)
{
	if ((CurrentState == EMediaState::Closed) ||
		(CurrentState == EMediaState::Error) ||
		(CurrentState == EMediaState::Preparing))
	{
		return false;
	}

	ShouldLoop = Looping;

	return true;
}


bool FPS4MediaPlayer::SetRate(float Rate)
{
	if ((CurrentState == EMediaState::Closed) ||
		(CurrentState == EMediaState::Error))
	{
		return false;
	}

	if (Rate == CurrentRate)
	{
		return true; // rate already set
	}

	UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Setting rate to %f"), this, Rate);

	// validate rate
	if (!ThinnedRates.Contains(Rate))
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: The rate %f is not supported"), this, Rate);
		return false;
	}

	if (CurrentState == EMediaState::Preparing)
	{
		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Requesting rate change after pending initialization"), this);

		RestartRate = Rate;
		RestartTime = FTimespan::Zero();

		return true;
	}

	if (Rate == CurrentRate)
	{
		return true;
	}

	return CommitRate(Rate);
}


/* IMediaSamples interface
 *****************************************************************************/

bool FPS4MediaPlayer::FetchAudio(TRange<FTimespan> TimeRange, TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe>& OutSample)
{
	return Samples->FetchAudio(TimeRange, OutSample);
}


bool FPS4MediaPlayer::FetchCaption(TRange<FTimespan> TimeRange, TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe>& OutSample)
{
	return Samples->FetchCaption(TimeRange, OutSample);
}


bool FPS4MediaPlayer::FetchVideo(TRange<FTimespan> TimeRange, TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& OutSample)
{
	return Samples->FetchVideo(TimeRange, OutSample);
}


void FPS4MediaPlayer::FlushSamples()
{
	// don't flush thumbnails
	if (CurrentState != EMediaState::Paused)
	{
		Samples->FlushSamples();

		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Flushed samples"), this);
	}

	// restart sampling
	if (Callbacks.IsValid())
	{
		Callbacks->Restart();

		UE_LOG(LogPS4Media, Verbose, TEXT("Player %p: Restarted sampling"), this);
	}
}


#undef LOCTEXT_NAMESPACE
