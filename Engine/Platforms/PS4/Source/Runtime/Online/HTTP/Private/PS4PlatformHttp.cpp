// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4PlatformHttp.h"
#include "SonyHttp.h"

//The PS4 Net library
#include <net.h>

void FPS4PlatformHttp::Init()
{
	// Attempt to open an Internet connection

	UE_LOG(LogHttp, Log, TEXT("Initializing PS4 Http settings"));

	/*Net library initialization*/
	int RetValue = sceNetInit();
	// From PS4 Dev Net Returns a negative value for errors. Although there is no specific situation in which this function is presumed to return an error, the application must not malfunction even if an error is returned
	verify(RetValue > -1);

	FSonyPlatformHttp::Init();
}

void FPS4PlatformHttp::Shutdown()
{
	FSonyPlatformHttp::Shutdown();

	UE_LOG(LogHttp, Log, TEXT("Closing PS4 Http settings"));

	int ReturnCode = sceNetTerm();
	//Not checking return code, there is only one which is that is was not initialised, which will occur due to calling shutdown at the start of the initialise.
}
