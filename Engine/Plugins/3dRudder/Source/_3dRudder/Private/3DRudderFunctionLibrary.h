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

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Curves/CurveFloat.h"
#include "3dRudderFunctionLibrary.generated.h"


/** Defines the Status Value*/
UENUM(BlueprintType)
enum class E3dRudderStatus : uint8
{
	/// While the 3dRudder initializes.
	NoStatus,
	/// Puts the 3dRudder on the floor, curved side below, without putting your feet on the device. The user waits for approx. 5 seconds for the 3dRudder to boot up until 3 short beeps are heard.
	NoFootStayStill,
	/// The 3dRudder initialize for about 2 seconds. Once done a long beep will be heard from the device. The 3dRudder is then operational.
	Initialization,
	/// Put your first feet on the 3dRudder.
	PutYourFeet,
	/// Put your second Foot on the 3dRudder.
	PutSecondFoot,
	/// The user must wait still for half a second for user calibration until a last short beep is heard from the device. The 3dRudder is then ready to be used.
	StayStill,
	/// The 3dRudder is in use.
	InUse,
	/// The 3dRudder is frozen.
	Frozen = 253,
	/// The 3dRudder is not connected.
	IsNotConnected = 254,
	/// Call GetLastError function to get the error code
	Error = 255,
};

/** Defines the error code*/
UENUM(BlueprintType)
enum class E3dRudderError : uint8
{
	/// The command had been successful
	Success = 0,
	/// The 3dRudder is not connected
	NotConnected,
	/// The device fail to execute the command
	Fail,
	/// Incorrect intern command
	IncorrectCommand,
	/// Timeout communication with the 3dRudder
	Timeout,
	/// Device not supported by the SDK
	DeviceNotSupported,
	/// The new connected 3dRudder did an error at the Initialization
	DeviceInitError,
	/// The security of the 3dRudder had not been validated.
	ValidationError,
	/// The security of the 3dRudder did a timeout : it could append when you stop the thread when debugging. 
	ValidationTimeOut,
	/// The 3dRudder isn't ready
	NotReady,
	/// Indicated that the Firmware must be updated
	FirmwareNeedToBeUpdated,
	/// The 3dRudder's SDK isn't initialized
	NotInitialized,
	/// This command is not supported in this version  of the SDK (or plateform).
	NotSupported,
	/// The dashboard is not installed
	DashboardInstallError,
	/// The dashboard need to be updated
	DashboardUpdateError,
	/// Other Errors.
	Other = 0xFF
};

/**
 * Curve Editor 
 */
/*class CurveUnreal : public ns3dRudder::Curve
{
public:
	CurveUnreal() : ns3dRudder::Curve() { pCurve = nullptr; }
	CurveUnreal(float fxSat, float fDeadZone, float fExp) :ns3dRudder::Curve(fxSat, fDeadZone, fExp) { pCurve = nullptr; }
	CurveUnreal(float fxSat, UCurveFloat* curve) : ns3dRudder::Curve(fxSat, 0.0f, 1.0f)
	{
		pCurve = curve;
	}

	virtual float CalcCurveValue(float fValue)  const 
	{
		if ( pCurve != nullptr )
			return GetXSat()*pCurve->GetFloatValue(fValue);
		else
			return ns3dRudder::GetSDK()->CalcCurveValue(GetDeadZone(), GetXSat(), GetExp(), fValue);		
	}

private:
	UCurveFloat* pCurve;
};*/

/*class AxesParamUnReal : public ns3dRudder::IAxesParam
{
public:
	AxesParamUnReal(UCurveFloat*LeftRightCurve=nullptr, UCurveFloat*ForwardBackwardCurve = nullptr, UCurveFloat*UpDownCurve = nullptr, UCurveFloat*RotatonCurve = nullptr) : IAxesParam()
	{		
		m_Curve[ns3dRudder::LeftRight] = CurveUnreal(1.0f, LeftRightCurve);
		m_Curve[ns3dRudder::ForwardBackward] = CurveUnreal(1.0f, ForwardBackwardCurve);
		m_Curve[ns3dRudder::UpDown] = CurveUnreal(0.6f, UpDownCurve);
		m_Curve[ns3dRudder::Rotation] = CurveUnreal(1.0f, RotatonCurve);

		SetNonSymmetrical(true);
	}

	ns3dRudder::ErrorCode UpdateParam(uint32_t nPortNumber)
	{
		ns3dRudder::ErrorCode error = ns3dRudder::Success;
		ns3dRudder::DeviceInformation *pInfo = ns3dRudder::GetSDK()->GetDeviceInformation(nPortNumber);
		if (pInfo != nullptr)
		{
			if (LeftRightCurve)
				m_Curve[ns3dRudder::LeftRight] = CurveUnreal(pInfo->GetUserRoll() / pInfo->GetMaxRoll(), LeftRightCurve);
			else
				m_Curve[ns3dRudder::LeftRight] = CurveUnreal(pInfo->GetUserRoll() / pInfo->GetMaxRoll(), 2.0f / pInfo->GetMaxRoll(), 2.0f);
			if (ForwardBackwardCurve)
				m_Curve[ns3dRudder::ForwardBackward] = CurveUnreal(pInfo->GetUserPitch() / pInfo->GetMaxPitch(), ForwardBackwardCurve);
			else
				m_Curve[ns3dRudder::ForwardBackward] = CurveUnreal(pInfo->GetUserPitch() / pInfo->GetMaxPitch(), 2.0f / pInfo->GetMaxPitch(), 2.0f);
			if (UpDownCurve)
				m_Curve[ns3dRudder::UpDown] = CurveUnreal(0.6f, UpDownCurve);
			else
				m_Curve[ns3dRudder::UpDown] = CurveUnreal(0.6f, 0.08f, 4.0f);
			if (RotatonCurve)
				m_Curve[ns3dRudder::Rotation] = CurveUnreal(pInfo->GetUserYaw() / pInfo->GetMaxYaw(), RotatonCurve);
			else
				m_Curve[ns3dRudder::Rotation] = CurveUnreal(pInfo->GetUserYaw() / pInfo->GetMaxYaw(), 3.0f / pInfo->GetMaxYaw(), 2.0f);
		}
		else
			error = ns3dRudder::Fail;

		SetCurve(ns3dRudder::LeftRight, &m_Curve[ns3dRudder::LeftRight]);
		SetCurve(ns3dRudder::ForwardBackward, &m_Curve[ns3dRudder::ForwardBackward]);
		SetCurve(ns3dRudder::UpDown, &m_Curve[ns3dRudder::UpDown]);
		SetCurve(ns3dRudder::Rotation, &m_Curve[ns3dRudder::Rotation]);

		SetRoll2YawCompensation(pInfo->GetDefaultRoll2YawCompensation());
		return error;
	}
private:
	CurveUnreal m_Curve[ns3dRudder::MaxAxes];
};*/

/**
*
*/
UCLASS()
class _3DRUDDER_API U3dRudderFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintPure, Category = "3dRudder")
		static FString GetSDKVersion();

	UFUNCTION(BlueprintPure, Category = "3dRudder")
		static int32 GetNumberOfConnectedDevice();

	UFUNCTION(BlueprintPure, Category = "3dRudder")
		static bool IsConnected(int32 portNumber);

	UFUNCTION(BlueprintPure, Category = "3dRudder")
		static FString GetFirmwareVersion(int32 portNumber);

	UFUNCTION(BlueprintPure, Category = "3dRudder")
		static E3dRudderStatus GetStatus(int32 portNumber);

	UFUNCTION(BlueprintPure, Category = "3dRudder")
		static FString GetStatusString(int32 portNumber);

	UFUNCTION(BlueprintCallable, Category = "3dRudder")
		static bool PlaySound(int32 portNumber, int32 frequency, int32 duration);

	UFUNCTION(BlueprintPure, Category = "3dRudder")
		static E3dRudderError GetAxes(int32 portNumber, UAxesParamAsset* axesParamAsset, float &LeftRight, float &ForwardBackward, float &UpDown, float &Rotation);

	UFUNCTION(BlueprintPure, Category = "3dRudder")
		static void ComputeSmooth(FSmoothMovement smooth, float DeltaTime, float LeftRight, float ForwardBackward, float UpDown, float Rotation, 
			float &LeftRightSmooth, float &ForwardBackwardSmooth, float &UpDownSmooth, float &RotationSmooth, FSmoothMovement &speed);

	UFUNCTION(BlueprintPure, Category = "3dRudder")
		static void GetSensor(int32 portNumber, int32 &sensor1, int32 &sensor2, int32 &sensor3, int32 &sensor4, int32 &sensor5, int32 &sensor6);
};
