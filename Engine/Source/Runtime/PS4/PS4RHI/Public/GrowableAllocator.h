// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GrowableAllocator.h: Memory allocator that allocates direct memory for GPU access (Onion or Garlic)
=============================================================================*/

#pragma once

#if USE_NEW_PS4_MEMORY_SYSTEM == 0

#include "CoreMinimal.h"

/**
 * Base class for all Onion/Garlic memory allocators
 */
class FGpuMallocBase
{
public:
	/** Constructor */
	FGpuMallocBase()
		: TotalWaste(0)
		, CurrentAllocs(0)
		, TotalAllocs(0)
	{}

	virtual void* Malloc(uint32 Size, uint32 Alignment)=0;
	virtual bool Free(void* Ptr)=0;
	virtual void GetAllocationInfo(uint64& Used, uint64& Free)=0;

	/**
	 *  Prints usage and fragmentation information to the log.
	 */
	virtual void DumpInfo()
	{
	}
//	virtual void MapMemoryToGPU()=0;

	/** 
	 * Returns approximated amount of memory wasted due to allocations' alignment. 
	 */
	virtual uint64 GetWasteApproximation()
	{
		double Waste = ((double)TotalWaste / (double)TotalAllocs) * CurrentAllocs;
		return (uint64)Waste;
	}

protected:

	/** The total amount of memory wasted due to allocations' alignment. */
	uint64			TotalWaste;

	/** The current number of allocations. */
	uint64			CurrentAllocs;

	/** The total number of allocations. */
	uint64			TotalAllocs;
};

/**
 * Allocator that will grow as needed with direct mapped memory for a given memory type
 */
class FGrowableAllocator : public FGpuMallocBase
{
public:
	
	/**
	 * Constructor
	 * Internally allocates address space for use only by this allocator
	 *
	 * @param Type - The kernel memory type to allocate with this allocator (onion, garlic, etc)
	 * @param StatRegion - The region of memory this is responsible for, for updating the region max sizes
	 * @param InMinSizeAllowed - min reserved space by this allocator
	 * @param InMaxSizeAllowed - total size of allocations won't exceed this limit
	 */
	FGrowableAllocator(SceKernelMemoryType Type, FPlatformMemory::EMemoryCounterRegion StatRegion, uint64 InMinSizeAllowed=0, uint64 InMaxSizeAllowed=SCE_KERNEL_MAIN_DMEM_SIZE);
	
	/**
	* Destructor
	*/
	virtual ~FGrowableAllocator();

	// FMallocGcmBase interface

	virtual void* Malloc(uint32 Size, uint32 Alignment) override;
	virtual bool Free(void* Ptr) override;
	virtual void GetAllocationInfo(uint64& Used, uint64& Free) override;

	virtual void DumpInfo() override;

private:

	friend class FGpuMallocChunk;

	/** base for the address space used by this alloc */
	uint8* AddressBase;
	/** largest total allocation allowed */
	const uint64 MaxSizeAllowed;
	/** minimum reserved space initially allocated */
	const uint64 MinSizeAllowed;
	/** total currently allocated from OS */
	uint64 CurSizeAllocated;
	/** Shared critical section object to protect from use on multiple threads */
	static FCriticalSection CriticalSection;	

	/** The type of memory this allocator allocates from the kernel */
	SceKernelMemoryType MemoryType;

	/** The stat memory region to update max size */
	FPlatformMemory::EMemoryCounterRegion StatRegion;

	/** list of currently allocated chunks */
	TArray<class FGpuMallocChunk*> AllocChunks;

	/** Updates the memory stat max sizes when chunks are added/removed */
	void UpdateMemoryStatMaxSizes();

	/** 
	* Find a contiguous space in the address space for this allocator
	*
	* @param SizeNeeded - total contiguous memory needed 
	* @param OutAddressBase - [out] pointer to available space in address space
	* @param OutAddressSize - [out] avaialbe space in address space >= SizeNeeded
	* @return true if success
	*/
	bool GetAvailableAddressSpace(uint8*& OutAddressBase, uint64& OutAddressSize, uint64 SizeNeeded);

	/**
	* Create a new allocation chunk to fit the requested size. All chunks are aligned to MIN_CHUNK_ALIGNMENT
	*
	* @param Size - size of chunk
	*/
	class FGpuMallocChunk* CreateAllocChunk(uint64 Size);

	/**
	* Removes an existing allocated chunk. Unmaps its memory, decommits physical memory back to OS,
	* flushes address entries associated with it, and deletes it
	*
	* @param Chunk - existing chunk to remove
	*/
	void RemoveAllocChunk(class FGpuMallocChunk* Chunk);

	/** triggered during out of memory failure for this allocator */
	FORCENOINLINE void OutOfMemory( uint32 Size );
};

#endif // USE_NEW_PS4_MEMORY_SYSTEM == 0
