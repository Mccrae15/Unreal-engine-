// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineSubsystemPS4Package.h"
#include "Interfaces/OnlinePresenceInterface.h"

class FOnlineUserPresencePS4;
using FOnlineFriendPS4Ref = TSharedRef<class FOnlineFriendPS4>;

/** Info associated with an online friend on the PSN service */
class FOnlineFriendPS4 : public FOnlineFriend
{
public:
	static FOnlineFriendPS4Ref Create(const FOnlineSubsystemPS4& PS4Subsystem, const NpToolkit::Friend::Friend& NpFriend);
	static FOnlineFriendPS4Ref Create(const FOnlineSubsystemPS4& PS4Subsystem, const NpToolkit::UserProfile::NpProfile& NpProfile, const NpToolkit::Presence::Presence& NpPresence);
	
	virtual inline TSharedRef<const FUniqueNetId> GetUserId() const override final { return UniqueNetIdPS4; }
	virtual inline FString GetRealName() const override final { return RealName; }
	virtual inline FString GetDisplayName(const FString& Platform = FString()) const override final { return DisplayName; }
	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override;

	virtual EInviteStatus::Type GetInviteStatus() const override;
	virtual const FOnlineUserPresence& GetPresence() const override;

PACKAGE_SCOPE:
	FOnlineUserPresencePS4& GetPresencePS4();

private:
	FOnlineFriendPS4(const FOnlineSubsystemPS4& PS4Subsystem, const NpToolkit::UserProfile::NpProfile& FriendProfile, const NpToolkit::Presence::Presence& FriendPresence);
	
	FUniqueNetIdPS4Ref UniqueNetIdPS4;

	FString RealName;
	FString DisplayName;
	TSharedRef<FOnlineUserPresencePS4> Presence;
};

/** Implements the PS4 specific interface for friends */
class FOnlineFriendsPS4 : public IOnlineFriends, public TSharedFromThis<FOnlineFriendsPS4, ESPMode::ThreadSafe>
{
public:
	// IOnlineFriends
	// Friends list functionality requires both Presence and Sessions services to be enabled for a title on DevNet.  See:
	// https://ps4.scedev.net/forums/thread/18904/
	virtual bool ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate = FOnReadFriendsListComplete()) override;
	virtual bool DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate = FOnDeleteFriendsListComplete()) override;
	virtual bool SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName,  const FOnSendInviteComplete& Delegate = FOnSendInviteComplete()) override;
	virtual bool AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate = FOnAcceptInviteComplete()) override;
 	virtual bool RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual void SetFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FString& Alias, const FOnSetFriendAliasComplete& Delegate = FOnSetFriendAliasComplete()) override;
	virtual void DeleteFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnDeleteFriendAliasComplete& Delegate = FOnDeleteFriendAliasComplete()) override;
	virtual bool DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends) override;
	virtual TSharedPtr<FOnlineFriend> GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName) override;
	virtual bool QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace) override;
	virtual bool GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers) override;
	virtual void DumpRecentPlayers() const override;
	virtual bool BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId) override;
	virtual bool QueryBlockedPlayers(const FUniqueNetId& UserId) override;
	virtual bool GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers) override;
	virtual void DumpBlockedPlayers() const override;

PACKAGE_SCOPE:
	explicit FOnlineFriendsPS4(FOnlineSubsystemPS4& InPS4Subsystem)
		: PS4Subsystem(InPS4Subsystem)
	{}
	
	static NpToolkit::Friend::FriendsRetrievalMode GetFriendListRequestFlagPS4(const FString& ListName);

	void RebuildFriendsList(FUniqueNetIdPS4Ref LocalUserId, const FString& ListName, const NpToolkit::Friend::Friends& NpFriendsList, const TUniqueNetIdMap<FString>& SessionIdsByFriendId);
	bool AddFriend(FUniqueNetIdPS4Ref LocalUserId, const NpToolkit::UserProfile::NpProfile& NpProfile, const NpToolkit::Presence::Presence& NpPresence);
	bool RemoveFriend(FUniqueNetIdPS4Ref LocalUserId, FUniqueNetIdPS4Ref RemovedFriendId);

	/** Map of players who have friend list reads in progress */
	using FReadListCompleteCallbacksByListName = TMap<FString, TArray<FOnReadFriendsListComplete>>;
	TUniqueNetIdMap<FReadListCompleteCallbacksByListName> AllReadListCallbacksByLocalId;

private:
	FOnlineSubsystemPS4& PS4Subsystem;

	/** All cached friends lists, organized first by local user ID, then by the name of each list */
	using FFriendListsByName = TMap<FString, TArray<FOnlineFriendPS4Ref>>;
	TUniqueNetIdMap<FFriendListsByName> AllFriendListsByUserId;
};

typedef TSharedPtr<FOnlineFriendsPS4, ESPMode::ThreadSafe> FOnlineFriendsPS4Ptr;
typedef TSharedRef<FOnlineFriendsPS4, ESPMode::ThreadSafe> FOnlineFriendsPS4Ref;
