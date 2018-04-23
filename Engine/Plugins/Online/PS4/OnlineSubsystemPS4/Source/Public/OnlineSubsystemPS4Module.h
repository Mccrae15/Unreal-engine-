// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterface.h"

/**
 * Online subsystem module class (PS4 Implementation)
 * Code related to the loading and handling of the PS4 module.
 */
class FOnlineSubsystemPS4Module : public IModuleInterface
{
private:

	/** Class responsible for creating instance(s) of the subsystem */
	class FOnlineFactoryPS4* PS4Factory;

public:

	FOnlineSubsystemPS4Module() :
		PS4Factory(NULL)
	{
	}

	virtual ~FOnlineSubsystemPS4Module() {}

	//~ Begin IModuleInterface Interface
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
	//~ End IModuleInterface Interface
};

typedef TSharedPtr<FOnlineSubsystemPS4Module, ESPMode::ThreadSafe> FOnlineSubsystemPS4ModulePtr;

