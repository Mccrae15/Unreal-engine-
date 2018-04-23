// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4Package.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineAsyncTaskManager.h"
#include "WebApiPS4Task.h"

/**
* Delegate fired when getting the session has completed.
*
* @param UserId the unique net id of the user who made the request
* @param SessionId the session id which we're getting data for
* @param SessionInfo the session info retrieved for the session
* @param bWasSuccessful true if the async action completed without error, false if there was an error
*/
DECLARE_MULTICAST_DELEGATE_FourParams(FOnGetSessionComplete, const FUniqueNetId&, TSharedPtr<FOnlineSessionSearch>, const FString&, bool);
typedef FOnGetSessionComplete::FDelegate FOnGetSessionCompleteDelegate;

/**
* Delegate fired when getting the session data has completed.
*
* @param UserId the unique net id of the user who made the request
* @param SessionId the session id which we're getting data for
* @param SessionData the data retrieved for the session
* @param bWasSuccessful true if the async action completed without error, false if there was an error
*/
DECLARE_MULTICAST_DELEGATE_FourParams(FOnGetSessionDataComplete, const FUniqueNetId&, TSharedPtr<FOnlineSessionSearch>, const FString&, bool);
typedef FOnGetSessionDataComplete::FDelegate FOnGetSessionDataCompleteDelegate;

struct SessionCreateTaskData
{
	FOnlineSessionSettings NewSessionSettings;
	FName SessionName;
	FNamedOnlineSession * Session;
	bool bWasSuccessful;
};

class FOnlineSessionPS4;

class FOnlineAsyncTaskPS4SessionCreate : public FNonAbandonableTask
{
public:

	FOnlineAsyncTaskPS4SessionCreate(FOnlineSubsystemPS4 * InPS4Subsystem, int32 InUserWebApiContext, SceNpMatching2ContextId InUserMatching2Context, const SessionCreateTaskData& InData)
		: PS4Subsystem(InPS4Subsystem), UserWebApiContext(InUserWebApiContext), UserMatching2Context(InUserMatching2Context), Data(InData)
	{}

	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FOnlineAsyncTaskPS4SessionCreate, STATGROUP_ThreadPoolAsyncTasks );
	}

private:

	void SendResults();

	FOnlineSubsystemPS4 * PS4Subsystem;
	int32 UserWebApiContext;
	SceNpMatching2ContextId UserMatching2Context;
	SessionCreateTaskData Data;

};

typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4SessionCreate> AutoDeleteSessionCreateTask;

class FOnlineAsyncTaskPS4SessionDestroy : public FNonAbandonableTask
{
public:

	FOnlineAsyncTaskPS4SessionDestroy(FOnlineSubsystemPS4 * InPS4Subsystem, int32 InUserWebApiContext, FNamedOnlineSession * InSession, const FOnDestroySessionCompleteDelegate& InCompletionDelegate)
		: PS4Subsystem(InPS4Subsystem), UserWebApiContext(InUserWebApiContext), Session(InSession), CompletionDelegate(InCompletionDelegate)
	{}

	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FOnlineAsyncTaskPS4SessionDestroy, STATGROUP_ThreadPoolAsyncTasks );
	}

private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	int32 UserWebApiContext;
	FNamedOnlineSession * Session;
	FOnDestroySessionCompleteDelegate CompletionDelegate;

};

typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4SessionDestroy> AutoDeleteSessionDestroyTask;

class FOnlineAsyncTaskPS4SessionJoin : public FNonAbandonableTask
{
public:

	FOnlineAsyncTaskPS4SessionJoin(FOnlineSubsystemPS4 * InPS4Subsystem, int32 InUserWebApiContext, FNamedOnlineSession * InSession, const FOnlineSessionSearchResult& InDesiredSession)
		: PS4Subsystem(InPS4Subsystem), UserWebApiContext(InUserWebApiContext), Session(InSession), DesiredSession(InDesiredSession)
	{}

	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FOnlineAsyncTaskPS4SessionJoin, STATGROUP_ThreadPoolAsyncTasks );
	}

private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	int32 UserWebApiContext;
	FNamedOnlineSession * Session;
	FOnlineSessionSearchResult DesiredSession;

};

typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4SessionJoin> AutoDeleteSessionJoinTask;

class FOnlineAsyncTaskPS4SessionLeave : public FNonAbandonableTask
{
public:

	FOnlineAsyncTaskPS4SessionLeave(FOnlineSubsystemPS4 * InPS4Subsystem, int32 InUserWebApiContext, FNamedOnlineSession * InSession, const FOnDestroySessionCompleteDelegate& InCompletionDelegate)
		: PS4Subsystem(InPS4Subsystem), UserWebApiContext(InUserWebApiContext), Session(InSession), CompletionDelegate(InCompletionDelegate)
	{}

	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FOnlineAsyncTaskPS4SessionLeave, STATGROUP_ThreadPoolAsyncTasks );
	}

private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	int32 UserWebApiContext;
	FNamedOnlineSession * Session;
	FOnDestroySessionCompleteDelegate CompletionDelegate;

};

class FOnlineAsyncTaskPS4GetSessionData : public FOnlineAsyncTaskBasic<FOnlineSubsystemPS4>
{
public:
	FOnlineAsyncTaskPS4GetSessionData(FOnlineSubsystemPS4* InSubsystem, TSharedRef<const FUniqueNetIdPS4> InUserId, TSharedPtr<FOnlineSessionSearch> InSessionSearch, const FOnGetSessionDataCompleteDelegate& InOnGetSessionDataCompleteDelegate);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("GetSessionData"); }
	virtual void TriggerDelegates() override;

	// FOnlineAsyncTask
	virtual void Tick() override;

private:
	FOnlineSubsystemPS4* PS4Subsystem;
	TSharedRef<const FUniqueNetIdPS4> UserId;
	TSharedPtr<FOnlineSessionSearch> OnlineSessionSearch;
	FOnGetSessionDataCompleteDelegate OnGetSessionDataCompleteDelegate;
	bool bInitialized;

	FString TaskResponseBody;

};

class FOnlineAsyncTaskPS4GetSession : public FOnlineAsyncTaskBasic<FOnlineSubsystemPS4>
{
public:
	FOnlineAsyncTaskPS4GetSession(FOnlineSubsystemPS4* InSubsystem, TSharedRef<const FUniqueNetIdPS4> InUserId, TSharedPtr<FOnlineSessionSearch> InSessionSearch, const FOnGetSessionCompleteDelegate& InOnGetSessionCompleteDelegate);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("GetSession"); }
	virtual void TriggerDelegates() override;

	// FOnlineAsyncTask
	virtual void Tick() override;

private:
	FOnlineSubsystemPS4* PS4Subsystem;
	TSharedRef<const FUniqueNetIdPS4> UserId;
	TSharedPtr<FOnlineSessionSearch> OnlineSessionSearch;
	FOnGetSessionCompleteDelegate OnGetSessionCompleteDelegate;
	bool bInitialized;

	FString TaskResponseBody;

};

typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4SessionLeave> AutoDeleteSessionLeaveTask;

class FOnlineAsyncTaskPS4SessionGetChangeableSessionData : public FNonAbandonableTask
{
public:

	DECLARE_DELEGATE_FourParams(FOnSessionGetChangeableSessionDataComplete, int32, const FString &, const FString &, bool);

	/**
	* Constructor.
	*
	* @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	* @param InUserWebApiContext The context for the user performing the request
	* @param InSessionId ID of the session we got the data of
	* @param InCallback The Callback to call on the main thread once the data is retrieved
	*/
	FOnlineAsyncTaskPS4SessionGetChangeableSessionData(FOnlineSubsystemPS4 * InPS4Subsystem, int32 InUserWebApiContext, TSharedPtr<const FUniqueNetIdPS4> InUserId, const FString& InSessionId, const FOnSessionGetChangeableSessionDataComplete & InCallback = FOnSessionGetChangeableSessionDataComplete())
		: PS4Subsystem(InPS4Subsystem), UserWebApiContext(InUserWebApiContext), UserId(InUserId), SessionId(InSessionId), Callback(InCallback)
	{}

	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FOnlineAsyncTaskPS4SessionGetChangeableSessionData, STATGROUP_ThreadPoolAsyncTasks );
	}

private:

	FOnlineSubsystemPS4 * PS4Subsystem;	
	int32 UserWebApiContext;
	TSharedPtr<const FUniqueNetIdPS4> UserId;
	FString SessionId;
	FOnSessionGetChangeableSessionDataComplete Callback;

};

typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4SessionGetChangeableSessionData> AutoDeleteSessionGetChangeableSessionDataTask;

class FOnlineAsyncTaskPS4SessionPutChangeableSessionData : public FNonAbandonableTask
{
public:

	/**
	* Constructor.
	*
	* @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	* @param InUserWebApiContext The context for the user performing the request
	* @param InSessionId ID of the session we got the data of
	* @param InData The data to store
	*/
	FOnlineAsyncTaskPS4SessionPutChangeableSessionData(FOnlineSubsystemPS4 * InPS4Subsystem, int32 InUserWebApiContext, const FUniqueNetIdString* InSessionId, const FString& InData)
		: PS4Subsystem(InPS4Subsystem), UserWebApiContext(InUserWebApiContext), SessionId(InSessionId->ToString()), Data(InData)
	{}

	void SetData(const FString& InData)
	{
		Data = InData;
	}

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4SessionPutChangeableSessionData, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork();

	static const TCHAR* Name()
	{
		return TEXT("FOnlineAsyncTaskPS4SessionPutChangeableSessionData");
	}

private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	int32 UserWebApiContext;
	FUniqueNetIdString SessionId;
	FString Data;

};

typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4SessionPutChangeableSessionData> AutoDeleteSessionPutChangeableSessionDataTask;

class FOnlineAsyncTaskPS4SessionPutSession : public FNonAbandonableTask
{
public:

	/**
	* Constructor.
	*
	* @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	* @param InUserWebApiContext The context for the user performing the request
	* @param InSessionId ID of the session we got the data of
	* @param InUpdatedSessionSettings The session settings to use to update the session
	*/
	FOnlineAsyncTaskPS4SessionPutSession(FOnlineSubsystemPS4 * InPS4Subsystem, int32 InUserWebApiContext, const FUniqueNetIdString* InSessionId, const FOnlineSessionSettings& InUpdatedSessionSettings)
		: PS4Subsystem(InPS4Subsystem), UserWebApiContext(InUserWebApiContext), SessionId(InSessionId->ToString()), UpdatedSessionSettings(InUpdatedSessionSettings)
	{}

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4SessionPutSession, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork();

	static const TCHAR* Name()
	{
		return TEXT("FOnlineAsyncTaskPS4SessionPutSession");
	}

private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	int32 UserWebApiContext;
	FUniqueNetIdString SessionId;
	const FOnlineSessionSettings& UpdatedSessionSettings;

};

typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4SessionPutSession> AutoDeleteSessionPutSessionTask;

class FAsyncEventSessionJoinTask : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	FOnlineSessionPS4 * SessionPS4;
	TSharedRef<FUniqueNetIdPS4 const> JoiningPlayerId;
	FName SessionName;
	FOnlineSessionSearchResult DesiredSession;

public:
	/**
	* Constructor.
	*
	* @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	*/
	FAsyncEventSessionJoinTask(FOnlineSubsystemPS4* InPS4Subsystem, FOnlineSessionPS4 * InSessionPS4, const FUniqueNetIdPS4& InJoiningPlayerId, FName InSessionName, const FOnlineSessionSearchResult& InDesiredSession) :
		FOnlineAsyncEvent(InPS4Subsystem),
		PS4Subsystem(InPS4Subsystem),
		SessionPS4(InSessionPS4),
		JoiningPlayerId(InJoiningPlayerId.AsShared()),
		SessionName(InSessionName),
		DesiredSession(InDesiredSession)
	{
	}

	virtual FString ToString() const override
	{
		return TEXT("Session join task");
	}

	virtual void TriggerDelegates() override;
};

class FAsyncEventSessionCreateTaskCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	SessionCreateTaskData Data;
	
public:
	/**
	* Constructor.
	*
	* @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	* @param InData All the data relating to the task
	*/
	FAsyncEventSessionCreateTaskCompleted(FOnlineSubsystemPS4* InPS4Subsystem, const SessionCreateTaskData & InData) :
		FOnlineAsyncEvent(InPS4Subsystem),
		PS4Subsystem(InPS4Subsystem),
		Data(InData)
	{
	}

	virtual FString ToString() const override
	{
		return TEXT("Session create task complete");
	}

	virtual void TriggerDelegates() override;
};

class FAsyncEventSessionDestroyTaskCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	FName SessionName;
	bool bWasSuccessful;
	FOnDestroySessionCompleteDelegate CompletionDelegate;

public:
	/**
	 * Constructor.
	 *
	 * @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	 * @param InSessionName Name of the session we tried to destroy
	 * @param bInWasSuccessful Whether or not the destroy was successful
	 */
	FAsyncEventSessionDestroyTaskCompleted(FOnlineSubsystemPS4* InPS4Subsystem, const FName& InSessionName, bool bInWasSuccessful, const FOnDestroySessionCompleteDelegate& InCompletionDelegate) :
		FOnlineAsyncEvent(InPS4Subsystem),
		PS4Subsystem(InPS4Subsystem),
		SessionName(InSessionName),
		bWasSuccessful(bInWasSuccessful),
		CompletionDelegate(InCompletionDelegate)
	{
	}

	virtual FString ToString() const override
	{
		return TEXT("Session destroy task complete");
	}

	virtual void TriggerDelegates() override;
};

class FAsyncEventSessionJoinTaskCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	FName SessionName;
	FOnlineSessionSearchResult DesiredSession;
	EOnJoinSessionCompleteResult::Type Result;

public:
	/**
	* Constructor.
	*
	* @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	* @param InSessionName Name of the session we tried to join
	* @param bInWasSuccessful Whether or not the join was successful
	*/
	FAsyncEventSessionJoinTaskCompleted(FOnlineSubsystemPS4* InPS4Subsystem, const FName& InSessionName, const FOnlineSessionSearchResult& InDesiredSession, EOnJoinSessionCompleteResult::Type InResult) :
		FOnlineAsyncEvent(InPS4Subsystem),
		PS4Subsystem(InPS4Subsystem),
		SessionName(InSessionName),
		DesiredSession(InDesiredSession),
		Result(InResult)
	{
	}

	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FAsyncEventSessionJoinTaskCompleted SessionName: %s Result: %s"), *SessionName.ToString(), Lex::ToString(Result));
	}

	virtual void TriggerDelegates() override;
};

class FAsyncEventSessionLeaveTaskCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	FName SessionName;
	bool bWasSuccessful;
	FOnDestroySessionCompleteDelegate CompletionDelegate;

public:
	/**
	* Constructor.
	*
	* @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	* @param InSessionName Name of the session we tried to join
	* @param bInWasSuccessful Whether or not the join was successful
	*/
	FAsyncEventSessionLeaveTaskCompleted(FOnlineSubsystemPS4* InPS4Subsystem, const FName& InSessionName, bool bInWasSuccessful, const FOnDestroySessionCompleteDelegate& InCompletionDelegate) :
		FOnlineAsyncEvent(InPS4Subsystem),
		PS4Subsystem(InPS4Subsystem),
		SessionName(InSessionName),
		bWasSuccessful(bInWasSuccessful),
		CompletionDelegate(InCompletionDelegate)
	{
	}

	virtual FString ToString() const override
	{
		return TEXT("Session leave task complete");
	}

	virtual void TriggerDelegates() override;
};

class FAsyncEventSessionGetChangeableSessionDataTaskCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
private:
	
	FOnlineSubsystemPS4 * PS4Subsystem;	
	TSharedPtr<const FUniqueNetIdPS4> UserId;
	FString SessionId;
	FString ChangeableSessionData;
	FOnlineAsyncTaskPS4SessionGetChangeableSessionData::FOnSessionGetChangeableSessionDataComplete Callback;
	bool bWasSuccessful;

public:
	/**
	* Constructor.
	*
	* @param InPS4Subsystem The owner of the external UI interface that triggered this event.
	* @param InSessionId ID of the session we got the data of
	* @param InChangeableSessionData The data
	* @param InCallback Callback for the async task to call on the main thread
	* @param bInWasSuccessful Whether or not the get was successful
	*/
	FAsyncEventSessionGetChangeableSessionDataTaskCompleted(FOnlineSubsystemPS4* InPS4Subsystem, TSharedPtr<const FUniqueNetIdPS4> InUserId, const FString& InSessionId, const FString & InChangeableSessionData,
			const FOnlineAsyncTaskPS4SessionGetChangeableSessionData::FOnSessionGetChangeableSessionDataComplete & InCallback, bool bInWasSuccessful) :
		FOnlineAsyncEvent(InPS4Subsystem),
		PS4Subsystem(InPS4Subsystem),		
		UserId(InUserId),
		SessionId(InSessionId),
		ChangeableSessionData(InChangeableSessionData),
		Callback(InCallback),
		bWasSuccessful(bInWasSuccessful)
	{
	}

	virtual FString ToString() const override
	{
		return TEXT("Session get changeable session data task complete");
	}

	virtual void TriggerDelegates() override;
};

class FOnlineAsyncTaskPS4LeaveRoom : public FOnlineAsyncTaskBasic<FOnlineSubsystemPS4>
{
public:
	FOnlineAsyncTaskPS4LeaveRoom(FOnlineSubsystemPS4* InSubsystem, FNamedOnlineSession* InSession, const FOnDestroySessionCompleteDelegate& InDestroySessionDelegate, SceNpMatching2RequestId InRequestId);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("LeaveRoom"); }
	virtual void TriggerDelegates() override;
	void ProcessCallbackResult(int InErrorCode);

	// FOnlineAsyncTask
	virtual void Tick() override;

private:
	FOnlineSubsystemPS4* PS4Subsystem;
	FNamedOnlineSession* Session;
	FOnDestroySessionCompleteDelegate DestroySessionDelegate;
	bool bInitialized;
	int ErrorCode;
	SceNpMatching2RequestId RequestId;
};

class FOnlineAsyncTaskPS4SendInvitation : public FOnlineAsyncTaskBasic<FOnlineSubsystemPS4>
{
public:

	FOnlineAsyncTaskPS4SendInvitation(FOnlineSubsystemPS4* InSubsystem, const FUniqueNetIdPS4& InUserId, const FNamedOnlineSession& InSession, const TArray< TSharedRef<const FUniqueNetId> >& InFriends);

	// FOnlineAsyncItem
	virtual FString ToString() const override { return TEXT("SendInvitation"); }

	// FOnlineAsyncTask
	virtual void Tick() override;

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4SendInvitation, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	FOnlineSubsystemPS4* PS4Subsystem;
	NpToolkit::Core::Response<NpToolkit::Core::Empty> Response;
};

enum class EMatching2ContextState
{
	Invalid,
	Starting,
	Started,
	Ending,
	Ended
};

struct Matching2ContextState
{
	Matching2ContextState(SceNpMatching2ContextId InContextId, EMatching2ContextState InState) :
		ContextId(InContextId),
		ContextState(InState)
	{}
	SceNpMatching2ContextId ContextId;
	EMatching2ContextState ContextState;
};

/**
 * Interface definition for the online services session services 
 * Session services are defined as anything related managing a session 
 * and its state within a platform service
 */
class FOnlineSessionPS4 : public IOnlineSession
{

public:
	//methods for grabbing the quicksearching data
	FName GetQuickmatchSearchingSessionName(){ return QuickmatchSearchingSessionName; }
	const FUniqueNetIdPS4& GetQuickmatchSearchingPlayerId() const { return *QuickmatchSearchingPlayerId; }

	static const int32 MAX_SESSION_DATA_LENGTH = 1024;

private:
	/**
	* Delegate fired when getting the session data has completed.
	*
	* @param UserId the unique net id of the user who made the request
	* @param SessionId the session id which we're getting data for
	* @param SessionData the data retrieved for the session
	* @param bWasSuccessful true if the async action completed without error, false if there was an error
	*/
	DEFINE_ONLINE_DELEGATE_FOUR_PARAM(OnGetSessionDataComplete, const FUniqueNetId&, TSharedPtr<FOnlineSessionSearch>, const FString&, bool);

	/**
	* Delegate fired when getting the session has completed.
	*
	* @param UserId the unique net id of the user who made the request
	* @param SessionId the session id which we're getting data for
	* @param SessionData the info retrieved for the session
	* @param bWasSuccessful true if the async action completed without error, false if there was an error
	*/
	DEFINE_ONLINE_DELEGATE_FOUR_PARAM(OnGetSessionComplete, const FUniqueNetId&, TSharedPtr<FOnlineSessionSearch>, const FString&, bool);

	friend class FMatching2CreateJoinRoomCompleted;
	friend class FAsyncGetRoomOwnerAddressTask;
	friend class FMatching2JoinRoomCompleted;

	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

	/** The server  */
	SceNpMatching2ServerId ServerId;

	//Quicksearching Data
	FName QuickmatchSearchingSessionName;
	TSharedPtr<FUniqueNetIdPS4 const> QuickmatchSearchingPlayerId;
	bool bUsingQuickmatch;	

	/** Request Id for last request */
	SceNpMatching2RequestId RequestId;

	SceNpMatching2WorldId WorldId;

	
	TMap<FName, int32> SessionSettingMapping;

	/** Hidden on purpose */
	FOnlineSessionPS4() = delete;

	/** helper to map session keys to SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_*_ID */
	int32 SessionSettingToExternalIntAttrID(const FName& SessionSetting);

	/**
	* Checks parental controls to see if chat is disabled for a user
	*
	* @param UserOnlineId of the player to look up
	*/
	bool IsChatDisabled(const FUniqueNetIdPS4& User);

	/**
	 * Registers all local players with the current session
	 *
	 * @param Session the session that they are registering in
	 */
	void RegisterLocalPlayers(FNamedOnlineSession* Session);

	/**
	 *	Join a lobby session, advertised on the PSN backend
	 * 
	 * @param PlayerNum local index of the user initiating the request
	 * @param Session newly allocated session with join information
	 * 
	 * @return ERROR_SUCCESS if successful, an error code otherwise
	 */
	uint32 JoinLobbySession(int32 PlayerNum, FNamedOnlineSession* Session, const FOnlineSession* SearchSession);

	/**
	 *	Find a game server session, given the RoomId
	 * 
	 * @param RoomId the PSN room id
	 * 
	 * @return the online session
	 */
	FNamedOnlineSession* GetRoomIdSession(SceNpMatching2RoomId RoomId);

	/**
	 *	Find a game server lobby, given the LobbyId
	 * 
	 * @param LobbyId the PSN lobby id
	 * 
	 * @return the online session
	 */
	FNamedOnlineSession* GetLobbyIdSession(SceNpMatching2LobbyId LobbyId);

	/**
	 *	Find the first session in state creating
	 * 
	 * @return the online session
	 */
	FNamedOnlineSession* GetCreatingSession();

	/**
	 *	Find a game server session and remove it, given the RoomId
	 * 
	 * @param RoomId the PSN room id
	 */
	void RemoveRoomIdSession(SceNpMatching2RoomId RoomId);

	/**
	 *	Find a game server lobby and remove it, given the LobbyId
	 * 
	 * @param LobbyId the PSN lobby id
	 */
	void RemoveLobbyIdSession(SceNpMatching2LobbyId LobbyId);	

	void RegisterVoice(const FUniqueNetId& PlayerId);
	void UnregisterVoice(const FUniqueNetId& PlayerId);

	typedef FAsyncTask<FWebApiPS4Task> BackgroundWebApiTask;

PACKAGE_SCOPE:
	/** Critical sections for thread safe operation of session lists */
	mutable FCriticalSection SessionLock;

	/** Current session settings */
	TArray<FNamedOnlineSession> Sessions;

	FCriticalSection Matching2ContextMutex;

	/** The mapping of users to Matching2 contexts */
	TMap<SceUserServiceUserId, Matching2ContextState> UserToWebApiContexts;

	/** Current search object */
	TSharedPtr<FOnlineSessionSearch> CurrentSessionSearch;

	/** Delegate for handling an accepted invite */
	FOnSessionUserInviteAcceptedDelegate OnSessionUserInviteAcceptedDelegate;

	FOnlineSessionPS4(class FOnlineSubsystemPS4* InSubsystem) :
		PS4Subsystem(InSubsystem),
		ServerId(SCE_NP_MATCHING2_INVALID_SERVER_ID),		
		RequestId(SCE_NP_MATCHING2_INVALID_REQUEST_ID),
		WorldId(SCE_NP_MATCHING2_INVALID_WORLD_ID),
		CurrentSessionSearch(NULL)
	{}

	/**
	 * Initialize the Sessions
	 *
	 * @return true if the Online Session could be initialised
	 */
	bool Init();

	/**
	 * Finalize the Sessions
	 *
	 * @return true if the Online Session could be initialised
	 */
	void Finalize();

	/** 
	 * Hack to start context creation and world discovery early.
	 * This will be replaced with a more correct Create/Join Session flow for multiple players.
	 *
	 * @returns boolean indicating success or failure
	 */	
	bool CreateInitialContext();

	SceNpMatching2ContextId GetUserMatching2Context(const FUniqueNetIdPS4& UserNetId, bool bCreateIfUnavailable);	
	EMatching2ContextState GetUserMatching2ContextState(const FUniqueNetIdPS4& UserNetId);
	EMatching2ContextState GetUserMatching2ContextState(SceNpMatching2ContextId ContextId);
	void SetUserMatching2ContextState(const FUniqueNetIdPS4& UserNetId, EMatching2ContextState State);
	void SetUserMatching2ContextState(SceNpMatching2ContextId ContextId, EMatching2ContextState State);

	/** 
	 * Destroy valid contexts
	 *
	 * @returns boolean indicating success or failure
	 */
	bool DoDestroyContext(const FUniqueNetIdPS4& PlayerId);
	bool DoDestroyContext(SceNpMatching2ContextId ContextId);

	/** 
	 * Stops a created context
	 *
	 * @returns boolean indicating success or failure
	 */
	bool DoStopContext(const FUniqueNetIdPS4& PlayerId);

	void DoStopAllContexts();
	void DoDestroyAllContexts();

	/** 
	 * Create context and register context callbacks for accessing the NP Matching2 library
	 *
	 * @returns boolean indicating success or failure
	 */
	bool DoGetWorldInfoList(SceNpMatching2ContextId Matching2Context);

	/** 
	 * Create a new room in the world and join it
	 *
	 * @returns boolean indicating success or failure
	 */
	bool DoCreateJoinRoom(FNamedOnlineSession* Session, SceNpMatching2ContextId UserMatching2Context, FOnlineAsyncTaskPS4SessionPutChangeableSessionData* PutDataTask);

	/** 
	 * Search for a room
	 *
	 * @param SearchSettings for performing the search of sessions
	 * @returns boolean indicating success or failure
	 */
	bool DoSearchRooms(const TSharedRef<FOnlineSessionSearch>& SearchSettings, const FUniqueNetIdPS4& SearchingPlayerId);

	/**
	 * Join a given room
	 * 
	 * @param JoiningPlayerId NetId of the user initiating the request
	 * @param Session newly allocated session with join information
	 * @param SearchSession the desired session to join
	 * 
	 * @returns boolean indicating success or failure
	 */
	bool DoJoinRoom(const FUniqueNetIdPS4& JoiningPlayerId, SceNpMatching2ContextId UserMatching2Context, FNamedOnlineSession* Session, const FOnlineSession* SearchSession);
		
	/**
	 * Join a given lobby
	 * 
	 * @param PlayerNum local index of the user initiating the request
	 * @param Session newly allocated session with join information
	 * @param SearchSession the desired session to join
	 * 
	 * @returns boolean indicating success or failure
	 */
	bool DoJoinLobby(int32 PlayerNum, FNamedOnlineSession* Session, const FOnlineSession* SearchSession);

	/** 
	 * Leave a room
	 *
	 * @returns boolean indicating success or failure
	 */
	bool DoLeaveRoom(FNamedOnlineSession* Session, const FOnDestroySessionCompleteDelegate& CompletionDelegate);

	/** 
	 * Leave a lobby
	 *
	 * @returns boolean indicating success or failure
	 */
	bool DoLeaveLobby(FNamedOnlineSession* Session);

	/**
	 * Leave a session
	 *
	 * @returns boolean indicating success or failure
	 */
	bool DoLeaveSession(FNamedOnlineSession* Session, const FOnDestroySessionCompleteDelegate& CompletionDelegate);

	/**
	* Destroy a session if it is owner bound and we are the host, otherwise leave it
	*
	* @returns boolean indicating success or failure
	*/
	bool DoLeaveOrDestroySession(FNamedOnlineSession* Session, const FOnDestroySessionCompleteDelegate& CompletionDelegate);
	
	/** 
	 * Ping a searchresult
	 *
	 * @param SearchSettings for performing the search of sessions
	 * @param RoomId to be pinged
	 * @returns boolean indicating success or failure
	 */
	bool DoPingSearchResults(const FUniqueNetIdPS4& SearchingPlayerId, FOnlineSessionSearch & SearchSettings);

	void OnSessionCreateComplete(const SessionCreateTaskData & InData);
	void OnSessionDestroyComplete(FName SessionName, bool bWasSuccessful, const FOnDestroySessionCompleteDelegate& CompletionDelegate);
	void OnSessionJoinComplete(FName SessionName, const FOnlineSessionSearchResult& DesiredSession, EOnJoinSessionCompleteResult::Type Result);
	//void OnSessionLeaveComplete(FName SessionName, bool bWasSuccessful);
	void OnSessionInviteAccepted(const FString& LocalUserIdStr, const FString& SessionId);
	void OnRoomLeaveComplete(FName SessionName, bool bWasSuccessful, const FOnDestroySessionCompleteDelegate& CompletionDelegate);
	void HandleGetChangeableDataComplete(int32 LocalUserNum, const FString& SessionId, const FString& ChangeableSessionData, bool bWasSuccessful);
	void HandleGetChangeableDataCompleteRoomsDisabled(int32 LocalUserNum, const FString& SessionId, const FString& ChangeableSessionData, bool bWasSuccessful);
	void HandleGetChangeableDataCompleteFindSessionById(int32 LocalUserNum, const FString& SessionId, const FString& ChangeableSessionData, bool bWasSuccessful, FOnSingleSessionResultCompleteDelegate CompletionDelegates);
	void OnGetSessionCompleted(const FUniqueNetId& UserId, TSharedPtr<FOnlineSessionSearch> OnlineSessionSearch, const FString& SessionData, bool bWasSuccessful, FOnlineAsyncTaskPS4SessionGetChangeableSessionData::FOnSessionGetChangeableSessionDataComplete GetChangeableSessionDataCompleteDelegate);
	void OnGetSessionDataCompleted(const FUniqueNetId& UserId, TSharedPtr<FOnlineSessionSearch> OnlineSessionSearch, const FString& SessionData, bool bWasSuccessful);
	void OnGetSessionDataCompletedFindSession(const FUniqueNetId& UserId, TSharedPtr<FOnlineSessionSearch> OnlineSessionSearch, const FString& SessionData, bool bWasSuccessful, FOnSingleSessionResultCompleteDelegate CompletionDelegates);

public:

	void PS4ContextProcessing(SceNpMatching2ContextId CtxId, SceNpMatching2Event Event, SceNpMatching2EventCause EventCause, int ErrorCode, void *Arg);

	void PS4RequestProcessing(SceNpMatching2ContextId CtxId, SceNpMatching2RequestId ReqId, SceNpMatching2Event Event, int ErrorCode, const void *Data, void *Arg);

	void PS4SignalingProcessing(SceNpMatching2ContextId CtxId, SceNpMatching2RoomId RoomId, SceNpMatching2RoomMemberId PeerMemberId, SceNpMatching2Event Event, int ErrorCode, void *Arg);

	void PS4RoomEventProcessing(SceNpMatching2ContextId CtxId, SceNpMatching2RoomId RoomId, SceNpMatching2Event Event, const void *Data, void *Arg);

	void PS4LobbyEventProcessing(SceNpMatching2ContextId CtxId, SceNpMatching2LobbyId LobbyId, SceNpMatching2Event Event, const void *Data, void *Arg);

	virtual ~FOnlineSessionPS4() {Finalize();}

	//~ Begin IOnlineSession Interface
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSessionSettings& SessionSettings) override
	{
		FScopeLock ScopeLock(&SessionLock);
		return new (Sessions) FNamedOnlineSession(SessionName, SessionSettings);
	}
	class FNamedOnlineSession* AddNamedSession(FName SessionName, const FOnlineSession& Session) override
	{
		FScopeLock ScopeLock(&SessionLock);
		return new (Sessions) FNamedOnlineSession(SessionName, Session);
	}

	virtual FNamedOnlineSession* GetNamedSession(FName SessionName) override;

	//ONLY EVER CALL ON A FUNCTION GUARANTEED TO BE ON THE MAIN THREAD.
	virtual void RemoveNamedSession(FName SessionName) override;
	virtual EOnlineSessionState::Type GetSessionState(FName SessionName) const override;
	virtual bool HasPresenceSession() override;
	virtual bool CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings) override;
	virtual bool StartSession(FName SessionName) override;
	virtual bool UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData = false) override;
	virtual bool EndSession(FName SessionName) override;
	virtual bool DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate = FOnDestroySessionCompleteDelegate()) override;
	virtual bool IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId) override;
	virtual bool StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName) override;
	virtual bool CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName) override;
	virtual bool FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings) override;
	virtual bool FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegate) override;
	virtual bool CancelFindSessions() override;
	virtual bool PingSearchResults(const FOnlineSessionSearchResult& SearchResult) override;
	virtual bool JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession) override;
	virtual bool FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend) override;
	virtual bool FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList) override;
	virtual bool SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend) override;
	virtual bool SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
	virtual bool SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends) override;
	virtual bool GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType) override;
	virtual bool GetResolvedConnectString(const class FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo) override;
	virtual FOnlineSessionSettings* GetSessionSettings(FName SessionName) override;
	virtual bool RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited) override;
	virtual bool RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited = false) override;
	virtual bool UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId) override;
	virtual bool UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players) override;
	virtual void RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual void UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate) override;
	virtual int32 GetNumSessions() override;
	virtual void DumpSessionState() override;
	//~ End IOnlineSession Interface
};
