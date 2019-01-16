#include "MixedRealityRenderer.h"
#include <Engine/Engine.h>
#include <Kismet/KismetRenderingLibrary.h>
#include <Kismet/KismetMathLibrary.h>
#include <SteamVRFunctionLibrary.h>
#include <cassert>
#include "Compatibility.h"
#include "SharedTextureProtocol.h"

#ifdef UE_4_17_OR_LATER
#include <HeadMountedDisplayFunctionLibrary.h>
#else
#include <Kismet/HeadMountedDisplayFunctionLibrary.h>
#endif

UMixedRealityRenderer::UMixedRealityRenderer()
	: CapturePostprocessType(ECapturePostprocessType::NoPostprocess)
#ifdef UE_4_17_OR_LATER
	, PrimitiveRenderMode(ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives)
#endif
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool UMixedRealityRenderer::InitializeIfPossible()
{
	if (bIsInitialized)
		return true;

	const auto Owner = GetOwner();
	const auto PlayerPawn = Cast<APawn>(Owner);

	verifyf(PlayerPawn != nullptr, TEXT("MixedRealityRenderer must be a child of a Pawn!"));

	if (!PlayerPawn->IsControlled())
		return false;

	if (!PlayerPawn->IsLocallyControlled())
	{
		DestroyComponent();
		return false;
	}

	Initialize();

	return true;
}

void UMixedRealityRenderer::Initialize()
{
	if (bIsInitialized)
		return;

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Calibration = GetWorld()->SpawnActor<ACalibration>(SpawnInfo);

	const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, false);

	ExternalCamera = NewObject<USceneComponent>(this);
	ExternalCamera->RegisterComponentWithWorld(GetWorld());
	ExternalCamera->AttachToComponent(this, AttachmentRules);
	ExternalCamera->SetVisibility(false);

	// Spawn scene capture components
	BGCapture = NewObject<USceneCaptureComponent2D>(this);
	BGCapture->RegisterComponentWithWorld(GetWorld());
	BGCapture->AttachToComponent(ExternalCamera, AttachmentRules);
	BGCapture->FOVAngle = Calibration->FOV;
	BGCapture->HiddenActors = HiddenActors;
	BGCapture->HiddenComponents = HiddenComponents;
	BGCapture->ShowOnlyActors = ShowOnlyActors;
	BGCapture->ShowOnlyComponents = ShowOnlyComponents;

	FGCapture = NewObject<USceneCaptureComponent2D>(this);
	FGCapture->RegisterComponentWithWorld(GetWorld());
	FGCapture->AttachToComponent(ExternalCamera, AttachmentRules);
	FGCapture->FOVAngle = Calibration->FOV;
	FGCapture->HiddenActors = HiddenActors;
	FGCapture->HiddenComponents = HiddenComponents;
	FGCapture->ShowOnlyActors = ShowOnlyActors;
	FGCapture->ShowOnlyComponents = ShowOnlyComponents;
	FGCapture->bEnableClipPlane = true;

	switch (CapturePostprocessType)
	{
	case ECapturePostprocessType::NoPostprocess:
		BGCapture->CaptureSource = SCS_SceneColorHDR;
		FGCapture->CaptureSource = SCS_SceneColorHDR;
		PostFGCapture = nullptr;
		break;

	case ECapturePostprocessType::BackgroundPostprocess:
		BGCapture->CaptureSource = SCS_FinalColorLDR;
		FGCapture->CaptureSource = SCS_SceneColorHDR;
		PostFGCapture = nullptr;
		break;

	case ECapturePostprocessType::FullPostprocess:
		BGCapture->CaptureSource = SCS_FinalColorLDR;
		FGCapture->CaptureSource = SCS_SceneColorHDR;
		PostFGCapture = NewObject<USceneCaptureComponent2D>(this);
		PostFGCapture->RegisterComponentWithWorld(GetWorld());
		PostFGCapture->AttachToComponent(ExternalCamera, AttachmentRules);
		PostFGCapture->FOVAngle = Calibration->FOV;
		PostFGCapture->HiddenActors = HiddenActors;
		PostFGCapture->HiddenComponents = HiddenComponents;
		PostFGCapture->ShowOnlyActors = ShowOnlyActors;
		PostFGCapture->ShowOnlyComponents = ShowOnlyComponents;
		PostFGCapture->bEnableClipPlane = true;
		PostFGCapture->CaptureSource = SCS_FinalColorLDR;
		break;
	}

	bIsInitialized = true;
}

void UMixedRealityRenderer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsOpenVRActive())
		return;

	if (!bIsInitialized && !InitializeIfPossible())
		return;


	const auto IsProtocolActive = SharedTextureProtocol::IsActive();


	if (!bWasProtocolActive && IsProtocolActive) // Just activated!
		CreateRenderTargets(SharedTextureProtocol::GetWidth(), SharedTextureProtocol::GetHeight());

	if (bWasProtocolActive && !IsProtocolActive) // Just shutdown!
		ReleaseRenderTargets();

	bWasProtocolActive = IsProtocolActive;


	if (MixedRealityCanvas == nullptr || !IsProtocolActive)
		return;


	if (OpenVRDeviceId == vr::k_unTrackedDeviceIndexInvalid || !IsDeviceConnected(OpenVRDeviceId))
	{
		OpenVRDeviceId = GetOpenVRDeviceIndex();
	}

	if (OpenVRDeviceId != vr::k_unTrackedDeviceIndexInvalid)
	{
		FVector ExternalCameraPosition;
		FRotator ExternalCameraRotation;

		USteamVRFunctionLibrary::GetTrackedDevicePositionAndOrientation(
			OpenVRDeviceId,
			ExternalCameraPosition,
			ExternalCameraRotation
		);

		ExternalCamera->SetRelativeLocationAndRotation(ExternalCameraPosition, ExternalCameraRotation);
	}


	const auto WorldToMeters = UHeadMountedDisplayFunctionLibrary::GetWorldToMetersScale(GetWorld());

	auto CameraLocationOffset = FVector(0);
	auto CameraRotationOffset = FRotator(ForceInitToZero);
	auto CameraNormalRotator = ExternalCamera->GetComponentRotation();

	if (Calibration->IsConfigAvailable)
	{
		CameraLocationOffset = Calibration->LocationOffset * WorldToMeters;
		CameraRotationOffset = Calibration->RotationOffset;

		CameraNormalRotator = UKismetMathLibrary::ComposeRotators(
			Calibration->RotationOffset,
			ExternalCamera->GetComponentRotation()
		);
	}

	const auto CameraNormal = (CameraNormalRotator.Vector() * -1).GetSafeNormal();

	BGCapture->SetRelativeLocation(CameraLocationOffset);
	BGCapture->SetRelativeRotation(CameraRotationOffset);
#ifdef UE_4_17_OR_LATER
	BGCapture->PrimitiveRenderMode = PrimitiveRenderMode;
#endif

	FGCapture->SetRelativeLocation(CameraLocationOffset);
	FGCapture->SetRelativeRotation(CameraRotationOffset);
#ifdef UE_4_17_OR_LATER
	FGCapture->PrimitiveRenderMode = PrimitiveRenderMode;
#endif
	FGCapture->ClipPlaneNormal = CameraNormal;
	FGCapture->ClipPlaneBase = GetOpenVRHMDLocation();

	if (PostFGCapture)
	{
		PostFGCapture->SetRelativeLocation(CameraLocationOffset);
		PostFGCapture->SetRelativeRotation(CameraRotationOffset);
#ifdef UE_4_17_OR_LATER
		PostFGCapture->PrimitiveRenderMode = PrimitiveRenderMode;
#endif
		PostFGCapture->ClipPlaneNormal = CameraNormal;
		PostFGCapture->ClipPlaneBase = FGCapture->ClipPlaneBase;
	}

	MixedRealityCanvas->UpdateResource();
}

void UMixedRealityRenderer::CreateRenderTargets(const int Width, const int Height)
{
	// Create canvas to draw output to
	const auto Canvas = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(
		GetWorld(),
		UMixedRealityCanvas::StaticClass(),
		Width,
		Height
	);
	MixedRealityCanvas = dynamic_cast<UMixedRealityCanvas*>(Canvas);

	// Create background texture
	const auto BGRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		GetWorld(),
		Width,
		Height / 2
	);
	BGCapture->TextureTarget = BGRenderTarget;
	MixedRealityCanvas->BackgroundTexture = BGRenderTarget;

	// Create foreground texture
	const auto FGRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		GetWorld(),
		Width,
		Height / 2
	);
	FGCapture->TextureTarget = FGRenderTarget;
	MixedRealityCanvas->ForegroundTexture = FGRenderTarget;

	switch (CapturePostprocessType)
	{
	case ECapturePostprocessType::NoPostprocess:
		MixedRealityCanvas->bPostprocessBackground = false;
		MixedRealityCanvas->bPostprocessForeground = false;
		break;

	case ECapturePostprocessType::BackgroundPostprocess:
		BGRenderTarget->TargetGamma = 2.2;

		MixedRealityCanvas->bPostprocessBackground = true;
		MixedRealityCanvas->bPostprocessForeground = false;
		break;

	case ECapturePostprocessType::FullPostprocess:
		BGRenderTarget->TargetGamma = 2.2;

		if (PostFGCapture)
		{
			const auto PostFGRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
				GetWorld(),
				Width,
				Height / 2
			);
			PostFGCapture->TextureTarget = PostFGRenderTarget;
			MixedRealityCanvas->PostprocessedForegroundTexture = PostFGRenderTarget;
			PostFGRenderTarget->TargetGamma = 2.2;
		}

		MixedRealityCanvas->bPostprocessBackground = true;
		MixedRealityCanvas->bPostprocessForeground = true;
		break;
	}

	MixedRealityCanvas->UpdateMaterialsTextures();
}
void UMixedRealityRenderer::ReleaseRenderTargets()
{
	if (MixedRealityCanvas)
	{
		MixedRealityCanvas->ReleaseResource();
		MixedRealityCanvas = nullptr;
	}

	if (BGCapture && BGCapture->TextureTarget)
	{
		BGCapture->TextureTarget->ReleaseResource();
		BGCapture->TextureTarget = nullptr;
	}

	if (FGCapture && FGCapture->TextureTarget)
	{
		FGCapture->TextureTarget->ReleaseResource();
		FGCapture->TextureTarget = nullptr;
	}

	if (PostFGCapture && PostFGCapture->TextureTarget)
	{
		PostFGCapture->TextureTarget->ReleaseResource();
		PostFGCapture->TextureTarget = nullptr;
	}
}

bool UMixedRealityRenderer::IsOpenVRActive()
{
	bool IsVRAvailable;

#ifdef UE_4_17_OR_LATER
	IsVRAvailable = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
#else
	IsVRAvailable = GEngine->HMDDevice.IsValid();
#endif

	if (!IsVRAvailable) return false;

	return LIVCompatibility::OpenVRCompositor() != nullptr;
}

FVector UMixedRealityRenderer::GetOpenVRHMDLocation() const
{
	FVector HMDPosition;
	FRotator HMDOrientation;

	USteamVRFunctionLibrary::GetTrackedDevicePositionAndOrientation(
		vr::k_unTrackedDeviceIndex_Hmd,
		HMDPosition,
		HMDOrientation
	);

	return GetComponentTransform().TransformPosition(HMDPosition);
}

uint32_t UMixedRealityRenderer::GetOpenVRDeviceIndex()
{
	const auto VRCompositor = LIVCompatibility::OpenVRCompositor();

	vr::TrackedDevicePose_t RenderPoses[vr::k_unMaxTrackedDeviceCount];
	vr::TrackedDevicePose_t Poses[vr::k_unMaxTrackedDeviceCount];

	auto VRError = VRCompositor->GetLastPoses(
		RenderPoses, vr::k_unMaxTrackedDeviceCount,
		Poses, vr::k_unMaxTrackedDeviceCount
	);
	assert(VRError == vr::VRCompositorError_None);

	uint32_t ChosenDevice = vr::k_unTrackedDeviceIndexInvalid;
	auto ChosenDeviceWeight = 0;

	for (uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i)
	{
		if (!Poses[i].bDeviceIsConnected) continue;

		const auto Weight = GetOpenVRDeviceWeight(i);

		if (Weight > ChosenDeviceWeight)
		{
			ChosenDevice = i;
			ChosenDeviceWeight = Weight;
		}
	}

	return ChosenDevice;
}

int UMixedRealityRenderer::GetOpenVRDeviceWeight(const uint32_t DeviceIndex)
{
	const auto VRSystem = LIVCompatibility::OpenVRSystem();

	const auto DeviceClass = static_cast<int>(VRSystem->GetTrackedDeviceClass(DeviceIndex));
	const auto DeviceRole = static_cast<int>(VRSystem->GetControllerRoleForTrackedDeviceIndex(DeviceIndex));

	if (DeviceClass != LIVCompatibility::TrackedDeviceClassController &&
		DeviceClass != LIVCompatibility::TrackedDeviceClassTracker)
		return 0;

	char DeviceModel[vr::k_unMaxPropertyStringSize];

	const auto ModelLength = VRSystem->GetStringTrackedDeviceProperty(
		DeviceIndex,
		vr::Prop_ModelNumber_String,
		DeviceModel,
		vr::k_unMaxPropertyStringSize
	);

	if (ModelLength != 0)
	{
		if (strcmp(DeviceModel, "LIV Virtual Camera") == 0) return 10;
		if (strcmp(DeviceModel, "Virtual Controller Device") == 0) return 9;
	}

	if (DeviceClass == LIVCompatibility::TrackedDeviceClassTracker) return 5;

	if (DeviceClass == LIVCompatibility::TrackedDeviceClassController)
	{
		char DeviceRenderModel[vr::k_unMaxPropertyStringSize];

		const auto RenderModelLength = VRSystem->GetStringTrackedDeviceProperty(
			DeviceIndex,
			vr::Prop_RenderModelName_String,
			DeviceRenderModel,
			vr::k_unMaxPropertyStringSize
		);

		if (RenderModelLength != 0 && strcmp(DeviceRenderModel, "{htc}vr_tracker_vive_1_0") == 0)
			return 8;

		if (DeviceRole == LIVCompatibility::TrackedDeviceRoleOptOut) return 7;
		if (DeviceRole == LIVCompatibility::TrackedDeviceRoleInvalid) return 6;

		return 1;
	}

	return 0;
}

bool UMixedRealityRenderer::IsDeviceConnected(const uint32_t DeviceIndex)
{
	const auto VRCompositor = LIVCompatibility::OpenVRCompositor();

	vr::TrackedDevicePose_t RenderPoses[vr::k_unMaxTrackedDeviceCount];
	vr::TrackedDevicePose_t Poses[vr::k_unMaxTrackedDeviceCount];

	auto VRError = VRCompositor->GetLastPoses(
		RenderPoses, vr::k_unMaxTrackedDeviceCount,
		Poses, vr::k_unMaxTrackedDeviceCount
	);
	assert(VRError == vr::VRCompositorError_None);

	return DeviceIndex < vr::k_unMaxTrackedDeviceCount && Poses[DeviceIndex].bDeviceIsConnected;
}
