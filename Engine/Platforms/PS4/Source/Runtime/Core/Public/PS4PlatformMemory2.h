// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PS4PlatformMemory.h"

#if USE_NEW_PS4_MEMORY_SYSTEM

// The memory regions
enum class EGnmMemType
{
	GnmMem_GPU = 0,
	GnmMem_CPU = 1,
	GnmMem_FrameBuffer = 2,
	GnmMem_OnionDirect = 3,

	// track the number of memory types
	GnmMem_MaxNumTypes = 4,

	// used for invalid memblocks (will never be freed)
	GnmMem_Invalid = 4,
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
	// TODO: Optimize / Pack this

	/** CPU/GPU addressable pointer. */
	void* Pointer;

	/** Original requested size of the memory block allocation. */
	uint32 Size;

	/** Actual size of the allocation, due to memory alignment / small block allocator. */
	uint32 AlignedSize;

	/** When allocated from a small block, a pointer to the small block page which did the allocating. */
	class FGPUMemoryPage* SmallBlockPage;

	/** Which allocator was used to allocate this block */
	EGnmMemType MemType;

	/** Render thread fence value (Logic is managed in GnmManager) */
	static uint64 RTFenceValue;

	/**	RHI thread fence value (Logic is managed in GnmManager) */
	static uint64 RHITFenceValue;

	/**
	 * Allocate memory in the given region
	 */
	static FMemBlock Allocate(uint32 Size, uint32 Alignment, EGnmMemType Type, struct TStatId Stat);

	/**
	 * Free the given memory
	 */
	static void Free(FMemBlock Mem);

	/**
	 * Process one frame of outstanding delayed frees
	 */
	static void ProcessDelayedFrees(uint64 FenceValue);

#if DO_CHECK
	/**
	 * Returns true if there are mem blocks in the pending free list.
	 */
	static bool HasPendingFrees();
#endif

	/**
	 * Prints memory allocation info 
	 */
	static void Dump();

	/**
	 * Dump tracked deleted memory (that check if memory has been deleted while still in use by GPU)
	 * @param Ar - The output device
	 */
	static void DumpDeletedMemory(FOutputDevice& Ar, uint64 GPUFenceValue);

	/**
	 * Returns memory allocation info
	 */
	static FGnmMemoryStat GetStats();

	/**
	 * NULL memory
	 */
	FMemBlock()
		: Pointer(nullptr)
		, Size(0)
		, AlignedSize(0)
		, SmallBlockPage(nullptr)
		, MemType(EGnmMemType::GnmMem_Invalid)
	{}

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

#endif // USE_NEW_PS4_MEMORY_SYSTEM
