// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlinePurchaseInterface.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4Package.h"

class FOnlineSubsystemPS4;
struct FUserPurchaseInfo;

/**
 *	FOnlinePurchasePS4 - Interface class for code redemption and checkout.
 */
class FOnlinePurchasePS4 
: public IOnlinePurchase
, public TSharedFromThis<FOnlinePurchasePS4, ESPMode::ThreadSafe>
{
public:
	FOnlinePurchasePS4(FOnlineSubsystemPS4* InSubsystem);
	virtual ~FOnlinePurchasePS4();

	// Begin IOnlinePurchase interface
	virtual bool IsAllowedToPurchase(const FUniqueNetId& UserId) override;
	virtual void Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate) override;
	virtual void FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId) override;
	virtual void RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate) override;
	virtual void QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate) override;
	virtual void GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const override;
	// End IOnlinePurchase interface

private:

	/** Reference to the main OSS */
	FOnlineSubsystemPS4* PS4Subsystem;

	TSharedPtr<FUserPurchaseInfo> GetUserInfo(const FUniqueNetIdPS4& UserId) const;
	TSharedRef<FUserPurchaseInfo> GetOrCreateUserInfo(const FUniqueNetIdPS4& UserId);

	TMap<FString, TSharedRef<FUserPurchaseInfo> > UserInfoMap;

	friend class FOnlineAsyncTaskPS4Checkout;
	bool bCheckoutPending;
};

typedef TSharedPtr<FOnlinePurchasePS4, ESPMode::ThreadSafe> FOnlinePurchasePS4Ptr;
