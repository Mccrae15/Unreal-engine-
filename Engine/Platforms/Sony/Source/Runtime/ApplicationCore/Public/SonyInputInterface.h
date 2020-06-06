// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/IntVector.h"
#include "Misc/CallbackDevice.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"

// event queue
#include <sys/event.h>

// joypad lib
#include <user_service.h>
#include <pad.h>

// mouse support
#include <mouse.h>

#define SUPPORT_IME_KEYBOARD 1

#include "GenericPlatform/IInputInterface.h"
#include COMPILED_PLATFORM_HEADER(Controllers.h)

/**
 * Interface class for Sony input devices.    
 */
class FSonyInputInterface
	: public IInputInterface
{
public:

	/** Initializes the interface. */
	static TSharedRef< FSonyInputInterface > Create(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor);

public:

	~FSonyInputInterface() { }

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	/** Tick the interface (i.e check for new controllers) */
	void Tick( float DeltaTime );

	/** Poll for controller state and send events if needed. */
	void SendControllerEvents();

	/**
	 * IInputInterface implementation.
	 */
	virtual void SetForceFeedbackChannelValue (int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override;
	virtual void SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values) override;

	virtual void SetDeviceProperty(int32 ControllerId, const FInputDeviceProperty* Property) override;

	virtual void SetLightColor(int32 ControllerId, FColor Color) override;
	virtual void ResetLightColor(int32 ControllerId) override;

	bool IsMouseAttached() const;
	bool IsGamepadAttached() const;

	void ResetControllerOrientation(int32 UserIndex);

	// Connects the user controller state to the specified user ID
	int32 ConnectControllerStateToUser(int32 UserID, int32 UserIndex);
	
	// Discnnects the user controller state from the specified user ID
	void DisconnectControllerStateFromUser(int32 UserID, int32 UserIndex);

	virtual void AddExternalInputDevice(TSharedPtr<class IInputDevice> InputDevice);

	uint32 GetImeKeyCodeStatus() { return ImeKeyCodeStatus; }

	bool HasExternalKeyboard() const;

private:

	FSonyInputInterface(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor);

	/**
	 * Updates internal controller state.
	 */
	static void ProcessControllerInput();

	/**
	 * Updates the controller vibe settings.
	 */
	struct FControllerState;
	void UpdateVibeMotors(FControllerState &State);

#if SUPPORT_IME_KEYBOARD
	/**
	 * Callback for handling IME events.
	 */
	static void KeyboardEventCallback(void* Arg, const struct SceImeEvent* Event);

#endif	

private:

	bool bIMENeedsOpen;
	bool bIMENeedsClose;
	uint32 ImeKeyCodeStatus;

	// the object that encapsulates all the controller logic, shared with the windows input plug-in.
	FPlatformControllers Controllers;

	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;
	const TSharedPtr< ICursor > Cursor;	

	/** List of input devices implemented in external modules. */
	TArray<TSharedPtr<class IInputDevice>> ExternalInputDevices;
};
