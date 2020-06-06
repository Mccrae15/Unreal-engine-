// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineFriendsInterfacePS4.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineIdentityInterfacePS4.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlinePresenceInterfacePS4.h"
#include "OnlineExternalUIInterfacePS4.h"

#include "AsyncTasks/OnlineAsyncItemsPS4_Sessions.h"
#include "AsyncTasks/OnlineAsyncItemsPS4_Friends.h"
#include <np_profile_dialog.h>

//#include "Async/AsyncWork.h"
//#include "WebApiPS4Task.h"
//
//#include "target/include/json2.h"

namespace
{
	const TCHAR* OnlineStatusToTCHAR(const NpToolkit::Presence::OnlineStatus OnlineStatus)
	{
		switch (OnlineStatus)
		{
		case NpToolkit::Presence::OnlineStatus::notRequested:
			return TEXT("Not Requested");
		case NpToolkit::Presence::OnlineStatus::offline:
			return TEXT("Offline");
		case NpToolkit::Presence::OnlineStatus::online:
			return TEXT("Online");
		case NpToolkit::Presence::OnlineStatus::standBy:
			return TEXT("Standby");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* PlatformTypeToTCHAR(const NpToolkit::Messaging::PlatformType PlatformType)
	{
		switch (PlatformType)
		{
		case NpToolkit::Messaging::PlatformType::none:
			return TEXT("None");
		case NpToolkit::Messaging::PlatformType::ps3:
			return TEXT("PS3");
		case NpToolkit::Messaging::PlatformType::ps4:
			return TEXT("PS4");
		case NpToolkit::Messaging::PlatformType::psVita:
			return TEXT("PSVita");
		default:
			return TEXT("Unknown");
		}
	}
}

// FOnlineFriendPS4

FOnlineFriendPS4Ref FOnlineFriendPS4::Create(const FOnlineSubsystemPS4& PS4Subsystem, const NpToolkit::Friend::Friend& NpFriend)
{
	return Create(PS4Subsystem, NpFriend.profile, NpFriend.presence);
}

FOnlineFriendPS4Ref FOnlineFriendPS4::Create(const FOnlineSubsystemPS4& PS4Subsystem, const NpToolkit::UserProfile::NpProfile& NpProfile, const NpToolkit::Presence::Presence& NpPresence)
{
	return MakeShareable(new FOnlineFriendPS4(PS4Subsystem, NpProfile, NpPresence));
}

FOnlineUserPresencePS4& FOnlineFriendPS4::GetPresencePS4()
{
	return *Presence;
}

FOnlineFriendPS4::FOnlineFriendPS4(const FOnlineSubsystemPS4& PS4Subsystem, const NpToolkit::UserProfile::NpProfile& NpProfile , const NpToolkit::Presence::Presence& NpPresence)
	: UniqueNetIdPS4(FUniqueNetIdPS4::FindOrCreate(NpProfile.onlineUser))
	, RealName(PS4FullRealName(NpProfile))
	, DisplayName(PS4OnlineIdToString(NpProfile.onlineUser.onlineId))
	, Presence(StaticCastSharedPtr<FOnlinePresencePS4>(PS4Subsystem.GetPresenceInterface())->FindOrCreatePresence(UniqueNetIdPS4))
{
	Presence->Update(PS4Subsystem, NpPresence);

	// Debug printing of friend presence
	if (UE_LOG_ACTIVE(LogOnlineFriend, VeryVerbose))
	{
		UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("========="));
		UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("Friend OnlineId: %s"), *DisplayName);
		UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("Friend AccountId: %llu"), NpProfile.onlineUser.accountId);
		UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("Online Status: %s"), OnlineStatusToTCHAR(NpPresence.psnOnlineStatus));
		UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("Session Id: %s"), Presence->SessionId ? *Presence->SessionId->ToDebugString() : TEXT("None"));
		for (int32 PlatformIndex = 0; PlatformIndex < NpPresence.numPlatforms; ++PlatformIndex)
		{
			const NpToolkit::Presence::PlatformPresence& PlatformPresence = NpPresence.platforms[PlatformIndex];

			UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("Platform Presence"));
			UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("  Platform: %s"), PlatformTypeToTCHAR(PlatformPresence.platform));
			UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("  Title Id: %s"), ANSI_TO_TCHAR(PlatformPresence.npTitleId.id));
			UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("  Title Name: %s"), UTF8_TO_TCHAR(PlatformPresence.npTitleName));
			UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("  Game Status: %s"), UTF8_TO_TCHAR(PlatformPresence.gameStatus));
			UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("  Online Status: %s"), OnlineStatusToTCHAR(PlatformPresence.onlineStatusOnPlatform));
		}
		UE_LOG_ONLINE_FRIEND(VeryVerbose, TEXT("========="));
	}
}

bool FOnlineFriendPS4::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	if (AttrName.Equals(USER_ATTR_ID, ESearchCase::IgnoreCase))
	{
		OutAttrValue = PS4AccountIdToString(UniqueNetIdPS4->GetAccountId());
		return true;
	}

	return false;
}

EInviteStatus::Type FOnlineFriendPS4::GetInviteStatus() const
{
	// Not supported on PS4
	return EInviteStatus::Accepted;
}

const FOnlineUserPresence& FOnlineFriendPS4::GetPresence() const
{
	return *Presence;
}

/*static*/ NpToolkit::Friend::FriendsRetrievalMode FOnlineFriendsPS4::GetFriendListRequestFlagPS4(const FString& ListName)
{
	if (ListName.Equals(EFriendsLists::ToString(EFriendsLists::Default), ESearchCase::IgnoreCase))
	{
		return NpToolkit::Friend::FriendsRetrievalMode::all;
	}

	if (ListName.Equals(EFriendsLists::ToString(EFriendsLists::OnlinePlayers), ESearchCase::IgnoreCase))
	{
		return NpToolkit::Friend::FriendsRetrievalMode::online;
	}

	if (ListName.Equals(EFriendsLists::ToString(EFriendsLists::InGamePlayers), ESearchCase::IgnoreCase))
	{
		return NpToolkit::Friend::FriendsRetrievalMode::inContext;
	}

	UE_LOG_ONLINE_FRIEND(Warning, TEXT("Friend list not supported on PS4: ListName=%s"), *ListName);
	return NpToolkit::Friend::FriendsRetrievalMode::invalid;
}

bool FOnlineFriendsPS4::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate /*= FOnReadFriendsListComplete()*/)
{
	TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(PS4Subsystem.GetIdentityInterface()->GetUniquePlayerId(LocalUserNum));

	FString ErrorStr;
	if (GetFriendListRequestFlagPS4(ListName) == NpToolkit::Friend::FriendsRetrievalMode::invalid)
	{
		ErrorStr = FString::Printf(TEXT("List name not supported. ListName=%s"), *ListName);
	}
	else if (LocalUserId.IsValid())
	{
		// Add our delegate to this user's delegate map
		TMap<FString, TArray<FOnReadFriendsListComplete>>& ReadCompleteCallbacksByListName = AllReadListCallbacksByLocalId.FindOrAdd(LocalUserId.ToSharedRef());
		TArray<FOnReadFriendsListComplete>& ReadCompleteCallbacks = ReadCompleteCallbacksByListName.FindOrAdd(ListName);

		ReadCompleteCallbacks.Add(Delegate);

		const bool bRequestsAreInProgress = ReadCompleteCallbacks.Num() > 1;
		if (!bRequestsAreInProgress)
		{
			// Only start the task if no requests are in progress (in-progress task will call our delegate registered above)
			PS4Subsystem.GetAsyncTaskManager()->AddToInQueue(new FOnlineAsyncTaskPS4_ReadFriendsList(PS4Subsystem, LocalUserId.ToSharedRef(), LocalUserNum, ListName));
		}
	}
	else
	{
		ErrorStr = FString::Printf(TEXT("No valid LocalUserNum=%d"), LocalUserNum);
	}

	if (!ErrorStr.IsEmpty())
	{
		Delegate.ExecuteIfBound(LocalUserNum, false, ListName, ErrorStr);
		return false;
	}
	return true;
}


bool FOnlineFriendsPS4::DeleteFriendsList(int32 LocalUserNum, const FString& ListName, const FOnDeleteFriendsListComplete& Delegate /*= FOnDeleteFriendsListComplete()*/)
{
	Delegate.ExecuteIfBound(LocalUserNum, false, ListName, FString(TEXT("DeleteFriendsList() is not supported on PS4 platform")));
	return false;
}


bool FOnlineFriendsPS4::SendInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnSendInviteComplete& Delegate /*= FOnSendInviteComplete()*/)
{
	TSharedRef<const FUniqueNetIdPS4> PS4PersonToFriend = FUniqueNetIdPS4::Cast(FriendId).AsShared();

	IOnlineIdentityPtr IdentityPtr = PS4Subsystem.GetIdentityInterface();
	check(IdentityPtr.IsValid());

	TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(IdentityPtr->GetUniquePlayerId(LocalUserNum));
	if (!LocalUserId.IsValid())
	{
		PS4Subsystem.ExecuteNextTick([LocalUserNum, PS4PersonToFriend, ListName, Delegate]()
		{
			UE_LOG_ONLINE_FRIEND(Warning, TEXT("FOnlineFriendsPS4::SendInvite - Local user not signed in"));
			Delegate.ExecuteIfBound(LocalUserNum, false, *PS4PersonToFriend, ListName, TEXT("Local user not signed in"));
		});
		return false;
	}

	if (IsFriend(LocalUserNum, FriendId, ListName))
	{
		PS4Subsystem.ExecuteNextTick([LocalUserNum, PS4PersonToFriend, ListName, Delegate]()
		{
			UE_LOG_ONLINE_FRIEND(Warning, TEXT("FOnlineFriendsPS4::SendInvite - %s is already a friend"), *PS4PersonToFriend->ToDebugString());
			Delegate.ExecuteIfBound(LocalUserNum, false, *PS4PersonToFriend, ListName, TEXT("Selected user is already a friend"));
		});
		return false;
	}

	int32 ReturnCode = sceNpProfileDialogInitialize();
	if (ReturnCode < 0)
	{
		// FAILURE
		UE_LOG_ONLINE_FRIEND(Error, TEXT("sceNpProfileDialogInitialize error = 0x%08x"), ReturnCode);
		sceNpProfileDialogTerminate();

		PS4Subsystem.ExecuteNextTick([LocalUserNum, PS4PersonToFriend, ListName, Delegate]()
		{
			UE_LOG_ONLINE_FRIEND(Warning, TEXT("FOnlineFriendsPS4::SendInvite - Failed to init dialog"));
			Delegate.ExecuteIfBound(LocalUserNum, false, *PS4PersonToFriend, ListName, TEXT("Failed to open Send Friend Invite dialog"));
		});
		return false;
	}

	SceNpProfileDialogParamA DialogParam;
	sceNpProfileDialogParamInitializeA(&DialogParam);
	DialogParam.mode = SCE_NP_PROFILE_DIALOG_MODE_FRIEND_REQUEST;
	DialogParam.userId = LocalUserId->GetUserId();
	DialogParam.targetAccountId = PS4PersonToFriend->GetAccountId();

	ReturnCode = sceNpProfileDialogOpenA(&DialogParam);
	if (ReturnCode < 0)
	{
		// FAILURE
		UE_LOG_ONLINE_FRIEND(Error, TEXT("sceNpProfileDialogOpenA error = 0x%08x"), ReturnCode);
		sceNpProfileDialogTerminate();

		PS4Subsystem.ExecuteNextTick([LocalUserNum, PS4PersonToFriend, ListName, Delegate]()
		{
			UE_LOG_ONLINE_FRIEND(Warning, TEXT("FOnlineFriendsPS4::SendInvite - Failed to open dialog"));
			Delegate.ExecuteIfBound(LocalUserNum, false, *PS4PersonToFriend, ListName, TEXT("Failed to open Send Friend Invite dialog"));
		});
		return false;
	}

	// Trigger to let us know if the external UI has been made active
	IOnlineExternalUIPtr ExternalUIPtr = PS4Subsystem.GetExternalUIInterface();
	if (ExternalUIPtr.IsValid())
	{
		ExternalUIPtr->TriggerOnExternalUIChangeDelegates(true);
	}

	const FOnProfileUIClosedDelegate CompletionDelegate = FOnProfileUIClosedDelegate::CreateLambda([LocalUserNum, PS4PersonToFriend, ListName, Delegate]()
	{
		Delegate.ExecuteIfBound(LocalUserNum, true, *PS4PersonToFriend, ListName, FString());
	});

	// Async task to monitor when the profile external UI has been closed
	FOnlineAsyncTaskPS4TrackProfileUIStatus* const NewShowProfileUITask = new FOnlineAsyncTaskPS4TrackProfileUIStatus(PS4Subsystem, CompletionDelegate);
	PS4Subsystem.GetAsyncTaskManager()->AddToParallelTasks(NewShowProfileUITask);

	return true;
}


bool FOnlineFriendsPS4::AcceptInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnAcceptInviteComplete& Delegate /*= FOnAcceptInviteComplete()*/)
{
	Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, FString(TEXT("AcceptInvite() is not supported on PS4 platform")));
	return false;
}


bool FOnlineFriendsPS4::RejectInvite(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TriggerOnRejectInviteCompleteDelegates(LocalUserNum, false, FriendId, ListName, FString(TEXT("RejectInvite() is not supported on PS4 platform")));
	return false;
}


void FOnlineFriendsPS4::SetFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FString& Alias, const FOnSetFriendAliasComplete& Delegate /*= FOnSetFriendAliasComplete()*/)
{
	TSharedRef<const FUniqueNetId> FriendIdRef = FriendId.AsShared();
	PS4Subsystem.ExecuteNextTick([LocalUserNum, FriendIdRef, ListName, Delegate]()
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("FOnlineFriendsPS4::SetFriendAlias is not supported on PS4 platform"));
		Delegate.ExecuteIfBound(LocalUserNum, *FriendIdRef, ListName, FOnlineError(EOnlineErrorResult::NotImplemented));
	});
}


void FOnlineFriendsPS4::DeleteFriendAlias(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName, const FOnDeleteFriendAliasComplete& Delegate)
{
	TSharedRef<const FUniqueNetId> FriendIdRef = FriendId.AsShared();
	PS4Subsystem.ExecuteNextTick([LocalUserNum, FriendIdRef, ListName, Delegate]()
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("FOnlineFriendsPS4::DeleteFriendAlias is not supported on PS4 platform"));
		Delegate.ExecuteIfBound(LocalUserNum, *FriendIdRef, ListName, FOnlineError(EOnlineErrorResult::NotImplemented));
	});
}


bool FOnlineFriendsPS4::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TSharedRef<const FUniqueNetIdPS4> PS4PersonToFriend = FUniqueNetIdPS4::Cast(FriendId).AsShared();

	IOnlineIdentityPtr IdentityPtr = PS4Subsystem.GetIdentityInterface();
	check(IdentityPtr.IsValid());

	TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(IdentityPtr->GetUniquePlayerId(LocalUserNum));
	if (!LocalUserId.IsValid())
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("FOnlineFriendsPS4::SendInvite - Local user not signed in"));
		return false;
	}

	if (!IsFriend(LocalUserNum, FriendId, ListName))
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("FOnlineFriendsPS4::SendInvite - %s is already not a friend"), *PS4PersonToFriend->ToDebugString());
		return false;
	}

	int32 ReturnCode = sceNpProfileDialogInitialize();
	if (ReturnCode < 0)
	{
		// FAILURE
		UE_LOG_ONLINE_FRIEND(Error, TEXT("FOnlineFriendsPS4::SendInvite - Failed to open dialog; sceNpProfileDialogInitialize error = 0x%08x"), ReturnCode);
		sceNpProfileDialogTerminate();
		return false;
	}

	SceNpProfileDialogParamA DialogParam;
	sceNpProfileDialogParamInitializeA(&DialogParam);
	DialogParam.mode = SCE_NP_PROFILE_DIALOG_MODE_FRIEND_REQUEST;
	DialogParam.userId = LocalUserId->GetUserId();
	DialogParam.targetAccountId = PS4PersonToFriend->GetAccountId();

	ReturnCode = sceNpProfileDialogOpenA(&DialogParam);
	if (ReturnCode < 0)
	{
		// FAILURE
		UE_LOG_ONLINE_FRIEND(Error, TEXT("FOnlineFriendsPS4::SendInvite - Failed to open dialog; sceNpProfileDialogOpenA error = 0x%08x"), ReturnCode);
		sceNpProfileDialogTerminate();
		return false;
	}

	// Trigger to let us know if the external UI has been made active
	IOnlineExternalUIPtr ExternalUIPtr = PS4Subsystem.GetExternalUIInterface();
	if (ExternalUIPtr.IsValid())
	{
		ExternalUIPtr->TriggerOnExternalUIChangeDelegates(true);
	}

	// Async task to monitor when the profile external UI has been closed
	FOnlineAsyncTaskPS4TrackProfileUIStatus* const NewTrackProfileUIStatus = new FOnlineAsyncTaskPS4TrackProfileUIStatus(PS4Subsystem, FOnProfileUIClosedDelegate());
	PS4Subsystem.GetAsyncTaskManager()->AddToParallelTasks(NewTrackProfileUIStatus);

	return true;
}


bool FOnlineFriendsPS4::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends)
{
	bool bResult = false;

	if (TSharedPtr<const FUniqueNetId> LocalUserId = PS4Subsystem.GetIdentityInterface()->GetUniquePlayerId(LocalUserNum))
	{
		const FFriendListsByName* FriendListsByName = AllFriendListsByUserId.Find(LocalUserId.ToSharedRef());
		if (const TArray<FOnlineFriendPS4Ref>* FriendsList = FriendListsByName ? FriendListsByName->Find(ListName) : nullptr)
		{
			OutFriends.Empty();
			OutFriends.Append(*FriendsList);
			bResult = true;
		}
		else
		{
			UE_LOG_ONLINE_FRIEND(Warning, TEXT("Friends list name is not supported. ListName=%s"), *ListName);
		}
	}
	
	return bResult;
}


TSharedPtr<FOnlineFriend> FOnlineFriendsPS4::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	if (TSharedPtr<const FUniqueNetId> LocalUserId = PS4Subsystem.GetIdentityInterface()->GetUniquePlayerId(LocalUserNum))
	{
		const FFriendListsByName* FriendListsByName = AllFriendListsByUserId.Find(LocalUserId.ToSharedRef());
		if (const TArray<FOnlineFriendPS4Ref>* FriendsList = FriendListsByName ? FriendListsByName->Find(ListName) : nullptr)
		{
			const FOnlineFriendPS4Ref* MatchingFriend = FriendsList->FindByPredicate(
				[&FriendId](const FOnlineFriendPS4Ref& FriendEntry)
				{
					return *FriendEntry->GetUserId() == FriendId;
				});

			if (MatchingFriend)
			{
				return *MatchingFriend;
			}
		}
		else
		{
			UE_LOG_ONLINE_FRIEND(Warning, TEXT("Friends list name is not supported. ListName=%s"), *ListName);
		}
	}
	return TSharedPtr<FOnlineFriend>();
}


bool FOnlineFriendsPS4::IsFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TSharedPtr<FOnlineFriend> Friend = GetFriend(LocalUserNum, FriendId, ListName);
	return Friend.IsValid();
}

bool FOnlineFriendsPS4::QueryRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace)
{
	TriggerOnQueryRecentPlayersCompleteDelegates(UserId, Namespace, false, TEXT("not implemented"));

	return false;
}

bool FOnlineFriendsPS4::GetRecentPlayers(const FUniqueNetId& UserId, const FString& Namespace, TArray< TSharedRef<FOnlineRecentPlayer> >& OutRecentPlayers)
{
	return false;
}

void FOnlineFriendsPS4::DumpRecentPlayers() const
{
}

bool FOnlineFriendsPS4::BlockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	return false;
}

bool FOnlineFriendsPS4::UnblockPlayer(int32 LocalUserNum, const FUniqueNetId& PlayerId)
{
	return false;
}

bool FOnlineFriendsPS4::QueryBlockedPlayers(const FUniqueNetId& UserId)
{
	return false;
}

bool FOnlineFriendsPS4::GetBlockedPlayers(const FUniqueNetId& UserId, TArray< TSharedRef<FOnlineBlockedPlayer> >& OutBlockedPlayers)
{
	return false;
}

void FOnlineFriendsPS4::DumpBlockedPlayers() const
{
}

void FOnlineFriendsPS4::RebuildFriendsList(FUniqueNetIdPS4Ref LocalUserId, const FString& ListName, const NpToolkit::Friend::Friends& NpFriendsList, const TUniqueNetIdMap<FString>& SessionIdsByFriendId)
{
	// Copy the friend data into our friend list
	FFriendListsByName& FriendListsByName = AllFriendListsByUserId.FindOrAdd(LocalUserId);
	TArray<FOnlineFriendPS4Ref>& FriendsList = FriendListsByName.FindOrAdd(ListName);

	//@todo DanH: Let's be more responsible and surgical here - go through all the friends we got from the list and update friends in situ
	FriendsList.Empty();

	for (int32 FriendIdx = 0; FriendIdx < NpFriendsList.numFriends; ++FriendIdx)
	{
		const NpToolkit::Friend::Friend& NpFriend = NpFriendsList.friends[FriendIdx];
		FOnlineFriendPS4Ref NewFriendInfo = FOnlineFriendPS4::Create(PS4Subsystem, NpFriend);
		
		if (const FString* SessionId = SessionIdsByFriendId.Find(NewFriendInfo->GetUserId()))
		{
			NewFriendInfo->GetPresencePS4().SetSessionId(*SessionId);
		}
		
		FriendsList.Add(NewFriendInfo);
	}
}

bool FriendsListTypeFromString(const FString& ListName, EFriendsLists::Type& OutListType)
{
	if (ListName == ToString(EFriendsLists::Default))
	{
		OutListType = EFriendsLists::Default;
	}
	else if (ListName == ToString(EFriendsLists::OnlinePlayers))
	{
		OutListType = EFriendsLists::OnlinePlayers;
	}
	else if (ListName == ToString(EFriendsLists::InGamePlayers))
	{
		OutListType = EFriendsLists::InGamePlayers;
	}
	// Not supported on PS4
	/*else if (ListName == ToString(EFriendsLists::InGameAndSessionPlayers))
	{
		OutListType = EFriendsLists::InGameAndSessionPlayers;
	}*/
	else
	{
		return false;
	}
	return true;
}

bool FOnlineFriendsPS4::AddFriend(FUniqueNetIdPS4Ref LocalUserId, const NpToolkit::UserProfile::NpProfile& NpProfile, const NpToolkit::Presence::Presence& NpPresence)
{
	bool bWasAddedSuccessfully = false;

	const FOnlineFriendPS4Ref NewFriend = FOnlineFriendPS4::Create(PS4Subsystem, NpProfile, NpPresence);
	
	if (FFriendListsByName* FriendListsByName = AllFriendListsByUserId.Find(LocalUserId))
	{
		for (TPair<FString, TArray<FOnlineFriendPS4Ref>>& NameListPair : *FriendListsByName)
		{
			// Be sure we don't add the same user twice
			const bool bAlreadyInList = NameListPair.Value.ContainsByPredicate(
				[&NewFriend](const FOnlineFriendPS4Ref& FriendEntry)
				{
					return *FriendEntry->GetUserId() == *NewFriend->GetUserId();
				});

			if (!bAlreadyInList)
			{
				// Only add to this particular named list if they're eligible for it
				EFriendsLists::Type ListType = EFriendsLists::Default;
				if (FriendsListTypeFromString(NameListPair.Key, ListType))
				{
					bool bIsEligibleForList = false;
					switch (ListType)
					{
					case EFriendsLists::Default:
						bIsEligibleForList = true;
						break;
					case EFriendsLists::OnlinePlayers:
						bIsEligibleForList = NewFriend->GetPresence().bIsOnline;
						break;
					case EFriendsLists::InGamePlayers:
						bIsEligibleForList = NewFriend->GetPresence().bIsPlayingThisGame;
						break;
					}

					if (bIsEligibleForList)
					{
						bWasAddedSuccessfully = true;
						NameListPair.Value.Add(NewFriend);
					}
				}
			}
			else
			{
				UE_LOG_ONLINE_FRIEND(Warning, TEXT("Attempting to add new PS4 friend [%s], but they are already in list [%s]"), *NewFriend->GetUserId()->ToString(), *NameListPair.Key);
			}
		}
	}

	return bWasAddedSuccessfully;
}

bool FOnlineFriendsPS4::RemoveFriend(const FUniqueNetIdPS4Ref LocalUserId, FUniqueNetIdPS4Ref RemovedFriendId)
{
	int32 NumRemoved = 0;
	if (FFriendListsByName* FriendListsByName = AllFriendListsByUserId.Find(LocalUserId))
	{
		// Remove the entry for this user from all lists
		for (TPair<FString, TArray<FOnlineFriendPS4Ref>>& NameListPair : *FriendListsByName)
		{
			NumRemoved += NameListPair.Value.RemoveAll(
				[RemovedFriendId](const FOnlineFriendPS4Ref& CachedFriend)
				{
					return *CachedFriend->GetUserId() == *RemovedFriendId;
				});
		}
	}

	return NumRemoved > 0;
}
