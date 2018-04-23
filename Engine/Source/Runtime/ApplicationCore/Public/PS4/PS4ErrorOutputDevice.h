// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"
#include "OutputDeviceError.h"

/**
 * Device for error output on PS4
 */
class FPS4ErrorOutputDevice : public FOutputDeviceError
{
public:
	/** Constructor */
	FPS4ErrorOutputDevice();

	/**
	* Serializes the passed in data unless the current event is suppressed.
	*
	* @param	Data	Text to log
	* @param	Event	Event name used for suppression purposes
	*/
	virtual void Serialize(const TCHAR* Msg, ELogVerbosity::Type Verbosity, const class FName& Category) override;

	/**
	* Error handling function that is being called from within the system wide global
	* error handler, e.g. using structured exception handling on the PC.
	*/
	virtual void HandleError() override;

private:
	FName LogOutputDevice;
};
