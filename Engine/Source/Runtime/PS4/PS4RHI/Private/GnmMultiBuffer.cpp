// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmVertexBuffer.cpp: Gnm vertex buffer RHI implementation.
=============================================================================*/

#include "GnmRHIPrivate.h"

#if PS4_USE_NEW_MULTIBUFFER

FGnmMultiBufferResource::FGnmMultiBufferResource(uint32 Size, uint32 Usage, TStatId StatGPU, TStatId StatCPU)
	: CreatedSize(Size)
	, MemoryType((Usage & BUF_KeepCPUAccessible) ? EGnmMemType::GnmMem_CPU : EGnmMemType::GnmMem_GPU)
	, StatId((Usage & BUF_KeepCPUAccessible) ? StatCPU : StatGPU)
	, bLocked(false)
	, bFirstLock(true)
{
	CurrentBufferRT = FMemBlock::Allocate(CreatedSize, DEFAULT_VIDEO_ALIGNMENT, MemoryType, StatId);
	CurrentBufferRHIT = CurrentBufferRT;
}

FGnmMultiBufferResource::~FGnmMultiBufferResource()
{
	checkf(!bLocked, TEXT("Cannot destruct a buffer which is still locked. Unlock the buffer first."));
	FMemBlock::Free(CurrentBufferRHIT);
}

void* FGnmMultiBufferResource::GetCurrentBuffer()
{
	// The render thread is allowed to lock/unlock buffers, so it has its own copy for its time line.
	// All other threads use the second copy, which is updated by the RHI thread. Parallel translate tasks should also use this copy.
	// The RHI thread fence in Unlock() ensures no parallel translate threads will run until the unlock has completed.
	FMemBlock& Buffer = IsInRenderingThread()
		? CurrentBufferRT
		: CurrentBufferRHIT;

	void* Pointer = Buffer.GetPointer();
	check(Pointer);

	return Pointer;
}

void* FGnmMultiBufferResource::Lock(FRHICommandListImmediate& RHICmdList, EResourceLockMode LockMode, uint32 Size, uint32 Offset)
{
	// Various parts of the engine lock/unlock buffers on the RHI thread. This is safe, so long
	// as those buffers are then never locked/unlocked on any other thread for their lifetime.
	//check(IsInRenderingThread());

	checkf(!bLocked, TEXT("The buffer is already locked."));

	void* Result = nullptr;

	if (LockMode == RLM_WriteOnly)
	{
		// We don't support locking for write with offsets. Writes always return a new buffer,
		// so the old memory outside of the lock area would have to be copied over.
		check(Offset == 0);
		check(Size <= CreatedSize);

		if (!bFirstLock)
		{
			CurrentBufferRT = FMemBlock::Allocate(CreatedSize, DEFAULT_VIDEO_ALIGNMENT, MemoryType, StatId);
		}
		
		Result = CurrentBufferRT.GetPointer();
	}
	else
	{
		// Only the render thread can lock for read access.
		check(IsInRenderingThread());
		check(LockMode == RLM_ReadOnly);

		// Locking for read must happen immediately, so we need to flush the RHI thread and the GPU.
		// This will be expensive and will result in a hitch.
		// TODO: Implement finer grain fencing to avoid a complete flush.
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GGnmManager.WaitForGPUIdleNoReset();

		// Since the RHIT has been flushed, we can safely access the RHIT version of CurrentBuffer
		checkf(CurrentBufferRHIT.GetPointer() != nullptr, TEXT("There is no buffer data to read back."));

		Result = CurrentBufferRHIT.GetPointer();
	}

	if (!bFirstLock && !IsInRenderingThread())
	{
		// Only the RHI or render threads can lock/unlock buffers.
		check(IsInRHIThread());

		// The RHIT did the lock. Update the RHIT pointer now, so things stay in sync.
		FMemBlock::Free(CurrentBufferRHIT);
		CurrentBufferRHIT = CurrentBufferRT;
	}

	check(Result);

	bLocked = true;
	return reinterpret_cast<uint8*>(Result) + Offset;
}

void FGnmMultiBufferResource::Unlock(FRHICommandListImmediate& RHICmdList)
{
	check(bLocked);
	bLocked = false;

	if (bFirstLock)
	{
		// Nothing to do on the first lock. Pointers are already initialized
		// by the constructor, and we don't need to free anything.
		bFirstLock = false;
		return;
	}

	if (!IsInRenderingThread())
	{
		// Nothing to do on the RHI thread
		check(IsInRHIThread());
		return;
	}

	// Swap the buffer pointers on the RHI thread
	bool bAddFence = RHICmdList.EnqueueLambda([NewBuffer = CurrentBufferRT, this](FRHICommandListImmediate& RHICmdList)
	{
		FMemBlock::Free(CurrentBufferRHIT);
		CurrentBufferRHIT = NewBuffer;
	});

	if (bAddFence)
	{
		// Insert the RHI thread lock fence. This stops any parallel translate tasks
		// running until the unlock command above has completed on the RHI thread.
		RHICmdList.RHIThreadFence(true);
	}
}

#else

#define PS4_DEFRAG_MULTIBUFFER 1

FGnmMultiBufferResource::FGnmMultiBufferResource(uint32 Size, uint32 Usage, TStatId StatGPU, TStatId StatCPU)	
	: CurrentBuffer(0)
	, bMultiBufferDefraggable(false)
	, bVolatile(false)
#if ENABLE_BUFFER_INTEGRITY_CHECK
	, Buffer0AllocationFrame(0xFFFFFFFF)
#endif	
{
	// default to GPU-accessible memory. Writes to Garlic are close enough to Onion speeds and it's better for the GPU.
	EGnmMemType MemoryType = EGnmMemType::GnmMem_GPU;
	TStatId Stat = StatGPU;

	// only go to Onion if we want CPU readback
	if (Usage & BUF_KeepCPUAccessible)
	{
		MemoryType = EGnmMemType::GnmMem_CPU;
		Stat = StatCPU;
	}

	int32 NumBuffers = 1;

	bool bMayNeedGPUWrite = !(Usage & BUF_Static) || BUF_UnorderedAccess;

	// figure out how many buffers to use (0, 1, or 2), and what memory to use
	if (Usage & BUF_Volatile)
	{		
		bVolatile = true;
	}
	// BUF_UnorderedAccess wants to be in GPU memory, like static
	else if (Usage & (BUF_Static | BUF_UnorderedAccess))
	{
		NumBuffers = 1;
	}
	else if (Usage & BUF_Dynamic)
	{
		NumBuffers = 3;
	}
	else
	{
		UE_LOG(LogPS4, Fatal, TEXT("FGnmMultiBufferResource created without Static, Volatile, or Dynamic specified"))
	}		

#if PS4_DEFRAG_MULTIBUFFER		
	if (!bVolatile && (Size > MAX_SIZE_GPU_ALLOC_POOL) && MemoryType == EGnmMemType::GnmMem_GPU)
	{
		bMultiBufferDefraggable = true;
	}
#endif
	
	//add zeroed up front so if the inline allocator isn't sufficient we don't end up allocating a new chunk and doing
	//memmoves to shift our constructed buffer objects around. (which won't work because each one will register with the defrag allocator as necessary)
	Buffers.AddZeroed(NumBuffers);
	new (&Buffers[0]) FMultiBufferInternalDefrag(this, 0, bMayNeedGPUWrite);
	// handle "zero buffer" resources, where the buffer is allocated from a ring buffer before every write
	if (bVolatile)
	{
		// no buffer yet!		
		Buffers[0].GetMemBlock().SetTransientSize(Size);
	}
	else
	{
		// allocate main buffer
		Buffers[0].Allocate(Size, MemoryType, bMultiBufferDefraggable, Stat);
	}

		
	// allocate memblock storage
	for (uint32 BufferIndex = 1; BufferIndex < NumBuffers; BufferIndex++)
	{		
		new (&Buffers[BufferIndex]) FMultiBufferInternalDefrag(this, BufferIndex, bMayNeedGPUWrite);
		Buffers[BufferIndex].Allocate(Size, MemoryType, bMultiBufferDefraggable, Stat);
	}
}

FGnmMultiBufferResource::~FGnmMultiBufferResource()
{
	//buffers should get cleaned up by Tarray destruction
}

void* FGnmMultiBufferResource::GetCurrentBuffer(bool bNeedWriteAccess, bool bUpdatePointer, bool bDefragLock)
{	
	void* BufferData = nullptr;
	if (bVolatile)
	{
		FMemBlock& Buffer0 = Buffers[0].GetMemBlock();
		if (bNeedWriteAccess)
		{			
			BufferData = GGnmManager.AllocateFromTempRingBuffer(Buffer0.GetSize());

			if (bUpdatePointer)
			{
				// volatile buffers allocate new memory every write (we may write > 2 times per frame)
				Buffer0.OverridePointer(BufferData);

#if ENABLE_BUFFER_INTEGRITY_CHECK
				Buffer0AllocationFrame = GGnmManager.GetFrameCount();
#endif
			}
		}
		else
		{
			BufferData = Buffer0.GetPointer();
		}

#if ENABLE_BUFFER_INTEGRITY_CHECK
		if (bUpdatePointer || !bNeedWriteAccess)
		{
			// Check that we are using the buffer the same frame it was allocated from the ring buffer
			uint32 CurrentFrame = GGnmManager.GetFrameCount();

			bool bValidAllocation = (Buffer0AllocationFrame == CurrentFrame) || BufferData == nullptr;;
			checkf(bValidAllocation, TEXT("Volatile buffer is used over multiple frames"));
		}
#endif

		return BufferData;
	}

	const int32 NumBuffers = Buffers.Num();
	uint32 DataBufferIndex = CurrentBuffer; 

	// before we return a buffer for writing, we need to move to the next one so as not to stomp
	// on the buffer the GPU may still be using
	if (bNeedWriteAccess)
	{
		DataBufferIndex = (CurrentBuffer + 1) % NumBuffers;
	}

	if (bUpdatePointer)
	{
#if ENABLE_BUFFER_INTEGRITY_CHECK
		// Check that we are using the buffer the same frame it was allocated from the ring buffer
		uint32 CurrentFrame = GGnmManager.GetFrameCount();

		bool bValidAllocation = Buffer0AllocationFrame != CurrentFrame;
		checkf(bValidAllocation, TEXT("Dynamic buffer updated more than once in the same frame. Not supported on PS4"));
#endif
	
		CurrentBuffer = DataBufferIndex;
	}		

	if (bDefragLock && bMultiBufferDefraggable)
	{
		Buffers[DataBufferIndex].Lock();
	}

	// now return the proper mem block	
	return Buffers[DataBufferIndex].GetMemBlock().GetPointer();
}


void* FGnmMultiBufferResource::Lock(EResourceLockMode LockMode, uint32 Size, uint32 Offset)
{	
	uint8* BufferData = nullptr;

#if USE_DEFRAG_ALLOCATOR
	if (bMultiBufferDefraggable)
	{
		check(!bVolatile);
		//on unlock we won't know the proper one to unlock, so lock them all.
		//all locks normally go through deferredlock when running normally anyway.
		FScopedGPUDefragLock DefragLock(GGnmManager.DefragAllocator);
		for (int32 i = 0; i < Buffers.Num(); ++i)
		{
			Buffers[i].Lock();
		}
	}
#endif

	// we need to make sure the GPU is done with the vertex buffer
	if (LockMode == RLM_ReadOnly)
	{
		// we are assuming the caller has flushed and waited on the GPU if necessary
		// get the buffer for reading directly as CPU pointer
		BufferData = (uint8*)GetCurrentBuffer(false, false);
	}
	else
	{
		//we don't upport locking for write with offsets.  Writes always return a new buffer, so the old memory outside of the lock area would have to be copied over.
		//Preferably you avoid this situation
		check(Size == Buffers[0].GetMemBlock().GetSize() || Offset == 0);

		// get the buffer for writing (for volatile buffers, we only need to allocate the size needed for locking)
		BufferData = (uint8*)GetCurrentBuffer(true, true);
	}	

	return BufferData + Offset;
}

void FGnmMultiBufferResource::Unlock()
{	
	// @todo mem: do we need to flush any caches for UC_GARLIC memory?
#if USE_DEFRAG_ALLOCATOR
	if (bMultiBufferDefraggable)
	{
		FScopedGPUDefragLock DefragLock(GGnmManager.DefragAllocator);
		for (int32 i = 0; i < Buffers.Num(); ++i)
		{
			Buffers[i].Unlock();
		}
	}
#endif
}

/** Prepare a CPU accessible buffer for without modifying the resource object */
void* FGnmMultiBufferResource::LockDeferredUpdate(EResourceLockMode LockMode, uint32 Size, uint32 Offset)
{
	uint8* BufferData = nullptr;
	// we need to make sure the GPU is done with the vertex buffer
	if (LockMode == RLM_ReadOnly)
	{
		// we are assuming the caller has flushed and waited on the GPU if necessary
		// get the buffer for reading directly as CPU pointer
		BufferData = (uint8*)GetCurrentBuffer(false, false, true);
	}
	else
	{
		//we don't upport locking for write with offsets.  Writes always return a new buffer, so the old memory outside of the lock area would have to be copied over.
		//Preferably you avoid this situation
		check(Size == Buffers[0].GetMemBlock().GetSize() || Offset == 0);

		BufferData = (uint8*)GetCurrentBuffer(true, false, true);
	}	

	return BufferData + Offset;
}

/** Modify the buffer to point to the updated data */
void FGnmMultiBufferResource::UnlockDeferredUpdate(void* BufferData)
{			
	if (bVolatile)
	{
		check(!bMultiBufferDefraggable);
#if ENABLE_BUFFER_INTEGRITY_CHECK
		Buffer0AllocationFrame = GGnmManager.GetFrameCount();
#endif
		// volatile buffers allocate new memory every write (we may write > 2 times per frame)
		Buffers[0].GetMemBlock().OverridePointer(BufferData);
		return;
	}

	int32 NumBuffers = Buffers.Num();
	uint32 DataBufferIndex = (CurrentBuffer + 1) % NumBuffers;
	CurrentBuffer = DataBufferIndex;	
	
	//make sure we've been passed the correct data to go with this unlock.
	void* CheckBuffer = Buffers[DataBufferIndex].GetMemBlock().GetPointer();
	check(CheckBuffer == BufferData);

	if (bMultiBufferDefraggable)
	{
		Buffers[DataBufferIndex].Unlock();
	}
}

FGnmMultiBufferResource::FMultiBufferInternalDefrag::FMultiBufferInternalDefrag(FGnmMultiBufferResource* InOwner, int32 InBufferIndex, bool InMayNeedGPUWrite)
: Owner(InOwner)
, BufferIndex(InBufferIndex)
, bDefraggable(false)
#if MULTIBUFFER_OVERRUN_DETECTION
, LockCount(0)
, bLocallyOwned(false)
#endif
#if VALIDATE_MEMORY_PROTECTION
, bMayNeedGPUWrite(InMayNeedGPUWrite)
#endif
{
	check(Owner);
}

bool FGnmMultiBufferResource::FMultiBufferInternalDefrag::CanRelocate() const
{
	return bDefraggable;
}

void* FGnmMultiBufferResource::FMultiBufferInternalDefrag::GetBaseAddress()
{
	return MemBlock.GetPointer();
}

void FGnmMultiBufferResource::FMultiBufferInternalDefrag::UpdateBaseAddress(void* NewBaseAddress)
{
#if MULTIBUFFER_CACHED_LOCK_DETECTION
	AllowCPUAccess();
#endif
	MemBlock.OverridePointer(NewBaseAddress);
#if MULTIBUFFER_CACHED_LOCK_DETECTION
	ProtectFromCPUAccess();
#endif
}

#if MULTIBUFFER_CACHED_LOCK_DETECTION
void FGnmMultiBufferResource::FMultiBufferInternalDefrag::ProtectFromCPUAccess()
{
	int32 Ret = sceKernelMprotect(MemBlock.GetPointer(), MemBlock.GetSize(), SCE_KERNEL_PROT_GPU_RW);
	check(Ret == SCE_OK);
}

void FGnmMultiBufferResource::FMultiBufferInternalDefrag::AllowCPUAccess()
{
	int32 Ret = sceKernelMprotect(MemBlock.GetPointer(), MemBlock.GetSize(), SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_ALL);
	check(Ret == SCE_OK);
}
#endif

#if MULTIBUFFER_OVERRUN_DETECTION
void FGnmMultiBufferResource::FMultiBufferInternalDefrag::CheckCanary(bool bForce) const
{

	bool bLocked = (LockCount > 0) || !bDefraggable || bForce;

	//when using mprotect to remove cpu RW we can only read the canary memory at specific spots.
	//PS4 does not support a cpu read-only mprotect mode.
#if MULTIBUFFER_CACHED_LOCK_DETECTION
	bLocked = (LockCount > 0) || !bDefraggable;
#endif

	if (bLocallyOwned && bLocked)
	{
		check((MemBlock.GetSize() % 4) == 0);
		uint32* CanaryMem = (uint32*)MemBlock.GetPointer();
		CanaryMem += (MemBlock.GetSize() / 4) - 1;
		checkf(*CanaryMem == CanaryValue, TEXT("MultiBuffer corruption detected: 0x%x, 0x%p, 0x%p, 0x%p"), *CanaryMem, CanaryMem, MemBlock.GetPointer(), this);
	}
}
#endif

const uint32 MemProtectAlign = 16 * 1024;
void FGnmMultiBufferResource::FMultiBufferInternalDefrag::Allocate(uint32 Size, EGnmMemType MemoryType, bool bAllowDefrag, TStatId Stat)
{
	check(MemBlock.GetPointer() == nullptr);
	check(MemBlock.GetSize() == 0);
	check(!bAllowDefrag || MemoryType == EGnmMemType::GnmMem_GPU);

	uint32 Alignment = DEFAULT_VIDEO_ALIGNMENT;

#if MULTIBUFFER_OVERRUN_DETECTION
	bLocallyOwned = true;
	Size = Align(Size, 4) + 4;

#if MULTIBUFFER_CACHED_LOCK_DETECTION
	//memprotect works in 16KB sized and aligned pages.
	Size = Align(Size, MemProtectAlign);
	Alignment = MemProtectAlign;
#endif
#endif
	
#if USE_DEFRAG_ALLOCATOR
	bDefraggable = bAllowDefrag;
	if (bDefraggable)
	{
		void* BufferDefragMem = GGnmManager.DefragAllocator.AllocateLocked(Size, Alignment, Stat, true);
		if (BufferDefragMem)
		{
			MemBlock.OverridePointer(BufferDefragMem);
			MemBlock.OverrideSize(Size);
		}
		else
		{
			bDefraggable = false;
		}
	}
#else
	bDefraggable = false;
#endif

	if (!bDefraggable)
	{
		MemBlock = FMemBlock::Allocate(Size, DEFAULT_VIDEO_ALIGNMENT, MemoryType, Stat);
	}	

#if MULTIBUFFER_OVERRUN_DETECTION
	check((Size % 4) == 0);
	uint32* CanaryMem = (uint32*)MemBlock.GetPointer();
	CanaryMem += (Size / 4) - 1;
	*CanaryMem = CanaryValue;
#endif

#if USE_DEFRAG_ALLOCATOR
	if (bDefraggable)
	{
#if MULTIBUFFER_CACHED_LOCK_DETECTION
		ProtectFromCPUAccess();
#endif
		GGnmManager.DefragAllocator.RegisterDefraggable(this);
		GGnmManager.DefragAllocator.Unlock(MemBlock.GetPointer());
	}
#endif

}

void FGnmMultiBufferResource::FMultiBufferInternalDefrag::Free()
{	
#if USE_DEFRAG_ALLOCATOR
	if (bDefraggable)
	{
		FPS4GPUDefragAllocator& DefragAllocator = GGnmManager.DefragAllocator;
		FScopedGPUDefragLock DefragAllocatorLock(DefragAllocator);		
#if MULTIBUFFER_CACHED_LOCK_DETECTION
		AllowCPUAccess();
#endif
		DefragAllocator.UnregisterDefraggable(this);
		DefragAllocator.Free(GetBaseAddress());
#if MULTIBUFFER_OVERRUN_DETECTION
		CheckCanary(true);
#endif
	}
	else
#endif
	{
#if MULTIBUFFER_OVERRUN_DETECTION
		CheckCanary();
#endif
		FMemBlock::Free(MemBlock);
	}

#if MULTIBUFFER_OVERRUN_DETECTION
	bLocallyOwned = false;
#endif
}

void* FGnmMultiBufferResource::FMultiBufferInternalDefrag::Lock()
{
#if USE_DEFRAG_ALLOCATOR
	if (bDefraggable)
	{
		FPS4GPUDefragAllocator& DefragAllocator = GGnmManager.DefragAllocator;
		FScopedGPUDefragLock DefragAllocatorLock(DefragAllocator);
		DefragAllocator.Lock(MemBlock.GetPointer());

#if MULTIBUFFER_CACHED_LOCK_DETECTION
		AllowCPUAccess();
#endif

#if MULTIBUFFER_OVERRUN_DETECTION
		FPlatformAtomics::InterlockedIncrement(&LockCount);
		CheckCanary();
#endif
	}
#endif
	return MemBlock.GetPointer();
}

void FGnmMultiBufferResource::FMultiBufferInternalDefrag::Unlock()
{
#if USE_DEFRAG_ALLOCATOR
	if (bDefraggable)
	{
		FPS4GPUDefragAllocator& DefragAllocator = GGnmManager.DefragAllocator;
		FScopedGPUDefragLock DefragAllocatorLock(DefragAllocator);

#if MULTIBUFFER_OVERRUN_DETECTION
		FPlatformAtomics::InterlockedDecrement(&LockCount);
		CheckCanary();
#endif

#if MULTIBUFFER_CACHED_LOCK_DETECTION
		ProtectFromCPUAccess();
#endif
		DefragAllocator.Unlock(MemBlock.GetPointer());
	}
#endif
}

#endif