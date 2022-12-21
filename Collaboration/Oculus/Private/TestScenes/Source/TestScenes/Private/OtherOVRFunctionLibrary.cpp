// Fill out your copyright notice in the Description page of Project Settings.

#include "OtherOVRFunctionLibrary.h"

float UOtherOVRFunctionLibrary::GetScreenPercentage()
{
	FDynamicResolutionStateInfos DynamicResolutionStateInfos;
	GEngine->GetDynamicResolutionCurrentStateInfos(DynamicResolutionStateInfos);

	return DynamicResolutionStateInfos.ResolutionFractionApproximations[GDynamicPrimaryResolutionFraction] > 0.0f ? DynamicResolutionStateInfos.ResolutionFractionApproximations[GDynamicPrimaryResolutionFraction] : 1.0f;
}
