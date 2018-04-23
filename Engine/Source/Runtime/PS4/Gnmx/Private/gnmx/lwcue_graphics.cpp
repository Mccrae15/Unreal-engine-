/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if defined(__ORBIS__) // LCUE isn't supported offline

#include <gnmx/lwcue_graphics.h>

using namespace sce;
using namespace Gnmx;

/*
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static bool handleReserveFailed(sce::Gnm::CommandBuffer *cb, uint32_t dwordCount, void *userData)
{
	LightweightGfxContext &gfxc = *(LightweightGfxContext*)userData;
	SCE_GNM_UNUSED(cb);
	SCE_GNM_UNUSED(gfxc);
	SCE_GNM_UNUSED(dwordCount);
	// TODO 
	SCE_GNM_ASSERT(0);
	return false;
}
#pragma clang diagnostic pop
*/

// Graphics User Data or Memory Bindings

static SCE_GNM_FORCE_INLINE void setGfxPipePersistentRegisterRange(GnmxDrawCommandBuffer* dcb, Gnm::ShaderStage shaderStage, uint32_t startSgpr, const uint32_t* values, uint32_t valuesCount)
{
	dcb->setUserDataRegion(shaderStage, startSgpr, values, valuesCount);
}


static SCE_GNM_FORCE_INLINE void setGfxPipePtrInPersistentRegister(GnmxDrawCommandBuffer* dcb, Gnm::ShaderStage shaderStage, uint32_t startSgpr, const void* address)
{
	void* gpuAddress = (void*)address;
	dcb->setPointerInUserData(shaderStage, startSgpr, gpuAddress);
}


static SCE_GNM_FORCE_INLINE void setGfxPipeDataInUserDataSgprOrMemory(GnmxDrawCommandBuffer* dcb, uint32_t* scratchBuffer, Gnm::ShaderStage shaderStage, uint16_t shaderResourceOffset, const void* __restrict data, uint32_t dataSizeInBytes)
{
	SCE_GNM_ASSERT_MSG(dcb != NULL, "dcb is NULL, setDrawCommandBuffer() must be called prior to resource bindings");
	SCE_GNM_ASSERT(data != NULL && dataSizeInBytes > 0 && (dataSizeInBytes%4) == 0);

	uint32_t userDataRegisterOrMemoryOffset = (shaderResourceOffset & LightweightConstantUpdateEngine::kResourceValueMask);
	if ((shaderResourceOffset & LightweightConstantUpdateEngine::kResourceInUserDataSgpr) != 0)
	{
		setGfxPipePersistentRegisterRange(dcb, shaderStage, userDataRegisterOrMemoryOffset, (uint32_t*)data, dataSizeInBytes / sizeof(uint32_t));
	}
	else
	{
		uint32_t* __restrict scratchDestAddress = (uint32_t*)(scratchBuffer + ((int)shaderStage * LightweightConstantUpdateEngine::kGpuStageBufferSizeInDwords) + userDataRegisterOrMemoryOffset);
		__builtin_memcpy(scratchDestAddress, data, dataSizeInBytes);
	}
}


// Dispatch Draw Async Compute User Data Bindings

static SCE_GNM_FORCE_INLINE void setDispatchDrawPersistentRegisterRange(GnmxDispatchCommandBuffer* dcb, uint32_t startSgpr, const uint32_t* values, uint32_t valuesCount)
{
	dcb->setUserDataRegion(startSgpr, values, valuesCount);
}


static SCE_GNM_FORCE_INLINE void setDispatchDrawPtrInPersistentRegister(GnmxDispatchCommandBuffer* dcb, uint32_t startSgpr, const void* address)
{
	void* gpuAddress = (void*)address;
	dcb->setPointerInUserData(startSgpr, gpuAddress);
}


SCE_GNM_LCUE_USED
static SCE_GNM_FORCE_INLINE void setDispatchDrawDataInUserDataSgpr(GnmxDispatchCommandBuffer* dcb, uint16_t shaderResourceOffset, const void* __restrict data, uint32_t dataSizeInBytes)
{
	SCE_GNM_ASSERT_MSG(dcb != NULL, "dispatch draw acb is NULL, setDispatchDrawCommandBuffer() must be called prior to resource bindings");
	SCE_GNM_ASSERT(data != NULL && dataSizeInBytes > 0 && (dataSizeInBytes%4) == 0);

	if ((shaderResourceOffset & LightweightConstantUpdateEngine::kResourceInUserDataSgpr) != 0)
	{
		uint32_t userDataRegisterOffset = (shaderResourceOffset & LightweightConstantUpdateEngine::kResourceValueMask);
		setDispatchDrawPersistentRegisterRange(dcb, userDataRegisterOffset, (uint32_t*)data, dataSizeInBytes / sizeof(uint32_t));
	}
}


#if SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
static bool isResourceBindingComplete(const LightweightConstantUpdateEngine::ShaderResourceBindingValidation* validationTable, const InputResourceOffsets* table)
{
	static const char* shaderStageString[LightweightConstantUpdateEngine::kNumShaderStages] = { "kShaderStageCs", "kShaderStagePs", "kShaderStageVs", "kShaderStageGs", "kShaderStageEs", "kShaderStageHs" "kShaderStageLs", "kShaderStageCsVs" };
	
	bool isValid = true;
	SCE_GNM_ASSERT(table != NULL);
	if (validationTable != NULL)
	{
		int32_t i=0; // Failed resource index
		const char* shaderStage = shaderStageString[table->shaderStage]; SCE_GNM_UNUSED(shaderStage);
		while (isValid && i < table->resourceSlotCount)		{ isValid &= validationTable->resourceOffsetIsBound[i]; ++i; } --i;		SCE_GNM_ASSERT_MSG(isValid, "[%s] Resource at slot %d, was not bound", shaderStage, i); i = 0;		// Resource not bound
		while (isValid && i < table->rwResourceSlotCount)	{ isValid &= validationTable->rwResourceOffsetIsBound[i]; ++i; } --i;	SCE_GNM_ASSERT_MSG(isValid, "[%s] RW resource at slot %d, was not bound", shaderStage, i); i = 0;	// RW-resource not bound
		while (isValid && i < table->samplerSlotCount)		{ isValid &= validationTable->samplerOffsetIsBound[i]; ++i; } --i;		SCE_GNM_ASSERT_MSG(isValid, "[%s] Sampler at slot %d, was not bound", shaderStage, i); i = 0;		// Sampler not bound
		while (isValid && i < table->vertexBufferSlotCount)	{ isValid &= validationTable->vertexBufferOffsetIsBound[i]; ++i; } --i;	SCE_GNM_ASSERT_MSG(isValid, "[%s] Vertex buffer at slot %d, was not bound", shaderStage, i); i = 0;	// Vertex buffer not bound
		while (isValid && i < table->constBufferSlotCount)	{ isValid &= validationTable->constBufferOffsetIsBound[i]; ++i; } --i;	SCE_GNM_ASSERT_MSG(isValid, "[%s] Constant buffer slot %d, was not bound", shaderStage, i); i = 0;	// Constant buffer not bound
//		while (isValid && i<kMaxStreamOutBufferCount)		{ isValid &= validationTable->streamOutOffsetIsBound[i]; ++i; } --i;	SCE_GNM_ASSERT_MSG(isValid, "[%s] Stream-out buffer at slot %d, was not bound", shaderStage, i); i=0;// Stream-out not bound
		isValid &= validationTable->appendConsumeCounterIsBound;																	SCE_GNM_ASSERT_MSG(isValid, "[%s] AppendConsumeCounter not bound", shaderStage);		// AppendConsumeCounter not bound
		isValid &= validationTable->gdsMemoryRangeIsBound;																			SCE_GNM_ASSERT_MSG(isValid, "[%s] gdsMemoryRange not bound", shaderStage);				// gdsMemoryRange not bound
// 		isValid &= validationTable->onChipEsVertsPerSubGroupIsBound;																SCE_GNM_ASSERT_MSG(isValid, "[%s] onChipEsVertsPerSubGroup not bound", shaderStage);	// onChipEsVertsPerSubGroup not bound for ldsEsGsSize 
// 		isValid &= validationTable->onChipEsExportVertexSizeIsBound;																SCE_GNM_ASSERT_MSG(isValid, "[%s] onChipEsExportVertexSize not bound", shaderStage);	// onChipEsExportVertexSize not bound for ldsEsGsSize 

		// Note: if failing on Constant-Buffer slot 15, see updateEmbeddedCb(), it might be related
	}
	return isValid;
}


static void initResourceBindingValidation(LightweightConstantUpdateEngine::ShaderResourceBindingValidation* validationTable, const InputResourceOffsets* table)
{
	SCE_GNM_ASSERT(validationTable != NULL);
	if (table != NULL)
	{
		// Mark all expected resources slots (!= 0xFFFF) as not bound (false)
		for (int32_t i=0; i<table->resourceSlotCount; ++i)		validationTable->resourceOffsetIsBound[i]		= (table->resourceDwOffset[i] == 0xFFFF);
		for (int32_t i=0; i<table->rwResourceSlotCount; ++i)	validationTable->rwResourceOffsetIsBound[i]		= (table->rwResourceDwOffset[i]	== 0xFFFF);
		for (int32_t i=0; i<table->samplerSlotCount; ++i)		validationTable->samplerOffsetIsBound[i]		= (table->samplerDwOffset[i] == 0xFFFF);
		for (int32_t i=0; i<table->vertexBufferSlotCount; ++i)	validationTable->vertexBufferOffsetIsBound[i]	= (table->vertexBufferDwOffset[i] == 0xFFFF);
		for (int32_t i=0; i<table->constBufferSlotCount; ++i)	validationTable->constBufferOffsetIsBound[i]	= (table->constBufferDwOffset[i] == 0xFFFF);
//		for (int32_t i=0; i<LightweightConstantUpdateEngine::kMaxStreamOutBufferCount; ++i)	validationTable->streamOutOffsetIsBound[i]	= (table->streamOutDwOffset[i]	== 0xFFFF);
		validationTable->appendConsumeCounterIsBound = (table->appendConsumeCounterSgpr == 0xFF);
		validationTable->gdsMemoryRangeIsBound = (table->gdsMemoryRangeSgpr == 0xFF);
// 		validationTable->onChipEsVertsPerSubGroupIsBound = (table->ldsEsGsSizeSgpr == 0xFF);
// 		validationTable->onChipEsExportVertexSizeIsBound = (table->ldsEsGsSizeSgpr == 0xFF);
	}
	else
	{
		// If there's no table, all resources are valid
		SCE_GNM_ASSERT( sizeof(bool) == sizeof(unsigned char) );
		__builtin_memset(validationTable, true, sizeof(LightweightConstantUpdateEngine::ShaderResourceBindingValidation));
	}
}
#endif	// SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED

void LightweightGraphicsConstantUpdateEngine::init(uint32_t** resourceBuffersInGarlic, int32_t resourceBufferCount, uint32_t resourceBufferSizeInDwords, void* globalInternalResourceTableAddr)
{
	BaseConstantUpdateEngine::init(resourceBuffersInGarlic, resourceBufferCount, resourceBufferSizeInDwords, globalInternalResourceTableAddr);
	__builtin_memset(m_scratchBuffer, 0, sizeof(uint32_t) * LightweightConstantUpdateEngine::kGraphicsScratchBufferSizeInDwords);

	invalidateAllBindings();

	m_drawCommandBuffer = NULL;
	m_activeShaderStages = Gnm::kActiveShaderStagesVsPs;
	m_dispatchDrawActive = false;

	// Fixed shader resource input table for the VS GPU stage when the Geometry pipeline is enabled (the copy shader)
	__builtin_memset(&m_fixedGsVsShaderResourceOffsets, 0xFF, sizeof(InputResourceOffsets));
	m_fixedGsVsShaderResourceOffsets.initSupportedResourceCounts();
	m_fixedGsVsShaderResourceOffsets.isSrtShader = false;
	m_fixedGsVsShaderResourceOffsets.requiredBufferSizeInDwords = 0;
	m_fixedGsVsShaderResourceOffsets.shaderStage = Gnm::kShaderStageVs;
	m_fixedGsVsShaderResourceOffsets.globalInternalPtrSgpr = LightweightConstantUpdateEngine::kDefaultGlobalInternalTablePtrSgpr;
	
	__builtin_memset(&m_fixedGsVsStreamOutShaderResourceOffsets, 0xFF, sizeof(InputResourceOffsets));
	m_fixedGsVsStreamOutShaderResourceOffsets.initSupportedResourceCounts();
	m_fixedGsVsStreamOutShaderResourceOffsets.isSrtShader = false;
	m_fixedGsVsStreamOutShaderResourceOffsets.requiredBufferSizeInDwords = LightweightConstantUpdateEngine::kMaxStreamOutBufferCount * sizeof(Gnm::Buffer); // 4 stream-out buffers
	m_fixedGsVsStreamOutShaderResourceOffsets.shaderStage = Gnm::kShaderStageVs;
	m_fixedGsVsStreamOutShaderResourceOffsets.globalInternalPtrSgpr = LightweightConstantUpdateEngine::kDefaultGlobalInternalTablePtrSgpr;
	m_fixedGsVsStreamOutShaderResourceOffsets.streamOutPtrSgpr = LightweightConstantUpdateEngine::kDefaultStreamOutTablePtrSgpr;
	m_fixedGsVsStreamOutShaderResourceOffsets.streamOutArrayDwOffset = 0;
	for (uint16_t i=0; i<LightweightConstantUpdateEngine::kMaxStreamOutBufferCount; i++)
		m_fixedGsVsStreamOutShaderResourceOffsets.streamOutDwOffset[i] = i * sizeof(Gnm::Buffer) / sizeof(uint32_t);

	// No global table is used with on chip, replaced by ldsEsGsSizeSgpr
	__builtin_memset(&m_fixedOnChipGsVsShaderResourceOffsets, 0xFF, sizeof(InputResourceOffsets));
	m_fixedOnChipGsVsShaderResourceOffsets.initSupportedResourceCounts();
	m_fixedOnChipGsVsShaderResourceOffsets.isSrtShader = false;
	m_fixedOnChipGsVsShaderResourceOffsets.requiredBufferSizeInDwords = 0;
	m_fixedOnChipGsVsShaderResourceOffsets.shaderStage = Gnm::kShaderStageVs;
	m_fixedOnChipGsVsShaderResourceOffsets.ldsEsGsSizeSgpr = LightweightConstantUpdateEngine::kDefaultVertexldsEsGsSizeSgpr;

	__builtin_memset(&m_fixedOnChipGsVsStreamOutShaderResourceOffsets, 0xFF, sizeof(InputResourceOffsets));
	m_fixedOnChipGsVsStreamOutShaderResourceOffsets.initSupportedResourceCounts();
	m_fixedOnChipGsVsStreamOutShaderResourceOffsets.isSrtShader = false;
	m_fixedOnChipGsVsStreamOutShaderResourceOffsets.requiredBufferSizeInDwords = LightweightConstantUpdateEngine::kMaxStreamOutBufferCount * sizeof(Gnm::Buffer); // 4 stream-out buffers
	m_fixedOnChipGsVsStreamOutShaderResourceOffsets.shaderStage = Gnm::kShaderStageVs;
	m_fixedOnChipGsVsStreamOutShaderResourceOffsets.ldsEsGsSizeSgpr = LightweightConstantUpdateEngine::kDefaultVertexldsEsGsSizeSgpr;
	m_fixedOnChipGsVsStreamOutShaderResourceOffsets.streamOutPtrSgpr = LightweightConstantUpdateEngine::kDefaultStreamOutTablePtrSgpr;
	m_fixedOnChipGsVsStreamOutShaderResourceOffsets.streamOutArrayDwOffset = 0;
	for (uint16_t i=0; i<LightweightConstantUpdateEngine::kMaxStreamOutBufferCount; i++)
		m_fixedOnChipGsVsStreamOutShaderResourceOffsets.streamOutDwOffset[i] = i * sizeof(Gnm::Buffer) / sizeof(uint32_t);

	m_gsMode = Gnm::kGsModeDisable;
	m_gsMaxOutput = Gnm::kGsMaxOutputPrimitiveDwordSize1024;

	m_tessellationAutoManageReservedCb = true;
	m_tessellationCurrentTgPatchCount = 0;
	m_tessellationCurrentRegs.m_vgtLsHsConfig = 0;
	m_tessellationCurrentOffChipFactorThreshold = 512.0f;

	m_onChipEsVertsPerSubGroup = 0;
	m_onChipEsExportVertexSizeInDword = 0;
	m_onChipLdsSizeIn512Bytes = 0;
	m_onChipGsSignature = LightweightConstantUpdateEngine::kOnChipGsInvalidSignature;
}


void LightweightGraphicsConstantUpdateEngine::swapBuffers()
{
	BaseConstantUpdateEngine::swapBuffers();
	invalidateAllBindings();

#if SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
	// Because swapBuffers() will reuse memory, it becomes necessary to invalidate the KCache/L1/L2, this ensures no stale data will be fetched by the GPU
	// Note this will be the very first command in the dcb, even before WaitUntilSafeForRender()
	if (m_drawCommandBuffer->m_endptr > m_drawCommandBuffer->m_beginptr)
		m_drawCommandBuffer->flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionInvalidateKCache, Gnm::kStallCommandBufferParserDisable);
#endif // SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
}


void LightweightGraphicsConstantUpdateEngine::invalidateAllBindings()
{
	m_shaderBindingIsValid = 0;
	m_updateShaderBindingIsValid = 0;
	__builtin_memset(m_dirtyShaderResources, 0, sizeof(bool) * LightweightConstantUpdateEngine::kNumShaderStages);
	__builtin_memset(m_dirtyShader, 0, sizeof(bool) * LightweightConstantUpdateEngine::kNumShaderStages);
	__builtin_memset(m_dirtyShaderUpdateOnly, 0, sizeof(bool) * LightweightConstantUpdateEngine::kNumShaderStages);
	__builtin_memset(m_boundShaderResourceOffsets, 0, sizeof(void*) * LightweightConstantUpdateEngine::kNumShaderStages);
	__builtin_memset(m_boundShader, 0, sizeof(void*) * LightweightConstantUpdateEngine::kNumShaderStages);
	m_dirtyShader[Gnm::kShaderStagePs] = true; // First Pixel Shader MUST be marked as dirty (handles issue when first PS is NULL)
	m_psInputsTable = NULL;

	m_asyncCsShaderOrderedAppendMode		= m_gfxCsShaderOrderedAppendMode = 0;
	m_dispatchDrawIndexDeallocNumBits		= 0;
	m_tessellationCurrentDistributionMode	= Gnm::kTessellationDistributionNone;

#if SCE_GNM_LCUE_ENABLE_CACHED_PS_SEMANTIC_TABLE
	__builtin_memset(m_cachedPsInputTable, 0, sizeof(uint32_t) * LightweightConstantUpdateEngine::kMaxPsInputUsageCount);
#endif // SCE_GNM_LCUE_ENABLE_CACHED_PS_SEMANTIC_TABLE

#if SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
	__builtin_memset(m_boundShaderResourcesValidation, true, sizeof(LightweightConstantUpdateEngine::ShaderResourceBindingValidation) * LightweightConstantUpdateEngine::kNumShaderStages);
#endif // SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
}


void LightweightGraphicsConstantUpdateEngine::invalidateShaderStage(sce::Gnm::ShaderStage shaderStage)
{
	m_shaderBindingIsValid &= ~(1 << shaderStage);
	m_updateShaderBindingIsValid &= ~(1 << shaderStage);
	m_dirtyShader[shaderStage] = false;
	m_dirtyShaderResources[shaderStage] = false;
	m_dirtyShaderUpdateOnly[shaderStage] = false;

	m_boundShader[shaderStage] = NULL;
	m_boundShaderResourceOffsets[shaderStage] = NULL;

	switch (shaderStage)
	{
		case sce::Gnm::kShaderStageCs:
			m_gfxCsShaderOrderedAppendMode = 0;
			break;
		case LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute:
			m_asyncCsShaderOrderedAppendMode = 0, m_dispatchDrawIndexDeallocNumBits = 0;
			break;
		case sce::Gnm::kShaderStagePs:
#if SCE_GNM_LCUE_ENABLE_CACHED_PS_SEMANTIC_TABLE
			__builtin_memset(m_cachedPsInputTable, 0, sizeof(uint32_t) * LightweightConstantUpdateEngine::kMaxPsInputUsageCount);
#endif // SCE_GNM_LCUE_ENABLE_CACHED_PS_SEMANTIC_TABLE
			break;
		case sce::Gnm::kShaderStageHs:
			m_tessellationCurrentDistributionMode = Gnm::kTessellationDistributionNone;
			break;
		default:
			break;
	}

#if SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
	__builtin_memset(&m_boundShaderResourcesValidation[shaderStage], true, sizeof(LightweightConstantUpdateEngine::ShaderResourceBindingValidation));
#endif // SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
};


SCE_GNM_FORCE_INLINE void LightweightGraphicsConstantUpdateEngine::setOrUseCachedPsInputUsageTable(const uint32_t *psInputTable, uint32_t numInputSemantics)
{
	SCE_GNM_ASSERT_MSG(psInputTable != NULL, "psInput must not be NULL");
	SCE_GNM_ASSERT_MSG(numInputSemantics > 0 && numInputSemantics <= LightweightConstantUpdateEngine::kMaxPsInputUsageCount, "numInputSemantics (%u) must be in the range [1..32].", numInputSemantics);

#if SCE_GNM_LCUE_ENABLE_CACHED_PS_SEMANTIC_TABLE

	// if already set from previous draw, we can safely skip binding the pixel shader usage table and avoid a context roll
	bool bIsSame = __builtin_memcmp(m_cachedPsInputTable, psInputTable, sizeof(uint32_t) * numInputSemantics) == 0;
	if (!bIsSame)
	{
		m_drawCommandBuffer->setPsShaderUsage(psInputTable, numInputSemantics);

		// cache updated table, unrolled loop to avoid overhead of memcpy
		#define UNROLL_STEP(n) case n: m_cachedPsInputTable[n - 1] = psInputTable[n - 1]
		switch (numInputSemantics)
		{
			UNROLL_STEP(32);
			UNROLL_STEP(31);
			UNROLL_STEP(30);
			UNROLL_STEP(29);
			UNROLL_STEP(28);
			UNROLL_STEP(27);
			UNROLL_STEP(26);
			UNROLL_STEP(25);
			UNROLL_STEP(24);
			UNROLL_STEP(23);
			UNROLL_STEP(22);
			UNROLL_STEP(21);
			UNROLL_STEP(20);
			UNROLL_STEP(19);
			UNROLL_STEP(18);
			UNROLL_STEP(17);
			UNROLL_STEP(16);
			UNROLL_STEP(15);
			UNROLL_STEP(14);
			UNROLL_STEP(13);
			UNROLL_STEP(12);
			UNROLL_STEP(11);
			UNROLL_STEP(10);
			UNROLL_STEP(9);
			UNROLL_STEP(8);
			UNROLL_STEP(7);
			UNROLL_STEP(6);
			UNROLL_STEP(5);
			UNROLL_STEP(4);
			UNROLL_STEP(3);
			UNROLL_STEP(2);
			UNROLL_STEP(1);
				break;
			default: // 0
				// if zero inputs, were done
				break;
		}
		#undef UNROLL_STEP
	}

#else // !SCE_GNM_LCUE_ENABLE_CACHED_PS_SEMANTIC_TABLE
	m_drawCommandBuffer->setPsShaderUsage(psInputTable, numInputSemantics);
#endif // SCE_GNM_LCUE_ENABLE_CACHED_PS_SEMANTIC_TABLE
}


SCE_GNM_FORCE_INLINE uint32_t* LightweightGraphicsConstantUpdateEngine::flushScratchBuffer(uint8_t shaderStage)
{
	SCE_GNM_ASSERT(m_boundShader[shaderStage] != NULL);

	// Copy scratch data over the main buffer
	const InputResourceOffsets* table = m_boundShaderResourceOffsets[shaderStage];
	uint32_t* __restrict destAddr = reserveResourceBufferSpace(table->requiredBufferSizeInDwords);
	uint32_t* __restrict sourceAddr = m_scratchBuffer + (shaderStage * LightweightConstantUpdateEngine::kGpuStageBufferSizeInDwords);
	__builtin_memcpy(destAddr, sourceAddr, table->requiredBufferSizeInDwords * sizeof(uint32_t));

	return destAddr;
}


SCE_GNM_FORCE_INLINE void LightweightGraphicsConstantUpdateEngine::updateOnChipParametersInUserDataSgprs(sce::Gnm::ShaderStage shaderStage)
{
	SCE_GNM_ASSERT(shaderStage == Gnm::kShaderStageGs || shaderStage == Gnm::kShaderStageVs);
	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];

	if (table->ldsEsGsSizeSgpr != 0xFF)
	{
		uint32_t ldsSizeInDwords = m_onChipEsVertsPerSubGroup * m_onChipEsExportVertexSizeInDword;
		setGfxPipePersistentRegisterRange(m_drawCommandBuffer, shaderStage, table->ldsEsGsSizeSgpr, &ldsSizeInDwords, 1);
	}
}


SCE_GNM_FORCE_INLINE void LightweightGraphicsConstantUpdateEngine::updateLsEsVsPtrsInUserDataSgprs(Gnm::ShaderStage shaderStage, const uint32_t* resourceBufferFlushedAddress)
{
	SCE_GNM_ASSERT(m_boundShader[(int32_t)shaderStage] != NULL);
	SCE_GNM_ASSERT(shaderStage == Gnm::kShaderStageLs || shaderStage == Gnm::kShaderStageEs || shaderStage == Gnm::kShaderStageVs);
	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];

	if (table->fetchShaderPtrSgpr != 0xFF)
	{
		SCE_GNM_ASSERT(m_boundFetchShader[(int32_t)shaderStage] != NULL);
		setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, shaderStage, table->fetchShaderPtrSgpr, m_boundFetchShader[(int32_t)shaderStage]);
	}
	if (table->vertexBufferPtrSgpr != 0xFF)
	{
		setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, shaderStage, table->vertexBufferPtrSgpr, resourceBufferFlushedAddress + table->vertexBufferArrayDwOffset);
	}
	if (table->streamOutPtrSgpr != 0xFF)
	{
		setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, shaderStage, table->streamOutPtrSgpr, resourceBufferFlushedAddress + table->streamOutArrayDwOffset);
	}
}


SCE_GNM_FORCE_INLINE void LightweightGraphicsConstantUpdateEngine::updateCommonPtrsInUserDataSgprs(Gnm::ShaderStage shaderStage, const uint32_t* resourceBufferFlushedAddress)
{
	SCE_GNM_ASSERT(m_boundShader[(int32_t)shaderStage] != NULL);
	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];
	
	if (table->userExtendedData1PtrSgpr != 0xFF)
	{
		setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, shaderStage, table->userExtendedData1PtrSgpr, resourceBufferFlushedAddress);
	}
	if (table->constBufferPtrSgpr != 0xFF)
	{
		setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, shaderStage, table->constBufferPtrSgpr, resourceBufferFlushedAddress + table->constBufferArrayDwOffset);
	}
	if (table->resourcePtrSgpr != 0xFF)
	{
		setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, shaderStage, table->resourcePtrSgpr, resourceBufferFlushedAddress + table->resourceArrayDwOffset);
	}
	if (table->rwResourcePtrSgpr != 0xFF)
	{
		setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, shaderStage, table->rwResourcePtrSgpr, resourceBufferFlushedAddress + table->rwResourceArrayDwOffset);
	}
	if (table->samplerPtrSgpr != 0xFF)
	{
		setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, shaderStage, table->samplerPtrSgpr, resourceBufferFlushedAddress + table->samplerArrayDwOffset);
	}
	if (table->globalInternalPtrSgpr != 0xFF)
	{
		SCE_GNM_ASSERT(m_globalInternalResourceTableAddr != NULL);
		setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, shaderStage, table->globalInternalPtrSgpr, m_globalInternalResourceTableAddr);
	}
	if (table->appendConsumeCounterSgpr != 0xFF)
	{
		setGfxPipePersistentRegisterRange(m_drawCommandBuffer, shaderStage, table->appendConsumeCounterSgpr, &m_boundShaderAppendConsumeCounterRange[shaderStage], 1);
	}
	if (table->gdsMemoryRangeSgpr != 0xFF)
	{
		setGfxPipePersistentRegisterRange(m_drawCommandBuffer, shaderStage, table->gdsMemoryRangeSgpr, &m_boundShaderGdsMemoryRange[shaderStage], 1);
	}
}


SCE_GNM_FORCE_INLINE void LightweightGraphicsConstantUpdateEngine::updateVsDispatchDrawPtrsInUserDataSgprs(const uint32_t* resourceBufferFlushedAddress)
{
	SCE_GNM_UNUSED(resourceBufferFlushedAddress);
	SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageVs] != NULL);
	const InputResourceOffsets* table = m_boundShaderResourceOffsets[Gnm::kShaderStageVs];

	if (table->dispatchDrawPtrSgpr != 0xFF)
	{
		SCE_GNM_ASSERT_MSG(m_pDispatchDrawData, "DispatchDrawData pointer not specified. Call setDispatchDrawData() first!");
		setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, Gnm::kShaderStageVs, table->dispatchDrawPtrSgpr, m_pDispatchDrawData);
	}
}
	

SCE_GNM_FORCE_INLINE void LightweightGraphicsConstantUpdateEngine::updateCsDispatchDrawPtrsInUserDataSgprs(const uint32_t* resourceBufferFlushedAddress)
{

	SCE_GNM_ASSERT(m_boundShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
	const InputResourceOffsets* table = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];

	// update common resources
	if (table->userExtendedData1PtrSgpr != 0xFF)
	{
		setDispatchDrawPtrInPersistentRegister(m_asyncDispatchCommandBuffer, table->userExtendedData1PtrSgpr, resourceBufferFlushedAddress);
	}
	if (table->constBufferPtrSgpr != 0xFF)
	{
		setDispatchDrawPtrInPersistentRegister(m_asyncDispatchCommandBuffer, table->constBufferPtrSgpr, resourceBufferFlushedAddress + table->constBufferArrayDwOffset);
	}
	if (table->resourcePtrSgpr != 0xFF)
	{
		setDispatchDrawPtrInPersistentRegister(m_asyncDispatchCommandBuffer, table->resourcePtrSgpr, resourceBufferFlushedAddress + table->resourceArrayDwOffset);
	}
	if (table->rwResourcePtrSgpr != 0xFF)
	{
		setDispatchDrawPtrInPersistentRegister(m_asyncDispatchCommandBuffer, table->rwResourcePtrSgpr, resourceBufferFlushedAddress + table->rwResourceArrayDwOffset);
	}
	if (table->samplerPtrSgpr != 0xFF)
	{
		setDispatchDrawPtrInPersistentRegister(m_asyncDispatchCommandBuffer, table->samplerPtrSgpr, resourceBufferFlushedAddress + table->samplerArrayDwOffset);
	}
	if (table->globalInternalPtrSgpr != 0xFF)
	{
		SCE_GNM_ASSERT(m_globalInternalResourceTableAddr != NULL);
		setDispatchDrawPtrInPersistentRegister(m_asyncDispatchCommandBuffer, table->globalInternalPtrSgpr, m_globalInternalResourceTableAddr);
	}
	if (table->appendConsumeCounterSgpr != 0xFF)
	{
		setDispatchDrawPersistentRegisterRange(m_asyncDispatchCommandBuffer, table->appendConsumeCounterSgpr, &m_boundShaderAppendConsumeCounterRange[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute], 1);
	}
	if (table->gdsMemoryRangeSgpr != 0xFF)
	{
		setDispatchDrawPersistentRegisterRange(m_asyncDispatchCommandBuffer, table->gdsMemoryRangeSgpr, &m_boundShaderGdsMemoryRange[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute], 1);
	}

	if (table->fetchShaderPtrSgpr != 0xFF)
	{
		SCE_GNM_ASSERT(m_boundFetchShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
		setDispatchDrawPtrInPersistentRegister(m_asyncDispatchCommandBuffer, table->fetchShaderPtrSgpr, m_boundFetchShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute]);
	}
	if (table->vertexBufferPtrSgpr != 0xFF)
	{
		setDispatchDrawPtrInPersistentRegister(m_asyncDispatchCommandBuffer, table->vertexBufferPtrSgpr, resourceBufferFlushedAddress + table->vertexBufferArrayDwOffset);
	}
	if (table->streamOutPtrSgpr != 0xFF)
	{
		setDispatchDrawPtrInPersistentRegister(m_asyncDispatchCommandBuffer, table->streamOutPtrSgpr, resourceBufferFlushedAddress + table->streamOutArrayDwOffset);
	}

	if (table->dispatchDrawPtrSgpr != 0xFF)
	{
		SCE_GNM_ASSERT_MSG(m_pDispatchDrawData, "DispatchDrawData pointer not specified. Call setDispatchDrawData() first!");
		setDispatchDrawPtrInPersistentRegister(m_asyncDispatchCommandBuffer, table->dispatchDrawPtrSgpr, m_pDispatchDrawData);
	}
}


/** @brief The Embedded Constant Buffer (ECB) descriptor is automatically generated and bound for shaders that have dependencies
 *	An ECB is generated when global static arrays are accessed through dynamic variables, preventing the compiler from embedding its immediate values in the shader code.
 *  The ECB is stored after the end of the shader code area, and is expected on API-slot 15 (kShaderSlotEmbeddedConstantBuffer).
 */
SCE_GNM_FORCE_INLINE void LightweightGraphicsConstantUpdateEngine::updateEmbeddedCb(sce::Gnm::ShaderStage shaderStage, const Gnmx::ShaderCommonData* shaderCommon)
{
#if SCE_GNMX_TRACK_EMBEDDED_CB
	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];
	if (shaderCommon != NULL && table->constBufferDwOffset[LightweightConstantUpdateEngine::kConstantBufferInternalApiSlotForEmbeddedData] != 0xFFFF)
	{
		SCE_GNM_ASSERT(shaderCommon->m_embeddedConstantBufferSizeInDQW > 0);

		const uint32_t* shaderRegisters = (const uint32_t*)(shaderCommon + 1);
		const uint8_t* shaderCode = (uint8_t*)(((uintptr_t)shaderRegisters[0] << 8ULL) | ((uintptr_t)shaderRegisters[1] << 40ULL));

		Gnm::Buffer embeddedCb;
		embeddedCb.initAsConstantBuffer((void*)(shaderCode + shaderCommon->m_shaderSize), shaderCommon->m_embeddedConstantBufferSizeInDQW << 4);
		setConstantBuffers(shaderStage, LightweightConstantUpdateEngine::kConstantBufferInternalApiSlotForEmbeddedData, 1, &embeddedCb);
	}
#else // SCE_GNMX_TRACK_EBEDDED_CB
	SCE_GNM_UNUSED(shaderStage);
	SCE_GNM_UNUSED(shaderCommon);
#endif //SCE_GNMX_TRACK_EMBEDDED_CB
}


void LightweightGraphicsConstantUpdateEngine::preDispatch()
{
	SCE_GNM_ASSERT(m_drawCommandBuffer != NULL);

	const Gnmx::CsShader* csShader = ((const Gnmx::CsShader*)m_boundShader[Gnm::kShaderStageCs]);
	if (m_dirtyShader[Gnm::kShaderStageCs])
	{
		m_drawCommandBuffer->setCsShader( &csShader->m_csStageRegisters );
		if (m_prefetchShaderCode)
		{
			SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)csShader->m_csStageRegisters.m_computePgmLo << 8), csShader->m_common.m_shaderSize);
			m_drawCommandBuffer->prefetchIntoL2((void*)((uintptr_t)csShader->m_csStageRegisters.m_computePgmLo << 8), csShader->m_common.m_shaderSize);
		}
	}

	// Handle embedded Constant Buffer on CB slot 15 (kShaderSlotEmbeddedConstantBuffer)
	if (m_dirtyShader[Gnm::kShaderStageCs] || m_dirtyShaderResources[Gnm::kShaderStageCs])
		updateEmbeddedCb(Gnm::kShaderStageCs, (const Gnmx::ShaderCommonData*)csShader);

	if (m_dirtyShaderResources[Gnm::kShaderStageCs])
	{
		SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageCs], m_boundShaderResourceOffsets[Gnm::kShaderStageCs]);

		const uint32_t* flushAddress = flushScratchBuffer(Gnm::kShaderStageCs);
		updateCommonPtrsInUserDataSgprs(Gnm::kShaderStageCs, flushAddress);
	}

	m_dirtyShaderResources[Gnm::kShaderStageCs] = false;
	m_dirtyShader[Gnm::kShaderStageCs] = false;
}

void LightweightGraphicsConstantUpdateEngine::preDispatchDraw(Gnm::DispatchOrderedAppendMode *pOutOrderedAppendMode, uint32_t *pOutDispatchDrawIndexDeallocationMask, 
															  uint32_t *pOutSgprKrbLoc, uint32_t *pOutSgprInstanceCs,
															  Gnm::DispatchDrawMode *pOutDispatchDrawMode, uint32_t *pOutSgprVrbLoc, uint32_t *pOutSgprInstanceVs)
{
#if	!SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	SCE_GNM_ASSERT_MSG(0, "Dispatch draw has been disabled, please recompile Gnmx with SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW (can be found in Gnmx/config.h)");
#endif // !SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW

	SCE_GNM_ASSERT(m_drawCommandBuffer != NULL && m_asyncDispatchCommandBuffer != NULL);
	SCE_GNM_ASSERT(m_boundShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
	SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageVs] != NULL);
	SCE_GNM_ASSERT_MSG(m_dispatchDrawActive == true, "a dispatch draw vertex shader must be set before preDispatchDraw()");
	SCE_GNM_ASSERT(m_activeShaderStages == Gnm::kActiveShaderStagesDispatchDrawVsPs);

	const Gnmx::CsShader* csShader = ((const Gnmx::CsShader*)m_boundShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute]);
	if (m_dirtyShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute])
	{
		m_asyncDispatchCommandBuffer->setCsShader( &csShader->m_csStageRegisters );
		if (m_prefetchShaderCode)
		{
			SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)csShader->m_csStageRegisters.m_computePgmLo << 8), csShader->m_common.m_shaderSize);
			m_asyncDispatchCommandBuffer->prefetchIntoL2((void*)((uintptr_t)csShader->m_csStageRegisters.m_computePgmLo << 8), csShader->m_common.m_shaderSize);
		}
	}

	const Gnmx::VsShader* vsShader = ((const Gnmx::VsShader*)m_boundShader[Gnm::kShaderStageVs]);
	if (m_dirtyShader[Gnm::kShaderStageVs])
	{
#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_dirtyShaderUpdateOnly[Gnm::kShaderStageVs])
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStageVs);
			m_drawCommandBuffer->updateVsShader(&vsShader->m_vsStageRegisters, m_boundShaderModifier[Gnm::kShaderStageVs]);
		}
		else
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStageVs);	// shader has been bound, it's okay to use update*Shader() method
			m_drawCommandBuffer->setVsShader(&vsShader->m_vsStageRegisters, m_boundShaderModifier[Gnm::kShaderStageVs]);
		}
#else // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		m_drawCommandBuffer->setVsShader(&vsShader->m_vsStageRegisters, m_boundShaderModifier[Gnm::kShaderStageVs]);
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_prefetchShaderCode)
		{
			SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)vsShader->m_vsStageRegisters.m_spiShaderPgmLoVs << 8), vsShader->m_common.m_shaderSize);
			m_drawCommandBuffer->prefetchIntoL2((void*)((uintptr_t)vsShader->m_vsStageRegisters.m_spiShaderPgmLoVs << 8), vsShader->m_common.m_shaderSize);
		}
	}

	const Gnmx::PsShader* psShader = ((const Gnmx::PsShader*)m_boundShader[Gnm::kShaderStagePs]);
	if (m_dirtyShader[Gnm::kShaderStagePs])
	{
#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_dirtyShaderUpdateOnly[Gnm::kShaderStagePs])
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStagePs);
			m_drawCommandBuffer->updatePsShader((psShader != NULL) ? &psShader->m_psStageRegisters : NULL);
		}
		else
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStagePs);	// shader has been bound, it's okay to use update*Shader() method
			m_drawCommandBuffer->setPsShader((psShader != NULL) ? &psShader->m_psStageRegisters : NULL);
		}
#else // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		m_drawCommandBuffer->setPsShader((psShader != NULL) ? &psShader->m_psStageRegisters : NULL);
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_prefetchShaderCode && psShader != NULL)
		{
			SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)psShader->m_psStageRegisters.m_spiShaderPgmLoPs << 8), psShader->m_common.m_shaderSize);
			m_drawCommandBuffer->prefetchIntoL2((void*)((uintptr_t)psShader->m_psStageRegisters.m_spiShaderPgmLoPs << 8), psShader->m_common.m_shaderSize);
		}
	}

	// Handle embedded Constant Buffer on CB slot 15 (kShaderSlotEmbeddedConstantBuffer)
	if (m_dirtyShader[Gnm::kShaderStageVs] || m_dirtyShaderResources[Gnm::kShaderStageVs])
		updateEmbeddedCb(Gnm::kShaderStageVs, (const Gnmx::ShaderCommonData*)vsShader);
	if (m_dirtyShader[Gnm::kShaderStagePs] || m_dirtyShaderResources[Gnm::kShaderStagePs])
		updateEmbeddedCb(Gnm::kShaderStagePs, (const Gnmx::ShaderCommonData*)psShader);

	// Bind VS Stage resources for both CS and VS
	*pOutSgprVrbLoc = (uint32_t)-1;
	*pOutDispatchDrawMode = Gnm::kDispatchDrawModeIndexRingBufferOnly;
	if (pOutSgprInstanceVs != NULL)
		*pOutSgprInstanceVs = (uint32_t)-1;

	*pOutSgprKrbLoc = (uint32_t)-1;
	if (pOutSgprInstanceCs != NULL)
		*pOutSgprInstanceCs = (uint32_t)-1;
	SCE_GNM_ASSERT_MSG(m_asyncCsShaderOrderedAppendMode == Gnm::kDispatchOrderedAppendModeIndexPerWavefront || m_asyncCsShaderOrderedAppendMode == Gnm::kDispatchOrderedAppendModeIndexPerThreadgroup, "dispatchDraw expects a CS shader with m_orderedAppendMode != kDispatchOrderedAppendModeDisabled");
	SCE_GNM_ASSERT_MSG(m_dispatchDrawIndexDeallocNumBits >= 1 && m_dispatchDrawIndexDeallocNumBits <= 15, "dispatchDraw expects a CS shader with m_dispatchDrawIndexDeallocNumBits in the range [1:15]");
	*pOutOrderedAppendMode = (Gnm::DispatchOrderedAppendMode)m_asyncCsShaderOrderedAppendMode;
	*pOutDispatchDrawIndexDeallocationMask = 0x0000FFFF & (0xFFFF << m_dispatchDrawIndexDeallocNumBits);

	if (m_dirtyShaderResources[Gnm::kShaderStageVs] || m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute])
	{
		// Validate after handling embedded CBs (if necessary) and before flushing scratch buffer
		SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageVs], m_boundShaderResourceOffsets[Gnm::kShaderStageVs]);

		const uint32_t* CsVsflushAddress = flushScratchBuffer(Gnm::kShaderStageVs);
		updateCommonPtrsInUserDataSgprs(Gnm::kShaderStageVs, CsVsflushAddress);
		updateLsEsVsPtrsInUserDataSgprs(Gnm::kShaderStageVs, CsVsflushAddress);
		updateVsDispatchDrawPtrsInUserDataSgprs(CsVsflushAddress);

		// Also explicitly prefetch the dispatch draw data into L2 for the compute shaders:
		if (m_pDispatchDrawData && m_sizeofDispatchDrawData)
		{
			m_asyncDispatchCommandBuffer->prefetchIntoL2((void*)m_pDispatchDrawData, m_sizeofDispatchDrawData);
		}

		const InputResourceOffsets* vsTable = m_boundShaderResourceOffsets[Gnm::kShaderStageVs];
		if (vsTable->vertexRingBufferOffsetSgpr != 0xFF)
		{
			*pOutSgprVrbLoc = vsTable->vertexRingBufferOffsetSgpr;
			*pOutDispatchDrawMode = Gnm::kDispatchDrawModeIndexAndVertexRingBuffer;
		}

		if (vsTable->dispatchDrawInstancesSgpr != 0xFF)
		{
			if (pOutSgprInstanceVs != NULL)
				*pOutSgprInstanceVs = vsTable->dispatchDrawInstancesSgpr;
			else
				m_drawCommandBuffer->setUserData(Gnm::kShaderStageVs, vsTable->dispatchDrawInstancesSgpr, 0);
		}

		// The dispatch draw CS shader shares the same EUD + Input usage tables as the VS stage buffer, so updateDispatchDrawPtrsInUserDataSgprs() 
		// will bind the same pointers using the shared CsVsflushAddress computed above...
		updateCsDispatchDrawPtrsInUserDataSgprs(CsVsflushAddress);

		const InputResourceOffsets* csTable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
		if (csTable->gdsKickRingBufferOffsetSgpr != 0xFF)
		{
			*pOutSgprKrbLoc = csTable->gdsKickRingBufferOffsetSgpr;
		}
		if (csTable->dispatchDrawInstancesSgpr != 0xFF)
		{
			if (pOutSgprInstanceCs != NULL)
				*pOutSgprInstanceCs = csTable->dispatchDrawInstancesSgpr;
			else
				m_asyncDispatchCommandBuffer->setUserData(csTable->dispatchDrawInstancesSgpr, 0);
		}
	}
	SCE_GNM_ASSERT_MSG(*pOutSgprKrbLoc != (uint32_t)-1, "dispatchDraw expects a CS shader with an InputUsageSlot of m_usageType kShaderInputUsageImmGdsKickRingBufferOffset in a real user sgpr");

	if (m_dirtyShaderResources[Gnm::kShaderStagePs])
	{
		// Validate after handling embedded CBs (if necessary) and before flushing scratch buffer
		SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStagePs], m_boundShaderResourceOffsets[Gnm::kShaderStagePs]);

		const uint32_t* flushAddress = flushScratchBuffer(Gnm::kShaderStagePs);
		updateCommonPtrsInUserDataSgprs(Gnm::kShaderStagePs, flushAddress);
	}

	// This can be disabled if an application will call Gnm::drawCommandBuffer::setPsShaderUsage() manually.
	// Note: If context bound, since this additional path will always result in a context roll, 
	// if either the Vs or Ps shaders are marked dirty, it would be best to disable SCE_GNM_LCUE_SKIP_VS_PS_SEMANTIC_TABLE
	// and manually track changes within the application and bind appropriately. 
#if !SCE_GNM_LCUE_SKIP_VS_PS_SEMANTIC_TABLE
	if (m_dirtyShader[Gnm::kShaderStageVs] || m_dirtyShader[Gnm::kShaderStagePs])
	{
		if (psShader != NULL && psShader->m_numInputSemantics != 0)
		{
			if (m_psInputsTable)
			{
				setOrUseCachedPsInputUsageTable(m_psInputsTable, psShader->m_numInputSemantics);
			}
			else
			{
				uint32_t psInputsTable[32];
				Gnm::generatePsShaderUsageTable(psInputsTable,
												vsShader->getExportSemanticTable(), vsShader->m_numExportSemantics,
												psShader->getPixelInputSemanticTable(), psShader->m_numInputSemantics);

				setOrUseCachedPsInputUsageTable(psInputsTable, psShader->m_numInputSemantics);
			}
		}
	}
#endif // !SCE_GNM_LCUE_SKIP_VS_PS_SEMANTIC_TABLE

	// clear dirty Compute vertex shader states
	m_dirtyShaderResources[Gnm::kShaderStagePs] = false;
	m_dirtyShader[Gnm::kShaderStagePs] = false;
	m_dirtyShaderUpdateOnly[Gnm::kShaderStagePs] = false;

	m_dirtyShaderResources[Gnm::kShaderStageVs] = false;
	m_dirtyShader[Gnm::kShaderStageVs] = false;
	m_dirtyShaderUpdateOnly[Gnm::kShaderStageVs] = false;

	m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = false;
	m_dirtyShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = false;
}


SCE_GNM_FORCE_INLINE void LightweightGraphicsConstantUpdateEngine::preDrawTessellation(bool geometryShaderEnabled)
{
	SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageLs] != NULL); // LS cannot be NULL for LsHs pipeline configuration
	SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageHs] != NULL); // HS cannot be NULL for LsHs pipeline configuration

	const Gnmx::LsShader* lsShader = (const Gnmx::LsShader*)m_boundShader[Gnm::kShaderStageLs];
	const Gnmx::HsShader* hsShader = (const Gnmx::HsShader*)m_boundShader[Gnm::kShaderStageHs];

	// if this is 0, its a bug in the PSSL compiler, MAX_TESS_FACTOR was not defined!
	SCE_GNM_ASSERT(hsShader->m_hsStageRegisters.m_vgtHosMaxTessLevel != 0);

	// if enabled, LCUE will use space in the main buffer, and set the requires tessellation constant buffers itself.
	if (m_tessellationAutoManageReservedCb)	// Note: enabled by default in GNMX, if user chooses to manage and set the buffers themselves, make sure to disable
	{
		// LS or HS shader has been marked dirty, so reset the tessellation data constant buffers
		if (m_dirtyShader[Gnm::kShaderStageLs] || m_dirtyShader[Gnm::kShaderStageHs])
		{
			SCE_GNM_ASSERT(sizeof(Gnm::TessellationDataConstantBuffer) % 4 == 0);
			const uint32_t kTesselationDataConstantBufferSizeInDwords = sizeof(Gnm::TessellationDataConstantBuffer) / sizeof(uint32_t);

			// Generate internal TessellationDataConstantBuffer using some space from the main buffer
			Gnm::TessellationDataConstantBuffer* tessellationInternalConstantBuffer = (Gnm::TessellationDataConstantBuffer*)reserveResourceBufferSpace(kTesselationDataConstantBufferSizeInDwords);
			
			tessellationInternalConstantBuffer->init(&hsShader->m_hullStateConstants, lsShader->m_lsStride, m_tessellationCurrentTgPatchCount, m_tessellationCurrentOffChipFactorThreshold);

			// Update tessellation internal CB on API-slot 19
			m_tessellationCurrentCb.initAsConstantBuffer(tessellationInternalConstantBuffer, sizeof(Gnm::TessellationDataConstantBuffer));
			m_tessellationCurrentCb.setResourceMemoryType(Gnm::kResourceMemoryTypeRO); // tessellation constant buffer, is read-only
		}

		// update hull shader and domain shader (domain runs on either ES or VS stage depending on if geometry shading is enabled)
		setConstantBuffers(Gnm::kShaderStageHs, LightweightConstantUpdateEngine::kConstantBufferInternalApiSlotForTessellation, 1, &m_tessellationCurrentCb);
		setConstantBuffers((geometryShaderEnabled)? Gnm::kShaderStageEs : Gnm::kShaderStageVs, LightweightConstantUpdateEngine::kConstantBufferInternalApiSlotForTessellation, 1, &m_tessellationCurrentCb);
	}

	if (m_dirtyShader[Gnm::kShaderStageLs])
	{
		m_drawCommandBuffer->setLsShader(&lsShader->m_lsStageRegisters, m_boundShaderModifier[Gnm::kShaderStageLs]);
		if (m_prefetchShaderCode)
		{
			SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)lsShader->m_lsStageRegisters.m_spiShaderPgmLoLs << 8), lsShader->m_common.m_shaderSize);
			m_drawCommandBuffer->prefetchIntoL2((void*)((uintptr_t)lsShader->m_lsStageRegisters.m_spiShaderPgmLoLs << 8), lsShader->m_common.m_shaderSize);
		}
	}

	if (m_dirtyShader[Gnm::kShaderStageHs])
	{
		Gnm::TessellationRegisters tessellationVgtLsHsConfiguration;
		tessellationVgtLsHsConfiguration.init( &hsShader->m_hullStateConstants, m_tessellationCurrentTgPatchCount);

		Gnm::HsStageRegisters hsStageRegisters = hsShader->m_hsStageRegisters;

		// Update TessellationDistributionMode (Neo only)
		if ((m_tessellationCurrentDistributionMode & LightweightConstantUpdateEngine::kTesselationDistrbutionEnabledMask) == LightweightConstantUpdateEngine::kTesselationDistrbutionEnabledMask)
		{
			Gnm::TessellationDistributionMode distributionMode = static_cast<Gnm::TessellationDistributionMode>(m_tessellationCurrentDistributionMode & LightweightConstantUpdateEngine::kTesselationDistrbutionMask);
			hsStageRegisters.setTessellationDistributionMode(distributionMode);
		}

#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_dirtyShaderUpdateOnly[Gnm::kShaderStageHs])
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStageHs);
			m_drawCommandBuffer->updateHsShader(&hsStageRegisters, &tessellationVgtLsHsConfiguration);
		}
		else
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStageHs);	// shader has been bound, it's okay to use update*Shader() method
			m_drawCommandBuffer->setHsShader(&hsStageRegisters, &tessellationVgtLsHsConfiguration);
		}
#else // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		m_drawCommandBuffer->setHsShader(&hsStageRegisters, &tessellationVgtLsHsConfiguration);
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_prefetchShaderCode)
		{
			SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)hsShader->m_hsStageRegisters.m_spiShaderPgmLoHs << 8), hsShader->m_common.m_shaderSize);
			m_drawCommandBuffer->prefetchIntoL2((void*)((uintptr_t)hsShader->m_hsStageRegisters.m_spiShaderPgmLoHs << 8), hsShader->m_common.m_shaderSize);
		}
	}

	// Handle embedded Constant Buffer on CB slot 15 (kShaderSlotEmbeddedConstantBuffer)
	if (m_dirtyShader[Gnm::kShaderStageLs] || m_dirtyShaderResources[Gnm::kShaderStageLs])
		updateEmbeddedCb(Gnm::kShaderStageLs, (const Gnmx::ShaderCommonData*)lsShader);
	if (m_dirtyShader[Gnm::kShaderStageHs] || m_dirtyShaderResources[Gnm::kShaderStageHs])
		updateEmbeddedCb(Gnm::kShaderStageHs, (const Gnmx::ShaderCommonData*)hsShader);

	if (m_dirtyShaderResources[Gnm::kShaderStageLs])
	{
		// Validate after handling embedded CBs (if necessary) and before flushing scratch buffer
		SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageLs], m_boundShaderResourceOffsets[Gnm::kShaderStageLs]);

		const uint32_t* flushAddress = flushScratchBuffer(Gnm::kShaderStageLs);
		updateCommonPtrsInUserDataSgprs(Gnm::kShaderStageLs, flushAddress);
		updateLsEsVsPtrsInUserDataSgprs(Gnm::kShaderStageLs, flushAddress);
	}
	if (m_dirtyShaderResources[Gnm::kShaderStageHs])
	{
		// Validate after handling embedded CBs (if necessary) and before flushing scratch buffer
		SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageHs], m_boundShaderResourceOffsets[Gnm::kShaderStageHs]);

		const uint32_t* flushAddress = flushScratchBuffer(Gnm::kShaderStageHs);
		updateCommonPtrsInUserDataSgprs(Gnm::kShaderStageHs, flushAddress);
	}

	// clear dirty tessellation stages
	m_dirtyShaderResources[Gnm::kShaderStageLs] = false;
	m_dirtyShader[Gnm::kShaderStageLs] = false;

	m_dirtyShaderResources[Gnm::kShaderStageHs] = false;
	m_dirtyShader[Gnm::kShaderStageHs] = false;
	m_dirtyShaderUpdateOnly[Gnm::kShaderStageHs] = false;
}


SCE_GNM_FORCE_INLINE void LightweightGraphicsConstantUpdateEngine::preDrawGeometryShader()
{
	SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageEs] != NULL); // ES cannot be NULL for any EsGs pipeline configuration
	SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageGs] != NULL); // GS cannot be NULL for any EsGs pipeline configuration

	const Gnmx::EsShader* esShader = (const Gnmx::EsShader*)m_boundShader[Gnm::kShaderStageEs];
	const Gnmx::GsShader* gsShader = (const Gnmx::GsShader*)m_boundShader[Gnm::kShaderStageGs];

	if (m_dirtyShader[Gnm::kShaderStageEs])
	{
		if (m_onChipLdsSizeIn512Bytes!= 0)
		{
			Gnm::EsStageRegisters esRegsCopy = esShader->m_esStageRegisters; // m_currentESB is read-only
			esRegsCopy.updateLdsSize(m_onChipLdsSizeIn512Bytes);
			m_drawCommandBuffer->setEsShader(&esRegsCopy, m_boundShaderModifier[Gnm::kShaderStageEs]);
			if (m_prefetchShaderCode)
			{
				SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)esShader->m_esStageRegisters.m_spiShaderPgmLoEs << 8), esShader->m_common.m_shaderSize);
				m_drawCommandBuffer->prefetchIntoL2((void*)((uintptr_t)esShader->m_esStageRegisters.m_spiShaderPgmLoEs << 8), esShader->m_common.m_shaderSize);
			}
		}
		else
		{
			m_drawCommandBuffer->setEsShader(&esShader->m_esStageRegisters, m_boundShaderModifier[Gnm::kShaderStageEs]);
			if (m_prefetchShaderCode)
			{
				SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)esShader->m_esStageRegisters.m_spiShaderPgmLoEs << 8), esShader->m_common.m_shaderSize);
				m_drawCommandBuffer->prefetchIntoL2((void*)((uintptr_t)esShader->m_esStageRegisters.m_spiShaderPgmLoEs << 8), esShader->m_common.m_shaderSize);
			}
		}
	}
	if (m_dirtyShader[Gnm::kShaderStageGs])
	{
#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_dirtyShaderUpdateOnly[Gnm::kShaderStageGs])
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStageGs);
			m_drawCommandBuffer->updateGsShader(&gsShader->m_gsStageRegisters);
		}
		else
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStageGs);	// shader has been bound, it's okay to use update*Shader() method
			m_drawCommandBuffer->setGsShader(&gsShader->m_gsStageRegisters);
		}
#else // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		m_drawCommandBuffer->setGsShader(&gsShader->m_gsStageRegisters);
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_prefetchShaderCode)
		{
			SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)gsShader->m_gsStageRegisters.m_spiShaderPgmLoGs << 8), gsShader->m_common.m_shaderSize);
			m_drawCommandBuffer->prefetchIntoL2((void*)((uintptr_t)gsShader->m_gsStageRegisters.m_spiShaderPgmLoGs << 8), gsShader->m_common.m_shaderSize);
		}
	}

	// Handle embedded Constant Buffer on CB slot 15 (kShaderSlotEmbeddedConstantBuffer)
	if (m_dirtyShader[Gnm::kShaderStageEs] || m_dirtyShaderResources[Gnm::kShaderStageEs])
		updateEmbeddedCb(Gnm::kShaderStageEs, (const Gnmx::ShaderCommonData*)esShader);
	if (m_dirtyShader[Gnm::kShaderStageGs] || m_dirtyShaderResources[Gnm::kShaderStageGs])
		updateEmbeddedCb(Gnm::kShaderStageGs, (const Gnmx::ShaderCommonData*)gsShader);

	if (m_dirtyShaderResources[Gnm::kShaderStageEs])
	{
		// Validate after handling embedded CBs (if necessary) and before flushing scratch buffer
		SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageEs], m_boundShaderResourceOffsets[Gnm::kShaderStageEs]);

		const uint32_t* flushAddress = flushScratchBuffer(Gnm::kShaderStageEs);
		updateCommonPtrsInUserDataSgprs(Gnm::kShaderStageEs, flushAddress);
		updateLsEsVsPtrsInUserDataSgprs(Gnm::kShaderStageEs, flushAddress);
	}
	if (m_dirtyShaderResources[Gnm::kShaderStageGs])
	{
		// Validate after handling embedded CBs (if necessary) and before flushing scratch buffer
		SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageGs], m_boundShaderResourceOffsets[Gnm::kShaderStageGs]);

		const uint32_t* flushAddress = flushScratchBuffer(Gnm::kShaderStageGs);
		updateCommonPtrsInUserDataSgprs(Gnm::kShaderStageGs, flushAddress);
		updateOnChipParametersInUserDataSgprs(Gnm::kShaderStageGs);
	}

	// clear dirty geometry stages
	m_dirtyShaderResources[Gnm::kShaderStageEs] = false;
	m_dirtyShader[Gnm::kShaderStageEs] = false;

	m_dirtyShaderResources[Gnm::kShaderStageGs] = false;
	m_dirtyShader[Gnm::kShaderStageGs] = false;
	m_dirtyShaderUpdateOnly[Gnm::kShaderStageGs] = false;
}


void LightweightGraphicsConstantUpdateEngine::preDraw()
{
	SCE_GNM_ASSERT(m_drawCommandBuffer != NULL );
	SCE_GNM_ASSERT(m_activeShaderStages != Gnm::kActiveShaderStagesDispatchDrawVsPs);

#if PS4_ENABLE_COMMANDBUFFER_RESERVE
	m_drawCommandBuffer->reserveSpaceInDwords( COMMANDBUFFER_RESERVE_SIZE_BYTES / sizeof( uint32 ) );
#endif

	bool tessellationEnabled = false;
	bool geometryShaderEnabled = false;

	geometryShaderEnabled = (m_activeShaderStages >> 5) & 1;
	if (geometryShaderEnabled)
		preDrawGeometryShader();

	tessellationEnabled = (m_activeShaderStages & 1);
	if (tessellationEnabled)
		preDrawTessellation(geometryShaderEnabled);
	
 	const Gnmx::VsShader* vsShader = ((const Gnmx::VsShader*)m_boundShader[Gnm::kShaderStageVs]);
	const Gnmx::PsShader* psShader = ((const Gnmx::PsShader*)m_boundShader[Gnm::kShaderStagePs]);

	if (m_dirtyShader[Gnm::kShaderStageVs])
	{
#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_dirtyShaderUpdateOnly[Gnm::kShaderStageVs])
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStageVs);
			m_drawCommandBuffer->updateVsShader(&vsShader->m_vsStageRegisters, m_boundShaderModifier[Gnm::kShaderStageVs]);
		}
		else
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStageVs);	// shader has been bound, it's okay to use updateVsShader()
			m_drawCommandBuffer->setVsShader(&vsShader->m_vsStageRegisters, m_boundShaderModifier[Gnm::kShaderStageVs]);
		}
#else // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER 
		m_drawCommandBuffer->setVsShader(&vsShader->m_vsStageRegisters, m_boundShaderModifier[Gnm::kShaderStageVs]);
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_prefetchShaderCode)
		{
			SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)vsShader->m_vsStageRegisters.m_spiShaderPgmLoVs << 8), vsShader->m_common.m_shaderSize);
			m_drawCommandBuffer->prefetchIntoL2((void*)((uintptr_t)vsShader->m_vsStageRegisters.m_spiShaderPgmLoVs << 8), vsShader->m_common.m_shaderSize);
		}
	}

	if (m_dirtyShader[Gnm::kShaderStagePs])
	{
#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_dirtyShaderUpdateOnly[Gnm::kShaderStagePs])
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStagePs);
			m_drawCommandBuffer->updatePsShader((psShader != NULL) ? &psShader->m_psStageRegisters : NULL);
		}
		else
		{
			m_updateShaderBindingIsValid |= (1 << Gnm::kShaderStagePs);	// shader has been bound, it's okay to use update*Shader() method
			m_drawCommandBuffer->setPsShader((psShader != NULL) ? &psShader->m_psStageRegisters : NULL);
		}
#else // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		m_drawCommandBuffer->setPsShader((psShader != NULL) ? &psShader->m_psStageRegisters : NULL);
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
		if (m_prefetchShaderCode && psShader != NULL)
		{
			SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)psShader->m_psStageRegisters.m_spiShaderPgmLoPs << 8), psShader->m_common.m_shaderSize);
			m_drawCommandBuffer->prefetchIntoL2((void*)((uintptr_t)psShader->m_psStageRegisters.m_spiShaderPgmLoPs << 8), psShader->m_common.m_shaderSize);
		}
	}
	
	// Handle embedded Constant Buffer on CB slot 15 (kShaderSlotEmbeddedConstantBuffer)
	if (m_dirtyShader[Gnm::kShaderStageVs] || m_dirtyShaderResources[Gnm::kShaderStageVs])
		updateEmbeddedCb(Gnm::kShaderStageVs, (const Gnmx::ShaderCommonData*)vsShader);
	if (m_dirtyShader[Gnm::kShaderStagePs] || m_dirtyShaderResources[Gnm::kShaderStagePs])
		updateEmbeddedCb(Gnm::kShaderStagePs, (const Gnmx::ShaderCommonData*)psShader);

	if (m_dirtyShaderResources[Gnm::kShaderStageVs])
	{
		// Validate after handling embedded CBs (if necessary) and before flushing scratch buffer
		SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageVs], m_boundShaderResourceOffsets[Gnm::kShaderStageVs]);

		const uint32_t* flushAddress = flushScratchBuffer(Gnm::kShaderStageVs);
		updateCommonPtrsInUserDataSgprs(Gnm::kShaderStageVs, flushAddress);
		updateLsEsVsPtrsInUserDataSgprs(Gnm::kShaderStageVs, flushAddress);
		updateOnChipParametersInUserDataSgprs(Gnm::kShaderStageVs);
	}
	if (m_dirtyShaderResources[Gnm::kShaderStagePs])
	{
		// Validate after handling embedded CBs (if necessary) and before flushing scratch buffer
		SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStagePs], m_boundShaderResourceOffsets[Gnm::kShaderStagePs]);

		const uint32_t* flushAddress = flushScratchBuffer(Gnm::kShaderStagePs);
		updateCommonPtrsInUserDataSgprs(Gnm::kShaderStagePs, flushAddress);
	}

	// This can be disabled if an application will call Gnm::drawCommandBuffer::setPsShaderUsage() manually.
	// Note: If context bound, since this additional path will always result in a context roll, 
	// if either the Vs or Ps shaders are marked dirty, it would be best to disable SCE_GNM_LCUE_SKIP_VS_PS_SEMANTIC_TABLE
	// and manually track changes within the application and bind appropriately. 
#if !SCE_GNM_LCUE_SKIP_VS_PS_SEMANTIC_TABLE
	if (m_dirtyShader[Gnm::kShaderStageVs] || m_dirtyShader[Gnm::kShaderStagePs])
	{
		if (vsShader != NULL && psShader != NULL && psShader->m_numInputSemantics != 0)
		{
			if (m_psInputsTable)
			{
				setOrUseCachedPsInputUsageTable(m_psInputsTable, psShader->m_numInputSemantics);
			}
			else
			{
				uint32_t psInputsTable[32];
				Gnm::generatePsShaderUsageTable(psInputsTable,
												vsShader->getExportSemanticTable(), vsShader->m_numExportSemantics,
												psShader->getPixelInputSemanticTable(), psShader->m_numInputSemantics);

				setOrUseCachedPsInputUsageTable(psInputsTable, psShader->m_numInputSemantics);
			}
		}
	}
#endif // !SCE_GNM_LCUE_SKIP_VS_PS_SEMANTIC_TABLE

	// clear dirty PS and VS stages
	m_dirtyShaderResources[Gnm::kShaderStagePs] = false;
	m_dirtyShader[Gnm::kShaderStagePs] = false;
	m_dirtyShaderUpdateOnly[Gnm::kShaderStagePs] = false;

	m_dirtyShaderResources[Gnm::kShaderStageVs] = false;
	m_dirtyShader[Gnm::kShaderStageVs] = false;
	m_dirtyShaderUpdateOnly[Gnm::kShaderStageVs] = false;
}


void LightweightGraphicsConstantUpdateEngine::setEsShader(const sce::Gnmx::EsShader* shader, uint32_t shaderModifier, const void* fetchShader, const InputResourceOffsets* table)
{
	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == Gnm::kShaderStageEs);
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageEs], table);

	m_dirtyShaderResources[Gnm::kShaderStageEs] = true;
	m_dirtyShader[Gnm::kShaderStageEs] |= (m_boundShader[Gnm::kShaderStageEs] != shader);

	m_shaderBindingIsValid |= (1 << Gnm::kShaderStageEs);
	m_boundShaderResourceOffsets[Gnm::kShaderStageEs] = table;
	m_boundShader[Gnm::kShaderStageEs] = shader;
	m_boundFetchShader[Gnm::kShaderStageEs] = fetchShader;
	m_boundShaderModifier[Gnm::kShaderStageEs] = shaderModifier;

	// on chip isn't being used
	m_onChipLdsSizeIn512Bytes = 0;
}


void LightweightGraphicsConstantUpdateEngine::setOnChipEsShader(const Gnmx::EsShader *shader, uint32_t ldsSizeIn512Bytes, uint32_t shaderModifier, const void *fetchShader, const InputResourceOffsets* table)
{
	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == Gnm::kShaderStageEs);
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageEs], table);

	m_dirtyShaderResources[Gnm::kShaderStageEs] = true;
	m_dirtyShader[Gnm::kShaderStageEs] |= (m_boundShader[Gnm::kShaderStageEs] != shader);

	m_shaderBindingIsValid |= (1 << Gnm::kShaderStageEs);
	m_boundShaderResourceOffsets[Gnm::kShaderStageEs] = table;
	m_boundShader[Gnm::kShaderStageEs] = shader;
	m_boundFetchShader[Gnm::kShaderStageEs] = fetchShader;
	m_boundShaderModifier[Gnm::kShaderStageEs] = shaderModifier;

	m_onChipLdsSizeIn512Bytes = ldsSizeIn512Bytes;
}


void LightweightGraphicsConstantUpdateEngine::setEsFetchShader(uint32_t shaderModifier, const void* fetchShader)
{
	m_boundShaderModifier[Gnm::kShaderStageEs] = shaderModifier;
	m_boundFetchShader[Gnm::kShaderStageEs] = fetchShader;
}


void LightweightGraphicsConstantUpdateEngine::setGsVsShaders(const sce::Gnmx::GsShader* shader, const InputResourceOffsets* table)
{
	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == Gnm::kShaderStageGs && !shader->isOnChip());
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageGs], table);

	// cache the GS shader
	m_dirtyShaderResources[Gnm::kShaderStageGs] = true;
	m_dirtyShader[Gnm::kShaderStageGs] |= (m_boundShader[Gnm::kShaderStageGs] != shader);

#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
	// Note: Because the DCB binding of the shader is deferred until preDraw(), we must first check to verify the shader has at least been bound with its context registers in preDraw()
	m_dirtyShaderUpdateOnly[Gnm::kShaderStageGs] = (m_updateShaderBindingIsValid & (1 << Gnm::kShaderStageGs)) != 0 && m_dirtyShader[Gnm::kShaderStageGs] && m_boundShader[Gnm::kShaderStageGs] 
													&& shader->m_gsStageRegisters.isSharingContext(((Gnmx::GsShader*)m_boundShader[Gnm::kShaderStageGs])->m_gsStageRegisters);

	// Avoid marking shader as updateable unless preDraw() is called again, necessary to work around redundant shader bindings
	m_updateShaderBindingIsValid &= ~(1 << Gnm::kShaderStageGs);
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER

	m_shaderBindingIsValid |= (1 << Gnm::kShaderStageGs);
	m_boundShaderResourceOffsets[Gnm::kShaderStageGs] = table;
	m_boundShader[Gnm::kShaderStageGs] = shader;

	// when setting the GS, copy shader needs to be bound to the VS stage as well
	const sce::Gnmx::VsShader* vsShader = shader->getCopyShader();
	const InputResourceOffsets* vsShaderResourceOffsets = (vsShader->m_common.m_numInputUsageSlots == 1) ?
														&m_fixedGsVsShaderResourceOffsets : &m_fixedGsVsStreamOutShaderResourceOffsets;
	setVsShader(vsShader, vsShaderResourceOffsets);

	// This rolls the hardware context, thus we should only set it if it's really necessary
	Gnm::GsMaxOutputPrimitiveDwordSize requiredGsMaxOutput = shader->getGsMaxOutputPrimitiveDwordSize();
	if(m_gsMode != Gnm::kGsModeEnable || requiredGsMaxOutput != m_gsMaxOutput)
	{
		m_drawCommandBuffer->enableGsMode(requiredGsMaxOutput);
	}

	m_gsMode = Gnm::kGsModeEnable;
	m_gsMaxOutput = requiredGsMaxOutput;
}


void LightweightGraphicsConstantUpdateEngine::setOnChipGsVsShaders(const sce::Gnmx::GsShader* shader, uint32_t gsPrimsPerSubGroup, const InputResourceOffsets* table)
{
	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == Gnm::kShaderStageGs && shader->isOnChip());
	SCE_GNM_ASSERT_MSG(gsPrimsPerSubGroup * shader->getSizePerPrimitiveInBytes() <= 64 * 1024, "gsPrimsPerSubGroup*shader->getSizePerPrimitiveInBytes() will not fit in 64KB LDS");
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageGs], table);

	// cache the GS shader
	m_dirtyShaderResources[Gnm::kShaderStageGs] = true;
	m_dirtyShader[Gnm::kShaderStageGs] |= (m_boundShader[Gnm::kShaderStageGs] != shader);

#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
	// Note: Because the DCB binding of the shader is deferred until preDraw(), we must first check to verify the shader has at least been bound with its context registers in preDraw()
	m_dirtyShaderUpdateOnly[Gnm::kShaderStageGs] = (m_updateShaderBindingIsValid & (1 << Gnm::kShaderStageGs)) != 0 && m_dirtyShader[Gnm::kShaderStageGs] && m_boundShader[Gnm::kShaderStageGs] 
													&& shader->m_gsStageRegisters.isSharingContext(((Gnmx::GsShader*)m_boundShader[Gnm::kShaderStageGs])->m_gsStageRegisters);

	// Avoid marking shader as updateable unless preDraw() is called again, necessary to work around redundant shader bindings
	m_updateShaderBindingIsValid &= ~(1 << Gnm::kShaderStageGs);
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER

	m_shaderBindingIsValid |= (1 << Gnm::kShaderStageGs);
	m_boundShaderResourceOffsets[Gnm::kShaderStageGs] = table;
	m_boundShader[Gnm::kShaderStageGs] = shader;

	// when setting the GS, copy shader needs to be bound to the VS stage as well
	const sce::Gnmx::VsShader* vsShader = shader->getCopyShader();
	const InputResourceOffsets* vsShaderResourceOffsets = (vsShader->m_common.m_numInputUsageSlots == 1) ? 
														&m_fixedOnChipGsVsShaderResourceOffsets : &m_fixedOnChipGsVsStreamOutShaderResourceOffsets;
	setVsShader(vsShader, vsShaderResourceOffsets);

	uint16_t esVertsPerSubGroup = (uint16_t)((shader->m_inputVertexCountMinus1 + 1) * gsPrimsPerSubGroup);
	SCE_GNM_ASSERT(esVertsPerSubGroup <= 2047);

	Gnm::GsMaxOutputPrimitiveDwordSize requiredGsMaxOutput = shader->getGsMaxOutputPrimitiveDwordSize();
	SCE_GNM_ASSERT_MSG((uint32_t)requiredGsMaxOutput < 4, "requiredGsMaxOutput (0x%02X) is invalid enum value.", requiredGsMaxOutput);

	// Only enabled Gs on-chip when required
	const uint32_t kSignature = (((uint32_t)requiredGsMaxOutput) << 22) | (esVertsPerSubGroup << 11) | (gsPrimsPerSubGroup);
	if (m_gsMode != Gnm::kGsModeEnableOnChip || kSignature != m_onChipGsSignature)
	{
		// If we are coming from a clean state/non-GS on-chip draw, we can skip disabling GS Mode and avoid a context roll
		if (m_onChipGsSignature != LightweightConstantUpdateEngine::kOnChipGsInvalidSignature)
		{
			// Note: if the on-chip GS mode has been set previously, but our signature has changed we need to turn it off before enableGsModeOnChip is called again, 
			// otherwise the GPU may hang, See technote: https://ps4.scedev.net/technotes/view/680/1
			m_drawCommandBuffer->disableGsMode();
		}

		m_drawCommandBuffer->enableGsModeOnChip(requiredGsMaxOutput,
												esVertsPerSubGroup, gsPrimsPerSubGroup);
	}

	setOnChipEsVertsPerSubGroup(esVertsPerSubGroup);

	m_gsMode = Gnm::kGsModeEnableOnChip;
	m_gsMaxOutput = requiredGsMaxOutput;
	m_onChipGsSignature = kSignature;
}


void LightweightGraphicsConstantUpdateEngine::setOnChipEsVertsPerSubGroup(uint16_t onChipEsVertsPerSubGroup)
{
	// ldsEsGsSizeSgpr is actually bound to both the GS/VS Copy stage
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[Gnm::kShaderStageGs] != NULL);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)Gnm::kShaderStageGs] != NULL);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[Gnm::kShaderStageVs] != NULL);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)Gnm::kShaderStageVs] != NULL);

	SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets[Gnm::kShaderStageGs]->ldsEsGsSizeSgpr != 0xFF);
	if (m_boundShaderResourceOffsets[Gnm::kShaderStageGs]->ldsEsGsSizeSgpr != 0xFF)
	{
		m_onChipEsVertsPerSubGroup = onChipEsVertsPerSubGroup;
	}

	m_dirtyShaderResources[Gnm::kShaderStageGs] = true;
	m_dirtyShaderResources[Gnm::kShaderStageVs] = true;
}


void LightweightGraphicsConstantUpdateEngine::setOnChipEsExportVertexSizeInDword(uint16_t onChipEsExportVertexSizeInDword)
{
	// ldsEsGsSizeSgpr is actually bound to both the GS/VS Copy stage
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[Gnm::kShaderStageGs] != NULL);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)Gnm::kShaderStageGs] != NULL);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[Gnm::kShaderStageVs] != NULL);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)Gnm::kShaderStageVs] != NULL);

	SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets[Gnm::kShaderStageGs]->ldsEsGsSizeSgpr != 0xFF);
	if (m_boundShaderResourceOffsets[Gnm::kShaderStageGs]->ldsEsGsSizeSgpr != 0xFF)
	{
		m_onChipEsExportVertexSizeInDword = onChipEsExportVertexSizeInDword;
	}

	m_dirtyShaderResources[Gnm::kShaderStageGs] = true;
	m_dirtyShaderResources[Gnm::kShaderStageVs] = true;
}


void LightweightGraphicsConstantUpdateEngine::setStreamoutBuffers(int32_t startSlot, int32_t numSlots, const sce::Gnm::Buffer* buffers)
{
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)Gnm::kShaderStageVs] != NULL && buffers != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)Gnm::kShaderStageVs];

	SCE_GNM_ASSERT(startSlot >= 0 && startSlot < LightweightConstantUpdateEngine::kMaxStreamOutBufferCount);
	SCE_GNM_ASSERT(numSlots >= 0 && numSlots <= LightweightConstantUpdateEngine::kMaxStreamOutBufferCount);
	SCE_GNM_ASSERT(startSlot + numSlots <= LightweightConstantUpdateEngine::kMaxStreamOutBufferCount);
	//SCE_GNM_LCUE_VALIDATE_RESOURCE_MEMORY_MAPPED(buffer, SCE_KERNEL_PROT_GPU_READ|SCE_KERNEL_PROT_GPU_WRITE);

	for (int32_t i = 0; i<numSlots; ++i)
	{
		SCE_GNM_ASSERT((buffers+i)->isBuffer());
		int32_t currentApiSlot = startSlot+i;

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets[(int32_t)Gnm::kShaderStageVs]->streamOutPtrSgpr != 0xFF);
		if (m_boundShaderResourceOffsets[(int32_t)Gnm::kShaderStageVs]->streamOutPtrSgpr != 0xFF)
		{
			// SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[Gnm::kShaderStageVs].streamOutOffsetIsBound[currentApiSlot]);
			setGfxPipeDataInUserDataSgprOrMemory(m_drawCommandBuffer, m_scratchBuffer, Gnm::kShaderStageVs, table->streamOutDwOffset[currentApiSlot], buffers + i, sizeof(Gnm::Buffer));
		}
	}

	m_dirtyShaderResources[(int32_t)Gnm::kShaderStageVs] = true;
}


void LightweightGraphicsConstantUpdateEngine::setLsShader(const Gnmx::LsShader* shader, uint32_t shaderModifier, const void* fetchShader, const InputResourceOffsets* table)
{
	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == Gnm::kShaderStageLs);
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageLs], table);

	m_dirtyShaderResources[Gnm::kShaderStageLs] = true;
	m_dirtyShader[Gnm::kShaderStageLs] |= (m_boundShader[Gnm::kShaderStageLs] != shader);

	m_shaderBindingIsValid |= (1 << Gnm::kShaderStageLs);
	m_boundShaderResourceOffsets[Gnm::kShaderStageLs] = table;
	m_boundShader[Gnm::kShaderStageLs] = shader;
	m_boundFetchShader[Gnm::kShaderStageLs] = fetchShader;
	m_boundShaderModifier[Gnm::kShaderStageLs] = shaderModifier;
}


void LightweightGraphicsConstantUpdateEngine::setLsFetchShader(uint32_t shaderModifier, const void* fetchShader)
{
	m_boundShaderModifier[Gnm::kShaderStageLs] = shaderModifier;
	m_boundFetchShader[Gnm::kShaderStageLs] = fetchShader;
}


void LightweightGraphicsConstantUpdateEngine::setHsShader(const Gnmx::HsShader* shader, const InputResourceOffsets* table, uint32_t tgPatchCount)
{
	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == Gnm::kShaderStageHs);
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageHs], table);

	m_dirtyShaderResources[Gnm::kShaderStageHs] = true;
	m_dirtyShader[Gnm::kShaderStageHs] |= (m_boundShader[Gnm::kShaderStageHs] != shader);

	Gnm::TessellationRegisters tessRegs;
	if(shader)
	{
		tessRegs.init(&shader->m_hullStateConstants, tgPatchCount);
	}

#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
	// Note: Because the DCB binding of the shader is deferred until preDraw(), we must first check to verify the shader has at least been bound with its context registers in preDraw()
	Gnm::TessellationDistributionMode currentDistributionMode = static_cast<Gnm::TessellationDistributionMode>(m_tessellationCurrentDistributionMode & LightweightConstantUpdateEngine::kTesselationDistrbutionMask);
	m_dirtyShaderUpdateOnly[Gnm::kShaderStageHs] = (m_updateShaderBindingIsValid & (1 << Gnm::kShaderStageHs)) != 0 && m_dirtyShader[Gnm::kShaderStageHs] && m_boundShader[Gnm::kShaderStageHs] 
													&& shader->m_hsStageRegisters.isSharingContext(((Gnmx::HsShader*)m_boundShader[Gnm::kShaderStageHs])->m_hsStageRegisters) 
													&& m_tessellationCurrentRegs.m_vgtLsHsConfig == tessRegs.m_vgtLsHsConfig
													&& currentDistributionMode == Gnm::kTessellationDistributionNone;

	// Avoid marking shader as updateable unless preDraw() is called again, necessary to work around redundant shader bindings
	m_updateShaderBindingIsValid &= ~(1 << Gnm::kShaderStageHs);
	m_tessellationCurrentRegs = tessRegs;
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER

	m_shaderBindingIsValid |= (1 << Gnm::kShaderStageHs);
	m_boundShaderResourceOffsets[Gnm::kShaderStageHs] = table;
	m_boundShader[Gnm::kShaderStageHs] = shader;
	m_tessellationCurrentTgPatchCount = tgPatchCount;

	// force kTessellationDistributionNone and clear tessellationCurrentDistributionMode enable bit
	m_tessellationCurrentDistributionMode = Gnm::kTessellationDistributionNone;
}

void LightweightGraphicsConstantUpdateEngine::setHsShader(const Gnmx::HsShader* shader, const InputResourceOffsets* table, uint32_t tgPatchCount, Gnm::TessellationDistributionMode distributionMode)
{
	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == Gnm::kShaderStageHs);
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageHs], table);

	m_dirtyShaderResources[Gnm::kShaderStageHs] = true;
	m_dirtyShader[Gnm::kShaderStageHs] |= (m_boundShader[Gnm::kShaderStageHs] != shader);

	Gnm::TessellationRegisters tessRegs;
	if (shader)
	{
		tessRegs.init(&shader->m_hullStateConstants, tgPatchCount);
	}

#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
	// Note: Because the DCB binding of the shader is deferred until preDraw(), we must first check to verify the shader has at least been bound with its context registers in preDraw()
	Gnm::TessellationDistributionMode currentDistributionMode = static_cast<Gnm::TessellationDistributionMode>(m_tessellationCurrentDistributionMode & LightweightConstantUpdateEngine::kTesselationDistrbutionMask);
	m_dirtyShaderUpdateOnly[Gnm::kShaderStageHs] = (m_updateShaderBindingIsValid & (1 << Gnm::kShaderStageHs)) != 0 && m_dirtyShader[Gnm::kShaderStageHs] && m_boundShader[Gnm::kShaderStageHs]
													&& shader->m_hsStageRegisters.isSharingContext(((Gnmx::HsShader*)m_boundShader[Gnm::kShaderStageHs])->m_hsStageRegisters)
													&& m_tessellationCurrentRegs.m_vgtLsHsConfig == tessRegs.m_vgtLsHsConfig
													&& currentDistributionMode == distributionMode;

	// Avoid marking shader as updateable unless preDraw() is called again, necessary to work around redundant shader bindings
	m_updateShaderBindingIsValid &= ~(1 << Gnm::kShaderStageHs);
	m_tessellationCurrentRegs = tessRegs;
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER

	m_shaderBindingIsValid |= (1 << Gnm::kShaderStageHs);
	m_boundShaderResourceOffsets[Gnm::kShaderStageHs] = table;
	m_boundShader[Gnm::kShaderStageHs] = shader;
	m_tessellationCurrentTgPatchCount = tgPatchCount;

	// set TessellationDistributionMode and set tessellationCurrentDistributionMode bit to enabled
	m_tessellationCurrentDistributionMode = (LightweightConstantUpdateEngine::kTesselationDistrbutionEnabledMask | distributionMode);
}

void LightweightGraphicsConstantUpdateEngine::setVsShader(const Gnmx::VsShader* shader, uint32_t shaderModifier, const void* fetchShader, const InputResourceOffsets* table)
{
	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == Gnm::kShaderStageVs);
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageVs], table);

	m_dirtyShaderResources[Gnm::kShaderStageVs] = true;
	m_dirtyShader[Gnm::kShaderStageVs] |= (m_boundShader[Gnm::kShaderStageVs] != shader);

#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
	// Note: Because the DCB binding of the shader is deferred until preDraw(), we must first check to verify the shader has at least been bound with its context registers in preDraw()
	m_dirtyShaderUpdateOnly[Gnm::kShaderStageVs] = (m_updateShaderBindingIsValid & (1 << Gnm::kShaderStageVs)) != 0 && m_dirtyShader[Gnm::kShaderStageVs] && m_boundShader[Gnm::kShaderStageVs]  
												   && shader->m_vsStageRegisters.isSharingContext(((Gnmx::VsShader*)m_boundShader[Gnm::kShaderStageVs])->m_vsStageRegisters);

	// Avoid marking shader as updateable unless preDraw() is called again, necessary to work around redundant shader bindings
	m_updateShaderBindingIsValid &= ~(1 << Gnm::kShaderStageVs);
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER

	m_shaderBindingIsValid |= (1 << Gnm::kShaderStageVs);
	m_boundShaderResourceOffsets[Gnm::kShaderStageVs] = table;
	m_boundShader[Gnm::kShaderStageVs] = shader;
	m_boundFetchShader[Gnm::kShaderStageVs] = fetchShader;
	m_boundShaderModifier[Gnm::kShaderStageVs] = shaderModifier;

	m_dispatchDrawActive = (table->dispatchDrawPtrSgpr != 0xFF);
	m_psInputsTable = NULL;
}


void LightweightGraphicsConstantUpdateEngine::setVsFetchShader(uint32_t shaderModifier, const void* fetchShader)
{
	m_boundShaderModifier[Gnm::kShaderStageVs] = shaderModifier;
	m_boundFetchShader[Gnm::kShaderStageVs] = fetchShader;
}


void LightweightGraphicsConstantUpdateEngine::setPsShader(const Gnmx::PsShader* shader, const InputResourceOffsets* table)
{
	SCE_GNM_ASSERT((shader == NULL && table == NULL) || (shader != NULL && table != NULL));
	SCE_GNM_ASSERT(table == NULL || table->shaderStage == Gnm::kShaderStagePs);
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStagePs], table);

	// Special case: if the Pixel Shader is NULL we don't mark shaderResourceOffsets as dirty to prevent flushing the scratch buffer
	m_dirtyShaderResources[Gnm::kShaderStagePs] = (shader != NULL); 
	m_dirtyShader[Gnm::kShaderStagePs] |= (m_boundShader[Gnm::kShaderStagePs] != shader);

#if SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
	// Note: Because the DCB binding of the shader is deferred until preDraw(), we must first check to verify the shader has at least been bound with its context registers in preDraw()
	m_dirtyShaderUpdateOnly[Gnm::kShaderStagePs] = (m_updateShaderBindingIsValid & (1 << Gnm::kShaderStagePs)) != 0 && m_dirtyShader[Gnm::kShaderStagePs] && m_boundShader[Gnm::kShaderStagePs] 
													&& shader && shader->m_psStageRegisters.isSharingContext(((Gnmx::PsShader*)m_boundShader[Gnm::kShaderStagePs])->m_psStageRegisters);

	// Avoid marking shader as updateable unless preDraw() is called again, necessary to work around redundant shader bindings
	m_updateShaderBindingIsValid &= ~(1 << Gnm::kShaderStagePs);
#endif // SCE_GNM_LCUE_ENABLE_UPDATE_SHADER

	m_shaderBindingIsValid |= (1 << Gnm::kShaderStagePs);
	m_boundShaderResourceOffsets[Gnm::kShaderStagePs] = table;
	m_boundShader[Gnm::kShaderStagePs] = shader;
	m_psInputsTable = NULL;
}


void LightweightGraphicsConstantUpdateEngine::setCsShader(const Gnmx::CsShader* shader, const InputResourceOffsets* table)
{
	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == Gnm::kShaderStageCs);
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[Gnm::kShaderStageCs], table);

	m_dirtyShaderResources[Gnm::kShaderStageCs] = true;
	m_dirtyShader[Gnm::kShaderStageCs] |= (m_boundShader[Gnm::kShaderStageCs] != shader);

	m_shaderBindingIsValid |= (1 << Gnm::kShaderStageCs);
	m_boundShaderResourceOffsets[Gnm::kShaderStageCs] = table;
	m_boundShader[Gnm::kShaderStageCs] = shader;

	m_gfxCsShaderOrderedAppendMode = shader->m_orderedAppendMode;
}


void LightweightGraphicsConstantUpdateEngine::setCsVsShaders(const Gnmx::CsVsShader *csvsb, uint32_t shaderModifierVs, void *fetchShaderAddrVs, const InputResourceOffsets* VsTable,
																							uint32_t shaderModifierCs, void *fetchShaderAddrCs, const InputResourceOffsets* CsTable)
{
#if	!SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	SCE_GNM_ASSERT_MSG(0, "Dispatch draw has been disabled, please recompile Gnmx with SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW (can be found in Gnmx/config.h)");
#endif // !SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW

	const Gnmx::CsShader *csShader = csvsb->getComputeShader();
	const Gnmx::VsShader *vsShader = csvsb->getVertexShader();

	setAsynchronousComputeShader(csShader, shaderModifierCs, fetchShaderAddrCs, CsTable);
	setVsShader(vsShader, shaderModifierVs, fetchShaderAddrVs, VsTable);
}


void LightweightGraphicsConstantUpdateEngine::setAsynchronousComputeShader(const Gnmx::CsShader *shader, uint32_t shaderModifierCs, void *fetchShaderAddrCs, const InputResourceOffsets* table)
{
#if	!SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	SCE_GNM_ASSERT_MSG(0, "Dispatch draw has been disabled, please recompile Gnmx with SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW (can be found in Gnmx/config.h)");
#endif // !SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW

	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute);
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute], table);

	m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = true;
	m_dirtyShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] |= (m_boundShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != shader);

	m_shaderBindingIsValid |= (1 << LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute);
	m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = table;
	m_boundShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = shader;
	m_boundFetchShader[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = fetchShaderAddrCs;
	m_boundShaderModifier[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = shaderModifierCs;

	m_dispatchDrawIndexDeallocNumBits = shader->m_dispatchDrawIndexDeallocNumBits;
	m_asyncCsShaderOrderedAppendMode = shader->m_orderedAppendMode;

	// Handle embedded constant buffer
	SCE_GNM_ASSERT_MSG( shader->m_common.m_embeddedConstantBufferSizeInDQW == 0, "asynchronous compute shaders do not support embedded constant buffers");
}


void LightweightGraphicsConstantUpdateEngine::setAppendConsumeCounterRange(Gnm::ShaderStage shaderStage, uint32_t gdsMemoryBaseInBytes, uint32_t countersSizeInBytes)
{
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount && m_boundShaderResourceOffsets[(int32_t)shaderStage] != NULL);
	SCE_GNM_ASSERT((gdsMemoryBaseInBytes%4)==0 && gdsMemoryBaseInBytes < UINT16_MAX && (countersSizeInBytes%4)==0 && countersSizeInBytes < UINT16_MAX);
	
	SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets[(int32_t)shaderStage]->appendConsumeCounterSgpr != 0xFF);
	if (m_boundShaderResourceOffsets[(int32_t)shaderStage]->appendConsumeCounterSgpr != 0xFF)
	{
		SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[shaderStage].appendConsumeCounterIsBound);
		m_boundShaderAppendConsumeCounterRange[shaderStage] = (gdsMemoryBaseInBytes << 16) | countersSizeInBytes;
	}
#if	SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	if (m_dispatchDrawActive && shaderStage == Gnm::ShaderStage::kShaderStageVs) // any immediate data for dispatch draw
	{
		SCE_GNM_ASSERT(m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
		const InputResourceOffsets* Cstable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
		if (Cstable->appendConsumeCounterSgpr != 0xFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute].appendConsumeCounterIsBound);
			m_boundShaderAppendConsumeCounterRange[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = (gdsMemoryBaseInBytes << 16) | countersSizeInBytes;
			m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = true;
		}
	}
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	m_dirtyShaderResources[(int32_t)shaderStage] = true;
}

void LightweightGraphicsConstantUpdateEngine::setGdsMemoryRange(Gnm::ShaderStage shaderStage, uint32_t baseOffsetInBytes, uint32_t rangeSizeInBytes)
{
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount && m_boundShaderResourceOffsets[(int32_t)shaderStage] != NULL);

	SCE_GNM_ASSERT(((baseOffsetInBytes | rangeSizeInBytes)&3) == 0);
	SCE_GNM_ASSERT(baseOffsetInBytes < Gnm::kGdsAccessibleMemorySizeInBytes);
	SCE_GNM_ASSERT(rangeSizeInBytes <= Gnm::kGdsAccessibleMemorySizeInBytes);
	SCE_GNM_ASSERT((baseOffsetInBytes+rangeSizeInBytes) <= Gnm::kGdsAccessibleMemorySizeInBytes);

	SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets[(int32_t)shaderStage]->gdsMemoryRangeSgpr != 0xFF);
	if (m_boundShaderResourceOffsets[(int32_t)shaderStage]->gdsMemoryRangeSgpr != 0xFF)
	{
		SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[shaderStage].gdsMemoryRangeIsBound);
		m_boundShaderGdsMemoryRange[shaderStage] = (baseOffsetInBytes << 16) | rangeSizeInBytes;
	}
#if	SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	if (m_dispatchDrawActive && shaderStage == Gnm::ShaderStage::kShaderStageVs) // any immediate data for dispatch draw
	{
		SCE_GNM_ASSERT(m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
		const InputResourceOffsets* Cstable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
		if (Cstable->gdsMemoryRangeSgpr != 0xFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute].gdsMemoryRangeIsBound);
			m_boundShaderGdsMemoryRange[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = (baseOffsetInBytes << 16) | rangeSizeInBytes;
			m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = true;
		}
	}
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	m_dirtyShaderResources[(int32_t)shaderStage] = true;
}


void LightweightGraphicsConstantUpdateEngine::setConstantBuffers(Gnm::ShaderStage shaderStage, uint32_t startApiSlot, uint32_t apiSlotCount, const Gnm::Buffer* buffer)
{
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)shaderStage] != NULL && buffer != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->constBufferSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->constBufferSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->constBufferSlotCount);

	for (uint32_t i=0; i<apiSlotCount; i++)
	{
		uint32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_CONSTANT_BUFFER((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, buffer+i);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(table->constBufferDwOffset[currentApiSlot] != 0xFFFF);
		if (table->constBufferDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[shaderStage].constBufferOffsetIsBound[currentApiSlot]);
			setGfxPipeDataInUserDataSgprOrMemory(m_drawCommandBuffer, m_scratchBuffer, shaderStage, table->constBufferDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
		}
#if	SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
		if (m_dispatchDrawActive && shaderStage == Gnm::ShaderStage::kShaderStageVs) // any immediate data for dispatch draw
		{
			SCE_GNM_ASSERT(m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
			const InputResourceOffsets* Cstable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
			if (Cstable->constBufferDwOffset[currentApiSlot] != 0xFFFF)
			{
				SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute].constBufferOffsetIsBound[currentApiSlot]);
				setDispatchDrawDataInUserDataSgpr(m_asyncDispatchCommandBuffer, Cstable->constBufferDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
				m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = true;
			}
		}
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	}
	m_dirtyShaderResources[(int32_t)shaderStage] = true;
}


void LightweightGraphicsConstantUpdateEngine::setVertexBuffers(Gnm::ShaderStage shaderStage, uint32_t startApiSlot, uint32_t apiSlotCount, const Gnm::Buffer* buffer)
{
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)shaderStage] != NULL && buffer != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->vertexBufferSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->vertexBufferSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->vertexBufferSlotCount);

	for (uint32_t i=0; i<apiSlotCount; ++i)
	{
		uint32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_VERTEX_BUFFER((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, buffer+i);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(table->vertexBufferDwOffset[currentApiSlot] != 0xFFFF);
		if (table->vertexBufferDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[shaderStage].vertexBufferOffsetIsBound[currentApiSlot]);
			setGfxPipeDataInUserDataSgprOrMemory(m_drawCommandBuffer, m_scratchBuffer, shaderStage, table->vertexBufferDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
		}
#if	SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
		if (m_dispatchDrawActive && shaderStage == Gnm::ShaderStage::kShaderStageVs) // any immediate data for dispatch draw
		{
			SCE_GNM_ASSERT(m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
			const InputResourceOffsets* Cstable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
			if (Cstable->vertexBufferDwOffset[currentApiSlot] != 0xFFFF)
			{
				SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute].vertexBufferOffsetIsBound[currentApiSlot]);
				setDispatchDrawDataInUserDataSgpr(m_asyncDispatchCommandBuffer, Cstable->vertexBufferDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
				m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = true;
			}
		}
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	}
	m_dirtyShaderResources[(int32_t)shaderStage] = true;
}


void LightweightGraphicsConstantUpdateEngine::setBuffers(Gnm::ShaderStage shaderStage, uint32_t startApiSlot, uint32_t apiSlotCount, const Gnm::Buffer* buffer)
{
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)shaderStage] != NULL && buffer != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->resourceSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->resourceSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->resourceSlotCount);

	for (uint32_t i=0; i<apiSlotCount; i++)
	{
		uint32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_BUFFER((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, buffer+i);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(table->resourceDwOffset[currentApiSlot] != 0xFFFF);
		if (table->resourceDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[shaderStage].resourceOffsetIsBound[currentApiSlot]);
			setGfxPipeDataInUserDataSgprOrMemory(m_drawCommandBuffer, m_scratchBuffer, shaderStage, table->resourceDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
		}
#if	SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
		if (m_dispatchDrawActive && shaderStage == Gnm::ShaderStage::kShaderStageVs) // any immediate data for dispatch draw
		{
			SCE_GNM_ASSERT(m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
			const InputResourceOffsets* Cstable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
			if (Cstable->resourceDwOffset[currentApiSlot] != 0xFFFF)
			{
				SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute].resourceOffsetIsBound[currentApiSlot]);
				setDispatchDrawDataInUserDataSgpr(m_asyncDispatchCommandBuffer, Cstable->resourceDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
				m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = true;
			}
		}
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	}
	m_dirtyShaderResources[(int32_t)shaderStage] = true;
}


void LightweightGraphicsConstantUpdateEngine::setRwBuffers(Gnm::ShaderStage shaderStage, uint32_t startApiSlot, uint32_t apiSlotCount, const Gnm::Buffer* buffer)
{
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)shaderStage] != NULL && buffer != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->rwResourceSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->rwResourceSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->rwResourceSlotCount);

	for (uint32_t i=0; i<apiSlotCount; i++)
	{
		SCE_GNM_ASSERT((buffer+i)->getResourceMemoryType() != Gnm::kResourceMemoryTypeRO);

		uint32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_RWBUFFER((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, buffer+i);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(table->rwResourceDwOffset[currentApiSlot] != 0xFFFF);
		if (table->rwResourceDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[shaderStage].rwResourceOffsetIsBound[currentApiSlot]);
			setGfxPipeDataInUserDataSgprOrMemory(m_drawCommandBuffer, m_scratchBuffer, shaderStage, table->rwResourceDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
		}
#if	SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
		if (m_dispatchDrawActive && shaderStage == Gnm::ShaderStage::kShaderStageVs) // any immediate data for dispatch draw
		{
			SCE_GNM_ASSERT(m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
			const InputResourceOffsets* Cstable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
			if (Cstable->rwResourceDwOffset[currentApiSlot] != 0xFFFF)
			{
				SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute].rwResourceOffsetIsBound[currentApiSlot]);
				setDispatchDrawDataInUserDataSgpr(m_asyncDispatchCommandBuffer, Cstable->rwResourceDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
				m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = true;
			}
		}
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	}
	m_dirtyShaderResources[(int32_t)shaderStage] = true;
}


void LightweightGraphicsConstantUpdateEngine::setTextures(Gnm::ShaderStage shaderStage, uint32_t startApiSlot, uint32_t apiSlotCount, const Gnm::Texture* texture)
{
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)shaderStage] != NULL && texture != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->resourceSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->resourceSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->resourceSlotCount);

	for (uint32_t i=0; i<apiSlotCount; i++)
	{
		uint32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_TEXTURE((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, texture+i);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(table->resourceDwOffset[currentApiSlot] != 0xFFFF);
		if (table->resourceDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[shaderStage].resourceOffsetIsBound[currentApiSlot]);
			setGfxPipeDataInUserDataSgprOrMemory(m_drawCommandBuffer, m_scratchBuffer, shaderStage, table->resourceDwOffset[currentApiSlot], texture + i, sizeof(Gnm::Texture));
		}
#if	SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
		if (m_dispatchDrawActive && shaderStage == Gnm::ShaderStage::kShaderStageVs) // any immediate data for dispatch draw
		{
			SCE_GNM_ASSERT(m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
			const InputResourceOffsets* Cstable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
			if (Cstable->resourceDwOffset[currentApiSlot] != 0xFFFF)
			{
				SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute].resourceOffsetIsBound[currentApiSlot]);
				setDispatchDrawDataInUserDataSgpr(m_asyncDispatchCommandBuffer, Cstable->resourceDwOffset[currentApiSlot], texture + i, sizeof(Gnm::Texture));
				m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = true;
			}
		}
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	}
	m_dirtyShaderResources[(int32_t)shaderStage] = true;
}


void LightweightGraphicsConstantUpdateEngine::setRwTextures(Gnm::ShaderStage shaderStage, uint32_t startApiSlot, uint32_t apiSlotCount, const Gnm::Texture* texture)
{
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)shaderStage] != NULL && texture != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->rwResourceSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->rwResourceSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->rwResourceSlotCount);

	for (uint32_t i=0; i<apiSlotCount; i++)
	{
		SCE_GNM_ASSERT((texture+i)->getResourceMemoryType() != Gnm::kResourceMemoryTypeRO);

		uint32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_RWTEXTURE((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, texture+i);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(table->rwResourceDwOffset[currentApiSlot] != 0xFFFF);
		if (table->rwResourceDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[shaderStage].rwResourceOffsetIsBound[currentApiSlot]);
			setGfxPipeDataInUserDataSgprOrMemory(m_drawCommandBuffer, m_scratchBuffer, shaderStage, table->rwResourceDwOffset[currentApiSlot], texture + i, sizeof(Gnm::Texture));
		}
#if	SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
		if (m_dispatchDrawActive && shaderStage == Gnm::ShaderStage::kShaderStageVs) // any immediate data for dispatch draw
		{
			SCE_GNM_ASSERT(m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
			const InputResourceOffsets* Cstable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
			if (Cstable->rwResourceDwOffset[currentApiSlot] != 0xFFFF)
			{
				SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute].rwResourceOffsetIsBound[currentApiSlot]);
				setDispatchDrawDataInUserDataSgpr(m_asyncDispatchCommandBuffer, Cstable->rwResourceDwOffset[currentApiSlot], texture + i, sizeof(Gnm::Texture));
				m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = true;
			}
		}
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	}
	m_dirtyShaderResources[(int32_t)shaderStage] = true;
}


void LightweightGraphicsConstantUpdateEngine::setSamplers(Gnm::ShaderStage shaderStage, uint32_t startApiSlot, uint32_t apiSlotCount, const Gnm::Sampler* sampler)
{
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)shaderStage] != NULL && sampler != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->samplerSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->samplerSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->samplerSlotCount);

	for (uint32_t i=0; i<apiSlotCount; i++)
	{
		uint32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_SAMPLER((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, sampler+i);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(table->samplerDwOffset[currentApiSlot] != 0xFFFF);
		if (table->samplerDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[shaderStage].samplerOffsetIsBound[currentApiSlot]);
			setGfxPipeDataInUserDataSgprOrMemory(m_drawCommandBuffer, m_scratchBuffer, shaderStage, table->samplerDwOffset[currentApiSlot], sampler + i, sizeof(Gnm::Sampler));
		}
#if	SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
		if (m_dispatchDrawActive && shaderStage == Gnm::ShaderStage::kShaderStageVs) // any immediate data for dispatch draw
		{
			SCE_GNM_ASSERT(m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
			const InputResourceOffsets* Cstable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
			if (Cstable->samplerDwOffset[currentApiSlot] != 0xFFFF)
			{
				SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute].samplerOffsetIsBound[currentApiSlot]);
				setDispatchDrawDataInUserDataSgpr(m_asyncDispatchCommandBuffer, Cstable->samplerDwOffset[currentApiSlot], sampler + i, sizeof(Gnm::Sampler));
				m_dirtyShaderResources[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] = true;
			}
		}
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	}
	m_dirtyShaderResources[(int32_t)shaderStage] = true;
}


void LightweightGraphicsConstantUpdateEngine::setUserData(sce::Gnm::ShaderStage shaderStage, uint32_t startSgpr, uint32_t sgprCount, const uint32_t* data)
{
	SCE_GNM_ASSERT(startSgpr >= 0 && startSgpr < LightweightConstantUpdateEngine::kMaxUserDataCount);
	SCE_GNM_ASSERT(sgprCount >= 0 && sgprCount <= LightweightConstantUpdateEngine::kMaxUserDataCount);
	SCE_GNM_ASSERT(startSgpr+sgprCount <= LightweightConstantUpdateEngine::kMaxUserDataCount);
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount);
	setGfxPipePersistentRegisterRange(m_drawCommandBuffer, shaderStage, startSgpr, data, sgprCount);
}


void LightweightGraphicsConstantUpdateEngine::setPtrInUserData(sce::Gnm::ShaderStage shaderStage, uint32_t startSgpr, const void* gpuAddress)
{
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount && startSgpr >= 0 && (startSgpr) <= LightweightConstantUpdateEngine::kMaxUserDataCount);
	setGfxPipePtrInPersistentRegister(m_drawCommandBuffer, shaderStage, startSgpr, gpuAddress);
}


void LightweightGraphicsConstantUpdateEngine::setUserSrtBuffer(sce::Gnm::ShaderStage shaderStage, const void* buffer, uint32_t sizeInDwords)
{
	SCE_GNM_UNUSED(sizeInDwords);
	SCE_GNM_ASSERT(shaderStage < Gnm::kShaderStageCount);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets[(int32_t)shaderStage] != NULL && buffer != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets[(int32_t)shaderStage];
	SCE_GNM_ASSERT(sizeInDwords >= table->userSrtDataCount && sizeInDwords <= LightweightConstantUpdateEngine::kMaxSrtUserDataCount);
	setUserData(shaderStage, table->userSrtDataSgpr, table->userSrtDataCount, (const uint32_t *)buffer);

#if	SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
	if (m_dispatchDrawActive && shaderStage == Gnm::ShaderStage::kShaderStageVs)
	{
		SCE_GNM_ASSERT(m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute] != NULL);
		const InputResourceOffsets* Cstable = m_boundShaderResourceOffsets[LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute];
		SCE_GNM_ASSERT(sizeInDwords >= Cstable->userSrtDataCount);
		setDispatchDrawPersistentRegisterRange(m_asyncDispatchCommandBuffer, Cstable->userSrtDataSgpr, (const uint32_t *)buffer, Cstable->userSrtDataCount);
	}
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
}


void LightweightGraphicsConstantUpdateEngine::setVertexAndInstanceOffset(uint32_t vertexOffset, uint32_t instanceOffset)
{
	Gnm::ShaderStage fetchShaderStage = Gnm::kShaderStageCount; // initialize with an invalid value
	uint32_t vertexOffsetUserSlot = 0;
	uint32_t instanceOffsetUserSlot = 0;

	// find where the fetch shader is 
	switch(m_activeShaderStages)
	{
		case Gnm::kActiveShaderStagesVsPs:
		{
			SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageVs] != NULL);
			const Gnmx::VsShader *pVS = (const Gnmx::VsShader*)m_boundShader[Gnm::kShaderStageVs]; 

			fetchShaderStage= Gnm::kShaderStageVs;
			vertexOffsetUserSlot = pVS->getVertexOffsetUserRegister();
			instanceOffsetUserSlot = pVS->getInstanceOffsetUserRegister();
			break;
		}
		case Gnm::kActiveShaderStagesEsGsVsPs:
		{
			SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageEs] != NULL);
			const Gnmx::EsShader *pES = (const Gnmx::EsShader*)m_boundShader[Gnm::kShaderStageEs]; 

			fetchShaderStage = Gnm::kShaderStageEs;
			vertexOffsetUserSlot = pES->getVertexOffsetUserRegister();
			instanceOffsetUserSlot = pES->getInstanceOffsetUserRegister();
			break;
		}
		case Gnm::kActiveShaderStagesLsHsVsPs:
		case Gnm::kActiveShaderStagesLsHsEsGsVsPs:
		case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
		case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
		{
			SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageLs] != NULL);
			const Gnmx::LsShader *pLS = (const Gnmx::LsShader*)m_boundShader[Gnm::kShaderStageLs]; 

			fetchShaderStage = Gnm::kShaderStageLs;
			vertexOffsetUserSlot = pLS->getVertexOffsetUserRegister();
			instanceOffsetUserSlot = pLS->getInstanceOffsetUserRegister();
			break;
		}
		case Gnm::kActiveShaderStagesDispatchDrawVsPs:
			SCE_GNM_ERROR("Current shader configuration does not support vertex/instance offset");
	}

	if (fetchShaderStage != Gnm::kShaderStageCount)
	{
		if( vertexOffsetUserSlot )
			m_drawCommandBuffer->setUserData(fetchShaderStage,vertexOffsetUserSlot,vertexOffset);
		if( instanceOffsetUserSlot )
			m_drawCommandBuffer->setUserData(fetchShaderStage,instanceOffsetUserSlot, instanceOffset);
	}
}


bool LightweightGraphicsConstantUpdateEngine::isVertexOrInstanceOffsetEnabled() const
{
	uint32_t vertexOffsetUserSlot = 0;
	uint32_t instanceOffsetUserSlot = 0;

	if ((m_shaderBindingIsValid & (1 << Gnm::kShaderStageVs)) == 0)
		return false;

	// find where the fetch shader is 
	switch(m_activeShaderStages)
	{
		case Gnm::kActiveShaderStagesVsPs:
		{
			SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageVs] != NULL);
			const Gnmx::VsShader *pVS = (const Gnmx::VsShader*)m_boundShader[Gnm::kShaderStageVs]; 

			vertexOffsetUserSlot = pVS->getVertexOffsetUserRegister();
			instanceOffsetUserSlot = pVS->getInstanceOffsetUserRegister();
			break;
		}
		case Gnm::kActiveShaderStagesEsGsVsPs:
		{
			SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageEs] != NULL);
			const Gnmx::EsShader *pES = (const Gnmx::EsShader*)m_boundShader[Gnm::kShaderStageEs]; 

			vertexOffsetUserSlot = pES->getVertexOffsetUserRegister();
			instanceOffsetUserSlot = pES->getInstanceOffsetUserRegister();
			break;
		}
		case Gnm::kActiveShaderStagesLsHsVsPs:
		case Gnm::kActiveShaderStagesLsHsEsGsVsPs:
		case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
		case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
		{
			SCE_GNM_ASSERT(m_boundShader[Gnm::kShaderStageLs] != NULL);
			const Gnmx::LsShader *pLS = (const Gnmx::LsShader*)m_boundShader[Gnm::kShaderStageLs]; 

			vertexOffsetUserSlot = pLS->getVertexOffsetUserRegister();
			instanceOffsetUserSlot = pLS->getInstanceOffsetUserRegister();
			break;
		}
		case Gnm::kActiveShaderStagesDispatchDrawVsPs:
			return false;
	}

	return vertexOffsetUserSlot || instanceOffsetUserSlot;
}


#endif // defined(__ORBIS__)
