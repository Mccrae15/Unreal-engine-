// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "OnlineSubsystemPS4Types.h"

class FOnlineSubsystemPS4;

class FOnlineUserPresencePS4 : public FOnlineUserPresence
{
PACKAGE_SCOPE:	
	void SetSessionId(const FString& NewSessionId);
	void Update(const FOnlineSubsystemPS4& PS4Subsystem, const NpToolkit::Presence::Presence& NpPresence);

private:
	friend class FOnlinePresencePS4;
	FOnlineUserPresencePS4()
		: FOnlineUserPresence()
	{}
};

/** Implementation for the PS4 rich presence interface */
class FOnlinePresencePS4 : public IOnlinePresence
{
public:
	virtual void SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual void QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual EOnlineCachedResult::Type GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence) override;
	virtual EOnlineCachedResult::Type GetCachedPresenceForApp(const FUniqueNetId& LocalUserId, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence) override;

PACKAGE_SCOPE:
	explicit FOnlinePresencePS4(FOnlineSubsystemPS4& InPS4Subsystem) 
		: PS4Subsystem(InPS4Subsystem)
	{}

	TSharedPtr<FOnlineUserPresencePS4> FindUserPresence(FUniqueNetIdPS4Ref UserId) const;
	TSharedRef<FOnlineUserPresencePS4> FindOrCreatePresence(FUniqueNetIdPS4Ref UserId);

private:
	FOnlineSubsystemPS4& PS4Subsystem;

	/** All presence information we have */
	TMap<FString, TSharedRef<FOnlineUserPresencePS4>> CachedPresenceByUserId;
};

typedef TSharedPtr<FOnlinePresencePS4, ESPMode::ThreadSafe> FOnlinePresencePS4Ptr;

