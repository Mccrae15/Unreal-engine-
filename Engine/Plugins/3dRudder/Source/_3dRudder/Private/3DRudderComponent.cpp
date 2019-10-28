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

#include "3dRudderComponent.h"
#include "3dRudderDevice.h"

FSpeedFactor::FSpeedFactor()
{
	LeftRight = 1.0f;
	ForwardBackward = 1.0f;
	UpDown = 1.0f;
	Rotation = 1.0f;
}

// Sets default values for this component's properties
U3dRudderComponent::U3dRudderComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;	
	// ...
	FSoftObjectPath AxesParamsClassName = FSoftObjectPath(FString("/3dRudder/Default.Default"));
	if (AxesParamsClassName.IsValid())
		AxesParam = LoadObject<UAxesParamAsset>(nullptr, *AxesParamsClassName.ToString());
	m_pAxesParam = nullptr;
}

void U3dRudderComponent::BeginPlay()
{
	Super::BeginPlay();	

	if (!delegateHandle.IsValid())
	{
		Event3dRudder* events = F3dRudderDevice::s_Events;
		if (events != nullptr)
			delegateHandle = events->Delegate.AddUObject(this, &U3dRudderComponent::OnConnect);
	}

	if (AxesParam != nullptr)
	{
		m_pAxesParam = AxesParam->GetAxesParam();
	}
	else
	{
		m_pAxesParam = &F3dRudderDevice::s_AxesParamDefault;
	}
	check(m_pAxesParam);
}

void U3dRudderComponent::OnConnect(const uint32 PortNumber, const bool Connected)
{
	if (Port == PortNumber)
		On3dRudderConnected.Broadcast(Connected);
}

U3dRudderComponent::~U3dRudderComponent()
{
	Event3dRudder* events = F3dRudderDevice::s_Events;
	if (events != nullptr && delegateHandle.IsValid())
		events->Delegate.Remove(delegateHandle);

	if ( m_pAxesParam )
	{
		//delete m_pAxesParam;
		m_pAxesParam = nullptr;
	}
}

bool U3dRudderComponent::IsConnected()
{
	if (F3dRudderDevice::s_pSdk != nullptr)
		return F3dRudderDevice::s_pSdk->IsDeviceConnected(Port);
	else
		return false;
}

FString U3dRudderComponent::GetFirmwareVersion()
{
	if (F3dRudderDevice::s_pSdk != nullptr)
		return FString::Printf(TEXT("%04x"), F3dRudderDevice::s_pSdk->GetVersion(Port));
	else
		return FString("ffff");
}

E3dRudderStatus U3dRudderComponent::GetStatus()
{
	if (F3dRudderDevice::s_pSdk != nullptr)
		return (E3dRudderStatus)F3dRudderDevice::s_pSdk->GetStatus(Port);
	else
		return E3dRudderStatus::NoStatus;
}

FString U3dRudderComponent::GetStatusString()
{
	FString statusString = "NoStatus";
	if (F3dRudderDevice::s_pSdk != nullptr)
	{
		switch (F3dRudderDevice::s_pSdk->GetStatus(Port))
		{
		case ns3dRudder::NoFootStayStill:
			statusString = "No Foot Stay Still";
			break;
		case ns3dRudder::Initialization:
			statusString = "Initialization";
			break;
		case ns3dRudder::PutYourFeet:
			statusString = "Put Your Feet";
			break;
		case ns3dRudder::PutSecondFoot:
			statusString = "Put Second Foot";
			break;
		case ns3dRudder::StayStill:
			statusString = "Stay Still";
			break;
		case ns3dRudder::InUse:
			statusString = "In Use";
			break;
		case ns3dRudder::Frozen:
			statusString = "Frozen";
			break;
		case ns3dRudder::IsNotConnected:
			statusString = "Is Not Connected";
			break;
		case ns3dRudder::Error:
			statusString = "Error";
			break;
		default:
			statusString = "No Status";
			break;
		}
	}
	return statusString;
}

E3dRudderError U3dRudderComponent::GetAxes(float DeltaTime, float &LeftRight, float &ForwardBackward, float &UpDown, float &Rotation)
{	
	LeftRight = ForwardBackward = UpDown = Rotation = 0.0;
	if (F3dRudderDevice::s_pSdk != nullptr)
	{
		ns3dRudder::AxesValue axesValue;
		ns3dRudder::ErrorCode error = F3dRudderDevice::s_pSdk->GetAxes(Port, m_pAxesParam, &axesValue);
		if (error == ns3dRudder::Success)
		{
			if (Smooth.ForwardBackward.Enable)
				ForwardBackward = Smooth.ForwardBackward.ComputeSpeed(SpeedFactor.ForwardBackward * axesValue.Get(ns3dRudder::ForwardBackward), DeltaTime);
			else
				ForwardBackward = SpeedFactor.ForwardBackward * axesValue.Get(ns3dRudder::ForwardBackward);

			if (Smooth.LeftRight.Enable)
				LeftRight = Smooth.LeftRight.ComputeSpeed(SpeedFactor.LeftRight * axesValue.Get(ns3dRudder::LeftRight), DeltaTime);
			else
				LeftRight = SpeedFactor.LeftRight * axesValue.Get(ns3dRudder::LeftRight);

			if (Smooth.UpDown.Enable)
				UpDown = Smooth.UpDown.ComputeSpeed(SpeedFactor.UpDown * axesValue.Get(ns3dRudder::UpDown), DeltaTime);
			else
				UpDown = SpeedFactor.UpDown * axesValue.Get(ns3dRudder::UpDown);

			if (Smooth.Rotation.Enable)
				Rotation = Smooth.Rotation.ComputeSpeed(SpeedFactor.Rotation * axesValue.Get(ns3dRudder::Rotation), DeltaTime);
			else
				Rotation = SpeedFactor.Rotation * axesValue.Get(ns3dRudder::Rotation);
		}
		return (E3dRudderError)error;
	}
	return E3dRudderError::NotInitialized;
}

bool U3dRudderComponent::PlaySound(int32 frequency, int32 duration)
{
	if (F3dRudderDevice::s_pSdk != nullptr)
	{
		if (F3dRudderDevice::s_pSdk->PlaySnd(Port, frequency, duration) == ns3dRudder::Success)
		{
			return true;
		}
	}
	return false;
}

void U3dRudderComponent::GetSensor(int32 &sensor1, int32 &sensor2, int32 &sensor3, int32 &sensor4, int32 &sensor5, int32 &sensor6)
{
	sensor1 = sensor2 = sensor3 = sensor4 = sensor5 = sensor6 = 0;
	if (F3dRudderDevice::s_pSdk != nullptr)
	{
		sensor1 = F3dRudderDevice::s_pSdk->GetSensor(Port, 0);
		sensor2 = F3dRudderDevice::s_pSdk->GetSensor(Port, 1);
		sensor3 = F3dRudderDevice::s_pSdk->GetSensor(Port, 2);
		sensor4 = F3dRudderDevice::s_pSdk->GetSensor(Port, 3);
		sensor5 = F3dRudderDevice::s_pSdk->GetSensor(Port, 4);
		sensor6 = F3dRudderDevice::s_pSdk->GetSensor(Port, 5);
	}
}

// Called every frame
void U3dRudderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (F3dRudderDevice::s_pSdk != nullptr)
	{
		ns3dRudder::Status _status = m_Status;
		m_Status = F3dRudderDevice::s_pSdk->GetStatus(Port);
		if (m_Status != _status)
		{
			OnStatusChangedDelegate.Broadcast(GetStatusString());
		}
	}
}

