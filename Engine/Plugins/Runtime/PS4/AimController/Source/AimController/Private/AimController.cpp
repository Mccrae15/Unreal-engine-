// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AimController.h"
#include "IInputDevice.h"
#include "IMotionController.h"
#include "IHapticDevice.h"
#include "PS4Tracker.h"
#include "XRMotionControllerBase.h"
#include "PS4Tracker.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "GenericPlatform/IInputInterface.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include <pad.h>

/******************************************************************************
*
*	Controller API
*
******************************************************************************/

class ControllerAPI
{
public:

	struct EAimControllerButton
	{
		enum Type
		{
			Invalid0 = 0,
			Gun_LeftThumb,
			Gun_RightThumb,
			Gun_SpecialRight,
			Gun_DPadUp,
			Gun_DPadRight,
			Gun_DPadDown,
			Gun_DPadLeft,
			Gun_LeftTriggerThreshold,
			Gun_RightTriggerThreshold,
			Gun_LeftShoulder,
			Gun_RightShoulder,
			Gun_FaceButtonTop,
			Gun_FaceButtonRight,
			Gun_FaceButtonBottom,
			Gun_FaceButtonLeft,
			Invalid1,
			Invalid2,
			Invalid3,
			Invalid4,
			Gun_SpecialLeft,

			// now the virtual
			Gun_LeftStickLeft,
			Gun_LeftStickRight,
			Gun_LeftStickUp,
			Gun_LeftStickDown,

			Gun_RightStickLeft,
			Gun_RightStickRight,
			Gun_RightStickUp,
			Gun_RightStickDown,

			/** Max number of controller buttons */
			TotalButtonCount
		};
	};

	struct FStateData{
		bool	ButtonStates[EAimControllerButton::TotalButtonCount];
		float	TriggerAnalog;
	};

	/** ControllerAPI Initialization and termination */
	static void Init();
	static void Term();

	/** Controller methods */
	static int32 ControllerOpen(int32 UserId, int32 HandIndex);
	static void ControllerClose(int32 Handle);
	static bool ControllerIsReady(int32 Handle);

	/** Return true on valid UserId */
	static bool UserValidate (int32 UserId);
};



/******************************************************************************
*
*	PS4 Controller API implementation
*
******************************************************************************/

#include <libsysmodule.h>

const int CONTROLLERS_PER_PLAYER = 1;
const int MAX_LOGIN_PLAYERS = SCE_USER_SERVICE_MAX_LOGIN_USERS;

/** Number of physical and virtual buttons */
const int NUM_PHYSICAL_BUTTONS = 21;
const int NUM_VIRTUAL_BUTTONS = 8;

/** Max number of controller buttons (touchpad button is bit 20). */
const int TotalButtonCount = (NUM_PHYSICAL_BUTTONS + NUM_VIRTUAL_BUTTONS);

static_assert(TotalButtonCount == ControllerAPI::EAimControllerButton::TotalButtonCount, "TotalButtonCount mismatch!");

DEFINE_LOG_CATEGORY_STATIC(LogAimController, Log, All);

//=============================================================================
void ControllerAPI::Init()
{
	scePadInit();
	sceUserServiceInitialize(nullptr);
}

//=============================================================================
void ControllerAPI::Term()
{
}

//=============================================================================
int32 ControllerAPI::ControllerOpen(int32 UserID, int32 HandIndex)
{
	return scePadOpen(UserID, SCE_PAD_PORT_TYPE_SPECIAL, 0, nullptr);
}

//=============================================================================
void ControllerAPI::ControllerClose(int32 Handle)
{
	int32 Ret = scePadClose(Handle);
	check(Ret == SCE_OK);
}

//=============================================================================
bool ControllerAPI::ControllerIsReady(int32 Handle)
{
	ScePadControllerInformation ControllerInfo;
	int32 Ret = scePadGetControllerInformation(Handle, &ControllerInfo);
	if (Ret == SCE_OK)
	{
		if (ControllerInfo.connected)
		{
			check(ControllerInfo.deviceClass == SCE_PAD_DEVICE_CLASS_GUN);
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

//=============================================================================
bool ControllerAPI::UserValidate(int32 UserID)
{
	return UserID != SCE_USER_SERVICE_USER_ID_INVALID;
}


/** Move motion controller implementation */
 class FAimController : public IInputDevice, public FXRMotionControllerBase, public IHapticDevice
{
public:
	FAimController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);
	virtual ~FAimController ();

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
		const static FName DefaultName(TEXT("PS4AimController"));
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

		/** Current world position */
		mutable FVector	Position;

		/** Current world orientation */
		mutable FQuat Orientation;

		/** Last frame's button states, so we only send events on edges */
		ScePadData PreviousPadData;

		/** This frame's pad data */
		ScePadData PadData;

		/** Was the given virtual button down last frame? */
		uint8 PreviousButtonState[TotalButtonCount];

		/** Next time a repeat event should be generated for each button */
		double NextRepeatTime[TotalButtonCount];

		/** This maps the id's in the pad event to a strict [0..SCE_PAD_MAX_TOUCH_NUM-1] range */
		FIntVector TouchMapping[SCE_PAD_MAX_TOUCH_NUM];

		/** The handle to read the controller state */
		int32 Handle;

		/** The handle to read the controller state */
		int32 RefCount;

		/** The handle to read the mouse state */
		int32 MouseHandle;

		/** Last frame's mouse button state */
		uint32 MouseButtonState;

		/** If the controller is currently connected */
		bool bIsConnected;

		/** If the controller is a remote play connection */
		bool bIsRemotePlay;

		/** DualShock4 has a touchpad resoultion of 1920 x 942.  However future controllers may differ */
		FIntVector TouchPadExtents;

		/** Holds information about the controller, including if it is a remote play connection or not */
		ScePadControllerInformation ControllerInfo;

		/** Vibration settings */
		FForceFeedbackValues VibeValues;
		ScePadVibrationParam VibeSettings;

		/** Left or right hand which the controller represents */
		EControllerHand Hand;
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

		/** Per controller state, 1 gun allowed per user. */
		FControllerState	ControllerStates[ CONTROLLERS_PER_PLAYER ];
	} UserStates[ MAX_LOGIN_PLAYERS ];

	/** Mapping of controller buttons */
	FGamepadKeyNames::Type Buttons[ CONTROLLERS_PER_PLAYER ][ ControllerAPI::EAimControllerButton::TotalButtonCount ];

	/** handler to send all messages to */
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	/** whether or not we emit motion events from the controller */
	bool bDS4MotionEvents;

	/** Delay before sending a repeat message after a button was first pressed */
	float InitialButtonRepeatDelay;

	/** Delay before sending a repeat message after a button has been pressed for a while */
	float ButtonRepeatDelay;

	/** Whether stereo rendering was active on the game thread */
	mutable bool bIsStereoEnabled;

	/** Handle for accessing the PS4 positional tracking */
	mutable FPSTrackerHandle PS4Tracker;

	/** Sets the vibration intensity for the specified controller state */
	void UpdateVibeMotors(FControllerState &State);
};

//=============================================================================
FAimController::FAimController(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	: MessageHandler(InMessageHandler)
	, bDS4MotionEvents(false)
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

	Buttons[0][ControllerAPI::EAimControllerButton::Invalid0]					= FGamepadKeyNames::Invalid;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_LeftThumb]				= FGamepadKeyNames::LeftThumb;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_RightThumb]				= FGamepadKeyNames::RightThumb;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_SpecialRight]			= FGamepadKeyNames::SpecialRight;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_DPadUp]					= FGamepadKeyNames::DPadUp;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_DPadRight]				= FGamepadKeyNames::DPadRight;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_DPadDown]				= FGamepadKeyNames::DPadDown;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_DPadLeft]				= FGamepadKeyNames::DPadLeft;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_LeftTriggerThreshold]	= FGamepadKeyNames::LeftTriggerThreshold;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_RightTriggerThreshold]	= FGamepadKeyNames::RightTriggerThreshold;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_LeftShoulder]			= FGamepadKeyNames::LeftShoulder;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_RightShoulder]			= FGamepadKeyNames::RightShoulder;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_FaceButtonTop]			= FGamepadKeyNames::FaceButtonTop;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_FaceButtonRight]		= FGamepadKeyNames::FaceButtonRight;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_FaceButtonBottom]		= FGamepadKeyNames::FaceButtonBottom;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_FaceButtonLeft]			= FGamepadKeyNames::FaceButtonLeft;
	Buttons[0][ControllerAPI::EAimControllerButton::Invalid1]					= FGamepadKeyNames::Invalid;
	Buttons[0][ControllerAPI::EAimControllerButton::Invalid2]					= FGamepadKeyNames::Invalid;
	Buttons[0][ControllerAPI::EAimControllerButton::Invalid3]					= FGamepadKeyNames::Invalid;
	Buttons[0][ControllerAPI::EAimControllerButton::Invalid4]					= FGamepadKeyNames::Invalid;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_SpecialLeft]			= FGamepadKeyNames::SpecialLeft;

	Buttons[0][ControllerAPI::EAimControllerButton::Gun_LeftStickLeft]			= FGamepadKeyNames::LeftStickLeft;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_LeftStickRight]			= FGamepadKeyNames::LeftStickRight;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_LeftStickUp]			= FGamepadKeyNames::LeftStickUp;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_LeftStickDown]			= FGamepadKeyNames::LeftStickDown;

	Buttons[0][ControllerAPI::EAimControllerButton::Gun_RightStickLeft]			= FGamepadKeyNames::RightStickLeft;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_RightStickRight]		= FGamepadKeyNames::RightStickRight;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_RightStickUp]			= FGamepadKeyNames::RightStickUp;
	Buttons[0][ControllerAPI::EAimControllerButton::Gun_RightStickDown]			= FGamepadKeyNames::RightStickDown;


	ControllerAPI::Init();

	// Register to receive IAimController calls
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);

	// Register for user login/logout events
	FCoreDelegates::OnUserLoginChangedEvent.AddRaw(this, &FAimController::UserLoginEventHandler);
}

//=============================================================================
FAimController::~FAimController ()
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
			//SetVibration(ControllerState, 0.0f);
			ControllerAPI::ControllerClose(ControllerState.Handle);
		}
	}

	ControllerAPI::Term();
}

//=============================================================================
void FAimController::Tick(float DeltaTime)
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

				if (ControllerAPI::ControllerIsReady(ControllerState.Handle))
				{
					ControllerState.Tracker = PS4Tracker->AcquireTracker(ControllerState.Handle, UserState.ControllerId, IPS4Tracker::EDeviceType::GUN);
					ControllerState.Position = FVector::ZeroVector;
					ControllerState.Orientation = FQuat::Identity;
				}
			}
		}
	}
}

namespace {

	float ANALOG_TO_FLOAT(uint8 x)
	{
		return (((float)(x)-128.0f) / 128.f);
	}

	float TRIGGER_TO_FLOAT(uint8 x)
	{
		return ((float)(x) / 255.f);
	}

	const uint8 NEGATIVE_DEADZONE = 64;
	const uint8 POSITIVE_DEADZONE = 128 + 64;
}

//=============================================================================
void FAimController::SendControllerEvents()
{
	for (auto& UserState : UserStates)
	{
		FControllerState& ControllerState = UserState.ControllerStates[0];
		const int32 UserIndex = 0;

		// get a controller of input
		int32 Result = scePadReadState(ControllerState.Handle, &ControllerState.PadData);

		// this will occur in any situation where the system takes control of input, such as opening the PS menu
		if (ControllerState.PadData.buttons & SCE_PAD_BUTTON_INTERCEPTED)
		{
			// Pad-Overview_e.pdf: "Implement applications so that output data with the SCE_PAD_BUTTON_INTERCEPTED bit is always discarded."
			return;
		}

		// Remember the current connection state to detect transitions
		bool bWasConnected = ControllerState.bIsConnected;

		// is it connected?
		ControllerState.bIsConnected = Result == SCE_OK && ControllerState.PadData.connected;

		// If the controller is connected send events or if the controller was connected send a final event with default states so that 
		// the game doesn't think that controller buttons are still held down
		if (ControllerState.bIsConnected || bWasConnected)
		{
			// If the controller is connected now but was not before, refresh the information
			if (!bWasConnected && ControllerState.bIsConnected)
			{
				int32 result = scePadGetControllerInformation(ControllerState.Handle, &ControllerState.ControllerInfo);
				if (result == SCE_OK)
				{
					// All is well
					ControllerState.bIsRemotePlay = (ControllerState.ControllerInfo.connectionType == SCE_PAD_CONNECTION_TYPE_REMOTE);
					ControllerState.TouchPadExtents.X = ControllerState.ControllerInfo.touchPadInfo.resolution.x;
					ControllerState.TouchPadExtents.Y = ControllerState.ControllerInfo.touchPadInfo.resolution.y;
					ControllerState.TouchPadExtents.Z = 0;
				}
				FCoreDelegates::OnControllerConnectionChange.Broadcast(true, UserState.UserID, UserIndex);
			}
			else if (bWasConnected && !ControllerState.bIsConnected)
			{
				FCoreDelegates::OnControllerConnectionChange.Broadcast(false, UserState.UserID, UserIndex);
			}

			// Check Analog state

			// Axis, convert range -32768..+32767 set up to +/- 1.
			if (ControllerState.PadData.leftStick.x != ControllerState.PreviousPadData.leftStick.x)
			{
				float Value = ANALOG_TO_FLOAT(ControllerState.PadData.leftStick.x);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, UserState.ControllerId, Value);
			}

			if (ControllerState.PadData.leftStick.y != ControllerState.PreviousPadData.leftStick.y)
			{
				float Value = ANALOG_TO_FLOAT(255 - ControllerState.PadData.leftStick.y);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, UserState.ControllerId, Value);
			}

			if (ControllerState.PadData.rightStick.x != ControllerState.PreviousPadData.rightStick.x)
			{
				float Value = ANALOG_TO_FLOAT(ControllerState.PadData.rightStick.x);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogX, UserState.ControllerId, Value);
			}

			if (ControllerState.PadData.rightStick.y != ControllerState.PreviousPadData.rightStick.y)
			{
				float Value = ANALOG_TO_FLOAT(255 - ControllerState.PadData.rightStick.y);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogY, UserState.ControllerId, Value);
			}

			if (ControllerState.PadData.analogButtons.l2 != ControllerState.PreviousPadData.analogButtons.l2)
			{
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftTriggerAnalog, UserState.ControllerId, TRIGGER_TO_FLOAT(ControllerState.PadData.analogButtons.l2));
			}

			if (ControllerState.PadData.analogButtons.r2 != ControllerState.PreviousPadData.analogButtons.r2)
			{
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightTriggerAnalog, UserState.ControllerId, TRIGGER_TO_FLOAT(ControllerState.PadData.analogButtons.r2));
			}

			// In DS4 implementation the touch axis handling was here.

			uint8 NewButtonState[TotalButtonCount];

			// handle virtual buttons
			NewButtonState[NUM_PHYSICAL_BUTTONS + 0] = ControllerState.PadData.leftStick.x < NEGATIVE_DEADZONE;
			NewButtonState[NUM_PHYSICAL_BUTTONS + 1] = ControllerState.PadData.leftStick.x > POSITIVE_DEADZONE;
			NewButtonState[NUM_PHYSICAL_BUTTONS + 2] = ControllerState.PadData.leftStick.y < NEGATIVE_DEADZONE;
			NewButtonState[NUM_PHYSICAL_BUTTONS + 3] = ControllerState.PadData.leftStick.y > POSITIVE_DEADZONE;
			NewButtonState[NUM_PHYSICAL_BUTTONS + 4] = ControllerState.PadData.rightStick.x < NEGATIVE_DEADZONE;
			NewButtonState[NUM_PHYSICAL_BUTTONS + 5] = ControllerState.PadData.rightStick.x > POSITIVE_DEADZONE;
			NewButtonState[NUM_PHYSICAL_BUTTONS + 6] = ControllerState.PadData.rightStick.y < NEGATIVE_DEADZONE;
			NewButtonState[NUM_PHYSICAL_BUTTONS + 7] = ControllerState.PadData.rightStick.y > POSITIVE_DEADZONE;

			const double CurrentTime = FPlatformTime::Seconds();

			// For each button check against the previous state and send the correct message if any
			for (int32 ButtonIndex = 0; ButtonIndex < NUM_PHYSICAL_BUTTONS; ++ButtonIndex)
			{
				uint32 Mask = (1 << ButtonIndex);
				NewButtonState[ButtonIndex] = (ControllerState.PadData.buttons & Mask) != 0;
			}


			for (int32 ButtonIndex = 0; ButtonIndex < TotalButtonCount; ++ButtonIndex)
			{
				if (NewButtonState[ButtonIndex] != ControllerState.PreviousButtonState[ButtonIndex])
				{
					// was it pressed or released?
					if (NewButtonState[ButtonIndex] != 0)
					{
						MessageHandler->OnControllerButtonPressed(Buttons[0][ButtonIndex], UserState.ControllerId, false);

						// set the button's NextRepeatTime to the InitialButtonRepeatDelay
						ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
					}
					else
					{
						MessageHandler->OnControllerButtonReleased(Buttons[0][ButtonIndex], UserState.ControllerId, false);
					}
				}
				else if (NewButtonState[ButtonIndex] != 0 && ControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime)
				{
					MessageHandler->OnControllerButtonPressed(Buttons[0][ButtonIndex], UserState.ControllerId, true);

					// set the button's NextRepeatTime to the ButtonRepeatDelay
					ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
				}
			}

			// In DS4 implementation the touch event and mouse input handling was here

			if (bDS4MotionEvents)
			{
				// get tilt info (always send it)
				FVector Tilt, RotationRate, Acceleration, Gravity;

				// convert the quaternion into unreal rotation
				FQuat Orientation(ControllerState.PadData.orientation.x, ControllerState.PadData.orientation.y, ControllerState.PadData.orientation.z, ControllerState.PadData.orientation.w);
				FRotator Rotation = Orientation.Rotator();

				// @todo: These values need to be fixed up to be inline with other platforms
				MessageHandler->OnMotionDetected(
					FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll),
					FVector::RadiansToDegrees(FVector(ControllerState.PadData.angularVelocity.x, ControllerState.PadData.angularVelocity.y, ControllerState.PadData.angularVelocity.z)),
					FVector(0, 0, 0),
					FVector(ControllerState.PadData.acceleration.x, ControllerState.PadData.acceleration.y, ControllerState.PadData.acceleration.z),
					UserState.ControllerId
					);
			}

			// now update last frame with this one
			ControllerState.PreviousPadData = ControllerState.PadData;
			FMemory::Memcpy(ControllerState.PreviousButtonState, NewButtonState);
		}
	}
}

//=============================================================================
void FAimController::SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
	MessageHandler = InMessageHandler;
}

//=============================================================================
void FAimController::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	// Don't bother looking if this isn't a channel the PS4 supports
	if (ChannelType != FForceFeedbackChannelType::LEFT_LARGE && ChannelType != FForceFeedbackChannelType::LEFT_SMALL)
	{
		return;
	}

	for (auto& UserState : UserStates)
	{
		FControllerState& ControllerState = UserState.ControllerStates[0];
		if (ControllerState.bIsConnected && UserState.ControllerId == ControllerId)
		{
			//
			// Found the controller, so set the new value for the given channel
			//
	
			// Save a copy of the value for future comparison
			switch (ChannelType)
			{
			case FForceFeedbackChannelType::LEFT_LARGE:
				ControllerState.VibeValues.LeftLarge = Value;
				break;
	
			case FForceFeedbackChannelType::LEFT_SMALL:
				ControllerState.VibeValues.LeftSmall = Value;
				break;
	
			case FForceFeedbackChannelType::RIGHT_LARGE:
				ControllerState.VibeValues.RightLarge = Value;
				break;
	
			case FForceFeedbackChannelType::RIGHT_SMALL:
				ControllerState.VibeValues.RightSmall = Value;
				break;
	
			default:
				// Unknown channel, so ignore it
				break;
			}
	
			// Update with the latest values
			UpdateVibeMotors(ControllerState);
	
			// We found the controller so break out of the loop
			break;
		}
	}
}

//=============================================================================
void FAimController::SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values)
{
	// The channel is valid, so if the controller is open, set its value
	for (auto& UserState : UserStates)
	{
		FControllerState& ControllerState = UserState.ControllerStates[0];
		if (ControllerState.bIsConnected && UserState.ControllerId == ControllerId)
		{
			//
			// Found the controller, so set its values according to those provided
			//
			ControllerState.VibeValues = Values;
	
			UpdateVibeMotors(ControllerState);
	
			// We found the controller so break out of the loop
			break;
		}
	}
}

//=============================================================================
void FAimController::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	if (Hand != (int32)EControllerHand::Gun)
	{
		return;
	}

	// Is the specified controller connected?
	for (auto& UserState : UserStates)
	{
		FControllerState& ControllerState = UserState.ControllerStates[0];
		if (ControllerState.bIsConnected && UserState.ControllerId == ControllerId)
		{
			const float Amplitude = (Values.Frequency > 0.0f) ? Values.Amplitude : 0.0f;
			FForceFeedbackValues& FFV = ControllerState.VibeValues;
			FFV.LeftLarge = FFV.RightLarge = FFV.LeftSmall = FFV.RightSmall = Amplitude;
			UpdateVibeMotors(ControllerState);
		}
	}
}

//=============================================================================
void FAimController::GetHapticFrequencyRange(float& MinFrequency, float& MaxFrequency) const
{
	MinFrequency = 0.f;
	MaxFrequency = 1.f;
}

//=============================================================================
float FAimController::GetHapticAmplitudeScale() const
{
	return 1.0f;
}

/**
* Float to byte converter, with clamping
*/
static FORCEINLINE int32 ConvertToByte(float Value)
{
	int32 Setting = Value * 255;
	if (Setting < 0)
	{
		Setting = 0;
	}
	return (Setting &= 0x000000FF);
}

void FAimController::UpdateVibeMotors(FControllerState &State)
{
	// Calculate the appropriate settings by mapping the full channel value set onto the large and small motors.
	const float LargeValue = (State.VibeValues.LeftLarge > State.VibeValues.RightLarge ? State.VibeValues.LeftLarge : State.VibeValues.RightLarge);
	const float SmallValue = (State.VibeValues.LeftSmall > State.VibeValues.RightSmall ? State.VibeValues.LeftSmall : State.VibeValues.RightSmall);

	State.VibeSettings.largeMotor = ConvertToByte(LargeValue);
	State.VibeSettings.smallMotor = ConvertToByte(SmallValue);

	// Send the new values to the controller
	scePadSetVibration(State.Handle, &State.VibeSettings);
}

//=============================================================================
bool FAimController::GetControllerOrientationAndPosition(const int32 ControllerId, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	check(ControllerId < ARRAY_COUNT(UserStates));

	// Is the specified controller connected?
	const auto& UserState = UserStates[ControllerId];
	if (UserState.ControllerId != ControllerId)
	{
		return false;
	}

	// Is this a valid hand?
	if (DeviceHand != EControllerHand::Gun)
	{
		return false;
	}

	// Is the specified controller tracking?
	const FControllerState& ControllerState = UserState.ControllerStates[0];
	if (ControllerState.Tracker == IPS4Tracker::INVALID_TRACKER_HANDLE)
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
ETrackingStatus FAimController::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	check(ControllerIndex < ARRAY_COUNT(UserStates));

	// Is the specified controller connected?
	const auto& UserState = UserStates[ControllerIndex];
	if (UserState.ControllerId != ControllerIndex)
	{
		return ETrackingStatus::NotTracked;
	}

	const FControllerState& ControllerState = UserState.ControllerStates[0];

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
void FAimController::UserLoginEventHandler(bool bLoggingIn, int32 UserID, int32 UserIndex)
{		
	check(ControllerAPI::UserValidate(UserID));
		
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
void FAimController::ConnectStateToUser(int32 UserID, int32 UserIndex)
{
	check(UserIndex < ARRAY_COUNT(UserStates));

	// make sure this user isn't already logged in on some state.
	for (int i = 0; i < ARRAY_COUNT(UserStates); ++i)
	{
		check(UserStates[i].UserID != UserID);
	}
	
	FUserState& UserState = UserStates[UserIndex];

	check(UserState.UserID == 0);	
	check(UserState.ControllerStates[0].Handle == 0);

	FMemory::Memzero(&UserState, sizeof(FUserState));

	UserState.bIsConnected = true;
	UserState.ControllerId = UserIndex;
	UserState.UserID = UserID;
	check(UserID != SCE_USER_SERVICE_USER_ID_INVALID);

	FControllerState& ControllerState = UserState.ControllerStates[0];
	ControllerState.Handle = ControllerAPI::ControllerOpen(UserID, 0);
	ControllerState.Hand = EControllerHand::Gun;
	//ControllerState.VibrationIntensity = -1.0f; //TODO haptics
	ControllerState.Tracker = IPS4Tracker::INVALID_TRACKER_HANDLE;
}

//=============================================================================
void FAimController::DisconnectStateFromUser(int32 UserID, int32 UserIndex)
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
		ControllerAPI::ControllerClose(UserState.ControllerStates[0].Handle);
	}

	FMemory::Memzero(&UserState, sizeof(FUserState));
	UserState.ControllerStates[0].Tracker = IPS4Tracker::INVALID_TRACKER_HANDLE;
}


//** Aim controller plugin implementation */
class FAimControllerPlugin : public IInputDeviceModule
{
public:
	/** IInputDeviceModule implementation */
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override
	{
		return TSharedPtr< class IInputDevice >(new FAimController(InMessageHandler));
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
IMPLEMENT_MODULE(FAimControllerPlugin, AimController)

//=============================================================================
void FAimControllerPlugin::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
}

//=============================================================================
void FAimControllerPlugin::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

