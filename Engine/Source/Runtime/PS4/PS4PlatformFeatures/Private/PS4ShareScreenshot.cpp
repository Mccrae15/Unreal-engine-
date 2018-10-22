// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4ShareScreenshot.h"
#include <screenshot.h>
#include <libsysmodule.h>

FPS4ShareScreenshot::FPS4ShareScreenshot () :
	bIsInitialized(false),
	bIsEnabled(false)
{
	Initialize();
}

FPS4ShareScreenshot::~FPS4ShareScreenshot()
{
	Shutdown();
}

void FPS4ShareScreenshot::EnableScreenshots(bool Enable)
{
	if (bIsInitialized)
	{
		bIsEnabled = Enable;
		if (bIsEnabled)
		{
			int32 Result = sceScreenShotEnable();
			if (Result != SCE_OK)
			{
				UE_LOG(LogPS4, Error, TEXT("sceScreenShotEnable failed. Error code: 0x%08x"), Result);
			}
		}
		else
		{
			int32 Result = sceScreenShotDisable();
			if (Result != SCE_OK)
			{
				UE_LOG(LogPS4, Error, TEXT("sceScreenShotDisable failed. Error code: 0x%08x"), Result);
			}

		}
	}
	else
	{
		UE_LOG(LogPS4, Error, TEXT("FPS4ShareScreenshot not initialized"));
	}
}

bool FPS4ShareScreenshot::Initialize()
{
	if (!bIsInitialized)
	{
		// Load the system module
		int32 Result = sceSysmoduleLoadModule(SCE_SYSMODULE_SCREEN_SHOT);
  
  		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_SCREEN_SHOT) failed. Error code: 0x%08x"), Result);

			// Don't continue setting things up
			return false;
  		}
		bIsInitialized = true;
	}
	
	return bIsInitialized;
}

void FPS4ShareScreenshot::Shutdown()
{
	if (bIsInitialized)
	{
		int32 Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_SCREEN_SHOT);
  		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_SCREEN_SHOT) failed. Error code: 0x%08x"), Result);
  		}
		
		// Assume that we terminated and unloaded the library successfully
		bIsInitialized = false;
	}
}
