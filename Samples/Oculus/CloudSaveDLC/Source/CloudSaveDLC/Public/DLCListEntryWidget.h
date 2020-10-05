// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DLCListEntryWidget.generated.h"

class UOVRProduct;

/**
 * UDLCListEntryWidget interfaces between the Blueprint list item and 
 * UOVRProduct model type, allowing the UI to query information about
 * the UOVRProduct and to launch the purchase flow.
 */
UCLASS()
class CLOUDSAVEDLC_API UDLCListEntryWidget : public UUserWidget
{
	GENERATED_BODY()
	
private:
	
	UOVRProduct* Product;
public:

	UFUNCTION(BlueprintCallable, Category = "BPDLC")
	void SetWidgetObject(UObject* object);

	UFUNCTION(BlueprintCallable, Category = "BPDLC")
	void PurchaseProduct();

	UFUNCTION(BlueprintCallable, Category = "BPDLC")
	bool IsProductPurchased() const;

	UFUNCTION(BlueprintCallable, Category = "BPDLC")
	FString GetProductName() const;

	UFUNCTION(BlueprintCallable, Category = "BPDLC")
	FString GetProductPrice() const;

};
