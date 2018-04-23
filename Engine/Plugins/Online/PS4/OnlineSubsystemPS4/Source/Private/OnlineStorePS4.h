// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineStoreInterfaceV2.h"
#include "OnlineSubsystemPS4Package.h"

class FOnlineSubsystemPS4;
class FStoreTaskBase;

/**
 * Implementation for online store via PSN
 */
class FOnlineStorePS4 :
	public IOnlineStoreV2,
	public TSharedFromThis<FOnlineStorePS4, ESPMode::ThreadSafe>
{
public:
	FOnlineStorePS4(FOnlineSubsystemPS4* InPS4Subsystem);
	virtual ~FOnlineStorePS4();

public:// IOnlineStoreV2
	virtual void QueryCategories(const FUniqueNetId& UserId, const FOnQueryOnlineStoreCategoriesComplete& Delegate) override;
	virtual void GetCategories(TArray<FOnlineStoreCategory>& OutCategories) const override;
	virtual void QueryOffersByFilter(const FUniqueNetId& UserId, const FOnlineStoreFilter& Filter, const FOnQueryOnlineStoreOffersComplete& Delegate) override;
	virtual void QueryOffersById(const FUniqueNetId& UserId, const TArray<FUniqueOfferId>& OfferIds, const FOnQueryOnlineStoreOffersComplete& Delegate) override;
	virtual void GetOffers(TArray<FOnlineStoreOfferRef>& OutOffers) const override;
	virtual TSharedPtr<FOnlineStoreOffer> GetOffer(const FUniqueOfferId& OfferId) const override;

private:
	void QueueStoreTask(FStoreTaskBase* Task);

PACKAGE_SCOPE:
	FOnlineSubsystemPS4* Ps4Subsystem;
	TArray<FOnlineStoreCategory> CachedCategories;
	TMap<FUniqueOfferId,FOnlineStoreOfferRef> CachedOffers;
};

typedef TSharedPtr<FOnlineStorePS4, ESPMode::ThreadSafe> FOnlineStorePS4Ptr;
