// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineVoiceInterfacePS4Server.h"
#include "VoicePacketPS4Server.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Engine.h"

bool FOnlineVoicePS4Server::Init()
{
	bool bHasVoiceEnabled = false;
	if (GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bHasVoiceEnabled"), bHasVoiceEnabled, GEngineIni) && bHasVoiceEnabled)
	{
		return true;
	}

	return false;
}

void FOnlineVoicePS4Server::ClearVoicePackets()
{
	// Nothing to do for dedicated servers.
}

void FOnlineVoicePS4Server::Tick(float DeltaTime)
{
	// Nothing to do for dedicated servers.
}

void FOnlineVoicePS4Server::StartNetworkedVoice(uint8 LocalUserNum)
{
	// Nothing to do for dedicated servers.
}

void FOnlineVoicePS4Server::StopNetworkedVoice(uint8 LocalUserNum)
{
	// Nothing to do for dedicated servers.
}

bool FOnlineVoicePS4Server::RegisterLocalTalker(uint32 LocalUserNum)
{
	// Nothing to do for dedicated servers.
	return true;
}

void FOnlineVoicePS4Server::RegisterLocalTalkers()
{
	// Nothing to do for dedicated servers.
}

bool FOnlineVoicePS4Server::UnregisterLocalTalker(uint32 LocalUserNum)
{
	// Nothing to do for dedicated servers.
	return true;
}

void FOnlineVoicePS4Server::UnregisterLocalTalkers()
{
	// Nothing to do for dedicated servers.
}

bool FOnlineVoicePS4Server::RegisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	// Unimplemented for dedicated servers.
	return true;
}

bool FOnlineVoicePS4Server::UnregisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	// Unimplemented for dedicated servers.
	return true;
}

void FOnlineVoicePS4Server::RemoveAllRemoteTalkers()
{
	// Unimplemented for dedicated servers.
}

bool FOnlineVoicePS4Server::IsHeadsetPresent(uint32 LocalUserNum)
{
	// Nothing to do for dedicated servers.
	return false;
}

bool FOnlineVoicePS4Server::IsLocalPlayerTalking(uint32 LocalUserNum)
{
	// Nothing to do for dedicated servers.
	return false;
}

bool FOnlineVoicePS4Server::IsRemotePlayerTalking(const FUniqueNetId& UniqueId)
{
	// Unimplemented for dedicated servers.
	return false;
}

bool FOnlineVoicePS4Server::IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const
{
	// Unimplemented for dedicated servers.
	return false;
}

bool FOnlineVoicePS4Server::MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	// Unimplemented for dedicated servers.
	return true;
}

bool FOnlineVoicePS4Server::UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	// Unimplemented for dedicated servers.
	return true;
}

void FOnlineVoicePS4Server::ProcessMuteChangeNotification()
{
	// Nothing to do for dedicated servers.
}

TSharedPtr<FVoicePacket> FOnlineVoicePS4Server::SerializeRemotePacket(FArchive& Ar)
{
	TSharedPtr<FVoicePacketPS4Server> NewPacket = MakeShared<FVoicePacketPS4Server>();
	NewPacket->Serialize(Ar);

	if (Ar.IsError() == false && NewPacket->GetBufferSize() > 0)
	{
		return NewPacket;
	}

	return nullptr;
}

TSharedPtr<FVoicePacket> FOnlineVoicePS4Server::GetLocalPacket(uint32 LocalUserNum)
{
	// Nothing to do for dedicated servers.
	return nullptr;
}

FString FOnlineVoicePS4Server::GetVoiceDebugState() const
{
	return FString(TEXT("No voice state to report for dedicated server."));
}

int32 FOnlineVoicePS4Server::GetNumLocalTalkers()
{
	// Dedicated servers can't have local talkers.
	return 0;
}
