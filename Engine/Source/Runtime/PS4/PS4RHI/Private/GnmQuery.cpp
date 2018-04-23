// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmQuery.cpp: Gnm query RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "PS4/PS4LLM.h"


#if ENABLE_SUBMITWAIT_TIMING

static int GAdjustRenderQueryTimestamps = 1;
static FAutoConsoleVariableRef CVarAdjustRenderQueryTimestamps(
	TEXT("r.ps4.AdjustRenderQueryTimestamps"),
	GAdjustRenderQueryTimestamps,
	TEXT("If true, this adjusts render query timings to remove gaps between command list submissions\n")
	TEXT("Time elapsed during preceeding bubbles (since the frame start) is subtraced from the timestamp for a given query") );

#endif

/**
 * Constructor
 */
FGnmRenderQuery::FGnmRenderQuery(ERenderQueryType InQueryType)
	: OcclusionQueryResults(NULL)
	, TimeStampResults(NULL)
	, bResultIsCached(false)
	, QueryType(InQueryType)
{
	LLM_SCOPE_PS4(ELLMTagPS4::GnmMisc);

	// using SystemShared sped up the rendering thread by a decent amount!

	if (QueryType == RQT_Occlusion)
	{
		ResultsMemory = FMemBlock::Allocate(sizeof(Gnm::OcclusionQueryResults), QUERY_VIDEO_ALIGNMENT, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_RenderQuery));
		OcclusionQueryResults = (Gnm::OcclusionQueryResults*)ResultsMemory.GetPointer();
	}
	else if (QueryType == RQT_AbsoluteTime)
	{
		ResultsMemory = FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_RenderQuery));
		TimeStampResults = (uint64*)ResultsMemory.GetPointer();
	}
	else
	{
		check(0);
	}

	FrameSubmitted = -1;
	bSubmitted = false;
}

FGnmRenderQuery::~FGnmRenderQuery()
{
	FMemBlock::Free(ResultsMemory);
}

/**
 * Kick off an occlusion/timer test 
 */
void FGnmRenderQuery::Begin(FGnmCommandListContext& GnmCommandContext)
{
	if (QueryType == RQT_Occlusion)
	{
		// clear the results before beginning query to avoid race condition - https://ps4.scedev.net/forums/thread/11692/
		OcclusionQueryResults->reset();

		// tell GPU to start writing ZPass numbers to the query object
		GnmCommandContext.GetContext().writeOcclusionQuery(Gnm::kOcclusionQueryOpBeginWithoutClear, OcclusionQueryResults);

		// start counting, with exact zpass counting
		GnmCommandContext.GetContext().setDbCountControl(Gnm::kDbCountControlPerfectZPassCountsEnable, 0);

		bSubmitted = true;
		FrameSubmitted = GGnmManager.GetFrameCount();
	}
	else
	{
		// not supported/needed for RQT_AbsoluteTime
		check(0);
	}
}

/**
 * Finish up an occlusion/timer test 
 */
void FGnmRenderQuery::End(FGnmCommandListContext& GnmCommandContext)
{
	if (QueryType == RQT_Occlusion)
	{
		// tell GPU to stop writing ZPass numbers to the query object
		GnmCommandContext.GetContext().writeOcclusionQuery(Gnm::kOcclusionQueryOpEnd, OcclusionQueryResults);

		// stop precise counting
		GnmCommandContext.GetContext().setDbCountControl(Gnm::kDbCountControlPerfectZPassCountsDisable, 0);
	}
	else if (QueryType == RQT_AbsoluteTime)
	{
		ensureMsgf(!bSubmitted || bResultIsCached, TEXT("FGnmRenderQuery had End called twice without RHIGetRenderQueryResult after first End, possible race condition"));

		bSubmitted = true;
		FrameSubmitted = GGnmManager.GetFrameCount();

		bResultIsCached = false;
		// Initialize to zero so we can tell when it's complete
		// This will cause a race condition with the GPU if re-issued too soon
		*TimeStampResults = 0;
		GnmCommandContext.GetContext().writeTimestampAtEndOfPipe(Gnm::kEopFlushCbDbCaches, TimeStampResults, Gnm::kCacheActionNone);
	}
	else
	{
		check(0);
	}
}

bool FGnmRenderQuery::BlockUntilComplete(bool& bSuccess)
{
	SCOPE_CYCLE_COUNTER( STAT_RenderQueryResultTime );
	uint32 IdleStart = FPlatformTime::Cycles();
	double StartTime = FPlatformTime::Seconds();

	ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();

	// if we block until it's ready, loop here until it is
	while (!bSuccess)
	{
		FPlatformProcess::SleepNoStats(0);

		// pump RHIThread to make sure these queries have actually been submitted to the GPU.
		if (IsInActualRenderingThread())
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(RenderThread_Local);
		}
		bSuccess = (QueryType == RQT_AbsoluteTime) ? *TimeStampResults != 0 : OcclusionQueryResults->isReady();

		// timer queries are used for Benchmarks which can stall a bit more
		const double TimeoutValue = (QueryType == RQT_AbsoluteTime) ? 2.0 : 0.5;

		// look for gpu stuck/crashed
		if ((FPlatformTime::Seconds() - StartTime) > TimeoutValue)
		{
#if USE_GPU_OVERWRITE_CHECKING
			// look to see if any overwrites caused this
			extern void CheckForGPUOverwrites();
			CheckForGPUOverwrites();
#endif

			UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion/timer results. (%.1f s)"), TimeoutValue);
			return false;
		}
	}

	// track idle time blocking on GPU
	GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
	GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;

	return true;
}




FRenderQueryRHIRef FGnmDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	// timer is not yet supported in this RHI
	check(QueryType == RQT_Occlusion || QueryType == RQT_AbsoluteTime);

	return new FGnmRenderQuery(QueryType);
}


bool FGnmDynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI,uint64& OutNumPixels,bool bWait)
{
	FGnmRenderQuery* Query = ResourceCast(QueryRHI);

	bool bSuccess = true;

	// if there's NO chance of this query ever returning a good result, just bail.  Otherwise screw up will cause
	if (!Query->bSubmitted || (!IsRunningRHIInSeparateThread() && GGnmManager.GetFrameCount() == Query->FrameSubmitted))
	{
		UE_LOG(LogRHI, Log, TEXT("Trying to check occlusion query before it's actually been submitted to the GPU: submitted: %i, Frame: %i, CurrentFrame: %i"), (int)Query->bSubmitted, Query->FrameSubmitted, GGnmManager.GetFrameCount());
		return false;
	}
	
	if (!Query->bResultIsCached)
	{
		if (Query->QueryType == RQT_Occlusion)
		{
			// are results ready
			bSuccess = Query->OcclusionQueryResults->isReady();

			if (!bSuccess && bWait)
			{
				if (!Query->BlockUntilComplete(bSuccess))
				{
					return false;
				}
			}

			// remember if we succeeded in case we ask again
			Query->bResultIsCached = bSuccess;
			Query->Result = bSuccess ? Query->OcclusionQueryResults->getZPassCount() : 0;
		}
		else if (Query->QueryType == RQT_AbsoluteTime)
		{
			bSuccess = (*Query->TimeStampResults) != 0;

			if (!bSuccess && bWait)
			{
				if (!Query->BlockUntilComplete(bSuccess))
				{
					return false;
				}
			}

			Query->bResultIsCached = bSuccess;

			if (bSuccess)
			{
				uint64 GPUTiming = (*Query->TimeStampResults) & 0xFFFFFFFF;

#if ENABLE_SUBMITWAIT_TIMING
				if (GAdjustRenderQueryTimestamps)
				{
					GPUTiming = GGnmManager.SubmissionGapRecorder.AdjustTimestampForSubmissionGaps(Query->FrameSubmitted, GPUTiming);
				}
#endif

				// Convert to microseconds, consistent with D3D11 implementation
				uint64 Div = SCE_GNM_GPU_CORE_CLOCK_FREQUENCY / (1000 * 1000);

				Query->Result = GPUTiming / Div;
			}
			else
			{
				Query->Result = 0;
			}
		}
	}

	// return cached result
	OutNumPixels = Query->Result;

	// return if we managed to get results
	return bSuccess;
}
