// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineMessageInterfacePS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "HAL/PlatformFilemanager.h"

// The GameCustomData system only retains 100 items
#define SERIALIZE_MESSAGES_RAW_BYTES 0

//
// Async task for enumerating game custom data items
//
void FOnlineAsyncTaskPS4EnumerateMessages::FetchItemList()
{
	// NOTE: Assumes main thread

	// Make sure the specified local user has storage on the item structure.
	// NOTE: It is not the responsibility of this class to create that storage, just to verify that it is there
	if (!ItemsByUser || !ItemsByUser->Contains(LocalUserIndex))
	{
		ErrorString = TEXT("FOnlineAsyncTaskPS4EnumerateMessages requires item storage to be created prior to the call");
		bWasSuccessful = false;
		bIsComplete = true;
		return;
	}

	// Call into the toolkit to make the request, asking for the maximum possible list of items
	NpToolkit::Messaging::Request::GetReceivedGameDataMessages Request;
	Request.userId = LocalUserId->GetUserId();
	Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
	Request.async = true;
	Request.pageSize = NpToolkit::Messaging::Request::GetReceivedGameDataMessages::MAX_PAGE_SIZE;
	Request.type = NpToolkit::Messaging::Request::GameDataMessagesToRetrieve::all;
	
	int32 Result = NpToolkit::Messaging::getReceivedGameDataMessages(Request, &Response);
	if (Result < SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		ErrorString = FString::Printf(TEXT("getReceivedGameDataMessages() failed. Error code: %#x"), Result);
		bWasSuccessful = false;
		bIsComplete = true;
	}
	else
	{
		// The call was made successfully
		bWasSuccessful = false;
		bIsComplete = false;
	}
}

FString FOnlineAsyncTaskPS4EnumerateMessages::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4EnumerateMessages: %#x"), reinterpret_cast<uint64>(this));
}

void FOnlineAsyncTaskPS4EnumerateMessages::Tick()
{
	// Job thread
	if (Response.isLocked())
	{
		// Still busy, so bail
		return;
	}

	// The call is complete, so fetch the results and succeeded/failed state
	bWasSuccessful = Response.getReturnCode() == SCE_TOOLKIT_NP_V2_SUCCESS;

	// @todo: consider moving the translation of the message data from PS4 format to UE4 format here, rather than each time a message
	// is retrieved.

	// We are complete regardless of success or failure
	bIsComplete = true;
}
	
void FOnlineAsyncTaskPS4EnumerateMessages::Finalize()
{
	if (bWasSuccessful)
	{
		TArray<TSharedRef<FOnlineMessageHeader>> OutHeaders;

		NpToolkit::Messaging::GameDataMessages const* Messages = Response.get();
		for (int32 Index = 0; Index < Messages->numGameDataMessages; ++Index)
		{
			NpToolkit::Messaging::GameDataMessage const& Item = Messages->gameDataMessages[Index];

			if (Item.isUsed)
			{
				// Skip this item because it has already been used, i.e. it has been deleted
				continue;
			}

			TSharedRef<FUniqueNetIdPS4> FromUser = FUniqueNetIdPS4::FindOrCreate(Item.fromUser.accountId, Item.fromUser.onlineId);
			TSharedRef<FUniqueMessageIdPS4> MessageId = MakeShared<FUniqueMessageIdPS4>(Item.gameDataMsgId);
			TSharedRef<FOnlineMessageHeader> Header = MakeShared<FOnlineMessageHeader>(FromUser, MessageId);
			Header->FromName = PS4OnlineIdToString(Item.fromUser.onlineId);
			Header->TimeStamp = FString(UTF8_TO_TCHAR(Item.receivedDate));

			// TODO: Sony moved the dataDescription field into the Item.details struct, which we don't have here (Item.hasDetails == false) ...
			//Header->Type = FString(UTF8_TO_TCHAR(Item.details.dataDescription));

			OutHeaders.Add(Header);
		}

		// Copy the results into their final resting place
		if (ItemsByUser && ItemsByUser->Contains(LocalUserIndex))
		{
			(*ItemsByUser)[LocalUserIndex] = OutHeaders;
		}
		else
		{
			// The storage for the items was changed between the call to fetch and now, which is very naughty.
			// So we've no choice but to discard the response.
		}
	}
	else
	{
		ErrorString = FString::Printf(TEXT("getReceivedGameDataMessages() failed. Error code: %#x"), Response.getReturnCode());
	}
}

void FOnlineAsyncTaskPS4EnumerateMessages::TriggerDelegates()
{
	FOnlineAsyncTaskPS4::TriggerDelegates();

	IOnlineMessagePtr MessageInterface = Subsystem->GetMessageInterface();
	MessageInterface->TriggerOnEnumerateMessagesCompleteDelegates(LocalUserIndex, bWasSuccessful, ErrorString);
}

//
// FOnlineAsyncTaskPS4GetMessage implementation
//
void FOnlineAsyncTaskPS4GetMessage::GetMessage()
{
	// Call into the toolkit to make the request
	NpToolkit::Messaging::Request::GetGameDataMessageAttachment Request;
	Request.userId = LocalUserId->GetUserId();
	Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
	Request.async = true;
	Request.gameDataMsgId = MessageToGet.MessageId;

	int32 Result = NpToolkit::Messaging::getGameDataMessageAttachment(Request, &Response);
	if (Result < SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		ErrorString = FString::Printf(TEXT("getGameDataMessageAttachment() failed. Error code: %#x"), Result);
		bWasSuccessful = false;
		bIsComplete = true;
	}
	else
	{
		// The call was made successfully
		bWasSuccessful = false;
		bIsComplete = false;
	}
}

FString FOnlineAsyncTaskPS4GetMessage::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4GetMessage: %#x"), reinterpret_cast<uint64>(this));
}

void  FOnlineAsyncTaskPS4GetMessage::Tick()
{
	// Job thread
	if (Response.isLocked())
	{
		// Still busy, so bail
		return;
	}

	// The call is complete, so fetch the results and succeeded/failed state
	bWasSuccessful = Response.getReturnCode() == SCE_TOOLKIT_NP_V2_SUCCESS;

	// We are complete regardless of success or failure
	bIsComplete = true;
}

void  FOnlineAsyncTaskPS4GetMessage::Finalize()
{
	if (bWasSuccessful)
	{
		if (MessagesCollection)
		{
			NpToolkit::Messaging::GameDataMessageAttachment const* Attachment = Response.get();
			if (Attachment)
			{
				// Create a new message if we don't already have one for this message id
				if (!MessagesCollection->Contains(MessageToGet.MessageId))
				{
					TSharedPtr<FUniqueMessageIdPS4> MessageId = MakeShareable(new FUniqueMessageIdPS4(MessageToGet));
					MessagesCollection->Add(MessageToGet.MessageId, MakeShareable(new FOnlineMessage(MessageId.Get())));
				}

				// Deserialize the received message item into the storage
				TSharedRef<FOnlineMessage> MessageStorage((*MessagesCollection)[MessageToGet.MessageId]);

				TArray<uint8> MessageBytes;
				MessageBytes.AddZeroed(Attachment->attachmentSize);
				FMemory::Memcpy(MessageBytes.GetData(), Attachment->attachment, Attachment->attachmentSize);
#if SERIALIZE_MESSAGES_RAW_BYTES
				MessageStorage->Payload.FromBytes(MessageBytes);
#else
				MessageStorage->Payload.FromJsonStr(UTF8_TO_TCHAR(MessageBytes.GetData()));
#endif
			}
			else
			{
				ErrorString = FString::Printf(TEXT("No message content for message id %d"), MessageToGet.MessageId);
				bWasSuccessful = false;
			}
		}
	}
	else
	{
		ErrorString = FString::Printf(TEXT("getGameData() reported an error. Error code: %#x"), Response.getReturnCode());
	}
}

void  FOnlineAsyncTaskPS4GetMessage::TriggerDelegates()
{
	FOnlineAsyncTaskPS4::TriggerDelegates();

	IOnlineMessagePtr MessageInterface = Subsystem->GetMessageInterface();
	MessageInterface->TriggerOnReadMessageCompleteDelegates(LocalUserIndex, bWasSuccessful, MessageToGet, ErrorString);
}

//
// FOnlineAsyncTaskPS4DeleteMessage implementation
//

void FOnlineAsyncTaskPS4DeleteMessage::DeleteMessage()
{
	// Call into the toolkit to make the request
	NpToolkit::Messaging::Request::ConsumeGameDataMessage Request;
	Request.userId = LocalUserId->GetUserId();
	Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
	Request.async = true;
	Request.gameDataMsgId = MessageToDelete.MessageId;

	int32 Result = NpToolkit::Messaging::consumeGameDataMessage(Request, &Response);
	if (Result < SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		ErrorString = FString::Printf(TEXT("consumeGameDataMessage() failed. Error code: %#x"), Result);
		bWasSuccessful = false;
		bIsComplete = true;
	}
	else
	{
		// The call was made successfully
		bWasSuccessful = false;
		bIsComplete = false;
	}
}

FString FOnlineAsyncTaskPS4DeleteMessage::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4DeleteMessage: %#x"), reinterpret_cast<uint64>(this));
}

void FOnlineAsyncTaskPS4DeleteMessage::Tick()
{
	// Job thread
	if (Response.isLocked())
	{
		// Still busy, so bail
		return;
	}

	// The call is complete, so fetch the results and succeeded/failed state
	bWasSuccessful = Response.getReturnCode() == SCE_TOOLKIT_NP_V2_SUCCESS;

	// We are complete regardless of success or failure
	bIsComplete = true;
}

void FOnlineAsyncTaskPS4DeleteMessage::Finalize()
{
	if (!bWasSuccessful)
	{
		ErrorString = FString::Printf(TEXT("setMessageUseFlag() reported an error. Error code: %#x"), Response.getReturnCode());
	}
}

void FOnlineAsyncTaskPS4DeleteMessage::TriggerDelegates()
{
	FOnlineAsyncTaskPS4::TriggerDelegates();

	IOnlineMessagePtr MessageInterface = Subsystem->GetMessageInterface();
	MessageInterface->TriggerOnDeleteMessageCompleteDelegates(LocalUserIndex, bWasSuccessful, MessageToDelete, ErrorString);
}

//
// FOnlineAsyncTaskPS4SendMessage implementation
//

FOnlineAsyncTaskPS4SendMessage::FOnlineAsyncTaskPS4SendMessage(
		FOnlineSubsystemPS4* InSubsystem,
		FString const& InMessageType,
		FString const& InMessageText,
		FString const& InImagePath,
		FUniqueNetIdPS4 const& InLocalUserId,
		TArray<TSharedRef<FUniqueNetId const>> const& InRecipientIds,
		FOnlineMessagePayload const& Payload)
	: FOnlineAsyncTaskPS4(InSubsystem)
	, LocalUserId(InLocalUserId.AsShared())
{
	bWasSuccessful = false;
	bIsComplete = false;

	// Read the image file into a byte array.
	{
		FString ImageFullPath = FPaths::ProjectContentDir() + FString::Printf(TEXT("OSS/PS4/Messages/%s.jpg"), *InImagePath);
		UE_LOG_ONLINE(Log, TEXT("Attempting to load gameCustomData image from file: %s"), *ImageFullPath);

		IFileHandle* ImageFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*ImageFullPath);

		if (ImageFileHandle)
		{
			ImageData.AddZeroed(ImageFileHandle->Size());
			if (!ImageFileHandle->Read(ImageData.GetData(), ImageData.Num()))
			{
				ImageData.Empty();
			}
			delete ImageFileHandle;
		}
	}

	using SendGameDataMessage = NpToolkit::Messaging::Request::SendGameDataMessage;
	using GameDataMessageImage = NpToolkit::Messaging::Request::GameDataMessageImage;

	check(InRecipientIds.Num() >= 1);
	check(InRecipientIds.Num() <= SendGameDataMessage::MAX_NUM_RECIPIENTS);
	check(ImageData.Num() <= GameDataMessageImage::IMAGE_MAX_SIZE);

	SendGameDataMessage Request;
	Request.userId = LocalUserId->GetUserId();
	Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
	Request.async = true;

	// Copy recipient account IDs into the request
	for (int32 Index = 0; Index < InRecipientIds.Num(); ++Index)
	{
		Request.recipients[Index] = FUniqueNetIdPS4::Cast(InRecipientIds[Index])->GetAccountId();
	}

	FString AccountIdStr = PS4AccountIdToString(InLocalUserId.GetAccountId());

	// NOTE: We use the sending player's nickname as the data name, and stuff the message type into the
	// data description. This is so they can be pulled out and placed into the message header on the receive side
	// NOTE: All of these items are visible to the player in the system UI, so stuffing them in here might not be the best choice
	// @todo: consider pulling these values out of the message payload itself, allowing per-message configuration on the PS4
	FCStringAnsi::Strcpy(Request.dataName, TCHAR_TO_ANSI(*AccountIdStr));
	FCStringAnsi::Strcpy(Request.dataDescription, TCHAR_TO_ANSI(*InMessageType));
	FCStringAnsi::Strcpy(Request.textMessage, TCHAR_TO_ANSI(*InMessageText));

	Request.type = NpToolkit::Messaging::GameCustomDataType::attachment;
	Request.isPS4Available = true;

#if SERIALIZE_MESSAGES_RAW_BYTES
	// Serialize the payload properties as just bytes
	Payload.ToBytes(PayloadBuffer);
#else
	// Serialize the payload properties as a json string
	FString PayloadJson = Payload.ToJsonStr();
	PayloadBuffer.Empty();
	PayloadBuffer.AddZeroed(PayloadJson.Len() + 1);
	FMemory::Memcpy(PayloadBuffer.GetData(), TCHAR_TO_ANSI(*PayloadJson), PayloadBuffer.Num());
#endif

	Request.attachment = PayloadBuffer.GetData();
	Request.attachmentSize = PayloadBuffer.Num();

	int32 Ret = NpToolkit::Messaging::sendGameDataMessage(Request, &Response);
	if (Ret < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("sendGameDataMessage failed with error (sync): 0x%x"), Ret);
		bIsComplete = true;
	}
}

FString FOnlineAsyncTaskPS4SendMessage::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4SendMessage bWasSuccessful: %d"), bWasSuccessful);
}

void FOnlineAsyncTaskPS4SendMessage::Tick()
{
	if (Response.isLocked())
		return;

	bIsComplete = true;
	bWasSuccessful = Response.getReturnCode() == SCE_TOOLKIT_NP_V2_SUCCESS;
}

void FOnlineAsyncTaskPS4SendMessage::Finalize()
{
	if (!bWasSuccessful)
	{
		ErrorString = FString::Printf(TEXT("sendGameDataMessage failed with error (async): 0x%x"), Response.getReturnCode());
	}
}

void FOnlineAsyncTaskPS4SendMessage::TriggerDelegates()
{
	IOnlineMessagePtr MessageInterface = Subsystem->GetMessageInterface();

	int32 LocalUserIndex = FOnlineIdentityPS4::GetLocalUserIndex(LocalUserId->GetUserId());
	MessageInterface->TriggerOnSendMessageCompleteDelegates(LocalUserIndex, bWasSuccessful, ErrorString);
}

// 
// IOnlineMessage implementation
//

bool FOnlineMessagePS4::Init()
{
	LoadConfig();
	return true;
}

void FOnlineMessagePS4::Shutdown()
{
}

FOnlineMessagePS4::~FOnlineMessagePS4()
{
}

static const TCHAR *OSS_PS4_ONLINE_MESSAGE = TEXT("OnlineSubsystemPS4.OnlineMessage");

void FOnlineMessagePS4::LoadConfig()
{
	// Fetch the default image name and text to send
	if (!GConfig->GetString( OSS_PS4_ONLINE_MESSAGE, TEXT("DefaultImagePath"), DefaultImagePath, GEngineIni))
	{
		DefaultImagePath = TEXT("");
	}

	if (!GConfig->GetString( OSS_PS4_ONLINE_MESSAGE, TEXT("DefaultMessageText"), DefaultMessageText, GEngineIni))
	{
		DefaultMessageText = TEXT("UE4 Data Message");
	}
}

bool FOnlineMessagePS4::EnumerateMessages(int32 LocalUserNum)
{
	TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum));
	check(LocalUserId.IsValid());

	// Make sure there is an empty items entry for this local user.
	if (!ItemsByUser.Contains(LocalUserNum))
	{
		ItemsByUser.Add(LocalUserNum);
	}
	ItemsByUser[LocalUserNum].Empty();

	// Create the request for writing and then queue it
	FOnlineAsyncTaskPS4EnumerateMessages* Task = new FOnlineAsyncTaskPS4EnumerateMessages(PS4Subsystem, this, *LocalUserId, LocalUserNum, &ItemsByUser);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( Task );
	return true;
}

bool FOnlineMessagePS4::GetMessageHeaders(int32 LocalUserNum, TArray< TSharedRef<class FOnlineMessageHeader> >& OutHeaders)
{
	// Can't get message headers if we don't have a list of them
	if (!ItemsByUser.Contains(LocalUserNum))
	{
		return false;
	}

	OutHeaders = ItemsByUser[LocalUserNum];
	return true;
}

bool FOnlineMessagePS4::ClearMessageHeaders(int32 LocalUserNum)
{
	// Can't get message headers if we don't have a list of them
	if (!ItemsByUser.Contains(LocalUserNum))
	{
		return false;
	}
	ItemsByUser[LocalUserNum].Empty();
	return true;
}

bool FOnlineMessagePS4::ReadMessage(int32 LocalUserNum, const FUniqueMessageId& MessageId)
{
	FUniqueMessageIdPS4 const& PS4MessageId = static_cast<FUniqueMessageIdPS4 const&>(MessageId);

	TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum));
	check(LocalUserId.IsValid());

	// Make sure there is a messages entry for this local user.
	if (!MessagesByUser.Contains(LocalUserNum))
	{
		MessagesByUser.Add(LocalUserNum);
	}

	// Create the async task and queue it up.
	FOnlineAsyncTaskPS4GetMessage* Task = new FOnlineAsyncTaskPS4GetMessage(PS4Subsystem, this, *LocalUserId, LocalUserNum, PS4MessageId, &MessagesByUser[LocalUserNum]);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( Task );
	return false;
}

TSharedPtr<class FOnlineMessage> FOnlineMessagePS4::GetMessage(int32 LocalUserNum, const FUniqueMessageId& MessageId)
{
	// Make sure there is a messages entry for this local user.
	if (!MessagesByUser.Contains(LocalUserNum))
	{
		// No message list for user, therefore no cached messages
		return NULL;
	}

	// Attempt to get the indexed message
	FMessagesById& MessageListForUser = MessagesByUser[LocalUserNum];
	const SceUInt64 IdToGet = static_cast<const FUniqueMessageIdPS4 &>(MessageId).MessageId;

	if (MessageListForUser.Contains(IdToGet))
	{
		return MessageListForUser[IdToGet];
	}
	return NULL;
}

bool FOnlineMessagePS4::ClearMessage(int32 LocalUserNum, const FUniqueMessageId& MessageId)
{
	// Make sure there is a messages entry for this local user.
	if (!MessagesByUser.Contains(LocalUserNum))
	{
		// No message list for user, therefore no cached messages
		return true;
	}

	const SceUInt64 IdToClear = static_cast<const FUniqueMessageIdPS4 &>(MessageId).MessageId;
	MessagesByUser[LocalUserNum].Remove(IdToClear);
	return true;
}

bool FOnlineMessagePS4::ClearMessages(int32 LocalUserNum)
{
	// Clear all of the messages for the given local user
	if (!MessagesByUser.Contains(LocalUserNum))
	{
		// No message list for user, therefore no cached messages
		return true;
	}
	MessagesByUser[LocalUserNum].Empty();
	return true;
}

bool FOnlineMessagePS4::SendMessage(int32 LocalUserNum, const TArray< TSharedRef<const FUniqueNetId> >& RecipientIds, const FString& MessageType, const FOnlineMessagePayload& Payload)
{
	// NOTE: Can only send to 16 users at any given time
	if (RecipientIds.Num() > NpToolkit::Messaging::Request::SendGameDataMessage::MAX_NUM_RECIPIENTS)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineMessagePS4::SendMessage() - No more than %d recipients are allowed. %d were requested"),
			NpToolkit::Messaging::Request::SendGameDataMessage::MAX_NUM_RECIPIENTS,
			RecipientIds.Num());
		return false;
	}

	IOnlineIdentityPtr Identity = PS4Subsystem->GetIdentityInterface();
	TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(Identity->GetUniquePlayerId(LocalUserNum));

	FOnlineAsyncTaskPS4SendMessage* Task = new FOnlineAsyncTaskPS4SendMessage(PS4Subsystem, MessageType, DefaultMessageText, DefaultImagePath, *LocalUserId, RecipientIds, Payload);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(Task);
	return true;
}

bool FOnlineMessagePS4::DeleteMessage(int32 LocalUserNum, const FUniqueMessageId& MessageId)
{
	FUniqueMessageIdPS4 const& PS4MessageId = static_cast<FUniqueMessageIdPS4 const&>(MessageId);

	TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum));
	check(LocalUserId.IsValid());

	// Create the request for deleting and then queue it
	FOnlineAsyncTaskPS4DeleteMessage* Task = new FOnlineAsyncTaskPS4DeleteMessage(PS4Subsystem, this, *LocalUserId, LocalUserNum, PS4MessageId);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( Task );
	return true;
}
