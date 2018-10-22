// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4DeviceProfileSelectorModule.h"
#include "Modules/ModuleManager.h"
#include <system_service.h>
#include <kernel.h>
#include <video_out.h>

IMPLEMENT_MODULE(FPS4DeviceProfileSelectorModule, PS4DeviceProfileSelector);


void FPS4DeviceProfileSelectorModule::StartupModule()
{
}


void FPS4DeviceProfileSelectorModule::ShutdownModule()
{
}


FString const FPS4DeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	static FString ProfileName;
	
	if (ProfileName.IsEmpty())
	{
		ProfileName = FPlatformProperties::PlatformName();

		if( sceKernelIsNeoMode() )
		{
			ProfileName = "PS4_Neo";

			SceVideoOutResolutionStatus ResolutionStatus;
			int Handle = sceVideoOutOpen( SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL );
			if( Handle > 0 )
			{
				if( SCE_OK == sceVideoOutGetResolutionStatus( Handle, &ResolutionStatus ) && ResolutionStatus.fullHeight > 1080 )
				{
					// 4k Mode
					ProfileName = "PS4_Neo_4k";
				}
				sceVideoOutClose( Handle );
			}
		}

		UE_LOG( LogPS4, Log, TEXT( "Selected Device Profile: [%s]" ), *ProfileName );
	}

	return ProfileName;
}

