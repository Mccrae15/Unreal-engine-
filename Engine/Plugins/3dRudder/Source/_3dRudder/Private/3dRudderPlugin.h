// Copyright 3dRudder 2017, Inc. All Rights Reserved.

#pragma once
#include "I3dRudderPlugin.h"
#include "3dRudderDevice.h"
 
class F3dRudderPlugin : public I3dRudderPlugin
{
public:
	/** I3dRudderInterface implementation */
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);
 
	//virtual void StartupModule() override; // This is not required as IInputDeviceModule handels it!
	virtual void ShutdownModule();

	TSharedPtr< class F3dRudderDevice > m_3dRudderDevice;

private:
	void RegisterSettings();
	void UnregisterSettings();
};