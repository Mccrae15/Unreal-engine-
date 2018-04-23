// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "IInputDeviceModule.h"

/**
 * The public interface to this module.
 */
class IAimController : public IInputDeviceModule
{
public:

	/** Returns singleton instance, loading the module on demand if needed */
	static inline IAimController& Get()
	{
		return FModuleManager::LoadModuleChecked< IAimController >( "AimController" );
	}

	/** Returns True if the module is loaded and ready to use */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "AimController" );
	}
};

