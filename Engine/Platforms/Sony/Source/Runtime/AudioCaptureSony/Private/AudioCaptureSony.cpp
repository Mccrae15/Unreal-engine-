// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioCaptureSony.h"
#include "AudioCaptureCoreLog.h"
#include <audioin.h>
#include <user_service/user_service_api.h>

// Set this to one if you want to use the AudioInHq function calls.
// AudioInHq can return a 48kHz stereo input stream, but bypasses key features (for example, the mic volume level that can be set by the user).
#define USE_HIGH_QUALITY_AUDIO_IN 0

#if USE_HIGH_QUALITY_AUDIO_IN
#define SONY_AUDIOIN_SAMPLERATE SCE_AUDIO_IN_FREQ_HQ
#define SONY_AUDIOIN_NUMCHANNELS 2
#define SONY_AUDIOIN_NUMFRAMES SCE_AUDIO_IN_TIME_SAMPLE_HQ
#define SONY_AUDIOIN_MAXCALLBACKSIZE SCE_AUDIO_IN_GRAIN_MAX_HQ
#define SONY_AUDIOIN_FORMAT SCE_AUDIO_IN_PARAM_FORMAT_S16_STEREO
#else
#define SONY_AUDIOIN_SAMPLERATE SCE_AUDIO_IN_FREQ_DEFAULT
#define SONY_AUDIOIN_NUMCHANNELS 1
#define SONY_AUDIOIN_NUMFRAMES SCE_AUDIO_IN_GRAIN_DEFAULT
#define SONY_AUDIOIN_MAXCALLBACKSIZE SCE_AUDIO_IN_GRAIN_DEFAULT
#define SONY_AUDIOIN_FORMAT SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO
#endif

FCriticalSection Audio::FAudioCaptureSonyStream::UserIdListLock;

Audio::FAudioCaptureSonyStream::FAudioCaptureSonyStream()
	: NumChannels(SONY_AUDIOIN_NUMCHANNELS)
	, SampleRate(SONY_AUDIOIN_SAMPLERATE)
	, PortID(INDEX_NONE)
	, bStreamStarted(false)
{
}

bool Audio::FAudioCaptureSonyStream::RegisterUser(const TCHAR* UserId)
{
	// Convert to sony id.
	SceUserServiceUserId SceUserId = FCString::Atoi(UserId);

	FScopeLock Lock(&UserIdListLock);

	// Loop over our users.
	TArray<SceUserServiceUserId>& UserIdList = GetUserIdList();
	int Index = 0;
	int FreeIndex = -1;
	bool UserFound = false;
	for (; Index < UserIdList.Num(); ++Index)
	{
		// Find first free index.
		if ((FreeIndex < 0) && (UserIdList[Index] == SCE_USER_SERVICE_USER_ID_INVALID))
		{
			FreeIndex = Index;
		}

		// Is this our user?
		if (UserIdList[Index] == SceUserId)
		{
			UserFound = true;
			break;
		}
	}

	// Add this user if we don't already have it.
	if (UserFound == false)
	{
		if (FreeIndex >= 0)
		{
			UserIdList[FreeIndex] = SceUserId;
		}
		else
		{
			UserIdList.Emplace(SceUserId);
		}
	}

	return true;
}

bool Audio::FAudioCaptureSonyStream::UnregisterUser(const TCHAR* UserId)
{
	// Convert to sony id.
	SceUserServiceUserId SceUserId = FCString::Atoi(UserId);

	FScopeLock Lock(&UserIdListLock);

	// Remove from our list.
	TArray<SceUserServiceUserId>& UserIdList = GetUserIdList();
	for (SceUserServiceUserId& ListId : UserIdList)
	{
		if (ListId == SceUserId)
		{
			ListId = SCE_USER_SERVICE_USER_ID_INVALID;
			break;
		}
	}

	return true;
}

bool Audio::FAudioCaptureSonyStream::GetCaptureDeviceInfo(FCaptureDeviceInfo& OutInfo, int32 DeviceIndex)
{
	// Default device?
	if (DeviceIndex <= 0)
	{
		OutInfo.DeviceName = FString(TEXT("AudioIn Library"));
	}
	else
	{
		FScopeLock Lock(&UserIdListLock);
		const TArray<SceUserServiceUserId>& UserIdList = GetUserIdList();
		if (DeviceIndex > UserIdList.Num())
		{
			return false;
		}

		OutInfo.DeviceId = FString::FromInt(UserIdList[DeviceIndex - 1]);
		OutInfo.DeviceName = TEXT("PS4 AudioIn Device ") + OutInfo.DeviceId;
	}

	OutInfo.InputChannels = NumChannels;
	OutInfo.PreferredSampleRate = SONY_AUDIOIN_SAMPLERATE;

	return true;
}

bool Audio::FAudioCaptureSonyStream::OpenCaptureStream(const FAudioCaptureDeviceParams& InParams, FOnCaptureFunction InOnCapture, uint32 NumFramesDesired)
{
	SceUserServiceUserId UserID;
	int32 DeviceIndex = InParams.DeviceIndex;
	if (DeviceIndex == DefaultDeviceIndex)
	{
		DeviceIndex = 0;
	}

	// Get user id.
	if (DeviceIndex == 0)
	{
		if (sceUserServiceGetInitialUser(&UserID) != SCE_OK)
		{
			UE_LOG(LogAudioCaptureCore, Error, TEXT("Failed to get default user."));
			return false;
		}
	}
	else
	{
		FScopeLock Lock(&UserIdListLock);
		const TArray<SceUserServiceUserId>& UserIdList = GetUserIdList();
		if (DeviceIndex > UserIdList.Num())
		{
			return false;
		}
		UserID = UserIdList[DeviceIndex - 1];
	}

#if USE_HIGH_QUALITY_AUDIO_IN
	PortID = sceAudioInHqOpen(UserID, SCE_AUDIO_IN_TYPE_VOICE_CHAT, 0, SONY_AUDIOIN_NUMFRAMES, SONY_AUDIOIN_SAMPLERATE, SONY_AUDIOIN_FORMAT);
#else
	PortID = sceAudioInOpen(UserID, SCE_AUDIO_IN_TYPE_VOICE_CHAT, 0, SONY_AUDIOIN_NUMFRAMES, SONY_AUDIOIN_SAMPLERATE, SONY_AUDIOIN_FORMAT);
#endif

	if (PortID < 0)
	{
		UE_LOG(LogAudioCaptureCore, Error, TEXT("Failed to open an AudioIn port: Error code %d"), PortID);
		return false;
	}

	VoicePortWorkspace.Reset();
	VoicePortWorkspace.AddUninitialized(SONY_AUDIOIN_MAXCALLBACKSIZE * NumChannels);

	OnCapture = MoveTemp(InOnCapture);

	return true;
}

bool Audio::FAudioCaptureSonyStream::CloseStream()
{
	StopStream();
	sceAudioInInput(PortID, nullptr);
	sceAudioInClose(PortID);
	PortID = INDEX_NONE;
	return true;
}

bool Audio::FAudioCaptureSonyStream::StartStream()
{
	if (PortID == INDEX_NONE)
	{
		UE_LOG(LogAudioCaptureCore, Error, TEXT("StrartStream() was called before the input stream was opened."));
		return false;
	}

	// Since this is a polling mechanism we ensure that we poll more frequently than our audio is being generated:
	constexpr float CallbackDuration = ((float)SONY_AUDIOIN_NUMFRAMES) * 0.99f / SONY_AUDIOIN_SAMPLERATE;

	InputCallback.Reset(new Audio::FMixerNullCallback(CallbackDuration, [&]()
	{
		int32 NumSamples = sceAudioInInput(PortID, VoicePortWorkspace.GetData());
		if (NumSamples < 0)
		{
			UE_LOG(LogAudioCaptureCore, Error, TEXT("Failed to get AudioIn input data: Error code %d"), SONY_AUDIOIN_NUMFRAMES);
		}
		else
		{
			OnAudioCapture(VoicePortWorkspace.GetData(), NumSamples / NumChannels, 0.0, false);
		}

	}, TPri_Highest));

	bStreamStarted = true;
	return true;
}

bool Audio::FAudioCaptureSonyStream::StopStream()
{
	InputCallback.Reset();
	bStreamStarted = false;
	return true;
}

bool Audio::FAudioCaptureSonyStream::AbortStream()
{
	StopStream();
	CloseStream();
	return true;
}

bool Audio::FAudioCaptureSonyStream::GetStreamTime(double& OutStreamTime)
{
	OutStreamTime = 0.0f;
	return true;
}

bool Audio::FAudioCaptureSonyStream::IsStreamOpen() const
{
	return PortID != INDEX_NONE;
}

bool Audio::FAudioCaptureSonyStream::IsCapturing() const
{
	return bStreamStarted;
}

void Audio::FAudioCaptureSonyStream::OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow)
{
	// InBuffer is int16, so convert to float before we pass it on.
	int16* Buffer = (int16*)InBuffer;
	ConversionBuffer.Reset();
	ConversionBuffer.Append(Buffer, InBufferFrames, NumChannels, SampleRate);
	OnCapture(ConversionBuffer.GetData(), InBufferFrames, NumChannels, SampleRate, StreamTime, bOverflow);
}

bool Audio::FAudioCaptureSonyStream::GetInputDevicesAvailable(TArray<FCaptureDeviceInfo>& OutDevices)
{
	// Get number of devices.
	int NumDevices = 1;
	const TArray<SceUserServiceUserId>& UserIdList = GetUserIdList();
	NumDevices += UserIdList.Num();

	// Get devices.
	OutDevices.Reset();
	for (int Index = 0; Index < NumDevices; ++Index)
	{
		FCaptureDeviceInfo& DeviceInfo = OutDevices.AddDefaulted_GetRef();
		GetCaptureDeviceInfo(DeviceInfo, Index);
	}

	return true;
}

TArray<SceUserServiceUserId>& Audio::FAudioCaptureSonyStream::GetUserIdList()
{
	static TArray<SceUserServiceUserId> UserIDList;
	return UserIDList;
}

