// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GnmBridge.h: Gnm plugin support
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include <gnm/constants.h>

enum class EPS4OutputMode
{
	Standard2D,
#if HAS_MORPHEUS
	MorpheusRender60Scanout120,
	MorpheusRender90Scanout90,
	MorpheusRender120Scanout120,
#endif
};

enum class EPS4SocialScreenOutputMode
{
	Mirroring,
	Separate30FPS//,
	//Separate60FPS // not yet implemented
};

namespace GnmBridge {

struct MorpheusDistortionData
{
	MorpheusDistortionData()
	{

	}

	MorpheusDistortionData(
	FTexture2DRHIRef InEyeTextureL,
	FTexture2DRHIRef InEyeTextureR,
	FTexture2DRHIRef InOverlayTextureL,
	FTexture2DRHIRef InOverlayTextureR,
	uint64_t InTrackerTimestamp,
	uint64_t InSensorReadTimestamp,
	uint32 InFrameNumber,
	const SceFVector3 &InTrackerPosition,
	const SceFQuaternion &InTrackerOrientationQuat,
	float InDeviceTanIn,
	float InDeviceTanOut,
	float InDeviceTanTop,
	float InDeviceTanBottom,
	int32 InMorpheusHandle)
	: EyeTextureL(InEyeTextureL)
	, EyeTextureR(InEyeTextureR)
	, OverlayTextureL(InOverlayTextureL)
	, OverlayTextureR(InOverlayTextureR)
	, TrackerTimestamp(InTrackerTimestamp)
	, SensorReadSystemTimestamp(InSensorReadTimestamp)
	, FrameNumber(InFrameNumber)
	, TrackerPosition(InTrackerPosition)
	, TrackerOrientationQuat(InTrackerOrientationQuat)
	, DeviceTanIn(InDeviceTanIn)
	, DeviceTanOut(InDeviceTanOut)
	, DeviceTanTop(InDeviceTanTop)
	, DeviceTanBottom(InDeviceTanBottom)
	, MorpheusHandle(InMorpheusHandle)
	{		
	}

	FTexture2DRHIRef EyeTextureL;
	FTexture2DRHIRef EyeTextureR;
	FTexture2DRHIRef OverlayTextureL;
	FTexture2DRHIRef OverlayTextureR;
	uint64_t TrackerTimestamp = 0;
	uint64_t SensorReadSystemTimestamp = 0;
	uint32 FrameNumber = 0;
	SceFVector3 TrackerPosition;
	SceFQuaternion TrackerOrientationQuat;
	float DeviceTanIn = 0.0f;
	float DeviceTanOut = 0.0f;
	float DeviceTanTop = 0.0f;
	float DeviceTanBottom = 0.0f;
	int32 MorpheusHandle = -1;
	int32 RHIContextIndex = -1;
	uint32 FrameCounter = 0;
	bool Used = false;
};

struct Morpheus2DVRReprojectionData
{
	Morpheus2DVRReprojectionData()
		: Scale(1.0f, 1.0f)
		, Offset(0.0f, 0.0f)
	{
	}

	Morpheus2DVRReprojectionData(
		FTexture2DRHIRef InTexture,
		FVector2D InScale,
		FVector2D InOffset)
		: Texture(InTexture)
		, Scale(InScale)
		, Offset(InOffset)
	{
	}

	FTexture2DRHIRef Texture;
	FVector2D Scale;
	FVector2D Offset;
	int32 RHIContextIndex = -1;
	uint32 FrameCounter = 0;
};

struct MorpheusApplyReprojectionInfo
{
	uint32_t	FrameNumber = 0;
	uint64		PreviousFlipTime = 0;
};

PS4RHI_API void SetReprojectionSamplerWrapMode(sce::Gnm::WrapMode ReprojectionSamplerWrapMode);
PS4RHI_API void CreateSocialScreenBackBuffers();
PS4RHI_API void ChangeOutputMode(EPS4OutputMode InMode);
PS4RHI_API void ChangeSocialScreenOutputMode(EPS4SocialScreenOutputMode InMode);
PS4RHI_API void CacheReprojectionData(MorpheusDistortionData& DistortionData);
PS4RHI_API void StartMorpheus2DVRReprojection(Morpheus2DVRReprojectionData& ReprojectionData);
PS4RHI_API void StopMorpheus2DVRReprojection();

PS4RHI_API EPS4OutputMode GetOutputMode();
PS4RHI_API EPS4SocialScreenOutputMode GetSocialScreenOutputMode();
PS4RHI_API FTexture2DRHIRef GetSocialScreenRenderTarget();
PS4RHI_API bool ShouldRenderSocialScreenThisFrame();
PS4RHI_API void TranslateSocialScreenOutput(FRHICommandListImmediate& RHICmdList);
PS4RHI_API uint64 GetLastFlipTime();
PS4RHI_API uint32 GetVideoOutPort();
PS4RHI_API void GetLastApplyReprojectionInfo(GnmBridge::MorpheusApplyReprojectionInfo& OutInfo);


}//GnmBridge