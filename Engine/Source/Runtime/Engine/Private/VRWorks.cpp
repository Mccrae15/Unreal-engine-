// CopyRight 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VRWorks.cpp: Implements the VR Works utility functions.
=============================================================================*/

#include "VRWorks.h"
#include "RHI.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarMultiRes(
	TEXT("vr.MultiRes"),
	0,
	TEXT("0 to disable MultiRes support, 1 to enable."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMultiResRendering(
	TEXT("vr.MultiResRendering"),
	0,
	TEXT("Multi-resolution rendering level:\n")
	TEXT("0: off (default)\n")
	TEXT("1: conservative (saves 28% pixels)\n")
	TEXT("2: balanced (saves 42% pixels)\n")
	TEXT("3: aggressive (saves 60% pixels)\n")
	TEXT("Press Numpad0 to cycle between settings."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarLensMatchedShading(
	TEXT("vr.LensMatchedShading"),
	0,
	TEXT("0 to disable ModifiedW support, 1 to enable."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarLensMatchedShadingRendering(
	TEXT("vr.LensMatchedShadingRendering"),
	0,
	TEXT("Lens Matched Shading rendering toggle:\n")
	TEXT("0: off (default)\n")
	TEXT("1: quality\n")
	TEXT("2: conservative\n")
	TEXT("3: aggressive\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarLensMatchedShadingUnwarpScale(
	TEXT("vr.LensMatchedShadingUnwarpScale"),
	1.5f,
	TEXT("Since Lens matched shading super samples the center region compared with the original resolution,\n")
	TEXT("It's preferred to perform unwarp to a larger render target so the super sampled information is preserved.\n")
	TEXT("The default value is 1.5. It's a conservative estimate of increased shading rate. The default value should be used for most of the times.\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarLensMatchedShadingDrawRectangleOptimization(
	TEXT("vr.LensMatchedShadingRectangleOptimization"),
	1,
	TEXT("Lens Matched Shading rectangle optimization replaces the full screen rectangle to an octagon:\n")
	TEXT("1.0f (default)\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSinglePassStereo(
	TEXT("vr.SinglePassStereo"),
	0,
	TEXT("0 to disable SinglePassStereo support, 1 to enable."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSinglePassStereoRendering(
	TEXT("vr.SinglePassStereoRendering"),
	0,
	TEXT("Enable SinglePassStereo\n")
	TEXT("0: off (default)\n")
	TEXT("1: on"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarEnableVRSLI(
	TEXT("vr.MGPU"),
	0,
	TEXT("Toggles VR SLI support.\n")
	TEXT("0: off (default)\n")
	TEXT("1: on (requires NVAPI support for SetGPUMask)\n"),
	ECVF_RenderThreadSafe);

static void ToggleMultiResLevel()
{
	int32 Value = CVarMultiResRendering.GetValueOnAnyThread();
	Value = (Value + 1) % 3;
	CVarMultiResRendering.AsVariable()->Set(Value, ECVF_SetByConsole);
}

static FAutoConsoleCommand ToggleMultiResLevelCmd(
	TEXT("ToggleMultiResLevel"),
	TEXT("Cycle through levels of multires."),
	FConsoleCommandDelegate::CreateStatic(ToggleMultiResLevel)
	);


bool FVRWorks::IsFastGSNeeded()
{
	static const bool bMultiResShaders = CVarMultiRes.GetValueOnAnyThread() != 0;
	static const bool bSPSShaders = CVarSinglePassStereo.GetValueOnAnyThread() != 0;
	static const bool bLMSShaders = CVarLensMatchedShading.GetValueOnAnyThread() != 0;
	const bool bFastGS = (bMultiResShaders || bSPSShaders || bLMSShaders);
	return bFastGS;
}

bool FVRWorks::IsMultiResSupportEnabled()
{
	return CVarMultiRes.GetValueOnAnyThread() > 0;
}

int FVRWorks::GetMultiResRenderingLevel()
{
	return (GSupportsFastGeometryShader && CVarMultiRes.GetValueOnAnyThread() > 0)
		? CVarMultiResRendering.GetValueOnAnyThread() > 0
		: 0;
}

bool FVRWorks::IsSinglePassStereoSupportEnabled()
{
	return CVarSinglePassStereo.GetValueOnAnyThread() > 0;
}

bool FVRWorks::IsSinglePassStereoRenderingEnabled()
{
	return GSupportsSinglePassStereo && CVarSinglePassStereo.GetValueOnAnyThread() > 0 
		&& CVarSinglePassStereoRendering.GetValueOnAnyThread() > 0;
}

void FVRWorks::DisableSinglePassStereoRendering()
{
	CVarSinglePassStereoRendering.AsVariable()->Set(0, ECVF_SetByConsole);
}

bool FVRWorks::IsLensMatchedShadingSupportEnabled()
{
	return CVarLensMatchedShading.GetValueOnAnyThread() > 0;
}

int FVRWorks::GetLensMatchedShadingLevel()
{
	return (GSupportsFastGeometryShader && GSupportsModifiedW
		&& CVarLensMatchedShading.GetValueOnAnyThread() > 0)
		? CVarLensMatchedShadingRendering.GetValueOnAnyThread() > 0
		: 0;
}

bool FVRWorks::IsOctagonOptimizationEnabled()
{
	return GetLensMatchedShadingLevel() > 0
		&& CVarLensMatchedShadingDrawRectangleOptimization.GetValueOnAnyThread() > 0;
}

float FVRWorks::GetLensMatchedShadingUnwarpScale()
{
	return GetLensMatchedShadingLevel() > 0
		? CVarLensMatchedShadingUnwarpScale.GetValueOnAnyThread()
		: 1.f;
}

bool FVRWorks::IsVRSLIEnabled()
{
	return GNumExplicitGPUsForRendering > 1 && CVarEnableVRSLI.GetValueOnAnyThread() > 0;
}
