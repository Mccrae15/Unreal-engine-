// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GnmTempBlockAllocator.h: Class to manage fast single-frame lifetime GPU allocations for 
DCB and LCUE resource allocations.
=============================================================================*/
#pragma once
#include "GnmMemory.h"

#if !defined(PS4_SUPPORTS_PARALLEL_RHI_EXECUTE)
#error "PS4_SUPPORTS_PARALLEL_RHI_EXECUTE must be defined."
#endif

class FScopedTempBlockManagerLock;

// class to manage generic lists of pooled allocations of specific sizes.
// the lifetime for these allocations is a single frame.  Allocations are double buffered
// automatically so the GPU will read valid data.
class FGnmTempBlockManager
{
public:
	static const int32 NumBufferedFrames = 4;

	// pool sizes that we support
	enum class EBlockSize : uint8
	{
		E64k,		
		E512k,		
		E8MB,
		SizesCount,
	};

	constexpr static uint32 PoolSizes[(int32)EBlockSize::SizesCount] =
	{
		64 * 1024,		
		512 * 1024,
		8 * 1024 * 1024
	};

	static void Init();

	static FGnmTempBlockManager& Get();

	// Allocate block of gpu memory that's valid for a single frame.
	// Allocations do not need to be freed as they will all be reused from frame to frame.
	// Allocations are threadsafe.
	void* AllocateBlock(EGnmMemType MemType, EBlockSize BlockSize);

	// switches current memory arrays and resets allocations.
	void EndFrame();	

private:

	struct FTempMemBlock 
	{
		FTempMemBlock(uint32 InFrame, const FMemBlock& InBlock)
			: LastUsedFrame(InFrame)
			, MemBlock(InBlock)
		{
		}

		uint32 LastUsedFrame;
		FMemBlock MemBlock;
	};

	// frame-buffered arrays of memory blocks for use by FGnmTempBlockAllocator
	TArray<FTempMemBlock> Blocks[NumBufferedFrames][(int32)EGnmMemType::GnmMem_MaxNumTypes][(int32)EBlockSize::SizesCount];

	// which block to hand out next.  Allocates a new block if we've run out.
	uint32 CurrentBlockIndexArray[(int32)EGnmMemType::GnmMem_MaxNumTypes][(int32)EBlockSize::SizesCount];
	uint32 CurrentFrameArray;

	FCriticalSection BlockMutex;

	static FGnmTempBlockManager Manager;

	friend FScopedTempBlockManagerLock;
};

//FScopedGPUDefragLock can't cover any scope that will add dcb commands or we might deadlock with a master reserve failure.
class FScopedTempBlockManagerLock
{
public:
	FScopedTempBlockManagerLock(FGnmTempBlockManager& InBlockManager)
		: BlockManager(InBlockManager)
	{
		BlockManager.BlockMutex.Lock();
	}

	~FScopedTempBlockManagerLock()
	{
		BlockManager.BlockMutex.Unlock();
	}
private:
	FGnmTempBlockManager& BlockManager;
};

// Fast allocator for threaded temporary allocations.  Allocator is not threadsafe.  Each thread is expected to maintain
// its own allocator.  Backed by FGnmTempBlockManager which is threadsafe.
template<EGnmMemType TMemType, FGnmTempBlockManager::EBlockSize TBlockSizeName, uint32 TBlockSizeBytes = FGnmTempBlockManager::PoolSizes[(int32)TBlockSizeName] >
class FGnmTempBlockAllocator
{
public:

	FGnmTempBlockAllocator() :
		CurrentPtr(nullptr),
		CurrentOffset(0),
		CurrentBlock(0)
	{
	}	
	
	/*
	 * Allocates the given size from the current block or the next pre-existing block if the allocator was reset.
	 * If the there is not enough memory pre-allocated to service the request a new block is allocated from the BlockManager.
	 */
	void* Allocate(int32 Size)
	{
		check(Size > 0);

		void* AllocMem = nullptr;
		checkf(Size <= TBlockSizeBytes, TEXT("Allocation of %i is too large for pool size: %i"), Size, TBlockSizeBytes);
		const int32 Remaining = TBlockSizeBytes - CurrentOffset;
		if (!CurrentPtr || Remaining < Size)
		{
			//if the allocator has been cleared of some previous usage, reuse the memory.
			if (AllocatedBlocks.Num() > (CurrentBlock + 1))
			{
				++CurrentBlock;
				CurrentPtr = AllocatedBlocks[CurrentBlock];
				CurrentOffset = 0;
			}
			else
			{
				CurrentPtr = (uint8*)FGnmTempBlockManager::Get().AllocateBlock(TMemType, TBlockSizeName);
				CurrentBlock = AllocatedBlocks.Add(CurrentPtr);
				CurrentOffset = 0;
			}
		}
		AllocMem = CurrentPtr;
		CurrentPtr += Size;
		CurrentOffset += Size;

		return AllocMem;
	}

	void* AllocateChunk()
	{
		return Allocate(TBlockSizeBytes);
	}

	/*
	 * All allocated memory is treated as unused. New allocations will reuse existing allocated memory first.
	 * Be sure nothing (CPU or GPU) is done reading all data before calling this.
	 */
	void Reset()
	{
		if (AllocatedBlocks.Num() > 0)
		{
			CurrentPtr = AllocatedBlocks[0];
			CurrentOffset = 0;
			CurrentBlock = 0;
		}
	}

	/*
	 * Clears all current memory usage.  New blocks will be allocated from the manager if more allocations are made.
	 */
	void Clear()
	{
		AllocatedBlocks.Reset();
		CurrentPtr = nullptr;
		CurrentOffset = 0;
		CurrentBlock = -1;
	}

	uint32 GetChunkSize() const
	{
		return TBlockSizeBytes;
	}

private:
	TArray<uint8*>	AllocatedBlocks;
	uint8*			CurrentPtr;
	uint32			CurrentOffset;
	int32			CurrentBlock;
};

typedef FGnmTempBlockAllocator<EGnmMemType::GnmMem_GPU, FGnmTempBlockManager::EBlockSize::E8MB> TTempFrameGPUAllocator;
typedef FGnmTempBlockAllocator<EGnmMemType::GnmMem_GPU, FGnmTempBlockManager::EBlockSize::E64k> TTempContextFrameGPUAllocator;

#if PS4_SUPPORTS_PARALLEL_RHI_EXECUTE
typedef FGnmTempBlockAllocator<EGnmMemType::GnmMem_CPU, FGnmTempBlockManager::EBlockSize::E64k> TDCBAllocator;
typedef FGnmTempBlockAllocator<EGnmMemType::GnmMem_GPU, FGnmTempBlockManager::EBlockSize::E64k> TLCUEResourceAllocator;
#else
typedef FGnmTempBlockAllocator<EGnmMemType::GnmMem_CPU, FGnmTempBlockManager::EBlockSize::E512k> TDCBAllocator;
typedef FGnmTempBlockAllocator<EGnmMemType::GnmMem_GPU, FGnmTempBlockManager::EBlockSize::E64k> TLCUEResourceAllocator;
#endif
