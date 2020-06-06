// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4Package.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineAsyncTaskManager.h"
#include "WebApiPS4Task.h"

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

class FOnlineAsyncTaskPS4_PutChangeableSessionData;

DECLARE_DELEGATE_FourParams(FOnGetSessionComplete, const FUniqueNetId& /*UserId*/, TSharedPtr<FOnlineSessionSearch> /*SessionInfo*/, const FString& /*SessionId*/, bool /*bWasSuccessful*/);
DECLARE_DELEGATE_FourParams(FOnGetChangeableSessionDataComplete, int32, const FString &, const FString &, bool);

class FStaticPS4SessionCallbacks
{
public:
	static void HandleRequestEvent(SceNpMatching2ContextId CtxId, SceNpMatching2RequestId ReqId, SceNpMatching2Event Event, int32 ErrorCode, const void* Data, void* Arg);
	static void HandleContextEvent(SceNpMatching2ContextId CtxId, SceNpMatching2Event Event, SceNpMatching2EventCause EventCause, int32 ErrorCode, void* Arg);
	static void HandleRoomEvent(SceNpMatching2ContextId CtxId, SceNpMatching2RoomId RoomId, SceNpMatching2Event Event, const void* Data, void* Arg);
	static void HandleLobbyEvent(SceNpMatching2ContextId CtxId, SceNpMatching2LobbyId LobbyId, SceNpMatching2Event Event, const void* Data, void* Arg);
	static void HandleSignalingEvent(SceNpMatching2ContextId CtxId, SceNpMatching2RoomId RoomId, SceNpMatching2RoomMemberId PeerMemberId, SceNpMatching2Event Event, int32 ErrorCode, void* Arg);
};

/**
 * Interface definition for the online services session services 
 * Session services are defined as anything related managing a session 
 * and its state within a platform service
 */
class FOnlineSessionPS4 : public IOnlineSession
{

public:
	/**
	* Delegate fired when getting the session data has completed.
	*
	* @param UserId the unique net id of the user who made the request
	* @param SessionId the session id which we're getting data for
	* @param SessionData the data retrieved for the session
	* @param bWasSuccessful true if the async action completed without error, false if there was an error
	*/
	//DEFINE_ONLINE_DELEGATE_FOUR_PARAM(OnGetSessionDataComplete, const FUniqueNetId&, TSharedPtr<FOnlineSessionSearch>, const FString&, bool);

	/**
	* Delegate fired when getting the session has completed.
	*
	* @param UserId the unique net id of the user who made the request
	* @param SessionId the session id which we're getting data for
	* @param SessionData the info retrieved for the session
	* @param bWasSuccessful true if the async action completed without error, false if there was an error
	*/
	//DEFINE_ONLINE_DELEGATE_FOUR_PARAM(OnGetSessionComplete, const FUniqueNetId&, TSharedPtr<FOnlineSessionSearch>, const FString&, bool);

	//methods for grabbing the quicksearching data
	FName GetQuickmatchSearchingSessionName(){ return QuickmatchSearchingSessionName; }
	const FUniqueNetIdPS4& GetQuickmatchSearchingPlayerId() const { return *QuickmatchSearchingPlayerId; }

	static const int32 MAX_SESSION_DATA_LENGTH = 1024;

PACKAGE_SCOPE:
	static int32 CopyCustomSettingDataToBuffer(const FOnlineSessionSettings& SessionSettings, char* Buffer);

	/**
	 * Checks parental controls to see if chat is disabled for a user
	 *
	 * @param UserOnlineId of the player to look up
	 */
	bool IsChatDisabled(const FUniqueNetIdPS4& User);

private:
	friend class FMatching2CreateJoinRoomCompleted;
	friend class FAsyncGetRoomOwnerAddressTask;
	friend class FMatching2JoinRoomCompleted;

	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

	/** The server  */
	SceNpMatching2ServerId ServerId;

	//Quicksearching Data
	FName QuickmatchSearchingSessionName;
	TSharedPtr<const FUniqueNetIdPS4> QuickmatchSearchingPlayerId;
	bool bUsingQuickmatch;	

	/** Request Id for last request */
	SceNpMatching2RequestId RequestId;

	SceNpMatching2WorldId WorldId;

	
	TMap<FName, int32> SessionSettingMapping;

	/** Queued Session delegates to be called when a session is finished being deleted */
	TMap<FName, TArray<FOnDestroySessionCompleteDelegate> > SessionDeleteDelegates;

	/** Hidden on purpose */
	FOnlineSessionPS4() = delete;

	/** helper to map session keys to SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_*_ID */
	int32 SessionSettingToExternalIntAttrID(const FName& SessionSetting);

	/**
	 * Registers all local players with the current session
	 *
	 * @param Session the session that they are registering in
	 */
	void RegisterLocalPlayers(FNamedOnlineSession* Session);

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
	FNamedOnlineSession* GetSessionById(const FString& SessionId);

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
	TSparseArray<FNamedOnlineSession> Sessions;

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
	bool DoCreateJoinRoom(FNamedOnlineSession* Session, SceNpMatching2ContextId UserMatching2Context, FOnlineAsyncTaskPS4_PutChangeableSessionData* PutDataTask);

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
	bool DoLeaveRoom(FNamedOnlineSession& Session, const FOnDestroySessionCompleteDelegate& CompletionDelegate);

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
	bool DoLeaveOrDestroySession(FNamedOnlineSession& Session, const FOnDestroySessionCompleteDelegate& CompletionDelegate);
	
	/** 
	 * Ping a searchresult
	 *
	 * @param SearchSettings for performing the search of sessions
	 * @param RoomId to be pinged
	 * @returns boolean indicating success or failure
	 */
	bool DoPingSearchResults(const FUniqueNetIdPS4& SearchingPlayerId, FOnlineSessionSearch & SearchSettings);

	void OnSessionCreateComplete(const struct SessionCreateTaskData& InData);
	void OnSessionDestroyComplete(FName SessionName, bool bWasSuccessful, const FOnDestroySessionCompleteDelegate& CompletionDelegate);
	void OnSessionJoinComplete(FName SessionName, const FOnlineSessionSearchResult& DesiredSession, EOnJoinSessionCompleteResult::Type Result);
	//void OnSessionLeaveComplete(FName SessionName, bool bWasSuccessful);
	void OnSessionInviteAccepted(const FString& LocalUserIdStr, const FString& SessionId);
	void OnRoomLeaveComplete(FName SessionName, bool bWasSuccessful, const FOnDestroySessionCompleteDelegate& CompletionDelegate);
	void HandleGetChangeableDataCompleteSessionInviteRoomsEnabled(int32 LocalUserNum, const FString& SessionId, const FString& ChangeableSessionData, bool bWasSuccessful);
	void HandleGetChangeableDataCompleteSessionInviteRoomsDisabled(int32 LocalUserNum, const FString& SessionId, const FString& ChangeableSessionData, bool bWasSuccessful);
	void HandleGetChangeableDataCompleteFindSessionById(int32 LocalUserNum, const FString& SessionId, const FString& ChangeableSessionData, bool bWasSuccessful, FOnSingleSessionResultCompleteDelegate CompletionDelegates);
	void OnGetSessionCompleted(const FUniqueNetId& UserId, TSharedPtr<FOnlineSessionSearch> OnlineSessionSearch, const FString& SessionData, bool bWasSuccessful, FOnGetChangeableSessionDataComplete GetChangeableSessionDataCompleteDelegate);
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

	virtual TSharedPtr<const FUniqueNetId> CreateSessionIdFromString(const FString& SessionIdStr) override;
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
