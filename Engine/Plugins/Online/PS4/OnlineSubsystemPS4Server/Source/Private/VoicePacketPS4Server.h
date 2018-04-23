// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VoiceDataCommon.h"
#include "OnlineSubsystemPS4ServerPackage.h"

class FUniqueNetId;

/** Defines the data involved in a PS4 voice packet. Serialization must be compatible with FVoicePacketPS4! */
class FVoicePacketPS4Server : public FVoicePacket
{
private:
	/** The unique net id of the talker sending the data. Used to route the packet */
	TSharedPtr<const FUniqueNetId> Sender;

	/** The string-based name of the sender, what is actually sent across the wire */
	FString SenderName;

PACKAGE_SCOPE:

	/** The current amount of space used in the buffer for this packet */
	uint16 Length;

	/** The data that is to be sent/processed */
	TArray<uint8> Buffer;

public:
	FVoicePacketPS4Server() :
		Sender(nullptr),
		Length(0)
	{
		// NOTE: Fixed size buffers here to help prevent fragmentation
		Buffer.Empty(MAX_VOICE_DATA_SIZE);
		Buffer.AddUninitialized(MAX_VOICE_DATA_SIZE);
	}

	virtual uint16 GetTotalPacketSize() override;
	virtual uint16 GetBufferSize() override;
	virtual TSharedPtr<const FUniqueNetId> GetSender() override;
	virtual bool IsReliable() override { return false; }
	virtual void Serialize(class FArchive& Ar) override;
};
