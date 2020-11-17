// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VivoxVoiceChat.h"

class FPS4VivoxVoiceChatUser : public FVivoxVoiceChatUser
{
public:
	FPS4VivoxVoiceChatUser(FVivoxVoiceChat& InVivoxVoiceChat) : FVivoxVoiceChatUser(InVivoxVoiceChat) {}

	// ~Begin FVivoxVoiceChatUser Interface
	virtual void Login(FPlatformUserId PlatformId, const FString& PlayerName, const FString& Credentials, const FOnVoiceChatLoginCompleteDelegate& Delegate) override;
	// ~End FVivoxVoiceChatUser Interface
};

class FPS4VivoxVoiceChat : public FVivoxVoiceChat
{
public:
	FPS4VivoxVoiceChat();
	virtual ~FPS4VivoxVoiceChat();

	// ~Begin FVivoxVoiceChat Interface 
	virtual IVoiceChatUser* CreateUser() override;
	// ~End FVivoxVoiceChat Interface

	virtual void SetVivoxSdkConfigHints(vx_sdk_config_t& Hints);
};
