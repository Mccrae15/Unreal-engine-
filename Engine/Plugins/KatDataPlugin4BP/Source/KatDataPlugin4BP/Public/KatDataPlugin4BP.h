// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "ModuleManager.h"
#include "Core.h"

class FKatDataPlugin4BPModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};