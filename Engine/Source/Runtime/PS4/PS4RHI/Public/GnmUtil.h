// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmUtil.h: Gnm RHI utility definitions.
=============================================================================*/

#pragma once

#include "GPUProfiler.h"

// Do we want to track resources?
// This wont' work under parallel rhi thread.
// Also won't work with shader stripping.
// todo: mw fix for parallel rhi thread.
#define ENABLE_GNM_VERIFICATION 0

// Do we want to enable checking (by CPU) for an overwrite to memory
#define ENABLE_MEMORY_CHECKING 0

// Do we want to check for render target / shader output format mismatch
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
#define ENABLE_RENDERTARGET_VERIFICATION 1
#else
#define ENABLE_RENDERTARGET_VERIFICATION 0
#endif

#if ENABLE_GNM_VERIFICATION

/** When we initialize the hardware state, we unbind everything */
void ResetBoundResources();

/** Start tracking shaders and their resources */
void StartVertexShaderVerification(FGnmVertexShader* Shader);
void StartPixelShaderVerification(FGnmPixelShader* Shader);

/** Mark that a resource slot was set */
void TrackResource(uint32 ResourceType, uint32 ResourceIndex, Gnm::ShaderStage Stage);

/** Check to see that all resources have been set by now, and report if not */
void EndShaderVerification();

#else

#define ResetBoundResources(...)
#define StartVertexShaderVerification(...)
#define StartPixelShaderVerification(...)
#define TrackResource(...)
#define EndShaderVerification(...)

#endif

#if ENABLE_RENDERTARGET_VERIFICATION
void RequestRenderTargetVerification();
void VerifyRenderTargets(FGnmCommandListContext& GnmCommandContext, FGnmBoundShaderState* BoundShaderState);
#else
#define RequestRenderTargetVerification(...)
#define VerifyRenderTargets(...)
#endif

class FGnmGPUTiming : public FGPUTiming
{
public:
	FGnmGPUTiming() : 
		bIsTiming(false),
		bEndTimestampIssued(false)
	{}

	/**
	 * Start a GPU timing measurement.
	 */
	void StartTiming();

	/**
	 * End a GPU timing measurement.
	 * The timing for this particular measurement will be resolved at a later time by the GPU.
	 */
	void EndTiming();

	/**
	 * Retrieves the most recently resolved timing measurement.
	 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
	 *
	 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
	 */
	uint64 GetTiming(bool bGetCurrentResultsAndBlock = false);

	/**
	 * Initializes all Gnm resources.
	 */
	void Initialize();

	/**
	 * Releases all Gnm resources.
	 */
	void Release();

	bool IsComplete() const
	{
		check(bEndTimestampIssued);
		return *((uint64*)EndTimestamp.GetPointer()) != 0;
	}

private:
	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData);

	/** Timestamps for all StartTimings. */
	FMemBlock	StartTimestamp;
	/** Timestamps for all EndTimings. */
	FMemBlock	EndTimestamp;
	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	bool bIsTiming;
	bool bEndTimestampIssued;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FGnmEventNode : public FGPUProfilerEventNode
{
public:
	FGnmEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) :
		FGPUProfilerEventNode(InName, InParent)
	{
		// Initialize Buffered timestamp queries 
		Timing.Initialize();
	}

	virtual ~FGnmEventNode()
	{
		Timing.Release(); 
	}

	/** 
	 * Returns the time in ms that the GPU spent in this draw event.  
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	virtual float GetTiming() override;


	virtual void StartTiming() override
	{
		Timing.StartTiming();
	}

	virtual void StopTiming() override
	{
		Timing.EndTiming();
	}

	FGnmGPUTiming Timing;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FGnmEventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:

	FGnmEventNodeFrame()
	{
		RootEventTiming.Initialize();
	}

	~FGnmEventNodeFrame()
	{
		RootEventTiming.Release();
	}

	/** Start this frame of per tracking */
	virtual void StartFrame() override;

	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() override;

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override;

	virtual bool PlatformDisablesVSync() const { return true; }

	/** Timer tracking inclusive time spent in the root nodes. */
	FGnmGPUTiming RootEventTiming;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FGnmGPUProfiler : public FGPUProfiler
{
	/** GPU hitch profile histories */
	TIndirectArray<FGnmEventNodeFrame> GPUHitchEventNodeFrames;

	FGnmGPUProfiler() :
		FGPUProfiler(), 
		bCommandlistSubmitted(false),
		bTracing(false)
	{
	}

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override
	{
		FGnmEventNode* EventNode = new FGnmEventNode(InName, InParent);
		return EventNode;
	}

	virtual void PushEvent(const TCHAR* Name, FColor Color) override;
	virtual void PopEvent() override;

	void BeginFrame();

	void EndFrameBeforeSubmit();
	void EndFrame();

	void BeginTrace();
	void EndTrace();
	void FinalizeTrace();

	bool bCommandlistSubmitted;
	
	bool bTracing;
	volatile uint64_t *GPUTraceLabel;
};

/** 
 * Allocates memory to inspect for writes (if ENABLE_MEMORY_CHECKING is on)
 */
void AllocateMemoryCheckBlock(uint64 Size=0, uint8 Fill=0x80);

/**
 * Looks at the memory allocated in AllocateMemoryCheckBlock for writes (and will break/crash if there are)
 */
void ValidateMemoryCheckBlock(bool bBlockCPUUntilGPUIdle=true);
