// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct APPLICATIONCORE_API FPS4ApplicationMisc : public FGenericPlatformApplicationMisc
{
	static class FFeedbackContext* GetFeedbackContext();
	static class FOutputDeviceError* GetErrorOutputDevice();
	static class GenericApplication* CreateApplication();
	static bool RequiresVirtualKeyboard();
};

typedef FPS4ApplicationMisc FPlatformApplicationMisc;
