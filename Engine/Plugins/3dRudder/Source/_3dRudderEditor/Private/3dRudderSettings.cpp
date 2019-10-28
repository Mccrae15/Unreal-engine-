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

#include "3dRudderSettings.h"

DEFINE_LOG_CATEGORY_STATIC(_3dRudderSettings, Log, All)
#define LOCTEXT_NAMESPACE "3dRudderSettings"

U3dRudderSettings::U3dRudderSettings(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bActive = false;
	Translation = FVector::OneVector;
	Rotation = 1.0f;
	AxesParamsClassName = FSoftObjectPath(FString("/3dRudder/Default.Default"));
	LoadAxesParam();
}

void U3dRudderSettings::LoadAxesParam()
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
void U3dRudderSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
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