// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineAsyncTaskManager.h"

class FOnlineSubsystemPS4;

/**
 * Root of PS4 online aync tasks
 */
class FOnlineAsyncTaskPS4 : public FOnlineAsyncTaskBasic<FOnlineSubsystemPS4>
{
public:
	/**
	 * Constructor
	 * @param Subsystem the owning subsystem
	 */
	FOnlineAsyncTaskPS4(FOnlineSubsystemPS4* const Subsystem)
		: FOnlineAsyncTaskBasic(Subsystem)
	{
	}

	/** Destructor */
	virtual ~FOnlineAsyncTaskPS4() = default;

	/**
	 * Print out the server error from the NPToolkit
	 * @param Prefix string to prefix the log lines with
	 * @param ServerError the server error to print
	 */
	void PrintNPToolkitServerError(const FString& Prefix, const NpToolkit::Core::ServerError& ServerError);

private:
	/** Default constructor disabled */
	FOnlineAsyncTaskPS4() = delete;
};


/**
 * PS4 version of the async task manager to register the various PS4 callbacks with the engine
 */
class FOnlineAsyncTaskManagerPS4 : public FOnlineAsyncTaskManager
{
protected:
	/** reference to the online subsystem */
	FOnlineSubsystemPS4* const PS4Subsystem;

public:
	/**
	 * Constructor
	 * @param Subsystem the owning subsystem
	 */
	FOnlineAsyncTaskManagerPS4(FOnlineSubsystemPS4* InOnlineSubsystem)
		: PS4Subsystem(InOnlineSubsystem)
	{
	}

	/** Destructor */
	virtual ~FOnlineAsyncTaskManagerPS4()
	{
	}

	//~ Begin FOnlineAsyncTaskManager
	virtual void OnlineTick() override;
	//~ End FOnlineAsyncTaskManager

	/**
	 * Check if the current thread is the online thread
	 * @see IsOnGameThread
	 * @return true if we are on the online thread, false if not
	 */
	bool IsInOnlineThread() const;
};
