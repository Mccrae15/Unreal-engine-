// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DS4MovementController.h"

#include "IInputDevice.h"
#include "XRMotionControllerBase.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "PS4Tracker.h"

#include "PS4/PS4Application.h"
#include "PS4/PS4Controllers.h"
#include <pad.h>

#include "Engine/Engine.h"

#define MAX_LOGIN_PLAYERS		SCE_USER_SERVICE_MAX_LOGIN_USERS

/** Dualshock 4 motion controller implementation */
class FDS4MovementController : public FXRMotionControllerBase, public IInputDevice
{
public:
	FDS4MovementController();
	virtual ~FDS4MovementController();

	/** IInputDevice implementation */
	virtual void Tick(float DeltaTime) override;
	virtual void SendControllerEvents() override {};
	virtual void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override {};
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override { return false; }
	virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override {};
	virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override {};

	/** IMotionController implementation */
	virtual FName GetMotionControllerDeviceTypeName() const override
	{
		const static FName DefaultName(TEXT("PS4DualShockController"));
		return DefaultName;
	}
	virtual bool GetControllerOrientationAndPosition(const int32 ControllerId, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override;
	virtual ETrackingStatus GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;

	/** Helpers for binding user id/index to controller state */
	void ConnectStateToUser(int32 UserID, int32 UserIndex);
	void DisconnectStateFromUser(int32 UserID, int32 UserIndex);

	void UserLoginEventHandler(bool bLoggingIn, int32 UserID, int32 UserIndex);

	bool ControllerIsReady(int32 Handle) const;
private:

	bool IsValid(const int32 ControllerIndex, const EControllerHand DeviceHand) const;

	/** State for user connections */
	struct FUserState
	{
		/** Whether or not this user slot has a connection */
		bool bIsConnected;

		/** The controller associated with this user */
		int32 ControllerId;

		/** User ID issued by the sce API */
		int32 UserID;

		/** Motion controller tracker handle */
		int32 Tracker;

		/** The tracking state of the Controller */
		mutable IPS4Tracker::ETrackingStatus TrackingStatus;

		/** Current world position */
		mutable FVector	Position;

		/** Current world orientation */
		mutable FQuat Orientation;

		/** scePad Device handle */
		int32 Pad;
	} UserStates[MAX_LOGIN_PLAYERS];

	mutable FPSTrackerHandle PS4Tracker;
	mutable bool bIsStereoEnabled;
};

//=============================================================================
FDS4MovementController::FDS4MovementController()
	: bIsStereoEnabled(false)
{	
	FMemory::Memzero(UserStates, sizeof(UserStates));
	for (auto& UserState : UserStates)
	{
		UserState.Tracker = IPS4Tracker::INVALID_TRACKER_HANDLE;
	}

	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
 
	FCoreDelegates::OnUserLoginChangedEvent.AddRaw(this, &FDS4MovementController::UserLoginEventHandler);
}

//=============================================================================
FDS4MovementController::~FDS4MovementController()
{
	// Cleanup controllers
	for (auto& UserState : UserStates)
	{
		if (!UserState.bIsConnected)
		{
			continue;
		}

		if (UserState.Tracker != IPS4Tracker::INVALID_TRACKER_HANDLE)
		{
			PS4Tracker->ReleaseTracker(UserState.Tracker);
		}
		FPS4Application::GetPS4Application()->DisconnectControllerStateFromUser(UserState.UserID, UserState.ControllerId);
	}
}

//=============================================================================
void FDS4MovementController::UserLoginEventHandler(bool bLoggingIn, int32 UserID, int32 UserIndex)
{
	check(UserID != SCE_USER_SERVICE_USER_ID_INVALID);
		
	if (bLoggingIn)		
	{				
		ConnectStateToUser(UserID, UserIndex);				
	}
	else
	{	
		DisconnectStateFromUser(UserID, UserIndex);			
	}
}

//=============================================================================
bool FDS4MovementController::ControllerIsReady(int32 Handle) const
{
	ScePadControllerInformation ControllerInfo;
	int32 Ret = scePadGetControllerInformation(Handle, &ControllerInfo);
	return (Ret == SCE_OK && ControllerInfo.connected);
}

//=============================================================================
void FDS4MovementController::Tick(float DeltaTime)
{
	for (auto& UserState : UserStates)
	{
		if (!UserState.bIsConnected)
		{
			continue;
		}

		if (UserState.Tracker == IPS4Tracker::INVALID_TRACKER_HANDLE)
		{
			if (ControllerIsReady(UserState.Pad))
			{
				UserState.Tracker = PS4Tracker->AcquireTracker(UserState.Pad, UserState.ControllerId, IPS4Tracker::EDeviceType::PAD);
				UserState.Position = FVector::ZeroVector;
				UserState.Orientation = FQuat::Identity;
			}
		}
	}
}

//=============================================================================
bool FDS4MovementController::GetControllerOrientationAndPosition(const int32 ControllerId, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
	check(ControllerId < ARRAY_COUNT(UserStates));

	// Is the specified controller connected?
	if (!IsValid(ControllerId, DeviceHand))
	{
		return false;
	}

 	if (IsInGameThread())
 	{
 		bIsStereoEnabled = GEngine->IsStereoscopic3D();
 	}
 
 	IPS4Tracker::FTrackingData TrackingData;
	const auto& UserState = UserStates[ControllerId];
 	PS4Tracker->GetTrackingData(UserState.Tracker, TrackingData);
	FVector Position = TrackingData.Position * WorldToMetersScale;
 	if (GEngine->XRSystem.IsValid() && bIsStereoEnabled)
 	{
 		GEngine->XRSystem->RebaseObjectOrientationAndPosition(Position, TrackingData.Orientation);
 	}
	
	UserState.TrackingStatus = TrackingData.Status;

 	if (TrackingData.Status == IPS4Tracker::ETrackingStatus::TRACKING)
 	{
 		UserState.Position = Position;
 	}
	UserState.Orientation = TrackingData.Orientation;

	// Output values
	OutPosition = UserState.Position;
	OutOrientation = FRotator(UserState.Orientation);

	return (TrackingData.Status == IPS4Tracker::ETrackingStatus::TRACKING) || (TrackingData.Status == IPS4Tracker::ETrackingStatus::NOT_TRACKING);
}

//=============================================================================
ETrackingStatus FDS4MovementController::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	check(ControllerIndex < ARRAY_COUNT(UserStates));
	const auto& UserState = UserStates[ControllerIndex];
	if (UserState.ControllerId != ControllerIndex || !UserState.bIsConnected)
	{
		return ETrackingStatus::NotTracked;
	}

	if (UserState.TrackingStatus == IPS4Tracker::ETrackingStatus::TRACKING)
	{
		// Device is within the camera's tracking field
		return ETrackingStatus::Tracked;
	}
	else if (UserState.TrackingStatus == IPS4Tracker::ETrackingStatus::NOT_TRACKING)
	{
		// Device is within the camera's tracking field
		return ETrackingStatus::InertialOnly;
	}
	else
	{
		return ETrackingStatus::NotTracked;
	}
}

//=============================================================================
bool FDS4MovementController::IsValid(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
	if (ControllerIndex < 0 || ControllerIndex >= ARRAY_COUNT(UserStates))
	{
		return false;
	}

	// Is the specified controller connected?
	const auto& UserState = UserStates[ControllerIndex];
	if (UserState.ControllerId != ControllerIndex || UserState.Tracker == IPS4Tracker::INVALID_TRACKER_HANDLE)
	{
		return false;
	}

	if (DeviceHand != EControllerHand::Pad)
	{
		return false;
	}

	if (!ControllerIsReady(UserState.Pad))
	{
		return false;
	}

	return true;
}

//=============================================================================
void FDS4MovementController::ConnectStateToUser(int32 UserID, int32 UserIndex)
{
	check(UserIndex < ARRAY_COUNT(UserStates));

	// make sure this user isn't already logged in on some state.
	for (int i = 0; i < ARRAY_COUNT(UserStates); ++i)
	{
		check(UserStates[i].UserID != UserID);
	}
	
	FUserState& UserState = UserStates[UserIndex];

	check(UserState.UserID == 0);	

	UserState.bIsConnected = true;
	UserState.ControllerId = UserIndex;
	UserState.UserID = UserID;
	UserState.Tracker = IPS4Tracker::INVALID_TRACKER_HANDLE;
	UserState.Pad = FPS4Application::GetPS4Application()->ConnectControllerStateToUser(UserID, UserIndex);
}

//=============================================================================
void FDS4MovementController::DisconnectStateFromUser(int32 UserID, int32 UserIndex)
{
	check(UserIndex < ARRAY_COUNT(UserStates));

	FUserState& UserState = UserStates[UserIndex];
	check(UserState.ControllerId == UserIndex);
	check(UserState.UserID == UserID);		

	if (UserState.Tracker != IPS4Tracker::INVALID_TRACKER_HANDLE)
	{
		PS4Tracker->ReleaseTracker(UserState.Tracker);
	}
	
	FPS4Application::GetPS4Application()->DisconnectControllerStateFromUser(UserID, UserIndex);
	UserState.Pad = 0;

	FMemory::Memzero(&UserState, sizeof(FUserState));
	UserState.Tracker = IPS4Tracker::INVALID_TRACKER_HANDLE;
}

//** Dualshock 4 controller plugin implementation */
class FDS4MovementControllerPlugin : public IDS4MovementController
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override
	{
		return TSharedPtr<class IInputDevice>(new FDS4MovementController());
	}
};

//=============================================================================
void FDS4MovementControllerPlugin::StartupModule()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("DS4 MOTION CONTROLLER STARTUP"));
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
}

//=============================================================================
void FDS4MovementControllerPlugin::ShutdownModule()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("DS4 MOTION CONTROLLER SHUTDOWN"));
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

IMPLEMENT_MODULE(FDS4MovementControllerPlugin, DS4MovementController)