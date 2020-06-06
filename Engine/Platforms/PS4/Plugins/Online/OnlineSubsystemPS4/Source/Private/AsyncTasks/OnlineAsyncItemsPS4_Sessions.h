// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4.h"
#include "OnlineSessionInterfacePS4.h"

/*--------------------- FNonAbandonableTasks (unclear why they aren't FOnlineAsyncTasks) ---------------------*/
//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_CreateSession
//////////////////////////////////////////////////////////////////////////

struct SessionCreateTaskData
{
	FOnlineSessionSettings NewSessionSettings;
	FName SessionName;
	FNamedOnlineSession* Session = nullptr;
	bool bWasSuccessful = false;
};

class FOnlineAsyncTaskPS4_CreateSession : public FNonAbandonableTask
{
public:
	FOnlineAsyncTaskPS4_CreateSession(FOnlineSubsystemPS4& InPS4Subsystem, int32 InUserWebApiContext, SceNpMatching2ContextId InUserMatching2Context, const SessionCreateTaskData& InData)
		: PS4Subsystem(InPS4Subsystem)
		, UserWebApiContext(InUserWebApiContext)
		, UserMatching2Context(InUserMatching2Context)
		, Data(InData)
	{}

	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4_CreateSession, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	void SendResults();

	FOnlineSubsystemPS4& PS4Subsystem;
	int32 UserWebApiContext = INDEX_NONE;
	SceNpMatching2ContextId UserMatching2Context;
	SessionCreateTaskData Data;
};
typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4_CreateSession> AutoDeleteSessionCreateTask;

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_DestroySession
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_DestroySession : public FNonAbandonableTask
{
public:
	FOnlineAsyncTaskPS4_DestroySession(FOnlineSubsystemPS4& InPS4Subsystem, int32 InUserWebApiContext, const FName& InSessionName, const FString& InSessionId, const FOnDestroySessionCompleteDelegate& InCompletionDelegate)
		: PS4Subsystem(InPS4Subsystem)
		, UserWebApiContext(InUserWebApiContext)
		, SessionName(InSessionName)
		, SessionId(InSessionId)
		, CompletionDelegate(InCompletionDelegate)
	{}

	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FOnlineAsyncTaskPS4_DestroySession, STATGROUP_ThreadPoolAsyncTasks );
	}

private:
	FOnlineSubsystemPS4& PS4Subsystem;
	int32 UserWebApiContext = INDEX_NONE;
	const FName SessionName;
	const FString SessionId;
	FOnDestroySessionCompleteDelegate CompletionDelegate;
};
typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4_DestroySession> FAutoDeleteSessionDestroyTask;

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_JoinSession
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_JoinSession : public FNonAbandonableTask
{
public:
	FOnlineAsyncTaskPS4_JoinSession(FOnlineSubsystemPS4& InPS4Subsystem, int32 InUserWebApiContext, const FName& InSessionName, const FString& InSessionId, const FOnlineSessionSearchResult& InDesiredSession)
		: PS4Subsystem(InPS4Subsystem)
		, UserWebApiContext(InUserWebApiContext)
		, SessionName(InSessionName)
		, SessionId(InSessionId)
		, DesiredSession(InDesiredSession)
	{}

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4_JoinSession, STATGROUP_ThreadPoolAsyncTasks); }
	void DoWork();

private:
	FOnlineSubsystemPS4& PS4Subsystem;
	int32 UserWebApiContext = INDEX_NONE;
	const FName SessionName;
	const FString SessionId;
	FOnlineSessionSearchResult DesiredSession;
};
typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4_JoinSession> AutoDeleteSessionJoinTask;

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_LeaveSession
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_LeaveSession : public FNonAbandonableTask
{
public:
	FOnlineAsyncTaskPS4_LeaveSession(FOnlineSubsystemPS4& InPS4Subsystem, int32 InUserWebApiContext, const FName& InSessionName, const FString& InSessionId, const FOnDestroySessionCompleteDelegate& InCompletionDelegate)
		: PS4Subsystem(InPS4Subsystem)
		, UserWebApiContext(InUserWebApiContext)
		, SessionName(InSessionName)
		, SessionId(InSessionId)
		, CompletionDelegate(InCompletionDelegate)
	{}

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4_LeaveSession, STATGROUP_ThreadPoolAsyncTasks); }
	void DoWork();

private:
	FOnlineSubsystemPS4& PS4Subsystem;
	int32 UserWebApiContext = INDEX_NONE;
	const FName SessionName;
	const FString SessionId;
	FOnDestroySessionCompleteDelegate CompletionDelegate;
};
typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4_LeaveSession> FAutoDeleteSessionLeaveTask;


//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_GetSession
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_GetChangeableSessionData : public FNonAbandonableTask
{
public:
	FOnlineAsyncTaskPS4_GetChangeableSessionData(FOnlineSubsystemPS4& InPS4Subsystem, int32 InUserWebApiContext, FUniqueNetIdPS4Ref InUserId, const FString& InSessionId, const FOnGetChangeableSessionDataComplete& InCallback = FOnGetChangeableSessionDataComplete())
		: PS4Subsystem(InPS4Subsystem)
		, UserWebApiContext(InUserWebApiContext)
		, UserId(InUserId)
		, SessionId(InSessionId)
		, Callback(InCallback)
	{}

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4_GetChangeableSessionData, STATGROUP_ThreadPoolAsyncTasks); }
	void DoWork();

private:
	FOnlineSubsystemPS4& PS4Subsystem;
	int32 UserWebApiContext = INDEX_NONE;
	FUniqueNetIdPS4Ref UserId;
	FString SessionId;
	FOnGetChangeableSessionDataComplete Callback;
};
typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4_GetChangeableSessionData> AutoDeleteSessionGetChangeableSessionDataTask;

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_PutChangeableSessionData
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_PutChangeableSessionData : public FNonAbandonableTask
{
public:
	FOnlineAsyncTaskPS4_PutChangeableSessionData(FOnlineSubsystemPS4& InPS4Subsystem, int32 InUserWebApiContext, const FUniqueNetIdString& InSessionId, const FString& InData)
		: PS4Subsystem(InPS4Subsystem)
		, UserWebApiContext(InUserWebApiContext)
		, SessionId(InSessionId.ToString())
		, Data(InData)
	{
		check(InSessionId.GetType() == PS4_SUBSYSTEM);
	}

	static const TCHAR* Name() { return TEXT("FOnlineAsyncTaskPS4_PutChangeableSessionData"); }
	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4_PutChangeableSessionData, STATGROUP_ThreadPoolAsyncTasks); }

	void DoWork();

	void SetData(const FString& InData)
	{
		Data = InData;
	}

PACKAGE_SCOPE:
	FOnlineSubsystemPS4& PS4Subsystem;
	int32 UserWebApiContext = INDEX_NONE;
	FString SessionId;
	FString Data;
};
typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4_PutChangeableSessionData> AutoDeleteSessionPutChangeableSessionDataTask;

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_PutSession
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_PutSession : public FNonAbandonableTask
{
public:
	FOnlineAsyncTaskPS4_PutSession(FOnlineSubsystemPS4& InPS4Subsystem, int32 InUserWebApiContext, const FUniqueNetIdString& InSessionId, const FOnlineSessionSettings& InUpdatedSessionSettings)
		: PS4Subsystem(InPS4Subsystem)
		, UserWebApiContext(InUserWebApiContext)
		, SessionId(InSessionId)
		, UpdatedSessionSettings(InUpdatedSessionSettings)
	{
		ensure(SessionId.GetType() == PS4_SUBSYSTEM);
	}

	static const TCHAR* Name() { return TEXT("FOnlineAsyncTaskPS4_PutSession"); }
	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4_PutSession, STATGROUP_ThreadPoolAsyncTasks); }

	void DoWork();

private:
	FOnlineSubsystemPS4& PS4Subsystem;
	int32 UserWebApiContext = INDEX_NONE;
	FUniqueNetIdString SessionId;
	const FOnlineSessionSettings& UpdatedSessionSettings;

};
typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4_PutSession> AutoDeleteSessionPutSessionTask;

/*--------------------- Async Tasks ---------------------*/

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_GetSessionData
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_GetSessionData : public FOnlineAsyncTaskPS4
{
public:
	FOnlineAsyncTaskPS4_GetSessionData(FOnlineSubsystemPS4& PS4Subsystem, TSharedRef<const FUniqueNetIdPS4> InUserId, TSharedPtr<FOnlineSessionSearch> InSessionSearch, const FOnGetSessionComplete& InOnGetSessionComplete);

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskPS4_GetSessionData"); }
	virtual void Tick() override;
	virtual void TriggerDelegates() override;

private:
	FUniqueNetIdPS4Ref UserId;
	TSharedPtr<FOnlineSessionSearch> OnlineSessionSearch;
	FOnGetSessionComplete OnGetSessionDataComplete;
	bool bIsInitialized = false;

	FString TaskResponseBody;
};

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_GetSession
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_GetSession : public FOnlineAsyncTaskPS4
{
public:
	FOnlineAsyncTaskPS4_GetSession(FOnlineSubsystemPS4& PS4Subsystem, TSharedRef<const FUniqueNetIdPS4> InUserId, TSharedPtr<FOnlineSessionSearch> InSessionSearch, const FOnGetSessionComplete& InOnGetSessionComplete);

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskPS4_GetSession"); }
	virtual void Tick() override;
	virtual void TriggerDelegates() override;

private:
	TSharedRef<const FUniqueNetIdPS4> UserId;
	TSharedPtr<FOnlineSessionSearch> OnlineSessionSearch;
	FOnGetSessionComplete OnGetSessionComplete;
	bool bIsInitialized = false;

	FString TaskResponseBody;
};

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_LeaveRoom
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_LeaveRoom : public FOnlineAsyncTaskPS4
{
public:
	FOnlineAsyncTaskPS4_LeaveRoom(FOnlineSubsystemPS4& InPS4Subsystem, const FName& InSessionName, const FOnDestroySessionCompleteDelegate& InDestroySessionDelegate, SceNpMatching2RequestId InRequestId);

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskPS4_LeaveRoom"); }
	virtual void Tick() override;
	virtual void TriggerDelegates() override;

	void ProcessCallbackResult(int InErrorCode);
	
private:
	FOnlineSubsystemPS4& PS4Subsystem;
	const FName SessionName;
	FOnDestroySessionCompleteDelegate DestroySessionDelegate;
	SceNpMatching2RequestId RequestId;
	bool bIsInitialized = false;
	int32 ErrorCode = INT_MAX;
};

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_SendSessionInvite
//////////////////////////////////////////////////////////////////////////

class FOnlineAsyncTaskPS4_SendSessionInvite : public FOnlineAsyncTaskPS4
{
public:
	FOnlineAsyncTaskPS4_SendSessionInvite(FOnlineSubsystemPS4& InPS4Subsystem, const FUniqueNetIdPS4Ref& InUserId, const FName& InSessionName, const FString& InSessionId, const TArray<TSharedRef<const FUniqueNetId>>& InFriends);

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskPS4_SendSessionInvite"); }
	virtual void Tick() override;

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4_SendSessionInvite, STATGROUP_ThreadPoolAsyncTasks); }

private:
	FOnlineSubsystemPS4& PS4Subsystem;
	NpToolkit::Core::Response<NpToolkit::Core::Empty> SendInviteResponse;
};

/*--------------------- Async Events (most are just the result of the NonAbandonableTasks - should be rolled together into an FOnlineAsyncTask) ---------------------*/

//////////////////////////////////////////////////////////////////////////
// FAsyncEventPS4_CreateSessionComplete
//////////////////////////////////////////////////////////////////////////

class FAsyncEventPS4_CreateSessionComplete : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FAsyncEventPS4_CreateSessionComplete(FOnlineSubsystemPS4& InPS4Subsystem, const SessionCreateTaskData& InData) 
		: FOnlineAsyncEvent(&InPS4Subsystem)
		, Data(InData)
	{}

	virtual FString ToString() const override { return TEXT("FAsyncEventPS4_CreateSessionComplete"); }
	virtual void TriggerDelegates() override;

private:
	SessionCreateTaskData Data;
};

//////////////////////////////////////////////////////////////////////////
// FAsyncEventPS4_DestroySessionComplete
//////////////////////////////////////////////////////////////////////////

class FAsyncEventPS4_DestroySessionComplete : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FAsyncEventPS4_DestroySessionComplete(FOnlineSubsystemPS4& InPS4Subsystem, const FName& InSessionName, bool bInWasSuccessful, const FOnDestroySessionCompleteDelegate& InCompletionDelegate) 
		: FOnlineAsyncEvent(&InPS4Subsystem)
		, SessionName(InSessionName)
		, bWasSuccessful(bInWasSuccessful)
		, CompletionDelegate(InCompletionDelegate)
	{}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FAsyncEventPS4_DestroySessionComplete SessionName: %s bWasSuccessful: %d"), *SessionName.ToString(), bWasSuccessful);
	}

	virtual void TriggerDelegates() override;

private:
	const FName SessionName;
	bool bWasSuccessful = false;
	FOnDestroySessionCompleteDelegate CompletionDelegate;
};

//////////////////////////////////////////////////////////////////////////
// FAsyncEventPS4_JoinSession
//////////////////////////////////////////////////////////////////////////

class FOnlineSessionPS4;

class FAsyncEventPS4_JoinSession : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FAsyncEventPS4_JoinSession(FOnlineSubsystemPS4& InPS4Subsystem, FUniqueNetIdPS4Ref InJoiningPlayerId, const FName& InSessionName, const FOnlineSessionSearchResult& InDesiredSession) 
		: FOnlineAsyncEvent(&InPS4Subsystem)
		, JoiningPlayerId(InJoiningPlayerId)
		, SessionName(InSessionName)
		, DesiredSession(InDesiredSession)
	{}

	virtual FString ToString() const override { return TEXT("FAsyncEventPS4_JoinSession"); }
	virtual void TriggerDelegates() override;

private:
	FUniqueNetIdPS4Ref JoiningPlayerId;
	const FName SessionName;
	FOnlineSessionSearchResult DesiredSession;
};

//////////////////////////////////////////////////////////////////////////
// FAsyncEventPS4_JoinSessionComplete
//////////////////////////////////////////////////////////////////////////

class FAsyncEventPS4_JoinSessionComplete : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FAsyncEventPS4_JoinSessionComplete(FOnlineSubsystemPS4& InPS4Subsystem, const FName& InSessionName, const FOnlineSessionSearchResult& InDesiredSession, EOnJoinSessionCompleteResult::Type InResult) 
		: FOnlineAsyncEvent(&InPS4Subsystem)
		, SessionName(InSessionName)
		, DesiredSession(InDesiredSession)
		, Result(InResult)
	{}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FAsyncEventPS4_JoinSessionComplete SessionName: %s Result: %s"), *SessionName.ToString(), LexToString(Result));
	}

	virtual void TriggerDelegates() override;

private:
	const FName SessionName;
	FOnlineSessionSearchResult DesiredSession;
	EOnJoinSessionCompleteResult::Type Result;
};

//////////////////////////////////////////////////////////////////////////
// FAsyncEventPS4_LeaveSessionComplete
//////////////////////////////////////////////////////////////////////////

class FAsyncEventPS4_LeaveSessionComplete : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FAsyncEventPS4_LeaveSessionComplete(FOnlineSubsystemPS4& InPS4Subsystem, const FName& InSessionName, bool bInWasSuccessful, const FOnDestroySessionCompleteDelegate& InCompletionDelegate) :
		FOnlineAsyncEvent(&InPS4Subsystem),
		SessionName(InSessionName),
		bWasSuccessful(bInWasSuccessful),
		CompletionDelegate(InCompletionDelegate)
	{}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FAsyncEventPS4_LeaveSessionComplete SessionName: %s bWasSuccessful: %d"), *SessionName.ToString(), bWasSuccessful);
	}

	virtual void TriggerDelegates() override;

private:
	const FName SessionName;
	bool bWasSuccessful = false;
	FOnDestroySessionCompleteDelegate CompletionDelegate;
};

//////////////////////////////////////////////////////////////////////////
// FAsyncEventPS4_GetChangeableSessionDataComplete
//////////////////////////////////////////////////////////////////////////

class FAsyncEventPS4_GetChangeableSessionDataComplete : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FAsyncEventPS4_GetChangeableSessionDataComplete(FOnlineSubsystemPS4& InPS4Subsystem, FUniqueNetIdPS4Ref InUserId, FString&& InSessionId, FString&& InChangeableSessionData, const FOnGetChangeableSessionDataComplete& InCallback, bool bInWasSuccessful)
		: FOnlineAsyncEvent(&InPS4Subsystem)
		, UserId(InUserId)
		, SessionId(MoveTemp(InSessionId))
		, ChangeableSessionData(MoveTemp(InChangeableSessionData))
		, Callback(InCallback)
		, bWasSuccessful(bInWasSuccessful)
	{}

	virtual FString ToString() const override
	{
		return TEXT("FAsyncEventPS4_GetChangeableSessionDataComplete");
	}

	virtual void TriggerDelegates() override;

private:
	FUniqueNetIdPS4Ref UserId;
	FString SessionId;
	FString ChangeableSessionData;
	FOnGetChangeableSessionDataComplete Callback;
	bool bWasSuccessful = false;
};