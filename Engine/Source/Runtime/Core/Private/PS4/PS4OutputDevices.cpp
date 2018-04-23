// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4OutputDevices.h"
#include "OutputDeviceHelper.h"
#include "PS4File.h"

FString FPS4OutputDevices::GetAbsoluteLogFilename()
{

#if UE_BUILD_SHIPPING

	// In shipping builds store log file to temporary directory (/temp0)

	static TCHAR Filename[1024] = { 0 };

	if( !Filename[0] )
	{
		const FString& TempDirectory = FPS4PlatformFile::GetTempDirectory();
		FCString::Strcpy( Filename, *TempDirectory );
		FCString::Strcat( Filename, TEXT( "/UE4.log" ) );
	}

	return Filename;

#else

	return FGenericPlatformOutputDevices::GetAbsoluteLogFilename();

#endif
}
