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
#include "Components/ActorComponent.h"
#include "AxesParamAsset.h"
#define _3DRUDDER_SDK_STATIC
#include "3DRudderSDK.h"
#include "3dRudderFunctionLibrary.h"
#include "3dRudderComponent.generated.h"

USTRUCT(Blueprintable)
struct FSpeedFactor
{
	GENERATED_USTRUCT_BODY()
	
	FSpeedFactor();

	UPROPERTY(EditAnywhere, Category = "3dRudder")
		float LeftRight;

	UPROPERTY(EditAnywhere, Category = "3dRudder")
		float ForwardBackward;

	UPROPERTY(EditAnywhere, Category = "3dRudder")
		float UpDown;

	UPROPERTY(EditAnywhere, Category = "3dRudder")
		float Rotation;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DisplayName = "3dRudder Actor")
class U3dRudderComponent : public UActorComponent
{
	GENERATED_BODY()

public:		

	// Sets default values for this component's properties
	U3dRudderComponent();
	~U3dRudderComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	//virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void OnConnect(const uint32 PortNumber, const bool Connected);

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "3dRudder Actor", meta = (ClampMin = "0", ClampMax = "4"))
		int32 Port;

	UPROPERTY(EditAnywhere, Category = "3dRudder Actor")
		FSpeedFactor SpeedFactor;

	UPROPERTY(EditAnywhere, Category = "3dRudder Actor")
		FSmoothMovement Smooth;	

	UPROPERTY(EditAnywhere, Category = "3dRudder Actor")
		UAxesParamAsset* AxesParam;	

	UFUNCTION(BlueprintPure, Category = "3dRudder Actor")
		bool IsConnected();

	UFUNCTION(BlueprintPure, Category = "3dRudder Actor")
		FString GetFirmwareVersion();

	UFUNCTION(BlueprintPure, Category = "3dRudder Actor")
		E3dRudderStatus GetStatus();

	UFUNCTION(BlueprintPure, Category = "3dRudder Actor")
		FString GetStatusString();

	UFUNCTION(BlueprintCallable, Category = "3dRudder Actor")
		bool PlaySound(int32 frequency, int32 duration);

	UFUNCTION(BlueprintPure, Category = "3dRudder Actor")
		E3dRudderError GetAxes(float DeltaTime, float &LeftRight, float &ForwardBackward, float &UpDown, float &Rotation);

	UFUNCTION(BlueprintPure, Category = "3dRudder Actor")
		void GetSensor(int32 &sensor1, int32 &sensor2, int32 &sensor3, int32 &sensor4, int32 &sensor5, int32 &sensor6);

	// This is declaration of "Static Multicast Delegate".
	// Plugin can broadcast it to any other parts of the code that has been binded to it.
	/*DECLARE_MULTICAST_DELEGATE_OneParam(FXPlugin_OnReceivedTestMessageDelegate, const FString&);
		static FXPlugin_OnReceivedTestMessageDelegate OnReceivedTestMessageDelegate;*/

	// This is declaration of "Dynamic Multicast Delegate". 
	// This is the delegate that will be invoking the Blueprint Event.
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStatusChanged, const FString&, Status);
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Status changed"))
		FOnStatusChanged OnStatusChangedDelegate;
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn3dRudderConnected, const bool, Connected);
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On 3dRudder connected"))
		FOn3dRudderConnected On3dRudderConnected;
		
private:

	ns3dRudder::Status m_Status;
	ns3dRudder::IAxesParam *m_pAxesParam;
	FDelegateHandle delegateHandle;
};
