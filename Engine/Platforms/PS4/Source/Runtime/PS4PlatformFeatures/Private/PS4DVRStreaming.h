// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SonyDVRStreaming.h"
#include <game_live_streaming.h>

class FPS4DVRStreamingSystem : public FSonyDVRStreamingSystem
{
public:
	FPS4DVRStreamingSystem();
	virtual ~FPS4DVRStreamingSystem();

	virtual void EnableStreaming(bool Enable) override;
};
