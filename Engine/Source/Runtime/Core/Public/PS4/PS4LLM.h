// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/LowLevelMemTracker.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

#define LLM_SCOPE_PS4(Tag) LLM_SCOPE((ELLMTag)Tag)
#define LLM_PLATFORM_SCOPE_PS4(Tag) LLM_PLATFORM_SCOPE((ELLMTag)Tag)

enum class ELLMTagPS4 : LLM_TAG_TYPE
{
	GnmMisc = (LLM_TAG_TYPE)ELLMTag::PlatformTagStart,
	GnmTempBlocks,

#if !USE_NEW_PS4_MEMORY_SYSTEM
	MallocPool,
	Malloc,
	GarlicHeap,
	OnionHeap,
#endif

	Count
};

static inline const char* LLMPlatformGetTagNameANSI(ELLMTagPS4 Tag)
{
	switch (Tag)
	{
		case ELLMTagPS4::GnmMisc:		return "GnmMisc";
		case ELLMTagPS4::GnmTempBlocks:	return "GnmTempBlocks";

#if !USE_NEW_PS4_MEMORY_SYSTEM
		case ELLMTagPS4::MallocPool:    return "MallocPool";
		case ELLMTagPS4::Malloc:        return "Malloc";
		case ELLMTagPS4::GarlicHeap:    return "GarlicHeap";
		case ELLMTagPS4::OnionHeap:     return "OnionHeap";
#endif

		default: return nullptr;
	}
}

static_assert((int32)ELLMTagPS4::Count <= (int32)ELLMTag::PlatformTagEnd, "too many ELLMTagPS4 tags");

namespace PS4LLM
{
	void Initialise();
}

#else

#define LLM_SCOPE_PS4(...)
#define LLM_PLATFORM_SCOPE_PS4(...)

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

