// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4InputInterface.h"
#include "IInputDevice.h"
#include "PS4Application.h"
#include "Misc/ConfigCacheIni.h"
#include "IHapticDevice.h"

#include <pad.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <user_service.h>
#include <libsysmodule.h>
#include <libime.h>

int32 GPS4EnableMouse = 0;
static FAutoConsoleVariableRef CVarEnableMouse(
	TEXT( "PS4.EnableMouse" ),
	GPS4EnableMouse,
	TEXT( "Whether mouse support is enabled. Cannot be changed at runtime.\n" )
	TEXT( " 0: disabled (default)\n" )
	TEXT( " 1: enabled" ),
	ECVF_ReadOnly );

TSharedRef< FPS4InputInterface > FPS4InputInterface::Create(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor)
{	
	return MakeShareable( new FPS4InputInterface( InMessageHandler, InCursor ) );
}

FPS4InputInterface::FPS4InputInterface(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor)
	: ImeKeyCodeStatus(0)
	, MessageHandler(InMessageHandler)
	, Cursor(InCursor)
{
	int32 Result;

#if SUPPORT_IME_KEYBOARD
	Result = sceSysmoduleLoadModule(SCE_SYSMODULE_LIBIME);
	checkf(Result == SCE_OK, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_LIBIME) failed with: 0x%x"), Result);

	bIMENeedsOpen = true;
	bIMENeedsClose = false;
#endif

	// Enable mouse support
	if( GPS4EnableMouse )
	{
	Result = sceSysmoduleLoadModule( SCE_SYSMODULE_MOUSE );
	checkf( Result == SCE_OK, TEXT( "sceSysmoduleLoadModule(SCE_SYSMODULE_MOUSE) failed with: 0x%x" ), Result );
	sceMouseInit();
	}

	{
		// Configure touch and mouse events
		bool bDS4TouchEvents = false;
		bool bDS4TouchAxisButtons = false;
		bool bDS4MouseEvents = false;
		bool bDS4MotionEvents = false;

		if( GConfig )
		{
			// Configure PS4Controllers to emit touch events from the DS4 touchpad if the application wants them.
			GConfig->GetBool( TEXT( "PS4Application" ), TEXT( "bDS4TouchEvents" ), bDS4TouchEvents, GEngineIni );

			// Configure PS4Controllers to emit axis events from the DS4 touchpad if the application wants them.
			GConfig->GetBool( TEXT("PS4Application"), TEXT("bDS4TouchAxisButtons"), bDS4TouchAxisButtons, GEngineIni );

			// Configure PS4Controllers to emit mouse events from the DS4 touchpad if the application wants them
			GConfig->GetBool( TEXT( "PS4Application" ), TEXT( "bDS4MouseEvents" ), bDS4MouseEvents, GEngineIni );

			// Configure PS4Controllers to emit motion events from the DS4 if the application wants them
			GConfig->GetBool(TEXT("PS4Application"), TEXT("bDS4MotionEvents"), bDS4MotionEvents, GEngineIni);			
		}

		Controllers.SetEmitTouchEvents( bDS4TouchEvents );
		Controllers.SetEmitTouchAxisEvents( bDS4TouchAxisButtons );
		Controllers.SetEmitMouseEvents( bDS4MouseEvents );
		Controllers.SetEmitMotionEvents( bDS4MotionEvents );
	}

	Controllers.SetCursor(Cursor);

	// init dialog for virtual keyboard text widget support.
	Result = sceSysmoduleLoadModule(SCE_SYSMODULE_IME_DIALOG);
	checkf(Result == SCE_OK, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_IME_DIALOG) failed with: 0x%x"), Result);
}


void FPS4InputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetMessageHandler(InMessageHandler);
	}
}

void FPS4InputInterface::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		ExternalInputDevices.Add(InputDevice);
	}
}

void FPS4InputInterface::Tick(float DeltaTime)
{
#if SUPPORT_IME_KEYBOARD
	// the docs for sceImeKeyboardOpen say that SCE_IME_KEYBOARD_EVENT_ABORT may come at any time.  In these cases we must
	// close and reopen IME.
	if (bIMENeedsClose)
	{		
		sceImeKeyboardClose(SCE_USER_SERVICE_USER_ID_EVERYONE);
		bIMENeedsOpen = true;
		bIMENeedsClose = false;
	}

	// sceImeKeyboardOpen may fail and require a retry
	if (bIMENeedsOpen)
	{
		SceUserServiceUserId UserId;
		int Result = sceUserServiceGetInitialUser( &UserId );
		if( Result == SCE_OK )
		{
			SceImeKeyboardParam KeyboardParams;
			KeyboardParams.option = SCE_IME_KEYBOARD_OPTION_REPEAT | SCE_IME_KEYBOARD_OPTION_REPEAT_EACH_KEY;
			KeyboardParams.arg = this;
			KeyboardParams.handler = FPS4InputInterface::KeyboardEventCallback;
			FMemory::Memset(KeyboardParams.reserved1, 0x00, sizeof(KeyboardParams.reserved1));
			FMemory::Memset(KeyboardParams.reserved2, 0x00, sizeof(KeyboardParams.reserved2));
			int32 Ret = sceImeKeyboardOpen(UserId, &KeyboardParams);
			bIMENeedsOpen = Ret != SCE_OK;
		}
		else
		{
			bIMENeedsOpen = false;
		}
	}
#endif

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->Tick(DeltaTime);
	}
}

bool FPS4InputInterface::IsGamepadAttached() const
{
	if (Controllers.IsGamepadAttached())
	{
		return true;
	}

	for (auto DeviceIt = ExternalInputDevices.CreateConstIterator(); DeviceIt; ++DeviceIt)
	{
		if ((*DeviceIt)->IsGamepadAttached())
		{
			return true;
		}
	}

	return false;
}

void FPS4InputInterface::SendControllerEvents()
{
	Controllers.SendControllerEvents(MessageHandler);

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SendControllerEvents();
	}

#if SUPPORT_IME_KEYBOARD
	sceImeUpdate(FPS4InputInterface::KeyboardEventCallback);
#endif
}


// IInputInterface methods

void FPS4InputInterface::SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	Controllers.SetForceFeedbackChannelValue(ControllerId, ChannelType, Value);

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetChannelValue(ControllerId, ChannelType, Value);
	}
}


void FPS4InputInterface::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
{
	Controllers.SetForceFeedbackChannelValues(ControllerId, Values);

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetChannelValues(ControllerId, Values);
	}
}

void FPS4InputInterface::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	// I think this is sort of what you would do to support haptics on the controller.
	//if (Hand == EControllerHand::Pad)
	//{
	//	Controllers.SetForceFeedbackChannelValues(ControllerId, Values);
	//}

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		IHapticDevice* HapticDevice = (*DeviceIt)->GetHapticDevice();
		if (HapticDevice != nullptr)
		{
			HapticDevice->SetHapticFeedbackValues(ControllerId, Hand, Values);
		}
	}
}

void FPS4InputInterface::SetLightColor(int32 ControllerId, FColor Color)
{
	Controllers.SetLightColor(ControllerId, Color);
}

void FPS4InputInterface::ResetControllerOrientation(int32 UserIndex)
{
	Controllers.ResetControllerOrientation(UserIndex);
}

int32 FPS4InputInterface::ConnectControllerStateToUser(int32 UserID, int32 UserIndex)
{
	return Controllers.ConnectStateToUser(UserID, UserIndex);
}

void FPS4InputInterface::DisconnectControllerStateFromUser(int32 UserID, int32 UserIndex)
{
	Controllers.DisconnectStateFromUser(UserID, UserIndex);
}

#if SUPPORT_IME_KEYBOARD
void FPS4InputInterface::KeyboardEventCallback(void* Arg, const SceImeEvent* Event)
{
	check(Arg);
	FPS4InputInterface& InputInterface = *(FPS4InputInterface*)Arg;

	switch (Event->id)
	{
		case SCE_IME_KEYBOARD_EVENT_OPEN:
			UE_LOG(LogPS4, Log, TEXT("Keyboard library is opened[uid=0x%08x]:Resource ID(%d, %d, %d)\n"), Event->param.resourceIdArray.userId, Event->param.resourceIdArray.resourceId[0], Event->param.resourceIdArray.resourceId[1], Event->param.resourceIdArray.resourceId[2]);
			break;
		case SCE_IME_KEYBOARD_EVENT_KEYCODE_DOWN:
			/*
			UE_LOG(LogPS4, Log, TEXT("Key DOWN:\"0x%04X\"(%lc), M 0x%05X(%d%d%d%d-%d%d%d%d)\n"), Event->param.keycode.keycode, (Event->param.keycode.status & 2) ? Event->param.keycode.character : '-',
				Event->param.keycode.status,
				(Event->param.keycode.status >> 15) & 1, (Event->param.keycode.status >> 14) & 1, (Event->param.keycode.status >> 13) & 1, (Event->param.keycode.status >> 12) & 1,
				(Event->param.keycode.status >> 11) & 1, (Event->param.keycode.status >> 10) & 1, (Event->param.keycode.status >> 9) & 1, (Event->param.keycode.status >> 8) & 1);
			*/
			InputInterface.ImeKeyCodeStatus = Event->param.keycode.status;
			InputInterface.MessageHandler->OnKeyDown(Event->param.keycode.keycode, Event->param.keycode.character, false);
			if( Event->param.keycode.character != 0 )
			{
				InputInterface.MessageHandler->OnKeyChar( Event->param.keycode.character, false );
			}
			break;
		case SCE_IME_KEYBOARD_EVENT_KEYCODE_UP:
			//UE_LOG(LogPS4, Log, TEXT("Key UP:\"0x%04X\"(%lc), M 0x%05X\n"), Event->param.keycode.keycode, (Event->param.keycode.status & 2) ? Event->param.keycode.character : '-', Event->param.keycode.status);			
			InputInterface.ImeKeyCodeStatus = Event->param.keycode.status;
			InputInterface.MessageHandler->OnKeyUp(Event->param.keycode.keycode, Event->param.keycode.character, false);
			break;
		case SCE_IME_KEYBOARD_EVENT_KEYCODE_REPEAT:
			//UE_LOG(LogPS4, Log, TEXT("Key REPEAT:\"0x%04X\"(%lc)\n"), Event->param.keycode.keycode, (Event->param.keycode.status & 2) ? Event->param.keycode.character : '-');
			InputInterface.ImeKeyCodeStatus = Event->param.keycode.status;
			InputInterface.MessageHandler->OnKeyDown(Event->param.keycode.keycode, Event->param.keycode.character, true);
			if( Event->param.keycode.character != 0 )
			{
				InputInterface.MessageHandler->OnKeyChar( Event->param.keycode.character, true );
			}
			break;
		case SCE_IME_KEYBOARD_EVENT_CONNECTION:
			UE_LOG(LogPS4, Log, TEXT("Keyboard is connected[uid=0x%08x]:Resource ID(%d, %d, %d)\n"), Event->param.resourceIdArray.userId, Event->param.resourceIdArray.resourceId[0], Event->param.resourceIdArray.resourceId[1], Event->param.resourceIdArray.resourceId[2]);
			break;
		case SCE_IME_KEYBOARD_EVENT_DISCONNECTION:
			UE_LOG(LogPS4, Log, TEXT("Keyboard is disconnected[uid=0x%08x]:Resource ID(%d, %d, %d)\n"), Event->param.resourceIdArray.userId, Event->param.resourceIdArray.resourceId[0], Event->param.resourceIdArray.resourceId[1], Event->param.resourceIdArray.resourceId[2]);
			break;
		case SCE_IME_KEYBOARD_EVENT_ABORT:			
			UE_LOG(LogPS4, Log, TEXT("Keyboard manager was aborted.  Keyboard will cease functioning.\n"));
			InputInterface.bIMENeedsClose = true;
			break;
		default:
			UE_LOG(LogPS4, Log, TEXT("Invalid Event: 0x%x\n"), Event->id);
			break;
	}
}
#endif

bool FPS4InputInterface::HasExternalKeyboard() const
{
	bool bHasExternal = false;
#if SUPPORT_IME_KEYBOARD
	SceUserServiceUserId UserId;
	int32 Result = sceUserServiceGetInitialUser(&UserId);
	if (Result == SCE_OK)
	{
		SceImeKeyboardResourceIdArray KeyboardArray;
		Result = sceImeKeyboardGetResourceId(UserId, &KeyboardArray);
		if (Result == SCE_OK)
		{
			for (int32 i = 0; i < SCE_IME_KEYBOARD_MAX_NUMBER; ++i)
			{
				SceImeKeyboardInfo Info;
				Result = sceImeKeyboardGetInfo(KeyboardArray.resourceId[i], &Info);
				if (Result == SCE_OK)
				{
					if (Info.device == SCE_IME_KEYBOARD_DEVICE_TYPE_KEYBOARD && Info.status == SCE_IME_KEYBOARD_STATE_CONNECTED)
					{
						bHasExternal = true;
						break;
					}
				}
			}
		}
	}
#endif
	return bHasExternal;
}