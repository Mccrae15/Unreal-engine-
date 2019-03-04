// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SteamVRControllerLibrary.generated.h"

/** Defines the set of input events you want associated with your SteamVR d-pad buttons */
UENUM(BlueprintType)
enum class ESteamVRTouchDPadMapping : uint8
{
	/** Maps each controller's four touchpad buttons to the respective FaceButton1/2/3/4 events */
	FaceButtons,

	/** Maps each controller's four touchpad buttons to the respective Thumbstick_Up/Down/Left/Right events  */
	ThumbstickDirections,

	/** Unmaps all directional touchpad input so that the d-pad buttons won't trigger their own input events */
	Disabled
};

/** */
UCLASS()
class STEAMVRCONTROLLER_API USteamVRControllerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Sets which input events you want associated with the SteamVR controller's 
	 * directional touchpad buttons - does so for both left and right controllers.
	 *
	 * @param  NewMapping	Defines the set of input events you want associated with the four directional touchpad buttons.
	 */
	UFUNCTION(BlueprintCallable, Category="SteamVR")
	static void SetTouchDPadMapping(ESteamVRTouchDPadMapping NewMapping = ESteamVRTouchDPadMapping::FaceButtons); // @see SteamVRController.cpp for implementation

	/**
	* Enables or disables the use of the touchpad as a thumbstick for VR devices that have thumbsticks.
	*
	* @param	isTouchpadPrimary		Sets touchpad as "Motion Controller Thumbstick" events if true, "Secondary Thumbstick" events if false.
	*/
	UFUNCTION(BlueprintCallable, Category = "SteamVR")
		static void ToggleTouchpadSource(bool isTouchpadPrimary);

	/**
	 * Set the axis to poll for a secondary thumbstick.
	 *
	 * @param	axis		The Axis of a potential second thumbstick.  This corresponds to vr::k_EButton_Axis#.
	 */
	UFUNCTION(BlueprintCallable, Category = "SteamVR")
		static void SetSecondaryThumbstickSource(int axis);
};
