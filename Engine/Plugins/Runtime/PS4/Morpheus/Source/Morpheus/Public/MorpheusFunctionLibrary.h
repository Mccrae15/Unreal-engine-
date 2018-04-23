// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMorpheusPlugin.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MorpheusTypes.h"
#include "MorpheusFunctionLibrary.generated.h"

/**
 * Morpheus Extensions Function Library
 */
UCLASS()
class MORPHEUS_API UMorpheusFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/**
	* Show provided texture as a reprojected 2D VR splash screen on PlayStation VR.
	*
	* @param Texture			(in) A texture to be used for the splash. B8R8G8A8 format.
	* @param Scale				(in) Scale of the texture.
	* @param Offset				(in) Position from which to start rendering the texture.
	*/
	UFUNCTION(BlueprintCallable, Category = "Morpheus")
		static void Show2DVRSplashScreen(class UTexture* Texture, FVector2D Scale = FVector2D(1.0f, 1.0f), FVector2D Offset = FVector2D(0.0f, 0.0f));

	/**
	* Hide the 2D VR splash screen and return to normal VR display and reprojection.
	*/
	UFUNCTION(BlueprintCallable, Category = "Morpheus")
		static void Hide2DVRSplashScreen();

	/**
	* Sets the minimum image color after reprojection.  Can be used to minimize reprojection artifacts, can also cause artifacts.  See Sony documentation on sceHmdReprojectionSetOutputMinColor for more information.
	*
	* @param MinColor				(in) The minimum output color.  RGB only.
	*/
	UFUNCTION(BlueprintCallable, Category = "Morpheus")
	static void HMDReprojectionSetOutputMinColor(FLinearColor MinColor);

};
