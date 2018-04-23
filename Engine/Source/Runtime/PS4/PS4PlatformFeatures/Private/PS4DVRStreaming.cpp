// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4DVRStreaming.h"
#include "PlatformFeatures.h"
#include "PS4PlatformFeatures.h"
#include <libsysmodule.h>

FPS4DVRStreamingSystem::FPS4DVRStreamingSystem () :
	bIsInitialized(false),
	bIsStreamingEnabled(false),
	bIsLibraryLoaded(false)
{
	Initialize();
}

FPS4DVRStreamingSystem::~FPS4DVRStreamingSystem()
{
	Shutdown();
}

void FPS4DVRStreamingSystem::GetStreamingStatus(FDVRStreamingStatus &StreamingStatus)
{
	// Fill out the defaults
	StreamingStatus.bIsStreaming = false;
	StreamingStatus.bIsStreamingEnabled = bIsStreamingEnabled;

	StreamingStatus.ViewerCount = 0;
	StreamingStatus.ProgramName = TEXT("");
	StreamingStatus.HLSUrl = TEXT("");
	StreamingStatus.ProviderUrl = TEXT("");

	// If the system is up and running query for the current status and info
	if (bIsInitialized)
	{
		SceGameLiveStreamingStatus2 status;
		int32 Result = sceGameLiveStreamingGetCurrentStatus2(&status);
		if (Result == SCE_OK)
		{
			StreamingStatus.bIsStreamingEnabled = bIsStreamingEnabled;
			StreamingStatus.bIsStreaming = status.isOnAir;
			if (StreamingStatus.bIsStreaming)
			{
				StreamingStatus.ViewerCount = status.spectatorCounts;

				SceGameLiveStreamingProgramInfo info;
				Result = sceGameLiveStreamingGetProgramInfo(&info);
				if (Result == SCE_OK)
				{
					StreamingStatus.ProgramName = FString::Printf(TEXT("%hs"), info.programName);
					StreamingStatus.HLSUrl = FString::Printf(TEXT("%hs"), info.hlsUrl);
					StreamingStatus.ProviderUrl = FString::Printf(TEXT("%hs"), info.programUrl);
				}
				else
				{
					UE_LOG(LogPS4, Warning, TEXT("sceGameLiveStreamingGetProgramInfo() failed. Error code: 0x%08x"), Result);
					// NOTE: The default info will be returned
				}
			}
		}
		else
		{
			UE_LOG(LogPS4, Warning, TEXT("sceGameLiveStreamingGetCurrentStatus() failed. Error code: 0x%08x"), Result);
			// NOTE: The default status and info will be returned
		}
	}
}

void FPS4DVRStreamingSystem::EnableStreaming(bool Enable)
{
	if (bIsInitialized)
	{
		bIsStreamingEnabled = Enable;
		int32 Result = sceGameLiveStreamingEnableLiveStreaming(bIsStreamingEnabled);
		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceGameLiveStreamingEnableLiveStreaming failed. Error code: 0x%08x"), Result);
		}
	}
	else
	{
		UE_LOG(LogPS4, Error, TEXT("FPS4DVRStreamingSystem not initialized"));
	}
}

bool FPS4DVRStreamingSystem::Initialize()
{
	if (!bIsInitialized)
	{
		// Load the system module for live streaming
  		int32 Result = sceSysmoduleLoadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING);
  
  		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING) failed. Error code: 0x%08x"), Result);

			// Don't continue setting things up
			return false;
  		}
		bIsLibraryLoaded = true;

		// Initialize the streaming library
		Result = sceGameLiveStreamingInitialize (SCE_GAME_LIVE_STREAMING_HEAP_SIZE);
		if (Result == SCE_OK)
		{
			bIsInitialized = true;
		}
		else
		{
			UE_LOG(LogPS4, Error, TEXT("sceGameLiveStreamingInitialize (SCE_GAME_LIVE_STREAMING_HEAP_SIZE) failed. Error code: 0x%08x"), Result);
		}
	}
	
	return bIsInitialized;
}

void FPS4DVRStreamingSystem::Shutdown()
{
	if (bIsInitialized)
	{
		// NOTE: Not monitoring the return value here since there are only two potential errors: not-initialized or internal error
		// Regardless, it won't be initialized by the time this function returns.
		sceGameLiveStreamingTerminate();

  		int32 Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING);
  
  		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING) failed. Error code: 0x%08x"), Result);
  		}
		
		// Assume that we terminated and unloaded the library successfully
		bIsLibraryLoaded = false;
		bIsInitialized = false;
	}
}
