/************************************************************************************

	Copyright © 2014-2019, 3dRudder SA, All rights reserved
	For terms of use: https://3drudder-dev.com/docs/introduction/sdk_licensing_agreement/
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met :

	* Redistributions of source code must retain the above copyright notice, and
	this list of conditions.
	* Redistributions in binary form must reproduce the above copyright
	notice and this list of conditions.
	* The name of 3dRudder may not be used to endorse or promote products derived from
	this software without specific prior written permission.

    Copyright 1998-2019 Epic Games, Inc. All Rights Reserved

************************************************************************************/

#include "3dRudderPlugin.h" 
#include "3dRudderPrivatePCH.h"

#include "Internationalization/Internationalization.h" // LOCTEXT
#include "InputCoreTypes.h"

// Settings
#include "3dRudderPluginSettings.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "ISettingsContainer.h"

// Plugins
#include "I3dRudderPlugin.h"

IMPLEMENT_MODULE(F3dRudderPlugin, _3dRudder)
DEFINE_LOG_CATEGORY_STATIC(_3dRudderPlugin, Log, All); 
#define LOCTEXT_NAMESPACE "3dRudderPlugin"

// This function is called by *Application.cpp after startup to instantiate the modules InputDevice
TSharedPtr< class IInputDevice > F3dRudderPlugin::CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	UE_LOG(_3dRudderPlugin, Log, TEXT("Create Input Device"));

	RegisterSettings();

	m_3dRudderDevice = MakeShareable(new F3dRudderDevice(InMessageHandler));	

	return m_3dRudderDevice;
}

// This function may be called during shutdown to clean up the module.
void F3dRudderPlugin::ShutdownModule()
{
	UnregisterSettings();

	m_3dRudderDevice->~F3dRudderDevice();

	UE_LOG(_3dRudderPlugin, Log, TEXT("Shutdown Module"));
}

void F3dRudderPlugin::RegisterSettings()
{
	// Registering some settings is just a matter of exposijg the default UObject of
	// your desired class, feel free to add here all those settings you want to expose
	// to your LDs or artists.

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		// Register the settings
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "3dRudder",
			LOCTEXT("RuntimeGeneralSettingsName", "3dRudder"),
			LOCTEXT("RuntimeGeneralSettingsDescription", "Input configuration for 3dRudder"),
			GetMutableDefault<U3dRudderPluginSettings>()
		);
	}
}

void F3dRudderPlugin::UnregisterSettings()
{
	// Ensure to unregister all of your registered settings here, hot-reload would
	// otherwise yield unexpected results.

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "3dRudder", "General");
	}
}

#undef LOCTEXT_NAMESPACE