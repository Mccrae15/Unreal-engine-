// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyDVRStreaming.h"
#include "PlatformFeatures.h"
#include "SonyPlatformFeatures.h"
#include <libsysmodule.h>

FSonyDVRStreamingSystem::FSonyDVRStreamingSystem () :
	bIsInitialized(false),
	bIsStreamingEnabled(false),
	bIsLibraryLoaded(false)
{
	Initialize();
}

FSonyDVRStreamingSystem::~FSonyDVRStreamingSystem()
{
	Shutdown();
}

void FSonyDVRStreamingSystem::GetStreamingStatus(FDVRStreamingStatus &StreamingStatus)
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

#if GAME_LIVE_STREAMING_HAS_PROGRAM_INFO
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
					UE_LOG(LogSony, Warning, TEXT("sceGameLiveStreamingGetProgramInfo() failed. Error code: 0x%08x"), Result);
					// NOTE: The default info will be returned
				}
#endif
			}
		}
		else
		{
			UE_LOG(LogSony, Warning, TEXT("sceGameLiveStreamingGetCurrentStatus() failed. Error code: 0x%08x"), Result);
			// NOTE: The default status and info will be returned
		}
	}
}

bool FSonyDVRStreamingSystem::Initialize()
{
	if (!bIsInitialized)
	{
		// Load the system module for live streaming
  		int32 Result = sceSysmoduleLoadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING);
  
  		if (Result != SCE_OK)
		{
			UE_LOG(LogSony, Error, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING) failed. Error code: 0x%08x"), Result);

			// Don't continue setting things up
			return false;
  		}
		bIsLibraryLoaded = true;

		// Initialize the streaming library
		Result = sceGameLiveStreamingInitialize(SCE_GAME_LIVE_STREAMING_HEAP_SIZE);
		if (Result == SCE_OK)
		{
			bIsInitialized = true;
		}
		else
		{
			UE_LOG(LogSony, Error, TEXT("sceGameLiveStreamingInitialize (SCE_GAME_LIVE_STREAMING_HEAP_SIZE) failed. Error code: 0x%08x"), Result);
		}
	}
	
	return bIsInitialized;
}

void FSonyDVRStreamingSystem::Shutdown()
{
	if (bIsInitialized)
	{
		// NOTE: Not monitoring the return value here since there are only two potential errors: not-initialized or internal error
		// Regardless, it won't be initialized by the time this function returns.
		sceGameLiveStreamingTerminate();

  		int32 Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING);
  
  		if (Result != SCE_OK)
		{
			UE_LOG(LogSony, Error, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_GAME_LIVE_STREAMING) failed. Error code: 0x%08x"), Result);
  		}
		
		// Assume that we terminated and unloaded the library successfully
		bIsLibraryLoaded = false;
		bIsInitialized = false;
	}
}
