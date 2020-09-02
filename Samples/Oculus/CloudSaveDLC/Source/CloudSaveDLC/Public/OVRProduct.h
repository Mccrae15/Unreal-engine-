// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "OVR_Platform.h"
#include "OnlineSubsystemOculus.h"
#include "OVRProduct.generated.h"

/**
 * UOVRProduct wraps an ovrProductHandle to provide a data source for the 
 * DLCEntryList list view (which must be a UObject). It also provides functionality
 * to launch the checkout flow for an ovrProduct.
 */
UCLASS()
class CLOUDSAVEDLC_API UOVRProduct : public UObject
{
	GENERATED_BODY()
	
private:
	ovrProductHandle ProductHandle;
	int32* ConsumableCounter;
	
public:
	void SetProduct(ovrProductHandle ProductHandle, bool IsConsumable, bool IsPurchased, int32* ConsumableCounter);

	bool IsConsumable;
	bool IsPurchased;

	FString Name;
	FString FormattedPrice;

	/// Launches the checkout flow to purchase a product.
	void PurchaseProduct();
};
