// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4.h"
#include "OnlineFriendsInterfacePS4.h"

//////////////////////////////////////////////////////////////////////////
// FOnlineEventPS4_FriendRemoved 
//////////////////////////////////////////////////////////////////////////

class FOnlineEventPS4_FriendRemoved : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FOnlineEventPS4_FriendRemoved(FOnlineSubsystemPS4& PS4Subsystem, FUniqueNetIdPS4Ref InLocalUserId, FUniqueNetIdPS4Ref InRemovedFriendId)
		: FOnlineAsyncEvent(&PS4Subsystem)
		, LocalUserId(InLocalUserId)
		, RemovedFriendId(InRemovedFriendId)
	{}

	virtual FString ToString() const override { return TEXT("FOnlineEventPS4_FriendRemoved"); }

	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	FUniqueNetIdPS4Ref LocalUserId;
	FUniqueNetIdPS4Ref RemovedFriendId;
	bool bRemovedCachedInfo = false;
};

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_FriendOnlineStatusChanged
//////////////////////////////////////////////////////////////////////////

//class FOnlineAsyncTaskPS4_FriendOnlineStatusChanged : public FOnlineAsyncTaskPS4
//{
//public:
//	FOnlineAsyncTaskPS4_FriendOnlineStatusChanged(FOnlineSubsystemPS4& PS4Subsystem, const TSharedRef<const FUniqueNetIdPS4>& InUpdatedFriendId)
//		: FOnlineAsyncTaskPS4(&PS4Subsystem)
//		, UpdatedFriendId(InUpdatedFriendId)
//	{
//	}
//
//	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskPS4_FriendOnlineStatusChanged"); }
//
//	virtual void Tick() override;
//	virtual void Finalize() override;
//	virtual void TriggerDelegates() override;
//
//private:
//	TSharedPtr<const FUniqueNetIdPS4> UpdatedFriendId;
//	NpToolkit::Core::Response<NpToolkit::Presence::Presence> PresenceResponse;
//};

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_FriendAdded
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_FriendAdded : public FOnlineAsyncTaskPS4
{
public:
	FOnlineAsyncTaskPS4_FriendAdded(FOnlineSubsystemPS4& PS4Subsystem, FUniqueNetIdPS4Ref InLocalUserId, FUniqueNetIdPS4Ref InAddedFriendId)
		: FOnlineAsyncTaskPS4(&PS4Subsystem)
		, LocalUserId(InLocalUserId)
		, AddedFriendId(InAddedFriendId)
	{
	}

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskPS4_FriendAdded"); }
	virtual void Initialize() override;
	virtual void Tick() override;
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	FUniqueNetIdPS4Ref LocalUserId;
	FUniqueNetIdPS4Ref AddedFriendId;
	bool bWasActuallyAddedToList = false;

	NpToolkit::Core::Response<NpToolkit::UserProfile::NpProfiles> ProfileInfoResponse;
	NpToolkit::Core::Response<NpToolkit::Presence::Presence> PresenceResponse;
};

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_ReadFriendsList
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_ReadFriendsList : public FOnlineAsyncTaskPS4
{
public:
	FOnlineAsyncTaskPS4_ReadFriendsList(FOnlineSubsystemPS4& PS4Subsystem, FUniqueNetIdPS4Ref InLocalUserId, int32 InLocalUserNum, const FString& InListName);

	virtual FString ToString() const override;
	virtual void Initialize() override;
	virtual void Tick() override;
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	int32 LocalUserNum = INDEX_NONE;
	FUniqueNetIdPS4Ref LocalUserId;

	FString ListName;
	int32 NumFriendsFound = 0;
	FString ErrorStr;

	NpToolkit::Core::Response<NpToolkit::Friend::Friends> GetFriendsResponse;
	NpToolkit::Core::Response<NpToolkit::Session::Sessions> FindFriendSessionsResponse;
};