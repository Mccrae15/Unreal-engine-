// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4/PS4LLM.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

struct FLLMTagInfoPS4
{
	const TCHAR* Name;
	FName StatName;				// shows in the LLMFULL stat group
	FName SummaryStatName;		// shows in the LLM summary stat group
};

DECLARE_LLM_MEMORY_STAT(TEXT("GnmMisc"), STAT_GnmMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GnmTempBlocks"), STAT_GnmTempBlocksLLM, STATGROUP_LLMFULL);

#if !USE_NEW_PS4_MEMORY_SYSTEM
DECLARE_LLM_MEMORY_STAT(TEXT("MallocPool"), STAT_MallocPoolLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Malloc"), STAT_MallocLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GarlicHeap"), STAT_GarlicHeapLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("OnionHeap"), STAT_OnionHeapLLM, STATGROUP_LLMPlatform);
#endif

// *** order must match ELLMTagPS4 enum ***
const FLLMTagInfoPS4 ELLMTagNamesPS4[] =
{
	// csv name						// stat name								// summary stat name						// enum value
	{ TEXT("GnmMisc"),				GET_STATFNAME(STAT_GnmMiscLLM),				GET_STATFNAME(STAT_EngineSummaryLLM) },		// ELLMTagPS4::GnmMisc
	{ TEXT("GnmTempBlocks"),		GET_STATFNAME(STAT_GnmTempBlocksLLM),		GET_STATFNAME(STAT_EngineSummaryLLM) },		// ELLMTagPS4::GnmTempBlocks

#if !USE_NEW_PS4_MEMORY_SYSTEM
	{ TEXT("MallocPool"),			GET_STATFNAME(STAT_MallocPoolLLM),			NAME_None },								// ELLMTagPS4::MallocPool
	{ TEXT("Malloc"),				GET_STATFNAME(STAT_MallocLLM),				GET_STATFNAME(STAT_EngineSummaryLLM) },		// ELLMTagPS4::Malloc
	{ TEXT("GarlicHeap"),			GET_STATFNAME(STAT_GarlicHeapLLM),			NAME_None },								// ELLMTagPS4::GarlicHeap
	{ TEXT("OnionHeap"),			GET_STATFNAME(STAT_OnionHeapLLM),			NAME_None },								// ELLMTagPS4::OnionHeap
#endif
};

/*
 * Register PS4 tags with LLM
 */
void PS4LLM::Initialise()
{
	int32 TagCount = sizeof(ELLMTagNamesPS4) / sizeof(FLLMTagInfoPS4);

	for (int32 Index = 0; Index < TagCount; ++Index)
	{
		int32 Tag = (int32)ELLMTag::PlatformTagStart + Index;
		const FLLMTagInfoPS4& TagInfo = ELLMTagNamesPS4[Index];

		FLowLevelMemTracker::Get().RegisterPlatformTag(Tag, TagInfo.Name, TagInfo.StatName, TagInfo.SummaryStatName);
	}
}

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

