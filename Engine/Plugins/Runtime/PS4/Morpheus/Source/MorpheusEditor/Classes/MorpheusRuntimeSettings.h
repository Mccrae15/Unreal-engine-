// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "../../Morpheus/Public/MorpheusTypes.h"
#include "MorpheusRuntimeSettings.generated.h"

/**
* Implements the settings for the Morpheus plugin.
*/
UCLASS(config = Engine, defaultconfig)
class MORPHEUSEDITOR_API UMorpheusRuntimeSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	// Morpheus plugin enabled for this project.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Settings, meta = (
		DisplayName = "Enable Morpheus on PC",
		ToolTip = "If true PSVR can be run on PC.  This will result in a timeout delay on startup if no PSVR is connected."))
	bool bEnableMorpheus;

	//interpupillary distance.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Settings)
	float IPD;

	// All three modes do seem to work well and razor captures were as expected.  
	// The tracking timing of 90hz is a bit off of sony's reccomendation because we try to process camera data at 90hz, but it only 
	// takes pictures at 60, so every third call fails.  However it did give better results than 60/120 mode.  In particular the hmd
	// translation reprojection artifact (double image of close objects) does not occur.
	UPROPERTY(config, EditAnywhere, Category = Settings, AdvancedDisplay, meta = (
		DisplayName = "PSVR Frame Sequence",
		ToolTip = "Defines the framerate for rendering, reprojection, and hmd position/orientation sampling. See Sony 'VrTracker Library Overview' document 'Usage' section."))
	EPSVRFrameSequence PSVRFrameSequence;

	UPROPERTY(config, EditAnywhere, Category = Settings, meta = (
		DisplayName = "Reprojection Sampler Wrap Mode",
		ToolTip = "The method by which pixels outside the rendered area will be filled.  Mirror will work best for most content. Very black scenes or debugging might benefit from ClampBorder.  See Sony documentation for sceHmdReprojectionInitialize."))
	EReprojectionSamplerWrapMode ReprojectionSamplerWrapMode;

	//enable/disable head position tracking.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Settings)
	bool bPositionTracking;

	// Disable HMD orientation tracking until HMD has been tracked by the camera.  Reprojection does not work until that happens.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Settings)
	bool bDisableHMDOrientationUntilHMDHasBeenTracked;

	UPROPERTY(config, EditAnywhere, Category = Settings, meta = (
		DisplayName = "Hmd Setup Dialog Canceled Behavior",
		ToolTip = "App behavior if the HmdSetupDialog (which pops up when the hmd is disconnected) is canceled by the user."))
	EHmdSetupDialogCanceledBehavior HmdSetupDialogCanceledBehavior;

	// If true it is possible to switch to 'separate mode' and show things other than a mirror of the vr output to the social screen.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Settings, meta = (
		DisplayName = "Enable Social Screen Separate Mode",
		ToolTip = "If true it is possible to switch to 'separate mode' and show things other than a mirror of the vr output to the social screen.  There is a constant memory cost to enabling this, and performance costs to any social screen mode except Mirror."))
	bool bEnableSocialScreenSeparateMode;

	//initial desktop location of the game window when running on PC.
	//used to move the game window automatically to the morpheus headset 'monitor'
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Settings)
	FVector DesktopOffset;

	//enable camera to apply the pitch of the user's head.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Settings)
	bool bEnableControllerPitch;

	//enable camera to apply the roll of the user's head.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Settings)
	bool bEnableControllerRoll;

	//enable camera to apply orientation of the user's head.
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Settings)
	bool bChangePlayerView;

	// ip address for hmd server
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Settings)
	FString HMDServerAddress;
};
