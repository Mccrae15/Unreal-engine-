// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemPS4ServerPackage.h"

class FOnlineVoicePS4Server;

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineVoicePS4Server, ESPMode::ThreadSafe> FOnlineVoicePS4ServerPtr;

/**
 *	OnlineSubsystemPS4Server - Implementation of the online subsystem for dedicated servers serving PS4 clients
 */
class ONLINESUBSYSTEMPS4SERVER_API FOnlineSubsystemPS4Server : 
	public FOnlineSubsystemImpl
{

public:

	// IOnlineSubsystem

	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;	
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineStorePtr GetStoreInterface() const override;
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override { return nullptr; }
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override { return nullptr; }
	virtual IOnlineEventsPtr GetEventsInterface() const override;
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlineSharingPtr GetSharingInterface() const override;
	virtual IOnlineUserPtr GetUserInterface() const override;
	virtual IOnlineMessagePtr GetMessageInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override;
	virtual IOnlineChatPtr GetChatInterface() const override;
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override;

	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual FText GetOnlineServiceName() const override;

	// FTickerObjectBase
	
	virtual bool Tick(float DeltaTime) override;

PACKAGE_SCOPE:

	/** Only the factory makes instances */
	FOnlineSubsystemPS4Server(FName InInstanceName) :
		FOnlineSubsystemImpl(PS4SERVER_SUBSYSTEM, InInstanceName)
	{}

private:

	/** Interface for voice communication */
	FOnlineVoicePS4ServerPtr VoiceInterface;
};

typedef TSharedPtr<FOnlineSubsystemPS4Server, ESPMode::ThreadSafe> FOnlineSubsystemPS4ServerPtr;

