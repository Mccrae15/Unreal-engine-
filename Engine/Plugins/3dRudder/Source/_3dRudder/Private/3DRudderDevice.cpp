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

#include "3dRudderDevice.h"
#include "3dRudderPrivatePCH.h"
#include "3dRudderPluginSettings.h" 
#include "SlateBasics.h"
#include "Runtime/Launch/Resources/Version.h"

#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "InputDevice.h"

#if PLATFORM_PS4
#include "PS4/PS4Misc.h"
#endif

const FKey EKeys3dRudder::LeftRight("Left_Right");
const FKey EKeys3dRudder::ForwardBackward("Forward_Backward");
const FKey EKeys3dRudder::UpDown("Up_Down");
const FKey EKeys3dRudder::Rotation("Rotation");

const FKey EKeys3dRudder::Left("3dRLeft");
const FKey EKeys3dRudder::Right("3dRRight");
const FKey EKeys3dRudder::Forward("3dRForward");
const FKey EKeys3dRudder::Backward("3dRBackward");
const FKey EKeys3dRudder::Up("3dRUp");
const FKey EKeys3dRudder::Down("3dRDown"); 
const FKey EKeys3dRudder::RotationLeft("3dRRotationLeft");
const FKey EKeys3dRudder::RotationRight("3dRRotationRight");


DEFINE_LOG_CATEGORY_STATIC(Log3dRudderDevice, Log, All);
#define LOCTEXT_NAMESPACE "3dRudderDevice"

ns3dRudder::CSdk* F3dRudderDevice::s_pSdk;
ns3dRudder::AxesParamDefault F3dRudderDevice::s_AxesParamDefault;
Event3dRudder* F3dRudderDevice::s_Events = new Event3dRudder();

//UE v4.6 IM event wrappers
bool EmitKeyUpEventForKey(FKey key, int32 user, bool repeat)
{
	FKeyEvent KeyEvent(key, FSlateApplication::Get().GetModifierKeys(), user, repeat, 0, 0);
	return FSlateApplication::Get().ProcessKeyUpEvent(KeyEvent);
}

bool EmitKeyDownEventForKey(FKey key, int32 user, bool repeat)
{
	FKeyEvent KeyEvent(key, FSlateApplication::Get().GetModifierKeys(), user, repeat, 0, 0);
	return FSlateApplication::Get().ProcessKeyDownEvent(KeyEvent);
}

bool EmitAnalogInputEventForKey(FKey key, float value, int32 user, bool repeat)
{
	FAnalogInputEvent AnalogInputEvent(key, FSlateApplication::Get().GetModifierKeys(), user, repeat, 0, 0, value);
	return FSlateApplication::Get().ProcessAnalogInputEvent(AnalogInputEvent);
}

F3dRudderDevice::F3dRudderDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	: m_MessageHandler(InMessageHandler)
{
	// 3dRudder SDK
	ns3dRudder::ErrorCode error = ns3dRudder::LoadSDK(_3DRUDDER_SDK_LAST_COMPATIBLE_VERSION);
	if (error == ns3dRudder::Success && s_pSdk == nullptr)
	{				
		s_pSdk = ns3dRudder::GetSDK();
		s_pSdk->SetEvent(s_Events);		
		s_pSdk->Init();
		UE_LOG(Log3dRudderDevice, Log, TEXT("3dRudder version %x"), s_pSdk->GetSDKVersion());
		if (s_pSdk->GetSDKVersion() != _3DRUDDER_SDK_VERSION)
		{			
			UE_LOG(Log3dRudderDevice, Log, TEXT("3dRudder SDK no up to date %x"), s_pSdk->GetSDKVersion());			
		}
	}
	else
	{	
		UE_LOG(Log3dRudderDevice, Log, TEXT("SDK error %s"), ANSI_TO_TCHAR(ns3dRudder::GetErrorText(error)));
	}
	
	// Register the FKeys (Gamepad key for controllers)
	EKeys::AddMenuCategoryDisplayInfo("3dRudder", LOCTEXT("3dRudderSubCateogry", "3dRudder"), TEXT("GraphEditor.PadEvent_16x"));

	EKeys::AddKey(FKeyDetails(EKeys3dRudder::LeftRight, LOCTEXT("LeftRight", "3dRudder Left Right"), FKeyDetails::FloatAxis, FName(TEXT("3dRudder"))));
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::ForwardBackward, LOCTEXT("ForwardBackward", "3dRudder Forward Backward"), FKeyDetails::FloatAxis, FName(TEXT("3dRudder"))));
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::UpDown, LOCTEXT("UpDown", "3dRudder Up Down"), FKeyDetails::FloatAxis, FName(TEXT("3dRudder"))));
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::Rotation, LOCTEXT("Rotation", "3dRudder Rotation"), FKeyDetails::FloatAxis, FName(TEXT("3dRudder"))));

#if ENGINE_MAJOR_VERSION>=4 && ENGINE_MINOR_VERSION	> 20
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::Left, LOCTEXT("3dRLeft", "3dRudder Left Action"), FKeyDetails::Touch, FName(TEXT("3dRudder"))));
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::Right, LOCTEXT("3dRRight", "3dRudder Right Action"), FKeyDetails::Touch, FName(TEXT("3dRudder"))));
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::Forward, LOCTEXT("3dRForward", "3dRudder Forward Action"), FKeyDetails::Touch, FName(TEXT("3dRudder"))));
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::Backward, LOCTEXT("3dRBackward", "3dRudder Backward Action"), FKeyDetails::Touch, FName(TEXT("3dRudder"))));
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::Up, LOCTEXT("3dRUp", "3dRudder Up Action"), FKeyDetails::Touch, FName(TEXT("3dRudder"))));
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::Down, LOCTEXT("3dRDown", "3dRudder Down Action"), FKeyDetails::Touch, FName(TEXT("3dRudder"))));
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::RotationLeft, LOCTEXT("3dRRotationLeft", "3dRudder Rotation Left Action"), FKeyDetails::Touch, FName(TEXT("3dRudder"))));
	EKeys::AddKey(FKeyDetails(EKeys3dRudder::RotationRight, LOCTEXT("3dRRotationRight", "3dRudder Rotation Right Action"), FKeyDetails::Touch, FName(TEXT("3dRudder"))));
#endif

	for (int i = 0; i < _3DRUDDER_SDK_MAX_DEVICE; i++) {
		previousState[i].leftIsActive = false;
		previousState[i].rightIsActive = false;
		previousState[i].forwardIsActive = false;
		previousState[i].backwardIsActive = false;
		previousState[i].upIsActive = false;
		previousState[i].downIsActive = false;
		previousState[i].rotationLeftIsActive = false;
		previousState[i].rotationRightIsActive = false;
	}
}

void F3dRudderDevice::Tick(float DeltaTime)
{
	// This will spam the log heavily, comment it out for real plugins :)
	// UE_LOG(Log3dRudderDevice, Log, TEXT("Tick %f"), DeltaTime);
#if PLATFORM_PS4 && DIALOG_3DRUDDER
	m_dialog.Update();
	if (m_wantClose)
	{
		m_timer += DeltaTime;
		if (m_timer > 1.0f) // seconds?
		{
			m_timer = 0;
			m_dialog.CloseDialog();
		}
	}
#endif

	if (s_pSdk)
	{
		ns3dRudder::Status status = s_pSdk->GetStatus(0);
		if (m_status != status)
		{
			m_status = status;			
#if PLATFORM_PS4 && DIALOG_3DRUDDER
			m_wantClose = false;
			m_timer = 0;
			switch (status)
			{
			case ns3dRudder::NoFootStayStill:
				m_dialog.StartDialogProgressBar(TEXT("3dRudder Connected: don't put your feet"));
                m_dialog.SetProgressBarPercent(33);
				m_init = true;
				break;
			case ns3dRudder::Initialization:
				m_dialog.SetProgressBarPercent(66);
				break;
			case ns3dRudder::PutYourFeet:
				if (m_init)
				{
					m_dialog.SetProgressBarMessage(TEXT("You can put your feet, now"));
					m_dialog.SetProgressBarPercent(100);
					m_init = false;
				}
				m_wantClose = true;
				break;
			case ns3dRudder::PutSecondFoot:
				m_dialog.StartDialogProgressBar(TEXT("Put your second foot"));
                m_dialog.SetProgressBarPercent(33);
				break;
			case ns3dRudder::StayStill:
				m_dialog.SetProgressBarMessage(TEXT("Don't move"));
                m_dialog.SetProgressBarPercent(66);
				break;
			case ns3dRudder::InUse:
				m_dialog.SetProgressBarMessage(TEXT("Nice, you can play"));
				m_dialog.SetProgressBarPercent(100);
				//FPS4Misc::MessageBoxExt(EAppMsgType::Ok, TEXT("In Use"), TEXT("In Use"));
				//m_dialog.StartDialogUserMessage(TEXT("In Use"));				
				m_wantClose = true;
				break;
			case ns3dRudder::Frozen:
				//OpenDialog(TEXT("Frozen"));
				break;
			case ns3dRudder::IsNotConnected:
				m_dialog.StartDialogUserMessage(TEXT("3dRudder Disconnected"));
				m_wantClose = true;
				break;
			}
#endif
		}
	}
}


void Update3dRActionInputData(FKey key, bool isPressed, bool previousState, int user)
{
	if (isPressed && !previousState) {
		EmitKeyDownEventForKey(key, user, 0);
	}
	else if (!isPressed && previousState) {
		EmitKeyUpEventForKey(key, user, 0);
	}
	previousState = isPressed;
}

void F3dRudderDevice::SendControllerEvents() 
{
	if (s_pSdk == nullptr)
		return;

	// For each device (4)
	for (unsigned int i = 0;i < _3DRUDDER_SDK_MAX_DEVICE;i++)
	{
		if (s_pSdk->IsDeviceConnected(i))
		{
			// Axis : X, Y, Z, rZ
			ns3dRudder::AxesValue axesValue;
			ns3dRudder::IAxesParam* pAxesParam = GetDefault<U3dRudderPluginSettings>()->pAxesParam;
			if (pAxesParam == nullptr)
				pAxesParam = &s_AxesParamDefault;

			if (s_pSdk->GetAxes(i, pAxesParam, &axesValue) == ns3dRudder::Success)
			{
				float leftRightValue = axesValue.Get(ns3dRudder::LeftRight);
				float forwardBackwardValue = axesValue.Get(ns3dRudder::ForwardBackward);
				float upDownValue = axesValue.Get(ns3dRudder::UpDown);
				float rotationValue = axesValue.Get(ns3dRudder::Rotation);

				Last3dRudderState actualState;

				actualState.leftIsActive		  = leftRightValue < -GetDefault<U3dRudderPluginSettings>()->LeftThreshold;
				actualState.rightIsActive		  = leftRightValue > GetDefault<U3dRudderPluginSettings>()->RightThreshold;
				actualState.forwardIsActive		  = forwardBackwardValue > GetDefault<U3dRudderPluginSettings>()->ForwardThreshold;
				actualState.backwardIsActive	  = forwardBackwardValue < -GetDefault<U3dRudderPluginSettings>()->BackwardThreshold;
				actualState.upIsActive			  = upDownValue > GetDefault<U3dRudderPluginSettings>()->UpThreshold;
				actualState.downIsActive		  = upDownValue < -GetDefault<U3dRudderPluginSettings>()->DownThreshold;
				actualState.rotationLeftIsActive  = rotationValue < -GetDefault<U3dRudderPluginSettings>()->RotationLeftThreshold;
				actualState.rotationRightIsActive = rotationValue > GetDefault<U3dRudderPluginSettings>()->RotationRightTreshold;

				/** 3dRudder axes */
				EmitAnalogInputEventForKey(EKeys3dRudder::LeftRight, leftRightValue, i, 0);
				EmitAnalogInputEventForKey(EKeys3dRudder::ForwardBackward, forwardBackwardValue, i, 0);
				EmitAnalogInputEventForKey(EKeys3dRudder::UpDown, upDownValue, i, 0);
				EmitAnalogInputEventForKey(EKeys3dRudder::Rotation, rotationValue, i, 0);

				/** 3dRudder Action */
#if ENGINE_MAJOR_VERSION>=4 && ENGINE_MINOR_VERSION	> 20
				Update3dRActionInputData(EKeys3dRudder::Left, actualState.leftIsActive, previousState[i].leftIsActive, i);
				Update3dRActionInputData(EKeys3dRudder::Right, actualState.rightIsActive, previousState[i].rightIsActive, i);
				Update3dRActionInputData(EKeys3dRudder::Forward, actualState.forwardIsActive, previousState[i].forwardIsActive, i);
				Update3dRActionInputData(EKeys3dRudder::Backward, actualState.backwardIsActive, previousState[i].backwardIsActive, i);
				Update3dRActionInputData(EKeys3dRudder::Up, actualState.upIsActive, previousState[i].upIsActive, i);
				Update3dRActionInputData(EKeys3dRudder::Down, actualState.downIsActive, previousState[i].downIsActive, i);
				Update3dRActionInputData(EKeys3dRudder::RotationLeft, actualState.rotationLeftIsActive, previousState[i].rotationLeftIsActive, i);
				Update3dRActionInputData(EKeys3dRudder::RotationRight, actualState.rotationRightIsActive, previousState[i].rotationRightIsActive, i);
#endif
				previousState[i] = actualState;
			}							   
		}
	}
}

void F3dRudderDevice::SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) 
{
	//UE_LOG(Log3dRudderDevice, Log, TEXT("Set Message Handler"));
	m_MessageHandler = InMessageHandler;
}
 
bool F3dRudderDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) 
{
	//UE_LOG(Log3dRudderDevice, Log, TEXT("Execute Console Command: %s"), Cmd);
 
	// Put your fancy custom console command code here... 
	// ToDo: use this to let you fire pseudo controller events
 
	return true;
}

// IForceFeedbackSystem pass through functions
void F3dRudderDevice::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) 
{
	//UE_LOG(Log3dRudderDevice, Log, TEXT("Set Force Feedback %f"), Value);
}

void F3dRudderDevice::SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) 
{
	// This will spam the log heavily, comment it out for real plugins :)
	//UE_LOG(Log3dRudderDevice, Log, TEXT("Set Force Feedback Values"));
} 

// This is where you nicely clean up your plugin when its told to shut down!
F3dRudderDevice::~F3dRudderDevice() 
{
	if (s_pSdk != nullptr)
		s_pSdk->Stop();

	if (s_Events != nullptr)
	{
		delete s_Events;
		s_Events = nullptr;
	}

	UE_LOG(Log3dRudderDevice, Log, TEXT("Closing 3dRudderDevice"));
}

#undef LOCTEXT_NAMESPACE