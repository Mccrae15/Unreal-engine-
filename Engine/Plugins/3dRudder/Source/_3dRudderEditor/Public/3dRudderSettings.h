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

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AxesParamAsset.h"
#include "3dRudderSettings.generated.h"

/**
* Setting object used to hold both config settings and editable ones in one place
* To ensure the settings are saved to the specified config file make sure to add
* props using the globalconfig or config meta.
*/
UCLASS(config = Editor, defaultconfig)
class U3dRudderSettings : public UObject
{
	GENERATED_BODY()

public:

	U3dRudderSettings(const FObjectInitializer& ObjectInitializer);
	void LoadAxesParam();

	// UObject overrides
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Enable/Disable */
	UPROPERTY(EditAnywhere, config, Category = Move)
		bool bActive;

	/** Speed Translation */
	UPROPERTY(EditAnywhere, config, Category = Speed, meta = (EditCondition = "bActive"))
		FVector Translation;

	/** Speed Rotation (Yaw) */
	UPROPERTY(EditAnywhere, config, Category = Speed, meta = (EditCondition = "bActive", DisplayName = "Rotation" ))
		float Rotation;

	/** Path to the axes param asset*/
	UPROPERTY(config, EditAnywhere, Category = Locomotion, meta = (EditCondition = "bActive", AllowedClasses = "AxesParamAsset", DisplayName = "Editor Axes Param"))
		FSoftObjectPath AxesParamsClassName;

	/** Smooth Movement */
	UPROPERTY(EditAnywhere, config, Category = Locomotion)
		FSmoothMovement Smooth;

	ns3dRudder::IAxesParam* pAxesParam;
};