// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyPlatformApplicationMisc.h"
#include "SonyApplication.h"
#include "SonyInputInterface.h"
#include "SonyFeedbackContext.h"
#include "SonyErrorOutputDevice.h"

class FFeedbackContext* FSonyApplicationMisc::GetFeedbackContext()
{
	static FSonyFeedbackContext Singleton;
	return &Singleton;
}

class FOutputDeviceError* FSonyApplicationMisc::GetErrorOutputDevice()
{
	static FSonyErrorOutputDevice Singleton;
	return &Singleton;
}

bool FSonyApplicationMisc::RequiresVirtualKeyboard()
{
	return true;
}
