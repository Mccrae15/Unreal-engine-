// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GrowableAllocator.cpp: Memory allocator that allocates direct memory for GPU access (Onion or Garlic)
=============================================================================*/

#if USE_NEW_PS4_MEMORY_SYSTEM == 0

#include "GnmRHIPrivate.h"
#include "GrowableAllocator.h"
#include "List.h"
#include "Misc/ScopeLock.h"
#include "PS4/PS4LLM.h"

#define DEBUG_LOGS 0

// alignment of chunks ("page size"?)
#define GPU_CHUNK_ALIGNMENT 2 * 1024 * 1024

// minimum alignment of allocations inside of chunks
#define MIN_GPU_ALIGNMENT 4

#define ENABLE_GPUMALLOC_SANITYCHECK 0

/** Shared critical section object to protect the heap array from use on multiple threads */
FCriticalSection GGpuMallocCriticalSection;

// global shared instance
FCriticalSection FGrowableAllocator::CriticalSection;

/**
* Chunk used for allocations within a region of the FGrowableAllocator address space
*/
class FGpuMallocChunk : public FGpuMallocBase
{
	class FFreeEntry
	{
	public:
		/** Constructor */
		FFreeEntry(uint8* InLocation, uint64 InSize)
			: Location(InLocation)
			, BlockSize(InSize)
		{
		}

		/**
		* Determine if the given allocation with this alignment and size will fit
		* @param AllocationSize	Already aligned size of an allocation
		* @param Alignment			Alignment of the allocation (location may need to increase to match alignment)
		* @return true if the allocation will fit
		*/
		bool CanFit(uint64 AlignedSize, uint32 Alignment)
		{
			// location of the aligned allocation
			uint8* AlignedLocation = Align<uint8*>(Location, Alignment);

			// if we fit even after moving up the location for alignment, then we are good
			return (AlignedSize + (AlignedLocation - Location)) <= BlockSize;
		}

		/**
		* Take a free chunk, and split it into a used chunk and a free chunk
		* @param UsedSize	The size of the used amount (anything left over becomes free chunk)
		* @param Alignment	The alignment of the allocation (location and size)
		* @param bDelete Whether or not to delete this FreeEntry (ie no more is left over after splitting)
		* 
		* @return The pointer to the free data
		*/
		uint8* Split(uint64 UsedSize, uint32 Alignment, TMap<void*, __uint128_t>& TrackedAllocations, uint64& OutSizeWithPadding)
		{
			// make sure we are already aligned
			check((UsedSize & (Alignment - 1)) == 0);

			// this is the pointer to the free data
			uint8* FreePtr = Align<uint8*>( Location, Alignment );

			// Adjust the allocated size for any alignment padding
			uint64 Padding = uint64(FreePtr - Location);
			uint64 AllocationSize = UsedSize + uint64(Padding);
			OutSizeWithPadding = AllocationSize;

			
			{
				// update this free entry to just point to what's left after using the UsedSize
				Location += AllocationSize;
				BlockSize -= AllocationSize;
			}

			__uint128_t PointerSize = (__uint128_t)Padding << 64 | UsedSize;
			TrackedAllocations.Add(FreePtr, PointerSize);

			// return a usable pointer!
			return FreePtr;
		}

		/** Address in the heap */
		uint8*		Location;

		/** Size of the free block */
		uint32		BlockSize;
	};	

	/**
	* Bin of equally sized free chunks for fast allocs/frees
	*/
	class FFreePool
	{		
	public:

		FFreePool(FGpuMallocChunk* InParentChunk, int32 InPoolAllocSize)
		{
			bReservedAlloc = false;
			ParentChunk = InParentChunk;
			TotalPoolBytes = MaxPoolBytes;
			MaxAlignment = InPoolAllocSize;
			PoolAllocSize = InPoolAllocSize;

			//align to the poolsize so pointers can be easily mapped to their respective pools on free by masking the low bits.  Costs us some waste in padding.
			PoolBase = (uint8*)ParentChunk->MallocReservedPool();
			bReservedAlloc = PoolBase != nullptr;

			if (!PoolBase)
			{
				PoolBase = (uint8*)ParentChunk->Malloc(TotalPoolBytes, PoolBaseAlignment);
			}

			//make sure we start aligned, and each alloc is trivially aligned.
			check(PoolBase);
			check((uint64)PoolBase % MaxAlignment == 0);
			check(PoolAllocSize % MaxAlignment == 0);
			//fill our array with each alloc.
			int32 MaxAllocs = TotalPoolBytes / PoolAllocSize;
			uint8* AllocBase = (uint8*)PoolBase;			
			AvailableAllocs.Reserve(MaxAllocs);
			for (int32 i = 0; i < MaxAllocs; ++i)
			{
				AvailableAllocs.Add(AllocBase);
				AllocBase += PoolAllocSize;
			}
		}	

		~FFreePool()
		{
			int32 MaxAllocs = TotalPoolBytes / PoolAllocSize;
			checkf(AvailableAllocs.Num() == MaxAllocs, TEXT("Trying to free Pool %i byte Pool with oustanding %i allocations"), PoolAllocSize, MaxAllocs - AvailableAllocs.Num());
			if (bReservedAlloc)
			{
				ParentChunk->FreeReservedPool(PoolBase);
			}
			else
			{
				ParentChunk->Free(PoolBase);
			}
			bReservedAlloc = false;
		}

		/** Entry will fit properly within an allocation in this Pool.*/
		bool CanAllowEntry(uint32 Size, uint32 Alignment)
		{
			return Size <= PoolAllocSize && Alignment <= MaxAlignment;
		}

		bool CanFitEntry(uint32 Size, uint32 Alignment)
		{
			return AvailableAllocs.Num() > 0;
		}

		bool IsMemoryInPool(void* Ptr)
		{
			return (Ptr > PoolBase) && (Ptr < (PoolBase + TotalPoolBytes));
		}

		void* Malloc(uint32 Size, uint32 Alignment)
		{
			check(CanAllowEntry(Size, Alignment));
			return AvailableAllocs.Pop(false);			
		}

		void Free(void* FreePtr)
		{
			check((FreePtr >= PoolBase) && (FreePtr <= (PoolBase + TotalPoolBytes - PoolAllocSize)));
			AvailableAllocs.Add(FreePtr);
			check(AvailableAllocs.Num() <= TotalPoolBytes / PoolAllocSize);
		}

		TArray<void*> AvailableAllocs;

		FGpuMallocChunk* ParentChunk;

		//Beginning of the memory for this Pool.
		uint8* PoolBase;

		//total bytes allocated for this Pool.
		int32 TotalPoolBytes;

		int32 MaxAlignment;

		//Maximum size of an allocation in this Pool.
		int32 PoolAllocSize;

		bool bReservedAlloc;
	};

public:

	// pool sizes that we support
	enum class EPoolSizes : uint8
	{
		E128,
		E256,
		E512,
		E1k,
		E2k,
		E8k,
		E16k,		
		SizesCount,				
	};	

	constexpr static uint32 PoolSizes[(int32)EPoolSizes::SizesCount] =
	{
		128,
		256,
		512,
		1024,
		2048,
		8192,
		16 * 1024,		
	};	

	static const int32 MaxPoolAllocSize = PoolSizes[(int32)EPoolSizes::SizesCount - 1];
	static const int32 MaxPoolBytes = 4*1024*1024;
	static const uint64 PoolBaseAlignment = MaxPoolBytes;
	static const uint64 PoolBaseAlignMask = 0xFFFFFFFFFFC00000ULL;
	static const int32 NumPoolAllocsReserved = 8;
	

	/**
	* Constructor
	* @param InSize - size of this chunk
	* @param InBase - base address for this chunk
	* @param InGPUHostMemoryOffset - io address needed when the chunk's memory has to be unmapped
	*/
	FGpuMallocChunk(FGrowableAllocator* InAllocator, uint64 InSize, SceKernelMemoryType Type)
		: HeapSize(InSize)
		, UsedMemorySize(0)
		, UsedMemorySizePlusPadding(0)
	{
		extern void* MapLargeBlock(uint64 Size, int BusType, int Protection, off_t& OutMemOffset);
		HeapBase = MapLargeBlock(HeapSize, Type, SCE_KERNEL_PROT_CPU_READ|SCE_KERNEL_PROT_CPU_WRITE|SCE_KERNEL_PROT_GPU_ALL, MemoryOffset);

		if (HeapBase == NULL)
		{
			int32 NumChunks = InAllocator->AllocChunks.Num();
			int32 TotalAllocated = 0;
			int32 TotalWaste = 0;
			UE_LOG(LogPS4, Error,TEXT("===== Dumping %d chunks:"),NumChunks);
			for (int32 ChunkIndex = 0; ChunkIndex < NumChunks; ++ChunkIndex)
			{
				FGpuMallocChunk* Chunk = InAllocator->AllocChunks[ChunkIndex];
				if (Chunk)
				{
					uint32 LargestContiguous = 0;
					TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* CurrentNode = Chunk->FreeList.GetHead();
					
					while (CurrentNode)
					{
						FFreeEntry& EntryData = CurrentNode->GetValue();
						LargestContiguous = FMath::Max<uint32>(LargestContiguous, EntryData.BlockSize);
						CurrentNode = CurrentNode->GetNextNode();
					}
					UE_LOG(LogPS4,Error,TEXT("[%d]: %lld bytes %lld free %d contiguous"),
						ChunkIndex,
						Chunk->HeapSize, 
						Chunk->HeapSize - Chunk->UsedMemorySize,
						LargestContiguous
						);
				}
				else
				{
					UE_LOG(LogPS4,Error,TEXT("[%d]: NULL"),ChunkIndex);
				}
			}
			UE_LOG(LogPS4, Fatal, TEXT("Failed to allocate %11d bytes of direct memory, type %d"), HeapSize, (uint32)Type);
		}

		// entire chunk is free
		FreeList.AddHead(FFreeEntry((uint8*)HeapBase, HeapSize));		

		for (int32 i = 0; i < (int32)EPoolSizes::SizesCount; ++i)
		{
			KnownAvailablePool[i] = -1;
		}		
	}
	/**
	* Destructor
	*/
	virtual ~FGpuMallocChunk()
	{
		checkf(IsEmpty(),TEXT("Chunk memory not freed!"));

		BaseMemPoolMap.Empty();

		extern void* ReleaseLargeBlock(uint64 Size, off_t MemoryOffset);
		ReleaseLargeBlock(HeapSize, MemoryOffset);
	}

	/**
	 * Check free list for an entry big enough to fit the requested Size with Alignment
	 * @param Size - allocation size
	 * @param Alignment - allocation alignment
	 * @return true if available entry was found
	 */
	bool CanFitEntry(uint32 Size, uint32 Alignment)
	{
		bool bResult=false;

		if (Size < MaxPoolAllocSize)
		{
			EPoolSizes PoolType = GetPoolTypeForAlloc(Size, Alignment);
			FFreePool* FreePool = GetFreePool(PoolType);
			bResult = FreePool->CanFitEntry(Size, Alignment);
		}

		if (!bResult)
		{
			// look for a good free chunk
			TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* CurrentNode = FreeList.GetHead();
			while (CurrentNode)
			{
				FFreeEntry& EntryData = CurrentNode->GetValue();
				if (EntryData.CanFit(Size, Alignment))
				{
					bResult = true;
					break;
				}
				CurrentNode = CurrentNode->GetNextNode();
			}
		}
		return bResult;
	}
	/**
	* @return true if this chunk has no used memory
	*/
	bool IsEmpty()
	{
		return UsedMemorySize == 0;
	}

	bool AddressRangeContainsPtr(void* Ptr)
	{
		// check how far it is from our base
		int64 DistanceFromStart = (uint8*)Ptr - (uint8*)HeapBase;
		
		// make sure it's in this block
		return DistanceFromStart >= 0 && DistanceFromStart < HeapSize;
	}

	void SanityCheck()
	{
#if ENABLE_GPUMALLOC_SANITYCHECK
		uint64 TotalFree = 0;
		TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* CurrentNode = FreeList.GetHead();
		while (CurrentNode)
		{
			FFreeEntry& EntryData = CurrentNode->GetValue();
			TotalFree += EntryData.BlockSize;

			TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* NextNode = CurrentNode->GetNextNode();
			if (NextNode)
			{				
				FFreeEntry& NextEntryData = NextNode->GetValue();
				check((EntryData.Location + EntryData.BlockSize) <= NextEntryData.Location);				
			}

			CurrentNode = CurrentNode->GetNextNode();
		}
		checkf(TotalFree + UsedMemorySizePlusPadding == HeapSize, TEXT("Allocator losing track of memory. FreeBytes: 0x%x, UsedBytes: 0x%x, Sum: 0x%x, ActualSize: 0x%x"), TotalFree, UsedMemorySize, TotalFree + UsedMemorySize, HeapSize);
#endif
	}

	virtual void* Malloc(uint32 Size, uint32 Alignment) override
	{
		// multi-thread protection
		FScopeLock ScopeLock(&GGpuMallocCriticalSection);		

		if (Size <= MaxPoolAllocSize && Alignment <= MaxPoolAllocSize)
		{
			return MallocFromPool(Size, Alignment);
		}

		// Alignment here is assumed to be for location and size
		Alignment = FPlatformMath::Max<uint32>(Alignment, MIN_GPU_ALIGNMENT);
		const uint32 AlignedSize = Align<uint32>(Size, Alignment);		

		// Update stats.
		const uint32 WastedSize = AlignedSize - Size;
		TotalWaste += WastedSize;
		CurrentAllocs++;
		TotalAllocs++;

		SanityCheck();

		// look for a good free chunk	
		TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* CurrentNode = FreeList.GetHead();
		while (CurrentNode)
		{			
			FFreeEntry& EntryData = CurrentNode->GetValue();
			if (EntryData.CanFit(AlignedSize, Alignment))
			{
				// Use it, leaving over any unused space
				UsedMemorySize += AlignedSize;
				
				uint64 SizeWithPadding = 0;
				uint8* Ptr = EntryData.Split(AlignedSize, Alignment, PointerSizes, SizeWithPadding);
				UsedMemorySizePlusPadding += SizeWithPadding;
				if (EntryData.BlockSize == 0)
				{
					FreeList.RemoveNode(CurrentNode);
				}				
				return Ptr;
			}
			CurrentNode = CurrentNode->GetNextNode();
		}

		// if no suitable blocks were found, we must fail
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to allocate GPU memory (Size: %d)"), AlignedSize);
		return nullptr;
	}

	virtual bool Free(void* Ptr) override
	{
		if (Ptr == NULL)
		{
			return true;
		}

		// multi-thread protection
		FScopeLock ScopeLock(&GGpuMallocCriticalSection);

		void* PoolBaseAlignedPtr = (void*)((uint64)Ptr & PoolBaseAlignMask);
		uint64* ContainingPoolLoc = BaseMemPoolMap.Find(PoolBaseAlignedPtr);
		if (ContainingPoolLoc)
		{
			//only works as long as we never remove Pools
			uint32 PoolType = (*ContainingPoolLoc) >> 32;
			uint32 PoolIndex = (*ContainingPoolLoc) & 0xFFFFFFFF;

			FFreePool& ContainingPool = FixedSizeAllocPools[PoolType][PoolIndex];
			ContainingPool.Free(Ptr);
			return true;
		}


		// are we tracking this pointer?
		__uint128_t PointerSize = PointerSizes.FindRef(Ptr);
		if (PointerSize == 0)
		{
			check(false);
			return false;
		}
		uint64 Padding = PointerSize >> 64;
		uint64 Size = PointerSize & DECLARE_UINT64(0xFFFFFFFFFFFFFFFF);
		uint64 AllocationSize = Padding + Size;
		
		// no longer track this pointer
		PointerSizes.Remove(Ptr);

		UsedMemorySize -= Size;
		UsedMemorySizePlusPadding -= AllocationSize;
		CurrentAllocs--;

		// Search for where a place to insert a new free entry.		
		TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* EntryAfterPtr = FreeList.GetHead();
		while (EntryAfterPtr && Ptr > EntryAfterPtr->GetValue().Location)
		{			
			EntryAfterPtr = EntryAfterPtr->GetNextNode();
		}
		if (EntryAfterPtr == FreeList.GetHead())
		{
			EntryAfterPtr = nullptr;
		}

		// Are we right after the previous free entry?
		if (EntryAfterPtr && (EntryAfterPtr->GetValue().Location + EntryAfterPtr->GetValue().BlockSize + Padding) == (uint8*)Ptr)
		{
			check(false); //how?
			return true;
		}

		// Are we right before this free entry?  If so, merge as many free blocks together as possible.
		if (EntryAfterPtr && (((uint8*)Ptr + Size) == EntryAfterPtr->GetValue().Location))
		{
			//first, merge in our newly freed memory with the following free block.
			FFreeEntry& EntryAfterPtrData = EntryAfterPtr->GetValue();
			EntryAfterPtrData.Location -= AllocationSize;			
			EntryAfterPtrData.BlockSize += AllocationSize;

			//next, merge as many blocks together as we can.  Blocks might be touching now that we've merged in some more freed memory.
			TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* MiddleEntryNode = EntryAfterPtr;
			TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* AfterEntryNode = nullptr;
			TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* BeforeEntryNode = nullptr;
			EntryAfterPtr = nullptr;

			SanityCheck();

			bool bTryMerge = true;
			while (1)
			{
				TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* LeftMergeNode = nullptr;
				TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* RightMergeNode = nullptr;

				BeforeEntryNode = MiddleEntryNode->GetPrevNode();
				AfterEntryNode = MiddleEntryNode->GetNextNode();
				bool bCanMerge = false;

				FFreeEntry& MiddleEntryData = MiddleEntryNode->GetValue();
				if (BeforeEntryNode)
				{
					FFreeEntry& BeforeEntryData = BeforeEntryNode->GetValue();
					bCanMerge = (BeforeEntryData.Location + BeforeEntryData.BlockSize) == MiddleEntryData.Location;
					if (bCanMerge)
					{
						LeftMergeNode = BeforeEntryNode;
						RightMergeNode = MiddleEntryNode;
					}
				}

				if (!bCanMerge && AfterEntryNode)
				{
					FFreeEntry& AfterEntryData = AfterEntryNode->GetValue();
					bCanMerge = (MiddleEntryData.Location + MiddleEntryData.BlockSize) == AfterEntryData.Location;
					if (bCanMerge)
					{
						LeftMergeNode = MiddleEntryNode;
						RightMergeNode = AfterEntryNode;						
					}
				}

				if (LeftMergeNode && RightMergeNode)
				{
					MergeNodes(LeftMergeNode, RightMergeNode);
					MiddleEntryNode = LeftMergeNode;
					AfterEntryNode = nullptr;
					BeforeEntryNode = nullptr;				
				}
				else
				{
					break;
				}
			}			
			return true;
		}

		// Insert a new entry.
		FFreeEntry NewFreeEntry(((uint8*)Ptr) - Padding, AllocationSize);
		if (EntryAfterPtr)
		{
			FreeList.InsertNode(NewFreeEntry, EntryAfterPtr);
		}
		else
		{
			FreeList.AddHead(NewFreeEntry);
		}

		SanityCheck();
				
		return true;
	}

	virtual void GetAllocationInfo(uint64& Used, uint64& Free) override
	{
		// @todo: Validate this accounts for alignment padding in the right way
		Used = UsedMemorySize;
		Free = HeapSize - UsedMemorySize;
	}

	// needed to access FGpuMallocChunk members from FGrowableAllocator
	friend class FGrowableAllocator;

	void* MallocReservedPool()
	{
		if (ReservedPoolAllocs.Num() == 0)
		{
			for (int32 i = 0; i < NumPoolAllocsReserved; ++i)
			{
				void* PoolMem = FGpuMallocChunk::Malloc(MaxPoolBytes, PoolBaseAlignment);
				ReservedPoolAllocs.Add(PoolMem);
				FreeReservedPoolAllocs.Add(PoolMem);
			}
		}

		if (FreeReservedPoolAllocs.Num() > 0)
		{
			return FreeReservedPoolAllocs.Pop(false);
		}
		return nullptr;
	}

	void FreeReservedPool(void* PoolMem)
	{
		check(ReservedPoolAllocs.Contains(PoolMem));
		FreeReservedPoolAllocs.Add(PoolMem);
	}

	virtual void DumpInfo() override
	{
		uint32 MaxFreeBlock = 0;
		for (auto Node = FreeList.GetHead(); Node; Node = Node->GetNextNode())
		{
			MaxFreeBlock = FMath::Max(Node->GetValue().BlockSize, MaxFreeBlock);
		}
		const float KBDivisor = 1024.0f;
		const float TotalSizeKB = (float)HeapSize / KBDivisor;
		const float UsedSizeKB = (float)UsedMemorySizePlusPadding / KBDivisor;
		const float PaddingSizeKB = (float)(UsedMemorySizePlusPadding - UsedMemorySize) / KBDivisor;
		const float TotalFreeKB = TotalSizeKB - UsedSizeKB;
		const float MaxFreeBlockKB = (float)MaxFreeBlock / KBDivisor;

		uint64 AllPoolMem = 0;
		uint64 AllUsedPoolMem = 0;
		uint64 AllFreePoolMem = 0;

		for (int32 i = 0; i < (int32)EPoolSizes::SizesCount; ++i)
		{
			uint64 TotalPoolMem = 0;
			uint64 TotalUsedPoolMem = 0;
			uint64 TotalFreePoolMem = 0;

			const TArray<FFreePool>& FreePools = FixedSizeAllocPools[i];
			for (int32 PoolIndex = 0; PoolIndex < FreePools.Num(); ++PoolIndex)
			{
				const FFreePool& FreePool = FreePools[PoolIndex];
				const uint64 FreePoolMem = FreePool.AvailableAllocs.Num() * PoolSizes[i];
				const uint64 UsedPoolMem = FreePool.TotalPoolBytes - FreePoolMem;
				TotalPoolMem += FreePool.TotalPoolBytes;
				TotalUsedPoolMem += UsedPoolMem;
				TotalFreePoolMem += FreePoolMem;
			}

			AllPoolMem += TotalPoolMem;
			AllUsedPoolMem += TotalUsedPoolMem;
			AllFreePoolMem += TotalFreePoolMem;
			UE_LOG(LogPS4, Log, TEXT("FreePool %i bytes: TotalSize: %iKB, TotalUsed: %iKB, TotalFree: %iKB"), PoolSizes[i], (TotalPoolMem / 1024), (TotalUsedPoolMem / 1024), (TotalFreePoolMem / 1024));
		}
		const uint64 FreeReservedPoolAllocSize = (FreeReservedPoolAllocs.Num() * MaxPoolBytes);
		UE_LOG(LogPS4, Log, TEXT("AllPools: TotalSize: %iKB, TotalUsed: %iKB, TotalFree: %iKB, FreeReservedPools: %iKB"), AllPoolMem / 1024, AllUsedPoolMem / 1024, AllFreePoolMem / 1024, FreeReservedPoolAllocSize / 1024);

		UE_LOG(LogPS4, Log, TEXT("FGpuMallocChunk: Base: 0x%p, Size: %.2fKB, UsedwPadding: %.2fKB, Padding: %.2fKB, TotalFreeLoose: %.2fKB, TotalFree: %.2fKB, NumFreeBlocks: %i, LargestFreeBlock: %.2fKB"), HeapBase, TotalSizeKB, UsedSizeKB, PaddingSizeKB, TotalFreeKB, TotalFreeKB + (AllFreePoolMem / 1024), FreeList.Num(), MaxFreeBlockKB);
		UE_LOG(LogPS4, Log, TEXT("FreeBlocks:"));
		for (auto Node = FreeList.GetHead(); Node; Node = Node->GetNextNode())
		{
			const FFreeEntry& Entry = Node->GetValue();
			UE_LOG(LogPS4, Log, TEXT("Ptr: 0x%p, Size: %.2fKB"), Entry.Location, (float)Entry.BlockSize / KBDivisor);
		}
	}

protected:

	EPoolSizes GetPoolTypeForAlloc(uint32 Size, uint32 Alignment)
	{
		EPoolSizes PoolSize = EPoolSizes::SizesCount;
		for (int32 i = 0; i < (int32)EPoolSizes::SizesCount; ++i)
		{
			if (PoolSizes[i] >= Size)
			{
				PoolSize = (EPoolSizes)i;
				break;
			}
		}
		return PoolSize;
	}

	void* MallocFromPool(uint32 Size, uint32 Alignment)
	{
		SanityCheck();
		EPoolSizes PoolSize = GetPoolTypeForAlloc(Size, Alignment);
		check(PoolSize != EPoolSizes::SizesCount);
		check(Alignment <= PoolSizes[(int32)PoolSize]);

		FFreePool* Pool = GetFreePool(PoolSize);
		void* MallocMem = Pool->Malloc(Size, Alignment);

		if (!Pool->CanFitEntry(Size, Alignment))
		{
			UpdateKnownFree(PoolSize);
		}

		check(AddressRangeContainsPtr(MallocMem));
		SanityCheck();
		return MallocMem;
	}

	FFreePool* AddPool(EPoolSizes PoolType)
	{
		SanityCheck();
		int32 PoolAllocSize = PoolSizes[(int32)PoolType];		
		uint64 NewPoolIndex = FixedSizeAllocPools[(int32)PoolType].Emplace(this, PoolAllocSize);

		SanityCheck();

		FFreePool& NewPool = FixedSizeAllocPools[(int32)PoolType][NewPoolIndex];

		uint64 PoolTypeIndex = (uint64)PoolType;
		uint64 PoolMapEntry = (PoolTypeIndex << 32) | NewPoolIndex;
		BaseMemPoolMap.Add(NewPool.PoolBase, PoolMapEntry);

		SanityCheck();
		return &NewPool;
	}

	FFreePool* GetFreePool(EPoolSizes PoolType)
	{
		FFreePool* FreePool = nullptr;
		int32 KnownFreePool = KnownAvailablePool[(int32)PoolType];
		if (KnownFreePool != -1)
		{
			FreePool = &FixedSizeAllocPools[(int32)PoolType][KnownFreePool];
			check(FreePool->AvailableAllocs.Num() > 0);			
		}
		else
		{
			FreePool = AddPool(PoolType);
			UpdateKnownFree(PoolType);
		}
		check(FreePool);
		return FreePool;
	}

	void UpdateKnownFree(EPoolSizes PoolType)
	{
		TArray<FFreePool>& FreePools = FixedSizeAllocPools[(int32)PoolType];

		int32 KnownFreePool = -1;
		for (int32 i = 0; i < FreePools.Num(); ++i)
		{
			if (FreePools[i].AvailableAllocs.Num() > 0)
			{
				KnownFreePool = i;
				break;
			}
		}
		KnownAvailablePool[(int32)PoolType] = KnownFreePool;
	}

	void MergeNodes(TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* LeftNode, TDoubleLinkedList<FFreeEntry>::TDoubleLinkedListNode* RightNode)
	{
		SanityCheck();

		check(LeftNode);
		check(RightNode);

		FFreeEntry& LeftData = LeftNode->GetValue();
		FFreeEntry& RightData = RightNode->GetValue();
		check((LeftData.Location + LeftData.BlockSize) == RightData.Location);

		LeftData.BlockSize += RightData.BlockSize;
		FreeList.RemoveNode(RightNode);

		SanityCheck();
	}

	/** Start of the heap */
	void* HeapBase;

	/** Size of the heap */
	uint64 HeapSize;

	/** Offset of the allocated direct memory */
	off_t MemoryOffset;

	/** Size of used memory */
	uint64 UsedMemorySize;
	uint64 UsedMemorySizePlusPadding;

	/** List of free blocks */
	TDoubleLinkedList<FFreeEntry> FreeList;
	
	TArray<FFreePool> FixedSizeAllocPools[(int32)EPoolSizes::SizesCount];

	//maps a base pointer to an OR'd set of indices (PoolType, PoolIndex). Only functions as long as Pools are not rearranged. 
	//Can't store a pointer to the Pool because adding more pools will cause a TArray re-alloc and invalidate them.
	TMap<void*, uint64> BaseMemPoolMap;
	int32 KnownAvailablePool[(int32)EPoolSizes::SizesCount];

	/** Mapping if the pointer and how big it was */
	TMap<void*, __uint128_t> PointerSizes;

	/** Small array of pre-allocated pool-sized chunks to reduce pool alignment overhead if pool usage remains at a standard size. */
	TArray<void*> ReservedPoolAllocs;
	TArray<void*> FreeReservedPoolAllocs;

	friend class FFreePool;
};

constexpr uint32 FGpuMallocChunk::PoolSizes[(int32)FGpuMallocChunk::EPoolSizes::SizesCount];

/**
* Constructor
* Internally allocates address space for use only by this allocator
*
* @param InMinSizeAllowed - min reserved space by this allocator
* @param InMaxSizeAllowed - total size of allocations won't exceed this limit
* @param InPageSize - physical memory allocs are done in increments of this page size
*/
FGrowableAllocator::FGrowableAllocator(SceKernelMemoryType InType, FPlatformMemory::EMemoryCounterRegion InStatRegion, uint64 InMinSizeAllowed, uint64 InMaxSizeAllowed)
	: MaxSizeAllowed(Align<uint64>(InMaxSizeAllowed, GPU_CHUNK_ALIGNMENT))
	, MinSizeAllowed(Align<uint64>(InMinSizeAllowed, GPU_CHUNK_ALIGNMENT))
	, MemoryType(InType)
	, StatRegion(InStatRegion)
{
	static_assert(MAX_SIZE_GPU_ALLOC_POOL == FGpuMallocChunk::PoolSizes[(int32)FGpuMallocChunk::EPoolSizes::SizesCount - 1], "MAX_SIZE_GPU_ALLOC_POOL needs to be updated to match the actual pool max");
	if (InType == SCE_KERNEL_WC_GARLIC)
	{
		LLM_PLATFORM_SCOPE_PS4(ELLMTagPS4::GarlicHeap);
		extern uint64 GGarlicHeapSize;
		CreateAllocChunk(GGarlicHeapSize);
	}
	else
	{
		LLM_PLATFORM_SCOPE_PS4(ELLMTagPS4::OnionHeap);
		extern uint64 GOnionHeapSize;
		CreateAllocChunk(GOnionHeapSize);
	}
}

/**
 * Destructor
 */
FGrowableAllocator::~FGrowableAllocator()
{
	// remove any existing chunks
	for (int32 Index=0; Index < AllocChunks.Num(); Index++)
	{
		FGpuMallocChunk* Chunk = AllocChunks[Index];
		if( Chunk )
		{
			RemoveAllocChunk(Chunk);
		}
	}
}


/**
* Create a new allocation chunk to fit the requested size. All chunks are aligned to GPU_CHUNK_ALIGNMENT
*
* @param Size - size of chunk
*/
FGpuMallocChunk* FGrowableAllocator::CreateAllocChunk(uint64 Size)
{
	FGpuMallocChunk* NewChunk = new FGpuMallocChunk(this, Align<uint64>(Size, GPU_CHUNK_ALIGNMENT), MemoryType);

	// add a new entry to list (reusing any empty slots)
	int32 EmptyEntryIdx = AllocChunks.Find(NULL);
	if (EmptyEntryIdx == INDEX_NONE)
	{
		EmptyEntryIdx = AllocChunks.Add(NewChunk);
	}
	else
	{
		AllocChunks[EmptyEntryIdx] = NewChunk;
	}
	
//	uint64 Used, Free;
//	GetAllocationInfo(Used, Free);
//	UE_LOG(LogPS4, Display, TEXT("Created chunk, stats are: %.2f Used, %.2f Free"), (double)Used / (1024.0 * 1024.0), (double)Free / (1024.0 * 1024.0));

#if DEBUG_LOGS
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FGrowableAllocator::CreateAllocChunk  %f KB %d B"),NewChunk->HeapSize/1024.f,NewChunk->HeapSize);
#endif

#if STATS
	UpdateMemoryStatMaxSizes();
#endif

	return NewChunk;
}

/**
 * Removes an existing allocated chunk. Unmaps its memory, decommits physical memory back to OS,
 * flushes address entries associated with it, and deletes it
 *
 * @param Chunk - existing chunk to remove
 */
void FGrowableAllocator::RemoveAllocChunk(FGpuMallocChunk* Chunk)
{
#if DEBUG_LOGS
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FGrowableAllocator::RemoveAllocChunk  %f KB %d B"),Chunk->HeapSize/1024.f,Chunk->HeapSize);
#endif

	checkSlow(Chunk);
	
	// remove entry
	int32 FoundIdx = AllocChunks.Find(Chunk);
	check(AllocChunks.IsValidIndex(FoundIdx));
	AllocChunks[FoundIdx] = NULL;			
	delete Chunk;	

//	uint64 Used, Free;
//	GetAllocationInfo(Used, Free);
//	UE_LOG(LogPS4, Display, TEXT("Deleted chunk, stats are: %.2f Used, %.2f Free"), (double)Used / (1024.0 * 1024.0), (double)Free / (1024.0 * 1024.0));

#if STATS
	UpdateMemoryStatMaxSizes();
#endif
}

void FGrowableAllocator::UpdateMemoryStatMaxSizes()
{
}

void FGrowableAllocator::OutOfMemory( uint32 Size )
{
#if !UE_BUILD_SHIPPING
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FGrowableAllocator: OOM allocating %dbytes %fMB"), Size, Size/1024.0f/1024.0f );
	UE_LOG(LogPS4, Fatal, TEXT("FGrowableAllocator: OOM allocating %dbytes %fMB"), Size, Size/1024.0f/1024.0f );
 #endif
}

void* FGrowableAllocator::Malloc(uint32 Size, uint32 Alignment)
{
	// multi-thread protection
	FScopeLock ScopeLock(&CriticalSection);

	checkSlow(Alignment <= GPU_CHUNK_ALIGNMENT);

	void* Result = NULL;
	FGpuMallocChunk* AvailableChunk = NULL;
	
	// align the size to match what Malloc does below
	const uint32 AlignedSize = Align<uint32>(Size, Alignment);

	// Update stats.
	const uint32 WastedSize = AlignedSize - Size;
	TotalWaste += WastedSize;
	CurrentAllocs++;
	TotalAllocs++;

	// search for an existing alloc chunk with enough space
	for (int32 ChunkIndex=0; ChunkIndex < AllocChunks.Num(); ChunkIndex++)
	{
		FGpuMallocChunk* Chunk = AllocChunks[ChunkIndex];
		if (Chunk && Chunk->CanFitEntry(AlignedSize,Alignment))
		{
			AvailableChunk = Chunk;
			break;			
		}
	}

	// create a new chunk with enough space + alignment to GPU_CHUNK_ALIGNMENT and allocate out of it
	if (AvailableChunk == NULL)
	{	
		AvailableChunk = CreateAllocChunk(AlignedSize);
	}

	// allocate from the space in the chunk
	if (AvailableChunk)
	{		
		Result = AvailableChunk->Malloc(AlignedSize, Alignment);		
	}

	if (AvailableChunk == NULL || Result == NULL)
	{
		OutOfMemory(AlignedSize);	
	}

#if DEBUG_LOGS
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FGrowableAllocator::Malloc  %f KB %d B"),AlignedSize/1024.f,AlignedSize);
#endif
	
	return Result;
}

/** 
* Free memory previously allocated from this allocator
*
* @param Ptr - memory to free
*/
bool FGrowableAllocator::Free(void* Ptr)
{
	// multi-thread protection
	FScopeLock ScopeLock(&CriticalSection);

	// starting address space idx used by the chunk containing the allocation
	for (int32 ChunkIndex = 0; ChunkIndex < AllocChunks.Num(); ChunkIndex++)
	{
		FGpuMallocChunk* Chunk = AllocChunks[ChunkIndex];
		if (Chunk && Chunk->AddressRangeContainsPtr(Ptr))
		{
			// free space in the chunk
			if (Chunk->Free(Ptr))
			{
				CurrentAllocs--;
				if (Chunk->IsEmpty())
				{
					// if empty then unmap and decommit physical memory
					RemoveAllocChunk(Chunk);
				}

				// return success
				return true;
			}
		}
	}

	// if we got here, we failed to free the pointer
	UE_LOG(LogPS4, Fatal, TEXT("Tried to free invalid pointer"));
	return false;
}

void FGrowableAllocator::GetAllocationInfo(uint64& Used, uint64& Free)
{
	// multi-thread protection
	FScopeLock ScopeLock(&CriticalSection);

	Used = 0;
	Free = 0;
	// pass off to individual alloc chunks
	for (int32 ChunkIndex=0; ChunkIndex < AllocChunks.Num(); ChunkIndex++)
	{
		FGpuMallocChunk* Chunk = AllocChunks[ChunkIndex];
		if( Chunk )
		{
			uint64 ChunkUsed=0;
			uint64 ChunkFree=0;
			Chunk->GetAllocationInfo(ChunkUsed,ChunkFree);
			Used += ChunkUsed;
			Free += ChunkFree;
		}
	}
}

void FGrowableAllocator::DumpInfo()
{
	UE_LOG(LogPS4, Log, TEXT("Growable Allocator memory type: %i"), MemoryType);

	for (int32 ChunkIndex = 0; ChunkIndex < AllocChunks.Num(); ChunkIndex++)
	{
		FGpuMallocChunk* Chunk = AllocChunks[ChunkIndex];
		if (Chunk)
		{
			Chunk->DumpInfo();
		}
	}
}

#endif // USE_NEW_PS4_MEMORY_SYSTEM == 0
