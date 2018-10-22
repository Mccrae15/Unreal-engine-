// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MorpheusEditorModule.h"
#include "MorpheusRuntimeSettings.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Class.h"

// Settings
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "MorpheusEditor"

//////////////////////////////////////////////////////////////////////////
// FMorpheusEditor

class FMorpheusEditor : public IMorpheusEditorModule   
{
public:
	virtual void StartupModule() override
	{				
		RegisterSettings();
	}

	virtual void ShutdownModule() override
	{
		if (UObjectInitialized())
		{
			UnregisterSettings();  
		}		
	}
private:
	
	void RegisterSettings()
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "Morpheus",
				LOCTEXT("RuntimeSettingsName", "Morpheus"),
				LOCTEXT("RuntimeSettingsDescription", "Configure the Morpheus plugin"),
				GetMutableDefault<UMorpheusRuntimeSettings>()
			);
		}
	}

	void UnregisterSettings()
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "Morpheus");
		}
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FMorpheusEditor, MorpheusEditor);

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE