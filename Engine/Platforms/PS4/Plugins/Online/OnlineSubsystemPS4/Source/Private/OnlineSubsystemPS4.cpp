// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPS4.h"
#include "PlatformFeatures.h"
#include "OnlineSessionInterfacePS4.h"
#include "OnlineFriendsInterfacePS4.h"
#include "MessageSanitizerInterfacePS4.h"
#include "OnlineAchievementsInterfacePS4.h"
#include "OnlinePurchasePS4.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "OnlineLeaderboardsInterfacePS4.h"
#include "OnlineUserInterfacePS4.h"
#include "OnlineUserCloudInterfacePS4.h"
#include "OnlineTitleFileInterfacePS4.h"
#include "OnlineMessageInterfacePS4.h"
#include "OnlineExternalUIInterfacePS4.h"
#include "OnlineSharingInterfacePS4.h"
#include "OnlineVoiceInterfacePS4.h"
#include "OnlinePresenceInterfacePS4.h"
#include "OnlineStorePS4.h"
#include "OnlineTournamentInterfacePS4.h"
#include "HAL/RunnableThread.h"
#include "Misc/CommandLine.h"
#include "PS4Application.h"

#include "AsyncTasks/OnlineAsyncItemsPS4_Friends.h"
#include "AsyncTasks/OnlineAsyncItemsPS4_Presence.h"

#ifndef UE4_PROJECT_NPTITLEID
#	define UE4_PROJECT_NPTITLEID 0
#endif

#ifndef UE4_PROJECT_NPLICENSEEID
#	define UE4_PROJECT_NPLICENSEEID 0
#endif

#ifndef UE4_PROJECT_NPTITLESECRET
#	define UE4_PROJECT_NPTITLESECRET 0
#endif

#ifndef UE4_PROJECT_NPCLIENTID
#	define UE4_PROJECT_NPCLIENTID 0
#endif

#ifndef NULL_ID
#	define NULL_ID 0
#endif
         
class FOnlineEventPS4_LoginStatusChanged : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FOnlineEventPS4_LoginStatusChanged(FOnlineSubsystemPS4* PS4Subsystem, bool bInLoggedIn, SceUserServiceUserId InUserId)
		: FOnlineAsyncEvent(PS4Subsystem)
		, bIsLoggedIn(bInLoggedIn)
		, UserId(InUserId)
	{}

	FString ToString() const { return TEXT("FOnlineEventPS4_LoginStatusChanged"); }

	void TriggerDelegates()
	{
		if(FOnlineIdentityPS4* IdentityPS4 = (FOnlineIdentityPS4*)Subsystem->GetIdentityInterface().Get())
		{
			IdentityPS4->UserPlatformLoginEventHandler(bIsLoggedIn, UserId);
		}
	}

private:
	bool bIsLoggedIn = false;
	SceUserServiceUserId UserId;
};

class FOnlineEventPS4_ConnectionStatusChanged : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
public:
	FOnlineEventPS4_ConnectionStatusChanged(FOnlineSubsystemPS4* PS4Subsystem, bool bInConnected)
		: FOnlineAsyncEvent(PS4Subsystem)
		, bIsConnected(bInConnected)
	{}

	FString ToString() const { return TEXT("FOnlineEventPS4_ConnectionStatusChanged"); }

	void TriggerDelegates()
	{
		const EOnlineServerConnectionStatus::Type ConnectionStatus = bIsConnected ? EOnlineServerConnectionStatus::Connected : EOnlineServerConnectionStatus::ServiceUnavailable;
		Subsystem->TriggerOnConnectionStatusChangedDelegates(Subsystem->GetSubsystemName().ToString(), EOnlineServerConnectionStatus::Normal, ConnectionStatus);
	}

private:
	bool bIsConnected = false;
};

static EOnlineEnvironment::Type IssuerIdToEnvironment(NpToolkit::Auth::IssuerIdType IssuerId)
{
	switch (IssuerId)
	{
	case NpToolkit::Auth::IssuerIdType::development: // sp-int
		return EOnlineEnvironment::Development;

	case NpToolkit::Auth::IssuerIdType::certification: // prod-qa
		return EOnlineEnvironment::Certification;

	case NpToolkit::Auth::IssuerIdType::live: // np
		return EOnlineEnvironment::Production;
	}
	return EOnlineEnvironment::Unknown;
}

void HandleNpToolkitCallbackEvent(NpToolkit::Core::CallbackEvent* InEvent)
{
	UE_LOG_ONLINE(Log, TEXT("HandleNpToolkitCallbackEvent: service = %d, apiCalled = %d"), (int)InEvent->service, (int)InEvent->apiCalled);

	// Get the online subsystem from the app data pointer
	FOnlineSubsystemPS4* PS4Subsystem = reinterpret_cast<FOnlineSubsystemPS4*>(InEvent->appData);
	check(PS4Subsystem);

	// Some events require a fully initialized SubSystem
	const bool IsSubSystemInitialized = PS4Subsystem->IsFullyInitialized();

	FOnlineAsyncTaskManagerPS4* AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
	check(AsyncTaskManager);

	FOnlineAsyncItem* NewEvent = nullptr;
	FOnlineAsyncTask* NewTask = nullptr;
	switch (InEvent->apiCalled)
	{
	// Notifications about local user and PSN sign in/sign out
	case NpToolkit::Core::FunctionType::notificationUserStateChange:
		{
			const auto& UserStateChange = *reinterpret_cast<NpToolkit::Core::Response<NpToolkit::NpUtils::Notification::UserStateChange>*>(InEvent->response)->get();
			switch (UserStateChange.stateChanged)
			{
			// Notifications about PSN sign in, sign out...
			case NpToolkit::NpUtils::Notification::StateChanged::signedInState:
				{
					switch (UserStateChange.currentSignInState)
					{
					case NpToolkit::NpUtils::Notification::SignInState::signedIn:
						NewEvent = new FOnlineEventPS4_LoginStatusChanged(PS4Subsystem, true, UserStateChange.userId);
						break;

					case NpToolkit::NpUtils::Notification::SignInState::signedOut:
						NewEvent = new FOnlineEventPS4_LoginStatusChanged(PS4Subsystem, false, UserStateChange.userId);
						break;

					}
				}
				break;
			}
		}
		break;

	// Notifications about network connect/disconnect
	case NpToolkit::Core::FunctionType::notificationNetStateChange:
		{
			auto const& NetStateChange = *reinterpret_cast<NpToolkit::Core::Response<NpToolkit::NetworkUtils::Notification::NetStateChange>*>(InEvent->response)->get();
			switch (NetStateChange.event)
			{
			case NpToolkit::NetworkUtils::NetworkEvent::networkConnected:
				NewEvent = new FOnlineEventPS4_ConnectionStatusChanged(PS4Subsystem, true);
				break;

			case NpToolkit::NetworkUtils::NetworkEvent::networkDisconnected:
				NewEvent = new FOnlineEventPS4_ConnectionStatusChanged(PS4Subsystem, false);
				break;
			}
		}
		break;
	/*case NpToolkit::Core::FunctionType::notificationNewInGameMessage:
		break;
	case NpToolkit::Core::FunctionType::notificationNewInvitation:
		break;
	case NpToolkit::Core::FunctionType::notificationNewGameDataMessage:
		break;*/
	case NpToolkit::Core::FunctionType::notificationUpdateFriendsList:
		{
			//Early out as these events require a fully initialized PS4Subsystem 
			if (!IsSubSystemInitialized)
			{
				return;
			}

			auto const& FriendListUpdate = *reinterpret_cast<NpToolkit::Core::Response<NpToolkit::Friend::Notification::FriendlistUpdate>*>(InEvent->response)->get();
			switch (FriendListUpdate.eventType)
			{
			case NpToolkit::Friend::FriendlistUpdateEvent::friendAdded:
				NewTask = new FOnlineAsyncTaskPS4_FriendAdded(*PS4Subsystem, FUniqueNetIdPS4::FindOrCreate(FriendListUpdate.localUpdatedUser), FUniqueNetIdPS4::FindOrCreate(FriendListUpdate.remoteUser));
				break;
			case NpToolkit::Friend::FriendlistUpdateEvent::friendRemoved:
				NewEvent = new FOnlineEventPS4_FriendRemoved(*PS4Subsystem, FUniqueNetIdPS4::FindOrCreate(FriendListUpdate.localUpdatedUser), FUniqueNetIdPS4::FindOrCreate(FriendListUpdate.remoteUser));
				break;
			case NpToolkit::Friend::FriendlistUpdateEvent::friendOnlineStatusChanged:
				NewTask = new FOnlineAsyncTaskPS4_RefreshPresence(*PS4Subsystem, FUniqueNetIdPS4::FindOrCreate(FriendListUpdate.localUpdatedUser), FUniqueNetIdPS4::FindOrCreate(FriendListUpdate.remoteUser));
				break;
			}
		}
		break;
	case NpToolkit::Core::FunctionType::notificationUpdateFriendPresence:
		{
			//Early out as these events require a fully initialized PS4Subsystem 
			if (!IsSubSystemInitialized)
			{
				return;
			}
			const auto& PresenceUpdate = *reinterpret_cast<NpToolkit::Core::Response<NpToolkit::Presence::Notification::PresenceUpdate>*>(InEvent->response)->get();
			if (PresenceUpdate.platform == NpToolkit::Presence::PlatformPresenceType::ps4)
			{
				switch (PresenceUpdate.updateType)
				{
				case NpToolkit::Presence::Notification::PresenceUpdateType::gameTitle:
					NewTask = new FOnlineAsyncTaskPS4_RefreshPresence(*PS4Subsystem, FUniqueNetIdPS4::FindOrCreate(PresenceUpdate.localUpdatedUser), FUniqueNetIdPS4::FindOrCreate(PresenceUpdate.remoteUser));
					break;
				case NpToolkit::Presence::Notification::PresenceUpdateType::gameStatus:
					NewEvent = new FOnlineEventPS4_PresenceStatusChanged(*PS4Subsystem, FUniqueNetIdPS4::FindOrCreate(PresenceUpdate.remoteUser), FString(UTF8_TO_TCHAR(PresenceUpdate.gameStatus)));
					break;
				case NpToolkit::Presence::Notification::PresenceUpdateType::gameData:
					NewEvent = new FOnlineEventPS4_PresenceGameDataChanged(*PS4Subsystem, FUniqueNetIdPS4::FindOrCreate(PresenceUpdate.remoteUser), FString(PresenceUpdate.binaryGameDataSize, UTF8_TO_TCHAR(PresenceUpdate.binaryGameData)));
					break;
				}
				break;
			}
		}
		//@todo DanH: Support PSN block list
		//case NpToolkit::Core::FunctionType::notificationUpdateBlockedUsersList:
	//	{
	//		auto const& BlockListUpdate = *reinterpret_cast<NpToolkit::Core::Response<NpToolkit::Friend::Notification::BlocklistUpdate>*>(InEvent->response)->get();
	//		// We aren't told whether the indicated user was just blocked or unblocked, so we need to check the block list I guess and see if this user is on it? Or see if we have them cached as being blocked and go off that?
	//	}
	//	break;
	}

	if (NewTask)
	{
		AsyncTaskManager->AddToParallelTasks(NewTask);
	}
	if (NewEvent)
	{
		AsyncTaskManager->AddToOutQueue(NewEvent);
	}
}

static void* NPToolkit2_Allocate(size_t SizeInBytes)
{
	void* Ptr = ::operator new(SizeInBytes);

	if (!Ptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to allocate %d bytes for NPtoolkit2!"), SizeInBytes);
	}
	return Ptr;
}

static void NPToolkit2_Deallocate(void* Ptr)
{
	::operator delete(Ptr);
}

bool FOnlineSubsystemPS4::InitNPGameSettings()
{	
	FString NpTitleIdString = PREPROCESSOR_TO_STRING(UE4_PROJECT_NPTITLEID);
	if (NpTitleIdString == PREPROCESSOR_TO_STRING(NULL_ID))
	{
		// get the title settings
		TSharedPtr<const FJsonObject> Settings = IPlatformFeaturesModule::Get().GetTitleSettings();
		if (Settings.IsValid())
		{
			NpTitleIdString = Settings->GetStringField(TEXT("title_id"));
		}
	}
	FString NpTitleSecretStringTemp = PREPROCESSOR_TO_STRING(UE4_PROJECT_NPTITLESECRET);
	if (NpTitleSecretStringTemp == PREPROCESSOR_TO_STRING(NULL_ID))
	{
		// get the title settings
		TSharedPtr<const FJsonObject> Settings = IPlatformFeaturesModule::Get().GetTitleSettings();
		if (Settings.IsValid())
		{
			NpTitleSecretStringTemp = Settings->GetStringField(TEXT("title_secret"));
		}
	}
	FString NpClientIdString = PREPROCESSOR_TO_STRING(UE4_PROJECT_NPCLIENTID);
	if (NpClientIdString == PREPROCESSOR_TO_STRING(NULL_ID))
	{
		// get the title settings
		TSharedPtr<const FJsonObject> Settings = IPlatformFeaturesModule::Get().GetTitleSettings();
		if (Settings.IsValid())
		{
			NpClientIdString = Settings->GetStringField(TEXT("client_id"));
		}
	}

	FMemory::Memset(&NpTitleId, 0, sizeof(NpTitleId));
	FMemory::Memset(&NpClientId, 0, sizeof(NpClientId));
	
	bool bInitialized = NpTitleIdString.Len() > 1 && NpTitleSecretStringTemp.Len() > 1;
	if (bInitialized)
	{	
		strncpy(NpTitleId.id, TCHAR_TO_ANSI(*NpTitleIdString), NpTitleIdString.Len());
		NpTitleSecretString = NpTitleSecretStringTemp;
	}
	if (NpClientIdString.Len() > 1)
	{
		strncpy(NpClientId.id, TCHAR_TO_ANSI(*NpClientIdString), NpClientIdString.Len());
	}
	
	NpLicenseeId = PREPROCESSOR_TO_STRING(UE4_PROJECT_NPLICENSEEID);
	if (NpLicenseeId == PREPROCESSOR_TO_STRING(NULL_ID))
	{
		// get the title settings
		TSharedPtr<const FJsonObject> Settings = IPlatformFeaturesModule::Get().GetTitleSettings();
		if (Settings.IsValid())
		{
			NpLicenseeId = Settings->GetStringField(TEXT("licensee_id"));
		}
	}

	if (bInitialized)
	{
		UE_LOG(LogInit, Log, TEXT("PSN TitleId: %s"), *NpTitleIdString);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogInit, Log, TEXT("PSN TitleSecret: %s"), *NpTitleSecretString);
		UE_LOG(LogInit, Log, TEXT("PSN ClientId: %s"), *NpClientIdString);
		UE_LOG(LogInit, Log, TEXT("PSN LicenseeId: %s"), *NpLicenseeId);
	}
	else
	{
		UE_LOG(LogInit, VeryVerbose, TEXT("Failed to init NPGameSettings.  TitleId: %s TitleSecret: %s"), *NpTitleIdString, *NpTitleSecretStringTemp);
#endif // !UE_BUILD_SHIPPING
	}

	return bInitialized;
}

bool FOnlineSubsystemPS4::InitNpToolkit()
{
	int LoadNpToolkitDllReturnCode = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TOOLKIT2);
	if( LoadNpToolkitDllReturnCode < SCE_OK )
	{
		UE_LOG_ONLINE( Error, TEXT( "Failed to load NP Toolkit 2 dll : 0x%x" ), LoadNpToolkitDllReturnCode );
		return false;
	}

	int LoadNpUtilityDllReturnCode = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_UTILITY);
	if (LoadNpUtilityDllReturnCode < SCE_OK)
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to load NP utility dll : 0x%x"), LoadNpUtilityDllReturnCode);
		return false;
	}

	NpToolkit::Core::Request::InitParams InitParams;
	InitParams.npToolkitCallbackInfo.callback = HandleNpToolkitCallbackEvent;
	InitParams.npToolkitCallbackInfo.appData = this;
	InitParams.externalAlloc.allocate = NPToolkit2_Allocate;
	InitParams.externalAlloc.deallocate = NPToolkit2_Deallocate;

	// Indicate desired push notifications
	//InitParams.serverPushNotifications.newInGameMessage = true;
	//InitParams.serverPushNotifications.newGameDataMessage = true;
	//InitParams.serverPushNotifications.newInvitation = true;
	InitParams.serverPushNotifications.updateFriendPresence = true;
	InitParams.serverPushNotifications.updateFriendsList = true;
	//@todo DanH: Implement support for PS4 block list
	//InitParams.serverPushNotifications.updateBlockedUsersList = true;

	// Select the correct thread affinity for the NpToolkit2 library based 
	// on the available CPU cores, otherwise NpToolkit::Core::init() fails.
	switch (sceKernelGetCpumode())
	{
	case SCE_KERNEL_CPUMODE_6CPU:
		InitParams.threadProperties.affinity = SCE_KERNEL_CPUMASK_6CPU_ALL;
		break;

	case SCE_KERNEL_CPUMODE_7CPU_LOW:
	case SCE_KERNEL_CPUMODE_7CPU_NORMAL:
	default:
		InitParams.threadProperties.affinity = SCE_KERNEL_CPUMASK_7CPU_ALL;
		break;
	}

	// @todo: Set the memory usage and pool from values besides the defaults?
	InitParams.memPools.sslPoolSize = 4 * 1024 * 1024;

	// temp - use 6MB as we know 2MB crashes :(
	InitParams.memPools.jsonPoolSize = 6 * 1024 * 1024;

	// We do not use the in game message library so set the pool size to 0 to avoid an NpDebug warning.
	InitParams.memPools.inGameMessagePoolSize = 0;

	const int32 ToolkitInitReturnCode = NpToolkit::Core::init(InitParams);
	if (ToolkitInitReturnCode != SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to initialize ToolkitNp2 library : 0x%x"), ToolkitInitReturnCode);
		return false;
	}
	
	NpToolkit::NpUtils::Request::SetTitleIdForDevelopment SetTitleIdRequest;
	SetTitleIdRequest.async = false;
	SetTitleIdRequest.userId = SCE_USER_SERVICE_USER_ID_SYSTEM;
	SetTitleIdRequest.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
	SetTitleIdRequest.titleId = NpTitleId;
	FCStringAnsi::Strcpy(SetTitleIdRequest.titleSecretString, TCHAR_TO_ANSI(*NpTitleSecretString));
	SetTitleIdRequest.titleSecretStringSize = NpTitleSecretString.Len();

	NpToolkit::Core::Response<NpToolkit::Core::Empty> Response;
	int32 SetTitleIdReturnCode = NpToolkit::NpUtils::setTitleIdForDevelopment(SetTitleIdRequest, &Response);
	if (SetTitleIdReturnCode != SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to set NP title details : 0x%x"), SetTitleIdReturnCode);
		return false;
	}

	return true;
}

bool FOnlineSubsystemPS4::InitWebApi()
{
	const int32 POOLSIZE_NET = 16 * 1024;
	const int32 POOLSIZE_SSL = 256 * 1024;
	const int32 POOLSIZE_LIBHTTP = 256 * 1024;
	const int32 POOLSIZE_WEBAPI = 1024 * 1024;

	// Initialize the pool for PS4 net functionality
	int32 ReturnCode = sceNetPoolCreate("PS4_WebApi", POOLSIZE_NET, 0);
	if (ReturnCode < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to initialize WebApi. sceNetPoolCreate error: 0x%x"), ReturnCode);
		return false;
	}
	NetLibraryMemoryId = ReturnCode;

	// Initialze the pool for PS4 SSL functionality
	ReturnCode = sceSslInit(POOLSIZE_SSL);
	if (ReturnCode < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to initialize WebApi. sceSslInit error: 0x%x"), ReturnCode);
		return false;
	}
	SslLibraryContextId = ReturnCode;

	// Initialize the HTTP pool
	ReturnCode = sceHttpInit(NetLibraryMemoryId, SslLibraryContextId, POOLSIZE_LIBHTTP);
	if (ReturnCode < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to initialize WebApi. sceHttpInit error: 0x%x"), ReturnCode);
		return false;
	}
	HttpLibraryContext = ReturnCode;

	// Initialize the WebApi pool
	ReturnCode = sceNpWebApiInitialize(HttpLibraryContext, POOLSIZE_WEBAPI);
	if (ReturnCode < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to initialize WebApi. sceNpWebApiInitialize error: 0x%x"), ReturnCode);
		return false;
	}
	WebApiContext = ReturnCode;

	return true;
}

bool FOnlineSubsystemPS4::FinalizeWebApi()
{
	// Delete all our WebApi user contexts
	for (TMap<int32, int32>::TConstIterator It(LocalUserToWebApiContexts); It; ++It)
	{
		const int32 UserWebApiContext = It.Value();
		int32 ReturnCode = sceNpWebApiDeleteContext(UserWebApiContext);
		if (ReturnCode < 0)
		{
			UE_LOG_ONLINE(Error, TEXT("sceNpWebApiDeleteContext() failed. error = 0x%x\n"), ReturnCode);
		}
	}

	// Terminate the WebAPI 
	if (WebApiContext >= 0)
	{
		int32 ReturnCode = sceNpWebApiTerminate(WebApiContext);
		if (ReturnCode < 0)
		{
			UE_LOG_ONLINE(Error, TEXT("sceNpWebApiTerminate() failed. error = 0x%x\n"), ReturnCode);
		}
	}
	WebApiContext = -1;

	// Close out Http
	if (HttpLibraryContext >= 1)
	{
		int ReturnCode = sceHttpTerm(HttpLibraryContext);
		if (ReturnCode != SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("sceHttpTerm() failed. error = 0x%x"), ReturnCode);
		}
	}
	HttpLibraryContext = -1;

	// Close out Ssl
	if (SslLibraryContextId >= 1)
	{
		int32 ReturnCode = sceSslTerm(SslLibraryContextId);
		if (ReturnCode != SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("sceSslTerm() failed. error = 0x%x\n"), ReturnCode);
		}
	}
	SslLibraryContextId = -1;

	// Close out the net library
	if (NetLibraryMemoryId >= 0)
	{
		int32 ReturnCode = sceNetPoolDestroy(NetLibraryMemoryId);
		if (ReturnCode != SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("sceNetPoolDestroy() failed. error = 0x%x\n"), ReturnCode);
		}
	}
	NetLibraryMemoryId = -1;

	return true;
}

IOnlineSessionPtr FOnlineSubsystemPS4::GetSessionInterface() const
{
	return SessionInterface;
}

IOnlineFriendsPtr FOnlineSubsystemPS4::GetFriendsInterface() const
{
	return FriendsInterface;
}

IMessageSanitizerPtr FOnlineSubsystemPS4::GetMessageSanitizer(int32 LocalUserNum, FString& OutAuthTypeToExclude) const
{
	TSharedPtr<const FUniqueNetIdPS4> UserId = FUniqueNetIdPS4::Cast(IdentityInterface->GetUniquePlayerId(LocalUserNum));
	if (UserId.IsValid())
	{
		uint32 PrivilegeFailures = (uint32)IOnlineIdentity::EPrivilegeResults::NoFailures;
		if (IdentityInterface->GetCachedUserPrivilege(*UserId.Get(), EUserPrivileges::CanUseUserGeneratedContent, PrivilegeFailures) &&
			(PrivilegeFailures & (uint32)IOnlineIdentity::EPrivilegeResults::UGCRestriction) != 0)
		{
			OutAuthTypeToExclude = IdentityInterface->GetAuthType();
			return MessageSanitizer;
		}
	}
	return nullptr;
}

IOnlinePartyPtr FOnlineSubsystemPS4::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemPS4::GetGroupsInterface() const
{
	return nullptr;
}
IOnlineSharedCloudPtr FOnlineSubsystemPS4::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemPS4::GetUserCloudInterface() const
{
	return UserCloudInterface;
}

IOnlineEntitlementsPtr FOnlineSubsystemPS4::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineEventsPtr FOnlineSubsystemPS4::GetEventsInterface() const
{
	return nullptr;
}

IOnlineStoreV2Ptr FOnlineSubsystemPS4::GetStoreV2Interface() const
{
	return StoreInterface;
}

IOnlinePurchasePtr FOnlineSubsystemPS4::GetPurchaseInterface() const
{
	return PurchaseInterface;
}

IOnlineAchievementsPtr FOnlineSubsystemPS4::GetAchievementsInterface() const
{
	return AchievementsInterface;
}

IOnlineLeaderboardsPtr FOnlineSubsystemPS4::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemPS4::GetVoiceInterface() const
{
	return VoiceInterface;
}

IOnlineExternalUIPtr FOnlineSubsystemPS4::GetExternalUIInterface() const
{
	return ExternalUIInterface;
}

IOnlineTimePtr FOnlineSubsystemPS4::GetTimeInterface() const
{
	return nullptr;
}

IOnlineIdentityPtr FOnlineSubsystemPS4::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineTitleFilePtr FOnlineSubsystemPS4::GetTitleFileInterface() const
{
	return TitleFileInterface;
}

IOnlineSharingPtr FOnlineSubsystemPS4::GetSharingInterface() const
{
	return SharingInterface;
}

IOnlineUserPtr FOnlineSubsystemPS4::GetUserInterface() const
{
	return UserInterface;
}

IOnlineMessagePtr FOnlineSubsystemPS4::GetMessageInterface() const
{
	return MessageInterface;
}

IOnlinePresencePtr FOnlineSubsystemPS4::GetPresenceInterface() const
{
	return PresenceInterface;
}

IOnlineChatPtr FOnlineSubsystemPS4::GetChatInterface() const
{
	return nullptr;
}

IOnlineStatsPtr FOnlineSubsystemPS4::GetStatsInterface() const
{
	return nullptr;
}

IOnlineTurnBasedPtr FOnlineSubsystemPS4::GetTurnBasedInterface() const
{
	return nullptr;
}

IOnlineTournamentPtr FOnlineSubsystemPS4::GetTournamentInterface() const
{
	return TournamentInterface;
}

bool FOnlineSubsystemPS4::Init()
{
#if UE_BUILD_SHIPPING
	// default to production for shipping builds
	IssuerId = NpToolkit::Auth::IssuerIdType::live;
#else
	IssuerId = NpToolkit::Auth::IssuerIdType::development;
#endif
	UE_LOG_ONLINE(Log, TEXT("Online Environment initialized to %s"), EOnlineEnvironment::ToString(IssuerIdToEnvironment(IssuerId)));

	bool bSuccessfullyStartedUp = true;

	//Ensure the http module has been loaded
	FModuleManager::Get().LoadModule(TEXT("HTTP"));

	for (int i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++i)
	{
		UserServiceIdsOnline[i] = SCE_USER_SERVICE_USER_ID_INVALID;
	}

	GConfig->GetBool(TEXT("OnlineSubsystemPS4"), TEXT("bAreRoomsEnabled"), bAreRoomsEnabled, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystemPS4"), TEXT("bAchievementsEnabled"), bAchievementsEnabled, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystemPS4"), TEXT("bUserCloudEnabled"), bUserCloudEnabled, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystemPS4"), TEXT("bLeaderboardsEnabled"), bLeaderboardsEnabled, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystemPS4"), TEXT("bTitleFileEnabled"), bTitleFileEnabled, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystemPS4"), TEXT("bSharingEnabled"), bSharingEnabled, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystemPS4"), TEXT("bMessagingEnabled"), bMessagingEnabled, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystemPS4"), TEXT("bTournamentsEnabled"), bTournamentsEnabled, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystemPS4"), TEXT("bPSNPlusRequired"), bPSNPlusRequired, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystemPS4"), TEXT("bStoreEnabled"), bStoreEnabled, GEngineIni);
	GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bHasVoiceEnabled"), bHasVoiceEnabled, GEngineIni);

	TSharedPtr<const FJsonObject> Settings = IPlatformFeaturesModule::Get().GetTitleSettings();
	if (Settings.IsValid())
	{
		Settings->TryGetBoolField(TEXT("allow_achievements"), bAchievementsEnabled);
		Settings->TryGetBoolField(TEXT("allow_mtx"), bStoreEnabled);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("NoAchievements")))
	{
		bAchievementsEnabled = false;
	}

	//Set NP Title ID
	//The NP Title ID and NP Title Secret are issued per application after registration on the PlayStationÂ®4 Developer Network (https://ps4.scedev.net/)
	if (InitNPGameSettings())
	{
		// Initialize the user service
		SceUserServiceInitializeParams UserServiceInitializeParameters;
		FMemory::Memset(&UserServiceInitializeParameters, 0, sizeof(UserServiceInitializeParameters));
		UserServiceInitializeParameters.priority = SCE_KERNEL_PRIO_FIFO_DEFAULT;

		int UserServerInitializeReturnCode = sceUserServiceInitialize(&UserServiceInitializeParameters);
		if (UserServerInitializeReturnCode < 0 &&
			UserServerInitializeReturnCode != SCE_USER_SERVICE_ERROR_ALREADY_INITIALIZED)
		{
			UE_LOG_ONLINE(Warning, TEXT("Failed to initialize the user service : 0x%x"), UserServerInitializeReturnCode);
			bSuccessfullyStartedUp = false;
		}
		else
		{
			bInitializedUserService = true;

			// Create the online async task thread before we initialize NpToolkit,
			// so we catch all the user login/logout events.
			OnlineAsyncTaskThreadRunnable = new FOnlineAsyncTaskManagerPS4(this);
			check(OnlineAsyncTaskThreadRunnable);
			OnlineAsyncTaskThread = FRunnableThread::Create(OnlineAsyncTaskThreadRunnable, *FString::Printf(TEXT("OnlineAsyncTaskThreadPS4 %s"), *InstanceName.ToString()));
			check(OnlineAsyncTaskThread);
			UE_LOG_ONLINE(Verbose, TEXT("Created thread (ID:%d)."), OnlineAsyncTaskThread->GetThreadID());

			// Initialize the toolkit
			const bool bInitToolkit = InitNpToolkit();
			if (bInitToolkit == false)
			{
				UE_LOG_ONLINE(Fatal, TEXT("Failed to initialize the np toolkit"));
				bSuccessfullyStartedUp = false;
			}
			else
			{
				int LoadCommerceReturnCode = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_COMMERCE);
				if (LoadCommerceReturnCode < SCE_OK)
				{
					UE_LOG_ONLINE(Warning, TEXT("Failed to initialize SCE_SYSMODULE_NP_COMMERCE"));
					bSuccessfullyStartedUp = false;
				}
				else
				{
					int LoadJSONReturnCode = sceSysmoduleLoadModule(SCE_SYSMODULE_JSON2);
					if (LoadJSONReturnCode < SCE_OK)
					{
						UE_LOG_ONLINE(Warning, TEXT("Failed to initialize SCE_SYSMODULE_JSON2"));
						bSuccessfullyStartedUp = false;
					}
					else
					{
						InitWebApi();

						// Create and initialize all of the individual interface implementations
						SessionInterface = MakeShareable(new FOnlineSessionPS4(this));
						//todo decide what to do if the session interface can not be initialized?
						if (!SessionInterface->Init())
						{
							// can't just outright fail the whole OSS init, or the game can't even use the IdentityInterface.
							// Also this can fail if the initial user just isn't signed up for PSN.
							UE_LOG_ONLINE(Log, TEXT("Session interface failed init."));
							//bSuccessfullyStartedUp = false;
						}
						//SessionInterface->CheckPendingSessionInvite();

						IdentityInterface = MakeShareable(new FOnlineIdentityPS4(this));
						FriendsInterface = MakeShareable(new FOnlineFriendsPS4(*this));
						MessageSanitizer = MakeShareable(new FMessageSanitizerPS4(this));
						UserInterface = MakeShareable(new FOnlineUserPS4(this));
						ExternalUIInterface = MakeShareable(new FOnlineExternalUIPS4(this));
						PresenceInterface = MakeShareable(new FOnlinePresencePS4(*this));

						if (bStoreEnabled)
						{
							StoreInterface = MakeShareable(new FOnlineStorePS4(this));
							PurchaseInterface = MakeShareable(new FOnlinePurchasePS4(this));
						}
						if (bAchievementsEnabled)
						{
							AchievementsInterface = MakeShareable(new FOnlineAchievementsPS4(this));
						}
						if (bLeaderboardsEnabled)
						{
							LeaderboardsInterface = MakeShareable(new FOnlineLeaderboardsPS4(this));
						}
						if (bSharingEnabled)
						{
							SharingInterface = MakeShareable(new FOnlineSharingPS4(this));
						}
						if (bUserCloudEnabled)
						{
							UserCloudInterface = MakeShareable(new FOnlineUserCloudPS4(this));
							UserCloudInterface->Init();
						}
						if (bTitleFileEnabled)
						{
							TitleFileInterface = MakeShareable(new FOnlineTitleFilePS4(this));
							TitleFileInterface->Init();
						}
						if (bMessagingEnabled)
						{
							MessageInterface = MakeShareable(new FOnlineMessagePS4(this));
							MessageInterface->Init();
						}

						if (bHasVoiceEnabled)
						{
							VoiceInterface = MakeShareable(new FOnlineVoicePS4());
							if (!VoiceInterface->Init())
							{
								VoiceInterface.Reset();
							}
						}

						if (bTournamentsEnabled)
						{
							TournamentInterface = MakeShared<FOnlineTournamentPS4, ESPMode::ThreadSafe>(*this);
							TournamentInterface->Init();
						}
					}
				}
			}
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("No valid NP title id/secret has been setup!"));
		bSuccessfullyStartedUp = false;
	}

	// Cache init success for allowing system events
	bFullyInitialized = bSuccessfullyStartedUp;

	return bSuccessfullyStartedUp;
}

bool FOnlineSubsystemPS4::Shutdown()
{
	bool bSuccessfullyShutdown = true;

	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineSubsystemPS4::Shutdown()"));

	FOnlineSubsystemImpl::Shutdown();

	if (FPS4Application::IsInitialized())
	{
		FPS4Application* const Application = FPS4Application::GetPS4Application();
		Application->OnPlayTogetherSystemServiceEventDelegate.Remove(OnPlayTogetherSystemServiceEventDelegateHandle);
		Application->OnTournamentJoinEventDelegate.Remove(OnTournamentJoinEventDelegateHandle);
		Application->OnTournamentJoinMatchEventDelegate.Remove(OnTournamentJoinMatchEventDelegateHandle);
		Application->OnTournamentJoinTeamMatchEventDelegate.Remove(OnTournamentJoinTeamMatchEventDelegateHandle);
	}

	FinalizeWebApi();

	if (OnlineAsyncTaskThread)
	{
		// Destroy the online async task thread
		delete OnlineAsyncTaskThread;
		OnlineAsyncTaskThread = nullptr;
	}

	if (OnlineAsyncTaskThreadRunnable)
	{
		delete OnlineAsyncTaskThreadRunnable;
		OnlineAsyncTaskThreadRunnable = nullptr;
	}

	if (VoiceInterface.IsValid())
	{
		VoiceInterface->Shutdown();
	}

	if (MessageInterface.IsValid())
	{
		MessageInterface->Shutdown();
	}

	if (TitleFileInterface.IsValid())
	{
		TitleFileInterface->Shutdown();
	}

	if (UserCloudInterface.IsValid())
	{
		UserCloudInterface->Shutdown();
	}

#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
		ensure(Interface.IsUnique()); \
		Interface = nullptr; \
	}

	DESTRUCT_INTERFACE(VoiceInterface);
	DESTRUCT_INTERFACE(MessageInterface);
	DESTRUCT_INTERFACE(TitleFileInterface);
	DESTRUCT_INTERFACE(UserCloudInterface);
	DESTRUCT_INTERFACE(PresenceInterface);
	DESTRUCT_INTERFACE(SharingInterface);
	DESTRUCT_INTERFACE(ExternalUIInterface);
	DESTRUCT_INTERFACE(UserInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
	DESTRUCT_INTERFACE(AchievementsInterface);
	DESTRUCT_INTERFACE(StoreInterface);
	DESTRUCT_INTERFACE(PurchaseInterface);
	DESTRUCT_INTERFACE(FriendsInterface);
	DESTRUCT_INTERFACE(IdentityInterface);
	DESTRUCT_INTERFACE(SessionInterface);

#undef DESTRUCT_INTERFACE

	// Shutdown the Np Toolkit library
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_TOOLKIT2) == SCE_SYSMODULE_LOADED)
	{
		NpToolkit::Core::Request::TermParams TermParams;
		const int32 TermResult = NpToolkit::Core::term(TermParams);
		if (TermResult != SCE_TOOLKIT_NP_V2_SUCCESS)
		{
			UE_LOG_ONLINE(Warning, TEXT("Failed to terminate ToolkitNp2 library : 0x%x"), TermResult);
		}
	}

	// Unload the JSON library
	sceSysmoduleUnloadModule(SCE_SYSMODULE_JSON2);
	
	// Unload the NP utility library
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_UTILITY);

	// Unload the NP toolkit library
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TOOLKIT2);

	if (bInitializedUserService)
	{
		sceUserServiceTerminate();
		bInitializedUserService = false;
	}

	return bSuccessfullyShutdown;
}

FString FOnlineSubsystemPS4::GetAppId() const
{
	return ANSI_TO_TCHAR(NpTitleId.id);
}

static TSharedPtr<const FUniqueNetId> GetExecCommandUserId(IOnlineIdentityPtr IdentityInterface, const TCHAR*& Cmd, const bool bFallbackToLoggedInUser)
{
	TSharedPtr<const FUniqueNetId> UserId;
	{
		FString UserIdString = FParse::Token(Cmd, false);
		if (!UserIdString.IsEmpty())
		{
			UserId = FUniqueNetIdPS4::FromString(UserIdString);
		}
	}
	if (!UserId.IsValid() && bFallbackToLoggedInUser)
	{
		if (IdentityInterface.IsValid())
		{
			UserId = GetFirstSignedInUser(IdentityInterface);
		}
	}
	return UserId;
}

bool FOnlineSubsystemPS4::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) 
{
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}

	bool bWasHandled = false;
	if (FParse::Command(&Cmd, TEXT("WEBEMBED")))
	{
		FString Url = FParse::Token(Cmd, false);
		int32 X = FCString::Atoi(*FParse::Token(Cmd, false));
		int32 Y = FCString::Atoi(*FParse::Token(Cmd, false));
		int32 Width = FCString::Atoi(*FParse::Token(Cmd, false));
		int32 Height = FCString::Atoi(*FParse::Token(Cmd, false));
		if (Url == TEXT("close"))
		{
			ExternalUIInterface->CloseWebURL();
		}
		else if (Url.IsEmpty() || X < 0 || Y < 0 || Width <= 0 || Height <= 0)
		{
			UE_LOG_ONLINE(Warning, TEXT("WEBEMBED <url> <x> <y> <width> <height>"));
		}
		else 
		{
			FShowWebUrlParams Params(true, X, Y, Width, Height);
			Params.CallbackPath = TEXT("hasRedirect=");
			ExternalUIInterface->ShowWebURL(Url, Params, FOnShowWebUrlClosedDelegate());
		}
		bWasHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("SHOWSTORE")))
	{
		FString Category = FParse::Token(Cmd, false);
		ExternalUIInterface->ShowStoreUI(0, FShowStoreParams(Category), FOnShowStoreUIClosedDelegate());
		bWasHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("SHOWSENDMSG")))
	{
		FString Title = FParse::Token(Cmd, false);
		FString Message = FParse::Token(Cmd, false);
		FString Details = FParse::Token(Cmd, false);
		int32 MaxRecipients = FCString::Atoi(*FParse::Token(Cmd, false));

		if (Title.IsEmpty() || Message.IsEmpty() || Details.IsEmpty() || MaxRecipients <= 0)
		{
			UE_LOG_ONLINE(Warning, TEXT("SHOWSENDMSG <title> <message> <details> <max recipients>"));
		}
		else
		{
			FShowSendMessageParams MsgParams;
			MsgParams.DisplayTitle = FText::FromString(Title);
			MsgParams.DisplayMessage = FText::FromString(Message);
			MsgParams.DisplayDetails = FText::FromString(Details);
			MsgParams.MaxRecipients = MaxRecipients;
			MsgParams.DataPayload.SetAttribute(TEXT("Code"), TEXT("1234567890"));
			ExternalUIInterface->ShowSendMessageUI(0, MsgParams, FOnShowSendMessageUIClosedDelegate());
		}
		bWasHandled = true;
	}
	else if (FParse::Command(&Cmd, TEXT("ACHIEVEMENTS")))
	{
		if (AchievementsInterface.IsValid())
		{
			bWasHandled = true;
			if (FParse::Command(&Cmd, TEXT("DUMP")))
			{
				AchievementsInterface->Dump();
			}
			else if (FParse::Command(&Cmd, TEXT("QUERYDESCRIPTIONS")))
			{
				const TSharedPtr<const FUniqueNetId> UserId = GetExecCommandUserId(IdentityInterface, Cmd, true);
				if (UserId.IsValid())
				{
					AchievementsInterface->QueryAchievementDescriptions(*UserId, FOnQueryAchievementsCompleteDelegate::CreateLambda([this](const FUniqueNetId&, const bool)
					{
						AchievementsInterface->Dump();
					}));
				}
				else
				{
					UE_LOG_ONLINE(Log, TEXT("Missing UserId"));
				}
			}
			else if (FParse::Command(&Cmd, TEXT("QUERYACHIEVEMENTS")))
			{
				const TSharedPtr<const FUniqueNetId> UserId = GetExecCommandUserId(IdentityInterface, Cmd, true);
				if (UserId.IsValid())
				{
					AchievementsInterface->QueryAchievements(*UserId, FOnQueryAchievementsCompleteDelegate::CreateLambda([this](const FUniqueNetId&, const bool)
					{
						AchievementsInterface->Dump();
					}));
				}
				else
				{
					UE_LOG_ONLINE(Log, TEXT("Missing UserId"));
				}
			}
			else
			{
				bWasHandled = false;
			}
		}
		else
		{
			UE_LOG_ONLINE(Log, TEXT("No achievements interface"));
		}
	}
	else if (FParse::Command(&Cmd, TEXT("SESSIONS")))
	{
		if (SessionInterface.IsValid())
		{
			if (FParse::Command(&Cmd, TEXT("ISCHATDISABLED")))
			{
				const TSharedPtr<const FUniqueNetId> UserId = GetExecCommandUserId(IdentityInterface, Cmd, true);
				if (UserId.IsValid())
				{
					bool bIsChatDisabled = SessionInterface->IsChatDisabled(*StaticCastSharedPtr<const FUniqueNetIdPS4>(UserId));
					UE_LOG_ONLINE(Log, TEXT("ISCHATDISABLED: User=[%s] bIsChatDisabled=%d"), *UserId->ToDebugString(), static_cast<int32>(bIsChatDisabled));
				}
			}
		}
	}
#if !UE_BUILD_SHIPPING
	else if (FParse::Command(&Cmd, TEXT("TOURNAMENTS")))
	{
		if (TournamentInterface.IsValid())
		{
			TSharedPtr<const FUniqueNetId> UserIdPtr;
			if (IdentityInterface.IsValid())
			{
				UserIdPtr = GetFirstSignedInUser(IdentityInterface);
			}

			if (UserIdPtr.IsValid())
			{
				TSharedRef<const FUniqueNetId> UserId = UserIdPtr.ToSharedRef();
				if (FParse::Command(&Cmd, TEXT("DUMP")))
				{
					TournamentInterface->DumpCachedTournamentInfo(UserId);
					TournamentInterface->DumpCachedMatchInfo(UserId);
					TournamentInterface->DumpCachedParticipantInfo(UserId);
					TournamentInterface->DumpCachedTeamInfo(UserId);
					bWasHandled = true;
				}
				else if (FParse::Command(&Cmd, TEXT("QUERY")))
				{
					TournamentInterface->TestQueryTournamentInfo(UserId);
					bWasHandled = true;
				}
				else
				{
					UE_LOG_ONLINE(Log, TEXT("Unknown Tournament Command %s"), Cmd);
				}
			}
			else
			{
				UE_LOG_ONLINE(Log, TEXT("Missing UserId"));
			}
		}
		else
		{
			UE_LOG_ONLINE(Log, TEXT("Tournaments are disabled"));
		}
	}
#endif // !UE_BUILD_SHIPPING

	return bWasHandled;
}

FText FOnlineSubsystemPS4::GetOnlineServiceName() const
{
	const FText TrademarkText = FText::FromString(FString(TEXT("\u2122"))); /*TM*/
	return FText::Format(NSLOCTEXT("OnlineSubsystemPS4", "OnlineServiceName", "PlayStation{0}Network"), TrademarkText);
}

const SceNpTitleId& FOnlineSubsystemPS4::GetNpTitleId() const
{
	return NpTitleId;
}

const SceNpClientId& FOnlineSubsystemPS4::GetNpClientId() const
{
	return NpClientId;
}

const FString& FOnlineSubsystemPS4::GetNpLicenseeId() const
{
	return NpLicenseeId;
}

FNpWebApiUserContext FOnlineSubsystemPS4::GetUserWebApiContext(const FUniqueNetIdPS4& NetUserIdPS4)
{
	int32 ServiceUserId = NetUserIdPS4.GetUserId();
	if (LocalUserToWebApiContexts.Contains(ServiceUserId))
	{
		return LocalUserToWebApiContexts[ServiceUserId];
	}
	else if (WebApiContext != -1)
	{
		// Create a context for the user
		if (NetUserIdPS4.IsLocalId())
		{
			FNpWebApiUserContext ReturnCode = sceNpWebApiCreateContextA(WebApiContext, ServiceUserId);
			if (ReturnCode < 0)
			{
				UE_LOG_ONLINE(Error, TEXT("sceNpWebApiCreateContextA error: 0x%x"), ReturnCode);
			}
			else
			{
				LocalUserToWebApiContexts.Add(ServiceUserId, ReturnCode);
				return ReturnCode;
			}
		}
	}

	// Could not find or create a context for the user
	return -1;
}

void FOnlineSubsystemPS4::CacheOnlineId(SceNpAccountId AccountId, SceNpOnlineId OnlineId)
{
	FScopeLock Lock(&OnlineIdCacheCS);
	OnlineIdMap.Insert(AccountId, OnlineId);
}

class FOnlineAsyncTaskPS4ResolveBase : public FOnlineAsyncTaskPS4
{
public:
	FOnlineAsyncTaskPS4ResolveBase(
			FOnlineSubsystemPS4* InSubsystem, 
			uint32 InUserContext, 
			TCHAR const* InUrl, 
			FString const& RequestBody,
			FOnlineIdMap const& InExistingResultsMap, 
			FOnIdResolveComplete const& InDelegate)

		: FOnlineAsyncTaskPS4(InSubsystem)
		, AsyncTask(InUserContext)
		, ResultsMap(InExistingResultsMap)
		, Delegate(InDelegate)
	{
		FWebApiPS4Task& Task = AsyncTask.GetTask();
		Task.SetRequest(ENpApiGroup::IdentityMapper, InUrl, SCE_NP_WEBAPI_HTTP_METHOD_POST);
		Task.SetRequestBody(RequestBody);
		AsyncTask.StartBackgroundTask();
	}

	virtual void Tick() override
	{
		if (AsyncTask.IsDone())
		{
			bIsComplete = true;
			bWasSuccessful = AsyncTask.GetTask().GetHttpStatusCode() == 200;
		}
	}

	virtual void Finalize() override
	{
		FWebApiPS4Task& Task = AsyncTask.GetTask();
		if (bWasSuccessful)
		{
			// Parse the Json response
			TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<>::Create(Task.GetResponseBody());

			EJsonNotation Notation;
			if (JsonReader->ReadNext(Notation) && Notation == EJsonNotation::ArrayStart)
			{
				while (JsonReader->ReadNext(Notation) && Notation == EJsonNotation::ObjectStart)
				{
					// Read the inner object
					TMap<FString, FString> KeyValuePairs;
					while (JsonReader->ReadNext(Notation) && Notation != EJsonNotation::ObjectEnd)
					{
						if (Notation == EJsonNotation::String)
						{
							KeyValuePairs.FindOrAdd(JsonReader->GetIdentifier()) = JsonReader->GetValueAsString();
						}
					}

					FString* pAccountIdValue = KeyValuePairs.Find(TEXT("accountId"));
					FString* pOnlineIdValue = KeyValuePairs.Find(TEXT("onlineId"));

					if (pAccountIdValue && pOnlineIdValue)
					{
						// Both online and account IDs are present. This is a valid PSN account.
						SceNpOnlineId OnlineId = PS4StringToOnlineId(*pOnlineIdValue);
						SceNpAccountId AccountId = PS4StringToAccountId(*pAccountIdValue);

						ResultsMap.Insert(AccountId, OnlineId);
						Subsystem->CacheOnlineId(AccountId, OnlineId); // Update the OSS's cache too.
					}
				}
			}
		}
		else
		{
			ErrorString = Task.GetErrorString();
		}
	}

	virtual void TriggerDelegates() override
	{
		Delegate.Broadcast(ResultsMap, bWasSuccessful, ErrorString);
	}

private:
	FAsyncTask<FWebApiPS4Task> AsyncTask;

	FOnlineIdMap ResultsMap;
	FOnIdResolveComplete Delegate;
	FString ErrorString;
};

class FOnlineAsyncTaskPS4ResolveAccountIds : public FOnlineAsyncTaskPS4ResolveBase
{
	static FString BuildRequestBody(FSceNpOnlineIdSet const& OnlineIds)
	{
		// Construct the JSON array of online ids.
		TArray<uint8> RequestBytes;
		FMemoryWriter Ar(RequestBytes);

		auto Writer = TJsonWriterFactory<>::Create(&Ar);
		Writer->WriteArrayStart();
		for (SceNpOnlineId OnlineId : OnlineIds)
		{
			Writer->WriteValue(PS4OnlineIdToString(OnlineId));
		}
		Writer->WriteArrayEnd();
		RequestBytes.AddZeroed(sizeof(TCHAR)); // Add null terminator

		return FString(RequestBytes.Num(), reinterpret_cast<TCHAR*>(RequestBytes.GetData()));
	}

public:
	FOnlineAsyncTaskPS4ResolveAccountIds(
			FOnlineSubsystemPS4* InSubsystem,
			uint32 InUserContext,
			FOnlineIdMap const& InExistingResultsMap,
			FSceNpOnlineIdSet const& InOnlineIds,
			FOnIdResolveComplete const& InDelegate)
		: FOnlineAsyncTaskPS4ResolveBase(InSubsystem, InUserContext, TEXT("/v2/accounts/map/onlineId2accountId"), BuildRequestBody(InOnlineIds), InExistingResultsMap, InDelegate)
	{}

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskPS4ResolveAccountIds"); }
};

void FOnlineSubsystemPS4::ResolveAccountIdsAsync(FUniqueNetIdPS4 const& LocalUserId, TArray<SceNpOnlineId> OnlineIds, FOnIdResolveComplete const& Delegate)
{
	FString ErrorString;
	FOnlineIdMap ResultsMap;
	FSceNpOnlineIdSet OnlineIdsToResolve;

	{
		FScopeLock Lock(&OnlineIdCacheCS);

		// Scan the cache for existing results.
		for (SceNpOnlineId OnlineId : OnlineIds)
		{
			SceNpAccountId AccountId;
			if (OnlineIdMap.GetAccountId(OnlineId, AccountId))
			{
				ResultsMap.Insert(AccountId, OnlineId);
			}
			else
			{
				OnlineIdsToResolve.Add(OnlineId);
			}
		}

		if (OnlineIdsToResolve.Num())
		{
			// We have IDs to resolve.
			const int32 UserContext = GetUserWebApiContext(LocalUserId);
			if (UserContext == INDEX_NONE)
			{
				ErrorString = FString::Printf(TEXT("ResolveAccountIdsAsync: Failed to get web api context for local user %d."), LocalUserId.GetUserId());
			}
			else
			{
				GetAsyncTaskManager()->AddToInQueue(new FOnlineAsyncTaskPS4ResolveAccountIds(this, UserContext, ResultsMap, OnlineIdsToResolve, Delegate));
				return;
			}
		}
	}

	// The call completed synchronously. Trigger the delegate.
	Delegate.Broadcast(ResultsMap, ErrorString.IsEmpty(), ErrorString);
}

class FOnlineAsyncTaskPS4ResolveOnlineIds : public FOnlineAsyncTaskPS4ResolveBase
{
	static FString BuildRequestBody(FSceNpAccountIdSet const& AccountIds)
	{
		// Construct the JSON array of account ids.
		TArray<uint8> RequestBytes;
		FMemoryWriter Ar(RequestBytes);

		auto Writer = TJsonWriterFactory<>::Create(&Ar);
		Writer->WriteArrayStart();
		for (SceNpAccountId AccountId : AccountIds)
		{
			Writer->WriteValue(PS4AccountIdToString(AccountId));
		}
		Writer->WriteArrayEnd();
		RequestBytes.AddZeroed(sizeof(TCHAR)); // Add null terminator

		return FString(RequestBytes.Num(), reinterpret_cast<TCHAR*>(RequestBytes.GetData()));
	}

public:
	FOnlineAsyncTaskPS4ResolveOnlineIds(
			FOnlineSubsystemPS4* InSubsystem, 
			uint32 InUserContext,
			FOnlineIdMap const& InExistingResultsMap, 
			FSceNpAccountIdSet const& InAccountIds,
			FOnIdResolveComplete const& InDelegate)
		: FOnlineAsyncTaskPS4ResolveBase(InSubsystem, InUserContext, TEXT("/v2/accounts/map/accountId2onlineId"), BuildRequestBody(InAccountIds), InExistingResultsMap, InDelegate)
	{}

	virtual FString ToString() const override { return TEXT("FOnlineAsyncTaskPS4ResolveOnlineIds"); }
};

void FOnlineSubsystemPS4::ResolveOnlineIdsAsync(FUniqueNetIdPS4 const& LocalUserId, TArray<SceNpAccountId> const& AccountIds, FOnIdResolveComplete const& Delegate)
{
	FString ErrorString;
	FOnlineIdMap ResultsMap;
	FSceNpAccountIdSet AccountIdsToResolve;

	{
		FScopeLock Lock(&OnlineIdCacheCS);

		// Scan the cache for existing results.
		for (SceNpAccountId AccountId : AccountIds)
		{
			SceNpOnlineId OnlineId;
			if (OnlineIdMap.GetOnlineId(AccountId, OnlineId))
			{
				ResultsMap.Insert(AccountId, OnlineId);
			}
			else
			{
				// ID not in the cache. If the account ID belongs to a local, signed in
				// player, we don't need to call out to PSN to resolve the online ID.
				SceUserServiceUserId SceUserId;
				if (sceNpGetUserIdByAccountId(AccountId, &SceUserId) == SCE_OK &&
					sceNpGetOnlineId(SceUserId, &OnlineId) == SCE_OK)
				{
					ResultsMap.Insert(AccountId, OnlineId);
					OnlineIdMap.Insert(AccountId, OnlineId); // Update the cache too.
				}
				else
				{
					// Account ID is a remote player, or a non-signed-in local player.
					// We'll need to call out to PSN services to resolve this.
					AccountIdsToResolve.Add(AccountId);
				}
			}
		}

		if (AccountIdsToResolve.Num())
		{
			// We have IDs to resolve.
			const int32 UserContext = GetUserWebApiContext(LocalUserId);
			if (UserContext == INDEX_NONE)
			{
				ErrorString = FString::Printf(TEXT("ResolveOnlineIdsAsync: Failed to get web api context for local user %d."), LocalUserId.GetUserId());
			}
			else
			{
				GetAsyncTaskManager()->AddToInQueue(new FOnlineAsyncTaskPS4ResolveOnlineIds(this, UserContext, ResultsMap, AccountIdsToResolve, Delegate));
				return;
			}
		}
	}

	// The call completed synchronously. Trigger the delegate.
	Delegate.Broadcast(ResultsMap, ErrorString.IsEmpty(), ErrorString);
}

void FOnlineSubsystemPS4::SetUsingMultiplayerFeatures(const FUniqueNetId& UniqueId, bool bUsingMP)
{
	SceUserServiceUserId SceUserId = FUniqueNetIdPS4::Cast(UniqueId).GetUserId();
	
	if (SceUserId != SCE_USER_SERVICE_USER_ID_INVALID)
	{
		int32 FirstEmptySlot = -1;
		int32 OnlineArrayIndex = -1;
		for (int i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++i)
		{
			if (UserServiceIdsOnline[i] == SceUserId)
			{
				check(OnlineArrayIndex == -1);
				OnlineArrayIndex = i;
			}
			if (FirstEmptySlot == -1 && UserServiceIdsOnline[i] == SCE_USER_SERVICE_USER_ID_INVALID)
			{
				FirstEmptySlot = i;
			}
		}

		if (bUsingMP)
		{
			// fill a slot if we haven't been marked as using online features yet.
			if (OnlineArrayIndex == -1)
			{
				check(FirstEmptySlot != -1);
				UserServiceIdsOnline[FirstEmptySlot] = SceUserId;
			}
		}
		else
		{
			// clear the slot if we've already been marked.
			if (OnlineArrayIndex != -1)
			{
				UserServiceIdsOnline[OnlineArrayIndex] = SCE_USER_SERVICE_USER_ID_INVALID;
			}
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Trying to set an invalid user's multiplayer status."));
	}
}

bool FOnlineSubsystemPS4::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	// handle user login/logout
	//ProcessUserLoginEvents();

	if (OnlineAsyncTaskThreadRunnable)
	{
		OnlineAsyncTaskThreadRunnable->GameTick();
	}

	if (VoiceInterface.IsValid())
	{
		VoiceInterface->Tick(DeltaTime);
	}

	for (int i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++i)
	{
		if (UserServiceIdsOnline[i] != SCE_USER_SERVICE_USER_ID_INVALID)
		{
			SceNpNotifyPlusFeatureParameter NotifyParams;
			FMemory::Memzero(NotifyParams);

			NotifyParams.size = sizeof(NotifyParams);
			NotifyParams.userId = UserServiceIdsOnline[i];
			NotifyParams.features = SCE_NP_PLUS_FEATURE_REALTIME_MULTIPLAY;
			int32 Ret = sceNpNotifyPlusFeature(&NotifyParams);
			if (Ret != SCE_OK)
			{
				UE_LOG_ONLINE(Verbose, TEXT("sceNpNotifyPlusFeature failed with error: 0x%x"), Ret);
			}
		}
	}

	// Since the FPS4Application object doesn't exist when the OSS is initialized, bind the play together delegate the first chance we get.
	FPS4Application* const PS4Application = FPS4Application::GetPS4Application();
	if (PS4Application)
	{
		if (!OnPlayTogetherSystemServiceEventDelegateHandle.IsValid())
		{
			OnPlayTogetherSystemServiceEventDelegateHandle = PS4Application->OnPlayTogetherSystemServiceEventDelegate.AddRaw(this, &FOnlineSubsystemPS4::OnPlayTogetherEventReceived);
		}

		if (TournamentInterface.IsValid())
		{
			if (!OnTournamentJoinEventDelegateHandle.IsValid())
			{
				OnTournamentJoinEventDelegateHandle = PS4Application->OnTournamentJoinEventDelegate.AddThreadSafeSP(&(*TournamentInterface), &FOnlineTournamentPS4::OnTournamentJoinEvent);
			}
			if (!OnTournamentJoinMatchEventDelegateHandle.IsValid())
			{
				OnTournamentJoinMatchEventDelegateHandle = PS4Application->OnTournamentJoinMatchEventDelegate.AddThreadSafeSP(&(*TournamentInterface), &FOnlineTournamentPS4::OnTournamentJoinMatchEvent);
			}
			if (!OnTournamentJoinTeamMatchEventDelegateHandle.IsValid())
			{
				OnTournamentJoinTeamMatchEventDelegateHandle = PS4Application->OnTournamentJoinTeamMatchEventDelegate.AddThreadSafeSP(&(*TournamentInterface), &FOnlineTournamentPS4::OnTournamentJoinTeamMatchEvent);
			}
		}
	}

	if (ExternalUIInterface.IsValid())
	{
		ExternalUIInterface->Tick(DeltaTime);
	}

	// This is needed to for the timeout feature of the web API to work, we are supposed to call it "periodically"
	sceNpWebApiCheckTimeout();

	return true;
}

void FOnlineSubsystemPS4::OnPlayTogetherEventReceived(const SceNpPlayTogetherHostEventParamA& EventParam)
{
	UE_LOG_ONLINE(Log, TEXT("Online Play together event received for %d party members."), EventParam.inviteeListLen);

	if (const FSonyApplication* const SonyApplication = FSonyApplication::GetSonyApplication())
	{
		TArray<TSharedPtr<const FUniqueNetId>> UserIdList;
		for (int32 i = 0; i < EventParam.inviteeListLen; i++)
		{
			TSharedPtr<const FUniqueNetIdPS4> UserId = FUniqueNetIdPS4::FindOrCreate(EventParam.inviteeList[i].accountId);
			UserIdList.Add(UserId);
		}
		OnPlayTogetherEventReceivedDelegates.Broadcast(SonyApplication->GetUserIndex(EventParam.userId), UserIdList);
	}
}

EOnlineEnvironment::Type FOnlineSubsystemPS4::GetOnlineEnvironment() const
{
	return IssuerIdToEnvironment(IssuerId);
}

void FOnlineSubsystemPS4::SetIssuerId(NpToolkit::Auth::IssuerIdType NewIssuerId)
{
	if (IssuerId != NewIssuerId)
	{
		EOnlineEnvironment::Type LastOnlineEnvironment = IssuerIdToEnvironment(IssuerId);
		EOnlineEnvironment::Type NewOnlineEnvironment = IssuerIdToEnvironment(NewIssuerId);
		IssuerId = NewIssuerId;

		UE_LOG_ONLINE(Log, TEXT("Online Environment updated old=%s new=%s"), 
			EOnlineEnvironment::ToString(LastOnlineEnvironment), EOnlineEnvironment::ToString(NewOnlineEnvironment));
	
		TriggerOnOnlineEnvironmentChangedDelegates(LastOnlineEnvironment, NewOnlineEnvironment);
	}
}
