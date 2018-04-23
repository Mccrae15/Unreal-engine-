// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmManager.h: Class to wrap around libgnm
=============================================================================*/

#pragma once 

#include <video_out.h>
#include <kernel.h>
#include "GnmTempBlockAllocator.h"
#include "GnmBridge.h"
#include "PS4GPUDefragAllocator.h"
#include "GnmUtil.h"

/** How many back buffers to cycle through */
#define NUM_RENDER_BUFFERS 2

/** How many social screen back buffers.  Sony recommends 3 to avoid any impact on the VR framerate.*/
#define NUM_AUX_RENDER_BUFFERS 3

/** How many back buffers to cycle through */
#define NUM_TEMP_RING_BUFFERS 2

/** How many gfx contexts to write GPU commands to*/
#define NUM_GFX_CONTEXTS 2

#if (UE_BUILD_TEST || UE_BUILD_SHIPPING)
#define PS4_RECORD_SUBMITS 0
#else
#define PS4_RECORD_SUBMITS 1
#endif

/** 
 * Custom event ID used to manually signal the VideoOutQueue.
 * Used by the RHI flip tracker to wake the thread blocked on the vsync.
 */
#define PS4_FLIP_USER_EVENT_ID 0x1000

#if ENABLE_SUBMITWAIT_TIMING
/** Class for tracking timestamps for recording bubbles between commandlist submissions */
class FGnmSubmissionGapRecorder
{
	struct FGapSpan
	{
		uint32 BeginCycles;
		uint32 DurationCycles;
	};
	struct FFrame
	{
		FFrame() { bIsValid = false; bSafeToReadOnRenderThread = false;  FrameNumber = -1; }

		TArray<FGapSpan> GapSpans;
		uint32 FrameNumber;
		uint32 TotalWaitCycles;
		uint32 StartCycles;
		uint32 EndCycles;
		bool bIsValid;
		bool bSafeToReadOnRenderThread;
	};
public:
	FGnmSubmissionGapRecorder();

	// Submits the gap timestamps for a frame. Typically called from the RHI thread in EndFrame. Returns the total number of cycles spent waiting
	uint32 SubmitSubmissionTimestampsForFrame(uint32 FrameNumber, const TArray<uint64*>& BeginSubmissionTimestamps, const TArray<uint64*>& EndSubmissionTimestamps );
	
	// Adjusts a timestamp by subtracting any preceeding submission gaps
	uint64 AdjustTimestampForSubmissionGaps( uint32 FrameSubmitted, uint64 Timestamp );

	// Called when we advance the frame from the renderthread (in EndDrawingViewport)
	void OnRenderThreadAdvanceFrame();

private:

	TArray<FGnmSubmissionGapRecorder::FFrame> FrameRingbuffer;

	FCriticalSection GapSpanMutex;
	uint32 WriteIndex;
	uint32 WriteIndexRT;
	uint32 ReadIndex;
	uint32 CurrentGapSpanReadIndex;
	uint32 CurrentElapsedWaitCycles;
	uint32 LastTimestampAdjusted;
};
#endif

/** Forward declare Async Submit Done Task */
class SubmitDoneTaskRunnable;

class FGnmCommandListContext;

struct SGnmComputeSubmission;

/** The interface which is implemented by the dynamically bound RHI. */
class FGnmManager 
{
public:	
	/** Initialization constructor. */
	FGnmManager();

	/**
	 * Bring up libgnm
	 */
	bool Initialize();

	/** Create the back buffer memory */
	void CreateBackBuffers(uint32 SizeX, uint32 SizeY);
	void RecreateBackBuffers(uint32 SizeX, uint32 SizeY);

	void CreateSocialScreenBackBuffers();

	void UpdateFlipRate(bool bForce);

	bool SupportsHDR();

	void SetReprojectionSamplerWrapMode(sce::Gnm::WrapMode ReprojectionSamplerWrapMode);
	void ChangeOutputMode(EPS4OutputMode InMode);
	void ChangeSocialScreenOutputMode(EPS4SocialScreenOutputMode InMode);
	void CacheReprojectionData(GnmBridge::MorpheusDistortionData& ReprojectionData);
	void StartMorpheus2DVRReprojection(GnmBridge::Morpheus2DVRReprojectionData& ReprojectionData);
	void StopMorpheus2DVRReprojection();

	EPS4OutputMode GetOutputMode() const;
	EPS4SocialScreenOutputMode GetSocialScreenOutputMode() const;
	FTexture2DRHIRef GetSocialScreenRenderTarget() const;
	bool ShouldRenderSocialScreenThisFrame() const;
	void TranslateSocialScreenOutput(FRHICommandListImmediate& RHICmdList);
	uint64 GetLastFlipTime();
	uint32 GetVideoOutPort();
	void GetLastApplyReprojectionInfo(GnmBridge::MorpheusApplyReprojectionInfo& OutInfo);

	uint32 GetParallelTranslateTLS()
	{
		return ParallelTranslateTLS;
	}

	bool IsInParallelTranslateThread()
	{
		return (uint64)FPlatformTLS::GetTlsValue(ParallelTranslateTLS) != 0;
	}

	/**
	 * Convert an Unreal format to Gnm DataFormat, and optionally make it an SRGB format
	 *
	 * @param Format Unreal format
	 * @param bMakeSRGB If true, and the format supports it, the returned format will be SRGB
	 *
	 * @return a valid DataFormat object
	 */
	static Gnm::DataFormat GetDataFormat(EPixelFormat Format, bool bMakeSRGB=false);

	/**
	 * @return the global context object
	 */
	FGnmCommandListContext& GetImmediateContext()
	{
		return *ImmediateContext;
	}

	FGnmComputeCommandListContext& GetImmediateComputeContext()
	{
		return *ImmediateComputeContext;
	}

	FGnmCommandListContext* AcquireDeferredContext();	

	void ReleaseDeferredContext(FGnmCommandListContext* Context);	

	void AddSubmission(const SGnmCommandSubmission& Submission);
	void AddFinalSubmission(const SGnmCommandSubmission& Submission);

	void AddComputeSubmission(const SGnmComputeSubmission& Submission);

	void BeginParallelContexts();
	void EndParallelContexts();

	//when continuous submits is enabled the CPU may end up starving the GPU.
	//We want to time the GPU idle time between submits.
	void TimeSubmitOnCmdListBegin(FGnmCommandListContext* Context); //add timestamps at the front of next commandlist to be generated
	void TimeSubmitOnCmdListEnd(FGnmCommandListContext* Context); //add timestamps at the end of the current commandlist about to be submitted.
	

	/**
 	 * @return current back buffer object
 	 */
 	FGnmTexture2D*  GetBackBuffer(bool bForRenderThread) const
 	{
 		return RenderBuffers[bForRenderThread ? CurrentBackBuffer_RenderThread : CurrentBackBuffer];
 	}

#if HAS_MORPHEUS
	FGnmTexture2D*  GetReprojectionRedirect(bool bForRenderThread) const
	{
		return ReprojectionBuffers[bForRenderThread ? CurrentBackBuffer_RenderThread : CurrentBackBuffer];
	}
#endif

	int32 GetBackBufferIndex(bool bForRenderThread) const
	{
		return bForRenderThread ? CurrentBackBuffer_RenderThread : CurrentBackBuffer;
	}

	void Advance_CurrentBackBuffer_RenderThread();

	/**
 	 * @return current back buffer object
 	 */
 	FGnmTexture2D*  GetRenderBuffer(uint32 BufferIndex)
 	{
 		return RenderBuffers[BufferIndex];
 	}

#if HAS_MORPHEUS
	FGnmTexture2D*  GetAuxRenderBuffer(bool bForRenderThread) const
	{
		return AuxRenderBuffers[bForRenderThread ? CurrentAuxBufferIndex_RenderThread : CurrentAuxBufferIndex];
	}
	FGnmTexture2D*  GetAuxOutputBuffer(bool bForRenderThread) const
	{
		return AuxVideoOutBuffers[bForRenderThread ? CurrentAuxBufferIndex_RenderThread : CurrentAuxBufferIndex];
	}
	FUnorderedAccessViewRHIRef GetAuxUAVRef(bool bForRenderThread) const
	{
		return AuxUAVs[bForRenderThread ? CurrentAuxBufferIndex_RenderThread : CurrentAuxBufferIndex];
	}

	uint64 GetEndOfFrameLabel(int32 ContextIndex)
	{
		return *EndOfFrameLabels[ContextIndex];
	}
#endif

	/**
	 * @return true if we are between BeginFrame and EndFrame
	 */
	bool IsCurrentlyRendering()
	{
		return bIsCurrentlyRendering;
	}

	/**
	 * Initialize the context, and any global state
	 */
	void InitializeState();

	/**
	 * Wait for GPU to finish all tasks with minimal state reset
	 */
	void WaitForGPUIdleNoReset();

	/**
	* Wait for GPU to finish all tasks with full state reset
	*/
	void WaitForGPUIdleReset();

	/**
	 * Block the CPU until the GPU has completed rendering the submitted frame
	 */
	void WaitForGPUFrameCompletion();	

	void BeginFrame();

	/**
	 * End the frame, and optionally flip the back buffer
	 */
	void EndFrame(bool bPresent, bool bUpdatingCubemap );

	/**
	 * Begin Scene Rendering.  Tracks frame for Resource caching.
	 */
	void BeginScene();

	/**
	 * End Scene Rendering.  Tracks frame for Resource caching.
	 */
	void EndScene();

	/**
	 * Inserts a gnm::submitdone call if necessary and updates tracking appropriately.
	 */
	void SubmitDone(bool bForce, bool bEnableAutoSubmitDone);

	/**
	 * Begin Scene Rendering.  Tracks frame for Resource caching.
	 */
	int32 GetResourceTableFrameCounter() const
	{
		return ResourceTableFrameCounter;
	}	
	
	FORCEINLINE uint32 GetFrameCount() const { return FrameCounter; }
	FORCEINLINE uint32 GetFrameCountRT() const { return FrameCounterRenderThread; }

	enum class ETempRingBufferAllocateMode
	{
		AllowFailure, // Returns null if allocation is too large
		NoFailure     // (default) Fatal error if allocation is too large
	};

	void* AllocateFromTempRingBuffer(uint32 Size, ETempRingBufferAllocateMode Mode = ETempRingBufferAllocateMode::NoFailure);
	static bool SafeToAllocateFromTempRingBuffer();

	uint64* AllocateGPULabelLocation();
	void FreeGPULabelLocation(uint64* LabelLoc);

	/**
	 * Maximum number of wavefronts that could be using the scratch buffer simultaneously.
	 * This number should less than or equal to 32 times the number of compute units.
	 */
	static const int ScratchBufferMaxNumWaves = 32 * 18;

	enum EScratchBufferResourceType
	{
		SB_GRAPHIC_RESOURCE = 0,
		SB_COMPUTE_RESOURCE = 1
	};

	class FShaderScratchBuffer
	{
	public:
		FShaderScratchBuffer( sce::Gnm::ShaderGlobalResourceType InResourceType) :
			ResourceType(InResourceType)
		{
		}

		void ResizeScratchBuffer( uint32 RequestedScratchBufferSize );
		void Commit( GnmContextType& Context );
		uint32 GetScratchBufferSize() {	return ScratchMem.GetSize(); }

	private:
		FMemBlock ScratchMem;
		sce::Gnm::Buffer ScratchBuffer;
		sce::Gnm::ShaderGlobalResourceType ResourceType;
	};

	/**
 	 * Update the maximum size for a shader scratch buffer
	 */
	void UpdateShaderScratchBufferMaxSize(EScratchBufferResourceType ScratchBufferType, uint32 ScratchSizeInDWPerThread );	

	void WaitForBackBufferSafety(GnmContextType& GnmContext);

	/**dumps the current frame's PM4 packets to a file at the end of the frame. */
	void DumpPM4();

	/*
	* Resets the FrameTempBuffer so that memory already allocated this frame will be reused by subsequent drawing.
	* Only valid if the GPU is completely done drawing all queued commands, and subsequent drawing will not refer back to
	* resources allocated previously from the buffer.
	*/
	void ResetFrameTempBuffer();

	/*
	* Clears the FrameTempBuffer.  All references to previously allocated memory is lost, but the data will survive until the BlockManager
	* is swapped and hands out the data blocks again.
	*/
	void ClearFrameTempBuffer();
	/*
	* Indicates that the RHI thread should start using whatever allocator the render thread was using
	*/
	void StartRHIThreadTempBufferFrame(int32 FrameIndex);
	/*
	* Advances the rendering thread temp ring buffer to the next slot
	*/
	void StartRenderThreadTempBufferFrame();

	inline void SetCustomPresent(FRHICustomPresent* InCustomPresent)
	{
		CustomPresent = InCustomPresent;
	}

	inline int32 GetVideoOutHandle() const
	{
		return VideoOutHandle;
	}

	inline sce::Gnm::OwnerHandle GetOwnerHandle() const
	{
		return Owner;
	}

#if USE_DEFRAG_ALLOCATOR
	FPS4GPUDefragAllocator& GetDefragAllocator()
	{
		return DefragAllocator;
	}
#endif

	/** Scratch buffers for the 2 shader resource types */
	FShaderScratchBuffer GraphicShaderScratchBuffer;
	FShaderScratchBuffer ComputeShaderScratchBuffer;

	/** Memory for ES->GS and GS->VS ring buffers */
	FMemBlock ESGSRingBuffer;
	FMemBlock GSVSRingBuffer;

	bool bGPUFlipWaitedThisFrame;

	FCriticalSection SubmissionMutex;
	FGnmGPUProfiler GPUProfilingData;
#if ENABLE_SUBMITWAIT_TIMING
	FGnmSubmissionGapRecorder SubmissionGapRecorder;
#endif

private:	

	/* Used to store VSync flip timestamps */
	struct FFlipMarker
	{
		volatile uint64* StartTimestamp;
		volatile uint64* EndTimestamp;

		void Initialize();
	};

	void ChangeVideoOutputMode(EPS4OutputMode InMode);

public:
	void HandleMorpheusReprojection();
	void ApplyMorpheusReprojection(GnmBridge::MorpheusDistortionData& ReprojectionData);
	void ApplyMorpheus2DVRReprojection(GnmBridge::Morpheus2DVRReprojectionData& ReprojectionData);
private:

	void TranslateRGBToYUV(FRHICommandListImmediate& RHICmdList);

	/**
	* Submit the current frame to the GPU.  Also performs various bookkeeping operations.
	*/
	void SubmitFrame(bool bVsync, bool bPresent, bool bAuxFrame);

	/** 
	 * Simply submits all queued dcbs. 
	 */
	void SubmitQueuedDCBs(bool bVsync, bool bPresent, bool bAuxFrame);
	
	/* actually perform the dump */
	void DumpPM4Internal(uint32 ContextIndex);

	void UpdateVsyncMargins();

	Gnmx::ComputeQueue ComputeQueue;
	FMemBlock ComputeQueueRing;
	FMemBlock ComputeQueueRead;

	/** Set of data format objects, indexed by the PlatformFormat number set in GPixelFormats */
	TArray<Gnm::DataFormat> FormatMap;

 	/** Front/back buffers */
 	TRefCountPtr<FGnmTexture2D> RenderBuffers[NUM_RENDER_BUFFERS];

#if HAS_MORPHEUS
	TRefCountPtr<FGnmTexture2D> ReprojectionBuffers[NUM_RENDER_BUFFERS];
	GnmBridge::MorpheusDistortionData ReprojectionDataBuffers[NUM_RENDER_BUFFERS];
	GnmBridge::Morpheus2DVRReprojectionData ReprojectionData2DVR;
	GnmBridge::MorpheusApplyReprojectionInfo LastApplyReprojectionInfo;
	FCriticalSection LastApplyReprojectionInfoMutex;
	bool bReprojectionData2DVRDirty = false;
	bool bDo2DVRReprojection = false;
	FCriticalSection ReprojectionDataCriticalSection;
	uint64_t* ReprojectionLabels[2];
	TSharedPtr<class MorpheusReprojectionTaskRunnable, ESPMode::ThreadSafe> MorpheusReprojectionAsyncTaskThreadRunnable;
#endif

	/** Video out state */
	int32 VideoOutHandle;
	SceKernelEqueue VideoOutQueue;
	uint64 LastVBlankProcessTime;
	int32 VideoOutBufferRegistrationHandle;

	/** Which buffer is the current back buffer */
	int32 CurrentBackBuffer;

	/** Which buffer is the current back buffer, from the perspective of the render thread */
	int32 CurrentBackBuffer_RenderThread;

	/** Which GFX context is the current context we are writing to */
	int32 CurrentContextIndex;

#if HAS_MORPHEUS
	TRefCountPtr<FGnmTexture2D> AuxRenderBuffers[NUM_AUX_RENDER_BUFFERS];
	TRefCountPtr<FGnmTexture2D> AuxVideoOutBuffers[NUM_AUX_RENDER_BUFFERS];
	FUnorderedAccessViewRHIRef AuxUAVs[NUM_AUX_RENDER_BUFFERS];

	int32 AuxVideoOutHandle;
	int32 AuxVideoOutBufferRegistrationHandle;
	int32 CurrentAuxBufferIndex;
	int32 CurrentAuxBufferIndex_RenderThread;
	int32 NextAuxBufferIndexForRenderThread;
	bool bCurrentFrameIsAuxFrame_RenderThread;
	int32 AuxLastRenderedVCount;
#endif

	FGnmCommandListContext* ImmediateContext;	
	FGnmComputeCommandListContext* ImmediateComputeContext;

	/** Memory for the GPU to write to when it's complete */
	volatile uint64* EndOfFrameLabels[2];
	volatile uint64* BlockUntilIdleLabel;
	volatile uint64* StartOfFrameTimestampLabel[2];
	volatile uint64* EndOfFrameTimestampLabel[2];
	volatile uint64* StartOfDefragTimestampLabel[2];
	volatile uint64* EndOfDefragTimestampLabel[2];

#if USE_NEW_PS4_MEMORY_SYSTEM
	volatile uint64* MemBlockFreeLabel;
#endif
	void FreePendingMemBlocks();

	TArray<uint64*> StartOfSubmissionTimestamp[2];
	TArray<uint64*> EndOfSubmissionTimestamp[2];

	/* Used to keep track of the VSync timings */
	TArray<FFlipMarker> FlipMarkers[2];
	int32 FlipMarkersIndex[2];

	/** This is used to match against EndOfFrameLabel */
	uint32 FrameCounter;
	uint32 FrameCounterRenderThread;
	uint32 LastSubmitDoneFrame;
	uint32 SubmissionsThisFrame;

	int32 CombinedMarkerStackLevel;

	/** Are we current between Begin/EndFrame? */
	bool bIsCurrentlyRendering;			

	/* flag to dump PM4 packets when the frame is complete. */
	bool bDumpPM4OnEndFrame;

	/* Allocator for cheap single-frame allocations for rendering.  Mostly holds constant buffer data. */
	int32 RenderThreadTempBufferIndex;
	int32 RHIThreadTempBufferIndex;
	int32 TempAllocIndex();
	TTempFrameGPUAllocator FrameTempAllocator[NUM_TEMP_RING_BUFFERS];

	/** Async Task to send SubmitDone messages to GPU periodically to avoid TRC violations when rendering is blocked **/
	TSharedPtr<class SubmitDoneTaskRunnable, ESPMode::ThreadSafe> SubmitDoneAsyncTaskThreadRunnable;
	TSharedPtr<class SubmitCommandBuffersTaskRunnable, ESPMode::ThreadSafe> SubmitBuffersAsyncTaskThreadRunnable;
	
	sce::Gnm::OwnerHandle Owner;

	TArray<FGnmCommandListContext*> GnmCommandContexts;
	TArray<uint64*> FreeLabelLocations;		
	TArray<FMemBlock> BaseLabelAllocs;

	static const int32 NUM_LABELDELAY_BUFFERS = NUM_TEMP_RING_BUFFERS + 1;
	TArray<uint64*> InFlightLabels[NUM_LABELDELAY_BUFFERS];

	// Final submission in the frame which contains EOF timers and flip preparation.
	SGnmCommandSubmission FinalFrameSubmission;
	bool bAddedFinalSubmit;

	FRHICustomPresent* CustomPresent;	

	SceKernelEqueue VBlankEventQueue;

	uint32 LastFlipRate;

	// Debugging support
public:
	TRefCountPtr<FGnmUniformBuffer> DummyConstantBuffer;

#if PS4_RECORD_SUBMITS
	TArray<SGnmCommandSubmission> SubmittedCommandsDebug[2];
#endif

	/** Internal frame counter, incremented on each call to RHIBeginScene. */
	uint32 SceneFrameCounter;

	/**
	 * Internal counter used for resource table caching.
	 * INDEX_NONE means caching is not allowed.
	 */
	uint32 ResourceTableFrameCounter;		

	sce::Gnm::WrapMode ReprojectionSamplerWrapMode;

	EPS4OutputMode CurrentOutputMode;

	EPS4SocialScreenOutputMode ConfiguredSocialScreenOutputMode;  // Mirroring is equivalent to None.
	EPS4SocialScreenOutputMode CurrentSocialScreenOutputMode;
	mutable FCriticalSection SocialScreenOutputModeMutex; // protect multithreaded accesses to CurrentSocialScreenOutputMode

	uint32 ParallelTranslateTLS;

	SceKernelEqueue FrameCompleteQueue;

	float BottomVsyncMargin;
	float TopVsyncMargin;

#if USE_DEFRAG_ALLOCATOR
	FPS4GPUDefragAllocator DefragAllocator;
#endif

	friend class FGnmDynamicRHI;

	FRHIFlipDetails WaitForFlip(double TimeoutInSeconds);
	void SignalFlipEvent();
};

/** Singleton object to wrap libgnm */
extern FGnmManager GGnmManager;
