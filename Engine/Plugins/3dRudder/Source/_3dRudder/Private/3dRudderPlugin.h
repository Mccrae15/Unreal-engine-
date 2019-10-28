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