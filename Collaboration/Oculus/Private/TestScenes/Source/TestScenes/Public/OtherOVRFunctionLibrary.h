// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OculusXRHMD.h"
#include "OtherOVRFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class TESTSCENES_API UOtherOVRFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Other VR Functions")
		static float GetScreenPercentage();

};
