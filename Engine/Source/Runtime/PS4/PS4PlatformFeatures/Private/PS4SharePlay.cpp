// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4SharePlay.h"
#include <share_play.h>
#include <libsysmodule.h>

FPS4SharePlay::FPS4SharePlay () :
	bIsInitialized(false),
	bIsEnabled(false)
{
	Initialize();
}

FPS4SharePlay::~FPS4SharePlay()
{
	Shutdown();
}

void FPS4SharePlay::EnableSharing(bool Enable)
{
	if (bIsInitialized)
	{
		bIsEnabled = Enable;
		if (bIsEnabled)
		{
			// allow screen/play sharing
			int32 Result = sceSharePlaySetProhibition(SCE_SHARE_PLAY_PROHIBITION_MODE_OFF);
			if (Result != SCE_OK)
			{
				UE_LOG(LogPS4, Error, TEXT("sceSharePlaySetProhibition failed. Error code: 0x%08x"), Result);
			}
		}
		else
		{
			// disallow screen/play sharing
			int32 Result = sceSharePlaySetProhibition(SCE_SHARE_PLAY_PROHIBITION_MODE_CONTROL_SCREEN);
			if (Result != SCE_OK)
			{
				UE_LOG(LogPS4, Error, TEXT("sceSharePlaySetProhibition failed. Error code: 0x%08x"), Result);
			}
		}
	}
	else
	{
		UE_LOG(LogPS4, Error, TEXT("FPS4SharePlay not initialized"));
	}
}

bool FPS4SharePlay::Initialize()
{
	if (!bIsInitialized)
	{
		// Load the system module
		int32 Result = sceSysmoduleLoadModule(SCE_SYSMODULE_SHARE_PLAY);
  		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_SHARE_PLAY) failed. Error code: 0x%08x"), Result);

			// Don't continue setting things up
			return false;
  		}

		Result = sceSharePlayInitialize(NULL, 0);
		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSharePlayInitialize failed. Error code: 0x%08x"), Result);

			// Don't continue setting things up
			return false;
		}
		bIsInitialized = true;
	}
	
	return bIsInitialized;
}

void FPS4SharePlay::Shutdown()
{
	if (bIsInitialized)
	{
		int32 Result = sceSharePlayTerminate();
		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSharePlayTerminate failed. Error code: 0x%08x"), Result);
		}

		Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_SHARE_PLAY);
  		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_SHARE_PLAY) failed. Error code: 0x%08x"), Result);
  		}
		
		// Assume that we terminated and unloaded the library successfully
		bIsInitialized = false;
	}
}
