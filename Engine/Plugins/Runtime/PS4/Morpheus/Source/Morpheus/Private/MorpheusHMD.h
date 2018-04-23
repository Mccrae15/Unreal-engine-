// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "IMorpheusPlugin.h"

#if MORPHEUS_SUPPORTED_PLATFORMS

// Supported Prototypes:
// DVT 1.1

// Setup instructions:
// Install Morpheus SDK patch from Sony. UBT uses this to decide whether or not to compile in Morpheus support.
// Add .ini settings to your project ini.  See example above LoadFromIni

#include "HeadMountedDisplayBase.h"
#include "SceneViewExtension.h"
#include "RenderResource.h"
#include "PS4Tracker.h"
#include "MorpheusTypes.h"
#include "TickableObjectRenderThread.h"
#include "StereoLayerManager.h"
#include "XRRenderTargetManager.h"
#include "HeadMountedDisplayTypes.h"
#include "ThreadSafeCounter.h"

#if PLATFORM_PS4
#include "GnmBridge.h"
#include <hmd.h>
#include <hmd/distortion_correct.h>
#include <hmd/reprojection.h>
#include <gnm/constants.h>
#endif
#if PLATFORM_WINDOWS
#include "ThirdParty/PS4/HmdClient/include/hmd_client.h"
#endif

#define STEREO_LAYER_BUFFER_COUNT	3		// With 2 we saw flicker in face locked layers.  Looked like partial rendering.  A better fix may exist.

class FMorpheusConsoleCommands : private FSelfRegisteringExec
{
public:
	FMorpheusConsoleCommands(class FMorpheusHMD* InHMDPtr);
private:
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	FAutoConsoleCommand ShowSettingsCommand;
	FAutoConsoleCommand IPDCommand;
};

/**
 * Morpheus Head Mounted Display
 */
class FMorpheusHMD : public FHeadMountedDisplayBase, public FXRRenderTargetManager, public TSharedFromThis<FMorpheusHMD, ESPMode::ThreadSafe>
{
	friend class FMorpheusConsoleCommands;

	struct MorpheusHMDInfo
	{
		// Size of the entire screen, in pixels.
		unsigned  HResolution, VResolution; 
		// Physical dimensions of the active screen in meters. Can be used to calculate
		// projection center while considering IPD.
		float     HScreenSize, VScreenSize;				
		// Configured distance between the user's eye centers, in meters. Defaults to 0.064.
		float     InterpupillaryDistance;			

		float DesktopWindowPosX;
		float DesktopWindowPosY;

		FString HMDServerAddress;
	};

public:	

#if PLATFORM_PS4
	class FMorpheusBridge : public FRHICustomPresent
	{
	public:
		FMorpheusBridge(FMorpheusHMD* InPlugin);

		// Returns true if it is initialized and used.
		bool IsInitialized() const { return bInitialized; }

		void UpdateViewport(const FViewport& Viewport, FRHIViewport* ViewportRHI);

		FTexture2DRHIRef GetUnscaledEyeTexture () const { return UnscaledEyeTexture; }
		void SetUnscaledEyeTexture(const FTexture2DRHIRef& EyeInput);
		void SetTrackingData(const IPS4Tracker::FTrackingData& InTrackingData);
		void SetDoReprojection(bool bInDoReprojection);

		void Reset();
		void Shutdown() { Reset(); }

		void Init();		

		// Implementation of FRHICustomPresent
		// Resets Viewport-specific resources.
		virtual void OnBackBufferResize() override;
		virtual bool NeedsNativePresent() override { return true; }
		virtual bool Present(int& SyncInterval) override;

	private:
		FMorpheusHMD*			Plugin;
		bool					bInitialized;
		bool					bDoReprojection;
		sce::Gnm::WrapMode		ReprojectionSamplerWrapMode;
		//Eye texture at size unaffected by screenpct.  Generally the one from the viewport.  Can be larger than 1080p if CalculateRenderTargetSize() returns a larger size and useseparaterendertarget is true
		FTexture2DRHIRef		UnscaledEyeTexture; 
		IPS4Tracker::FTrackingData	RenderedTrackingData;

		uint32				RenderTargetWidth;
		uint32				RenderTargetHeight;		
	};

	TRefCountPtr<FMorpheusBridge> MorpheusBridge;
#endif

	/** Constructor */
	FMorpheusHMD();

	/** Destructor */
	virtual ~FMorpheusHMD();

	static const FName MorpheusSystemName;

	/** IXRTrackingSystem interface */
	virtual FName GetSystemName() const override
	{
		return MorpheusSystemName;
	}

	virtual void RebaseObjectOrientationAndPosition(FVector& Position, FQuat& Orientation) const override;

	virtual class IHeadMountedDisplay* GetHMDDevice() override
	{
		return this;
	}

	virtual class TSharedPtr< class IStereoRendering, ESPMode::ThreadSafe > GetStereoRenderingDevice() override
	{
		return AsShared();
	}

	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;
	virtual void OnBeginRendering_GameThread() override;
	virtual void OnBeginRendering_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;

	virtual bool DoesSupportPositionalTracking() const override;
	virtual bool HasValidTrackingPosition() override;
	virtual bool EnumerateTrackedDevices(TArray<int32>& TrackedIds, EXRTrackedDeviceType DeviceType = EXRTrackedDeviceType::Any) override;

	virtual bool GetTrackingSensorProperties(int32 InDeviceId, FQuat& OutOrientation, FVector& OutOrigin, FXRSensorProperties& OutSensorProperties) override;
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition) override;
	virtual bool GetRelativeEyePose(int32 DeviceId, EStereoscopicPass Eye, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual FVector GetAudioListenerOffset(int32 DeviceId = HMDDeviceId) const override;

	virtual void ResetOrientationAndPosition(float yaw = 0.f) override;
	virtual void ResetPosition() override;

	virtual void SetBaseRotation(const FRotator& BaseRot)	{ BaseHmdOrientation = BaseRot.Quaternion(); }
	virtual FRotator GetBaseRotation() const { return BaseHmdOrientation.Rotator(); }
	virtual void SetBaseOrientation(const FQuat& BaseOrient) { BaseHmdOrientation = BaseOrient; }
	virtual FQuat GetBaseOrientation() const { return BaseHmdOrientation; }

protected:
	/** FXRTrackingSystemBase interface */
	virtual float GetWorldToMetersScale() const override { return GetTrackingFrame().WorldToMetersScale; }

public:
	/** @return	True if the HMD was initialized OK */
	bool IsInitialized() const;

	/** IHeadMountedDisplay interface */
	virtual bool IsHMDEnabled() const override;
	virtual bool IsHMDConnected() override;
	virtual EHMDWornState::Type GetHMDWornState() override;
	virtual void EnableHMD(bool bEnable = true) override;
	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;
	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
	virtual float GetInterpupillaryDistance() const override;
	virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 InTexFlags, uint32 InTargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) override;

	virtual bool GetHMDDistortionEnabled(EShadingPath ShadingPath) const override;
    virtual float GetDistortionScalingFactor() const override;
    virtual float GetLensCenterOffset() const override;
    virtual void GetDistortionWarpValues(FVector4& K) const override;
	virtual bool IsChromaAbCorrectionEnabled() const override;
	virtual bool GetChromaAbCorrectionValues(FVector4& K) const override;

	virtual float GetPixelDenity() const override { return PixelDensity; }

	virtual void SetPixelDensity(const float NewDensity) override
	{ 
		check(NewDensity > 0.0f);
		PixelDensity = NewDensity;
	}

	virtual FIntPoint GetIdealRenderTargetSize() const override { return IdealRenderTargetSize; }
		
	/** Morpheus hidden area mask support */
	virtual bool HasHiddenAreaMesh() const override { return HiddenAreaMeshes[0].IsValid() && HiddenAreaMeshes[1].IsValid(); }
	virtual void DrawHiddenAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	virtual bool HasVisibleAreaMesh() const override { return VisibleAreaMeshes[0].IsValid() && VisibleAreaMeshes[1].IsValid(); }
	virtual void DrawVisibleAreaMesh_RenderThread(FRHICommandList& RHICmdList, EStereoscopicPass StereoPass) const override;

	/** Morpheus-specific Distortion Shader Support */
	virtual FVector2D GetTextureOffsetLeft()  const override;
	virtual FVector2D GetTextureOffsetRight() const override;
	virtual FVector2D GetTextureScaleLeft() const override;
	virtual FVector2D GetTextureScaleRight() const override;
#if PLATFORM_WINDOWS
	virtual const float* GetRedDistortionParameters() const override;
	virtual const float* GetGreenDistortionParameters() const override;
	virtual const float* GetBlueDistortionParameters() const override;
#endif

	/** Morpheus 2D VR quad display */
	void Show2DVRSplashScreen(class UTexture* Texture, FVector2D Scale, FVector2D Offset);
	void Show2DVRSplashScreen(FTexture2DRHIRef Texture, FVector2D Scale, FVector2D Offset);
	void Hide2DVRSplashScreen();

	/** Morpheus sceHmdReprojectionSetOutputMinColor */
	void HMDReprojectionSetOutputMinColor(FLinearColor MinColor);

	/** IStereoRendering interface */
	virtual void GetEyeRenderParams_RenderThread(const FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override
	{
		EyeToSrcUVOffsetValue = FVector2D::ZeroVector;
		EyeToSrcUVScaleValue = FVector2D(1.0f, 1.0f);
	}
	virtual bool IsStereoEnabled() const override;
	virtual bool EnableStereo(bool bStereo = true) override;
    virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override;
	virtual void GetOrthoProjection(int32 RTWidth, int32 RTHeight, float OrthoDistance, FMatrix OrthoProjection[2]) const override;
	virtual void InitCanvasFromView(FSceneView* InView, UCanvas* Canvas) override;
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override { return this; }

	virtual IStereoLayers* GetStereoLayers() override;
	virtual void RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* BackBuffer, FRHITexture2D* SrcTexture, FVector2D WindowSize) const override;
	
	void SetupOverlayTargets(FTexture2DRHIRef SceneRenderTarget);
	virtual FTexture2DRHIRef GetOverlayLayerTarget_RenderThread(EStereoscopicPass StereoPass, FIntRect& OutViewport) override;
	virtual FTexture2DRHIRef GetSceneLayerTarget_RenderThread(EStereoscopicPass StereoPass, FIntRect& OutViewport) override;

	/** FXRRenderTargetManager interface */
	virtual uint32 GetNumberOfBufferedFrames() const override { return 3; }
	virtual void UpdateViewportRHIBridge(bool bUseSeparateRenderTarget, const class FViewport& Viewport, FRHIViewport* const ViewportRHI) override;
	virtual bool ShouldUseSeparateRenderTarget() const override
	{
#if PLATFORM_PS4
		return IsStereoEnabled();
#else
		// for some reason, on Windows nothing renders at all if this returns true
		return false;
#endif
	}

	int32 GetHMDHandle() const
	{
#if PLATFORM_PS4
		return HMDHandle;
#else
		return 0;
#endif
	}

#if PLATFORM_PS4

	const IPS4Tracker::FTrackingData& GetLastTrackingData()
	{
		return LastTrackingData;
	}

#endif

private:
	struct FTrackingFrame
	{
		uint32 FrameNumber;

		float WorldToMetersScale;

		// Pose of the device screen (Unreal-space)
		FVector DevicePosition;
		FQuat DeviceOrientation;

		// Pose of each eye itself, 0 = left, 1 = right (Unreal-space)
		FVector EyePosition[2];
		FQuat	EyeOrientation[2];

		// Pose of the head (center of the ears) (Unreal-space)
		FVector HeadPosition;
		FQuat	HeadOrientation;

		FTrackingFrame()
		{
			FrameNumber = 0;
			WorldToMetersScale = 100.0f;
			DevicePosition = FVector::ZeroVector;
			DeviceOrientation = FQuat::Identity;
			EyePosition[0] = EyePosition[1] = FVector::ZeroVector;
			EyeOrientation[0] = EyeOrientation[1] = FQuat::Identity;
			HeadPosition = FVector::ZeroVector;
			HeadOrientation = FQuat::Identity;
		}
	};	

	void FetchTrackingData(FTrackingFrame& TrackingFrame, FQuat& CurrentOrientation, FVector& CurrentPosition, bool bUpdateLastTrackingData, bool bEarlyPoll, bool bPollImmediately);
	void GetCurrentEyePose(const EStereoscopicPass StereoPassType, FQuat& ViewRotation, FVector& ViewLocation);
	const FTrackingFrame& GetTrackingFrame() const;

	/** From target/include/hmd_utility/distortion_canceller_common.h
 	 * @brief Parameters to translate texture for canceling distortion.
	 */
	struct TextureTranslation
	{
		FVector2D Scale;
		FVector2D Offset;
		
		/** @brief Constructor.
		*/
		TextureTranslation();		

		/** @brief Set parameters by tangent values of output image angle from center.
		@param[in]	left	Left side angle of output image from center by tangent.
		@param[in]	right	Right side angle of output image from center by tangent.
		@param[in]	top		Top side angle of output image from center by tangent.
		@param[in]	bottom	Bottom side angle of output image from center by tangent.
		*/
		void SetFromTan(float TanL,float TanR,float TanT,float TanB);
	};	

		
	FTrackingFrame RenderThreadFrame;
	FTrackingFrame GameThreadFrame;	

	FCriticalSection FrameTrackerMutex;

	// SocialScreen
	bool SocialScreenStartup();
	void SocialScreenShutdown();
	void SocialScreen_BeginRenderViewFamily();
	void SocialScreen_BeginRendering_RenderThread();
	void RenderSocialScreen_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* BackBuffer, FTexture2DRHIRef RenderTarget, FVector2D WindowSize) const;
	void UpdateSpectatorScreenMode_RenderThread();

public:
	// Morpheus overrides this because some of its spectator modes are replaced by API level hardware supported mirroring.
	virtual bool IsSpectatorScreenActive() const override;
	virtual FIntRect GetFullFlatEyeRect_RenderThread(FTexture2DRHIRef EyeTexture) const override;
	virtual void CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef SrcTexture, FIntRect SrcRect, FTexture2DRHIParamRef DstTexture, FIntRect DstRect, bool bClearBlack) const override;

	// To do an override acquire a receipt and store it in a shared pointer.  
	// To release it null the shared pointer, or let its destructor release the receipt.
	struct FSocialScreenOverrideReceipt : public TSharedFromThis<FSocialScreenOverrideReceipt, ESPMode::ThreadSafe>
	{
		FSocialScreenOverrideReceipt();
		~FSocialScreenOverrideReceipt();
	};
	TSharedPtr<FSocialScreenOverrideReceipt, ESPMode::ThreadSafe> AcquireSocialScreenOverrideReceipt();
	bool IsSocialScreenOverriddenToMirror() const;
private:
	enum class ESocialScreenState
	{
		Constructed,			// Pre-startup.
		MirrorMode,				// The default for the api, mirrors hmd display to TV for free.  Also system dialogs are visible on the TV only in this mode.  Important for the HMD disconnect screen in particular.
		SeparateMode30FPS,		// We provide video output to Aux, that is displayed on the TV at 30fps.
		//DialogFor60FPSUp,
		//SeparateMode60FPS,	// not yet supported, will require system dialog handling and support to fall back to 30 (60 conflicts with some other system features, eg. streaming video)
		Shutdown,				// Ready for destructor.
		Failed,					// Something went wrong, and we won't recover from it.
	};
	ESocialScreenState DesiredSocialScreenState = ESocialScreenState::Constructed;

	static FThreadSafeCounter SocialScreenOverrideToMirrorCount; // Static so this can work even during the MorpheusHMD constructor.
	bool bSocialScreenOverriddenToMirror_RenderThread = false; // Cache the value we will use at the beginning of the renderframe, so it does not change in the middle.

	FTexture2DRHIRef OverlayLayerRenderTarget[2][STEREO_LAYER_BUFFER_COUNT];
	// This flag prevents us from using the overlay render targets before they have been written to after they are re-created (eg when the target size changes)
	bool OverlayLayerRenderTargetWritten[2][STEREO_LAYER_BUFFER_COUNT];

	uint32 EyeTextureSizeX = 0;
	uint32 EyeTextureSizeY = 0;
	uint32 StereoLayerBufferIndex = 0;

	/**
	 * Starts up the Morpheus device
	 */
	void Startup();
	bool OpenHMD();
	void CloseHmd();
	void DestroyHmd();

#if PLATFORM_PS4
	bool GetHMDDeviceInformation(SceHmdDeviceInformation& Info) const;
#endif
	int32 GetHMDDeviceStatus() const;
	void BlockingHMDSetupDialog();
	bool StartHMDSetupDialog();
	bool UpdateHMDSetupDialog();
	void EndHMDSetupDialog();

	/**
	 * Shuts down Morpheus
	 */
	void Shutdown();

	void LoadFromIni();
	void SaveToIni();

	/** Hidden area mask asset setup */
	void SetupOcclusionMeshes();

	/** Update the tracker with the most appropriate refresh rate */
	void UpdateTrackerPredictionTiming(bool bReprojectionEnabled);

	FHMDViewMesh HiddenAreaMeshes[2];
	FHMDViewMesh VisibleAreaMeshes[2];

	/** Whether or not the HMD hardware is connected, powered, and ready. It may not actually be in use. */
	bool bIsHMDAvailable;

	/** Whether or not the Morpheus was successfully initialized */
	bool bWasInitialized;

	/** Whether or not the HMD is enabled for stereo rendering */
	bool bEnabled;

	/** Whether or not the HMD server is enabled for the PC */
	bool bEnableMorpheusOnPC;

	/** Is the HMD Setup Dialog being shown now (because the HMD needs to be connected) */
	bool bHMDSetupDialogActive;

	/** Receipt for overriding the social screen to mirror while the HMDSetupDialog is up, so that it is visible (system dialogs are not shown on the TV in Separate mode). */
	TSharedPtr<FSocialScreenOverrideReceipt, ESPMode::ThreadSafe> HMDSetupSocialScreenOverrideReceipt;

	/** Is the HMD being worn by the user (according to the api) */
	EHMDWornState::Type HMDWornState;
	
	/** Whether or not we want to have stereo enabled */
	bool bStereoEnabled;

	/** When true bStereoEnabled has been set to false, but we need to run the MorpheusHMD rendering path one more time so it can set us back to 2d rendering. */
	bool bStereoShuttingDown;

	/** Defines the behavior of the app if the user cancels out of the HmdSetupDialog (that pops up when the hmd is disconnected).*/
	EHmdSetupDialogCanceledBehavior HmdSetupDialogCanceledBehavior;
	bool bSuppressHMDSetupDialogAfterCancel;

	/** Debugging:  Whether or not the stereo rendering settings have been manually overridden by an exec command.  They will no longer be auto-calculated */
    bool bOverrideStereo;

	/** If true it is possible to switch to 'separate mode' and show things other than a mirror of the vr output to the social screen. Back buffers will be allocated.*/
	bool bEnableSocialScreenSeparateMode;

	/** whether or not we do head position tracking */
	bool bPositionTrackingEnabled;

	/** If true HMD tracking will be entirely disabled until the HMD has been tracked by the camera.  Orientation based reprojection does not function if the PlayStation VR HMD has never been tracked, so a subtly bad user experience is possible. Delegates are fired to instruct the user. Defaults to true.*/
	bool bDisableHMDOrientationUntilHMDHasBeenTracked;

	/** Whether or not to include controller/pitch roll - may concievably need this for flight sims **/
	bool bControllerPitchEnabled;
	bool bControllerRollEnabled;

	/** Offset for stereo rendering of the HUD to bring it out of the periphery */
	float HudOffset;

	/** Screen center adjustment used when rendering the canvas, to line up better with the eye's default orientation */
	float CanvasCenterOffset;		

	/** Calibrated tracking bases **/
	FQuat BaseHmdOrientation;
	FVector BaseHmdPosition;	

	FRotator DeltaControlRotation;  // used from ApplyHmdRotation
											   
	/** Accumulated Player Orientation **/
	FVector PlayerPosition;

	float WorldToMeterScale;

	TextureTranslation LeftTranslation;
	TextureTranslation RightTranslation;

	MorpheusHMDInfo			MorpheusDeviceInfo;	

	IRendererModule* RendererModule;
	static const int32 CameraDeviceId = (static_cast<int32>(EXRTrackedDeviceType::TrackingReference) << 16);
#if PLATFORM_PS4
	EPS4OutputMode PSVROutputMode;
	sce::Gnm::WrapMode ReprojectionSamplerWrapMode;

	static const int32 INVALID_HMD_HANDLE = -1;

	int32			HMDHandle;

	enum kStereoEye
	{
		kStereoEyeLeft,
		kStereoEyeRight,

		kStereoEyeCount
	};
	SceHmdEyeOffset		EyeStatus[kStereoEyeCount];	
#endif
	int32	TrackerHandle;

	IPS4Tracker::FTrackingData LastTrackingData;

	uint64 TrackerUpdateTime[2];
#if PLATFORM_WINDOWS
	HMDServerHMDDistortionParams HMDDistortionParams;
#endif

	// Windows output device
	int32					MorpheusOutputDeviceEnum[2];	

	bool					bRecenterView;		// Recenter the view, setting only the current position of the HMD as the origin
	bool					bReorientView;
	float					RecenterExtraYaw;

	/** Whether or not current tracking state is valid **/
	/** Would prefer to change GetCurrentOrientationAndPosition to return whether or not it's valid...**/
	bool			bCurrentTrackerStateValid;
	bool			bTrackerStateHasBeenRead;

	FIntPoint IdealRenderTargetSize;
	float PixelDensity;

	/** Handle for accessing the PS4 positional tracking */
	FPSTrackerHandle PS4Tracker;

	void ShowSettingsCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void IPDCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	FMorpheusConsoleCommands ConsoleCommands;
};

#endif //MORPHEUS_SUPPORTED_PLATFORMS