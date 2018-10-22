// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
//

#include "MorpheusStereoLayers.h"
#include "MorpheusHMD.h"
#include "SceneViewExtension.h"

#include "CoreMinimal.h"
#include "IMorpheusPlugin.h"

#if MORPHEUS_SUPPORTED_PLATFORMS


//=============================================================================
void FMorpheusStereoLayers::UpdateSplashScreen()
{
	if (bSplashIsShown)
	{
		static_cast<FMorpheusHMD*>(HMDDevice)->Show2DVRSplashScreen(
			(bSplashShowMovie && SplashMovie.IsValid()) ? SplashMovie : SplashTexture,
			SplashScale, 
			SplashOffset
			);
	}
	else
	{
		static_cast<FMorpheusHMD*>(HMDDevice)->Hide2DVRSplashScreen();
	}
}

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
