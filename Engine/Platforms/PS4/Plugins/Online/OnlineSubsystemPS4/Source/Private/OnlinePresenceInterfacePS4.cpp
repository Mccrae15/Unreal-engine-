// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlinePresenceInterfacePS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "Misc/App.h"

#include "OnlineAsyncTaskManagerPS4.h"
#include "AsyncTasks/OnlineAsyncItemsPS4_Presence.h"

//////////////////////////////////////////////////////////////////////////
// FOnlineUserPresencePS4
//////////////////////////////////////////////////////////////////////////

EOnlinePresenceState::Type NpOnlineStatusToPresenceState(NpToolkit::Presence::OnlineStatus NpOnlineStatus)
{
	if (NpOnlineStatus == NpToolkit::Presence::OnlineStatus::online)
	{
		return EOnlinePresenceState::Online;
	}
	else if (NpOnlineStatus == NpToolkit::Presence::OnlineStatus::standBy)
	{
		return EOnlinePresenceState::Away;
	}
	return EOnlinePresenceState::Offline;
}

void FOnlineUserPresencePS4::SetSessionId(const FString& NewSessionId)
{
	SessionId = MakeShared<FUniqueNetIdString>(NewSessionId, PS4_SUBSYSTEM);

	// *** TODO: how to tell if the session is joinable? (Should be baked into presence data when set?)
	bIsJoinable = bIsPlayingThisGame && SessionId.IsValid();
}

void FOnlineUserPresencePS4::Update(const FOnlineSubsystemPS4& PS4Subsystem, const NpToolkit::Presence::Presence& NpPresence)
{
	//@todo DanH: Yank this out of the game data on the presence if enabled
	//SessionId = MakeShared<FUniqueNetIdString>(SessionId, PS4_SUBSYSTEM);

	// Find the PS4 Platform presence data
	NpToolkit::Presence::PlatformPresence* PS4PlatformPresence = nullptr;
	for (int32 PlatformIdx = 0; PlatformIdx < NpPresence.numPlatforms; ++PlatformIdx)
	{
		if (NpPresence.platforms[PlatformIdx].platform == NpToolkit::Messaging::PlatformType::ps4)
		{
			PS4PlatformPresence = &NpPresence.platforms[PlatformIdx];
			break;
		}
	}

	if (PS4PlatformPresence)
	{
		Status.State = NpOnlineStatusToPresenceState(PS4PlatformPresence->onlineStatusOnPlatform);

		Status.StatusStr = FString(UTF8_TO_TCHAR(PS4PlatformPresence->gameStatus));
		Status.Properties.FindOrAdd(CustomPresenceDataKey) = FVariantData(FString(PS4PlatformPresence->binaryGameDataSize, UTF8_TO_TCHAR(PS4PlatformPresence->binaryGameData)));

		// This is super redundant, but was like this previously and is where other console OSS' put this info.
		// StatusStr is definitely the better place for this, but leaving this in for now as well.
		Status.Properties.FindOrAdd(DefaultPresenceKey) = FVariantData(Status.StatusStr);

		// Is our friend playing this game? (Presence status is only set for matching titles)
		const FString OurTitleId = PS4Subsystem.GetAppId();
		const bool bHasPresenceStatus = !Status.StatusStr.IsEmpty();
		const bool bIsSameTitleId = OurTitleId.Equals(ANSI_TO_TCHAR(PS4PlatformPresence->npTitleId.id));
		bIsPlayingThisGame = bHasPresenceStatus || bIsSameTitleId;
	}
	else
	{	
		Status.State = NpOnlineStatusToPresenceState(NpPresence.psnOnlineStatus);
		bIsPlayingThisGame = false;
	}

	if (Status.StatusStr.IsEmpty())
	{
		// No presence string found in the PS4 platform. Choose the first, non-null status we find on other platforms.
		for (int32 PlatformIdx = 0; PlatformIdx < NpPresence.numPlatforms; ++PlatformIdx)
		{
			if (NpPresence.platforms[PlatformIdx].gameStatus[0])
			{
				Status.StatusStr = FString(UTF8_TO_TCHAR(NpPresence.platforms[PlatformIdx].gameStatus));
				break;
			}
		}
	}

	bIsOnline = Status.State == EOnlinePresenceState::Online;

	// *** TODO: how to tell if the session is joinable? (Should be baked into presence data when set?)
	bIsJoinable = bIsPlayingThisGame && SessionId.IsValid();

	// *** TODO: How do we tell if the user is active or idle? (Should be baked into presence data when set?)
	bIsPlaying = true;

	// *** TODO: Presence information is not being written by the PS4 OSS yet so this bit of code is untested.
	// *** there is 128 bytes in NpFriend.presence.gameInfo.gameData what we can (un)serialize some Unreal-specific data into/from.
	// *** We'll need to agree on some kind of scheme for that and apply that here.
	bHasVoiceSupport = false;
	//Presence.Status.Properties; // *** TODO: Need a scheme for key/value pairs in/out of InOnlineNpFriend.presence.gameInfo.gameData

}

//////////////////////////////////////////////////////////////////////////
// FOnlinePresencePS4
//////////////////////////////////////////////////////////////////////////

void FOnlinePresencePS4::SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	FUniqueNetIdPS4Ref PS4UserId = FUniqueNetIdPS4::Cast(User.AsShared());
	PS4Subsystem.GetAsyncTaskManager()->AddToInQueue(new FOnlineAsyncTaskPS4_SetPresence(PS4Subsystem, PS4UserId, Status, Delegate));
}

void FOnlinePresencePS4::QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	// With the current signature, this seems to only support querying the presence of the local user...?
	FUniqueNetIdPS4Ref PS4UserId = FUniqueNetIdPS4::Cast(User.AsShared());
	PS4Subsystem.GetAsyncTaskManager()->AddToParallelTasks(new FOnlineAsyncTaskPS4_RefreshPresence(PS4Subsystem, PS4UserId, PS4UserId, Delegate));
}

EOnlineCachedResult::Type FOnlinePresencePS4::GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	if (TSharedRef<FOnlineUserPresencePS4>* FoundPresence = CachedPresenceByUserId.Find(User.ToString()))
	{
		OutPresence = *FoundPresence;
		return EOnlineCachedResult::Success;
	}

	return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FOnlinePresencePS4::GetCachedPresenceForApp(const FUniqueNetId& /*LocalUserId*/, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	EOnlineCachedResult::Type Result = EOnlineCachedResult::NotFound;

	if (PS4Subsystem.GetAppId() == AppId)
	{
		Result = GetCachedPresence(User, OutPresence);
	}

	return Result;
}

TSharedPtr<FOnlineUserPresencePS4> FOnlinePresencePS4::FindUserPresence(FUniqueNetIdPS4Ref UserId) const
{
	if (const TSharedRef<FOnlineUserPresencePS4>* UserPresence = CachedPresenceByUserId.Find(UserId->ToString()))
	{
		return *UserPresence;
	}
	return TSharedPtr<FOnlineUserPresencePS4>();
}

TSharedRef<FOnlineUserPresencePS4> FOnlinePresencePS4::FindOrCreatePresence(FUniqueNetIdPS4Ref UserId)
{
	TSharedRef<FOnlineUserPresencePS4>* UserPresence = CachedPresenceByUserId.Find(UserId->ToString());
	if (!UserPresence)
	{
		UserPresence = &CachedPresenceByUserId.Add(UserId->ToString(), MakeShareable(new FOnlineUserPresencePS4));
	}
	return *UserPresence;
}