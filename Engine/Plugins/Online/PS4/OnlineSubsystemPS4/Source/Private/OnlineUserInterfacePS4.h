// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineUserInterface.h"
#include "OnlineSubsystemPS4Types.h"

/**
 * Info associated with an online user on the PSN service
 */
class FOnlineUserInfoPS4 : public FOnlineUser
{
public:

	// FOnlineUser

	virtual inline TSharedRef<const FUniqueNetId> GetUserId() const override final { return UniqueNetIdPS4; }
	virtual inline FString GetRealName() const override final { return PS4FullRealName(OnlineNpProfile); }
	virtual inline FString GetDisplayName(const FString& Platform = FString()) const override final { return PS4OnlineIdToString(OnlineNpProfile.onlineUser.onlineId); }
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;

	// FOnlineUserInfoPS4

	/**
	 * Constructor
	 */
	explicit FOnlineUserInfoPS4(const NpToolkit::UserProfile::NpProfile& InOnlineNpProfile)
		: OnlineNpProfile(InOnlineNpProfile)
 		, UniqueNetIdPS4(FUniqueNetIdPS4::FindOrCreate(InOnlineNpProfile.onlineUser.accountId, InOnlineNpProfile.onlineUser.onlineId))
	{
	}

	/**
	 * Destructor
	 */
	virtual ~FOnlineUserInfoPS4()
	{
	}

private:

	NpToolkit::UserProfile::NpProfile OnlineNpProfile;
	TSharedRef<const FUniqueNetIdPS4> UniqueNetIdPS4;
};

/** Shared reference to PS4 user info */
typedef TSharedRef<FOnlineUserInfoPS4> FOnlineUserInfoPS4Ref;

/** Mapping from local user id to map of user to online user data */
typedef TMap< FString, TMap<FString, FOnlineUserInfoPS4Ref> > FOnlineUserPS4Map;



/**
 * PS4 service implementation of the online user interface
 */
class FOnlineUserPS4 : public IOnlineUser
{
public:

	// IOnlineUser

	virtual bool QueryUserInfo(int32 LocalUserNum, const TArray<TSharedRef<const FUniqueNetId>>& UserIds) override;
	virtual bool GetAllUserInfo(int32 LocalUserNum, TArray<TSharedRef<class FOnlineUser>>& OutUsers) override;
	virtual TSharedPtr<FOnlineUser> GetUserInfo(int32 LocalUserNum, const class FUniqueNetId& UserId) override;
	virtual bool QueryUserIdMapping(const FUniqueNetId& UserId, const FString& DisplayNameOrEmail, const FOnQueryUserMappingComplete& Delegate = FOnQueryUserMappingComplete()) override;
	virtual bool QueryExternalIdMappings(const FUniqueNetId& LocalUserId, const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, const FOnQueryExternalIdMappingsComplete& Delegate = FOnQueryExternalIdMappingsComplete()) override;
	virtual void GetExternalIdMappings(const FExternalIdQueryOptions& QueryOptions, const TArray<FString>& ExternalIds, TArray<TSharedPtr<const FUniqueNetId>>& OutIds) override;
	virtual TSharedPtr<const FUniqueNetId> GetExternalIdMapping(const FExternalIdQueryOptions& QueryOptions, const FString& ExternalId) override;

	// FOnlineUserPS4

	/**
	 * Constructor
	 *
	 * @param InSubsystem PS4 subsystem being used
	 */
	explicit FOnlineUserPS4(class FOnlineSubsystemPS4* InSubsystem);

	/**
	 * Destructor
	 */
	virtual ~FOnlineUserPS4();

private:

	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

	/** Map of local users to array of online PS4 users */
	FOnlineUserPS4Map UsersMap;

};

typedef TSharedPtr<FOnlineUserPS4, ESPMode::ThreadSafe> FOnlineUsersPS4Ptr;