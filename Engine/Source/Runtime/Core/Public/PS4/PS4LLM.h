// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

static_assert((int32)ELLMTagPS4::Count <= (int32)ELLMTag::PlatformTagEnd, "too many ELLMTagPS4 tags");

namespace PS4LLM
{
	void Initialise();
}

#else

#define LLM_SCOPE_PS4(...)
#define LLM_PLATFORM_SCOPE_PS4(...)

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

