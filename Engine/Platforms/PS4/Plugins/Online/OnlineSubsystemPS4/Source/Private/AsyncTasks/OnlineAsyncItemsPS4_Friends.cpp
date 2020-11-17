// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncItemsPS4_Friends.h"

//////////////////////////////////////////////////////////////////////////
// FOnlineEventPS4_FriendRemoved 
//////////////////////////////////////////////////////////////////////////

void FOnlineEventPS4_FriendRemoved::Finalize()
{
	bRemovedCachedInfo = StaticCastSharedPtr<FOnlineFriendsPS4>(Subsystem->GetFriendsInterface())->RemoveFriend(LocalUserId, RemovedFriendId);
}

void FOnlineEventPS4_FriendRemoved::TriggerDelegates()
{
	if (bRemovedCachedInfo)
	{
		Subsystem->GetFriendsInterface()->TriggerOnFriendRemovedDelegates(*LocalUserId, *RemovedFriendId);
	}
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_FriendAdded
//////////////////////////////////////////////////////////////////////////

void FOnlineAsyncTaskPS4_FriendAdded::Initialize()
{
	// This is two calls, but only asking for what we need. There's no way to get info for one specific friend, so getting the whole list seems a bit ridiculous.
	// We need both the profile and the presence for this user to get the equivalent info provided by the Friend
	NpToolkit::UserProfile::Request::GetNpProfiles GetProfilesRequest;
	GetProfilesRequest.userId = LocalUserId->GetUserId();
	GetProfilesRequest.accountIds[0] = AddedFriendId->GetAccountId();
	GetProfilesRequest.numValidAccountIds = 1;
	NpToolkit::UserProfile::getNpProfiles(GetProfilesRequest, &ProfileInfoResponse);

	NpToolkit::Presence::Request::GetPresence GetPresenceRequest;
	GetPresenceRequest.userId = LocalUserId->GetUserId();
	GetPresenceRequest.fromUser = AddedFriendId->GetAccountId();
	NpToolkit::Presence::getPresence(GetPresenceRequest, &PresenceResponse);
}

void FOnlineAsyncTaskPS4_FriendAdded::Tick()
{
	FOnlineAsyncTaskPS4::Tick();

	if (ProfileInfoResponse.getState() == NpToolkit::Core::ResponseState::ready && PresenceResponse.getState() == NpToolkit::Core::ResponseState::ready)
	{
		bWasSuccessful = ProfileInfoResponse.getReturnCode() == SCE_TOOLKIT_NP_V2_SUCCESS && PresenceResponse.getReturnCode() == SCE_TOOLKIT_NP_V2_SUCCESS;
		bIsComplete = true;
	}
}

void FOnlineAsyncTaskPS4_FriendAdded::Finalize()
{
	if (bWasSuccessful)
	{
		const NpToolkit::UserProfile::NpProfile& NpProfile = ProfileInfoResponse.get()->npProfiles[0];
		const NpToolkit::Presence::Presence& NpPresence = *PresenceResponse.get();

		bWasActuallyAddedToList = StaticCastSharedPtr<FOnlineFriendsPS4>(Subsystem->GetFriendsInterface())->AddFriend(LocalUserId, NpProfile, NpPresence);
	}
}

void FOnlineAsyncTaskPS4_FriendAdded::TriggerDelegates()
{
	if (bWasActuallyAddedToList)
	{
		Subsystem->GetFriendsInterface()->TriggerOnInviteAcceptedDelegates(*LocalUserId, *AddedFriendId);
	}
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_ReadFriendsList
//////////////////////////////////////////////////////////////////////////

FOnlineAsyncTaskPS4_ReadFriendsList::FOnlineAsyncTaskPS4_ReadFriendsList(FOnlineSubsystemPS4& PS4Subsystem, TSharedRef<const FUniqueNetIdPS4> InLocalUserId, int32 InLocalUserNum, const FString& InListName)
	: FOnlineAsyncTaskPS4(&PS4Subsystem)
	, LocalUserNum(InLocalUserNum)
	, LocalUserId(InLocalUserId)
	, ListName(InListName)
{
}

FString FOnlineAsyncTaskPS4_ReadFriendsList::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4_ReadFriendsList bWasSuccessful: %d NumFriendsFound: %d ErrorStr: %s"), WasSuccessful(), NumFriendsFound, *ErrorStr);
}

void FOnlineAsyncTaskPS4_ReadFriendsList::Initialize()
{
	// Request the friends list from the PSN toolkit
	NpToolkit::Friend::Request::GetFriends Request;
	Request.userId = LocalUserId->GetUserId();
	Request.mode = FOnlineFriendsPS4::GetFriendListRequestFlagPS4(ListName);
	Request.limit = 0;
	Request.offset = 0;

	int32 ResponseCode = NpToolkit::Friend::getFriends(Request, &GetFriendsResponse);
	if (ResponseCode < SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		ErrorStr += FString::Printf(TEXT("getFriends failed: 0x%x"), ResponseCode);
		bIsComplete = true;
	}

	//@todo DanH: We don't need to do this if/when the presence info contains the current session ID already
	//		Also, this assumes that there's only one session per user, which is by no means required
	NpToolkit::Session::Request::Search FindSessionsRequest;
	FindSessionsRequest.userId = LocalUserId->GetUserId();
	FindSessionsRequest.type = NpToolkit::Session::SearchType::friends;
	ResponseCode = NpToolkit::Session::search(FindSessionsRequest, &FindFriendSessionsResponse);
	if (ResponseCode < SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		ErrorStr += FString::Printf(TEXT("Session::search failed: 0x%x"), ResponseCode);
		bIsComplete = true;
	}
}

void FOnlineAsyncTaskPS4_ReadFriendsList::Tick()
{
	FOnlineAsyncTaskPS4::Tick();

	if (GetFriendsResponse.getState() == NpToolkit::Core::ResponseState::ready && FindFriendSessionsResponse.getState() == NpToolkit::Core::ResponseState::ready)
	{
		bIsComplete = true;
		
		const bool bGetFriendsSuccess = GetFriendsResponse.getReturnCode() == SCE_TOOLKIT_NP_V2_SUCCESS;
		const bool bFindSessionsSuccess = FindFriendSessionsResponse.getReturnCode() == SCE_TOOLKIT_NP_V2_SUCCESS;
		if (bGetFriendsSuccess && bFindSessionsSuccess)
		{
			bWasSuccessful = true;
			NumFriendsFound = GetFriendsResponse.get()->numFriends;
		}
		else
		{
			if (!bGetFriendsSuccess)
			{
				ErrorStr += FString::Printf(TEXT("Friend::getFriends aysnc failed : 0x%x"), GetFriendsResponse.getReturnCode());
			}
			if (!bFindSessionsSuccess)
			{
				ErrorStr += FString::Printf(TEXT(" Session::search aysnc failed : 0x%x"), FindFriendSessionsResponse.getReturnCode());
			}
		}	
	}
}

void FOnlineAsyncTaskPS4_ReadFriendsList::Finalize()
{
	if (bWasSuccessful)
	{
		TUniqueNetIdMap<FString> SessionIdsByFriendId;

		// Populate the map associating friends with their current session
		const NpToolkit::Session::Sessions& NpSessions = *FindFriendSessionsResponse.get();
		for (int32 SessionIdx = 0; SessionIdx < NpSessions.numSessions; ++SessionIdx)
		{
			const NpToolkit::Session::Session& NpSession = NpSessions.sessions[SessionIdx];
			const FString SessionId(ANSI_TO_TCHAR(NpSession.sessionId.data));
			for (int32 FriendMemberIdx = 0; FriendMemberIdx < NpSession.numUsers; ++FriendMemberIdx)
			{
				const NpToolkit::Session::SessionMember& FriendMember = NpSession.users[FriendMemberIdx];
				if (FriendMember.platform == NpToolkit::Session::CurrentPlatform::ps4)
				{
					SessionIdsByFriendId.Add(FUniqueNetIdPS4::FindOrCreate(FriendMember.onlineUser), SessionId);
				}
			}
		}

		StaticCastSharedPtr<FOnlineFriendsPS4>(Subsystem->GetFriendsInterface())->RebuildFriendsList(LocalUserId, ListName, *GetFriendsResponse.get(), SessionIdsByFriendId);
	}
	else
	{
		UE_LOG_ONLINE_FRIEND(Warning, TEXT("%s"), *ToString());
	}
}

void FOnlineAsyncTaskPS4_ReadFriendsList::TriggerDelegates()
{
	// Get our delegates to call
	FOnlineFriendsPS4Ptr FriendsInterfacePS4 = StaticCastSharedPtr<FOnlineFriendsPS4>(Subsystem->GetFriendsInterface());
	TMap<FString, TArray<FOnReadFriendsListComplete>>& ReadListCompleteCallbacksByName = FriendsInterfacePS4->AllReadListCallbacksByLocalId.FindOrAdd(LocalUserId);
	TArray<FOnReadFriendsListComplete> CompletionCallbacks = MoveTemp(ReadListCompleteCallbacksByName.FindOrAdd(ListName));

	// Ensure our delegates are reset
	ReadListCompleteCallbacksByName.FindChecked(ListName).Reset();

	for (const FOnReadFriendsListComplete& Callback : CompletionCallbacks)
	{
		Callback.ExecuteIfBound(LocalUserNum, bWasSuccessful, ListName, ErrorStr);
	}
	FriendsInterfacePS4->TriggerOnFriendsChangeDelegates(LocalUserNum);
}