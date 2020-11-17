// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4DVRStreaming.h"
#include "PlatformFeatures.h"
#include "PS4PlatformFeatures.h"

FPS4DVRStreamingSystem::FPS4DVRStreamingSystem ()
{
}

FPS4DVRStreamingSystem::~FPS4DVRStreamingSystem()
{
}

void FPS4DVRStreamingSystem::EnableStreaming(bool Enable)
{
	if (bIsInitialized)
	{
		bIsStreamingEnabled = Enable;
		int32 Result = sceGameLiveStreamingEnableLiveStreaming(bIsStreamingEnabled);
		if (Result != SCE_OK)
		{
			UE_LOG(LogSony, Error, TEXT("sceGameLiveStreamingEnableLiveStreaming failed. Error code: 0x%08x"), Result);
		}
	}
	else
	{
		UE_LOG(LogSony, Error, TEXT("FPS4DVRStreamingSystem not initialized"));
	}
}
