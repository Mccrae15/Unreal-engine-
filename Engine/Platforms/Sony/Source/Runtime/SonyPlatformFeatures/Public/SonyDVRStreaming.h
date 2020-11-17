// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DVRStreaming.h"
#include <game_live_streaming.h>

class FSonyDVRStreamingSystem : public IDVRStreamingSystem
{
public:
	FSonyDVRStreamingSystem();
	virtual ~FSonyDVRStreamingSystem();

	virtual void GetStreamingStatus(FDVRStreamingStatus &StreamingStatus) override;

protected:
	/**
	 * Loads the live streaming library, initializes the live streaming system and sets defaults
	 */
	bool Initialize();

	/**
	 * Shutsdown the live streaming system and unloads the associated library
	 */
	void Shutdown();

protected:
	bool bIsInitialized;
	bool bIsStreamingEnabled;
	bool bIsLibraryLoaded;
};
