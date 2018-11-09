// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MorpheusTypes.generated.h"

/**
 * Morpheus Types
 */

 /**
 * Enumerates VR Frame sequences for PSVR.
 */
UENUM()
enum class EPSVRFrameSequence : uint8
{
	Render60Scanout120 = 0 UMETA(ToolTip = "Unreal will render frames at 60Hz, PS VR Hmd will reproject and flip at 120Hz."),
	Render90Scanout90 = 1 UMETA(ToolTip = "Unreal will render frames at 90Hz, PS VR Hmd will reproject and flip at 90Hz."),
	Render120Scanout120 = 2 UMETA(ToolTip = "Unreal will render frames at 120Hz, PS VR Hmd will reproject and flip at 120Hz."),
};

 /**
 * Enumerates reprojection sampler wrap mode options.
 */
UENUM()
enum class EReprojectionSamplerWrapMode : uint8
{
	Mirror = 0 UMETA(ToolTip = "Pixels mapping beyond the rendered area will be uv mirrored to nearby pixels."),
	ClampBorder = 1 UMETA(ToolTip = "Pixels mapping beyond the rendered area will be clamped to the border color."),
};

/**
* Enumerates app behaviors if the HmdSetupDialog (which pops up when the hmd is disconnected) is canceled by the user.
*/
UENUM()
enum class EHmdSetupDialogCanceledBehavior : uint8
{
	DisallowCancel = 0 UMETA(ToolTip = "The HmdSetupDialog will be put up again AND the HmdConnectCanceled delegate will fire.  Appropriate for VR only apps.  Sony suggested this to one licensee.  A better implementation may come from sony eventually."),
	SwitchTo2D = 1 UMETA(ToolTip = "The app will switch to 2D mode AND the HmdConnectCanceled delegate will fire.  Appropriate for apps that support 2D and VR."),
	DelegateDefined = 2 UMETA(ToolTip = "The HmdConnectCanceled delegate will fire AND the app will stay in VR rendering mode.  The project needs to handle that and take appropriate action to avoid TRC violations."),
};

