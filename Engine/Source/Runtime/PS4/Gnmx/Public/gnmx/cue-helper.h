/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if !defined(_SCE_GNMX_CUE_HELPER_H)
#define _SCE_GNMX_CUE_HELPER_H

#ifdef __ORBIS__
#include <x86intrin.h>
#else //__ORBIS__
#include <intrin.h>
#endif //__ORBIS__
#include <stddef.h>
#include <gnm.h>
#include <gnmx/common.h>
#include <gnmx/cue.h>


#define CUE2_SHOW_UNIMPLEMENTED 0

#ifndef __ORBIS__
#define __builtin_memset memset
#endif // __ORBIS__

namespace ConstantUpdateEngineHelper
{

	typedef sce::Gnmx::ConstantUpdateEngine::StageChunkState sce::Gnmx::ConstantUpdateEngine::StageInfo::* StageChunkStatePtr;
	typedef uint64_t									     sce::Gnmx::ConstantUpdateEngine::StageInfo::* StageQwordPtr;
	typedef uint32_t									     sce::Gnmx::ConstantUpdateEngine::StageInfo::* StageDwordPtr;

	typedef void (sce::Gnmx::ConstantUpdateEngine::* UpdateDelegate)(uint32_t usageSlot, sce::Gnmx::ConstantUpdateEngine::ApplyUsageDataState* state);


	//

	static const uint32_t  kDirtyVsOrPsMask  = (1<<sce::Gnm::kShaderStageVs) | (1<<sce::Gnm::kShaderStagePs);
	static const uint32_t  kMaxResourceTypes = 64;

	SCE_GNM_STATIC_ASSERT((sce::Gnmx::ConstantUpdateEngine::kResourceChunkSize & (sce::Gnmx::ConstantUpdateEngine::kResourceChunkSize-1)) == 0); // needs to be a power of 2
	SCE_GNM_STATIC_ASSERT(sce::Gnmx::ConstantUpdateEngine::kResourceChunkSize <= 16);
	SCE_GNM_STATIC_ASSERT(sce::Gnmx::ConstantUpdateEngine::kResourceChunkSize*sce::Gnmx::ConstantUpdateEngine::kResourceNumChunks == sce::Gnm::kSlotCountResource);
	SCE_GNM_STATIC_ASSERT((sce::Gnmx::ConstantUpdateEngine::kSamplerChunkSize & (sce::Gnmx::ConstantUpdateEngine::kSamplerChunkSize-1)) == 0); // needs to be a power of 2
	SCE_GNM_STATIC_ASSERT(sce::Gnmx::ConstantUpdateEngine::kSamplerChunkSize <= 16);
	SCE_GNM_STATIC_ASSERT(sce::Gnmx::ConstantUpdateEngine::kSamplerChunkSize*sce::Gnmx::ConstantUpdateEngine::kSamplerNumChunks == sce::Gnm::kSlotCountSampler);
	SCE_GNM_STATIC_ASSERT((sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize & (sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize-1)) == 0); // needs to be a power of 2
	SCE_GNM_STATIC_ASSERT(sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize <= 16);
	SCE_GNM_STATIC_ASSERT(sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize*sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks == (sce::Gnm::kSlotCountConstantBuffer+4)); // special case: count = 20.
	SCE_GNM_STATIC_ASSERT((sce::Gnmx::ConstantUpdateEngine::kVertexBufferChunkSize & (sce::Gnmx::ConstantUpdateEngine::kVertexBufferChunkSize-1)) == 0); // needs to be a power of 2
	SCE_GNM_STATIC_ASSERT(sce::Gnmx::ConstantUpdateEngine::kVertexBufferChunkSize <= 16);
	SCE_GNM_STATIC_ASSERT(sce::Gnmx::ConstantUpdateEngine::kVertexBufferChunkSize*sce::Gnmx::ConstantUpdateEngine::kVertexBufferNumChunks == (sce::Gnm::kSlotCountVertexBuffer));
	SCE_GNM_STATIC_ASSERT((sce::Gnmx::ConstantUpdateEngine::kRwResourceChunkSize & (sce::Gnmx::ConstantUpdateEngine::kRwResourceChunkSize-1)) == 0); // needs to be a power of 2
	SCE_GNM_STATIC_ASSERT(sce::Gnmx::ConstantUpdateEngine::kRwResourceChunkSize <= 16);
	SCE_GNM_STATIC_ASSERT(sce::Gnmx::ConstantUpdateEngine::kRwResourceChunkSize*sce::Gnmx::ConstantUpdateEngine::kRwResourceNumChunks == sce::Gnm::kSlotCountRwResource);
	SCE_GNM_STATIC_ASSERT(sce::Gnm::kSlotCountStreamoutBuffer <= 8);

	static const uint32_t		kResourceChunkSizeMask			= sce::Gnmx::ConstantUpdateEngine::kResourceChunkSize-1;
	static const uint32_t		kResourceChunkSizeInDWord		= sce::Gnm::kDwordSizeResource * sce::Gnmx::ConstantUpdateEngine::kResourceChunkSize;
	static const uint32_t		kResourceSizeInDqWord			= sce::Gnm::kDwordSizeResource / 4;
	static const uint32_t		kSamplerChunkSizeMask			= sce::Gnmx::ConstantUpdateEngine::kSamplerChunkSize-1;
	static const uint32_t		kSamplerChunkSizeInDWord		= sce::Gnm::kDwordSizeSampler * sce::Gnmx::ConstantUpdateEngine::kSamplerChunkSize;
	static const uint32_t		kSamplerSizeInDqWord			= sce::Gnm::kDwordSizeSampler / 4;
	static const uint32_t		kConstantBufferChunkSizeMask	= sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize-1;
	static const uint32_t		kConstantBufferChunkSizeInDWord = sce::Gnm::kDwordSizeConstantBuffer * sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize;
	static const uint32_t		kConstantBufferSizeInDqWord		= sce::Gnm::kDwordSizeConstantBuffer / 4;
	static const uint32_t		kVertexBufferChunkSizeMask		= sce::Gnmx::ConstantUpdateEngine::kVertexBufferChunkSize-1;
	static const uint32_t		kVertexBufferChunkSizeInDWord	= sce::Gnm::kDwordSizeVertexBuffer * sce::Gnmx::ConstantUpdateEngine::kVertexBufferChunkSize;
	static const uint32_t		kVertexBufferSizeInDqWord		= sce::Gnm::kDwordSizeVertexBuffer / 4;
	static const uint32_t		kRwResourceChunkSizeMask		= sce::Gnmx::ConstantUpdateEngine::kRwResourceChunkSize-1;
	static const uint32_t		kRwResourceChunkSizeInDWord		= sce::Gnm::kDwordSizeRwResource * sce::Gnmx::ConstantUpdateEngine::kRwResourceChunkSize;
	static const uint32_t		kRwResourceSizeInDqWord			= sce::Gnm::kDwordSizeRwResource / 4;
	static const uint32_t		kStreamoutChunkSizeInBytes		= sce::Gnm::kDwordSizeStreamoutBuffer * sce::Gnm::kSlotCountStreamoutBuffer * 4;
	static const uint32_t		kStreamoutBufferSizeInDqWord	= sce::Gnm::kDwordSizeStreamoutBuffer / 4;


	// Offset into each type's range of data within a shader stage, in DWORDs.
	static const uint32_t		kDwordOffsetResource	   = 0;
	static const uint32_t		kDwordOffsetRwResource     = kDwordOffsetResource                     + sce::Gnm::kSlotCountResource                  * sce::Gnm::kDwordSizeResource;
	static const uint32_t		kDwordOffsetSampler		   = kDwordOffsetRwResource                   + sce::Gnm::kSlotCountRwResource                * sce::Gnm::kDwordSizeRwResource;
	static const uint32_t		kDwordOffsetVertexBuffer   = kDwordOffsetSampler                      + sce::Gnm::kSlotCountSampler                   * sce::Gnm::kDwordSizeSampler;
	static const uint32_t		kDwordOffsetConstantBuffer = kDwordOffsetVertexBuffer                 + sce::Gnm::kSlotCountVertexBuffer              * sce::Gnm::kDwordSizeVertexBuffer;
	static const uint32_t		kPerStageDwordSize		   = kDwordOffsetConstantBuffer               + sce::Gnm::kSlotCountConstantBuffer            * sce::Gnm::kDwordSizeConstantBuffer;


	const uint64_t kImmediateTypesMask =
		(1ULL << sce::Gnm::kShaderInputUsageImmResource)				|
		(1ULL << sce::Gnm::kShaderInputUsageImmSampler)					|
		(1ULL << sce::Gnm::kShaderInputUsageImmConstBuffer)				|
		(1ULL << sce::Gnm::kShaderInputUsageImmRwResource);

	const uint64_t kPointerTypesMask =
		(1ULL << sce::Gnm::kShaderInputUsagePtrResourceTable)			|
		(1ULL << sce::Gnm::kShaderInputUsagePtrVertexBufferTable)		|
		(1ULL << sce::Gnm::kShaderInputUsagePtrSamplerTable)			|
		(1ULL << sce::Gnm::kShaderInputUsagePtrConstBufferTable)		|
	    (1ULL << sce::Gnm::kShaderInputUsagePtrRwResourceTable);

	const uint64_t kDwordTypesMask =
		(1ULL << sce::Gnm::kShaderInputUsageImmAluBool32Const)			|
		(1ULL << sce::Gnm::kShaderInputUsageImmGdsCounterRange)			|
		(1ULL << sce::Gnm::kShaderInputUsageImmGdsMemoryRange);

	const uint64_t kDelegatedTypesMask =
		(1ULL << sce::Gnm::kShaderInputUsageImmShaderResourceTable)		|
		(1ULL << sce::Gnm::kShaderInputUsageImmLdsEsGsSize)				|
		(1ULL << sce::Gnm::kShaderInputUsagePtrSoBufferTable)			|
		(1ULL << sce::Gnm::kShaderInputUsagePtrInternalGlobalTable)		|
		(1ULL << sce::Gnm::kShaderInputUsagePtrExtendedUserData)		|
		(1ULL << sce::Gnm::kShaderInputUsagePtrDispatchDraw);


	static SCE_GNM_CONSTEXPR const uint8_t inputSizeInDWords[] =
	{
		8, // kShaderInputUsageImmResource
		4, // kShaderInputUsageImmSampler
		4, // kShaderInputUsageImmConstBuffer
		4, // kShaderInputUsageImmVertexBuffer
		8, // kShaderInputUsageImmRwResource
		1, // kShaderInputUsageImmAluFloatConst
		1, // kShaderInputUsageImmAluBool32Const
		1, // kShaderInputUsageImmGdsCounterRange
		1, // kShaderInputUsageImmGdsMemoryRange
		1, // kShaderInputUsageImmGwsBase
		2, // kShaderInputUsageImmShaderResourceTable
		0, //
		0, //
		1, // kShaderInputUsageImmLdsEsGsSize
		0, //
		0, //
		0, //
		0, //
		2, // kShaderInputUsageSubPtrFetchShader
		2, // kShaderInputUsagePtrResourceTable
		2, // kShaderInputUsagePtrInternalResourceTable
		2, // kShaderInputUsagePtrSamplerTable
		2, // kShaderInputUsagePtrConstBufferTable
		2, // kShaderInputUsagePtrVertexBufferTable
		2, // kShaderInputUsagePtrSoBufferTable
		2, // kShaderInputUsagePtrRwResourceTable
		2, // kShaderInputUsagePtrInternalGlobalTable
		2, // kShaderInputUsagePtrExtendedUserData
		2, // kShaderInputUsagePtrIndirectResourceTable
		2, // kShaderInputUsagePtrIndirectInternalResourceTable
		2, // kShaderInputUsagePtrIndirectRwResourceTable
		0, //
		0, //
		0, //
		1, // kShaderInputUsageImmGdsKickRingBufferOffset
		1, // kShaderInputUsageImmVertexRingBufferOffset
		2, // kShaderInputUsagePtrDispatchDraw
		1, // kShaderInputUsageImmDispatchDrawInstances
	};

	static SCE_GNM_CONSTEXPR const uint16_t ringBufferOffsetPerStageInCpRam[] =
	{
		kDwordOffsetResource,
		kDwordOffsetRwResource,
		kDwordOffsetSampler,
		kDwordOffsetVertexBuffer,
		kDwordOffsetConstantBuffer,
	};

	static SCE_GNM_CONSTEXPR const uint16_t slotSizeInDword[] =
	{
		sce::Gnm::kDwordSizeResource,
		sce::Gnm::kDwordSizeRwResource,
		sce::Gnm::kDwordSizeSampler,
		sce::Gnm::kDwordSizeVertexBuffer,
		sce::Gnm::kDwordSizeConstantBuffer,
	};

	static SCE_GNM_CONSTEXPR const struct { uint8_t size; uint8_t dqw; } chunkSizes[kMaxResourceTypes] =
	{
		{ sce::Gnmx::ConstantUpdateEngine::kResourceChunkSize,       kResourceSizeInDqWord },		// 0x00 -- kShaderInputUsageImmResource
		{ sce::Gnmx::ConstantUpdateEngine::kResourceChunkSize,       kSamplerSizeInDqWord },		// 0x01 -- kShaderInputUsageImmSampler
		{ sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize, kConstantBufferSizeInDqWord }, // 0x02 -- kShaderInputUsageImmConstBuffer
		{ sce::Gnmx::ConstantUpdateEngine::kVertexBufferChunkSize,   kVertexBufferSizeInDqWord },   // 0x03 -- invalid (kShaderInputUsageImmVertexBuffer)
		{ sce::Gnmx::ConstantUpdateEngine::kRwResourceChunkSize,     kRwResourceSizeInDqWord },		// 0x04 -- kShaderInputUsageImmRwResource
		{ 0, 0 }																					// the rest
	};

	static const StageQwordPtr eudSetPointers[kMaxResourceTypes] =
	{
		(StageQwordPtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::eudResourceSet,		  // 0x00 -- kShaderInputUsageImmResource
		(StageQwordPtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::eudSamplerSet,		  // 0x01 -- kShaderInputUsageImmSampler
		(StageQwordPtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::eudConstantBufferSet, // 0x02 -- kShaderInputUsageImmConstBuffer
		nullptr,																		  // 0x03 -- kShaderInputUsageImmVertexBuffer
		(StageQwordPtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::eudRwResourceSet,	  // 0x04 -- kShaderInputUsageImmRwResource
		nullptr,																		  // the rest
	};

	static const StageQwordPtr activeBitsPointers[kMaxResourceTypes] =
	{
		(StageQwordPtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::activeResource,		  // 0x00 kShaderInputUsageImmResource
		(StageQwordPtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::activeSampler,		  // 0x01 kShaderInputUsageImmSampler
		(StageQwordPtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::activeConstantBuffer, // 0x02 kShaderInputUsageImmConstBuffer
		nullptr,																		  // 0x03 kShaderInputUsageImmVertexBuffer
		(StageQwordPtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::activeRwResource,	  // 0x04 kShaderInputUsageImmRwResource
		nullptr																			  // the rest
	};
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	static const StageChunkStatePtr chunkStatePointers[kMaxResourceTypes] =
	{
		(StageChunkStatePtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::resourceStage,		  // 0x00 -- kShaderInputUsageImmResource
		(StageChunkStatePtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::samplerStage,		  // 0x01 -- kShaderInputUsageImmSampler
		(StageChunkStatePtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::constantBufferStage, // 0x02 -- kShaderInputUsageImmConstBuffer
		(StageChunkStatePtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::vertexBufferStage,	  // 0x03 -- kShaderInputUsageImmVertexBuffer
		(StageChunkStatePtr)&sce::Gnmx::ConstantUpdateEngine::StageInfo::rwResourceStage,	  // 0x04 -- kShaderInputUsageImmRwResource
		nullptr																				  // rest is invalid
	};
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	static const uint32_t stageUserTableOffsets[kMaxResourceTypes] =
	{
		sce::Gnmx::ConstantUpdateEngine::kUserTableResource,			// 0x00 -- kShaderInputUsageImmResource
		sce::Gnmx::ConstantUpdateEngine::kUserTableSampler,				// 0x01 -- kShaderInputUsageImmSampler
		sce::Gnmx::ConstantUpdateEngine::kUserTableConstantBuffer,		// 0x02 -- kShaderInputUsageImmConstBuffer
		sce::Gnmx::ConstantUpdateEngine::kUserTableVertexBuffer,		// 0x03 -- kShaderInputUsageImmVertexBuffer
		sce::Gnmx::ConstantUpdateEngine::kUserTableRwResource,			// 0x04 -- kShaderInputUsageImmRwResource
		sce::Gnmx::ConstantUpdateEngine::kNumUserTablesPerStage			// rest is invalid
	};
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES

	static const uint16_t dwordValueOffsets[kMaxResourceTypes] =
	{
		0,																			// 0x00
		0,																			// 0x01
		0,																			// 0x02
		0,																			// 0x03
		0,																			// 0x04
		0,																			// 0x05
		(offsetof(sce::Gnmx::ConstantUpdateEngine::StageInfo,boolValue)),			// 0x06 kShaderInputUsageImmAluBool32Const
		(offsetof(sce::Gnmx::ConstantUpdateEngine::StageInfo,appendConsumeDword)),	// 0x07 kShaderInputUsageImmGdsCounterRangev
		(offsetof(sce::Gnmx::ConstantUpdateEngine::StageInfo,gdsMemoryRangeDword)), // 0x08 kShaderInputUsageImmGdsMemoryRange
		0																			// the rest
	};


	static const UpdateDelegate updateDelegates[kMaxResourceTypes][2] =
	{
		// { <update usgpr>, <update usgpr> }
		{ nullptr, nullptr },																									 // 0x00
		{ nullptr, nullptr },																									 // 0x01
		{ nullptr, nullptr },																									 // 0x02
		{ nullptr, nullptr },																									 // 0x03
		{ nullptr, nullptr },																									 // 0x04
		{ nullptr, nullptr },																									 // 0x05
		{ nullptr, nullptr },																									 // 0x06
		{ nullptr, nullptr },																									 // 0x07
		{ nullptr, nullptr },																									 // 0x08
		{ nullptr, nullptr },																									 // 0x09
		{ &sce::Gnmx::ConstantUpdateEngine::updateSrtIUser, nullptr },															 // 0x0a kShaderInputUsageImmShaderResourceTable
		{ nullptr, nullptr },																									 // 0x0b
		{ nullptr, nullptr },																									 // 0x0c
		{ &sce::Gnmx::ConstantUpdateEngine::updateLdsEsGsSizeUser, &sce::Gnmx::ConstantUpdateEngine::updateLdsEsGsSizeEud },	 // 0x0d kShaderInputUsageImmLdsEsGsSize
		{ nullptr, nullptr },																									 // 0x0e
		{ nullptr, nullptr },																									 // 0x0f
		{ nullptr, nullptr },																									 // 0x10
		{ nullptr, nullptr },																									 // 0x11
		{ nullptr, nullptr },																									 // 0x12
		{ nullptr, nullptr },																									 // 0x13
		{ nullptr, nullptr },																									 // 0x14
		{ nullptr, nullptr },																									 // 0x15
		{ nullptr, nullptr },																									 // 0x16
		{ nullptr, nullptr },																									 // 0x17
		{ &sce::Gnmx::ConstantUpdateEngine::updateSoBufferTableUser, &sce::Gnmx::ConstantUpdateEngine::updateSoBufferTableEud }, // 0x18 kShaderInputUsagePtrSoBufferTable
		{ nullptr, nullptr },																									 // 0x19
		{ &sce::Gnmx::ConstantUpdateEngine::updateGlobalTableUser, &sce::Gnmx::ConstantUpdateEngine::updateGlobalTableEud },	 // 0x1a kShaderInputUsagePtrInternalGlobalTable
		{ &sce::Gnmx::ConstantUpdateEngine::updateEudUser, nullptr },															 // 0x1b kShaderInputUsagePtrExtendedUserData
		{ nullptr, nullptr },																									 // 0x1c
		{ nullptr, nullptr },																									 // 0x1d
		{ nullptr, nullptr },																									 // 0x1e
		{ nullptr, nullptr },																									 // 0x1f
		{ nullptr, nullptr },																									 // 0x20
		{ nullptr, nullptr },																									 // 0x21
		{ nullptr, nullptr },																									 // 0x22
		{ nullptr, nullptr },																									 // 0x23
		{ &sce::Gnmx::ConstantUpdateEngine::updateDispatchDrawUser, nullptr },													 // 0x24 kShaderInputUsagePtrDispatchDraw
		{ nullptr, nullptr },																									 // the rest
	};


#ifdef __ORBIS__
	SCE_GNM_STATIC_ASSERT(ringBufferOffsetPerStageInCpRam[sce::Gnmx::ConstantUpdateEngine::kRingBuffersIndexResource]			== kDwordOffsetResource);
	SCE_GNM_STATIC_ASSERT(ringBufferOffsetPerStageInCpRam[sce::Gnmx::ConstantUpdateEngine::kRingBuffersIndexRwResource]			== kDwordOffsetRwResource);
	SCE_GNM_STATIC_ASSERT(ringBufferOffsetPerStageInCpRam[sce::Gnmx::ConstantUpdateEngine::kRingBuffersIndexSampler]			== kDwordOffsetSampler);
	SCE_GNM_STATIC_ASSERT(ringBufferOffsetPerStageInCpRam[sce::Gnmx::ConstantUpdateEngine::kRingBuffersIndexVertexBuffer]		== kDwordOffsetVertexBuffer);
	SCE_GNM_STATIC_ASSERT(ringBufferOffsetPerStageInCpRam[sce::Gnmx::ConstantUpdateEngine::kRingBuffersIndexConstantBuffer]		== kDwordOffsetConstantBuffer);

	SCE_GNM_STATIC_ASSERT(slotSizeInDword[sce::Gnmx::ConstantUpdateEngine::kRingBuffersIndexResource]			== sce::Gnm::kDwordSizeResource);
	SCE_GNM_STATIC_ASSERT(slotSizeInDword[sce::Gnmx::ConstantUpdateEngine::kRingBuffersIndexRwResource]			== sce::Gnm::kDwordSizeRwResource);
	SCE_GNM_STATIC_ASSERT(slotSizeInDword[sce::Gnmx::ConstantUpdateEngine::kRingBuffersIndexSampler]			== sce::Gnm::kDwordSizeSampler);
	SCE_GNM_STATIC_ASSERT(slotSizeInDword[sce::Gnmx::ConstantUpdateEngine::kRingBuffersIndexVertexBuffer]		== sce::Gnm::kDwordSizeVertexBuffer);
	SCE_GNM_STATIC_ASSERT(slotSizeInDword[sce::Gnmx::ConstantUpdateEngine::kRingBuffersIndexConstantBuffer]		== sce::Gnm::kDwordSizeConstantBuffer);
#endif // __ORBIS__

	//--------------------------------------------------------------------------------//



	static inline __int128_t *allocateRegionToCopyToCpRam(sce::Gnmx::GnmxConstantCommandBuffer *ccb,
														  uint32_t stage, uint32_t baseResourceOffset,
														  uint32_t chunk, uint32_t chunkSizeInDW)
	{
		const uint32_t stageOffset      = stage * kPerStageDwordSize;
		const uint32_t baseOffsetInCp   = baseResourceOffset + stageOffset;
		const uint32_t allocationOffset = baseOffsetInCp + chunk * chunkSizeInDW;

		__int128_t * const allocatedRegion = (__int128_t*)ccb->allocateRegionToCopyToCpRam((uint16_t)(allocationOffset*4), chunkSizeInDW);
		SCE_GNM_ASSERT_MSG(allocatedRegion, "Constant Update Engine Error: Couldn't allocate memory from the cpRam");

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		__builtin_memset((uint32_t*)allocatedRegion, 0, chunkSizeInDW*4);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		return allocatedRegion;
	}

	static inline __int128_t *allocateRegionToCopyToCpRamForConstantBuffer(sce::Gnmx::GnmxConstantCommandBuffer *ccb,
																		   uint32_t stage, uint32_t chunk, uint32_t chunkSizeInDW,
																		   uint32_t allocationSizeInDW)
	{
		const uint32_t stageOffset      = stage * kPerStageDwordSize;
		const uint32_t baseOffsetInCp   = ConstantUpdateEngineHelper::kDwordOffsetConstantBuffer+ stageOffset;
		const uint32_t allocationOffset = baseOffsetInCp + chunk * chunkSizeInDW;

		__int128_t * const allocatedRegion = (__int128_t*)ccb->allocateRegionToCopyToCpRam((uint16_t)(allocationOffset*4), allocationSizeInDW);
		SCE_GNM_ASSERT_MSG(allocatedRegion, "Constant Update Engine Error: Couldn't allocate memory from the cpRam");

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		__builtin_memset((uint32_t*)allocatedRegion, 0, allocationSizeInDW*4);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		return allocatedRegion;
	}

	static inline void validateRingSetup(sce::Gnmx::ConstantUpdateEngine::RingSetup *ringSetup)
	{
		SCE_GNM_UNUSED(ringSetup);

		SCE_GNM_ASSERT_MSG(ringSetup->numResourceSlots	 <= sce::Gnm::kSlotCountResource,	  "numResourceSlots must be less or equal than Gnm::kSlotCountResource.");
		SCE_GNM_ASSERT_MSG(ringSetup->numRwResourceSlots	 <= sce::Gnm::kSlotCountRwResource,	  "numRwResourceSlots must be less or equal than Gnm::kSlotCountRwResource.");
		SCE_GNM_ASSERT_MSG(ringSetup->numSampleSlots		 <= sce::Gnm::kSlotCountSampler,	  "numSampleSlots must be less or equal than Gnm::kSlotCountSampler.");
		SCE_GNM_ASSERT_MSG(ringSetup->numVertexBufferSlots <= sce::Gnm::kSlotCountVertexBuffer, "numVertexBufferSlots must be less or equal than Gnm::kSlotCountVertexBuffer.");

		SCE_GNM_ASSERT_MSG((ringSetup->numResourceSlots	  & kResourceChunkSizeMask)		== 0, "numResourceSlots must be a multiple of ConstantUpdateEngine::kResourceChunkSize.");
		SCE_GNM_ASSERT_MSG((ringSetup->numRwResourceSlots	  & kRwResourceChunkSizeMask)	== 0, "numRwResourceSlots must be a multiple of ConstantUpdateEngine::kRwResourceChunkSize.");
		SCE_GNM_ASSERT_MSG((ringSetup->numSampleSlots		  & kSamplerChunkSizeMask)		== 0, "numSampleSlots must must be a multiple of ConstantUpdateEngine::kSamplerChunkSize.");
		SCE_GNM_ASSERT_MSG((ringSetup->numVertexBufferSlots & kVertexBufferChunkSizeMask)	== 0, "numVertexBufferSlots must be a multiple of ConstantUpdateEngine::kVertexBufferChunkSize.");

		ringSetup->numResourceSlots		= (ringSetup->numResourceSlots	   + kResourceChunkSizeMask)	 & ~kResourceChunkSizeMask;
		ringSetup->numRwResourceSlots	= (ringSetup->numRwResourceSlots   + kRwResourceChunkSizeMask)	 & ~kRwResourceChunkSizeMask;
		ringSetup->numSampleSlots		= (ringSetup->numSampleSlots	   + kSamplerChunkSizeMask)		 & ~kSamplerChunkSizeMask;
		ringSetup->numVertexBufferSlots = (ringSetup->numVertexBufferSlots + kVertexBufferChunkSizeMask) & ~kVertexBufferChunkSizeMask;
	}

	static inline void cleanEud(sce::Gnmx::ConstantUpdateEngine *cue, sce::Gnm::ShaderStage stage)
	{
		sce::Gnmx::ConstantUpdateEngine::StageInfo *stageInfo = cue->m_stageInfo+stage;

		// Clean EUD:
		cue->m_shaderDirtyEud &= ~(1 << stage);

		// Clear the EUD resource usage:
		stageInfo->eudResourceSet[0]	= 0;
		stageInfo->eudResourceSet[1]	= 0;
		stageInfo->eudSamplerSet		= 0;
		stageInfo->eudConstantBufferSet = 0;
		stageInfo->eudRwResourceSet 	= 0;

		cue->m_eudReferencesStreamoutBuffers = false;
#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
		cue->m_eudReferencesGlobalTable	&= ~(1<<stage);
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	}

	static inline uint16_t calculateEudSizeInDWord(const sce::Gnm::InputUsageSlot *inputUsageTable, uint32_t inputUsageTableSize)
	{
		if ( inputUsageTableSize == 0)
			return 0;

#if SCE_GNM_CUE2_PARSE_INPUTS_TO_COMPUTE_EUD_SIZE || defined(SCE_GNM_DEBUG)
		uint32_t lastIndex = inputUsageTableSize-1;
		for (uint32_t iInputSlot = 0; iInputSlot < inputUsageTableSize; ++iInputSlot)
		{
			if ( inputUsageTable[iInputSlot].m_startRegister > inputUsageTable[lastIndex].m_startRegister )
				lastIndex = iInputSlot;
		}

#if !SCE_GNM_CUE2_PARSE_INPUTS_TO_COMPUTE_EUD_SIZE && defined(SCE_GNM_DEBUG)
		SCE_GNM_ASSERT_MSG(lastIndex == inputUsageTableSize-1,
							"The input usage slot in the ExtentedUserData buffer are out of order\n"
							"Please update PSSLC or enable the #define: SCE_GNM_CUE2_PARSE_INPUTS_TO_COMPUTE_EUD_SIZE in cue-helper.h");
#endif // !defined(SCE_GNM_CUE2_PARSE_INPUTS_TO_COMPUTE_EUD_SIZE) && defined(SCE_GNM_DEBUG)

		inputUsageTableSize = lastIndex + 1;
#endif // (SCE_GNM_CUE2_PARSE_INPUTS_TO_COMPUTE_EUD_SIZE) || defined(SCE_GNM_DEBUG)
		const sce::Gnm::InputUsageSlot lastInput = inputUsageTable[inputUsageTableSize-1];
		return lastInput.m_startRegister > 15 ? lastInput.m_startRegister + inputSizeInDWords[lastInput.m_usageType] - 16 : 0;
	}

	static inline uint16_t lzcnt128(const uint64_t value[2])
	{
		const uint64_t lo = value[0];
		const uint64_t hi = value[1];

		const uint64_t hicnt = __lzcnt64(hi);
		const uint64_t locnt = __lzcnt64(lo);

		return (uint16_t)(hicnt == 64 ? 64 + locnt : hicnt);
	}

	static inline uint32_t isBitSet(const uint64_t value[2], uint32_t bit)
	{
		return value[bit>>6] & (((uint64_t)1) << (bit & 63)) ? 1 : 0;
	}

	//--------------------------------------------------------------------------------//

	void *getRingAddress(const sce::Gnmx::ConstantUpdateEngine::StageInfo *stageInfo, uint32_t ringIndex);

	static inline uint32_t computeShaderResourceRingSize(uint32_t elemSizeDwords, uint32_t elemCount)
	{
		return (elemSizeDwords)*elemCount*sizeof(uint32_t);
	}

	static inline void* initializeRingBuffer(sce::Gnmx::ConstantUpdateEngine::ShaderResourceRingBuffer *ringBuffer, void *bufferAddr, uint32_t bufferBytes, uint32_t elemSizeDwords, uint32_t elemCount)
	{
		SCE_GNM_ASSERT_MSG(bufferBytes >= computeShaderResourceRingSize(elemSizeDwords, elemCount), "bufferBytes (%u) is too small; use computeSpaceRequirements() to determine the minimum size.", bufferBytes);
		SCE_GNM_ASSERT_MSG(elemCount > 1, "elemCount must be at least 2"); // Need at least two elements
		ringBuffer->headElemIndex	 = elemCount-1;
		ringBuffer->elementsAddr	 = bufferAddr;
		ringBuffer->elemSizeDwords = elemSizeDwords;
		ringBuffer->elemCount		 = elemCount;
		ringBuffer->wrappedIndex	 = 0;
		ringBuffer->halfPointIndex = elemCount/2;

		return (uint8_t*)bufferAddr + bufferBytes;
	}

	static inline void *getRingBuffersNextHead(const sce::Gnmx::ConstantUpdateEngine::ShaderResourceRingBuffer *ringBuffer)
	{
		const uint32_t nextElemIndex = (ringBuffer->headElemIndex+1) % ringBuffer->elemCount;
		return (uint8_t*)ringBuffer->elementsAddr + (nextElemIndex * ringBuffer->elemSizeDwords * sizeof(uint32_t));
	}

	static inline bool advanceRingBuffersHead(sce::Gnmx::ConstantUpdateEngine::ShaderResourceRingBuffer *ringBuffer)
	{
		ringBuffer->headElemIndex = (ringBuffer->headElemIndex+1) % ringBuffer->elemCount;
		return ringBuffer->headElemIndex == ringBuffer->wrappedIndex || ringBuffer->headElemIndex == ringBuffer->halfPointIndex;
	}

	static inline void updateRingBuffersStatePostSubmission(sce::Gnmx::ConstantUpdateEngine::ShaderResourceRingBuffer *ringBuffer)
	{
		ringBuffer->wrappedIndex = ringBuffer->headElemIndex;
		ringBuffer->halfPointIndex = (ringBuffer->headElemIndex + ringBuffer->elemCount/2)%ringBuffer->elemCount;
	}
}


#endif // _SCE_GNMX_CUE_HELPER_H
