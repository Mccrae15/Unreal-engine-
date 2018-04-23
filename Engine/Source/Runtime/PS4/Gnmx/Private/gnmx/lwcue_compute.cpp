/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if defined(__ORBIS__) // LCUE isn't supported offline

#include <gnmx/lwcue_compute.h>

using namespace sce;
using namespace Gnmx;

static SCE_GNM_FORCE_INLINE void setAsycPipePersistentRegisterRange(GnmxDispatchCommandBuffer* dcb, uint32_t startSgpr, const uint32_t* values, uint32_t valuesCount)
{
	dcb->setUserDataRegion(startSgpr, values, valuesCount);
}


static SCE_GNM_FORCE_INLINE void setAsycPipePtrInPersistentRegister(GnmxDispatchCommandBuffer* dcb, uint32_t startSgpr, const void* address)
{
	void* gpuAddress = (void*)address;
	dcb->setPointerInUserData(startSgpr, gpuAddress);
}


static SCE_GNM_FORCE_INLINE void setAsycPipeDataInUserDataSgprOrMemory(GnmxDispatchCommandBuffer* dcb, uint32_t* scratchBuffer, uint16_t shaderResourceOffset, const void* restrict data, uint32_t dataSizeInBytes)
{
	SCE_GNM_ASSERT(dcb != NULL);
	SCE_GNM_ASSERT(data != NULL && dataSizeInBytes > 0 && (dataSizeInBytes%4) == 0);

	uint32_t userDataRegisterOrMemoryOffset = (shaderResourceOffset & LightweightConstantUpdateEngine::kResourceValueMask);
	if ((shaderResourceOffset & LightweightConstantUpdateEngine::kResourceInUserDataSgpr) != 0)
	{
		setAsycPipePersistentRegisterRange(dcb, userDataRegisterOrMemoryOffset, (uint32_t*)data, dataSizeInBytes / sizeof(uint32_t));
	}
	else
	{
		uint32_t* restrict scratchDestAddress = (uint32_t*)(scratchBuffer + ((int)Gnm::kShaderStageCs * LightweightConstantUpdateEngine::kGpuStageBufferSizeInDwords) + userDataRegisterOrMemoryOffset);
		__builtin_memcpy(scratchDestAddress, data, dataSizeInBytes);
	}
}


#if SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
static bool isResourceBindingComplete(const LightweightConstantUpdateEngine::ShaderResourceBindingValidation* validationTable, const InputResourceOffsets* table)
{
	bool isValid = true;
	SCE_GNM_ASSERT(table != NULL);
	if (validationTable != NULL)
	{
		int32_t i=0; // Failed resource index
		while (isValid && i<table->resourceSlotCount)		{ isValid &= validationTable->resourceOffsetIsBound[i]; ++i; } --i;		SCE_GNM_ASSERT_MSG(isValid, "[kShaderStageCs] Resource at slot %d, was not bound", i); i = 0;		// Resource not bound
		while (isValid && i<table->rwResourceSlotCount)		{ isValid &= validationTable->rwResourceOffsetIsBound[i]; ++i; } --i;	SCE_GNM_ASSERT_MSG(isValid, "[kShaderStageCs] RW resource at slot %d, was not bound", i); i = 0;	// RW-resource not bound
		while (isValid && i<table->samplerSlotCount)		{ isValid &= validationTable->samplerOffsetIsBound[i]; ++i; } --i;		SCE_GNM_ASSERT_MSG(isValid, "[kShaderStageCs] Sampler at slot %d, was not bound", i); i = 0;		// Sampler not bound
		while (isValid && i<table->constBufferSlotCount)	{ isValid &= validationTable->constBufferOffsetIsBound[i]; ++i; } --i;	SCE_GNM_ASSERT_MSG(isValid, "[kShaderStageCs] Constant buffer slot %d, was not bound", i); i = 0;	// Constant buffer not bound
		isValid &= validationTable->appendConsumeCounterIsBound;																	SCE_GNM_ASSERT_MSG(isValid, "[kShaderStageCs] AppendConsumeCounter not bound");						// AppendConsumeCounter not bound
		isValid &= validationTable->gdsMemoryRangeIsBound;																			SCE_GNM_ASSERT_MSG(isValid, "[kShaderStageCs] gdsMemoryRange not bound");							// gdsMemoryRange not bound

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
		for (int32_t i=0; i<table->resourceSlotCount;++i)		validationTable->resourceOffsetIsBound[i]		= (table->resourceDwOffset[i] == 0xFFFF);
		for (int32_t i=0; i<table->rwResourceSlotCount;	++i)	validationTable->rwResourceOffsetIsBound[i]		= (table->rwResourceDwOffset[i] == 0xFFFF);
		for (int32_t i=0; i<table->samplerSlotCount; ++i)		validationTable->samplerOffsetIsBound[i]		= (table->samplerDwOffset[i] == 0xFFFF);
		for (int32_t i=0; i<table->constBufferSlotCount; ++i)	validationTable->constBufferOffsetIsBound[i]	= (table->constBufferDwOffset[i] == 0xFFFF);
		validationTable->appendConsumeCounterIsBound = (table->appendConsumeCounterSgpr == 0xFF);
		validationTable->gdsMemoryRangeIsBound = (table->gdsMemoryRangeSgpr == 0xFF);
	}
	else
	{
		// If there's no table, all resources are valid
		SCE_GNM_ASSERT( sizeof(bool) == sizeof(unsigned char) );
		__builtin_memset(validationTable, true, sizeof(LightweightConstantUpdateEngine::ShaderResourceBindingValidation));
	}
}
#endif	// SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED


void ComputeConstantUpdateEngine::init(uint32_t** resourceBuffersInGarlic, int32_t resourceBufferCount, uint32_t resourceBufferSizeInDwords, void* globalInternalResourceTableAddr)
{
	BaseConstantUpdateEngine::init(resourceBuffersInGarlic, resourceBufferCount, resourceBufferSizeInDwords, globalInternalResourceTableAddr);
	__builtin_memset(m_scratchBuffer, 0, sizeof(uint32_t) * LightweightConstantUpdateEngine::kComputeScratchBufferSizeInDwords);

	m_prefetchShaderCode = false;	// Disabling on Async Compute by default, see techNote #. Can be overridden by setShaderCodePrefetchEnable()
	m_dispatchCommandBuffer = NULL;

	invalidateAllBindings();
}


void ComputeConstantUpdateEngine::swapBuffers()
{
	BaseConstantUpdateEngine::swapBuffers();
	invalidateAllBindings();

#if SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
	// Because swapBuffers() will reuse memory, it becomes necessary to invalidate the KCache/L1/L2, this ensures no stale data will be fetched by the GPU
	if (m_dispatchCommandBuffer->m_endptr > m_dispatchCommandBuffer->m_beginptr)
		m_dispatchCommandBuffer->flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionInvalidateKCache);
#endif // SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
}


void ComputeConstantUpdateEngine::invalidateAllBindings()
{
	m_dirtyShaderResources = false;
	m_dirtyShader = false;
	m_boundShaderResourceOffsets = NULL;
	m_boundShader = NULL;
	m_csShaderOrderedAppendMode = 0;

#if SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
	__builtin_memset(&m_boundShaderResourcesValidation, true, sizeof(LightweightConstantUpdateEngine::ShaderResourceBindingValidation));
#endif // SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
}


SCE_GNM_FORCE_INLINE uint32_t* ComputeConstantUpdateEngine::flushScratchBuffer()
{
	SCE_GNM_ASSERT(m_boundShader != NULL && m_boundShaderResourceOffsets != NULL);

	// Copy scratch data over the main buffer
	const InputResourceOffsets* table = m_boundShaderResourceOffsets;
	uint32_t* __restrict destAddr = reserveResourceBufferSpace(table->requiredBufferSizeInDwords);
	uint32_t* __restrict sourceAddr = m_scratchBuffer;
	__builtin_memcpy(destAddr, sourceAddr, table->requiredBufferSizeInDwords * sizeof(uint32_t));

	return destAddr;
}


SCE_GNM_FORCE_INLINE void ComputeConstantUpdateEngine::updateCommonPtrsInUserDataSgprs(const uint32_t* resourceBufferFlushedAddress)
{
	SCE_GNM_ASSERT(m_boundShader != NULL);
	const InputResourceOffsets* table = m_boundShaderResourceOffsets;

	if (table->userExtendedData1PtrSgpr != 0xFF)
	{
		setAsycPipePtrInPersistentRegister(m_dispatchCommandBuffer, table->userExtendedData1PtrSgpr, resourceBufferFlushedAddress);
	}
	if (table->constBufferPtrSgpr != 0xFF)
	{
		setAsycPipePtrInPersistentRegister(m_dispatchCommandBuffer, table->constBufferPtrSgpr, resourceBufferFlushedAddress + table->constBufferArrayDwOffset);
	}
	if (table->resourcePtrSgpr != 0xFF)
	{
		setAsycPipePtrInPersistentRegister(m_dispatchCommandBuffer, table->resourcePtrSgpr, resourceBufferFlushedAddress + table->resourceArrayDwOffset);
	}
	if (table->rwResourcePtrSgpr != 0xFF)
	{
		setAsycPipePtrInPersistentRegister(m_dispatchCommandBuffer, table->rwResourcePtrSgpr, resourceBufferFlushedAddress + table->rwResourceArrayDwOffset);
	}
	if (table->samplerPtrSgpr != 0xFF)
	{
		setAsycPipePtrInPersistentRegister(m_dispatchCommandBuffer, table->samplerPtrSgpr, resourceBufferFlushedAddress + table->samplerArrayDwOffset);
	}
	if (table->globalInternalPtrSgpr != 0xFF)
	{
		SCE_GNM_ASSERT(m_globalInternalResourceTableAddr != NULL);
		setAsycPipePtrInPersistentRegister(m_dispatchCommandBuffer, table->globalInternalPtrSgpr, m_globalInternalResourceTableAddr);
	}
	if (table->appendConsumeCounterSgpr != 0xFF)
	{
		setAsycPipePersistentRegisterRange(m_dispatchCommandBuffer, table->appendConsumeCounterSgpr, &m_boundShaderAppendConsumeCounterRange, 1);
	}
	if (table->gdsMemoryRangeSgpr != 0xFF)
	{
		setAsycPipePersistentRegisterRange(m_dispatchCommandBuffer, table->gdsMemoryRangeSgpr, &m_boundShaderGdsMemoryRange, 1);
	}
}


/** @brief The Embedded Constant Buffer (ECB) descriptor is automatically generated and bound for shaders that have dependencies
 *	An ECB is generated when global static arrays are accessed through dynamic variables, preventing the compiler from embedding its immediate values in the shader code.
 *  The ECB is stored after the end of the shader code area, and is expected on API-slot 15 (kShaderSlotEmbeddedConstantBuffer).
 */
SCE_GNM_FORCE_INLINE void ComputeConstantUpdateEngine::updateEmbeddedCb(const Gnmx::ShaderCommonData* shaderCommon)
{
#if SCE_GNMX_TRACK_EMBEDDED_CB
	const InputResourceOffsets* table = m_boundShaderResourceOffsets;
	if (shaderCommon != NULL && table->constBufferDwOffset[LightweightConstantUpdateEngine::kConstantBufferInternalApiSlotForEmbeddedData] != 0xFFFF)
	{
		SCE_GNM_ASSERT(shaderCommon->m_embeddedConstantBufferSizeInDQW > 0);

		const uint32_t* shaderRegisters = (const uint32_t*)(shaderCommon + 1);
		const uint8_t* shaderCode = (uint8_t*)( ((uintptr_t)shaderRegisters[0] << 8ULL) | ((uintptr_t)shaderRegisters[1] << 40ULL) );

		Gnm::Buffer embeddedCb;
		embeddedCb.initAsConstantBuffer((void*)(shaderCode + shaderCommon->m_shaderSize), shaderCommon->m_embeddedConstantBufferSizeInDQW << 4);
		setConstantBuffers(LightweightConstantUpdateEngine::kConstantBufferInternalApiSlotForEmbeddedData, 1, &embeddedCb);
	}
#else // SCE_GNMX_TRACK_EMBEDDED_CB
	SCE_GNM_UNUSED(shaderCommon);
#endif //SCE_GNMX_TRACK_EMBEDDED_CB
}


void ComputeConstantUpdateEngine::preDispatch()
{
	SCE_GNM_ASSERT(m_dispatchCommandBuffer != NULL);

	const Gnmx::CsShader* csShader = (const Gnmx::CsShader*)m_boundShader;
	if (m_dirtyShader)
	{
		m_dispatchCommandBuffer->setCsShader( &csShader->m_csStageRegisters );
		if (m_prefetchShaderCode)
		{
			SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, (void*)((uintptr_t)csShader->m_csStageRegisters.m_computePgmLo << 8), csShader->m_common.m_shaderSize);
			m_dispatchCommandBuffer->prefetchIntoL2((void*)((uintptr_t)csShader->m_csStageRegisters.m_computePgmLo << 8), csShader->m_common.m_shaderSize);
		}
	}

	// Handle Immediate Constant Buffer on CB slot 15 (kShaderSlotEmbeddedConstantBuffer)
	if (m_dirtyShader || m_dirtyShaderResources)
		updateEmbeddedCb((const Gnmx::ShaderCommonData*)csShader);

	if (m_dirtyShaderResources)
	{
		SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(&m_boundShaderResourcesValidation, m_boundShaderResourceOffsets);
		updateCommonPtrsInUserDataSgprs( flushScratchBuffer() );
	}

	m_dirtyShaderResources = false;
	m_dirtyShader = false;
}


void ComputeConstantUpdateEngine::setCsShader(const Gnmx::CsShader* shader, const InputResourceOffsets* table)
{
	SCE_GNM_ASSERT(shader != NULL && table != NULL && table->shaderStage == Gnm::kShaderStageCs);
	SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(&m_boundShaderResourcesValidation, table);

	m_dirtyShaderResources = true;
	m_dirtyShader |= (m_boundShader != shader);
	m_boundShaderResourceOffsets = table;
	m_boundShader = shader;

	m_csShaderOrderedAppendMode = shader->m_orderedAppendMode;
}


void ComputeConstantUpdateEngine::setAppendConsumeCounterRange(uint32_t gdsMemoryBaseInBytes, uint32_t countersSizeInBytes)
{
	SCE_GNM_ASSERT((gdsMemoryBaseInBytes%4)==0 && gdsMemoryBaseInBytes < UINT16_MAX && (countersSizeInBytes%4)==0 && countersSizeInBytes < UINT16_MAX);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets != NULL);

	SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets->appendConsumeCounterSgpr != 0xFF);
	if (m_boundShaderResourceOffsets->appendConsumeCounterSgpr != 0xFF)
	{
		SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation.appendConsumeCounterIsBound);
		m_boundShaderAppendConsumeCounterRange = (gdsMemoryBaseInBytes << 16) | countersSizeInBytes;
	}

	m_dirtyShaderResources = true;
}


void ComputeConstantUpdateEngine::setGdsMemoryRange(uint32_t baseOffsetInBytes, uint32_t rangeSizeInBytes)
{
	SCE_GNM_ASSERT(((baseOffsetInBytes | rangeSizeInBytes) & 3) == 0);
	SCE_GNM_ASSERT(baseOffsetInBytes < Gnm::kGdsAccessibleMemorySizeInBytes);
	SCE_GNM_ASSERT(rangeSizeInBytes <= Gnm::kGdsAccessibleMemorySizeInBytes);
	SCE_GNM_ASSERT((baseOffsetInBytes+rangeSizeInBytes) <= Gnm::kGdsAccessibleMemorySizeInBytes);

	SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets->gdsMemoryRangeSgpr != 0xFF);
	if (m_boundShaderResourceOffsets->gdsMemoryRangeSgpr != 0xFF)
	{
		SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation.gdsMemoryRangeIsBound);
		m_boundShaderGdsMemoryRange = (baseOffsetInBytes << 16) | rangeSizeInBytes;
	}

	m_dirtyShaderResources = true;
}


void ComputeConstantUpdateEngine::setConstantBuffers(int32_t startApiSlot, int32_t apiSlotCount, const Gnm::Buffer* buffer)
{
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets != NULL && buffer != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets;

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->constBufferSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->constBufferSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->constBufferSlotCount);

	for (int32_t i=0; i<apiSlotCount; i++)
	{
		int32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_CONSTANT_BUFFER((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, buffer+i);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets->constBufferDwOffset[currentApiSlot] != 0xFFFF);
		if (m_boundShaderResourceOffsets->constBufferDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED( m_boundShaderResourcesValidation.constBufferOffsetIsBound[currentApiSlot] );
			setAsycPipeDataInUserDataSgprOrMemory(m_dispatchCommandBuffer, m_scratchBuffer, table->constBufferDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
		}
	}

	m_dirtyShaderResources = true;
}


void ComputeConstantUpdateEngine::setBuffers(int32_t startApiSlot, int32_t apiSlotCount, const Gnm::Buffer* buffer)
{
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets != NULL && buffer != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets;

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->resourceSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->resourceSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->resourceSlotCount);

	for (int32_t i=0; i<apiSlotCount; i++)
	{
		int32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_BUFFER((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, buffer+i);
		// Cannot currently be validated as this information is not stored for entries in the resource-flat-table (possible to retrieve from Shader::Binary)
		//SCE_GNM_ASSERT((m_boundShaderResourceOffsets[(int32_t)shaderStage]->resourceDwOffset[currentApiSlot] & kResourceIsVSharp) != 0);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets->resourceDwOffset[currentApiSlot] != 0xFFFF);
		if (m_boundShaderResourceOffsets->resourceDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation.resourceOffsetIsBound[currentApiSlot]);
			setAsycPipeDataInUserDataSgprOrMemory(m_dispatchCommandBuffer, m_scratchBuffer, table->resourceDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
		}
	}

	m_dirtyShaderResources = true;
}


void ComputeConstantUpdateEngine::setRwBuffers(int32_t startApiSlot, int32_t apiSlotCount, const Gnm::Buffer* buffer)
{
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets != NULL && buffer != NULL);
	SCE_GNM_ASSERT(buffer->getResourceMemoryType() != Gnm::kResourceMemoryTypeRO);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets;

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->rwResourceSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->rwResourceSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->rwResourceSlotCount);
	
	for (int32_t i=0; i<apiSlotCount; i++)
	{
		int32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_RWBUFFER((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, buffer+i);
		// Cannot currently be validated as this information is not stored for entries in the resource-flat-table (possible to retrieve from Shader::Binary)
		//SCE_GNM_ASSERT((m_boundShaderResourceOffsets[(int32_t)shaderStage]->rwResourceDwOffset[currentApiSlot] & kResourceIsVSharp) != 0);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets->rwResourceDwOffset[currentApiSlot] != 0xFFFF);
		if (m_boundShaderResourceOffsets->rwResourceDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation.rwResourceOffsetIsBound[currentApiSlot]);
			setAsycPipeDataInUserDataSgprOrMemory(m_dispatchCommandBuffer, m_scratchBuffer, table->rwResourceDwOffset[currentApiSlot], buffer + i, sizeof(Gnm::Buffer));
		}
	}

	m_dirtyShaderResources = true;
}


void ComputeConstantUpdateEngine::setTextures(int32_t startApiSlot, int32_t apiSlotCount, const Gnm::Texture* texture)
{
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets != NULL && texture != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets;

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->resourceSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->resourceSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->resourceSlotCount);

	for (int32_t i=0; i<apiSlotCount; i++)
	{
		int32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_TEXTURE((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, texture+i);
		// Cannot currently be validated as this information is not stored for entries in the resource-flat-table (possible to retrieve from Shader::Binary)
		//SCE_GNM_ASSERT((m_boundShaderResourceOffsets[(int32_t)shaderStage]->resourceDwOffset[currentApiSlot] & kResourceIsVSharp) == 0);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets->resourceDwOffset[currentApiSlot] != 0xFFFF);
		if (m_boundShaderResourceOffsets->resourceDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation.resourceOffsetIsBound[currentApiSlot]);
			setAsycPipeDataInUserDataSgprOrMemory(m_dispatchCommandBuffer, m_scratchBuffer, table->resourceDwOffset[currentApiSlot], texture + i, sizeof(Gnm::Texture));
		}
	}

	m_dirtyShaderResources = true;
}


void ComputeConstantUpdateEngine::setRwTextures(int32_t startApiSlot, int32_t apiSlotCount, const Gnm::Texture* texture)
{
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets != NULL && texture != NULL);
	SCE_GNM_ASSERT(texture->getResourceMemoryType() != Gnm::kResourceMemoryTypeRO);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets;

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->rwResourceSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->rwResourceSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->rwResourceSlotCount);

	for (int32_t i=0; i<apiSlotCount; i++)
	{
		int32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_RWTEXTURE((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, texture+i);
		// Cannot currently be validated as this information is not stored for entries in the resource-flat-table (possible to retrieve from Shader::Binary)
		//SCE_GNM_ASSERT((m_boundShaderResourceOffsets[(int32_t)shaderStage]->rwResourceDwOffset[currentApiSlot] & kResourceIsVSharp) == 0);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets->rwResourceDwOffset[currentApiSlot] != 0xFFFF);
		if (m_boundShaderResourceOffsets->rwResourceDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation.rwResourceOffsetIsBound[currentApiSlot]);
			setAsycPipeDataInUserDataSgprOrMemory(m_dispatchCommandBuffer, m_scratchBuffer, table->rwResourceDwOffset[currentApiSlot], texture + i, sizeof(Gnm::Texture));
		}
	}

	m_dirtyShaderResources = true;
}


void ComputeConstantUpdateEngine::setSamplers(int32_t startApiSlot, int32_t apiSlotCount, const Gnm::Sampler* sampler)
{
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets != NULL && sampler != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets;

	SCE_GNM_ASSERT(startApiSlot >= 0 && startApiSlot < table->samplerSlotCount);
	SCE_GNM_ASSERT(apiSlotCount >= 0 && apiSlotCount <= table->samplerSlotCount);
	SCE_GNM_ASSERT(startApiSlot + apiSlotCount <= table->samplerSlotCount);

	for (int32_t i=0; i<apiSlotCount; i++)
	{
		int32_t currentApiSlot = startApiSlot+i;
		SCE_GNM_LCUE_VALIDATE_SAMPLER((const LightweightConstantUpdateEngine::BaseConstantUpdateEngine*)this, &this->m_resourceErrorCallback, currentApiSlot, sampler+i);

		SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(m_boundShaderResourceOffsets->samplerDwOffset[currentApiSlot] != 0xFFFF);
		if (m_boundShaderResourceOffsets->samplerDwOffset[currentApiSlot] != 0xFFFF)
		{
			SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(m_boundShaderResourcesValidation.samplerOffsetIsBound[currentApiSlot]);
			setAsycPipeDataInUserDataSgprOrMemory(m_dispatchCommandBuffer, m_scratchBuffer, table->samplerDwOffset[currentApiSlot], sampler + i, sizeof(Gnm::Sampler));
		}
	}

	m_dirtyShaderResources = true;
}


void ComputeConstantUpdateEngine::setUserData(uint32_t startSgpr, uint32_t sgprCount, const uint32_t* data)
{
	SCE_GNM_ASSERT(startSgpr >= 0 && (startSgpr+sgprCount) <= LightweightConstantUpdateEngine::kMaxUserDataCount);
	setAsycPipePersistentRegisterRange(m_dispatchCommandBuffer, startSgpr, data, sgprCount);
}


void ComputeConstantUpdateEngine::setPtrInUserData(uint32_t startSgpr, const void* gpuAddress)
{
	SCE_GNM_ASSERT(startSgpr >= 0 && (startSgpr) <= LightweightConstantUpdateEngine::kMaxUserDataCount);
	setAsycPipePtrInPersistentRegister(m_dispatchCommandBuffer, startSgpr, gpuAddress);
}


void ComputeConstantUpdateEngine::setUserSrtBuffer(const void* buffer, uint32_t sizeInDwords)
{
	SCE_GNM_UNUSED(sizeInDwords);
	SCE_GNM_ASSERT(m_boundShaderResourceOffsets != NULL && buffer != NULL);

	const InputResourceOffsets* table = m_boundShaderResourceOffsets;
	SCE_GNM_ASSERT(sizeInDwords >= table->userSrtDataCount && sizeInDwords <= LightweightConstantUpdateEngine::kMaxSrtUserDataCount);

	setUserData(table->userSrtDataSgpr, table->userSrtDataCount, (const uint32_t *)buffer);
}

#endif // defined(__ORBIS__)
