// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4Package.h"

/** Forward declarations of all interface classes */
typedef TSharedPtr<class FOnlineSessionPS4, ESPMode::ThreadSafe> FOnlineSessionPS4Ptr;
typedef TSharedPtr<class FOnlineFriendsPS4, ESPMode::ThreadSafe> FOnlineFriendsPS4Ptr;
typedef TSharedPtr<class FMessageSanitizerPS4, ESPMode::ThreadSafe> FMessageSanitizerPS4Ptr;
typedef TSharedPtr<class FOnlineIdentityPS4, ESPMode::ThreadSafe> FOnlineIdentityPS4Ptr;
typedef TSharedPtr<class FOnlineAchievementsPS4, ESPMode::ThreadSafe> FOnlineAchievementsPS4Ptr;
typedef TSharedPtr<class FOnlinePurchasePS4, ESPMode::ThreadSafe> FOnlinePurchasePS4Ptr;
typedef TSharedPtr<class FOnlineStorePS4, ESPMode::ThreadSafe> FOnlineStorePS4Ptr;
typedef TSharedPtr<class FOnlineUserCloudPS4, ESPMode::ThreadSafe> FOnlineUserCloudPS4Ptr;
typedef TSharedPtr<class FOnlineLeaderboardsPS4, ESPMode::ThreadSafe> FOnlineLeaderboardsPS4Ptr;
typedef TSharedPtr<class FOnlineUserPS4, ESPMode::ThreadSafe> FOnlineUserPS4Ptr;
typedef TSharedPtr<class FOnlineTitleFilePS4, ESPMode::ThreadSafe> FOnlineTitleFilePS4Ptr;
typedef TSharedPtr<class FOnlineExternalUIPS4, ESPMode::ThreadSafe> FOnlineExternalUIPS4Ptr;
typedef TSharedPtr<class FOnlineSharingPS4, ESPMode::ThreadSafe> FOnlineSharingPS4Ptr;
typedef TSharedPtr<class FOnlineMessagePS4, ESPMode::ThreadSafe> FOnlineMessagePS4Ptr;
typedef TSharedPtr<class FOnlineVoicePS4, ESPMode::ThreadSafe> FOnlineVoicePS4Ptr;
typedef TSharedPtr<class FOnlinePresencePS4, ESPMode::ThreadSafe> FOnlinePresencePS4Ptr;

class FUniqueNetIdPS4;

/**
 * Delegate used as the callback for online/account id resolution.
 *
 * @param ResolvedIds The map containing the resolved online and account ids.
 * @param bWasSuccessful True when no errors occurred
 * @param ErrorString Error message, valid when bWasSuccessful is false.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnIdResolveComplete, FOnlineIdMap /*ResolvedIds*/, bool /*bWasSuccessful*/, FString /*ErrorString*/);

/**
 *	OnlineSubsystemPS4 - Implementation of the online subsystem for PS4 services
 */
class ONLINESUBSYSTEMPS4_API FOnlineSubsystemPS4 :
	public FOnlineSubsystemImpl
{
protected:

	/** NP Licensee Id identifying the organization owning the title */
	FString NpLicenseeId;

	/** NP Title Id for the running game */
	SceNpTitleId NpTitleId;

	/** NP Title Secret for the running game */
	FString NpTitleSecretString;

	/** NP Client Id for the running game */
	SceNpClientId NpClientId;

	/** Interface to the session services */
	FOnlineSessionPS4Ptr SessionInterface;

	/** Interface to the profile services */
	FOnlineIdentityPS4Ptr IdentityInterface;

	/** Interface to the friends services */
	FOnlineFriendsPS4Ptr FriendsInterface;

	/** Interface to the message sanitizer */
	FMessageSanitizerPS4Ptr MessageSanitizer;

	/** Interface to the trophy services */
	FOnlineAchievementsPS4Ptr AchievementsInterface;

	/** Interface to purchasing */
	FOnlinePurchasePS4Ptr PurchaseInterface;

	/** Interface to storefront */
	FOnlineStorePS4Ptr StoreInterface;

	/** Interface to the user cloud services */
	FOnlineUserCloudPS4Ptr UserCloudInterface;

	/** Interface to the leaderboards services */
	FOnlineLeaderboardsPS4Ptr LeaderboardsInterface;

	/** Interface for user info services on PS4 */ 
	FOnlineUserPS4Ptr UserInterface;

	/** Interface to the title file services */
	FOnlineTitleFilePS4Ptr TitleFileInterface;

	/** Interface for external UI services on PS4 */
	FOnlineExternalUIPS4Ptr ExternalUIInterface;

	/** Interface to the sharing services */
	FOnlineSharingPS4Ptr SharingInterface;

	/** Interface to the online message services */
	FOnlineMessagePS4Ptr MessageInterface;

	/** Interface to the rich presence services */
	FOnlinePresencePS4Ptr PresenceInterface;

	/** Interface to the online voice services */
	FOnlineVoicePS4Ptr VoiceInterface;

	/** Online async task runnable */
	class FOnlineAsyncTaskManagerPS4* OnlineAsyncTaskThreadRunnable;

	/** Online async task thread */
	class FRunnableThread* OnlineAsyncTaskThread;

	/** Critical Section to guard access to the online id cache */
	FCriticalSection OnlineIdCacheCS;

	/** Map of cached PSN account OnlineIDs. */
	FOnlineIdMap OnlineIdMap;

	/** must pump sceNpNotifyPlusFeature() is multiplayer is being used by any user */
	int32 UserServiceIdsOnline[SCE_USER_SERVICE_MAX_LOGIN_USERS];

	/** true by default, this enables room creation. Can be disabled to use third party matchmaking. */
	bool bAreRoomsEnabled;
	/** true by default, toggle for achivements features */
	bool bAchievementsEnabled;
	/** true by default, toggle for user cloud features */
	bool bUserCloudEnabled;
	/** true by default, toggle for leaderboards features */
	bool bLeaderboardsEnabled;
	/** true by default, toggle for title file features */
	bool bTitleFileEnabled;
	/** true by default, toggle for sharing features */
	bool bSharingEnabled;
	/** true by default, toggle for messaging features */
	bool bMessagingEnabled;
	/** true by default, PSN Plus required for online play */
	bool bPSNPlusRequired;
	/** true by default, can disable the PSN store */
	bool bStoreEnabled;

	/** true if we initialized user service during init */
	bool bInitializedUserService;

	/** Handles for Net library */
	int32 NetLibraryMemoryId;
	int32 SslLibraryContextId;
	int32 HttpLibraryContext;
	int32 WebApiContext;

	/** IssuerId from last auth token request */
	NpToolkit::Auth::IssuerIdType IssuerId;

	/** The mapping of local users to user WebApi contexts */
	TMap<int32, int32> LocalUserToWebApiContexts;

	/** Delegate handle for the play together system event */
	FDelegateHandle OnPlayTogetherSystemServiceEventDelegateHandle;

	/** Set NP values required for this game, update for shipping game */
	bool InitNPGameSettings();	

	/** Initialize the NP toolkit */
	bool InitNpToolkit();

	/** Initialize the WebApi */
	bool InitWebApi();

	/** Finalize the WebApi */
	bool FinalizeWebApi();

PACKAGE_SCOPE:
	/** Only the factory makes instances */
	FOnlineSubsystemPS4()
		: SessionInterface(nullptr)
		, IdentityInterface(nullptr)
		, FriendsInterface(nullptr)
		, AchievementsInterface(nullptr)
		, PurchaseInterface(nullptr)
		, UserCloudInterface(nullptr)
		, LeaderboardsInterface(nullptr)
		, UserInterface(nullptr)
		, TitleFileInterface(nullptr)
		, ExternalUIInterface(nullptr)
		, SharingInterface(nullptr)
		, MessageInterface(nullptr)
		, VoiceInterface(nullptr)
		, OnlineAsyncTaskThreadRunnable(nullptr)
		, OnlineAsyncTaskThread(nullptr)
		, bAreRoomsEnabled(true)
		, bAchievementsEnabled(true)
		, bUserCloudEnabled(true)
		, bLeaderboardsEnabled(true)
		, bTitleFileEnabled(true)
		, bSharingEnabled(true)
		, bMessagingEnabled(true)
		, bPSNPlusRequired(true)
		, bStoreEnabled(true)
		, bInitializedUserService(false)
		, NetLibraryMemoryId(-1)
		, SslLibraryContextId(-1)
		, HttpLibraryContext(-1)
		, WebApiContext(-1)
		, IssuerId(NpToolkit::Auth::IssuerIdType::invalid)
	{}

	FOnlineSubsystemPS4(FName InInstanceName)
		: FOnlineSubsystemImpl(PS4_SUBSYSTEM, InInstanceName)
		, SessionInterface(nullptr)
		, IdentityInterface(nullptr)
		, FriendsInterface(nullptr)
		, AchievementsInterface(nullptr)
		, PurchaseInterface(nullptr)
		, UserCloudInterface(nullptr)
		, LeaderboardsInterface(nullptr)
		, UserInterface(nullptr)
		, TitleFileInterface(nullptr)
		, ExternalUIInterface(nullptr)
		, SharingInterface(nullptr)
		, MessageInterface(nullptr)
		, VoiceInterface(nullptr)
		, OnlineAsyncTaskThreadRunnable(nullptr)
		, OnlineAsyncTaskThread(nullptr)
		, bAreRoomsEnabled(true)
		, bAchievementsEnabled(true)
		, bUserCloudEnabled(true)
		, bLeaderboardsEnabled(true)
		, bTitleFileEnabled(true)
		, bSharingEnabled(true)
		, bMessagingEnabled(true)
		, bPSNPlusRequired(true)
		, bStoreEnabled(true)
		, bInitializedUserService(false)
		, NetLibraryMemoryId(-1)
		, SslLibraryContextId(-1)
		, HttpLibraryContext(-1)
		, WebApiContext(-1)
		, IssuerId(NpToolkit::Auth::IssuerIdType::invalid)
	{
	}

public:

	static const uint64 INVALID_OSSREQUESTID = 0;

	virtual ~FOnlineSubsystemPS4() {}

	// IOnlineSubsystem

	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IMessageSanitizerPtr GetMessageSanitizer(int32 LocalUserNum, FString& OutAuthTypeToExclude) const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineStorePtr GetStoreInterface() const override;
	virtual IOnlineStoreV2Ptr GetStoreV2Interface() const override;
	virtual IOnlinePurchasePtr GetPurchaseInterface() const override;
	virtual IOnlineEventsPtr GetEventsInterface() const override;
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineSharingPtr GetSharingInterface() const override;
	virtual IOnlineUserPtr GetUserInterface() const override;
	virtual IOnlineMessagePtr GetMessageInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override;
	virtual IOnlineChatPtr GetChatInterface() const override;
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override;
	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual EOnlineEnvironment::Type GetOnlineEnvironment() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	virtual FText GetOnlineServiceName() const override;

	// FTickerObjectBase

	virtual bool Tick(float DeltaTime) override;

	/**
	 * Get the SceNpTitleId for the running game/service
	 *
	 * @return SceNpTitleId registered for the title
	 */
	const SceNpTitleId& GetNpTitleId() const;

	/**
	 * Get the client id for making service requests
	 *
	 * @return SceNpClientId registered for the title
	 */
	const SceNpClientId& GetNpClientId() const;

	/**
	 * Get the licensee id identifying the organization this title belongs to
	 * 
	 * @return Sony issued Licensee ID
	 */
	const FString& GetNpLicenseeId() const;

	/** 
	 * Get a user WebApi context for the local user passed in
	 *
	 * @return WebAPI context for the local user
	 */
	int32 GetUserWebApiContext(const FUniqueNetIdPS4& NetUserIdPS4);

	/**
	 * Caches the mapping of account to online ID, without calling out to PSN services.
	 * Call this whenever a PSN API returns both the account ID and online ID of a user.
	 */
	void CacheOnlineId(SceNpAccountId AccountId, SceNpOnlineId OnlineId);

	/**
	 * Asynchronously resolves online IDs from the specified accounts IDs.
	 */
	void ResolveOnlineIdsAsync(FUniqueNetIdPS4 const& LocalUserId, TArray<SceNpAccountId> const& AccountIds, FOnIdResolveComplete const& Delegate);

	/**
	 * Asynchronously resolves account IDs from the specified online IDs.
	 */
	void ResolveAccountIdsAsync(FUniqueNetIdPS4 const& LocalUserId, TArray<SceNpOnlineId> OnlineIds, FOnIdResolveComplete const& Delegate);

	virtual void SetUsingMultiplayerFeatures(const FUniqueNetId& UniqueId, bool bUsingMP) override;
	
	/**
	* Checks if sessions are using PS4 rooms.
	*
	* @return true if rooms are enabled.
	*/
	bool AreRoomsEnabled() { return bAreRoomsEnabled; }

	/**
	 * @return true if PSN Plus is required for online play
	 */
	bool IsPSNPlusRequired() { return bPSNPlusRequired; }

PACKAGE_SCOPE:
	class FOnlineAsyncTaskManagerPS4 * GetAsyncTaskManager() { return OnlineAsyncTaskThreadRunnable; }

	/**
	 * Set the current issuerd id. Triggers FOnOnlineEnvironmentChanged if updated
	 * 
	 * @param NewIssuerId - new issuer id to update
	 */
	void SetIssuerId(NpToolkit::Auth::IssuerIdType NewIssuerId);

private:

	/** Delegate for handling the play together system event */
	void OnPlayTogetherEventReceived(const SceNpPlayTogetherHostEventParamA& EventParam);
};

typedef TSharedPtr<FOnlineSubsystemPS4, ESPMode::ThreadSafe> FOnlineSubsystemPS4Ptr;
