// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GnmContextCommon.cpp: Functions common between gfx and compute contexts
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "GnmContextCommon.h"
#include "ShaderParameterUtils.h"
#include "ProfilingDebugging/ScopedTimers.h"

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

SceKernelEqueue FGnmGPUFence::FenceEQ = nullptr;
void FGnmGPUFence::InitFenceQueue()
{
	check(FenceEQ == nullptr);
	int32 RetVal = sceKernelCreateEqueue(&FenceEQ, "sampleEQ");
	checkf(RetVal == SCE_OK, TEXT("GPUFence sceKernelCreateEqueue failed: 0x%x"), RetVal);
}

FGnmGPUFence::~FGnmGPUFence()
{
	if (LabelLoc)
	{
		GGnmManager.FreeGPULabelLocation(LabelLoc);
		LabelLoc = nullptr;
	}
}

void FGnmGPUFence::WriteInternal(FGnmComputeCommandListContext& CommandContext)
{
	// Correct implementation is not obvious - needs some expert care!
	check(false);

	// // add an event queue so we can grab the value of the label
	// sceKernelCreateEqueue(&FenceEQ, "sampleEQ");
	// Gnm::addEqEvent(FenceEQ, sce::Gnm::kEqEventCsEop, nullptr);

	// // write the label
	// Gnm::CacheAction CacheAction = Gnm::kCacheActionNone;
	// uint64* LabelLoc = (uint64*)GGnmManager.AllocateGPULabelLocation();
	// CommandContext.GetContext().writeAtEndOfPipeWithInterrupt(Gnm::kEopCsDone, Gnm::kEventWriteDestMemory, LabelLoc,
	// 	Gnm::kEventWriteSource32BitsImmediate, 0x1, Gnm::kCacheActionNone, Gnm::kCachePolicyBypass);
}

void FGnmGPUFence::WriteInternal(FGnmCommandListContext& CommandContext)
{
	check(FenceEQ);
	check(IsInRHIThread() || IsInRenderingThread() || !GIsThreadedRendering);

	Gnm::addEqEvent(FenceEQ, sce::Gnm::kEqEventGfxEop, nullptr);

	// write the label
	Gnm::CacheAction CacheAction = Gnm::kCacheActionNone;
	if (!LabelLoc)
	{
		LabelLoc = (uint64*)GGnmManager.AllocateGPULabelLocation();
	}
	*LabelLoc = 0;
	//#todo-mw: Use an incrementing counter instead of 1 to check if the GPU is actually done and we didn't re-use the label
	CommandContext.GetContext().writeAtEndOfPipeWithInterrupt(Gnm::kEopCsDone, Gnm::kEventWriteDestMemory, LabelLoc,
		Gnm::kEventWriteSource32BitsImmediate, 0x1, Gnm::kCacheActionNone, Gnm::kCachePolicyBypass);
}

void FGnmGPUFence::Clear()
{
	if (LabelLoc)
	{
		*LabelLoc = 0;
	}
}

bool FGnmGPUFence::Poll() const
{
	if (LabelLoc)
	{
		return (*LabelLoc) == 1;
	}
	return false;
}

FGnmStagingBuffer::~FGnmStagingBuffer()
{
	if (ShadowBuffer.GetPointer() != nullptr)
	{
		FMemBlock::Free(ShadowBuffer);
	}
}

void* FGnmStagingBuffer::Lock(uint32 Offset, uint32 NumBytes)
{
	check(!bIsLocked);
	uint8* Ptr = reinterpret_cast<uint8*>(ShadowBuffer.GetPointer());
	check(Ptr);
	check(uint64(Offset + NumBytes) <= uint64(ShadowBuffer.GetSize()));
	bIsLocked = true;

	return reinterpret_cast<void*>(Ptr + Offset);
}

void FGnmStagingBuffer::Unlock()
{
	check(bIsLocked);
	bIsLocked = false;
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
