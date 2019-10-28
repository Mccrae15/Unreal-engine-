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

#include "AxesParamAssetFactory.h"

UAxesParamAssetFactory::UAxesParamAssetFactory(const class FObjectInitializer &OBJ) : Super(OBJ) {
	SupportedClass = UAxesParamAsset::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UAxesParamAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UAxesParamAsset::StaticClass()));
	return NewObject<UAxesParamAsset>(InParent, Class, Name, Flags | RF_Transactional, Context);
}