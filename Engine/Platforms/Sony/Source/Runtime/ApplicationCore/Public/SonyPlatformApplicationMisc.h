// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct APPLICATIONCORE_API FSonyApplicationMisc : public FGenericPlatformApplicationMisc
{
	static class FFeedbackContext* GetFeedbackContext();
	static class FOutputDeviceError* GetErrorOutputDevice();
	static bool RequiresVirtualKeyboard();
};
