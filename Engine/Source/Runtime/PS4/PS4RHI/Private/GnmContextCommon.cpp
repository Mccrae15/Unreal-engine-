// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GnmContextCommon.cpp: Functions common between gfx and compute contexts
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "GnmContextCommon.h"
#include "ShaderParameterUtils.h"

FGnmComputeFence::FGnmComputeFence(FName InName, uint64* InLabelLoc)
	: FRHIComputeFence(InName)
	, LabelLoc(InLabelLoc)
{
	check(LabelLoc);
	*LabelLoc = 0;
}

FGnmComputeFence::~FGnmComputeFence()
{
	GGnmManager.FreeGPULabelLocation(LabelLoc);
	LabelLoc = nullptr;
}

void FGnmComputeFence::WriteFenceGPU(FGnmComputeCommandListContext& ComputeContext, bool bDoFlush)
{
	FRHIComputeFence::WriteFence();

	Gnm::CacheAction CacheAction = bDoFlush ? Gnm::kCacheActionWriteBackAndInvalidateL1andL2 : Gnm::kCacheActionNone;
	ComputeContext.GetContext().writeReleaseMemEvent(Gnm::kReleaseMemEventCsDone, Gnm::kEventWriteDestMemory, LabelLoc,
		Gnm::kEventWriteSource32BitsImmediate, 0x1, CacheAction, Gnm::kCachePolicyLru);
}

void FGnmComputeFence::WriteFenceGPU(FGnmCommandListContext& CommandContext, bool bDoFlush)
{
	FRHIComputeFence::WriteFence();

	Gnm::CacheAction CacheAction = bDoFlush ? Gnm::kCacheActionWriteBackAndInvalidateL1andL2 : Gnm::kCacheActionNone;

	// NOTE: kEopCbDbReadsDone and kEopCsDone are two names for the same value, so this EOP event does cover both graphics and compute
	// use cases.
	CommandContext.GetContext().writeAtEndOfPipe(Gnm::kEopCbDbReadsDone, Gnm::kEventWriteDestMemory, LabelLoc,
		Gnm::kEventWriteSource32BitsImmediate, 0x1, CacheAction, Gnm::kCachePolicyLru);
}

void FGnmComputeFence::WaitFence(FGnmCommandListContext& CommandContext)
{
	checkf(GetWriteEnqueued(), TEXT("Waiting on fence that will not be written in time.  GPU hang incoming.  %s"), *GetName().ToString());
	CommandContext.GetContext().waitOnAddress(LabelLoc, 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
}

void FGnmComputeFence::WaitFence(FGnmComputeCommandListContext& ComputeContext)
{
	checkf(GetWriteEnqueued(), TEXT("Waiting on fence that will not be written in time.  GPU hang incoming.  %s"), *GetName().ToString());
	ComputeContext.GetContext().waitOnAddress(LabelLoc, 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
}


bool FGnmGPUFence::Write()
{
	
	//WriteInternal();
	return true;
}

void FGnmGPUFence::WriteInternal(FGnmCommandListContext& CommandContext)
{
	// add an event queue so we can grab the value of the label
	sceKernelCreateEqueue(&FenceEQ, "sampleEQ");
	Gnm::addEqEvent(FenceEQ, sce::Gnm::kEqEventGfxEop, nullptr);

	// write the label
	Gnm::CacheAction CacheAction = Gnm::kCacheActionNone;
	uint64* LabelLoc = (uint64*)GGnmManager.AllocateGPULabelLocation();
	CommandContext.GetContext().writeAtEndOfPipeWithInterrupt(Gnm::kEopCsDone, Gnm::kEventWriteDestMemory, LabelLoc,
		Gnm::kEventWriteSource32BitsImmediate, 0x1, Gnm::kCacheActionNone, Gnm::kCachePolicyBypass);
}

bool FGnmGPUFence::Poll() const
{
	SceKernelEvent ev;
	int out;
	SceKernelUseconds Timeout = 0;
	sceKernelWaitEqueue(FenceEQ, &ev, 1, &out, &Timeout);

	return out == 1;
}

bool FGnmGPUFence::Wait(float TimeoutMs) const
{
	SceKernelEvent ev;
	int out;
	SceKernelUseconds Timeout = TimeoutMs*1000;
	sceKernelWaitEqueue(FenceEQ, &ev, 1, &out, &Timeout);

	return out == 1;
}


bool FGnmContextCommon::ValidateSRVForSet(FGnmShaderResourceView* SRV)
{
	if (!IsRunningRHIInSeparateThread())
	{
		const EResourceTransitionAccess CurrentAccess = SRV->GetResourceAccess();
		const bool bAccessPass = CurrentAccess == EResourceTransitionAccess::EReadable || (CurrentAccess == EResourceTransitionAccess::ERWBarrier && !SRV->IsResourceDirty()) || CurrentAccess == EResourceTransitionAccess::ERWSubResBarrier;
		if (!(bAccessPass || SRV->GetLastFrameWritten() != GGnmManager.GetFrameCount()))
		{
			return false;
		}
	}
	return true;
}
