// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "SonyInputInterface.h"
#include "SonyCursor.h"
#include "HAL/ThreadingBase.h"
#include "HAL/PlatformMallocCrash.h"
#include "Misc/EngineVersion.h"
#include "IInputDeviceModule.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/ThreadSafeBool.h"

#include <libime.h>
#include <coredump.h>
#include <system_service.h>
#include <video_out.h>

FSonyApplication* FSonyApplication::Singleton = nullptr;

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
		SystemEventGatherTaskThread = FRunnableThread::Create(this, TEXT("SystemEventGatherThreadSony"));
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

	bool ProcessResults_GameThread(FSonyApplication* SonyApp)
	{
		check(IsInGameThread());

		bool bProcessed = false;
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
							if (SonyApp->GetUserIndex(LoginEvent.UserId) == INDEX_NONE)
							{
								// Insert the new user ID before we broadcast the user changed event.
								int32 UserIndex = SonyApp->InsertUserID(LoginEvent.UserId);
								UE_LOG(LogSony, Log, TEXT("Sony User Service Event: LogIn - UserIndex: %d, UserID: %d"), UserIndex, LoginEvent.UserId);
								FCoreDelegates::OnUserLoginChangedEvent.Broadcast(true, LoginEvent.UserId, UserIndex);
							}
						}
						break;

					case ELoginChangeEventType::SystemLogOut:
						{
							// Guard against duplicate events for the same user
							int32 UserIndex = SonyApp->GetUserIndex(LoginEvent.UserId);
							if (UserIndex != INDEX_NONE)
							{
								UE_LOG(LogSony, Log, TEXT("Sony User Service Event: LogOut - UserIndex: %d, UserID: %d"), UserIndex, LoginEvent.UserId);
								FCoreDelegates::OnUserLoginChangedEvent.Broadcast(false, LoginEvent.UserId, UserIndex);

								// Remove the user id from the array after we've broadcast the user changed event.
								SonyApp->RemoveUserID(LoginEvent.UserId);
							}
						}
						break;
					}
				}
				DetectedLoginChangeEvents.Reset();
			}

			SonyApp->SetIsInBackground(bIsInBackground);

			for (SceSystemServiceEvent& SystemEvent : SystemServiceEvents)
			{
				if (!SonyApp->HandleSystemServiceEvent(SystemEvent))
				{
					switch (SystemEvent.eventType)
					{
					case SCE_SYSTEM_SERVICE_EVENT_ON_RESUME:
						// detect transition from 'Suspended' state to 'ForegroundState'
						// it is not possible to detect the transition TO Suspended or Termination state according to:
						// https://ps4.scedev.net/docs/ps4-en,Programming-Startup_Guide-orbis,Application_State_Transitions/1
						// We could try to hack something up based on the rules about users, but this might miss some other suspension case that
						// only the system software knows about.  Therefore, we never broadcast ApplicationWillEnterBackgroundDelegate or ApplicationWillTerminateDelegate
						FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();

						SonyApp->CheckForSafeAreaChanges();
						break;
					}
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
	FThreadSafeBool bIsRunning;
	FThreadSafeBool bResultsReady;
};

FSonyApplication::~FSonyApplication()
{
#if !UE_BUILD_SHIPPING
	ConsoleInput.Finalize();
#endif
}

FSonyApplication::FSonyApplication()
: GenericApplication( MakeShareable(new FSonyCursor()) )
	, InputInterface( FSonyInputInterface::Create( MessageHandler, Cursor ) )
	, SafeAreaPercentage(DefaultSafeAreaPercentage) // a good default value in case user never sets it
	, bIsInBackground(false)
{
	for (int32 Index = 0; Index < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++Index)
	{
		UserIDs[Index] = SCE_USER_SERVICE_USER_ID_INVALID;
	}

	// retrieve the safe zone set by the user
	SceSystemServiceDisplaySafeAreaInfo Info;
	int32 Result = sceSystemServiceGetDisplaySafeAreaInfo(&Info);

	if (Result == SCE_OK)
	{
		SafeAreaPercentage = Info.ratio;
	}

	FDisplayMetrics::RebuildDisplayMetrics(LastKnownMetrics);

#if !UE_BUILD_SHIPPING
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

	//lock the mouse to the screen by default
	Cursor->Lock(nullptr);
}


void FSonyApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( MessageHandler );
}


void FSonyApplication::PollGameDeviceState( const float TimeDelta )
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

				UE_LOG(LogSony, Log, TEXT("Safe zone margin has been updated to %f"), SafeAreaPercentage);
			}
			break;
		}
	}
*/
}

void FSonyApplication::Tick(const float TimeDelta)
{
	//only trigger a new get if the old one completed.
	if (SystemEventGatherThreadRunnable->ProcessResults_GameThread(this))
	{
		SystemEventGatherThreadRunnable->TriggerGetEvents();
	}

	//generate event that will end up calling 'QueryCursor' in slate to support proper reporting of the cursor's type.
	MessageHandler->OnCursorSet();
}

bool FSonyApplication::IsMouseAttached() const
{
	return InputInterface->IsMouseAttached();
}

bool FSonyApplication::IsGamepadAttached() const
{
	return InputInterface->IsGamepadAttached();
}


int32 FSonyApplication::GetUserIndex(SceUserServiceUserId UserID) const
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


SceUserServiceUserId FSonyApplication::GetUserID(int32 UserIndex) const
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


int32 FSonyApplication::InsertUserID(SceUserServiceUserId NewUserId)
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

void FSonyApplication::RemoveUserID(SceUserServiceUserId UserId)
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

void FSonyApplication::SetIsInBackground(bool InIsInBackground)
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

int32 FSonyApplication::ConnectControllerStateToUser(int32 UserID, int32 UserIndex)
{
	return InputInterface->ConnectControllerStateToUser(UserID, UserIndex);
}

void FSonyApplication::DisconnectControllerStateFromUser(int32 UserID, int32 UserIndex)
{
	InputInterface->DisconnectControllerStateFromUser(UserID, UserIndex);
}

void FSonyApplication::ResetControllerOrientation(int32 UserIndex)
{
	InputInterface->ResetControllerOrientation(UserIndex);
}

IInputInterface* FSonyApplication::GetInputInterface()
{
	// NOTE: This does not increase the reference count, so don't cache the result
	return InputInterface.Get();
}

TSharedRef< FGenericWindow > FSonyApplication::MakeWindow()
{
	TSharedRef<FSonyWindow> Window = MakeShareable(new FSonyWindow());
	Window.Get().SetVideoOutBusType(SCE_VIDEO_OUT_BUS_TYPE_MAIN);
	return Window;
}

FModifierKeysState FSonyApplication::GetModifierKeys() const
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

void FSonyApplication::RegisterConsoleCommandListener( const FOnConsoleCommandListener& InListener )
{
#if !UE_BUILD_SHIPPING
	if ( InListener.IsBound() )
	{
		CommandListeners.Add( InListener );
	}
#endif 
}

void FSonyApplication::AddPendingConsoleCommand( const FString& InCommand )
{
#if !UE_BUILD_SHIPPING
	if ( CommandListeners.IsBound() )
	{
		CommandListeners.Broadcast( InCommand );
	}
#endif 
}

void FSonyApplication::CheckForSafeAreaChanges()
{
	SceSystemServiceDisplaySafeAreaInfo Info;
	if (sceSystemServiceGetDisplaySafeAreaInfo(&Info) == SCE_OK)
	{
		UE_LOG(LogSony, Log, TEXT("Detected safe zone change"));
		if (SafeAreaPercentage != Info.ratio)
		{
			SafeAreaPercentage = Info.ratio;
			FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
		}
	}
}