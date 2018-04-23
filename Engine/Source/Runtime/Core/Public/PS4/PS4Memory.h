// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformMemory.h"
#include "CoreTypes.h"
#include "PS4/PS4SystemIncludes.h"

class FOutputDevice;

extern volatile int64 GPS4Allocated_MemBlockOnion;  // Onion allocations made directly from the FMemBlock API
extern volatile int64 GPS4Allocated_MemBlockGarlic; // Garlic allocations made directly from the FMemBlock API (excluding the initial FMemBlock allocation for the defrag heap).

/**
 *	PS4 implementation of the FGenericPlatformMemoryStats.
 */
struct FPlatformMemoryStats
	: public FGenericPlatformMemoryStats
{
	/** Default constructor, clears all variables. */
	FPlatformMemoryStats()
		: FGenericPlatformMemoryStats()
		, Direct(0)
		, Garlic(0)
		, Onion(0)
		, Flexible(0)
		, FlexibleHeapSize(0)
#if USE_NEW_PS4_MEMORY_SYSTEM
		, FrameBuffer(0)
		, FrameBufferHeapSize(0)
		, MemBlockTotal(0)
		, MemBlockUsed(0)
		, MemBlockWasted(0)
#else
		, DirectHeapSize(0)
		, GarlicHeapSize(0)
		, OnionHeapSize(0)
#endif
#if USE_DEFRAG_ALLOCATOR
		, DefragHeapSize(0)
		, DefragUsed(0)
		, DefragFree(0)
		, DefragWasted(0)
#endif
	{ }

	SIZE_T Direct;
	SIZE_T Garlic;
	SIZE_T Onion;
	SIZE_T Flexible;
	SIZE_T FlexibleHeapSize;

#if USE_NEW_PS4_MEMORY_SYSTEM
	SIZE_T FrameBuffer;
	SIZE_T FrameBufferHeapSize;
	SIZE_T MemBlockTotal;
	SIZE_T MemBlockUsed;
	SIZE_T MemBlockWasted;
#else
	SIZE_T DirectHeapSize;
	SIZE_T GarlicHeapSize;
	SIZE_T OnionHeapSize;
#endif

#if USE_DEFRAG_ALLOCATOR
	SIZE_T DefragHeapSize;
	SIZE_T DefragUsed;
	SIZE_T DefragFree;
	SIZE_T DefragWasted;
#endif
};

//callback for dumping gpu mem stats on fatal memory errors.
typedef void (*GPUMemStatDump)(void);

/**
 * PS4 implementation of the memory OS functions.
 */
struct CORE_API FPS4PlatformMemory
	: public FGenericPlatformMemory
{
	enum EMemoryCounterRegion
	{
		/** Not memory. */
		MCR_Invalid,

		/** Main system memory. */
		MCR_Physical,		

		/** memory directly a GPU (graphics card, etc). */
		MCR_GPU,

		/** Presized texture pools. */
		MCR_TexturePool,

		/** Amount of texture pool available for streaming. */
		MCR_StreamingPool, 

		/** Amount of texture pool used for streaming. */
		MCR_UsedStreamingPool, 

		/** presized pool of memory that can be defragmented. */
		MCR_GPUDefragPool, 

		/** System memory directly accessible by a GPU. */
		MCR_GPUSystem,

		/** Total physical memory including CPU and GPU */
		MCR_PhysicalLLM,

#if USE_NEW_PS4_MEMORY_SYSTEM
		/** PS4 specific flexible memory pool. Memory in this pool is allocated when direct memory is exhausted. */
		MCR_Flexible,

		/** PS4 specific frame buffer pool. Memory in this pool is both physically and virtually contiguous. */
		MCR_FrameBuffer,
#endif // USE_NEW_PS4_MEMORY_SYSTEM

		MCR_MAX
	};

	static GPUMemStatDump GPUStatDumpFunc;

public:

	// FGenericPlatformMemory interface

	static void Init();
	static uint32 GetBackMemoryPoolSize()
	{
		// @todo ps4: What's a good size here?
		return 8 * 1024 * 1024;
	}
	static class FMalloc* BaseAllocator();
	static const FPlatformMemoryConstants& GetConstants();
	static FPlatformMemoryStats GetStats();
	static void DumpStats(FOutputDevice& Ar);
	static void OnOutOfMemory(uint64 Size, uint32 Alignment);
	static void* BinnedAllocFromOS( SIZE_T Size );
	static void BinnedFreeToOS( void* Ptr, SIZE_T Size );

	static void RegisterGPUMemStatDump(GPUMemStatDump InGPUStatDumpFunc)
	{
		GPUStatDumpFunc = InGPUStatDumpFunc;
	}

	static void DumpGPUMemoryStats()
	{
		if (GPUStatDumpFunc != nullptr)
		{
			GPUStatDumpFunc();
		}
	}

	static FORCEINLINE void* Memcpy(void* RESTRICT Dest, const void* RESTRICT Src, SIZE_T Count)
	{
		if(Count <= 64)
		{
			uint8* RESTRICT DestAddr = (uint8* RESTRICT)Dest;
			uint8* RESTRICT SrcAddr = (uint8* RESTRICT)Src;
			if (Count >= 16)
			{
				while (Count >= 32)
				{
					((uint64*)DestAddr)[0] = ((uint64*)SrcAddr)[0];
					((uint64*)DestAddr)[1] = ((uint64*)SrcAddr)[1];
					((uint64*)DestAddr)[2] = ((uint64*)SrcAddr)[2];
					((uint64*)DestAddr)[3] = ((uint64*)SrcAddr)[3];
					Count -= 32;
					DestAddr += 32;
					SrcAddr += 32;
				}
				while (Count >= 16)
				{
					((uint64*)DestAddr)[0] = ((uint64*)SrcAddr)[0];
					((uint64*)DestAddr)[1] = ((uint64*)SrcAddr)[1];
					Count -= 16;
					DestAddr += 16;
					SrcAddr += 16;
				}
			}
			while (Count >= 4)
			{
				((uint32*)DestAddr)[0] = ((uint32*)SrcAddr)[0];
				Count -= 4;
				DestAddr += 4;
				SrcAddr += 4;
			}
			while (Count >= 1)
			{
				((uint8*)DestAddr)[0] = ((uint8*)SrcAddr)[0];
				Count--;
				DestAddr++;
				SrcAddr++;
			}
		}
		else
		{
			return memcpy(Dest, Src, Count);
		}

		return Dest;
	}

#if !UE_BUILD_SHIPPING	// This function is not guaranteed to be correct and should not be called in shipping builds
	static bool IsExtraDevelopmentMemoryAvailable();
#endif
	
	static bool GetLLMAllocFunctions(void*(*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment);

protected:

	friend struct FGenericStatsUpdater;	

	static void InternalUpdateStats(const FPlatformMemoryStats& MemoryStats);
};

// Memory allocator callbacks used by the PS4 sceAvPlayer library
namespace PS4MediaAllocators
{
	extern void* Allocate(void* Jumpback, uint32 Alignment, uint32 Size);
	extern void* AllocateTexture(void* Jumpback, uint32 Alignment, uint32 Size);

	extern void Deallocate(void* Jumpback, void* Mem);
	extern void DeallocateTexture(void* Jumpback, void* Memory);
}

typedef FPS4PlatformMemory FPlatformMemory;
