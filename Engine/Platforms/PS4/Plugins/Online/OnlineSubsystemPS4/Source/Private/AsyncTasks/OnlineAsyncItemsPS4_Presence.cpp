// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncItemsPS4_Presence.h"

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_SetPresence
//////////////////////////////////////////////////////////////////////////

FOnlineAsyncTaskPS4_SetPresence::FOnlineAsyncTaskPS4_SetPresence(FOnlineSubsystemPS4& PS4Subsystem, FUniqueNetIdPS4Ref InLocalUserId, const FOnlineUserPresenceStatus& PresenceStatus, const IOnlinePresence::FOnPresenceTaskCompleteDelegate& InCompletionDelegate)
	: FOnlineAsyncTaskPS4(&PS4Subsystem)
	, LocalUserId(InLocalUserId)
	, CompletionDelegate(InCompletionDelegate)
{
	const FString GameStatusStr = PresenceStatus.StatusStr.Left(NpToolkit::Presence::Request::SetPresence::MAX_SIZE_DEFAULT_GAME_STATUS);
	
	FString GameDataStr;
	const FVariantData* GameDataProperty = PresenceStatus.Properties.Find(CustomPresenceDataKey);
	if (GameDataProperty && GameDataProperty->GetType() == EOnlineKeyValuePairDataType::String)
	{
		GameDataProperty->GetValue(GameDataStr);
		GameDataStr = GameDataStr.Left(NpToolkit::Presence::Request::SetPresence::MAX_SIZE_GAME_DATA);
	}

	NpToolkit::Presence::Request::SetPresence SetPresenceRequest;
	SetPresenceRequest.userId = InLocalUserId->GetUserId();
	FPlatformString::Strcpy(SetPresenceRequest.defaultGameStatus, NpToolkit::Presence::Request::SetPresence::MAX_SIZE_DEFAULT_GAME_STATUS, TCHAR_TO_UTF8(*GameStatusStr));
	FPlatformString::Strcpy(SetPresenceRequest.binaryGameData, NpToolkit::Presence::Request::SetPresence::MAX_SIZE_GAME_DATA, TCHAR_TO_UTF8(*GameDataStr));
	SetPresenceRequest.binaryGameDataSize = GameDataStr.Len() + 1;
	NpToolkit::Presence::setPresence(SetPresenceRequest, &SetPresenceResponse);

	// Update the cached presence as well
	TSharedRef<FOnlineUserPresencePS4> LocalCachedPresence = StaticCastSharedPtr<FOnlinePresencePS4>(Subsystem->GetPresenceInterface())->FindOrCreatePresence(LocalUserId);
	LocalCachedPresence->Status = PresenceStatus;
	Subsystem->GetPresenceInterface()->TriggerOnPresenceReceivedDelegates(*LocalUserId, LocalCachedPresence);
}

void FOnlineAsyncTaskPS4_SetPresence::Tick()
{
	if (!SetPresenceResponse.isLocked())
	{
		bIsComplete = true;

		const int32 ReturnCode = SetPresenceResponse.getReturnCode();
		bWasSuccessful = ReturnCode == SCE_TOOLKIT_NP_V2_SUCCESS;
		UE_CLOG_ONLINE_PRESENCE(!bWasSuccessful, Warning, TEXT("%s failed with result 0x%x"), *ToString(), ReturnCode);
	}
}

void FOnlineAsyncTaskPS4_SetPresence::TriggerDelegates()
{
	CompletionDelegate.ExecuteIfBound(*LocalUserId, bWasSuccessful);
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_RefreshPresence
//////////////////////////////////////////////////////////////////////////

void FOnlineAsyncTaskPS4_RefreshPresence::Initialize()
{
	NpToolkit::Presence::Request::GetPresence GetPresenceRequest;
	GetPresenceRequest.userId = LocalUserId->GetUserId();
	GetPresenceRequest.fromUser = RemoteUserId->GetAccountId();
	NpToolkit::Presence::getPresence(GetPresenceRequest, &PresenceResponse);
}

void FOnlineAsyncTaskPS4_RefreshPresence::Tick()
{
	if (PresenceResponse.getState() == NpToolkit::Core::ResponseState::ready)
	{
		bIsComplete = true;

		const int32 ReturnCode = PresenceResponse.getReturnCode();
		bWasSuccessful = ReturnCode == SCE_TOOLKIT_NP_V2_SUCCESS;
		UE_CLOG_ONLINE_PRESENCE(!bWasSuccessful, Warning, TEXT("%s failed with result 0x%x"), *ToString(), ReturnCode);
	}
}

void FOnlineAsyncTaskPS4_RefreshPresence::Finalize()
{
	if (bWasSuccessful)
	{
		const NpToolkit::Presence::Presence& NpPresence = *PresenceResponse.get();
		TSharedRef<FOnlineUserPresencePS4> UserPresence = StaticCastSharedPtr<FOnlinePresencePS4>(Subsystem->GetPresenceInterface())->FindOrCreatePresence(RemoteUserId);
		UserPresence->Update(*Subsystem, NpPresence);
	}
}

void FOnlineAsyncTaskPS4_RefreshPresence::TriggerDelegates()
{
	CompletionDelegate.ExecuteIfBound(*RemoteUserId, bWasSuccessful);
	if (bWasSuccessful)
	{
		if (TSharedPtr<FOnlineUserPresencePS4> UserPresence = StaticCastSharedPtr<FOnlinePresencePS4>(Subsystem->GetPresenceInterface())->FindUserPresence(RemoteUserId))
		{
			Subsystem->GetPresenceInterface()->TriggerOnPresenceReceivedDelegates(*RemoteUserId, UserPresence.ToSharedRef());
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// FOnlineEventPS4_PresenceStatusChanged 
//////////////////////////////////////////////////////////////////////////

void FOnlineEventPS4_PresenceStatusChanged::Finalize()
{
	if (TSharedPtr<FOnlineUserPresencePS4> UserPresence = StaticCastSharedPtr<FOnlinePresencePS4>(Subsystem->GetPresenceInterface())->FindUserPresence(UpdatedUserId))
	{
		UserPresence->Status.StatusStr = NewStatus;
	}
}

void FOnlineEventPS4_PresenceStatusChanged::TriggerDelegates()
{
	if (TSharedPtr<FOnlineUserPresencePS4> UserPresence = StaticCastSharedPtr<FOnlinePresencePS4>(Subsystem->GetPresenceInterface())->FindUserPresence(UpdatedUserId))
	{
		Subsystem->GetPresenceInterface()->TriggerOnPresenceReceivedDelegates(*UpdatedUserId, UserPresence.ToSharedRef());
	}
}

//////////////////////////////////////////////////////////////////////////
// FOnlineEventPS4_PresenceGameDataChanged
//////////////////////////////////////////////////////////////////////////

void FOnlineEventPS4_PresenceGameDataChanged::Finalize()
{
	if (TSharedPtr<FOnlineUserPresencePS4> UserPresence = StaticCastSharedPtr<FOnlinePresencePS4>(Subsystem->GetPresenceInterface())->FindUserPresence(UpdatedUserId))
	{
		UserPresence->Status.Properties.FindOrAdd(CustomPresenceDataKey).SetValue(NewGameData);
	}
}

void FOnlineEventPS4_PresenceGameDataChanged::TriggerDelegates()
{
	if (TSharedPtr<FOnlineUserPresencePS4> UserPresence = StaticCastSharedPtr<FOnlinePresencePS4>(Subsystem->GetPresenceInterface())->FindUserPresence(UpdatedUserId))
	{
		Subsystem->GetPresenceInterface()->TriggerOnPresenceReceivedDelegates(*UpdatedUserId, UserPresence.ToSharedRef());
	}
}