// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmManager.cpp: Wrapper around libgnm
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "ClearReplacementShaders.h"
#include "ShaderParameterUtils.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "HAL/Runnable.h"
#include "EngineGlobals.h"
#include "RenderUtils.h"
#include "PS4/PS4LLM.h"

#if !defined(PS4_SUPPORTS_PARALLEL_RHI_EXECUTE)
#error "PS4_SUPPORTS_PARALLEL_RHI_EXECUTE must be defined."
#endif

// Enable this when trying to debug slow frames that cause the SubmitDone popup
#define PS4_GNM_SLOW_FRAME_DEBUGGING 0

#include <user_service.h>

#if HAS_MORPHEUS
#include <hmd.h>
#include <hmd/distortion_correct.h>
#include <hmd/reprojection.h>
#include <vision/vr_tracker.h>
#include <video_out/aux_sub.h>
#include <social_screen.h>
#endif
#include "RGBAToYUV420Shader.h"

const uint32 GDrawCommandBufferSize = 16*1024*1024;
const uint32 GConstantCommandBufferSize = 8*1024*1024;

/** Singleton object to wrap libgnm */
FGnmManager GGnmManager;


static void DumpPM4()
{	
	GGnmManager.DumpPM4();
}

FAutoConsoleCommand CmdDumpPM4(
	TEXT("r.PS4.DumpPM4"),
	TEXT("Dump the PM4 packets for the current frame.  PM4 packets are the raw GPU commands from the commandbuffer.  Dump expands them to human readable. See AMD ISA documents for packet descriptions."),
	FConsoleCommandDelegate::CreateStatic(DumpPM4)
	);


static int32 GPS4ContinuousSubmits = 1;
static FAutoConsoleVariableRef CVarPS4ContinuousSubmits(
	TEXT("r.PS4ContinuousSubmits"),
	GPS4ContinuousSubmits,
	TEXT("Defines when the PS4 GPU gets commands form the CPU.\n")
	TEXT(" 0: Submit GPU commands after CPU finished the frame (better for GPU profiling get rid of bubbles)\n")
	TEXT(" 1: The GPU gets it's commands fro the CPU at multiple defined spots during the frame\n")
	TEXT("    which is better for latency and better performance with high GPU workloads (default)"),
	ECVF_Default
	);

static int32 GPS4EventBasedFrameEnd = 1;
static FAutoConsoleVariableRef CVarPS4EventBasedFrameEnd(
	TEXT("r.PS4EventBasedFrameEnd"),
	GPS4EventBasedFrameEnd,
	TEXT("Event based submission thread.  0 will go back to polling"),
	ECVF_Default
	);

static int32 GPS4TimeSubmitWaits = 1;
static FAutoConsoleVariableRef CVarPS4TimeSubmitWaits(
	TEXT("r.PS4TimeSubmitWaits"),
	GPS4TimeSubmitWaits,
	TEXT("Time GPU starvation caused by submits not coming in fast enough.  This time is removed from the stat unit GPU time."),
	ECVF_Default
	);

// The default value must be less than GGarlicHeapSize
static int32 GPS4DefragPoolSize = 2300;
static FAutoConsoleVariableRef CVarPS4DefragPoolSize(
	TEXT("r.PS4DefragPoolSize"),
	GPS4DefragPoolSize,
	TEXT("Size in MB of the defrag pool (if enabled)."),
	ECVF_Default
	);

static int32 GPS4WarnOnGapBetweenSubmitDoneCalls = 1;
static FAutoConsoleVariableRef CVarPS4WarnOnGapBetweenSubmitCalls(
	TEXT("r.PS4WarnOnGapBetweenSubmitDoneCalls"),
	GPS4WarnOnGapBetweenSubmitDoneCalls,
	TEXT("Warns when there is a large gap (>2s) between SubmitDone calls."),
	ECVF_Default
	);

/**
 * Async Task to send submitDone messages to GPU periodically if rendering is not occurring.
 * Needed to avoid TRC Violations
 */
class SubmitDoneTaskRunnable : public FRunnable
{
public:
	SubmitDoneTaskRunnable()
		: SubmitAsyncTaskThread(nullptr)
		, bIsRunning(false)
		, bCanAutoSubmitDone(true)
	{
		LastSubmitDoneTime = FPlatformTime::Seconds();
	}

	~SubmitDoneTaskRunnable()
	{
		if( SubmitAsyncTaskThread != nullptr )
		{
			Stop();
			SubmitAsyncTaskThread->WaitForCompletion();
			delete SubmitAsyncTaskThread;
			SubmitAsyncTaskThread = nullptr;
		}
	}

	void Start()
	{
		bIsRunning = true;
		SubmitAsyncTaskThread = FRunnableThread::Create(this, TEXT("SubmitDoneAsyncTaskThreadPS4"));
		check(SubmitAsyncTaskThread);
	}

	virtual uint32 Run()
	{
		while( bIsRunning )
		{
			{
				FScopeLock Lock( &GGnmManager.SubmissionMutex );
				
				const double CurrentTime = FPlatformTime::Seconds();
				const double TimeSinceLastSubmission = CurrentTime - LastSubmitDoneTime;
				const double SlowFrameThreshold = 1.0f;
				const bool bNeedToSubmitSoon = TimeSinceLastSubmission >= SlowFrameThreshold;

				if (bNeedToSubmitSoon)
				{					
					if (bCanAutoSubmitDone)
					{
#if PS4_GNM_SLOW_FRAME_DEBUGGING
						UE_LOG(LogPS4, Log, TEXT("GGnmManager.SubmitDone by SubmitDoneTaskRunnable (%f s since last submit) in RT frame %u, RHI frame %u"), TimeSinceLastSubmission, GGnmManager.GetFrameCount());
#endif
						GGnmManager.SubmitDone(true, true);
					}
					else
					{
#if PS4_GNM_SLOW_FRAME_DEBUGGING
						UE_LOG(LogPS4, Warning, TEXT("Cannot call GGnmManager.SubmitDone because bCanAutoSubmitDone is false (%f s since last submit), RT frame %u, RHI frame %u"), TimeSinceLastSubmission, GGnmManager.GetFrameCountRT(), GGnmManager.GetFrameCount());
#endif
					}
				}
			}

			FPlatformProcess::Sleep( 0.2f );  
		}

		return 0;
	}

	virtual void Stop()
	{
		bIsRunning = false;
	}

	void UpdateSubmitDoneTime()
	{
		const double CurrentTime = FPlatformTime::Seconds();
		const double DeltaTime = CurrentTime - LastSubmitDoneTime;
		LastSubmitDoneTime = CurrentTime;

		if (GPS4WarnOnGapBetweenSubmitDoneCalls && DeltaTime > 2.0f)
		{
			UE_LOG(LogPS4, Warning, TEXT("Gap between SubmitDone calls of %f s"), DeltaTime);
		}
	}

	void EnableAutoSubmitDone( bool bEnable )
	{
		bCanAutoSubmitDone = bEnable;
	}

private:

	FRunnableThread* SubmitAsyncTaskThread;
	bool bIsRunning;
	volatile float LastSubmitDoneTime;
	volatile bool bCanAutoSubmitDone;
};

/**
* Async Task to send submit commandbuffers to the GPU without stalling the render/rhi threads.
*/
class SubmitCommandBuffersTaskRunnable : public FRunnable
{
public:
	SubmitCommandBuffersTaskRunnable(SubmitDoneTaskRunnable* InSubmitDoneTask, Gnmx::ComputeQueue* InAsyncComputeQueue)
		: SubmitCommandsAsyncTaskThread(nullptr)
		, bIsRunning(false)
	{
		check(InSubmitDoneTask);
		check(InAsyncComputeQueue);
		AsyncComputeQueue = InAsyncComputeQueue;
		SubmitDoneTask = InSubmitDoneTask;
		SubmitsPendingEvent = FPlatformProcess::GetSynchEventFromPool(true);
	}

	~SubmitCommandBuffersTaskRunnable()
	{
		if (SubmitCommandsAsyncTaskThread != nullptr)
		{
			Stop();
			SubmitCommandsAsyncTaskThread->WaitForCompletion();
			delete SubmitCommandsAsyncTaskThread;
			SubmitCommandsAsyncTaskThread = nullptr;

			FPlatformProcess::ReturnSynchEventToPool(SubmitsPendingEvent);			
			SubmitsPendingEvent = nullptr;
		}
	}

	void Start()
	{
		bIsRunning = true;

		//keep this thread on the second cluster.
		const int32 ThreadAffinity = 1 << 4 | 1 << 5;
		SubmitCommandsAsyncTaskThread = FRunnableThread::Create(this, TEXT("SubmitGPUCommandsAsyncTaskThreadPS4"), 0, TPri_Normal, ThreadAffinity);
		check(SubmitCommandsAsyncTaskThread);
	}

	void AddSubmission(const SGnmCommandSubmission& InSubmission)
	{
		FScopeLock Lock(&WriteLock);
		SGnmCommandSubmission& TargetSubmission = GetCurrentSubmissionBlock();		
		if (!TargetSubmission.AddSubmissionToQueue(InSubmission))
		{
			SubmissionBuffers.Add(InSubmission);
		}
		FPlatformMisc::MemoryBarrier();
		SubmitsPendingEvent->Trigger();
	}

	void AddAsyncComputeSubmission(const SGnmComputeSubmission& InSubmission)
	{
		FScopeLock Lock(&WriteLock);
		SGnmComputeSubmission& TargetSubmission = GetCurrentComputeSubmissionBlock();
		if (!TargetSubmission.AddSubmissionToQueue(InSubmission))
		{
			ComputeSubmissionBuffers.Add(InSubmission);
		}
		FPlatformMisc::MemoryBarrier();
		SubmitsPendingEvent->Trigger();
	}

	void Submit()
	{
		SCOPED_NAMED_EVENT_TEXT("Async Submit", FColor::Red);

		FScopeLock SubmissionLock(&GGnmManager.SubmissionMutex);		

		{
			FScopeLock ScopeWriteLock(&WriteLock);
			FScopeLock ScopeReadLock(&ReadLock);
			if (SubmissionBuffers.Num() > 0)
			{
				ActiveSubmissionBuffers.Append(SubmissionBuffers);
				SubmissionBuffers.Reset();
			}
			if (ComputeSubmissionBuffers.Num() > 0)
			{
				ActiveComputeSubmissionBuffers.Append(ComputeSubmissionBuffers);
				ComputeSubmissionBuffers.Reset();
			}
		}

		FScopeLock Lock(&ReadLock);	

		bool bWillSubmitGfx = ActiveSubmissionBuffers.Num() > 0;
		bool bWillSubmitCompute = ActiveComputeSubmissionBuffers.Num() > 0;

		// calling submit on the async thread is a pre-req for doing manual submissions to maintain submission order.
		// so we always do wait/submitdone handling to set up for manual submits, even if there are no async submits to handle.
		{
			//only do submitdones when the GPU is idle.  This makes the submitdone clear quickly, and avoids
			//pushing async compute work out unnecessarily (like Morpheus camera processing).
			QUICK_SCOPE_CYCLE_COUNTER(FGnmDynamicRHI_WaitForGPUFrameCompletion);
			GGnmManager.WaitForGPUFrameCompletion();

			//we never want the submission thread to leave the submitdone thread in a state to add submitdones.  A submitdone inserted
			//between submits in the same frame will crash the GPU.
			GGnmManager.SubmitDone(false, false);
		}

		if (bWillSubmitGfx)
		{
			for (int32 i = 0; i < ActiveSubmissionBuffers.Num(); ++i)
			{
				SGnmCommandSubmission& Submission = ActiveSubmissionBuffers[i];
				int32 Ret = sce::Gnm::submitCommandBuffers(Submission.SubmissionAddrs.Num(), Submission.SubmissionAddrs.GetData(), Submission.SubmissionSizesBytes.GetData(), 0, 0);
				if (Ret != SCE_OK)
				{
					for (int32 j = 0; j < Submission.SubmissionAddrs.Num(); ++j)
					{
						UE_LOG(LogPS4, Warning, TEXT("Submission: %i of group: %i, Addr: %p, Size: %i\n"), j, i, Submission.SubmissionAddrs[j], Submission.SubmissionSizesBytes[j]);						
					}
					UE_LOG(LogPS4, Fatal, TEXT("async submitCommandBuffers failed: 0x%x, %i, %p"), Ret, Submission.SubmissionAddrs.Num(), &Submission);
				}				
			}
			ActiveSubmissionBuffers.Reset();
		}
		if (bWillSubmitCompute)
		{
			for (int32 i = 0; i < ActiveComputeSubmissionBuffers.Num(); ++i)
			{
				SGnmComputeSubmission& Submission = ActiveComputeSubmissionBuffers[i];
				Gnmx::ComputeQueue::SubmissionStatus Status;
				const float SubmitWaitTime = 0.001f;
				float SubmitWaitCounter = 0.0f;
				do
				{
					Status = AsyncComputeQueue->submit(Submission.SubmissionCount, Submission.SubmissionAddrs, Submission.SubmissionSizesBytes);

					if (Status != Gnmx::ComputeQueue::kSubmitOK)
					{
						if (SubmitWaitTime > 5.0)
						{
							UE_LOG(LogPS4, Warning, TEXT("AsyncCompute Submission failing because queue is full. GPU hang?"));
						}
						FPlatformProcess::Sleep(SubmitWaitTime);
						SubmitWaitCounter += SubmitWaitTime;
					}
				} while (Status == Gnmx::ComputeQueue::kSubmitFailQueueIsFull);

				if (Status != Gnmx::ComputeQueue::kSubmitOK)
				{
					for (int32 j = 0; j < Submission.SubmissionCount; ++j)
					{
						UE_LOG(LogPS4, Warning, TEXT("ComputeSubmission: %i of group: %j, Addr: %p, Size: %i\n"), j, i, Submission.SubmissionAddrs[j], Submission.SubmissionSizesBytes[j]);
					}
					UE_LOG(LogPS4, Fatal, TEXT("async compute submit failed: 0x%x, %i, %p"), Status, Submission.SubmissionCount, &Submission);
				}
			}

			ActiveComputeSubmissionBuffers.Reset();
		}
		SubmitsPendingEvent->Reset();
	}

	virtual uint32 Run()
	{
		while (bIsRunning)
		{			
			if (GPS4ContinuousSubmits)
			{
				SubmitsPendingEvent->Wait();
				Submit();
			}
			else
			{
				FPlatformProcess::Sleep(0.01666f);
			}
		}
		return 0;
	}

	virtual void Stop()
	{
		bIsRunning = false;
	}

	Gnmx::ComputeQueue* AsyncComputeQueue;
	mutable	FCriticalSection WriteLock;
	mutable	FCriticalSection ReadLock;

private:

	SGnmCommandSubmission& GetCurrentSubmissionBlock()
	{
		if (SubmissionBuffers.Num() == 0)
		{
			SubmissionBuffers.Add(SGnmCommandSubmission());
		}
		return SubmissionBuffers.Last();
	}

	SGnmComputeSubmission& GetCurrentComputeSubmissionBlock()
	{
		if (ComputeSubmissionBuffers.Num() == 0)
		{
			ComputeSubmissionBuffers.Add(SGnmComputeSubmission());
		}
		return ComputeSubmissionBuffers.Last();
	}

	SubmitDoneTaskRunnable* SubmitDoneTask;
	TArray<SGnmCommandSubmission> SubmissionBuffers;
	TArray<SGnmCommandSubmission> ActiveSubmissionBuffers;

	TArray<SGnmComputeSubmission> ComputeSubmissionBuffers;
	TArray<SGnmComputeSubmission> ActiveComputeSubmissionBuffers;


	FRunnableThread* SubmitCommandsAsyncTaskThread;
	FEvent* SubmitsPendingEvent;
	bool bIsRunning;
};

#if HAS_MORPHEUS
class MorpheusReprojectionTaskRunnable : public FRunnable
{
public:
	MorpheusReprojectionTaskRunnable()
		: AsyncTaskThread(nullptr)
		, bIsRunning(false)
	{
		ReprojectionPendingEvent = FPlatformProcess::GetSynchEventFromPool(true);

		int Ret = sceKernelCreateEqueue(&FrameCompleteQueue, "SubmitEndOfFrameQueue");
		checkf(Ret == SCE_OK, TEXT("sceKernelCreateEqueue: 0x%x"), Ret);

		Ret = sce::Gnm::addEqEvent(FrameCompleteQueue, sce::Gnm::kEqEventGfxEop, nullptr);
		checkf(Ret == SCE_OK, TEXT("addEqEvent: 0x%x"), Ret);
	}

	~MorpheusReprojectionTaskRunnable()
	{
		if (AsyncTaskThread != nullptr)
		{
			Stop();
			AsyncTaskThread->WaitForCompletion();
			delete AsyncTaskThread;
			AsyncTaskThread = nullptr;

			FPlatformProcess::ReturnSynchEventToPool(ReprojectionPendingEvent);
			ReprojectionPendingEvent = nullptr;
		}
	}

	void Start()
	{
		bIsRunning = true;

		//keep this thread on the second cluster.
		const int32 ThreadAffinity = 1 << 4 | 1 << 5;
		AsyncTaskThread = FRunnableThread::Create(this, TEXT("ReprojectionAsyncTaskThreadPS4"), 0, TPri_Normal, ThreadAffinity);
		check(AsyncTaskThread);
	}

	void QueueReprojection(const GnmBridge::MorpheusDistortionData& InData)
	{
		FScopeLock Lock(&ReprojectionDataLock);

		ReprojectionData = InData;

		FPlatformMisc::MemoryBarrier();
		ReprojectionPendingEvent->Trigger();
	}

	void DequeueReprojection()
	{
		FScopeLock Lock(&ReprojectionDataLock);
		ActiveReprojectionData = ReprojectionData;
	}

	void Reproject()
	{
		SCOPED_NAMED_EVENT_TEXT("Async Reprojection", FColor::Red);

		DequeueReprojection();

		// Block until the frame is complete
		{
			QUICK_SCOPE_CYCLE_COUNTER(FGnmDynamicRHI_WaitForGPUFrameCompletionMinimal);

			WaitForGPUFrameCompletion();
			GGnmManager.ApplyMorpheusReprojection(ActiveReprojectionData);
		}

		ReprojectionPendingEvent->Reset();
	}

	void WaitForGPUFrameCompletion()
	{
		const static int32 NumEventsArray = 4;
		SceKernelEvent EventArray[NumEventsArray];
		int32 NumReceivedEvents = 0;

		//wait in intervals for an event.  Theoretically possible for the kernel to lose an event so we don't want to wait forever.
		//https://ps4.scedev.net/technotes/view/141/1
		SceKernelUseconds Timeout = 60000;
		// Wait for frames to end until the one we are looking for ends.
		while (GGnmManager.GetEndOfFrameLabel(ActiveReprojectionData.RHIContextIndex) < ActiveReprojectionData.FrameCounter - 1)
		{
			if (GPS4EventBasedFrameEnd != 0)
			{
				int32 Ret = sceKernelWaitEqueue(FrameCompleteQueue, EventArray, NumEventsArray, &NumReceivedEvents, &Timeout);
				checkf(Ret == SCE_OK || Ret == SCE_KERNEL_ERROR_ETIMEDOUT, TEXT("sceKernelWaitEqueue failed: 0x%x"), Ret);
				if (Ret == SCE_KERNEL_ERROR_ETIMEDOUT)
				{
					UE_LOG(LogPS4, Warning, TEXT("EOF Event Wait Timed Out"));
				}
			}
			else
			{
				// This time is counted toward the present idle timer external to this funciton.
				// Don't double up by counting it toward the thread wait time.
				FPlatformProcess::SleepNoStats(0);
			}
		}
	}

	virtual uint32 Run()
	{
		while (bIsRunning)
		{
			ReprojectionPendingEvent->Wait();
			Reproject();
		}
		return 0;
	}

	virtual void Stop()
	{
		bIsRunning = false;
	}

private:
	mutable FCriticalSection ReprojectionDataLock;
	GnmBridge::MorpheusDistortionData ReprojectionData;
	GnmBridge::MorpheusDistortionData ActiveReprojectionData;

	SceKernelEqueue FrameCompleteQueue;

	FRunnableThread* AsyncTaskThread;
	FEvent* ReprojectionPendingEvent;
	bool bIsRunning;
};
#endif

FCriticalSection PS4GPULabelAllocLock;
FCriticalSection GnmContextsLock;


FGnmCommandListContext* FGnmManager::AcquireDeferredContext()
{
	FScopeLock Lock(&GnmContextsLock);
	return GnmCommandContexts.Pop(false);
}

void FGnmManager::ReleaseDeferredContext(FGnmCommandListContext* Context)
{
	FScopeLock Lock(&GnmContextsLock);
	check(Context);
	GnmCommandContexts.Push(Context);
}

void FGnmManager::AddSubmission(const SGnmCommandSubmission& Submission)
{
	check((IsInRenderingThread() || IsInRHIThread()));
	SubmitBuffersAsyncTaskThreadRunnable->AddSubmission(Submission);
	++SubmissionsThisFrame;

#if ENABLE_SUBMITWAIT_TIMING
	StartOfSubmissionTimestamp[CurrentContextIndex].Add(Submission.BeginTimestamp);
	EndOfSubmissionTimestamp[CurrentContextIndex].Add(Submission.EndTimestamp);
#endif

#if PS4_RECORD_SUBMITS
	SubmittedCommandsDebug[CurrentContextIndex].Add(Submission);
#endif
}

void FGnmManager::AddComputeSubmission(const SGnmComputeSubmission& Submission)
{
	check((IsInRenderingThread() || IsInRHIThread()));
	SubmitBuffersAsyncTaskThreadRunnable->AddAsyncComputeSubmission(Submission);
}

void FGnmManager::AddFinalSubmission(const SGnmCommandSubmission& Submission)
{
	check((IsInRenderingThread() || IsInRHIThread()));
	check(!bAddedFinalSubmit);
	FinalFrameSubmission = Submission;	
	bAddedFinalSubmit = true;

	++SubmissionsThisFrame;

#if ENABLE_SUBMITWAIT_TIMING
	StartOfSubmissionTimestamp[CurrentContextIndex].Add(Submission.BeginTimestamp);
	EndOfSubmissionTimestamp[CurrentContextIndex].Add(Submission.EndTimestamp);
#endif

#if PS4_RECORD_SUBMITS
	SubmittedCommandsDebug[CurrentContextIndex].Add(Submission);
#endif
}

void FGnmManager::BeginParallelContexts()
{
	check((IsInRenderingThread() || IsInRHIThread()));
	FGnmCommandListContext& ImmediateContextLoc = GetImmediateContext();

	//add marker at end of immediate context submission
	TimeSubmitOnCmdListEnd(&ImmediateContextLoc);	
	AddSubmission(ImmediateContextLoc.GetContext().Finalize(ImmediateContextLoc.GetBeginCmdListTimestamp(), ImmediateContextLoc.GetEndCmdListTimestamp()));
}

void FGnmManager::EndParallelContexts()
{
	check((IsInRenderingThread() || IsInRHIThread()));
	
	FGnmCommandListContext& ImmediateContextRef = GetImmediateContext();
	ImmediateContextRef.InitContextBuffers();
	ImmediateContextRef.ClearState();	
}

void FGnmManager::TimeSubmitOnCmdListBegin(FGnmCommandListContext* Context)
{	
#if ENABLE_SUBMITWAIT_TIMING
	if (GPS4TimeSubmitWaits)
	{
		uint64* TimstampLoc = Context->AllocateBeginCmdListTimestamp();
		Context->GetContext().writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)TimstampLoc, Gnm::kCacheActionNone);
	}
#endif
}

void FGnmManager::TimeSubmitOnCmdListEnd(FGnmCommandListContext* Context)
{
#if ENABLE_SUBMITWAIT_TIMING
	if (GPS4TimeSubmitWaits)
	{
		uint64* TimstampLoc = Context->AllocateEndCmdListTimestamp();
		Context->GetContext().writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)TimstampLoc, Gnm::kCacheActionNone);
	}
#endif
}

/** Initialization constructor. */
FGnmManager::FGnmManager() 
	: GraphicShaderScratchBuffer(sce::Gnm::kShaderGlobalResourceScratchRingForGraphic)
	, ComputeShaderScratchBuffer(sce::Gnm::kShaderGlobalResourceScratchRingForCompute)
	, bGPUFlipWaitedThisFrame(false)
	, VideoOutHandle(-1)
	, LastVBlankProcessTime(0)
	, CurrentBackBuffer(0)
	, CurrentBackBuffer_RenderThread(0)
	, CurrentContextIndex(0)		
#if HAS_MORPHEUS
	, AuxVideoOutHandle(-1)
	, AuxVideoOutBufferRegistrationHandle(-1)
	, CurrentAuxBufferIndex(0)
	, CurrentAuxBufferIndex_RenderThread(0)
	, NextAuxBufferIndexForRenderThread(0)
	, bCurrentFrameIsAuxFrame_RenderThread(false)
	, AuxLastRenderedVCount(-1)
#endif
	, FrameCounter(0)
	, FrameCounterRenderThread(0)
	, LastSubmitDoneFrame(-1)	
	, SubmissionsThisFrame(0)
	, CombinedMarkerStackLevel(0)
	, bDumpPM4OnEndFrame(false)
	, RenderThreadTempBufferIndex(0)
	, RHIThreadTempBufferIndex(0)
	, Owner(sce::Gnm::kInvalidOwnerHandle)
	, CustomPresent(nullptr)
	, LastFlipRate(INDEX_NONE)
	, SceneFrameCounter(0)
	, ResourceTableFrameCounter(INDEX_NONE)
	, ReprojectionSamplerWrapMode(sce::Gnm::kWrapModeMirror)
	, CurrentOutputMode(EPS4OutputMode::Standard2D)
	, ConfiguredSocialScreenOutputMode(EPS4SocialScreenOutputMode::Mirroring)
	, CurrentSocialScreenOutputMode(EPS4SocialScreenOutputMode::Mirroring)
	, ParallelTranslateTLS(-1)
	, BottomVsyncMargin(0.0f)
	, TopVsyncMargin(0.0f)
{
}

/**
 * Bring up libgnm
 */
bool FGnmManager::Initialize()
{
	ParallelTranslateTLS = FPlatformTLS::AllocTlsSlot();
	bGPUFlipWaitedThisFrame = false;

	int32 Ret = sceKernelCreateEqueue(&VBlankEventQueue, "TrackerVblankQueue");
	checkf(Ret == SCE_OK, TEXT("sceKernelCreateEqueue: 0x%x"), Ret);

	Gnm::registerOwner(&Owner, "UE4");
	
#if PS4_SUPPORTS_PARALLEL_RHI_EXECUTE
	int32 Num = FTaskGraphInterface::Get().GetNumWorkerThreads();
	for (int32 Index = 0; Index < Num; Index++)
	{		
		FGnmCommandListContext* CmdContext = new FGnmCommandListContext(false);		
		GnmCommandContexts.Add(CmdContext);
	}
#endif

	ImmediateContext = new FGnmCommandListContext(true);
	ImmediateComputeContext = new FGnmComputeCommandListContext(true);

	for (int32 BufferIndex = 0; BufferIndex < 2; BufferIndex++)
	{
		// allocate all memory for the GfxContext, including double buffer command buffers
		{
			StartOfFrameTimestampLabel[BufferIndex] = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
			*StartOfFrameTimestampLabel[BufferIndex] = 0;

			EndOfFrameTimestampLabel[BufferIndex] = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
			*EndOfFrameTimestampLabel[BufferIndex] = 0;
		}

		{
			EndOfFrameLabels[BufferIndex] = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
			*EndOfFrameLabels[BufferIndex] = -1;
		}

		{
			FlipMarkersIndex[BufferIndex] = 0;
		}

		{
			StartOfDefragTimestampLabel[BufferIndex] = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
			*StartOfDefragTimestampLabel[BufferIndex] = 0;

			EndOfDefragTimestampLabel[BufferIndex] = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
			*EndOfDefragTimestampLabel[BufferIndex] = 0;			
		}

#if HAS_MORPHEUS
		{
			ReprojectionLabels[BufferIndex] = (uint64_t*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_Label)).GetPointer();
			*ReprojectionLabels[BufferIndex] = 0;
		}
#endif
	}

#if USE_NEW_PS4_MEMORY_SYSTEM
	MemBlockFreeLabel = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
	*MemBlockFreeLabel = 0;
#endif

	// allocate ring buffers for ES->GS and GS->VS stages
	// @todo mem: Check this usage (does it need CPU access?)
	ESGSRingBuffer = FMemBlock::Allocate(Gnm::kGsRingSizeSetup4Mb, Gnm::kAlignmentOfBufferInBytes, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_ShaderHelperMem));
	GSVSRingBuffer = FMemBlock::Allocate(Gnm::kGsRingSizeSetup4Mb, Gnm::kAlignmentOfBufferInBytes, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_ShaderHelperMem));

	static const int32 DefaultShaderScratchSize = 16 * 1024;
	GraphicShaderScratchBuffer.ResizeScratchBuffer(DefaultShaderScratchSize);
	ComputeShaderScratchBuffer.ResizeScratchBuffer(DefaultShaderScratchSize);	

	BlockUntilIdleLabel = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
	*BlockUntilIdleLabel = 0xFFFFFFFF;	
		
	static FName DummyLayoutName	(TEXT("DummyLayout"));
	FRHIUniformBufferLayout DummyLayout(DummyLayoutName);
	DummyLayout.ConstantBufferSize = 32;
	DummyLayout.ResourceOffset = 0;
	DummyConstantBuffer = new FGnmUniformBuffer(alloca(32), DummyLayout, UniformBuffer_MultiFrame);

	const uint32_t MemRingSize = 4 * 1024;
	ComputeQueueRing = FMemBlock::Allocate(MemRingSize, 256, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_DrawCommandBuffer));
	ComputeQueueRead = FMemBlock::Allocate(4, 16, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_DrawCommandBuffer));

	// Allocate memory for a ring buffer to hold compute queue.	
	void* MemRing = ComputeQueueRing.GetPointer();
	void* MemReadPtr = ComputeQueueRead.GetPointer();
	FMemory::Memset(MemRing, 0, MemRingSize);

	// Initialize a compute queue and map a ring buffer to it.
	static const int32 ASYNC_PIPE = 5;
	static const int32 ASYNC_QUEUE = 0;
	ComputeQueue.initialize(ASYNC_PIPE, ASYNC_QUEUE);
	ComputeQueue.map(MemRing, MemRingSize / 4, MemReadPtr);


	Ret = sceKernelCreateEqueue(&FrameCompleteQueue, "SubmitEndOfFrameQueue");
	checkf(Ret == SCE_OK, TEXT("sceKernelCreateEqueue: 0x%x"), Ret);

	Ret = sce::Gnm::addEqEvent(FrameCompleteQueue, sce::Gnm::kEqEventGfxEop, nullptr);
	checkf(Ret == SCE_OK, TEXT("addEqEvent: 0x%x"), Ret);

	// Create async task to handle sending submit done messages to GPU periodically when needed
	SubmitDoneAsyncTaskThreadRunnable = MakeShareable(new SubmitDoneTaskRunnable());
	SubmitBuffersAsyncTaskThreadRunnable = MakeShareable(new SubmitCommandBuffersTaskRunnable(SubmitDoneAsyncTaskThreadRunnable.Get(), &ComputeQueue));

#if USE_DEFRAG_ALLOCATOR	
	const int32 DefragPoolSizeInMB = CVarPS4DefragPoolSize->GetInt();
	check(DefragPoolSizeInMB > 0);

	const uint64 DefragPoolSize = (uint64)DefragPoolSizeInMB * 1024 * 1024;

	uint8* DefragPoolBase = (uint8*)FMemBlock::Allocate(DefragPoolSize, 1024 * 1024, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_DefragHeap), false).GetPointer();
	DefragAllocator.Initialize(DefragPoolBase, DefragPoolSize);
#endif

	SubmitDoneAsyncTaskThreadRunnable->Start();
	SubmitBuffersAsyncTaskThreadRunnable->Start();
	
	GetImmediateContext().InitContextBuffers();

	return true;
}

bool IsHDRDisplayOutputRequired()
{
	return GRHISupportsHDROutput && IsHDREnabled();
}

void FGnmManager::CreateBackBuffers(uint32 SizeX, uint32 SizeY)
{
	RHIShutdownFlipTracking();

	VideoOutHandle = sceVideoOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
	check(VideoOutHandle >= 0);

	// Now we have a VideoOut handle, update flags that rely on it for querying
	GRHISupportsHDROutput = SupportsHDR();

	// HDR formats
	const bool bEnableHDROutput = IsHDRDisplayOutputRequired();
	uint32 BufferOutputFormat = bEnableHDROutput ? (uint32)SCE_VIDEO_OUT_PIXEL_FORMAT_B10_G10_R10_A2_BT2020_PQ : (uint32)SCE_VIDEO_OUT_PIXEL_FORMAT_B8_G8_R8_A8_SRGB;
	EPixelFormat BackBufferFormat = bEnableHDROutput ? GRHIHDRDisplayOutputFormat : PF_B8G8R8A8;

	// allocate the buffer video memory
	void* VideoOutBuffers[NUM_RENDER_BUFFERS];
	for (int32 BufferIndex = 0; BufferIndex < NUM_RENDER_BUFFERS; BufferIndex++)
	{
		FRHIResourceCreateInfo CreateInfo;
		RenderBuffers[BufferIndex] = (FGnmTexture2D*)(FTexture2DRHIParamRef)RHICreateTexture2D(SizeX, SizeY, BackBufferFormat, 1, 1, TexCreate_RenderTargetable | TexCreate_Presentable, CreateInfo);

#if HAS_MORPHEUS
		ReprojectionBuffers[BufferIndex] = (FGnmTexture2D*)(FTexture2DRHIParamRef)RHICreateTexture2D(SizeX, SizeY, BackBufferFormat, 1, 1, TexCreate_RenderTargetable | TexCreate_Presentable, CreateInfo);
#endif

		VideoOutBuffers[BufferIndex] = RenderBuffers[BufferIndex]->Surface.BufferMem.GetPointer();
	}

	int32 Ret = sceVideoOutAddVblankEvent(VBlankEventQueue, VideoOutHandle, this);
	checkf(Ret == SCE_OK, TEXT("sceVideoOutAddVblankEvent: 0x%x"), Ret);

	// Create an equeue to receive flip and vblank events
	verify(sceKernelCreateEqueue(&VideoOutQueue, "VideoOutQueue") >= 0);
	verify(sceVideoOutAddFlipEvent(VideoOutQueue, VideoOutHandle, nullptr) >= 0);
	verify(sceVideoOutAddVblankEvent(VideoOutQueue, VideoOutHandle, nullptr) >= 0);
	verify(sceKernelAddUserEventEdge(VideoOutQueue, PS4_FLIP_USER_EVENT_ID) >= 0);

	SceVideoOutBufferAttribute Attr;
	sceVideoOutSetBufferAttribute(&Attr,
		BufferOutputFormat,
		SCE_VIDEO_OUT_TILING_MODE_TILE,
		SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
		SizeX, SizeY, SizeX);

	verify((VideoOutBufferRegistrationHandle = sceVideoOutRegisterBuffers(VideoOutHandle, 0, VideoOutBuffers, NUM_RENDER_BUFFERS, &Attr)) >= 0);

	UpdateFlipRate(true);

	if (bEnableHDROutput)
	{
		SceVideoOutMode VOut;
		sceVideoOutModeSetAny(&VOut);
		VOut.colorimetry = SCE_VIDEO_OUT_COLORIMETRY_BT2020_PQ;
		Ret = sceVideoOutConfigureOutputMode(VideoOutHandle, 0, &VOut, 0);
		checkf(Ret == SCE_OK, TEXT("sceVideoOutConfigureOutputMode failed: 0x%x"), Ret);
	}

	// initialize context
	InitializeState();
	ResetBoundResources();

	RHIInitializeFlipTracking();
}

void FGnmManager::CreateSocialScreenBackBuffers()
{
#if HAS_MORPHEUS
	UE_LOG(LogPS4, Log, TEXT("FGnmManager::CreateBackBuffers creating social screen buffers."));
	check(AuxVideoOutHandle == -1);
	AuxVideoOutHandle = sceVideoOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_AUX, 0, NULL);
	check(AuxVideoOutHandle >= 0);

	SceVideoOutResolutionStatus Status;
	int32 Ret = sceVideoOutGetResolutionStatus(AuxVideoOutHandle, &Status);
	checkf(Ret == SCE_OK, TEXT("sceVideoGetResolutionStatus failed: 0x%08x"), Ret);

	uint32 AuxSizeX = Status.fullWidth;
	uint32 AuxSizeY = Status.fullHeight;

	// allocate the buffer video memory
	void* VoidAuxVideoOutBuffers[NUM_AUX_RENDER_BUFFERS];
	for (int32 BufferIndex = 0; BufferIndex < NUM_AUX_RENDER_BUFFERS; BufferIndex++)
	{
		FRHIResourceCreateInfo CreateInfo;
		AuxRenderBuffers[BufferIndex] = (FGnmTexture2D*)(FTexture2DRHIParamRef)RHICreateTexture2D(AuxSizeX, AuxSizeY, PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable, CreateInfo);
		// AuxVideoOutBuffers need specific and unusual pixel formats, see sony docs.
		uint32 AuxVideoOutHeight = AuxSizeY * 3 / 2; // YUV has Y bytes for intensity of each pixel, then half that many UV for color of each quad of pixels
		AuxVideoOutBuffers[BufferIndex] = (FGnmTexture2D*)(FTexture2DRHIParamRef)RHICreateTexture2D(AuxSizeX, AuxVideoOutHeight, PF_L8, 1, 1, TexCreate_RenderTargetable | TexCreate_NoTiling | TexCreate_Presentable | TexCreate_UAV, CreateInfo);
		VoidAuxVideoOutBuffers[BufferIndex] = AuxVideoOutBuffers[BufferIndex]->Surface.BufferMem.GetPointer();
		AuxUAVs[BufferIndex] = RHICreateUnorderedAccessView(AuxVideoOutBuffers[BufferIndex]);
	}

	FGnmSurface* Surface = &GetGnmSurfaceFromRHITexture(AuxVideoOutBuffers[0]);
	Gnm::RenderTarget TempRenderTarget = *Surface->ColorBuffer;
	uint32 AuxPitch = TempRenderTarget.getPitch();

	SceVideoOutBufferAttribute AuxAttr;
	sceVideoOutSetBufferAttribute(&AuxAttr,
		SCE_VIDEO_OUT_PIXEL_FORMAT_YCBCR420_BT709,
		SCE_VIDEO_OUT_TILING_MODE_LINEAR, //SCE_VIDEO_OUT_TILING_MODE_TILE,
		SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
		AuxSizeX, AuxSizeY, AuxPitch);

	verify((AuxVideoOutBufferRegistrationHandle = sceVideoOutRegisterBuffers(AuxVideoOutHandle, 0, VoidAuxVideoOutBuffers, NUM_AUX_RENDER_BUFFERS, &AuxAttr)) >= 0);

	//// Set flip rate to 60hz
	//sceVideoOutSetFlipRate(VideoOutHandle, 0);
#endif // HAS_MORPHEUS
}

void FGnmManager::RecreateBackBuffers(uint32 SizeX, uint32 SizeY)
{
	// make sure all rendering commands are flushed and the GPU is idle.
	FlushRenderingCommands();

	FGnmCommandListContext& CmdListContext = GetImmediateContext();
	GnmContextType& Context = CmdListContext.GetContext();

	int32 Ret;
	check(VideoOutHandle >= 0);

	// clear out any queued reprojection flips
	SceVideoOutFlipStatus videoOutFlipStatus;
	sceVideoOutGetFlipStatus(VideoOutHandle, &videoOutFlipStatus);
	while (videoOutFlipStatus.flipPendingNum > 0)
	{
		sceVideoOutWaitVblank(VideoOutHandle);
		sceVideoOutGetFlipStatus(VideoOutHandle, &videoOutFlipStatus);
	}

	//set currentbackbuffer such that next frame won't hang forever on WaitUntilSafeForRendering.
	CurrentBackBuffer = (videoOutFlipStatus.currentBuffer + 1) % NUM_RENDER_BUFFERS;
	CurrentBackBuffer_RenderThread = CurrentBackBuffer;

	// we must flip to blank and wait for all flips to clear before we can unregister the old backbuffers.
	Ret = sceVideoOutSubmitFlip(
		VideoOutHandle,
		SCE_VIDEO_OUT_BUFFER_INDEX_BLANK,
		SCE_VIDEO_OUT_FLIP_MODE_VSYNC,
		CurrentBackBuffer);
	checkf(Ret == SCE_OK, TEXT("sceVideoOutSubmitFlip failed: 0x%x"), Ret);	

	// wait for all outstanding flips to clear, including our flip to blank.
	SceVideoOutFlipStatus flipStatus;
	for (;;)
	{
		Ret = sceVideoOutGetFlipStatus(VideoOutHandle, &flipStatus);
		checkf(Ret == SCE_OK, TEXT("sceVideoOutGetFlipStatus failed: 0x%x"), Ret);

		if (flipStatus.flipPendingNum == 0)
		{
			break;
		}	
		sceKernelUsleep(1);
	}

	// all flips are done, we should be able to unregister now.
	Ret = sceVideoOutUnregisterBuffers(VideoOutHandle, VideoOutBufferRegistrationHandle);
	checkf(Ret >= 0, TEXT("sceVideoOutUnregisterBuffers failed: 0x%x"), Ret);

	// HDR formats
	const bool bEnableHDROutput = IsHDRDisplayOutputRequired();
	uint32 BufferOutputFormat = bEnableHDROutput ? (uint32)SCE_VIDEO_OUT_PIXEL_FORMAT_B10_G10_R10_A2_BT2020_PQ : (uint32)SCE_VIDEO_OUT_PIXEL_FORMAT_B8_G8_R8_A8_SRGB;
	EPixelFormat BackBufferFormat = bEnableHDROutput ? GRHIHDRDisplayOutputFormat : PF_B8G8R8A8;

	// Skip reallocation if we don't actually need an update
	int32 PrevSizeX = RenderBuffers[0]->Surface.ColorBuffer->getWidth();
	int32 PrevSizeY = RenderBuffers[0]->Surface.ColorBuffer->getHeight();
	sce::Gnm::DataFormat PrevDataFormat = RenderBuffers[0]->Surface.ColorBuffer->getDataFormat();

	const bool bFormatChanged = SizeX != PrevSizeX || SizeY != PrevSizeY || GPixelFormats[BackBufferFormat].PlatformFormat != PrevDataFormat.m_asInt;
	ensureMsgf(bFormatChanged, TEXT("Gnm::RecreateBackBuffers called unecessarily as buffer size and type unchanged."));

	void* VideoOutBuffers[NUM_RENDER_BUFFERS];
	for (int32 BufferIndex = 0; BufferIndex < NUM_RENDER_BUFFERS; BufferIndex++)
	{
		if (bFormatChanged)
		{
			FRHIResourceCreateInfo CreateInfo;
			RenderBuffers[BufferIndex] = (FGnmTexture2D*)(FTexture2DRHIParamRef)RHICreateTexture2D(SizeX, SizeY, BackBufferFormat, 1, 1, TexCreate_RenderTargetable | TexCreate_Presentable, CreateInfo);

#if HAS_MORPHEUS
			ReprojectionBuffers[BufferIndex] = (FGnmTexture2D*)(FTexture2DRHIParamRef)RHICreateTexture2D(SizeX, SizeY, BackBufferFormat, 1, 1, TexCreate_RenderTargetable | TexCreate_Presentable, CreateInfo);
#endif
		}

		VideoOutBuffers[BufferIndex] = RenderBuffers[BufferIndex]->Surface.BufferMem.GetPointer();
	}

	SceVideoOutBufferAttribute Attr;
	sceVideoOutSetBufferAttribute(&Attr,
		BufferOutputFormat,
		SCE_VIDEO_OUT_TILING_MODE_TILE,
		SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
		SizeX, SizeY, SizeX);

	verify((VideoOutBufferRegistrationHandle = sceVideoOutRegisterBuffers(VideoOutHandle, 0, VideoOutBuffers, NUM_RENDER_BUFFERS, &Attr)) >= 0);

	UpdateFlipRate(true);

	if (bEnableHDROutput)
	{
		SceVideoOutMode VOut;
		sceVideoOutModeSetAny(&VOut);
		VOut.colorimetry = SCE_VIDEO_OUT_COLORIMETRY_BT2020_PQ;
		Ret = sceVideoOutConfigureOutputMode(VideoOutHandle, 0, &VOut, 0);
		checkf(Ret == SCE_OK, TEXT("sceVideoOutConfigureOutputMode failed: 0x%x"), Ret);
	}
}

void FGnmManager::UpdateFlipRate(bool bForce)
{
	// FlipRate: 0 -> 60Hz, 1 -> 30Hz, 2 -> 20Hz
	uint32 TargetFlipRate = FMath::Clamp(RHIGetSyncInterval(), 1u, 3u) - 1;

	// Standard or windowed VSync. Update Vsync rate if required
	if (bForce || LastFlipRate != TargetFlipRate)
	{
		int32 RetVal;
		if ((RetVal = sceVideoOutSetFlipRate(VideoOutHandle, TargetFlipRate)) != 0)
		{
			UE_LOG(LogPS4, Fatal, TEXT("sceVideoOutSetFlipRate return error code 0x%08x"), RetVal);
		}

		LastFlipRate = TargetFlipRate;
	}
}

bool FGnmManager::SupportsHDR()
{
	bool bSupportsHDR = false;
	check(VideoOutHandle >= 0);

	SceVideoOutDeviceCapabilityInfo VideoCaps;
	if (SCE_OK == sceVideoOutGetDeviceCapabilityInfo(VideoOutHandle, &VideoCaps))
	{
		bSupportsHDR = (VideoCaps.capability & SCE_VIDEO_OUT_DEVICE_CAPABILITY_BT2020_PQ) != 0;
	}

	return bSupportsHDR;
}

void FGnmManager::ChangeVideoOutputMode(EPS4OutputMode InMode)
{
	if (InMode == CurrentOutputMode)
	{
		return;
	}

#if HAS_MORPHEUS
	const bool bVrEnabled = InMode != EPS4OutputMode::Standard2D;
#endif

	check(IsInRenderingThread());

	// make sure all rendering commands are flushed and the GPU is idle.
	FRHICommandListImmediate& RHICmdList = GRHICommandList.GetImmediateCommandList();
	RHICmdList.BlockUntilGPUIdle();
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

	FGnmCommandListContext& CmdListContext = GetImmediateContext();
	GnmContextType& Context = CmdListContext.GetContext();

	int32 Ret;	

	// we must flip to blank and wait for all flips to clear before we can unregister the old backbuffers.
	Ret = sceVideoOutSubmitFlip(
		VideoOutHandle,
		SCE_VIDEO_OUT_BUFFER_INDEX_BLANK,
		SCE_VIDEO_OUT_FLIP_MODE_VSYNC,
		CurrentBackBuffer);

	checkf(Ret == SCE_OK, TEXT("sceVideoOutSubmitFlip failed: 0x%x"), Ret);	

	
	// wait for all outanding flips to clear, including our flip to blank.
	SceVideoOutFlipStatus flipStatus;
	for (;;)
	{
		Ret = sceVideoOutGetFlipStatus(VideoOutHandle, &flipStatus);
		checkf(Ret == SCE_OK, TEXT("sceVideoOutGetFlipStatus failed: 0x%x"), Ret);

		if (flipStatus.flipPendingNum == 0)
		{
			break;
		}	
		sceKernelUsleep(1);
	}

	// all flips are done, we should be able to unregister now.
	Ret = sceVideoOutUnregisterBuffers(VideoOutHandle, VideoOutBufferRegistrationHandle);
	checkf(Ret >= 0, TEXT("sceVideoOutUnregisterBuffers failed: 0x%x"), Ret);

	int32 SizeX = RenderBuffers[0]->Surface.ColorBuffer->getWidth();
	int32 SizeY = RenderBuffers[0]->Surface.ColorBuffer->getHeight();
	
	SceVideoOutBufferAttribute VideoOutBufferAttribute;
	sceVideoOutSetBufferAttribute(
		&VideoOutBufferAttribute,
		SCE_VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB,
		SCE_VIDEO_OUT_TILING_MODE_TILE,
		SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
		SizeX,
		SizeY,
		SizeX);	

	void* VideoOutBuffers[NUM_RENDER_BUFFERS];
#if HAS_MORPHEUS
	VideoOutBufferAttribute.option = bVrEnabled ? SCE_VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_VR : SCE_VIDEO_OUT_BUFFER_ATTRIBUTE_OPTION_NONE;
	if (InMode == EPS4OutputMode::MorpheusRender60Scanout120)
	{
		for (int32 BufferIndex = 0; BufferIndex < NUM_RENDER_BUFFERS; BufferIndex++)
		{
			VideoOutBuffers[BufferIndex] = ReprojectionBuffers[BufferIndex]->Surface.BufferMem.GetPointer();
		}		
	}
	else
#endif
	{
		for (int32 BufferIndex = 0; BufferIndex < NUM_RENDER_BUFFERS; BufferIndex++)
		{
			VideoOutBuffers[BufferIndex] = RenderBuffers[BufferIndex]->Surface.BufferMem.GetPointer();
		}
	}	
	
	verify((VideoOutBufferRegistrationHandle = sceVideoOutRegisterBuffers(VideoOutHandle, 0, VideoOutBuffers, NUM_RENDER_BUFFERS, &VideoOutBufferAttribute)) >= 0);

	SceVideoOutMode VOut;
	sceVideoOutModeSetAny(&VOut);

#if HAS_MORPHEUS
	if (bVrEnabled)
	{
		VOut.resolution = SCE_VIDEO_OUT_RESOLUTION_ANY_VR_VIEW;
		switch (InMode)
		{
		case EPS4OutputMode::MorpheusRender60Scanout120:
			VOut.refreshRate = SCE_VIDEO_OUT_REFRESH_RATE_119_88HZ;
			break;
		case EPS4OutputMode::MorpheusRender90Scanout90:
			VOut.refreshRate = SCE_VIDEO_OUT_REFRESH_RATE_89_91HZ;
			break;
		case EPS4OutputMode::MorpheusRender120Scanout120:
			VOut.refreshRate = SCE_VIDEO_OUT_REFRESH_RATE_119_88HZ;
			break;
		default:
			check(false);
			break;
		}	
	}
#endif

	// skip this if vr was previously enabled
	Ret = sceVideoOutConfigureOutputMode(VideoOutHandle, 0, &VOut, 0);
	if( Ret != SCE_OK )
	{
#if HAS_MORPHEUS
		if( bVrEnabled )
		{
			checkf(false, TEXT("sceVideoOutConfigureOutputMode failed: 0x%08X\nCheck that Application Supports Morpheus is enabled in your param.sfo\n"), Ret);
		}
		else
#endif
		{
			checkf(false, TEXT("sceVideoOutConfigureOutputMode failed: 0x%08X\n"), Ret);
		}
	}
	else
	{
		UE_LOG(LogPS4, Log, TEXT("ChangeVideoOutputMode: changed video output mode to %i refreshRate to 0x%x."), (int)InMode, VOut.refreshRate);
	}

	CurrentOutputMode = InMode;
}
void FGnmManager::CacheReprojectionData(GnmBridge::MorpheusDistortionData& DistortionData)
{
#if HAS_MORPHEUS
	SCOPED_NAMED_EVENT_TEXT("FGnmManager::CacheReprojectionData", FColor::Turquoise);
	FScopeLock Lock(&ReprojectionDataCriticalSection);
	ReprojectionDataBuffers[CurrentContextIndex] = DistortionData;
#endif
}

void FGnmManager::HandleMorpheusReprojection()
{
#if HAS_MORPHEUS
	if (CurrentOutputMode != EPS4OutputMode::Standard2D)
	{
		check(CurrentOutputMode == EPS4OutputMode::MorpheusRender60Scanout120
			|| CurrentOutputMode == EPS4OutputMode::MorpheusRender90Scanout90
			|| CurrentOutputMode == EPS4OutputMode::MorpheusRender120Scanout120);

		{
			FScopeLock Lock2(&ReprojectionDataCriticalSection);
			if (bDo2DVRReprojection)
			{
				if (bReprojectionData2DVRDirty)
				{
					ReprojectionData2DVR.RHIContextIndex = CurrentContextIndex;
					ReprojectionData2DVR.FrameCounter = FrameCounter;
					ApplyMorpheus2DVRReprojection(ReprojectionData2DVR);
				}

				{
					// We need to update this for motion controller tracking if we are not doing vr reprojection.
					FScopeLock Lock(&LastApplyReprojectionInfoMutex);
					LastApplyReprojectionInfo.FrameNumber = GFrameNumberRenderThread - 2;
					LastApplyReprojectionInfo.PreviousFlipTime = GetLastFlipTime();
				}
			}
			else
			{
				GnmBridge::MorpheusDistortionData& ReprojectionData = ReprojectionDataBuffers[CurrentContextIndex];
				ReprojectionData.RHIContextIndex = CurrentContextIndex;
				ReprojectionData.FrameCounter = FrameCounter;

				if (!MorpheusReprojectionAsyncTaskThreadRunnable.IsValid())
				{
					MorpheusReprojectionAsyncTaskThreadRunnable = MakeShareable(new MorpheusReprojectionTaskRunnable());
					MorpheusReprojectionAsyncTaskThreadRunnable->Start();
				}

				MorpheusReprojectionAsyncTaskThreadRunnable->QueueReprojection(ReprojectionData);
			}
		}
	}
	else
	{
		// We need to update this for motion controller tracking if we are not doing vr reprojection.
		FScopeLock Lock(&LastApplyReprojectionInfoMutex);
		LastApplyReprojectionInfo.FrameNumber = GFrameNumberRenderThread - 2;
		LastApplyReprojectionInfo.PreviousFlipTime = GetLastFlipTime();
	}

#endif
}

void FGnmManager::ApplyMorpheusReprojection(GnmBridge::MorpheusDistortionData& DistortionData)
{
#if HAS_MORPHEUS
	LLM_SCOPE_PS4(ELLMTagPS4::GnmMisc);

	SCOPED_NAMED_EVENT_TEXT("FGnmManager::ApplyMorpheusReprojection", FColor::Turquoise);

	if (!DistortionData.EyeTextureL.IsValid() || !DistortionData.EyeTextureR.IsValid())
	{
		DistortionData.EyeTextureL = GWhiteTexture->TextureRHI->GetTexture2D();
		DistortionData.EyeTextureR = GWhiteTexture->TextureRHI->GetTexture2D();		
	}
	//We only have a single eye texture because the engine renders to a combined texture for both eyes.
	FGnmSurface& EyeTargetL = GetGnmSurfaceFromRHITexture(DistortionData.EyeTextureL);
	FGnmSurface& EyeTargetR = GetGnmSurfaceFromRHITexture(DistortionData.EyeTextureR);

	static bool bInitSampler = true;
	static Gnm::Sampler ReprojectionSampler;
	static sce::Gnm::Sampler DistortionSampler;
	if (bInitSampler)
	{
		ReprojectionSampler.init();
		ReprojectionSampler.setWrapMode(ReprojectionSamplerWrapMode, ReprojectionSamplerWrapMode, ReprojectionSamplerWrapMode);
		ReprojectionSampler.setXyFilterMode(sce::Gnm::kFilterModeBilinear, sce::Gnm::kFilterModeBilinear);
		ReprojectionSampler.setBorderColor(sce::Gnm::kBorderColorOpaqueBlack);

		DistortionSampler.init();
		DistortionSampler.setWrapMode(sce::Gnm::kWrapModeClampBorder,
			sce::Gnm::kWrapModeClampBorder,
			sce::Gnm::kWrapModeClampBorder);
		DistortionSampler.setXyFilterMode(sce::Gnm::kFilterModeBilinear, sce::Gnm::kFilterModeBilinear);
		DistortionSampler.setBorderColor(sce::Gnm::kBorderColorOpaqueBlack);
		bInitSampler = false;
	}

	SceHmdDistortionTextureTranslation LeftTranslation;
	SceHmdDistortionTextureTranslation RightTranslation;

	if (DistortionData.EyeTextureL == DistortionData.EyeTextureR)
	{
		//hack support for both eyes on a combined rendertarget.  must fudge offset values
		//left and right translation
		const float ScaleX = 0.5f / (DistortionData.DeviceTanOut + DistortionData.DeviceTanIn);
		const float ScaleY = 1.f / (DistortionData.DeviceTanTop + DistortionData.DeviceTanBottom);

		const float LeftOffsetX = DistortionData.DeviceTanOut * ScaleX;
		const float RightOffsetX = DistortionData.DeviceTanIn * ScaleX + 0.5f;

		const float OffsetY = DistortionData.DeviceTanTop * ScaleY;

		LeftTranslation.offset_x = LeftOffsetX;
		LeftTranslation.offset_y = OffsetY;
		LeftTranslation.scale_x = ScaleX;
		LeftTranslation.scale_y = ScaleY;

		RightTranslation.offset_x = RightOffsetX;
		RightTranslation.offset_y = OffsetY;
		RightTranslation.scale_x = ScaleX;
		RightTranslation.scale_y = ScaleY;
	}
	else
	{
		const float ScaleX = 1.0f / (DistortionData.DeviceTanOut + DistortionData.DeviceTanIn);
		const float ScaleY = 1.0f / (DistortionData.DeviceTanTop + DistortionData.DeviceTanBottom);
		const float OffsetX = DistortionData.DeviceTanOut * ScaleX;
		const float OffsetY = DistortionData.DeviceTanTop * ScaleY;
		LeftTranslation.offset_x = RightTranslation.offset_x = OffsetX;
		LeftTranslation.offset_y = RightTranslation.offset_y = OffsetY;
		LeftTranslation.scale_x = RightTranslation.scale_x = ScaleX;
		LeftTranslation.scale_y = RightTranslation.scale_y = ScaleY;
	}

	bool bNative120hz = false;
	SceHmdReprojectionParameter parameter = {};
	parameter.srcL = EyeTargetL.Texture;
	parameter.srcR = EyeTargetR.Texture;
	check(DistortionData.RHIContextIndex >= 0 && DistortionData.RHIContextIndex < 2);
	parameter.label = ReprojectionLabels[DistortionData.RHIContextIndex];
	if (*parameter.label != 0)
	{
		// label should always be clear here, but the cvar switch between previous and current context index break that (for one frame)
		UE_LOG(LogPS4, Warning, TEXT("ApplyMorpheusReprojection parameter.label != 0.  Ok if you just switched to/from previous context mode."));
	}
	*parameter.label = 1;  // set to 0 when reprojection is done
	parameter.sampler = &ReprojectionSampler;
	parameter.timing = SCE_HMD_REPROJECTION_REPROJECTION_TIMING_4000USEC;
	parameter.leftTranslation = LeftTranslation;
	parameter.rightTranslation = RightTranslation;
	parameter.predictionType = SCE_HMD_REPROJECTION_PREDICTION_AUTO;
	parameter.predictionTime = 0; // 3000 + (bNative120hz ? 8333 : 16666);
	parameter.flags = SCE_HMD_REPROJECTION_START_FLAGS_WITH_UPDATE_MOTION_SENSOR_DATA | SCE_HMD_REPROJECTION_START_FLAGS_HIGH_PRIORITY_REPROJECTION;
	//parameter.flags |= SCE_HMD_REPROJECTION_START_FLAGS_ONLY_DISTORTION_AND_FLIP;
	//parameter.flags |= SCE_HMD_REPROJECTION_START_FLAGS_SKIP_TIMESTAMP_CHECK;
	//parameter.flags |= SCE_HMD_REPROJECTION_START_FLAGS_DEBUG_SHOW_MESSAGE;

	SceHmdReprojectionTrackerState trackerState = {};
	trackerState.position = DistortionData.TrackerPosition;
	trackerState.orientation = DistortionData.TrackerOrientationQuat;
	trackerState.timestamp = DistortionData.TrackerTimestamp;
	trackerState.sensorReadSystemTimestamp = DistortionData.SensorReadSystemTimestamp;
	trackerState.userFrameNumber = DistortionData.FrameNumber;

	if (DistortionData.OverlayTextureL.IsValid())
	{
		check(DistortionData.OverlayTextureR.IsValid());

		sce::Gnm::Sampler OverlaySampler;
		OverlaySampler.init();
		OverlaySampler.setWrapMode(sce::Gnm::kWrapModeClampBorder, sce::Gnm::kWrapModeClampBorder, sce::Gnm::kWrapModeClampBorder);
		OverlaySampler.setXyFilterMode(sce::Gnm::kFilterModeBilinear, sce::Gnm::kFilterModeBilinear);
		OverlaySampler.setBorderColor(sce::Gnm::kBorderColorOpaqueBlack);

		SceHmdReprojectionOverlayParameter OverlayParam;
		OverlayParam.srcL = GetGnmSurfaceFromRHITexture(DistortionData.OverlayTextureL).Texture;
		OverlayParam.srcR = GetGnmSurfaceFromRHITexture(DistortionData.OverlayTextureR).Texture;
		OverlayParam.sampler = &OverlaySampler;
				
		const float ScaleX = 1.0f / (DistortionData.DeviceTanOut + DistortionData.DeviceTanIn);
		const float ScaleY = 1.0f / (DistortionData.DeviceTanTop + DistortionData.DeviceTanBottom);
		const float OffsetX = DistortionData.DeviceTanOut * ScaleX;
		const float OffsetY = DistortionData.DeviceTanTop * ScaleY;
		OverlayParam.leftTranslation.offset_x = OverlayParam.rightTranslation.offset_x = OffsetX;
		OverlayParam.leftTranslation.offset_y = OverlayParam.rightTranslation.offset_y = OffsetY;
		OverlayParam.leftTranslation.scale_x = OverlayParam.rightTranslation.scale_x = ScaleX;
		OverlayParam.leftTranslation.scale_y = OverlayParam.rightTranslation.scale_y = ScaleY;
		FMemory::Memzero(OverlayParam.reserved);
		
		int ret = sceHmdReprojectionStartWithOverlay(&parameter, &trackerState, trackerState.userFrameNumber, &OverlayParam, nullptr);
		SCE_GNM_ASSERT(ret >= 0);
		if (ret != SCE_OK)
		{
			// on failure clear the label manually
			*parameter.label = 0;
		}
	}
	else
	{				
		int ret = sceHmdReprojectionStart(&parameter, &trackerState, trackerState.userFrameNumber, nullptr);
		SCE_GNM_ASSERT(ret >= 0);
		if (ret != SCE_OK)
		{
			// on failure clear the label manually
			*parameter.label = 0;
		}
	}

	// If the MorpheusHandle is -1 we are not getting new data, so we have to grab the framenumber elsewhere.
	// The minus two is about how far back this projection should have started.
	const uint32_t FrameNumber = (DistortionData.MorpheusHandle != -1) ? trackerState.userFrameNumber : GFrameNumberRenderThread - 2;

	{
		FScopeLock Lock2(&LastApplyReprojectionInfoMutex);
		LastApplyReprojectionInfo.FrameNumber = FrameNumber;
		LastApplyReprojectionInfo.PreviousFlipTime = GetLastFlipTime();
	}

#endif
}

void FGnmManager::StartMorpheus2DVRReprojection(GnmBridge::Morpheus2DVRReprojectionData& ReprojectionData)
{
#if HAS_MORPHEUS
	FScopeLock Lock(&ReprojectionDataCriticalSection);
	if (bDo2DVRReprojection)
	{
		UE_LOG(LogPS4, Warning, TEXT("StartMorpheus2DVRReprojection called, but already in 2DVR mode! May need a stack of 2DVR requests or something."));
	}
	ReprojectionData2DVR = ReprojectionData;
	bReprojectionData2DVRDirty = true;
	bDo2DVRReprojection = true;
#endif
}

void FGnmManager::StopMorpheus2DVRReprojection()
{
#if HAS_MORPHEUS
	FScopeLock Lock(&ReprojectionDataCriticalSection);
	bDo2DVRReprojection = false;
	bReprojectionData2DVRDirty = false;
#endif
}

void FGnmManager::ApplyMorpheus2DVRReprojection(GnmBridge::Morpheus2DVRReprojectionData& ReprojectionData)
{
#if HAS_MORPHEUS
	LLM_SCOPE_PS4(ELLMTagPS4::GnmMisc);

	SCOPED_NAMED_EVENT_TEXT("FGnmManager::ApplyMorpheus2DVRReprojection", FColor::Turquoise);

	if (!ReprojectionData.Texture.IsValid())
	{
		UE_LOG(LogPS4, Warning, TEXT("ApplyMorpheus2DVRReprojection: reprojection texture is invalid. Falling back to GWhiteTexture."));
		ReprojectionData.Texture = GWhiteTexture->TextureRHI->GetTexture2D();
	}
	if (ReprojectionData.Texture->GetFormat() != PF_B8G8R8A8)
	{
		UE_LOG(LogPS4, Warning, TEXT("ApplyMorpheus2DVRReprojection: reprojection texture format is incorrect. Must be B8G8R8A8. Falling back to GWhiteTexture."));
		ReprojectionData.Texture = GWhiteTexture->TextureRHI->GetTexture2D();
	}

	FGnmSurface& Target = GetGnmSurfaceFromRHITexture(ReprojectionData.Texture);

	static bool bInitSampler = true;
	static Gnm::Sampler ReprojectionSampler;
	if (bInitSampler)
	{
		ReprojectionSampler.init();
		ReprojectionSampler.setWrapMode(sce::Gnm::kWrapModeClampBorder, sce::Gnm::kWrapModeClampBorder, sce::Gnm::kWrapModeClampBorder);
		ReprojectionSampler.setXyFilterMode(sce::Gnm::kFilterModeBilinear, sce::Gnm::kFilterModeBilinear);
		ReprojectionSampler.setBorderColor(sce::Gnm::kBorderColorOpaqueBlack);
		bInitSampler = false;
	}

	SceHmdDistortionTextureTranslation TextureTranslation;
	{
		TextureTranslation.offset_x = ReprojectionData.Offset.X;
		TextureTranslation.offset_y = ReprojectionData.Offset.Y;
		TextureTranslation.scale_x  = ReprojectionData.Scale.X;
		TextureTranslation.scale_y  = ReprojectionData.Scale.Y;
	}

	SceHmdReprojection2dVrParameter parameter = {};
	parameter.srcTexture = Target.Texture;
	parameter.sampler = &ReprojectionSampler;
	parameter.textureTranslation = TextureTranslation;
	check(ReprojectionData.RHIContextIndex >= 0 && ReprojectionData.RHIContextIndex < 2);
	parameter.label = ReprojectionLabels[ReprojectionData.RHIContextIndex];;
	if (*parameter.label != 0)
	{
		// label should always be clear here, but the cvar switch between previous and current context index break that (for one frame)
		UE_LOG(LogPS4, Warning, TEXT("ApplyMorpheus2DVRReprojection parameter.label != 0.  Ok if you just switched to/from previous context mode."));
	}
	*parameter.label = 1;  // set to 0 when reprojection is done
	parameter.timing = SCE_HMD_REPROJECTION_REPROJECTION_TIMING_4000USEC;

	{
		static int64 FlipNumber2DVR = 0;
		int ret = sceHmdReprojectionStart2dVr(&parameter, FlipNumber2DVR++, nullptr);
		SCE_GNM_ASSERT(ret >= 0);
		if (ret != SCE_OK)
		{
			// on failure clear the label manually
			*parameter.label = 0;
		}
	}
#endif
}

void FGnmManager::SetReprojectionSamplerWrapMode(sce::Gnm::WrapMode InReprojectionSamplerWrapMode)
{
	ReprojectionSamplerWrapMode = InReprojectionSamplerWrapMode;
}

//function requires a full RHI flush before calling.
void FGnmManager::ChangeOutputMode(EPS4OutputMode InPS4OutputMode)
{
#if HAS_MORPHEUS
	LLM_SCOPE_PS4(ELLMTagPS4::GnmMisc);

	check(IsInRenderingThread());
	if (CurrentOutputMode == InPS4OutputMode)
	{
		return;
	}

	// switching between PS4 VR output modes at runtime is not supported, only between 2D and a VR mode.
	check((CurrentOutputMode != EPS4OutputMode::Standard2D && InPS4OutputMode == EPS4OutputMode::Standard2D) ||
		(CurrentOutputMode == EPS4OutputMode::Standard2D && InPS4OutputMode != EPS4OutputMode::Standard2D));

	// we're turning off reprojection
	if (InPS4OutputMode == EPS4OutputMode::Standard2D)
	{
		//shut down any async reprojection tasks
		sceHmdReprojectionStop();

		//clear out any queued reprojection flips
		SceVideoOutFlipStatus videoOutFlipStatus;
		sceVideoOutGetFlipStatus(VideoOutHandle, &videoOutFlipStatus);
		while (videoOutFlipStatus.flipPendingNum > 0)
		{
			sceVideoOutWaitVblank(VideoOutHandle);
			sceVideoOutGetFlipStatus(VideoOutHandle, &videoOutFlipStatus);
		}		

		//set currentbackbuffer such that next frame won't hang forever on WaitUntilSafeForRendering.
		CurrentBackBuffer = (videoOutFlipStatus.currentBuffer + 1) % NUM_RENDER_BUFFERS;
		CurrentBackBuffer_RenderThread = CurrentBackBuffer;

		sceHmdReprojectionUnsetDisplayBuffers();
		
		ChangeVideoOutputMode(InPS4OutputMode);
	}
	else if (InPS4OutputMode != EPS4OutputMode::Standard2D)
	{
		static bool ReprojectionInitialized = false;
		if (!ReprojectionInitialized)
		{
			const uint32 OnionAlign = sceHmdReprojectionQueryOnionBuffAlign();
			const uint32 OnionSize = sceHmdReprojectionQueryOnionBuffSize();
			const uint32 GarlicAlign = sceHmdReprojectionQueryGarlicBuffAlign();
			const uint32 GarlicSize = sceHmdReprojectionQueryGarlicBuffSize();

			const SceHmdReprojectionResourceInfo resource
			{
				FMemBlock::Allocate(OnionSize, OnionAlign, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Texture)).GetPointer(),
				FMemBlock::Allocate(GarlicSize, GarlicAlign, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_Texture)).GetPointer(),
				SCE_KERNEL_PRIO_FIFO_HIGHEST,
				SCE_KERNEL_CPUMASK_6CPU_ALL,
				3,
				3,
				{}
			};
			int32 Ret;
			Ret = sceHmdReprojectionInitialize(&resource, SCE_HMD_REPROJECTION_TYPE_BORDER_FOR_SINGLE, nullptr);

			checkf(Ret >= 0, TEXT("sceHmdReprojectionInitialized failed with 0x%08x"), Ret);
			ReprojectionInitialized = true;
		}
		ChangeVideoOutputMode(InPS4OutputMode);
		int32 Ret = sceHmdReprojectionSetDisplayBuffers(VideoOutHandle, 0, 1, nullptr);
		checkf(Ret >= 0, TEXT("sceHmdReprojectionSetDisplayBuffers failed with 0x%08x"), Ret);

		// Start 2DVR mode again, if we are supposed to be in it.
		{
			FScopeLock Lock(&ReprojectionDataCriticalSection);
			if (bDo2DVRReprojection)
			{
				bReprojectionData2DVRDirty = true;
			}
		}
	}
#else
	checkf(false, TEXT("Tried to change EPS4OutputMode to %i but HAS_MORPHEUS is not defined.  Cannot change mode.  If you are trying to run PSVR you probably need to rebuild the engine with the Morpheus Plugin enabled."), (int32)InPS4OutputMode);
#endif
}

EPS4OutputMode FGnmManager::GetOutputMode() const
{
	return CurrentOutputMode;
}

//function requires a full RHI flush before calling.
void FGnmManager::ChangeSocialScreenOutputMode(EPS4SocialScreenOutputMode InPS4SocialScreenOutputMode)
{
#if HAS_MORPHEUS
	FScopeLock Lock(&SocialScreenOutputModeMutex);

	UE_LOG(LogPS4, Log, TEXT("ChangeSocialScreenOutputMode from %i to %i."), static_cast<uint32>(CurrentSocialScreenOutputMode), static_cast<uint32>(InPS4SocialScreenOutputMode));

	check(IsInRenderingThread());
	if (CurrentSocialScreenOutputMode == InPS4SocialScreenOutputMode)
	{
		return;
	}

	// If the new mode is not mirroring or the one we already have configured we need to configure it.
	if (ConfiguredSocialScreenOutputMode != InPS4SocialScreenOutputMode && InPS4SocialScreenOutputMode != EPS4SocialScreenOutputMode::Mirroring)
	{
		if (ConfiguredSocialScreenOutputMode != EPS4SocialScreenOutputMode::Mirroring)
		{
			// cleanup the currently configured mode, we need to configure a different one
			check(InPS4SocialScreenOutputMode != EPS4SocialScreenOutputMode::Mirroring);

			// exit the mode if necessary
			if (CurrentSocialScreenOutputMode == ConfiguredSocialScreenOutputMode)
			{
				int32 Ret = sceSocialScreenSetMode(SCE_SOCIAL_SCREEN_MODE_MIRRORING);
				if (Ret == SCE_OK)
				{
					UE_LOG(LogPS4, Log, TEXT("ChangeSocialScreenOutputMode set mode to mirroring while we close the configured mode."));
					CurrentSocialScreenOutputMode = EPS4SocialScreenOutputMode::Mirroring;
				}
				else
				{
					UE_LOG(LogPS4, Error, TEXT("sceSocialScreenSetMode failed with code 0x%x.  Mode not changed."), Ret);
					return;
				}
			}

			// close the mode
			int32 Ret = sceSocialScreenCloseSeparateMode();
			if (Ret == SCE_OK)
			{
				UE_LOG(LogPS4, Log, TEXT("ChangeSocialScreenOutputMode previously configured mode closed."));
			}
			else
			{
				UE_LOG(LogPS4, Error, TEXT("sceSocialScreenCloseSeparateMode failed with code 0x%x.  Mode not changed."), Ret);
				return;
			}
		}

		// configure and open the new mode
		SceSocialScreenSeparateModeParameter SSParam;
		sceSocialScreenInitializeSeparateModeParameter(&SSParam);
		switch (InPS4SocialScreenOutputMode)
		{
		case EPS4SocialScreenOutputMode::Separate30FPS:
			SSParam.frameRate = SCE_SOCIAL_SCREEN_FRAME_RATE_30;
			break;
		//case EPS4SocialScreenOutputMode::Separate60FPS:
		//	SSParam.frameRate = SCE_SOCIAL_SCREEN_FRAME_RATE_60;
			break;
		default:
			check(false);
		}
		int32 Ret = sceSocialScreenConfigureSeparateMode(&SSParam);
		if (Ret != SCE_OK)
		{
			UE_LOG(LogPS4, Error, TEXT("sceSocialScreenConfigureSeparateMode failed with code 0x%x.  Separate mirror mode will not start."), Ret);
			return;
		}

		Ret = sceSocialScreenOpenSeparateMode();
		if (Ret == SCE_OK)
		{
			UE_LOG(LogPS4, Log, TEXT("ChangeSocialScreenOutputMode opened mode %i."), static_cast<uint32>(InPS4SocialScreenOutputMode));
			ConfiguredSocialScreenOutputMode = InPS4SocialScreenOutputMode;
		}
		else
		{
			if (Ret == SCE_SOCIAL_SCREEN_ERROR_CANNOT_OPEN_SEPARATE_MODE)
			{
				UE_LOG(LogPS4, Error, TEXT("sceSocialScreenOpenSeparateMode failed with code SCE_SOCIAL_SCREEN_ERROR_CANNOT_OPEN_SEPARATE_MODE(0x%x).  The dialog to tell you the user how to fix this is supposed to pop up now."), Ret);
				return;
			}
			else
			{
				UE_LOG(LogPS4, Error, TEXT("sceSocialScreenOpenSeparateMode failed with code 0x%x.  Separate mirror mode will not start."), Ret);
				return;
			}
		}
	}


	// The mode we want to switch to is Mirroring or is configured now
	{
		check(InPS4SocialScreenOutputMode == EPS4SocialScreenOutputMode::Mirroring || InPS4SocialScreenOutputMode == ConfiguredSocialScreenOutputMode);

		SceSocialScreenMode NewSCESocialScreenMode = SCE_SOCIAL_SCREEN_MODE_MIRRORING;
		switch (InPS4SocialScreenOutputMode)
		{
		case EPS4SocialScreenOutputMode::Mirroring:
			NewSCESocialScreenMode = SCE_SOCIAL_SCREEN_MODE_MIRRORING;
			break;
		case EPS4SocialScreenOutputMode::Separate30FPS: //fall through
		//case EPS4SocialScreenOutputMode::Separate60FPS:
			NewSCESocialScreenMode = SCE_SOCIAL_SCREEN_MODE_SEPARATE;
			break;
		default:
			check(false);
		}
		int32 Ret = sceSocialScreenSetMode(NewSCESocialScreenMode);
		if (Ret == SCE_OK)
		{
			UE_LOG(LogPS4, Log, TEXT("ChangeSocialScreenOutputMode switched mode to %i."), static_cast<uint32>(InPS4SocialScreenOutputMode));
			CurrentSocialScreenOutputMode = InPS4SocialScreenOutputMode;
		}
		else
		{
			UE_LOG(LogPS4, Error, TEXT("sceSocialScreenSetMode failed with code 0x%x.  Mode not changed."), Ret);
			return;
		}
	}
#else
	checkf(false, TEXT("Social Screen is a morpheus only feature."));
#endif // HAS_MORPHEUS
}

EPS4SocialScreenOutputMode FGnmManager::GetSocialScreenOutputMode() const
{
	FScopeLock Lock(&SocialScreenOutputModeMutex);
	return CurrentSocialScreenOutputMode;
}

FTexture2DRHIRef FGnmManager::GetSocialScreenRenderTarget() const
{
#if HAS_MORPHEUS
	check(IsInRenderingThread);
	// Get the *next* render buffer, the current index one is the one that will be copied to output this frame.
	FGnmTexture2D* tex = AuxRenderBuffers[(CurrentAuxBufferIndex_RenderThread + 1) % NUM_AUX_RENDER_BUFFERS];
	return tex;
#else
	return nullptr;
#endif
}

bool FGnmManager::ShouldRenderSocialScreenThisFrame() const
{
#if HAS_MORPHEUS
	check(IsInRenderingThread);
	return bCurrentFrameIsAuxFrame_RenderThread;
#else
	return false;
#endif
}

void FGnmManager::TranslateSocialScreenOutput(FRHICommandListImmediate& RHICmdList)
{
	TranslateRGBToYUV(RHICmdList);
}

uint64 FGnmManager::GetLastFlipTime()
{	
	SceVideoOutFlipStatus videoOutFlipStatus;
	sceVideoOutGetFlipStatus(VideoOutHandle, &videoOutFlipStatus);
	return videoOutFlipStatus.processTime;	
}

uint32 FGnmManager::GetVideoOutPort()
{
	return VideoOutHandle;
}

void FGnmManager::GetLastApplyReprojectionInfo(GnmBridge::MorpheusApplyReprojectionInfo& OutInfo)
{
#if HAS_MORPHEUS
	FScopeLock Lock(&LastApplyReprojectionInfoMutex);
	OutInfo = LastApplyReprojectionInfo;
#endif
}


/**
 * Convert an Unreal format to Gnm DataFormat, and optionally make it an SRGB format
 *
 * @param Format Unreal format
 * @param bMakeSRGB If true, and the format supports it, the returned format will be SRGB
 *
 * @return a valid DataFormat object
 */
Gnm::DataFormat FGnmManager::GetDataFormat(EPixelFormat PixelFormat, bool bMakeSRGB)
{
	// look it up
	Gnm::DataFormat Format;
	Format.m_asInt = GPixelFormats[PixelFormat].PlatformFormat;

	// make the format SRGB if requested
	if (bMakeSRGB && Format.getTextureChannelType() == Gnm::kTextureChannelTypeUNorm)
	{
		Format.m_bits.m_channelType = Gnm::kTextureChannelTypeSrgb;
	}

	return Format;
}

/**
 * Initialize the context, and any global state
 */
void FGnmManager::InitializeState()
{	
	bGPUFlipWaitedThisFrame = false;
	ImmediateContext->InitializeStateForFrameStart();
	ImmediateComputeContext->InitializeStateForFrameStart();
}

void FGnmManager::DumpPM4()
{	
	bDumpPM4OnEndFrame = true;
}

void FGnmManager::DumpPM4Internal(uint32 ContextIndex)
{	
#if PS4_RECORD_SUBMITS
	TArray<SGnmCommandSubmission>& ContextSubmissions = SubmittedCommandsDebug[ContextIndex];
	int32 DumpIndex = 0;
	for (int i = 0; i < ContextSubmissions.Num(); ++i)
	{
		SGnmCommandSubmission& Submission = ContextSubmissions[i];
		for (int j = 0; j < Submission.SubmissionAddrs.Num(); j++, DumpIndex++)
		{
			const int32 PathSize = 256;
			char PM4FilePath[PathSize];

			FCStringAnsi::Snprintf(PM4FilePath, sizeof(PM4FilePath), "/hostapp/pm4dump%i.txt", DumpIndex);
			FILE* log = fopen(PM4FilePath, "w");
			Gnm::Pm4Dump::dumpPm4PacketStream(log, (uint32*)Submission.SubmissionAddrs[j], Submission.SubmissionSizesBytes[j] / sizeof(uint32));
			fclose(log);
		}
	}
#endif
}

void FGnmManager::UpdateVsyncMargins()
{
	float NewTop, NewBottom;
	RHIGetPresentThresholds(NewTop, NewBottom);

	if (RenderBuffers[0].GetReference() && ((BottomVsyncMargin != NewBottom) || (TopVsyncMargin != NewTop)))
		{
		BottomVsyncMargin = NewBottom;
		TopVsyncMargin = NewTop;

				uint32 BackBufferHeight = RenderBuffers[0]->Surface.Texture->getHeight();
				uint32 TopMarginLines = (uint32)(TopVsyncMargin * (float)BackBufferHeight);
				uint32 BottomMarginLines = (uint32)(BottomVsyncMargin * (float)BackBufferHeight);

		// Neo doesn't support the bottom margin. Always set both here. We decide which to use when we call submitAndFlipCommandBuffers.
				int32 Ret = sceVideoOutSetWindowModeMargins(VideoOutHandle, TopMarginLines, BottomMarginLines);
				ensureMsgf(Ret == SCE_OK, TEXT("sceVideoOutSetWindowModeMargins failed: 0x%x, %i, %i"), Ret, TopMarginLines, BottomMarginLines);
			}
}

/**
 * Startup the frame
 */
void FGnmManager::BeginFrame()
{
	bIsCurrentlyRendering = true;
}

/**
 * Block the CPU until the GPU has completed rendering the previously submitted frame
 */
void FGnmManager::WaitForGPUFrameCompletion()
{
	bool bIgnoreGPUTimeout = false;
	uint32 GPUTimeoutCounter = 0;

	int32 PreviousContextIndex = 1 - CurrentContextIndex;

	int32 FrameToCompare = FrameCounter - 1;

	const static int32 NumEventsArray = 4;
	SceKernelEvent EventArray[NumEventsArray];
	int32 NumReceivedEvents = 0;

	//wait in intervals for an event.  Theoretically possible for the kernel to lose an event so we don't want to wait forever.
	//https://ps4.scedev.net/technotes/view/141/1
	SceKernelUseconds Timeout = 60000;
	while( *EndOfFrameLabels[PreviousContextIndex] != FrameToCompare )	
	{
		if (GPS4EventBasedFrameEnd != 0)
		{
			int32 Ret = sceKernelWaitEqueue(FrameCompleteQueue, EventArray, NumEventsArray, &NumReceivedEvents, &Timeout);
			checkf(Ret == SCE_OK || Ret == SCE_KERNEL_ERROR_ETIMEDOUT, TEXT("sceKernelWaitEqueue failed: 0x%x"), Ret);
			if (Ret == SCE_KERNEL_ERROR_ETIMEDOUT)
			{
				UE_LOG(LogPS4, Warning, TEXT("EOF Event Wait Timed Out"));
			}
		}
		else
		{
			// This time is counted toward the present idle timer external to this funciton.
			// Don't double up by counting it toward the thread wait time.
			FPlatformProcess::SleepNoStats(0);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && !ENABLE_GPU_DEBUGGER

		if (FPS4Misc::IsRunningOnDevKit())
		{
			bIgnoreGPUTimeout |= sceRazorCpuIsCapturing() == SCE_RAZOR_CPU_CAPTURING;
			bIgnoreGPUTimeout |= Gnm::isCaptureInProgress();
		}

		GPUTimeoutCounter++;
		if (GPUTimeoutCounter > 5000000 && bIgnoreGPUTimeout == false)
		{
#if USE_GPU_OVERWRITE_CHECKING
			// look to see if any overwrites caused this
			extern void CheckForGPUOverwrites();
			CheckForGPUOverwrites();
#endif

#if PS4_RECORD_SUBMITS
			DumpPM4Internal(PreviousContextIndex);			
#endif

			Gnm::debugHardwareStatus(Gnm::kHardwareStatusDump);  
			checkf( false, TEXT("The GPU has crashed!") );
		}
#endif
	}	
}

/**
 * Submit the current frame to the GPU
 */
void FGnmManager::SubmitFrame(bool bVsync, bool bPresent, bool bAuxFrame)
{	
	GnmContextType& Context = ImmediateContext->GetContext();

	//Async compute can't cross frame boundaries yet.
	ImmediateComputeContext->RHISubmitCommandsHint();

	// Timestamp the end of the current frame
	Context.writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)EndOfFrameTimestampLabel[CurrentContextIndex], Gnm::kCacheActionNone);

	// Mark we are done with the current frame, the GPU will set the label to FrameCount when it's done	
	Context.writeAtEndOfPipeWithInterrupt(Gnm::kEopFlushCbDbCaches,
		Gnm::kEventWriteDestMemory, (void*)EndOfFrameLabels[CurrentContextIndex],
		Gnm::kEventWriteSource64BitsImmediate, FrameCounter,
		Gnm::kCacheActionNone, Gnm::kCachePolicyLru);

	GPUProfilingData.EndFrameBeforeSubmit();

	// Make sure there are no outstanding user markers
	ImmediateContext->PopAllMarkers();

#if USE_DEFRAG_ALLOCATOR
	ImmediateContext->PushMarker("GPUMemDefrag");
	ImmediateContext->GetContext().writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)StartOfDefragTimestampLabel[CurrentContextIndex], Gnm::kCacheActionNone);
	FGPUDefragAllocator::FRelocationStats AllocatorStats;
	DefragAllocator.Tick(AllocatorStats, false);
	ImmediateContext->FlushAfterComputeShader();
	ImmediateContext->GetContext().writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)EndOfDefragTimestampLabel[CurrentContextIndex], Gnm::kCacheActionNone);
	ImmediateContext->PopMarker();
#endif

	// Kick the current frame
	//verify(Context->validate() == sce::Gnm::kValidateSuccess);
	//Context->validate();
	
	{		
		if (bPresent || bAuxFrame)
		{
			Context.PrepareFlip();
		}
		AddFinalSubmission(Context.Finalize(ImmediateContext->GetBeginCmdListTimestamp(), ImmediateContext->GetEndCmdListTimestamp()));
		SubmitQueuedDCBs(bVsync, bPresent, bAuxFrame);
	}

	++FrameCounter;
}

void FGnmManager::SubmitQueuedDCBs(bool bVsync, bool bPresent, bool bAuxFrame)
{
	//always submit anything still pending submit in the off-thread queue first.
	//should be very very rare anything is still in here since we're using an event to trigger off-thread submits.
	SubmitBuffersAsyncTaskThreadRunnable->Submit();
	if (bAddedFinalSubmit)
	{
		int32 RetVal;
		if (bPresent)
		{
			UpdateFlipRate(false);

			//
			// Neo doesn't support the bottom flip margin, only the top.
			//
			// ------------------------------------------------------------------------------------
			//                Mode                 |         Description           | Base  |  Neo  
			// ------------------------------------|-------------------------------|-------|-------
			//  - SCE_VIDEO_OUT_FLIP_MODE_HSYNC    |        No Vsync - Tear        |   Y   |   Y   
			//  - SCE_VIDEO_OUT_FLIP_MODE_VSYNC    | Regular vsync without margins |   Y   |   Y   
			//  - SCE_VIDEO_OUT_FLIP_MODE_WINDOW   |  Both top and bottom margins  |   Y   |   -   
			//  - SCE_VIDEO_OUT_FLIP_MODE_WINDOW_2 |      Only the top margin      |   Y   |   Y   
			// ------------------------------------|-------------------------------|-------|-------
			//

			static bool bSupportsBottomMargin = sce::Gnm::getGpuMode() != sce::Gnm::kGpuModeNeo;
			static SceVideoOutFlipMode WindowedVsyncFlipMode = bSupportsBottomMargin
				? SCE_VIDEO_OUT_FLIP_MODE_WINDOW
				: SCE_VIDEO_OUT_FLIP_MODE_WINDOW_2;

			// Use a windowed vsync if we have margins specified, otherwise use a regular vsync.
			bool bWindowedVsync = TopVsyncMargin != 0 || (bSupportsBottomMargin && BottomVsyncMargin != 0);
			SceVideoOutFlipMode VsyncFlipMode = bWindowedVsync
				? WindowedVsyncFlipMode
				: SCE_VIDEO_OUT_FLIP_MODE_VSYNC;

			SceVideoOutFlipMode SelectedFlipMode = bVsync
				? VsyncFlipMode
				: SCE_VIDEO_OUT_FLIP_MODE_HSYNC;

			RetVal = sce::Gnm::submitAndFlipCommandBuffers(
				FinalFrameSubmission.SubmissionAddrs.Num(),
				FinalFrameSubmission.SubmissionAddrs.GetData(), 
				FinalFrameSubmission.SubmissionSizesBytes.GetData(),
				0,
				0,
				VideoOutHandle,
				CurrentBackBuffer,
				SelectedFlipMode,
				GRHIPresentCounter++);
		}
#if HAS_MORPHEUS
		else if (bAuxFrame)
		{
			SCOPED_NAMED_EVENT_TEXT("FGnmManager::SubmitQueuedDCBs() bAuxFrame", FColor::Orange);
			SceVideoOutFlipMode FlipMode = SCE_VIDEO_OUT_FLIP_MODE_VSYNC_MULTI;
			RetVal = sce::Gnm::submitAndFlipCommandBuffers(FinalFrameSubmission.SubmissionAddrs.Num(), FinalFrameSubmission.SubmissionAddrs.GetData(), FinalFrameSubmission.SubmissionSizesBytes.GetData(), 0, 0, AuxVideoOutHandle, CurrentAuxBufferIndex, FlipMode, 0);
		}
#endif
		else
		{
			RetVal = sce::Gnm::submitCommandBuffers(FinalFrameSubmission.SubmissionAddrs.Num(), FinalFrameSubmission.SubmissionAddrs.GetData(), FinalFrameSubmission.SubmissionSizesBytes.GetData(), 0, 0);
		}		

		if (RetVal != SCE_OK)
		{
			UE_LOG(LogPS4, Fatal, TEXT("Final submission failed with: 0x%x, %p"), RetVal, &FinalFrameSubmission);
		}

		if (bPresent)
		{
			SubmitDoneAsyncTaskThreadRunnable->EnableAutoSubmitDone(true);
		}

		bAddedFinalSubmit = false;
	}
}

//namespace 
//{
//	// Potentially useful debug info about reprojection, however cannot call this function in retail build.
//	// I think Sony's VR Trace tool pretty much makes this obsolete.
//	void PrintReprojectionDebugInfo()
//	{
//#if HAS_MORPHEUS
//		static int count = 0;
//		++count;
//		if (count % 1200 > 8)
//			return;
//
//		SceHmdReprojectionDebugInfo info;
//		sceHmdReprojectionDebugGetLastInfo(&info);
//		printf("SceHmdReprojectionDebugInfo:\n");
//		printf(" reprojectionCount      %lu\n", info.reprojectionCount);
//		printf(" renderedTrackerState:\n");
//		printf("   timestamp            %lu\n", info.renderedTrackerState.timestamp);
//		printf("   srstimestamp         %lu\n", info.renderedTrackerState.sensorReadSystemTimestamp);
//		printf("   userFrameNumber      %i\n", info.renderedTrackerState.userFrameNumber);
//		printf(" reprojectionTrackerState0:\n");
//		printf("   timestamp            %lu\n", info.reprojectionTrackerState0.timestamp);
//		printf("   srstimestamp         %lu\n", info.reprojectionTrackerState0.sensorReadSystemTimestamp);
//		printf("   userFrameNumber      %i\n", info.reprojectionTrackerState0.userFrameNumber);
//		printf(" reprojectionTrackerState1:\n");
//		printf("   timestamp            %lu\n", info.reprojectionTrackerState1.timestamp);
//		printf("   srstimestamp         %lu\n", info.reprojectionTrackerState1.sensorReadSystemTimestamp);
//		printf("   userFrameNumber      %i\n", info.reprojectionTrackerState1.userFrameNumber);
//#endif
//	}
//}


/**
 * End the frame, and optionally flip the back buffer
 */
void FGnmManager::EndFrame(bool bPresent, bool bUpdatingCubemap )
{	
#if HAS_MORPHEUS
	bool bSocialScreenOutputModeNotMirroring = false;
	{
		FScopeLock Lock2(&SocialScreenOutputModeMutex);
		bSocialScreenOutputModeNotMirroring = CurrentSocialScreenOutputMode != EPS4SocialScreenOutputMode::Mirroring;
	}
#endif

	// Make sure async task can't call submitDone or submit
	FScopeLock Lock(&SubmissionMutex);

	SCOPED_NAMED_EVENT_TEXT("FGnmManager::EndFrame()", FColor::Turquoise);

	//when reprojection is on, reprojection does the flips.
	bPresent &= CurrentOutputMode == EPS4OutputMode::Standard2D;

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
	check( CVar );
	const bool bUseVSync = CVar->GetValueOnRenderThread() != 0;

	bool bNeedsNativePresent = true;
	if (CustomPresent != nullptr)
	{
		int32 SyncInterval = 0;
		bNeedsNativePresent = CustomPresent->Present(SyncInterval);
	}
	check(bNeedsNativePresent);


	bool bAuxFrame = false;
#if HAS_MORPHEUS
	if (bSocialScreenOutputModeNotMirroring)
	{
		// for aux output. Building command buffer here should not take long, or move it before the userEvent.
		SceVideoOutVblankStatus vblankStatus;
		sceVideoOutGetVblankStatus(AuxVideoOutHandle, &vblankStatus);
		if (vblankStatus.count != AuxLastRenderedVCount)
		{ //assume main loop (60Hz...120Hz) is faster than aux frame (30Hz)
			if (sceVideoOutIsFlipPending(AuxVideoOutHandle) < (NUM_AUX_RENDER_BUFFERS - 1))
			{
				// if any free display buffer to render is available on aux
				bAuxFrame = true;
				AuxLastRenderedVCount = vblankStatus.count;
			}
		}
	}
#endif

	SubmitFrame(bUseVSync, bPresent, bAuxFrame);

	if (CustomPresent != nullptr)
	{
		CustomPresent->PostPresent();
	}

#if !UE_BUILD_SHIPPING
	if (bDumpPM4OnEndFrame)
	{
		DumpPM4Internal(CurrentContextIndex);
		bDumpPM4OnEndFrame = false;
	}
#endif

	//frame is over, re-enable submitdone thread in case the next frame stalls out.
	SubmitDoneAsyncTaskThreadRunnable->EnableAutoSubmitDone(true);

#if HAS_MORPHEUS
	switch (CurrentOutputMode)
	{
	case EPS4OutputMode::Standard2D:
		// do nothing.
		break;
	case EPS4OutputMode::MorpheusRender60Scanout120:
	{
		// In 60/120 mode we want to sync to the SECOND vblank, so we wait twice.
		// The first wait may instantly return, because we have already passed one vblank.
		// If we are missing framerate we may have already passed MORE than 1 vblank.
		// Regardless we then wait again to get vsynced.
		// The end result is that we never run faster than 60fps, and when missing framerate we will wait
		// an additional 8ms as often as necessary to catch up.  The typical missing-by-a-little behavior
		// is therefore to do a run of 16ms frames and then a 24ms frame, repeat.

		SceKernelEvent Event;
		int32 NumEvents;
		{
			SCOPED_NAMED_EVENT_TEXT("FGnmManager::EndFrame wait4vblank 1", FColor::Turquoise);
			//hack to make sure we lock the CPU to 60fps.  Let's us simplify the waituntilsafeforrendering logic for now.
			//if we haven't seen a vblank since last time, wait for the next one.
			sceKernelWaitEqueue(VBlankEventQueue, &Event, 1, &NumEvents, nullptr);
		}

//#if !UE_BUILD_SHIPPING
//			PrintReprojectionDebugInfo(); // TRC violation to call this in a retail build.
//#endif

		{
			SCOPED_NAMED_EVENT_TEXT("FGnmManager::EndFrame wait4vblank 2", FColor::Turquoise);
			const int32 VideoOutCount = sceVideoOutGetEventCount(&Event);
			check(VideoOutCount > 0);
			//if (VideoOutCount >= 1)
			//{
			//	UE_LOG(LogPS4, Warning, TEXT("EndFrame found that %i frames have flip sinced the last EndFrame.  We are probably missing framerate."));
			//}
			sceKernelWaitEqueue(VBlankEventQueue, &Event, 1, &NumEvents, nullptr);
		}
		break;
	}
	case EPS4OutputMode::MorpheusRender90Scanout90:
	case EPS4OutputMode::MorpheusRender120Scanout120:
	{
		SCOPED_NAMED_EVENT_TEXT("FGnmManager::EndFrame wait4vblank", FColor::Turquoise);
		SceKernelEvent Event;
		int32 NumEvents;
		sceKernelWaitEqueue(VBlankEventQueue, &Event, 1, &NumEvents, nullptr);
		break;
	}
	default:
		check(false);
		break;
	}

	HandleMorpheusReprojection();

#endif

	// Calculate how long the GPU frame took, by subtracting timestamps
	// the GPU currently only writes to the lower 32 bits, and the upper 32 are garbage
	int32 PreviousContext = 1 - CurrentContextIndex;
	uint32 Start = *StartOfFrameTimestampLabel[PreviousContext] & 0xFFFFFFFF;
	uint32 End = *EndOfFrameTimestampLabel[PreviousContext] & 0xFFFFFFFF;
	GGPUFrameTime = FPlatformMath::TruncToInt( float( End - Start ) / float( SCE_GNM_GPU_CORE_CLOCK_FREQUENCY ) / float( FPlatformTime::GetSecondsPerCycle() ) );
	SET_CYCLE_COUNTER(STAT_GnmGPUTotalTime, GGPUFrameTime);

	// All the flip handling is done by the GPU.  Subtract off any time the GPU spent waiting for a flip from the GPU time so we get an accurate view of the actual
	// work the GPU is doing.
	if (bUseVSync)
	{
		uint32 FlipDifference = 0;
		for (int32 i = 0; i < FlipMarkersIndex[PreviousContext]; i++)
		{
			FFlipMarker& CurrentMarker = FlipMarkers[PreviousContext][i];

			uint32 FlipWaitStart = *CurrentMarker.StartTimestamp & 0xFFFFFFFF;
			uint32 FlipWaitEnd = *CurrentMarker.EndTimestamp & 0xFFFFFFFF;

			FlipDifference += FlipWaitEnd - FlipWaitStart;

		}

		float FlipWaitSeconds = float(FlipDifference) / float(SCE_GNM_GPU_CORE_CLOCK_FREQUENCY);

		uint32 FlipWaitCycles = FPlatformMath::TruncToInt(FlipWaitSeconds / float(FPlatformTime::GetSecondsPerCycle()));
		GGPUFrameTime -= FlipWaitCycles;		
		SET_CYCLE_COUNTER(STAT_GnmGPUFlipWaitTime, FlipWaitCycles);
	}
	FlipMarkersIndex[PreviousContext] = 0;

#if ENABLE_SUBMITWAIT_TIMING
	// Process the timestamp submission gaps for the previous frame
	// Skip this if we're just updating a cubemap rather than the "real" end frame
	if (!bUpdatingCubemap)
	{
		uint32 TotalSubmitWaitGPUCycles = SubmissionGapRecorder.SubmitSubmissionTimestampsForFrame(FrameCounter, StartOfSubmissionTimestamp[PreviousContext], EndOfSubmissionTimestamp[PreviousContext]);
		double TotalSubmitWaitTimeSeconds = double(TotalSubmitWaitGPUCycles) / float(SCE_GNM_GPU_CORE_CLOCK_FREQUENCY);
		uint32 TotalSubmitWaitCycles = FPlatformMath::TruncToInt(TotalSubmitWaitTimeSeconds / FPlatformTime::GetSecondsPerCycle());
		GGPUFrameTime -= TotalSubmitWaitCycles;
		SET_CYCLE_COUNTER(STAT_GnmGPUSubmitWaitTime, TotalSubmitWaitCycles);
	}
	StartOfSubmissionTimestamp[PreviousContext].Reset();
	EndOfSubmissionTimestamp[PreviousContext].Reset();
#endif
	SET_DWORD_STAT(STAT_GnmNumSubmissions, SubmissionsThisFrame);
	
	Start = *StartOfDefragTimestampLabel[PreviousContext] & 0xFFFFFFFF;
	End = *EndOfDefragTimestampLabel[PreviousContext] & 0xFFFFFFFF;
	float GPUDefragSeconds = float(End - Start) / float(SCE_GNM_GPU_CORE_CLOCK_FREQUENCY);
	uint32 GPUDefragCycles = FPlatformMath::TruncToInt(GPUDefragSeconds / float(FPlatformTime::GetSecondsPerCycle()));
	SET_CYCLE_COUNTER(STAT_GPUDefragTime, GPUDefragCycles);

	// Start the next frame now
	FGnmTempBlockManager::Get().EndFrame();
	ClearFrameTempBuffer();
	CurrentBackBuffer = ( CurrentBackBuffer + 1 ) % NUM_RENDER_BUFFERS;
	CurrentContextIndex = 1 - CurrentContextIndex;
	
#if HAS_MORPHEUS
	if (bAuxFrame)
	{
		CurrentAuxBufferIndex = (CurrentAuxBufferIndex + 1) % NUM_AUX_RENDER_BUFFERS;
		//check(bCurrentFrameIsAuxFrame_RenderThread == false);
		bCurrentFrameIsAuxFrame_RenderThread = true;
		NextAuxBufferIndexForRenderThread = (CurrentAuxBufferIndex + 1) % NUM_AUX_RENDER_BUFFERS;
	}
#endif

#if PS4_RECORD_SUBMITS
	SubmittedCommandsDebug[CurrentContextIndex].Reset();
#endif

	UpdateVsyncMargins();

	InitializeState();
	ResetBoundResources();
	SubmissionsThisFrame = 0;

	// Timestamp start of the frame	
	GnmContextType& Context = ImmediateContext->GetContext();
	Context.writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)StartOfFrameTimestampLabel[CurrentContextIndex], Gnm::kCacheActionNone);


	// Free any pending GPU memory
	FreePendingMemBlocks();

	{
		FScopeLock SubLock(&PS4GPULabelAllocLock);
		// cycle labels over multiple frames to we don't re-issue an address that's in use by the GPU.
		FreeLabelLocations.Append(InFlightLabels[NUM_LABELDELAY_BUFFERS - 1]);
		InFlightLabels[NUM_LABELDELAY_BUFFERS - 1].Reset();
		for (int32 i = (NUM_LABELDELAY_BUFFERS - 2); i >= 0; --i)
		{
			Exchange(InFlightLabels[i + 1], InFlightLabels[i]);
		}
	}

	bIsCurrentlyRendering = false;
}

void FGnmManager::FreePendingMemBlocks()
{
#if USE_NEW_PS4_MEMORY_SYSTEM

	// Free all pending mem blocks whose GPU fence has signaled.
	uint64 FenceToSignal = FMemBlock::ProcessDelayedFrees(*MemBlockFreeLabel);

	// Enqueue the next fence signal on the GPU.
	GnmContextType& Context = ImmediateContext->GetContext();
	Context.writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)MemBlockFreeLabel, FenceToSignal, Gnm::kCacheActionNone);

#else

	FMemBlock::ProcessDelayedFrees();

#endif
}

void FGnmManager::TranslateRGBToYUV(FRHICommandListImmediate& RHICmdList)
{
#if HAS_MORPHEUS
	SCOPED_NAMED_EVENT_TEXT("FGnmManager::TranslateRGBToYUV()", FColor::Magenta);

	bool bIsRenderThread = IsInRenderingThread();
	//UE_LOG(LogPS4, Log, TEXT("TranslateRGBToYUV() getting target with index %i for frame %i"), CurrentAuxBufferIndex, FrameCounter);
	FUnorderedAccessViewRHIRef DstUAV = GetAuxUAVRef(bIsRenderThread);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DstUAV);

	TShaderMapRef< FRGBAToYUV420CS > ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	//TODO gamma handling, analogous to this (from sony sample)
	//sce::Gnm::Texture tex;
	//tex.initFromRenderTarget(&m_stereoRenderTarget[Hmd::kStereoEyeRight].GetRenderTarget(), false);
	//sce::Gnm::DataFormat dFormat = tex.getDataFormat();
	//// We have to use kTextureTypeUNorm for source texture to invalidate degamma. 
	//if (dFormat.getTextureChannelType() != sce::Gnm::kTextureChannelTypeUNorm)
	//{
	//	tex.setDataFormat(sce::Gnm::DataFormat::build(dFormat.getSurfaceFormat(), sce::Gnm::kTextureChannelTypeUNorm, dFormat.getChannel(0), dFormat.getChannel(1), dFormat.getChannel(2), dFormat.getChannel(3)));
	//}
	//auxSrtData->srcTex = tex;

	TRefCountPtr<FRHITexture2D> SrcTex = GetAuxRenderBuffer(bIsRenderThread);
	TRefCountPtr<FRHITexture2D> DstTex = GetAuxOutputBuffer(bIsRenderThread);
	const float TargetHeight = DstTex->GetSizeY() * 2 / 3; // undo the YUV size expansion
	const float TargetWidth = DstTex->GetSizeX();
	const float TextureScale = SrcTex->GetSizeX() / (float)DstTex->GetSizeX();
	const float ScaleFactorX = 1.f * TextureScale / SrcTex->GetSizeX(); // normalized 1 pixel size for x axis
	const float ScaleFactorY = 1.f * TextureScale / SrcTex->GetSizeY(); // normalized 1 pixel size for y axis
	// offset to adjust to src texture center area
	const float TextureYOffset = (float)(SrcTex->GetSizeY() - TargetHeight * TextureScale) / 2.f;
	ComputeShader->SetParameters(RHICmdList, SrcTex, DstUAV, TargetHeight, ScaleFactorX, ScaleFactorY, TextureYOffset);

	const uint32 ThreadGroupCountX = ((uint32)TargetWidth / 2) / 32;	// 32 matches value in .usf
	const uint32 ThreadGroupCountY = ((uint32)TargetHeight / 2) / 2;	// 2 matches value in .usf
	const uint32 ThreadGroupCountZ = 1;									// 1 matches value in .usf
	DispatchComputeShader(RHICmdList, *ComputeShader, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);

	ComputeShader->UnbindBuffers(RHICmdList);

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToGfx, DstUAV);
#endif // HAS_MORPHEUS
}

void FGnmManager::BeginScene()
{
	// Increment the frame counter. INDEX_NONE is a special value meaning "uninitialized", so if
	// we hit it just wrap around to zero.
	SceneFrameCounter++;
	if (SceneFrameCounter == INDEX_NONE)
	{
		SceneFrameCounter++;
	}

	static auto* ResourceTableCachingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("rhi.ResourceTableCaching"));
	if (ResourceTableCachingCvar == NULL || ResourceTableCachingCvar->GetValueOnAnyThread() == 1)
	{
		ResourceTableFrameCounter = SceneFrameCounter;
	}
}

void FGnmManager::EndScene()
{
	ResourceTableFrameCounter = INDEX_NONE;
}

void FGnmManager::SubmitDone(bool bForce, bool bEnableAutoSubmitDone)
{	
	LLM_SCOPE_PS4(ELLMTagPS4::GnmMisc);

	FScopeLock Lock(&SubmissionMutex);
	if (LastSubmitDoneFrame != FrameCounter - 1 || bForce)
	{
		Gnm::submitDone();
		LastSubmitDoneFrame = FrameCounter - 1;
		SubmitDoneAsyncTaskThreadRunnable->UpdateSubmitDoneTime();		
	}

	//always honor the autoenable setting.
	SubmitDoneAsyncTaskThreadRunnable->EnableAutoSubmitDone(bEnableAutoSubmitDone);
}


void FGnmManager::WaitForGPUIdleNoReset()
{	
	check(IsInRenderingThread() || IsInRHIThread());

	// Make sure async task can't call submitDone or submit while in this function.
	FScopeLock Lock(&SubmissionMutex);
	
	FGnmCommandListContext& ImmediateContextRef = GetImmediateContext();
	GnmContextType& Context = ImmediateContextRef.GetContext();
	volatile uint64* Label = (volatile uint64*)Context.allocateFromCommandBuffer(sizeof(uint64), Gnm::kEmbeddedDataAlignment8);
	*Label = 0;
	Context.writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)Label, 1, Gnm::kCacheActionWriteBackAndInvalidateL1andL2);
	AddFinalSubmission(Context.Finalize(ImmediateContextRef.GetBeginCmdListTimestamp(), ImmediateContextRef.GetEndCmdListTimestamp()));
	SubmitQueuedDCBs(false, false, false);

	volatile uint32_t wait = 0;
	while (*Label != 1)
	{
		wait++;
	}

	ImmediateContextRef.InitContextBuffers(true, true);
	ImmediateContextRef.AllocateGlobalResourceTable();	// We are overwriting previously allocated command buffers so need to realloc the GRT

	//it isn't safe to reset the resource buffer midframe.  Some objects with 1-frame lifetime are allocated from here and must persist across
	//the submit boundary because they may be reused by subsequent commands.
	//ResetCurrentBuffer();
}

void FGnmManager::WaitForGPUIdleReset()
{
	check(IsInRenderingThread() || IsInRHIThread());

	// Make sure async task can't call submitDone or submit while in this function.
	FScopeLock Lock(&SubmissionMutex);
	
	FGnmCommandListContext& ImmediateContextRef = GetImmediateContext();
	GnmContextType& Context = ImmediateContextRef.GetContext();
	volatile uint64* Label = (volatile uint64*)Context.allocateFromCommandBuffer(sizeof(uint64), Gnm::kEmbeddedDataAlignment8);
	*Label = 0;
	Context.writeImmediateAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)Label, 1, Gnm::kCacheActionWriteBackAndInvalidateL1andL2);	
	AddFinalSubmission(Context.Finalize(ImmediateContextRef.GetBeginCmdListTimestamp(), ImmediateContextRef.GetEndCmdListTimestamp()));
	SubmitQueuedDCBs(false, false, false);
	
	volatile uint32_t wait = 0;
	while (*Label != 1)
	{
		wait++;
	}	

	ClearFrameTempBuffer();

	//full reset to re-enable autosubmit.  Fixes TRC errors during loadmap which flushes resources and calls this function.
	ImmediateContextRef.InitContextBuffers(true);
	ImmediateContextRef.InitializeStateForFrameStart();
	ImmediateContextRef.ClearState();	
	SubmitDoneAsyncTaskThreadRunnable->EnableAutoSubmitDone(true);

	// Go ahead and call submit done
#if PS4_GNM_SLOW_FRAME_DEBUGGING
	UE_LOG(LogPS4, Log, TEXT("Flushing and calling submit done"))
#endif
	SubmitDone(/*bForce=*/ true, /*bEnableAutoSubmitDone=*/ true);
}


// remember to adjust this 	static const int32 NumBufferedFrames = 4; we need 4 when everyone can do temp allocs, 3 otherwise
#define ENABLE_ALL_TEMP_ALLOCS (1)   

int32 FGnmManager::TempAllocIndex()
{
#if ENABLE_ALL_TEMP_ALLOCS
	// parallel translate tasks are known to be synchronized to the render thread. This might be relaxed in the future and so we will need to know the frame.
	return IsInRHIThread() ? RHIThreadTempBufferIndex : RenderThreadTempBufferIndex;
#else
	return 0;
#endif
}

/**
 * Retrieve memory from the ring buffer for dynamic draw data
 *
 * @param Size Size in bytes of data we need
 *
 * @return Pointer to start of data (aligned to 16 bytes)
 */
FCriticalSection AllocateFromTempRingBufferLock[NUM_TEMP_RING_BUFFERS];
void* FGnmManager::AllocateFromTempRingBuffer(uint32 Size, ETempRingBufferAllocateMode Mode)
{
	// align our size request 
	Size = Align(Size, DEFAULT_VIDEO_ALIGNMENT);

	int32 Index = TempAllocIndex();
	uint32 ChunkSize = FrameTempAllocator[Index].GetChunkSize();

	if (Size > ChunkSize)
	{
		switch (Mode)
		{
		default:
			UE_LOG(LogPS4, Fatal, TEXT("Cannot allocate %d bytes in temp ring buffer. Max size allowed is %d."), Size, ChunkSize);
			// Fall through

		case ETempRingBufferAllocateMode::AllowFailure:
			return nullptr;
		}
	}

	void* Buffer;
	{
		FScopeLock Lock(&AllocateFromTempRingBufferLock[Index]);
		Buffer = FrameTempAllocator[Index].Allocate(Size);
	}
	check(Buffer);

	return Buffer;
}

uint64* FGnmManager::AllocateGPULabelLocation()
{
	LLM_SCOPE_PS4(ELLMTagPS4::GnmMisc);

	FScopeLock Lock(&PS4GPULabelAllocLock);

	uint64* LabelLoc = nullptr;
	if (FreeLabelLocations.Num() > 0)
	{
		LabelLoc = FreeLabelLocations[0];
		FreeLabelLocations.RemoveAtSwap(0, 1, false);
	}
	else
	{
		//allocate in blocks to reduce allocation pool overhead.
		static const int32 LabelBlockSize = 1024;
		static const int32 LabelsPerBlock = 1024 / sizeof(uint64);
		
		FMemBlock LabelAlloc = FMemBlock::Allocate(LabelBlockSize, sizeof(uint64), EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_Label));
		BaseLabelAllocs.Add(LabelAlloc);

		uint64* CurrentLabel = (uint64*)LabelAlloc.GetPointer();
		LabelLoc = CurrentLabel++;

		for (int32 i = 1; i < LabelsPerBlock; ++i, ++CurrentLabel)
		{
			FreeLabelLocations.Add(CurrentLabel);
		}
	}

	check(LabelLoc);
	return LabelLoc;
}

void FGnmManager::FreeGPULabelLocation(uint64* LabelLoc)
{
	FScopeLock Lock(&PS4GPULabelAllocLock);

#if defined(VALIDATE_LABELS) && VALIDATE_LABELS
	for (int32 i = 0; i < BaseLabelAllocs.Num(); ++i)
	{
		check(LabelLoc >= BaseLabelAllocs[i].GetPointer() && LabelLoc < BaseLabelAllocs[i].GetPointer() + BaseLabelAllocs[i].GetSize());
	}
#endif
	InFlightLabels[0].Add(LabelLoc);
}

bool FGnmManager::SafeToAllocateFromTempRingBuffer()
{
#if !ENABLE_ALL_TEMP_ALLOCS
	//the ring buffer is flipped by EndFrame, so it's only safe to allocate from here on the thread that executes RHI Commands.
	//however if a globalflush was just performed then it's safe on the RenderThread as any EndFrame commands have been flushed and the RHIThread is empty.
#if PS4_SUPPORTS_PARALLEL_RHI_EXECUTE
	return IsRunningRHIInSeparateThread() ? (IsInRHIThread() || GGnmManager.IsInParallelTranslateThread()	|| (IsInRenderingThread() && FRHICommandListExecutor::IsRHIThreadCompletelyFlushed())) : IsInRenderingThread();
#else	
	return IsRunningRHIInSeparateThread() ? (IsInRHIThread() ||										(IsInRenderingThread() && FRHICommandListExecutor::IsRHIThreadCompletelyFlushed())) : IsInRenderingThread();
#endif
#else
	return true;
#endif
}

void FGnmManager::ResetFrameTempBuffer()
{
	for (int32 Index = 0; Index < NUM_TEMP_RING_BUFFERS; Index++)
	{
		FrameTempAllocator[Index].Reset();
	}
	RenderThreadTempBufferIndex = 0;
	RHIThreadTempBufferIndex = 0;
}

void FGnmManager::ClearFrameTempBuffer()
{
	FrameTempAllocator[RHIThreadTempBufferIndex].Clear();
}

void FGnmManager::StartRHIThreadTempBufferFrame(int32 FrameIndex)
{
#if ENABLE_ALL_TEMP_ALLOCS
	RHIThreadTempBufferIndex = FrameIndex;
	check(RHIThreadTempBufferIndex >= 0 && RHIThreadTempBufferIndex < NUM_TEMP_RING_BUFFERS);
#endif
}

struct FRHICommandStartRHIThreadTempBufferFrame final : public FRHICommand<FRHICommandStartRHIThreadTempBufferFrame>
{
	int32 FrameIndex;

	FORCEINLINE_DEBUGGABLE FRHICommandStartRHIThreadTempBufferFrame(int32 InFrameIndex)
		: FrameIndex(InFrameIndex)
	{
		check(FrameIndex >= 0 && FrameIndex < NUM_TEMP_RING_BUFFERS);
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		GGnmManager.StartRHIThreadTempBufferFrame(FrameIndex);
	}
};

void FGnmManager::StartRenderThreadTempBufferFrame()
{
#if ENABLE_ALL_TEMP_ALLOCS
	RenderThreadTempBufferIndex++;
	if (RenderThreadTempBufferIndex >= NUM_TEMP_RING_BUFFERS)
	{
		RenderThreadTempBufferIndex = 0;
	}
	check(RenderThreadTempBufferIndex >= 0 && RenderThreadTempBufferIndex < NUM_TEMP_RING_BUFFERS);
	if (!FRHICommandListExecutor::GetImmediateCommandList().Bypass())
	{
		check(IsInRenderingThread());
		new (FRHICommandListExecutor::GetImmediateCommandList().AllocCommand<FRHICommandStartRHIThreadTempBufferFrame>()) FRHICommandStartRHIThreadTempBufferFrame(RenderThreadTempBufferIndex);
	}
	else
	{
		// There is no concurrency, we can just set it.
		RHIThreadTempBufferIndex = RenderThreadTempBufferIndex;
		check(RHIThreadTempBufferIndex >= 0 && RHIThreadTempBufferIndex < NUM_TEMP_RING_BUFFERS);
	}
#endif
}

/**
 * Update the maximum size for a shader scratch buffer
 */
void FGnmManager::UpdateShaderScratchBufferMaxSize( EScratchBufferResourceType ScratchBufferType, uint32 ScratchSizeInDWPerThread )
{
	uint32 Num1KbyteChunksPerWave = (ScratchSizeInDWPerThread + 3) / 4; 
	uint32 RequestedScratchBufferSize = ScratchBufferMaxNumWaves * Num1KbyteChunksPerWave * 1024; 

	FShaderScratchBuffer& ShaderScratchBuffer = (ScratchBufferType == SB_GRAPHIC_RESOURCE) ? GraphicShaderScratchBuffer : ComputeShaderScratchBuffer ;

	if( RequestedScratchBufferSize > ShaderScratchBuffer.GetScratchBufferSize() )
	{
		checkf(false, TEXT("ShaderSize resize not implemented for parallel rhi execute.  Desired scratch: %i"), RequestedScratchBufferSize);

#if 0
		// The 2 contexts share the same scratch buffer so we need to wait until the GPU
		// isn't using it before we can resize
		GGnmManager.WaitForGPUIdleNoReset();

		ShaderScratchBuffer.ResizeScratchBuffer( RequestedScratchBufferSize );

		// Update the scratch buffer in the context
 		ShaderScratchBuffer.Commit( *Context );
#endif
	}
}

void FGnmManager::WaitForBackBufferSafety(GnmContextType& GnmContext)
{
	GnmContext.pushMarker("WaitForBackBufferSafe");
	//cause the GPU to stall until it can render into the new flip target without visible tearing.		
	// time the stall so we can show the actual GPU busy time without the stall time.
	FFlipMarker CurrentMarker;
	int32 CurrentIndex = FlipMarkersIndex[CurrentContextIndex];
	TArray<FFlipMarker>& CurrentArray = FlipMarkers[CurrentContextIndex];

	if (CurrentIndex == CurrentArray.Num())
	{
		CurrentMarker.Initialize();
		CurrentArray.Add(CurrentMarker);
	}
	else
	{
		CurrentMarker = CurrentArray[CurrentIndex];
	}
	
	GnmContext.writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)CurrentMarker.StartTimestamp, Gnm::kCacheActionNone);
	if (CurrentOutputMode != EPS4OutputMode::Standard2D)
	{
		//GnmContext.waitUntilSafeForRendering(VideoOutHandle, CurrentBackBuffer);
	}
	else
	{
		GnmContext.waitUntilSafeForRendering(VideoOutHandle, CurrentBackBuffer);
	}	
	
	GnmContext.writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, (void*)CurrentMarker.EndTimestamp, Gnm::kCacheActionNone);
	
	FlipMarkersIndex[CurrentContextIndex]++;
	
	GnmContext.popMarker();
}

void FGnmManager::FFlipMarker::Initialize()
{
	StartTimestamp = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
	EndTimestamp = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
}

void FGnmManager::FShaderScratchBuffer::ResizeScratchBuffer( uint32 RequestedScratchBufferSize )
{
	LLM_SCOPE_PS4(ELLMTagPS4::GnmMisc);

	// Reallocate buffer
	FMemBlock::Free(ScratchMem);
	ScratchMem = FMemBlock::Allocate(RequestedScratchBufferSize, Gnm::kAlignmentOfBufferInBytes, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_ShaderHelperMem));
	ScratchBuffer.initAsScratchRingBuffer(ScratchMem.GetPointer(), RequestedScratchBufferSize);
}

void FGnmManager::FShaderScratchBuffer::Commit( GnmContextType& Context )
{
	Context.m_lwcue.setGlobalDescriptor( ResourceType, &ScratchBuffer );
}


void FGnmManager::Advance_CurrentBackBuffer_RenderThread()
{
#if HAS_MORPHEUS
	if (bCurrentFrameIsAuxFrame_RenderThread)
	{
		bCurrentFrameIsAuxFrame_RenderThread = false;
		CurrentAuxBufferIndex_RenderThread = NextAuxBufferIndexForRenderThread;
	}
#endif

	CurrentBackBuffer_RenderThread = (CurrentBackBuffer_RenderThread + 1) % NUM_RENDER_BUFFERS;
	++FrameCounterRenderThread;
	StartRenderThreadTempBufferFrame();
#if ENABLE_SUBMITWAIT_TIMING
	SubmissionGapRecorder.OnRenderThreadAdvanceFrame();
#endif
}



#if ENABLE_SUBMITWAIT_TIMING

/** FGnmSubmissionGapRecorder class */
FGnmSubmissionGapRecorder::FGnmSubmissionGapRecorder()
	: WriteIndex(0)
	, WriteIndexRT(0)
	, ReadIndex(0)
	, CurrentGapSpanReadIndex(0)
	, CurrentElapsedWaitCycles(0)
	, LastTimestampAdjusted(0xFFFFFFFF)
{
	// Add 8 frames to the ringbuffer. This gives a reasonable amount of history
	// for buffered queries when we want to read the results back later
	for (int i = 0; i < 8; i++)
	{
		FrameRingbuffer.Add(FGnmSubmissionGapRecorder::FFrame());
	}
}

uint32 FGnmSubmissionGapRecorder::SubmitSubmissionTimestampsForFrame(uint32 FrameCounter, const TArray<uint64*>& PrevFrameBeginSubmissionTimestamps, const TArray<uint64*>& PrevFrameEndSubmissionTimestamps)
{
	// NB: The frame number for the previous frame is actually FrameCounter-2, because we've already incremented FrameCounter at this point
	uint32 FrameNumber = FrameCounter - 2;

	ensureMsgf(PrevFrameBeginSubmissionTimestamps.Num() == PrevFrameEndSubmissionTimestamps.Num(), TEXT("Start/End Submission timestamps don't match. %i, %i"), PrevFrameBeginSubmissionTimestamps.Num(), PrevFrameEndSubmissionTimestamps.Num());

	FGnmSubmissionGapRecorder::FFrame& Frame = FrameRingbuffer[WriteIndex];

	// It seems GapSpans can be modified on both the render thread and RHI thread, so we need a critical section
	FScopeLock ScopeLock(&GapSpanMutex);

	Frame.GapSpans.Empty();
	Frame.FrameNumber = FrameNumber;

	uint32 TotalWaitCycles = 0;
	bool bValid = true;

	// Do some rudimentary checks. Note: the first 2 frames are always invalid, because we don't have any data yet
	if (PrevFrameBeginSubmissionTimestamps.Num() != PrevFrameEndSubmissionTimestamps.Num() || FrameCounter < 2 )
	{
		bValid = false;
	}
	else
	{
		// Store the timestamp values
		for (int i = 0; i < PrevFrameBeginSubmissionTimestamps.Num() - 1; i++)
		{
			FGapSpan GapSpan;

			uint64* BeginTimestampPtr = PrevFrameEndSubmissionTimestamps[i];
			uint64* EndTimestampPtr = PrevFrameBeginSubmissionTimestamps[i + 1];

			// If one of the pointers isn't valid, just give up
			if (BeginTimestampPtr == nullptr || EndTimestampPtr == nullptr)
			{
				bValid = false;
				break;
			}

			GapSpan.BeginCycles = *BeginTimestampPtr & 0xFFFFFFFFull;
			uint32 EndCycles = *EndTimestampPtr & 0xFFFFFFFFull;

			// Check begin/end is contiguous
			if (EndCycles < GapSpan.BeginCycles)
			{
				bValid = false;
				break;
			}
			GapSpan.DurationCycles = EndCycles - GapSpan.BeginCycles;

			// Check gap spans are contiguous (TODO: we might want to modify this to support async compute submissions which overlap)
			if ( i > 0 )
			{
				const FGapSpan& PrevGap = Frame.GapSpans[i - 1];
				uint32 PrevGapEndCycles = PrevGap.BeginCycles + PrevGap.DurationCycles;
				if (GapSpan.BeginCycles < PrevGapEndCycles)
				{
					bValid = false;
					break;
				}
			}

			TotalWaitCycles += GapSpan.DurationCycles;

			Frame.GapSpans.Add(GapSpan);
		}
	}

	if (!bValid)
	{
		// If the frame isn't valid, just clear it
		Frame.GapSpans.Empty();
		TotalWaitCycles = 0;
	}

	Frame.TotalWaitCycles = TotalWaitCycles;
	WriteIndex = (WriteIndex + 1) % FrameRingbuffer.Num();

	// Keep track of the begin/end span for the frame (mostly for debugging at this point)
	Frame.EndCycles = 0;
	Frame.StartCycles = 0;
	if (Frame.GapSpans.Num() > 0)
	{
		Frame.StartCycles = Frame.GapSpans[0].BeginCycles;

		const FGapSpan& LastSpan = Frame.GapSpans.Last();
		Frame.EndCycles = LastSpan.BeginCycles + LastSpan.DurationCycles;
	}
	Frame.bIsValid = bValid;
	return TotalWaitCycles;
}


// Called at the end of the RenderThread frame, before the current RHIThread hits EndFrame() and the next gap is submitted
void FGnmSubmissionGapRecorder::OnRenderThreadAdvanceFrame()
{
	check(IsInRenderingThread());
	for (int i = 0; i < FrameRingbuffer.Num(); i++)
	{
		FrameRingbuffer[i].bSafeToReadOnRenderThread = true;
	}

	WriteIndexRT = (WriteIndexRT + 1) % FrameRingbuffer.Num();

#if DO_CHECK
	// Check the write indices don't drift. Shouldn't be possible, but just in case... 
	{
		int Diff = FMath::Abs((int)WriteIndexRT - (int)WriteIndex);
		ensure(Diff <= 1 || Diff == FrameRingbuffer.Num() - 1);
	}
#endif

	// If we have an RHIThread, the frame at WriteIndex is about to be written, so mark it as not safe to read. 
	if (IsRunningRHIInSeparateThread())
	{
		FrameRingbuffer[WriteIndexRT].bSafeToReadOnRenderThread = false;
	}
}

uint64 FGnmSubmissionGapRecorder::AdjustTimestampForSubmissionGaps(uint32 FrameSubmitted, uint64 Timestamp)
{
	// Note: this function looks heavy, but in most cases it should be efficient, as it takes advantage of wait times computed on previous calls.
	// Large numbers of timestamps requested out of order may be slower

	// It seems GapSpans can be modified on both the render thread and RHI thread, so we need a critical section
	FScopeLock ScopeLock(&GapSpanMutex);

	// Get the current frame (in most cases we'll just skip over this)
	if (FrameRingbuffer[ReadIndex].FrameNumber != FrameSubmitted)
	{
		// This isn't the right frame, so try to find it
		bool bFound = false;
		for (int i = 0; i < FrameRingbuffer.Num() - 1; i++)
		{
			ReadIndex = (ReadIndex + 1) % FrameRingbuffer.Num();
			if (FrameRingbuffer[ReadIndex].FrameNumber == FrameSubmitted)
			{
				LastTimestampAdjusted = 0xFFFFFFFF;
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			// The frame wasn't found, so don't adjust the timestamp
			return Timestamp;
		}
	}

	FFrame& CurrentFrame = FrameRingbuffer[ReadIndex];
	bool bValid = CurrentFrame.bSafeToReadOnRenderThread && CurrentFrame.bIsValid;
	if ( !bValid )
	{
		// If the frame isn't valid, don't adjust the timestamp
		return Timestamp;
	}

	// If the timestamps are read back out-of-order (or this is the first frame), we need to start from the beginning
	if (Timestamp < LastTimestampAdjusted)
	{
		CurrentGapSpanReadIndex = 0;
		CurrentElapsedWaitCycles = 0;
	}
	LastTimestampAdjusted = Timestamp;

	// Find all gaps before this timestamp and add up the time (this continues where we left off last time if possible)
	for ( ;CurrentGapSpanReadIndex < CurrentFrame.GapSpans.Num(); CurrentGapSpanReadIndex++)
	{
		const FGapSpan& GapSpan = CurrentFrame.GapSpans[CurrentGapSpanReadIndex];
		if (GapSpan.BeginCycles >= Timestamp)
		{
			// The next gap begins before this timestamp happened, so we're done
			break;
		}
		CurrentElapsedWaitCycles += GapSpan.DurationCycles;
	}

	if (Timestamp < CurrentElapsedWaitCycles)
	{
		// Something went wrong. Likely a result of 32-bit uint overflow. Don't adjust
		return Timestamp;
	}
	return Timestamp - CurrentElapsedWaitCycles;
}

#endif // ENABLE_SUBMITWAIT_TIMING