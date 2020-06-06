// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyPlatformOutputDevices.h"
#include "Misc/OutputDeviceHelper.h"

/*FString FSonyOutputDevices::GetAbsoluteLogFilename()
{

#if UE_BUILD_SHIPPING

	// In shipping builds store log file to temporary directory (/temp0)

	static TCHAR Filename[1024] = { 0 };

	if( !Filename[0] )
	{
		const FString& TempDirectory = FSonyPlatformFile::GetTempDirectory();
		FCString::Strcpy( Filename, *TempDirectory );
		FCString::Strcat( Filename, TEXT( "/UE4.log" ) );
	}

	return Filename;

#else

	return FGenericPlatformOutputDevices::GetAbsoluteLogFilename();

#endif
}*/
