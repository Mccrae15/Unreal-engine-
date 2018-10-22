// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Misc/App.h"
#include "Misc/FeedbackContext.h"
#include "Misc/OutputDeviceRedirector.h"

/**
 * Feedback context implementation for PS4.
 */
class FPS4FeedbackContext : public FFeedbackContext
{
public:

	/** Default constructor. */
	FPS4FeedbackContext();

	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override;
	virtual bool YesNof(const FText& Question) override;
};
