// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformInput.h"

struct INPUTCORE_API FPS4PlatformInput : FGenericPlatformInput
{
	static uint32 GetKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings);
};

typedef FPS4PlatformInput FPlatformInput;
