// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneRendering.h"
#include "RenderCore.h"
#include "TranslucentRendering.h"

void FMobileSceneRenderer::RenderTranslucency(FRHICommandList& RHICmdList, const FViewInfo& View)
{	
	const bool bShouldRenderTranslucency = ShouldRenderTranslucency(StandardTranslucencyPass) && ViewFamily.EngineShowFlags.Translucency;
	if (bShouldRenderTranslucency)
	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RenderTranslucency);
		SCOPE_CYCLE_COUNTER(STAT_TranslucencyDrawTime);
		SCOPED_DRAW_EVENT(RHICmdList, Translucency);
		SCOPED_GPU_STAT(RHICmdList, Translucency);

		// BEGIN META SECTION - Multi-View Per View Viewports / Render Areas
		SetViewport(RHICmdList, View);
		// END META SECTION - Multi-View Per View Viewports / Render Areas

		View.ParallelMeshDrawCommandPasses[StandardTranslucencyMeshPass].DispatchDraw(nullptr, RHICmdList, &MeshPassInstanceCullingDrawParams[StandardTranslucencyMeshPass]);
	}
}
