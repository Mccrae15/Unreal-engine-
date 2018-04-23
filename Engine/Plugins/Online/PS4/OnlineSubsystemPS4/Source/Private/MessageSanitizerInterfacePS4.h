// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "IMessageSanitizerInterface.h"
#include "OnlineSubsystemPS4Package.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "Async/AsyncWork.h"
#include "Containers/Ticker.h"

class FOnlineSubsystemPS4;

struct FSanitizeMessage
{
	FSanitizeMessage(const FString& InRawMessage, FOnMessageProcessed InProcessCompleteDelegate)
		: RawMessage(InRawMessage)
		, CompletionDelegate(InProcessCompleteDelegate)
	{
	}

	FString RawMessage;
	FOnMessageProcessed CompletionDelegate;
};

struct FMultiPartMessage
{
	TArray<FString> AlreadyProcessedMessages;
	TArray<int32> AlreadyProcessedIndex;
	TArray<FString> SanitizedMessages;
	FString MessageToSanitize;
	bool bCompleted;
	int32 PartNumber;
};

struct FSanitizerTaskData
{
	FSanitizerTaskData(FSanitizeMessage InMessage)
		: Message(InMessage)
		, bSuccess(false)
	{}

	FSanitizeMessage Message;
	bool bSuccess;
};

// worker task
class FMessageSanitizerTask : public FNonAbandonableTask
{
public:

	/** Constructor */
	FMessageSanitizerTask(FOnlineSubsystemPS4 * InPS4Subsystem, const FSanitizerTaskData & InData)
		: PS4Subsystem(InPS4Subsystem), Data(InData)
	{
	}

	/** Performs work on thread */
	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FMessageSanitizerTask, STATGROUP_ThreadPoolAsyncTasks );
	}

private:
	FOnlineSubsystemPS4 * PS4Subsystem;
	FSanitizerTaskData Data;
};

typedef FAsyncTask<FMessageSanitizerTask> MessageSanitizerTask;

class FAsyncEventSanitizerTaskCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
private:

	FOnlineSubsystemPS4 * PS4Subsystem;

	FSanitizerTaskData Data;

public:
	/**
	 * Constructor.
	 *
	 * @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	 * @param InData All the data relating to the task
	 */
	FAsyncEventSanitizerTaskCompleted(FOnlineSubsystemPS4* InPS4Subsystem, const FSanitizerTaskData & InData) :
		FOnlineAsyncEvent(InPS4Subsystem),
		PS4Subsystem(InPS4Subsystem),
		Data(InData)
	{
	}

	virtual FString ToString() const override
	{
		return TEXT("Sanitize string complete");
	}

	virtual void TriggerDelegates() override;
};


/**
 * Implements the PS4 specific interface chat message sanitization
 */
class FMessageSanitizerPS4 :
	public IMessageSanitizer
{

public:

	// IMessageSanitizer
	virtual void SanitizeDisplayName(const FString& DisplayName, const FOnMessageProcessed& CompletionDelegate) override;
	virtual void SanitizeDisplayNames(const TArray<FString>& DisplayNames, const FOnMessageArrayProcessed& CompletionDelegate) override;
	virtual void QueryBlockedUser(int32 LocalUserNum, const FString& FromUserId, const FOnQueryUserBlockedResponse& CompletionDelegate) override;
	virtual void ResetBlockedUserCache() override {}
	// FMessageSanitizerPS4

	explicit FMessageSanitizerPS4(FOnlineSubsystemPS4* InPS4Subsystem) :
		PS4Subsystem(InPS4Subsystem)
	{
		check(PS4Subsystem);
		TickDelegate = FTickerDelegate::CreateRaw(this, &FMessageSanitizerPS4::HandleTicker);
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, 1.0f);
	}

	virtual ~FMessageSanitizerPS4();

private:
	bool HandleTicker(float DeltaTime);
	void CleanTaskList();
	void HandleMessageSanitized(bool bSuccess, const FString& SanitizedMessage, FOnMessageProcessed CompletionDelegate, FString UnsanitizedMessage);
	void HandleMessageArraySanitized(bool bSuccess, const FString& SanitizedMessage, 
		FOnMessageArrayProcessed CompletionDelegate,
		TArray<TSharedRef<FMultiPartMessage> > MultiPartArray,
		int32 PartIndex);

private:

	// Holds the list of messages being processed
	TArray<FSanitizeMessage> ProcessList;

	/** List of current tasks */
	TArray<TSharedPtr<MessageSanitizerTask>> CurrentTasks;

	// Holds a map of sanitized words
	TMap<FString, FString> WordMap;

	/** Holds a delegate to be invoked when the server ticks. */
	FTickerDelegate TickDelegate;

	/** Handle to the registered TickDelegate. */
	FDelegateHandle TickDelegateHandle;

	/** Reference to the main PS4 subsystem */
	FOnlineSubsystemPS4* PS4Subsystem;
};

typedef TSharedPtr<FMessageSanitizerPS4, ESPMode::ThreadSafe> FMessageSanitizerPS4Ptr;
