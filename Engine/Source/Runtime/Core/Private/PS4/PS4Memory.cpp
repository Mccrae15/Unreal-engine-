// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#if USE_NEW_PS4_MEMORY_SYSTEM == 0

#include "PS4/PS4Memory.h"
#include "Misc/ScopeLock.h"
#include "Misc/ConfigCacheIni.h"
#include "MallocAnsi.h"
#if USE_MALLOC_BINNED2
#include "MallocBinned2.h"
#else
#include "MallocBinned.h"
#endif
#include "GenericPlatformMemoryPoolStats.h"
#include "HAL/LowLevelMemTracker.h"
#include "PS4/PS4LLM.h"


// Access the additional title memory available in SDK 4.508.101.
// (128 MB in Base, 256 MB in Neo).
SCE_KERNEL_EXTENDED_DMEM_BASE_128;
SCE_KERNEL_EXTENDED_DMEM_NEO_256;


DECLARE_MEMORY_STAT_POOL(TEXT("Shared system and GPU memory [Onion]"), MCR_GPUSystem, STATGROUP_Memory, FPlatformMemory::MCR_GPUSystem);

DECLARE_STATS_GROUP(TEXT("PS4 Memory Details"), STATGROUP_PS4MemDetails, STATCAT_Advanced);

DECLARE_MEMORY_STAT(TEXT("Total Flexible Memory"), STAT_TotalFlexible, STATGROUP_PS4MemDetails);
DECLARE_MEMORY_STAT(TEXT("Available Flexible Memory"), STAT_AvailableFlexible, STATGROUP_PS4MemDetails);
DECLARE_MEMORY_STAT(TEXT("Used Flexible Memory"), STAT_UsedFlexible, STATGROUP_PS4MemDetails);

#define VIRTUAL_ADDRESS_HASH_SIZE (65536)
#define DEFAULT_DIRECT_MEM_BLOCK_SIZE (256 * 1024 * 1024)
#define ANSI_STARTUP_MEM_POOL_SIZE (64 * 1024 * 1024)
#define PS4_PSEUDOPAGESIZE (64 * 1024)
#define PS4_TRUEPAGESIZE (16 * 1024)
#define VIRTUAL_ADDRESS_HASH_SHIFT (14) // related to PS4_TRUEPAGESIZE

// #defines so static init order can't get confused
#ifndef GARLIC_HEAP_SIZE
#	define GARLIC_HEAP_SIZE			(2600ULL * 1024 * 1024)
#endif
#ifndef ONION_HEAP_SIZE
#	define ONION_HEAP_SIZE			(200ULL * 1024 * 1024)
#endif
#ifndef RESERVED_MEMORY_SIZE
#	define RESERVED_MEMORY_SIZE		(0ULL * 1024 * 1024) // Not needed anymore. Mspace heaps and alignment leave ~1.5MB of direct memory unallocated, so anything additional is just waste.
#endif

//Reserve slop for malloc calls made by SCEs libc
#ifndef LIBC_MALLOC_SIZE			
#	define LIBC_MALLOC_SIZE			(8ULL * 1024 * 1024)
#endif
// default is 448MB according to docs (this must be a #define, not a looked up .ini value)
#ifndef FLEXIBLE_MEMORY_SIZE
#	define FLEXIBLE_MEMORY_SIZE		(448ULL * 1024 * 1024)
#endif
// set the flex mem size
SCE_KERNEL_FLEXIBLE_MEMORY_SIZE(FLEXIBLE_MEMORY_SIZE)


#if FORCE_ANSI_ALLOCATOR
//With ANSI allocator we hand over all memory to GMspace, but it comes across in two steps
#define PREMAIN_HEAP_SIZE ANSI_STARTUP_MEM_POOL_SIZE
#else
//Without Ansi, just reserve a small amount of slop for libc calls, global constructors, etc
#define PREMAIN_HEAP_SIZE LIBC_MALLOC_SIZE
#endif

uint64 GGarlicHeapSize				= GARLIC_HEAP_SIZE;
uint64 GOnionHeapSize				= ONION_HEAP_SIZE;
// set in user_malloc_init
uint64 GMainHeapSize				= 0;
uint64 GFlexibleMemoryAtStart		= 0;
uint64 GEstimatedProgramSize		= 0;
uint64 GMappingSpaceSize			= 0; // Used by the direct memory allocator

#if ENABLE_LOW_LEVEL_MEM_TRACKER
int64 LLMMallocTotal = 0;
#endif

GPUMemStatDump FPS4PlatformMemory::GPUStatDumpFunc;

volatile int64 GPS4Allocated_MemBlockOnion = 0;  // Onion allocations made directly from the FMemBlock API
volatile int64 GPS4Allocated_MemBlockGarlic = 0; // Garlic allocations made directly from the FMemBlock API (excluding the initial FMemBlock allocation for the defrag heap).

#define PS4_USE_FLEXIBLE_MEMORY 1

class FPS4DirectMemoryMapper
{
public:

	/** Default constructor. */
	FPS4DirectMemoryMapper()
		: Base(nullptr)
		, End(nullptr)
		, Top(nullptr)
		, MaxAllocatedBlocks(0)
		, AllocatedBlocks(0)
		, FreeBlocks(0)
		, Merge(nullptr)
		, Compare(nullptr)
		, VAHashTable(nullptr)
		, FreeHashEntryNodes(nullptr)
	{
		off_t PhysicalAddress;
		size_t AvailableSize;
		sceKernelAvailableDirectMemorySize(0, SCE_KERNEL_MAIN_DMEM_SIZE, 0, &PhysicalAddress, &AvailableSize);
		AvailableSize -= RESERVED_MEMORY_SIZE;
		AvailableDirect = AvailableSize;
	}

	struct FDMBlocks
	{
		off_t	StartOff;
		uint32	Blocks;
	};

	struct FVAEntry
	{
		off_t		DirectMemOff;
		void*		VirtualAddress;
		FVAEntry*	Next;
		uint32		Blocks;
		uint32		End;
	};

	struct FVAFreeNode
	{
		uint32			Blocks;
		FVAFreeNode*	Next;
	};

	FCriticalSection CritSection;

	FDMBlocks* Base;
	FDMBlocks* End;
	FDMBlocks* Top;

	uint64		AvailableDirect;
	uint32		MaxAllocatedBlocks;
	uint32		AllocatedBlocks;
	uint32		FreeBlocks;

	FDMBlocks* Merge;
	FDMBlocks* Compare;

	FVAEntry*		VAHashTable;
	FVAFreeNode*	FreeHashEntryNodes;

	static FPS4DirectMemoryMapper* GetInstance()
	{
		//Handled in this way to get around issues with global construction order
		static FPS4DirectMemoryMapper Mapper;
		return &Mapper;
	}

	static constexpr uint64 BlockSize = PS4_TRUEPAGESIZE;

	static void ReserveRemaining()
	{
		FPS4DirectMemoryMapper* MemMapper = GetInstance();
		// Initialize if needed
		MemMapper->Initialize();

		int64 AvailableDirect = MemMapper->AvailableDirect;

#if ENABLE_LOW_LEVEL_MEM_TRACKER
		// if LLM is enabled we can't reserve all of memory because LLM gets its memory directly from the OS
		if (FLowLevelMemTracker::Get().IsEnabled())
			AvailableDirect -= LLM_MEMORY_OVERHEAD;
#endif

		MemMapper->AllocateDirectMemoryChunk(AvailableDirect, SCE_KERNEL_WB_ONION, false);
	}

	static void* Mmap(SIZE_T Size, uint32 Alignment)
	{
		return GetInstance()->PrivateMmap(Size, Alignment);
	}

	static void Unmap(void* Ptr)
	{
		check(Ptr);
		GetInstance()->PrivateUnmap(Ptr);
	}

	static bool AllocateDirectMemory(uint64 Size, int32 MemoryType, off_t& DirectOut, uint32 Alignment = 0)
	{
		return GetInstance()->PrivateAllocateDirectMemory(Size, MemoryType, DirectOut, Alignment, false);
	}

	static void ReleaseDirectMemory(uint64 Size, off_t MemOffset)
	{
		GetInstance()->PrivateReleaseDirectMemory(Size, MemOffset);
	}

	static void GetStats(uint64& OutAllocatedBytes, uint64& OutPeakAllocatedBytes, uint64& OutFreeBytes, uint64& OutFragmentedDirectMemBlocks)
	{
		FPS4DirectMemoryMapper* MemMapper = GetInstance();

		uint64 AllocatedPhysical = uint64(MemMapper->AllocatedBlocks) * BlockSize;
		MaxAllocatedMemory = FMath::Max(MaxAllocatedMemory, AllocatedPhysical);

		OutAllocatedBytes = AllocatedPhysical;
		OutPeakAllocatedBytes = MaxAllocatedMemory;
		OutFreeBytes = uint64(MemMapper->FreeBlocks) * BlockSize;
		OutFragmentedDirectMemBlocks = (UPTRINT(MemMapper->End) - UPTRINT(MemMapper->Top)) / sizeof(FDMBlocks);
	}

	static uint64 GetDirectMemoryBytesRemaining()
	{
		return GetInstance()->AvailableDirect;
	}

private:

	void Initialize()
	{
		FScopeLock Sentry(&CritSection);
		if (Base)
		{
			return;
		}

		off_t DMBase;
		uint64 StackBlocks = ((AvailableDirect + (BlockSize - 1)) / BlockSize);
		GMappingSpaceSize = Align(StackBlocks * sizeof(FDMBlocks), 16384); // [BlockSize]k block count
		GMappingSpaceSize += VIRTUAL_ADDRESS_HASH_SIZE * 2 * sizeof(FVAEntry);

		PrivateAllocateDirectMemory(GMappingSpaceSize, SCE_KERNEL_WB_ONION, DMBase, 0, false);

		int32 Result = sceKernelMapDirectMemory(
			(void**)&Base,
			GMappingSpaceSize,
			SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE,
			SCE_KERNEL_MAP_NO_OVERWRITE,
			DMBase,
			0);
		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Fatal, TEXT("Failed to map direct memory, error = %d"), Result);
		}
		VAHashTable = (FVAEntry*)(Base + StackBlocks);
		FMemory::Memzero(VAHashTable, VIRTUAL_ADDRESS_HASH_SIZE * 2 * sizeof(FVAEntry));
		FreeHashEntryNodes = (FVAFreeNode*)(VAHashTable + VIRTUAL_ADDRESS_HASH_SIZE);
		FreeHashEntryNodes->Blocks = VIRTUAL_ADDRESS_HASH_SIZE;
		FreeHashEntryNodes->Next = nullptr;

		// Init an empty stack
		End = Base + StackBlocks;
		Top = End;
		Merge = Top;
		Compare = Top;

		AllocatedBlocks = 0;
		MaxAllocatedBlocks = 0;
		FreeBlocks = 0;

		UE_LOG(LogPS4, Log, TEXT("PS4 Direct Memory Mapper: Total Bytes %llu, Book keeping bytes %llu, Available Blocks %u\n"), AvailableDirect, GMappingSpaceSize, FreeBlocks);
	}

	bool PrivateAllocateDirectMemory(uint64 Size, int32 MemoryType, off_t& OutDirect, uint32 Alignment, bool bAllowFlexibleFallback)
	{
		FScopeLock Sentry(&CritSection);
		int32 Result = sceKernelAllocateDirectMemory(
			0, // search start
			SCE_KERNEL_MAIN_DMEM_SIZE, // search length
			Size, // amount to allocate
			Alignment, // alignment
			MemoryType, // bus memory type (onion, garlic, etc)
			&OutDirect);

		//flexible is WB_ONION only.
		bAllowFlexibleFallback &= (MemoryType == SCE_KERNEL_WB_ONION);

		bool bGotDirectMem = Result == SCE_OK;
		if (!bGotDirectMem && !bAllowFlexibleFallback)
		{
			const TCHAR* MemoryTypeName = TEXT("Unknown");

			if (MemoryType == SCE_KERNEL_WB_ONION)
			{
				MemoryTypeName = TEXT("Onion");
			}
			else if (MemoryType == SCE_KERNEL_WC_GARLIC)
			{
				MemoryTypeName = TEXT("Garlic");
			}

			//Malloc may itself want to map memory for OS allocs. Dump will want to Malloc for Logging. Unlock to avoid a deadlock during log dumping.
			CritSection.Unlock();

			FPS4PlatformMemory::DumpGPUMemoryStats();
			UE_LOG(LogPS4, Fatal, TEXT("Failed to allocate direct memory, error = 0x%x, Size = %llu, MemoryType = %s"), Result, Size, MemoryTypeName);
			return false;
		}

		if (bGotDirectMem)
		{
			off_t PhysicalAddress;
			size_t AvailableSize;
			sceKernelAvailableDirectMemorySize(0, SCE_KERNEL_MAIN_DMEM_SIZE, 0, &PhysicalAddress, &AvailableSize);
			AvailableSize -= RESERVED_MEMORY_SIZE;
			AvailableDirect = AvailableSize;
		}

		return bGotDirectMem;
	}

	void PrivateReleaseDirectMemory(uint64 Size, off_t MemOffset)
	{
		FScopeLock Sentry(&CritSection);
		int32 Result = sceKernelReleaseDirectMemory(MemOffset, Size);
		if (Result != SCE_OK)
		{
			UE_LOG(LogPS4, Fatal, TEXT("Failed to release direct memory, error = %d"), Result);
			return;
		}

		off_t PhysicalAddress;
		size_t AvailableSize;
		sceKernelAvailableDirectMemorySize(0, SCE_KERNEL_MAIN_DMEM_SIZE, 0, &PhysicalAddress, &AvailableSize);
		AvailableSize -= RESERVED_MEMORY_SIZE;
		AvailableDirect = AvailableSize;

		return;
	}

	void AllocateDirectMemoryChunk(uint64 MinSize, int32 MemoryType, bool bAllowFlexibleFallback)
	{
		FScopeLock Sentry(&CritSection);

		if (AvailableDirect < BlockSize)
		{
			// Nothing left to grab
			return;
		}

		const uint64 BlockSizeMask = ~(BlockSize - 1);
		MinSize = FMath::Max(Align(MinSize, BlockSize), uint64(DEFAULT_DIRECT_MEM_BLOCK_SIZE) & BlockSizeMask);
		uint64 DirectMemRequestSize = FMath::Min(AvailableDirect & BlockSizeMask, MinSize);
		off_t PMBase;
		if (!PrivateAllocateDirectMemory(DirectMemRequestSize, MemoryType, PMBase, 0, bAllowFlexibleFallback))
		{
			return;
		}

		// push the new block on top of the stack
		--Top;
		Merge = Top;
		Compare = Top;
		Top->Blocks = DirectMemRequestSize / BlockSize;
		Top->StartOff = PMBase;

		FreeBlocks += DirectMemRequestSize / BlockSize;
	}

	FORCEINLINE FVAEntry* AllocateEntry()
	{
		check(FreeHashEntryNodes && FreeHashEntryNodes->Blocks >= 1);
		FVAEntry* Free = ((FVAEntry*)FreeHashEntryNodes) + (FreeHashEntryNodes->Blocks - 1);
		--FreeHashEntryNodes->Blocks;
		if (FreeHashEntryNodes->Blocks == 0)
		{
			FreeHashEntryNodes = FreeHashEntryNodes->Next;
		}
		return Free;
	}

	FORCEINLINE void FreeEntry(FVAEntry* Ptr)
	{
		FMemory::Memzero(Ptr, sizeof(FVAEntry));
		FVAFreeNode* Node = (FVAFreeNode*)Ptr;
		Node->Blocks = 1;
		Node->Next = FreeHashEntryNodes;
		FreeHashEntryNodes = Node;
	}

	void InsertVA(void* VirtualAddress, off_t DirectMem, uint32 Blocks, uint32 VAEnd)
	{
		check(!(UPTRINT(VirtualAddress) & ((1 << VIRTUAL_ADDRESS_HASH_SHIFT) - 1)));
		uint16 Key = ((UPTRINT)VirtualAddress >> VIRTUAL_ADDRESS_HASH_SHIFT) & (VIRTUAL_ADDRESS_HASH_SIZE - 1);
		check(Key < VIRTUAL_ADDRESS_HASH_SIZE);
		FVAEntry* Entry = &VAHashTable[Key];

		while (Entry)
		{
			if (Entry->VirtualAddress == nullptr)
			{
				break;
			}
			if (Entry->Next)
			{
				Entry = Entry->Next;
			}
			else
			{
				FVAEntry* NewEntry = AllocateEntry();
				Entry->Next = NewEntry;
				Entry = NewEntry;
				break;
			}
		}

		Entry->DirectMemOff = DirectMem;
		Entry->VirtualAddress = VirtualAddress;
		Entry->Blocks = Blocks;
		Entry->End = VAEnd;
		checkf(Entry->Next == nullptr, TEXT("Fresh entry should have a null next ptr: 0x%p"), Entry->Next);
	}

	void RemoveVA(void* VirtualAddress)
	{
		check(!(UPTRINT(VirtualAddress) & ((1 << VIRTUAL_ADDRESS_HASH_SHIFT) - 1)));
		uint16 Key = ((UPTRINT)VirtualAddress >> VIRTUAL_ADDRESS_HASH_SHIFT) & (VIRTUAL_ADDRESS_HASH_SIZE - 1);
		check(Key < VIRTUAL_ADDRESS_HASH_SIZE);
		FVAEntry* Entry = &VAHashTable[Key];
		FVAEntry* PrevEntry = nullptr;

		while (Entry)
		{
			if (Entry->VirtualAddress == VirtualAddress)
			{
				if (PrevEntry != nullptr)
				{
					PrevEntry->Next = Entry->Next;
					FreeEntry(Entry);
				}
				else
				{
					if (Entry->Next)
					{
						FVAEntry* ToRemove = Entry->Next;
						*Entry = *Entry->Next;
						FreeEntry(ToRemove);
					}
					else
					{
						Entry->VirtualAddress = nullptr;
						Entry->DirectMemOff = 0;
						Entry->End = 0;
						Entry->Blocks = 0;
						Entry->Next = nullptr;
					}
				}
				return;
			}
			PrevEntry = Entry;
			Entry = Entry->Next;
		}
	}

	bool GetDirectMemory(void* VirtualAddress, off_t& OutDirectMem, uint32& OutBlocks, uint32& OutEnd)
	{
		check(!(UPTRINT(VirtualAddress) & ((1 << VIRTUAL_ADDRESS_HASH_SHIFT) - 1)));
		uint16 Key = ((UPTRINT)VirtualAddress >> VIRTUAL_ADDRESS_HASH_SHIFT) & (VIRTUAL_ADDRESS_HASH_SIZE - 1);
		check(Key < VIRTUAL_ADDRESS_HASH_SIZE);
		FVAEntry* Entry = &VAHashTable[Key];

		while (Entry)
		{
			if (Entry->VirtualAddress == VirtualAddress)
			{
				OutDirectMem = Entry->DirectMemOff;
				OutBlocks = Entry->Blocks;
				OutEnd = Entry->End;
				return true;
			}
			Entry = Entry->Next;
		}

		check(Entry);
		return false;
	}

	void MergeDMStack(uint32 NumSteps)
	{
		/*
			Over a long run Top to End will become fragmented, so attempt to merge continuous blocks together.
			This is O(n^2) algorithm but we do it in small steps to not stall too much. Fragmentation
			won't prevent an allocation but it will slow down the calls to Mmap() and Unmap().
		*/
		if (AllocatedBlocks == 0 || Top == (End - 1))
		{
			return;
		}
		FScopeLock Sentry(&CritSection);
		
		NumSteps = FMath::Min<UPTRINT>(NumSteps, (UPTRINT(End) - UPTRINT(Top)) / sizeof(FDMBlocks));
		if (Merge < Top || Compare < Top)
		{
			Compare = Top;
			Merge = Top;
		}

		if (Merge == End)
		{
			Merge = Top;
		}

		if (Compare == End)
		{
			Compare = Top;
		}

		while (NumSteps)
		{
			if (Merge->StartOff + (Merge->Blocks * BlockSize) == Compare->StartOff)
			{
				Compare->StartOff = Merge->StartOff;
				Compare->Blocks += Merge->Blocks;
				
				*Merge = *Top;
				++Top;
				Compare = Top;
				Merge = Top;
			}
			else if (Compare->StartOff + (Compare->Blocks * BlockSize) == Merge->StartOff)
			{
				Compare->Blocks += Merge->Blocks;
				
				*Merge = *Top;
				++Top;
				Compare = Top;
				Merge = Top;
			}
			else
			{
				++Compare;
				if (Compare == End)
				{
					++Merge;
					Compare = Top;
				}
				if (Merge == End)
				{
					Merge = Top;
				}
			}
			--NumSteps;
		}
	}

	void* PrivateMmap(size_t Len, uint32 Alignment)
	{
		FScopeLock Sentry(&CritSection);

		check((Len & (BlockSize - 1)) == 0);

		// Done first time init?
		if (!Base)
		{
			Initialize();
		}

		void* VirtAddr = nullptr;
		int32 Result = sceKernelReserveVirtualRange(&VirtAddr, Len, SCE_KERNEL_MAP_NO_OVERWRITE, Alignment);
		if (Result != SCE_OK)
		{
			return nullptr;
		}

		// clamp off the number of blocks to Alloca so we don't blow the stack on large allocations.
		uint32 BlockCount = Len / BlockSize;
		const uint32 MAX_STACK_BLOCKS = 32;
		uint32 AllocedBlockCount = FMath::Min(BlockCount, MAX_STACK_BLOCKS);
		SceKernelBatchMapEntry* Blocks = (SceKernelBatchMapEntry*)FMemory_Alloca(AllocedBlockCount * sizeof(SceKernelBatchMapEntry));
		uint32 UsedBlocks = 0;

		bool bUseDirectChunk = true;

		void* Addr = VirtAddr;
		while (Len > 0)
		{
			if (Top == End)
			{
				// try to allocate direct memory, but allow fallback if we can't.
				AllocateDirectMemoryChunk(Len, SCE_KERNEL_WB_ONION, true);
				if (Top == End)
				{
					// indicate that we should fall back to flexible memory.
					bUseDirectChunk = false;
				}
			}

			if (Len > 0)
			{
				if (bUseDirectChunk)
				{
					uint32 Taken = FMath::Min(Top->Blocks, BlockCount);
					off_t DMoffset = Top->StartOff;
					Blocks[UsedBlocks].start = Addr;
					Blocks[UsedBlocks].length = Taken * BlockSize;
					Blocks[UsedBlocks].offset = DMoffset;
					Blocks[UsedBlocks].protection = SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE;
					Blocks[UsedBlocks].operation = SCE_KERNEL_MAP_OP_MAP_DIRECT;

					Top->Blocks -= Taken;
					Top->StartOff += Blocks[UsedBlocks].length;
					if (Top->Blocks == 0)
					{
						Top->StartOff = 0;
						++Top;
					}

					AllocatedBlocks += Taken;
					MaxAllocatedBlocks = FMath::Max(AllocatedBlocks, MaxAllocatedBlocks);
					FreeBlocks -= Taken;
					BlockCount -= Taken;
					Len -= Blocks[UsedBlocks].length;

					InsertVA(Addr, DMoffset, Taken, Len == 0 ? 1 : 0);

					Addr = ((uint8*)Addr) + Blocks[UsedBlocks].length;
					++UsedBlocks;
				}
#if PS4_USE_FLEXIBLE_MEMORY
				else
				{
					// we ran out of direct physical memory, so allocate the remainder as a single flexible mapping if possible.
					Result = sceKernelMapFlexibleMemory(&Addr, Len, SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE, SCE_KERNEL_MAP_FIXED);
					if (Result != SCE_OK)
					{
						// this was our last chance to allocate the memory we needed.  return if we couldn't.
						// Note - UE_LOG can allocate and cause things to go recursive, so use LLO
						FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\r\nsceKernelMapFlexibleMemory of size %i failed with 0x%x\r\n"), Len, Result);
						return nullptr;
					}
					InsertVA(Addr, 0, Len, 1);
					Len = 0;
					LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Addr, Len));
				}
#endif
			}

			// only have a limited number of block structures, so map and reuse when we're full.
			if (UsedBlocks == AllocedBlockCount)
			{
				int32 Completed;
				Result = sceKernelBatchMap(Blocks, UsedBlocks, &Completed);
				if (Result != SCE_OK || Completed != UsedBlocks)
				{
					return nullptr;
				}
				UsedBlocks = 0;
			}
		}

		// map any stragglers.
		if (UsedBlocks > 0)
		{
			int32 Completed;
			Result = sceKernelBatchMap(Blocks, UsedBlocks, &Completed);
			if (Result != SCE_OK || Completed != UsedBlocks)
			{
				return nullptr;
			}
		}

		// De-frag the stack a little
		MergeDMStack(16);

		check((UPTRINT(VirtAddr) & UPTRINT(Alignment - 1)) == 0);
		return VirtAddr;
	}

	void PrivateUnmap(void* Addr)
	{
		FScopeLock Sentry(&CritSection);

		SceKernelBatchMapEntry Blocks[128];
		int32 Completed, Result, BlocksUsed = 0;
		
		uint32 EndMarker = 0;
		do
		{
			uint32 BlockCount;
			UPTRINT AddrLen;
			off_t Offset;
			bool ok = GetDirectMemory(Addr, Offset, BlockCount, EndMarker);
			check(ok);
#if PS4_USE_FLEXIBLE_MEMORY
			if (Offset != 0)
#endif
			{
				AddrLen = BlockSize * BlockCount;
				// These lines can be used to verify that Mapping from GetDirectMemory are correct
				//   SceKernelVirtualQueryInfo VMInfo;
				//   Result = sceKernelVirtualQuery(Addr, 0, &VMInfo, sizeof(SceKernelVirtualQueryInfo));
				//   check(VMInfo.offset + ((UPTRINT)Addr - (UPTRINT)VMInfo.start) == Offset && AddrLen <= ((UPTRINT)VMInfo.end - (UPTRINT)Addr));

				if (BlocksUsed + 1 >= ARRAY_COUNT(Blocks))
				{
					// batch unmap
					Result = sceKernelBatchMap(Blocks, BlocksUsed, &Completed);
					check(Result == SCE_OK);
					BlocksUsed = 0;
				}
				Blocks[BlocksUsed].start = Addr;
				Blocks[BlocksUsed].offset = Offset;
				Blocks[BlocksUsed].length = AddrLen;
				Blocks[BlocksUsed].protection = 0;
				Blocks[BlocksUsed].operation = SCE_KERNEL_MAP_OP_UNMAP;
				++BlocksUsed;

				RemoveVA(Addr);
				Addr = ((uint8*)Addr) + AddrLen;
				--Top;
				check(Top >= Base);
				uint32 FreedBlocks = AddrLen / BlockSize;
				Top->Blocks = FreedBlocks;
				Top->StartOff = Offset;

				AllocatedBlocks -= FreedBlocks;
				FreeBlocks += FreedBlocks;
			}
#if PS4_USE_FLEXIBLE_MEMORY
			else
			{
				AddrLen = BlockCount;
				Result = sceKernelMunmap(Addr, AddrLen);
				checkf(Result == SCE_OK, TEXT("sceKernelMunmap failed: 0x%x"), Result);
				RemoveVA(Addr);
				Addr = ((uint8*)Addr) + AddrLen;
			}
#endif
		} while (!EndMarker);

		if (BlocksUsed != 0)
		{
			Result = sceKernelBatchMap(Blocks, BlocksUsed, &Completed);
			check(Result == SCE_OK);
		}
		// De-frag the stack a little
		// Frees will fragment so call with more steps
		MergeDMStack(48);
	}

	static uint64 MaxAllocatedMemory;
};

uint64 FPS4DirectMemoryMapper::MaxAllocatedMemory = 0;

static void PS4MemoryApplyPoolSize(const TCHAR* NeoString, const TCHAR* BaseString, uint64& Value)
{
	int32 BaseHeapSizeMB;
	uint64 BaseHeapSizeBytes;
	if (GConfig->GetInt(TEXT("SystemSettings"), BaseString, BaseHeapSizeMB, GEngineIni))
	{
		// Use value from config file
		BaseHeapSizeBytes = uint64(BaseHeapSizeMB) * 1024ull * 1024ull;
		Value = BaseHeapSizeBytes;
	}
	else
	{
		// Not provided in the config. Use the default (compiled in) value.
		BaseHeapSizeBytes = Value;
	}

	// Test for Neo override...
	int32 NeoHeapSizeMB;
	uint64 NeoHeapSizeBytes;
	if (!GConfig->GetInt(TEXT("SystemSettings"), NeoString, NeoHeapSizeMB, GEngineIni))
	{
		NeoHeapSizeMB = 0;
	}

		NeoHeapSizeBytes = uint64(NeoHeapSizeMB) * 1024ull * 1024ull;
	if (NeoHeapSizeBytes == 0)
	{
		NeoHeapSizeBytes = BaseHeapSizeBytes;
		UE_LOG(LogInit, Warning, TEXT("%s is not specified, or is set to zero. Neo kits will use the same %s as base kits (%d MB)."), NeoString, BaseString, BaseHeapSizeMB);
	}

		ensure(NeoHeapSizeBytes >= BaseHeapSizeBytes);
		if (!!sceKernelIsNeoMode())
		{
			Value = NeoHeapSizeBytes;
		}
}

void FPS4PlatformMemory::Init()
{
	LLM(PS4LLM::Initialise());

	GPUStatDumpFunc = nullptr;

	PS4MemoryApplyPoolSize(TEXT("r.GarlicHeapSizeMB_Neo"), TEXT("r.GarlicHeapSizeMB"), GGarlicHeapSize);
	PS4MemoryApplyPoolSize(TEXT("r.OnionHeapSizeMB_Neo"), TEXT("r.OnionHeapSizeMB"), GOnionHeapSize);

	GMainHeapSize = SCE_KERNEL_MAIN_DMEM_SIZE - RESERVED_MEMORY_SIZE - GMappingSpaceSize - PREMAIN_HEAP_SIZE - GOnionHeapSize - GGarlicHeapSize;

	FGenericPlatformMemory::Init();

	// set the pool size
	SET_MEMORY_STAT(MCR_GPUSystem, GOnionHeapSize);
	SET_MEMORY_STAT(MCR_GPU, GGarlicHeapSize);
	SET_MEMORY_STAT(MCR_Physical, GMainHeapSize + FLEXIBLE_MEMORY_SIZE);
	SET_MEMORY_STAT(MCR_PhysicalLLM, SCE_KERNEL_MAIN_DMEM_SIZE);
	SET_MEMORY_STAT(STAT_TotalFlexible, FLEXIBLE_MEMORY_SIZE);

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	UE_LOG(LogInit, Log, TEXT("Memory sizes: Heap - %.2fMB : Onion - %.2fMB : Garlic - %.2fMB, Flexible - %.2fMB, Physical=%.1fGB (%dGB approx) \n"),
		(double(GMainHeapSize)                 / (1024.0 * 1024.0)),
		(double(GOnionHeapSize)                / (1024.0 * 1024.0)),
		(double(GGarlicHeapSize)               / (1024.0 * 1024.0)),
		(double(GFlexibleMemoryAtStart)        / (1024.0 * 1024.0)),
		(double(MemoryConstants.TotalPhysical) / (1024.0 * 1024.0 * 1024.0)),
		MemoryConstants.TotalPhysicalGB);

}

FMalloc* FPS4PlatformMemory::BaseAllocator()
{
#if FORCE_ANSI_ALLOCATOR
	static FMalloc* MallocInstance = FGenericPlatformMemory::BaseAllocator();
	return MallocInstance;
#else
#if USE_MALLOC_BINNED2
	static FMallocBinned2 MallocBinned2;
	return &MallocBinned2;
#else
	// if possible and desired, use the Binned allocator
	// First parameter is page size, all allocs from BinnedAllocFromOS() MUST be aligned to this size
	// page size isn't a true page size, we can allocate in multiples of 16k (which is closer to a true page size), but 16k is too small to be efficient.
	// Also, 64k pages offer a in-game framerate increase over 16k!
	// Second parameter is estimate of the range of addresses expected to be returns by BinnedAllocFromOS(). Binned
	// Malloc will adjust it's internal structures to make look ups for memory allocations O(1) for this range. 
	// It's is ok to go outside this range, look ups will just be a little slower. Our initial estimate is 4GB
	static FMallocBinned MallocBinned(uint32(GetConstants().BinnedPageSize), (uint64)MAX_uint32 + 1);
	return &MallocBinned;
#endif
#endif
}


const FPlatformMemoryConstants& FPS4PlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if (MemoryConstants.TotalPhysical == 0)
	{
		size_t FlexibleMemSize;
		int32 Ret = sceKernelAvailableFlexibleMemorySize(&FlexibleMemSize);
		GFlexibleMemoryAtStart = FlexibleMemSize;

		// Gather platform memory stats.
		MemoryConstants.TotalPhysical = SCE_KERNEL_MAIN_DMEM_SIZE + FLEXIBLE_MEMORY_SIZE;
		MemoryConstants.TotalPhysicalGB = FMath::DivideAndRoundDown(uint64(MemoryConstants.TotalPhysical), 1024ull * 1024ull * 1024ull);
#if !FORCE_ANSI_ALLOCATOR
		MemoryConstants.BinnedPageSize = PS4_PSEUDOPAGESIZE;
		MemoryConstants.BinnedAllocationGranularity = FPS4DirectMemoryMapper::BlockSize;
		MemoryConstants.PageSize = PS4_PSEUDOPAGESIZE;
#endif
		MemoryConstants.TotalVirtual = SCE_KERNEL_APP_MAP_AREA_SIZE;
	}

	return MemoryConstants;
}


extern "C"
{
#include <mspace.h>
}


struct FMspaceHeap
{
	SceLibcMspace Mspace;
	UPTRINT		  BaseAddress;
	UPTRINT		  EndAddress;
};


FMspaceHeap GMspace[2];

extern "C" int user_malloc_stats_fast(SceLibcMallocManagedSize*);

FPlatformMemoryStats FPS4PlatformMemory::GetStats()
{
	uint64 DirectAllocated;
	uint64 DirectPeakAllocated;
	uint64 DirectFree;
	uint64 DirectFragmentedDMBlocks;
	FPS4DirectMemoryMapper::GetStats(DirectAllocated, DirectPeakAllocated, DirectFree, DirectFragmentedDMBlocks);

	size_t AvailableFlexible;
	sceKernelAvailableFlexibleMemorySize(&AvailableFlexible);
	uint64 UsedFlexible = FLEXIBLE_MEMORY_SIZE - AvailableFlexible;

	FPlatformMemoryStats Stats;
	Stats.UsedPhysical      = PREMAIN_HEAP_SIZE + GGarlicHeapSize + GOnionHeapSize + GMappingSpaceSize + UsedFlexible + DirectAllocated LLM(+LLMMallocTotal);
	Stats.PeakUsedPhysical  = PREMAIN_HEAP_SIZE + GGarlicHeapSize + GOnionHeapSize + GMappingSpaceSize + UsedFlexible + DirectPeakAllocated;
	Stats.AvailablePhysical = Stats.TotalPhysical - Stats.UsedPhysical;

	Stats.Direct   = DirectAllocated;
	Stats.Garlic   = FPlatformAtomics::AtomicRead(&GPS4Allocated_MemBlockGarlic);
	Stats.Onion    = FPlatformAtomics::AtomicRead(&GPS4Allocated_MemBlockOnion);
	Stats.Flexible = UsedFlexible;

	Stats.FlexibleHeapSize = FLEXIBLE_MEMORY_SIZE;
	Stats.DirectHeapSize = GMainHeapSize;
	Stats.GarlicHeapSize = GGarlicHeapSize;
	Stats.OnionHeapSize = GOnionHeapSize;

#if USE_DEFRAG_ALLOCATOR
	int64 DefragUsedSize;
	int64 DefragAvailableSize;
	int64 DefragPendingAdjustment;
	int64 DefragPaddingWaste;
	int64 DefragTotalSize;
	extern void PS4GetDefragMemoryStats(int64& OutUsedSize, int64& OutAvailableSize, int64& OutPendingAdjustment, int64& OutPaddingWaste, int64& OutTotalSize);
	PS4GetDefragMemoryStats(DefragUsedSize, DefragAvailableSize, DefragPendingAdjustment, DefragPaddingWaste, DefragTotalSize);

	Stats.DefragHeapSize = DefragTotalSize;
	Stats.DefragUsed = DefragUsedSize;
	Stats.DefragFree = DefragAvailableSize;
	Stats.DefragWasted = DefragPaddingWaste;
#endif

	return Stats;
}

void FPS4PlatformMemory::DumpStats(FOutputDevice& Ar)
{
	FGenericPlatformMemory::DumpStats(Ar);

	const float InvMB = 1.0f / 1024.0f / 1024.0f;
#if !NO_LOGGING
	const FName CategoryName(LogMemory.GetCategoryName());
#else
	const FName CategoryName(TEXT("LogMemory"));
#endif

	size_t FlexibleMemSize;
	sceKernelAvailableFlexibleMemorySize(&FlexibleMemSize);

	const uint64 AvailableFlexible = FlexibleMemSize;
	const uint64 AllocatedFlexible = GFlexibleMemoryAtStart - FlexibleMemSize;
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Flexible Memory: %.2f MB available, %.2f MB allocated since boot"), AvailableFlexible * InvMB, AllocatedFlexible * InvMB);

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Memory sizes\n\tHeap:%.2f MB, Onion: %.2f MB, Garlic: %.2f MB, Flexible: %.2f MB, Physical: %.1f MB (%dGB approx) \n"),
		(float)((double)GMainHeapSize / (1024.0 * 1024.0)),
		(float)((double)GOnionHeapSize / (1024.0 * 1024.0)),
		(float)((double)GGarlicHeapSize / (1024.0 * 1024.0)),
		(float)((double)GFlexibleMemoryAtStart / (1024.0 * 1024.0)),
		((double)MemoryConstants.TotalPhysical / 1024.0f / 1024.0f), MemoryConstants.TotalPhysicalGB);
}

void FPS4PlatformMemory::OnOutOfMemory(uint64 Size, uint32 Alignment)
{
#if FORCE_ANSI_ALLOCATOR
	SceLibcMallocManagedSize ManagedSize;
	SCE_LIBC_INIT_MALLOC_MANAGED_SIZE(ManagedSize);
	user_malloc_stats_fast(&ManagedSize);

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("PS4 OOM: CurrentSystemSize: %lld, CurrentInUseSize: %lld, MaxSystemSize: %lld, MaxInUseSize: %lld\n"),
		ManagedSize.currentSystemSize, ManagedSize.currentInuseSize, ManagedSize.maxSystemSize, ManagedSize.maxInuseSize);
#else

	FPlatformMemoryStats Stats = GetStats();

	FPlatformMisc::LowLevelOutputDebugStringf(
		TEXT("\r\nPS4 OOM:\r\n")
		TEXT("    AvailablePhysical: %lld\r\n")
		TEXT("    UsedPhysical:      %lld\r\n")
		TEXT("    Direct:            %lld\r\n")
		TEXT("    Garlic:            %lld\r\n")
		TEXT("    Onion:             %lld\r\n")
		TEXT("    Flexible:          %lld\r\n")
#if USE_DEFRAG_ALLOCATOR
		TEXT("    DefragUsed:        %lld\r\n")
		TEXT("    DefragFree:        %lld\r\n")
		TEXT("    DefragWasted:      %lld\r\n")
#endif
		TEXT("\r\n")
		, Stats.AvailablePhysical
		, Stats.UsedPhysical
		, Stats.Direct
		, Stats.Garlic
		, Stats.Onion
		, Stats.Flexible
#if USE_DEFRAG_ALLOCATOR
		, Stats.DefragUsed
		, Stats.DefragFree
		, Stats.DefragWasted
#endif
	);

#endif

	FGenericPlatformMemory::OnOutOfMemory(Size, Alignment);
}


void* FPS4PlatformMemory::BinnedAllocFromOS(SIZE_T Size)
{
	void* Addr = FPS4DirectMemoryMapper::Mmap(Size, PS4_PSEUDOPAGESIZE);
	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Addr, Align(Size, FPS4DirectMemoryMapper::BlockSize)));
	return Addr;
}


void FPS4PlatformMemory::BinnedFreeToOS(void* Ptr, SIZE_T Size)
{
	LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));
	FPS4DirectMemoryMapper::Unmap(Ptr);
}

void FPS4PlatformMemory::InternalUpdateStats(const FPlatformMemoryStats& MemoryStats)
{
	size_t FlexibleMemSize;
	sceKernelAvailableFlexibleMemorySize(&FlexibleMemSize);
	SET_MEMORY_STAT(STAT_AvailableFlexible, (uint32)FlexibleMemSize);
	SET_MEMORY_STAT(STAT_UsedFlexible, (uint32)GFlexibleMemoryAtStart - FlexibleMemSize);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	// update LLM program size
	int64 UsedFlexible = FLEXIBLE_MEMORY_SIZE - FlexibleMemSize;
	if (!GEstimatedProgramSize || UsedFlexible < GEstimatedProgramSize)
	{
		GEstimatedProgramSize = UsedFlexible;
		FLowLevelMemTracker::Get().SetProgramSize(UsedFlexible);
	}
#endif
}


/**
 * Direct-memory allocate one huge block. Helper for GPU heaps.
 *
 * @param Size The amount of memory to allocate.
 * @param BusType The memory bus type.
 * @param Protection The desired memory protection level.
 */
void* MapLargeBlock(uint64 Size, int BusType, int Protection, off_t& OutMemOffset)
{
	const int32 LARGE_BLOCK_ALIGNMENT = 2 * 1024 * 1024;
	void* BlockBase = nullptr;

	if (!FPS4DirectMemoryMapper::AllocateDirectMemory(Size, BusType, OutMemOffset, LARGE_BLOCK_ALIGNMENT))
	{
		return nullptr;
	}

	// map the memory
	int Result = sceKernelMapDirectMemory( &BlockBase,				// receives the CPU/GPU-accessible pointer
										   Size,					// all of the direct memory we just allocated
										   Protection,				// protections
										   0,						// no flags
										   OutMemOffset,			// the offset we allocated above
										   LARGE_BLOCK_ALIGNMENT	// same alignment as above
										 );

	if (Result != 0)
	{
		UE_LOG(LogPS4, Fatal, TEXT("Failed to map direct memory, error = %d"), Result);
	}

	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, (void*)OutMemOffset, Size));

	return BlockBase;
}

/**
* Direct-memory release one huge block. Helper for GPU heaps.
*
* @param MemOffset The offset returned from the original allocation.
* @param Size The amount of memory to free.  Should be the same amount allocated.
*/
void ReleaseLargeBlock(uint64 Size, off_t MemOffset)
{
	LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, reinterpret_cast<void*>(MemOffset)));

	FPS4DirectMemoryMapper::ReleaseDirectMemory(Size, MemOffset);
}

static bool GAllowBaseAllocatorFromLibcReplacement = false;

void AllocateRemainingPhysicalMemoryForCPU()
{
#if FORCE_ANSI_ALLOCATOR
	uint64 HeapSize = FPS4DirectMemoryMapper::GetDirectMemoryBytesRemaining();
	void* MainHeapBase = MapLargeBlock(HeapSize,
		SCE_KERNEL_WB_ONION,
		SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_ALL);
	// create the Mspace object to wrap the large buffer
	if (!GMspace[0].Mspace)
	{
		GMspace[0].BaseAddress = (UPTRINT)MainHeapBase;
		GMspace[0].EndAddress = (UPTRINT)MainHeapBase + HeapSize;
		SceLibcMspace MSpace = sceLibcMspaceCreate("User Malloc Main", MainHeapBase, HeapSize, 0);
		FPlatformAtomics::InterlockedExchangePtr(&GMspace[0].Mspace, MSpace);
	}
#else
	FPS4DirectMemoryMapper::ReserveRemaining();
	GAllowBaseAllocatorFromLibcReplacement = true;
#endif
}


extern "C"
{
	//////////////////////////////////////////////////////////////////////
	// Malloc replacements, to come out of a preallocate block
	//////////////////////////////////////////////////////////////////////

	int user_malloc_init(void)
	{
		LLM_PLATFORM_SCOPE_PS4(ELLMTagPS4::MallocPool);

		off_t MemOffset;
		void* MainHeapBase = MapLargeBlock(PREMAIN_HEAP_SIZE,
			SCE_KERNEL_WB_ONION,
			SCE_KERNEL_PROT_CPU_READ | SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_ALL,
			MemOffset);

		// create the Mspace object to wrap the large buffer
		GMspace[1].BaseAddress = (UPTRINT)MainHeapBase;
		GMspace[1].EndAddress = (UPTRINT)MainHeapBase + PREMAIN_HEAP_SIZE;
		GMspace[1].Mspace = sceLibcMspaceCreate("User Malloc Reserve", MainHeapBase, PREMAIN_HEAP_SIZE, 0);
		return GMspace[1].Mspace == nullptr;
	}

	int user_malloc_finalize(void)
	{
		for (uint32 i = 0; i < ARRAY_COUNT(GMspace); ++i)
		{
			if (GMspace[i].Mspace)
			{
				sceLibcMspaceDestroy(GMspace[i].Mspace);
			}
		}
		return 0;
	}

	void *user_malloc(size_t size)
	{
		void* ptr;
		for (uint32 i = 0; i < ARRAY_COUNT(GMspace); ++i)
		{
			if (GMspace[i].Mspace)
			{
				ptr = sceLibcMspaceMalloc(GMspace[i].Mspace, size);
				if (ptr)
				{
					LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, ptr, size, (ELLMTag)ELLMTagPS4::Malloc));
					return ptr;
				}
			}
		}

		checkf(GMspace[0].Mspace || GAllowBaseAllocatorFromLibcReplacement, TEXT("Out of Memory before second pool was created. Increase ANSI_STARTUP_MEM_POOL_SIZE in PS4Memory.cpp to avoid this."));

		if (GAllowBaseAllocatorFromLibcReplacement)
		{
			return FPS4PlatformMemory::BaseAllocator()->Malloc(size);
		}

		return nullptr;
	}

	void user_free(void *ptr)
	{
		for (uint32 i = 0; i < ARRAY_COUNT(GMspace); ++i)
		{
			if (GMspace[i].Mspace && (UPTRINT)ptr >= GMspace[i].BaseAddress && (UPTRINT)ptr < GMspace[i].EndAddress)
			{
				sceLibcMspaceFree(GMspace[i].Mspace, ptr);
				LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, ptr));
				return;
			}
		}

		if (ptr && GAllowBaseAllocatorFromLibcReplacement)
		{
			FPS4PlatformMemory::BaseAllocator()->Free(ptr);
		}
	}

	void *user_calloc(size_t nelem, size_t size)
	{
		void* ptr = nullptr;
		for (uint32 i = 0; i < ARRAY_COUNT(GMspace); ++i)
		{
			if (GMspace[i].Mspace)
			{
				ptr = sceLibcMspaceCalloc(GMspace[i].Mspace, nelem, size);
				if (ptr)
				{
					LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, ptr, size, (ELLMTag)ELLMTagPS4::Malloc));
					return ptr;
				}
			}
		}

		if (GAllowBaseAllocatorFromLibcReplacement)
		{
			const size_t alloc_size = nelem * size;
			ptr = FPS4PlatformMemory::BaseAllocator()->Malloc(alloc_size);
			FMemory::Memzero(ptr, alloc_size);
		}
		return ptr;
	}

	void *user_realloc(void *ptr, size_t size)
	{
		void* newptr;
		for (uint32 i = 0; i < ARRAY_COUNT(GMspace); ++i)
		{
			if (GMspace[i].Mspace && (!ptr || ((UPTRINT)ptr >= GMspace[i].BaseAddress && (UPTRINT)ptr < GMspace[i].EndAddress)))
			{
				newptr = sceLibcMspaceRealloc(GMspace[i].Mspace, ptr, size);
				if (!newptr)
				{// if it failed, try again from another heap
					newptr = user_malloc(size);
					if (newptr)
					{
						memcpy(newptr, ptr, sceLibcMspaceMallocUsableSize(ptr));
						user_free(ptr);
					}
				}
				LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, ptr));
				if (newptr)
				{
					LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, newptr, size, (ELLMTag)ELLMTagPS4::Malloc));
				}
				return newptr;
			}
		}

		if (GAllowBaseAllocatorFromLibcReplacement)
		{
			return FPS4PlatformMemory::BaseAllocator()->Realloc(ptr, size);
		}
		return nullptr;
	}

	void *user_memalign(size_t boundary, size_t size)
	{
		void* ptr;
		for (uint32 i = 0; i < ARRAY_COUNT(GMspace); ++i)
		{
			if (GMspace[i].Mspace)
			{
				ptr = sceLibcMspaceMemalign(GMspace[i].Mspace, boundary, size);
				if (ptr)
				{
					LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, ptr, size, (ELLMTag)ELLMTagPS4::Malloc));
					return ptr;
				}
			}
		}

		if (GAllowBaseAllocatorFromLibcReplacement)
		{
			return FPS4PlatformMemory::BaseAllocator()->Malloc(size, boundary);
		}

		return nullptr;
	}

	int user_posix_memalign(void **ptr, size_t boundary, size_t size)
	{
		for (uint32 i = 0; i < ARRAY_COUNT(GMspace); ++i)
		{
			if (GMspace[i].Mspace)
			{
				LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, ptr, size, (ELLMTag)ELLMTagPS4::Malloc));
				int32 Result = sceLibcMspacePosixMemalign(GMspace[i].Mspace, ptr, boundary, size);
				if (Result == ENOMEM)
				{
					// Allocation failed in this mspace. Try another one.
					continue;
				}
				else
				{
					// Success, or non out-of-memory error codes.
					return Result;
				}
			}
		}

		if (GAllowBaseAllocatorFromLibcReplacement)
		{
			// Check if the boundary is a power of 2 and that it is a multiple sizeof(void*)
			if (!FMath::IsPowerOfTwo(boundary) || (sizeof(void*) % boundary != 0))
			{
				return EINVAL;
			}

			// Alloc using base allocator
			void* PtrTemp = FPS4PlatformMemory::BaseAllocator()->Malloc(size, boundary);
			if (PtrTemp)
			{
				*ptr = PtrTemp;
				// 0 if succeeded
				return 0;
			}
		}

		return ENOMEM;
	}

	void *user_reallocalign(void *ptr, size_t size, size_t boundary)
	{
		void* newptr;
		for (uint32 i = 0; i < ARRAY_COUNT(GMspace); ++i)
		{
			if (GMspace[i].Mspace && (!ptr || ((UPTRINT)ptr >= GMspace[i].BaseAddress && (UPTRINT)ptr < GMspace[i].EndAddress)))
			{
				newptr = sceLibcMspaceReallocalign(GMspace[i].Mspace, ptr, boundary, size);
				if (!newptr)
				{// if it failed, try again from another heap
					newptr = user_memalign(boundary, size);
					if (newptr)
					{
						memcpy(newptr, ptr, sceLibcMspaceMallocUsableSize(ptr));
						user_free(ptr);
					}
				}
				LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, ptr));
				if (newptr)
				{
					LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, newptr, size, (ELLMTag)ELLMTagPS4::Malloc));
				}
				return newptr;
			}
		}

		if (GAllowBaseAllocatorFromLibcReplacement)
		{
			return FPS4PlatformMemory::BaseAllocator()->Realloc(ptr, size, boundary);
		}

		return nullptr;
	}

	int user_malloc_stats(SceLibcMallocManagedSize *mmsize)
	{
		for (uint32 i = 0; i < ARRAY_COUNT(GMspace); ++i)
		{
			if (GMspace[i].Mspace)
			{
				SceLibcMallocManagedSize Size;
				SCE_LIBC_INIT_MALLOC_MANAGED_SIZE(Size);
				sceLibcMspaceMallocStats(GMspace[i].Mspace, &Size);
				mmsize->size = Size.size;
				mmsize->version = Size.version;
				mmsize->maxSystemSize += Size.maxSystemSize;
				mmsize->currentSystemSize += Size.currentSystemSize;
				mmsize->maxInuseSize += Size.maxInuseSize;
				mmsize->currentInuseSize += Size.currentInuseSize;
			}
		}
		return 0;
	}

	int user_malloc_stats_fast(SceLibcMallocManagedSize *mmsize)
	{
		for (uint32 i = 0; i < ARRAY_COUNT(GMspace); ++i)
		{
			if (GMspace[i].Mspace)
			{
				SceLibcMallocManagedSize Size;
				SCE_LIBC_INIT_MALLOC_MANAGED_SIZE(Size);
				sceLibcMspaceMallocStatsFast(GMspace[i].Mspace, &Size);
				mmsize->size = Size.size;
				mmsize->version = Size.version;
				mmsize->maxSystemSize += Size.maxSystemSize;
				mmsize->currentSystemSize += Size.currentSystemSize;
				mmsize->maxInuseSize += Size.maxInuseSize;
				mmsize->currentInuseSize += Size.currentInuseSize;
			}
		}
		return 0;
	}

	size_t user_malloc_usable_size(void *ptr)
	{
		return sceLibcMspaceMallocUsableSize(ptr);
	}

};


#include <new>
#include <cstdlib>
#include <cstdio>

void *user_new(std::size_t size) throw(std::bad_alloc)
{
	void *ptr;

	if (size == 0)
	{
		size = 1;
	}

	// pass along to malloc
	return std::malloc(size);
}

void *user_new(std::size_t size, const std::nothrow_t& x) throw()
{
	void *ptr;

	if (size == 0)
	{
		size = 1;
	}

	return std::malloc(size);
}

void *user_new_array(std::size_t size) throw(std::bad_alloc)
{
	return user_new(size);
}

void *user_new_array(std::size_t size, const std::nothrow_t& x) throw()
{
	return user_new(size, x);
}

void user_delete(void *ptr) throw()
{
	if (ptr != nullptr)
	{
		std::free(ptr);
	}
}

void user_delete(void *ptr, const std::nothrow_t& x) throw()
{
	user_delete(ptr);
}

void user_delete_array(void *ptr) throw()
{
	user_delete(ptr);
}

void user_delete_array(void *ptr, const std::nothrow_t& x) throw()
{
	user_delete(ptr, x);
}

/**
 * Return true if the title has access to extra debug memory
 */

/**
* Return true if the title has access to extra debug memory
* This function is not robust and should only be used for debug code. It relies
* on hard coded values for the debug memory amounts which could change.
*/
#if !UE_BUILD_SHIPPING	// This function is not guaranteed to be correct and should not be called in shipping builds
bool FPS4PlatformMemory::IsExtraDevelopmentMemoryAvailable()
{
	int64 DMemSize = SCE_KERNEL_MAIN_DMEM_SIZE;
	const int64 PS4LargeMemorySize = 5376LL * 1024 * 1024;
	const int64 NeoLargeMemorySize = 10240LL * 1024 * 1024;
	return
		FPS4Misc::IsRunningOnDevKit() &&
		((!sceKernelIsNeoMode() && DMemSize == PS4LargeMemorySize) ||
		(sceKernelIsNeoMode() && DMemSize == NeoLargeMemorySize));
}
#endif

/**
* LLM uses these low level functions (LLMAlloc and LLMFree) to allocate memory. It grabs
* the function pointers by calling FPlatformMemory::GetLLMAllocFunctions. If these functions
* are not implemented GetLLMAllocFunctions should return false and LLM will be disabled.
*/

#if ENABLE_LOW_LEVEL_MEM_TRACKER

void* LLMAlloc(size_t Size)
{
	int AlignedSize = Align(Size, PS4_TRUEPAGESIZE);

	off_t DirectMem = 0;
	int ret = sceKernelAllocateDirectMemory(0, SCE_KERNEL_MAIN_DMEM_SIZE, AlignedSize, PS4_TRUEPAGESIZE, SCE_KERNEL_WB_ONION, &DirectMem);
	check(ret == SCE_OK);

	void* Addr = NULL;
	ret = sceKernelMapDirectMemory(&Addr, AlignedSize, SCE_KERNEL_PROT_CPU_RW, 0, DirectMem, PS4_TRUEPAGESIZE);
	check(ret == SCE_OK);

	LLMMallocTotal += AlignedSize;

	return Addr;
}

void LLMFree(void* Addr, size_t Size)
{
	SceKernelVirtualQueryInfo Info;
	sceKernelVirtualQuery(Addr, 0, &Info, sizeof(Info));
	int64 virtual_offset = (uint64)Addr - (uint64)Info.start;

	int AlignedSize = Align(Size, PS4_TRUEPAGESIZE);
	LLMMallocTotal -= AlignedSize;

	sceKernelReleaseDirectMemory(Info.offset + virtual_offset, AlignedSize);
}

bool FPS4PlatformMemory::GetLLMAllocFunctions(void*(*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment)
{
	OutAllocFunction = LLMAlloc;
	OutFreeFunction = LLMFree;
	OutAlignment = PS4_TRUEPAGESIZE;

	return true;
}

#endif // ENABLE_LOW_LEVEL_MEM_TRACKER

#endif // USE_NEW_PS4_MEMORY_SYSTEM == 0
