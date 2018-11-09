// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GnmRHIPrivate.h"
#include "PS4GPUDefragAllocator.h"
#include "GlobalShader.h"
#include "UpdateTextureShaders.h"
#include "GnmResources.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"

DEFINE_STAT(STAT_Garlic_Pool_FreeSize);
DEFINE_STAT(STAT_Num_Fragments);
DEFINE_STAT(STAT_Largest_Fragment);
DEFINE_STAT(STAT_GPUDefragTime);
DEFINE_STAT(STAT_CPUDefragTime);
DEFINE_STAT(STAT_NumRelocations);
DEFINE_STAT(STAT_SizeRelocations);
DEFINE_STAT(STAT_NunLockedChunks);
DEFINE_STAT(STAT_PaddingWaste);

/*-----------------------------------------------------------------------------
GPU Memory Move
-----------------------------------------------------------------------------*/

/**
* Performs a memcpy using compute shaders
* Does not handle overlapping memory, but all transfers are properly ordered so the caller
* can split up an overlapping transfer into a sequence of non-overlapping memcpys.
*
* @param Dest		destination start address
* @param Src		source start address
* @param NumBytes	Number of bytes to transfer
*/

//the amount of memory each thread of the compute shader moves.
static const int32 GPUMoveElementSize = sizeof(uint32)* 4;
static const int32 RequiredSizeAlignToRelocateLarge = sizeof(uint32)* 4 * 2 * 64;
static const int32 RequiredSizeAlignToRelocateSmall = sizeof(uint32)* 4 * 1 * 64;

bool GPS4UseCPUMemMove = false;
bool GPS4UseCPUForDefrag = false;
bool GPS4UseDMAForDefrag = false;

uint8* MemMoveSpeedTestBlock = nullptr;
static void PS4MemCopyWithCompute(const uint8* Dest, const uint8* Src, const uint64 NumBytes)
{
	bool bBefore = (Dest + NumBytes) <= Src;
	bool bAfter = Dest >= (Src + NumBytes);
	checkf(bBefore || bAfter, TEXT("Overlapped move detected. %i, %i"), (int32)bBefore, (int32)bAfter);

	FGnmCommandListContext& CmdContext = GGnmManager.GetImmediateContext();
	GnmContextType& Context = CmdContext.GetContext();

	CmdContext.GetContext().CommandBufferReserve();

	if (GPS4UseCPUForDefrag)
	{
		FMemory::Memcpy((void*)Dest, Src, NumBytes);
	}
	else if (GPS4UseDMAForDefrag)
	{
		CmdContext.GetContext().copyData((void*)Dest, Src, NumBytes, Gnm::kDmaDataBlockingEnable);
	}
	else
	{
		TShaderMapRef<TCopyDataCS<2>> ComputeShaderLarge(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		TShaderMapRef<TCopyDataCS<1>> ComputeShaderSmall(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		const bool bLargeCopy = (NumBytes % RequiredSizeAlignToRelocateLarge) == 0;
		FComputeShaderRHIParamRef ShaderRHI = bLargeCopy ? ComputeShaderLarge->GetComputeShader() : ComputeShaderSmall->GetComputeShader();
		CmdContext.RHISetComputeShader(ShaderRHI);
		
		uint32 CopyElementsPerThread = bLargeCopy ? 2 : 1;
		check(bLargeCopy || ((NumBytes % RequiredSizeAlignToRelocateSmall) == 0));

		//SetShaderValueOnContext(CmdContext, ShaderRHI, ComputeShader->CopyElementsPerThread, CopyElementsPerThread);
		Gnm::Buffer SourceBuffer;
		Gnm::Buffer DestBuffer;
		const Gnm::DataFormat DataFormat = Gnm::kDataFormatR32G32B32A32Uint;

		const uint32 NumElements = NumBytes / GPUMoveElementSize;
		SourceBuffer.initAsDataBuffer((void*)Src, DataFormat, NumElements);
		SourceBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);

		DestBuffer.initAsDataBuffer((void*)Dest, DataFormat, NumElements);
		DestBuffer.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);

		Context.setBuffers(Gnm::kShaderStageCs, bLargeCopy ? ComputeShaderLarge->SrcBuffer.GetBaseIndex() : ComputeShaderSmall->SrcBuffer.GetBaseIndex(), 1, &SourceBuffer);
		Context.setRwBuffers(Gnm::kShaderStageCs, bLargeCopy ? ComputeShaderLarge->DestBuffer.GetBaseIndex() : ComputeShaderSmall->SrcBuffer.GetBaseIndex(), 1, &DestBuffer);

		const uint32 NumThreadInGroup = 64;
		const uint32 NumThreadGroups = NumBytes / GPUMoveElementSize / CopyElementsPerThread / NumThreadInGroup;
		CmdContext.RHIDispatchComputeShader(NumThreadGroups, 1, 1);
	}
	CmdContext.FlushAfterComputeShader();
}

static void PS4MemCopyChunkedWithCompute(const uint8* Dest, const uint8* Src, const uint64 NumBytes)
{
	check((NumBytes % RequiredSizeAlignToRelocateSmall) == 0);
	
	FGnmCommandListContext& CmdContext = GGnmManager.GetImmediateContext();
	GnmContextType& Context = CmdContext.GetContext();
	
	int64_t SizeLeft = NumBytes;

	while (SizeLeft >= RequiredSizeAlignToRelocateLarge)
	{
		uint64 TransferSize = (SizeLeft / RequiredSizeAlignToRelocateLarge) * RequiredSizeAlignToRelocateLarge;
		PS4MemCopyWithCompute(Dest, Src, TransferSize);
		Dest += TransferSize;
		Src += TransferSize;
		SizeLeft -= TransferSize;		
	}
	if (SizeLeft > 0)
	{		
		PS4MemCopyWithCompute(Dest, Src, SizeLeft);		
	}
}

/**
* Performs a memmove within local memory using compute shaders
* Handles overlapping memory, and all transfers are properly ordered.
*
* @param Dest		destination start address
* @param Src		source start address
* @param NumBytes		Number of bytes to transfer
*/
void FPS4GPUDefragAllocator::PS4MemMoveWithCompute(const uint8* Dest, const uint8* Src, const uint64 NumBytes)
{
	int64 MemDistance = (int64)(Dest)-(int64)(Src);
	int64 BytesLeftToMove = NumBytes;
	check((MemDistance % RequiredSizeAlignToRelocateSmall) == 0);

	if (GPS4UseCPUMemMove)
	{
		FMemory::Memmove((void*)Dest, Src, NumBytes);
		return;
	}

	FGnmCommandListContext& CmdContext = GGnmManager.GetImmediateContext();
	CmdContext.PushMarker("Overlapped MemCopy");

	int64 AbsDistance = FMath::Abs(MemDistance);
	bool bOverlappedMove = AbsDistance < NumBytes;	

	//copy data 	
	int64 Offset = 0;
	if (bOverlappedMove)
	{		
		if (MemDistance > 0)
		{
			// Copying up (dst > src), copy overlapping chunks to higher addresses starting at the end in multiples of RequiredSizeAlignToOverlapMove
			Offset = NumBytes;
			while (BytesLeftToMove > 0)
			{
				const int32 NumTempMoveBytes = FMath::Min(BytesLeftToMove, MemMoveTempAreaSize);
				Offset -= NumTempMoveBytes;
				BytesLeftToMove -= NumTempMoveBytes;

				//move to temp location
				PS4MemCopyChunkedWithCompute(MemMoveTempArea, Src + Offset, NumTempMoveBytes);

				//move to shifted location
				PS4MemCopyChunkedWithCompute(Dest + Offset, MemMoveTempArea, NumTempMoveBytes);
			}
		}
		else
		{
			// Copying up (dst > src), copy overlapping chunks to higher addresses starting at the end in multiples of RequiredSizeAlignToOverlapMove
			MemDistance = -MemDistance;
			Offset = 0;
			while (BytesLeftToMove > 0)
			{
				const int32 NumTempMoveBytes = FMath::Min(BytesLeftToMove, MemMoveTempAreaSize);

				//move to temp location
				PS4MemCopyChunkedWithCompute(MemMoveTempArea, Src + Offset, NumTempMoveBytes);

				//move to shifted location
				PS4MemCopyChunkedWithCompute(Dest + Offset, MemMoveTempArea, NumTempMoveBytes);

				Offset += NumTempMoveBytes;
				BytesLeftToMove -= NumTempMoveBytes;
			}
		}
	}
	else
	{
		PS4MemCopyChunkedWithCompute(Dest, Src, NumBytes);
	}
	
	CmdContext.PopMarker();
}

#if VALIDATE_MEMORY_PROTECTION

void FPS4GPUDefragAllocator::PlatformSetNoMemoryPrivileges(const FMemProtectTracker& Block)
{
	const uint32 AllPrivs = 0;
	if (GGPUDefragDumpRelocationsToTTY)
	{
		printf("Setting no privileges: 0x%p, %i, 0x%x\n", Block.Memory, (int32)Block.BlockSize, AllPrivs);
	}
	int32 Ret = sceKernelMprotect(Block.Memory, Block.BlockSize, AllPrivs);
	check(Ret == SCE_OK);
}

void FPS4GPUDefragAllocator::PlatformSetStandardMemoryPrivileges(const FMemProtectTracker& Block)
{
	const uint32 AllPrivs = SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_ALL;
	if (GGPUDefragDumpRelocationsToTTY)
	{
		printf("Setting standard privileges: 0x%p, %i, 0x%x\n", Block.Memory, (int32)Block.BlockSize, AllPrivs);
	}
	int32 Ret = sceKernelMprotect(Block.Memory, Block.BlockSize, AllPrivs);
	check(Ret == SCE_OK);
}

void FPS4GPUDefragAllocator::PlatformSetStaticMemoryPrivileges(const FMemProtectTracker& BlockToAllow)
{
	FGnmDefraggable* Resource = (FGnmDefraggable*)BlockToAllow.UserPayload;	

	uint32 StaticPrivileges = SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_READ;
	if (!Resource || Resource->NeedsGPUWrite())
	{
		StaticPrivileges |= SCE_KERNEL_PROT_GPU_WRITE;
	}

	if (GGPUDefragDumpRelocationsToTTY)
	{
		printf("Setting static privileges: 0x%p, %i, 0x%x\n", BlockToAllow.Memory, (int32)BlockToAllow.BlockSize, StaticPrivileges);
	}
	int32 Ret = sceKernelMprotect(BlockToAllow.Memory, BlockToAllow.BlockSize, StaticPrivileges);
	check(Ret == SCE_OK);
}

void FPS4GPUDefragAllocator::PlatformSetRelocationMemoryPrivileges(const FMemProtectTracker& Block)
{
	int32 Ret = sceKernelMprotect(Block.Memory, Block.BlockSize, SCE_KERNEL_PROT_GPU_ALL);
	check(Ret == SCE_OK);
	if (GGPUDefragDumpRelocationsToTTY)
	{
		printf("Setting relocation privileges: 0x%p, %i, 0x%x\n", Block.Memory, (int32)Block.BlockSize, SCE_KERNEL_PROT_GPU_ALL);
	}
}

void FPS4GPUDefragAllocator::PlatformSetRelocationMemoryPrivileges(const TArray<FMemProtectTracker>& BlocksToRemove)
{
	for (int32 i = 0; i < BlocksToRemove.Num(); ++i)
	{
		const FMemProtectTracker& Block = BlocksToRemove[i];
		PlatformSetRelocationMemoryPrivileges(Block);		
	}	
}
#endif

/*-----------------------------------------------------------------------------
FPS4GPUDefragAllocator implementation.
-----------------------------------------------------------------------------*/

/** Enables/disables the texture memory defragmentation feature. */
bool GEnableTextureMemoryDefragmentation = true;

/**
* Constructor, initializes the FGPUDefragAllocator with already allocated memory
*
* @param PoolMemory				Pointer to the pre-allocated memory pool
* @param FailedAllocationMemory	Dummy pointer which be returned upon OutOfMemory
* @param PoolSize					Size of the memory pool
* @param Alignment					Default alignment for each allocation
*/
FPS4GPUDefragAllocator::FPS4GPUDefragAllocator()	
	: MemMoveTempArea(nullptr)
	, CurrentFreeBuffer(0)
{
}

/**
* Destructor
*/
FPS4GPUDefragAllocator::~FPS4GPUDefragAllocator()
{
	FMemBlock::Free(MemMoveTempAreaBlock);
}

void FPS4GPUDefragAllocator::Initialize(uint8* PoolMemory, int64 PoolSize)
{	

#if VALIDATE_MEMORY_PROTECTION || MULTIBUFFER_CACHED_LOCK_DETECTION
	//16k alignment needed for methods that use memory protection to track down bugs.
	const uint32 BlockAlignment = 16 * 1024;
#else
	const uint32 BlockAlignment = RequiredSizeAlignToRelocateLarge;
#endif

	FGPUDefragAllocator::Initialize(PoolMemory, PoolSize, BlockAlignment);

	// Allocator settings
	{
		FGPUDefragAllocator::FSettings AllocatorSettings;
		GetSettings(AllocatorSettings);
#if 0
		GConfig->GetBool(TEXT("TextureStreaming"), TEXT("bEnableAsyncDefrag"), AllocatorSettings.bEnableAsyncDefrag, GEngineIni);
		GConfig->GetBool(TEXT("TextureStreaming"), TEXT("bEnableAsyncReallocation"), AllocatorSettings.bEnableAsyncReallocation, GEngineIni);
#endif

		if (GConfig->GetInt(TEXT("TextureStreaming"), TEXT("MaxDefragRelocations"), AllocatorSettings.MaxDefragRelocations, GEngineIni))
		{
			// Convert from KB to bytes.
			AllocatorSettings.MaxDefragRelocations *= 1024;
		}
		if (GConfig->GetInt(TEXT("TextureStreaming"), TEXT("MaxDefragDownShift"), AllocatorSettings.MaxDefragDownShift, GEngineIni))
		{
			// Convert from KB to bytes.
			AllocatorSettings.MaxDefragDownShift *= 1024;
		}

		AllocatorSettings.MaxDefragRelocations = 8 * 1024 * 1024;
		AllocatorSettings.MaxDefragDownShift = 8 * 1024 * 1024;

		//BW is doubled for overlapped moves because we move to a temp location and then back to the shifted location
		AllocatorSettings.OverlappedBandwidthScale = 2;
		SetSettings(AllocatorSettings);
	}

	FenceLabel = (uint64*)FMemBlock::Allocate(8, 8, EGnmMemType::GnmMem_CPU, GET_STATID(STAT_Onion_Label)).GetPointer();
	*FenceLabel = 0;
	
	//allocate space to do overlapped mem moves more efficiently.
	MemMoveTempAreaBlock = FMemBlock::Allocate(MemMoveTempAreaSize, RequiredSizeAlignToRelocateLarge, EGnmMemType::GnmMem_GPU, GET_STATID(STAT_Garlic_Texture));
	MemMoveTempArea = (uint8*)MemMoveTempAreaBlock.GetPointer();
	check(MemMoveTempArea);

#if USE_DEFRAG_ALLOCATOR
	extern bool GPS4DefraggerInitialized;
	GPS4DefraggerInitialized = true;
#endif
}

void* FPS4GPUDefragAllocator::Allocate(int64 Size, int32 Alignment, TStatId InStat, bool bAllowFailure)
{
	FScopeLock Lock(&SynchronizationObject);	
	void* Pointer = FGPUDefragAllocator::Allocate(Size, Alignment, InStat, GEnableTextureMemoryDefragmentation || bAllowFailure);
	if (!Pointer && bAllowFailure)
	{
		return nullptr;
	}

	if (!Pointer)
	{
		UE_LOG(LogPS4, Fatal, TEXT("Could not allocate: %i bytes, %i align, FreeMem: %lld, NumChunks: %i, LargestAlloc: %lld"), Size, Alignment, AvailableMemorySize, CurrentNumHoles, CurrentLargestHole);
	}

	//can't safely do a panic defrag on PS4.
#if 0
	if (!bAllowFailure && !IsValidPoolMemory(Pointer) && GEnableTextureMemoryDefragmentation)
	{
		DefragmentTextureMemory();
		Pointer = FGPUDefragAllocator::Allocate(Size, false);
	}
#endif

	if (!bAllowFailure && !IsValidPoolMemory(Pointer))
	{
		check(false);
		// Mark texture memory as having been corrupted or not
		extern bool GIsTextureMemoryCorrupted;
		if (!GIsTextureMemoryCorrupted)
		{

//TODO: mwassmer
#if 0
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			int32 AllocatedSize, AvailableSize, PendingMemoryAdjustment, LargestAvailableAllocation, NumFreeChunks;
			GetMemoryStats(AllocatedSize, AvailableSize, PendingMemoryAdjustment, LargestAvailableAllocation, NumFreeChunks);
			GLog->Logf(TEXT("RAN OUT OF TEXTURE MEMORY, EXPECT CORRUPTION AND GPU HANGS!"));
			GLog->Logf(TEXT("Tried to allocate %d bytes"), Size);
			GLog->Logf(TEXT("Texture memory available: %.3f MB"), float(AvailableSize) / 1024.0f / 1024.0f);
			GLog->Logf(TEXT("Largest available allocation: %.3f MB (%d holes)"), float(LargestAvailableAllocation) / 1024.0f / 1024.0f, NumFreeChunks);
#endif
#endif
			GIsTextureMemoryCorrupted = true;
		}
	}
	check(IsAligned(Pointer, Alignment));

#if TRACK_GNM_MEMORY_USAGE
	FMemoryChunk* NewChunk = PointerToChunkMap.FindRef(Pointer);
	FMemBlock::TrackMemoryUsage(EGnmMemType::GnmMem_GPU, InStat, NewChunk->Size);
#endif

	return Pointer;
}

void* FPS4GPUDefragAllocator::AllocateLocked(int64 Size, int32 Alignment, TStatId Stat, bool bAllowFailure)
{

#if TRACK_GNM_MEMORY_USAGE && 0
	FMemBlock::TrackMemoryUsage(EGnmMemType::GnmMem_GPU, Stat, Size);
#endif

	FScopeLock Lock(&SynchronizationObject);
	void* AllocedMem = Allocate(Size, Alignment, Stat, bAllowFailure);

	//allocation may be allowed to fail.  if it does we can't lock.
	if (AllocedMem)
	{
		this->Lock(AllocedMem);
	}
	return AllocedMem;
}

void FPS4GPUDefragAllocator::Free(void* Pointer)
{
	FScopeLock Lock(&SynchronizationObject);
	DeferredFrees[CurrentFreeBuffer].Add(Pointer);

	//don't waste time defragging pending free blocks.  Also the RHITexture has been unregistered so
	//the relocate functions will fail checks.
	this->Lock(Pointer);
}

static int32 GPS4DefragPerfTest = 0;
static FAutoConsoleVariableRef CVarPS4DefragPerfTest(
	TEXT("r.PS4.GPUDefragPerfTest"),
	GPS4DefragPerfTest,
	TEXT("Uses the defrag GPU memmove functions to move a large chunk of memory each frame.\n"),
	ECVF_Default
	);

int32 GPS4DefragPerfTestSize = 8 * 1024 * 1024;
int32 GPS4DefragPerfTestDist = 8 * 1024 * 1024;
int32 GPS4DefragShaderGroupLimit = 80;

int32 FPS4GPUDefragAllocator::Tick(FRelocationStats& Stats, bool bPanicDefrag)
{
	uint32 StartTime = FPlatformTime::Cycles();
	//defrag commands might hit master reserve callback and need to allocate blocks.
	//lock first to avoid deadlocks from inverted acquisition on other threads 
	FScopedTempBlockManagerLock BlockLock(FGnmTempBlockManager::Get());
	FScopeLock Lock(&SynchronizationObject);

	CurrentFreeBuffer = (CurrentFreeBuffer + 1) % NUM_BUFFERED_FRAMES_FOR_FREES;

	struct FPointerFencePair
	{
		FPointerFencePair(void* InPointer, FGnmComputeFence* InFence)
		: Pointer(InPointer)
		, Fence(InFence)
		{
		}
		void* Pointer;
		FGnmComputeFence* Fence;
	};

	TArray<FPointerFencePair> FenceLocksToRemove;
	TArray<void*> ChunksToFreeToBaseAllocator;

	//if we have any chunks that are locked pending a GPU move by another system guarded by a fence, then we should check them to see if that fence
	//has been written and we can clear the lock.  accumulate a removal list to iterate the map safely.	
	{		
		for (auto& FenceLock : FenceLocks)
		{
			FComputeFenceRHIParamRef RHIComputeFence = FenceLock.Value.GetReference();
			FGnmComputeFence* WriteComputeFence = FGnmDynamicRHI::ResourceCast(RHIComputeFence);
			if (WriteComputeFence->HasGPUWrittenFence())
			{
				FenceLocksToRemove.Add(FPointerFencePair(FenceLock.Key, WriteComputeFence));

				//unlock to account for the lock in 'lockwithfence'
				Unlock(FenceLock.Key);
			}
		}

		//finalize removal
		for (FPointerFencePair& LockToRemove : FenceLocksToRemove)
		{
			FenceLocks.Remove(LockToRemove.Pointer, LockToRemove.Fence);
		}
		FenceLocksToRemove.Reset();
		FenceLocks.Shrink();
	}

	//if we have any chunks that are ready to free pending a GPU move by another system guarded by a fence, then we should check them to see if that fence
	//has been written so we can unlock and finish the free.  accumulate a removal list to iterate the map safely.	
	{
		for (auto& FenceFree : FenceFrees)
		{
			FComputeFenceRHIParamRef RHIComputeFence = FenceFree.Value.GetReference();
			FGnmComputeFence* WriteComputeFence = FGnmDynamicRHI::ResourceCast(RHIComputeFence);
			if (WriteComputeFence->HasGPUWrittenFence())
			{
				void* PointerToFree = FenceFree.Key;
				//account for the LockWithFence lock.
				Unlock(PointerToFree);

				FenceLocksToRemove.Add(FPointerFencePair(PointerToFree, WriteComputeFence));

				FGPUDefragAllocator::FMemoryChunk* ChunkToFree = PointerToChunkMap.FindRef(PointerToFree);
				if (ChunkToFree->LockCount != 0)
				{
					FreesWithLocks.Add(PointerToFree);
				}
				else
				{
					ChunksToFreeToBaseAllocator.Add(PointerToFree);
				}
			}
		}

		//finalize removal and unlock of any chunks guarded by completed fences.
		for (FPointerFencePair& LockToRemove : FenceLocksToRemove)
		{
			FenceLocks.Remove(LockToRemove.Pointer, LockToRemove.Fence);
		}
		FenceLocksToRemove.Reset();
	}

	// check memory we want to free with oustanding locks.
	{
		TArray<void*> FreesWithLocksToRemove;
		for (void* PointerToFree : FreesWithLocks)
		{
			FGPUDefragAllocator::FMemoryChunk* ChunkToFree = PointerToChunkMap.FindRef(PointerToFree);
			if (ChunkToFree->LockCount == 0)
			{
				ChunksToFreeToBaseAllocator.Add(PointerToFree);
				FreesWithLocksToRemove.Add(PointerToFree);
			}
		}

		for (void* ToRemove : FreesWithLocksToRemove)
		{
			FreesWithLocks.Remove(ToRemove);
		}		
	}

	TArray<void*>& FreesToProcess = DeferredFrees[CurrentFreeBuffer];
	for (int32 i = 0; i < FreesToProcess.Num(); ++i)
	{
		void* PointerToFree = FreesToProcess[i];

		//unlock to account for the lock we do when queuing the deferred free.
		this->Unlock(PointerToFree);

		//it is not legal to free memory that is part of a GPU move of another system.  If we find an outstanding fence lock we need to hold onto this memory a little longer before freeing it.
		//so add it to the FenceFrees list.
		FComputeFenceRHIRef* RHIFenceLock = FenceLocks.Find(PointerToFree);
		FGPUDefragAllocator::FMemoryChunk* ChunkToFree = PointerToChunkMap.FindRef(PointerToFree);
		if (RHIFenceLock)
		{
			checkf(!FenceFrees.Contains(PointerToFree), TEXT("Chunk is already in FenceFrees list. 0x%p"), PointerToFree);
			FenceFrees.Add(PointerToFree, *RHIFenceLock);

			FGnmComputeFence* FenceLock = FGnmDynamicRHI::ResourceCast(RHIFenceLock->GetReference());
			FenceLocks.Remove(PointerToFree, FenceLock);
		}
		else if (ChunkToFree->LockCount != 0)
		{
			FreesWithLocks.Add(PointerToFree);
		}
		else
		{
			ChunksToFreeToBaseAllocator.Add(PointerToFree);
		}
	}
	FreesToProcess.Empty();

	for (void* PointerToFree : ChunksToFreeToBaseAllocator)
	{
#if TRACK_GNM_MEMORY_USAGE
		FGPUDefragAllocator::FMemoryChunk* ChunkToFree = PointerToChunkMap.FindRef(PointerToFree);
		FMemBlock::TrackMemoryUsage(EGnmMemType::GnmMem_GPU, ChunkToFree->Stat, -ChunkToFree->Size);
#endif
		FGPUDefragAllocator::Free(PointerToFree);
	}

	// This is essential to improve performance. Too many threadgroups spoil memory access efficiency.
	FGnmCommandListContext& CmdContext = GGnmManager.GetImmediateContext();
	GnmContextType& Context = CmdContext.GetContext();
	Context.setComputeShaderControl(GPS4DefragShaderGroupLimit, 0, 0);
	if (GPS4DefragPerfTest == 0)
	{
		if (MemMoveSpeedTestBlock != nullptr)
		{
			this->Unlock(MemMoveSpeedTestBlock);
			Free(MemMoveSpeedTestBlock);
			MemMoveSpeedTestBlock = nullptr;
		}
	}
	else
	{
		if (MemMoveSpeedTestBlock == nullptr)
		{
			MemMoveSpeedTestBlock = (uint8*)Allocate(20 * 1024 * 1024, 1024, GET_STATID(STAT_Garlic_Texture), false);
			this->Lock(MemMoveSpeedTestBlock);
		}		
		PS4MemMoveWithCompute(MemMoveSpeedTestBlock + (GPS4DefragPerfTestDist), MemMoveSpeedTestBlock, GPS4DefragPerfTestSize);
	}	

	//flush L2 in case the CPU wrote uncached to any address that was cached in GPU cache.
	CmdContext.FlushBeforeComputeShader();
	
	int32 NumBytesRelocated = FGPUDefragAllocator::Tick(Stats, bPanicDefrag);	
	Context.setComputeShaderControl(0, 0, 0);

	SET_DWORD_STAT(STAT_Garlic_Pool_FreeSize, AvailableMemorySize)
	SET_DWORD_STAT(STAT_Num_Fragments, Stats.NumHoles);
	SET_DWORD_STAT(STAT_Largest_Fragment, Stats.LargestHoleSize);
	SET_DWORD_STAT(STAT_NumRelocations, Stats.NumRelocations);
	SET_DWORD_STAT(STAT_SizeRelocations, NumBytesRelocated);
	SET_DWORD_STAT(STAT_NunLockedChunks, NumLockedChunks);
	SET_DWORD_STAT(STAT_PaddingWaste, PaddingWasteSize);
	

	uint32 EndTime = FPlatformTime::Cycles();

	uint32 DefragCPUCycles = EndTime - StartTime;
	SET_CYCLE_COUNTER(STAT_CPUDefragTime, DefragCPUCycles);

	return NumBytesRelocated;
}

void FPS4GPUDefragAllocator::LockWithFence(void* Pointer, FComputeFenceRHIParamRef LockFence)
{
	FScopeLock ScopeLock(&SynchronizationObject);
	this->Lock(Pointer);
	checkf(!FenceFrees.Contains(Pointer), TEXT("Locking address that is still waiting on a fence to free memory.  0x%p, %s"), Pointer, *LockFence->GetName().ToString());
	FenceLocks.Add(Pointer, LockFence);	
}

void FPS4GPUDefragAllocator::RegisterDefraggable(FGnmDefraggable* Resource)
{
	FScopeLock ScopeLock(&SynchronizationObject);

	void* BaseAddress = Resource->GetBaseAddress();
	FMemoryChunk* MatchingChunk = PointerToChunkMap.FindRef(BaseAddress);
	checkf(MatchingChunk, TEXT("Couldn't find matching chunk for %x"), BaseAddress);

	check(BaseAddress == MatchingChunk->Base);	
	SetUserPayload(BaseAddress, Resource);
}

void FPS4GPUDefragAllocator::UnregisterDefraggable(FGnmDefraggable* Resource)
{
	FScopeLock ScopeLock(&SynchronizationObject);
	SetUserPayload(Resource->GetBaseAddress(), nullptr);
}

#if 0
/**
* Defragment the texture memory. This function can be called from both gamethread and renderthread.
* Texture memory is shuffled around primarily using GPU transfers. Texture memory that can't be associated
* with a tracked texture object will not be relocated. Locked textures will not be relocated either.
*/
void FPS4GPUDefragAllocator::DefragmentTextureMemory()
{
	if (IsInRenderingThread())
	{
		FPS4GPUDefragAllocator::FRelocationStats Stats;
		//		GPS3Gcm->BlockUntilGPUIdle();
		DeleteUnusedPS3Resources();
		DeleteUnusedPS3Resources();
		FGPUDefragAllocator::DefragmentMemory(Stats);
		//		GPS3Gcm->BlockUntilGPUIdle();
	}
	else
	{
		// Flush and delete all gamethread-deferred-deletion objects (like textures).
		//		FlushRenderingCommands();

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			DefragmentTextureMemoryCommand,
			{
			FPS4GPUDefragAllocator::FRelocationStats Stats;
			//			GPS3Gcm->BlockUntilGPUIdle();
			DeleteUnusedPS3Resources();
			DeleteUnusedPS3Resources();
			GPS3Gcm->GetTexturePool()->DefragmentMemory(Stats);
			//			GPS3Gcm->BlockUntilGPUIdle();
		});

		// Flush and blocks until defragmentation is completed.
		FlushRenderingCommands();
	}
}
#endif

float FPS4GPUDefragAllocator::EstimateGPUTimeForMove(int32 Size, bool bOverlapped)
{
	//current testing shows us moving data at ~96 GB/s when account for the read AND write when shifting memory.
	//if we make improvements to the method this should be adjusted.
	static const float EstimatedBandwidthInMBPerSec = 96.0f * 1024.0f;
	static const float MBDivisor = 1.0f / (1024.0f * 1024.0f);
	float SizeInMB = (float)Size / MBDivisor;
	float EstimatedGPUTime = SizeInMB / EstimatedBandwidthInMBPerSec;
	if (bOverlapped)
	{
		EstimatedGPUTime *= 2.0f;
	}
	return EstimatedGPUTime;
}

void FPS4GPUDefragAllocator::FullDefragAndFlushGPU()
{
	check(IsInRenderingThread());

	for (int32 i = 0; i < NUM_BUFFERED_FRAMES_FOR_FREES; ++i)
	{
		FullDefragAndFlushGPU_Internal();
		GGnmManager.WaitForGPUIdleReset();
	}
}

void FPS4GPUDefragAllocator::FullDefragAndFlushGPU_Internal()
{
	FRelocationStats Stats;
	Tick(Stats, true);
}

/**
* Copy memory from one location to another. If it returns false, the defragmentation
* process will assume the memory is not relocatable and keep it in place.
* Note: Source and destination may overlap.
*
* @param Dest			Destination memory start address
* @param Source		Source memory start address
* @param Size			Number of bytes to copy
* @param UserPayload	User payload for this allocation
*/
void FPS4GPUDefragAllocator::PlatformRelocate(void* Dest, const void* Source, int64 Size, void* UserPayload)
{
	FGnmDefraggable* Resource = (FGnmDefraggable*)UserPayload;
	check(Resource);

	const uint8* Src = (const uint8*)Source;
	uint8* Dst = (uint8*)Dest;
	
	PS4MemMoveWithCompute(Dst, Src, Size);
	Resource->UpdateBaseAddress(Dest);
}

/**
* Inserts a fence to synchronize relocations.
* The fence can be blocked on at a later time to ensure that all relocations initiated
* so far has been fully completed.
*
* @return		New fence value
*/
uint64 FPS4GPUDefragAllocator::PlatformInsertFence()
{
	FGnmCommandListContext& CmdContext = GGnmManager.GetImmediateContext();
	GnmContextType& Context = CmdContext.GetContext();

	// NOTE: kEopCbDbReadsDone and kEopCsDone are two names for the same value, so this EOP event does cover both graphics and compute
	// use cases.
	Context.writeAtEndOfPipe(Gnm::kEopCbDbReadsDone, Gnm::kEventWriteDestMemory,(void*)FenceLabel,
		Gnm::kEventWriteSource64BitsImmediate, CurrentSyncIndex, Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kCachePolicyLru);
	
	return (uint64)CurrentSyncIndex;
}

/**
* Blocks the calling thread until all relocations initiated before the fence
* was added has been fully completed.
*
* @param Fence		Fence to block on
*/
void FPS4GPUDefragAllocator::PlatformBlockOnFence(uint64 Fence)
{	
	while (*FenceLabel < Fence)
	{
	}	
}

/**
* Allows each platform to decide whether an allocation can be relocated at this time.
*
* @param Source		Base address of the allocation
* @param UserPayload	User payload for the allocation
* @return				true if the allocation can be relocated at this time
*/
static int32 GPS4AllowDefrag = 1;
static FAutoConsoleVariableRef CVarPS4AllowDefrag(
	TEXT("r.PS4.AllowDefrag"),
	GPS4AllowDefrag,
	TEXT("Allows GPU defragging.\n"),	
	ECVF_Default
	);

bool FPS4GPUDefragAllocator::PlatformCanRelocate(const void* Source, void* UserPayload) const
{
	if (!UserPayload)
	{
		//the only legit case of no payload is the in-flight tail of a relocation into an adjacent free chunk.
		//such a chunk should be on the pending free list.
		bool bFoundPendingFree = false;
		// Take the opportunity to free all chunks that couldn't be freed immediately before.
		for (TDoubleLinkedList<FMemoryChunk*>::TIterator It(PendingFreeChunks.GetHead()); It; ++It)
		{
			FMemoryChunk* Chunk = *It;
			if (Chunk->Base == Source)
			{
				bFoundPendingFree = true;
				break;
			}
		}
		checkf(bFoundPendingFree, TEXT("Node to relocate has no user data, and isn't a valid pending free."));
		return false;
	}
	FGnmDefraggable* Resource = (FGnmDefraggable*)UserPayload;
	check(Resource);

	check(Resource->GetBaseAddress() == Source);

	//we are always going to move the size of the CHUNK which is aligned to what the shader can move.
	//so we don't need to check it here;
	//uint32 TextureSize = Surface.GetMemorySize(false);
	return (Resource->CanRelocate()) && (GPS4AllowDefrag != 0);
}

/**
* Notifies the platform that an async reallocation request has been completed.
*
* @param FinishedRequest	The request that got completed
* @param UserPayload		User payload for the allocation
*/
void FPS4GPUDefragAllocator::PlatformNotifyReallocationFinished(FAsyncReallocationRequest* FinishedRequest, void* UserPayload)
{
	check(IsInRenderingThread() || IsInRHIThread());
	FGnmDefraggable* OldResource = (FGnmDefraggable*)UserPayload;	
	check(OldResource->GetBaseAddress() == FinishedRequest->GetNewBaseAddress());
}

/**
* Defragment the texture memory. This function can be called from both gamethread and renderthread.
* Texture memory is shuffled around primarily using GPU transfers. Texture memory that can't be associated
* with a tracked texture object will not be relocated. Locked textures will not be relocated either.
*/
#if 0
void appDefragmentTexturePool()
{
	if (GEnableTextureMemoryDefragmentation)
	{
		GPS3Gcm->GetTexturePool()->DefragmentTextureMemory();
	}
}

/**
* Checks if the texture data is allocated within the texture pool or not.
*/
bool appIsPoolTexture(FTextureRHIParamRef TextureRHI)
{
	return TextureRHI && GPS3Gcm->GetTexturePool()->IsTextureMemory(TextureRHI->GetBaseAddress());
}
#endif

/**
* Log the current texture memory stats.
*
* @param Message	This text will be included in the log
*/
#if 0
void appDumpTextureMemoryStats(const TCHAR* Message)
{
#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
	int32 AllocatedSize, AvailableSize, PendingMemoryAdjustment, LargestAvailableAllocation, NumFreeChunks;
	GPS3Gcm->GetTexturePool()->GetMemoryStats(AllocatedSize, AvailableSize, PendingMemoryAdjustment, LargestAvailableAllocation, NumFreeChunks);
	debugf(NAME_DevMemory, TEXT("%s - Texture memory available: %.3f MB. Largest available allocation: %.3f MB (%d holes)"), Message, float(AvailableSize) / 1024.0f / 1024.0f, float(LargestAvailableAllocation) / 1024.0f / 1024.0f, NumFreeChunks);
#endif
}
#endif
