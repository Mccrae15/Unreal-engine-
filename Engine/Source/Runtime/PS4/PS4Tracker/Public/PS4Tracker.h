// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

/**
 * Interface for platform feature modules
 */

/** Defines the interface to platform's positional tracking system */
class IPS4Tracker
{
public:
	enum class EDeviceType
	{
		HMD					= 0,	// Head mounted display
		MOTION_LEFT_HAND	= 1,	// Motion controller for left hand
		PAD					= 2,	// Game pad
		MOTION_RIGHT_HAND	= 3,	// Motion controller for right hand
		GUN					= 4		// PSVR Aim controller (gun like)
	};

	enum class ETrackingStatus
	{
		NOT_STARTED,	// Tracking still needs initialization
		TRACKING,		// Device is within the camera's tracking field
		NOT_TRACKING,	// Device is outside the camera's tracking field
		CALIBRATING		// Calibrating neutral pose
	};

	struct FTrackingData
	{
		ETrackingStatus	Status;
		FVector					DevicePosition;		// Position in device space.
		FQuat					DeviceOrientation;	// Orientation in device space
		FVector					EyePosition[2];		// Position in device space of the eyes (HMD only)
		FQuat					EyeOrientation[2];	// Orientation in device space of the eyes (HMD only)
		FVector					Position;			// Position in Unreal space.
		FQuat					Orientation;		// Orientation in Unreal space.
		FVector					HeadPosition;		// Position of the head (center of the ears) (HMD only)
		FQuat					HeadOrientation;	// Orientation of the head (center of the ears) (HMD only)
		FVector					UnrealEyePosition[2];		// Position of the eyes in Unreal space (HMD only)
		FQuat					UnrealEyeOrientation[2];	// Orientation of the eyes in Unreal space (HMD only)
		FVector					UnrealHeadPosition;			// Position of the head (center of ears) in Unreal space (HMD only)
		FQuat					UnrealHeadOrientation;		// Orientation of the head (center of the ears) in Unreal space (HMD only)
		uint64					TimeStamp;			// The time at which the data was sampled
		uint64					SensorReadSystemTimestamp;
		uint32					FrameNumber;
		FQuat					CameraOrientation;	//Carbon edit

		static void DefaultInitialize(FTrackingData& TrackingData)
		{
			TrackingData =
			{
				ETrackingStatus::NOT_STARTED,					// Status
				FVector::ZeroVector,							// DevicePosition
				FQuat::Identity,								// DeviceOrientation
				{ FVector::ZeroVector, FVector::ZeroVector },	// EyePosition
				{ FQuat::Identity, FQuat::Identity },			// EyeOrientation
				FVector::ZeroVector,							// Position
				FQuat::Identity,								// Orientation
				FVector::ZeroVector,							// HeadPosition
				FQuat::Identity,								// HeadOrientation
				{ FVector::ZeroVector, FVector::ZeroVector },	// UnrealEyePosition
				{ FQuat::Identity, FQuat::Identity },			// UnrealEyeOrientation
				FVector::ZeroVector,							// UnrealHeadPosition
				FQuat::Identity,								// UnrealHeadOrientation
				0,												// TimeStamp
				0,												// Sensor read timestamp
				0												// FrameNumber
			};
		}
	};

	static const int32 INVALID_TRACKER_HANDLE = -1;

	/** Returns a handle to the tracker or INVALID_TRACKER_HANDLE if unsuccessful  */
	virtual int32 AcquireTracker(int32 DeviceHandle, int32 ControllerIndex, EDeviceType DeviceType) = 0;

	/** Release the tracker handle. */
	virtual void ReleaseTracker(int32 TrackerHandle) = 0;

	/** Blocking call that waits until polling data is fresh */
	virtual void Synchronize(int32 TrackerHandle) = 0;
	
	/** Return the current tracker's tracking data */
	virtual void GetTrackingData(int32 TrackerHandle, FTrackingData& TrackingData, bool bPollImmediately = false, bool bEarlyPoll = false) = 0;

	/** Set the frame timing used to predict what the hmd orientation will be when the frame is displayed.  In microseconds. */
	virtual void SetPredictionTiming(int32 FlipToDisplayLatency, int32 RenderFrameTime, bool bIs60Render120ScanoutMode) = 0;

	virtual bool IsCameraConnected() const = 0;
};

/** Defines the interface to the module */
class IPS4TrackerModule : public IModuleInterface
{
private:
	/** Returns the PS4 tracker singleton, loading the module on demand if needed */
	static inline TSharedPtr< IPS4Tracker, ESPMode::ThreadSafe > GetTracker()
	{
		return FModuleManager::LoadModuleChecked<IPS4TrackerModule>( "PS4Tracker" ).GetTrackerInstance();
	}

	/** Returns the tracker instance */
	virtual TSharedPtr< IPS4Tracker, ESPMode::ThreadSafe > GetTrackerInstance() = 0;

	friend class FPSTrackerHandle;
};

/** All access to the IPS4Tracker is done through a handle */
class FPSTrackerHandle
{
public:
	TSharedPtr<IPS4Tracker, ESPMode::ThreadSafe> operator->()
	{
		if (!PS4Tracker.IsValid())
		{
			PS4Tracker = IPS4TrackerModule::GetTracker();
		}
		return PS4Tracker;
	}
private:
	TSharedPtr< IPS4Tracker, ESPMode::ThreadSafe > PS4Tracker;
};