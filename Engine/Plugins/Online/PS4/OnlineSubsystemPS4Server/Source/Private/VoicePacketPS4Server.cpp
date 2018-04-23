// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "VoicePacketPS4Server.h"
#include "Online.h"
#include "OnlineIdentityInterface.h"

uint16 FVoicePacketPS4Server::GetTotalPacketSize()
{
	return SenderName.Len() * sizeof(TCHAR) + sizeof(Length) + Length;
}

uint16 FVoicePacketPS4Server::GetBufferSize()
{
	return Length;
}

TSharedPtr<const FUniqueNetId> FVoicePacketPS4Server::GetSender()
{
	// If there is not a cached sender, create one from the string version, otherwise
	// return the cached version
	if (!Sender.IsValid() && !SenderName.IsEmpty())
	{
		Sender = MakeShared<FUniqueNetIdString>(SenderName);
	}
	return Sender;
}

void FVoicePacketPS4Server::Serialize(class FArchive& Ar)
{
	// Make sure not to overflow the buffer by reading an invalid amount
	if (Ar.IsLoading())
	{
		// Reading from the archive...
		// NOTE: Will read the string value of the sender's unique id. The actual
		// unique net id will only be created if asked for later
		Sender.Reset();
		Ar << SenderName;
		Ar << Length;

		// Verify the packet is a valid size
		if (Length <= MAX_VOICE_DATA_SIZE)
		{
			Buffer.Empty(Length);
			Buffer.AddUninitialized(Length);
			Ar.Serialize(Buffer.GetData(), Length);
		}
		else
		{
			Length = 0;
			Ar.SetError();
		}
	}
	else
	{
		check(!SenderName.IsEmpty());
		Ar << SenderName;
		Ar << Length;

		// Always safe to save the data as the voice code prevents overwrites
		Ar.Serialize(Buffer.GetData(), Length);
	}
}
