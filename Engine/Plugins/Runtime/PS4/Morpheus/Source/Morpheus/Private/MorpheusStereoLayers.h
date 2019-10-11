// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "IMorpheusPlugin.h"

#include "DefaultStereoLayers.h"
#if MORPHEUS_SUPPORTED_PLATFORMS

class FMorpheusStereoLayers : public FDefaultStereoLayers
{
public:
	FMorpheusStereoLayers(const class FAutoRegister& AutoRegister, class FHeadMountedDisplayBase* InHmd) : FDefaultStereoLayers(AutoRegister, InHmd) {}

	//=============================================================================
	virtual void UpdateSplashScreen() override;

	virtual bool ShouldCopyDebugLayersToSpectatorScreen() const override { return true; }
};
#endif //MORPHEUS_SUPPORTED_PLATFORMS