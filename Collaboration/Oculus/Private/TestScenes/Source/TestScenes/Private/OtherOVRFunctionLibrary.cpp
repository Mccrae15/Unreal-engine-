// Fill out your copyright notice in the Description page of Project Settings.

#include "OtherOVRFunctionLibrary.h"

float UOtherOVRFunctionLibrary::GetScreenPercentage()
{
	FDynamicResolutionStateInfos DynamicResolutionStateInfos;
	GEngine->GetDynamicResolutionCurrentStateInfos(DynamicResolutionStateInfos);

	return DynamicResolutionStateInfos.ResolutionFractionApproximation > 0.0f ? DynamicResolutionStateInfos.ResolutionFractionApproximation : 1.0f;
}
