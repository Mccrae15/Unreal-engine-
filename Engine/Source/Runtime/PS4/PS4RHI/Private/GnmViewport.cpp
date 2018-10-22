// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmViewport.cpp: Gnm viewport RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"



FGnmViewport::FGnmViewport(void* WindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen):
	SizeX(InSizeX),
	SizeY(InSizeY),
	bIsFullscreen(bInIsFullscreen),
	bIsValid(true)
{
	// allocate the back buffers at the viewport size
	GGnmManager.CreateBackBuffers(SizeX, SizeY);

#if PLATFORM_WINDOWS
	ShowWindow((HWND)WindowHandle, SW_SHOWNORMAL);
#endif
}

FGnmViewport::~FGnmViewport()
{
}

void FGnmViewport::SetCustomPresent(FRHICustomPresent* InCustomPresent)
{
	GGnmManager.SetCustomPresent(InCustomPresent);
}

void FGnmViewport::Resize(uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen)
{
	GGnmManager.RecreateBackBuffers(InSizeX, InSizeY);
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FGnmDynamicRHI::RHICreateViewport(void* WindowHandle,uint32 SizeX,uint32 SizeY,bool bIsFullscreen,EPixelFormat PreferredPixelFormat /* ignored */)
{
	check( IsInGameThread() );
	return new FGnmViewport(WindowHandle, SizeX, SizeY, bIsFullscreen);
}

void FGnmDynamicRHI::RHIResizeViewport(FViewportRHIParamRef ViewportRHI,uint32 SizeX,uint32 SizeY,bool bIsFullscreen)
{
	check( IsInGameThread() );

	FGnmViewport* Viewport = ResourceCast(ViewportRHI);

	Viewport->Resize(SizeX,SizeY,bIsFullscreen);
}

void FGnmDynamicRHI::RHITick( float DeltaTime )
{
	check( IsInGameThread() );

}

/*=============================================================================
 *	Viewport functions.
 *=============================================================================*/

void FGnmCommandListContext::RHIBeginDrawingViewport(FViewportRHIParamRef ViewportRHI, FTextureRHIParamRef RenderTargetRHI)
{
	check(IsImmediate());
//	FGnmViewport* Viewport = ResourceCast(ViewportRHI);

	// @todo gnm render targets  - set the render target as active
	GGnmManager.BeginFrame();

	//don't need a formal barrier because we're n-buffering the backbuffer.
	FGnmTexture2D* BackBuffer = GGnmManager.GetBackBuffer(false);
	GnmContext->setupScreenViewport(0, 0, BackBuffer->GetSizeX(), BackBuffer->GetSizeY(), 0.5, 0.5);
	BackBuffer->Surface.SetCurrentGPUAccess(EResourceTransitionAccess::EWritable);
	BackBuffer->Surface.SetDirty(true, GGnmManager.GetFrameCount());

	FRHIRenderTargetView RTV(BackBuffer, ERenderTargetLoadAction::ELoad); // in the non-rhi thread case, BDV is a flush command, which means either copy of the current back buffer is fine.
	RHISetRenderTargets(1, &RTV, NULL, 0, NULL);
}

void FGnmCommandListContext::RHIEndDrawingViewport(FViewportRHIParamRef ViewportRHI, bool bPresent, bool bLockToVsync)
{
	check(IsImmediate());
//	FGnmViewport* Viewport = ResourceCast(ViewportRHI);
	QUICK_SCOPE_CYCLE_COUNTER(FGnmDynamicRHI_RHIEndDrawingViewport);
	GGnmManager.EndFrame(bPresent, false);
}

FTexture2DRHIRef FGnmDynamicRHI::RHIGetViewportBackBuffer(FViewportRHIParamRef ViewportRHI)
{
//	FGnmViewport* Viewport = ResourceCast(ViewportRHI);
	check(IsInRenderingThread());
	return GGnmManager.GetBackBuffer(true);
}


FRHIFlipDetails FGnmDynamicRHI::RHIWaitForFlip(double TimeoutInSeconds)
{
	return GGnmManager.WaitForFlip(TimeoutInSeconds);
}

void FGnmDynamicRHI::RHISignalFlipEvent() 
{
	GGnmManager.SignalFlipEvent();
}

FRHIFlipDetails FGnmManager::WaitForFlip(double TimeoutInSeconds)
{
	// Negative timeout means infinite.
	// Timeout should be nullptr if we are waiting infinitely.
	SceKernelUseconds* Timeout = nullptr;
	SceKernelUseconds TimeoutInMicroseconds;
	if (TimeoutInSeconds >= 0.0)
	{
		TimeoutInMicroseconds = SceKernelUseconds(TimeoutInSeconds * 1000000.0);
		Timeout = &TimeoutInMicroseconds;
	}
	
	// Wait for the next flip event
	SceKernelEvent Event;
	int32 NumEvents;
	sceKernelWaitEqueue(VideoOutQueue, &Event, 1, &NumEvents, Timeout);

	FRHIFlipDetails FlipDetails;

	// Return the index and timing details of the most recently flipped frame.
	SceVideoOutVblankStatus VBlankStatus = {};
	SceVideoOutFlipStatus FlipStatus = {};

	verify(sceVideoOutGetVblankStatus(VideoOutHandle, &VBlankStatus) == SCE_OK);
	verify(sceVideoOutGetFlipStatus(VideoOutHandle, &FlipStatus) == SCE_OK);

	if (VBlankStatus.processTime <= FlipStatus.processTime)
	{
		// A more recent flip has happened
		LastVBlankProcessTime = VBlankStatus.processTime;
	}

	// ProcessTime is in microseconds.
	const double ProcessTimeToSeconds = 1.0 / 1000000.0;
	FlipDetails.FlipTimeInSeconds = FlipStatus.processTime * ProcessTimeToSeconds;
	FlipDetails.VBlankTimeInSeconds = LastVBlankProcessTime * ProcessTimeToSeconds;
	FlipDetails.PresentIndex = (FlipStatus.flipArg == -1) ? 0 : FlipStatus.flipArg;

	return FlipDetails;
}

void FGnmManager::SignalFlipEvent()
{
	// Wake up the flip tracking thread by signaling the flip event.
	verify(sceKernelTriggerUserEvent(VideoOutQueue, PS4_FLIP_USER_EVENT_ID, nullptr) == SCE_OK);
}
