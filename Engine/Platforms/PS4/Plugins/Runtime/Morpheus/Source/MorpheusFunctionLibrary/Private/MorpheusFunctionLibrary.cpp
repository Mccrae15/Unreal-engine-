// Copyright Epic Games, Inc. All Rights Reserved.
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

FMorpheusHMD* GetMorpheusHMD()
{
#if HAS_MORPHEUS_HMD_SDK
	if (GEngine->XRSystem->GetSystemName() == FMorpheusHMD::MorpheusSystemName)
	{
		return static_cast<FMorpheusHMD*>(GEngine->XRSystem.Get());
	}
#endif

	return nullptr;
}

void UMorpheusFunctionLibrary::Show2DVRSplashScreen(class UTexture* Texture, FVector2D Scale, FVector2D Offset)
{
#if HAS_MORPHEUS_HMD_SDK
	FMorpheusHMD* MorpheusHMD = GetMorpheusHMD();
	if (MorpheusHMD)
	{
		MorpheusHMD->Show2DVRSplashScreen(Texture, Scale, Offset);
	}
#endif
}

void UMorpheusFunctionLibrary::Hide2DVRSplashScreen()
{
#if HAS_MORPHEUS_HMD_SDK
	FMorpheusHMD* MorpheusHMD = GetMorpheusHMD();
	if (MorpheusHMD)
	{
		MorpheusHMD->Hide2DVRSplashScreen();
	}
#endif
}

void UMorpheusFunctionLibrary::HMDReprojectionSetOutputMinColor(FLinearColor MinColor)
{
#if HAS_MORPHEUS_HMD_SDK
	FMorpheusHMD* MorpheusHMD = GetMorpheusHMD();
	if (MorpheusHMD)
	{
		MorpheusHMD->HMDReprojectionSetOutputMinColor(MinColor);
	}
#endif
}
