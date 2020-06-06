// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SonyPlatformApplicationMisc.h"
#include "PS4Application.h"

struct APPLICATIONCORE_API FPS4ApplicationMisc : public FSonyApplicationMisc
{
	static inline FPS4Application* CreateApplication() { return FPS4Application::CreatePS4Application(); }
};

typedef FPS4ApplicationMisc FPlatformApplicationMisc;
