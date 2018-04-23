// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "ModuleInterface.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"
#include "PS4TargetSettings.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Class.h"
#include "PS4TargetSettingsDetails.h"

#define LOCTEXT_NAMESPACE "FPS4PlatformEditorModule"

/**
 * Module for PS4 platform editor utilities
 */
class FPS4PlatformEditorModule
	: public IModuleInterface
{
	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// register detail customization
		static FName PropertyEditor("PropertyEditor");
		FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>(PropertyEditor);

		if (PropertyModule != nullptr)
		{
			static FName PS4TargetSettings("PS4TargetSettings");
			PropertyModule->RegisterCustomClassLayout(PS4TargetSettings, FOnGetDetailCustomizationInstance::CreateStatic(&FPS4TargetSettingsDetails::MakeInstance));
		}

		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "PS4",
				LOCTEXT("TargetSettingsName", "PlayStation 4"),
				LOCTEXT("TargetSettingsDescription", "PlayStation 4 project settings"),
				GetMutableDefault<UPS4TargetSettings>()
			);
		}

	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "PS4");
		}
	}
};


IMPLEMENT_MODULE(FPS4PlatformEditorModule, PS4PlatformEditor);

#undef LOCTEXT_NAMESPACE
