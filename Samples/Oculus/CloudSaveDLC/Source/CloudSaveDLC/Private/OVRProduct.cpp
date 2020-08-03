// Fill out your copyright notice in the Description page of Project Settings.


#include "OVRProduct.h"

void UOVRProduct::SetProduct(ovrProductHandle ProductHandle_, bool IsConsumable_, bool IsPurchased_, int32* ConsumableCounter_)
{
	ProductHandle = ProductHandle_;
	IsConsumable = IsConsumable_;
	// 'IsPurchased' is a permanent state for UOVRProduct, 
	// and since consumables can be re-purchased, it doesn't apply to consumables.
	IsPurchased = IsPurchased_ && !IsConsumable; 
	ConsumableCounter = ConsumableCounter_;
	Name = ovr_Product_GetName(ProductHandle);
	FormattedPrice = ovr_Product_GetFormattedPrice(ProductHandle);
}

/// Launches the checkout flow to purchase a product.
void UOVRProduct::PurchaseProduct()
{
	if (!ProductHandle)
	{
		return;
	}

	const char* SKU = ovr_Product_GetSKU(ProductHandle);
	ovrRequest RequestId = ovr_IAP_LaunchCheckoutFlow(SKU);

	FOnlineSubsystemOculus* OSS = static_cast<FOnlineSubsystemOculus*>(IOnlineSubsystem::Get());

	// Once the purchase flow has completed, execute this lambda to apply the purchase.
	OSS->AddRequestDelegate(RequestId, FOculusMessageOnCompleteDelegate::CreateLambda(
		[this, OSS](ovrMessageHandle Message, bool bIsError)
		{
			if (bIsError)
			{
				ovrErrorHandle Error = ovr_Message_GetError(Message);
				UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ovr_Error_GetMessage(Error)));
				return;
			}

			ovrPurchaseHandle PurchaseHandle = ovr_Message_GetPurchase(Message);
			const char* SKU = ovr_Purchase_GetSKU(PurchaseHandle);
			UE_LOG(LogTemp, Display, TEXT("Purchased %s"), SKU);

			if (IsConsumable)
			{
				// Here, you should apply the purchased product to the users account,
				// and then immediately consume the purchase.
				if (ConsumableCounter)
				{
					*ConsumableCounter += 1;
				}

				ovrRequest ConsumeRequest = ovr_IAP_ConsumePurchase(SKU);

				OSS->AddRequestDelegate(ConsumeRequest, FOculusMessageOnCompleteDelegate::CreateLambda(
					[this](ovrMessageHandle Message, bool bIsError)
					{
						if (bIsError)
						{
							ovrErrorHandle Error = ovr_Message_GetError(Message);
							UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ovr_Error_GetDisplayableMessage(Error)));
							return;
						}
					}));
			}
			else
			{
				// Mark the product as purchased in the local data. Next time
				// we call ovr_IAP_GetViewerPurchases (see UDLCWidget::FetchPurchasedProducts)
				// it will include this product's SKU in the list of purchased products.
				IsPurchased = true;
			}
		}));
}
