// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PS4VivoxVoiceChat.h"

#include "Async/Async.h"

TUniquePtr<FVivoxVoiceChat> CreateVivoxObject()
{
	return MakeUnique<FPS4VivoxVoiceChat>();
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

void FPS4VivoxVoiceChat::SetVivoxSdkConfigHints(vx_sdk_config_t& Hints)
{
	Hints.pf_on_audio_unit_capture_device_status_changed = &OnAudioUnitCaptureDeviceStatusChanged;
}

