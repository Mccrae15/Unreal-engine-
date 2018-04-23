/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx/common.h>

#include <gnmx/cue.h>
#include <gnmx/cue-helper.h>

using namespace sce::Gnm;
using namespace sce::Gnmx;

uint32_t ConstantUpdateEngine::computeHeapSize(uint32_t numRingEntries)
{
	// Note: this code is duplicated in the beginning of ConstantUpdateEngine::init()
	const uint32_t kResourceRingBufferBytes	        = ConstantUpdateEngineHelper::computeShaderResourceRingSize(Gnm::kSlotCountResource        *Gnm::kDwordSizeResource,         numRingEntries);
	const uint32_t kRwResourceRingBufferBytes	    = ConstantUpdateEngineHelper::computeShaderResourceRingSize(Gnm::kSlotCountRwResource      *Gnm::kDwordSizeRwResource,       numRingEntries);
	const uint32_t kSamplerRingBufferBytes		    = ConstantUpdateEngineHelper::computeShaderResourceRingSize(Gnm::kSlotCountSampler         *Gnm::kDwordSizeSampler,          numRingEntries);
	const uint32_t kVertexBufferRingBufferBytes     = ConstantUpdateEngineHelper::computeShaderResourceRingSize(Gnm::kSlotCountVertexBuffer    *Gnm::kDwordSizeVertexBuffer,     numRingEntries);
	const uint32_t kConstantBufferRingBufferBytes   = ConstantUpdateEngineHelper::computeShaderResourceRingSize(Gnm::kSlotCountConstantBuffer  *Gnm::kDwordSizeConstantBuffer,   numRingEntries);
	const uint32_t numStages = Gnm::kShaderStageCount;
	const uint32_t totalRingBufferRequiredSize = numStages * (kResourceRingBufferBytes +
															  kRwResourceRingBufferBytes +
															  kSamplerRingBufferBytes +
															  kVertexBufferRingBufferBytes +
															  kConstantBufferRingBufferBytes);

	return totalRingBufferRequiredSize;
}

uint32_t ConstantUpdateEngine::computeHeapSize(uint32_t numRingEntries, RingSetup ringSetup)
{
	ConstantUpdateEngineHelper::validateRingSetup(&ringSetup);

	// Note: this code is duplicated in the beginning of ConstantUpdateEngine::init()
	const uint32_t kResourceRingBufferBytes	        = ConstantUpdateEngineHelper::computeShaderResourceRingSize(ringSetup.numResourceSlots     * Gnm::kDwordSizeResource,         numRingEntries);
	const uint32_t kRwResourceRingBufferBytes	    = ConstantUpdateEngineHelper::computeShaderResourceRingSize(ringSetup.numRwResourceSlots   * Gnm::kDwordSizeRwResource,       numRingEntries);
	const uint32_t kSamplerRingBufferBytes		    = ConstantUpdateEngineHelper::computeShaderResourceRingSize(ringSetup.numSampleSlots       * Gnm::kDwordSizeSampler,          numRingEntries);
	const uint32_t kVertexBufferRingBufferBytes     = ConstantUpdateEngineHelper::computeShaderResourceRingSize(ringSetup.numVertexBufferSlots * Gnm::kDwordSizeVertexBuffer,     numRingEntries);
	const uint32_t kConstantBufferRingBufferBytes   = ConstantUpdateEngineHelper::computeShaderResourceRingSize(Gnm::kSlotCountConstantBuffer  * Gnm::kDwordSizeConstantBuffer,   numRingEntries);
	const uint32_t numStages = Gnm::kShaderStageCount;
	const uint32_t totalRingBufferRequiredSize = numStages * (kResourceRingBufferBytes +
															  kRwResourceRingBufferBytes +
															  kSamplerRingBufferBytes +
															  kVertexBufferRingBufferBytes +
															  kConstantBufferRingBufferBytes);

	return totalRingBufferRequiredSize;
}


//------------------------------------------------


void *ConstantUpdateEngineHelper::getRingAddress(const sce::Gnmx::ConstantUpdateEngine::StageInfo *stageInfo, uint32_t ringIndex)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stageInfo);
	SCE_GNM_UNUSED(ringIndex);
	return NULL;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	const sce::Gnmx::ConstantUpdateEngine::ShaderResourceRingBuffer *ringBuffer = &stageInfo->ringBuffers[ringIndex];

	const uint32_t dirtyRing = stageInfo->dirtyRing;
	const uint32_t ringBufferIndex = (dirtyRing&(1<<(31-ringIndex))) ?
									 (ringBuffer->headElemIndex+1) % ringBuffer->elemCount :
									 ringBuffer->headElemIndex;

	SCE_GNM_ASSERT_MSG(ringBuffer->elementsAddr, "Invalid pointer.\n");
	void * const ringBufferAddr = (uint8_t*)ringBuffer->elementsAddr + (ringBufferIndex * ringBuffer->elemSizeDwords * sizeof(uint32_t));

	return ringBufferAddr;
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

