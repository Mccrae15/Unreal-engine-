// Copyright 3dRudder 2017, Inc. All Rights Reserved.

#pragma once
 
#include <InputDevice.h>
#define _3DRUDDER_SDK_STATIC
#include "3DRudderSDK.h"
#include "Delegates/DelegateCombinations.h"

// Input Mapping Keys
struct EKeys3dRudder
{
	static const FKey LeftRight;
	static const FKey ForwardBackward;
	static const FKey UpDown;
	static const FKey Rotation;
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

/**
* Interface class for 3dRudder devices 
*/
class F3dRudderDevice : public IInputDevice
{
public:

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
	TSharedRef< FGenericApplicationMessageHandler > m_MessageHandler;
};