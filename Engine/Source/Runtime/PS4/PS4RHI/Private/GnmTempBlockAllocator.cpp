// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GnmTempBlockAllocator.cpp: Class to generate gnm command buffers from RHI CommandLists
=============================================================================*/

#include "GnmRHIPrivate.h"
#include "GnmTempBlockAllocator.h"
#include "Misc/ScopeLock.h"
#include "PS4/PS4LLM.h"

constexpr uint32 FGnmTempBlockManager::PoolSizes[(int32)EBlockSize::SizesCount];

#define DEFRAG_TEMP_BLOCKS USE_DEFRAG_ALLOCATOR

FGnmTempBlockManager FGnmTempBlockManager::Manager;
FGnmTempBlockManager& FGnmTempBlockManager::Get()
{	
	return Manager;
}

void FGnmTempBlockManager::Init()
{
	FGnmTempBlockManager& BlockManager = Get();
	
	FMemory::Memzero(Manager.CurrentBlockIndexArray);
	BlockManager.CurrentFrameArray = 0;
}

void* FGnmTempBlockManager::AllocateBlock(EGnmMemType MemType, EBlockSize BlockSize)
{
	LLM_SCOPE_PS4(ELLMTagPS4::GnmTempBlocks);

	FScopeLock Lock(&BlockMutex);

	uint32 CurrentFrame = GGnmManager.GetFrameCount();
	const int32 MemTypeIndex = (int32)MemType;
	const int32 BlockSizeIndex = (int32)BlockSize;

	void* AllocMem = nullptr;
	TArray<FTempMemBlock>& BlockArray = Blocks[CurrentFrameArray][MemTypeIndex][BlockSizeIndex];
	uint32& CurrentBlockIndex = CurrentBlockIndexArray[MemTypeIndex][BlockSizeIndex];

	if (CurrentBlockIndex < BlockArray.Num())
	{
		BlockArray[CurrentBlockIndex].LastUsedFrame = CurrentFrame;
		AllocMem = BlockArray[CurrentBlockIndex].MemBlock.GetPointer();
	}
	else
	{
		const uint32 BlockSizeBytes = PoolSizes[BlockSizeIndex];
		FMemBlock NewBlock;
		void* BlockAlloc = nullptr;
#if DEFRAG_TEMP_BLOCKS		
		if (MemType == EGnmMemType::GnmMem_GPU)
		{
			BlockAlloc = GGnmManager.DefragAllocator.AllocateLocked(BlockSizeBytes, DEFAULT_VIDEO_ALIGNMENT, GET_STATID(STAT_TempBlockAllocator), true);			
			if (BlockAlloc)
			{
				NewBlock.OverridePointer(BlockAlloc);
				NewBlock.OverrideSize(BlockSizeBytes);
				NewBlock.MemType = EGnmMemType::GnmMem_Invalid;
			}
		}
#endif

		//fall back to a non-defrag alloc if we can't make it.
		if (!BlockAlloc)
		{
			NewBlock = FMemBlock::Allocate(BlockSizeBytes, DEFAULT_VIDEO_ALIGNMENT, MemType, GET_STATID(STAT_TempBlockAllocator));		
		}
		BlockArray.Add(FTempMemBlock(CurrentFrame, NewBlock));
		AllocMem = NewBlock.GetPointer();
	}
	++CurrentBlockIndex;
	check(AllocMem);

	return AllocMem;
}

void FGnmTempBlockManager::EndFrame()
{
	FScopeLock Lock(&BlockMutex);

	FMemory::Memzero(CurrentBlockIndexArray);
	CurrentFrameArray = (CurrentFrameArray + 1) % NumBufferedFrames;	

#if USE_DEFRAG_ALLOCATOR
	FPS4GPUDefragAllocator& DefragAllocator = GGnmManager.DefragAllocator;
#endif

	const uint32 DeadFrame = NumBufferedFrames + 2;
	const uint32 CurrentFrame = GGnmManager.GetFrameCount();
	for (int32 BlockSizeIndex = 0; BlockSizeIndex < (int32)EBlockSize::SizesCount; ++BlockSizeIndex)
	{
		for (int32 MemType = 0; MemType < (int32)EGnmMemType::GnmMem_MaxNumTypes; ++MemType)
		{
			int32 NumBlocks = Manager.Blocks[CurrentFrameArray][MemType][BlockSizeIndex].Num();
			for (int32 Block = 0; Block < NumBlocks; ++Block)
			{
				uint32 FramesSinceLastUsed = CurrentFrame - Manager.Blocks[CurrentFrameArray][MemType][BlockSizeIndex][Block].LastUsedFrame;
				if (FramesSinceLastUsed >= DeadFrame)
				{
					const FMemBlock& MemBlock = Manager.Blocks[CurrentFrameArray][MemType][BlockSizeIndex][Block].MemBlock;
#if DEFRAG_TEMP_BLOCKS					
					if ((EGnmMemType)MemBlock.MemType == EGnmMemType::GnmMem_Invalid)
					{
						const void* BlockMem = MemBlock.GetPointer();
						DefragAllocator.Free((void*)BlockMem);
						DefragAllocator.Unlock(BlockMem);
					}
					else
#endif
					{
						FMemBlock::Free(Manager.Blocks[CurrentFrameArray][MemType][BlockSizeIndex][Block].MemBlock);
					}
					Manager.Blocks[CurrentFrameArray][MemType][BlockSizeIndex].RemoveAtSwap(Block);
					--Block;
					--NumBlocks;
				}
			}
		}
	}
}