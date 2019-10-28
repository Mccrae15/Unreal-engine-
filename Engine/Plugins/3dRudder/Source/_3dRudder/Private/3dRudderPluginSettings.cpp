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

#include "3dRudderPluginSettings.h"

DEFINE_LOG_CATEGORY_STATIC(_3dRudderPluginSettings, Log, All)
#define LOCTEXT_NAMESPACE "3dRudderPluginSettings"

U3dRudderPluginSettings::U3dRudderPluginSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{	
	AxesParamsClassName = FSoftObjectPath(FString("/3dRudder/Default.Default"));
	ForwardThreshold = 0.5f;
	BackwardThreshold = 0.5f;
	LeftThreshold = 0.5f;
	RightThreshold = 0.5f;
	UpThreshold = 0.5f;
	DownThreshold = 0.5f;
	RotationLeftThreshold = 0.5f;
	RotationRightTreshold = 0.5f;
	LoadAxesParam();
}

void U3dRudderPluginSettings::LoadAxesParam()
{
	if (AxesParamsClassName.IsValid())
	{
		UAxesParamAsset* pAxesParamAsset = LoadObject<UAxesParamAsset>(nullptr, *AxesParamsClassName.ToString());
		if (pAxesParamAsset != nullptr)
			pAxesParam = pAxesParamAsset->GetAxesParam();
		else
		{
			AxesParamsClassName = FSoftObjectPath(FString("/3dRudder/Default.Default"));
			pAxesParamAsset = LoadObject<UAxesParamAsset>(nullptr, *AxesParamsClassName.ToString());
			pAxesParam = pAxesParamAsset->GetAxesParam();
		}
	}
}

#if WITH_EDITOR
void U3dRudderPluginSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	//UE_LOG(_3DRudderEditor, Warning, TEXT("property change: %s"), *PropertyChangedEvent.Property->GetNameCPP());
	if (Name.ToString() == "AxesParamsClassName")
	{
		LoadAxesParam();
	}
}
#endif

#undef LOCTEXT_NAMESPACE