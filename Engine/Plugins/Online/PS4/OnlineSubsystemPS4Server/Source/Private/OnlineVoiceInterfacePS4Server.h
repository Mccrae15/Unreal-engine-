// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VoiceInterface.h"
#include "OnlineSubsystemPS4ServerPackage.h"

class FOnlineSubsystemPS4Server;

/**
 * Mostly stubbed implementation of the voice interface for dedicated servers
 * that can receive and forward PS4 voice packets to other clients.
 */
class FOnlineVoicePS4Server : public IOnlineVoice
{

PACKAGE_SCOPE:

	virtual void ProcessMuteChangeNotification() override;
	virtual bool Init() override;

public:

	virtual void StartNetworkedVoice(uint8 LocalUserNum) override;
	virtual void StopNetworkedVoice(uint8 LocalUserNum) override;
	virtual bool RegisterLocalTalker(uint32 LocalUserNum) override;
	virtual void RegisterLocalTalkers() override;
	virtual bool UnregisterLocalTalker(uint32 LocalUserNum) override;
	virtual void UnregisterLocalTalkers() override;
	virtual bool RegisterRemoteTalker(const FUniqueNetId& UniqueId) override;
	virtual bool UnregisterRemoteTalker(const FUniqueNetId& UniqueId) override;
	virtual void RemoveAllRemoteTalkers() override;
	virtual bool IsHeadsetPresent(uint32 LocalUserNum) override;
	virtual bool IsLocalPlayerTalking(uint32 LocalUserNum) override;
	virtual bool IsRemotePlayerTalking(const FUniqueNetId& UniqueId) override;
	bool IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const override;
	bool MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	bool UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual TSharedPtr<class FVoicePacket> SerializeRemotePacket(FArchive& Ar) override;
	virtual TSharedPtr<class FVoicePacket> GetLocalPacket(uint32 LocalUserNum) override;
	virtual int32 GetNumLocalTalkers() override;
	virtual void ClearVoicePackets() override;
	virtual void Tick(float DeltaTime) override;
	virtual FString GetVoiceDebugState() const override;
};

typedef TSharedPtr<FOnlineVoicePS4Server, ESPMode::ThreadSafe> FOnlineVoicePS4ServerPtr;
