// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"
#include "KatDataCore.h"
#include "KatDataPlugin4BPBPLibrary.generated.h"

/* 
*	Function library class.
*	Each function in it is expected to be static and represents blueprint node that can be called in any blueprint.
*
*	When declaring function you can define metadata for the node. Key function specifiers will be BlueprintPure and BlueprintCallable.
*	BlueprintPure - means the function does not affect the owning object in any way and thus creates a node without Exec pins.
*	BlueprintCallable - makes a function which can be executed in Blueprints - Thus it has Exec pins.
*	DisplayName - full name of the node, shown when you mouse over the node and in the blueprint drop down menu.
*				Its lets you name the node using characters not allowed in C++ function names.
*	CompactNodeTitle - the word(s) that appear on the node.
*	Keywords -	the list of keywords that helps you to find node when you search for it using Blueprint drop-down menu. 
*				Good example is "Print String" node which you can find also by using keyword "log".
*	Category -	the category your node will be under in the Blueprint drop-down menu.
*
*	For more info on custom blueprint nodes visit documentation:
*	https://wiki.unrealengine.com/Custom_Blueprint_Node_Creation
*/
UCLASS()
class UKatDataPlugin4BPBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()


public:

	UFUNCTION(BlueprintPure, meta = (DisplayName = "KatData_BodyYaw", Keywords = "KatDataPlugin4BP KatData"), Category = "KatDataPlugin4BP")
		static float BodyYaw_KatDataPlugin4BP();

	UFUNCTION(BlueprintPure, meta = (DisplayName = "KatData_WalkPower", Keywords = "KatDataPlugin4BP KatData"), Category = "KatDataPlugin4BP")
		static float WalkPower_KatDataPlugin4BP();

	UFUNCTION(BlueprintPure, meta = (DisplayName = "KatData_MoveDirection", Keywords = "KatDataPlugin4BP KatData"), Category = "KatDataPlugin4BP")
		static int32 MoveDirection_KatDataPlugin4BP();

	UFUNCTION(BlueprintPure, meta = (DisplayName = "KatData_IsMoving", Keywords = "KatDataPlugin4BP KatData"), Category = "KatDataPlugin4BP")
		static int32 IsMoving_KatDataPlugin4BP();

	UFUNCTION(BlueprintPure, meta = (DisplayName = "KatData_WalkDistance", Keywords = "KatDataPlugin4BP KatData"), Category = "KatDataPlugin4BP")
		static float WalkDistance_KatDataPlugin4BP();
};
