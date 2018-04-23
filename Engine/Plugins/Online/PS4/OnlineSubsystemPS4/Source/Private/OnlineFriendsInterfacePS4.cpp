// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineFriendsInterfacePS4.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineIdentityInterfacePS4.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlinePresenceInterfacePS4.h"

#include "Async/AsyncWork.h"
#include "WebApiPS4Task.h"

#include "target/include/json2.h"

NpToolkit::Friend::FriendsRetrievalMode GetFriendListRequestFlagPS4(const FString& ListName)
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

	UE_LOG_ONLINE(Warning, TEXT("Friend list not supported on PS4: ListName=%s"), *ListName);
	return NpToolkit::Friend::FriendsRetrievalMode::invalid;
}


/**
 *	Task object for getting PS4 friends list
 */
class FOnlineAsyncTaskPS4ReadFriendsList : public FOnlineAsyncTaskPS4
{
public:

	/** Constructor */
	FOnlineAsyncTaskPS4ReadFriendsList(FOnlineSubsystemPS4* InSubsystem, FOnlineFriendsPS4* InFriendsPtr, FUniqueNetIdPS4 const& InLocalUserId, int32 InLocalUserNum, FString const& InListName, FOnReadFriendsListComplete const& InDelegate)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, FriendsPtr(InFriendsPtr)
		, LocalUserId(InLocalUserId.AsShared())
		, LocalUserNum(InLocalUserNum)
		, ListName(InListName)
		, ErrorStr()
		, Delegate(InDelegate)
		, NumFriendsFound(0)
	{
		// Request the friends list from the PSN toolkit
		NpToolkit::Friend::Request::GetFriends Request;
		Request.userId = LocalUserId->GetUserId();
		Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
		Request.async = true;
		Request.mode = GetFriendListRequestFlagPS4(ListName);
		Request.limit = 0;
		Request.offset = 0;

		int32 RequestID = NpToolkit::Friend::getFriends(Request, &Response);
		if (RequestID < SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			ErrorStr += FString::Printf(TEXT("getFriends failed: 0x%x"), RequestID);
		}
	}


	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4ReadFriendsList bWasSuccessful: %d NumFriendsFound: %d FriendToSessionIdMap.Num: %d ErrorStr: %s"), bWasSuccessful, NumFriendsFound, FriendToSessionIdMap.Num(), *ErrorStr);
	}

	virtual void Tick() override
	{
		if (!Response.isLocked())
		{
			if (Response.getReturnCode() == SCE_TOOLKIT_NP_V2_SUCCESS)
			{
				NpToolkit::Friend::Friends const* ResponseFriends = Response.get();
				NumFriendsFound = (int32)ResponseFriends->numFriends;

				FAsyncTask<FWebApiPS4Task> AsyncTask(Subsystem->GetUserWebApiContext(LocalUserId.Get()));
				AsyncTask.GetTask().SetRequest(TEXT("sessionInvitation"), FString(TEXT("/v1/users/me/friends/sessions?fields=@default,sessionName,friends")), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);
				AsyncTask.StartSynchronousTask();
				
				const FWebApiPS4Task& PS4Task = AsyncTask.GetTask();
				bWasSuccessful = PS4Task.GetHttpStatusCode() == 200;

				if (bWasSuccessful)
				{
					const FString TaskResponseBody = PS4Task.GetResponseBody();
					sce::Json::Value Root;
					const int32 JsonParseResult = sce::Json::Parser::parse(Root, TCHAR_TO_ANSI(*TaskResponseBody), TaskResponseBody.Len());
					if (JsonParseResult >= 0)
					{
						const uint64 size = Root["size"].getUInteger();
						for (uint64 i = 0; i < size; ++i)
						{
							const FString SessionId = FString(ANSI_TO_TCHAR(Root["sessions"][i]["sessionId"].getString().c_str()));
							if (!SessionId.IsEmpty())
							{
								for (int j = 0; j < Root["sessions"][i]["friends"].count(); j++)
								{
									if (Root["sessions"][i]["friends"][j]["platform"].getString() == "PS4")
									{
										sce::Json::String FriendId = Root["sessions"][i]["friends"][j]["onlineId"].getString();
										const char* FriendIdStr = FriendId.c_str();
										FriendToSessionIdMap.Add(FString(ANSI_TO_TCHAR(FriendIdStr)), SessionId);
									}
								}
							}
						}
					}
					else
					{
						bWasSuccessful = false;
						ErrorStr = FString::Printf(TEXT("sce::Json::Parser::parse failed parsing the result : 0x%x"), JsonParseResult);
					}
				}
				else
				{
					bWasSuccessful = false;
					ErrorStr = FString::Printf(TEXT("sessionInvitation query failed : HttpStatusCode=%d, ErrorString=%s, ResponseBody=%s"), PS4Task.GetHttpStatusCode(), *PS4Task.GetErrorString(), *PS4Task.GetResponseBody());
				}
			}
			else
			{
				bWasSuccessful = false;
				ErrorStr = FString::Printf(TEXT("NpToolkit::Friend::getFriends aysnc failed : 0x%x"), Response.getReturnCode());
			}

			bIsComplete = true;
		}
	}


	virtual void Finalize() override
	{
		FOnlineAsyncTaskPS4::Finalize();

		if (!bWasSuccessful)
		{
			UE_LOG_ONLINE(Warning, TEXT("%s"), *ToString());
		}
		else
		{
			// Copy the friend data into our friend list
			FOnlineFriendsPS4Map& FriendsListMapping = FriendsPtr->FriendsMap.FindOrAdd(ListName);
			TArray<FOnlineFriendPS4Ref>& FriendsList = FriendsListMapping.FindOrAdd(LocalUserId->ToString());
			FriendsList.Empty();

			NpToolkit::Friend::Friends const* ResponseFriends = Response.get();

			for (int32 FriendIndex = 0; FriendIndex < ResponseFriends->numFriends; ++FriendIndex)
			{
				NpToolkit::Friend::Friend const& Friend = ResponseFriends->friends[FriendIndex];
				const FString OnlineId = PS4OnlineIdToString(Friend.profile.onlineUser.onlineId);

				const FString* const SessionId = FriendToSessionIdMap.Find(OnlineId);
				const int32 LocalFriendIndex = FriendsList.Add(MakeShared<FOnlineFriendPS4>(Friend, Subsystem, SessionId ? *SessionId : FString()));

				IOnlinePresencePtr PresenceInterface = Subsystem->GetPresenceInterface();
				FOnlinePresencePS4Ptr PS4PresenceInterface = StaticCastSharedPtr<FOnlinePresencePS4>(PresenceInterface);

				TSharedRef<FOnlineUserPresence> NewPresence(MakeShared<FOnlineUserPresence>(FriendsList[LocalFriendIndex]->GetPresence()));
				PS4PresenceInterface->CachedPresenceData.Add(PS4AccountIdToString(Friend.profile.onlineUser.accountId), NewPresence);
			}
		}
	}

	virtual void TriggerDelegates() override
	{
		Delegate.ExecuteIfBound(LocalUserNum, bWasSuccessful, ListName, ErrorStr);
		FriendsPtr->TriggerOnFriendsChangeDelegates(LocalUserNum);
	}


private:

	/** Interface pointer to trigger the delegates on */
	FOnlineFriendsPS4* FriendsPtr;

	/** The local user number requesting profile data */
	TSharedRef<FUniqueNetIdPS4 const> LocalUserId;
	int32 LocalUserNum;

	/** The type of friend list we are retrieving*/
	FString ListName;

	/** The array of friend profile data that is filled out asynchronously */
	NpToolkit::Core::Response<NpToolkit::Friend::Friends> Response;

	/** The error string */
	FString ErrorStr;

	/** Delegate to call after the friends list read request has completed */
	FOnReadFriendsListComplete Delegate;

	/** Map of friends to sessions they have joined */
	TMap<FString, FString> FriendToSessionIdMap;

	/** Number of friends found */
	int32 NumFriendsFound;
};

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

FOnlineFriendPS4::FOnlineFriendPS4(NpToolkit::Friend::Friend const& InOnlineNpFriend, FOnlineSubsystemPS4 const* InSubsystem, const FString& SessionId /*= FString()*/)
	: UniqueNetIdPS4(FUniqueNetIdPS4::FindOrCreate(InOnlineNpFriend.profile.onlineUser.accountId, InOnlineNpFriend.profile.onlineUser.onlineId))
	, RealName(PS4FullRealName(InOnlineNpFriend.profile))
	, DisplayName(PS4OnlineIdToString(InOnlineNpFriend.profile.onlineUser.onlineId))
{
	const NpToolkit::Presence::Presence& NpPresence = InOnlineNpFriend.presence;

	// Debug printing of friend presence
	if (UE_LOG_ACTIVE(LogOnline, VeryVerbose))
	{
		UE_LOG_ONLINE(VeryVerbose, TEXT("========="));
		UE_LOG_ONLINE(VeryVerbose, TEXT("Friend OnlineId: %s"), *PS4OnlineIdToString(InOnlineNpFriend.profile.onlineUser.onlineId));
		UE_LOG_ONLINE(VeryVerbose, TEXT("Friend AccountId: %llu"), InOnlineNpFriend.profile.onlineUser.accountId);
		UE_LOG_ONLINE(VeryVerbose, TEXT("Online Status: %s"), OnlineStatusToTCHAR(NpPresence.psnOnlineStatus));
		UE_LOG_ONLINE(VeryVerbose, TEXT("Session Id: %s"), SessionId.IsEmpty() ? TEXT("None") : *SessionId);
		for (int32 PlatformIndex = 0; PlatformIndex < NpPresence.numPlatforms; ++PlatformIndex)
		{
			const NpToolkit::Presence::PlatformPresence& PlatformPresence = NpPresence.platforms[PlatformIndex];

			UE_LOG_ONLINE(VeryVerbose, TEXT("Platform Presence"));
			UE_LOG_ONLINE(VeryVerbose, TEXT("  Platform: %s"), PlatformTypeToTCHAR(PlatformPresence.platform));
			UE_LOG_ONLINE(VeryVerbose, TEXT("  Title Id: %s"), ANSI_TO_TCHAR(PlatformPresence.npTitleId.id));
			UE_LOG_ONLINE(VeryVerbose, TEXT("  Title Name: %s"), UTF8_TO_TCHAR(PlatformPresence.npTitleName));
			UE_LOG_ONLINE(VeryVerbose, TEXT("  Game Status: %s"), UTF8_TO_TCHAR(PlatformPresence.gameStatus));
			UE_LOG_ONLINE(VeryVerbose, TEXT("  Online Status: %s"), OnlineStatusToTCHAR(PlatformPresence.onlineStatusOnPlatform));
		}
		UE_LOG_ONLINE(VeryVerbose, TEXT("========="));
	}

	// Find the PS4 Platform presence data
	NpToolkit::Presence::PlatformPresence* PS4PlatformPresence = nullptr;
	for (int32 PlatformIndex = 0; PlatformIndex < NpPresence.numPlatforms; ++PlatformIndex)
	{
		if (NpPresence.platforms[PlatformIndex].platform == NpToolkit::Messaging::PlatformType::ps4)
		{
			PS4PlatformPresence = &NpPresence.platforms[PlatformIndex];
			break;
		}
	}

	// Fill out presence details
	switch (NpPresence.psnOnlineStatus)
	{
	case NpToolkit::Presence::OnlineStatus::online:
		Presence.Status.State = EOnlinePresenceState::Online;
		break;

	case NpToolkit::Presence::OnlineStatus::standBy:
		Presence.Status.State = EOnlinePresenceState::Away;
		break;

	default:
		Presence.Status.State = EOnlinePresenceState::Offline;
		break;
	}

	Presence.bIsOnline = NpPresence.psnOnlineStatus == NpToolkit::Presence::OnlineStatus::online;
	Presence.SessionId = MakeShared<FUniqueNetIdString>(SessionId);
	
	if (PS4PlatformPresence)
	{
		Presence.Status.StatusStr = FString(UTF8_TO_TCHAR(PS4PlatformPresence->gameStatus));
		
		// Is our friend playing this game? (Presence status is only set for matching titles)
		const FString OurTitleId = InSubsystem->GetAppId();
		const bool bHasPresenceStatus = !Presence.Status.StatusStr.IsEmpty();
		const bool bIsSameTitleId = OurTitleId.Equals(ANSI_TO_TCHAR(PS4PlatformPresence->npTitleId.id));
		Presence.bIsPlayingThisGame = bHasPresenceStatus || bIsSameTitleId;

		// *** TODO: how to tell if the session is joinable? (Should be baked into presence data when set?)
		Presence.bIsJoinable = Presence.bIsPlayingThisGame && Presence.SessionId.IsValid();
	}
	else
	{
		Presence.bIsPlayingThisGame = false;
		Presence.bIsJoinable = false;
	}

	if (Presence.Status.StatusStr.IsEmpty())
	{
		// No presence string found in the PS4 platform. Choose the first, non-null status we find on other platforms.
		for (int32 PlatformIndex = 0; PlatformIndex < NpPresence.numPlatforms; ++PlatformIndex)
		{
			if (NpPresence.platforms[PlatformIndex].gameStatus[0])
			{
				Presence.Status.StatusStr = FString(UTF8_TO_TCHAR(NpPresence.platforms[PlatformIndex].gameStatus));
				break;
			}
		}
	}

	// *** TODO: How do we tell if the user is active or idle? (Should be baked into presence data when set?)
	Presence.bIsPlaying = true;
	
	// *** TODO: Presence information is not being written by the PS4 OSS yet so this bit of code is untested.
	// *** there is 128 bytes in NpFriend.presence.gameInfo.gameData what we can (un)serialize some Unreal-specific data into/from.
	// *** We'll need to agree on some kind of scheme for that and apply that here.
	Presence.bHasVoiceSupport = false;
	//Presence.Status.Properties; // *** TODO: Need a scheme for key/value pairs in/out of InOnlineNpFriend.presence.gameInfo.gameData
}

bool FOnlineFriendPS4::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	if (AttrName.Equals(TEXT("id"), ESearchCase::IgnoreCase))
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
	return Presence;
}


// FOnlineFriendsPS4

bool FOnlineFriendsPS4::ReadFriendsList(int32 LocalUserNum, const FString& ListName, const FOnReadFriendsListComplete& Delegate /*= FOnReadFriendsListComplete()*/)
{
	TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum));

	FString ErrorStr;
	if (GetFriendListRequestFlagPS4(ListName) == NpToolkit::Friend::FriendsRetrievalMode::invalid)
	{
		ErrorStr = FString::Printf(TEXT("List name not supported. ListName=%s"), *ListName);
	}
	else if (LocalUserId.IsValid())
	{
		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(new FOnlineAsyncTaskPS4ReadFriendsList(PS4Subsystem, this, *LocalUserId, LocalUserNum, ListName, Delegate));
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
	Delegate.ExecuteIfBound(LocalUserNum, false, FriendId, ListName, FString(TEXT("SendInvite() is not supported on PS4 platform")));
	return false;
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


bool FOnlineFriendsPS4::DeleteFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TriggerOnDeleteFriendCompleteDelegates(LocalUserNum, false, FriendId, ListName, FString(TEXT("DeleteFriend() is not supported on PS4 platform")));
	return false;
}


bool FOnlineFriendsPS4::GetFriendsList(int32 LocalUserNum, const FString& ListName, TArray< TSharedRef<FOnlineFriend> >& OutFriends)
{
	bool bResult = false;

	FOnlineFriendsPS4Map* FriendsListMapping = FriendsMap.Find(ListName);
	if (FriendsListMapping == NULL)
	{
		UE_LOG_ONLINE(Warning, TEXT("Friends list is supported. ListName=%s"), *ListName);
	}
	else
	{
		OutFriends.Empty();
		TSharedPtr<const FUniqueNetId> LocalUserId = PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum);
		if (LocalUserId.IsValid())
		{
			TArray<FOnlineFriendPS4Ref>* FriendsList = FriendsListMapping->Find(LocalUserId->ToString());
			if (FriendsList != NULL)
			{
				for (int32 FriendIdx=0; FriendIdx < (*FriendsList).Num(); ++FriendIdx)
				{
					OutFriends.Add((*FriendsList)[FriendIdx]);
				}
				bResult = true;
			}
		}
	}
	return bResult;
}


TSharedPtr<FOnlineFriend> FOnlineFriendsPS4::GetFriend(int32 LocalUserNum, const FUniqueNetId& FriendId, const FString& ListName)
{
	TSharedPtr<FOnlineFriend> Result;
	
	FOnlineFriendsPS4Map* FriendsListMapping = FriendsMap.Find(ListName);
	if (FriendsListMapping == NULL)
	{
		UE_LOG_ONLINE(Warning, TEXT("Friends list is supported. ListName=%s"), *ListName);
	}
	else
	{
		TSharedPtr<const FUniqueNetId> LocalUserId = PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum);
		if (LocalUserId.IsValid())
		{
			TArray<FOnlineFriendPS4Ref>* FriendsList = FriendsListMapping->Find(LocalUserId->ToString());
			if (FriendsList != NULL)
			{
				for (int32 FriendIdx=0; FriendIdx < (*FriendsList).Num(); ++FriendIdx)
				{
					const FUniqueNetId& CurrentFriendId = *((*FriendsList)[FriendIdx]->GetUserId());
					if (CurrentFriendId == FriendId)
					{
						Result = (*FriendsList)[FriendIdx];
						break;
					}
				}
			}
		}
	}

	return Result;
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
