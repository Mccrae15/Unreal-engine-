// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "ModuleManager.h"
#include "Misc/MessageDialog.h"

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = NULL;

	// Load the dynamic RHI module.
	IDynamicRHIModule* DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("PS4RHI"));

	// Create the dynamic RHI.
	if (!DynamicRHIModule->IsSupported())
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("PS4", "DynamicRHIFailure", "PS4RHI failure?"));
		FPlatformMisc::RequestExit(1);
	}
	else
	{
		DynamicRHI = DynamicRHIModule->CreateRHI();
	}

	return DynamicRHI;
}
