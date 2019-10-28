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

#pragma once
 
#include "Engine.h"
#include "Modules/ModuleManager.h"
#include "UnrealEd.h"
#define _3DRUDDER_SDK_STATIC
#include "3DRudderSDK.h"

class FSlateStyleSet;

class F3dRudderEditorModule: public IModuleInterface, public FTickableEditorObject
{
public:

	static ns3dRudder::CSdk* s_pSdk;

	// Module
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Settings
	void RegisterSettings();
	void UnregisterSettings(); 

	// Details custom
	void RegisterCustomizations();
	void UnregisterCustomizations();

	// Style custom
	void RegisterStyle();
	void unRegisterStyle();

	// Tickable
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const
	{
		return true;
	}
	virtual TStatId GetStatId() const
	{
		return TStatId();
	};

	// Editor
	void UpdateViewportCamera(const FVector& translation, float fRotation);

private:

	/** Slate styleset used by this module. **/
	TSharedPtr< FSlateStyleSet > StyleSet;
};