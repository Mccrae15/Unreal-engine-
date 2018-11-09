// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GnmRHI.h: Public Gnm RHI definitions.
=============================================================================*/

#pragma once 

#if USE_NEW_PS4_MEMORY_SYSTEM

	#define TRACK_GNM_MEMORY_USAGE 0

	#include "PS4/PS4Memory2.h"

#else

#define TRACK_GNM_MEMORY_USAGE (STATS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST)
// The memory regions
enum class EGnmMemType
{
	GnmMem_GPU,
	GnmMem_CPU,
	// if any types are added, see top of GnmMemory.cpp for the matching array

	// track the number of memory types
	GnmMem_MaxNumTypes,

	// used for invalid memblocks (will never be freed)
	GnmMem_Invalid,
};

// Statistics about GNM Memory
struct FGnmMemoryStat
{
	struct FGnmMemoryRegionStat
	{
		FGnmMemoryRegionStat()
			: HeapSize(0)
			, UsedSize(0)
		{}

		SIZE_T		HeapSize;
		SIZE_T		UsedSize;
	};

	FGnmMemoryRegionStat	GarlicMemory;
	FGnmMemoryRegionStat	OnionMemory;
};


struct FMemBlock
{
	/** An array of delay-initialized allocators */

public:
//private: // can't do this yet because the ps4 movie streamer is still broken
	static class FGrowableAllocator* Allocators[(int32)EGnmMemType::GnmMem_MaxNumTypes];
//public:
	/** Double buffered free lists so we delay free for when the GPU is done with the memory */
	static TArray<FMemBlock> DelayedFrees[2];

	/** Which of the double buffer we will store frees into */
	static uint32 CurrentFreeArray;

	/** Which allocator was used to allocate this block */
	EGnmMemType MemType;

	/** CPU/GPU addressable pointer */
	void* Pointer;

	/** Size of the memory block */
	uint32 Size;

#if TRACK_GNM_MEMORY_USAGE
	/** Tracks how much memory is allocated by tag (in each region type) */
	static TMap<FName, uint64> TaggedMemorySizes[(int32)EGnmMemType::GnmMem_MaxNumTypes];

	/** How much memory is available per type */
	static uint64 MaxMemorySize[(int32)EGnmMemType::GnmMem_MaxNumTypes];

	/** Remember the tag for this allocation */
	TStatId Stat;
#endif


	/**
	 * Allocate memory in the given region
	 */
	static FMemBlock Allocate(uint32 Size, uint32 Alignment, EGnmMemType Type, TStatId Stat, bool LlmTrackAlloc=true);

	/**
	 * Free the given memory
	 */
	static void Free(FMemBlock Mem);

	/**
	 * Process one frame of outstanding delayed frees
	 */
	static void ProcessDelayedFrees();

#if TRACK_GNM_MEMORY_USAGE
	/**
	 * Track GPU memory usage by region and tag
	 */
	static void TrackMemoryUsage(EGnmMemType Type, TStatId Stat, int64 ChangeInMemorySize);
#endif

	/**
	 * Prints memory allocation info 
	 */
	static void Dump();

	/**
	* Returns memory allocation info
	*/
	static FGnmMemoryStat GetStats();


	/**
	 * NULL memory
	 */
	FMemBlock()
		: MemType(EGnmMemType::GnmMem_Invalid)
		, Pointer(NULL)
		, Size(0)
	{
	}

	/**
	 * Initialize with a CPU addressable pointer (will fill out GPUAddr)
	 */
	FMemBlock(EGnmMemType MemType, void* InPtr, uint32 InSize, TStatId InStat);

	/**
	 * Initialize with another FMemBlock
	 */
	FMemBlock(const FMemBlock& Other);

	FORCEINLINE uint32 GetSize() const
	{
		return Size;
	}

	FORCEINLINE void* GetPointer()
	{
		return Pointer;
	}

	FORCEINLINE void const* GetPointer() const
	{
		return Pointer;
	}

	void SetTransientSize(uint32 InSize)
	{
		MemType = EGnmMemType::GnmMem_Invalid;
		Pointer = NULL;
		Size = InSize;
	}

	void OverrideSize(uint32 OverrideSize)
	{
		Size = OverrideSize;
	}

	void OverridePointer(void* InPointer)
	{
		Pointer = InPointer;
	}
};

#endif
