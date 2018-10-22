// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MorpheusRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// UMorpheusRuntimeSettings


UMorpheusRuntimeSettings::UMorpheusRuntimeSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, IPD(0.0635f)
, PSVRFrameSequence(EPSVRFrameSequence::Render60Scanout120)
, ReprojectionSamplerWrapMode(EReprojectionSamplerWrapMode::Mirror)
, bPositionTracking(true)
, bDisableHMDOrientationUntilHMDHasBeenTracked(true)
, HmdSetupDialogCanceledBehavior(EHmdSetupDialogCanceledBehavior::DisallowCancel)
, bEnableSocialScreenSeparateMode(false)
, DesktopOffset(0.0f, 0.0f, 0.0f)
, bEnableControllerPitch(true)
, bEnableControllerRoll(true)
, bChangePlayerView(true)
{
}
