// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4Tracker.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "SceneViewExtension.h"
#include "StereoRendering.h"

#define MAX_REGISTERED_DEVICES 8

namespace
{
	FORCEINLINE FQuat ConvertToUnrealCoords(const FQuat& InQuat)
	{
		return FQuat(-InQuat.Z, InQuat.X, InQuat.Y, -InQuat.W);
	}

	FORCEINLINE FVector ConvertToUnrealCoords(const FVector& InVector)
	{
		return FVector(-InVector.Z, InVector.X, InVector.Y);
	}
} // Anonymous namespace

DECLARE_LOG_CATEGORY_CLASS(LogPS4Tracker, Log, All);

#if PLATFORM_PS4
/******************************************************************************
*
*	PS4
*
******************************************************************************/

/** includes */
#include "GnmBridge.h"
#include "GnmMemory.h"
#include <libsysmodule.h>
#include <camera.h>
#include <vision/vr_tracker.h>
#include <video_out.h>
#include <common_dialog.h>
#include <message_dialog.h>
#include "IMotionTrackingSystemManagement.h"
#include "Features/IModularFeatures.h"
#include <hmd.h>

/** memory pools */
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic TrackingRam"), STAT_Garlic_TrackingRam, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion TrackingRam"), STAT_Onion_TrackingRam, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DEFINE_STAT(STAT_Garlic_TrackingRam);
DEFINE_STAT(STAT_Onion_TrackingRam);

class FPS4Tracker : public IPS4Tracker, public IMotionTrackingSystemManagement, public FSceneViewExtensionBase
{
public:
	FPS4Tracker(const FAutoRegister&);
	virtual ~FPS4Tracker();

	/** IPS4Tracker implementation */
	virtual int32 AcquireTracker(int32 DeviceHandle, int32 ControllerIndex, EDeviceType DeviceType) override;
	virtual void ReleaseTracker(int32 TrackerHandle) override;
	virtual void Synchronize(int32 TrackerHandle) override;
	virtual void GetTrackingData(int32 TrackerHandle, FTrackingData& TrackingData, bool bPollImmediately, bool bEarlyPoll) override;
	virtual void SetPredictionTiming(int32 FlipToDisplayLatency, int32 RenderFrameTime, bool bIs60Render120ScanoutMode) override;
	virtual bool IsCameraConnected() const override;

	/* IMotionTrackingSystemManagement */
	virtual void SetIsControllerMotionTrackingEnabledByDefault(bool bEnable) override;
	virtual int32 GetMaximumMotionTrackedControllerCount() const override { return MAX_TRACKED_CONTROLLERS; }
	virtual int32 GetMotionTrackingEnabledControllerCount() const override;
	virtual bool IsMotionTrackingEnabledForDevice(const int32 ControllerIndex, const EControllerHand DeviceHand) const override;
	virtual bool EnableMotionTrackingOfDevice(const int32 ControllerIndex, const EControllerHand DeviceHand) override;
	virtual void DisableMotionTrackingOfDevice(const int32 ControllerIndex, const EControllerHand DeviceHand) override;
	virtual void DisableMotionTrackingOfAllControllers() override;
	virtual void DisableMotionTrackingOfControllersForPlayer(int32 PlayerIndex) override;
	
	/** ISceneViewExtension interface */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual int32 GetPriority() const { return 10; }

private:
	struct FMotionTrackingEnableBlackboard
	{
	public:
		bool GetIsEnabled(int32 ControllerIndex, EDeviceType DeviceType) const;
		void SetIsEnabled(int32 ControllerIndex, EDeviceType DeviceType, bool bEnabled);
		void SetDisabledForPlayer(int32 ControllerIndex);
		void SetDisabledAll();
		void SetEnabledByDefault(bool bEnabled);
	private:
		bool bTrackingEnabledByDefault = true;
		struct FPlayerMotionTrackingEnableBlackboard
		{
			bool bTrackingDisabledForThisPlayer = false;
			TMap<EDeviceType, bool> DeviceToEnabledMap;
		};
		TMap<int32, FPlayerMotionTrackingEnableBlackboard> PlayerToEnableBlackboardMap;

	} MotionTrackingEnableBlackboard;

	struct FDeviceState
	{
		int32					RefCount;
		bool					bIsValid;
		bool					bIsSetToTrack;
		bool					bDeviceRegistrationProblem;
		int32					DeviceHandle;
		int32					ControllerIndex;
		EDeviceType				DeviceType;
		SceVrTrackerDeviceType	SceDeviceType;
		FTrackingData			TrackingData;
	} DeviceStates[MAX_REGISTERED_DEVICES];

	void Initialize();
	void ConfigureCamera();
	void Destroy();

	bool EnableTrackerInternal(FDeviceState& DeviceState);
	void DisableTrackerInternal(FDeviceState& DeviceState);
	int32 CreateTracker(int32 DeviceHandle, int32 ControllerIndex, EDeviceType DeviveType);
	int32 GetTrackerHandle(int32 ControllerIndex, EDeviceType DeviveType) const;
	void DestroyTracker(int32 TrackerHandle);

	bool ConsistencyCheck(int32 ControllerIndex, EControllerHand DeviceHand) const;
	bool ConsistencyCheck(int32 ControllerIndex, EDeviceType DeviceType) const;
		
	void CameraConnectionTick();
	bool StartCameraSetupDialog();
	bool UpdateCameraSetupDialog();
	void EndCameraSetupDialog();

	void Recalibrate();
	void UpdateMotionSensorData();
	void SubmitComputeWithCamera();
	bool IsCameraControlManual() const;
	int32 GetTrackedDeviceCount() const;

	void OnWorldTickStart(ELevelTick TickType, float DeltaSeconds);
	void CameraProcess();
	void MotionSensorProcess();

	void PollTrackingData(FDeviceState& DeviceState, FTrackingData& TrackingData, uint64 PredictionTimeOffset, bool bEarlyPoll);

	struct FPredictionInfo
	{
		uint64 CurrentTime;
		uint64 TimeSinceLastFlip;
		uint64 TimeUntilNextFlip;
		uint64 FlipToDisplayLatency;
		uint64 FrameTimeMicroseconds;
		uint64 NumFramesSinceLastFlip;
	};
	uint64 CalculatePredictionTime(bool bEarlyPoll) const;

	int32 ModuleHandle = -1;
	int32 CameraHandle = -1;
	int32 FlipToDisplayLatency = 60;
	int32 RenderFrameTime = 16666;
	bool bIs60Render120ScanoutMode = false;
	SceCameraFrameData CameraFrameData;

	FDelegateHandle OnWorldTickStartDelegateHandle;

	const int32 MAX_TRACKED_CONTROLLERS = 2;

	TMap<SceVrTrackerDeviceType, uint32> SceDeviceTypeRegistrationCount;
	TMap<SceVrTrackerDeviceType, uint32> SceDeviceTypeTrackingCount;

	int CameraConnectionState = -1;

	bool bCameraSetupDialogActive = false;

	struct ESubmitStatus
	{
		enum Type
		{
			DO,
			STOP
		};
	};

	ScePthread		CameraThread;
	ESubmitStatus::Type SubmitStatus;
	FCriticalSection SubmitCriticalSection;

	SceVrTrackerInitParam InitParam;
	SceVrTrackerGpuSubmitParam GpuSubmitParam;

	FMemBlock OnionTrackingRam;
	FMemBlock GarlicTrackingRam;

	mutable FCriticalSection CritSect;
};

namespace
{

	FPS4Tracker::EDeviceType ControllerHandToDeviceType(EControllerHand ControllerHand)
	{
		switch (ControllerHand)
		{
		case EControllerHand::Left:
			return FPS4Tracker::EDeviceType::MOTION_LEFT_HAND;
			break;
		case EControllerHand::Pad:
			return FPS4Tracker::EDeviceType::PAD;
			break;
		case EControllerHand::Right:
			return FPS4Tracker::EDeviceType::MOTION_RIGHT_HAND;
			break;
		case EControllerHand::Gun:
			return FPS4Tracker::EDeviceType::GUN;
			break;
		default:
			check(false);
			return FPS4Tracker::EDeviceType::HMD;
		}
	}

	SceVrTrackerDeviceType DeviceTypeToSceDeviceType(FPS4Tracker::EDeviceType DeviceType)
	{
		switch (DeviceType)
		{
		case FPS4Tracker::EDeviceType::HMD:
			return SCE_VR_TRACKER_DEVICE_HMD;
			break;
		case FPS4Tracker::EDeviceType::MOTION_LEFT_HAND:
			return SCE_VR_TRACKER_DEVICE_MOVE;
			break;
		case FPS4Tracker::EDeviceType::PAD:
			return SCE_VR_TRACKER_DEVICE_DUALSHOCK4;
			break;
		case FPS4Tracker::EDeviceType::MOTION_RIGHT_HAND:
			return SCE_VR_TRACKER_DEVICE_MOVE;
			break;
		case FPS4Tracker::EDeviceType::GUN:
			return SCE_VR_TRACKER_DEVICE_GUN;
			break;
		default:
			check(false);
			return SCE_VR_TRACKER_DEVICE_HMD;
		}
	}
} // Anonymous namespace

bool FPS4Tracker::FMotionTrackingEnableBlackboard::GetIsEnabled(int32 ControllerIndex, EDeviceType DeviceType) const
{
	if (DeviceType == IPS4Tracker::EDeviceType::HMD)
	{
		// HMD is always enabled for tracking.
		return true;
	}

	const FPlayerMotionTrackingEnableBlackboard* PlayerEntry = PlayerToEnableBlackboardMap.Find(ControllerIndex);
	if (PlayerEntry)
	{
		const bool* DeviceEnabled = PlayerEntry->DeviceToEnabledMap.Find(DeviceType);
		if (DeviceEnabled)
		{
			return *DeviceEnabled;
		}
		else if (PlayerEntry->bTrackingDisabledForThisPlayer)
		{
			return false;
		}
		else
		{
			return bTrackingEnabledByDefault;
		}
	}
	else
	{
		return bTrackingEnabledByDefault;
	}
}

void FPS4Tracker::FMotionTrackingEnableBlackboard::SetIsEnabled(int32 ControllerIndex, EDeviceType DeviceType, bool bEnabled)
{
	FPlayerMotionTrackingEnableBlackboard* PlayerEntry = PlayerToEnableBlackboardMap.Find(ControllerIndex);
	if (PlayerEntry == nullptr)
	{
		FPlayerMotionTrackingEnableBlackboard NewPlayerBlackboard;
		PlayerEntry = &PlayerToEnableBlackboardMap.Add(ControllerIndex, NewPlayerBlackboard);
	}

	bool* DeviceEnabled = PlayerEntry->DeviceToEnabledMap.Find(DeviceType);
	if (DeviceEnabled == nullptr)
	{
		DeviceEnabled = &PlayerEntry->DeviceToEnabledMap.Add(DeviceType);
	}
	*DeviceEnabled = bEnabled;
}

void FPS4Tracker::FMotionTrackingEnableBlackboard::SetDisabledForPlayer(int32 ControllerIndex)
{
	FPlayerMotionTrackingEnableBlackboard* PlayerEntry = PlayerToEnableBlackboardMap.Find(ControllerIndex);
	if (PlayerEntry)
	{
		PlayerEntry->bTrackingDisabledForThisPlayer = true;
		for (auto& DeviceEnabled : PlayerEntry->DeviceToEnabledMap)
		{
			DeviceEnabled.Value = false;
		}
	}
	else
	{
		FPlayerMotionTrackingEnableBlackboard NewPlayerBlackboard;
		NewPlayerBlackboard.bTrackingDisabledForThisPlayer = true;
		PlayerToEnableBlackboardMap.Add(ControllerIndex, NewPlayerBlackboard);
	}
}

void FPS4Tracker::FMotionTrackingEnableBlackboard::SetDisabledAll()
{
	bTrackingEnabledByDefault = false;
	for (auto& Itr : PlayerToEnableBlackboardMap)
	{
		FPlayerMotionTrackingEnableBlackboard& PlayerBlackboard = Itr.Value;
		PlayerBlackboard.bTrackingDisabledForThisPlayer = true;
		for (auto& Itr2 : PlayerBlackboard.DeviceToEnabledMap)
		{
			Itr2.Value = false;
		}
	}
}

void FPS4Tracker::FMotionTrackingEnableBlackboard::SetEnabledByDefault(bool bEnabled)
{
	bTrackingEnabledByDefault = bEnabled;
}


//=============================================================================
FPS4Tracker::FPS4Tracker(const FAutoRegister& AutoRegister)
 : FSceneViewExtensionBase(AutoRegister)
 , ModuleHandle(-1)
 , SubmitStatus(ESubmitStatus::DO)
{
	FMemory::Memzero(DeviceStates, sizeof(DeviceStates));

	Initialize();
}

//=============================================================================
FPS4Tracker::~FPS4Tracker()
{
	Destroy();
}

//=============================================================================
void FPS4Tracker::Initialize()
{
	ModuleHandle = sceSysmoduleLoadModule(SCE_SYSMODULE_VR_TRACKER);	
	if (ModuleHandle < 0)
	{
		UE_LOG(LogPS4Tracker, Warning, TEXT("sceSysmoduleLoadModule for SCE_SYSMODULE_VR_TRACKER failed: 0x%08X"), ModuleHandle);
		return;
	}

	int Result = sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
	if (Result != SCE_OK)
	{
		UE_LOG(LogPS4, Warning, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG) failed (result 0x%08x).  PS4 Tracker will probably not be able to pop up the camera disconnected dialog."), Result);
		return;
	}

	int32_t Ret = sceCommonDialogInitialize();
	if (Ret != SCE_OK && Ret != SCE_COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED)
	{
		UE_LOG(LogPS4Tracker, Error, TEXT("sceCommonDialogInitialize() failed. Error code 0x%x The game will probably not be able to put up system dialogs."), Ret);
	}

	CameraHandle = sceCameraOpen( SCE_USER_SERVICE_USER_ID_SYSTEM, 0, 0, NULL );
	checkf(CameraHandle >= 0, TEXT("sceCameraOpen failed: 0x%x"), CameraHandle);

	SceDeviceTypeRegistrationCount.Add(SCE_VR_TRACKER_DEVICE_HMD, 0);
	SceDeviceTypeRegistrationCount.Add(SCE_VR_TRACKER_DEVICE_DUALSHOCK4, 0);
	SceDeviceTypeRegistrationCount.Add(SCE_VR_TRACKER_DEVICE_MOVE, 0);
	SceDeviceTypeRegistrationCount.Add(SCE_VR_TRACKER_DEVICE_GUN, 0);

	SceDeviceTypeTrackingCount.Add(SCE_VR_TRACKER_DEVICE_HMD, 0);
	SceDeviceTypeTrackingCount.Add(SCE_VR_TRACKER_DEVICE_DUALSHOCK4, 0);
	SceDeviceTypeTrackingCount.Add(SCE_VR_TRACKER_DEVICE_MOVE, 0);
	SceDeviceTypeTrackingCount.Add(SCE_VR_TRACKER_DEVICE_GUN, 0);

	SceVrTrackerQueryMemoryParam QueryMemoryParam;
	FMemory::Memzero(&QueryMemoryParam, sizeof(SceVrTrackerQueryMemoryParam));
	QueryMemoryParam.sizeOfThis = sizeof(SceVrTrackerQueryMemoryParam);
	QueryMemoryParam.profile = SCE_VR_TRACKER_PROFILE_100;
	QueryMemoryParam.calibrationSettings.hmdPosition = SCE_VR_TRACKER_CALIBRATION_MANUAL;
	QueryMemoryParam.calibrationSettings.padPosition = SCE_VR_TRACKER_CALIBRATION_AUTO;
	QueryMemoryParam.calibrationSettings.movePosition = SCE_VR_TRACKER_CALIBRATION_AUTO;
	QueryMemoryParam.calibrationSettings.gunPosition = SCE_VR_TRACKER_CALIBRATION_AUTO;

	// Gets the size of the resources required by the tracker
	SceVrTrackerQueryMemoryResult QueryMemoryResult;
	FMemory::Memzero(&QueryMemoryResult, sizeof(SceVrTrackerQueryMemoryResult));
	QueryMemoryResult.sizeOfThis = sizeof(SceVrTrackerQueryMemoryResult);

	Ret = sceVrTrackerQueryMemory(&QueryMemoryParam, &QueryMemoryResult);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogPS4Tracker, Warning, TEXT("sceMorpheusHeadTrackerQueryMemory() failed: 0x%08X"), Ret);
		return;
	}

	// Sets up the parameters for initialising the tracker library
	FMemory::Memzero(&InitParam, sizeof(SceVrTrackerInitParam));

	InitParam.sizeOfThis = sizeof(SceVrTrackerInitParam);
	InitParam.profile = QueryMemoryParam.profile;
	InitParam.calibrationSettings = QueryMemoryParam.calibrationSettings;
	
	const EGnmMemType TrackerOnionMemType =
#if USE_NEW_PS4_MEMORY_SYSTEM
		// Allocate the tracker memory from the OnionDirect pool. This uses the old non-pooled kernel APIs.
		// The VR Tracker requires non-pooled memory (as of sdk 6 initialization will fail with pooled memory)
		EGnmMemType::GnmMem_OnionDirect;
#else
		EGnmMemType::GnmMem_CPU;
#endif
	OnionTrackingRam = FMemBlock::Allocate(QueryMemoryResult.directMemoryOnionSize, QueryMemoryResult.directMemoryOnionAlignment, TrackerOnionMemType, GET_STATID(STAT_Onion_TrackingRam));
	InitParam.directMemoryOnionPointer = OnionTrackingRam.GetPointer();
	InitParam.directMemoryOnionSize = QueryMemoryResult.directMemoryOnionSize;
	InitParam.directMemoryOnionAlignment = QueryMemoryResult.directMemoryOnionAlignment;
	
	const EGnmMemType TrackerGarlicMemType =
#if USE_NEW_PS4_MEMORY_SYSTEM
		// Allocate the garlic memory from the frame buffer pool. This uses the old non-pooled kernel APIs.
		// libSceVrTracker seems to call sceKernelBatchMap using this memory, so passing it memory from the new pooled APIs will fail.
		EGnmMemType::GnmMem_FrameBuffer;
#else
		EGnmMemType::GnmMem_GPU;
#endif

	GarlicTrackingRam = FMemBlock::Allocate(QueryMemoryResult.directMemoryGarlicSize, QueryMemoryResult.directMemoryGarlicAlignment, TrackerGarlicMemType, GET_STATID(STAT_Garlic_TrackingRam));
	InitParam.directMemoryGarlicPointer = GarlicTrackingRam.GetPointer();
	InitParam.directMemoryGarlicSize = QueryMemoryResult.directMemoryGarlicSize;
	InitParam.directMemoryGarlicAlignment = QueryMemoryResult.directMemoryGarlicAlignment;

	InitParam.workMemoryPointer = memalign(QueryMemoryResult.workMemoryAlignment, QueryMemoryResult.workMemorySize);
	InitParam.workMemorySize = QueryMemoryResult.workMemorySize;
	InitParam.workMemoryAlignment = QueryMemoryResult.workMemoryAlignment;
	if (InitParam.workMemoryPointer != NULL)
	{
		FMemory::Memzero(InitParam.workMemoryPointer, InitParam.workMemorySize);
	}

	// GPU settings
	InitParam.gpuPipeId = 6;
	InitParam.gpuQueueId = 0;

	Ret = sceVrTrackerInit(&InitParam);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerInit failed: 0x%08X"), Ret);
		if( Ret == SCE_VR_TRACKER_ERROR_INVALID_CPU_MODE )
		{
			checkf(false, TEXT("Morpheus requires that 6 CPU mode is enabled in your param.sfo\n"));
		}
		return;
	}

	if (IsCameraControlManual())
	{
		FMemory::Memzero(&GpuSubmitParam, sizeof(GpuSubmitParam));
		GpuSubmitParam.sizeOfThis = sizeof(GpuSubmitParam);
		GpuSubmitParam.robustnessLevel = SCE_VR_TRACKER_ROBUSTNESS_LEVEL_HIGH;

		FMemory::Memzero(&CameraFrameData, sizeof(CameraFrameData));
		CameraFrameData.sizeThis = sizeof(CameraFrameData);
		CameraFrameData.readMode |= SCE_CAMERA_FRAME_MEMORY_TYPE_GARLIC;
		CameraFrameData.readMode |= SCE_CAMERA_FRAME_WAIT_NEXTFRAME_OFF;

		ConfigureCamera();

		struct CameraThreadHelper
		{
			static void* Func(void* pUserData)
			{
				static_cast<FPS4Tracker*>(pUserData)->CameraProcess();
				return NULL;
			}
		};
		scePthreadCreate(&CameraThread, NULL, CameraThreadHelper::Func, this, "FPS4Tracker::CameraThread");
		scePthreadSetprio(CameraThread, SCE_KERNEL_PRIO_FIFO_HIGHEST);
	}

	// Note: The CameraThread has now started, so all initialization for it must have been completed by now.

	CameraConnectionState = sceCameraIsAttached(0);

	FlipToDisplayLatency = RenderFrameTime = 0;

	OnWorldTickStartDelegateHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FPS4Tracker::OnWorldTickStart);

	// Register to receive IMotionTrackingSystemManagement calls
	IModularFeatures::Get().RegisterModularFeature(IMotionTrackingSystemManagement::GetModularFeatureName(), this);

}

void FPS4Tracker::ConfigureCamera()
{
		SceCameraConfig CameraConfig = {};
		CameraConfig.sizeThis = sizeof(CameraConfig);
		CameraConfig.configType = SCE_CAMERA_CONFIG_TYPE5;
		int32_t ret = sceCameraSetConfig(CameraHandle, &CameraConfig);
		if (ret != SCE_OK)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceCameraSetConfig failed: 0x%08X"), ret);
		}

		// Sets camera linked to vsync
		SceCameraVideoSyncParameter CameraVsyncParam;
		FMemory::Memzero(&CameraVsyncParam, sizeof(CameraVsyncParam));
		CameraVsyncParam.sizeThis = sizeof(CameraVsyncParam);
		CameraVsyncParam.videoSyncMode = SCE_CAMERA_VIDEO_SYNC_MODE_ENABLE;
		CameraVsyncParam.pModeOption = nullptr;
		sceCameraSetVideoSync(CameraHandle, &CameraVsyncParam);


		SceCameraStartParameter StartParameter = {};
		StartParameter.sizeThis = (sizeof(SceCameraStartParameter));
		StartParameter.formatLevel[0] = SCE_CAMERA_FRAME_FORMAT_LEVEL_ALL;
		StartParameter.formatLevel[1] = SCE_CAMERA_FRAME_FORMAT_LEVEL_ALL;
		ret = sceCameraStart(CameraHandle, &StartParameter);
		if (ret != SCE_OK)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceCameraStart failed: 0x%08X"), ret);
		}
}

//=============================================================================
void FPS4Tracker::Destroy()
{
	if (ModuleHandle == -1)
	{
		return;
	}

	IModularFeatures::Get().UnregisterModularFeature(IMotionTrackingSystemManagement::GetModularFeatureName(), this);

	FWorldDelegates::OnWorldTickStart.Remove(OnWorldTickStartDelegateHandle);

	if (IsCameraControlManual())
	{
		{
			FScopeLock Lock(&SubmitCriticalSection);
			SubmitStatus = ESubmitStatus::STOP;
		}

		scePthreadJoin(CameraThread, NULL);

		int32 Ret = sceCameraStop(CameraHandle);
		if (Ret != SCE_OK)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceCameraStop failed: 0x%08X"), Ret);
		}
	}

	int32 Ret = sceVrTrackerTerm();
	if (Ret != SCE_OK)
	{
		UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerTerm failed: 0x%08X"), Ret);
		return;
	}

	FMemBlock::Free(GarlicTrackingRam);
	InitParam.directMemoryGarlicPointer = NULL;

	FMemBlock::Free(OnionTrackingRam);
	InitParam.directMemoryOnionPointer = NULL;

	free(InitParam.workMemoryPointer);
	InitParam.workMemoryPointer = NULL;

	Ret = sceSysmoduleUnloadModule(ModuleHandle);
	ModuleHandle = -1;
	if (Ret != SCE_OK)
	{
		UE_LOG(LogPS4Tracker, Warning, TEXT("sceSysmoduleUnloadModule failed: 0x%08X"), Ret);
		return;
	}
}

// Check camera connection, and put up a dialog if it is lost.
// NOTE: during startup MorpheusHMD catches lack of camera with the HMDSetupDialog, but that does not happen
// if you pull the camera after the HMD starts up.
void FPS4Tracker::CameraConnectionTick()
{
	if (bCameraSetupDialogActive)
	{
		UpdateCameraSetupDialog();
	}
	else
	{

		const int OldCameraConnectionState = CameraConnectionState;
		CameraConnectionState = sceCameraIsAttached(0);
		
		if (CameraConnectionState != OldCameraConnectionState)
		{
			UE_LOG(LogPS4Tracker, Log, TEXT("Playstation CameraConnectionState has changed from %i to %i"), OldCameraConnectionState, CameraConnectionState);
		}

		if (CameraConnectionState == 1) // connected
		{
			if (CameraConnectionState != OldCameraConnectionState)
			{
				UE_LOG(LogPS4Tracker, Log, TEXT("PlayStation Camera Connected.  Camera based tracking can resume."));
				ConfigureCamera();
			}
		}
		else // not connected
		{
			// If stereo rendering is on the HMD will pop up the HMDSetupDialog.  Sony wants that rather than the camera connection dialog.
			if (!GEngine->StereoRenderingDevice->IsStereoEnabled())
			{
				const int32 DeviceCount = GetTrackedDeviceCount();
				// If we are trying to track one or more devices pop up the system dialog to prompt the user to fix the camera problem.
				if (DeviceCount > 0)
				{
					UE_LOG(LogPS4Tracker, Log, TEXT("PlayStation Camera is Disconnected, but we are trying to track %i devices!  Tracking will fail or quality will be severely degraded."), DeviceCount);
					if (!sceCommonDialogIsUsed())
					{
						StartCameraSetupDialog();
					}
				}
			}
		}
	}
}

bool FPS4Tracker::StartCameraSetupDialog()
{
	UE_LOG(LogPS4Tracker, Log, TEXT("StartCameraSetupDialog()"));

	check(!bCameraSetupDialogActive);

	bCameraSetupDialogActive = true;

	int32 Ret;

	Ret = sceMsgDialogInitialize();
	if (Ret != SCE_OK)
	{
		printf("sceMsgDialogInitialize failed: 0x%08X\n", Ret);
		EndCameraSetupDialog();
		return false;
	}

	SceMsgDialogSystemMessageParam sysParam;
	memset(&sysParam, 0, sizeof(SceMsgDialogSystemMessageParam));
	sysParam.sysMsgType = SCE_MSG_DIALOG_SYSMSG_TYPE_CAMERA_NOT_CONNECTED;
	SceMsgDialogParam param;
	sceMsgDialogParamInitialize(&param);
	param.mode = SCE_MSG_DIALOG_MODE_SYSTEM_MSG;
	param.sysMsgParam = &sysParam;
	Ret = sceUserServiceGetInitialUser(&param.userId);
	if (Ret != SCE_OK)
	{
		printf("sceUserServiceGetInitialUser failed: 0x%08X\n", Ret);
		EndCameraSetupDialog();
		return false;
	}

	Ret = sceMsgDialogOpen(&param);
	if (Ret != SCE_OK)
	{
		printf("sceMsgDialogOpen failed: 0x%08X\n", Ret);
		EndCameraSetupDialog();
		return false;
	}

	return true;
}

// Return true if we are done handling the hmdSetupDialog.
bool FPS4Tracker::UpdateCameraSetupDialog()
{
	//UE_LOG(LogMorpheusHMD, Log, TEXT("UpdateCameraSetupDialog()")); // This one spams, so off by default.

	check(bCameraSetupDialogActive);

	SceCommonDialogStatus Status;

	Status = sceMsgDialogUpdateStatus();
	switch (Status)
	{
	case SCE_COMMON_DIALOG_STATUS_NONE:
		check(false); // should at least be initialized
		return true;
	case SCE_COMMON_DIALOG_STATUS_INITIALIZED:
		check(false); // dialog should be up, so running or finished.
		return true;
	case SCE_COMMON_DIALOG_STATUS_RUNNING:
		{
			if (GIsRequestingExit)
			{
				UE_LOG(LogPS4Tracker, Log, TEXT("UpdateCameraSetupDialog() terminating because GIsRequestingExit"));
				EndCameraSetupDialog();
				return true;
			}

			const int OldCameraConnectionState = CameraConnectionState;
			CameraConnectionState = sceCameraIsAttached(0);

			if (CameraConnectionState != OldCameraConnectionState)
			{
				UE_LOG(LogPS4Tracker, Log, TEXT("Playstation CameraConnectionState has changed from %i to %i"), OldCameraConnectionState, CameraConnectionState);
			}

			if (1 == CameraConnectionState) // connected
			{
				UE_LOG(LogPS4Tracker, Log, TEXT("UpdateCameraSetupDialog() terminating because the camera is now connected."));
				EndCameraSetupDialog();
				ConfigureCamera();
				return true;
			}
		}
		return false;
	case SCE_COMMON_DIALOG_STATUS_FINISHED:
		{
			UE_LOG(LogPS4Tracker, Log, TEXT("UpdateCameraSetupDialog() SCE_COMMON_DIALOG_STATUS_FINISHED, checking result"));

			SceMsgDialogResult dialogResult;
			memset(&dialogResult, 0, sizeof(SceMsgDialogResult));
			int32 Ret = sceMsgDialogGetResult(&dialogResult);
			if (Ret != SCE_OK)
			{
				printf("sceMsgDialogGetResult failed: 0x%08X\n", Ret);
				EndCameraSetupDialog();
				return false;
			}
			// NOTE: the camera setup dialog appears to only ever have a result of OK, even if you press the circle button rather than x.
			// Therefore we can't actually handle 'cancel' from that dialog.
			if (dialogResult.result != SCE_COMMON_DIALOG_RESULT_OK)
			{
				if (dialogResult.result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED)
				{
					UE_LOG(LogPS4Tracker, Log, TEXT("CameraSetupDialog canceled."), dialogResult.result);
				}
				else
				{
					UE_LOG(LogPS4Tracker, Warning, TEXT("sceMsgDialogGetResult() gave unexpected result: 0x%x  Treating it like cancel."), dialogResult.result);
				}
				EndCameraSetupDialog();

				UE_LOG(LogPS4Tracker, Log, TEXT("UpdateCameraSetupDialog() CameraSetupDialog was canceled. That is not allowed. CameraSetupDialog will be put back up."));
				StartCameraSetupDialog();
				check(bCameraSetupDialogActive);
				return false;
			}
			else
			{
				UE_LOG(LogPS4Tracker, Log, TEXT("UpdateCameraSetupDialog() dialogResult.result == SCE_COMMON_DIALOG_RESULT_OK"));
				EndCameraSetupDialog();
				return true;
			}
		}
		check(false); // should have returned.
		return true;
	default:
		check(false); // unknown status value
		return true;
	}
}

void FPS4Tracker::EndCameraSetupDialog()
{
	check(bCameraSetupDialogActive);

	sceMsgDialogTerminate();

	bCameraSetupDialogActive = false;
}


//=============================================================================
int32 FPS4Tracker::AcquireTracker(int32 DeviceHandle, int32 ControllerIndex, EDeviceType DeviceType)
{
	FScopeLock ScopeLock(&CritSect);

	if (ModuleHandle == -1)
	{
		return INVALID_TRACKER_HANDLE;
	}

	// First lets see if we already have one
	for (int32 TrackerHandle = 0; TrackerHandle < ARRAY_COUNT(DeviceStates); ++TrackerHandle)
	{
		auto& DeviceState = DeviceStates[TrackerHandle];
		if (DeviceState.DeviceHandle == DeviceHandle)
		{
			DeviceState.RefCount += 1;
			return TrackerHandle;
		}
	}

	return CreateTracker(DeviceHandle, ControllerIndex, DeviceType);
}

//=============================================================================
int32 FPS4Tracker::CreateTracker(int32 DeviceHandle, int32 ControllerIndex, EDeviceType DeviceType)
{
	FScopeLock ScopeLock(&CritSect);

	if (ModuleHandle == -1)
	{
		return INVALID_TRACKER_HANDLE;
	}
	///TODO tutaj szukaj jak dostać ten jebany handle
	// Find an unused device state
	int32 TrackerHandle = 0;
	for (; TrackerHandle < ARRAY_COUNT(DeviceStates); ++TrackerHandle)
	{
		auto& DeviceState = DeviceStates[TrackerHandle];
		if (!DeviceState.bIsValid)
		{
			break;
		}
	}
	if (TrackerHandle >= ARRAY_COUNT(DeviceStates))
	{
		UE_LOG(LogPS4Tracker, Warning, TEXT("Tried to create a tracker for %i (HMD,Left,Pad,Right,Gun), but we already have %i trackers, which is the maximum.  Failing."), (int)DeviceType, ARRAY_COUNT(DeviceStates));
		return INVALID_TRACKER_HANDLE;
	}

	auto& DeviceState = DeviceStates[TrackerHandle];

	FTrackingData::DefaultInitialize(DeviceState.TrackingData);
	DeviceState.DeviceHandle = DeviceHandle;
	DeviceState.ControllerIndex = ControllerIndex;
	DeviceState.bIsValid = true;
	DeviceState.bIsSetToTrack = false;
	DeviceState.RefCount = 1;
	DeviceState.DeviceType = DeviceType;
	DeviceState.SceDeviceType = DeviceTypeToSceDeviceType(DeviceType);
	SceDeviceTypeRegistrationCount[DeviceState.SceDeviceType] += 1;

	if (MotionTrackingEnableBlackboard.GetIsEnabled(ControllerIndex, DeviceType))
	{
		bool bSuccess = EnableTrackerInternal(DeviceState);
		// Ensures we have a specific enable state for this device, so if defaults change later consistency is maintained.
		MotionTrackingEnableBlackboard.SetIsEnabled(ControllerIndex, DeviceType, bSuccess);

		if (!bSuccess)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("Failed to activate tracker %i when creating it.  Disabling tracking for this device."), TrackerHandle);
		}
	}
	else
	{
		// Ensures we have a specific enable state for this device, so if defaults change later consistency is maintained.
		MotionTrackingEnableBlackboard.SetIsEnabled(ControllerIndex, DeviceType, false);
	}

#if !UE_BUILD_SHIPPING
	check(ConsistencyCheck(ControllerIndex, DeviceType));
#endif

	return TrackerHandle;
}

bool FPS4Tracker::EnableTrackerInternal(FDeviceState& DeviceState)
{
	FScopeLock ScopeLock(&CritSect);

	if (ModuleHandle == -1)
	{
		return false;
	}

	check(DeviceState.bIsValid);

	if (DeviceState.bIsSetToTrack)
	{
		return true;
	}

	// there is a limit on the number of controllers that can be tracked.
	if (DeviceState.SceDeviceType != SCE_VR_TRACKER_DEVICE_HMD)
	{
		int32 TrackedControllerCount = GetMotionTrackingEnabledControllerCount();
		if (TrackedControllerCount >= MAX_TRACKED_CONTROLLERS)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("Trying to create a tracker for %i (HMD,Pad,Move,Gun), but we already have %i controller trackers and the maximum is %i.  Failing."), DeviceState.SceDeviceType, TrackedControllerCount, MAX_TRACKED_CONTROLLERS);
			return false;
		}
	}

	int32 Ret = sceVrTrackerRegisterDevice(DeviceState.SceDeviceType, DeviceState.DeviceHandle);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerRegisterDevice failed: 0x%08X.  We will attempt to register it every frame until it succeeds."), Ret);
	}

	DeviceState.bIsSetToTrack = true;
	SceDeviceTypeTrackingCount[DeviceState.SceDeviceType] += 1;
	return true;
}

void FPS4Tracker::DisableTrackerInternal(FDeviceState& DeviceState)
{
	FScopeLock ScopeLock(&CritSect);

	if (ModuleHandle == -1)
	{
		return;
	}

	check(DeviceState.bIsValid);
	if (DeviceState.bIsSetToTrack)
	{
		int32 Ret = sceVrTrackerUnregisterDevice(DeviceState.DeviceHandle);
		if (Ret != SCE_OK)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerUnregisterDevice failed: 0x%08X"), Ret);
		}
		DeviceState.TrackingData.Status = ETrackingStatus::NOT_STARTED;
		check(SceDeviceTypeTrackingCount[DeviceState.SceDeviceType] > 0);
		SceDeviceTypeTrackingCount[DeviceState.SceDeviceType] -= 1;
		DeviceState.bIsSetToTrack = false;
	}
}

//=============================================================================
void FPS4Tracker::ReleaseTracker(int32 TrackerHandle)
{
	FScopeLock ScopeLock(&CritSect);

	if (ModuleHandle == -1)
	{
		return;
	}

	check(TrackerHandle < ARRAY_COUNT(DeviceStates));
	if (TrackerHandle == INVALID_TRACKER_HANDLE)
	{
		return;
	}
	check(TrackerHandle >= 0);

	auto& DeviceState = DeviceStates[TrackerHandle];
	check(DeviceState.RefCount >= 0);
	check(DeviceState.bIsValid);
	DeviceState.RefCount -= 1;

	if (DeviceState.RefCount == 0)
	{
		DestroyTracker(TrackerHandle);
	}
}

//=============================================================================
void FPS4Tracker::DestroyTracker(int32 TrackerHandle)
{
	FScopeLock ScopeLock(&CritSect);

	if (ModuleHandle == -1)
	{
		return;
	}

	check(TrackerHandle != INVALID_TRACKER_HANDLE);
	check(TrackerHandle < ARRAY_COUNT(DeviceStates));

	FDeviceState& DeviceState = DeviceStates[TrackerHandle];
	check(DeviceState.bIsValid);

	// Do not update MotionTrackingEnableBlackboard so that enable state persists across Destroy.
	DisableTrackerInternal(DeviceState);
	DeviceState.RefCount = 0;
	check(SceDeviceTypeRegistrationCount[DeviceState.SceDeviceType] > 0);
	SceDeviceTypeRegistrationCount[DeviceState.SceDeviceType] -= 1;
	DeviceState.bIsValid = false;
}

//=============================================================================
void FPS4Tracker::Synchronize(int32 TrackerHandle)
{
	SCOPED_NAMED_EVENT_TEXT("FPS4Tracker::Synchronize", FColor::Yellow);

	FScopeLock ScopeLock(&CritSect);

	check(IsInRenderingThread());
	check(TrackerHandle != INVALID_TRACKER_HANDLE);
	check(TrackerHandle < ARRAY_COUNT(DeviceStates));

	FDeviceState& DeviceState = DeviceStates[TrackerHandle];
	check(DeviceState.bIsValid);

	// Spin wait until polled data timestamp is fresh, but give up after a timeout
	// in case there is a problem with the tracking system.
	FTrackingData TrackingData;
	FTrackingData::DefaultInitialize(TrackingData);
	PollTrackingData(DeviceState, TrackingData, 0, false);
	const uint64 StartTimestamp = TrackingData.TimeStamp;

	SceKernelUseconds PollInterval = 500;
	uint64_t Timeout = 30000;
	uint64_t StartTime = 0;
	sceVrTrackerGetTime(&StartTime);
	uint64_t EndTime = StartTime + Timeout;

	while (StartTimestamp == TrackingData.TimeStamp)
	{
		sceKernelUsleep(PollInterval);
		PollTrackingData(DeviceState, TrackingData, 0, false);

		uint64_t Time = 0;
		sceVrTrackerGetTime(&Time);
		if (Time > EndTime)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("FPS4Tracker::Synchronize timed out.  There may be a problem with the tracking hardware.  Tracking data may be stale, or a default value."));
			break;
		}
	}
	DeviceState.TrackingData = TrackingData;
}

//=============================================================================
void FPS4Tracker::GetTrackingData(int32 TrackerHandle, FTrackingData& TrackingData, bool bPollImmediately, bool bEarlyPoll)
{
	//SCOPED_NAMED_EVENT_TEXT("FPS4Tracker::GetTrackingData", FColor::Turquoise);

	FScopeLock ScopeLock(&CritSect);

	// Initialize return values to zero
	FMemory::Memzero(&TrackingData, sizeof(FTrackingData));

	if (ModuleHandle == -1)
	{
		return;
	}

	check(TrackerHandle != INVALID_TRACKER_HANDLE);
	check(TrackerHandle < ARRAY_COUNT(DeviceStates));


	FDeviceState& DeviceState = DeviceStates[TrackerHandle];

	if (bPollImmediately)
	{
		const uint64 PredictionTimeOffset = CalculatePredictionTime(bEarlyPoll);
		PollTrackingData(DeviceState, TrackingData, PredictionTimeOffset, bEarlyPoll);
	}
	else
	{
		TrackingData = DeviceState.TrackingData;
	}
}

//=============================================================================
void FPS4Tracker::OnWorldTickStart(ELevelTick TickType, float DeltaSeconds)
{
	CameraConnectionTick();
}

//=============================================================================
void FPS4Tracker::SetPredictionTiming(int32 InFlipToDisplayLatency, int32 InRenderFrameTime, bool InbIs60Render120ScanoutMode)
{
	FScopeLock ScopeLock(&CritSect);
	
	FlipToDisplayLatency			= InFlipToDisplayLatency;
	RenderFrameTime					= InRenderFrameTime;
	bIs60Render120ScanoutMode		= InbIs60Render120ScanoutMode;
}

bool FPS4Tracker::IsCameraConnected() const
{
	return CameraConnectionState == 1;
}

int32 FPS4Tracker::GetTrackerHandle(int32 ControllerIndex, EDeviceType DeviceType) const
{
	for (int i = 0; i < ARRAY_COUNT(DeviceStates); ++i)
	{
		const FDeviceState& DeviceState = DeviceStates[i];
		if (DeviceState.bIsValid && DeviceState.ControllerIndex == ControllerIndex && DeviceState.DeviceType == DeviceType)
		{
			return i;
		}
	}

	return INDEX_NONE;
}

void FPS4Tracker::SetIsControllerMotionTrackingEnabledByDefault(bool bEnable)
{
	MotionTrackingEnableBlackboard.SetEnabledByDefault(bEnable);
}

int32 FPS4Tracker::GetMotionTrackingEnabledControllerCount() const
{
	int32 TrackedControllerCount = 0;
	for (const auto& TestDeviceState : DeviceStates)
	{
		if (TestDeviceState.SceDeviceType != SCE_VR_TRACKER_DEVICE_HMD && TestDeviceState.bIsSetToTrack)
		{
			++TrackedControllerCount;
		}
	}
	return TrackedControllerCount;
}

// Sanity checks to verify that the enable blackboard result matches any active DeviceState.
bool FPS4Tracker::ConsistencyCheck(int32 ControllerIndex, EControllerHand DeviceHand) const
{
	EDeviceType DeviceType = ControllerHandToDeviceType(DeviceHand);
	return ConsistencyCheck(ControllerIndex, DeviceType);
}
bool FPS4Tracker::ConsistencyCheck(int32 ControllerIndex, EDeviceType DeviceType) const
{
	bool Ret = MotionTrackingEnableBlackboard.GetIsEnabled(ControllerIndex, DeviceType);

	// Sanity checking that does some work.
	int32 TrackerHandle = GetTrackerHandle(ControllerIndex, DeviceType);
	if (TrackerHandle != INDEX_NONE)
	{
		const FDeviceState& DeviceState = DeviceStates[TrackerHandle];
		if (DeviceState.bIsValid)
		{
			return (Ret == DeviceState.bIsSetToTrack);
		}
	}

	return true;
}

bool FPS4Tracker::IsMotionTrackingEnabledForDevice(const int32 ControllerIndex, const EControllerHand DeviceHand) const
{
#if !UE_BUILD_SHIPPING
	check(ConsistencyCheck(ControllerIndex, DeviceHand));
#endif

	EDeviceType DeviceType = ControllerHandToDeviceType(DeviceHand);
	return MotionTrackingEnableBlackboard.GetIsEnabled(ControllerIndex, DeviceType);
}

bool FPS4Tracker::EnableMotionTrackingOfDevice(const int32 ControllerIndex, const EControllerHand DeviceHand)
{
#if !UE_BUILD_SHIPPING
	check(ConsistencyCheck(ControllerIndex, DeviceHand));
#endif

	EDeviceType DeviceType = ControllerHandToDeviceType(DeviceHand);
	for (FDeviceState& DeviceState : DeviceStates)
	{
		if (DeviceState.bIsValid && DeviceState.ControllerIndex == ControllerIndex && DeviceState.DeviceType == DeviceType)
		{
			if (DeviceState.bIsSetToTrack)
			{
				return true;
			}
			else
			{
				bool bSuccess = EnableTrackerInternal(DeviceState);
				MotionTrackingEnableBlackboard.SetIsEnabled(ControllerIndex, DeviceType, bSuccess);
#if !UE_BUILD_SHIPPING
				check(ConsistencyCheck(ControllerIndex, DeviceHand));
#endif
				return bSuccess;
			}
		}
	}

	return false;
}

void FPS4Tracker::DisableMotionTrackingOfDevice(const int32 ControllerIndex, const EControllerHand DeviceHand)
{
#if !UE_BUILD_SHIPPING
	check(ConsistencyCheck(ControllerIndex, DeviceHand));
#endif

	EDeviceType DeviceType = ControllerHandToDeviceType(DeviceHand);
	MotionTrackingEnableBlackboard.SetIsEnabled(ControllerIndex, DeviceType, false);
	for (FDeviceState& DeviceState : DeviceStates)
	{
		if (DeviceState.bIsValid && DeviceState.bIsSetToTrack && DeviceState.ControllerIndex == ControllerIndex && DeviceState.DeviceType == DeviceType)
		{
			DisableTrackerInternal(DeviceState);
			break;
		}
	}

#if !UE_BUILD_SHIPPING
	check(ConsistencyCheck(ControllerIndex, DeviceHand));
#endif
}

void FPS4Tracker::DisableMotionTrackingOfAllControllers()
{
	MotionTrackingEnableBlackboard.SetDisabledAll();
	for (FDeviceState& DeviceState : DeviceStates)
	{
		if (DeviceState.bIsValid && DeviceState.SceDeviceType != SCE_VR_TRACKER_DEVICE_HMD && DeviceState.bIsSetToTrack)
		{
			DisableTrackerInternal(DeviceState);

#if !UE_BUILD_SHIPPING
			check(ConsistencyCheck(DeviceState.ControllerIndex, DeviceState.DeviceType));
#endif
		}
	}
}

void FPS4Tracker::DisableMotionTrackingOfControllersForPlayer(int32 PlayerIndex)
{
	MotionTrackingEnableBlackboard.SetDisabledForPlayer(PlayerIndex);
	for (FDeviceState& DeviceState : DeviceStates)
	{
		if (DeviceState.bIsValid && DeviceState.SceDeviceType != SCE_VR_TRACKER_DEVICE_HMD && DeviceState.bIsSetToTrack && DeviceState.ControllerIndex == PlayerIndex)
		{
			DisableTrackerInternal(DeviceState);

#if !UE_BUILD_SHIPPING
			check(ConsistencyCheck(DeviceState.ControllerIndex, DeviceState.DeviceType));
#endif
		}
	}
}

//=============================================================================
void FPS4Tracker::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	SCOPED_NAMED_EVENT_TEXT("FPS4Tracker::PreRenderViewFamily_RenderThread", FColor::Turquoise);
	FScopeLock ScopeLock(&CritSect);

	const uint64 PredictionTimeOffset = CalculatePredictionTime(false);

	check(IsInRenderingThread());
	for (auto& DeviceState : DeviceStates)
	{
		if (!DeviceState.bIsValid)
		{
			continue;
		}
		PollTrackingData(DeviceState, DeviceState.TrackingData, PredictionTimeOffset, false);
	}
}

//=============================================================================
uint64 FPS4Tracker::CalculatePredictionTime(bool bEarlyPoll) const
{
	if (!RenderFrameTime)
	{
		return 0;
	}

	// The frame sequence goes:
	// game thread : render thread : GPU : present
	// Each step can take up to a full frame, and the current implementation forces the last step to take a full frame even if it could
	// be completed more quickly.

	// The early poll is done at the beginning of the game thread, so has the full 3 render frames of latency plus the flip delay.
	// The render thread does another poll when it starts up, about half way through the game thread, so it has about 2 render frames of latency
	// plus the flip delay.

	// We know what frame we are trying to predict for, from GFrameNumberRenderThread. 
	// We store the frame of the last ApplyReprojection call, and we can find the flip before that.
	// This allows us to calculate the time of the flip before the ApplyReprojection call we are
	// trying to predict for, and then we can offset a bit to the actual ideal prediction time when photons
	// will start coming out of the HMD.
	
	// If we are missing frame rate then at least some frames will take another flip or more of latency.
	// However unless framerate is consistently bad it would be hard to correctly predict which frames 
	// need the longer prediction in advance, and the reprojection system mostly works with under-prediction.
	// It has a tendency to completely fail with significant over-prediction, manifesting as jitter.
	// In this implementation we do not attempt to adapt to bad framerate, there are worse things than reprojection
	// edge visibility that come with poor framerate and stable framerate is critically important and TRC enforced.

	GnmBridge::MorpheusApplyReprojectionInfo Info;
	GnmBridge::GetLastApplyReprojectionInfo(Info);

	uint64_t CurrentTime;
	sceVrTrackerGetTime(&CurrentTime);

	// In 60/120 mode the 'present' frame is only half a render frame long, 90 and 120 it is a whole render frame.
	const uint64 ScanoutFrameTimeDivisor = bIs60Render120ScanoutMode ? 2 : 1;

	//const uint32_t RenderFramesBetweenLastApplyAndThisApply = GFrameNumberRenderThread - Info.FrameNumber;
	uint32_t RenderFramesBetweenLastApplyAndThisApply = GFrameNumberRenderThread - Info.FrameNumber;
	if (RenderFramesBetweenLastApplyAndThisApply > 3)
	{
		RenderFramesBetweenLastApplyAndThisApply = 3;
	}
	const uint64 TimeOfFlipBeforeLastApply = Info.PreviousFlipTime;
	const uint64 PredictedTimeForThisApply = TimeOfFlipBeforeLastApply + RenderFramesBetweenLastApplyAndThisApply * RenderFrameTime;
	const uint64 IdealPredictionTime = PredictedTimeForThisApply + (RenderFrameTime / ScanoutFrameTimeDivisor) + FlipToDisplayLatency;
	const uint64 TimeFromNowToPrediction = IdealPredictionTime - CurrentTime;
	
	return TimeFromNowToPrediction;
}

//=============================================================================
void FPS4Tracker::SubmitComputeWithCamera()
{
	SCOPED_NAMED_EVENT_TEXT("FPS4Tracker::SubmitComputeWithCamera", FColor::Turquoise);

	// Syncing the camera update to vsync keeps it from overlapping with reprojection
	uint32 VideoOutHandle = GnmBridge::GetVideoOutPort();
	sceVideoOutWaitVblank(VideoOutHandle);

	// If the camera is disconnected just UpdateMotionSensorData.
	if (CameraConnectionState != 1)
	{
		UpdateMotionSensorData();
		return;
	}

	FMemory::Memzero(&CameraFrameData, sizeof(CameraFrameData));
	CameraFrameData.readMode |= SCE_CAMERA_FRAME_MEMORY_TYPE_GARLIC;
	CameraFrameData.readMode |= SCE_CAMERA_FRAME_WAIT_NEXTFRAME_OFF;
	CameraFrameData.sizeThis = sizeof(CameraFrameData);
	int32 Ret = sceCameraGetFrameData(CameraHandle, &CameraFrameData);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogPS4Tracker, Warning, TEXT("sceCameraGetFrameData() failed: 0x%08X"), Ret);
	}

	for (int i = 0; i < 2; i++)
	{
		auto& EG = CameraFrameData.meta.exposureGain[i];
		EG.exposure = 20;
		EG.exposureControl = SCE_CAMERA_ATTRIBUTE_AEC_AGC_DISABLE;
		EG.gain = 100;
		EG.mode = SCE_CAMERA_ATTRIBUTE_EXPOSUREGAIN_MODE_0;
		CameraFrameData.status[i] = 1;
	}

	GpuSubmitParam.cameraFrameData = CameraFrameData;
	{
		SCOPED_NAMED_EVENT_TEXT("sceVrTrackerGpuSubmit", FColor::Turquoise);
		Ret = sceVrTrackerGpuSubmit(&GpuSubmitParam);
	}
	if (Ret != SCE_OK)
	{
		if (Ret != SCE_VR_TRACKER_ERROR_ALREADY_PROCESSING_CAMERA_FRAME)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerGpuSubmit failed: 0x%08X  Calling UpdateMotionSensorData as a fallback."), Ret);
		}
		UpdateMotionSensorData();
		return;
	}

	{
		SCOPED_NAMED_EVENT_TEXT("sceVrTrackerGpuWaitAndCpuProcess", FColor::Turquoise);
		Ret = sceVrTrackerGpuWaitAndCpuProcess();
	}
	if (Ret != SCE_OK)
	{
		UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerGpuWaitAndCpuProcess failed: 0x%08X"), Ret);
	}
}

//=============================================================================
void FPS4Tracker::Recalibrate()
{
	SceVrTrackerRecalibrateParam RecalibrateParam;
	FMemory::Memzero(&RecalibrateParam, sizeof(SceVrTrackerRecalibrateParam));
	RecalibrateParam.deviceType = SCE_VR_TRACKER_DEVICE_HMD;
	RecalibrateParam.calibrationType = SCE_VR_TRACKER_CALIBRATION_ALL;

	sceVrTrackerRecalibrate(&RecalibrateParam);
}

//=============================================================================
bool FPS4Tracker::IsCameraControlManual() const
{
	return true;
}

//=============================================================================
int32 FPS4Tracker::GetTrackedDeviceCount() const
{
	int32 count = 0;
	for (auto itr: SceDeviceTypeTrackingCount)
	{
		count += itr.Value;
	}
	return count;
}

//=============================================================================
void FPS4Tracker::UpdateMotionSensorData()
{
	SCOPED_NAMED_EVENT_TEXT("FPS4Tracker::UpdateMotionSensorData", FColor::Turquoise);

	SceVrTrackerUpdateMotionSensorDataParam updateMotionSensorDataParam = {};
	updateMotionSensorDataParam.sizeOfThis = sizeof(SceVrTrackerUpdateMotionSensorDataParam);
	int32 Ret = SCE_OK;

	SceVrTrackerDeviceType DeviceType = SCE_VR_TRACKER_DEVICE_HMD;
	if (SceDeviceTypeTrackingCount[DeviceType] > 0)
	{
		updateMotionSensorDataParam.deviceType = DeviceType;
		Ret = sceVrTrackerUpdateMotionSensorData(&updateMotionSensorDataParam);
		if (Ret != SCE_OK)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerUpdateMotionSensorData failed for HMD with return value 0x%x"), Ret)
		}
	}

	DeviceType = SCE_VR_TRACKER_DEVICE_DUALSHOCK4;
	if (SceDeviceTypeTrackingCount[DeviceType] > 0)
	{
		updateMotionSensorDataParam.deviceType = DeviceType;
		Ret = sceVrTrackerUpdateMotionSensorData(&updateMotionSensorDataParam);
		if (Ret != SCE_OK)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerUpdateMotionSensorData failed for DUALSHOCK4 with return value 0x%x"), Ret)
		}
	}

	DeviceType = SCE_VR_TRACKER_DEVICE_MOVE;
	if (SceDeviceTypeTrackingCount[DeviceType] > 0)
	{
		updateMotionSensorDataParam.deviceType = DeviceType;
		Ret = sceVrTrackerUpdateMotionSensorData(&updateMotionSensorDataParam);
		if (Ret != SCE_OK)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerUpdateMotionSensorData failed for MOVE(controller) with return value 0x%x"), Ret)
		}
	}

	DeviceType = SCE_VR_TRACKER_DEVICE_GUN;
	if (SceDeviceTypeTrackingCount[DeviceType] > 0)
	{
		updateMotionSensorDataParam.deviceType = DeviceType;
		Ret = sceVrTrackerUpdateMotionSensorData(&updateMotionSensorDataParam);
		if (Ret != SCE_OK)
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerUpdateMotionSensorData failed for GUN(controller) with return value 0x%x"), Ret)
		}
	}
}

//=============================================================================
// Run the camera thread at 1/vblank.  The camera is synced to vblank, so this also syncs to the camera.  
// In 60/120 mode this runs at 120, so half the SubmitComputeWithCamera calls will fall back to UpdateMotionSensorData.
void FPS4Tracker::CameraProcess()
{
	while (1)
	{
		if (SubmitStatus == ESubmitStatus::STOP)
		{
			return;
		}

		{
			FScopeLock Lock(&SubmitCriticalSection);
			if (SubmitStatus == ESubmitStatus::STOP)
			{
				return;
			}

			// This function blocks on vblank.
			SubmitComputeWithCamera();
		}
	}
}

//=============================================================================
void FPS4Tracker::PollTrackingData(FDeviceState& DeviceState, FTrackingData& TrackingData, uint64 PredictionTimeOffset, bool bEarlyPoll)
{
	SCOPED_NAMED_EVENT_TEXT("FPS4Tracker::PollTrackingData", FColor::Turquoise);

	check(DeviceState.bIsValid == true);
	if (DeviceState.bIsSetToTrack == false)
	{
		TrackingData.Status = ETrackingStatus::NOT_STARTED;
		return;
	}

	int32 DeviceHandle = DeviceState.DeviceHandle;
	SceVrTrackerDeviceType SceDeviceType = DeviceState.SceDeviceType;

	if (SceDeviceType == SCE_VR_TRACKER_DEVICE_HMD)
	{
#if HAS_MORPHEUS
		// If the HMD is disconnected sceVrTrackerGetResult will fail
		// unfortunately it does so with SCE_VR_TRACKER_ERROR_TIMESTAMP_OUT_OF_RANGE, which also happens for other reasons.
		// So we catch the disconnected case here.
		SceHmdDeviceInformation Info;
		int32_t Ret = sceHmdGetDeviceInformationByHandle(DeviceHandle, &Info);
		if (Ret != SCE_OK)
		{
			TrackingData.Status = ETrackingStatus::NOT_TRACKING;
			return;
		}
#else
		TrackingData.Status = ETrackingStatus::NOT_TRACKING;
		return;
#endif
	}

	uint64_t CurrentTime;
	int32 Ret = sceVrTrackerGetTime(&CurrentTime);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerGetTime() failed with return code %i."), Ret);
	}

	SceVrTrackerGetResultParam Param = {};
	Param.sizeOfThis = sizeof(Param);
	Param.handle = DeviceHandle;
	Param.resultType = SCE_VR_TRACKER_RESULT_PREDICTED;	
	Param.predictionTime = CurrentTime + PredictionTimeOffset;
	Param.orientationType = SCE_VR_TRACKER_ORIENTATION_ABSOLUTE;
	Param.debugMarkerType = bEarlyPoll ? SCE_VR_TRACKER_DEBUG_MARKER_DEFAULT_UPDATE : SCE_VR_TRACKER_DEBUG_MARKER_FINAL_UPDATE;
	Param.userFrameNumber = GFrameNumberRenderThread;

	SceVrTrackerResultData TrackerState;
	Ret = sceVrTrackerGetResult(&Param, &TrackerState);
	if (Ret == SCE_OK)
	{
		TrackingData.TimeStamp = TrackerState.timestamp;
		TrackingData.SensorReadSystemTimestamp = TrackerState.hmdInfo.sensorReadSystemTimestamp;
		TrackingData.FrameNumber = GFrameNumberRenderThread;
		TrackingData.CameraOrientation = FQuat(TrackerState.cameraOrientation.w, TrackerState.cameraOrientation.x, TrackerState.cameraOrientation.y, TrackerState.cameraOrientation.z);

		if (TrackerState.status == SCE_VR_TRACKER_STATUS_TRACKING)
		{
			if (SceDeviceType == SCE_VR_TRACKER_DEVICE_HMD)
			{
				// this is handled elsewhere.
			}
			else if (SceDeviceType == SCE_VR_TRACKER_DEVICE_DUALSHOCK4)
			{
				TrackingData.DevicePosition = FVector(
					TrackerState.padInfo.devicePose.position.x,
					TrackerState.padInfo.devicePose.position.y,
					TrackerState.padInfo.devicePose.position.z
					);
			}
			else if( SceDeviceType == SCE_VR_TRACKER_DEVICE_MOVE )
			{
				TrackingData.DevicePosition = FVector(
					TrackerState.moveInfo.devicePose.position.x,
					TrackerState.moveInfo.devicePose.position.y,
					TrackerState.moveInfo.devicePose.position.z
					);
			}
			else if (SceDeviceType == SCE_VR_TRACKER_DEVICE_GUN)
			{
				TrackingData.DevicePosition = FVector(
					TrackerState.gunInfo.devicePose.position.x,
					TrackerState.gunInfo.devicePose.position.y,
					TrackerState.gunInfo.devicePose.position.z
					);

			}
			else
			{
				check(false);
			}
			TrackingData.Position = ConvertToUnrealCoords(TrackingData.DevicePosition);
		}
		else
		{
			TrackingData.DevicePosition = TrackingData.Position = FVector::ZeroVector;
			TrackingData.HeadPosition = TrackingData.UnrealHeadPosition = FVector::ZeroVector;
		}

		if( SceDeviceType == SCE_VR_TRACKER_DEVICE_HMD )
		{
			TrackingData.DeviceOrientation = FQuat(
				TrackerState.hmdInfo.devicePose.orientation.x,
				TrackerState.hmdInfo.devicePose.orientation.y,
				TrackerState.hmdInfo.devicePose.orientation.z,
				TrackerState.hmdInfo.devicePose.orientation.w
				);

			TrackingData.EyeOrientation[0] = FQuat(
				TrackerState.hmdInfo.leftEyePose.orientation.x,
				TrackerState.hmdInfo.leftEyePose.orientation.y,
				TrackerState.hmdInfo.leftEyePose.orientation.z,
				TrackerState.hmdInfo.leftEyePose.orientation.w
				);

			TrackingData.EyeOrientation[1] = FQuat(
				TrackerState.hmdInfo.rightEyePose.orientation.x,
				TrackerState.hmdInfo.rightEyePose.orientation.y,
				TrackerState.hmdInfo.rightEyePose.orientation.z,
				TrackerState.hmdInfo.rightEyePose.orientation.w
				);

			TrackingData.HeadOrientation = FQuat(
				TrackerState.hmdInfo.headPose.orientation.x,
				TrackerState.hmdInfo.headPose.orientation.y,
				TrackerState.hmdInfo.headPose.orientation.z,
				TrackerState.hmdInfo.headPose.orientation.w
				);

			TrackingData.Orientation = ConvertToUnrealCoords(TrackingData.DeviceOrientation);
			TrackingData.UnrealEyeOrientation[0] = ConvertToUnrealCoords(TrackingData.EyeOrientation[0]);
			TrackingData.UnrealEyeOrientation[1] = ConvertToUnrealCoords(TrackingData.EyeOrientation[1]);
			TrackingData.UnrealHeadOrientation = ConvertToUnrealCoords(TrackingData.HeadOrientation);

			TrackingData.DevicePosition = FVector(
				TrackerState.hmdInfo.devicePose.position.x,
				TrackerState.hmdInfo.devicePose.position.y,
				TrackerState.hmdInfo.devicePose.position.z
				);

			TrackingData.EyePosition[0] = FVector(
				TrackerState.hmdInfo.leftEyePose.position.x,
				TrackerState.hmdInfo.leftEyePose.position.y,
				TrackerState.hmdInfo.leftEyePose.position.z
				);

			TrackingData.EyePosition[1] = FVector(
				TrackerState.hmdInfo.rightEyePose.position.x,
				TrackerState.hmdInfo.rightEyePose.position.y,
				TrackerState.hmdInfo.rightEyePose.position.z
				);

			TrackingData.HeadPosition = FVector(
				TrackerState.hmdInfo.headPose.position.x,
				TrackerState.hmdInfo.headPose.position.y,
				TrackerState.hmdInfo.headPose.position.z
				);

			TrackingData.UnrealEyePosition[0] = ConvertToUnrealCoords(TrackingData.EyePosition[0]);
			TrackingData.UnrealEyePosition[1] = ConvertToUnrealCoords(TrackingData.EyePosition[1]);
			TrackingData.UnrealHeadPosition = ConvertToUnrealCoords(TrackingData.HeadPosition);

			TrackingData.Position = ConvertToUnrealCoords(TrackingData.HeadPosition);
		}
		else if (TrackerState.status != SCE_VR_TRACKER_STATUS_NOT_STARTED)
		{
			if( SceDeviceType == SCE_VR_TRACKER_DEVICE_DUALSHOCK4 )
			{
				TrackingData.DeviceOrientation = FQuat(
					TrackerState.padInfo.devicePose.orientation.x,
					TrackerState.padInfo.devicePose.orientation.y,
					TrackerState.padInfo.devicePose.orientation.z,
					TrackerState.padInfo.devicePose.orientation.w
					);
			}
			else if( SceDeviceType == SCE_VR_TRACKER_DEVICE_MOVE )
			{
				TrackingData.DeviceOrientation = FQuat(
					TrackerState.moveInfo.devicePose.orientation.x,
					TrackerState.moveInfo.devicePose.orientation.y,
					TrackerState.moveInfo.devicePose.orientation.z,
					TrackerState.moveInfo.devicePose.orientation.w
					);
			}		
			else if (SceDeviceType == SCE_VR_TRACKER_DEVICE_GUN)
			{
				TrackingData.DeviceOrientation = FQuat(
					TrackerState.gunInfo.devicePose.orientation.x,
					TrackerState.gunInfo.devicePose.orientation.y,
					TrackerState.gunInfo.devicePose.orientation.z,
					TrackerState.gunInfo.devicePose.orientation.w
					);
			}
			else
			{
				check(false);
			}
			TrackingData.Orientation = ConvertToUnrealCoords(TrackingData.DeviceOrientation);
		}
		else
		{
			TrackingData.DeviceOrientation = TrackingData.Orientation = FQuat::Identity;
		}

		switch(TrackerState.status) 
		{
			case SCE_VR_TRACKER_STATUS_NOT_STARTED:
				TrackingData.Status = ETrackingStatus::NOT_STARTED;
				break;
			case SCE_VR_TRACKER_STATUS_TRACKING:
				TrackingData.Status = ETrackingStatus::TRACKING;
				break;
			case SCE_VR_TRACKER_STATUS_NOT_TRACKING:
				TrackingData.Status = ETrackingStatus::NOT_TRACKING;
				break;
			case SCE_VR_TRACKER_STATUS_CALIBRATING:
				TrackingData.Status = ETrackingStatus::CALIBRATING;
				break;
			default:
				TrackingData.Status = ETrackingStatus::NOT_STARTED;
				break;
		}
	}
	else
	{
		TrackingData.Status = ETrackingStatus::NOT_STARTED;
		TrackingData.FrameNumber = GFrameNumberRenderThread;

		const bool bDeviceHadRegistrationProblemLastPoll = DeviceState.bDeviceRegistrationProblem;
		DeviceState.bDeviceRegistrationProblem = false;

		if (Ret == SCE_VR_TRACKER_ERROR_DEVICE_NOT_REGISTERED)
		{
			DeviceState.bDeviceRegistrationProblem = true;

			// If a device we are trying to track is somehow not registered, try to register it now.
			// One way this can happen is if we try to register while the camera is disconnected.
			// The api reports that the registration was successful, but when we attempt to poll the device reports unregistered.
			// Once the camera is connected registration actually works.
			int32 Ret2 = sceVrTrackerRegisterDevice(SceDeviceType, DeviceHandle);
			if (!bDeviceHadRegistrationProblemLastPoll)
			{
				UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerGetResult() failed: 0x%08X SCE_VR_TRACKER_ERROR_DEVICE_NOT_REGISTERED.  This warning will be supressed for this device until the return code changes."), Ret);

				if (Ret2 != SCE_OK)
				{
					UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerRegisterDevice(%d, 0x%x) in PollTrackingData Failed with Result=0x%08X"), SceDeviceType, DeviceHandle, Ret2);
				}
				else
				{
					UE_LOG(LogPS4Tracker, Log, TEXT("sceVrTrackerRegisterDevice(%d, 0x%x)  Succeeded."), SceDeviceType, DeviceHandle);
				}
			}
		}
		else if (Ret == SCE_VR_TRACKER_ERROR_TIMESTAMP_OUT_OF_RANGE)
		{
			// The error 0x8126080C, SCE_VR_TRACKER_ERROR_TIMESTAMP_OUT_OF_RANGE according to sony support threads
			// https://ps4.scedev.net/forums/thread/136832/ this error can result from bad states that older dev hardware gets into
			// or from insufficient tracking information being received from a controller.  I (Jeff Fisher) found that if I hugged
			// my dualshock4 or psvr aim controller at my desk I could induce this error, presumably by obstructing its radio signals.
			// The error would cease when the hug ended.  I expect that tracking quality is degraded while this error is being continually
			// printed, but if it just pops out once or twice now and then, which is what I usually see, it probably will not be noticeable.
			// The inside of a game studio is probably something of a stress test for wireless controller communication.
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerGetResult() returned SCE_VR_TRACKER_ERROR_TIMESTAMP_OUT_OF_RANGE predictionTime=%lu (c %lu + pto %llu) deviceType=%i[hmd,ds4,move,gun] bEarlyPoll=%i  This error can result from a disruption of communication between the controller and the ps4."), Param.predictionTime, CurrentTime, PredictionTimeOffset, SceDeviceType, bEarlyPoll);
		}
		else
		{
			UE_LOG(LogPS4Tracker, Warning, TEXT("sceVrTrackerGetResult() failed: 0x%08X.."), Ret);
		}
	}
}


#elif PLATFORM_WINDOWS

/******************************************************************************
*
*	Windows
*
******************************************************************************/

#include "ThirdParty/PS4/HmdClient/include/hmd_client.h"

static const TCHAR* MorpheusSettings = TEXT("/Script/MorpheusEditor.MorpheusRuntimeSettings");


class FPS4Tracker : public IPS4Tracker, public FSceneViewExtensionBase
{
public:
	FPS4Tracker(const FAutoRegister&);
	virtual ~FPS4Tracker();

	/** IPS4Tracker implementation */
	virtual int32 AcquireTracker(int32 DeviceHandle, int32 ControllerIndex, EDeviceType DeviceType) override;
	virtual void ReleaseTracker(int32 TrackerHandle) override {}
	virtual void Synchronize(int32 TrackerHandle) override {}
	virtual void GetTrackingData(int32 TrackerHandle, FTrackingData& TrackingData, bool bPollImmediately, bool bEarlyPoll) override;
	virtual void SetPredictionTiming(int32 FlipToDisplayLatency, int32 RenderFrameTime, bool bIs60Render120ScanoutMode) override {}
	virtual bool IsCameraConnected() const override { return true; }

    /** ISceneViewExtension interface */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {}
	virtual int32 GetPriority() const { return 10; }

private:
	void*				HMDClientDLLHandle = nullptr;
	hmdclient_handle	HMDClientConnectionHandle = nullptr;
	FQuat PantoscopicTilt;
	FVector EyeOffsets[2];
};

//=============================================================================
FPS4Tracker::FPS4Tracker(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
	FString HMDServerAddress;
	if (GConfig->GetString(MorpheusSettings, TEXT("HMDServerAddress"), HMDServerAddress, GEngineIni))
	{
		FString HMDClientDLLDir = FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries"), TEXT("ThirdParty"), TEXT("PS4"), TEXT("Win64"));
		FString HMDClientDLLPath = FPaths::Combine(*HMDClientDLLDir, TEXT("hmd_client.dll"));
		HMDClientDLLHandle = FPlatformProcess::GetDllHandle(*HMDClientDLLPath);

		if (HMDClientDLLHandle)
		{
			HMDClientConnectionHandle = hmdclientConnect(TCHAR_TO_ANSI(*HMDServerAddress), "7899");

			if (HMDClientConnectionHandle)
			{
				hmdclientRequestServerVersion(HMDClientConnectionHandle);
				hmdclientRequestHMDInfo(HMDClientConnectionHandle);
				hmdclientRequestHMDViewStatus(HMDClientConnectionHandle);

				const double Start = FPlatformTime::Seconds();
				const double TimeoutDuration = 5.0;
				auto Timeout = [&]()
				{
					return (FPlatformTime::Seconds() - Start) > TimeoutDuration;
				};

				HMDServerHMDInfo2000 HMDInfo{};
				HMDServerHMDViewStatus HMDViewStatus{};

				while (!Timeout() && hmdclientGetHMDInfo2000(HMDClientConnectionHandle, &HMDInfo) == HMDCLIENT_ERROR_NOT_READY)
				{
				}
				while (!Timeout() && hmdclientGetHMDViewStatus(HMDClientConnectionHandle, &HMDViewStatus) == HMDCLIENT_ERROR_NOT_READY)
				{
				}

				PantoscopicTilt = ConvertToUnrealCoords({ HMDInfo.deviceInfo.orientation[0], HMDInfo.deviceInfo.orientation[1], HMDInfo.deviceInfo.orientation[2], HMDInfo.deviceInfo.orientation[3] });
				EyeOffsets[0] = ConvertToUnrealCoords({ HMDViewStatus.left.position[0], HMDViewStatus.left.position[1], HMDViewStatus.left.position[2] });
				EyeOffsets[1] = ConvertToUnrealCoords({ HMDViewStatus.right.position[0], HMDViewStatus.right.position[1], HMDViewStatus.right.position[2] });
			}
		}
	}
}

//=============================================================================
FPS4Tracker::~FPS4Tracker()
{
	if (HMDClientConnectionHandle)
	{
		hmdclientDisconnect(HMDClientConnectionHandle);
	}

	if (HMDClientDLLHandle)
	{
		FPlatformProcess::FreeDllHandle(HMDClientDLLHandle);
	}
}

//=============================================================================
int32 FPS4Tracker::AcquireTracker(int32 DeviceHandle, int32 ControllerIndex, EDeviceType DeviceType)
{
	if (DeviceType == EDeviceType::HMD				||
		DeviceType == EDeviceType::MOTION_LEFT_HAND	||
		DeviceType == EDeviceType::PAD				||
		DeviceType == EDeviceType::MOTION_RIGHT_HAND)
	{
		return int32(DeviceType);
	}

	return INVALID_TRACKER_HANDLE;
}

// CVARs to control some undocumented HMD server api settings.
// Prediction time 45000 seemed to be vaguely correct at 60fps.
static int32 GMorpheusPCPredictionTime = 45000.0f;
static FAutoConsoleVariableRef CVarPCMorpheusPredictionTime(
	TEXT("vr.morpheus.PCPredictionTime"),
	GMorpheusPCPredictionTime,
	TEXT("hmdclientRequestChangePredictionTime."),
	ECVF_Default
);
static int32 GMorpheusPCUpdateDelay = 0;
static FAutoConsoleVariableRef CVarPCMorpheusUpdateDelay(
	TEXT("vr.morpheus.PCUpdateDelay"),
	GMorpheusPCUpdateDelay,
	TEXT("hmdclientUpdateDelay."),
	ECVF_Default
);
static int32 GMorpheusPCLatencyOffset = 0;
static FAutoConsoleVariableRef CVarPCMorpheusLatencyOffset(
	TEXT("vr.morpheus.PCLatencyOffset"),
	GMorpheusPCLatencyOffset,
	TEXT("hmdclientLatencyOffset."),
	ECVF_Default
);

//=============================================================================
void FPS4Tracker::GetTrackingData(int32 TrackerHandle, FTrackingData& TrackingData, bool bPollImmediately, bool bEarlyPoll)
{
	// Initialize return values to zero
	FMemory::Memzero(&TrackingData, sizeof(FTrackingData));

	if (!HMDClientConnectionHandle)
	{
		return;
	}
	check(HMDClientDLLHandle);

	static uint32_t PredictionTime = 0;
	if (GMorpheusPCPredictionTime != PredictionTime)
	{
		PredictionTime = GMorpheusPCPredictionTime;
		hmdclientRequestChangePredictionTime(HMDClientConnectionHandle, PredictionTime);
	}

	static uint32_t Delay = 0;
	if (GMorpheusPCUpdateDelay != Delay)
	{
		Delay = GMorpheusPCUpdateDelay;
		hmdclientUpdateDelay(HMDClientConnectionHandle, Delay);
	}

	static uint32_t LatencyOffset = 0;
	if (GMorpheusPCLatencyOffset != LatencyOffset)
	{
		LatencyOffset = GMorpheusPCLatencyOffset;
		hmdclientLatencyOffset(HMDClientConnectionHandle, LatencyOffset);
	}

	HMDServerTackerStates States;
	if (hmdclientGetTrackerStates(HMDClientConnectionHandle, &States) == HMDCLIENT_ERROR_SUCCESS)
	{
		const MoveState& State = States.state[TrackerHandle];
		TrackingData.DevicePosition = FVector(State.pos[0], State.pos[1], State.pos[2]);
		TrackingData.DevicePosition *= 0.001; // HMD server is in mm
		TrackingData.DeviceOrientation = FQuat(State.quat[0], State.quat[1], State.quat[2], State.quat[3]);
		TrackingData.Position = ConvertToUnrealCoords(TrackingData.DevicePosition);
		TrackingData.Orientation = ConvertToUnrealCoords(TrackingData.DeviceOrientation);
		TrackingData.Status = ETrackingStatus::TRACKING;
		
		if (static_cast<EDeviceType>(TrackerHandle) == EDeviceType::HMD)
		{
			TrackingData.UnrealEyePosition[0] = TrackingData.Orientation.RotateVector(EyeOffsets[0]) + TrackingData.Position;
			TrackingData.UnrealEyePosition[1] = TrackingData.Orientation.RotateVector(EyeOffsets[1]) + TrackingData.Position;

			TrackingData.UnrealEyeOrientation[0] = PantoscopicTilt * TrackingData.Orientation;
			TrackingData.UnrealEyeOrientation[1] = PantoscopicTilt * TrackingData.Orientation;
		}
	}
}

#else

/******************************************************************************
*
*	Generic (Non-PS4/Windows)
*
******************************************************************************/

class FPS4Tracker : public IPS4Tracker, public FSceneViewExtensionBase
{
public:
	FPS4Tracker(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}
	virtual ~FPS4Tracker() {}

	/** IPS4Tracker implementation */
	virtual int32 AcquireTracker(int32 DeviceHandle, int32 ControllerIndex, EDeviceType DeviceType) override { return false; };
	virtual void ReleaseTracker(int32 TrackerHandle) override {}
	virtual void Synchronize(int32 TrackerHandle) override {}
	virtual void GetTrackingData(int32 TrackerHandle, FTrackingData& TrackingData, bool bPollImmediately, bool bEarlyPoll) override { FMemory::Memzero(&TrackingData, sizeof(FTrackingData)); }
	virtual void SetPredictionTiming(int32 FlipToDisplayLatency, int32 RenderFrameTime, bool bIs60Render120ScanoutMode) override {}
	virtual bool IsCameraConnected() const override { return false; }

	/** ISceneViewExtension interface */
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {}
	virtual int32 GetPriority() const override { return 10; }
};

#endif



/******************************************************************************
*
*	Module interface
*
******************************************************************************/

class FPS4TrackerModule : public IPS4TrackerModule
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** IPS4TrackerPlugin implementation */
	virtual TSharedPtr< IPS4Tracker, ESPMode::ThreadSafe > GetTrackerInstance() override;

	TSharedPtr< FPS4Tracker, ESPMode::ThreadSafe > Tracker;
};

//=============================================================================
void FPS4TrackerModule::StartupModule()
{
	check(!Tracker.IsValid());

	Tracker = FSceneViewExtensions::NewExtension<FPS4Tracker>();
}

//=============================================================================
void FPS4TrackerModule::ShutdownModule()
{
	check(Tracker.IsValid());
	Tracker.Reset();

	// @todo viewext What if the render thread is still using this ViewExtension?
	//               In this case we are unloading the code.
	//               Should we block here until render thread is done?
	//               This wasn't needed before, I guess...
}

//=============================================================================
TSharedPtr< IPS4Tracker, ESPMode::ThreadSafe > FPS4TrackerModule::GetTrackerInstance()
{
	check(Tracker.IsValid());
	return Tracker;
}

IMPLEMENT_MODULE( FPS4TrackerModule, PS4Tracker )
