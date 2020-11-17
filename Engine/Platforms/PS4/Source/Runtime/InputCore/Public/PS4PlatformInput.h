// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "SonyPlatformInput.h"

struct INPUTCORE_API FPS4PlatformInput : public FSonyPlatformInput
{
	static FKey GetGamepadAcceptKey();
	static FKey GetGamepadBackKey();
};

typedef FPS4PlatformInput FPlatformInput;
