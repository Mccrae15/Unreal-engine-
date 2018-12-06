// Copyright 3dRudder 2017, Inc. All Rights Reserved.

#include "3dRudderDevice.h"
#include "3dRudderPrivatePCH.h"
#include "3dRudderPluginSettings.h" 
#include "SlateBasics.h"
 
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "InputDevice.h"
 
const FKey EKeys3dRudder::LeftRight("Left_Right");
const FKey EKeys3dRudder::ForwardBackward("Forward_Backward");
const FKey EKeys3dRudder::UpDown("Up_Down");
const FKey EKeys3dRudder::Rotation("Rotation");

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
	
}

void F3dRudderDevice::Tick(float DeltaTime)
{
	// This will spam the log heavily, comment it out for real plugins :)
	// UE_LOG(Log3dRudderDevice, Log, TEXT("Tick %f"), DeltaTime);
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
				EmitAnalogInputEventForKey(EKeys3dRudder::LeftRight, axesValue.Get(ns3dRudder::LeftRight), i, 0);
				EmitAnalogInputEventForKey(EKeys3dRudder::ForwardBackward, axesValue.Get(ns3dRudder::ForwardBackward), i, 0);
				EmitAnalogInputEventForKey(EKeys3dRudder::UpDown, axesValue.Get(ns3dRudder::UpDown), i, 0);
				EmitAnalogInputEventForKey(EKeys3dRudder::Rotation, axesValue.Get(ns3dRudder::Rotation), i, 0);
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