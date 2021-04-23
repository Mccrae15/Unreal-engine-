// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMDRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// UOculusHMDRuntimeSettings

#include "OculusHMD_Settings.h"

UOculusHMDRuntimeSettings::UOculusHMDRuntimeSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, bAutoEnabled(true)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	// FSettings is the sole source of truth for Oculus default settings
	OculusHMD::FSettings DefaultSettings; 
	bSupportsDash = DefaultSettings.Flags.bSupportsDash;
	bCompositesDepth = DefaultSettings.Flags.bCompositeDepth;
	bHQDistortion = DefaultSettings.Flags.bHQDistortion;
	FFRLevel = DefaultSettings.FFRLevel;
	FFRDynamic = DefaultSettings.FFRDynamic;
	CPULevel = DefaultSettings.CPULevel;
	GPULevel = DefaultSettings.GPULevel;
	PixelDensityMin = DefaultSettings.PixelDensityMin;
	PixelDensityMax = DefaultSettings.PixelDensityMax;
	bFocusAware = DefaultSettings.Flags.bFocusAware;
	bEnableSpecificColorGamut = DefaultSettings.bEnableSpecificColorGamut;
	ColorSpace = DefaultSettings.ColorSpace;
	bRequiresSystemKeyboard = DefaultSettings.Flags.bRequiresSystemKeyboard;
	HandTrackingSupport = DefaultSettings.HandTrackingSupport;
	HandTrackingFrequency = DefaultSettings.HandTrackingFrequency;
#if WITH_LATE_LATCHING_CODE
	bLateLatching = DefaultSettings.bLateLatching;
#endif
	bPhaseSync = DefaultSettings.bPhaseSync;

#else
	// Some set of reasonable defaults, since blueprints are still available on non-Oculus platforms.
	bSupportsDash = false;
	bCompositesDepth = false;
	bHQDistortion = false;
	FFRLevel = EFixedFoveatedRenderingLevel::EFixedFoveatedRenderingLevel_Off;
	FFRDynamic = false;
	CPULevel = 2;
	GPULevel = 3;
	PixelDensityMin = 0.5f;
	PixelDensityMax = 1.0f;
	bFocusAware = true;
	bEnableSpecificColorGamut = false;
	ColorSpace = EColorSpace::Unknown;
	bRequiresSystemKeyboard = false;
	HandTrackingSupport = EHandTrackingSupport::ControllersOnly;
	HandTrackingFrequency = EHandTrackingFrequency::Low;
#if WITH_LATE_LATCHING_CODE
	bLateLatching = false;
#endif
	bPhaseSync = false;
#endif

	LoadFromIni();
}

void UOculusHMDRuntimeSettings::LoadFromIni()
{
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	bool v;
	float f;
	FVector vec;

	if (GConfig->GetFloat(OculusSettings, TEXT("PixelDensityMax"), f, GEngineIni))
	{
		check(!FMath::IsNaN(f));
		PixelDensityMax = f;
	}
	if (GConfig->GetFloat(OculusSettings, TEXT("PixelDensityMin"), f, GEngineIni))
	{
		check(!FMath::IsNaN(f));
		PixelDensityMin = f;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bHQDistortion"), v, GEngineIni))
	{
		bHQDistortion = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bCompositeDepth"), v, GEngineIni))
	{
		bCompositesDepth = v;
	}
}