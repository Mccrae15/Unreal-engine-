// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmMemory.cpp: Gnm memory allocation implementation
=============================================================================*/

#if USE_NEW_PS4_MEMORY_SYSTEM == 0

#include "GnmRHIPrivate.h"
#include "GrowableAllocator.h"
#include "Misc/ScopeLock.h"

///////////////////////////////////////////////////////////////////////////////////////
// FMemBlock functions
///////////////////////////////////////////////////////////////////////////////////////

#if USE_GPU_OVERWRITE_CHECKING
static TMap<void*, uint32*> GOverwriteCheckLocations;

void CheckForGPUOverwrites()
{
	for (auto It = GOverwriteCheckLocations.CreateIterator(); It; ++It)
	{
		checkf(*It.Value() == OVERWRITE_CHECKING_MAGIC, TEXT("GPU memory block starting at %p was overwritten at %p with %x"), It.Key(), It.Value(), *It.Value());
	}
}

#endif

// region descriptions
const TCHAR* MemoryDescriptions[(int32)EGnmMemType::GnmMem_MaxNumTypes] =
{ 
	TEXT("WC_Garlic_NV"),
	TEXT("WB_Onion_NV"),
};

SceKernelMemoryType SceMemoryTypes[(int32)EGnmMemType::GnmMem_MaxNumTypes] =
{
	SCE_KERNEL_WC_GARLIC,
	SCE_KERNEL_WB_ONION
};

FPlatformMemory::EMemoryCounterRegion MemoryStatRegions[(int32)EGnmMemType::GnmMem_MaxNumTypes] =
{ 
	FPlatformMemory::MCR_GPU,
	FPlatformMemory::MCR_GPUSystem,
};

// static vars
FCriticalSection MemBlockLock;
#define LOCK_MEMBLOCK (1)

FGrowableAllocator* FMemBlock::Allocators[(int32)EGnmMemType::GnmMem_MaxNumTypes] = { 0 };
TArray<FMemBlock> FMemBlock::DelayedFrees[2];
uint32 FMemBlock::CurrentFreeArray = 0;
#if TRACK_GNM_MEMORY_USAGE
TMap<FName, uint64> FMemBlock::TaggedMemorySizes[(int32)EGnmMemType::GnmMem_MaxNumTypes];
#endif

FMemBlock::FMemBlock(EGnmMemType Type, void* InPtr, uint32 InSize, TStatId InStat)
	: MemType(Type)
	, Pointer(InPtr)
	, Size(InSize)
#if TRACK_GNM_MEMORY_USAGE
	, Stat(InStat)
#endif
{
}

FMemBlock::FMemBlock(const FMemBlock& Other)
	: MemType(Other.MemType)
	, Pointer(Other.Pointer)
	, Size(Other.Size)
#if TRACK_GNM_MEMORY_USAGE
	, Stat(Other.Stat)
#endif
{
}

FMemBlock FMemBlock::Allocate(uint32 Size, uint32 Alignment, EGnmMemType Type, TStatId Stat, bool LlmTrackAlloc)
{
#if LOCK_MEMBLOCK
	FScopeLock Lock(&MemBlockLock);
#endif
	// this allocator doesn't support 0 sized allocations.  If we let it, it will return the same address twice causing problems on Free.
	check (Size > 0);

	// update out memory stats
#if TRACK_GNM_MEMORY_USAGE
	TrackMemoryUsage(Type, Stat, Size);
#endif

	// make sure the allocator exists
	if (Allocators[(int32)Type] == NULL)
	{
		for (uint TypeIndex = 0; TypeIndex < (int32)EGnmMemType::GnmMem_MaxNumTypes; ++TypeIndex)
		{
			if (Allocators[TypeIndex] == NULL)
			{
				Allocators[TypeIndex] = new FGrowableAllocator(SceMemoryTypes[TypeIndex], MemoryStatRegions[TypeIndex]);
			}
		}
		extern void AllocateRemainingPhysicalMemoryForCPU();
		AllocateRemainingPhysicalMemoryForCPU();
	}

	
#if USE_GPU_OVERWRITE_CHECKING
	const uint32 OverwriteCheckSize = 4;
#else
	const uint32 OverwriteCheckSize = 0;
#endif

	// allocate from the requested region
	void* Ptr = Allocators[(int32)Type]->Malloc(Size + OverwriteCheckSize, Alignment);

	// make sure it succeeded
	if (Ptr != NULL)
	{
#if USE_GPU_OVERWRITE_CHECKING
		// write the magic bits
		uint32* OverwriteCheckLoc = (uint32*)((uint8*)Ptr + Size);
		*OverwriteCheckLoc = OVERWRITE_CHECKING_MAGIC;

		// track it
		GOverwriteCheckLocations.Add(Ptr, OverwriteCheckLoc);
#endif

#if ENABLE_LOW_LEVEL_MEM_TRACKER
		if (LlmTrackAlloc)
		{
			FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, Ptr, Size);
		}
#endif

		switch (Type)
		{
		case EGnmMemType::GnmMem_GPU:
			FPlatformAtomics::InterlockedAdd(&GPS4Allocated_MemBlockGarlic, Size);
			break;

		case EGnmMemType::GnmMem_CPU:
			FPlatformAtomics::InterlockedAdd(&GPS4Allocated_MemBlockOnion, Size);
			break;
		}

		return FMemBlock(Type, Ptr, Size, Stat);
	}

#if TRACK_GNM_MEMORY_USAGE
	// if we got here, the allocation failed, so dump allocations
	UE_LOG(LogPS4, Warning, TEXT("\nFailed to allocate video memory in region %s, %d bytes. Current allocations:\n"), MemoryDescriptions[(int32)Type], Size);
	Dump();
#endif

	return FMemBlock();
}

void FMemBlock::Free(FMemBlock Mem)
{
#if LOCK_MEMBLOCK
	FScopeLock Lock(&MemBlockLock);
#endif
	if (Mem.Pointer != NULL)
	{
		DelayedFrees[CurrentFreeArray].Add(Mem);
	}
}

void FMemBlock::ProcessDelayedFrees()
{
#if LOCK_MEMBLOCK
	FScopeLock Lock(&MemBlockLock);
#endif
	// get the array to free from
	TArray<FMemBlock>& Frees = DelayedFrees[1 - CurrentFreeArray];

	for (int32 FreeIndex = 0; FreeIndex < Frees.Num(); FreeIndex++)
	{
		FMemBlock& Mem = Frees[FreeIndex];

		// blocks with invalid memtype are not freed (another FMemBlock should own the memory)
		if (Mem.MemType == EGnmMemType::GnmMem_Invalid)
		{
			continue;
		}

#if USE_GPU_OVERWRITE_CHECKING
		// remove tracking on this buffer now
		GOverwriteCheckLocations.Remove(Mem.Pointer);
#endif

		LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, Mem.GetPointer()));
		
		switch (Mem.MemType)
		{
		case EGnmMemType::GnmMem_GPU:
			FPlatformAtomics::InterlockedAdd(&GPS4Allocated_MemBlockGarlic, -int64(Mem.GetSize()));
			break;

		case EGnmMemType::GnmMem_CPU:
			FPlatformAtomics::InterlockedAdd(&GPS4Allocated_MemBlockOnion, -int64(Mem.GetSize()));
			break;
		}

		// free the memory now
		Allocators[(int32)Mem.MemType]->Free(Mem.Pointer);

		// update the memory stats
#if TRACK_GNM_MEMORY_USAGE
		TrackMemoryUsage(Mem.MemType, Mem.Stat, 0 - (int64)Mem.Size);
#endif
	}

	// done, and swap!
	Frees.Empty();
	CurrentFreeArray = 1 - CurrentFreeArray;
}

#if TRACK_GNM_MEMORY_USAGE

void FMemBlock::TrackMemoryUsage(EGnmMemType Type, TStatId Stat, int64 ChangeInMemorySize)
{
#if LOCK_MEMBLOCK
	FScopeLock Lock(&MemBlockLock);
#endif
	// track the memory (this is only used when we fail an allocation)
	uint64* MemCount = TaggedMemorySizes[(int32)Type].Find(Stat.GetName());
	if (MemCount == NULL)
	{
		MemCount = &TaggedMemorySizes[(int32)Type].Add(Stat.GetName(), 0);
	}
	*MemCount += ChangeInMemorySize;

	// track using normal stats system
	INC_DWORD_STAT_BY_FName(Stat.GetName(), ChangeInMemorySize);
}

#endif

FAutoConsoleCommand PS4CmdDumpGPUMem(
	TEXT("r.PS4.DumpGPUMem"),
	TEXT("Dump current GPU memory stats."),
	FConsoleCommandDelegate::CreateStatic(FMemBlock::Dump)
	);


FGnmMemoryStat FMemBlock::GetStats()
{
	FGnmMemoryStat Stat;

	// Called too early
	if (Allocators[0] == nullptr)
	{
		return Stat;
	}

	// Garlic
	uint64 Free;
	Allocators[0]->GetAllocationInfo(Stat.GarlicMemory.UsedSize, Free);
	Stat.GarlicMemory.HeapSize = Stat.GarlicMemory.UsedSize + Free;

	// Onion
	Allocators[1]->GetAllocationInfo(Stat.OnionMemory.UsedSize, Free);
	Stat.OnionMemory.HeapSize = Stat.OnionMemory.UsedSize + Free;

	return Stat;
}

void FMemBlock::Dump()
{
#if LOCK_MEMBLOCK
	FScopeLock Lock(&MemBlockLock);
#endif

#if TRACK_GNM_MEMORY_USAGE
	// look for queued up frees that aren't free yet
	uint64 OutstandingFrees[(int32)EGnmMemType::GnmMem_MaxNumTypes];
	FMemory::Memzero(OutstandingFrees, sizeof(OutstandingFrees));
	for (int32 Buffer = 0; Buffer < 2; Buffer++)
	{
		TArray<FMemBlock>& Frees = DelayedFrees[Buffer];
		for (int32 FreeIndex = 0; FreeIndex < Frees.Num(); FreeIndex++)
		{
			const FMemBlock& Mem = Frees[FreeIndex];
			if (Mem.MemType >= (EGnmMemType)0 && Mem.MemType < EGnmMemType::GnmMem_MaxNumTypes)
			{
			OutstandingFrees[(int32)Mem.MemType] += Mem.Size;
		}
	}
	}

	// printout the amount of memory allocated
	for (int32 Type = 0; Type < (int32)EGnmMemType::GnmMem_MaxNumTypes; Type++)
	{
		uint64 Total = 0;
		for (TMap<FName, uint64>::TIterator It(TaggedMemorySizes[Type]); It; ++It)
		{
			// track the memory for this tag in this region type
			UE_LOG(LogPS4, Log, TEXT("[%s] Tag %s is using %.2fMB"), MemoryDescriptions[Type], *It.Key().ToString(), (double)It.Value() / (1024.0 * 1024.0));
			Total += It.Value();
		}

		UE_LOG(LogPS4, Log, TEXT("[%s] --TOTAL-- %.2fMB [%.2fMB in pending deletes]\n"), MemoryDescriptions[Type],
			(double)Total / (1024.0 * 1024.0), (double)OutstandingFrees[Type] / (1024.0 * 1024.0));
	}
#endif

	for (int32 Type = 0; Type < (int32)EGnmMemType::GnmMem_MaxNumTypes; Type++)
	{
		Allocators[Type]->DumpInfo();
	}
}

#if USE_DEFRAG_ALLOCATOR
bool GPS4DefraggerInitialized = false;
extern void PS4GetDefragMemoryStats(int64& OutUsedSize, int64& OutAvailableSize, int64& OutPendingAdjustment, int64& OutPaddingWaste, int64& OutTotalSize)
{
	if (GPS4DefraggerInitialized)
	{
		GGnmManager.DefragAllocator.GetMemoryStats(OutUsedSize, OutAvailableSize, OutPendingAdjustment, OutPaddingWaste);
		OutTotalSize = GGnmManager.DefragAllocator.GetTotalSize();
	}
	else
	{
		OutUsedSize = 0;
		OutAvailableSize = 0;
		OutPendingAdjustment = 0;
		OutPaddingWaste = 0;
		OutTotalSize = 0;
	}
}
#endif

// -----------------------------------------------------------------------------------------------------
//
//                                   AVPlayer Media Library Allocator
//
// -----------------------------------------------------------------------------------------------------

// Currently we are allocating media memory from the CPU pool and granting GPU read/write
// access via memory protection bits, to work around OOM cases with our small ONION pool.
// Because of this, we lose the access protection of CPU pages, which may allow bad memory
// writes from the GPU to corrupt CPU data in those pages. Enabling this define will force
// all media allocations in the CPU pool to be page aligned, isolating them from other CPU
// data, at the expense of wasting memory.
#define PS4_MEDIA_FORCE_PAGE_ALIGNED_ALLOCATION 0

namespace PS4MediaAllocators
{
	/** Allocation table entry. */
	struct FAllocation
	{
		uint8* Ptr;
		size_t Size;
	};


	/** Critical section for synchronizing allocator access. */
	static FCriticalSection CPUAllocatorCS;
	static FCriticalSection GPUAllocatorCS;

	/** Collection of active allocations. */
	static TMap<void*, FAllocation> CPUAllocationsMap;
	static TMap<void*, FMemBlock>   GPUAllocationsMap;

	/** Maps allocations to page reference counts. */
	static TMap<void*, uint32> PageReferences;


	/** Page size. */
	static const size_t PageSize = FPlatformMemory::GetConstants().PageSize;

	/** Page alignment mask. */
	static const uintptr_t PageAlignMask = ~(PageSize - 1);


	void SetPageProtectionFlags(FAllocation const& Allocation, int32 ProtectionFlags)
	{
		uint32 NumPages = FMath::DivideAndRoundUp(Allocation.Size, PageSize);

		uint8* FirstPage = (uint8*)(uintptr_t(Allocation.Ptr) & PageAlignMask);
		uint8* LastPage = FirstPage + (NumPages * PageSize);

		// For each page the allocation covers...
		for (uint8* CurrentPage = FirstPage; CurrentPage < LastPage; CurrentPage += PageSize)
		{
			// Query the page protection flags for the current page.
			int32 PageProtection;
			int32 Result = sceKernelQueryMemoryProtection(CurrentPage, nullptr, nullptr, &PageProtection);
			check(Result == SCE_OK);

			if ((PageProtection & ProtectionFlags) != ProtectionFlags)
			{
				// Enable the specified protection flags for this page.
				Result = sceKernelMprotect(CurrentPage, PageSize, PageProtection | ProtectionFlags);
				check(Result == SCE_OK);
			}

			// Increment the ref count on this page.
			PageReferences.FindOrAdd(CurrentPage)++;
		}
	}


	void ResetPageProtectionFlags(FAllocation const& Allocation, int32 ProtectionFlags)
	{
		uint32 NumPages = FMath::DivideAndRoundUp(Allocation.Size, PageSize);

		uint8* FirstPage = (uint8*)(uintptr_t(Allocation.Ptr) & PageAlignMask);
		uint8* LastPage = FirstPage + (NumPages * PageSize);

		// For each page the allocation covers...
		for (uint8* CurrentPage = FirstPage; CurrentPage < LastPage; CurrentPage += PageSize)
		{
			uint32& RefCount = PageReferences.FindChecked(CurrentPage);

			if (--RefCount != 0)
				continue;

			int32 PageProtection;
			int32 Result;

			// Remove protection flag from this page.
			Result = sceKernelQueryMemoryProtection(CurrentPage, nullptr, nullptr, &PageProtection);
			check(Result == SCE_OK);

			PageProtection &= ~ProtectionFlags;

			Result = sceKernelMprotect(CurrentPage, PageSize, PageProtection);
			check(Result == SCE_OK);

			PageReferences.FindAndRemoveChecked(CurrentPage);
		}
	}


	void* Allocate(void* /*Jumpback*/, uint32 Alignment, uint32 Size)
	{
#if PS4_MEDIA_FORCE_PAGE_ALIGNED_ALLOCATION
		// Force allocations to be page aligned/sized to isolate them from CPU data.
		Alignment = Align(Alignment, PageSize);
		Size = Align(Size, PageSize);
#else
		// this is currently required due to a bug in AvPlayer
		// see: https://ps4.scedev.net/technotes/view/683/1
		Alignment = FMath::Max<uint32>(8, Alignment);
#endif

		FScopeLock Lock(&CPUAllocatorCS);

		uint8* UserMem = nullptr;

		FAllocation Allocation;
		Allocation.Size = Size;
		if (Alignment <= PageSize)
		{
			// For supported alignments, just use regular system allocator.
			UserMem = Allocation.Ptr = (uint8*)FMemory::Malloc(Size, Alignment);
		}
		else
		{
			// Since the binned allocator doesn't support large alignments,
			// we are going to allocate extra space and align it ourselves here.
			Allocation.Size = Size + Alignment;
			Allocation.Ptr = (uint8*)FMemory::Malloc(Allocation.Size, PageSize);
			UserMem = AlignArbitrary(Allocation.Ptr, Alignment);
		}

		// Sanity checks
		check((UserMem + Size) <= (Allocation.Ptr + Allocation.Size));
		check((uintptr_t(UserMem) % uintptr_t(Alignment)) == 0);

		// The GPU requires read/write access to these pages.
		SetPageProtectionFlags(Allocation, SCE_KERNEL_PROT_GPU_RW);

		// Keep the allocation in the map, so we can look up the original location when we free it.
		CPUAllocationsMap.Add(UserMem, Allocation);

		return UserMem;
	}


	void Deallocate(void* /*Jumpback*/, void* Mem)
	{
		// Use the map to find the original memory allocation.
		FAllocation Allocation;
		{
			FScopeLock Lock(&CPUAllocatorCS);
			Allocation = CPUAllocationsMap.FindAndRemoveChecked(Mem);
		}

		ResetPageProtectionFlags(Allocation, SCE_KERNEL_PROT_GPU_RW);
		FMemory::Free(Allocation.Ptr);
	}


	void* AllocateTexture(void* /*Jumpback*/, uint32 Alignment, uint32 Size)
	{
		// Note: No need to take the allocator lock here.

		// Allocate memory directly from the GARLIC pool
		FMemBlock Allocation = FMemBlock::Allocate(Size, Alignment, EGnmMemType::GnmMem_GPU, TStatId());
		void* AllocatedMem = Allocation.GetPointer();

		// Check for correct alignment
		check((uintptr_t)AllocatedMem % (uintptr_t)Alignment == 0);

		{
			FScopeLock Lock(&GPUAllocatorCS);
			GPUAllocationsMap.Add(AllocatedMem, Allocation);
		}

		return AllocatedMem;
	}


	void DeallocateTexture(void* /*Jumpback*/, void* Memory)
	{
		// Use the map to find the original memory allocation.
		FMemBlock MemBlock;
		{
			FScopeLock Lock(&GPUAllocatorCS);
			MemBlock = GPUAllocationsMap.FindAndRemoveChecked(Memory);
		}

		FMemBlock::Free(MemBlock);
	}

}

#endif // USE_NEW_PS4_MEMORY_SYSTEM == 0
