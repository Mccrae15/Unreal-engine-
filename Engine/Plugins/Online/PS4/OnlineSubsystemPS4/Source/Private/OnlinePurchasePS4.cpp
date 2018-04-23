// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// Module includes
#include "OnlinePurchasePS4.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "OnlineError.h"

// https://ps4.scedev.net/resources/documents/SDK/2.500/ToolkitNp-Overview/0012.html

struct FUserPurchaseInfo
{
	FUserPurchaseInfo(FUniqueNetIdPS4 const& InUserId)
		: UserId(InUserId.AsShared())
		, bPendingTransaction(false)
		, CachedReceipt(MakeShareable(new FPurchaseReceipt()))
	{
		CachedReceipt->TransactionState = EPurchaseTransactionState::NotStarted;
	}

	/** The user ID this info belongs to */
	TSharedRef<FUniqueNetIdPS4 const> const UserId;

	/** Whether or not we are already servicing this user info in another thread */
	bool bPendingTransaction;

	/** Computed receipt info cached by a successful call to QueryReceipts */
	TSharedRef<FPurchaseReceipt> CachedReceipt;

	/** How many valid receipts are cached */
	int32 CountReceipts() const
	{
		int32 Total = 0;
		for (const FPurchaseReceipt::FReceiptOfferEntry& Offer : CachedReceipt->ReceiptOffers)
		{
			for (const FPurchaseReceipt::FLineItemInfo& LineItem : Offer.LineItems)
			{
				if (LineItem.IsRedeemable())
				{
					++Total;
				}
			}
		}
		return Total;
	}
};

class FOnlinePurchaseTaskBase : public FOnlineAsyncTaskBasic < FOnlineSubsystemPS4 >
{
public:
	FOnlinePurchaseTaskBase(FOnlineSubsystemPS4* InSubsystem, TSharedRef<FUserPurchaseInfo> const& InUserInfo)
		: FOnlineAsyncTaskBasic(InSubsystem)
		, UserInfo(InUserInfo)
		, bIsBlockingTaskDone(false)
	{
		// assume success
		bWasSuccessful = true;

		// flag the user info with a pending transaction
		if (UserInfo->bPendingTransaction)
		{
			FailTask(TEXT("Operation already pending on this user"));
			return;
		}
		UserInfo->bPendingTransaction = true;
	}
	virtual void Finalize() override
	{
		FOnlineAsyncTaskBasic<FOnlineSubsystemPS4>::Finalize();

		UserInfo->bPendingTransaction = false;

		// update our receipt
		if (bWasSuccessful)
		{
			UpdateReceipt();
		}
	}

	// FOnlineAsyncTask
	virtual void Tick() override
	{
		if (!bIsComplete && !bIsBlockingTaskDone)
		{
			// perform our blocking task
			bIsBlockingTaskDone = true;
			DoBlockingTask();
			if (bIsComplete)
			{
				// we probably failed out
				return;
			}
			if (!bIsBlockingTaskDone)
			{
				// keep waiting
				return;
			}

			// now initiate an entitlement query (async)
			NpToolkit::Commerce::Request::GetServiceEntitlements Request;
			Request.userId = UserInfo->UserId->GetUserId();
			Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
			Request.async = true;
			int32 Ret = NpToolkit::Commerce::getServiceEntitlements(Request, &Entitlements);
			if (Ret < SCE_TOOLKIT_NP_V2_SUCCESS)
			{
				return FailTask(FString::Printf(TEXT("Commerce::getServiceEntitlements returned 0x%08x (sync)"), Ret));
			}
		}

		// wait for the future to complete
		if (!bIsComplete)
		{
			// check for completion
			if (!Entitlements.isLocked())
			{
				bIsComplete = true;

				// check for errors
				if (Entitlements.getReturnCode() != SCE_TOOLKIT_NP_V2_SUCCESS)
				{
					return FailTask(FString::Printf(TEXT("Commerce::getServiceEntitlements returned 0x%08x (async)"), Entitlements.getReturnCode()));
				}
			}
		}
	}

protected:
	virtual void FailTask(const FString& Error)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlinePurchasePS4::Checkout - %s"), *Error);

		bIsComplete = true;
		bWasSuccessful = false;
		ErrorString = Error;
	}

	virtual void DoBlockingTask()
	{
		// by default does nothing
	}

private:

	void UpdateReceipt()
	{
		// parse the entitlement data
		NpToolkit::Commerce::ServiceEntitlements const* EntList = Entitlements.get();
		if (EntList == nullptr)
		{
			return FailTask(TEXT("Entitlement future returned null"));
		}

		// user PSN id
		FString const AccountIdStr = PS4AccountIdToString(UserInfo->UserId->GetAccountId());

		// prep the receipt
		FPurchaseReceipt& Receipt = *UserInfo->CachedReceipt;
		Receipt.TransactionState = EPurchaseTransactionState::Purchased;
		Receipt.ReceiptOffers.Empty();

		// parse the entitlement list
		for (int32 EntIndex = 0; EntIndex < EntList->numEntitlements; ++EntIndex)
		{
			auto const& Ent = EntList->entitlements[EntIndex];

			// the full entitlement ID (title (and therefore region) specific)
			const FString EntitlementId = ANSI_TO_TCHAR(Ent.entitlementLabel.value);
			if (EntitlementId.IsEmpty())
			{
				continue;
			}

			FPurchaseReceipt::FReceiptOfferEntry Offer(FString(), EntitlementId, 1);

			// set up validation info for any receipts that can still be redeemed
			if (Ent.type == NpToolkit::Commerce::EntitlementType::serviceConsumable)
			{
				// report all unconsumed instances
				for (int32 Index = Ent.consumedCount; Index < Ent.remainingCount + Ent.consumedCount; ++Index)
				{
					// generate a unique exchange code
					NpToolkit::Auth::Request::GetAuthCode Request;
					Request.userId = UserInfo->UserId->GetUserId();
					Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
					Request.async = false;
					Request.clientId = Subsystem->GetNpClientId();
					FCStringAnsi::Strcpy(Request.scope, ARRAY_COUNT(Request.scope), "psn:s2s");

					NpToolkit::Core::Response<NpToolkit::Auth::AuthCode> Response;
					int32 Ret = NpToolkit::Auth::getAuthCode(Request, &Response);
					if (Ret == SCE_TOOLKIT_NP_V2_SUCCESS)
					{
						FString ExchangeCode = ANSI_TO_TCHAR(Response.get()->authCode.code);

						// Validate the auth token we just received
						if (ExchangeCode.Len() != 6)
						{
							UE_LOG_ONLINE(Warning, TEXT("PS4 Auth Token expected to be 6 characters, found %d in string %s"), ExchangeCode.Len(), *ExchangeCode);
						}
						else
						{
							for (int32 CharIdx = 0; CharIdx < ExchangeCode.Len(); ++CharIdx)
							{
								if (!FChar::IsAlnum(ExchangeCode[CharIdx]))
								{
									UE_LOG_ONLINE(Warning, TEXT("Encountered non alpha-numeric character in string %s."), *ExchangeCode);
									break;
								}
							}
						}

						// populate a line item for each consumption
						FPurchaseReceipt::FLineItemInfo LineItem;
						LineItem.ItemName = EntitlementId;
						LineItem.UniqueId = FString::Printf(TEXT("%s:%s:%d"), *AccountIdStr, *EntitlementId, Index);
						LineItem.ValidationInfo = FString::Printf(TEXT("%s:%s:%d"), *ExchangeCode, *EntitlementId, Index);
						Offer.LineItems.Add(LineItem);
					}
					else
					{
						UE_LOG_ONLINE(Error, TEXT("Auth::getAuthCode returned 0x%08x. Unable to expose entitlement %s:%d."), Ret, *EntitlementId, Index);
					}
				}
				// quantity remaining for redeeming a purchase
				Offer.Quantity = Ent.remainingCount;
			}
			Receipt.ReceiptOffers.Add(MoveTemp(Offer));
		}
	}

protected:
	TSharedRef<FUserPurchaseInfo> UserInfo;
	NpToolkit::Core::Response<NpToolkit::Commerce::ServiceEntitlements> Entitlements;
	FString ErrorString;
	bool bIsBlockingTaskDone;
};

FOnlinePurchasePS4::FOnlinePurchasePS4(FOnlineSubsystemPS4* InSubsystem)
: PS4Subsystem(InSubsystem)
, bCheckoutPending(false)
{
	check(PS4Subsystem);
}

FOnlinePurchasePS4::~FOnlinePurchasePS4()
{
}

TSharedPtr<FUserPurchaseInfo> FOnlinePurchasePS4::GetUserInfo(FUniqueNetIdPS4 const& UserId) const
{
	if (UserId.IsValid())
	{
		const TSharedRef<FUserPurchaseInfo>* Value = UserInfoMap.Find(UserId.ToString());
		if (Value != nullptr)
		{
			return *Value;
		}
	}
	return TSharedPtr<FUserPurchaseInfo>();
}

TSharedRef<FUserPurchaseInfo> FOnlinePurchasePS4::GetOrCreateUserInfo(FUniqueNetIdPS4 const& UserId)
{
	TSharedRef<FUserPurchaseInfo>* Value = UserInfoMap.Find(UserId.ToString());
	if (Value != nullptr)
		return *Value;

	return UserInfoMap.Add(UserId.ToString(), MakeShareable(new FUserPurchaseInfo(UserId)));
}

bool FOnlinePurchasePS4::IsAllowedToPurchase(const FUniqueNetId& UserId) 
{
	bool bResult = true; // Check parental controls?
	return bResult;
}

class FOnlineAsyncTaskPS4Checkout : public FOnlinePurchaseTaskBase
{
public:
	FOnlineAsyncTaskPS4Checkout(FOnlinePurchasePS4* Parent, FOnlineSubsystemPS4* InSubsystem, const TSharedRef<FUserPurchaseInfo>& InUserInfo, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& InDelegate)
		: FOnlinePurchaseTaskBase(InSubsystem, InUserInfo)
		, Params(CheckoutRequest)
		, Delegate(InDelegate)
		, bWaitingForCheckout(false)
		, WeakParent(Parent->AsShared())
		, bWasCancelled(false)
	{
		check(Parent);
		check(!Parent->bCheckoutPending);
		Parent->bCheckoutPending = true;
	}

	~FOnlineAsyncTaskPS4Checkout()
	{
		// mark checkout as complete
		auto StrongParent = WeakParent.Pin();
		if (StrongParent.IsValid())
		{
			check(StrongParent->bCheckoutPending);
			StrongParent->bCheckoutPending = false;
		}
	}

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("Checkout"); }
	virtual void TriggerDelegates() override
	{
		FOnlineError Error;
		Error.bSucceeded = bWasSuccessful;
		if (!bWasSuccessful)
		{
			Error.ErrorCode = bWasCancelled ? TEXT("UserCancelled") : FOnlineError::GenericErrorCode; 
			Error.ErrorRaw = ErrorString;
		}

		// filter out quantity <= 0 entitlements in the receipt. They definitely did not come from this checkout.
		FPurchaseReceipt& Receipt = *UserInfo->CachedReceipt;
		Receipt.ReceiptOffers.RemoveAllSwap([](const FPurchaseReceipt::FReceiptOfferEntry& Entry) {
			return Entry.Quantity <= 0;
		});

		// fire the delegate
		Delegate.ExecuteIfBound(Error, UserInfo->CachedReceipt);
	}

protected:
	virtual void FailTask(const FString& Error) override
	{
		FOnlinePurchaseTaskBase::FailTask(Error);
		UserInfo->CachedReceipt->TransactionState = EPurchaseTransactionState::Failed;
	}

	virtual void Finalize() override
	{
		FOnlinePurchaseTaskBase::Finalize();
		if (bWasSuccessful && UserInfo->CountReceipts() <= 0)
		{
			FailTask(TEXT("PS4 checkout succeeded but did not generate any new entitlements"));
		}
	}

	bool IsCheckoutComplete()
	{
		if (CheckoutResponse.isLocked())
		{
			// Checkout is still in progress
			return false;
		}
		else
		{
			int32 Ret = CheckoutResponse.getReturnCode();
			if (Ret < 0)
			{
				FailTask(FString::Printf(TEXT("Commerce::displayCheckoutDialog returned 0x%08x"), Ret));
			}
			else if (Ret == SCE_TOOLKIT_NP_V2_DIALOG_RESULT_USER_CANCELED)
			{
				FailTask(TEXT("UserCancelled"));
				bWasCancelled = true;
			}

			return true;
		}
	}

	virtual void DoBlockingTask() override
	{
		if (bWaitingForCheckout)
		{
			bIsBlockingTaskDone = IsCheckoutComplete();
			return;
		}

		// translate productIds to SkuIds
		NpToolkit::Commerce::Request::GetProducts GetProductsParams;
		GetProductsParams.userId = UserInfo->UserId->GetUserId();
		GetProductsParams.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
		GetProductsParams.async = false;
		GetProductsParams.keepHtmlTags = false;
		GetProductsParams.numProducts = Params.PurchaseOffers.Num();
		check(GetProductsParams.numProducts <= NpToolkit::Commerce::Request::GetProducts::MAX_PRODUCTS);

		for (int32 OfferIndex = 0; OfferIndex < Params.PurchaseOffers.Num(); ++OfferIndex)
		{
			auto const& Offer = Params.PurchaseOffers[OfferIndex];
			auto& ProductId = GetProductsParams.productLabels[OfferIndex];

			FCStringAnsi::Strncpy(ProductId.value, TCHAR_TO_ANSI(*Offer.OfferId), ARRAY_COUNT(ProductId.value));
			UE_LOG_ONLINE(Log, TEXT("Translating ProductID=%s"), *Offer.OfferId);
		}

		int32 Ret = NpToolkit::Commerce::getProducts(GetProductsParams, &ProductsResponse);
		if (Ret != SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			return FailTask(FString::Printf(TEXT("Commerce::getProducts returned 0x%08x"), Ret));
		}
		NpToolkit::Commerce::Products const* ProductsList = ProductsResponse.get();
		check(ProductsList != nullptr); // ret should be not ok if this weren't ready

		// add the skus to the checkout params
		CheckoutParams.userId = UserInfo->UserId->GetUserId();
		CheckoutParams.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
		CheckoutParams.async = true;
		CheckoutParams.numTargets = 0;
		for (const FPurchaseCheckoutRequest::FPurchaseOfferEntry& Offer : Params.PurchaseOffers)
		{
			// find the SkuId for this product
			NpToolkit::Commerce::SkuInfo const* SkuId = nullptr;
			int32 ProductIndex = 0;
			for (; ProductIndex < ProductsList->numProducts; ++ProductIndex)
			{
				auto const& Product = ProductsList->products[ProductIndex];
				check(Product.hasDetails);
				check(Product.details.numSkus >= 1); // TODO: Support more than 1 SKU

				if (Offer.OfferId == ANSI_TO_TCHAR(Product.productLabel.value))
				{
					SkuId = &Product.details.skuinfo[0];
					break;
				}
			}
			if (SkuId == nullptr)
			{
				return FailTask(FString::Printf(TEXT("Unable to look up SKU for OfferId='%s'"), *Offer.OfferId));
			}

			// add to the checkout params
			UE_LOG_ONLINE(Log, TEXT("FOnlinePurchasePS4::Checkout - Attempting to purchase %d of SKU %s"), Offer.Quantity, ANSI_TO_TCHAR(SkuId->label.value));
			for (int32 j = 0; j < Offer.Quantity; ++j)
			{
				const int32 TargetIndex = CheckoutParams.numTargets++;
				checkf(TargetIndex < SCE_NP_COMMERCE_DIALOG_NUM_TARGETS_MAX, TEXT("Attempted to purchase more than max products"));

				FMemory::Memcpy(CheckoutParams.targets[TargetIndex].productLabel.value, ProductsList->products[ProductIndex].productLabel.value);
				FMemory::Memcpy(CheckoutParams.targets[TargetIndex].skuLabel.value, SkuId->label.value);
			}
		}

		// run checkout UI (blocking)
		Ret = NpToolkit::Commerce::displayCheckoutDialog(CheckoutParams, &CheckoutResponse);
		if (Ret < SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			return FailTask(FString::Printf(TEXT("Commerce::displayCheckoutDialog returned 0x%08x"), Ret));
		}

		// change to waiting for checkout to complete
		bWaitingForCheckout = true;
		bIsBlockingTaskDone = false;
	}

private:
	FDelegateHandle DelegateHandle;
	FPurchaseCheckoutRequest Params;
	FOnPurchaseCheckoutComplete Delegate;
	bool bWaitingForCheckout;

	NpToolkit::Commerce::Request::DisplayCheckoutDialog CheckoutParams;
	NpToolkit::Core::Response<NpToolkit::Core::Empty> CheckoutResponse;

	NpToolkit::Core::Response<NpToolkit::Commerce::Products> ProductsResponse;
	TWeakPtr<FOnlinePurchasePS4, ESPMode::ThreadSafe> WeakParent;

	bool bWasCancelled;
};

void FOnlinePurchasePS4::Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate)
{
	FUniqueNetIdPS4 const& PS4UserId = FUniqueNetIdPS4::Cast(UserId);

	// check input params
	if (!PS4UserId.IsValid() || CheckoutRequest.PurchaseOffers.Num() <= 0)
	{
		PS4Subsystem->ExecuteNextTick([Delegate]() {
			Delegate.ExecuteIfBound(FOnlineError(TEXT("InvalidParameters")), MakeShareable(new FPurchaseReceipt()));
		});
		return;
	}

	// PS4 can only do one checkout at a time
	if (bCheckoutPending)
	{
		PS4Subsystem->ExecuteNextTick([Delegate]() {
			Delegate.ExecuteIfBound(FOnlineError(TEXT("CheckoutAlreadyPending")), MakeShareable(new FPurchaseReceipt()));
		});
		return;
	}

	// get info for this user
	TSharedRef<FUserPurchaseInfo> Info = GetOrCreateUserInfo(PS4UserId);

	// get the async task manager
	FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
	if (AsyncTaskManager == nullptr)
	{
		PS4Subsystem->ExecuteNextTick([Delegate]() {
			Delegate.ExecuteIfBound(FOnlineError(TEXT("AsyncTaskManagerInvalid")), MakeShareable(new FPurchaseReceipt()));
		});
		return;
	}
	
	// schedule an async task
	AsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskPS4Checkout(this, PS4Subsystem, Info, CheckoutRequest, Delegate));
}
		
void FOnlinePurchasePS4::FinalizePurchase(const FUniqueNetId& UserId, const FString& ReceiptId)
{
			
}

class FOnlineAsyncTaskPS4VoucherCodeInput : public FOnlinePurchaseTaskBase
{
public:
	FOnlineAsyncTaskPS4VoucherCodeInput(FOnlineSubsystemPS4* InSubsystem, const TSharedRef<FUserPurchaseInfo>& InUserInfo, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& InDelegate)
		: FOnlinePurchaseTaskBase(InSubsystem, InUserInfo)
		, Params(RedeemCodeRequest)
		, Delegate(InDelegate)
	{
	}

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("VoucherCodeInput"); }
	virtual void TriggerDelegates() override
	{
		FOnlineError Error;
		Error.bSucceeded = bWasSuccessful;
		Error.ErrorRaw = ErrorString;
		Delegate.ExecuteIfBound(Error, UserInfo->CachedReceipt);
	}

protected:
	virtual void DoBlockingTask() override
	{
		// we're just going to do this initial call synchronous (will block online thread)
		NpToolkit::Commerce::Request::DisplayVoucherCodeInputDialog VoucherParams;
		VoucherParams.userId = UserInfo->UserId->GetUserId();
		VoucherParams.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
		VoucherParams.async = false;

		int32 Ret = NpToolkit::Commerce::displayVoucherCodeInputDialog(VoucherParams, &Response);
		if (Ret != SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			return FailTask(FString::Printf(TEXT("Commerce::Interface::voucherCodeInput returned 0x%08x"), Ret));
		}
	}

private:
	NpToolkit::Core::Response<NpToolkit::Core::Empty> Response;
	FRedeemCodeRequest Params; 
	FOnPurchaseRedeemCodeComplete Delegate;
};

void FOnlinePurchasePS4::RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate)
{
	FUniqueNetIdPS4 const& PS4UserId = FUniqueNetIdPS4::Cast(UserId);

	// check input params
	if (!PS4UserId.IsValid())
	{
		PS4Subsystem->ExecuteNextTick([Delegate]() {
			Delegate.ExecuteIfBound(FOnlineError(TEXT("InvalidParameters")), MakeShareable(new FPurchaseReceipt()));
		});
		return;
	}

	// get info for this user
	TSharedRef<FUserPurchaseInfo> Info = GetOrCreateUserInfo(PS4UserId);

	// get the async task manager
	FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
	if (AsyncTaskManager == nullptr)
	{
		PS4Subsystem->ExecuteNextTick([Delegate]() {
			Delegate.ExecuteIfBound(FOnlineError(TEXT("AsyncTaskManagerInvalid")), MakeShareable(new FPurchaseReceipt()));
		});
		return;
	}

	// schedule an async task
	AsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskPS4VoucherCodeInput(PS4Subsystem, Info, RedeemCodeRequest, Delegate));
}

class FOnlineAsyncTaskPS4GetEntitlementList : public FOnlinePurchaseTaskBase
{
public:
	FOnlineAsyncTaskPS4GetEntitlementList(FOnlineSubsystemPS4* InSubsystem, const TSharedRef<FUserPurchaseInfo>& InUserInfo, const FOnQueryReceiptsComplete& InDelegate)
		: FOnlinePurchaseTaskBase(InSubsystem, InUserInfo)
		, Delegate(InDelegate)
	{
	}

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("GetEntitlementList"); }
	virtual void TriggerDelegates() override
	{
		FOnlineError Error;
		Error.bSucceeded = bWasSuccessful;
		Error.ErrorRaw = ErrorString;
		Delegate.ExecuteIfBound(Error);
	}

private:
	FOnQueryReceiptsComplete Delegate;
};

void FOnlinePurchasePS4::QueryReceipts(const FUniqueNetId& UserId, bool bRestoreReceipts, const FOnQueryReceiptsComplete& Delegate)
{
	FUniqueNetIdPS4 const& PS4UserId = FUniqueNetIdPS4::Cast(UserId);

	// check input params
	if (!PS4UserId.IsValid())
	{
		PS4Subsystem->ExecuteNextTick([Delegate]() {
			Delegate.ExecuteIfBound(FOnlineError(TEXT("InvalidParameters")));
		});
		return;
	}

	// get info for this user
	TSharedRef<FUserPurchaseInfo> Info = GetOrCreateUserInfo(PS4UserId);

	// get the async task manager
	FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
	if (AsyncTaskManager == nullptr)
	{
		PS4Subsystem->ExecuteNextTick([Delegate]() {
			Delegate.ExecuteIfBound(FOnlineError(TEXT("AsyncTaskManagerInvalid")));
		});
		return;
	}

	// schedule an async task
	AsyncTaskManager->AddToInQueue(new FOnlineAsyncTaskPS4GetEntitlementList(PS4Subsystem, Info, Delegate));
}

void FOnlinePurchasePS4::GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const
{
	FUniqueNetIdPS4 const& PS4UserId = FUniqueNetIdPS4::Cast(UserId);

	OutReceipts.Empty();
	TSharedPtr<FUserPurchaseInfo> Info = GetUserInfo(PS4UserId);
	if (Info.IsValid() && Info->CachedReceipt->TransactionState == EPurchaseTransactionState::Purchased)
	{
		OutReceipts.Add(*Info->CachedReceipt);
	}
}
