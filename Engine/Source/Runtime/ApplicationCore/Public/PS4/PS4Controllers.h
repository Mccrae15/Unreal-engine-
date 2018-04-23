// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

////////////////////////////////////////////////////////////////////
//
// This code is shared between PS4 runtime (PS4InputInterface) and a
// Windows input device plugin (WinDualShock). It assumes the necessary
// includes are already in place before including this header file!
//
////////////////////////////////////////////////////////////////////

#include "CoreTypes.h"
#include "HAL/PlatformTime.h"
#include "GenericPlatform/ICursor.h"
#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Misc/CoreDelegates.h"
#include "GenericPlatform/IInputInterface.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h" // for FGamepadKeyNames

/** Number of physical and virtual buttons */
#define NUM_PHYSICAL_BUTTONS 21
#define NUM_VIRTUAL_BUTTONS 8

/** Max number of controller buttons (touchpad button is bit 20). */
#define MAX_NUM_CONTROLLER_BUTTONS (NUM_PHYSICAL_BUTTONS + NUM_VIRTUAL_BUTTONS)

class FPS4Controllers
{
public:

	struct FControllerState
	{
		/** Last frame's button states, so we only send events on edges */
		ScePadData PreviousPadData;

		/** This frame's pad data */
		ScePadData PadData;

		/** Was the given virtual button down last frame? */
		uint8 PreviousButtonState[MAX_NUM_CONTROLLER_BUTTONS];

		/** Next time a repeat event should be generated for each button */
		double NextRepeatTime[MAX_NUM_CONTROLLER_BUTTONS];

		/** This maps the id's in the pad event to a strict [0..SCE_PAD_MAX_TOUCH_NUM-1] range */
		FIntVector TouchMapping[SCE_PAD_MAX_TOUCH_NUM];

		/** Id of the controller */
		int32 ControllerId;

		/** Id of the user logged in with this controller */
		int32 UserID;

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
	};

public:
	FPS4Controllers()
	{
		InitialButtonRepeatDelay = 0.2f;
		ButtonRepeatDelay = 0.1f;

		bDS4TouchEvents = false;
		bDS4TouchAxisButtons = false;
		bDS4MouseEvents = false;
		bDS4MotionEvents = false;		

		scePadInit();

		FMemory::Memzero( &ControllerStates, sizeof(ControllerStates) );

#if PLATFORM_PS4
		sceUserServiceInitialize(nullptr);		
#endif

		for (int32 UserIndex=0; UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS; UserIndex++)
		{						
			
#if PLATFORM_PS4
			//not necessary to do anything on platform.  a login event will happen triggering the UserLoginEventHandler delegate			
#else
			ConnectStateToUser(SCE_USER_SERVICE_STATIC_USER_ID_1 + UserIndex, UserIndex);			
#endif
			
			// @todo: We used to use the logged in user list for which user to connect to, but I think it's actually
			// fine to just use the USER_1... and so on because it should route to the proper user anyway (blue controller is 
			// always player 1, etc)
		}

		ButtonNames[0] = FGamepadKeyNames::Invalid;
		ButtonNames[1] = FGamepadKeyNames::LeftThumb;
		ButtonNames[2] = FGamepadKeyNames::RightThumb;
		ButtonNames[3] = FGamepadKeyNames::SpecialRight;
		ButtonNames[4] = FGamepadKeyNames::DPadUp;
		ButtonNames[5] = FGamepadKeyNames::DPadRight;
		ButtonNames[6] = FGamepadKeyNames::DPadDown;
		ButtonNames[7] = FGamepadKeyNames::DPadLeft;
		ButtonNames[8] = FGamepadKeyNames::LeftTriggerThreshold;
		ButtonNames[9] = FGamepadKeyNames::RightTriggerThreshold;
		ButtonNames[10] = FGamepadKeyNames::LeftShoulder;
		ButtonNames[11] = FGamepadKeyNames::RightShoulder;
		ButtonNames[12] = FGamepadKeyNames::FaceButtonTop;
		ButtonNames[13] = FGamepadKeyNames::FaceButtonRight;
		ButtonNames[14] = FGamepadKeyNames::FaceButtonBottom;
		ButtonNames[15] = FGamepadKeyNames::FaceButtonLeft;
		ButtonNames[16] = FGamepadKeyNames::Invalid;
		ButtonNames[17] = FGamepadKeyNames::Invalid;
		ButtonNames[18] = FGamepadKeyNames::Invalid;
		ButtonNames[19] = FGamepadKeyNames::Invalid;
		ButtonNames[20] = FGamepadKeyNames::SpecialLeft;

		// now the virtual buttons (starting at MAX_NUM_CONTROLLER_BUTTONS)
		ButtonNames[NUM_PHYSICAL_BUTTONS + 0] = FGamepadKeyNames::LeftStickLeft;
		ButtonNames[NUM_PHYSICAL_BUTTONS + 1] = FGamepadKeyNames::LeftStickRight;
		ButtonNames[NUM_PHYSICAL_BUTTONS + 2] = FGamepadKeyNames::LeftStickUp;
		ButtonNames[NUM_PHYSICAL_BUTTONS + 3] = FGamepadKeyNames::LeftStickDown;
		ButtonNames[NUM_PHYSICAL_BUTTONS + 4] = FGamepadKeyNames::RightStickLeft;
		ButtonNames[NUM_PHYSICAL_BUTTONS + 5] = FGamepadKeyNames::RightStickRight;
		ButtonNames[NUM_PHYSICAL_BUTTONS + 6] = FGamepadKeyNames::RightStickUp;
		ButtonNames[NUM_PHYSICAL_BUTTONS + 7] = FGamepadKeyNames::RightStickDown;

#if PLATFORM_PS4
		FCoreDelegates::OnUserLoginChangedEvent.AddRaw(this, &FPS4Controllers::UserLoginEventHandler);
#endif

	}

	virtual ~FPS4Controllers()
	{
	}

	bool IsGamepadAttached() const
	{
		for (int32 UserIndex = 0; UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS; UserIndex++)
		{
			const FControllerState& ControllerState = ControllerStates[UserIndex];
			if (ControllerState.bIsConnected)
			{
				return true;
			}
		}
		return false;
	}

	void SendControllerEvents(TSharedRef< FGenericApplicationMessageHandler >& MessageHandler)
	{
		for (int32 UserIndex = 0; UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS; UserIndex++)
		{
				SendControllerEvents(UserIndex, MessageHandler);
		}
	}

	void SendControllerEvents(int32 UserIndex, TSharedRef< FGenericApplicationMessageHandler >& MessageHandler)
	{		
		FControllerState& ControllerState = ControllerStates[UserIndex];

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
				FCoreDelegates::OnControllerConnectionChange.Broadcast(true, ControllerState.UserID, UserIndex);
			}
			else if (bWasConnected && !ControllerState.bIsConnected)
			{
				FCoreDelegates::OnControllerConnectionChange.Broadcast(false, ControllerState.UserID, UserIndex);
			}

#define ANALOG_TO_FLOAT(x) (((float)(x) - 128.0f) / 128.f)
#define TRIGGER_TO_FLOAT(x) ((float)(x) / 255.f)

#define NEGATIVE_DEADZONE 64
#define POSITIVE_DEADZONE 128 + 64

			const auto& PadData = ControllerState.PadData;
			const auto& PreviousPadData = ControllerState.PreviousPadData;

			// Send new analog data if it's different or outside the platform deadzone.
			if (PadData.leftStick.x != PreviousPadData.leftStick.x || (PadData.leftStick.x < NEGATIVE_DEADZONE || PadData.leftStick.x > POSITIVE_DEADZONE))
			{
				float Value = ANALOG_TO_FLOAT(PadData.leftStick.x);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, ControllerState.ControllerId, Value);
			}

			if (PadData.leftStick.y != PreviousPadData.leftStick.y || (PadData.leftStick.y < NEGATIVE_DEADZONE || PadData.leftStick.y > POSITIVE_DEADZONE))
			{
				float Value = ANALOG_TO_FLOAT(255 - PadData.leftStick.y);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, ControllerState.ControllerId, Value);
			}

			if (PadData.rightStick.x != PreviousPadData.rightStick.x || (PadData.rightStick.x < NEGATIVE_DEADZONE || PadData.rightStick.x > POSITIVE_DEADZONE))
			{
				float Value = ANALOG_TO_FLOAT(PadData.rightStick.x);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogX, ControllerState.ControllerId, Value);
			}

			if (PadData.rightStick.y != PreviousPadData.rightStick.y || (PadData.rightStick.y < NEGATIVE_DEADZONE || PadData.rightStick.y > POSITIVE_DEADZONE))
			{
				float Value = ANALOG_TO_FLOAT(255 - PadData.rightStick.y);
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogY, ControllerState.ControllerId, Value);
			}

			if (PadData.analogButtons.l2 != PreviousPadData.analogButtons.l2)
			{
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftTriggerAnalog, ControllerState.ControllerId, TRIGGER_TO_FLOAT(PadData.analogButtons.l2));
			}

			if (PadData.analogButtons.r2 != PreviousPadData.analogButtons.r2)
			{
				MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightTriggerAnalog, ControllerState.ControllerId, TRIGGER_TO_FLOAT(PadData.analogButtons.r2));
			}

			if (bDS4TouchAxisButtons && PadData.touchData.touchNum > 0)
			{
				bool bSendX = true;
				bool bSendY = true;
				const ScePadTouch& Touch = PadData.touchData.touch[0];
				const FVector2D NewTouchPos(Touch.x, Touch.y);

				// Set Sends to false if we find a corresponding old touch that matches the position
				for (int32 OldTouchIndex = 0; OldTouchIndex < SCE_PAD_MAX_TOUCH_NUM; OldTouchIndex++)
				{
					if (ControllerState.TouchMapping[OldTouchIndex].Z == Touch.id)
					{
						if (ControllerState.TouchMapping[OldTouchIndex].X == Touch.x)
						{
							bSendX = false;
						}

						if (ControllerState.TouchMapping[OldTouchIndex].Y == Touch.y)
						{
							bSendX = false;
						}
						break;
					}
				}

				if (bSendX)
				{
					const float NormalizedX = NewTouchPos.X / (float)ControllerState.TouchPadExtents.X;
					MessageHandler->OnControllerAnalog(FGamepadKeyNames::SpecialLeft_X, ControllerState.ControllerId, NormalizedX);
				}

				if (bSendY)
				{
					const float NormalizedY = NewTouchPos.Y / (float)ControllerState.TouchPadExtents.Y;
					MessageHandler->OnControllerAnalog(FGamepadKeyNames::SpecialLeft_Y, ControllerState.ControllerId, NormalizedY);
				}
			}

			uint8 NewButtonState[MAX_NUM_CONTROLLER_BUTTONS];

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


			for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ++ButtonIndex)
			{
				if (NewButtonState[ButtonIndex] != ControllerState.PreviousButtonState[ButtonIndex])
				{
					// was it pressed or released?
					if (NewButtonState[ButtonIndex] != 0)
					{
						MessageHandler->OnControllerButtonPressed(ButtonNames[ButtonIndex], ControllerState.ControllerId, false);

						// set the button's NextRepeatTime to the InitialButtonRepeatDelay
						ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
					}
					else
					{
						MessageHandler->OnControllerButtonReleased(ButtonNames[ButtonIndex], ControllerState.ControllerId, false);
					}
				}
				else if (NewButtonState[ButtonIndex] != 0 && ControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime)
				{
					MessageHandler->OnControllerButtonPressed(ButtonNames[ButtonIndex], ControllerState.ControllerId, true);

					// set the button's NextRepeatTime to the ButtonRepeatDelay
					ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
				}
			}			

			// process touches
			bool ProcessedMappings[SCE_PAD_MAX_TOUCH_NUM];
			bool ProcessedTouches[SCE_PAD_MAX_TOUCH_NUM];
			FMemory::Memzero(ProcessedMappings);
			FMemory::Memzero(ProcessedTouches);

			const int32 UserBoundToCursor = 0;
			const int32 TouchBoundToCursor = 0;
			FIntVector CursorMovement(0, 0, 0);

			// there was one here last frame, now we need to see if it's still held or if it was let go
			for (int NewTouchIndex = 0; NewTouchIndex < ControllerState.PadData.touchData.touchNum; NewTouchIndex++)
			{
				const ScePadTouch& Touch = ControllerState.PadData.touchData.touch[NewTouchIndex];

				bool bFoundOldTouch = false;
				for (int32 OldTouchIndex = 0; OldTouchIndex < SCE_PAD_MAX_TOUCH_NUM && !bFoundOldTouch; OldTouchIndex++)
				{
					// look for existing touch
					if (ControllerState.TouchMapping[OldTouchIndex].Z == Touch.id)
					{
						// only gate the actual message, is necessary for mouse events also.
						if (bDS4TouchEvents)
						{
							// found one, update it
							MessageHandler->OnTouchMoved(FVector2D(Touch.x, Touch.y), OldTouchIndex, ControllerState.ControllerId);	
						}

						bFoundOldTouch = true;

						if (UserIndex == UserBoundToCursor && OldTouchIndex == TouchBoundToCursor)
						{
							CursorMovement.X = Touch.x - ControllerState.TouchMapping[OldTouchIndex].X;
							CursorMovement.Y = Touch.y - ControllerState.TouchMapping[OldTouchIndex].Y;
						}

						// cache position for the release
						ControllerState.TouchMapping[OldTouchIndex].X = Touch.x;
						ControllerState.TouchMapping[OldTouchIndex].Y = Touch.y;

						// mark the mapping as processed
						ProcessedMappings[OldTouchIndex] = true;
						ProcessedTouches[NewTouchIndex] = true;
					}
				}
			}

			// now we look for existing mappings that weren't processed - this would mean they were let go
			for (int32 OldTouchIndex = 0; OldTouchIndex < SCE_PAD_MAX_TOUCH_NUM; OldTouchIndex++)
			{
				FIntVector& OldMapping = ControllerState.TouchMapping[OldTouchIndex];
				if (ProcessedMappings[OldTouchIndex] == false && OldMapping.Z != -1)
				{
					// any unprocessed old mappings are releases
					FVector2D Location(OldMapping.X, OldMapping.Y);

					// only gate the actual message, is necessary for mouse events also.
					if (bDS4TouchEvents)
					{
						MessageHandler->OnTouchEnded(Location, OldTouchIndex, ControllerState.ControllerId);
					}

					// mark it now as unused
					OldMapping.Z = -1;
				}
			}

			for (int NewTouchIndex = 0; NewTouchIndex < ControllerState.PadData.touchData.touchNum; NewTouchIndex++)
			{
				const ScePadTouch& Touch = ControllerState.PadData.touchData.touch[NewTouchIndex];

				// this is a new touch, find a slot to put it in
				if (!ProcessedTouches[NewTouchIndex])
				{
					bool bFoundSlot = false;
					for (int32 FindTouchIndex = 0; FindTouchIndex < SCE_PAD_MAX_TOUCH_NUM && !bFoundSlot; FindTouchIndex++)
					{
						// look for an empty slot
						if (ControllerState.TouchMapping[FindTouchIndex].Z == -1)
						{
							// only gate the actual message, is necessary for mouse events also.
							if (bDS4TouchEvents)
							{
								// send the new press to the game
								MessageHandler->OnTouchStarted(nullptr, FVector2D(Touch.x, Touch.y), NewTouchIndex, ControllerState.ControllerId);
							}

							// record it
							ControllerState.TouchMapping[FindTouchIndex].Z = Touch.id;
							bFoundSlot = true;

							// cache position for the release event
							ControllerState.TouchMapping[FindTouchIndex].X = Touch.x;
							ControllerState.TouchMapping[FindTouchIndex].Y = Touch.y;

							// mark the mapping as processed
							ProcessedMappings[FindTouchIndex] = true;
						}
					}
					ensureMsgf(bFoundSlot, TEXT("Unable to find empty touch slot, but there should be no way for that to happen"));
				}
			}

			// todo: proper user binding for cursor control
			if (bDS4MouseEvents && UserIndex == UserBoundToCursor && Cursor.IsValid())
			{
				const FIntVector& Touch0 = ControllerState.TouchMapping[0];
				if (Touch0.Z != -1)
				{
					// accumulate relative mouse movement as absolute positioning feels awful.
					// make this an option later.
					FVector2D CursorPos = Cursor->GetPosition();
					CursorPos.X += CursorMovement.X;
					CursorPos.Y += CursorMovement.Y;

					// todo: get the real extents to clamp for.
					Cursor->SetPosition(FMath::Clamp((int32)CursorPos.X, -20, 1940), FMath::Clamp((int32)CursorPos.Y, -20, 1100));
				}
			}			

#if PLATFORM_PS4
			// TODO: Currently only user 0 can use the mouse
			extern int32 GPS4EnableMouse;
			if( GPS4EnableMouse && ControllerState.MouseHandle >= 0 && UserIndex == UserBoundToCursor )
			{
				struct FSceMouseButtonToUE4MouseButton
				{
					int32 SceButton;
					EMouseButtons::Type	UE4Button;
				};

				static const FSceMouseButtonToUE4MouseButton MouseButtonMappings[] =
				{
					{ SCE_MOUSE_BUTTON_PRIMARY,		EMouseButtons::Left },
					{ SCE_MOUSE_BUTTON_SECONDARY,	EMouseButtons::Right },
					{ SCE_MOUSE_BUTTON_OPTIONAL,	EMouseButtons::Middle },
					{ SCE_MOUSE_BUTTON_OPTIONAL2,	EMouseButtons::Thumb01 },
					{ SCE_MOUSE_BUTTON_OPTIONAL3,	EMouseButtons::Thumb02 }
				};

				// Handle usb mouse
				SceMouseData MouseData[8];

				int32 NumData = sceMouseRead( ControllerState.MouseHandle, MouseData, ARRAY_COUNT( MouseData ) );
				if( NumData > 0 )
				{
					for( int32 MouseDataIndex = 0; MouseDataIndex < NumData; MouseDataIndex++ )
					{
						if( MouseData[MouseDataIndex].connected == true && ( ( MouseData[MouseDataIndex].buttons & SCE_MOUSE_BUTTON_INTERCEPTED ) == 0 ) )
						{
							if( Cursor.IsValid() )
							{
								// Update cursor position
								FVector2D CursorPos = Cursor->GetPosition();
								CursorPos.X += MouseData[MouseDataIndex].xAxis;
								CursorPos.Y += MouseData[MouseDataIndex].yAxis;
								Cursor->SetPosition( (int32)CursorPos.X, (int32)CursorPos.Y );
							}

							MessageHandler->OnRawMouseMove( MouseData[MouseDataIndex].xAxis, MouseData[MouseDataIndex].yAxis );

							// Update mouse wheel
							if( MouseData[MouseDataIndex].wheel )
							{
								MessageHandler->OnMouseWheel( MouseData[MouseDataIndex].wheel );
							}

							// Test each mouse button
							for( int32 ButtonIndex = 0; ButtonIndex < ARRAY_COUNT( MouseButtonMappings ); ButtonIndex++ )
							{
								int32 SceButton = MouseButtonMappings[ButtonIndex].SceButton;
								
								if( ( MouseData[MouseDataIndex].buttons & SceButton ) != ( ControllerState.MouseButtonState & SceButton ) )
								{
									EMouseButtons::Type UE4MouseButton = MouseButtonMappings[ButtonIndex].UE4Button;
									if( ( MouseData[MouseDataIndex].buttons & SceButton ) != 0 )
									{
										MessageHandler->OnMouseDown( nullptr, UE4MouseButton );
									}
									else
									{
										MessageHandler->OnMouseUp( UE4MouseButton );
									}
								}
							}

							ControllerState.MouseButtonState = MouseData[MouseDataIndex].buttons;
						}
					}
				}
			}
#endif

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
					ControllerState.ControllerId
					);
			}

			// now update last frame with this one
			ControllerState.PreviousPadData = ControllerState.PadData;
			FMemory::Memcpy(ControllerState.PreviousButtonState, NewButtonState);
		}		
#undef ANALOG_TO_FLOAT
#undef TRIGGER_TO_FLOAT

#undef NEGATIVE_DEADZONE
#undef POSITIVE_DEADZONE
	}

	void ResetControllerOrientation(int32 UserIndex)
	{
		check(UserIndex >= 0 && UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS);
		FControllerState& ControllerState = ControllerStates[UserIndex];
		scePadResetOrientation(ControllerState.Handle);
	}

	void SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
	{
		// Don't bother looking if this isn't a channel the PS4 supports
		if (ChannelType != FForceFeedbackChannelType::LEFT_LARGE && ChannelType != FForceFeedbackChannelType::LEFT_SMALL)
		{
			return;
		}

		// The channel is valid, so if the controller is open, set its value
		for (int32 UserIndex=0; UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++UserIndex)
		{
			FControllerState& ControllerState = ControllerStates[UserIndex];

			if (ControllerState.bIsConnected && ControllerState.ControllerId == ControllerId)
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

	void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
	{
		// The channel is valid, so if the controller is open, set its value
		for (int32 UserIndex=0; UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++UserIndex)
		{
			FControllerState& ControllerState = ControllerStates[UserIndex];

			if (ControllerState.bIsConnected && ControllerState.ControllerId == ControllerId)
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

	virtual void SetLightColor(int32 ControllerId, FColor Color)
	{
		// The channel is valid, so if the controller is open, set its value
		for (int32 UserIndex = 0; UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS; ++UserIndex)
		{
			FControllerState& ControllerState = ControllerStates[UserIndex];

			if (ControllerState.bIsConnected && ControllerState.ControllerId == ControllerId)
			{
				ScePadLightBarParam LightBarParam;
				LightBarParam.r = Color.R;
				LightBarParam.g = Color.G;
				LightBarParam.b = Color.B;

				// Send the new values to the controller
				scePadSetLightBar(ControllerState.Handle, &LightBarParam);
				break;
			}
		}
	}

	FControllerState& GetControllerState(int32 ControllerId)
	{
		check(ControllerId >= 0 && ControllerId < SCE_USER_SERVICE_MAX_LOGIN_USERS);
		return ControllerStates[ControllerId];
	}

	void SetCursor(const TSharedPtr<ICursor>& InCursor)
	{
		Cursor = InCursor;
	}

	void SetEmitTouchEvents(bool bInDS4TouchEvents)
	{
		bDS4TouchEvents = bInDS4TouchEvents;
	}

	void SetEmitTouchAxisEvents(bool bInDS4TouchAxisButtons)
	{
		bDS4TouchAxisButtons = bInDS4TouchAxisButtons;
	}

	void SetEmitMouseEvents(bool bInDS4MouseEvents)
	{
		bDS4MouseEvents = bInDS4MouseEvents;
	}

	void SetEmitMotionEvents(bool bInDS4MotionEvents)
	{
		bDS4MotionEvents = bInDS4MotionEvents;
	}

	int32 ConnectStateToUser(int32 UserID, int32 UserIndex)
	{
		FControllerState& ControllerState = ControllerStates[UserIndex];
		if (ControllerState.RefCount)
		{
			check(ControllerState.UserID == UserID);		
			check(ControllerState.Handle != 0);
			
			++ControllerState.RefCount;
			return ControllerState.Handle;
		}
		else
		{
			check(ControllerState.UserID == 0);		
			check(ControllerState.Handle == 0);
		}

		FMemory::Memzero( &ControllerState, sizeof(FControllerState) );
		ControllerState.ControllerId = UserIndex;
		ControllerState.UserID = UserID;
		ControllerState.RefCount = 1;

		// initialize the touches to untouched
		for (int32 TouchIndex = 0; TouchIndex < ARRAY_COUNT(ControllerState.TouchMapping); TouchIndex++)
		{
			ControllerState.TouchMapping[TouchIndex].Z = -1;
		}		

		// if the user is logged in, open the pad
#if PLATFORM_PS4
		check(UserID != SCE_USER_SERVICE_USER_ID_INVALID);
		ControllerState.Handle = scePadOpen(UserID, SCE_PAD_PORT_TYPE_STANDARD, 0, nullptr);

		//doc says all the errors are negative.
		checkf(ControllerState.Handle > 0, TEXT("scePadOpen failed with error 0x%x"), ControllerState.Handle);

		// Open the mouse TODO: Only user 0 can use the mouse
		extern int32 GPS4EnableMouse;
		if( GPS4EnableMouse && UserIndex == 0 )
		{
			SceMouseOpenParam MouseOpenParam;
			MouseOpenParam.behaviorFlag = SCE_MOUSE_OPEN_PARAM_MERGED;
			ControllerState.MouseHandle = sceMouseOpen( UserID, SCE_MOUSE_PORT_TYPE_STANDARD, 0, &MouseOpenParam );
			ControllerState.MouseButtonState = 0;
		}
		else
		{
			ControllerState.MouseHandle = SCE_MOUSE_ERROR_INVALID_HANDLE;
		}
#else		
		ControllerState.Handle = scePadOpen(SCE_USER_SERVICE_STATIC_USER_ID_1 + UserIndex, SCE_PAD_PORT_TYPE_STANDARD, 0, nullptr);
#endif	
		// setting this to false will perform the on-connection logic in the first tick for already connected controllers
		ControllerState.bIsConnected = false;		

		return ControllerState.Handle;
	}

	void DisconnectStateFromUser(int32 UserID, int32 UserIndex)
	{
		check(UserIndex < SCE_USER_SERVICE_MAX_LOGIN_USERS);
		
		FControllerState& ControllerState = ControllerStates[UserIndex];
		check(ControllerState.ControllerId == UserIndex);
		check(ControllerState.UserID == UserID);		
		check(ControllerState.RefCount > 0);
		
		if (--ControllerState.RefCount)
		{
			return;
		}

		int32 Ret = scePadClose(ControllerState.Handle);
		check(Ret == SCE_OK);

#if PLATFORM_PS4
		if( ControllerState.MouseHandle >= 0 )
		{
			sceMouseClose( ControllerState.MouseHandle );
		}
#endif

		FMemory::Memzero(&ControllerState, sizeof(FControllerState));
	}

protected:

#if PLATFORM_PS4
	void UserLoginEventHandler(bool bLoggingIn, int32 UserID, int32 UserIndex)
	{		
		check(UserID != SCE_USER_SERVICE_USER_ID_INVALID);
		
		if (bLoggingIn)		
		{				
			ConnectStateToUser(UserID, UserIndex);				
		}
		else
		{	
			DisconnectStateFromUser(UserID, UserIndex);			
		}
	}
#endif

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

	void UpdateVibeMotors(FControllerState &State)
	{
		// Calculate the appropriate settings by mapping the full channel value set onto the large and small motors.
		const float LargeValue = (State.VibeValues.LeftLarge > State.VibeValues.RightLarge ? State.VibeValues.LeftLarge : State.VibeValues.RightLarge);
		const float SmallValue = (State.VibeValues.LeftSmall > State.VibeValues.RightSmall ? State.VibeValues.LeftSmall : State.VibeValues.RightSmall);

		State.VibeSettings.largeMotor = ConvertToByte(LargeValue);
		State.VibeSettings.smallMotor = ConvertToByte(SmallValue);

		// Send the new values to the controller
		scePadSetVibration(State.Handle, &State.VibeSettings);
	}


	/** Names of all the buttons */
	FGamepadKeyNames::Type ButtonNames[MAX_NUM_CONTROLLER_BUTTONS];

	/** Controller states */
	FControllerState ControllerStates[SCE_USER_SERVICE_MAX_LOGIN_USERS];

	/** For now, a single cursor controlled by ControllerId0 */
	TSharedPtr<ICursor> Cursor;

	/** whether or not we emit touch events from the touchpad */
	bool bDS4TouchEvents;

	/** whether or not we emit axis analog events from the touchpad */
	bool bDS4TouchAxisButtons;

	/** whether or not we emit mouse events from the touchpad */
	bool bDS4MouseEvents;

	/** whether or not we emit motion events from the controller */
	bool bDS4MotionEvents;


	/** Delay before sending a repeat message after a button was first pressed */
	float InitialButtonRepeatDelay;

	/** Delay before sendign a repeat message after a button has been pressed for a while */
	float ButtonRepeatDelay;
};
