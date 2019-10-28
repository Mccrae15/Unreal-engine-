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

#include "3dRudderFunctionLibrary.h"
#include "AxesParamAsset.h"
#include "3dRudderDevice.h"
#include "3dRudderPrivatePCH.h"

DEFINE_LOG_CATEGORY_STATIC(Log3dRudderFunctionLibrary, Log, All);
#define LOCTEXT_NAMESPACE "3dRudderFunctionLibrary"

U3dRudderFunctionLibrary::U3dRudderFunctionLibrary(const FObjectInitializer& ObjectInitializer)	: Super(ObjectInitializer)
{
}

FString U3dRudderFunctionLibrary::GetSDKVersion()
{
	FString version("ffff");
	if (F3dRudderDevice::s_pSdk != nullptr)
		version = FString::Printf(TEXT("%04x"), F3dRudderDevice::s_pSdk->GetSDKVersion());
	return version;	
}

int32 U3dRudderFunctionLibrary::GetNumberOfConnectedDevice()
{
	if (F3dRudderDevice::s_pSdk != nullptr)
		return F3dRudderDevice::s_pSdk->GetNumberOfConnectedDevice();
	else
		return 0;
}

bool U3dRudderFunctionLibrary::IsConnected(int32 portNumber)
{
	if (F3dRudderDevice::s_pSdk != nullptr)
		return F3dRudderDevice::s_pSdk->IsDeviceConnected(portNumber);
	else
		return false;
}

FString U3dRudderFunctionLibrary::GetFirmwareVersion(int32 portNumber)
{
	FString version("ffff");
	if (F3dRudderDevice::s_pSdk != nullptr)
		version = FString::Printf(TEXT("%04x"), F3dRudderDevice::s_pSdk->GetVersion(portNumber));
	return version;
}

E3dRudderStatus U3dRudderFunctionLibrary::GetStatus(int32 portNumber)
{
	if (F3dRudderDevice::s_pSdk != nullptr)
		return (E3dRudderStatus)F3dRudderDevice::s_pSdk->GetStatus(portNumber);
	else
		return E3dRudderStatus::NoStatus;
}

FString U3dRudderFunctionLibrary::GetStatusString(int32 portNumber)
{
	FString status = "NoStatus";
	if (F3dRudderDevice::s_pSdk != nullptr)
	{
		switch (F3dRudderDevice::s_pSdk->GetStatus(portNumber))
		{
		case ns3dRudder::NoFootStayStill:
			status = "No Foot Stay Still";
			break;
		case ns3dRudder::Initialization:
			status = "Initialization";
			break;
		case ns3dRudder::PutYourFeet:
			status = "Put Your Feet";
			break;
		case ns3dRudder::PutSecondFoot:
			status = "Put Second Foot";
			break;
		case ns3dRudder::StayStill:
			status = "Stay Still";
			break;
		case ns3dRudder::InUse:
			status = "In Use";
			break;
		case ns3dRudder::Frozen:
			status = "Frozen";
			break;
		case ns3dRudder::IsNotConnected:
			status = "Is Not Connected";
			break;
		case ns3dRudder::Error:
			status = "Error";
			break;
		default:
			status = "NoStatus";
			break;
		}
	}
	return status;
}

E3dRudderError U3dRudderFunctionLibrary::GetAxes(int32 portNumber, UAxesParamAsset* axesParamAsset, float &LeftRight, float &ForwardBackward, float &UpDown, float &Rotation)
{		
	ns3dRudder::ErrorCode error = ns3dRudder::NotInitialized;
	LeftRight = ForwardBackward = UpDown = Rotation = 0.0;
	if (F3dRudderDevice::s_pSdk != nullptr)
	{
		ns3dRudder::IAxesParam* pAxesParam = nullptr;
		if (axesParamAsset != nullptr)
		{
			pAxesParam = axesParamAsset->GetAxesParam();
		}

		ns3dRudder::AxesValue axesValue;
		error = F3dRudderDevice::s_pSdk->GetAxes(portNumber, pAxesParam, &axesValue);
		if (error == ns3dRudder::Success)
		{
			ForwardBackward = axesValue.Get(ns3dRudder::ForwardBackward);
			LeftRight = axesValue.Get(ns3dRudder::LeftRight);			
			UpDown = axesValue.Get(ns3dRudder::UpDown);			
			Rotation = axesValue.Get(ns3dRudder::Rotation);
		}	
	}
	return (E3dRudderError)error;
}

void U3dRudderFunctionLibrary::ComputeSmooth(FSmoothMovement smooth, float DeltaTime, float LeftRight, float ForwardBackward, float UpDown, float Rotation, 
	float &LeftRightSmooth, float &ForwardBackwardSmooth, float &UpDownSmooth, float &RotationSmooth, FSmoothMovement &speed)
{
	if (smooth.ForwardBackward.Enable)
		ForwardBackwardSmooth = smooth.ForwardBackward.ComputeSpeed(ForwardBackward, DeltaTime);
	else
		ForwardBackwardSmooth = ForwardBackward;

	if (smooth.LeftRight.Enable)
		LeftRightSmooth = smooth.LeftRight.ComputeSpeed(LeftRight, DeltaTime);
	else
		LeftRightSmooth = LeftRight;

	if (smooth.UpDown.Enable)
		UpDownSmooth = smooth.UpDown.ComputeSpeed(UpDown, DeltaTime);
	else
		UpDownSmooth = UpDown;

	if (smooth.Rotation.Enable)
		RotationSmooth = smooth.Rotation.ComputeSpeed(Rotation, DeltaTime);
	else
		RotationSmooth = Rotation;

	speed = smooth;
}

bool U3dRudderFunctionLibrary::PlaySound(int32 portNumber, int32 frequency, int32 duration)
{
	if (F3dRudderDevice::s_pSdk != nullptr)
	{
		if (F3dRudderDevice::s_pSdk->PlaySnd(portNumber, frequency, duration) == ns3dRudder::Success)
		{
			return true;
		}
	}
	return false;
}

void U3dRudderFunctionLibrary::GetSensor(int32 portNumber, int32 &sensor1, int32 &sensor2, int32 &sensor3, int32 &sensor4, int32 &sensor5, int32 &sensor6)
{	
	sensor1 = sensor2 = sensor3 = sensor4 = sensor5 = sensor6 = 0;
	if (F3dRudderDevice::s_pSdk != nullptr)
	{
		sensor1 = F3dRudderDevice::s_pSdk->GetSensor(portNumber, 0);
		sensor2 = F3dRudderDevice::s_pSdk->GetSensor(portNumber, 1);
		sensor3 = F3dRudderDevice::s_pSdk->GetSensor(portNumber, 2);
		sensor4 = F3dRudderDevice::s_pSdk->GetSensor(portNumber, 3);
		sensor5 = F3dRudderDevice::s_pSdk->GetSensor(portNumber, 4);
		sensor6 = F3dRudderDevice::s_pSdk->GetSensor(portNumber, 5);
	}
}

#undef LOCTEXT_NAMESPACE