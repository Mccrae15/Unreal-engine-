// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4VivoxVoiceChat.h"

#include "Async/Async.h"
#include "PS4Application.h"

TUniquePtr<FVivoxVoiceChat> CreateVivoxObject()
{
	return MakeUnique<FPS4VivoxVoiceChat>();
}

void FPS4VivoxVoiceChatUser::Login(FPlatformUserId PlatformId, const FString& PlayerName, const FString& Credentials, const FOnVoiceChatLoginCompleteDelegate& Delegate)
{
	FVivoxVoiceChatUser::Login(PlatformId, PlayerName, Credentials, Delegate);

	// Set input/output device to the player id
	SceUserServiceUserId ServiceUserId = FPS4Application::GetPS4Application()->GetUserID(PlatformId);
	char SceUserIdString[11]; // 10 digits for max 32 bit int + null
	FCStringAnsi::Snprintf(SceUserIdString, sizeof(SceUserIdString), "%u", ServiceUserId);
	
	VivoxClientApi::AudioDeviceId InputDeviceId(SceUserIdString, SceUserIdString);
	const VivoxClientApi::AudioDeviceId* AudioDevices = nullptr;
	int NumAudioDevices = 0;
	VivoxClientConnection.GetAvailableAudioInputDevices(LoginSession.AccountName, AudioDevices, NumAudioDevices);
	if (AudioDevices)
	{
		for (int DeviceIndex = 0; DeviceIndex < NumAudioDevices; ++DeviceIndex)
		{
			if (FCStringAnsi::Strcmp(SceUserIdString, AudioDevices[DeviceIndex].GetAudioDeviceId()) == 0)
			{
				InputDeviceId = AudioDevices[DeviceIndex];
				break;
			}
		}
	}
	AudioInputOptions.DevicePolicy.SetSpecificAudioDevice(InputDeviceId);

	VivoxClientApi::AudioDeviceId OutputDeviceId(SceUserIdString, SceUserIdString);
	AudioDevices = nullptr;
	NumAudioDevices = 0;
	VivoxClientConnection.GetAvailableAudioOutputDevices(LoginSession.AccountName, AudioDevices, NumAudioDevices);
	if (AudioDevices)
	{
		for (int DeviceIndex = 0; DeviceIndex < NumAudioDevices; ++DeviceIndex)
		{
			if (FCStringAnsi::Strcmp(SceUserIdString, AudioDevices[DeviceIndex].GetAudioDeviceId()) == 0)
			{
				OutputDeviceId = AudioDevices[DeviceIndex];
				break;
			}
		}
	}
	AudioOutputOptions.DevicePolicy.SetSpecificAudioDevice(OutputDeviceId);
}

static void OnAudioUnitCaptureDeviceStatusChanged(int Status)
{
	// This called from the vivox audio thread, trigger the delegate on the game thread
	AsyncTask(ENamedThreads::GameThread, [Status]()
	{
		UE_LOG(LogVivoxVoiceChat, Log, TEXT("OnAudioUnitCaptureDeviceStatusChanged: %x"), Status);
		FVivoxDelegates::OnAudioUnitCaptureDeviceStatusChanged.Broadcast(Status);
	});
}

FPS4VivoxVoiceChat::FPS4VivoxVoiceChat()
{
}

FPS4VivoxVoiceChat::~FPS4VivoxVoiceChat()
{
}

IVoiceChatUser* FPS4VivoxVoiceChat::CreateUser()
{
	return new FPS4VivoxVoiceChatUser(*this);
}

void FPS4VivoxVoiceChat::SetVivoxSdkConfigHints(vx_sdk_config_t& Hints)
{
	Hints.pf_on_audio_unit_capture_device_status_changed = &OnAudioUnitCaptureDeviceStatusChanged;
}

