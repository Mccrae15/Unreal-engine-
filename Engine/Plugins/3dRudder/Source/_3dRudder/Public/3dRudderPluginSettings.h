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
#include "3dRudderPluginSettings.generated.h"

/**
* Setting object used to hold both config settings and editable ones in one place
* To ensure the settings are saved to the specified config file make sure to add
* props using the globalconfig or config meta.
*/
UCLASS(config = Input, defaultconfig)
class U3dRudderPluginSettings : public UObject
{
	GENERATED_BODY()

public:

	U3dRudderPluginSettings(const FObjectInitializer& ObjectInitializer);
	void LoadAxesParam();

	// UObject overrides
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif	

	/** Path to the axes param asset*/
	UPROPERTY(config, EditAnywhere, Category = "Axes Param", meta = (AllowedClasses = "AxesParamAsset", DisplayName = "Input Axes Param"))
		FSoftObjectPath AxesParamsClassName;

	/** Action input threshold*/
	UPROPERTY(config, EditAnywhere, Category = "Action Input Threshold", meta = (ClampMin = 0.0, ClampMax = 1.0, DisplayName = "Forward"))
		float ForwardThreshold;

	UPROPERTY(config, EditAnywhere, Category = "Action Input Threshold", meta = (ClampMin = 0.0, ClampMax = 1.0, DisplayName = "Backward"))
		float BackwardThreshold;

	UPROPERTY(config, EditAnywhere, Category = "Action Input Threshold", meta = (ClampMin = 0.0, ClampMax = 1.0, DisplayName = "Left"))
		float LeftThreshold;

	UPROPERTY(config, EditAnywhere, Category = "Action Input Threshold", meta = (ClampMin = 0.0, ClampMax = 1.0, DisplayName = "Right"))
		float RightThreshold;

	UPROPERTY(config, EditAnywhere, Category = "Action Input Threshold", meta = (ClampMin = 0.0, ClampMax = 1.0, DisplayName = "Up"))
		float UpThreshold;

	UPROPERTY(config, EditAnywhere, Category = "Action Input Threshold", meta = (ClampMin = 0.0, ClampMax = 1.0, DisplayName = "Down"))
		float DownThreshold;

	UPROPERTY(config, EditAnywhere, Category = "Action Input Threshold", meta = (ClampMin = 0.0, ClampMax = 1.0, DisplayName = "Left Rotation"))
		float RotationLeftThreshold;

	UPROPERTY(config, EditAnywhere, Category = "Action Input Threshold", meta = (ClampMin = 0.0, ClampMax = 1.0, DisplayName = "Right Rotation"))
		float RotationRightTreshold;

	ns3dRudder::IAxesParam* pAxesParam;
};