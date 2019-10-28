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
#include "Runtime/Engine/Classes/Engine/DataAsset.h"
#define _3DRUDDER_SDK_STATIC
#include "3DRudderSDK.h"
#include "AxesParamAsset.generated.h"

USTRUCT()
struct FMyCurve 
{
	GENERATED_USTRUCT_BODY()
		FMyCurve();
		FMyCurve(float deadzone, float sensitivity, float shape);

	UPROPERTY(EditAnywhere, Category = "3dRudder Curve", meta = (ClampMin = "0.0", ClampMax = "0.99"))
		float DeadZone;

	UPROPERTY(EditAnywhere, Category = "3dRudder Curve", meta = (ClampMin = "0.0", ClampMax = "2.0"))
		float Sensitivity;

	UPROPERTY(EditAnywhere, Category = "3dRudder Curve", meta = (ClampMin = "0.1", ClampMax = "5.0"))
		float Shape;
};

USTRUCT(Blueprintable)
struct FSmoothFactor
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "3dRudder Smooth")
		bool Enable;
	UPROPERTY(EditAnywhere, Category = "3dRudder Smooth", meta = (EditCondition = "Enable"))
		float Smoothness;

	float CurrentSpeed;

	FSmoothFactor()
	{
		Enable = false;
		Smoothness = 0.15f;
		CurrentSpeed = 0.0f;
	}

	float ComputeSpeed(float input, float deltatime)
	{
		float speedTarget = input; // m/s            
		float acceleration = (speedTarget - CurrentSpeed) / Smoothness; // m/s²            
		CurrentSpeed = CurrentSpeed + (acceleration * deltatime); // m/s            
		return CurrentSpeed;
	}
};

USTRUCT(Blueprintable)
struct FSmoothMovement
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "3dRudder Smooth")
		FSmoothFactor LeftRight;

	UPROPERTY(EditAnywhere, Category = "3dRudder Smooth")
		FSmoothFactor ForwardBackward;

	UPROPERTY(EditAnywhere, Category = "3dRudder Smooth")
		FSmoothFactor UpDown;

	UPROPERTY(EditAnywhere, Category = "3dRudder Smooth")
		FSmoothFactor Rotation;
};

/** Defines the Axes Param type*/
UENUM(BlueprintType)
enum class E3dRudderAxesParam : uint8
{
	Default = 0,
	NormalizedLinear,
	Custom,
};

/**
 * Axes param asset for 3dRudder controller
 */
UCLASS(Category = "3dRudder", BlueprintType, Blueprintable)
class _3DRUDDER_API UAxesParamAsset :  public UDataAsset
{
	GENERATED_BODY()

public:

	UAxesParamAsset(const FObjectInitializer& ObjectInitializer);
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;	

	// UObject overrides
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	ns3dRudder::IAxesParam* GetAxesParam() const { return pAxesParam; }

	UPROPERTY(EditAnywhere, Category = "Axes Param")
		E3dRudderAxesParam AxesParamType;

	UPROPERTY(EditAnywhere, Category = "Axes Param")
		bool NonSymmetricalPitch;

	UPROPERTY(EditAnywhere, Category = "Axes Param", meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float RollToYawCompensation;

	UPROPERTY(EditAnywhere, Category = "Axes Param")
		FMyCurve LeftRight;

	UPROPERTY(EditAnywhere, Category = "Axes Param")
		FMyCurve ForwardBackward;

	UPROPERTY(EditAnywhere, Category = "Axes Param")
		FMyCurve UpDown;

	UPROPERTY(EditAnywhere, Category = "Axes Param")
		FMyCurve Rotation;

	UPROPERTY(EditAnywhere, Category = "Axes Param", AdvancedDisplay)
		bool Test;
	
	UPROPERTY(EditAnywhere, Category = "Axes Param", AdvancedDisplay, meta = (EditCondition="Test", ClampMin = "0", ClampMax = "4"))
		uint32 PortNumber;

private:
	void CreateAxesParam();
	ns3dRudder::IAxesParam* pAxesParam;
};

class _3DRUDDER_API CAxesParamCustom : public ns3dRudder::IAxesParam
{
public:
	CAxesParamCustom(UAxesParamAsset* passet);	
	ns3dRudder::ErrorCode UpdateParam(uint32_t nPortNumber);

private:
	UAxesParamAsset* asset;
	ns3dRudder::Curve m_Curve[ns3dRudder::MaxAxes];
};