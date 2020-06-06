// Copyright Epic Games, Inc. All Rights Reserved.
//

#include "MorpheusStereoLayers.h"
#include "MorpheusHMD.h"
#include "SceneViewExtension.h"

#include "CoreMinimal.h"
#include "IMorpheusPlugin.h"

//=============================================================================
void FMorpheusStereoLayers::UpdateSplashScreen()
{
#if HAS_MORPHEUS_HMD_SDK
	if (bSplashIsShown)
	{
		static_cast<FMorpheusHMD*>(HMDDevice)->Show2DVRSplashScreen(
			(bSplashShowMovie && SplashMovie.IsValid()) ? SplashMovie : SplashTexture,
			SplashScale, 
			FVector2D(SplashOffset.X, SplashOffset.Y) // TODO: Update morpheus 2d splash screen to support 4 component offset
			);
	}
	else
	{
		static_cast<FMorpheusHMD*>(HMDDevice)->Hide2DVRSplashScreen();
	}
#endif
}

#if HAS_MORPHEUS_HMD_SDK

IStereoLayers* FMorpheusHMD::GetStereoLayers()
{
	if (!DefaultStereoLayers.IsValid())
	{
		TSharedPtr<FMorpheusStereoLayers, ESPMode::ThreadSafe> NewLayersPtr = FSceneViewExtensions::NewExtension<FMorpheusStereoLayers>(this);
		DefaultStereoLayers = StaticCastSharedPtr<FDefaultStereoLayers>(NewLayersPtr);
	}
	return DefaultStereoLayers.Get();
}

#endif