// Copyright Epic Games, Inc. All Rights Reserved.
//
#include "SteamVRFunctionLibrary.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"
#include "XRMotionControllerBase.h"

USteamVRFunctionLibrary::USteamVRFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

#if STEAMVR_SUPPORTED_PLATFORMS
FSteamVRHMD* GetSteamVRHMD()
{
	if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == FSteamVRHMD::SteamSystemName))
	{
		return static_cast<FSteamVRHMD*>(GEngine->XRSystem.Get());
	}

	return nullptr;
}

IMotionController* GetSteamMotionController()
{
	static FName DeviceTypeName(TEXT("SteamVRController"));
	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
	for (IMotionController* MotionController : MotionControllers)
	{
		if (MotionController->GetMotionControllerDeviceTypeName() == DeviceTypeName)
		{
			return MotionController;
		}
	}
	return nullptr;
}
#endif // STEAMVR_SUPPORTED_PLATFORMS

void USteamVRFunctionLibrary::GetValidTrackedDeviceIds(ESteamVRTrackedDeviceType DeviceType, TArray<int32>& OutTrackedDeviceIds)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	OutTrackedDeviceIds.Empty();

	EXRTrackedDeviceType XRDeviceType = EXRTrackedDeviceType::Invalid;
	switch (DeviceType)
	{
	case ESteamVRTrackedDeviceType::Controller:
		XRDeviceType = EXRTrackedDeviceType::Controller;
		break;
	case ESteamVRTrackedDeviceType::TrackingReference:
		XRDeviceType = EXRTrackedDeviceType::TrackingReference;
		break;
	case ESteamVRTrackedDeviceType::Other:
		XRDeviceType = EXRTrackedDeviceType::Other;
		break;
	case ESteamVRTrackedDeviceType::Invalid:
		XRDeviceType = EXRTrackedDeviceType::Invalid;
		break;
	default:
		break;
	}


	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		SteamVRHMD->EnumerateTrackedDevices(OutTrackedDeviceIds, XRDeviceType);
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS
}

bool USteamVRFunctionLibrary::GetTrackedDevicePositionAndOrientation(int32 DeviceId, FVector& OutPosition, FRotator& OutOrientation)
{
	bool RetVal = false;

#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		FQuat DeviceOrientation = FQuat::Identity;
		RetVal = SteamVRHMD->GetCurrentPose(DeviceId, DeviceOrientation, OutPosition);
		OutOrientation = DeviceOrientation.Rotator();
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return RetVal;
}

bool USteamVRFunctionLibrary::GetHandPositionAndOrientation(int32 ControllerIndex, EControllerHand Hand, FVector& OutPosition, FRotator& OutOrientation)
{
	bool RetVal = false;

#if STEAMVR_SUPPORTED_PLATFORMS
	IMotionController* SteamMotionController = GetSteamMotionController();
	if (SteamMotionController)
	{
		// Note: the steam motion controller ignores the WorldToMeters scale argument.
		RetVal = static_cast<FXRMotionControllerBase*>(SteamMotionController)->GetControllerOrientationAndPosition(ControllerIndex, Hand, OutOrientation, OutPosition, -1.0f);
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return RetVal;
}

FString USteamVRFunctionLibrary::GetControllerName(int32 DeviceIndex)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		return SteamVRHMD->GetControllerName(DeviceIndex);
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return FString();
}
FString USteamVRFunctionLibrary::GetHMDModel()
{
#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		return SteamVRHMD->GetHMDModel();
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return FString();
}

float USteamVRFunctionLibrary::GetHMD_Frequency()
{
#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		return SteamVRHMD->GetHMD_Frequency();
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return -1.0f;
}

FVector USteamVRFunctionLibrary::GetBasePosition()
{
#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		return SteamVRHMD->GetBasePosition();
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return FVector::ZeroVector;
}

FRotator USteamVRFunctionLibrary::GetBaseRotation()
{
#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		return SteamVRHMD->GetBaseRotation();
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return FRotator::ZeroRotator;
}

void USteamVRFunctionLibrary::SetBaseRotation(FRotator NewRotation)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		return SteamVRHMD->SetBaseRotation(NewRotation);
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

}

void USteamVRFunctionLibrary::SetBasePosition(FVector NewPosition)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		return SteamVRHMD->SetBasePosition(NewPosition);
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS
}
