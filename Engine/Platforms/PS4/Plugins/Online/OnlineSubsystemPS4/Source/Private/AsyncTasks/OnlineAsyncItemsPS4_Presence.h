// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemPS4.h"
#include "OnlinePresenceInterfacePS4.h"

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaslPS4_SetPresence
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_SetPresence : public FOnlineAsyncTaskPS4
{
public:
	FOnlineAsyncTaskPS4_SetPresence(FOnlineSubsystemPS4& PS4Subsystem, FUniqueNetIdPS4Ref InLocalUserId, const FOnlineUserPresenceStatus& Status, const IOnlinePresence::FOnPresenceTaskCompleteDelegate& InCompletionDelegate);

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskPS4_SetPresence"); }

	virtual void Tick() override;
	virtual void TriggerDelegates() override;

private:
	FUniqueNetIdPS4Ref LocalUserId;
	IOnlinePresence::FOnPresenceTaskCompleteDelegate CompletionDelegate;
	NpToolkit::Core::Response<NpToolkit::Core::Empty> SetPresenceResponse;
};


//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_RefreshPresence
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_RefreshPresence : public FOnlineAsyncTaskPS4
{
public:
	FOnlineAsyncTaskPS4_RefreshPresence(FOnlineSubsystemPS4& PS4Subsystem, FUniqueNetIdPS4Ref InLocalUserId, FUniqueNetIdPS4Ref InRemoteUserId, const IOnlinePresence::FOnPresenceTaskCompleteDelegate& InCompletionDelegate = IOnlinePresence::FOnPresenceTaskCompleteDelegate())
		: FOnlineAsyncTaskPS4(&PS4Subsystem)
		, LocalUserId(InLocalUserId)
		, RemoteUserId(InRemoteUserId)
		, CompletionDelegate(InCompletionDelegate)
	{}

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskPS4_RefreshPresence"); }

	virtual void Initialize() override;
	virtual void Tick() override;
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	FUniqueNetIdPS4Ref LocalUserId;
	FUniqueNetIdPS4Ref RemoteUserId;
	IOnlinePresence::FOnPresenceTaskCompleteDelegate CompletionDelegate;
	NpToolkit::Core::Response<NpToolkit::Presence::Presence> PresenceResponse;
};

//////////////////////////////////////////////////////////////////////////
// FOnlineEventPS4_PresenceStatusChanged
//////////////////////////////////////////////////////////////////////////

class FOnlineEventPS4_PresenceStatusChanged : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FOnlineEventPS4_PresenceStatusChanged(FOnlineSubsystemPS4& PS4Subsystem, FUniqueNetIdPS4Ref InUpdatedUserId, const FString& InNewStatus)
		: FOnlineAsyncEvent(&PS4Subsystem)
		, UpdatedUserId(InUpdatedUserId)
		, NewStatus(InNewStatus)
	{}

	virtual FString ToString() const override { return TEXT("FOnlineEventPS4_PresenceStatusChanged"); }

	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	FUniqueNetIdPS4Ref UpdatedUserId;
	FString NewStatus;
};

//////////////////////////////////////////////////////////////////////////
// FOnlineEventPS4_PresenceGameDataChanged
//////////////////////////////////////////////////////////////////////////

class FOnlineEventPS4_PresenceGameDataChanged : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FOnlineEventPS4_PresenceGameDataChanged(FOnlineSubsystemPS4& PS4Subsystem, FUniqueNetIdPS4Ref InUpdatedUserId, const FString& InUpdatedGameData)
		: FOnlineAsyncEvent(&PS4Subsystem)
		, UpdatedUserId(InUpdatedUserId)
		, NewGameData(InUpdatedGameData)
	{}

	virtual FString ToString() const override { return TEXT("FOnlineEventPS4_PresenceGameDataChanged"); }

	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	FUniqueNetIdPS4Ref UpdatedUserId;
	FString NewGameData;
};
