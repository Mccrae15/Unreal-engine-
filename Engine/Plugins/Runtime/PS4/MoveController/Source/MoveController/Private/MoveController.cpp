// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MoveController.h"
#include "IInputDevice.h"
#include "XRMotionControllerBase.h"
#include "IHapticDevice.h"
#include "MoveController.h"
#include "PS4Tracker.h"
#include "IMotionController.h"
#include "PS4Tracker.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "GenericPlatform/IInputInterface.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

#if PLATFORM_PS4
	#include <move.h>
#endif

/******************************************************************************
*
*	Move API common between platforms
*
******************************************************************************/

class MoveAPI
{
public:

	struct EMoveControllerButton
	{
		enum Type
		{
			Square,
			Cross,
			Circle,
			Triangle,
			Move,
			Trigger, 
			Start,
			Select,

			/** Max number of controller buttons */
			TotalButtonCount
		};
	};

	struct FStateData{
		bool	ButtonStates[EMoveControllerButton::TotalButtonCount];
		float	TriggerAnalog;
	};

	/** MoveAPI Initialization and termination */
	static void Init();
	static void Term();

	/** Controller methods */
	static int32 ControllerOpen(int32 UserId, int32 HandIndex);
	static void ControllerClose(int32 Handle);
	static bool ControllerIsReady(int32 Handle);
	static bool ControllerReadStateLatest(int32 Handle, FStateData& State);
	static void ControllerSetVibration(int32 Handle, float intensity);

	/** Return true on valid UserId */
	static bool UserValidate (int32 UserId);
};


#if PLATFORM_PS4

/******************************************************************************
*
*	PS4 Move API implementation
*
******************************************************************************/

#include <libsysmodule.h>

#define CONTROLLERS_PER_PLAYER	SCE_MOVE_MAX_CONTROLLERS_PER_PLAYER
#define MAX_LOGIN_PLAYERS		SCE_USER_SERVICE_MAX_LOGIN_USERS

//=============================================================================
void MoveAPI::Init()
{
	int32 Ret = SCE_OK;
	Ret = sceSysmoduleLoadModule(SCE_SYSMODULE_MOVE);
	checkf(Ret >= 0, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_MOVE) failed: 0x%x"), Ret);

	Ret = sceMoveInit();
	checkf(Ret >= 0, TEXT("sceMoveInit() failed: 0x%x"), Ret);
}

//=============================================================================
void MoveAPI::Term()
{
	int32 Ret = sceMoveTerm();
	checkf(Ret >= 0, TEXT("sceMoveTerm() failed: 0x%x"), Ret);
}

//=============================================================================
int32 MoveAPI::ControllerOpen(int32 UserID, int32 HandIndex)
{
	return sceMoveOpen(UserID, SCE_MOVE_TYPE_STANDARD, HandIndex);
}

//=============================================================================
void MoveAPI::ControllerClose(int32 Handle)
{
	int32 Ret = sceMoveClose(Handle);
	check(Ret == SCE_OK);
}

//=============================================================================
bool MoveAPI::ControllerIsReady(int32 Handle)
{
	SceMoveDeviceInfo DeviceInfo;
	int32 Ret = sceMoveGetDeviceInfo(Handle, &DeviceInfo);
	return (Ret == SCE_OK);
}

//=============================================================================
bool MoveAPI::ControllerReadStateLatest(int32 Handle, FStateData& State)
{
	SceMoveData MoveData;
	int32 Ret = sceMoveReadStateLatest(Handle, &MoveData);
	if (Ret != SCE_OK)
	{
		return false;
	}

	State.ButtonStates[ EMoveControllerButton::Square ]   = !!(MoveData.pad.digitalButtons & SCE_MOVE_BUTTON_SQUARE);
	State.ButtonStates[ EMoveControllerButton::Cross ]    = !!(MoveData.pad.digitalButtons & SCE_MOVE_BUTTON_CROSS);
	State.ButtonStates[ EMoveControllerButton::Circle ]   = !!(MoveData.pad.digitalButtons & SCE_MOVE_BUTTON_CIRCLE);
	State.ButtonStates[ EMoveControllerButton::Triangle ] = !!(MoveData.pad.digitalButtons & SCE_MOVE_BUTTON_TRIANGLE);
	State.ButtonStates[ EMoveControllerButton::Move ]     = !!(MoveData.pad.digitalButtons & SCE_MOVE_BUTTON_MOVE);
	State.ButtonStates[ EMoveControllerButton::Trigger ]  = !!(MoveData.pad.digitalButtons & SCE_MOVE_BUTTON_T);
	State.ButtonStates[ EMoveControllerButton::Start ]    = !!(MoveData.pad.digitalButtons & SCE_MOVE_BUTTON_START);
	State.ButtonStates[ EMoveControllerButton::Select ]   = !!(MoveData.pad.digitalButtons & SCE_MOVE_BUTTON_SELECT);

	State.TriggerAnalog = float(MoveData.pad.analogT) / 255.0f;

	return true;
}

//=============================================================================
void MoveAPI::ControllerSetVibration(int32 Handle, float intensity)
{
	sceMoveSetVibration(Handle, intensity);
}

//=============================================================================
bool MoveAPI::UserValidate(int32 UserID)
{
	return UserID != SCE_USER_SERVICE_USER_ID_INVALID;
}


#else

/******************************************************************************
*
*	Windows Move API implementation
*
******************************************************************************/

#include "WindowsHWrapper.h"
#include "ThirdParty/PS4/HmdClient/include/hmd_client.h"

#define CONTROLLERS_PER_PLAYER	2
#define MAX_LOGIN_PLAYERS		4
#define WINDOWS_USER_ID			1

// Flags pulled from SCE move.h
#define MOVE_BUTTON_SQUARE      (1 << 7)  ///< SCE_MOVE_BUTTON_SQUARE
#define MOVE_BUTTON_CROSS       (1 << 6)  ///< SCE_MOVE_BUTTON_CROSS
#define MOVE_BUTTON_CIRCLE      (1 << 5)  ///< SCE_MOVE_BUTTON_CIRCLE
#define MOVE_BUTTON_TRIANGLE    (1 << 4)  ///< SCE_MOVE_BUTTON_TRIANGLE
#define MOVE_BUTTON_MOVE        (1 << 2)  ///< SCE_MOVE_BUTTON_MOVE
#define MOVE_BUTTON_T           (1 << 1)  ///< SCE_MOVE_BUTTON_T
#define MOVE_BUTTON_START       (1 << 3)  ///< SCE_MOVE_BUTTON_START
#define MOVE_BUTTON_SELECT      (1 << 0)  ///< SCE_MOVE_BUTTON_SELECT
#define MOVE_BUTTON_INTERCEPTED (1 << 15) ///< SCE_MOVE_BUTTON_INTERCEPTED

static const TCHAR* MorpheusSettings = TEXT("/Script/MorpheusEditor.MorpheusRuntimeSettings");
static hmdclient_handle HMDClientConnectionHandle = nullptr;
static HMODULE HMDClientDLLHandle = nullptr;

int32 MoveAPI::ControllerOpen(int32 UserId, int32 HandIndex) { return static_cast<int32>((HandIndex == 0) ? IPS4Tracker::EDeviceType::MOTION_LEFT_HAND : IPS4Tracker::EDeviceType::MOTION_RIGHT_HAND); }
void MoveAPI::ControllerClose(int32 Handle) {}
bool MoveAPI::ControllerIsReady(int32 Handle) { return true; }
void MoveAPI::ControllerSetVibration(int32 Handle, float intensity) {}
bool MoveAPI::UserValidate (int32 UserId) { return true; }

//=============================================================================
void MoveAPI::Init()
{
#if PLATFORM_64BITS
	FString WinDir = TEXT("Win64");
#else
	FString WinDir = TEXT("Win32");
#endif

	FString HMDServerAddress;
	if (GConfig->GetString(MorpheusSettings, TEXT("HMDServerAddress"), HMDServerAddress, GEngineIni))
	{
		FString HMDClientDLLDir = FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries"), TEXT("ThirdParty"), TEXT("PS4"), *WinDir);
		FString HMDClientDLLPath = FPaths::Combine(*HMDClientDLLDir, TEXT("hmd_client.dll"));
		HMDClientDLLHandle = LoadLibraryW(*HMDClientDLLPath);
		if (HMDClientDLLHandle)
		{
			HMDClientConnectionHandle = hmdclientConnect(TCHAR_TO_ANSI(*HMDServerAddress), "7899");
		}
	}
}

//=============================================================================
void MoveAPI::Term()
{
	if (HMDClientConnectionHandle)
	{
		hmdclientDisconnect(HMDClientConnectionHandle);
	}

	if (HMDClientDLLHandle)
	{
		FreeLibrary(HMDClientDLLHandle);
	}
}

//=============================================================================
bool MoveAPI::ControllerReadStateLatest(int32 Handle, FStateData& State)
{
	if (!HMDClientConnectionHandle)
	{
		return false;
	}

	HMDServerTackerStates States;
	if (hmdclientGetTrackerStates(HMDClientConnectionHandle, &States) != HMDCLIENT_ERROR_SUCCESS)
	{
		return false;
	}

	const MovePadData& MoveData = States.state[Handle].pad;
	State.ButtonStates[ EMoveControllerButton::Square ]   = !!(MoveData.digital_buttons & MOVE_BUTTON_SQUARE);
	State.ButtonStates[ EMoveControllerButton::Cross ]    = !!(MoveData.digital_buttons & MOVE_BUTTON_CROSS);
	State.ButtonStates[ EMoveControllerButton::Circle ]   = !!(MoveData.digital_buttons & MOVE_BUTTON_CIRCLE);
	State.ButtonStates[ EMoveControllerButton::Triangle ] = !!(MoveData.digital_buttons & MOVE_BUTTON_TRIANGLE);
	State.ButtonStates[ EMoveControllerButton::Move ]     = !!(MoveData.digital_buttons & MOVE_BUTTON_MOVE);
	State.ButtonStates[ EMoveControllerButton::Trigger ]  = !!(MoveData.digital_buttons & MOVE_BUTTON_T);
	State.ButtonStates[ EMoveControllerButton::Start ]    = !!(MoveData.digital_buttons & MOVE_BUTTON_START);
	State.ButtonStates[ EMoveControllerButton::Select ]   = !!(MoveData.digital_buttons & MOVE_BUTTON_SELECT);

	State.TriggerAnalog = float(MoveData.analog_trigger) / 255.0f;

	return true;
}

#endif


/** Move motion controller implementation */
 class FMoveController : public IInputDevice, public FXRMotionControllerBase, public IHapticDevice
{
public:
	FMoveController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);
	virtual ~FMoveController ();

	/** IInputDevice implementation */
	virtual void Tick( float DeltaTime ) override;
	virtual void SendControllerEvents() override;
	virtual void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override { return false; }
	virtual void SetChannelValue (int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetChannelValues (int32 ControllerId, const FForceFeedbackValues &values) override;
	virtual IHapticDevice* GetHapticDevice() override { return this; }

	/** IHapticDevice implementation */
	virtual void SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values) override;
	virtual void GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const override;
	virtual float GetHapticAmplitudeScale() const override;

	/** IMotionController implementation */
	virtual FName GetMotionControllerDeviceTypeName() const override
	{
		const static FName DefaultName(TEXT("PS4MoveController"));
		return DefaultName;
	}
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerId, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override;
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;

	/** User login callback handler */
	void UserLoginEventHandler(bool bLoggingIn, int32 UserID, int32 UserIndex);

	/** Helpers for binding user id/index to controller state */
	void ConnectStateToUser(int32 UserID, int32 UserIndex);
	void DisconnectStateFromUser(int32 UserID, int32 UserIndex);

private:

	/** State for a controller */
	struct FControllerState
	{
		/** Whether or not the motion controller is registered for positional tracking */
		int32 Tracker;

		/** The tracking state of the Move Controller */
		mutable IPS4Tracker::ETrackingStatus TrackingStatus;

		/** Current button states for tracking cross frame changes */
		bool ButtonStates[ MoveAPI::EMoveControllerButton::TotalButtonCount ];

		/** sceMove Device handle */
		int32 Handle;

		/** Current world position */
		mutable FVector	Position;

		/** Current world orientation */
		mutable FQuat Orientation;

		/** Trigger pull intensity: 0 = fully released, 1 = fully pressed  */
		float TriggerAnalog;

		/** Current vibration setting, ranging from 0 - 1 */
		float VibrationIntensity;

		/** Left or right hand which the controller represents */
		EControllerHand Hand;

		/** Button repeat timings */
		double NextRepeatTime[ MoveAPI::EMoveControllerButton::TotalButtonCount ];
	};

	/** State for user connections */
	struct FUserState
	{
		/** Whether or not this user slot has a connection */
		bool bIsConnected;

		/** The controller associated with this user*/
		int32 ControllerId;

		/** User ID issued by the sce API*/
		int32 UserID;

		/** Per controller state, 2 per user for left & right hands */
		FControllerState	ControllerStates[ CONTROLLERS_PER_PLAYER ];
	} UserStates[ MAX_LOGIN_PLAYERS ];

	/** Mapping of controller buttons */
	FGamepadKeyNames::Type Buttons[ CONTROLLERS_PER_PLAYER ][ MoveAPI::EMoveControllerButton::TotalButtonCount ];

	/** handler to send all messages to */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	/** Delay before sending a repeat message after a button was first pressed */
	float InitialButtonRepeatDelay;

	/** Delay before sending a repeat message after a button has been pressed for a while */
	float ButtonRepeatDelay;

	/** Whether stereo rendering was active on the game thread */
	mutable bool bIsStereoEnabled;

	/** Handle for accessing the PS4 positional tracking */
	mutable FPSTrackerHandle PS4Tracker;

	/** Sets the vibration intensity for the specified controller state */
	void SetVibration(FControllerState& ControllerState, float Value);
};

//=============================================================================
FMoveController::FMoveController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	: MessageHandler(InMessageHandler)
	, InitialButtonRepeatDelay(0.2f)
	, ButtonRepeatDelay(0.1f)
	, bIsStereoEnabled(false)
{
	FMemory::Memzero(UserStates, sizeof(UserStates));
	for (auto& UserState : UserStates)
	{
		for (auto& ControllerState : UserState.ControllerStates)
		{
			ControllerState.Tracker = IPS4Tracker::INVALID_TRACKER_HANDLE;
		}
	}

	Buttons[ (int32)EControllerHand::Left ][ MoveAPI::EMoveControllerButton::Square ]   = FGamepadKeyNames::MotionController_Left_FaceButton1;
	Buttons[ (int32)EControllerHand::Left ][ MoveAPI::EMoveControllerButton::Triangle ] = FGamepadKeyNames::MotionController_Left_FaceButton2;
	Buttons[ (int32)EControllerHand::Left ][ MoveAPI::EMoveControllerButton::Circle ]   = FGamepadKeyNames::MotionController_Left_FaceButton3;
	Buttons[ (int32)EControllerHand::Left ][ MoveAPI::EMoveControllerButton::Cross ]    = FGamepadKeyNames::MotionController_Left_FaceButton4;
	Buttons[ (int32)EControllerHand::Left ][ MoveAPI::EMoveControllerButton::Move ]     = FGamepadKeyNames::MotionController_Left_Grip1;
	Buttons[ (int32)EControllerHand::Left ][ MoveAPI::EMoveControllerButton::Trigger ]  = FGamepadKeyNames::MotionController_Left_Trigger;
	Buttons[ (int32)EControllerHand::Left ][ MoveAPI::EMoveControllerButton::Start ]    = FGamepadKeyNames::MotionController_Left_FaceButton5;
	Buttons[ (int32)EControllerHand::Left ][ MoveAPI::EMoveControllerButton::Select ]   = FGamepadKeyNames::Invalid;

	Buttons[ (int32)EControllerHand::Right ][ MoveAPI::EMoveControllerButton::Square ]   = FGamepadKeyNames::MotionController_Right_FaceButton1;
	Buttons[ (int32)EControllerHand::Right ][ MoveAPI::EMoveControllerButton::Triangle ] = FGamepadKeyNames::MotionController_Right_FaceButton2;
	Buttons[ (int32)EControllerHand::Right ][ MoveAPI::EMoveControllerButton::Circle ]   = FGamepadKeyNames::MotionController_Right_FaceButton3;
	Buttons[ (int32)EControllerHand::Right ][ MoveAPI::EMoveControllerButton::Cross ]    = FGamepadKeyNames::MotionController_Right_FaceButton4;	
	Buttons[ (int32)EControllerHand::Right ][ MoveAPI::EMoveControllerButton::Move ]     = FGamepadKeyNames::MotionController_Right_Grip1;
	Buttons[ (int32)EControllerHand::Right ][ MoveAPI::EMoveControllerButton::Trigger ]  = FGamepadKeyNames::MotionController_Right_Trigger;
	Buttons[ (int32)EControllerHand::Right ][ MoveAPI::EMoveControllerButton::Start ]    = FGamepadKeyNames::MotionController_Right_FaceButton5;
	Buttons[ (int32)EControllerHand::Right ][ MoveAPI::EMoveControllerButton::Select ]   = FGamepadKeyNames::Invalid;

	MoveAPI::Init();

	// Register to receive IMotionConotroller calls
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);

#if PLATFORM_WINDOWS
	/// Simulate a user login
	UserLoginEventHandler(true, WINDOWS_USER_ID, 0);
#else
	// Register for user login/logout events
	FCoreDelegates::OnUserLoginChangedEvent.AddRaw(this, &FMoveController::UserLoginEventHandler);
#endif
}

//=============================================================================
FMoveController::~FMoveController ()
{
	// Cleanup Move controller dependencies
	for (auto& UserState : UserStates)
	{
		if (!UserState.bIsConnected)
		{
			continue;
		}

		for (auto& ControllerState : UserState.ControllerStates)
		{
			if (ControllerState.Tracker != IPS4Tracker::INVALID_TRACKER_HANDLE)
			{
				PS4Tracker->ReleaseTracker(ControllerState.Tracker);
			}
			SetVibration(ControllerState, 0.0f);
			MoveAPI::ControllerClose(ControllerState.Handle);
		}
	}

#if PLATFORM_WINDOWS
	/// Simulate a user logoff
	UserLoginEventHandler(false, WINDOWS_USER_ID, 0);
#endif

	MoveAPI::Term();
}

//=============================================================================
void FMoveController::Tick(float DeltaTime)
{
	for (auto& UserState : UserStates)
	{
		if (!UserState.bIsConnected)
		{
			continue;
		}

		for (auto& ControllerState : UserState.ControllerStates)
		{
			if (ControllerState.Tracker == IPS4Tracker::INVALID_TRACKER_HANDLE)
			{

				if (MoveAPI::ControllerIsReady(ControllerState.Handle))
				{
					IPS4Tracker::EDeviceType DeviceType;
					if (ControllerState.Hand == EControllerHand::Left)
					{
						DeviceType = IPS4Tracker::EDeviceType::MOTION_LEFT_HAND;
					}
					else
					{
						DeviceType = IPS4Tracker::EDeviceType::MOTION_RIGHT_HAND;
					}
					ControllerState.Tracker = PS4Tracker->AcquireTracker(ControllerState.Handle, UserState.ControllerId, DeviceType);
					ControllerState.Position = FVector::ZeroVector;
					ControllerState.Orientation = FQuat::Identity;
				}
			}
		}
	}
}

//=============================================================================
void FMoveController::SendControllerEvents()
{
	const double CurrentTime = FPlatformTime::Seconds();

	for (auto& UserState : UserStates)
	{
		if (!UserState.bIsConnected)
		{
			continue;
		}

		for (auto& ControllerState : UserState.ControllerStates)
		{
			MoveAPI::FStateData StateData;
			if (!MoveAPI::ControllerReadStateLatest(ControllerState.Handle, StateData))
			{
				continue;
			}

			const EControllerHand HandToUse = ControllerState.Hand;

			if (ControllerState.TriggerAnalog != StateData.TriggerAnalog)
			{
				const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_TriggerAxis : FGamepadKeyNames::MotionController_Right_TriggerAxis;
				MessageHandler->OnControllerAnalog(AxisButton, UserState.ControllerId, StateData.TriggerAnalog);
				ControllerState.TriggerAnalog = StateData.TriggerAnalog;
			}
			
			// For each button check against the previous state and send the correct message if any
			for (int32 ButtonIndex = 0; ButtonIndex < MoveAPI::EMoveControllerButton::TotalButtonCount; ++ButtonIndex)
			{
				if (StateData.ButtonStates[ButtonIndex] != ControllerState.ButtonStates[ButtonIndex])
				{
					if (StateData.ButtonStates[ButtonIndex])
					{
						MessageHandler->OnControllerButtonPressed(Buttons[ (int32)HandToUse ][ ButtonIndex ], UserState.ControllerId, false);

						// Move controller doesn't have a grip axis so we map the digital button to the resulting floating point value
						if (ButtonIndex == MoveAPI::EMoveControllerButton::Move)
						{
							const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_Grip1Axis : FGamepadKeyNames::MotionController_Right_Grip1Axis;
							MessageHandler->OnControllerAnalog(AxisButton, UserState.ControllerId, 1.0f);
						}
					}
					else
					{
						MessageHandler->OnControllerButtonReleased(Buttons[ (int32)HandToUse ][ ButtonIndex ], UserState.ControllerId, false);

						// Move controller doesn't have a grip axis so we map the digital button to the resulting floating point value
						if (ButtonIndex == MoveAPI::EMoveControllerButton::Move)
						{
							const FGamepadKeyNames::Type AxisButton = (HandToUse == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_Grip1Axis : FGamepadKeyNames::MotionController_Right_Grip1Axis;
							MessageHandler->OnControllerAnalog(AxisButton, UserState.ControllerId, 0.0f);
						}
					}

					if (StateData.ButtonStates[ButtonIndex] != 0)
					{
						// This button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
						ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
					}
				}
				else if (StateData.ButtonStates[ButtonIndex] && ControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime)
				{
					MessageHandler->OnControllerButtonPressed(Buttons[ (int32)HandToUse ][ ButtonIndex ], UserState.ControllerId, true);

					// Set the button's NextRepeatTime to the ButtonRepeatDelay
					ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
				}

				// Update the state for next time
				ControllerState.ButtonStates[ButtonIndex] = StateData.ButtonStates[ButtonIndex];
			}
		}
	}
}

//=============================================================================
void FMoveController::SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

//=============================================================================
void FMoveController::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	check(ControllerId < ARRAY_COUNT(UserStates));

	// Is the specified controller connected?
	auto& UserState = UserStates[ControllerId];
	if (UserState.ControllerId != ControllerId)
	{
		return;
	}

	switch (ChannelType)
	{
		case FForceFeedbackChannelType::LEFT_LARGE:
		{
			SetVibration(UserState.ControllerStates[(int32)EControllerHand::Left], Value);
			break;
		}
		case FForceFeedbackChannelType::RIGHT_LARGE:
		{
			SetVibration(UserState.ControllerStates[(int32)EControllerHand::Right], Value);
			break;
		}
		default:
			break; // noop
	}

}

//=============================================================================
void FMoveController::SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values)
{
	check(ControllerId < ARRAY_COUNT(UserStates));

	// Is the specified controller connected?
	auto& UserState = UserStates[ControllerId];
	if (UserState.ControllerId == ControllerId)
	{
		SetVibration(UserState.ControllerStates[(int32)EControllerHand::Left], Values.LeftLarge);
		SetVibration(UserState.ControllerStates[(int32)EControllerHand::Right], Values.RightLarge);
	}
}

//=============================================================================
void FMoveController::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	check(ControllerId < ARRAY_COUNT(UserStates));

	// Is the specified controller connected?
	auto& UserState = UserStates[ControllerId];
	if (UserState.ControllerId == ControllerId)
	{
		if (Hand == (int32)EControllerHand::Left || Hand == (int32)EControllerHand::Right)
		{
			const float Amplitude = (Values.Frequency > 0.0f) ? Values.Amplitude : 0.0f;
			SetVibration(UserState.ControllerStates[Hand], Amplitude);
		}
	}
}

//=============================================================================
void FMoveController::GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const
{
	MinFrequency = 0.f;
	MaxFrequency = 1.f;
}

//=============================================================================
float FMoveController::GetHapticAmplitudeScale() const
{
	return 1.0f;
}

//=============================================================================
bool FMoveController::GetControllerOrientationAndPosition(const int32 ControllerId, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	check(ControllerId < ARRAY_COUNT(UserStates));

	// Is the specified controller connected?
	const auto& UserState = UserStates[ControllerId];
	if (UserState.ControllerId != ControllerId)
	{
		return false;
	}

	// Is this a valid hand?
	if (DeviceHand != EControllerHand::Left && DeviceHand != EControllerHand::Right)
	{
		return false;
	}

	// Is the specified controller tracking?
	const FControllerState& ControllerState = UserState.ControllerStates[(int32)DeviceHand];
	if (ControllerState.Tracker == IPS4Tracker::INVALID_TRACKER_HANDLE)
	{
		return false;
	}

	if (!MoveAPI::ControllerIsReady(ControllerState.Handle))
	{
		return false;
	}

	if (IsInGameThread())
	{
		bIsStereoEnabled = GEngine->IsStereoscopic3D();
	}

	IPS4Tracker::FTrackingData TrackingData;
	PS4Tracker->GetTrackingData(ControllerState.Tracker, TrackingData);
	FVector Position = TrackingData.Position * WorldToMetersScale;
	if (GEngine->XRSystem.IsValid() && bIsStereoEnabled)
	{
		GEngine->XRSystem->RebaseObjectOrientationAndPosition(Position, TrackingData.Orientation);
	}

	ControllerState.TrackingStatus = TrackingData.Status;

	if (TrackingData.Status == IPS4Tracker::ETrackingStatus::TRACKING)
	{
		ControllerState.Position = Position;
	}
	ControllerState.Orientation = TrackingData.Orientation;

	// Output values
	OutPosition = ControllerState.Position;
	OutOrientation = FRotator(ControllerState.Orientation);

	return (TrackingData.Status == IPS4Tracker::ETrackingStatus::TRACKING) || (TrackingData.Status == IPS4Tracker::ETrackingStatus::NOT_TRACKING);
}

//=============================================================================
ETrackingStatus FMoveController::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	check(ControllerIndex < ARRAY_COUNT(UserStates));

	// Is the specified controller connected?
	const auto& UserState = UserStates[ControllerIndex];
	if (UserState.ControllerId != ControllerIndex)
	{
		return ETrackingStatus::NotTracked;
	}

	const FControllerState& ControllerState = UserState.ControllerStates[(int32)DeviceHand];

	if (ControllerState.TrackingStatus == IPS4Tracker::ETrackingStatus::TRACKING)
	{
		// Device is within the camera's tracking field
		return ETrackingStatus::Tracked;
	}
	else if (ControllerState.TrackingStatus == IPS4Tracker::ETrackingStatus::NOT_TRACKING)
	{
		// Device is within the camera's tracking field
		return ETrackingStatus::InertialOnly;
	}
	else
	{
		return ETrackingStatus::NotTracked;
	}
}

//=============================================================================
void FMoveController::UserLoginEventHandler(bool bLoggingIn, int32 UserID, int32 UserIndex)
{		
	check(MoveAPI::UserValidate(UserID));
		
	if (bLoggingIn)		
	{				
		ConnectStateToUser(UserID, UserIndex);				
	}
	else
	{	
		DisconnectStateFromUser(UserID, UserIndex);			
	}
}

//=============================================================================
void FMoveController::ConnectStateToUser(int32 UserID, int32 UserIndex)
{
	check(UserIndex < ARRAY_COUNT(UserStates));

	// make sure this user isn't already logged in on some state.
	for (int i = 0; i < ARRAY_COUNT(UserStates); ++i)
	{
		check(UserStates[i].UserID != UserID);
	}
	
	FUserState& UserState = UserStates[UserIndex];

	check(UserState.UserID == 0);	
	check(UserState.ControllerStates[(int32)EControllerHand::Left].Handle == 0);
	check(UserState.ControllerStates[(int32)EControllerHand::Right].Handle == 0);

	FMemory::Memzero(&UserState, sizeof(FUserState));

	UserState.bIsConnected = true;
	UserState.ControllerId = UserIndex;
	UserState.UserID = UserID;
	for (int32 HandIndex = 0; HandIndex < 2; ++HandIndex)
	{
		FControllerState& ControllerState = UserState.ControllerStates[HandIndex];
		ControllerState.Handle = MoveAPI::ControllerOpen(UserID, HandIndex);
		ControllerState.Hand = (EControllerHand)HandIndex;
		ControllerState.VibrationIntensity = -1.0f;
		ControllerState.Tracker = IPS4Tracker::INVALID_TRACKER_HANDLE;
	}
}

//=============================================================================
void FMoveController::DisconnectStateFromUser(int32 UserID, int32 UserIndex)
{
	check(UserIndex < ARRAY_COUNT(UserStates));

	FUserState& UserState = UserStates[UserIndex];
	check(UserState.ControllerId == UserIndex);
	check(UserState.UserID == UserID);		

	for (auto& ControllerState : UserState.ControllerStates)
	{
		if (ControllerState.Tracker != IPS4Tracker::INVALID_TRACKER_HANDLE)
		{
			PS4Tracker->ReleaseTracker(ControllerState.Tracker);
		}
	}

	if (UserState.bIsConnected)
	{
		MoveAPI::ControllerClose(UserState.ControllerStates[(int32)EControllerHand::Left].Handle);
		MoveAPI::ControllerClose(UserState.ControllerStates[(int32)EControllerHand::Right].Handle);
	}

	FMemory::Memzero(&UserState, sizeof(FUserState));
	UserState.ControllerStates[(int32)EControllerHand::Left].Tracker = IPS4Tracker::INVALID_TRACKER_HANDLE;
	UserState.ControllerStates[(int32)EControllerHand::Right].Tracker = IPS4Tracker::INVALID_TRACKER_HANDLE;
}

//=============================================================================
void FMoveController::SetVibration(FControllerState& ControllerState, float Value)
{
	if (ControllerState.VibrationIntensity == Value)
	{
		return;
	}

	const uint8 VibrationIntensity = (uint8)FMath::Clamp(FMath::TruncToInt(Value * 255.0f), 0, 255);
	MoveAPI::ControllerSetVibration(ControllerState.Handle, VibrationIntensity);
	ControllerState.VibrationIntensity = Value;
}



//** Move controller plugin implementation */
class FMoveControllerPlugin : public IInputDeviceModule
{
public:
	/** IInputDeviceModule implementation */
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override
	{
		return TSharedPtr< class IInputDevice >(new FMoveController(InMessageHandler));
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
IMPLEMENT_MODULE(FMoveControllerPlugin, MoveController)

//=============================================================================
void FMoveControllerPlugin::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
}

//=============================================================================
void FMoveControllerPlugin::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}
