// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DVRStreaming.h"
#include <game_live_streaming.h>

class FPS4DVRStreamingSystem : public IDVRStreamingSystem
{
public:
	FPS4DVRStreamingSystem();
	virtual ~FPS4DVRStreamingSystem();

	virtual void GetStreamingStatus(FDVRStreamingStatus &StreamingStatus) override;
	virtual void EnableStreaming(bool Enable) override;

private:
	/**
	 * Loads the live streaming library, initializes the live streaming system and sets defaults
	 */
	bool Initialize();

	/**
	 * Shutsdown the live streaming system and unloads the associated library
	 */
	void Shutdown();

private:
	bool bIsInitialized;
	bool bIsStreamingEnabled;
	bool bIsLibraryLoaded;
};
