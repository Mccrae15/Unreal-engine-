// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PS4VivoxVoiceChat.h"

TUniquePtr<FVivoxVoiceChat> CreateVivoxObject()
{
	return MakeUnique<FPS4VivoxVoiceChat>();
}

FPS4VivoxVoiceChat::FPS4VivoxVoiceChat()
{
}

FPS4VivoxVoiceChat::~FPS4VivoxVoiceChat()
{
}

