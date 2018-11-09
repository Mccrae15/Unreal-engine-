// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FOnlineFactoryPS4Server;

/**
 * Online subsystem module class (PS4 dedicated server implementation)
 * Code related to the loading of the module
 */
class FOnlineSubsystemPS4ServerModule : public IModuleInterface
{
private:

	/** Class responsible for creating instance(s) of the subsystem */
	TUniquePtr<FOnlineFactoryPS4Server> PS4ServerFactory;

public:

	// IModuleInterface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	virtual bool SupportsAutomaticShutdown() override
	{
		return false;
	}
};
