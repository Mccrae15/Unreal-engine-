// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "OnlineSubsystemPS4Private.h"
#include "OnlineMessageInterface.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineSubsystemPS4Package.h"

// Collection of Toolkit item lists, indexed by LocalUserNum
typedef TMap<int32, TArray<TSharedRef<FOnlineMessageHeader>>> FItemsByUserMap;

// Collection of downloaded messages, indexed by message id
typedef TMap<uint64, TSharedRef<FOnlineMessage>> FMessagesById;

// Colection of downloaded message collections, indexed by userid
typedef TMap<int, FMessagesById> FMessagesByUserMap;

#define MAX_PS4_RECIPIENTS 16

class FUniqueMessageIdPS4 : public FUniqueMessageId
{
PACKAGE_SCOPE:
	SceNpGameCustomDataId MessageId;

public:
	explicit FUniqueMessageIdPS4(SceNpGameCustomDataId InMessageId)
		: MessageId(InMessageId)
	{
	}

	// IOnlinePlatformData
	virtual const uint8* GetBytes() const override
	{
		return reinterpret_cast<const uint8 *>(&MessageId);
	}

	virtual int32 GetSize() const override
	{
		return sizeof(MessageId);
	}

	explicit operator SceNpGameCustomDataId() const
	{
		return MessageId;
	}

	virtual bool IsValid() const override
	{
		return MessageId > 0;
	}

	virtual FString ToString() const override
	{
		return GetHexEncodedString();
	}

	virtual FString ToDebugString() const override
	{
		return ToString();
	}
};

/** 
 *  Async task for enumerating game custom data items
 */
class FOnlineAsyncTaskPS4EnumerateMessages : public FOnlineAsyncTaskPS4
{
	/** Local user id/number */
	TSharedRef<FUniqueNetIdPS4 const> LocalUserId;
	int32 LocalUserIndex;

	/** The user cloud implementation that issued this enumeration task */
	class FOnlineMessagePS4* MessagePtr;

	/** Storage for retrieved items */
	FItemsByUserMap* ItemsByUser;

	/** Error string for the result */
	FString ErrorString;

	/** Holds the request response */
	NpToolkit::Core::Response<NpToolkit::Messaging::GameDataMessages> Response;

	/** Hidden on purpose */
	FOnlineAsyncTaskPS4EnumerateMessages() = delete;

	/** Calls the PS4 toolkit API to fetch the game custom data item list */
	void FetchItemList();

public:
	FOnlineAsyncTaskPS4EnumerateMessages(class FOnlineSubsystemPS4* InSubsystem, class FOnlineMessagePS4* InMessage, FUniqueNetIdPS4 const& InLocalUserId, int32 InLocalUserIndex, FItemsByUserMap* InItemsByUser)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, LocalUserId(InLocalUserId.AsShared())
		, LocalUserIndex(InLocalUserIndex)
		, MessagePtr(InMessage)
		, ItemsByUser(InItemsByUser)
	{
		FetchItemList();
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;
	
	/**
	 * Marshal the accumulated data into its final home
	 */
	virtual void Finalize() override;

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/** 
 *  Async task for deleting an individual message
 */
class FOnlineAsyncTaskPS4GetMessage : public FOnlineAsyncTaskPS4
{
	/** Local user number */
	TSharedRef<FUniqueNetIdPS4 const> LocalUserId;
	int32 LocalUserIndex;

	/** The user cloud implementation that issued this enumeration task */
	class FOnlineMessagePS4* MessagePtr;

	/** The collection of messages, indexed by message id. This is the destination for the received message */
	FMessagesById* MessagesCollection;

	/** Which message to get */
	FUniqueMessageIdPS4 MessageToGet;

	/** Error string for the result */
	FString ErrorString;

	/** Holds the request response */
	NpToolkit::Core::Response<NpToolkit::Messaging::GameDataMessageAttachment> Response;

	/** Hidden on purpose */
	FOnlineAsyncTaskPS4GetMessage() = delete;

	/** Calls the PS4 toolkit API get the indicated item */
	void GetMessage();

public:
	FOnlineAsyncTaskPS4GetMessage(class FOnlineSubsystemPS4* InSubsystem, class FOnlineMessagePS4* InMessage, FUniqueNetIdPS4 const& InLocalUserId, int32 InLocalUserIndex, const FUniqueMessageIdPS4& InMessageToGet, FMessagesById* InMessageCollection)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, LocalUserId(InLocalUserId.AsShared())
		, LocalUserIndex(InLocalUserIndex)
		, MessagePtr(InMessage)
		, MessagesCollection(InMessageCollection)
		, MessageToGet(InMessageToGet)
	{
		GetMessage();
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;
	
	/**
	 * Marshal the accumlated data into its final home
	 */
	virtual void Finalize() override;

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/** 
 *  Async task for deleting an individual message
 */
class FOnlineAsyncTaskPS4SendMessage : public FOnlineAsyncTaskPS4
{
	/** ID of the user sending the message. */
	TSharedRef<FUniqueNetIdPS4 const> LocalUserId;

	/** The serialized payload for the message. */
	TArray<uint8> PayloadBuffer;

	/** The image file data used for the thumbnail of the message. */
	TArray<uint8> ImageData;

	/** Response object to check result of async call. */
	NpToolkit::Core::Response<NpToolkit::Core::Empty> Response;

	FString ErrorString;

	/** Hidden on purpose */
	FOnlineAsyncTaskPS4SendMessage() = delete;

public:
	FOnlineAsyncTaskPS4SendMessage(
		class FOnlineSubsystemPS4* InSubsystem,
		FString const& InMessageType,
		FString const& InMessageText,
		FString const& InImagePath,
		FUniqueNetIdPS4 const& InLocalUserId,
		TArray<TSharedRef<FUniqueNetId const>> const& InRecipientIds,
		FOnlineMessagePayload const& Payload);

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;

	/**
	 * Marshal the accumulated data into its final home
	 */
	virtual void Finalize() override;

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/** 
 *  Async task for deleting an individual message
 */
class FOnlineAsyncTaskPS4DeleteMessage : public FOnlineAsyncTaskPS4
{
	/** Local user number */
	TSharedRef<FUniqueNetIdPS4 const> LocalUserId;
	int32 LocalUserIndex;

	/** The user cloud implementation that issued this enumeration task */
	class FOnlineMessagePS4* MessagePtr;

	/** Which message to delete */
	FUniqueMessageIdPS4 MessageToDelete;

	/** Error string for the result */
	FString ErrorString;

	/** Holds the request response */
	NpToolkit::Core::Response<NpToolkit::Core::Empty> Response;

	/** Hidden on purpose */
	FOnlineAsyncTaskPS4DeleteMessage() = delete;

	/** Calls the PS4 toolkit API delete the indicated item */
	void DeleteMessage();

public:
	FOnlineAsyncTaskPS4DeleteMessage(class FOnlineSubsystemPS4* InSubsystem, class FOnlineMessagePS4* InMessage, FUniqueNetIdPS4 const& InLocalUserId, int32 InLocalUserIndex, const FUniqueMessageIdPS4& InMessageToDelete)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, LocalUserId(InLocalUserId.AsShared())
		, LocalUserIndex(InLocalUserIndex)
		, MessagePtr(InMessage)
		, MessageToDelete(InMessageToDelete)
	{
		DeleteMessage();
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;
	
	/**
	 * Marshal the accumlated data into its final home
	 */
	virtual void Finalize() override;

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

/**
 * Provides access to online messages on PS4
 */

class FOnlineMessagePS4 : public IOnlineMessage
{
private:
	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

	FItemsByUserMap ItemsByUser;  // GameCustomData Items[localUserNum][itemIndex]
	FMessagesByUserMap MessagesByUser; // Message Messages[localUserNum][messageId]

	FString DefaultImagePath;
	FString DefaultMessageText;

	FOnlineMessagePS4() = delete;

	void LoadConfig();

PACKAGE_SCOPE:
	explicit FOnlineMessagePS4(class FOnlineSubsystemPS4* InSubsystem) :
		PS4Subsystem(InSubsystem)
	{
	}

	/** Initialize the underlying PS4 system */
	bool Init();

	/** Cleanup resources and close out the underlying PS4 system */
	void Shutdown();

public:
	virtual ~FOnlineMessagePS4();

	// IOnlineMessage
	virtual bool EnumerateMessages(int32 LocalUserNum) override;
	virtual bool GetMessageHeaders(int32 LocalUserNum, TArray< TSharedRef<class FOnlineMessageHeader> >& OutHeaders) override;
	virtual bool ClearMessageHeaders(int32 LocalUserNum) override;
	virtual bool ReadMessage(int32 LocalUserNum, const FUniqueMessageId& MessageId) override;
	virtual TSharedPtr<class FOnlineMessage> GetMessage(int32 LocalUserNum, const FUniqueMessageId& MessageId) override;
	virtual bool ClearMessage(int32 LocalUserNum, const FUniqueMessageId& MessageId) override;
	virtual bool ClearMessages(int32 LocalUserNum) override;
	virtual bool SendMessage(int32 LocalUserNum, const TArray< TSharedRef<const FUniqueNetId> >& RecipientIds, const FString& MessageType, const FOnlineMessagePayload& Payload) override;
	virtual bool DeleteMessage(int32 LocalUserNum, const FUniqueMessageId& MessageId) override;
};
