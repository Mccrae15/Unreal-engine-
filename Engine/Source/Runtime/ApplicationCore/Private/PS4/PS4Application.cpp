// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4Application.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "PS4InputInterface.h"
#include <system_service.h>
#include <coredump.h>
#include <invitation_dialog.h>
#include "PS4Cursor.h"
#include "ThreadingBase.h"
#include "PlatformMallocCrash.h"
#include "EngineVersion.h"
#include "IInputDeviceModule.h"
#include <libime.h>
#include "HAL/ThreadHeartBeat.h"
#include <video_out.h>

static const float GDefaultSafeAreaPercentage = 0.9f;
FPS4Application* PS4Application = nullptr;

static int32 GPS4Allow4kOutput = 0;
static FAutoConsoleVariableRef CVarPS4Allow4kOutput(
	TEXT("r.PS4Allow4kOutput"),
	GPS4Allow4kOutput,
	TEXT("Allows the engine to allocate 4k backbuffers if a 4k tv is detected."),
	ECVF_ReadOnly
	);

enum class ELoginChangeEventType
{
	SystemLogIn,
	SystemLogOut,
};

struct FLoginChangeEvent
{
	FLoginChangeEvent(SceUserServiceUserId InUserId, ELoginChangeEventType InType)
		: UserId(InUserId)
		, Type(InType)
	{}

	SceUserServiceUserId UserId;
	ELoginChangeEventType Type;
};

/**
* Async Task to send to gather system events.  Reduces pressure on gamethread.
*/
class SystemEventGatherRunnable : public FRunnable
{
public:
	SystemEventGatherRunnable()
	{		
		bIsRunning = true;
		bCheckEvents = false;
		bIsInBackground = false;
		
		DoGatherEvent = FPlatformProcess::GetSynchEventFromPool(true);
		bResultsReady = false;
		SystemEventGatherTaskThread = FRunnableThread::Create(this, TEXT("SystemEventGatherThreadPS4"));
		check(SystemEventGatherTaskThread);
	}

	~SystemEventGatherRunnable()
	{
		if (SystemEventGatherTaskThread != nullptr)
		{
			Stop();
			SystemEventGatherTaskThread->WaitForCompletion();
			delete SystemEventGatherTaskThread;
			SystemEventGatherTaskThread = nullptr;

			FPlatformProcess::ReturnSynchEventToPool(DoGatherEvent);
			DoGatherEvent = nullptr;			
		}
	}

	virtual uint32 Run()
	{
		while (bIsRunning)
		{			
			DoGatherEvent->Wait();
			ensureMsgf(!bResultsReady, TEXT("SystemEventGather triggered without processing previous results."));

			int32 Ret = 0;
			// User Events
			{
				
				SceUserServiceEvent Event;
				for (;;)
				{
					Ret = sceUserServiceGetEvent(&Event);

					// handle error codes
					if (Ret == SCE_USER_SERVICE_ERROR_NO_EVENT ||
						Ret == SCE_USER_SERVICE_ERROR_NOT_INITIALIZED)
					{
						// no events to handle
						break;
					}
					else if (Ret != SCE_OK)
					{
						checkf(false, TEXT("Unhandled sceUserServiceGetEvent() error: 0x%x"), Ret);
						break;
					}

					// call our login/logout delegates
					if (Event.eventType == SCE_USER_SERVICE_EVENT_TYPE_LOGIN)
					{
						// a user has logged in
						DetectedLoginChangeEvents.Emplace(FLoginChangeEvent(Event.userId, ELoginChangeEventType::SystemLogIn));
					}
					else if (Event.eventType == SCE_USER_SERVICE_EVENT_TYPE_LOGOUT)
					{
						// a user has logged out
						DetectedLoginChangeEvents.Emplace(FLoginChangeEvent(Event.userId, ELoginChangeEventType::SystemLogOut));
					}
				}
			}

			// see if there's any system events.
			SceSystemServiceStatus SystemStatus;
			Ret = sceSystemServiceGetStatus(&SystemStatus);
			if (Ret == SCE_OK)
			{
				if (bIsInBackground != SystemStatus.isInBackgroundExecution)
				{
					bIsInBackground = SystemStatus.isInBackgroundExecution;					
				}

				for (int EventIndex = 0; EventIndex < SystemStatus.eventNum; ++EventIndex)
				{
					SceSystemServiceEvent SystemEvent;
					Ret = sceSystemServiceReceiveEvent(&SystemEvent);
					if (Ret != SCE_OK)
					{
						checkf(false, TEXT("Unhandled sceSystemServiceReceiveEvent() error: 0x%x"), Ret);
						continue;
					}
					SystemServiceEvents.Add(SystemEvent);
				}
			}
			else
			{
				checkf(false, TEXT("Unhandled sceSystemServiceGetStatus() error: 0x%x"), Ret);
			}

			DoGatherEvent->Reset();

			FPlatformMisc::MemoryBarrier();
			bResultsReady = true;
		}

		return 0;
	}

	virtual void Stop()
	{
		bIsRunning = false;
		FPlatformMisc::MemoryBarrier();
		TriggerGetEvents();
	}

	void TriggerGetEvents()
	{
		DoGatherEvent->Trigger();
	}

	bool ProcessResults_GameThread()
	{		
		check(IsInGameThread());

		bool bProcessed = false;
		FPS4Application* PS4App = FPS4Application::GetPS4Application();
		if (bResultsReady)
		{
			if (DetectedLoginChangeEvents.Num() > 0)
			{
				for (const FLoginChangeEvent& LoginEvent : DetectedLoginChangeEvents)
				{
					switch (LoginEvent.Type)
					{
					case ELoginChangeEventType::SystemLogIn:
						{
							// Guard against duplicate events for the same user
							check(LoginEvent.UserId != SCE_USER_SERVICE_USER_ID_INVALID);
							if (PS4App->GetUserIndex(LoginEvent.UserId) == INDEX_NONE)
							{
								// Insert the new user ID before we broadcast the user changed event.
								int32 UserIndex = PS4App->InsertUserID(LoginEvent.UserId);
								UE_LOG(LogPS4, Log, TEXT("PS4 User Service Event: LogIn - UserIndex: %d, UserID: %d"), UserIndex, LoginEvent.UserId);
								FCoreDelegates::OnUserLoginChangedEvent.Broadcast(true, LoginEvent.UserId, UserIndex);
							}
						}
						break;

					case ELoginChangeEventType::SystemLogOut:
						{
							// Guard against duplicate events for the same user
							int32 UserIndex = PS4App->GetUserIndex(LoginEvent.UserId);
							if (UserIndex != INDEX_NONE)
							{
								UE_LOG(LogPS4, Log, TEXT("PS4 User Service Event: LogOut - UserIndex: %d, UserID: %d"), UserIndex, LoginEvent.UserId);
								FCoreDelegates::OnUserLoginChangedEvent.Broadcast(false, LoginEvent.UserId, UserIndex);

								// Remove the user id from the array after we've broadcast the user changed event.
								PS4App->RemoveUserID(LoginEvent.UserId);
							}
						}
						break;
					}
				}
				DetectedLoginChangeEvents.Reset();
			}

			PS4App->SetIsInBackground(bIsInBackground);

			for (int32 i = 0; i < SystemServiceEvents.Num(); ++i)
			{
				SceSystemServiceEvent& SystemEvent = SystemServiceEvents[i];
				switch (SystemEvent.eventType)
				{
				case SCE_SYSTEM_SERVICE_EVENT_DISPLAY_SAFE_AREA_UPDATE:
					FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
					break;
				case SCE_SYSTEM_SERVICE_EVENT_ON_RESUME:
					// detect transition from 'Suspended' state to 'ForegroundState'
					// it is not possible to detect the transition TO Suspended or Termination state according to:
					// https://ps4.scedev.net/docs/ps4-en,Programming-Startup_Guide-orbis,Application_State_Transitions/1
					// We could try to hack something up based on the rules about users, but this might miss some other suspension case that
					// only the system software knows about.  Therefore, we never broadcast ApplicationWillEnterBackgroundDelegate or ApplicationWillTerminateDelegate
					FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();

					PS4Application->CheckForSafeAreaChanges();
					break;
				case SCE_SYSTEM_SERVICE_EVENT_SESSION_INVITATION:
				{
					SceNpSessionInvitationEventParam& param = reinterpret_cast<SceNpSessionInvitationEventParam&>(SystemEvent.data.param);
					const FString SessionId(ANSI_TO_TCHAR(param.sessionId.data));
					FCoreDelegates::OnInviteAccepted.Broadcast(FString::Printf(TEXT("%d"), param.userId), SessionId);
					UE_LOG(LogPS4, Log, TEXT("Invite accepted with session id %s"), *SessionId);
					break;
				}
				case SCE_SYSTEM_SERVICE_EVENT_RESET_VR_POSITION:
					FCoreDelegates::VRHeadsetRecenter.Broadcast();
					break;
				case SCE_SYSTEM_SERVICE_EVENT_PLAY_TOGETHER_HOST_A:
				{
					SceNpPlayTogetherHostEventParamA& param = reinterpret_cast<SceNpPlayTogetherHostEventParamA&>(SystemEvent.data.param);
					UE_LOG(LogPS4, Log, TEXT("Play together event received for %d party members."), param.inviteeListLen);

					if (FPS4Application::GetPS4Application()->OnPlayTogetherSystemServiceEventDelegate.IsBound())
					{
						FPS4Application::GetPS4Application()->OnPlayTogetherSystemServiceEventDelegate.Broadcast(param);
					}
					else
					{
						FMemory::Memcpy(&FPS4Application::GetPS4Application()->CachedPlayTogetherHostEventParam, &param, sizeof(SceNpPlayTogetherHostEventParamA));
					}
					break;
				}
				default:
					break;
				}
			}
			SystemServiceEvents.Reset();
			bResultsReady = false;
			FPlatformMisc::MemoryBarrier();
			bProcessed = true;
		}
		return bProcessed;
	}

private:
	TArray<FLoginChangeEvent> DetectedLoginChangeEvents;
	TArray<SceSystemServiceEvent> SystemServiceEvents;
	bool bIsInBackground;

	FRunnableThread* SystemEventGatherTaskThread;
	FEvent* DoGatherEvent;
	bool bCheckEvents;	
	volatile bool bIsRunning;
	volatile bool bResultsReady;
};


FPS4Application* FPS4Application::CreatePS4Application()
{
	check(PS4Application == nullptr);
	PS4Application = new FPS4Application();
	return PS4Application;
}


FPS4Application* FPS4Application::GetPS4Application()
{
	// We no longer assert on null application, there is early code that runs, but checks for null outside
	// of this code, so it's safe to return null
	// check(PS4Application != nullptr);
	return PS4Application;
}

bool FPS4Application::IsInitialized()
{
	return PS4Application != nullptr;
}

FPS4Application::~FPS4Application()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST 
	ConsoleInput.Finalize();
#endif
}

FPS4Application::FPS4Application()
: GenericApplication( MakeShareable(new FPS4Cursor()) )
	, InputInterface( FPS4InputInterface::Create( MessageHandler, Cursor ) )
	, SafeAreaPercentage(GDefaultSafeAreaPercentage) // a good default value in case user never sets it
	, bIsInBackground(false)
{
	for (int32 Index = 0; Index < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++Index)
		UserIDs[Index] = SCE_USER_SERVICE_USER_ID_INVALID;

	FMemory::Memzero(&CachedPlayTogetherHostEventParam, sizeof(CachedPlayTogetherHostEventParam));

	// retrieve the safe zone set by the user
	SceSystemServiceDisplaySafeAreaInfo Info;
	int32 Result = sceSystemServiceGetDisplaySafeAreaInfo(&Info);

	if (Result == SCE_OK)
	{
		SafeAreaPercentage = Info.ratio;
	}

	FDisplayMetrics::GetDisplayMetrics(LastKnownMetrics);

	// in the error case, use the default value
	FConfigSection* AgeRestrictionConfigs = GConfig->GetSectionPrivate(TEXT("PS4Application"), false, true, GEngineIni);
	if (AgeRestrictionConfigs != nullptr)
	{
		for (FConfigSection::TIterator It(*AgeRestrictionConfigs); It; ++It)
		{
			if (It.Key() == TEXT("CountryAgeRestrictions"))
			{
				FCountryAgeRestriction CountryAgeRestriction;
				if (CountryAgeRestriction.ParseConfig(*It.Value().GetValue()))
				{
					CountryAgeRestrictions.Add(CountryAgeRestriction);
				}
			}
		}
	}

	int32 DefaultAgeRestriction = 0;
	if (GConfig->GetInt(TEXT("PS4Application"), TEXT("DefaultAgeRestriction"), DefaultAgeRestriction, GEngineIni) || CountryAgeRestrictions.Num() > 0)
	{
		SceNpContentRestriction ContentRestriction;
		FMemory::Memset(ContentRestriction, 0x00);
		ContentRestriction.size = sizeof(SceNpContentRestriction);
		ContentRestriction.defaultAgeRestriction = DefaultAgeRestriction;
		ContentRestriction.ageRestrictionCount = CountryAgeRestrictions.Num();
		SceNpAgeRestriction* ageRestriction = CountryAgeRestrictions.Num() > 0 ? new SceNpAgeRestriction[CountryAgeRestrictions.Num()] : nullptr;
		for (int Idx = 0; Idx < CountryAgeRestrictions.Num(); Idx++)
		{
			ageRestriction[Idx].age = CountryAgeRestrictions[Idx].Age;
			FMemory::Memset(ageRestriction[Idx].countryCode, 0x00);
			FMemory::Memcpy(&ageRestriction[Idx].countryCode.data, TCHAR_TO_ANSI(*CountryAgeRestrictions[Idx].Country), SCE_NP_COUNTRY_CODE_LENGTH);
		}
		ContentRestriction.ageRestriction = ageRestriction;
		int32 Ret = sceNpSetContentRestriction(&ContentRestriction);
		if (ageRestriction != nullptr)
		{
			delete[] ageRestriction;
		}
		check(Ret == SCE_OK);
	}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST 
	ConsoleInput.Initialize();
#endif
	// Force crash for testing the core dump handler
 	//int32* Pointer = nullptr;
 	//*Pointer = 0x12345678;

	// initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(IInputDeviceModule::GetModularFeatureName());
	for (auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt)
	{
		TSharedPtr<IInputDevice> Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
		InputInterface->AddExternalInputDevice(Device);
	}

	SystemEventGatherThreadRunnable = MakeShareable(new SystemEventGatherRunnable());
	SystemEventGatherThreadRunnable->TriggerGetEvents();
}


void FPS4Application::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}


void FPS4Application::PollGameDeviceState( const float TimeDelta )
{
	// Poll game device state and send new events
	InputInterface->Tick( TimeDelta );
	InputInterface->SendControllerEvents();

/* 
	@todo: It's not clear where this should be called - rendering thread maybe so we get messages
   	even during blocking loads?
	
	// check for events/messages from the system
	SceSystemServiceStatus Status;
	sceSystemServiceGetStatus(&Status);

	for (int32 EventIndex = 0; EventIndex < Status.eventNum; EventIndex++)
	{
		SceSystemServiceEvent Event;
		sceSystemServiceReceiveEvent(&Event);
		switch (Event.eventType)
		{
		case SCE_SYSTEM_SERVICE_EVENT_DISPLAY_SAFE_AREA_UPDATE:
			{
				SceSystemServiceDisplaySafeAreaInfo Info;
				sceSystemServiceGetDisplaySafeAreaInfo(&Info);
				SafeAreaPercentage = Info.ratio;

				UE_LOG(LogPS4, Log, TEXT("Safe zone margin has been updated to %f"), SafeAreaPercentage);
			}
			break;
		}
	}
*/
}

void FPS4Application::Tick(const float TimeDelta)
{
	//only trigger a new get if the old one completed.
	if (SystemEventGatherThreadRunnable->ProcessResults_GameThread())
	{
		SystemEventGatherThreadRunnable->TriggerGetEvents();
	}
	
	if (CachedPlayTogetherHostEventParam.inviteeListLen > 0)
	{
		if (OnPlayTogetherSystemServiceEventDelegate.IsBound())
		{
			OnPlayTogetherSystemServiceEventDelegate.Broadcast(CachedPlayTogetherHostEventParam);
			FMemory::Memzero(&CachedPlayTogetherHostEventParam, sizeof(SceNpPlayTogetherHostEventParamA));
		}
	}
	
	//generate event that will end up calling 'QueryCursor' in slate to support proper reporting of the cursor's type.
	MessageHandler->OnCursorSet();
}

bool FPS4Application::IsGamepadAttached() const
{
	return InputInterface->IsGamepadAttached();
}

inline FPlatformRect GetWorkAreaInternal(const FPlatformRect& CurrentWindow)
{
	//@todo ps4: Use the actual device settings here.
	static bool bDetectedResolution = false;
	static FPlatformRect WorkArea;

	if( bDetectedResolution == false )
	{
		WorkArea.Left = 0;
		WorkArea.Top = 0;
		WorkArea.Right = 1920;
		WorkArea.Bottom = 1080;

		if (GPS4Allow4kOutput)
		{
			SceVideoOutResolutionStatus ResolutionStatus;
			int Handle = sceVideoOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
			if (Handle > 0)
			{
				if (SCE_OK == sceVideoOutGetResolutionStatus(Handle, &ResolutionStatus) && ResolutionStatus.fullHeight > 1080)
				{
					// 4k Mode
					// TODO: Get target resolution from profile settings
					WorkArea.Right = 3840;
					WorkArea.Bottom = 2160;
				}
				sceVideoOutClose(Handle);
			}
		}

		bDetectedResolution = true;
	}

	return WorkArea;
}


FPlatformRect FPS4Application::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	return GetWorkAreaInternal(CurrentWindow);
}


void FDisplayMetrics::GetDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect = 
		GetWorkAreaInternal(OutDisplayMetrics.PrimaryDisplayWorkAreaRect);

	OutDisplayMetrics.PrimaryDisplayWidth = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right;
	OutDisplayMetrics.PrimaryDisplayHeight = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom;

	// calculate the size the safe area - on PS4 there's only one safe zone, so we use it
	// for TitleSafe and ActionSafe

	// @todo: SCE_SYSTEM_SERVICE_EVENT_DISPLAY_SAFE_AREA_UPDATE isn't currently being sent by OS (1.510), so we manually check it
	// use a local variable since we can't update SafeAreaPercentage in this const function
	float LocalSafePercent = GDefaultSafeAreaPercentage;
	SceSystemServiceDisplaySafeAreaInfo Info;
	if (sceSystemServiceGetDisplaySafeAreaInfo(&Info) == SCE_OK)
	{
		LocalSafePercent = Info.ratio;
	}

	float SafePaddingX = OutDisplayMetrics.PrimaryDisplayWidth * ((1.0f - LocalSafePercent) * 0.5f);
	float SafePaddingY = OutDisplayMetrics.PrimaryDisplayHeight * ((1.0f - LocalSafePercent) * 0.5f);

	OutDisplayMetrics.TitleSafePaddingSize = OutDisplayMetrics.ActionSafePaddingSize = 
		FVector4(SafePaddingX, SafePaddingY, SafePaddingX, SafePaddingY);
}


int32 FPS4Application::GetUserIndex(SceUserServiceUserId UserID) const
{
	FScopeLock UserLock(&UserCriticalSection);
	if (UserID == SCE_USER_SERVICE_USER_ID_INVALID)
	{
		return INDEX_NONE; 
	}

	for (int32 Index = 0; Index < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++Index)
	{
		if (UserIDs[Index] == UserID)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}


SceUserServiceUserId FPS4Application::GetUserID(int32 UserIndex) const
{
	FScopeLock UserLock(&UserCriticalSection);

	// INDEX_NONE is a common Unrealism for 'unknown user'
	if (UserIndex == INDEX_NONE)
	{
		return SCE_USER_SERVICE_USER_ID_INVALID;
	}
	check(UserIndex >= 0);
	check(UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS);

	return UserIDs[UserIndex];
}


int32 FPS4Application::InsertUserID(SceUserServiceUserId NewUserId)
{
	FScopeLock UserLock(&UserCriticalSection);

	int32 UserIndex = INDEX_NONE;

	// We can't just use the index from sceUserServiceGetLoginUserIdList, because Sony will remap that index as people sign in and out.
	// so we have to keep a local mapping here of UserIndex -> UserID
	for (int Index = 0; Index < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++Index)
	{
		if (UserIDs[Index] == SCE_USER_SERVICE_USER_ID_INVALID)
		{
			UserIndex = Index;
			break;
		}
	}
	check(UserIndex != INDEX_NONE);
	check(UserIDs[UserIndex] == SCE_USER_SERVICE_USER_ID_INVALID);
	UserIDs[UserIndex] = NewUserId;

	return UserIndex;
}

void FPS4Application::RemoveUserID(SceUserServiceUserId UserId)
{
	FScopeLock UserLock(&UserCriticalSection);

	int32 UserIndex = INDEX_NONE;

	// User is logged out, so it won't be in the SceUserServiceLoginUserIdList. Have to find it in our own mapping.
	for (int32 Index = 0; Index < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++Index)
	{
		if (UserIDs[Index] == UserId)
		{
			UserIndex = Index;
			break;
		}
	}
	check(UserIndex != INDEX_NONE);

	UserIDs[UserIndex] = SCE_USER_SERVICE_USER_ID_INVALID;
}

void FPS4Application::SetIsInBackground(bool InIsInBackground)
{
	check(IsInGameThread());
	if (bIsInBackground != InIsInBackground)
	{
		bIsInBackground = InIsInBackground;
		if (bIsInBackground)
		{
			// let the application know, so it can do things like preemptively pause the game
			FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
		}
		else
		{
			FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
			CheckForSafeAreaChanges();
		}
	}
}

int32 FPS4Application::ConnectControllerStateToUser(int32 UserID, int32 UserIndex)
{
	return InputInterface->ConnectControllerStateToUser(UserID, UserIndex);
}

void FPS4Application::DisconnectControllerStateFromUser(int32 UserID, int32 UserIndex)
{
	InputInterface->DisconnectControllerStateFromUser(UserID, UserIndex);
}

void FPS4Application::ResetControllerOrientation(int32 UserIndex)
{
	InputInterface->ResetControllerOrientation(UserIndex);
}

IInputInterface* FPS4Application::GetInputInterface()
{
	// NOTE: This does not increase the reference count, so don't cache the result
	return InputInterface.Get();
}

TSharedRef< FGenericWindow > FPS4Application::MakeWindow()
{
	if (!MainWindow.IsValid())
	{
		MainWindow = MakeShareable(new FPS4Window());
	}
	else
	{
		UE_LOG(LogPS4, Warning, TEXT("Attempting to create more than one window on PS4"));
	}
	return MainWindow.ToSharedRef();
}

FModifierKeysState FPS4Application::GetModifierKeys() const
{
	uint32 KeyCodeStatus = InputInterface->GetImeKeyCodeStatus();
	const bool bIsLeftShiftDown = (KeyCodeStatus & SCE_IME_KEYCODE_STATE_MODIFIER_L_SHIFT) != 0;
	const bool bIsRightShiftDown = (KeyCodeStatus & SCE_IME_KEYCODE_STATE_MODIFIER_R_SHIFT) != 0;
	const bool bIsLeftControlDown = (KeyCodeStatus & SCE_IME_KEYCODE_STATE_MODIFIER_L_CTRL) != 0;
	const bool bIsRightControlDown = (KeyCodeStatus & SCE_IME_KEYCODE_STATE_MODIFIER_R_CTRL) != 0;
	const bool bIsLeftAltDown = (KeyCodeStatus & SCE_IME_KEYCODE_STATE_MODIFIER_L_ALT) != 0;
	const bool bIsRightAltDown = (KeyCodeStatus & SCE_IME_KEYCODE_STATE_MODIFIER_R_ALT) != 0;
	const bool bAreCapsLocked = (KeyCodeStatus & SCE_IME_KEYCODE_STATE_LED_CAPS_LOCK) != 0;

	return FModifierKeysState(bIsLeftShiftDown, bIsRightShiftDown, bIsLeftControlDown, bIsRightControlDown, bIsLeftAltDown, bIsRightAltDown, false, false, bAreCapsLocked);
}

void FPS4Application::RegisterConsoleCommandListener( const FOnConsoleCommandListener& InListener )
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST 
	if ( InListener.IsBound() )
	{
		CommandListeners.Add( InListener );
	}
#endif //!UE_BUILD_SHIPPING && !UE_BUILD_TEST 
}

void FPS4Application::AddPendingConsoleCommand( const FString& InCommand )
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST 
	if ( CommandListeners.IsBound() )
	{
		CommandListeners.Broadcast( InCommand );
	}
#endif //!UE_BUILD_SHIPPING && !UE_BUILD_TEST 
}

void FPS4Application::CheckForSafeAreaChanges()
{
	SceSystemServiceDisplaySafeAreaInfo Info;
	if (sceSystemServiceGetDisplaySafeAreaInfo(&Info) == SCE_OK)
	{
		UE_LOG(LogPS4, Log, TEXT("Detected safe zone change"));
		if (PS4Application->SafeAreaPercentage != Info.ratio)
		{
			PS4Application->SafeAreaPercentage = Info.ratio;
			FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
		}
	}
}
struct FPS4PlatformCrashContext : public FGenericCrashContext
{
	virtual void AddPlatformSpecificProperties() const override
	{
#if USE_NEW_PS4_MEMORY_SYSTEM == 0
		extern uint64 GGarlicHeapSize;
		extern uint64 GOnionHeapSize;
		extern uint64 GMainHeapSize;
#endif

		//add data for the PS4 server to parse out into the 'Windows Error Report'.
		AddCrashProperty(TEXT("PS4BranchName"), *FApp::GetBranchName());
		AddCrashProperty(TEXT("PS4Changelist"), FEngineVersion::Current().GetChangelist());

		uint32 LocalUserId = -1;
		if (FPS4Application::IsInitialized())
		{
			LocalUserId = FPS4Application::GetPS4Application()->GetUserID(0);
		}

		FString LocalUserString = FString::Printf(TEXT("0x%x"), LocalUserId);
		AddCrashProperty(TEXT("PS4LocalInitialUser"), *LocalUserString);

#if USE_NEW_PS4_MEMORY_SYSTEM == 0
		// TODO: Fix this?
		AddCrashProperty( TEXT( "Memory.MainHeapSize" ), (float)((double)GMainHeapSize / (1024.0 * 1024.0)));
		AddCrashProperty( TEXT( "Memory.OnionHeapSize" ), (float)((double)GOnionHeapSize / (1024.0 * 1024.0)));
		AddCrashProperty( TEXT( "Memory.GarlicHeapSize" ), (float)((double)GGarlicHeapSize / (1024.0 * 1024.0)));
#endif
	}
};


typedef FPS4PlatformCrashContext FPlatformCrashContext;

struct FPS4CoreDumpHandler
{
	FPS4CoreDumpHandler()
	{
		// Called very early in process lifetime. Hooks our custom crash dump handler so we get orbisdmps with valid crash context user data.
		int32 Ret = sceCoredumpRegisterCoredumpHandler(HandleCoreDump, 16384, nullptr);
		check(Ret == SCE_OK);
	}

	static int32 HandleCoreDump(void* Common)
	{
		int32 RetVal = SCE_OK;
		
		// We only get here when we are crashing.
		GIsCriticalError = true;

		// Stop the heartbeat thread so that it doesn't interfere with crash reporting
		FThreadHeartBeat* HeartBeat = FThreadHeartBeat::GetNoInit();
		if (HeartBeat)
		{
			HeartBeat->Stop();
			FGameThreadHitchHeartBeat::Get().Stop();
		}

		// Switch to malloc crash.
		FPlatformMallocCrash::Get().SetAsGMalloc();

		FPlatformCrashContext CrashContext;
		CrashContext.SerializeContentToBuffer();

		int32 BytesWritten = sceCoredumpWriteUserData(TCHAR_TO_ANSI(*CrashContext.GetBuffer()), CrashContext.GetBuffer().Len() + 1);

		// propagate errors out of the callback.
		if (BytesWritten < 0)
		{
			RetVal = BytesWritten;
		}

		return RetVal;
	}

} GPS4CoreDumpHandler;
