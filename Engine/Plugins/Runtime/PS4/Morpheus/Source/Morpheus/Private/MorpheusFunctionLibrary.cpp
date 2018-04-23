// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
//
#include "MorpheusFunctionLibrary.h"
#include "MorpheusHMD.h"
#include "IXRTrackingSystem.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"

UMorpheusFunctionLibrary::UMorpheusFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

#if MORPHEUS_SUPPORTED_PLATFORMS
FMorpheusHMD* GetMorpheusHMD()
{
	if (GEngine->XRSystem->GetSystemName() == FMorpheusHMD::MorpheusSystemName)
	{
		return static_cast<FMorpheusHMD*>(GEngine->XRSystem.Get());
	}

	return nullptr;
}
#endif // MORPHEUS_SUPPORTED_PLATFORMS

void UMorpheusFunctionLibrary::Show2DVRSplashScreen(class UTexture* Texture, FVector2D Scale, FVector2D Offset)
{
#if MORPHEUS_SUPPORTED_PLATFORMS
	FMorpheusHMD* MorpheusHMD = GetMorpheusHMD();
	if (MorpheusHMD)
	{
		MorpheusHMD->Show2DVRSplashScreen(Texture, Scale, Offset);
	}
#endif // MORPHEUS_SUPPORTED_PLATFORMS
}

void UMorpheusFunctionLibrary::Hide2DVRSplashScreen()
{
#if MORPHEUS_SUPPORTED_PLATFORMS
	FMorpheusHMD* MorpheusHMD = GetMorpheusHMD();
	if (MorpheusHMD)
	{
		MorpheusHMD->Hide2DVRSplashScreen();
	}
#endif // MORPHEUS_SUPPORTED_PLATFORMS
}

void UMorpheusFunctionLibrary::HMDReprojectionSetOutputMinColor(FLinearColor MinColor)
{
#if MORPHEUS_SUPPORTED_PLATFORMS
	FMorpheusHMD* MorpheusHMD = GetMorpheusHMD();
	if (MorpheusHMD)
	{
		MorpheusHMD->HMDReprojectionSetOutputMinColor(MinColor);
	}
#endif // MORPHEUS_SUPPORTED_PLATFORMS
}
