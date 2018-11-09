// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MessageSanitizerInterfacePS4.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineIdentityInterfacePS4.h"

void FMessageSanitizerTask::DoWork()
{
	int ret = 0;
	SceUserServiceUserId userId = SCE_USER_SERVICE_USER_ID_INVALID;
	int titleCtxId = 0;
	int reqId = 0;
	char comment[SCE_NP_WORD_FILTER_CENSOR_COMMENT_MAXLEN + 1];
	char sanitizedComment[SCE_NP_WORD_FILTER_SANITIZE_COMMENT_MAXLEN + 1];

	memset(comment, 0, sizeof(comment));
	memset(sanitizedComment, 0, sizeof(sanitizedComment));

	strncpy(comment, TCHAR_TO_ANSI(*Data.Message.RawMessage), SCE_NP_WORD_FILTER_CENSOR_COMMENT_MAXLEN);

	// Ret values of < 0 are errors;

	ret = sceUserServiceGetInitialUser(&userId);
	if(ret == SCE_OK)
	{
		ret = sceNpWordFilterCreateTitleCtxA(userId);
	}

	titleCtxId = ret;

	if(ret > 0)
	{
		/* censor comment */
		ret = sceNpWordFilterCreateRequest(titleCtxId);
	}

	reqId = ret;

	if(ret > 0)
	{
		ret = sceNpWordFilterSanitizeComment(reqId, comment, sanitizedComment, NULL);
	}

	if(ret == SCE_OK)
	{
		Data.Message.RawMessage = FString(sanitizedComment);
		Data.bSuccess = true;
	}

	// Queue up an event in the async task manager so that the delegate can safely trigger in the game thread.
	if (PS4Subsystem->GetAsyncTaskManager())
	{
		auto NewEvent = new FAsyncEventSanitizerTaskCompleted(PS4Subsystem, Data);
		PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
	}
}

void FAsyncEventSanitizerTaskCompleted::TriggerDelegates()
{
	// this must be called from the main thread
	check(IsInGameThread());

	FOnlineAsyncEvent::TriggerDelegates();
	Data.Message.CompletionDelegate.ExecuteIfBound(Data.bSuccess, Data.Message.RawMessage);
}

void FMessageSanitizerPS4::SanitizeDisplayName(const FString& DisplayName, const FOnMessageProcessed& CompletionDelegate)
{
	const FString* FoundString = WordMap.Find(DisplayName);
	if (FoundString)
	{
		CompletionDelegate.ExecuteIfBound(true, *FoundString);
	}
	else
	{
		FOnMessageProcessed MessageSanitizerCallback = FOnMessageProcessed::CreateRaw(this, &FMessageSanitizerPS4::HandleMessageSanitized, CompletionDelegate, DisplayName);
		ProcessList.Add(FSanitizeMessage(DisplayName, MessageSanitizerCallback));
	}
}

void FMessageSanitizerPS4::SanitizeDisplayNames(const TArray<FString>& DisplayNames, const FOnMessageArrayProcessed& CompletionDelegate)
{
	FString SanitizeMessaged;
	TArray<FString> AlreadyProcessedMessages;
	TArray<int32> AlreadyProcessedIndex;
	TArray<TSharedRef<FMultiPartMessage> >  MultiPartArray;
	int32 PartNo = -1;
	int32 iMessagePart = -1;

	for (int32 iMessageIndex = 0; iMessageIndex < DisplayNames.Num(); iMessageIndex++)
	{
		const FString& DisplayName = DisplayNames[iMessageIndex];
		iMessagePart++;
		const FString* FoundString = WordMap.Find(DisplayName);
		if (!FoundString)
		{
			if (SanitizeMessaged.Len() + DisplayName.Len() + 1 > SCE_NP_WORD_FILTER_SANITIZE_COMMENT_MAXLEN)
			{
				PartNo++;
				TSharedRef<FMultiPartMessage> MultiPartMessage = MakeShared<FMultiPartMessage>();
				MultiPartMessage->PartNumber = PartNo;
				MultiPartMessage->bCompleted = false;
				MultiPartMessage->MessageToSanitize = SanitizeMessaged;
				MultiPartMessage->AlreadyProcessedMessages = AlreadyProcessedMessages;
				MultiPartMessage->AlreadyProcessedIndex = AlreadyProcessedIndex;
				MultiPartArray.Add(MultiPartMessage);
				SanitizeMessaged.Empty();
				AlreadyProcessedMessages.Empty();
				AlreadyProcessedIndex.Empty();
				iMessagePart = -1;
			}
			SanitizeMessaged += DisplayName + TEXT(",");
		}
		else
		{
			AlreadyProcessedMessages.Add(*FoundString);
			AlreadyProcessedIndex.Add(iMessagePart);
		}
	}

	if (MultiPartArray.Num() == 0)
	{
		if (!SanitizeMessaged.IsEmpty())
		{
			PartNo++;
			TSharedRef<FMultiPartMessage> MultiPartMessage = MakeShared<FMultiPartMessage>();
			MultiPartMessage->PartNumber = PartNo;
			MultiPartMessage->bCompleted = false;
			MultiPartMessage->MessageToSanitize = SanitizeMessaged;
			MultiPartMessage->AlreadyProcessedMessages = AlreadyProcessedMessages;
			MultiPartMessage->AlreadyProcessedIndex = AlreadyProcessedIndex;
			MultiPartArray.Add(MultiPartMessage);
			FOnMessageProcessed MessageSanitizerCallback = FOnMessageProcessed::CreateRaw(this, &FMessageSanitizerPS4::HandleMessageArraySanitized, CompletionDelegate, MultiPartArray, 0);
			ProcessList.Add(FSanitizeMessage(SanitizeMessaged, MessageSanitizerCallback));
		}
		else
		{
			CompletionDelegate.ExecuteIfBound(true, AlreadyProcessedMessages);
		}
	}
	else
	{
		PartNo++;
		TSharedRef<FMultiPartMessage> MultiPartMessage = MakeShared<FMultiPartMessage>();
		MultiPartMessage->PartNumber = PartNo;
		MultiPartMessage->bCompleted = false;
		MultiPartMessage->MessageToSanitize = SanitizeMessaged;
		MultiPartMessage->AlreadyProcessedMessages = AlreadyProcessedMessages;
		MultiPartMessage->AlreadyProcessedIndex = AlreadyProcessedIndex;
		MultiPartArray.Add(MultiPartMessage);

		for(const auto& MessagePart : MultiPartArray)
		{
			FOnMessageProcessed MessageSanitizerCallback = FOnMessageProcessed::CreateRaw(this, &FMessageSanitizerPS4::HandleMessageArraySanitized, CompletionDelegate, MultiPartArray, MessagePart->PartNumber);
			ProcessList.Add(FSanitizeMessage(MessagePart->MessageToSanitize, MessageSanitizerCallback));
		}
	}
}

void FMessageSanitizerPS4::QueryBlockedUser(int32 LocalUserNum, const FString& FromUserId, const FOnQueryUserBlockedResponse& CompletionDelegate)
{
	FBlockedQueryResult Result;
	Result.UserId = FromUserId;
	Result.bIsBlocked = false;
	CompletionDelegate.ExecuteIfBound(Result);
}

bool FMessageSanitizerPS4::HandleTicker(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FMessageSanitizerPS4_HandleTicker);
	if (ProcessList.Num())
	{
		FSanitizerTaskData Data(ProcessList[0]);
		TSharedRef<MessageSanitizerTask> SanitizerTask = MakeShared<MessageSanitizerTask>(PS4Subsystem, Data);
		SanitizerTask->StartBackgroundTask();
		CurrentTasks.Add(SanitizerTask);
		ProcessList.RemoveAt(0);
	}

	CleanTaskList();
	return true;
}

void FMessageSanitizerPS4::CleanTaskList()
{
	for (int i = CurrentTasks.Num() - 1; i >= 0; --i)
	{
		if (CurrentTasks[i]->IsDone())
		{
			CurrentTasks.RemoveAt(i);
		}
	}
}

void FMessageSanitizerPS4::HandleMessageSanitized(bool bSuccess, const FString& SanitizedMessage, FOnMessageProcessed CompletionDelegate, FString UnsanitizedMessage)
{
	// Add response
	if (!WordMap.Find(UnsanitizedMessage))
	{
		WordMap.Add(UnsanitizedMessage, SanitizedMessage);
	}
	CompletionDelegate.ExecuteIfBound(bSuccess, SanitizedMessage);
}

void FMessageSanitizerPS4::HandleMessageArraySanitized(bool bSuccess,
														const FString& SanitizedMessage, 
														FOnMessageArrayProcessed CompletionDelegate,
														TArray<TSharedRef<FMultiPartMessage> > MultiPartArray,
														int32 PartIndex)
{
	TArray<FString> SanitizedStrings;

	if (bSuccess == false)
	{
		CompletionDelegate.ExecuteIfBound(false, SanitizedStrings);
	}
	else
	{
		SanitizedMessage.ParseIntoArray(SanitizedStrings, TEXT(","), true);
		TSharedRef<FMultiPartMessage> CurrentMessageFragment = MultiPartArray[PartIndex];

		TArray<FString> UnsanitizedStrings;
		CurrentMessageFragment->MessageToSanitize.ParseIntoArray(UnsanitizedStrings, TEXT(","), true);

		check(SanitizedStrings.Num() == UnsanitizedStrings.Num())

		for (int32 iMessageIndex = 0; iMessageIndex < UnsanitizedStrings.Num(); iMessageIndex++)
		{
			if(!WordMap.Find(UnsanitizedStrings[iMessageIndex]))
			{
				WordMap.Add(UnsanitizedStrings[iMessageIndex], SanitizedStrings[iMessageIndex]);
			}
		}

		for (int32 MessageIndex = 0; MessageIndex < CurrentMessageFragment->AlreadyProcessedMessages.Num(); MessageIndex++)
		{
			SanitizedStrings.Insert(CurrentMessageFragment->AlreadyProcessedMessages[MessageIndex], CurrentMessageFragment->AlreadyProcessedIndex[MessageIndex]);
		}

		if (PartIndex == INDEX_NONE)
		{
			CompletionDelegate.ExecuteIfBound(true, SanitizedStrings);
		}
		else
		{
			check(MultiPartArray.Num() > PartIndex)
			CurrentMessageFragment->bCompleted = true;
			CurrentMessageFragment->SanitizedMessages = SanitizedStrings;
		
			bool bIsComplete = true;
			for (const auto& Part : MultiPartArray)
			{
				if (Part->bCompleted == false)
				{
					bIsComplete = false;
					break;
				}
			}

			if (bIsComplete)
			{
				TArray<FString> CompletedArray;
				for (const auto& Part : MultiPartArray)
				{
					CompletedArray += Part->SanitizedMessages;
				}
				CompletionDelegate.ExecuteIfBound(true, CompletedArray);
			}
		}
	}
}

FMessageSanitizerPS4::~FMessageSanitizerPS4()
{
	ProcessList.Empty();
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}