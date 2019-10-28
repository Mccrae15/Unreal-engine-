// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VivoxVoiceChat.h"

class FPS4VivoxVoiceChat : public FVivoxVoiceChat
{
public:
	FPS4VivoxVoiceChat();
	virtual ~FPS4VivoxVoiceChat();

	virtual void SetVivoxSdkConfigHints(vx_sdk_config_t& Hints);
};
