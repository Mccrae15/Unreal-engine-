// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPS4Server.h"
#include "OnlineVoiceInterfacePS4Server.h"

IOnlineSessionPtr FOnlineSubsystemPS4Server::GetSessionInterface() const
{
	return nullptr;
}

IOnlineFriendsPtr FOnlineSubsystemPS4Server::GetFriendsInterface() const
{
	return nullptr;
}

IOnlinePartyPtr FOnlineSubsystemPS4Server::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemPS4Server::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemPS4Server::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemPS4Server::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemPS4Server::GetEntitlementsInterface() const
{
	return nullptr;
};

IOnlineLeaderboardsPtr FOnlineSubsystemPS4Server::GetLeaderboardsInterface() const
{
	return nullptr;
}

IOnlineVoicePtr FOnlineSubsystemPS4Server::GetVoiceInterface() const
{
	return VoiceInterface;
}

IOnlineExternalUIPtr FOnlineSubsystemPS4Server::GetExternalUIInterface() const
{
	return nullptr;
}

IOnlineTimePtr FOnlineSubsystemPS4Server::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemPS4Server::GetIdentityInterface() const
{
	return nullptr;
}

IOnlineTitleFilePtr FOnlineSubsystemPS4Server::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineStorePtr FOnlineSubsystemPS4Server::GetStoreInterface() const
{
	return nullptr;
}

IOnlineEventsPtr FOnlineSubsystemPS4Server::GetEventsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemPS4Server::GetAchievementsInterface() const
{
	return nullptr;
}

IOnlineSharingPtr FOnlineSubsystemPS4Server::GetSharingInterface() const
{
	return nullptr;
}

IOnlineUserPtr FOnlineSubsystemPS4Server::GetUserInterface() const
{
	return nullptr;
}

IOnlineMessagePtr FOnlineSubsystemPS4Server::GetMessageInterface() const
{
	return nullptr;
}

IOnlinePresencePtr FOnlineSubsystemPS4Server::GetPresenceInterface() const
{
	return nullptr;
}

IOnlineChatPtr FOnlineSubsystemPS4Server::GetChatInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemPS4Server::GetTurnBasedInterface() const
{
	return nullptr;
}

bool FOnlineSubsystemPS4Server::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (VoiceInterface.IsValid())
	{
		VoiceInterface->Tick(DeltaTime);
	}

	return true;
}

bool FOnlineSubsystemPS4Server::Init()
{
	VoiceInterface = MakeShared<FOnlineVoicePS4Server, ESPMode::ThreadSafe>();
	if (!VoiceInterface->Init())
	{
		UE_LOG_ONLINE(Log, TEXT("FOnlineSubsystemPS4Server::Init: Voice interface disabled or failed to initialize."));
		VoiceInterface = nullptr;
	}

	return true;
}

bool FOnlineSubsystemPS4Server::Shutdown()
{
	UE_LOG_ONLINE(Display, TEXT("FOnlineSubsystemPS4Server::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();

#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		ensure(Interface.IsUnique()); \
		Interface = nullptr; \
	}

	// Destruct the interfaces
	DESTRUCT_INTERFACE(VoiceInterface);
	
#undef DESTRUCT_INTERFACE
	
	return true;
}

FString FOnlineSubsystemPS4Server::GetAppId() const
{
	return TEXT("");
}

FText FOnlineSubsystemPS4Server::GetOnlineServiceName() const
{
	const FText TrademarkText = FText::FromString(FString(TEXT("\u2122"))); /*TM*/
	return FText::Format(NSLOCTEXT("OnlineSubsystemPS4DedicatedServer", "OnlineServiceName", "PlayStation{0}Network"), TrademarkText);
}

