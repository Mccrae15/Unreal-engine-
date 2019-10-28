/************************************************************************************

	Copyright © 2014-2019, 3dRudder SA, All rights reserved
	For terms of use: https://3drudder-dev.com/docs/introduction/sdk_licensing_agreement/
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met :

	* Redistributions of source code must retain the above copyright notice, and
	this list of conditions.
	* Redistributions in binary form must reproduce the above copyright
	notice and this list of conditions.
	* The name of 3dRudder may not be used to endorse or promote products derived from
	this software without specific prior written permission.

    Copyright 1998-2019 Epic Games, Inc. All Rights Reserved

************************************************************************************/

#pragma once
 
#include <InputDevice.h>
#define _3DRUDDER_SDK_STATIC
#include "3DRudderSDK.h"
#include "Delegates/DelegateCombinations.h"

#define DIALOG_3DRUDDER 1
#if PLATFORM_PS4 && DIALOG_3DRUDDER
#include "3dRudderDialog.h"
#endif
// Input Mapping Keys
struct EKeys3dRudder
{
	static const FKey LeftRight;
	static const FKey ForwardBackward;
	static const FKey UpDown;
	static const FKey Rotation;

	static const FKey Left;
	static const FKey Right;
	static const FKey Forward;
	static const FKey Backward;
	static const FKey Up;
	static const FKey Down;
	static const FKey RotationLeft;
	static const FKey RotationRight;
	
	static const FKey Status;
	static const FKey Sensor1;
	static const FKey Sensor2;
	static const FKey Sensor3;
	static const FKey Sensor4;
	static const FKey Sensor5;
	static const FKey Sensor6;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConnected, const uint32, const bool);

class Event3dRudder : public ns3dRudder::IEvent
{
public:	
	Event3dRudder() {}

	virtual void OnConnect(uint32_t nDeviceNumber) { Delegate.Broadcast(nDeviceNumber, true); };
	virtual void OnDisconnect(uint32_t nDeviceNumber) { Delegate.Broadcast(nDeviceNumber, false); };

	FOnConnected Delegate;
};

struct Last3dRudderState {
	bool leftIsActive;
	bool rightIsActive;
	bool forwardIsActive;
	bool backwardIsActive;
	bool upIsActive;
	bool downIsActive;
	bool rotationLeftIsActive;
	bool rotationRightIsActive;
};

/**
* Interface class for 3dRudder devices 
*/
class F3dRudderDevice : public IInputDevice
{
public:

	Last3dRudderState previousState[_3DRUDDER_SDK_MAX_DEVICE];

	static ns3dRudder::CSdk* s_pSdk;
	static ns3dRudder::AxesParamDefault s_AxesParamDefault;
	static Event3dRudder* s_Events;

	F3dRudderDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);
 
	/** Tick the interface (e.g. check for new controllers) */
	void Tick(float DeltaTime) override;
 
	/** Poll for controller state and send events if needed */
	void SendControllerEvents() override;
 
	/** Set which MessageHandler will get the events from SendControllerEvents. */
	void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override;
 
	/** Exec handler to allow console commands to be passed through for debugging */
	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	// IForceFeedbackSystem pass through functions
	void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& Values) override;

	virtual ~F3dRudderDevice();

private:	
	
#if PLATFORM_PS4 && DIALOG_3DRUDDER
	C3dRudderDialog m_dialog;
	bool m_init = false;
	bool m_wantClose = false;
	float m_timer = 0.0f;
#endif
	TSharedRef< FGenericApplicationMessageHandler > m_MessageHandler;
	ns3dRudder::Status m_status;
};