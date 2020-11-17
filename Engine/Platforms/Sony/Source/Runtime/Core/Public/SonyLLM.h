// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/LowLevelMemTracker.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

#define LLM_SCOPE_SONY(Tag) LLM_SCOPE((ELLMTag)Tag)
#define LLM_PLATFORM_SCOPE_SONY(Tag) LLM_PLATFORM_SCOPE((ELLMTag)Tag)

enum class ELLMTagSony : LLM_TAG_TYPE
{
	GnmMisc = (LLM_TAG_TYPE)ELLMTag::PlatformTagStart,
	GnmTempBlocks,

	Count
};

static inline const char* LLMPlatformGetTagNameANSI(ELLMTagSony Tag)
{
	switch (Tag)
	{
		case ELLMTagSony::GnmMisc:		return "GnmMisc";
		case ELLMTagSony::GnmTempBlocks:	return "GnmTempBlocks";

		default: return nullptr;
	}
}

static_assert((int32)ELLMTagSony::Count <= (int32)ELLMTag::PlatformTagEnd, "too many ELLMTagSony tags");

namespace SonyLLM
{
	void Initialise();
}

#else

#define LLM_SCOPE_SONY(...)
#define LLM_PLATFORM_SCOPE_SONY(...)

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

