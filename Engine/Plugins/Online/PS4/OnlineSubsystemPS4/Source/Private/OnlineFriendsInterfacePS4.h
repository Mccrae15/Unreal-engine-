// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineFriendsInterface.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineSubsystemPS4Package.h"
#include "OnlinePresenceInterface.h"

/**
 * Info associated with an online friend on the PSN service
 */
class FOnlineFriendPS4 : 
	public FOnlineFriend
{
public:

	// FOnlineUser

	virtual inline TSharedRef<const FUniqueNetId> GetUserId() const override final { return UniqueNetIdPS4; }
	virtual inline FString GetRealName() const override final { return RealName; }
	virtual inline FString GetDisplayName(const FString& Platform = FString()) const override final { return DisplayName; }
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;

	// FOnlineFriend
	
	virtual EInviteStatus::Type GetInviteStatus() const override;
	virtual const FOnlineUserPresence& GetPresence() const override;

	// FOnlineFriendPS4

	FOnlineFriendPS4(const NpToolkit::Friend::Friend& InOnlineNpFriend, const FOnlineSubsystemPS4* InSubsystem, const FString& SessionId = FString());

	virtual ~FOnlineFriendPS4()
	{
	}

private:
	/** Unique PSN Id for the friend */
	TSharedRef<const FUniqueNetIdPS4> UniqueNetIdPS4;

	FString RealName;
	FString DisplayName;

	/** @temp presence info  */
	FOnlineUserPresence Presence;
};

/** Shared reference to friend data */
typedef TSharedRef<FOnlineFriendPS4> FOnlineFriendPS4Ref;

/** Mapping from local user id to array of friend user data */
typedef TMap<FString, TArray<FOnlineFriendPS4Ref>> FOnlineFriendsPS4Map;

/** Mapping friend list name to friend map */
typedef TMap<FString, FOnlineFriendsPS4Map> FListNameOnlineFriendsPS4Map;


/**
 * Implements the PS4 specific interface for friends
 */
class FOnlineFriendsPS4 :
	public IOnlineFriends
{
	/** The async task classes require friendship */
	friend class FOnlineAsyncTaskPS4ReadFriendsList;

public:

	// IOnlineFriends
	// Friends list functionality requires both Presence and Sessions services to be enabled for a title on DevNet.  See:
	// https://ps4.scedev.net/forums/thread/18904/
	virtual bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate = FOnReadFriendsListComplete()) override;
	virtual bool DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate = FOnDeleteFriendsListComplete()) override;
	virtual bool SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName,  const FOnSendInviteComplete& Delegate = FOnSendInviteComplete()) override;
	virtual bool AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate = FOnAcceptInviteComplete()) override;
 	virtual bool RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
 	virtual bool DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends) override;
	virtual TSharedPtr<FOnlineFriend> GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace) override;
	virtual bool GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers) override;
	virtual bool BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool QueryBlockedPlayers(const FUniqueNetId& UserId) override;
	virtual bool GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers) override;
	virtual void DumpBlockedPlayers() const override;

	// FOnlineFriendsPS4

	explicit FOnlineFriendsPS4(FOnlineSubsystemPS4* InPS4Subsystem) :
		PS4Subsystem(InPS4Subsystem)
	{
		check(PS4Subsystem);
	}

	virtual ~FOnlineFriendsPS4() {};

private:

	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

	/** Map of named friend list of local users to array of online PS4 users */
	FListNameOnlineFriendsPS4Map FriendsMap;
};

typedef TSharedPtr<FOnlineFriendsPS4, ESPMode::ThreadSafe> FOnlineFriendsPS4Ptr;
