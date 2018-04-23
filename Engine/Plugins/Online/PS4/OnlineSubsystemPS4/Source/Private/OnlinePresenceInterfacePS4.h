// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlinePresenceInterface.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineAsyncTaskManagerPS4.h"

#include "WebApiPS4Task.h"
#include "target/include/json2.h"

typedef FAsyncTask<FWebApiPS4Task> BackgroundWebApiTask;

class FOnlineSubsystemPS4;

struct ETaskType
{
	enum Type
	{
		Set,
		Query
	};
};

struct TaskData
{
	TaskData(const FUniqueNetIdPS4& InUser)
		: User(InUser.AsShared())
	{}

	TSharedRef<FUniqueNetIdPS4 const> User;
	TSharedPtr<BackgroundWebApiTask> Task;
	ETaskType::Type TaskType;
	IOnlinePresence::FOnPresenceTaskCompleteDelegate Delegate;
};

// worker task
class FPresenceTask : public FNonAbandonableTask
{
public:

	/** Constructor */
	FPresenceTask(FOnlineSubsystemPS4 * InPS4Subsystem, const TaskData & InData)
		: PS4Subsystem(InPS4Subsystem), Data(InData)
	{
	}

	/** Performs work on thread */
	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FPresenceTask, STATGROUP_ThreadPoolAsyncTasks );
	}

private:
	FOnlineSubsystemPS4 * PS4Subsystem;
	TaskData Data;
};

typedef FAsyncTask<FPresenceTask> BackgroundPresenceTask;

class FAsyncEventPresenceTaskCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
private:

	FOnlineSubsystemPS4 * PS4Subsystem;

	TaskData Data;

public:
	/**
	* Constructor.
	*
	* @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	* @param InData All the data relating to the task
	*/
	FAsyncEventPresenceTaskCompleted(FOnlineSubsystemPS4* InPS4Subsystem, const TaskData & InData) :
		FOnlineAsyncEvent(InPS4Subsystem),
		PS4Subsystem(InPS4Subsystem),
		Data(InData)
	{
	}

	virtual FString ToString() const override
	{
		return (Data.TaskType == ETaskType::Set) ? TEXT("Set presence complete") : (Data.TaskType == ETaskType::Query) ? TEXT("Query presence complete") : TEXT("Unknown task complete");
	}

	virtual void TriggerDelegates() override;
};

/**
* Boilerplate allocator for json reader
*/
class FJsonAllocator : public sce::Json::MemAllocator
{
public:
	FJsonAllocator()
	{}
	~FJsonAllocator()
	{}
	virtual void* allocate(size_t size, void *) override
	{
		return FMemory::Malloc(size);;
	}
	virtual void deallocate(void *ptr, void *) override
	{
		FMemory::Free(ptr);
	}
};

/**
* Wrapper for background tasks that includes the time the request was created. Used for tracking the call rate limit.
*/
struct FPresenceTaskBackgroundWrapper
{
	FPresenceTaskBackgroundWrapper(TSharedPtr<BackgroundPresenceTask> InBackgroundPresenceTask, double InRequestCreationTime)
		: BackgroundPresenceTask(InBackgroundPresenceTask)
		, RequestCreationTime(InRequestCreationTime)
	{
	}

	TSharedPtr<BackgroundPresenceTask> BackgroundPresenceTask;
	double RequestCreationTime;

};

/**
* Implementation for the PS4 rich presence interface
*/
class FOnlinePresencePS4 : public IOnlinePresence
{
PACKAGE_SCOPE:
	/** Constructor
	*
	* @param InSubsystem The owner of this external UI interface.
	*/
	explicit FOnlinePresencePS4(class FOnlineSubsystemPS4* InSubsystem) :
		PS4Subsystem(InSubsystem),
		JsonInitParameter(&JsonAllocator, 0, 2048)
	{
		JsonInitializer.initialize(&JsonInitParameter);
	}

	/** Reference to the owning subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

	// we may not want these here, but rather in their own module
	FJsonAllocator JsonAllocator;
	sce::Json::InitParameter JsonInitParameter;
	sce::Json::Initializer JsonInitializer;

	/** List of current tasks per request type */
	TArray<FPresenceTaskBackgroundWrapper> CurrentSetPresenceTasks;
	TArray<FPresenceTaskBackgroundWrapper> CurrentQueryPresenceTasks;

	/** Map of presence data retrieved by the query request */
	TMap<FString, TSharedRef<FOnlineUserPresence>> CachedPresenceData;

	/**
	* Checks the task list and removes any that are done
	*/
	void CleanTaskList(TArray<FPresenceTaskBackgroundWrapper>& CurrentTasks, double CallRateLimitDuration);

	/**
	* Called on the main thread after a set request is finished so we can respond to it
	*
	* @param Data All of the task data
	*/
	bool SetPresenceResponse(const TaskData & Data);

	/**
	* Called on the main thread after a query request is finished so we can respond to it
	*
	* @param Data All of the task data
	*/
	bool QueryPresenceResponse(const TaskData & Data);

public:
	// IOnlinePresence
	virtual void SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual void QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate = FOnPresenceTaskCompleteDelegate()) override;
	virtual EOnlineCachedResult::Type GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence) override;
	virtual EOnlineCachedResult::Type GetCachedPresenceForApp(const FUniqueNetId& LocalUserId, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence) override;	

};

typedef TSharedPtr<FOnlinePresencePS4, ESPMode::ThreadSafe> FOnlinePresencePS4Ptr;

