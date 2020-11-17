// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformInput.h"

struct INPUTCORE_API FSonyPlatformInput : public FGenericPlatformInput
{
	static uint32 GetKeyMap(uint32* KeyCodes, FString* KeyNames, uint32 MaxMappings);
};
