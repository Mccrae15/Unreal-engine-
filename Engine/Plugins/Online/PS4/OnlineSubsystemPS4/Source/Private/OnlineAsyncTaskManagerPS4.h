// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemPS4.h"

/**
 * Root of PS4 online aync tasks
 */
class FOnlineAsyncTaskPS4 : public FOnlineAsyncTaskBasic<FOnlineSubsystemPS4>
{
PACKAGE_SCOPE:

	/** Hidden as this is the established pattern. */
	FOnlineAsyncTaskPS4() :
		FOnlineAsyncTaskBasic(NULL)
	{
	}

public:
	FOnlineAsyncTaskPS4(class FOnlineSubsystemPS4* InPS4Subsystem) :
		FOnlineAsyncTaskBasic(InPS4Subsystem)
	{
	}

	virtual ~FOnlineAsyncTaskPS4()
	{
	}
};


/**
 *	PS4 version of the async task manager to register the various PS4 callbacks with the engine
 */
class FOnlineAsyncTaskManagerPS4 : public FOnlineAsyncTaskManager
{
protected:

	/** Cached reference to the main online subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

public:


	FOnlineAsyncTaskManagerPS4(class FOnlineSubsystemPS4* InOnlineSubsystem)
		: PS4Subsystem(InOnlineSubsystem)
	{
	}

	virtual ~FOnlineAsyncTaskManagerPS4()
	{
	}

	// FOnlineAsyncTaskManager
	virtual void OnlineTick() override;
};
