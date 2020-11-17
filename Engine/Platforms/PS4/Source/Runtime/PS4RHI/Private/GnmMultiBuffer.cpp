// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmVertexBuffer.cpp: Gnm vertex buffer RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"

FGnmMultiBufferResource::FGnmMultiBufferResource(uint32 Size, uint32 Usage, FRHIResourceCreateInfo& CreateInfo, TStatId StatGPU, TStatId StatCPU)
	: CreatedSize(CreateInfo.bWithoutNativeResource ? 0u : Size)
	, MemoryType((Usage & BUF_KeepCPUAccessible) ? EGnmMemType::GnmMem_CPU : EGnmMemType::GnmMem_GPU)
	, StatId((Usage & BUF_KeepCPUAccessible) ? StatCPU : StatGPU)
	, bLocked(false)
	, bFirstLock(true)
	, LockedMode(EResourceLockMode::RLM_Num)
{
	if (CreatedSize > 0)
	{
		CurrentBufferTopOfPipe = FMemBlock::Allocate(CreatedSize, DEFAULT_VIDEO_ALIGNMENT, MemoryType, StatId);
		CurrentBufferBottomOfPipe = CurrentBufferTopOfPipe;

		if (CreateInfo.ResourceArray)
		{
			check(CreatedSize == CreateInfo.ResourceArray->GetResourceDataSize());
			FMemory::Memcpy(CurrentBufferTopOfPipe.GetPointer(), CreateInfo.ResourceArray->GetResourceData(), CreatedSize);

			// Discard the resource array's contents.
			CreateInfo.ResourceArray->Discard();

			bFirstLock = false;
		}
	}
}

FGnmMultiBufferResource::~FGnmMultiBufferResource()
{
	checkf(!bLocked, TEXT("Cannot destruct a buffer which is still locked. Unlock the buffer first."));
	FMemBlock::Free(CurrentBufferBottomOfPipe);
}

void* FGnmMultiBufferResource::GetCurrentBuffer(bool bTopOfPipe)
{
	check(!IsRunningRHIInSeparateThread() || bTopOfPipe == IsInRenderingThread());

	// The render thread is allowed to lock/unlock buffers, so it has its own copy for its time line.
	// All other threads use the second copy, which is updated by the RHI thread. Parallel translate tasks should also use this copy.
	// The RHI thread fence in Unlock() ensures no parallel translate threads will run until the unlock has completed.
	FMemBlock& Buffer = bTopOfPipe
		? CurrentBufferTopOfPipe
		: CurrentBufferBottomOfPipe;

	void* Pointer = Buffer.GetPointer();
	check(Pointer);

	return Pointer;
}

void* FGnmMultiBufferResource::Lock(FRHICommandListImmediate& RHICmdList, EResourceLockMode InLockMode, uint32 Size, uint32 Offset)
{
	// Various parts of the engine lock/unlock buffers on the RHI thread. This is safe, so long
	// as those buffers are then never locked/unlocked on any other thread for their lifetime.
	//check(IsInRenderingThread());

	checkf(!bLocked, TEXT("The buffer is already locked."));

	void* Result = nullptr;

	if (InLockMode == RLM_WriteOnly)
	{
		// We don't support locking for write with offsets. Writes always return a new buffer,
		// so the old memory outside of the lock area would have to be copied over.
		check(Offset == 0);
		check(Size <= CreatedSize);

		if (!bFirstLock)
		{
			CurrentBufferTopOfPipe = FMemBlock::Allocate(CreatedSize, DEFAULT_VIDEO_ALIGNMENT, MemoryType, StatId);
		}
		
		Result = CurrentBufferTopOfPipe.GetPointer();
	}
	else
	{
		// Only the render thread can lock for read access.
		check(IsInRenderingThread());
		check(InLockMode == RLM_ReadOnly);

		// Locking for read must happen immediately, so we need to flush the RHI thread and the GPU.
		// This will be expensive and will result in a hitch.
		// TODO: Implement finer grain fencing to avoid a complete flush.
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GGnmManager.WaitForGPUIdleNoReset();

		// Since the RHIT has been flushed, we can safely access the RHIT version of CurrentBuffer
		checkf(CurrentBufferBottomOfPipe.GetPointer() != nullptr, TEXT("There is no buffer data to read back."));

		Result = CurrentBufferBottomOfPipe.GetPointer();
	}

	if (!bFirstLock && RHICmdList.IsBottomOfPipe())
	{
		// Update the RHIT pointer now, so things stay in sync.
		SwitchBuffers(CurrentBufferTopOfPipe);
	}

	check(Result);

	bLocked = true;
	LockedMode = InLockMode;
	return reinterpret_cast<uint8*>(Result) + Offset;
}

void FGnmMultiBufferResource::Unlock(FRHICommandListImmediate& RHICmdList)
{
	check(bLocked);

	if (bFirstLock)
	{
		// Nothing to do on the first lock. Pointers are already initialized
		// by the constructor, and we don't need to free anything.
		bFirstLock = false;
	}
	else if (LockedMode == RLM_WriteOnly && RHICmdList.IsTopOfPipe())
	{
		RHICmdList.EnqueueLambda([NewBuffer = CurrentBufferTopOfPipe, this](FRHICommandListImmediate& RHICmdList)
		{
			SwitchBuffers(NewBuffer);
		});

		RHICmdList.RHIThreadFence(true);
	}

	bLocked = false;
	LockedMode = EResourceLockMode::RLM_Num;
}

void FGnmMultiBufferResource::Swap(FGnmMultiBufferResource& Other)
{
	check(!bLocked);
	::Swap(*this, Other);
}

void FGnmMultiBufferResource::SwitchBuffers(const FMemBlock& NewBuffer)
{
	if (CurrentBufferBottomOfPipe.GetPointer() != NewBuffer.GetPointer())
	{
		FMemBlock::Free(CurrentBufferBottomOfPipe);
		CurrentBufferBottomOfPipe = NewBuffer;
	}
}


