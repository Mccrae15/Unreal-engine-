// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#if USE_NEW_PS4_MEMORY_SYSTEM

#if USE_DEFRAG_ALLOCATOR
	#error New PS4 Memory system is incompatible with the GPU defrag allocator.
#endif

#include "PS4Memory2.h"
#include "HAL/MallocBinned2.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/ThreadSafeCounter64.h"

#include "Stats/Stats.h"

#include "ScopeLock.h"
#include "List.h"

// When enabled, allows WB_ONION memory allocations to fallback
// to using flexible memory, when all direct memory is exhausted.
#define PS4_MEM_ENABLE_FLEXIBLE_FALLBACK 1

// When enabled, allows GPUGarlic heap memory to fallback to the
// framebuffer heap, when all direct memory is exhausted.
#define PS4_MEM_ENABLE_FRAMEBUFFER_FALLBACK 1

// When enabled, tracks sizes of memory allocations for
// the stats system and FPlatformMemory::GetStats().
#define PS4_MEM_ENABLE_STATS 1

// When enabled, performs additional checking of
// the allocator tree structures. VERY SLOW!
#define PS4_MEM_DO_CHECK 0

// When enabled, GPU pages are 2MB.
// When disabled, GPU pages are the same size as CPU pages (64KB).
#define PS4_MEM_ENABLE_GPU_2MB_PAGES 0

// When enabled, sets the GPO LEDs on the kit to an indication of total memory used.
// Valid options for PS4_MEM_GPO_TYPE are:
//    1. Binary Value from 0 (no memory allocated) - 255 (all memory allocated).
//    2. Progress Bar from right to left.
#define PS4_MEM_ENABLE_GPO (!UE_BUILD_SHIPPING && PS4_MEM_ENABLE_STATS)
#define PS4_MEM_GPO_TYPE 1

// The total amount of flexible memory the system allocates for this process. 448 MB is
// the default and maximum allowed. We use this define to know the actual size of flexible
// memory during early boot.
// There is no need to reduce flexible memory size, as WB_ONION allocations fall back to
// using flexible memory when direct memory is exhausted.
#define PS4_FLEXIBLE_MEMORY_SIZE (448ull * 1024ull * 1024ull) // 448MB
SCE_KERNEL_FLEXIBLE_MEMORY_SIZE(PS4_FLEXIBLE_MEMORY_SIZE);

// Amount of direct memory to leave unallocated, and not tracked by the pool APIs.
// It seems Gnm does some amount of internal direct memory allocation and fails to
// submit command buffers if we claim the whole lot.
#define NON_POOLED_DIRECT_MEMORY_SIZE (64ull * 1024ull) // 64KB

// Virtual address space sizes for each of the four memory pools.
// Must be a power of two. Increasing these sizes allows for greater VM space fragmentation,
// but will add an additional level to the binary tree structures with each power of two,
// making the structures slower.
#define VIRTUAL_ADDRESS_SPACE_SIZE_CPU               (8ull * 1024ull * 1024ull * 1024ull) //   8 GB
#define VIRTUAL_ADDRESS_SPACE_SIZE_GPU_GARLIC        (8ull * 1024ull * 1024ull * 1024ull) //   8 GB
#define VIRTUAL_ADDRESS_SPACE_SIZE_GPU_ONION         (1ull * 1024ull * 1024ull * 1024ull) //   1 GB
#define VIRTUAL_ADDRESS_SPACE_SIZE_GPU_FRAMEBUFFER   (        128ull * 1024ull * 1024ull) // 128 MB
#define VIRTUAL_ADDRESS_SPACE_SIZE_FLEXIBLE_FALLBACK (        512ull * 1024ull * 1024ull) // 512 MB

// Actual small page size on x86 is 4KB, but Sony's minimum allocation granularity is 4*4KB pages == 16KB.
// Sony's new "pooled memory" APIs have a minimum allocation granularity of 64KB.
static const uint64 CPUPageSize = 64 * 1024; // 64 KB
static const uint64 GPUPageSize = (PS4_MEM_ENABLE_GPU_2MB_PAGES)
	? (2 * 1024 * 1024)  //  2 MB
	: (      64 * 1024); // 64 KB

// Amount of direct memory we expect to have on base and neo PS4.
// Used to detect when the console is in LARGE memory mode.
static const uint64 GStandardMemorySize_ForDebugMemoryDetection_Base = 4736ull * 1024 * 1024;
static const uint64 GStandardMemorySize_ForDebugMemoryDetection_Neo  = 5376ull * 1024 * 1024;

#if PS4_MEM_ENABLE_STATS

	#include "GenericPlatformMemoryPoolStats.h"

	DECLARE_STATS_GROUP(TEXT("PS4 Memory Pools"), STATGROUP_MemoryPS4Pools, STATCAT_Hidden);
	DECLARE_STATS_GROUP(TEXT("PS4 Memory Details"), STATGROUP_MemoryPS4Details, STATCAT_Advanced);

	DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Flexible Memory Pool [Flexible]"), MCR_Flexible, STATGROUP_MemoryPS4Pools, FPS4PlatformMemory::MCR_Flexible, CORE_API);
	DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("FrameBuffer Memory Pool [FrameBuffer]"), MCR_FrameBuffer, STATGROUP_MemoryPS4Pools, FPS4PlatformMemory::MCR_FrameBuffer, CORE_API);
	DEFINE_STAT(MCR_Flexible);
	DEFINE_STAT(MCR_FrameBuffer);

	DECLARE_MEMORY_STAT_POOL(TEXT("Total CPU"),            STAT_PS4Memory_AllocatedCPU,         STATGROUP_MemoryPS4Details, FPS4PlatformMemory::MCR_Physical);
	DECLARE_MEMORY_STAT_POOL(TEXT("Total Garlic"),         STAT_PS4Memory_AllocatedGarlic,      STATGROUP_MemoryPS4Details, FPS4PlatformMemory::MCR_Physical);
	DECLARE_MEMORY_STAT_POOL(TEXT("Total Onion"),          STAT_PS4Memory_AllocatedOnion,       STATGROUP_MemoryPS4Details, FPS4PlatformMemory::MCR_Physical);
	DECLARE_MEMORY_STAT_POOL(TEXT("Total FrameBuffer"),    STAT_PS4Memory_AllocatedFrameBuffer, STATGROUP_MemoryPS4Details, FPS4PlatformMemory::MCR_FrameBuffer);
	DECLARE_MEMORY_STAT_POOL(TEXT("Total Flexible"),       STAT_PS4Memory_AllocatedFlexible,    STATGROUP_MemoryPS4Details, FPS4PlatformMemory::MCR_Flexible);
	DECLARE_MEMORY_STAT_POOL(TEXT("Available Flexible"),   STAT_PS4Memory_AvailableFlexible,    STATGROUP_MemoryPS4Details, FPS4PlatformMemory::MCR_Flexible);
	DECLARE_MEMORY_STAT_POOL(TEXT("System Used Flexible"), STAT_PS4Memory_SystemUsedFlexible,   STATGROUP_MemoryPS4Details, FPS4PlatformMemory::MCR_Flexible);
	DECLARE_MEMORY_STAT_POOL(TEXT("Total GPU MemBlock"),   STAT_PS4Memory_MemBlockTotal,        STATGROUP_MemoryPS4Details, FPS4PlatformMemory::MCR_Physical);
	DECLARE_MEMORY_STAT_POOL(TEXT("Used GPU MemBlock"),    STAT_PS4Memory_MemBlockUsed,         STATGROUP_MemoryPS4Details, FPS4PlatformMemory::MCR_Physical);
	DECLARE_MEMORY_STAT_POOL(TEXT("Wasted GPU MemBlock"),  STAT_PS4Memory_MemBlockWasted,       STATGROUP_MemoryPS4Details, FPS4PlatformMemory::MCR_Physical);

#endif // PS4_MEM_ENABLE_STATS

#if PS4_MEM_ENABLE_GPO
	#include <pthread.h>
#endif

#if PS4_MEM_DO_CHECK
	#include <iostream>
	#define PS4MemCheck(x) do { if ((x) == false) { printf("PS4 Mem Check Failed: %s\r\n", #x); __debugbreak(); } } while (false)
#else
	#define PS4MemCheck(...)
#endif

enum class EHeapType { CPU, GPUGarlic, GPUOnion, FrameBuffer, Flexible, Num };

constexpr static inline EHeapType GnmMemTypeToHeapType(EGnmMemType Type)
{
	switch (Type)
	{
	case EGnmMemType::GnmMem_GPU: return EHeapType::GPUGarlic;
	case EGnmMemType::GnmMem_CPU: return EHeapType::GPUOnion;
	case EGnmMemType::GnmMem_FrameBuffer: return EHeapType::FrameBuffer;
	}

	return EHeapType::Num;
}

constexpr static inline uintptr_t GetVirtualAddrStart(EHeapType Type)
{
	switch (Type)
	{
	case EHeapType::CPU:         return uintptr_t(SCE_KERNEL_APP_MAP_AREA_START_ADDR * 1ull);
	case EHeapType::GPUGarlic:   return uintptr_t(SCE_KERNEL_APP_MAP_AREA_START_ADDR * 2ull);
	case EHeapType::GPUOnion:    return uintptr_t(SCE_KERNEL_APP_MAP_AREA_START_ADDR * 3ull);
	case EHeapType::FrameBuffer: return uintptr_t(SCE_KERNEL_APP_MAP_AREA_START_ADDR * 4ull);
	case EHeapType::Flexible:    return uintptr_t(SCE_KERNEL_APP_MAP_AREA_START_ADDR * 5ull);
	}

	return 0;
}

constexpr static inline uint64 GetVirtualAddrLength(EHeapType Type)
{
	switch (Type)
	{
	case EHeapType::CPU:         return VIRTUAL_ADDRESS_SPACE_SIZE_CPU;
	case EHeapType::GPUGarlic:   return VIRTUAL_ADDRESS_SPACE_SIZE_GPU_GARLIC;
	case EHeapType::GPUOnion:    return VIRTUAL_ADDRESS_SPACE_SIZE_GPU_ONION;
	case EHeapType::FrameBuffer: return VIRTUAL_ADDRESS_SPACE_SIZE_GPU_FRAMEBUFFER;
	case EHeapType::Flexible:    return VIRTUAL_ADDRESS_SPACE_SIZE_FLEXIBLE_FALLBACK;
	}

	return 0;
};

constexpr static uint64 GTotalVirtualMemory =
	GetVirtualAddrLength(EHeapType::CPU) +
	GetVirtualAddrLength(EHeapType::GPUGarlic) +
	GetVirtualAddrLength(EHeapType::GPUOnion) +
	GetVirtualAddrLength(EHeapType::FrameBuffer) +
	GetVirtualAddrLength(EHeapType::Flexible);

constexpr static inline uintptr_t GetVirtualAddrEnd(EHeapType Type)
{
	return GetVirtualAddrStart(Type) + GetVirtualAddrLength(Type);
};

constexpr static inline bool IsVirtualAddrType(EHeapType Type, uintptr_t Addr)
{
	return Addr >= GetVirtualAddrStart(Type) && Addr < GetVirtualAddrEnd(Type);
}

constexpr static inline EHeapType GetVirtualAddressType(uintptr_t Addr)
{
	return
		 IsVirtualAddrType(EHeapType::CPU,         Addr) ? EHeapType::CPU :
		(IsVirtualAddrType(EHeapType::GPUGarlic,   Addr) ? EHeapType::GPUGarlic :
		(IsVirtualAddrType(EHeapType::GPUOnion,    Addr) ? EHeapType::GPUOnion : 
		(IsVirtualAddrType(EHeapType::FrameBuffer, Addr) ? EHeapType::FrameBuffer :
		EHeapType::Flexible)));
}

constexpr static inline SceKernelMemoryType GetSceKernelMemoryType(EHeapType Type)
{
	switch (Type)
	{
	case EHeapType::CPU:		 return SCE_KERNEL_WB_ONION;
	case EHeapType::GPUGarlic:	 return SCE_KERNEL_WC_GARLIC;
	case EHeapType::GPUOnion:	 return SCE_KERNEL_WB_ONION;
	case EHeapType::FrameBuffer: return SCE_KERNEL_WC_GARLIC;
	case EHeapType::Flexible:    return SCE_KERNEL_WB_ONION;
	}

	return SCE_KERNEL_MEMORY_TYPE_END;
}

constexpr static inline int32 GetSceKernelMemoryProtection(EHeapType Type)
{
	// Note: FrameBuffer memory must have the same permissions as GPUGarlic, as Garlic
	// allocations can fallback to framebuffer memory when direct memory is exhausted.
	switch (Type)
	{
	case EHeapType::CPU:		 return SCE_KERNEL_PROT_CPU_RW;
	case EHeapType::GPUGarlic:	 return SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_RW;
	case EHeapType::GPUOnion:	 return SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_RW;
	case EHeapType::FrameBuffer: return SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_RW;
	case EHeapType::Flexible:    return SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_RW;
	}

	return 0;
}

constexpr static inline const TCHAR* GetHeapTypeName(EHeapType Type)
{
	switch (Type)
	{
	case EHeapType::CPU:		 return TEXT("CPU");
	case EHeapType::GPUGarlic:	 return TEXT("GPU Garlic");
	case EHeapType::GPUOnion:	 return TEXT("GPU Onion");
	case EHeapType::FrameBuffer: return TEXT("Framebuffer");
	case EHeapType::Flexible:    return TEXT("Flexible");
	}

	return TEXT("Unknown");
}


//
// TMipField is a binary tree bit field container, used by the page allocators.
//
// Individual bits in the tree are addressable via a (mip, slot, bit) scheme.
// Each level of the tree (aka "mip") contains multiple "slots". Each "slot" contains 'n' bits.
//
// The following diagram shows a TMipField which represents 32 pages, with 1 bit per slot.
//
//									                              Mip Level
//                                1					                  0
//                1-------------------------------1			          1
//        1---------------1               1---------------1		      2
//    1-------1       1-------1       1-------1       *-------1		  3
//  1---1   1---1   1---1   1---1   1---1   1---1   1---1   1---1	  4
// 1-1 1-1 1-1 1-1 1-1 1-1 1-1 1-1 1-1 1-1 1-1 1-1 1-1 1-1 1-1 1-1    5
//
//                     1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2 3 3
// 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1  Page Index
//
// For example, the highlighted bit (*) in the above tree has a (mip, slot, bit) address of (3, 6, 0).
// That particular bit represents a 4 page region, starting at the 24th page (i.e. pages 24, 25, 26, and 27).
//
// The TMipFieldAllocator class uses a TMipField to store 2 bits per page: "ANY" and "ALL".
// When these bits are set to '1', they indicate that "any" or "all" child slots have the same bit set.
// e.g. If the highlighted slot's "ALL" bit was '1', this indicates the entire 4-page region (24 to 27) is allocated.
//      Likewise, if the "ANY" bit is set, this indicates at least 1 page in this region is allocated.
//
// The storage for the binary tree structure is provided by the TAllocator template argument. This allows for TMipField
// instances to use pre-allocated space at compile time before the general purpose allocators are initialized.
//

template <typename TAllocator>
struct TMipField
{
	// Computes the total number of elements across all mips in the mipfield.
	inline uint64 GetTotalNumElements(uint64 NumSlotsThisMip)
	{
		uint64 ParentMipNumElements = (NumSlotsThisMip > 1) ? GetTotalNumElements(NumSlotsThisMip / 2) : 0; // Recurse up to next mip (half size, hence divide by 2)
		uint64 ThisMipNumElements = (NumSlotsThisMip + (SlotsPerElement - 1)) / SlotsPerElement; // Round up

		return ThisMipNumElements + ParentMipNumElements;
	}

	// Underlying storage type.
	typedef uint8_t FMipFieldElement;

	// Total number of slots in the largest mip.
	uint64 const NumSlots;

	// total number of bits to allocate for each slot.
	uint64 const BitsPerSlot;

	// Number of bits in a single element
	uint64 const BitsPerElement;

	// The number of unique slots that can be packed into a single element.
	uint64 const SlotsPerElement;

	// The total number of levels in the mip chain.
	uint64 const NumLevels;

	// The total number of elements, which provide the storage for the entire mip chain.
	uint64 const NumElements;

	FMipFieldElement* Elements;
	uint64* MipLevelOffsets;

	inline void AddressSingleMipBit(uint64 Mip, uint64 Slot, uint64 Bit, uint64& OutElementIndex, uint64& OutBitIndex) const
	{
		PS4MemCheck(Mip < NumLevels);

		uint64 SlotsThisMip = uint64(1) << Mip;
		PS4MemCheck(Slot < SlotsThisMip);

		OutElementIndex = MipLevelOffsets[Mip] + (Slot / SlotsPerElement);
		OutBitIndex = ((Slot % SlotsPerElement) * BitsPerSlot) + Bit;
	}

	inline bool GetMipBit(uint64 Mip, uint64 Slot, uint64 Bit) const
	{
		uint64 ElementIndex, BitIndex;
		AddressSingleMipBit(Mip, Slot, Bit, ElementIndex, BitIndex);

		return (Elements[ElementIndex] & (uint64(1) << BitIndex)) != 0;
	}

	// Also returns the old value of the bit
	inline bool SetMipBit(uint64 Mip, uint64 Slot, uint64 Bit, bool Value)
	{
		uint64 ElementIndex, BitIndex;
		AddressSingleMipBit(Mip, Slot, Bit, ElementIndex, BitIndex);

		FMipFieldElement OldElementValue = Elements[ElementIndex];
		uint64 Mask = uint64(1) << BitIndex;
		if (Value)
		{
			Elements[ElementIndex] = OldElementValue | Mask;
		}
		else
		{
			Elements[ElementIndex] &= (~Mask);
		}

		return !!(OldElementValue & Mask);
	}

	TMipField(uint64 InNumSlots, uint64 InBitsPerSlot)
		: NumSlots(InNumSlots)
		, BitsPerSlot(InBitsPerSlot)
		, BitsPerElement(sizeof(FMipFieldElement) * 8)
		, SlotsPerElement(BitsPerElement / InBitsPerSlot)
		, NumLevels(FMath::FloorLogTwo64(NumSlots) + 1)
		, NumElements(GetTotalNumElements(NumSlots))
		, Elements(nullptr)
		, MipLevelOffsets(nullptr)
	{
		checkf(FMath::IsPowerOfTwo(NumSlots), TEXT("Number of slots in a mipfield must be a power of two."));
		checkf(FMath::IsPowerOfTwo(InBitsPerSlot), TEXT("Bits per slot must be a power of two."));
		checkf((sizeof(FMipFieldElement) * 8) >= InBitsPerSlot, TEXT("Mipfield element must be greater than or equal to the number of bits per slot."));

		Elements = reinterpret_cast<FMipFieldElement*>(TAllocator::Allocate(NumElements * sizeof(FMipFieldElement)));
		MipLevelOffsets = reinterpret_cast<uint64*>(TAllocator::Allocate(NumLevels * sizeof(uint64)));

		FMemory::Memzero(Elements, NumElements * sizeof(FMipFieldElement));

		MipLevelOffsets[0] = 0;
		for (uint64 Index = 0; Index < NumLevels - 1; ++Index)
		{
			MipLevelOffsets[Index + 1] = GetTotalNumElements(uint64(1) << Index);
		}
	}

	~TMipField()
	{
		TAllocator::Free(Elements);
		TAllocator::Free(MipLevelOffsets);
	}
};


template <typename TAllocator>
struct TMipFieldPageAllocator
{
	uint64 const TotalNumPages;

	TMipFieldPageAllocator(uint64 InTotalNumPages)
		: TotalNumPages(InTotalNumPages)
		, MipField(TotalNumPages, 2)
	{}

public:
	enum
	{
		ANY = 0,
		ALL = 1
	};

	// 2 bits per page, "any" and "all"
	TMipField<TAllocator> MipField;

	// Returns the absolute page index of the first page covered by the specified slot in the mip.
	inline uint64 MipSlot_To_PageIndex(uint64 Mip, uint64 Slot) const
	{
		return Slot << ((MipField.NumLevels - 1) - Mip);
	}

	// Finds the slot in the highest mip which covers the page region
	inline void PageIndexSize_To_MipSlot(uint64 PageIndex, uint64 Size, uint64& OutMip, uint64& OutSlot) const
	{
		OutMip = MipField.NumLevels - (FMath::FloorLogTwo64(Size) + 1);
		OutSlot = PageIndex / Size;
	}

	static inline uint64 GetSiblingSlot(uint64 Slot)
	{
		// Flip the bottom bit to find the sibling slot
		return ((~Slot) & 0x01) | (Slot & ((~uint64(0)) << 1));
	}

	// Returns the number of pages covered by a single slot in the given mip.
	inline uint64 GetPagesInSingleMipSlot(uint64 Mip) const
	{
		return TotalNumPages >> Mip;
	}

#if PS4_MEM_DO_CHECK
	void CheckTreeRecursive(uint64 Mip, uint64 LeftSlot, bool ParentAny, bool ParentAll) const
	{
		bool LeftAny = MipField.GetMipBit(Mip, LeftSlot, ANY);
		bool RightAny = MipField.GetMipBit(Mip, LeftSlot + 1, ANY);

		bool LeftAll = MipField.GetMipBit(Mip, LeftSlot, ALL);
		bool RightAll = MipField.GetMipBit(Mip, LeftSlot + 1, ALL);

		if (LeftAny || RightAny) { PS4MemCheck(ParentAny); }
		if (LeftAll && RightAll) { PS4MemCheck(ParentAll); }

		if (LeftAll) { PS4MemCheck(LeftAny); }
		if (RightAll) { PS4MemCheck(RightAny); }

		if (Mip < MipField.NumLevels - 1)
		{
			CheckTreeRecursive(Mip + 1, LeftSlot << 1, LeftAny, LeftAll);
			CheckTreeRecursive(Mip + 1, (LeftSlot + 1) << 1, RightAny, RightAll);
		}
	}

	void CheckTree() const
	{
		// Root node is special case
		bool RootAny = MipField.GetMipBit(0, 0, ANY);
		bool RootAll = MipField.GetMipBit(0, 0, ALL);
		if (RootAll) { PS4MemCheck(RootAny); }

		CheckTreeRecursive(1, 0, RootAny, RootAll);
	}

	void CheckPages(uint64 PageIndex, uint64 NumPages, bool IsAllocated) const
	{
		// Verify region is already set/free
		for (uint64 Index = 0; Index < NumPages; ++Index)
		{
			bool AnyBit = MipField.GetMipBit(MipField.NumLevels - 1, Index + PageIndex, ANY);
			bool AllBit = MipField.GetMipBit(MipField.NumLevels - 1, Index + PageIndex, ALL);

			PS4MemCheck(AnyBit == AllBit);
			PS4MemCheck(AnyBit == IsAllocated);
		}
	}
#endif

	inline void AssignSubtree(uint64 MipLevel, uint64 Slot, bool Value)
	{
#if PS4_MEM_DO_CHECK
		// Check this bit *is not* already set to [Value]
		//PS4MemCheck(MipField.GetMipBit(MipLevel, Slot, ANY) != Value);
		if (MipField.GetMipBit(MipLevel, Slot, ANY) == Value)
		{
			uint64 ElementIndex, BitIndex, BaseElement, BaseBit;
			uint64 PageIndex = MipSlot_To_PageIndex(MipLevel, Slot);
			MipField.AddressSingleMipBit(MipLevel, Slot, ANY, ElementIndex, BitIndex);
			MipField.AddressSingleMipBit(MipField.NumLevels - 1, PageIndex, ANY, BaseElement, BaseBit);
			printf("AssignSubtree FAILURE: PageIndex %lu\r\n"
				   "                       Mip %lu, Slot %lu\r\n"
				   "                       Element %lu, Bit %lu\r\n"
				   "                  Base Element %lu, Bit %lu\r\n", 
				PageIndex, MipLevel, Slot, ElementIndex, BitIndex, BaseElement, BaseBit);
			__debugbreak();
		}
		
		//PS4MemCheck(MipField.GetMipBit(MipLevel, Slot, ALL) != Value);
		if (MipField.GetMipBit(MipLevel, Slot, ALL) == Value)
		{
			uint64 ElementIndex, BitIndex, BaseElement, BaseBit;
			uint64 PageIndex = MipSlot_To_PageIndex(MipLevel, Slot);
			MipField.AddressSingleMipBit(MipLevel, Slot, ALL, ElementIndex, BitIndex);
			MipField.AddressSingleMipBit(MipField.NumLevels - 1, PageIndex, ALL, BaseElement, BaseBit);
			printf("AssignSubtree FAILURE: PageIndex %lu\r\n"
				"                       Mip %lu, Slot %lu\r\n"
				"                       Element %lu, Bit %lu\r\n"
				"                  Base Element %lu, Bit %lu\r\n",
				PageIndex, MipLevel, Slot, ElementIndex, BitIndex, BaseElement, BaseBit);
			__debugbreak();
		}
#endif

		if (MipLevel < MipField.NumLevels - 1)
		{
			// Recurse through tree to set bits
			AssignSubtree(MipLevel + 1, Slot << 1, Value);
			AssignSubtree(MipLevel + 1, (Slot << 1) + 1, Value);
		}

		MipField.SetMipBit(MipLevel, Slot, ANY, Value);
		MipField.SetMipBit(MipLevel, Slot, ALL, Value);
	}

	inline void FixupParents(uint64 StartMip, uint64 StartSlot)
	{
		uint64 CurrentMip = StartMip;
		uint64 CurrentSlot = StartSlot;

		// TODO: Early out of this loop when we hit a part of
		// the tree which is already at the expected values.
		while (CurrentMip >= 1)
		{
			uint64 ParentMip = CurrentMip - 1;
			uint64 ParentSlot = CurrentSlot >> 1;
			uint64 SiblingSlot = GetSiblingSlot(CurrentSlot);

			bool SiblingAny = MipField.GetMipBit(CurrentMip, SiblingSlot, ANY);
			bool SiblingAll = MipField.GetMipBit(CurrentMip, SiblingSlot, ALL);

			bool ThisAny = MipField.GetMipBit(CurrentMip, CurrentSlot, ANY);
			bool ThisAll = MipField.GetMipBit(CurrentMip, CurrentSlot, ALL);

			bool NewParentAny = ThisAny || SiblingAny;
			bool NewParentAll = ThisAll && SiblingAll;

			MipField.SetMipBit(ParentMip, ParentSlot, ANY, NewParentAny); // Update parent ANY bit.
			MipField.SetMipBit(ParentMip, ParentSlot, ALL, NewParentAll); // Update parent ALL bit.

			CurrentMip = ParentMip;
			CurrentSlot = ParentSlot;
		}
	}

	bool FindFreeRegionRecursive_PowerOfTwo(uint64 NumPages, uint64 MipLevel, uint64 LeftSlot, uint64& OutMipLevel, uint64& OutSlot) const
	{
		uint64 PagesInSlot = TotalNumPages >> MipLevel;
		uint64 RightSlot = LeftSlot + 1;

		if (PagesInSlot > NumPages)
		{
			// We've not reached the bottom of the tree yet.
			// Try searching left, only if ALL bit is not set for that subtree
			if (!MipField.GetMipBit(MipLevel, LeftSlot, ALL) && FindFreeRegionRecursive_PowerOfTwo(NumPages, MipLevel + 1, LeftSlot << 1, OutMipLevel, OutSlot))
			{
				return true;
			}
			// If that failed, try the right subtree.
			else if (!MipField.GetMipBit(MipLevel, RightSlot, ALL) && FindFreeRegionRecursive_PowerOfTwo(NumPages, MipLevel + 1, RightSlot << 1, OutMipLevel, OutSlot))
			{
				return true;
			}
			else
			{
				// No space in either child
				return false;
			}
		}
		else
		{
			if (!MipField.GetMipBit(MipLevel, LeftSlot, ANY))
			{
				PS4MemCheck(!MipField.GetMipBit(MipLevel, LeftSlot, ALL));

				// Found empty region in entire left tree
				OutMipLevel = MipLevel;
				OutSlot = LeftSlot;
				return true;
			}
			else if (!MipField.GetMipBit(MipLevel, RightSlot, ANY))
			{
				PS4MemCheck(!MipField.GetMipBit(MipLevel, RightSlot, ALL));

				// Found empty region in entire right tree
				OutMipLevel = MipLevel;
				OutSlot = RightSlot;
				return true;
			}
			else
			{
				// No space in either child
				return false;
			}
		}
	}

	bool FindFreeRegion_PowerOfTwo(uint64 NumPages, uint64& OutMipLevel, uint64& OutSlot) const
	{
		PS4MemCheck(NumPages != 0 && FMath::IsPowerOfTwo(NumPages));
		PS4MemCheck(NumPages <= TotalNumPages);

		// Root node is a special case (it has no sibling)
		if (NumPages == TotalNumPages)
		{
			OutMipLevel = 0;
			OutSlot = 0;
			return MipField.GetMipBit(0, 0, ANY) == false;
		}

		return FindFreeRegionRecursive_PowerOfTwo(NumPages, 1, 0, OutMipLevel, OutSlot);
	}

	// Marks the specified region as either allocated or free. The region must not already be in the specified state.
	// NumPages must be a power of two, and PageIndex should be aligned so that it indexes the first page of a power-of-two region.
	// i.e. PageIndex is a leftmost child of a subtree.
	void SetRegion_PowerOfTwo(uint64 PageIndex, uint64 NumPages, bool IsAllocated)
	{
		PS4MemCheck(NumPages > 0);
		PS4MemCheck(NumPages <= TotalNumPages);
		PS4MemCheck((NumPages + PageIndex) <= TotalNumPages);
		PS4MemCheck(FMath::IsPowerOfTwo(NumPages));

		// Ensure the offset to the first page is aligned with the power-of-two region
		PS4MemCheck(((NumPages - 1) & PageIndex) == 0);

		uint64 Mip, Slot;
		PageIndexSize_To_MipSlot(PageIndex, NumPages, Mip, Slot);

#if PS4_MEM_DO_CHECK
		CheckTree();
#endif

		AssignSubtree(Mip, Slot, IsAllocated);
		FixupParents(Mip, Slot);

#if PS4_MEM_DO_CHECK
		CheckTree();
#endif
	}

	inline void GetRegionBounds_ExpandRegion(uint64 PageIndex, bool IsAllocated, uint64& OutLowerBound, uint64& OutUpperBound) const
	{
		const uint64 BaseMip = MipField.NumLevels - 1;
		const uint64 TestBit = IsAllocated ? ALL : ANY;

		uint64 CurrentMip = BaseMip;
		uint64 CurrentSlot = PageIndex;

		while (CurrentMip >= 1 && MipField.GetMipBit(CurrentMip, GetSiblingSlot(CurrentSlot), TestBit) == IsAllocated)
		{
			CurrentMip--;
			CurrentSlot >>= 1;
		}

		// Get the bounds of this subtree
		OutLowerBound = MipSlot_To_PageIndex(CurrentMip, CurrentSlot);
		OutUpperBound = OutLowerBound + GetPagesInSingleMipSlot(CurrentMip);
	}

public:
	// Returns true if the allocator is completely full.
	inline bool IsFull() const { return MipField.GetMipBit(0, 0, ALL) == true; }

	// Returns true if the allocator is completely empty.
	inline bool IsEmpty() const { return MipField.GetMipBit(0, 0, ANY) == false; }

	// Returns true if the given arbitrary region is fully allocated.
	bool IsRegionFull(uint64 PageIndex, uint64 NumPages) const
	{
		while (NumPages > 0)
		{
			uint64 MaxPagesThisIteration = uint64(1) << FMath::Min(FMath::CountTrailingZeros64(PageIndex), MipField.NumLevels - 1);

			// Get the next power-of-two sized region to assign
			uint64 PagesToCheck = FMath::RoundDownToPowerOfTwo64(NumPages);
			PagesToCheck = FMath::Min(PagesToCheck, MaxPagesThisIteration);

			uint64 Mip, Slot;
			PageIndexSize_To_MipSlot(PageIndex, PagesToCheck, Mip, Slot);

			if (MipField.GetMipBit(Mip, Slot, ALL) == false)
			{
				return false;
			}

			PageIndex += PagesToCheck;
			NumPages -= PagesToCheck;
		}

		return true;
	}

	// For the given page index, computes the upper and lower bounds of the region
	// the specified page exists in, and returns if this region is free or allocated.
	bool GetRegionBounds(uint64 PageIndex, uint64& OutFirstPageIndex, uint64& OutNumPages) const
	{
		// Read the allocation bit for the current page
		const uint64 BaseMip = MipField.NumLevels - 1;
		const bool IsAllocated = MipField.GetMipBit(BaseMip, PageIndex, ANY);

		// Expand this region to the initial upper/lower bounds.
		uint64 LowerBound, UpperBound;
		GetRegionBounds_ExpandRegion(PageIndex, IsAllocated, LowerBound, UpperBound);

		// Whilst there is more room in the tree to the left, and the adjacent local region also has the same allocation type.
		while (LowerBound > 0 && MipField.GetMipBit(BaseMip, LowerBound - 1, ANY) == IsAllocated)
		{
			uint64 LocalLowerBound, LocalUpperBound;
			GetRegionBounds_ExpandRegion(LowerBound - 1, IsAllocated, LocalLowerBound, LocalUpperBound);

			PS4MemCheck(LocalUpperBound == LowerBound);
			PS4MemCheck(LocalLowerBound <= LowerBound);
			LowerBound = LocalLowerBound;
		}

		// Whilst there is more room in the tree to the right, and the adjacent local region also has the same allocation type.
		while (UpperBound < TotalNumPages && MipField.GetMipBit(BaseMip, UpperBound, ANY) == IsAllocated)
		{
			uint64 LocalLowerBound, LocalUpperBound;
			GetRegionBounds_ExpandRegion(UpperBound, IsAllocated, LocalLowerBound, LocalUpperBound);

			PS4MemCheck(LocalLowerBound == UpperBound);
			PS4MemCheck(UpperBound <= LocalUpperBound);
			UpperBound = LocalUpperBound;
		}

		OutFirstPageIndex = LowerBound;
		OutNumPages = UpperBound - LowerBound;

#if PS4_MEM_DO_CHECK
		// Verify result...
		if (LowerBound > 0)
		{
			PS4MemCheck(MipField.GetMipBit(BaseMip, LowerBound - 1, ANY) != IsAllocated);
		}
		if (UpperBound < TotalNumPages)
		{
			PS4MemCheck(MipField.GetMipBit(BaseMip, UpperBound, ANY) != IsAllocated);
		}
		for (uint64 Index = LowerBound; Index < UpperBound; ++Index)
		{
			PS4MemCheck(MipField.GetMipBit(BaseMip, Index, ANY) == IsAllocated);
		}

		CheckPages(OutFirstPageIndex, OutNumPages, IsAllocated);
#endif

		return IsAllocated;
	}

	// Marks the specified arbitrary region as either allocated or free. The region must not already be in the specified state.
	void SetRegion(uint64 PageIndex, uint64 NumPages, bool IsAllocated)
	{
#if PS4_MEM_DO_CHECK
		CheckPages(PageIndex, NumPages, !IsAllocated);
#endif

		while (NumPages > 0)
		{
			uint64 MaxPagesThisIteration = uint64(1) << FMath::Min(FMath::CountTrailingZeros64(PageIndex), MipField.NumLevels - 1);

			// Get the next power-of-two sized region to assign
			uint64 PagesToAssign = FMath::RoundDownToPowerOfTwo64(NumPages);
			PagesToAssign = FMath::Min(PagesToAssign, MaxPagesThisIteration);

			// Modify the tree
			SetRegion_PowerOfTwo(PageIndex, PagesToAssign, IsAllocated);

			PageIndex += PagesToAssign;
			NumPages -= PagesToAssign;
		}

#if PS4_MEM_DO_CHECK
		CheckPages(PageIndex, NumPages, IsAllocated);
#endif
	}

	// Finds and allocates a region large enough to hold [NumPages], returning the page index in the "out" argument and true on success, otherwise false.
	bool AllocatePages(uint64 NumPages, uint64 AlignmentInPages, uint64& OutPageIndex)
	{
		// The buddy allocator can only allocate space between power-of-two boundaries.
		// Find the smallest power-of-two sized region large enough to hold correctly aligned NumPages.
		AlignmentInPages = FMath::Max(NumPages, AlignmentInPages);
		uint64 RoundedNumPages = FMath::RoundUpToPowerOfTwo64(AlignmentInPages);

		uint64 RegionStartMip, RegionStartSlot;
		if (FindFreeRegion_PowerOfTwo(RoundedNumPages, RegionStartMip, RegionStartSlot))
		{
			OutPageIndex = MipSlot_To_PageIndex(RegionStartMip, RegionStartSlot);
			SetRegion(OutPageIndex, NumPages, true);
			return true;
		}
		else
		{
			// No power-of-two region exists. Failed to allocate.
			OutPageIndex = 0;
			return false;
		}
	}

	// Frees the region covered by [PageIndex, NumPages]. The region must already have been allocated.
	void FreePages(uint64 PageIndex, uint64 NumPages)
	{
		SetRegion(PageIndex, NumPages, false);
	}
};

// Allocator type for TMipFields which uses general purpose malloc.
// Used for runtime instances of TMipField, in the GPU small block allocator.
struct FMipFieldRunTimeAllocator
{
	static inline void* Allocate(uint64 Size) { return FPS4PlatformMemory::BaseAllocator()->Malloc(Size); }
	static inline void Free(void* Ptr) { FPS4PlatformMemory::BaseAllocator()->Free(Ptr); }
};

// Allocator type for TMipFields using a compile time defined memory pool.
// Used for the main virtual address allocators in FMemoryHeapManager.
struct FMipFieldCompileTimeAllocator
{
	static inline void* Allocate(uint64 Size)
	{
		// Linear allocate from the pool
		Size = Align(Size, 8);
		if ((Offset + Size) > MaxPoolSize)
			abort();

		void* Original = Pool + Offset;
		Offset += Size;
		return Original;
	}

	static inline void Free(void*)
	{
		// No-op
	}

private:
	static constexpr uint64 MaxPoolSize = 256 * 1024; // 256 KB
	static uint8 Pool[MaxPoolSize];
	static uint64 Offset;
};

uint8 FMipFieldCompileTimeAllocator::Pool[MaxPoolSize] = {};
uint64 FMipFieldCompileTimeAllocator::Offset = 0;

#if PS4_MEM_ENABLE_STATS
	volatile int64 MemBlockStat_Used = 0;
	volatile int64 MemBlockStat_Wasted = 0;
#endif // PS4_MEM_ENABLE_STATS

// Singleton memory heap manager for all page allocations on PS4.
// Tracks virtual memory allocations using TMipFieldPageAllocator instances, and uses
// Sony's pool memory kernel APIs to map physical memory to virtual address regions.
class FMemoryHeapManager
{
	static FMemoryHeapManager* Singleton;
	
public:
	size_t const TotalDirectMemorySize;   // Total size of direct memory on boot.
	size_t const TotalPhysicalMemorySize; // Total size of direct and flexible memory on boot.
private:
	size_t       TotalPooledMemorySize;   // Total size of direct memory that is handed to the sceKernelMemoryPool* APIs
#if ENABLE_LOW_LEVEL_MEM_TRACKER
	size_t		 ProgramSize;			  // Calculated from the total available flexible memory on boot.
#endif

#if PS4_MEM_ENABLE_STATS
	volatile int64 TotalAllocatedDirectBytes;
	volatile int64 TotalAllocatedVirtualBytes;

	inline void TrackDirectAllocation (int64 Size) { FPlatformAtomics::InterlockedAdd(&TotalAllocatedDirectBytes,  Size); }
	inline void TrackVirtualAllocation(int64 Size) { FPlatformAtomics::InterlockedAdd(&TotalAllocatedVirtualBytes, Size); }
#endif // PS4_MEM_ENABLE_STATS

	FPlatformMemoryConstants Constants;

	class FVirtualAddressAllocator 
	{
		TMipFieldPageAllocator<FMipFieldCompileTimeAllocator> Allocator;

		uint32     const PageSize;
		uintptr_t  const VirtStartAddr;
		uint64     const VirtLength;
		EHeapType  const HeapType;
#if PS4_MEM_ENABLE_STATS
		uint64           AllocatedBytes;
#endif // PS4_MEM_ENABLE_STATS

	public:
		FCriticalSection CS;

		FVirtualAddressAllocator(EHeapType InHeapType, uint32 InPageSize)
			: Allocator(GetVirtualAddrLength(InHeapType) / InPageSize)
			, PageSize(InPageSize)
			, VirtStartAddr(GetVirtualAddrStart(InHeapType))
			, VirtLength(GetVirtualAddrLength(InHeapType))
			, HeapType(InHeapType)
		{}

		bool Allocate(uint64 Size, uint32 Alignment, uintptr_t& OutAddress)
		{
			PS4MemCheck((Size % PageSize) == 0);
			PS4MemCheck((Alignment % PageSize) == 0);

			if (Size > VirtLength)
			{
				// Attempt to allocate more memory than the virtual address pool has in total.
				return false;
			}

			uint64 PageIndex;
			if (Allocator.AllocatePages(Size / PageSize, Alignment / PageSize, PageIndex))
			{
				OutAddress = VirtStartAddr + (PageIndex * PageSize);
#if PS4_MEM_ENABLE_STATS
				AllocatedBytes += Size;
#endif // PS4_MEM_ENABLE_STATS
				return true;
			}

			// Failed to allocate virtual address space
			return false;
		}

		void Free(uintptr_t Address, uint64 Size)
		{
			PS4MemCheck((Size % PageSize) == 0);
			PS4MemCheck((Address % PageSize) == 0);

			uintptr_t PageIndex = (Address - VirtStartAddr) / PageSize;

			Allocator.FreePages(PageIndex, Size / PageSize);
#if PS4_MEM_ENABLE_STATS
			AllocatedBytes -= Size;
#endif // PS4_MEM_ENABLE_STATS
		}

#if PS4_MEM_ENABLE_STATS
		inline uint64 GetAllocatedBytes()
		{
			FScopeLock Lock(&CS);
			return AllocatedBytes;
		}
#endif // PS4_MEM_ENABLE_STATS
	};

	FVirtualAddressAllocator Allocators[(int32)EHeapType::Num];

#if PS4_MEM_ENABLE_GPO
	char GPOValue;
#endif

	inline void ReservePool(EHeapType Type)
	{
		int32 Result;

		void* const VirtStart = reinterpret_cast<void*>(GetVirtualAddrStart(Type));
		uint64 const VirtLength = GetVirtualAddrLength(Type);

		void* MapAddr = nullptr;
		if ((Result = sceKernelMemoryPoolReserve(VirtStart, VirtLength, 0, SCE_KERNEL_MAP_FIXED | SCE_KERNEL_MAP_NO_OVERWRITE, &MapAddr)) != 0)
		{
			abort();
		}

		PS4MemCheck(MapAddr == VirtStart);
	}

	inline void AllocateFrameBuffer()
	{
		int32 Result;

		void* VirtStart = reinterpret_cast<void*>(GetVirtualAddrStart(EHeapType::FrameBuffer));
		uint64 VirtLength = GetVirtualAddrLength(EHeapType::FrameBuffer);

		// Reserve some space for the frame buffer textures, since these have to be physically contiguous, and not in pooled memory.
		off_t PhysStart;
		if ((Result = sceKernelAllocateMainDirectMemory(VirtLength, GPUPageSize, GetSceKernelMemoryType(EHeapType::FrameBuffer), &PhysStart)) != 0)
			abort();

		if ((Result = sceKernelMapDirectMemory(&VirtStart, VirtLength, GetSceKernelMemoryProtection(EHeapType::FrameBuffer), SCE_KERNEL_MAP_FIXED | SCE_KERNEL_MAP_NO_OVERWRITE, PhysStart, 0)) != 0)
			abort();

#if PS4_MEM_ENABLE_STATS
		// Track this as a physical allocation
		TrackDirectAllocation(VirtLength);
#endif // PS4_MEM_ENABLE_STATS
	}

	FMemoryHeapManager()
		: TotalDirectMemorySize(sceKernelGetDirectMemorySize())
		, TotalPhysicalMemorySize(TotalDirectMemorySize + PS4_FLEXIBLE_MEMORY_SIZE)
		, TotalPooledMemorySize(0)
#if ENABLE_LOW_LEVEL_MEM_TRACKER
		, ProgramSize(0)
#endif
#if PS4_MEM_ENABLE_STATS
		, TotalAllocatedDirectBytes(0)
		, TotalAllocatedVirtualBytes(0)
#endif // PS4_MEM_ENABLE_STATS
		, Allocators
		{
			{ EHeapType::CPU,         CPUPageSize },
			{ EHeapType::GPUGarlic,   GPUPageSize },
			{ EHeapType::GPUOnion,    GPUPageSize },
			{ EHeapType::FrameBuffer, GPUPageSize },
			{ EHeapType::Flexible,    CPUPageSize },
		}
#if PS4_MEM_ENABLE_GPO
		, GPOValue(-1)
#endif
	{
		int32 Result;

#if ENABLE_LOW_LEVEL_MEM_TRACKER
		// Grab the amount of flexible memory on boot to find the program size.
		size_t AvailableFlexibleMemory;
		if (sceKernelAvailableFlexibleMemorySize(&AvailableFlexibleMemory) == SCE_OK)
		{
			ProgramSize = PS4_FLEXIBLE_MEMORY_SIZE - AvailableFlexibleMemory;
		} 
#endif

		// Allocate the frame buffer before any other memory pools.
		// We need this memory to be both virtually and physically contiguous.
		AllocateFrameBuffer();

		// Allocate all remaining physical memory to the memory pool
		off_t AvailablePhysStart = 0;
		if ((Result = sceKernelAvailableDirectMemorySize(0, TotalDirectMemorySize, 0, &AvailablePhysStart, &TotalPooledMemorySize)) != 0)
			abort();

		TotalPooledMemorySize -= NON_POOLED_DIRECT_MEMORY_SIZE;

		off_t PoolPhysOffset = 0;
		if ((Result = sceKernelMemoryPoolExpand(AvailablePhysStart, TotalDirectMemorySize, TotalPooledMemorySize, 0, &PoolPhysOffset)) != 0)
			abort();

		// Reserve virtual address regions for the 3 memory types
		ReservePool(EHeapType::CPU);
		ReservePool(EHeapType::GPUGarlic);
		ReservePool(EHeapType::GPUOnion);

		Constants.TotalPhysical = TotalPhysicalMemorySize;
		Constants.PageSize = CPUPageSize;
		Constants.OsAllocationGranularity = CPUPageSize;
		Constants.BinnedPageSize = CPUPageSize;
		Constants.BinnedAllocationGranularity = 0;
		Constants.AddressLimit = FMath::RoundUpToPowerOfTwo64(GetVirtualAddrEnd(EHeapType::CPU));
		Constants.TotalPhysicalGB = FMath::DivideAndRoundUp(Constants.TotalPhysical, 1024ull * 1024ull * 1024ull);
	}
	
	enum class EVirtualAllocResult
	{
		OK,
		RetryFrameBuffer,
		RetryFlexible,
		FailedVirtual,
		FailedFlexibleMap,
		FailedPoolCommit
	};

	inline EVirtualAllocResult VirtualAllocInternal(EHeapType HeapType, uint64 Size, uint32 Alignment, uintptr_t& OutResult)
	{
		int32 Result;

		int32 MemProt = GetSceKernelMemoryProtection(HeapType);
		SceKernelMemoryType MemType = GetSceKernelMemoryType(HeapType);
		FVirtualAddressAllocator& VirtualAllocator = Allocators[(int32)HeapType];

		// Take the lock for the duration of this function.
		// We need to allocate virtual address space, and then commit the physical memory all in a single operation.
		FScopeLock Lock(&VirtualAllocator.CS);

		uintptr_t Pointer;
		if (!VirtualAllocator.Allocate(Size, Alignment, Pointer))
		{
			// Failed to allocate virtual address space
			return EVirtualAllocResult::FailedVirtual;
		}

		// Back the virtual address range with pages
		switch (HeapType)
		{
		case EHeapType::FrameBuffer:
			// Frame buffer allocations are already pre-mapped on boot.
			break;

		case EHeapType::Flexible:
			// Map flexible memory
			if ((Result = sceKernelMapFlexibleMemory(reinterpret_cast<void**>(&Pointer), Size, GetSceKernelMemoryProtection(EHeapType::Flexible), SCE_KERNEL_MAP_FIXED | SCE_KERNEL_MAP_NO_OVERWRITE)) != 0)
			{
				// Failed to allocate flexible memory pages.
				// Free the virtual allocation we made.
				VirtualAllocator.Free(Pointer, Size);
				OutResult = Result;
				return EVirtualAllocResult::FailedFlexibleMap;
			}
			break;

		default:
			// Map pooled memory
			if ((Result = sceKernelMemoryPoolCommit(reinterpret_cast<void*>(Pointer), Size, MemType, MemProt, 0)) != 0)
			{
				OutResult = Result;

					// Failed to allocate physical pages from the memory pool.
					// Free the virtual allocation we made.
					VirtualAllocator.Free(Pointer, Size);

				// Optionally return a fallback error
				if (Result == SCE_KERNEL_ERROR_ENOMEM)
					{
					if (PS4_MEM_ENABLE_FLEXIBLE_FALLBACK && (MemType == SCE_KERNEL_WB_ONION))
					{
						return EVirtualAllocResult::RetryFlexible;
					}
					else if (PS4_MEM_ENABLE_FRAMEBUFFER_FALLBACK && (HeapType == EHeapType::GPUGarlic))
					{
						return EVirtualAllocResult::RetryFrameBuffer;
					}
				}

				return EVirtualAllocResult::FailedPoolCommit;
			}
			else
			{
#if PS4_MEM_ENABLE_STATS
				TrackDirectAllocation(Size);
#endif // PS4_MEM_ENABLE_STATS
			}
		}

		OutResult = Pointer;
		return EVirtualAllocResult::OK;
	}

	inline void VirtualFreeInternal(EHeapType HeapType, uintptr_t VirtualAddr, uint64 Size)
	{
		int32 Result;

		if (!VirtualAddr)
			return;

		FVirtualAddressAllocator& VirtualAllocator = Allocators[(int32)HeapType];

		// Take the scope lock for the duration of this function.
		// The virtual address space free and memory unmapping have to happen atomically.
		FScopeLock Lock(&VirtualAllocator.CS);

		VirtualAllocator.Free(VirtualAddr, Size);

		switch (HeapType)
		{
		case EHeapType::FrameBuffer:
			// We never unmap frame buffer allocations.
			break;

		case EHeapType::Flexible:
			// Flexible memory free
			if ((Result = sceKernelReleaseFlexibleMemory(reinterpret_cast<void*>(VirtualAddr), Size)) != 0)
			{
				UE_LOG(LogPS4, Fatal, TEXT("sceKernelReleaseFlexibleMemory failed with error code: 0x%08x"), Result);
			}
			break;

		default:
			// Pooled memory free
			if ((Result = sceKernelMemoryPoolDecommit(reinterpret_cast<void*>(VirtualAddr), Size, 0)) != 0)
			{
				UE_LOG(LogPS4, Fatal, TEXT("sceKernelMemoryPoolDecommit failed with error code: 0x%08x"), Result);
			}
			else
			{
#if PS4_MEM_ENABLE_STATS
				TrackDirectAllocation(-int64(Size));
#endif // PS4_MEM_ENABLE_STATS
			}
			break;
		}
	}

public:
	static void Init();

	static inline FMemoryHeapManager& Get() { return *Singleton; }

	inline void* VirtualAlloc_NoLLM(EHeapType Type, uint64 Size, uint32 Alignment)
	{
		uintptr_t Result;
		EHeapType OriginalType = Type;

	RetryAlloc:
		switch (VirtualAllocInternal(Type, Size, Alignment, Result))
		{
		case EVirtualAllocResult::RetryFlexible:
			// No direct memory is available, so we should retry the allocation from the flexible pool.
			Type = EHeapType::Flexible;
			goto RetryAlloc;

		case EVirtualAllocResult::RetryFrameBuffer:
			// No direct memory is available, so we should retry the allocation from the frame buffer pool.
			Type = EHeapType::FrameBuffer;
			goto RetryAlloc;



		case EVirtualAllocResult::FailedVirtual:
			// Out of virtual memory condition.
			FPlatformMisc::LowLevelOutputDebugStringf(
				TEXT("\r\nFailed to allocate virtual address space. (Original Type: %s, Type: %s, Size: %llu, Alignment: %d)\r\n"),
				GetHeapTypeName(OriginalType), GetHeapTypeName(Type), Size, Alignment);

			FPlatformMemory::OnOutOfMemory(Size, Alignment);
			return nullptr;

		case EVirtualAllocResult::FailedFlexibleMap:
			// Out of physical memory condition.
			FPlatformMisc::LowLevelOutputDebugStringf(
				TEXT("\r\nsceKernelMapFlexibleMemory failed with error code 0x%08x. (Original Type: %s, Type: %s, Size: %llu, Alignment: %d)\r\n"),
				Result, GetHeapTypeName(OriginalType), GetHeapTypeName(Type), Size, Alignment);

			FPlatformMemory::OnOutOfMemory(Size, Alignment);
			return nullptr;

		case EVirtualAllocResult::FailedPoolCommit:
			// Out of physical memory condition.
			FPlatformMisc::LowLevelOutputDebugStringf(
				TEXT("\r\nsceKernelMemoryPoolCommit failed with error code 0x%08x. (Original Type: %s, Type: %s, Size: %llu, Alignment: %d)\r\n"),
				Result, GetHeapTypeName(OriginalType), GetHeapTypeName(Type), Size, Alignment);
			
			FPlatformMemory::OnOutOfMemory(Size, Alignment);
			return nullptr;
		}

#if PS4_MEM_ENABLE_STATS
		// Update stats
		TrackVirtualAllocation(Size);
#endif // PS4_MEM_ENABLE_STATS

		return reinterpret_cast<void*>(Result);
	}

	inline void VirtualFree_NoLLM(void* VirtualAddr, uint64 Size)
	{
		uintptr_t VirtualAddrPtr = reinterpret_cast<uintptr_t>(VirtualAddr);
		EHeapType HeapType = GetVirtualAddressType(VirtualAddrPtr);
		VirtualFreeInternal(HeapType, VirtualAddrPtr, Size);

#if PS4_MEM_ENABLE_STATS
		// Update stats
		TrackVirtualAllocation(-int64(Size));
#endif // PS4_MEM_ENABLE_STATS
	}

	inline void* VirtualAlloc(EHeapType Type, uint64 Size, uint32 Alignment)
	{
		void* Address = VirtualAlloc_NoLLM(Type, Size, Alignment);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
		// Don't track frame buffer allocations. The frame
		// buffer pool is allocated on boot and tracked then.
		if (Type != EHeapType::FrameBuffer)
		{
			FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Address, Size);
		}
#endif

		return Address;
	}

	inline void VirtualFree(void* VirtualAddr, uint64 Size)
	{
#if ENABLE_LOW_LEVEL_MEM_TRACKER
		// Don't track frame buffer allocations.
		if (GetVirtualAddressType((uintptr_t)VirtualAddr) != EHeapType::FrameBuffer)
		{
			FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, VirtualAddr);
		}
#endif

		VirtualFree_NoLLM(VirtualAddr, Size);
	}

	inline FPlatformMemoryConstants const& GetConstants() const
	{
		return Constants;
	}

	inline FPlatformMemoryStats GetStats()
	{
		FPlatformMemoryStats Stats;

#if PS4_MEM_ENABLE_STATS

		Stats.UsedPhysical = FPlatformAtomics::AtomicRead(&TotalAllocatedDirectBytes);
		Stats.UsedVirtual  = FPlatformAtomics::AtomicRead(&TotalAllocatedVirtualBytes);

		// Add the total flexible memory used (including system allocations) to "used physical/virtual"
		size_t AvailableFlexibleSize;
		sceKernelAvailableFlexibleMemorySize(&AvailableFlexibleSize);
		size_t UsedFlexible = PS4_FLEXIBLE_MEMORY_SIZE - AvailableFlexibleSize;
		Stats.UsedPhysical += UsedFlexible;
		Stats.UsedVirtual  += UsedFlexible;

		Stats.AvailablePhysical = FMath::Max(TotalPhysicalMemorySize - Stats.UsedPhysical, 0ull);
		Stats.AvailableVirtual  = FMath::Max(GTotalVirtualMemory - Stats.UsedVirtual, 0ull);

		Stats.Direct            = Allocators[(int32)EHeapType::CPU        ].GetAllocatedBytes();
		Stats.Garlic            = Allocators[(int32)EHeapType::GPUGarlic  ].GetAllocatedBytes();
		Stats.Onion             = Allocators[(int32)EHeapType::GPUOnion   ].GetAllocatedBytes();
		Stats.Flexible          = Allocators[(int32)EHeapType::Flexible   ].GetAllocatedBytes();
		Stats.FrameBuffer       = Allocators[(int32)EHeapType::FrameBuffer].GetAllocatedBytes();

		Stats.MemBlockUsed      = FPlatformAtomics::AtomicRead(&MemBlockStat_Used);
		Stats.MemBlockWasted    = FPlatformAtomics::AtomicRead(&MemBlockStat_Wasted);
		Stats.MemBlockTotal     = Stats.MemBlockUsed + Stats.MemBlockWasted;
#endif // PS4_MEM_ENABLE_STATS

		return Stats;
	}

#if PS4_MEM_ENABLE_GPO
	inline void UpdateGPO()
	{
		FPlatformMemoryStats Stats = GetStats();
		double Usage = double(Stats.UsedPhysical) / double(Stats.TotalPhysical);

#if PS4_MEM_GPO_TYPE == 1
		char NewValue = char(Usage * 255.0);
#elif PS4_MEM_GPO_TYPE == 2
		char NewValue = char((1u << uint32(Usage * 8.0)) - 1);
#endif

		if (Usage > 1.0)
		{
			NewValue = (FMath::Fmod(double(sceKernelGetProcessTime()) / 1000000.0, 0.25) > 0.125) ? 255u : 0;
		}

		if (NewValue != GPOValue)
		{
			GPOValue = NewValue;
			sceKernelSetGPO(GPOValue);
		}
	}
#endif // PS4_MEM_ENABLE_GPO

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	inline void LLMInit()
	{
		FLowLevelMemTracker::Get().SetProgramSize(ProgramSize);

		LLM_PLATFORM_SCOPE(ELLMTag::GraphicsPlatform);
		FLowLevelMemTracker::Get().OnLowLevelAlloc(
			ELLMTracker::Platform,
			reinterpret_cast<const void*>(GetVirtualAddrStart(EHeapType::FrameBuffer)),
			GetVirtualAddrLength(EHeapType::FrameBuffer));
	}
#endif
};

void FMemoryHeapManager::Init()
{
	static FMemoryHeapManager Manager;
	Singleton = &Manager;
}

FMemoryHeapManager* FMemoryHeapManager::Singleton = nullptr;

// -----------------------------------------------------------------------------------------------------
//
//                                     FPS4PlatformMemory Interface
//
// -----------------------------------------------------------------------------------------------------

GPUMemStatDump FPS4PlatformMemory::GPUStatDumpFunc = nullptr;

void FPS4PlatformMemory::Init()
{
	// Only allow this method to be called once
	{
		static bool bInitDone = false;
		if (bInitDone)
			return;
		bInitDone = true;
	}

	// Force init of the heap manager singleton.
	FMemoryHeapManager::Init();

#if PS4_MEM_ENABLE_GPO
	// Start the GPO thread
	auto Func = [](void* Arg) -> void*
	{
		// Set this thread to lowest priority
		ScePthread SelfThread = scePthreadSelf();
		SceKernelSchedParam Sched = {};
		Sched.sched_priority = SCE_KERNEL_PRIO_FIFO_LOWEST;
		scePthreadSetschedparam(SelfThread, SCHED_FIFO, &Sched);

		FMemoryHeapManager* Manager = (FMemoryHeapManager*)Arg;
		while (true)
		{
			Manager->UpdateGPO();
			sceKernelUsleep(10000); // 10 ms
		}
	};

	ScePthread Thread = {};
	ScePthreadAttr ThreadAttr = {};
	scePthreadAttrInit(&ThreadAttr);
	scePthreadAttrSetstacksize(&ThreadAttr, size_t(SCE_PTHREAD_STACK_MIN)); // 16 KB Stack
	scePthreadCreate(&Thread, &ThreadAttr, Func, &FMemoryHeapManager::Get(), "PS4MemGPO_Thread");
	scePthreadAttrDestroy(&ThreadAttr);
#endif // PS4_MEM_ENABLE_GPO

	FGenericPlatformMemory::Init();	

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	FMemoryHeapManager::Get().LLMInit();
#endif
}

FMalloc* FPS4PlatformMemory::BaseAllocator()
{
	static FMallocBinned2 Malloc;
	return &Malloc;
}

const FPlatformMemoryConstants& FPS4PlatformMemory::GetConstants()
{
	return FMemoryHeapManager::Get().GetConstants();
}

FPlatformMemoryStats FPS4PlatformMemory::GetStats()
{
	return FMemoryHeapManager::Get().GetStats();
}

void FPS4PlatformMemory::DumpStats(FOutputDevice& Ar)
{
	FGenericPlatformMemory::DumpStats(Ar);

#if PS4_MEM_ENABLE_STATS
	const float InvMB = 1.0f / 1024.0f / 1024.0f;
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
#if !NO_LOGGING
	const FName CategoryName(LogMemory.GetCategoryName());
#else
	const FName CategoryName(TEXT("LogMemory"));
#endif
	Ar.CategorizedLogf(CategoryName, ELogVerbosity::Log, TEXT("Allocated Memory: CPU %.2f MB, Garlic %.2f MB, Onion %.2f MB, FrameBuffer %.2f MB, Flexible %.2f MB"),
		MemoryStats.Direct * InvMB,
		MemoryStats.Garlic * InvMB,
		MemoryStats.Onion * InvMB,
		MemoryStats.FrameBuffer * InvMB,
		MemoryStats.Flexible * InvMB);
#endif // PS4_MEM_ENABLE_STATS
}

void FPS4PlatformMemory::InternalUpdateStats(const FPlatformMemoryStats& MemoryStats)
{
	FGenericPlatformMemory::InternalUpdateStats(MemoryStats);

#if (STATS && PS4_MEM_ENABLE_STATS)
	SET_MEMORY_STAT(MCR_Flexible,    PS4_FLEXIBLE_MEMORY_SIZE);
	SET_MEMORY_STAT(MCR_Physical,    FMemoryHeapManager::Get().TotalPhysicalMemorySize);
	SET_MEMORY_STAT(MCR_PhysicalLLM, FMemoryHeapManager::Get().TotalPhysicalMemorySize);
	SET_MEMORY_STAT(MCR_FrameBuffer, GetVirtualAddrLength(EHeapType::FrameBuffer));

	SET_MEMORY_STAT(STAT_PS4Memory_AllocatedCPU,         MemoryStats.Direct);
	SET_MEMORY_STAT(STAT_PS4Memory_AllocatedGarlic,      MemoryStats.Garlic);
	SET_MEMORY_STAT(STAT_PS4Memory_AllocatedOnion,       MemoryStats.Onion);
	SET_MEMORY_STAT(STAT_PS4Memory_AllocatedFrameBuffer, MemoryStats.FrameBuffer);
	SET_MEMORY_STAT(STAT_PS4Memory_AllocatedFlexible,    MemoryStats.Flexible);

	size_t FlexibleMemSize = 0;
	sceKernelAvailableFlexibleMemorySize(&FlexibleMemSize);
	SET_MEMORY_STAT(STAT_PS4Memory_AvailableFlexible,  FlexibleMemSize);
	SET_MEMORY_STAT(STAT_PS4Memory_SystemUsedFlexible, PS4_FLEXIBLE_MEMORY_SIZE - MemoryStats.Flexible - FlexibleMemSize);

	SET_MEMORY_STAT(STAT_PS4Memory_MemBlockTotal,  MemoryStats.MemBlockTotal);
	SET_MEMORY_STAT(STAT_PS4Memory_MemBlockUsed,   MemoryStats.MemBlockUsed);
	SET_MEMORY_STAT(STAT_PS4Memory_MemBlockWasted, MemoryStats.MemBlockWasted);
#endif // (STATS && PS4_MEM_ENABLE_STATS)
}

void FPS4PlatformMemory::OnOutOfMemory(uint64 Size, uint32 Alignment)
{
	FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
	
	const float InvMB = 1.0f / 1024.0f / 1024.0f;
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\r\nPS4 OOM: CPU %.2f MB, Garlic %.2f MB, Onion %.2f MB, FrameBuffer %.2f MB, Flexible %.2f MB\r\n"),
		MemoryStats.Direct * InvMB,
		MemoryStats.Garlic * InvMB,
		MemoryStats.Onion * InvMB,
		MemoryStats.FrameBuffer * InvMB,
		MemoryStats.Flexible * InvMB);

	FGenericPlatformMemory::OnOutOfMemory(Size, Alignment);
}

#if !UE_BUILD_SHIPPING	// This function is not guaranteed to be correct and should not be called in shipping builds
bool FPS4PlatformMemory::IsExtraDevelopmentMemoryAvailable()
{
	uint64 DMemSize = SCE_KERNEL_MAIN_DMEM_SIZE;

	bool bNormalMemoryMode = sceKernelIsNeoMode()
		? (DMemSize == GStandardMemorySize_ForDebugMemoryDetection_Neo)
		: (DMemSize == GStandardMemorySize_ForDebugMemoryDetection_Base);

	return bNormalMemoryMode == false;
}
#endif

void* FPS4PlatformMemory::BinnedAllocFromOS(SIZE_T Size)
{
	return FMemoryHeapManager::Get().VirtualAlloc(EHeapType::CPU, Size, CPUPageSize);
}

void FPS4PlatformMemory::BinnedFreeToOS(void* Ptr, SIZE_T Size)
{
	FMemoryHeapManager::Get().VirtualFree(Ptr, Size);
}




// -----------------------------------------------------------------------------------------------------
//
//                                    FMemBlock GPU Memory Allocator
//
// -----------------------------------------------------------------------------------------------------

static const constexpr uint32 GGPUBlockSizesInBytes[] =
{
	0x00000008, //   8  B
	0x00000010, //  16  B
	0x00000020, //  32  B
	0x00000040, //  64  B
	0x00000080, // 128  B
	0x00000100, // 256  B
	0x00000200, // 512  B
	0x00000400, //   1 KB
	0x00000800, //   2 KB
	0x00001000, //   4 KB
	0x00002000, //   8 KB
	0x00004000, //  16 KB
	0x00008000, //  32 KB
#if PS4_MEM_ENABLE_GPU_2MB_PAGES
	0x00010000, //  64 KB
	0x00020000, // 128 KB
	0x00040000, // 256 KB
	0x00080000, // 512 KB
	0x00100000  //   1 MB
#endif // PS4_MEM_ENABLE_GPU_2MB_PAGES
};

inline uint32 GetBlockSizeIndex(uint32 Size)
{
	for (int32 Index = 0; Index < ARRAY_COUNT(GGPUBlockSizesInBytes); ++Index)
	{
		if (Size <= GGPUBlockSizesInBytes[Index])
			return Index;
	}
	
	return INDEX_NONE;
}

// Sub-allocates fixed sized blocks from a single page.
class FGPUMemoryPage : public TIntrusiveLinkedList<FGPUMemoryPage>
{
	// Pointer to the allocated GPU page.
	void* const Base;

	EHeapType const HeapType;
	uint32 const BlockSize;

	TMipFieldPageAllocator<FMipFieldRunTimeAllocator> BitField;

public:
	FGPUMemoryPage(EHeapType InHeapType, uint32 InBlockSize)
		: Base(FMemoryHeapManager::Get().VirtualAlloc(InHeapType, GPUPageSize, GPUPageSize))
		, HeapType(InHeapType)
		, BlockSize(InBlockSize)
		, BitField(GPUPageSize / BlockSize)
	{
		checkf(FMath::IsPowerOfTwo(InBlockSize), TEXT("InBlockSize must be a power of two."));
	}

	~FGPUMemoryPage()
	{
		FMemoryHeapManager::Get().VirtualFree(Base, GPUPageSize);
	}

	inline bool IsFull() const { return BitField.IsFull(); }

	inline void Suballocate(void*& OutPointer)
	{
		PS4MemCheck(!IsFull());

		uint64 PageIndex;
		bool bAllocated = BitField.AllocatePages(1, 1, PageIndex);
		PS4MemCheck(bAllocated);

		OutPointer = reinterpret_cast<uint8*>(Base) + (BlockSize * PageIndex);

#if PS4_MEM_DO_CHECK
		if (IsFull())
		{
			// Verify no more slots can be allocated
			bAllocated = BitField.AllocatePages(1, 1, PageIndex);
			PS4MemCheck(bAllocated == false);
		}
#endif
	}

	inline void Free(void* Pointer)
	{
		uintptr_t Offset = reinterpret_cast<uintptr_t>(Pointer) - reinterpret_cast<uintptr_t>(Base);
		uintptr_t PageIndex = Offset / BlockSize;
		BitField.FreePages(PageIndex, 1);
	}
};

struct FGPUMemoryPageListHead
{
	FCriticalSection CS;
	FGPUMemoryPage* List;

	FGPUMemoryPageListHead()
		: List(nullptr)
	{}

} GPagesWithFreeBlocks[(int32)EHeapType::Num][ARRAY_COUNT(GGPUBlockSizesInBytes)];

inline FGPUMemoryPageListHead& GetSmallBlockHead(FMemBlock const& Block, EHeapType& OutHeapType, int32& OutBlockIndex)
{
	OutHeapType = GnmMemTypeToHeapType(Block.MemType);
	OutBlockIndex = GetBlockSizeIndex(Block.AlignedSize);
	check(OutBlockIndex != INDEX_NONE);

	return GPagesWithFreeBlocks[(int32)OutHeapType][OutBlockIndex];
}

void AllocateSmallBlock(FMemBlock& Block)
{
	EHeapType HeapType; 
	int32 BlockIndex;
	FGPUMemoryPageListHead& Head = GetSmallBlockHead(Block, HeapType, BlockIndex);

	FScopeLock Lock(&Head.CS);

	if (Head.List)
	{
		// At least one page exists with free space. Take from the first available.
		Block.SmallBlockPage = Head.List;
		Head.List->Suballocate(Block.Pointer);
		PS4MemCheck(Block.Pointer != nullptr);

		if (Head.List->IsFull())
		{
			// Page has no more free space
			Head.List->Unlink();
		}
	}
	else
	{
		// No pages with remaining free blocks. Allocate a new one.
		FGPUMemoryPage* Page = new FGPUMemoryPage(HeapType, GGPUBlockSizesInBytes[BlockIndex]);
		Block.SmallBlockPage = Page;
		Page->Suballocate(Block.Pointer);
		PS4MemCheck(Block.Pointer != nullptr);

		if (Page->IsFull() == false)
		{
			// Page has remaining space.
			Page->LinkHead(Head.List);
		}
	}
}

void FreeSmallBlock(FMemBlock& Block)
{
	PS4MemCheck(Block.SmallBlockPage != nullptr);

	EHeapType HeapType;
	int32 BlockIndex;
	FGPUMemoryPageListHead& Head = GetSmallBlockHead(Block, HeapType, BlockIndex);

	FScopeLock Lock(&Head.CS);

	bool bWasFull = Block.SmallBlockPage->IsFull();
	Block.SmallBlockPage->Free(Block.Pointer);

	if (bWasFull)
	{
		// This block *was* full, but now has an empty slot.
		Block.SmallBlockPage->LinkHead(Head.List);
	}
}

FMemBlock FMemBlock::Allocate(uint32 Size, uint32 Alignment, EGnmMemType Type, TStatId Stat)
{
	LLM_PLATFORM_SCOPE(ELLMTag::GraphicsPlatform);

	check(Size > 0);
	check(FMath::IsPowerOfTwo(Alignment) && Alignment > 0);

	FMemBlock Block;
	Block.MemType = Type;
	Block.Size = Size;
	Block.AlignedSize = Align(Size, Alignment);

	EHeapType HeapType = GnmMemTypeToHeapType(Type);
	check(HeapType != EHeapType::Num);

	if (Type == EGnmMemType::GnmMem_FrameBuffer || Block.AlignedSize > GGPUBlockSizesInBytes[ARRAY_COUNT(GGPUBlockSizesInBytes) - 1])
	{
		// Allocations greater than the largest block size are made in
		// whole pages directly from the virtual memory allocator.

		// Align to page boundary. Round size up to whole page.
		Alignment = FMath::Max(Alignment, uint32(GPUPageSize));
		Block.AlignedSize = Align(Size, Alignment);

		Block.Pointer = FMemoryHeapManager::Get().VirtualAlloc(HeapType, Block.AlignedSize, Alignment);
		Block.SmallBlockPage = nullptr;
	}
	else
	{
		// Otherwise we allocate from the sub-page allocator
		AllocateSmallBlock(Block);
	}
	
	PS4MemCheck(Block.Pointer != nullptr);

#if PS4_MEM_ENABLE_STATS
	FPlatformAtomics::InterlockedAdd(&MemBlockStat_Used, Block.Size);
	FPlatformAtomics::InterlockedAdd(&MemBlockStat_Wasted, Block.AlignedSize - Block.Size);
#endif // PS4_MEM_ENABLE_STATS

	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default, Block.Pointer, Block.Size));

	return Block;
}

FCriticalSection GFreeQueueCS;
TMap<uint64, TArray<FMemBlock>> GFreeQueue;
uint64 GNextFreeFenceValue = 1;

#if DO_CHECK
bool FMemBlock::HasPendingFrees()
{
	FScopeLock Lock(&GFreeQueueCS);

	// Ensure that all pending blocks have been freed, so there is no outstanding memory.
	for (auto Iter = GFreeQueue.CreateIterator(); Iter; ++Iter)
	{
		if (Iter.Value().Num() > 0)
			return true;
	}

	return false;
}
#endif

void FMemBlock::Free(FMemBlock Block)
{
	// Don't bother adding nullptr blocks to the free list.
	if (Block.GetPointer())
	{
		FScopeLock Lock(&GFreeQueueCS);
		GFreeQueue.FindOrAdd(GNextFreeFenceValue).Add(Block);
	}
}

uint64 FMemBlock::ProcessDelayedFrees(uint64 FenceValue)
{
	FScopeLock Lock(&GFreeQueueCS);

	for (auto Iter = GFreeQueue.CreateIterator(); Iter; ++Iter)
	{
		if (Iter.Key() > FenceValue)
		{
			// Skip frames that have not completed on the GPU yet.
			// The GPU may still be using this memory.
			continue;
		}

		for (FMemBlock& Block : Iter.Value())
		{
			if (Block.Pointer != nullptr && Block.MemType != EGnmMemType::GnmMem_Invalid)
			{
				LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default, Block.Pointer));

				if (Block.SmallBlockPage)
				{
					// Free the small block allocation
					FreeSmallBlock(Block);
				}
				else
				{
					FMemoryHeapManager::Get().VirtualFree(Block.Pointer, Block.AlignedSize);
				}

#if PS4_MEM_ENABLE_STATS
				FPlatformAtomics::InterlockedAdd(&MemBlockStat_Used, -int64(Block.Size));
				FPlatformAtomics::InterlockedAdd(&MemBlockStat_Wasted, -int64(Block.AlignedSize - Block.Size));
#endif // PS4_MEM_ENABLE_STATS
			}
		}

		Iter.RemoveCurrent();
	}

	// Increment the next fence value, and grab the current fence to signal on the GPU.
	uint64 NextFence = GNextFreeFenceValue++;

	// Return the current fence value to the higher level RHI.
	return NextFence;
}

void FMemBlock::Dump()
{
}

FGnmMemoryStat FMemBlock::GetStats()
{
	return FGnmMemoryStat();
}




// -----------------------------------------------------------------------------------------------------
//
//                                   AVPlayer Media Library Allocator
//
// -----------------------------------------------------------------------------------------------------

namespace PS4MediaAllocators
{
	static FCriticalSection GMediaAllocatorCS;
	static TMap<void*, FMemBlock> GMediaAllocationsMap;

	void* Allocate(void* Jumpback, uint32 Alignment, uint32 Size)
	{
		FMemBlock Block = FMemBlock::Allocate(Size, Alignment, EGnmMemType::GnmMem_CPU, TStatId());

		{
			FScopeLock Lock(&GMediaAllocatorCS);
			GMediaAllocationsMap.Add(Block.Pointer, Block);
		}

		return Block.Pointer;
	}

	void* AllocateTexture(void* Jumpback, uint32 Alignment, uint32 Size)
	{
		FMemBlock Block = FMemBlock::Allocate(Size, Alignment, EGnmMemType::GnmMem_GPU, TStatId());

		{
			FScopeLock Lock(&GMediaAllocatorCS);
			GMediaAllocationsMap.Add(Block.Pointer, Block);
		}

		return Block.Pointer;
	}

	void Deallocate(void* Jumpback, void* Memory)
	{
		FMemBlock Block;

		{
			FScopeLock Lock(&GMediaAllocatorCS);
			Block = GMediaAllocationsMap.FindAndRemoveChecked(Memory);
		}

		FMemBlock::Free(Block);
	}

	void DeallocateTexture(void* Jumpback, void* Memory)
	{
		FMemBlock Block;

		{
			FScopeLock Lock(&GMediaAllocatorCS);
			Block = GMediaAllocationsMap.FindAndRemoveChecked(Memory);
		}

		FMemBlock::Free(Block);
	}
}

// -----------------------------------------------------------------------------------------------------
//
//                                    Low-Level Memory Tracker Hooks
//
// -----------------------------------------------------------------------------------------------------

#if ENABLE_LOW_LEVEL_MEM_TRACKER

void* LLMAlloc(size_t Size)
{
	return FMemoryHeapManager::Get().VirtualAlloc_NoLLM(EHeapType::CPU, Align(Size, CPUPageSize), CPUPageSize);
}
void LLMFree(void* Addr, size_t Size)
{
	FMemoryHeapManager::Get().VirtualFree_NoLLM(Addr, Align(Size, CPUPageSize));
}

bool FPS4PlatformMemory::GetLLMAllocFunctions(void*(*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment)
{
	OutAllocFunction = LLMAlloc;
	OutFreeFunction = LLMFree;
	OutAlignment = CPUPageSize;

	return true;
}

#endif // ENABLE_LOW_LEVEL_MEM_TRACKER

// -----------------------------------------------------------------------------------------------------
//
//                                    Libc User Malloc/Free Overrides
//
// -----------------------------------------------------------------------------------------------------

extern "C"
{
	int user_malloc_init    (void) { FPS4PlatformMemory::Init(); return 0; }
	int user_malloc_finalize(void) { return 0; }

	void  user_free         (void* ptr)                               { return FPS4PlatformMemory::BaseAllocator()->Free(ptr); }
	void* user_malloc       (size_t size)                             { return FPS4PlatformMemory::BaseAllocator()->Malloc(size); }
	void* user_realloc      (void* ptr, size_t size)                  { return FPS4PlatformMemory::BaseAllocator()->Realloc(ptr, size); }
	void* user_memalign     (size_t boundary, size_t size)            { return FPS4PlatformMemory::BaseAllocator()->Malloc(size, boundary); }
	void* user_reallocalign (void* ptr, size_t size, size_t boundary) { return FPS4PlatformMemory::BaseAllocator()->Realloc(ptr, size, boundary); }

	void* user_calloc(size_t nelem, size_t size)
	{	
		void* pMem = FPS4PlatformMemory::BaseAllocator()->Malloc(nelem * size);
		FMemory::Memzero(pMem, nelem * size);
		return pMem;
	}

	int user_posix_memalign(void** ptr, size_t boundary, size_t size)
	{
		if (!FMath::IsPowerOfTwo(boundary) || ((boundary % sizeof(void*)) != 0))
		{
			return EINVAL;
		}

		void* pMem = FPS4PlatformMemory::BaseAllocator()->Malloc(size, boundary);
		if (pMem)
		{
			*ptr = pMem;
			return 0;
		}
		else
		{
			return ENOMEM;
		}
	}

	int user_malloc_stats         (SceLibcMallocManagedSize* mmsize) { abort(); *mmsize = {}; return 0; } // Not implemented
	int user_malloc_stats_fast    (SceLibcMallocManagedSize* mmsize) { abort(); *mmsize = {}; return 0; } // Not implemented
	size_t user_malloc_usable_size(void* ptr)                        { abort();               return 0; } // Not implemented
}

void* user_new        (std::size_t size, const std::nothrow_t& x)            throw() { return user_malloc(size); }
void* user_new_array  (std::size_t size, const std::nothrow_t& x)            throw() { return user_malloc(size); }
void* user_new        (std::size_t size)                       throw(std::bad_alloc) { return user_malloc(size); }
void* user_new_array  (std::size_t size)                       throw(std::bad_alloc) { return user_malloc(size); }

void user_delete      (void* ptr)                                            throw() { if (ptr) user_free(ptr); }
void user_delete      (void* ptr,                   const std::nothrow_t& x) throw() { if (ptr) user_free(ptr); }
void user_delete_array(void* ptr)                                            throw() { if (ptr) user_free(ptr); }
void user_delete_array(void* ptr,                   const std::nothrow_t& x) throw() { if (ptr) user_free(ptr); }
void user_delete      (void *ptr, std::size_t size)                          throw() { if (ptr) user_free(ptr); }
void user_delete      (void* ptr, std::size_t size, const std::nothrow_t& x) throw() { if (ptr) user_free(ptr); }
void user_delete_array(void* ptr, std::size_t size)                          throw() { if (ptr) user_free(ptr); }
void user_delete_array(void* ptr, std::size_t size, const std::nothrow_t& x) throw() { if (ptr) user_free(ptr); }

#endif // USE_NEW_PS4_MEMORY_SYSTEM
