// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyLLM.h"
#include "HAL/LowLevelMemStats.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

struct FLLMTagInfoSony
{
	const TCHAR* Name;
	FName StatName;				// shows in the LLMFULL stat group
	FName SummaryStatName;		// shows in the LLM summary stat group
};

DECLARE_LLM_MEMORY_STAT(TEXT("GnmMisc"), STAT_GnmMiscLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("GnmTempBlocks"), STAT_GnmTempBlocksLLM, STATGROUP_LLMFULL);

/*
 * Register Sony tags with LLM
 */
void SonyLLM::Initialise()
{
	// *** order must match ELLMTagSony enum ***
	static const FLLMTagInfoSony ELLMTagNamesSony[] =
	{
		// csv name						// stat name								// summary stat name						// enum value
		{ TEXT("GnmMisc"),				GET_STATFNAME(STAT_GnmMiscLLM),				GET_STATFNAME(STAT_EngineSummaryLLM) },		// ELLMTagSony::GnmMisc
		{ TEXT("GnmTempBlocks"),		GET_STATFNAME(STAT_GnmTempBlocksLLM),		GET_STATFNAME(STAT_EngineSummaryLLM) },		// ELLMTagSony::GnmTempBlocks
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ELLMTagNamesSony); ++Index)
	{
		int32 Tag = (int32)ELLMTag::PlatformTagStart + Index;
		const FLLMTagInfoSony& TagInfo = ELLMTagNamesSony[Index];

		FLowLevelMemTracker::Get().RegisterPlatformTag(Tag, TagInfo.Name, TagInfo.StatName, TagInfo.SummaryStatName);
	}
}

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

