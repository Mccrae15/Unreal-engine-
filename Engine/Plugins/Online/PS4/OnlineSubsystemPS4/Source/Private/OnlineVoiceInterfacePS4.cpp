// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineVoiceInterfacePS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include <audioin.h>
#include <audioout.h>

// Class for enforcing a quota on remote endpoints
class RemoteEndpointManager
{
private:
	int Count;
	int MaxCount;

public:
	explicit RemoteEndpointManager(int InMaxCount);
	~RemoteEndpointManager();

	SceVoiceQoSRemoteId AllocateRemoteEndpoint();
	void ReleaseRemoteEndpoint(SceVoiceQoSRemoteId RemoteEndpoint);

	int GetCount() { return Count; }
};

// NOTE: The PS4 can only have 8 voice codecs working, which means a limit of 7 remote endpoints,
// since one codec is used by the local endpoints
#define MAX_REMOTE_ENDPOINTS 7
static RemoteEndpointManager RemoteEndpointPool(MAX_REMOTE_ENDPOINTS);

RemoteEndpointManager::RemoteEndpointManager(int InMaxCount) :
	Count(0),
	MaxCount(InMaxCount)
{
}

RemoteEndpointManager::~RemoteEndpointManager()
{
}

SceVoiceQoSRemoteId RemoteEndpointManager::AllocateRemoteEndpoint()
{
	SceVoiceQoSRemoteId RemoteEndpoint = SCE_VOICE_QOS_INVALID_REMOTE_ID;
	if (Count + 1 <= MaxCount)
	{
		SceVoiceQoSError Result = sceVoiceQoSCreateRemoteEndpoint(&RemoteEndpoint);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("sceVoiceQoSCreateRemoteEndpoint() failed. Error code: %#x"), Result);
			RemoteEndpoint = SCE_VOICE_QOS_INVALID_REMOTE_ID;
		}
		else
		{
			++Count;
		}
	}
	return RemoteEndpoint;
}

void RemoteEndpointManager::ReleaseRemoteEndpoint(SceVoiceQoSRemoteId RemoteEndpoint)
{
	if (Count > 0 && RemoteEndpoint != SCE_VOICE_QOS_INVALID_REMOTE_ID)
	{
		SceVoiceQoSError Result = sceVoiceQoSDeleteRemoteEndpoint(RemoteEndpoint);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("sceVoiceQoSDeleteRemoteEndpoint() failed. Error code: %#x"), Result);
		}
		else
		{
			--Count;
		}
	}
}

//
// LocalTalkerPS4
//
int LocalTalkerPS4::MaxDeadAir = 30; // max number of frames to keep a connection open without a remote talker

LocalTalkerPS4::LocalTalkerPS4(uint32 InLocalUserNum, SceUserServiceUserId InUserId) :
	LocalUserNum(InLocalUserNum),
	UserId(InUserId),
	LocalEndpoint(SCE_VOICE_QOS_INVALID_LOCAL_ID),
	RemoteEndpoint(SCE_VOICE_QOS_INVALID_REMOTE_ID),
	LocalConnection(NULL)
{
	bWritingVoicePackets = false;

	// Create a new local endpoint for this user, using the virtual devices for 
	// chat input and voice output.
	// @todo: determine if the virtual devices are acceptable for hardwiring
	// @todo: expose the input and output port types via API or config file
	int32_t OutputPort = SCE_AUDIO_OUT_PORT_TYPE_MAIN; // SCE_AUDIO_OUT_PORT_TYPE_VOICE doesn't work for some reason
	SceVoiceQoSError Result = sceVoiceQoSCreateLocalEndpoint(&LocalEndpoint, UserId, SCE_AUDIO_IN_TYPE_VOICE_CHAT, OutputPort);
	if (Result != SCE_OK)
	{
		UE_LOG_ONLINE(Error, TEXT("sceVoiceQoSCreateLocalEndpoint() failed for SceUserServiceUserId %d. Error code: %#x"), UserId, Result);
		LocalEndpoint = SCE_VOICE_QOS_INVALID_LOCAL_ID;
	}

	// Create a new remote endpoint and use it to create a connection
	RemoteEndpoint = RemoteEndpointPool.AllocateRemoteEndpoint();
	if (RemoteEndpoint == SCE_VOICE_QOS_INVALID_REMOTE_ID)
	{
		// No connection will be created in this case, so the rest of the code needs to check
	}
	else
	{
		LocalConnection = MakeShareable(new VoiceConnectionPS4(LocalEndpoint, RemoteEndpoint));
	}

	// Determine the sender id to use for this local talker
	IOnlineSubsystem* OnlineSubSystem = IOnlineSubsystem::Get(PS4_SUBSYSTEM);
	IOnlineIdentityPtr IdentityInt = OnlineSubSystem->GetIdentityInterface();
	if (IdentityInt.IsValid())
	{
		SenderId = IdentityInt->GetUniquePlayerId(LocalUserNum);
	}
}

LocalTalkerPS4::~LocalTalkerPS4()
{
	// Free the local connection if any
	if (LocalConnection.IsValid())
	{
		// Make sure we aren't attached to a remote talker any longer
		LocalConnection->DetachRemoteTalker();

		// Forget about our local connection. If we are the last reference, then
		// the connection will be closed and deleted
		LocalConnection.Reset();
	}

	// Release the remote endpoint if it is valid
	if (RemoteEndpoint != SCE_VOICE_QOS_INVALID_REMOTE_ID)
	{
		RemoteEndpointPool.ReleaseRemoteEndpoint(RemoteEndpoint);
		RemoteEndpoint = SCE_VOICE_QOS_INVALID_REMOTE_ID;
	}

	// Delete the local endpoint if it is valid
	if (LocalEndpoint != SCE_VOICE_QOS_INVALID_LOCAL_ID)
	{
		SceVoiceQoSError Result = sceVoiceQoSDeleteLocalEndpoint(LocalEndpoint);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("sceVoiceQoSDeleteLocalEndpoint() failed. Error code: %#x"), Result);
		}
		LocalEndpoint = SCE_VOICE_QOS_INVALID_LOCAL_ID;
	}
}

void LocalTalkerPS4::UpdateConnections()
{
	if (!LocalConnection.IsValid())
	{
		return;
	}

	// Active connection are connections associated with remote talkers
	// When their dead air count hits the max, i.e. when they have been silent for that many frames,
	// they are removed from the list and detached from the remote talker
	int Index = 0;
	while (Index < ActiveConnections.Num())
	{
		if (ActiveConnections[Index]->AddDeadAir() > MaxDeadAir)
		{
			// The connection needs to be closed, so remove it from the list
			// of active connections and close it out.

			// Hold a local reference after removal
			VoiceConnectionPS4Ptr Connection = ActiveConnections[Index];
			ActiveConnections.RemoveAt(Index);

			// Detach the remote talker from it, if any
			Connection->DetachRemoteTalker();

			// If this connection is not the local connection, release its remote endpoint
			if (Connection != LocalConnection)
			{
				RemoteEndpointPool.ReleaseRemoteEndpoint(Connection->RemoteEndpoint);
			}

			// At this point, if the connection is not the local connection, it will be destroyed as
			// it falls out of scope. If it is the local connection, it will continue to exist.

			// Continue at the same index since that now represents the
			// connection beyond this one
		}
		else
		{
			// Advance to the next connection
			++Index;
		}
	}
}

void LocalTalkerPS4::WriteVoicePacket(FVoicePacketPS4 *VoicePacket)
{
	// don't say we're writing packets if the packet we're sending is a keep-alive packet
	if (VoicePacket->Length > 1)
	{
		bWritingVoicePackets = true;
	}

	// Check the sender against the mute list for this talker. It muted, skip the talker
	// NOTE: Checking based on the string version of the unique net id is sufficient
	if (IsMuted(VoicePacket->GetSenderName()))
	{
		return;
	}

	// Try to find an active connection attached to the sender of the packet
	for (int Index = 0; Index < ActiveConnections.Num(); ++Index)
	{
		VoiceConnectionPS4Ptr Connection = ActiveConnections[Index];
		if (Connection->GetAttachedRemoteTalker().IsValid())
		{
			if (Connection->GetAttachedRemoteTalker()->GetUserName().Equals(VoicePacket->GetSenderName()))
			{
				// We are attached to the packet sender, so write the packet to the connection
				Connection->WriteVoicePacket(VoicePacket);
				return;
			}
		}
	}

	// There is not an active connection for this remote talker, so attempt to create one.

	// Find the remote talker associated with this sender by asking the OSS:IOnlineVoice implementation
	// NOTE: The request is done using the string version of the network id
	IOnlineSubsystem* OnlineSubSystem = IOnlineSubsystem::Get(PS4_SUBSYSTEM);
	FOnlineVoicePS4 *OnlineVoice = static_cast<FOnlineVoicePS4 *>(OnlineSubSystem->GetVoiceInterface().Get());
	RemoteTalkerPS4Ptr TheRemoteTalker = OnlineVoice->FindRemoteTalker(VoicePacket->GetSenderName());
	if (!TheRemoteTalker.IsValid())
	{
		// This is not a registered remote talker, so don't bother creating a connection.
		return;
	}

	// We know the talker, so create the connection.
	// If the local connection is not already attached to a remote user, use it instead of
	// creating a new one.
	if (!LocalConnection->RemoteTalker.IsValid())
	{
		ActiveConnections.Add(LocalConnection);
		LocalConnection->AttachRemoteTalker(TheRemoteTalker);
		LocalConnection->WriteVoicePacket(VoicePacket);
	}
	else
	{
		// The local connection is in use, so see if there are any remote endpoints left.
		// If there is a remote endpoint available, create a connection using it
		SceVoiceQoSRemoteId NewRemoteEndpoint = RemoteEndpointPool.AllocateRemoteEndpoint();
		if (NewRemoteEndpoint == SCE_VOICE_QOS_INVALID_REMOTE_ID)
		{
			// We are out of endpoints, so drop the packet
			UE_LOG_ONLINE(Verbose, TEXT("Dropped packet from %s! No free endpoints."), *VoicePacket->GetSenderName());
		}
		else
		{
			VoiceConnectionPS4Ptr Connection = MakeShareable(new VoiceConnectionPS4(LocalEndpoint, NewRemoteEndpoint));
			ActiveConnections.Add(Connection);
			Connection->AttachRemoteTalker(TheRemoteTalker);
			Connection->WriteVoicePacket(VoicePacket);
		}
	}
	bWritingVoicePackets = false;
}

// This method is responsible for fetching voice packets from the voice input for this local talker
TSharedPtr<class FVoicePacket> LocalTalkerPS4::ReadVoicePacket()
{
	if (!LocalConnection.IsValid())
	{
		return NULL;
	}

	// Set this local talker to not speaking until proven otherwise
	bIsSpeaking = false;

	// If the mic is muted, don't try to read
	bool bIsMuted = false;
	SceVoiceQoSError Result = sceVoiceQoSGetLocalEndpointAttribute(LocalEndpoint, SCE_VOICE_QOS_ATTR_MIC_MUTE, &bIsMuted, sizeof(bIsMuted));
	if (Result != SCE_OK || bIsMuted)
	{
		// Could not query the mute state, or we are muted. Either way, skip
		// the attempt to read packet data
		return NULL;
	}

	// NOTE: large stack allocation here, 8K at this time
	uint8 VoiceBuffer[MAX_VOICE_DATA_SIZE];
	unsigned int VoiceBufferLength = sizeof(VoiceBuffer);

	// Attempt to read from the local connection
	Result = sceVoiceQoSReadPacket(LocalConnection->ConnectId, &VoiceBuffer, &VoiceBufferLength);
	if (Result != SCE_OK)
	{
		UE_LOG_ONLINE(Log, TEXT("sceVoiceQoSReadPacket() failed for connection id %d. Error code %#x"), LocalConnection->ConnectId, Result);
		return NULL;
	}
	
	// There were no errors, but that doesn't mean we read any data.
	// If no data, then no packet
	if (VoiceBufferLength <= 0)
	{
		return NULL;
	}

	// We've got data, so build a packet out of it and set the speaking flag (length == 1, is sent even when mics are muted/no talking)
	if (VoiceBufferLength > 1)
	{
		bIsSpeaking = true;
	}

	// Fill out and return the packet
	FVoicePacketPS4 *NewPacket = new FVoicePacketPS4();
	FMemory::Memcpy(NewPacket->Buffer.GetData(), VoiceBuffer, VoiceBufferLength);
	NewPacket->Length = VoiceBufferLength;
	NewPacket->SetSender(SenderId);

	return MakeShareable(NewPacket);
}

bool LocalTalkerPS4::IsMuted(const FUniqueNetId& RemotePlayerId) const
{
	return IsMuted(RemotePlayerId.ToString());
}

bool LocalTalkerPS4::IsMuted(const FString &RemotePlayerName) const
{
	return MutedRemotes.Contains(RemotePlayerName);
}

bool LocalTalkerPS4::IsRemoteTalking(const FUniqueNetId& RemotePlayerId) const
{
	return IsRemoteTalking(RemotePlayerId.ToString());
}

bool LocalTalkerPS4::IsRemoteTalking(const FString &RemotePlayerName) const
{
	if (!LocalConnection.IsValid())
	{
		return false;
	}

	// Walk the active connections, looking for one associated with this remote player id
	// NOTE: Using the string version of the unique net id here
	for (int Index = 0; Index < ActiveConnections.Num(); ++Index)
	{
		VoiceConnectionPS4Ptr Connection = ActiveConnections[Index];
		if (Connection->RemoteTalker->GetUserName().Equals(RemotePlayerName))
		{
			return true;
		}
	}
	return false;
}

void LocalTalkerPS4::MuteRemotePlayer(const FUniqueNetId& RemotePlayerId)
{
	MuteRemotePlayer(RemotePlayerId.ToString());
}

void LocalTalkerPS4::MuteRemotePlayer(const FString& RemotePlayerName)
{
	MutedRemotes.AddUnique(RemotePlayerName);
}

void LocalTalkerPS4::UnmuteRemotePlayer(const FUniqueNetId& RemotePlayerId)
{
	UnmuteRemotePlayer(RemotePlayerId.ToString());
}

void LocalTalkerPS4::UnmuteRemotePlayer(const FString& RemotePlayerName)
{
	MutedRemotes.Remove(RemotePlayerName);
}

bool LocalTalkerPS4::IsMicrophoneUsable()
{
	bool bIsUsable = false;
	SceVoiceQoSError Result = sceVoiceQoSGetLocalEndpointAttribute(LocalEndpoint, SCE_VOICE_QOS_ATTR_MIC_USABLE, &bIsUsable, sizeof(bIsUsable));
	if (Result != SCE_OK)
	{
		bIsUsable = false;
	}
	return bIsUsable;
}

void LocalTalkerPS4::StartNetworkedVoice()
{
	bool bIsMuted = false;
	SceVoiceQoSError Result = sceVoiceQoSSetLocalEndpointAttribute(LocalEndpoint, SCE_VOICE_QOS_ATTR_MIC_MUTE, &bIsMuted, sizeof(bIsMuted));
	if (Result != SCE_OK)
	{
		UE_LOG_ONLINE(Log, TEXT("sceVoiceQoSSetLocalEndpointAttribute() failed for connection id %d. Error code %#x"), LocalConnection->ConnectId, Result);
	}
}

void LocalTalkerPS4::StopNetworkedVoice()
{
	bool bIsMuted = true;
	
	SceVoiceQoSError Result = sceVoiceQoSSetLocalEndpointAttribute(LocalEndpoint, SCE_VOICE_QOS_ATTR_MIC_MUTE, &bIsMuted, sizeof(bIsMuted));
	if (Result != SCE_OK)
	{
		UE_LOG_ONLINE(Log, TEXT("sceVoiceQoSSetLocalEndpointAttribute() failed for connection id %d. Error code %#x"), LocalConnection->ConnectId, Result);
	}
}

FString LocalTalkerPS4::GetVoiceDebugState() const
{
	FString Output = FString::Printf(
		TEXT("== Local Talker %s==\n"),
		*SenderId->ToDebugString()
	);
	Output += FString::Printf(
		TEXT("Endpoint ids: L(%#x), R(%#x)\nLocal connection id: %#x\nActive connections: %d\nIs speaking: %s\n"),
		LocalEndpoint, RemoteEndpoint,
		LocalConnection->ConnectId,
		ActiveConnections.Num(),
		bIsSpeaking ? TEXT("YES") : TEXT("NO")
	);

	Output += TEXT("-- Connection List --\n");
	for (int Index = 0; Index < ActiveConnections.Num(); ++Index)
	{
		VoiceConnectionPS4Ptr Connection = ActiveConnections[Index];
		Output += FString::Printf(
			TEXT("Connection %d--\n\tConnection id: %d\n\tRemote talker: %s\n\tDead air count: %d\n"),
			Index,
			Connection->ConnectId,
			*(Connection->RemoteTalker->GetUserName()),
			Connection->DeadAirCount
		);
	}

	Output += TEXT("-- Remote mute list --\n");
	for (int Index = 0; Index < MutedRemotes.Num(); ++Index)
	{
		Output += FString::Printf(TEXT("%s\n"), *MutedRemotes[Index]);
	}
	return Output;
}

//
// RemoteTalkerPS4
//
RemoteTalkerPS4::RemoteTalkerPS4(const FString& InUserName) :
	UserId(NULL),
	UserName(InUserName)
{
}

RemoteTalkerPS4::~RemoteTalkerPS4()
{
}

TSharedPtr<const FUniqueNetId> RemoteTalkerPS4::GetUserId()
{
	// Return the cached unique id, creating one if we don't have one cached
	if (!UserId.IsValid())
	{
		IOnlineSubsystem* OnlineSubSystem = IOnlineSubsystem::Get(PS4_SUBSYSTEM);
		IOnlineIdentityPtr IdentityInt = OnlineSubSystem->GetIdentityInterface();
		if (IdentityInt.IsValid())
		{
			UserId = IdentityInt->CreateUniquePlayerId(UserName);
		}
	}
	return UserId;
}

//
// VoiceConnectionPS4
//
VoiceConnectionPS4::VoiceConnectionPS4(SceVoiceQoSLocalId InLocalEndpoint, SceVoiceQoSRemoteId InRemoteEndpoint) :
	ConnectId(SCE_VOICE_QOS_INVALID_CONNECTION_ID),
	LocalEndpoint(InLocalEndpoint),
	RemoteEndpoint(InRemoteEndpoint)
{
	// Create a new connection between the given local and remote endpoints
	SceVoiceQoSError Result = sceVoiceQoSConnect(&ConnectId, LocalEndpoint, RemoteEndpoint);
	if (Result != SCE_OK)
	{
		UE_LOG_ONLINE(Error, TEXT("sceVoiceQoSConnect() failed. Error code: %#x"), Result);
		ConnectId = SCE_VOICE_QOS_INVALID_CONNECTION_ID;
	}
}

VoiceConnectionPS4::~VoiceConnectionPS4()
{
	if (ConnectId != SCE_VOICE_QOS_INVALID_CONNECTION_ID)
	{
		// Destroy the QoS connection
		SceVoiceQoSError Result = sceVoiceQoSDisconnect(ConnectId);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("sceVoiceQoSDisconnect() failed. Error code: %#x"), Result);
		}
		ConnectId = SCE_VOICE_QOS_INVALID_CONNECTION_ID;
	}
}

void VoiceConnectionPS4::WriteVoicePacket(FVoicePacketPS4 *VoicePacket)
{
	if (ConnectId != SCE_VOICE_QOS_INVALID_CONNECTION_ID)
	{
		unsigned int WriteLength = VoicePacket->Length;
		SceVoiceQoSError Result = sceVoiceQoSWritePacket(ConnectId, VoicePacket->Buffer.GetData(), &WriteLength);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("sceVoiceQoSWritePacket() failed. Error code: %#x"), Result);
		}
		else
		{
			// Data was written. Make sure all of it was written
			if (WriteLength != VoicePacket->Length)
			{
				// In general the voice packets are small and are written entirely.
				// The PS4 docs indicate that a packet can be partially written but
				// the PS4 samples don't check.
				// Therefore, assuming it rare, we won't attempt to recover and will
				// instead simply log the event.
				UE_LOG_ONLINE(Error, TEXT("sceVoiceQoSDisconnect() only wrote %d bytes when %d were available."), WriteLength, VoicePacket->Length);
			}

			// Clear the dead air counter, since a packet was written to the connection
			DeadAirCount = 0;
		}
	}
}

bool VoiceConnectionPS4::AttachRemoteTalker (RemoteTalkerPS4Ptr InRemoteTalker)
{
	if (!RemoteTalker.IsValid())
	{
		RemoteTalker = InRemoteTalker;
		return true;
	}
	else if (RemoteTalker == InRemoteTalker)
	{
		return true;
	}
	return false;
}

void VoiceConnectionPS4::DetachRemoteTalker ()
{
	if (RemoteTalker.IsValid())
	{
		RemoteTalker.Reset();
	}
}

//
// Packet implementation
//
void FVoicePacketPS4::SetSender(TSharedPtr<const FUniqueNetId> InSender)
{
	Sender = InSender;
	SenderName = Sender->ToString();
}

uint16 FVoicePacketPS4::GetTotalPacketSize()
{
	return SenderName.Len() * sizeof(TCHAR) + sizeof(Length) + Length;
}

uint16 FVoicePacketPS4::GetBufferSize()
{
	return Length;
}

TSharedPtr<const FUniqueNetId> FVoicePacketPS4::GetSender()
{
	// If there is not a cached sender, create one from the string version, otherwise
	// return the cached version
	if (!Sender.IsValid())
	{		
		IOnlineSubsystem* OnlineSubSystem = IOnlineSubsystem::Get(PS4_SUBSYSTEM);
		IOnlineIdentityPtr IdentityInt = OnlineSubSystem->GetIdentityInterface();
		if (IdentityInt.IsValid())
		{
			Sender = IdentityInt->CreateUniquePlayerId(SenderName);
		}
	}
	return Sender;
}

void FVoicePacketPS4::Serialize(class FArchive& Ar)
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

//
// Main online voice implementation
//

FOnlineVoicePS4::FOnlineVoicePS4() :
	VoiceMemory(NULL),
	bIsInitialized(false)
{
}

FOnlineVoicePS4::~FOnlineVoicePS4()
{
	Shutdown();
}

bool FOnlineVoicePS4::Init()
{
	int Result = sceSysmoduleLoadModule(SCE_SYSMODULE_VOICEQOS);
	if (Result != SCE_OK)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_VOICEQOS) failed. Error code: 0x%08x"), Result);
		return false;
	}

	// NOTE: Required memory size is specified by the system
	VoiceMemory = FMemory::Malloc(SCE_VOICE_MEMORY_CONTAINER_SIZE);
	if (!VoiceMemory)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to allocate %d bytes for QoS voice memory"), SCE_VOICE_MEMORY_CONTAINER_SIZE);
		return false;
	}

	SceVoiceQoSError ErrorCode = sceVoiceQoSInit(VoiceMemory, SCE_VOICE_MEMORY_CONTAINER_SIZE, SCE_VOICE_APPTYPE_GAME);
	if (ErrorCode != SCE_OK)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceVoiceQoSInit() failed. Error code: 0x%08x"), ErrorCode);
		return false;
	}

	// All is well
	bIsInitialized = true;
	return bIsInitialized;
}

void FOnlineVoicePS4::Shutdown()
{
	if (bIsInitialized)
	{
		SceVoiceQoSError Result = sceVoiceQoSEnd();
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceVoiceQoSEnd() failed. Error code: 0x%08x"), Result);
		}
		bIsInitialized = false;
	}

	if (VoiceMemory != NULL)
	{
		FMemory::Free(VoiceMemory);
		VoiceMemory = NULL;
	}

	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_VOICEQOS))
	{
		int Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_VOICEQOS);
		if (Result != SCE_OK )
		{
			UE_LOG_ONLINE(Warning, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_VOICEQOS) failed. Error code: 0x%08x"), Result);
		}
	}
}

void FOnlineVoicePS4::ProcessMuteChangeNotification()
{
	if (!bIsInitialized) return;

	// For each local user with voice
	for (uint32 Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
	{
		// Find the very first ULocalPlayer for this ControllerId. 
		// This is imperfect and means we cannot support voice chat properly for
		// multiple UWorlds (but thats ok for the time being).
		ULocalPlayer* LP = GEngine->FindFirstLocalPlayerFromControllerId(Index);
		if (LP && LP->PlayerController)
		{
			if (LP->PlayerController != NULL)
			{
				UpdateMuteListForLocalTalker(Index, LP->PlayerController);
			}
		}
	}
}

void FOnlineVoicePS4::UpdateMuteListForLocalTalker(uint8 LocalUserNum, APlayerController* PlayerController)
{
	LocalTalkerPS4Ptr Local = FindLocalTalker (LocalUserNum);
	if (Local.IsValid())
	{
		// For each registered remote talker...
		for (int32 RemoteIndex = 0; RemoteIndex < RemoteTalkers.Num(); RemoteIndex++)
		{
			RemoteTalkerPS4Ptr Talker = RemoteTalkers[RemoteIndex];

			// If the given remote user is in the system mute list, tell the player controller to mute them
			// otherwise tell the player controller to unmute them
			FUniqueNetIdRepl UniqueIdRepl(Talker->GetUserId());

			if (SystemMuteList.Contains (Talker->GetUserName()))
			{
				// Mute on the server
				PlayerController->ServerMutePlayer(UniqueIdRepl);
			}
			else
			{
				// Unmute on the server
				PlayerController->ServerUnmutePlayer(UniqueIdRepl);
			}
		}
	}
}

void FOnlineVoicePS4::StartNetworkedVoice(uint8 LocalUserNum)
{
	LocalTalkerPS4Ptr Local = FindLocalTalker (LocalUserNum);
	if (Local.IsValid())
	{
		Local->StartNetworkedVoice();
	}
}

void FOnlineVoicePS4::StopNetworkedVoice(uint8 LocalUserNum)
{
	LocalTalkerPS4Ptr Local = FindLocalTalker (LocalUserNum);
	if (Local.IsValid())
	{
		Local->StopNetworkedVoice();
	}
}

bool FOnlineVoicePS4::RegisterLocalTalker(uint32 LocalUserNum)
{
	if (!bIsInitialized) return false;

	// Only register if the user service id is valid, i.e. they are logged in
	SceUserServiceUserId UserId = FOnlineIdentityPS4::GetSceUserId(LocalUserNum);
	if (UserId == SCE_USER_SERVICE_USER_ID_INVALID)
	{
		return false;
	}

	// Attempt to find the given local user within the list of registered local talkers
	for (int i = 0; i < LocalTalkers.Num(); ++i)
	{
		if (LocalTalkers[i]->LocalUserNum == LocalUserNum)
		{
			// Already registered so we succeeded
			return true;
		}
	}

	// No local talker for this user yet, so create one
	LocalTalkers.Add(MakeShareable(new LocalTalkerPS4(LocalUserNum, UserId)));
	return true;
}

void FOnlineVoicePS4::RegisterLocalTalkers()
{
	if (!bIsInitialized) return;

	// Loop through the 4 available players and attempt to register a talker for each
	// NOTE: Registration will fail for local players that are not logged in
	for (uint32 Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
	{
		RegisterLocalTalker(Index);
	}
}

bool FOnlineVoicePS4::UnregisterLocalTalkerAtIndex (int Index)
{
	if (Index >= 0 && Index < LocalTalkers.Num())
	{
		LocalTalkers.RemoveAt(Index);

		// With the last reference removed, the local talker will destroy itself,
		// in turn releasing its allocated connections and endpoints
		return true;
	}
	return false;
}

bool FOnlineVoicePS4::UnregisterLocalTalker(uint32 LocalUserNum)
{
	if (!bIsInitialized) return false;

	// Attempt to find the given local user within the list of registered local talkers
	for (int Index = 0; Index < LocalTalkers.Num(); ++Index)
	{
		if (LocalTalkers[Index]->LocalUserNum == LocalUserNum)
		{
			// Found it, so now remove it
			UnregisterLocalTalkerAtIndex(Index);
			return true;
		}
	}

	// Not in the list, so already not registered
	return true;
}

void FOnlineVoicePS4::UnregisterLocalTalkers()
{
	if (!bIsInitialized) return;

	while (LocalTalkers.Num() > 0)
	{
		UnregisterLocalTalkerAtIndex(LocalTalkers.Num()-1);
	}
}

bool FOnlineVoicePS4::RegisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	if (!bIsInitialized) return false;
	// Attempt to find the given remote user within the list of registered local talkers
	for (int i = 0; i < RemoteTalkers.Num(); ++i)
	{
		if (RemoteTalkers[i]->GetUserName().Equals(UniqueId.ToString()))
		{
			// Already registered
			return true;
		}
	}

	// No remote talker for this user yet, so create one. 
	// NOTE: Creating based on the string version of the unique id, as it is all that is
	// needed
	RemoteTalkers.Add(MakeShareable(new RemoteTalkerPS4(UniqueId.ToString())));
	return true;
}

bool FOnlineVoicePS4::UnregisterRemoteTalkerAtIndex (int Index)
{
	if (Index >= 0 && Index < RemoteTalkers.Num())
	{
		RemoteTalkers.RemoveAt(Index);

		// NOTE: Not telling the local talkers that the remote talker is gone.
		// Removal from this list will prevent received packets from being
		// sent to the local talkers, who will then drop the connections naturally.
		return true;
	}
	return false;
}

bool FOnlineVoicePS4::UnregisterRemoteTalker(const FUniqueNetId& UniqueId)
{
	if (!bIsInitialized) return false;

	// Attempt to find the given remote user within the list of registered remote talkers
	for (int Index = 0; Index < RemoteTalkers.Num(); ++Index)
	{
		if (RemoteTalkers[Index]->GetUserName().Equals(UniqueId.ToString()))
		{
			UnregisterRemoteTalkerAtIndex(Index);
			return true;
		}
	}

	// Not in the list, so already unregistered
	return true;
}

void FOnlineVoicePS4::RemoveAllRemoteTalkers()
{
	if (!bIsInitialized) return;

	while (RemoteTalkers.Num() > 0)
	{
		UnregisterRemoteTalkerAtIndex(RemoteTalkers.Num()-1);
	}
}

bool FOnlineVoicePS4::IsHeadsetPresent(uint32 LocalUserNum)
{
	if (!bIsInitialized) return false;
	LocalTalkerPS4Ptr Talker = FindLocalTalker(LocalUserNum);
	if (Talker.IsValid())
	{
		return Talker->IsMicrophoneUsable();
	}
	return false;
}

bool FOnlineVoicePS4::IsLocalPlayerTalking(uint32 LocalUserNum)
{
	if (!bIsInitialized) return false;
	LocalTalkerPS4Ptr Talker = FindLocalTalker(LocalUserNum);
	if (Talker.IsValid())
	{
		return Talker->IsLocalTalking();
	}
	return false;
}

bool FOnlineVoicePS4::IsRemotePlayerTalking(const FUniqueNetId& UniqueId)
{
	if (!bIsInitialized) return false;

	// If this remote talker is in any active connection with a local talker, return true.
	for (int Index = 0; Index < LocalTalkers.Num(); ++Index)
	{
		LocalTalkerPS4Ptr Local = LocalTalkers[Index];
		if (Local->IsRemoteTalking(UniqueId))
		{
			return true;
		}
	}
	return false;
}

bool FOnlineVoicePS4::IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const
{
	if (!bIsInitialized) return false;
	LocalTalkerPS4Ptr Talker = FindLocalTalker(LocalUserNum);
	if (Talker.IsValid())
	{
		return Talker->IsMuted(UniqueId);
	}
	return false;
}

bool FOnlineVoicePS4::MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	if (!bIsInitialized) return false;
	if (bIsSystemWide)
	{
		SystemMuteList.AddUnique(PlayerId.ToString());
		ProcessMuteChangeNotification();
		return true;
	}
	else
	{
		LocalTalkerPS4Ptr Talker = FindLocalTalker(LocalUserNum);
		if (Talker.IsValid())
		{
			Talker->MuteRemotePlayer(PlayerId);
			return true;
		}
	}
	return false;
}

bool FOnlineVoicePS4::UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide)
{
	if (!bIsInitialized) return false;
	if (bIsSystemWide)
	{
		SystemMuteList.Remove(PlayerId.ToString());
		ProcessMuteChangeNotification();
		return true;
	}
	else
	{
		LocalTalkerPS4Ptr Talker = FindLocalTalker(LocalUserNum);
		if (Talker.IsValid())
		{
			Talker->UnmuteRemotePlayer(PlayerId);
			return true;
		}
	}
	return false;
}

TSharedPtr<class FVoicePacket> FOnlineVoicePS4::SerializeRemotePacket(FArchive& Ar)
{
	if (!bIsInitialized) return NULL;

	// NOTE: This method intentionally has a side effect: it dispatches received packets
	// internally for playback subject to the current muting state in addition to
	// returning the newly created packet for the higher level code to handle.
	// This is not evident from the interface.

	// Create a new voice packet to hold the new data
	FVoicePacketPS4 *NewPacket = new FVoicePacketPS4();

	// De-Serialize data from the archive into the packet
	NewPacket->Serialize(Ar);

	// If the sender of the packet is a known remote talker...
	RemoteTalkerPS4Ptr Remote = FindRemoteTalker(NewPacket->GetSenderName());
	if (Remote.IsValid())
	{
		// Walk the list of local talkers, feeding the packet to each one
		for (int i = 0; i < LocalTalkers.Num(); ++i)
		{
			LocalTalkerPS4Ptr Local = LocalTalkers[i];

			// Send this packet to this local talker. The local talker will
			// handle dropping the packet if the remote is in its mute list
			Local->WriteVoicePacket(NewPacket);
		}
	}

	// Return the new packet, for potential further distribution
	return MakeShareable(NewPacket);
}

TSharedPtr<class FVoicePacket> FOnlineVoicePS4::GetLocalPacket(uint32 LocalUserNum)
{
	if (!bIsInitialized) return NULL;

	// NOTE: This is where the voice if fetchs from the local player's microphone.

	// If the local user number is registered, then fetch a packet's worth of data from
	// the connection for the local user
	LocalTalkerPS4Ptr Talker = FindLocalTalker(LocalUserNum);
	if (Talker.IsValid())
	{
		return Talker->ReadVoicePacket();
	}
	return NULL;
}

int32 FOnlineVoicePS4::GetNumLocalTalkers()
{
	if (!bIsInitialized) return 0;
	return LocalTalkers.Num();
}

void FOnlineVoicePS4::ClearVoicePackets()
{
	if (!bIsInitialized) return;
	// Since this system does not queue voice packets, do nothing here.
}

void FOnlineVoicePS4::Tick(float DeltaTime)
{
	if (!bIsInitialized) return;

	// Update all the active connections for each local talker
	for (int Index = 0; Index < LocalTalkers.Num(); ++Index)
	{
		LocalTalkers[Index]->UpdateConnections();
	}

	// Fire off any talking notifications for hud display
	ProcessTalkingDelegates(DeltaTime);
}

/**
* Processes any talking delegates that need to be fired off
*
* @param DeltaTime the amount of time that has elapsed since the last tick
*/
void FOnlineVoicePS4::ProcessTalkingDelegates(float DeltaTime)
{
	/** Critical sections for threadsafeness */
	FScopeLock LocalTalkersLock(&LocalTalkersCS);

	// Fire off any talker notification delegates for local talkers
	for (int32 LocalUserIndex = 0; LocalUserIndex < LocalTalkers.Num(); LocalUserIndex++)
	{
		LocalTalkerPS4Ptr& Talker = LocalTalkers[LocalUserIndex];

		// Skip all delegate handling if none are registered
		if (OnPlayerTalkingStateChangedDelegates.IsBound())
		{
			IOnlineSubsystem* OnlineSubSystem = IOnlineSubsystem::Get(PS4_SUBSYSTEM);
			IOnlineIdentityPtr IdentityInt = OnlineSubSystem->GetIdentityInterface();
			if (IdentityInt.IsValid())
			{
				//trigger local talker
				TSharedPtr<const FUniqueNetId> UniqueId = IdentityInt->GetUniquePlayerId(LocalUserIndex);
				if(UniqueId.IsValid())
				{
					TriggerOnPlayerTalkingStateChangedDelegates(UniqueId.ToSharedRef(), Talker->bIsSpeaking);

					//trigger remote talkers
					for (int32 RemoteTalkerIndex = 0; RemoteTalkerIndex < RemoteTalkers.Num(); ++RemoteTalkerIndex)
					{
						TSharedPtr<const FUniqueNetId> RemoteUniqueId = RemoteTalkers[RemoteTalkerIndex]->GetUserId();
						if (RemoteUniqueId->ToString() != UniqueId->ToString())
						{
							bool bIsRemoteTalking = Talker->IsRemoteTalking(*RemoteUniqueId);
							// Only send if the remote is actually talking (writing packets)
							if (Talker->bWritingVoicePackets)
							{
								// Send whether or not the remote talker is an active connection 
								TriggerOnPlayerTalkingStateChangedDelegates(RemoteUniqueId.ToSharedRef(), bIsRemoteTalking);
							}
						}
					}
				}
			}
		}
		//This causes too much spam in output and should only be enabled when testing/debugging
		//UE_LOG_ONLINE(Error, TEXT("Trigger %sTALKING"), Talker->bIsSpeaking ? TEXT("") : TEXT("NOT"));
	}
}

FString FOnlineVoicePS4::GetVoiceDebugState() const
{
	if (!bIsInitialized) return TEXT("Voice is uninitialized");
	FString Output;

	Output += FString::Printf(
		TEXT("Local talker count: %d\nRemote talker count: %d\nRemote endpoint count: %d\n"),
		LocalTalkers.Num(),
		RemoteTalkers.Num(),
		RemoteEndpointPool.GetCount()
	);

	Output += TEXT("## Local Talkers ##\n");
	for (int Index = 0; Index < LocalTalkers.Num(); ++Index)
	{
		LocalTalkerPS4Ptr Local = LocalTalkers[Index];
		Output += Local->GetVoiceDebugState();
	}

	Output += TEXT("## Remote Talkers ##\n");
	for (int Index = 0; Index < RemoteTalkers.Num(); ++Index)
	{
		RemoteTalkerPS4Ptr Remote = RemoteTalkers[Index];
		Output += FString::Printf(
			TEXT("\t%s\n"),
			*Remote->GetUserName()
		);
	}

	return Output;
}

LocalTalkerPS4Ptr FOnlineVoicePS4::FindLocalTalker (uint8 LocalUserNum) const
{
	for (int Index = 0; Index < LocalTalkers.Num(); ++Index)
	{
		if (LocalTalkers[Index]->LocalUserNum == LocalUserNum)
		{
			return LocalTalkers[Index];
		}
	}
	return NULL;
}

RemoteTalkerPS4Ptr FOnlineVoicePS4::FindRemoteTalker(TSharedPtr<const FUniqueNetId> UniqueId) const
{
	return FindRemoteTalker(UniqueId->ToString());
}

RemoteTalkerPS4Ptr FOnlineVoicePS4::FindRemoteTalker (const FString &UserName) const
{
	for (int Index = 0; Index < RemoteTalkers.Num(); ++Index)
	{
		if (RemoteTalkers[Index]->GetUserName().Equals(UserName))
		{
			return RemoteTalkers[Index];
		}
	}
	return NULL;
}
