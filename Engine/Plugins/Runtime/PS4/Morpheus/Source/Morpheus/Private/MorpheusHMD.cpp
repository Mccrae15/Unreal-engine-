// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MorpheusHMD.h"

#include "Misc/ScopeLock.h"
#include "Misc/CoreDelegates.h"
#include "SceneUtils.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "GeneralProjectSettings.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "GameFramework/PlayerController.h"

#include "IMorpheusPlugin.h"
#include "HeadMountedDisplay.h"
#include "MorpheusTypes.h"

#include "ModuleManager.h"
#include "IInputDevice.h"

#include "SlateApplication.h"
#include "Widgets/SViewport.h"

#if PLATFORM_PS4
#include <hmd_setup_dialog.h>
#include <camera.h>
#endif
#include "ScreenRendering.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#include "WindowsApplication.h"

#include "ThirdParty/PS4/HmdClient/include/hmd_client.h"

namespace
{
	hmdclient_handle HMDClientConnectionHandle = nullptr;
	HMODULE HMDClientDLLHandle = nullptr;
}
#endif

#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

#include "MorpheusMeshAssets.h"

//---------------------------------------------------
// Morpheus Plugin Implementation
//---------------------------------------------------
#if (DUALSHOCK4_SUPPORT && PLATFORM_WINDOWS)
#define USE_DUALSHOCK_MORPHEUS 1
#else
#define USE_DUALSHOCK_MORPHEUS 0
#endif

#if USE_DUALSHOCK_MORPHEUS

#include <pad.h>
#include "PS4/PS4Controllers.h"
#include "HeadMountedDisplayTypes.h"

namespace
{
	// JS - Morpheus support

	// Pad data
	// CellPadData
	const int offsetDigital1 = 2;
	const int offsetDigital2 = 3;
	const int offsetAnalogRightX = 4;
	const int offsetAnalogRightY = 5;
	const int offsetAnalogLeftX = 6;
	const int offsetAnalogLeftY = 7;
	const int offsetPressRight = 8;
	const int offsetPressLeft = 9;
	const int offsetPressUp = 10;
	const int offsetPressDown = 11;
	const int offsetPressTriangle = 12;
	const int offsetPressCircle = 13;
	const int offsetPressCross = 14;
	const int offsetPressSquare = 15;
	const int offsetPressL1 = 16;
	const int offsetPressR1 = 17;
	const int offsetPressL2 = 18;
	const int offsetPressR2 = 19;
	const int offsetSensorX = 20;
	const int offsetSensorY = 21;
	const int offsetSensorZ = 22;
	const int offsetSensorG = 23;

	// Digital1
	const int ctrlLeft = (1 << 7);
	const int ctrlDown = (1 << 6);
	const int ctrlRight = (1 << 5);
	const int ctrlUp = (1 << 4);
	const int ctrlStart = (1 << 3);
	const int ctrlR3 = (1 << 2);
	const int ctrlL3 = (1 << 1);
	const int ctrlSelect = (1 << 0);

	// Digital2
	const int ctrlSquare = (1 << 7);
	const int ctrlCross = (1 << 6);
	const int ctrlCircle = (1 << 5);
	const int ctrlTriangle = (1 << 4);
	const int ctrlR1 = (1 << 3);
	const int ctrlL1 = (1 << 2);
	const int ctrlR2 = (1 << 1);
	const int ctrlL2 = (1 << 0);

	// Gem-only
	const int ctrlTick = (1 << 2);
	const int ctrlTrigger = (1 << 1);
}


class FWinDualShockMorpheus : public IInputDevice
{
	bool Connected;
	HMDServerTackerStates LastTrackerStates;

public:
	FWinDualShockMorpheus(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
		: MessageHandler(InMessageHandler)
	{

	}

	virtual ~FWinDualShockMorpheus()
	{
	}

	virtual void Tick(float DeltaTime) override
	{

		Connected = false;
		if (HMDClientConnectionHandle)
		{
			if (hmdclientGetTrackerStates(HMDClientConnectionHandle, &LastTrackerStates) == HMDCLIENT_ERROR_SUCCESS)
			{
				Connected = true;
			}
		}
	}

	virtual void SendControllerEvents() override
	{
		FPS4Controllers::FControllerState& ControllerState = Controllers.GetControllerState(0);
		ControllerState.PadData.buttons = 0;
		
		ControllerState.PadData.connected = Connected;

		if (Connected)
		{
			auto& Buttons = LastTrackerStates.pad_data[0].button;
			if ((Buttons[offsetDigital1] & ctrlLeft) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_LEFT;
			if ((Buttons[offsetDigital1] & ctrlDown) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_DOWN;
			if ((Buttons[offsetDigital1] & ctrlRight) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_RIGHT;
			if ((Buttons[offsetDigital1] & ctrlUp) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_UP;
			if ((Buttons[offsetDigital1] & ctrlStart) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_OPTIONS;
			if ((Buttons[offsetDigital1] & ctrlR3) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_R3;
			if ((Buttons[offsetDigital1] & ctrlL3) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_L3;
			if ((Buttons[offsetDigital1] & ctrlSelect) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_TOUCH_PAD;



			if ((Buttons[offsetDigital2] & ctrlSquare) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_SQUARE;
			if ((Buttons[offsetDigital2] & ctrlCross) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_CROSS;
			if ((Buttons[offsetDigital2] & ctrlCircle) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_CIRCLE;
			if ((Buttons[offsetDigital2] & ctrlTriangle) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_TRIANGLE;
			if ((Buttons[offsetDigital2] & ctrlR1) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_R1;
			if ((Buttons[offsetDigital2] & ctrlL1) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_L1;
			if ((Buttons[offsetDigital2] & ctrlR2) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_R2;
			if ((Buttons[offsetDigital2] & ctrlL2) != 0)
				ControllerState.PadData.buttons |= SCE_PAD_BUTTON_L2;


			{
				// Thumbsticks
				ControllerState.PadData.leftStick.x = Buttons[offsetAnalogLeftX];
				ControllerState.PadData.leftStick.y = Buttons[offsetAnalogLeftY];
				
				ControllerState.PadData.rightStick.x = Buttons[offsetAnalogRightX];
				ControllerState.PadData.rightStick.y = Buttons[offsetAnalogRightY];
			}
		}
		
		// let PS4Controllers handle everything else.
		Controllers.SendControllerEvents(0, MessageHandler);
	}

	void SetChannelValue (int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
	{
		Controllers.SetForceFeedbackChannelValue(ControllerId, ChannelType, Value);
	}

	void SetChannelValues (int32 ControllerId, const FForceFeedbackValues &Values)
	{
		Controllers.SetForceFeedbackChannelValues(ControllerId, Values);
	}

	virtual void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override
	{
		MessageHandler = InMessageHandler;
	}

	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		return false;
	}

private:
	// handler to send all messages to
	TSharedRef<FGenericApplicationMessageHandler> MessageHandler;

	// the object that encapsulates all the controller logic
	FPS4Controllers Controllers;
};
#endif

class FMorpheusPlugin : public IMorpheusPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > CreateTrackingSystem() override;

	FString GetModuleKeyName() const
	{
		return FString(TEXT("Morpheus"));
	}

	/** IInputDeviceModule implementation */
	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
	{
#if USE_DUALSHOCK_MORPHEUS
		// now we attempt to delay load the DLL by environment variable
		TCHAR SceDir[MAX_PATH];
		FPlatformMisc::GetEnvironmentVariable(TEXT("SCE_ROOT_DIR"), SceDir, MAX_PATH);

		// make path to the DLL
		FString DLLPath = FString::Printf(TEXT("%s\\Common\\External Tools\\Pad Library for Windows(PS4)\\bin\\libScePad_x64.dll"), SceDir);
		void* DLLHandle = FPlatformProcess::GetDllHandle(*DLLPath);

		// if not found in the SDK, look in ThirdParty
		if (DLLHandle == NULL)
		{
			DLLPath = FString::Printf(TEXT("%s\\Binaries\\ThirdParty\\PS4\\Sce\\libScePad_x64.dll"), *FPaths::EngineDir());
			DLLHandle = FPlatformProcess::GetDllHandle(*DLLPath);
		}

		if (DLLHandle != NULL)
		{
			return TSharedPtr< class IInputDevice >(new FWinDualShockMorpheus(InMessageHandler));
		}
#endif		
		return NULL;
	}
};

IMPLEMENT_MODULE( FMorpheusPlugin, Morpheus )

TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FMorpheusPlugin::CreateTrackingSystem()
{
#if MORPHEUS_SUPPORTED_PLATFORMS


#if USE_DUALSHOCK_MORPHEUS
	if (FSlateApplication::IsInitialized())
	{
		TSharedPtr<GenericApplication> GenericApplication = FSlateApplication::Get().GetPlatformApplication();
		FWindowsApplication* WindowsApplication = (FWindowsApplication*)GenericApplication.Get();
		TSharedPtr<IInputDevice> InputDevice = CreateInputDevice(WindowsApplication->GetMessageHandler());
		if (InputDevice.IsValid())
		{
			WindowsApplication->AddExternalInputDevice(InputDevice);
		}
	}
#endif

	TSharedPtr< FMorpheusHMD, ESPMode::ThreadSafe > MorpheusHMD( new FMorpheusHMD() );
	if( MorpheusHMD->IsInitialized() )
	{
		return MorpheusHMD;
	}
#endif

	return nullptr;
}

DEFINE_LOG_CATEGORY_STATIC(LogMorpheusHMD, Log, All);

#if MORPHEUS_SUPPORTED_PLATFORMS


//---------------------------------------------------
// Morpheus HMD Implementation
//---------------------------------------------------

struct FDeviceFieldOfView
{
	float TanOut;
	float TanIn;
	float TanTop;
	float TanBottom;
};

FDeviceFieldOfView DeviceInfoFOV;

#if PLATFORM_PS4
#include "GnmBridge.h"
#include "GnmMemory.h"
#include <gpu_address.h>
#include <libsysmodule.h>
#include <gnm/platform.h>

DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic TrackingRam"), STAT_Garlic_MorpheusRam, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion TrackingRam"), STAT_Onion_MorpheusRam, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );

DEFINE_STAT(STAT_Garlic_MorpheusRam);
DEFINE_STAT(STAT_Onion_MorpheusRam);
#endif


FMorpheusHMD::TextureTranslation::TextureTranslation()
{

}

void FMorpheusHMD::TextureTranslation::SetFromTan(float TanL,float TanR,float TanT,float TanB)
{
	Scale.X  =	1 / (TanL+TanR);
	Offset.X =	TanL / (TanL+TanR);

	Scale.Y  =	1 / (TanT+TanB);
	Offset.Y =	TanT / (TanT+TanB);
}

#if PLATFORM_PS4
FMorpheusHMD::FMorpheusBridge::FMorpheusBridge(FMorpheusHMD* InPlugin)
: FRHICustomPresent(nullptr)
{
	check(InPlugin);
	Plugin = InPlugin;
	bInitialized = true;
	bDoReprojection = false;
	ReprojectionSamplerWrapMode = InPlugin->ReprojectionSamplerWrapMode;

	FMemory::Memset(UnscaledEyeTexture, 0);		
}

struct FRHISetDistortionInputTexture final : public FRHICommand<FRHISetDistortionInputTexture>
{
	FMorpheusHMD::FMorpheusBridge* MorpheusBridge;
	FTexture2DRHIRef DistortionInput;

	FORCEINLINE_DEBUGGABLE FRHISetDistortionInputTexture(FMorpheusHMD::FMorpheusBridge* InMorpheusBridge, const FTexture2DRHIRef& InDistortionInput)
		: MorpheusBridge(InMorpheusBridge)
		, DistortionInput(InDistortionInput)		
	{
	}

	MORPHEUS_API void Execute(FRHICommandListBase& CmdList)
	{
		MorpheusBridge->SetUnscaledEyeTexture(DistortionInput);		
	}
};

struct FRHISetTrackerData final : public FRHICommand<FRHISetTrackerData>
{
	FMorpheusHMD::FMorpheusBridge* MorpheusBridge;	
	IPS4Tracker::FTrackingData TrackingData;

	FORCEINLINE_DEBUGGABLE FRHISetTrackerData(FMorpheusHMD::FMorpheusBridge* InMorpheusBridge, const IPS4Tracker::FTrackingData& InTrackingData)
		: MorpheusBridge(InMorpheusBridge)		
		, TrackingData(InTrackingData)
	{
	}

	MORPHEUS_API void Execute(FRHICommandListBase& CmdList)
	{		
		MorpheusBridge->SetTrackingData(TrackingData);
	}
};

void FMorpheusHMD::FMorpheusBridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI)
{
	check(IsInGameThread());
	check(ViewportRHI);
	
	FTexture2DRHIRef RT = Viewport.GetRenderTargetTexture();	

	// setting the texture must be done on the thread that's executing RHI commandlists.
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		SetUnscaledEyeTexture,
		FMorpheusHMD::FMorpheusBridge*, Bridge, this,
		FTexture2DRHIRef,RT,RT,
		{
		
		if (RHICmdList.Bypass())
		{
			FRHISetDistortionInputTexture Command(Bridge, RT);
			Command.Execute(RHICmdList);			
			return;
		}	
		new (RHICmdList.AllocCommand<FRHISetDistortionInputTexture>()) FRHISetDistortionInputTexture(Bridge, RT);
	});
	

	this->ViewportRHI = ViewportRHI;
	ViewportRHI->SetCustomPresent(this);
}

void FMorpheusHMD::FMorpheusBridge::SetUnscaledEyeTexture(const FTexture2DRHIRef& EyeInput)
{
	UnscaledEyeTexture = EyeInput;
}

void FMorpheusHMD::FMorpheusBridge::SetTrackingData(const IPS4Tracker::FTrackingData& InTrackingData)
{
	SCOPED_NAMED_EVENT_TEXT("FMorpheusBridge::SetTrackingData", FColor::Turquoise);
	RenderedTrackingData = InTrackingData;
}

void FMorpheusHMD::FMorpheusBridge::SetDoReprojection(bool bInDoReprojection)
{
	bDoReprojection = bInDoReprojection;
}

void FMorpheusHMD::FMorpheusBridge::Reset()
{
}

void FMorpheusHMD::FMorpheusBridge::Init()
{
}

void FMorpheusHMD::FMorpheusBridge::OnBackBufferResize()
{
}

bool FMorpheusHMD::FMorpheusBridge::Present(int& SyncInterval)
{	
	
	check(IsInRenderingThread() || IsInRHIThread());	
	{		
		const IPS4Tracker::FTrackingData& TrackingData = RenderedTrackingData;

		GnmBridge::MorpheusDistortionData DistortionData(
			UnscaledEyeTexture.GetReference(), 
			UnscaledEyeTexture.GetReference(),
			Plugin->OverlayLayerRenderTargetWritten[0][Plugin->StereoLayerBufferIndex] ? Plugin->OverlayLayerRenderTarget[0][Plugin->StereoLayerBufferIndex].GetReference() : nullptr,
			Plugin->OverlayLayerRenderTargetWritten[1][Plugin->StereoLayerBufferIndex] ? Plugin->OverlayLayerRenderTarget[1][Plugin->StereoLayerBufferIndex].GetReference() : nullptr,
			TrackingData.TimeStamp, 
			TrackingData.SensorReadSystemTimestamp, 
			TrackingData.FrameNumber,
			{ TrackingData.DevicePosition.X, TrackingData.DevicePosition.Y, TrackingData.DevicePosition.Z }, 
			{ TrackingData.DeviceOrientation.X, TrackingData.DeviceOrientation.Y, TrackingData.DeviceOrientation.Z, TrackingData.DeviceOrientation.W },
			DeviceInfoFOV.TanIn,
			DeviceInfoFOV.TanOut,
			DeviceInfoFOV.TanTop,
			DeviceInfoFOV.TanBottom,
			Plugin->GetHMDHandle());

		GnmBridge::CacheReprojectionData(DistortionData);

		Plugin->StereoLayerBufferIndex = (Plugin->StereoLayerBufferIndex + 1) % STEREO_LAYER_BUFFER_COUNT;

	}
	return true;
}
#endif

FMorpheusHMD::FMorpheusHMD()
	: bIsHMDAvailable(false)
	, bWasInitialized(false)
	, bEnabled(false)
	, bEnableMorpheusOnPC(false)
	, bHMDSetupDialogActive(false)
	, HMDWornState(EHMDWornState::Unknown)
	, bStereoEnabled(false)
	, bStereoShuttingDown(false)
	, HmdSetupDialogCanceledBehavior(EHmdSetupDialogCanceledBehavior::DisallowCancel)
	, bSuppressHMDSetupDialogAfterCancel(false)
	, bOverrideStereo(false)
	, bEnableSocialScreenSeparateMode(false)
	, bPositionTrackingEnabled(true)
	, bDisableHMDOrientationUntilHMDHasBeenTracked(true)
	, bControllerPitchEnabled(false)
	, bControllerRollEnabled(false)
	, BaseHmdOrientation(FQuat::Identity)
	, BaseHmdPosition(FVector::ZeroVector)	
	, DeltaControlRotation(FRotator::ZeroRotator)
	, PlayerPosition(FVector::ZeroVector)
	, WorldToMeterScale(100.0f)
#if PLATFORM_PS4
	, PSVROutputMode(EPS4OutputMode::MorpheusRender60Scanout120)
	, ReprojectionSamplerWrapMode(sce::Gnm::WrapMode::kWrapModeMirror)
	, HMDHandle(INVALID_HMD_HANDLE)
#endif
	, TrackerHandle(IPS4Tracker::INVALID_TRACKER_HANDLE)
	, bRecenterView(true)
	, bReorientView(true)
	, bCurrentTrackerStateValid(false)
	, bTrackerStateHasBeenRead(false)
	, PixelDensity(1.0f)
	, ConsoleCommands(this)
{	
	RecenterExtraYaw = 0.0f;

	IPS4Tracker::FTrackingData::DefaultInitialize(LastTrackingData);

	Startup();

	FMemory::Memzero(OverlayLayerRenderTarget);

	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
}


FMorpheusHMD::~FMorpheusHMD()
{
	Shutdown();
}

const FName FMorpheusHMD::MorpheusSystemName(TEXT("PSVR"));

#if PLATFORM_PS4
bool FMorpheusHMD::GetHMDDeviceInformation(SceHmdDeviceInformation& Info) const
{
	FMemory::Memzero(Info);

	int32 Ret = SCE_OK;
	if (HMDHandle != INVALID_HMD_HANDLE)
	{
		Ret = sceHmdGetDeviceInformationByHandle(HMDHandle, &Info);
	}
	else
	{
		Ret = sceHmdGetDeviceInformation(&Info);
	}

	if (Ret == SCE_OK)
	{
		return true;
	}
	else
	{
		UE_LOG(LogMorpheusHMD, Warning, TEXT("sceHmdGetDeviceInformation failed with code: 0x%08X"), Ret);
		return false;
	}
}
#endif

int32 FMorpheusHMD::GetHMDDeviceStatus() const
{
#if PLATFORM_PS4
	SceHmdDeviceInformation	DeviceInfo;
	if (GetHMDDeviceInformation(DeviceInfo))
	{
		return DeviceInfo.status;
	}
	else
	{
		return -1;
	}
#else
	return -1;
#endif
}

bool FMorpheusHMD::OnStartGameFrame(FWorldContext& InWorldContext)
{
	if (!bEnabled)
		return false;

	RefreshTrackingToWorldTransform(InWorldContext);

	SCOPED_NAMED_EVENT_TEXT("FMorpheusHMD::OnStartGameFrame()", FColor::Turquoise);

	{
		FVector CurHmdPosition;
		FQuat CurHmdOrientation;
		FetchTrackingData(GameThreadFrame, CurHmdOrientation, CurHmdPosition, false, true, true);
	}

#if PLATFORM_PS4

	if (bHMDSetupDialogActive)
	{
		UpdateHMDSetupDialog();
	}
	else
	{
		EHMDWornState::Type NewHMDWornState = EHMDWornState::Unknown;
		uint32_t DeviceStatus = -1;
		SceHmdDeviceInformation	DeviceInfo;

		bIsHMDAvailable = GetHMDDeviceInformation(DeviceInfo) && DeviceInfo.status == SCE_HMD_DEVICE_STATUS_READY;

		if (bIsHMDAvailable)
		{
			DeviceStatus = DeviceInfo.status;
			NewHMDWornState = DeviceInfo.hmuMount == SCE_HMD_HMU_MOUNT ? EHMDWornState::Worn : EHMDWornState::NotWorn;
		}

		if (NewHMDWornState != HMDWornState)
		{
			HMDWornState = NewHMDWornState;
			if (HMDWornState == EHMDWornState::Worn)
			{
				FCoreDelegates::VRHeadsetPutOnHead.Broadcast();
			}
			else if (HMDWornState == EHMDWornState::NotWorn)
			{
				FCoreDelegates::VRHeadsetRemovedFromHead.Broadcast();
			}
		}

		if (bStereoEnabled)
		{
			if (bIsHMDAvailable)
			{
				if (HMDHandle < 0)
				{
					bool OpenSucceeded = OpenHMD();
					if (OpenSucceeded)
					{
						UE_LOG(LogMorpheusHMD, Log, TEXT("MorpheusHMD Reconnected."));
						FCoreDelegates::VRHeadsetReconnected.Broadcast();
					}
					else
					{
						UE_LOG(LogMorpheusHMD, Log, TEXT("MorpheusHMD Reconnect failed to open hmd.  Maybe it was just disconnected?  Will try again next frame or pop up the setup dialog again."));
					}
					bSuppressHMDSetupDialogAfterCancel = false;
				}
			}
			else
			{
				if (HMDHandle >= 0)
				{
					UE_LOG(LogMorpheusHMD, Log, TEXT("MorpheusHMD Lost. SceHmdDeviceStatus is %i"), DeviceStatus);
					CloseHmd();
					FCoreDelegates::VRHeadsetLost.Broadcast();
				}

				// DelegateDefined cancel behavior means that something else (eg blueprints) must handle the problem if the user
				// cancels the dialog, so we want it to go up the first time, but not subsequent times.
				if (!bSuppressHMDSetupDialogAfterCancel && !sceCommonDialogIsUsed())
				{
					StartHMDSetupDialog();
				}
			}
		}	
	}
#else
	if (bStereoEnabled && TrackerHandle == IPS4Tracker::INVALID_TRACKER_HANDLE)
	{
		OpenHMD();
	}
#endif
	return false;
}

void FMorpheusHMD::BlockingHMDSetupDialog()
{
#if PLATFORM_PS4
	while (!StartHMDSetupDialog())
	{ }

	while (!UpdateHMDSetupDialog())
	{
		sceKernelUsleep(5);
	}
#endif
}

bool FMorpheusHMD::StartHMDSetupDialog()
{
#if PLATFORM_PS4
	UE_LOG(LogMorpheusHMD, Log, TEXT("StartHMDSetupDialog()"));

	check(!bHMDSetupDialogActive);

	bHMDSetupDialogActive = true;

	int32 Ret;

	Ret = sceHmdSetupDialogInitialize();
	if (Ret != SCE_OK)
	{
		UE_LOG(LogMorpheusHMD, Warning, TEXT("sceHmdSetupDialogInitialize failed: 0x%08X"), Ret);
		EndHMDSetupDialog();
		return false;
	}

	SceHmdSetupDialogParam param;
	sceHmdSetupDialogParamInitialize(&param);
	Ret = sceUserServiceGetInitialUser(&param.userId);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogMorpheusHMD, Warning, TEXT("sceUserServiceGetInitialUser failed: 0x%08X"), Ret);
		EndHMDSetupDialog();
		return false;
	}

	Ret = sceHmdSetupDialogOpen(&param);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogMorpheusHMD, Warning, TEXT("sceHmdSetupDialogOpen failed: 0x%08X"), Ret);
		EndHMDSetupDialog();
		return false;
	}

	HMDSetupSocialScreenOverrideReceipt = AcquireSocialScreenOverrideReceipt();
#endif

	return true;
}

// Return true if we are done handling the hmdSetupDialog.
bool FMorpheusHMD::UpdateHMDSetupDialog()
{
#if PLATFORM_PS4
	//UE_LOG(LogMorpheusHMD, Log, TEXT("UpdateHMDSetupDialog()")); // This one spams, so off by default.

	check(bHMDSetupDialogActive);

	SceCommonDialogStatus Status;

	Status = sceHmdSetupDialogUpdateStatus();
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
			// this is how you cancel the dialog programatically
			if (!bStereoEnabled) 
			{
				UE_LOG(LogMorpheusHMD, Log, TEXT("UpdateHMDSetupDialog() forcing the dialog cancel because !bStereoEnabled"));
				
				if (HmdSetupDialogCanceledBehavior == EHmdSetupDialogCanceledBehavior::DisallowCancel)
				{
					UE_LOG(LogMorpheusHMD, Warning, TEXT("UpdateHMDSetupDialog() forcing the dialog cancel because !bStereoEnabled, but we are in 'DisallowCancel' hmdSetupDialog mode (per project setting).  Something is wrong."));
				}
				
				sceHmdSetupDialogClose();
				return true;
			}

			if (GIsRequestingExit)
			{
				UE_LOG(LogMorpheusHMD, Log, TEXT("UpdateHMDSetupDialog() terminating because GIsRequestingExit"));
				EndHMDSetupDialog();
				return true;
			}
		}
		return false;
	case SCE_COMMON_DIALOG_STATUS_FINISHED:
		{
			UE_LOG(LogMorpheusHMD, Log, TEXT("UpdateHMDSetupDialog() SCE_COMMON_DIALOG_STATUS_FINISHED, checking result"));

			SceHmdSetupDialogResult dialogResult;
			memset(&dialogResult, 0, sizeof(SceHmdSetupDialogResult));
			int32 Ret = sceHmdSetupDialogGetResult(&dialogResult);
			if (Ret != SCE_OK)
			{
				UE_LOG(LogMorpheusHMD, Warning, TEXT("sceHmdSetupDialogGetResult failed: 0x%08X\n"), Ret);
				EndHMDSetupDialog();
				return false;
			}
			if (dialogResult.result != SCE_COMMON_DIALOG_RESULT_OK)
			{
				if (dialogResult.result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED)
				{
					UE_LOG(LogMorpheusHMD, Log, TEXT("HMDSetupDialog canceled."), dialogResult.result);
				}
				else
				{
					UE_LOG(LogMorpheusHMD, Warning, TEXT("sceHmdSetupDialogGetResult() gave unexpected result: 0x%x  Treating it like cancel."), dialogResult.result);
				}
				EndHMDSetupDialog();

				switch (HmdSetupDialogCanceledBehavior)
				{
				case EHmdSetupDialogCanceledBehavior::DisallowCancel:
					UE_LOG(LogMorpheusHMD, Log, TEXT("UpdateHMDSetupDialog() HMDSetupDialog was canceled. Project does not allow that. HMDSetupDialog will be put back up. HMDConnectCanceledDelegate will be broadcast."));
					StartHMDSetupDialog();
					FCoreDelegates::VRHeadsetConnectCanceled.Broadcast();
					check(bHMDSetupDialogActive);
					return false;
				case EHmdSetupDialogCanceledBehavior::SwitchTo2D:
					UE_LOG(LogMorpheusHMD, Log, TEXT("UpdateHMDSetupDialog() HMDSetupDialog was canceled. Project is set to switch to 2D mode. HMDConnectCanceledDelegate will be broadcast."));
					EnableStereo(false);
					FCoreDelegates::VRHeadsetConnectCanceled.Broadcast();
					return true;
				case EHmdSetupDialogCanceledBehavior::DelegateDefined:
					UE_LOG(LogMorpheusHMD, Log, TEXT("UpdateHMDSetupDialog() HMDSetupDialog was canceled. Project is set to only broacast the HMDConnectCanceledDelegate. We will stay in VR mode, but the HMD is off. Handle the delegate to avoid TRC violations."));
					FCoreDelegates::VRHeadsetConnectCanceled.Broadcast();
					bSuppressHMDSetupDialogAfterCancel = true;
					return true;
				default:
					check(false);
				}
			}
			else
			{
				UE_LOG(LogMorpheusHMD, Log, TEXT("UpdateHMDSetupDialog() dialogResult.result == SCE_COMMON_DIALOG_RESULT_OK"));
				EndHMDSetupDialog();
				return true;
			}

		}
		check(false); // should have returned.
		return true;
	default:
		check(false); // unknown status value
		return true;
	}
#else
	return true;
#endif
}

void FMorpheusHMD::EndHMDSetupDialog()
{
#if PLATFORM_PS4
	check(bHMDSetupDialogActive);

	sceHmdSetupDialogTerminate();

	bHMDSetupDialogActive = false;

	HMDSetupSocialScreenOverrideReceipt = nullptr;
#endif
}


bool FMorpheusHMD::IsHMDEnabled() const
{
	return bEnabled;
}

bool FMorpheusHMD::IsHMDConnected()
{ 
	return IsHMDEnabled() && bIsHMDAvailable;
}

EHMDWornState::Type FMorpheusHMD::GetHMDWornState()
{
	return HMDWornState;
}

void FMorpheusHMD::EnableHMD(bool bEnable)
{
#if PLATFORM_PS4
	bEnabled = bEnable;
#else
	bEnabled = bEnable && bEnableMorpheusOnPC;
#endif
	if( !bEnabled )
	{
		EnableStereo(false);
	}
}

void DumpTextureAsDataBlob(void* BaseAddress, uint32 SizeInBytes)
{
#if PLATFORM_PS4
	check(SizeInBytes % 4 == 0);
	uint32 Offset = 0;
	uint32* BaseData = (uint32*)BaseAddress;

	FPlatformMisc::LowLevelOutputDebugString(TEXT("TextureBlob\n\n"));
	FILE* TextureFile = fopen("/hostapp/texture.dat", "w");	
	fwrite(BaseAddress, 1, SizeInBytes, TextureFile);	
	fclose(TextureFile);
	FPlatformMisc::LowLevelOutputDebugString(TEXT("EndTextureBlob\n\n"));
#endif
}

void FMorpheusHMD::Startup()
{
	static const size_t RESOURCE_ALIGN = 2U * 1024U * 1024U;

	FMemory::Memset(TrackerUpdateTime, 0);

	// default parameters	
	MorpheusDeviceInfo.DesktopWindowPosX = 0;
	MorpheusDeviceInfo.DesktopWindowPosY = 0;
	MorpheusDeviceInfo.HResolution = 1920;
	MorpheusDeviceInfo.VResolution = 1080;
	MorpheusDeviceInfo.HScreenSize = 0.110016f;			// actual Morpheus panel size, in meters
	MorpheusDeviceInfo.VScreenSize = 0.061884f;
	MorpheusDeviceInfo.InterpupillaryDistance = 0.0635f; // in meters

	// which display device to use
	MorpheusOutputDeviceEnum[0] = 3;
	MorpheusOutputDeviceEnum[1] = 0;

	HudOffset = 0.25f * MorpheusDeviceInfo.InterpupillaryDistance * (MorpheusDeviceInfo.HResolution / MorpheusDeviceInfo.HScreenSize) / 15.0f;
	CanvasCenterOffset = 0.0f;

	const float IdealPixelDensity = 1.4f; // Per the psvr documentation
	IdealRenderTargetSize.X = FMath::CeilToInt(MorpheusDeviceInfo.HResolution * IdealPixelDensity);
	IdealRenderTargetSize.Y = FMath::CeilToInt(MorpheusDeviceInfo.VResolution * IdealPixelDensity);

	static const auto PixelDensityCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("vr.PixelDensity"));
	if (PixelDensityCVar)
	{
		PixelDensity = FMath::Clamp(PixelDensityCVar->GetFloat(), PixelDensityMin, PixelDensityMax);
	}

	// Values reported from a Morpheus PVT Hmd.
	// These are used if you go into stereo rendering, but do not have and never have had an HMD connected.
	DeviceInfoFOV.TanOut    = 1.20743024f;
	DeviceInfoFOV.TanIn     = 1.18134594f;
	DeviceInfoFOV.TanTop    = 1.26287246f;
	DeviceInfoFOV.TanBottom = 1.26287246f;
	LeftTranslation.SetFromTan(DeviceInfoFOV.TanOut, DeviceInfoFOV.TanIn, DeviceInfoFOV.TanTop, DeviceInfoFOV.TanBottom);
	RightTranslation.SetFromTan(DeviceInfoFOV.TanIn, DeviceInfoFOV.TanOut, DeviceInfoFOV.TanTop, DeviceInfoFOV.TanBottom);

	BaseHmdOrientation = FQuat::Identity;
	BaseHmdPosition = FVector::ZeroVector;

	if (!HasHiddenAreaMesh())
	{
		SetupOcclusionMeshes();
	}

	LoadFromIni();
	const bool bStartInVR = FParse::Param(FCommandLine::Get(), TEXT("vr")) || GetDefault<UGeneralProjectSettings>()->bStartInVR;

#if PLATFORM_PS4
	// Always enable on the PS4, so we have the option to drop in whenever we want
	bEnabled = true;
#endif // PLATFORM_PS4

	if (bEnabled)
	{
#if PLATFORM_PS4
		int32 Ret = SCE_OK;
		Ret = sceSysmoduleLoadModule(SCE_SYSMODULE_HMD);
		checkf(Ret >= 0, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_HMD) failed: 0x%x"), Ret);

		Ret = sceSysmoduleLoadModule(SCE_SYSMODULE_HMD_SETUP_DIALOG);
		if (Ret != SCE_OK)
		{
			checkf(Ret == SCE_OK, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_HMD_SETUP_DIALOG) failed: 0x%x"), Ret);
			return;
		}

		Ret = sceCommonDialogInitialize();
		if (Ret != SCE_OK && Ret != SCE_COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED)
		{
			UE_LOG(LogMorpheusHMD, Error, TEXT("sceCommonDialogInitialize() failed. Error code 0x%x The game will probably not be able to put up system dialogs."), Ret);
		}

		// Initializes the Hmd library
		SceHmdInitializeParam param = {};
		Ret = sceHmdInitialize(&param);
		checkf(Ret == SCE_OK, TEXT("sceHmdInitialize failed: 0x%x"), Ret);

		// Initialize the user service library, we use it to get the initial user when displaying the hmd setup dialog
		Ret = sceUserServiceInitialize(nullptr);
		if (Ret != SCE_OK && Ret != SCE_USER_SERVICE_ERROR_ALREADY_INITIALIZED)
		{
			UE_LOG(LogMorpheusHMD, Error, TEXT("sceUserServiceInitialize() failed. Error code 0x%x The game will probably not be able to put up the hmd setup dialogs."), Ret);
		}

		MorpheusBridge = new FMorpheusBridge(this);

		if (bStartInVR)
		{
			EnableStereo(true);

			BlockingHMDSetupDialog();

			const int32 DeviceStatus = GetHMDDeviceStatus();
			bIsHMDAvailable = DeviceStatus == SCE_HMD_DEVICE_STATUS_READY;

			if (!bIsHMDAvailable && HmdSetupDialogCanceledBehavior == EHmdSetupDialogCanceledBehavior::DisallowCancel)
			{
				UE_LOG(LogMorpheusHMD, Warning, TEXT("MorpheusHMD BlockingHMDSetupDialog did not exit with success, but cancel is not allowed.  Setup presumably failed."));
			}

			if (bIsHMDAvailable)
			{
				if (!OpenHMD())
				{
					UE_LOG(LogMorpheusHMD, Warning, TEXT("MorpheusHMD OpenHMD failed during startup, after the HMDSetupDialog check.  Perhaps it was disconnected during startup?  Perhaps the user canceled out of the setup?  This will be treated like a disconnection."));
					FCoreDelegates::VRHeadsetLost.Broadcast();
				}
			}
		}

		SocialScreenStartup();

#else
		UpdateTrackerPredictionTiming(false);
		
		bIsHMDAvailable = true;

		if (bStartInVR)
		{
			EnableStereo(true);
		}

		// enable vsync
		IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
		if (CVSyncVar)
		{
			CVSyncVar->Set(true);
		}
#endif // PLATFORM_PS4
	}

	bWasInitialized = true;
}

bool FMorpheusHMD::OpenHMD()
{
#if PLATFORM_PS4
	SceHmdDeviceInformation	DeviceInfo;
	FMemory::Memzero(DeviceInfo);

	int32 Ret = sceHmdGetDeviceInformation(&DeviceInfo);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogMorpheusHMD, Warning, TEXT("sceHmdGetDeviceInformation failed code 0x%x.\n"), Ret);
		return false;
	}

	if (DeviceInfo.status != SCE_HMD_DEVICE_STATUS_READY)
	{
		UE_LOG(LogMorpheusHMD, Warning, TEXT("sceHmdGetDeviceInformation found that the HMD is not ready 0x%x. Is it plugged in via USB and powered on?\n"), Ret);
		return false;
	}

	MorpheusDeviceInfo.HResolution = DeviceInfo.deviceInfo.panelResolution.width;
	MorpheusDeviceInfo.VResolution = DeviceInfo.deviceInfo.panelResolution.height;

	/*
	HMDResource.garlicMemory = FMemBlock::Allocate(ResourceInfo.garlicMemorySize, RESOURCE_ALIGN, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_MorpheusRam)).GetPointer();

	// needs large alignment, so go through FMemBlock
	HMDResource.onionMemory = FMemBlock::Allocate(ResourceInfo.onionMemorySize, RESOURCE_ALIGN, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_MorpheusRam)).GetPointer();
	HMDResource.garlicMemorySize	= ResourceInfo.garlicMemorySize;
	HMDResource.onionMemorySize		= ResourceInfo.onionMemorySize;

	check(HMDResource.garlicMemory != nullptr);
	check(HMDResource.onionMemory != nullptr);
	*/

	// open the device
	HMDHandle = sceHmdOpen(DeviceInfo.userId, 0, 0, nullptr);
	checkf(HMDHandle >= 0, TEXT("sceHmdOpen failed: %i"), HMDHandle);

	Ret = sceHmdGet2DEyeOffset(HMDHandle, &EyeStatus[kStereoEyeLeft], &EyeStatus[kStereoEyeRight]);
	checkf(Ret == SCE_OK, TEXT("sceHmdGetEyeStatus: %i"), Ret);

	/*
	// initialize the distortion textures.
	sce::Gnm::Texture       TextureL, TextureR;
	sce::Gnm::Sampler       Sampler;		

	sceHmdGetDistortionMap(HMDHandle, &TextureL, &TextureR, &Sampler);		

	uint64_t TextureSize;
	sce::Gnm::AlignmentType TextureAlignment;
	Result = sce::GpuAddress::computeTotalTiledTextureSize(&TextureSize, &TextureAlignment, &TextureL);
	check(Result == SCE_OK);

	check(TextureL.getDataFormat().getSurfaceFormat() == sce::Gnm::kSurfaceFormat16_16);
	check(TextureL.getDataFormat().getTextureChannelType() == sce::Gnm::kTextureChannelTypeFloat);		
	check(TextureL.getWidth() == FMorpheusDistortionTexture::Width);
	check(TextureL.getHeight() == FMorpheusDistortionTexture::Height);
	check(TextureL.getLastMipLevel() + 1 == FMorpheusDistortionTexture::NumMips);
	check(TextureL.getDepth() == FMorpheusDistortionTexture::NumSlices);

	LeftDistortionTextureArray = new FMorpheusDistortionTexture(TextureL.getBaseAddress(), TextureSize);
	RightDistortionTextureArray = new FMorpheusDistortionTexture(TextureR.getBaseAddress(), TextureSize);
	*/

	//DumpTextureAsDataBlob(TextureL.getBaseAddress(), TextureSize);
	//DumpTextureAsDataBlob(TextureR.getBaseAddress(), TextureSize);

	// initialize tracking
	TrackerHandle = PS4Tracker->AcquireTracker(HMDHandle, 0, IPS4Tracker::EDeviceType::HMD);			
	UpdateTrackerPredictionTiming(bStereoEnabled);

	// Get FOV parameters
	SceHmdFieldOfView HMDDeviceFov;
	Ret = sceHmdGetFieldOfView(HMDHandle, &HMDDeviceFov);
	checkf(Ret == SCE_OK, TEXT("sceHmdGetFieldOfView: %i"), Ret);

	DeviceInfoFOV.TanOut	= HMDDeviceFov.tanOut;
	DeviceInfoFOV.TanIn		= HMDDeviceFov.tanIn;
	DeviceInfoFOV.TanTop	= HMDDeviceFov.tanTop;
	DeviceInfoFOV.TanBottom = HMDDeviceFov.tanBottom;	

	LeftTranslation.SetFromTan(DeviceInfoFOV.TanOut, DeviceInfoFOV.TanIn, DeviceInfoFOV.TanTop, DeviceInfoFOV.TanBottom);
	RightTranslation.SetFromTan(DeviceInfoFOV.TanIn, DeviceInfoFOV.TanOut, DeviceInfoFOV.TanTop, DeviceInfoFOV.TanBottom);


#else

	FString HMDClientDLLDir = FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries"), TEXT("ThirdParty"), TEXT("PS4"), TEXT("Win64"));
	FString HMDClientDLLPath = FPaths::Combine(*HMDClientDLLDir, TEXT("hmd_client.dll"));
	HMDClientDLLHandle = LoadLibraryW(*HMDClientDLLPath);
	if (HMDClientDLLHandle)
	{
		const double Timeout = 5.0;
		const double StartTime = FPlatformTime::Seconds();
		const auto& ip = MorpheusDeviceInfo.HMDServerAddress;
		if (!ip.IsEmpty())
		{
			HMDClientConnectionHandle = hmdclientConnect(TCHAR_TO_ANSI(*ip), "7899");
			if (HMDClientConnectionHandle)
			{
				hmdclientRequestServerVersion(HMDClientConnectionHandle);
				hmdclientRequestHMDInfo(HMDClientConnectionHandle);
				hmdclientRequestHMDDistortionParams(HMDClientConnectionHandle);

				HMDServerHMDInfo2000 deviceInfo;
				while (hmdclientGetHMDInfo2000(HMDClientConnectionHandle, &deviceInfo) == HMDCLIENT_ERROR_NOT_READY)
				{
					if ((FPlatformTime::Seconds() - StartTime) > Timeout)
					{
						hmdclientDisconnect(HMDClientConnectionHandle);
						HMDClientConnectionHandle = nullptr;
						break;
					}
				}

				if (HMDClientConnectionHandle)
				{
					DeviceInfoFOV.TanOut = deviceInfo.deviceInfo.fieldOfView.tanOut;
					DeviceInfoFOV.TanIn = deviceInfo.deviceInfo.fieldOfView.tanIn;
					DeviceInfoFOV.TanBottom = deviceInfo.deviceInfo.fieldOfView.tanBottom;
					DeviceInfoFOV.TanTop = deviceInfo.deviceInfo.fieldOfView.tanTop;
					
					LeftTranslation.SetFromTan(DeviceInfoFOV.TanOut, DeviceInfoFOV.TanIn, DeviceInfoFOV.TanTop, DeviceInfoFOV.TanBottom);
					RightTranslation.SetFromTan(DeviceInfoFOV.TanIn, DeviceInfoFOV.TanOut, DeviceInfoFOV.TanTop, DeviceInfoFOV.TanBottom);
					
					while (hmdclientGetHMDDistortionParams(HMDClientConnectionHandle, &HMDDistortionParams) == HMDCLIENT_ERROR_NOT_READY)
					{
						if ((FPlatformTime::Seconds() - StartTime) > Timeout)
						{
							hmdclientDisconnect(HMDClientConnectionHandle);
							HMDClientConnectionHandle = nullptr;
							break;
						}
					}
				}
			}
			else
			{
				MessageBox(nullptr, TEXT("Failed to connect to HMD Server."), TEXT("Error"), MB_ICONEXCLAMATION | MB_OK);
			}
		}
	}

	// initialize tracking
	TrackerHandle = PS4Tracker->AcquireTracker(0, 0, IPS4Tracker::EDeviceType::HMD);
	UpdateTrackerPredictionTiming(bStereoEnabled);

#endif // PLATFORM_PS4

	return true;
}

void FMorpheusHMD::CloseHmd()
{
#if PLATFORM_PS4
	PS4Tracker->ReleaseTracker(TrackerHandle);
	TrackerHandle = IPS4Tracker::INVALID_TRACKER_HANDLE;
	LastTrackingData.Status = IPS4Tracker::ETrackingStatus::NOT_STARTED;

	sceHmdClose(HMDHandle);
	HMDHandle = INVALID_HMD_HANDLE;
#endif
}

void FMorpheusHMD::DestroyHmd()
{
#if PLATFORM_PS4
	CloseHmd();
	sceHmdTerminate();

	//getAllocator().release(m_hmd.m_distortionWorkMemoryAddress);

#endif // PLATFORM_PS4
}

void FMorpheusHMD::UpdateTrackerPredictionTiming(bool bReprojectionEnabled)
{
	if (TrackerHandle == IPS4Tracker::INVALID_TRACKER_HANDLE)
	{
		return;
	}

	int32 FlipToDisplayLatency = 60; // microseconds
	int32 RenderFrameTime = 16666; // 60Hz = 16.666ms
	bool bIs60Render120ScanoutMode = false;
	int32 CameraProcessStartTime = 0;
	int32 UpdateMotionSensorDataStartTime = -1; // -1 means don't do it.
#if PLATFORM_PS4
	if (bReprojectionEnabled)
	{
		SceHmdDeviceInformation HmdInfo;
		int ret = sceHmdGetDeviceInformation(&HmdInfo);
		switch (PSVROutputMode)
		{
		case EPS4OutputMode::MorpheusRender60Scanout120:
			FlipToDisplayLatency = HmdInfo.deviceInfo.flipToDisplayLatency.refreshRate120Hz;
			RenderFrameTime = 16666llu; // 60Hz = 16.666ms
			bIs60Render120ScanoutMode = true;
			CameraProcessStartTime = 2000;
			UpdateMotionSensorDataStartTime = 13000;
			break;
		case EPS4OutputMode::MorpheusRender90Scanout90:
			FlipToDisplayLatency = HmdInfo.deviceInfo.flipToDisplayLatency.refreshRate90Hz;
			RenderFrameTime = 11111llu; // 90Hz = 11.111ms
			break;
		case EPS4OutputMode::MorpheusRender120Scanout120:
			FlipToDisplayLatency = HmdInfo.deviceInfo.flipToDisplayLatency.refreshRate120Hz;
			RenderFrameTime = 8333llu; // 120Hz = 8.333ms	
			break;
		default:
			check(false);
			break;
		}
	}
#endif
	PS4Tracker->SetPredictionTiming(FlipToDisplayLatency, RenderFrameTime, bIs60Render120ScanoutMode, CameraProcessStartTime, UpdateMotionSensorDataStartTime);
}

const int32 FMorpheusHMD::CameraDeviceId;

// override defaults with ini values. Can be modified in the editor.
static const TCHAR* MorpheusSettings = TEXT("/Script/MorpheusEditor.MorpheusRuntimeSettings");

void FMorpheusHMD::LoadFromIni()
{	
	float f;
	FVector v;
	FVector4 v4;
	FString s;
	bool b;

	// can't rely on the MoveMe connection because the connect function returns 0 for success and failure.  Plus,
	// if you're NOT running the moveme server the timeout is quite long.  so only attempt it if asked.
	if (GConfig->GetBool(MorpheusSettings, TEXT("bEnableMorpheus"), b, GEngineIni))
	{
		bEnabled = b;
		bEnableMorpheusOnPC = b;
	}		

	if (GConfig->GetFloat(MorpheusSettings, TEXT("IPD"), f, GEngineIni))
	{
		MorpheusDeviceInfo.InterpupillaryDistance = f;
	}

	if (GConfig->GetString(MorpheusSettings, TEXT("PSVRFrameSequence"), s, GEngineIni))
	{
		static const UEnum* Enum = CastChecked<UEnum>(StaticLoadObject(UEnum::StaticClass(), NULL, TEXT("/Script/Morpheus.EPSVRFrameSequence")));
		const int64 EnumValue = Enum->GetValueByName(FName(*s));
		if (EnumValue != INDEX_NONE)
		{
#if PLATFORM_PS4
			const EPSVRFrameSequence PSVRFrameSequence = static_cast<EPSVRFrameSequence>(EnumValue);
			switch (PSVRFrameSequence)
			{
			case EPSVRFrameSequence::Render60Scanout120:
				PSVROutputMode = EPS4OutputMode::MorpheusRender60Scanout120;
				break;
			case EPSVRFrameSequence::Render90Scanout90:
				PSVROutputMode = EPS4OutputMode::MorpheusRender90Scanout90;
				break;
			case EPSVRFrameSequence::Render120Scanout120:
				PSVROutputMode = EPS4OutputMode::MorpheusRender120Scanout120;
				break;
			default:
				check(false);
				break;
			}
#endif
		}
		else
		{
			UE_LOG(LogMorpheusHMD, Error, TEXT("MorpheusHMD tried to read PSVRFrameSequence from ini, but encountered unexpected value %s.  ini setting will be ignored."), *s);
		}
	}

	if (GConfig->GetString(MorpheusSettings, TEXT("ReprojectionSamplerWrapMode"), s, GEngineIni))
	{
		static const UEnum* Enum = CastChecked<UEnum>(StaticLoadObject(UEnum::StaticClass(), NULL, TEXT("/Script/Morpheus.EReprojectionSamplerWrapMode")));
		const int64 EnumValue = Enum->GetValueByName(FName(*s));
		if (EnumValue != INDEX_NONE)
		{
			const EReprojectionSamplerWrapMode mode = static_cast<EReprojectionSamplerWrapMode>(EnumValue);
			switch (mode)
			{
			case EReprojectionSamplerWrapMode::Mirror:
#if PLATFORM_PS4
				ReprojectionSamplerWrapMode = sce::Gnm::WrapMode::kWrapModeMirror;
#endif
				break;
			case EReprojectionSamplerWrapMode::ClampBorder:
#if PLATFORM_PS4
				ReprojectionSamplerWrapMode = sce::Gnm::WrapMode::kWrapModeClampBorder;
#endif
				break;
			default:
				UE_LOG(LogMorpheusHMD, Error, TEXT("MorpheusHMD tried to read ReprojectionSamplerWrapMode from ini, but encountered unexpected value %s %i.  ini setting will be ignored."), *s, EnumValue);
			}
		}
		else
		{
			UE_LOG(LogMorpheusHMD, Error, TEXT("MorpheusHMD tried to read ReprojectionSamplerWrapMode from ini, but encountered unexpected value %s.  ini setting will be ignored."), *s);
		}
	}

	if (GConfig->GetBool(MorpheusSettings, TEXT("bPositionTracking"), b, GEngineIni))
	{
		bPositionTrackingEnabled = b;
	}

	if (GConfig->GetString(MorpheusSettings, TEXT("HmdSetupDialogCanceledBehavior"), s, GEngineIni))
	{
		static const UEnum* Enum = CastChecked<UEnum>(StaticLoadObject(UEnum::StaticClass(), NULL, TEXT("/Script/Morpheus.EHmdSetupDialogCanceledBehavior")));
		const int64 EnumValue = Enum->GetValueByName(FName(*s));
		if (EnumValue != INDEX_NONE)
		{
			HmdSetupDialogCanceledBehavior = static_cast<EHmdSetupDialogCanceledBehavior>(EnumValue);
		}
		else
		{
			UE_LOG(LogMorpheusHMD, Error, TEXT("MorpheusHMD tried to read HmdSetupDialogCanceledBehavior from ini, but encountered unexpected value %s.  ini setting will be ignored."), *s);
		}
	}	

	if (GConfig->GetBool(MorpheusSettings, TEXT("bDisableHMDOrientationUntilHMDHasBeenTracked"), b, GEngineIni))
	{
		bDisableHMDOrientationUntilHMDHasBeenTracked = b;
	}

	if (GConfig->GetVector(MorpheusSettings, TEXT("DesktopOffset"), v, GEngineIni))
	{
		MorpheusDeviceInfo.DesktopWindowPosX = v.X;
		MorpheusDeviceInfo.DesktopWindowPosY = v.Y;
	}

	if (GConfig->GetBool(MorpheusSettings, TEXT("bEnableControllerPitch"), b, GEngineIni))
	{
		bControllerPitchEnabled = b;
	}

	if (GConfig->GetBool(MorpheusSettings, TEXT("bEnableControllerRoll"), b, GEngineIni))
	{
		bControllerRollEnabled = b;
	}

	if (GConfig->GetString(MorpheusSettings, TEXT("HMDServerAddress"), s, GEngineIni))
	{
		MorpheusDeviceInfo.HMDServerAddress = s;
	}

	if (GConfig->GetBool(MorpheusSettings, TEXT("bEnableSocialScreenSeparateMode"), b, GEngineIni))
	{
		bEnableSocialScreenSeparateMode = b;
	}
}

void FMorpheusHMD::SaveToIni()
{	
	GConfig->SetFloat(MorpheusSettings, TEXT("IPD"), MorpheusDeviceInfo.InterpupillaryDistance, GEngineIni);
	GConfig->SetBool(MorpheusSettings, TEXT("bPositionTracking"), bPositionTrackingEnabled, GEngineIni);
	GConfig->SetBool(MorpheusSettings, TEXT("bEnableControllerPitch"), bControllerPitchEnabled, GEngineIni);
	GConfig->SetBool(MorpheusSettings, TEXT("bEnableControllerRoll"), bControllerRollEnabled, GEngineIni);
}

void FMorpheusHMD::Shutdown()
{
#if PLATFORM_PS4
	SocialScreenShutdown();
		
	DestroyHmd();
#else
	if (HMDClientConnectionHandle)
	{
		hmdclientDisconnect(HMDClientConnectionHandle);
		HMDClientConnectionHandle = nullptr;
	}
	if (HMDClientDLLHandle)
	{
		FreeLibrary(HMDClientDLLHandle);
		HMDClientDLLHandle = nullptr;
	}
#endif

	if (TrackerHandle != IPS4Tracker::INVALID_TRACKER_HANDLE)
	{
		PS4Tracker->ReleaseTracker(TrackerHandle);
	}

	SaveToIni();	
}

bool FMorpheusHMD::IsInitialized() const
{
	return bWasInitialized;
}

bool FMorpheusHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	if(IsInitialized())
	{
#if PLATFORM_PS4
		MonitorDesc.MonitorName = FString::Printf(TEXT("\\\\.\\DISPLAY%d\\Monitor%d"), MorpheusOutputDeviceEnum[0], MorpheusOutputDeviceEnum[1]);;
		MonitorDesc.MonitorId = 0;
		MonitorDesc.DesktopX = MorpheusDeviceInfo.DesktopWindowPosX;
		MonitorDesc.DesktopY = MorpheusDeviceInfo.DesktopWindowPosY;
		MonitorDesc.ResolutionX = MorpheusDeviceInfo.HResolution;
		MonitorDesc.ResolutionY = MorpheusDeviceInfo.VResolution;
		return true;
#else
		// Try to find a Morpheus HMD by looking at the displays we know about.
		// This is certainly not all that reliable, but this is a development feature, not a shipping one.

		FDisplayMetrics DisplayMetrics;
		FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);
		const int32 NumMonitors = DisplayMetrics.MonitorInfo.Num();

		for (int32 MonitorInfoIndex = NumMonitors - 1; MonitorInfoIndex >= 0; --MonitorInfoIndex)
		{
			static const FString MorpheusHMDName(TEXT("SNYB403"));
			FMonitorInfo& MonitorInfo = DisplayMetrics.MonitorInfo[MonitorInfoIndex];
			if (MonitorInfo.Name.Compare(MorpheusHMDName) == 0)
			{
				MonitorDesc.MonitorName = MonitorInfo.ID;
				MonitorDesc.MonitorId = MonitorInfoIndex + 1;
				MonitorDesc.DesktopX = MonitorInfo.WorkArea.Left;
				MonitorDesc.DesktopY = MonitorInfo.WorkArea.Top;
				MonitorDesc.ResolutionX = MonitorInfo.NativeWidth;
				MonitorDesc.ResolutionY = MonitorInfo.NativeHeight;

				UE_LOG(LogMorpheusHMD, Log, TEXT("FMorpheusHMD::GetHMDMonitorInfo found that monitor %i is a morpheus.  Using it."), MonitorDesc.MonitorId);
				return true;
			}
		}

		for (int32 MonitorInfoIndex = NumMonitors - 1; MonitorInfoIndex >= 0; --MonitorInfoIndex)
		{
			static const FString MorpheusHMDPartialName(TEXT("SNY"));
			FMonitorInfo& MonitorInfo = DisplayMetrics.MonitorInfo[MonitorInfoIndex];
			if (MonitorInfo.Name.Find(MorpheusHMDPartialName) != INDEX_NONE)
			{
				if (MonitorInfo.NativeWidth == MorpheusDeviceInfo.HResolution && MonitorInfo.NativeHeight == MorpheusDeviceInfo.VResolution)
				{
					MonitorDesc.MonitorName = MonitorInfo.ID;
					MonitorDesc.MonitorId = MonitorInfoIndex + 1;
					MonitorDesc.DesktopX = MonitorInfo.WorkArea.Left;
					MonitorDesc.DesktopY = MonitorInfo.WorkArea.Top;
					MonitorDesc.ResolutionX = MonitorInfo.NativeWidth;
					MonitorDesc.ResolutionY = MonitorInfo.NativeHeight;

					UE_LOG(LogMorpheusHMD, Log, TEXT("FMorpheusHMD::GetHMDMonitorInfo found that monitor %i is the right size, and from sony.  Using it."), MonitorDesc.MonitorId);
					return true;
				}
			}
		}

		for (int32 MonitorInfoIndex = NumMonitors - 1; MonitorInfoIndex >= 0; --MonitorInfoIndex)
		{
			FMonitorInfo& MonitorInfo = DisplayMetrics.MonitorInfo[MonitorInfoIndex];
			if (MonitorInfo.NativeWidth == MorpheusDeviceInfo.HResolution && MonitorInfo.NativeHeight == MorpheusDeviceInfo.VResolution)
			{
				MonitorDesc.MonitorName = MonitorInfo.ID;
				MonitorDesc.MonitorId = MonitorInfoIndex + 1;
				MonitorDesc.DesktopX = MonitorInfo.WorkArea.Left;
				MonitorDesc.DesktopY = MonitorInfo.WorkArea.Top;
				MonitorDesc.ResolutionX = MonitorInfo.NativeWidth;
				MonitorDesc.ResolutionY = MonitorInfo.NativeHeight;

				UE_LOG(LogMorpheusHMD, Log, TEXT("FMorpheusHMD::GetHMDMonitorInfo found that monitor %i is the right size.  Using it."), MonitorDesc.MonitorId);
				return true;
			}
		}


		// If we didn't find one we liked just default, so we draw something.
		// Old default way of doing it, works if morpheus is monitor 3, and there are only 1920 pixels of other monitors to its left.
		MonitorDesc.MonitorName = FString::Printf(TEXT("\\\\.\\DISPLAY%d\\Monitor%d"), MorpheusOutputDeviceEnum[0], MorpheusOutputDeviceEnum[1]);;
		MonitorDesc.MonitorId = 0;
		MonitorDesc.DesktopX = MorpheusDeviceInfo.DesktopWindowPosX;
		MonitorDesc.DesktopY = MorpheusDeviceInfo.DesktopWindowPosY;
		MonitorDesc.ResolutionX = MorpheusDeviceInfo.HResolution;
		MonitorDesc.ResolutionY = MorpheusDeviceInfo.VResolution;

		UE_LOG(LogMorpheusHMD, Warning, TEXT("FMorpheusHMD::GetHMDMonitorInfo did not find a monitor it thinks is a morpheus HMD. Drawing it at %i, %i."), MonitorDesc.DesktopX, MonitorDesc.DesktopY);
		return true;
#endif
	}

	return false;
}

void FMorpheusHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	OutHFOVInDegrees = 0.0f;
	OutVFOVInDegrees = 0.0f;
}

bool FMorpheusHMD::DoesSupportPositionalTracking() const
{
	return true;
}

bool FMorpheusHMD::HasValidTrackingPosition()
{
	return bCurrentTrackerStateValid;
}

bool FMorpheusHMD::EnumerateTrackedDevices(TArray<int32>& TrackedIds, EXRTrackedDeviceType DeviceType /*= EXRTrackedDeviceType::Any*/)
{
	if (DeviceType == EXRTrackedDeviceType::Any || DeviceType == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		TrackedIds.Add(HMDDeviceId);
	}
	if (DeviceType == EXRTrackedDeviceType::TrackingReference || DeviceType == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		TrackedIds.Add(CameraDeviceId);
	}
	return TrackedIds.Num() > 0;
}

bool FMorpheusHMD::GetTrackingSensorProperties(int32 InDeviceId, FQuat& OutOrientation, FVector& OutOrigin, FXRSensorProperties& OutSensorProperties)
{
	if (InDeviceId == CameraDeviceId)
	{
		// Constants from https://ps4.scedev.net/resources/documents/SDK/3.500/VrTracker-Overview/0003.html figure 11:
		static const float MinDistanceMeters = 0.5f;
		static const float MaxDistanceMeters = 3.0f;
#if PLATFORM_PS4
		static const float HalfHorizontalFOVDegrees = FMath::RadiansToDegrees(SCE_CAMERA_HORIZONTAL_FOV) * 0.5f;
		static const float HalfVerticalFOVDegrees = FMath::RadiansToDegrees(SCE_CAMERA_VERTICAL_FOV) * 0.5f;
#else
		static const float HalfHorizontalFOVDegrees = 37.85f;
		static const float HalfVerticalFOVDegrees = 25.90f;
#endif

		OutSensorProperties.LeftFOV = OutSensorProperties.RightFOV = HalfHorizontalFOVDegrees;
		OutSensorProperties.TopFOV = OutSensorProperties.BottomFOV = HalfVerticalFOVDegrees;
		OutSensorProperties.NearPlane = MinDistanceMeters * WorldToMeterScale;
		OutSensorProperties.FarPlane = MaxDistanceMeters * WorldToMeterScale;
		OutSensorProperties.CameraDistance = 0.f;
		OutOrigin = FVector::ZeroVector;
		// The Camera is rotated 180 degrees around Y so it faces into the playable frustum, rather than toward the camera end of the frustum, like the base hmd rotation.
		OutOrientation = FQuat(0, 1, 0, 0);
		RebaseObjectOrientationAndPosition(OutOrigin, OutOrientation);
		return true;
	}
	return false;
}

bool FMorpheusHMD::IsStereoEnabled() const
{
	return bStereoEnabled && bEnabled;
}

bool FMorpheusHMD::EnableStereo(bool bStereo)
{
	if (bStereo)
	{
		bStereoEnabled = true;
	}
	else if (bStereoEnabled)
	{
		bStereoShuttingDown = true;
	}
	GEngine->bForceDisableFrameRateSmoothing = bStereo;
	return bStereo;
}

void FMorpheusHMD::Show2DVRSplashScreen(class UTexture* Texture, FVector2D Scale, FVector2D Offset)
{
	FTexture2DRHIRef TextureRHI;
	if (Texture)
	{
		FTextureResource* TextureResource = Texture->Resource;
		if (TextureResource && TextureResource->TextureRHI)
		{
			TextureRHI = TextureResource->TextureRHI->GetTexture2D();
		}

		if (!TextureRHI)
		{
			UE_LOG(LogMorpheusHMD, Error, TEXT("Show2DVRSplashScreen called but Texture is Invalid."));
		}
		if (TextureRHI->GetFormat() != PF_B8G8R8A8)
		{
			UE_LOG(LogMorpheusHMD, Error, TEXT("Show2DVRSplashScreen called but Texture format is wrong.  Must be B8G8R8A8."));
		}
	}
	else
	{
		UE_LOG(LogMorpheusHMD, Error, TEXT("Show2DVRSplashScreen called but Texture is NULL."))
	}
	Show2DVRSplashScreen(TextureRHI, Scale, Offset);
}

void FMorpheusHMD::Show2DVRSplashScreen(FTexture2DRHIRef Texture, FVector2D Scale, FVector2D Offset)
{
#if PLATFORM_PS4
	GnmBridge::Morpheus2DVRReprojectionData ReprojectionData(
		Texture,
		Scale,
		Offset
		);
	
	GnmBridge::StartMorpheus2DVRReprojection(ReprojectionData);
#endif
}

void FMorpheusHMD::Hide2DVRSplashScreen()
{
#if PLATFORM_PS4
	GnmBridge::StopMorpheus2DVRReprojection();
#endif
}

void FMorpheusHMD::HMDReprojectionSetOutputMinColor(FLinearColor MinColor)
{
#if PLATFORM_PS4
	int Ret = sceHmdReprojectionSetOutputMinColor(MinColor.R, MinColor.G, MinColor.B);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogMorpheusHMD, Error, TEXT("sceHmdReprojectionSetOutputMinColor failed! returned 0x%x"), Ret)
	}
#endif
}

void FMorpheusHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	SizeX = FMath::CeilToInt(IdealRenderTargetSize.X * PixelDensity);
	SizeY = FMath::CeilToInt(IdealRenderTargetSize.Y * PixelDensity);

	SizeX = SizeX / 2;
	if( StereoPass == eSSP_RIGHT_EYE )
	{
		X += SizeX;
	}
}

void FMorpheusHMD::GetCurrentEyePose(const EStereoscopicPass StereoPassType, FQuat& ViewRotation, FVector& ViewLocation)
{
	const bool bIsInGameThread = IsInGameThread();
	auto& TrackingFrame = GetTrackingFrame();
	// Make sure we're always on the current frame in the render thread, where it counts!
	if (!(
		(TrackerHandle == IPS4Tracker::INVALID_TRACKER_HANDLE)
		|| (LastTrackingData.Status != IPS4Tracker::ETrackingStatus::TRACKING && LastTrackingData.Status != IPS4Tracker::ETrackingStatus::NOT_TRACKING)
		|| (TrackingFrame.FrameNumber == 0)
		|| bIsInGameThread
		|| (TrackingFrame.FrameNumber == GFrameNumberRenderThread)
		))
	{
		UE_LOG(LogMorpheusHMD, Warning, TEXT("Render thread tracking frame mismatch!  If this happens once after a breakpoint its ok."));
		UE_LOG(LogMorpheusHMD, Warning, TEXT("TrackerHandle=%i LastTrackingData.Status=%i TrackingFrame.FrameNumber=%i bIsInGameThread=%i GFrameNumberRenderThread=%i"), TrackerHandle, static_cast<int32>(LastTrackingData.Status), TrackingFrame.FrameNumber, bIsInGameThread, GFrameNumberRenderThread);
	}

	const uint8 Eye = (StereoPassType == eSSP_LEFT_EYE) ? 0 : 1;
	ViewRotation = TrackingFrame.EyeOrientation[Eye];
	ViewLocation = TrackingFrame.EyePosition[Eye];
}

bool FMorpheusHMD::GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition)
{
	bool retval = false;
	if (DeviceId == HMDDeviceId && (Eye == eSSP_LEFT_EYE || Eye == eSSP_RIGHT_EYE))
	{
		auto& TrackingFrame = GetTrackingFrame();
		const uint8 EyeIdx = (Eye == eSSP_LEFT_EYE) ? 0 : 1;

		OutOrientation = TrackingFrame.DeviceOrientation.Inverse() * TrackingFrame.EyeOrientation[EyeIdx];
		OutOrientation.Normalize();
		OutPosition = TrackingFrame.DeviceOrientation.UnrotateVector(TrackingFrame.EyePosition[EyeIdx] - TrackingFrame.DevicePosition);
		retval = HasValidTrackingPosition();
	}
	else
	{
		OutOrientation = FQuat::Identity;
		OutPosition = FVector::ZeroVector;
	}
	return retval;
}

namespace
{

	// Create projection matrix from near plane rectangular distances, but using reversed infinitely distant far plane
	// for z projection.  Can't use standard perspective matrix functions because Morpheus optics have different effective
	// FOVs on all sides.
	FMatrix GetMorpheusProjectionMatrix(float Left, float Right, float Bottom, float Top, float ZNear)
	{
		float SumRL, SumTB, InvRL, InvTB, N2;
		SumRL = (Right + Left);
		SumTB = (Top + Bottom);
		InvRL = (1.0f / (Right - Left));
		InvTB = (1.0f / (Top - Bottom));
		N2 = (ZNear + ZNear);

		return FMatrix(
			FPlane((N2 * InvRL), 0.0f, 0.0f, 0.0f),
			FPlane(0.0f, (N2 * InvTB), 0.0f, 0.0f),
			FPlane(-(SumRL * InvRL), -(SumTB * InvTB), 0.0f, 1.0f),
			FPlane(0.0f, 0.0f, ZNear, 0.0f)
			);
	}
} // Anonymous namespace

FMatrix FMorpheusHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
{
	FMatrix MorpheusProjectionMatrix;
	{	
		const float InNearZ = GNearClippingPlane;			
		if (StereoPassType == eSSP_LEFT_EYE)  
		{
			MorpheusProjectionMatrix = GetMorpheusProjectionMatrix(-DeviceInfoFOV.TanOut * InNearZ, DeviceInfoFOV.TanIn * InNearZ, -DeviceInfoFOV.TanBottom * InNearZ, DeviceInfoFOV.TanTop * InNearZ, InNearZ);
		}
		else
		{
			MorpheusProjectionMatrix = GetMorpheusProjectionMatrix(-DeviceInfoFOV.TanIn * InNearZ, DeviceInfoFOV.TanOut * InNearZ, -DeviceInfoFOV.TanBottom * InNearZ, DeviceInfoFOV.TanTop * InNearZ, InNearZ);
		}
	}
	return MorpheusProjectionMatrix;
}

void FMorpheusHMD::GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const
{
	const float Disparity = (HudOffset + CanvasCenterOffset);
	const int32 RenderTargetWidth = FMath::CeilToInt(static_cast<float>(IdealRenderTargetSize.X) * PixelDensity);
	const int32 RenderTargetHeight = FMath::CeilToInt(static_cast<float>(IdealRenderTargetSize.Y) * PixelDensity);

	OrthoProjection[0] = FTranslationMatrix(FVector(Disparity, 0.0f, 0.0f));
	OrthoProjection[1] = FTranslationMatrix(FVector(-Disparity + RenderTargetWidth * 0.5f, 0.0f, 0.0f));

	const FScaleMatrix RTScale(FVector(static_cast<float>(RTWidth) / static_cast<float>(RenderTargetWidth), static_cast<float>(RTHeight) / static_cast<float>(RenderTargetHeight), 1.0f));
	OrthoProjection[0] *= RTScale;
	OrthoProjection[1] *= RTScale;
}

void FMorpheusHMD::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
{
	FSceneView HMDView(*InView);
 
	const FQuat DeltaRot = HMDView.BaseHmdOrientation.Inverse() * Canvas->HmdOrientation;
	HMDView.ViewRotation = FRotator(HMDView.ViewRotation.Quaternion() * DeltaRot);
	HMDView.UpdateViewMatrix();
	Canvas->ViewProjectionMatrix = HMDView.ViewMatrices.GetViewProjectionMatrix();
}

void FMorpheusHMD::UpdateViewportRHIBridge(bool /* bUseSeparateRenderTarget */, const class FViewport& Viewport, FRHIViewport* const ViewportRHI)
{
	check(IsInGameThread());

#if PLATFORM_PS4
	MorpheusBridge->UpdateViewport(Viewport, ViewportRHI);
#endif
}

void FMorpheusHMD::RenderTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, class FRHITexture2D* BackBuffer, class FRHITexture2D* SrcTexture, FVector2D WindowSize) const
{
	RenderSocialScreen_RenderThread(RHICmdList, BackBuffer, SrcTexture, WindowSize);
}

bool FMorpheusHMD::AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InTexFlags, uint32 InTargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples)
{
	if (!IsStereoEnabled())
	{
		return false;
	}

	FRHIResourceCreateInfo CreateInfo;
	RHICreateTargetableShaderResource2D(SizeX, SizeY, PF_B8G8R8A8, 1, TexCreate_None, TexCreate_RenderTargetable, false, CreateInfo, OutTargetableTexture, OutShaderResourceTexture);

	return true;
}

bool FMorpheusHMD::GetHMDDistortionEnabled(EShadingPath ShadingPath) const
{
#if defined(MORPHEUS_ENGINE_DISTORTION) && MORPHEUS_ENGINE_DISTORTION
	return true;
#else
	return false;
#endif
}

float FMorpheusHMD::GetDistortionScalingFactor() const
{
	return 1.0f;
}

float FMorpheusHMD::GetLensCenterOffset() const
{
	return 0.0f;
}


void FMorpheusHMD::GetDistortionWarpValues(FVector4& K) const
{
	K = FVector4();
}

bool FMorpheusHMD::IsChromaAbCorrectionEnabled() const
{
	return true;
}

bool FMorpheusHMD::GetChromaAbCorrectionValues(FVector4& K) const
{
	K = FVector4();
	return true;
}

void FMorpheusHMD::SetupOcclusionMeshes()
{
	HiddenAreaMeshes[0].BuildMesh(LeftEyeHiddenAreaPositions, HiddenAreaVertexCount, FHMDViewMesh::MT_HiddenArea);
	HiddenAreaMeshes[1].BuildMesh(RightEyeHiddenAreaPositions, HiddenAreaVertexCount, FHMDViewMesh::MT_HiddenArea);
	VisibleAreaMeshes[0].BuildMesh(LeftEyeVisibleAreaPositions, VisibleAreaVertexCount, FHMDViewMesh::MT_VisibleArea);
	VisibleAreaMeshes[1].BuildMesh(RightEyeVisibleAreaPositions, VisibleAreaVertexCount, FHMDViewMesh::MT_VisibleArea);
}

static void DrawOcclusionMesh(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass, const FHMDViewMesh MeshAssets[])
{
	check(IsInRenderingThread());
	check(StereoPass != eSSP_FULL);

	const uint32 MeshIndex = (StereoPass == eSSP_LEFT_EYE) ? 0 : 1;
	const FHMDViewMesh& Mesh = MeshAssets[MeshIndex];
	check(Mesh.IsValid());

	DrawIndexedPrimitiveUP(
		RHICmdList,
		PT_TriangleList,
		0,
		Mesh.NumVertices,
		Mesh.NumTriangles,
		Mesh.pIndices,
		sizeof(Mesh.pIndices[0]),
		Mesh.pVertices,
		sizeof(Mesh.pVertices[0])
		);
}

void FMorpheusHMD::DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, HiddenAreaMeshes);
}

void FMorpheusHMD::DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const
{
	DrawOcclusionMesh(RHICmdList, StereoPass, VisibleAreaMeshes);
}

FVector2D FMorpheusHMD::GetTextureOffsetLeft()  const
{
	return LeftTranslation.Offset;
}

FVector2D FMorpheusHMD::GetTextureOffsetRight() const
{
	return RightTranslation.Offset;
}

FVector2D FMorpheusHMD::GetTextureScaleLeft() const
{
	return LeftTranslation.Scale;
}

FVector2D FMorpheusHMD::GetTextureScaleRight() const
{
	return RightTranslation.Scale;
}

void FMorpheusHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
	MorpheusDeviceInfo.InterpupillaryDistance = NewInterpupillaryDistance;
}

float FMorpheusHMD::GetInterpupillaryDistance() const
{
	return MorpheusDeviceInfo.InterpupillaryDistance;
}

void FMorpheusHMD::SetupOverlayTargets(FTexture2DRHIRef SceneRenderTarget)
{
	if (!DefaultStereoLayers.IsValid() || !SceneRenderTarget.IsValid())
	{
		return;
	}

	// Update render textures for each eye
	// Assumes side-by-side stereo viewport in same target
	const uint32 EyeSizeX = SceneRenderTarget->GetSizeX() / 2;
	const uint32 EyeSizeY = SceneRenderTarget->GetSizeY();
	if (!OverlayLayerRenderTarget[0][0].IsValid() || EyeSizeX != EyeTextureSizeX || EyeSizeY != EyeTextureSizeY)
	{
		FRHIResourceCreateInfo CreateInfo;

		for (uint32 BufferIndex = 0; BufferIndex < STEREO_LAYER_BUFFER_COUNT; ++BufferIndex)
		{
			// Left eye targets
			OverlayLayerRenderTarget[0][BufferIndex] = RHICreateTexture2D(
				EyeSizeX,
				EyeSizeY,
				SceneRenderTarget->GetFormat(),
				1,
				1,
				SceneRenderTarget->GetFlags(),
				CreateInfo
			);
			OverlayLayerRenderTargetWritten[0][BufferIndex] = false;

			// Right eye targets
			OverlayLayerRenderTarget[1][BufferIndex] = RHICreateTexture2D(
				EyeSizeX,
				EyeSizeY,
				SceneRenderTarget->GetFormat(),
				1,
				1,
				SceneRenderTarget->GetFlags(),
				CreateInfo
			);
			OverlayLayerRenderTargetWritten[1][BufferIndex] = false;
		}

		EyeTextureSizeX = EyeSizeX;
		EyeTextureSizeY = EyeSizeY;
	}
}

FTexture2DRHIRef FMorpheusHMD::GetOverlayLayerTarget_RenderThread(EStereoscopicPass StereoPass, FIntRect& InOutViewport)
{
	int EyeIndex = (StereoPass == EStereoscopicPass::eSSP_LEFT_EYE) ? 0 : 1;
	if (OverlayLayerRenderTarget[EyeIndex][StereoLayerBufferIndex].IsValid())
	{
		InOutViewport = FIntRect(0, 0, EyeTextureSizeX, EyeTextureSizeY);
		OverlayLayerRenderTargetWritten[EyeIndex][StereoLayerBufferIndex] = true;
		return OverlayLayerRenderTarget[EyeIndex][StereoLayerBufferIndex];
	}
	else
	{
		return nullptr;
	}
}

FTexture2DRHIRef FMorpheusHMD::GetSceneLayerTarget_RenderThread(EStereoscopicPass StereoPass, FIntRect& InOutViewport)
{
	FTexture2DRHIRef Texture = nullptr;
#if PLATFORM_PS4
	Texture =  MorpheusBridge->GetUnscaledEyeTexture();
	if (Texture.IsValid())
	{
		InOutViewport = FIntRect(0, 0, EyeTextureSizeX, EyeTextureSizeY);
		if (StereoPass == EStereoscopicPass::eSSP_RIGHT_EYE)
		{
			InOutViewport.Min.X += EyeTextureSizeX;
			InOutViewport.Max.X += EyeTextureSizeX;
		}
	}
#endif
	return Texture;
}

void FMorpheusHMD::ResetOrientationAndPosition(float Yaw /*= 0.f*/)
{
	RecenterExtraYaw = Yaw;
	bRecenterView = true;
	bReorientView = true;
}

void FMorpheusHMD::ResetPosition()
{
	bRecenterView = true;
}

void FMorpheusHMD::RebaseObjectOrientationAndPosition(FVector& Position, FQuat& Orientation) const
{
	Position = BaseHmdOrientation.Inverse().RotateVector(Position - BaseHmdPosition);
	Orientation = BaseHmdOrientation.Inverse() * Orientation;
}

FVector FMorpheusHMD::GetAudioListenerOffset(int32 DeviceId /*= HMDDeviceId*/) const
{
	if (DeviceId == HMDDeviceId)
	{
		auto& TrackingFrame = GetTrackingFrame();
		return TrackingFrame.DeviceOrientation.Inverse().RotateVector(TrackingFrame.HeadPosition - TrackingFrame.DevicePosition);
	}
	else
	{
		return FVector::ZeroVector;
	}
}

const FMorpheusHMD::FTrackingFrame& FMorpheusHMD::GetTrackingFrame() const
{
	if (IsInRenderingThread())
	{
		return RenderThreadFrame;
	}
	else
	{
		return GameThreadFrame;
	}
}

bool FMorpheusHMD::GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	switch (DeviceId)
	{
	case HMDDeviceId:
		{
			auto& TrackingFrame = GetTrackingFrame();
			CurrentOrientation = TrackingFrame.DeviceOrientation;
			CurrentPosition = TrackingFrame.DevicePosition;
			return HasValidTrackingPosition();
		}
	case CameraDeviceId:
		{
			// The Camera is rotated 180 degrees around Y so it faces into the playable frustum, rather than toward the camera end of the frustum, like the base HMD rotation.
			CurrentOrientation = FQuat(0, 1, 0, 0);
			CurrentPosition = FVector::ZeroVector;
			RebaseObjectOrientationAndPosition(CurrentPosition, CurrentOrientation);
			return true;
		}
	default:
		return false;
	}
}

void FMorpheusHMD::FetchTrackingData(FTrackingFrame& TrackingFrame, FQuat& CurrentOrientation, FVector& CurrentPosition, bool bUpdateLastTrackingData, bool bEarlyPoll, bool bPollImmediately)
{
	FScopeLock FrameLock(&FrameTrackerMutex);

	CurrentOrientation = FQuat::Identity;
	CurrentPosition = FVector::ZeroVector;

	if (TrackerHandle != IPS4Tracker::INVALID_TRACKER_HANDLE)
	{
		// Tracking data comes from PS4Tracker in tracking space, so we need to apply worldscale to all positions.
		IPS4Tracker::FTrackingData TrackingData;
		PS4Tracker->GetTrackingData(TrackerHandle, TrackingData, bPollImmediately, bEarlyPoll);

		// In order to retain IPD separation, the PSVR will send back offset positions, even when not tracking
		if (TrackingData.Status != IPS4Tracker::ETrackingStatus::CALIBRATING)
		{
			if (bReorientView || !BaseHmdOrientation.IsNormalized())
			{
				BaseHmdOrientation = TrackingData.Orientation;

				FRotator BaseHmdRotation = BaseHmdOrientation.Rotator();
				BaseHmdRotation.Yaw -= RecenterExtraYaw; // I am unsure as to whether this should be + or -.
				BaseHmdRotation.Pitch = 0;
				BaseHmdRotation.Roll = 0;
				BaseHmdRotation.Normalize();
				BaseHmdOrientation = BaseHmdRotation.Quaternion();
				bReorientView = false;
				RecenterExtraYaw = 0.0f;
			}
			if (bRecenterView)
			{
				BaseHmdPosition = TrackingData.Position * WorldToMeterScale;
				bRecenterView = false;
			}
			CurrentPosition = BaseHmdOrientation.Inverse().RotateVector((TrackingData.Position * WorldToMeterScale) - BaseHmdPosition);

			TrackingFrame.EyePosition[0] = BaseHmdOrientation.Inverse().RotateVector((TrackingData.UnrealEyePosition[0] * WorldToMeterScale) - BaseHmdPosition);
			TrackingFrame.EyePosition[1] = BaseHmdOrientation.Inverse().RotateVector((TrackingData.UnrealEyePosition[1] * WorldToMeterScale) - BaseHmdPosition);
		}

		if (bDisableHMDOrientationUntilHMDHasBeenTracked && (TrackingData.Status == IPS4Tracker::ETrackingStatus::CALIBRATING || TrackingData.Status == IPS4Tracker::ETrackingStatus::NOT_STARTED))
		{
			// if we have lost our valid state, or if this is the first time we are reading one
			if (bCurrentTrackerStateValid || !bTrackerStateHasBeenRead)
			{
				UE_LOG(LogMorpheusHMD, Log, TEXT("MorpheusHMD Initialization is waiting to successfully track the HMD before it is complete."));
				FCoreDelegates::VRHeadsetTrackingInitializingAndNeedsHMDToBeTrackedDelegate.Broadcast();
			}
			bCurrentTrackerStateValid = false;
		}
		else
		{
			CurrentOrientation = BaseHmdOrientation.Inverse() * TrackingData.Orientation;
			CurrentOrientation.Normalize();

			TrackingFrame.EyeOrientation[0] = BaseHmdOrientation.Inverse() * TrackingData.UnrealEyeOrientation[0];
			TrackingFrame.EyeOrientation[0].Normalize();
			TrackingFrame.EyeOrientation[1] = BaseHmdOrientation.Inverse() * TrackingData.UnrealEyeOrientation[1];
			TrackingFrame.EyeOrientation[1].Normalize();

			TrackingFrame.HeadPosition = BaseHmdOrientation.Inverse().RotateVector((TrackingData.UnrealHeadPosition * WorldToMeterScale) - BaseHmdPosition);
			TrackingFrame.HeadOrientation = BaseHmdOrientation.Inverse() * TrackingData.UnrealHeadOrientation;

			if (bCurrentTrackerStateValid == false)
			{
				UE_LOG(LogMorpheusHMD, Log, TEXT("MorpheusHMD initialization completed."));
				FCoreDelegates::VRHeadsetTrackingInitializedDelegate.Broadcast();
			}

			bCurrentTrackerStateValid = true;
		}

		bTrackerStateHasBeenRead = true;

		if (bUpdateLastTrackingData)
		{
			LastTrackingData = TrackingData;
		}
		TrackingFrame.FrameNumber = TrackingData.FrameNumber;
	}

	TrackingFrame.DeviceOrientation = CurrentOrientation;
	TrackingFrame.DevicePosition = CurrentPosition;
}

void FMorpheusHMD::OnBeginRendering_GameThread()
{
	SocialScreen_BeginRenderViewFamily();
}

void FMorpheusHMD::OnBeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) 
{
	check(IsInRenderingThread());
	{
		FQuat CurrentOrientation;
		FVector CurrentPosition;
		SCOPED_NAMED_EVENT_TEXT("FMorpheusHMD::OnBeginRendering_RenderThread FetchTrackingData for RenderThreadFrame", FColor::Turquoise);
		FetchTrackingData(RenderThreadFrame, CurrentOrientation, CurrentPosition, true, false, false);
	}

	SCOPED_NAMED_EVENT_TEXT("FMorpheusHMD::OnBeginRendering_RenderThread", FColor::Turquoise); 

	if (bStereoShuttingDown)
	{
		bStereoEnabled = false;
		bStereoShuttingDown = false;
	}

#if PLATFORM_PS4
	//CVAR is only safe to check on the rendering thread.  We must propagate proper reprojection state forward in a threadsafe manner for the current frame.	

	const bool bWasStereoEnabled = GnmBridge::GetOutputMode() != EPS4OutputMode::Standard2D;
	bool bDoRHICommandUpdate = false;
	if (bStereoEnabled && !bWasStereoEnabled)
	{
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GnmBridge::ChangeOutputMode(PSVROutputMode);
		GnmBridge::SetReprojectionSamplerWrapMode(ReprojectionSamplerWrapMode);
		bDoRHICommandUpdate = true;
		UpdateTrackerPredictionTiming(true);
	}
	else if (!bStereoEnabled && bWasStereoEnabled)
	{
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GnmBridge::ChangeOutputMode(EPS4OutputMode::Standard2D);
		bDoRHICommandUpdate = true;
		UpdateTrackerPredictionTiming(false);
	}
	//flush before grabbing tracker data to make sure tracker data isn't stale.
	if (bDoRHICommandUpdate)
	{
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);

		if (TrackerHandle != IPS4Tracker::INVALID_TRACKER_HANDLE)
		{
			PS4Tracker->Synchronize(TrackerHandle);
		}
	}
#endif

	FTexture2DRHIRef UnscaledEyeTexture;
#if PLATFORM_PS4
	UnscaledEyeTexture = MorpheusBridge->GetUnscaledEyeTexture();
#endif
	SetupOverlayTargets(UnscaledEyeTexture);

#if PLATFORM_PS4

	if (bDoRHICommandUpdate)
	{	
		MorpheusBridge->SetDoReprojection(bStereoEnabled);
	}
	const IPS4Tracker::FTrackingData& LastTrackingDataObtained = GetLastTrackingData();
	if (RHICmdList.Bypass())
	{
		FRHISetTrackerData Command(MorpheusBridge, LastTrackingDataObtained);
		Command.Execute(RHICmdList);
		return;
	}
	new (RHICmdList.AllocCommand<FRHISetTrackerData>()) FRHISetTrackerData(MorpheusBridge, LastTrackingDataObtained);
#endif

	SocialScreen_BeginRendering_RenderThread();

}

FMorpheusConsoleCommands::FMorpheusConsoleCommands(class FMorpheusHMD* InHMDPtr)
	: ShowSettingsCommand(TEXT("vr.morpheus.Debug.Show"),
	*NSLOCTEXT("Morpheus", "CCommandText_Show",
		"Morpheus specific extension.\n"
		"Shows the current value of various stereo rendering params.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FMorpheusHMD::ShowSettingsCommandHandler))
	, IPDCommand(TEXT("vr.morpheus.Debug.IPD"),
		*NSLOCTEXT("Morpheus", "CCommandText_IPD",
			"Morpheus specific extension.\n"
			"Shows or changes the current interpupillary distance in meters.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(InHMDPtr, &FMorpheusHMD::IPDCommandHandler))
{}

void FMorpheusHMD::ShowSettingsCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	IHeadMountedDisplay::MonitorInfo MonitorInfo;
	GetHMDMonitorInfo(MonitorInfo);

	Ar.Logf(TEXT("DisplayDeviceName = %s"), *MonitorInfo.MonitorName);
	Ar.Logf(TEXT("DisplayId = %d"), MonitorInfo.MonitorId);
	Ar.Logf(TEXT("InterpupillaryDistance = %f"), MorpheusDeviceInfo.InterpupillaryDistance);
}

void FMorpheusHMD::IPDCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num() > 0)
	{
		float Value = FCString::Atof(*Args[0]);
		if (Value > 0)
		{
			SetInterpupillaryDistance(Value);
		}
	}
	Ar.Logf(TEXT("vr.morpheus.Debug.IPD = %f"), GetInterpupillaryDistance());
}

bool FMorpheusConsoleCommands::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	const TCHAR* OrigCmd = Cmd;
	FString AliasedCommand;

	if (FParse::Command( &Cmd, TEXT("STEREO") ))
	{
		// normal configuration
		
		float Value;
		if (FParse::Command( &Cmd, TEXT("DUMP") ))
		{
			AliasedCommand = TEXT("vr.morpheus.Debug.Show");
		}

		else if(FParse::Command( &Cmd, TEXT("RECENTER") ))
		{
			AliasedCommand = TEXT("vr.HeadTracking.ResetPosition");
		}

		else if (FParse::Value( Cmd, TEXT("E="), Value))
		{
			AliasedCommand = FString::Printf(TEXT("vr.morpheus.Debug.IPD %f"), Value);
		}
	}

	if (!AliasedCommand.IsEmpty())
	{
		Ar.Logf(ELogVerbosity::Warning, TEXT("%s is deprecated. Use %s instead"), OrigCmd, *AliasedCommand);
		return IConsoleManager::Get().ProcessUserConsoleInput(*AliasedCommand, Ar, InWorld);
	}

	return false;
}

#if PLATFORM_WINDOWS
const float* FMorpheusHMD::GetRedDistortionParameters() const
{
	return HMDDistortionParams.coefficient_red;
}
const float* FMorpheusHMD::GetGreenDistortionParameters() const
{
	return HMDDistortionParams.coefficient_green;
}
const float* FMorpheusHMD::GetBlueDistortionParameters() const
{
	return HMDDistortionParams.coefficient_blue;
}
#endif // PLATFORM_WINDOWS

#endif //MORPHEUS_SUPPORTED_PLATFORMS
