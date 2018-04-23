// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformOutputDevices.h"

struct CORE_API FPS4OutputDevices : public FGenericPlatformOutputDevices
{
	static FString GetAbsoluteLogFilename();
};

typedef FPS4OutputDevices FPlatformOutputDevices;