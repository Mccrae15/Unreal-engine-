// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "OnlineSubsystemPS4Private.h"
#include "VoiceInterface.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4Package.h"
#include "VoiceDataCommon.h"
#include <voice_qos.h>

class FUniqueNetId;

//
// Structures representing the local and remote talkers
//

// NOTE: The voice packet's 
// Custom PS4 implementation of a voice packet
class FVoicePacketPS4: public FVoicePacket
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

	/** Sets the packet as to having been sent by this sender */
	void SetSender(TSharedPtr<const FUniqueNetId> InSender);

	/** Access to the unique name of the packet sender */
	const FString& GetSenderName() const { return SenderName; }

public:
	FVoicePacketPS4() :
		Sender(NULL),
		Length(0)
	{
		// NOTE: Fixed size buffers here to help prevent fragmentation
		Buffer.Empty(MAX_VOICE_DATA_SIZE);
		Buffer.AddUninitialized(MAX_VOICE_DATA_SIZE);
	}

	virtual ~FVoicePacketPS4()
	{
	}

	/** @return the amount of space this packet will consume in a buffer */
	virtual uint16 GetTotalPacketSize() override;

	/** @return the amount of space used by the internal voice buffer */
	virtual uint16 GetBufferSize() override;

	/** 
		Will fetch the net id of the sender of the packet (if known) 
		or will resolve the sender name into a new net id.
		@return the sender of this voice packet
	*/
	virtual TSharedPtr<const FUniqueNetId> GetSender() override;

	virtual bool IsReliable() override { return false; }

	/** 
	 * Serialize the voice packet data into or out of the buffer, depending on the archive
	 *
	 * @param Ar buffer to write into
	 */
	virtual void Serialize(class FArchive& Ar) override;
};

// Simplified versions of shared pointer types
typedef TSharedPtr<class LocalTalkerPS4> LocalTalkerPS4Ptr;
typedef TSharedPtr<class RemoteTalkerPS4> RemoteTalkerPS4Ptr;
typedef TSharedPtr<class VoiceConnectionPS4> VoiceConnectionPS4Ptr;

// Encapsulates characteristic information about a local talker.
class LocalTalkerPS4
{
PACKAGE_SCOPE:
	/** UE4 local user for this talker */
	uint32 LocalUserNum;

	/** Local PS4 user id, not online id */
	SceUserServiceUserId UserId;

	/** QoS library id for this local user's voice endpoint */
	SceVoiceQoSLocalId LocalEndpoint;

	/** QoS library id for a remote user endpoint, not always active */
	SceVoiceQoSRemoteId RemoteEndpoint;

	/** The connection between the local and remote endpoints, used for reading local voice input */
	VoiceConnectionPS4Ptr LocalConnection;

	/** The list of currently speaking connections from remote talkers. Includes LocalConnection when in use */
	TArray<VoiceConnectionPS4Ptr> ActiveConnections;
	
	/** The unique net id of this local talker */
	TSharedPtr<const FUniqueNetId> SenderId;

	/** List of remote talkers muted for this local talker */
	TArray<FString> MutedRemotes;

	/** True if this talker sent a voice packet last frame */
	bool bIsSpeaking;

	/* Helper bool to figure out if remote users are actually talking or not*/
	bool bWritingVoicePackets;

	/** The max number of frames a remote connection is maintained without receiving voice data */
	static int MaxDeadAir; 

public:
	LocalTalkerPS4(uint32 LocalUserNum, SceUserServiceUserId InUserId);
	~LocalTalkerPS4();

	/** Closes any connections that have gone stale */
	void UpdateConnections();

	/** Writes the given packet to this local talker if not muted for the packet's sender */
	void WriteVoicePacket(FVoicePacketPS4* VoicePacket);

	/** Potentially read a packet from this local talker
		@return NULL if no packet is available.
	*/
	TSharedPtr<class FVoicePacket> ReadVoicePacket();

	/** Determine if the given remote player is in the muted or not */
	bool IsMuted(const FUniqueNetId& RemotePlayerId) const;
	bool IsMuted(const FString& RemotePlayerName) const;

	/** Determine if this local talker has an active connection with this remote player */
	bool IsRemoteTalking(const FUniqueNetId& RemotePlayerId) const;
	bool IsRemoteTalking(const FString& RemotePlayerName) const;

	/** Determine if this local player has talked recently */
	bool IsLocalTalking() const { return bIsSpeaking; }

	/** Mute this remote player with respect to this local talker */
	void MuteRemotePlayer(const FUniqueNetId& RemotePlayerId);
	void MuteRemotePlayer(const FString& RemotePlayterName);

	/** Unmute this remote player with respect to this local talker */
	void UnmuteRemotePlayer(const FUniqueNetId& RemotePlayerId);
	void UnmuteRemotePlayer(const FString& RemotePlayerName);

	/** Determine if the microphone is present */
	bool IsMicrophoneUsable();

	/** Start and stop networked voice operations for this local talker */
	void StartNetworkedVoice();
	void StopNetworkedVoice();

	/** Create some debug output */
	FString GetVoiceDebugState() const;
};

// Encapsulates characteristic information about a remote talker
class RemoteTalkerPS4
{
private:
	/** The actual unique user id for the remote talker, if resolved. Will not be valid until resolved */
	TSharedPtr<const FUniqueNetId> UserId;

	/** The remote talker's online id in string form. Must be valid. */
	FString UserName;

PACKAGE_SCOPE:
	/** @return the unique net id of the remote talker, resolving from name if needed */
	TSharedPtr<const FUniqueNetId> GetUserId();

	/** @return the string version (wire transmittable) of the unique net id of the remote talker */
	const FString &GetUserName() const { return UserName; }

public:
	/** Initializes the remote talker from the string version of its unique net id */
	explicit RemoteTalkerPS4(const FString& InUserName);
	~RemoteTalkerPS4();
};

// Encapsulates connections between local and remote endpoints
// NOTE: The connection can exist without being associated with a remote talker. This
// allows the voice connection to be reused, pulled from a pool, etc.
class VoiceConnectionPS4
{
PACKAGE_SCOPE:
	/** The QoS library id of the connection */
	SceVoiceQoSConnectionId ConnectId;

	/** The QoS library id of the local endpoint for the connection */
	SceVoiceQoSLocalId LocalEndpoint;

	/** The QoS library id of the remote endpoint for the connection */
	SceVoiceQoSRemoteId RemoteEndpoint;

	/** The associated remote talker, when attached */
	RemoteTalkerPS4Ptr RemoteTalker;

	/** Number of frames that have passed without a received packet from the remote end */
	int DeadAirCount;

public:
	VoiceConnectionPS4(SceVoiceQoSLocalId InLocalEndpoint, SceVoiceQoSRemoteId InRemoteEndpoint);
	~VoiceConnectionPS4();

	/** Associates the given remote talker with this connection */
	bool AttachRemoteTalker(RemoteTalkerPS4Ptr InRemoteTalker);

	/** Disassociates the given remote talker from this connection */
	void DetachRemoteTalker();

	/** Fetches the attached remote talker */
	RemoteTalkerPS4Ptr GetAttachedRemoteTalker()
	{
		return RemoteTalker;
	}

	/** Writes the given voice packet's data to the connection. Clears the dead air counter */
	void WriteVoicePacket(FVoicePacketPS4 *VoicePacket);

	/** Increments the dead air counter for this connection */
	int AddDeadAir()
	{
		return ++DeadAirCount;
	}
};

//
// IOnlineVoice implementation
//
class FOnlineVoicePS4 : public IOnlineVoice
{
	/** Memory buffer for QoS voice library */
	void* VoiceMemory;

	/** True if modules have been loaded and system been set up */
	bool bIsInitialized;

	/** The list of registered local talkers */
	TArray<LocalTalkerPS4Ptr> LocalTalkers;

	/** The list of registered remote talkers */
	TArray<RemoteTalkerPS4Ptr> RemoteTalkers;

	/** The list of active voice connections, i.e. remote talkers actually talking */
	TArray<VoiceConnectionPS4Ptr> VoiceConnections;

	/** Removes the local talker at the given array index */
	bool UnregisterLocalTalkerAtIndex(int Index);

	/** Removes the remote talker at the given array index */
	bool UnregisterRemoteTalkerAtIndex(int Index);

	/** The system wide mute list */
	TArray<FString> SystemMuteList;

	/** Critical section for threadsafeness */
	mutable FCriticalSection	LocalTalkersCS;

PACKAGE_SCOPE:
	/** Loads and initializes the QoS voice library */
	virtual bool Init() override;

	/**
	* Processes any talking delegates that need to be fired off
	*
	* @param DeltaTime the amount of time that has elapsed since the last tick
	*/
	void ProcessTalkingDelegates(float DeltaTime);

	/** Shuts down, cleans up, and unloads the QoS voice library */
	void Shutdown();

	/** @return the local talker registered for the given local user num, NULL if not found */
	LocalTalkerPS4Ptr FindLocalTalker(uint8 LocalUserNum) const;

	/** @return the remote talker registered for the given unique net id, NULL if not found */
	RemoteTalkerPS4Ptr FindRemoteTalker(TSharedPtr<const FUniqueNetId> UniqueId) const;

	/** @return the remote talker registered for the given string version of the unique net id, NULL if not found */
	RemoteTalkerPS4Ptr FindRemoteTalker(const FString& UserName) const;

	/** Sync the local mute setup to the replicated mute setup on the player controller */
	void UpdateMuteListForLocalTalker(uint8 LocalUserNum, class APlayerController* PlayerController);

	/** Handle changes in mute setup */
	virtual void ProcessMuteChangeNotification() override;

public:
	FOnlineVoicePS4();
	~FOnlineVoicePS4();

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
	virtual bool IsMuted(uint32 LocalUserNum, const FUniqueNetId& UniqueId) const override;
	virtual bool MuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual bool UnmuteRemoteTalker(uint8 LocalUserNum, const FUniqueNetId& PlayerId, bool bIsSystemWide) override;
	virtual TSharedPtr<class FVoicePacket> SerializeRemotePacket(FArchive& Ar) override;
	virtual TSharedPtr<class FVoicePacket> GetLocalPacket(uint32 LocalUserNum) override;
	virtual int32 GetNumLocalTalkers() override;
	virtual void ClearVoicePackets() override;
	virtual void Tick(float DeltaTime) override;
	virtual FString GetVoiceDebugState() const override;
};

