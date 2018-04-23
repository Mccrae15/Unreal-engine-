// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4ApplicationMisc.h"
#include "PS4Application.h"
#include "PS4InputInterface.h"
#include "PS4FeedbackContext.h"
#include "PS4ErrorOutputDevice.h"

class FFeedbackContext* FPS4ApplicationMisc::GetFeedbackContext()
{
	static FPS4FeedbackContext Singleton;
	return &Singleton;
}

class FOutputDeviceError* FPS4ApplicationMisc::GetErrorOutputDevice()
{
	static FPS4ErrorOutputDevice Singleton;
	return &Singleton;
}

GenericApplication* FPS4ApplicationMisc::CreateApplication()
{
	return FPS4Application::CreatePS4Application();
}

bool FPS4ApplicationMisc::RequiresVirtualKeyboard()
{
	FPS4InputInterface* InputInterface = (FPS4InputInterface*)FPS4Application::GetPS4Application()->GetInputInterface();
	if (InputInterface)
	{
		return !InputInterface->HasExternalKeyboard();
	}
	return true;
}
