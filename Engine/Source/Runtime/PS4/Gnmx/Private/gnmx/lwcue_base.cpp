/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <algorithm>
#include <gnmx/lwcue_base.h>

using namespace sce;
using namespace Gnmx;

//Post SDK-1.700 shader compilers and assemblers may add a vertex buffer usage mask for kShaderInputUsagePtrVertexBufferTable.
//In order to maintain full binary compatibility with old runtime/validation/tools code, the table mask dword must go at the  
//end of all other chunk masks rather than in order.
#define SCE_GNM_LCUE_USE_VERTEX_BUFFER_TABLE_MASK_IF_AVAILABLE 0

#if defined(_MSC_VER)
#pragma warning( disable : 4244)
#define __builtin_memset memset
#endif

#if defined(__ORBIS__) // LCUE isn't supported offline

void BaseConstantUpdateEngine::init(uint32_t** resourceBuffersInGarlic, int32_t resourceBufferCount, uint32_t resourceBufferSizeInDwords, void* globalInternalResourceTableAddr)
{
	SCE_GNM_ASSERT_MSG(resourceBufferCount == 0 || resourceBuffersInGarlic != NULL, "if resourceBufferCount is not 0, than resourceBuffersInGarlic must not be NULL");
	SCE_GNM_ASSERT_MSG(resourceBufferCount <= LightweightConstantUpdateEngine::kMaxResourceBufferCount, "resourceBufferCount must not be greater than LightweightConstantUpdateEngine::kMaxResourceBufferCount");

	__builtin_memset(&m_bufferBegin, 0, sizeof(uint32_t) * LightweightConstantUpdateEngine::kMaxResourceBufferCount);
	__builtin_memset(&m_bufferEnd, 	0, sizeof(uint32_t) * LightweightConstantUpdateEngine::kMaxResourceBufferCount);

	for (int32_t i = 0; i < resourceBufferCount; ++i)
	{
		m_bufferBegin[i] = resourceBuffersInGarlic[i];
		m_bufferEnd[i] = m_bufferBegin[i] + resourceBufferSizeInDwords;
	}

	m_bufferIndex = 0;
	m_bufferCount = std::max(std::min(resourceBufferCount, LightweightConstantUpdateEngine::kMaxResourceBufferCount), 1);

	m_bufferCurrent	= m_bufferBegin[m_bufferIndex];
	m_bufferCurrentBegin = m_bufferBegin[m_bufferIndex];
	m_bufferCurrentEnd = m_bufferEnd[m_bufferIndex];

	m_prefetchShaderCode = true;
	m_resourceBufferCallback.m_func	= NULL;
	m_resourceBufferCallback.m_userData = NULL;
	m_resourceErrorCallback.m_func = NULL;
	m_resourceErrorCallback.m_userData = NULL;

	m_globalInternalResourceTableAddr = (Gnm::Buffer*)globalInternalResourceTableAddr;
}


void BaseConstantUpdateEngine::swapBuffers()
{
	m_bufferIndex = (m_bufferIndex+1) % m_bufferCount;
	m_bufferCurrent	= m_bufferBegin[m_bufferIndex];
	m_bufferCurrentBegin = m_bufferBegin[m_bufferIndex];
	m_bufferCurrentEnd = m_bufferEnd[m_bufferIndex];
}


uint32_t* BaseConstantUpdateEngine::reserveResourceBufferSpace(uint32_t sizeInDwords)
{
	uint32_t* resourceBufferPtr = NULL;
	if (!resourceBufferHasSpace(sizeInDwords))
	{
		uint32_t resultSizeInDwords = 0;
		if (m_resourceBufferCallback.m_func)
		{
			resourceBufferPtr = m_resourceBufferCallback.m_func(this, sizeInDwords, &resultSizeInDwords, m_resourceBufferCallback.m_userData);
			if (!resourceBufferPtr)
			{
				SCE_GNM_ERROR("reserveResourceBufferSpace failed due to out-of-memory, callback returned a NULL pointer");
				return NULL;
			}
		}
		else
		{
			SCE_GNM_ERROR("reserveResourceBufferSpace failed due to out-of-memory, please increase the resource buffer size when calling LightweightGraphicsConstantUpdateEngine::init() "
						  "or provide a callback function to allow additional frame block allocations");
			return NULL;
		}

		// ensure there is enough space in the new allocation for the required resource buffer space
		SCE_GNM_ASSERT(resultSizeInDwords >= sizeInDwords);

		// update current resource buffer pointer tracking
		m_bufferCurrent = m_bufferCurrentBegin = resourceBufferPtr;
		m_bufferCurrentEnd = resourceBufferPtr + resultSizeInDwords;
	}
	else
	{
		// space still exists, so continue to use the existing allocation
		resourceBufferPtr = m_bufferCurrent;
	}

	// increment the current resource buffer pointers
	m_bufferCurrent += sizeInDwords;

	return resourceBufferPtr;
}


void BaseConstantUpdateEngine::setGlobalDescriptor(Gnm::ShaderGlobalResourceType resourceType, const Gnm::Buffer* buffer)
{
	SCE_GNM_ASSERT(m_globalInternalResourceTableAddr != NULL && buffer != NULL);
	SCE_GNM_ASSERT(resourceType >= 0 && resourceType < Gnm::kShaderGlobalResourceCount);

	// Tessellation-factor-buffer - Checks address, alignment, size and memory type (apparently, must be Gnm::kResourceMemoryTypeGC)
	SCE_GNM_ASSERT(resourceType != Gnm::kShaderGlobalResourceTessFactorBuffer || (buffer->getBaseAddress() == sce::Gnm::getTessellationFactorRingBufferBaseAddress() &&
				  ((uintptr_t)buffer->getBaseAddress() % Gnm::kAlignmentOfTessFactorBufferInBytes) == 0 && buffer->getSize() == Gnm::kTfRingSizeInBytes) );

	__builtin_memcpy(&m_globalInternalResourceTableAddr[(int32_t)resourceType], buffer, sizeof(Gnm::Buffer));
}


void sce::Gnmx::computeTessellationTgParameters(uint32_t *outVgtPrimitiveCount, uint32_t* outTgPatchCount, uint32_t desiredTgLdsSizeInBytes, uint32_t desiredTgPatchCount, const Gnmx::LsShader* localShader, const Gnmx::HsShader* hullShader)
{
	uint32_t vgtPrimitiveCount = 0;
	uint32_t tgPatchCount = 0; 
	uint32_t tgLdsSizeInBytes = 0; 

	const uint32_t kMaxTGLdsSizeInBytes = 32*1024;	// max allocation of LDS a thread group can make.
	SCE_GNM_ASSERT(desiredTgLdsSizeInBytes <= kMaxTGLdsSizeInBytes); SCE_GNM_UNUSED(kMaxTGLdsSizeInBytes);

	// calculate the minimum LDS required
	tgLdsSizeInBytes = hullShader->m_hsStageRegisters.isOffChip() ? computeLdsUsagePerPatchInBytesPerThreadGroupOffChip(&hullShader->m_hullStateConstants, localShader->m_lsStride) :
		computeLdsUsagePerPatchInBytesPerThreadGroup(&hullShader->m_hullStateConstants, localShader->m_lsStride);

	SCE_GNM_ASSERT(tgLdsSizeInBytes <= kMaxTGLdsSizeInBytes);
	tgLdsSizeInBytes = SCE_GNM_MAX(tgLdsSizeInBytes, desiredTgLdsSizeInBytes); // use desired value if the user decided to use more than the minimum

	// calculate the VGT primitive count and the minimum patch count for tessellation
	uint32_t maxVgprCount = hullShader->m_hsStageRegisters.getNumVgprs();
	SCE_GNM_ASSERT(maxVgprCount%4==0);

	Gnmx::computeVgtPrimitiveAndPatchCounts(&vgtPrimitiveCount, &tgPatchCount, maxVgprCount, tgLdsSizeInBytes, localShader, hullShader);
	tgPatchCount = SCE_GNM_MAX(tgPatchCount, desiredTgPatchCount); // use which ever patch count is the maximum

	// write out the VGT and Patch count
	*outVgtPrimitiveCount	= vgtPrimitiveCount;
	*outTgPatchCount		= tgPatchCount;
}


#endif // defined(__ORBIS__)


static uint32_t getUsedApiSlotsFromMask(uint32_t* outUsedApiSlots, uint8_t usedApiSlotsCount, uint32_t mask[4], uint32_t maxResourceCount)
{
	SCE_GNM_UNUSED(maxResourceCount);

	uint32_t resourceCount = 0;
	for (uint32_t slot = 0; slot < usedApiSlotsCount; ++slot)
		if (mask[slot>>5] & (1<<(slot & 0x1F)))
			outUsedApiSlots[resourceCount++] = slot;

#if SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
	// Check if the shader uses any API-slot over the maximum count configured to be handled by the LCUE
	for (uint32_t slot = usedApiSlotsCount; slot < maxResourceCount; ++slot)
		SCE_GNM_ASSERT( (mask[slot>>5] & (1<<(slot & 0x1F))) == 0);
#endif // SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED

	return resourceCount;
}


static uint32_t getUsedApiSlotsFromMask(uint32_t* outUsedApiSlots, uint8_t usedApiSlotsCount, uint32_t mask, uint32_t maxResourceCount)
{
	SCE_GNM_UNUSED(maxResourceCount);

	uint32_t resourceCount = 0;
	for (uint32_t slot = 0; slot < usedApiSlotsCount; ++slot)
		if ( ((1<<slot) & mask))
			outUsedApiSlots[resourceCount++] = slot;

#if SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
	// Check if the shader uses any API-slot over the maximum count configured to be handled by the LCUE
	for (uint32_t slot = usedApiSlotsCount; slot < maxResourceCount; ++slot)
		SCE_GNM_ASSERT( ((1<<slot) & mask) == 0);
#endif // SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED

	return resourceCount;
}

static uint32_t remapVertexBufferOffsetsWithSemanticTable(uint32_t* outUsedApiSlots, uint32_t firstUsedApiSlot, uint32_t lastUsedApiSlot, uint8_t maxVertexBufferSlotCount, 
													     const void *semanticRemapTable, const uint32_t numElementsInSemanticRemapTable)
{
	// we only care which slots are in use, dictated by the remap table
	uint32_t vertexBufferChunkMask = 0x0;
	const uint32_t* remapTable = static_cast<const uint32_t*>(semanticRemapTable);
	for (uint8_t slotIndex = 0; slotIndex < numElementsInSemanticRemapTable; slotIndex++)
	{
		if (remapTable[slotIndex] >= firstUsedApiSlot && remapTable[slotIndex] <= lastUsedApiSlot)
		{
			vertexBufferChunkMask |= 0x1 << slotIndex;
		}
	}

	return getUsedApiSlotsFromMask(&outUsedApiSlots[0], maxVertexBufferSlotCount, vertexBufferChunkMask, Gnm::kSlotCountVertexBuffer);
}


void sce::Gnmx::generateInputResourceOffsetTable(InputResourceOffsets* outTable, Gnm::ShaderStage shaderStage, const void* gnmxShaderStruct)
{
	generateInputResourceOffsetTable(outTable, shaderStage, gnmxShaderStruct, NULL, 0);
}


void sce::Gnmx::generateInputResourceOffsetTable(InputResourceOffsets* outTable, Gnm::ShaderStage shaderStage, const void* gnmxShaderStruct, const void *semanticRemapTable, const uint32_t numElementsInSemanticRemapTable)
{
	// Get the shader pointer and its size from the GNMX shader type
	const Gnmx::ShaderCommonData* shaderCommonData = (const Gnmx::ShaderCommonData*)gnmxShaderStruct;
	const uint32_t* shaderRegisters = (const uint32_t*)(shaderCommonData + 1);

	const void* shaderCode = (void*)(((uintptr_t)shaderRegisters[0] << 8ULL) | ((uintptr_t)shaderRegisters[1] << 40ULL));
	uint32_t shaderCodeSize = ((const Gnmx::ShaderCommonData*)gnmxShaderStruct)->m_shaderSize;
	SCE_GNM_ASSERT(shaderCode != NULL && shaderCodeSize > 0);

	generateInputResourceOffsetTable(outTable, shaderStage, gnmxShaderStruct, shaderCode, shaderCodeSize, shaderCommonData->m_shaderIsUsingSrt, false, semanticRemapTable, numElementsInSemanticRemapTable);
}


void sce::Gnmx::generateInputResourceOffsetTableForDispatchDraw(InputResourceOffsets* CsOutTable, InputResourceOffsets* VsOutTable, const void* gnmxShaderStruct)
{
	generateInputResourceOffsetTableForDispatchDraw(CsOutTable, VsOutTable, gnmxShaderStruct, NULL, 0);
}


void sce::Gnmx::generateInputResourceOffsetTableForDispatchDraw(InputResourceOffsets* CsOutTable, InputResourceOffsets* VsOutTable, const void* gnmxShaderStruct, const void *semanticRemapTable, const uint32_t numElementsInSemanticRemapTable)
{
	const Gnmx::ShaderCommonData* shaderCommonData;
	const uint32_t* shaderRegisters;
	const void* shaderCode;
	uint32_t shaderCodeSize;

	// Get the vertex shader pointer and its size from the GNMX shader type
	const Gnmx::VsShader *vsShader = ((Gnmx::CsVsShader*)gnmxShaderStruct)->getVertexShader();
	shaderCommonData = (const Gnmx::ShaderCommonData*)vsShader;
	shaderRegisters = (const uint32_t*)(shaderCommonData + 1);

	shaderCode = (void*)(((uintptr_t)shaderRegisters[0] << 8ULL) | ((uintptr_t)shaderRegisters[1] << 40ULL));
	shaderCodeSize = shaderCommonData->m_shaderSize;
	SCE_GNM_ASSERT(shaderCode != NULL && shaderCodeSize > 0);

	generateInputResourceOffsetTable(VsOutTable, Gnm::kShaderStageVs, gnmxShaderStruct, shaderCode, shaderCodeSize, shaderCommonData->m_shaderIsUsingSrt, true, semanticRemapTable, numElementsInSemanticRemapTable);

	// Get the compute shader pointer and its size from the GNMX shader type
	const Gnmx::CsShader *csShader = ((Gnmx::CsVsShader*)gnmxShaderStruct)->getComputeShader();
	shaderCommonData = (const Gnmx::ShaderCommonData*)csShader;
	shaderRegisters = (const uint32_t*)(shaderCommonData + 1);

	shaderCode = (void*)(((uintptr_t)shaderRegisters[0] << 8ULL) | ((uintptr_t)shaderRegisters[1] << 40ULL));
	shaderCodeSize = shaderCommonData->m_shaderSize;
	SCE_GNM_ASSERT(shaderCode != NULL && shaderCodeSize > 0);

	generateInputResourceOffsetTable(CsOutTable, Gnm::kShaderStageCs, gnmxShaderStruct, shaderCode, shaderCodeSize, shaderCommonData->m_shaderIsUsingSrt, true, semanticRemapTable, numElementsInSemanticRemapTable);
}


void sce::Gnmx::generateInputResourceOffsetTable(InputResourceOffsets* outTable, Gnm::ShaderStage shaderStage, const void* gnmxShaderStruct, 
												 const void* shaderCode, uint32_t shaderCodeSizeInBytes, bool isSrtUsed, bool isDispatchDraw,
												 const void *semanticRemapTable, const uint32_t numElementsInSemanticRemapTable)
{
	typedef struct
	{
		uint8_t			m_signature[7];				// 'OrbShdr'
		uint8_t			m_version;					// ShaderBinaryInfoVersion

		unsigned int	m_pssl_or_cg				: 1;	// 1 = PSSL / Cg, 0 = IL / shtb
		unsigned int	m_cached					: 1;	// 1 = when compile, debugging source was cached.  May only make sense for PSSL=1
		uint32_t		m_type						: 4;	// See enum ShaderBinaryType
		uint32_t		m_source_type				: 2;	// See enum ShaderSourceType
		unsigned int	m_length					: 24;	// Binary code length (does not include this structure or any of its preceding associated tables)

		uint8_t			m_chunkUsageBaseOffsetInDW;			// in DW, which starts at ((uint32_t*)&ShaderBinaryInfo) - m_chunkUsageBaseOffsetInDW; max is currently 7 dwords (128 T# + 32 V# + 20 CB V# + 16 UAV T#/V#)
		uint8_t			m_numInputUsageSlots;				// Up to 16 user data reg slots + 128 extended user data dwords supported by CUE; up to 16 user data reg slots + 240 extended user data dwords supported by Gnm::InputUsageSlot
		uint8_t         m_isSrt						: 1;	// 1 if this shader uses shader resource tables and has an SrtDef table embedded below the input usage table and any extended usage info
		uint8_t         m_isSrtUsedInfoValid		: 1;	// 1 if SrtDef::m_isUsed=0 indicates an element is definitely unused; 0 if SrtDef::m_isUsed=0 indicates only that the element is not known to be used (m_isUsed=1 always indicates a resource is known to be used)
		uint8_t         m_isExtendedUsageInfo		: 1;	// 1 if this shader has extended usage info for the InputUsage table embedded below the input usage table
		uint8_t         m_reserved2					: 5;	// For future use
		uint8_t         m_reserved3;						// For future use

		uint32_t		m_shaderHash0;				// Association hash first 4 bytes
		uint32_t		m_shaderHash1;				// Association hash second 4 bytes
		uint32_t		m_crc32;					// crc32 of shader + this struct, just up till this field
	} ShaderBinaryInfo;

	SCE_GNM_ASSERT(outTable != NULL);
	SCE_GNM_ASSERT(shaderStage <= Gnm::kShaderStageCount);

	// Get resource info to populate ShaderResourceOffsets
	ShaderBinaryInfo const *shaderBinaryInfo = (ShaderBinaryInfo const*)((uintptr_t)shaderCode + shaderCodeSizeInBytes - sizeof(ShaderBinaryInfo));
	SCE_GNM_ASSERT((*(reinterpret_cast<uint64_t const*>(shaderBinaryInfo->m_signature)) & LightweightConstantUpdateEngine::kShaderBinaryInfoSignatureMask) == LightweightConstantUpdateEngine::kShaderBinaryInfoSignatureU64);
	
	// Get usage masks and input usage slots
	uint32_t const* usageMasks = reinterpret_cast<unsigned int const*>((unsigned char const*)shaderBinaryInfo - shaderBinaryInfo->m_chunkUsageBaseOffsetInDW*4);
	int32_t inputUsageSlotsCount = shaderBinaryInfo->m_numInputUsageSlots;
	Gnm::InputUsageSlot const* inputUsageSlots = (Gnm::InputUsageSlot const*)usageMasks - inputUsageSlotsCount;
	 
	// Cache shader input information into the ShaderResource Offsets table
	__builtin_memset(outTable, 0xFF, sizeof(InputResourceOffsets));
	outTable->initSupportedResourceCounts();
	outTable->shaderStage = isDispatchDraw && shaderStage == Gnm::kShaderStageCs ? LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute : shaderStage;
	outTable->isSrtShader = isSrtUsed;
	int32_t	lastUserDataResourceSizeInDwords = 0;
	uint16_t requiredMemorySizeInDwords = 0;

	// Here we handle all immediate resources s[1:16] plus s[16:48] (extended user data)
	// resources that go into the extended user data also have "immediate" usage type, although they are stored in a table (not loaded by the SPI)
	for (int32_t i=0; i<inputUsageSlotsCount; ++i)
	{
		uint8_t apiSlot = inputUsageSlots[i].m_apiSlot;
		uint8_t startRegister = inputUsageSlots[i].m_startRegister;
		bool isVSharp = (inputUsageSlots[i].m_resourceType == 0);
		uint16_t vsharpFlag = (isVSharp)? LightweightConstantUpdateEngine::kResourceIsVSharp : 0;

		uint16_t extendedRegisterOffsetInDwords = (startRegister >= LightweightConstantUpdateEngine::kMaxUserDataCount) ? 
												  (startRegister-LightweightConstantUpdateEngine::kMaxUserDataCount) : 0;
		requiredMemorySizeInDwords = (requiredMemorySizeInDwords > extendedRegisterOffsetInDwords) ?
									 requiredMemorySizeInDwords : extendedRegisterOffsetInDwords;

		// Handle immediate resources, including some pointer types
		switch (inputUsageSlots[i].m_usageType)
		{
		case Gnm::kShaderInputUsageImmGdsCounterRange:
			outTable->appendConsumeCounterSgpr = startRegister;
			break;

		case Gnm::kShaderInputUsageImmGdsMemoryRange:
			outTable->gdsMemoryRangeSgpr = startRegister;
			break;

		case Gnm::kShaderInputUsageImmLdsEsGsSize:
			outTable->ldsEsGsSizeSgpr = startRegister; 
			break;

		case Gnm::kShaderInputUsageSubPtrFetchShader:
			SCE_GNM_ASSERT(apiSlot == 0);
			outTable->fetchShaderPtrSgpr = startRegister;
			break;

		case Gnm::kShaderInputUsagePtrInternalGlobalTable:
			SCE_GNM_ASSERT(apiSlot == 0);
			outTable->globalInternalPtrSgpr = startRegister;
			break;

		case Gnm::kShaderInputUsagePtrExtendedUserData:
			SCE_GNM_ASSERT(apiSlot == 1);
			outTable->userExtendedData1PtrSgpr = startRegister;
			break;

		case Gnm::kShaderInputUsageImmGdsKickRingBufferOffset:
			SCE_GNM_ASSERT(apiSlot == 0);
			SCE_GNM_ASSERT(startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount);
			outTable->gdsKickRingBufferOffsetSgpr = startRegister; 
			break;

		case Gnm::kShaderInputUsageImmVertexRingBufferOffset:
			SCE_GNM_ASSERT(apiSlot == 0);
			SCE_GNM_ASSERT(startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount);
			outTable->vertexRingBufferOffsetSgpr = startRegister;
			break;

		case Gnm::kShaderInputUsagePtrDispatchDraw:
			SCE_GNM_ASSERT(apiSlot == 0);
			outTable->dispatchDrawPtrSgpr = startRegister;
			break;

		case Gnm::kShaderInputUsageImmDispatchDrawInstances:
			SCE_GNM_ASSERT(apiSlot == 0);
			SCE_GNM_ASSERT(startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount);
			outTable->dispatchDrawInstancesSgpr = startRegister;
			break;

		// below resources can either be inside UserData or the EUD
		case Gnm::kShaderInputUsageImmResource:
			SCE_GNM_ASSERT(apiSlot >= 0 && apiSlot < outTable->resourceSlotCount);
			outTable->resourceDwOffset[apiSlot] = (startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount) ?
												  (LightweightConstantUpdateEngine::kResourceInUserDataSgpr | vsharpFlag | startRegister) : (vsharpFlag | extendedRegisterOffsetInDwords);
			lastUserDataResourceSizeInDwords = (startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount) ? 0 : Gnm::kDwordSizeResource;
			break;

		case Gnm::kShaderInputUsageImmRwResource:
			SCE_GNM_ASSERT(apiSlot >= 0 && apiSlot < outTable->rwResourceSlotCount);
			outTable->rwResourceDwOffset[apiSlot] = (startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount)?
													(LightweightConstantUpdateEngine::kResourceInUserDataSgpr | vsharpFlag | startRegister) : (vsharpFlag | extendedRegisterOffsetInDwords);
			lastUserDataResourceSizeInDwords = (startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount)? 0 : Gnm::kDwordSizeRwResource;
			break;

		case Gnm::kShaderInputUsageImmSampler:
			SCE_GNM_ASSERT(apiSlot >= 0 && apiSlot < outTable->samplerSlotCount);
			outTable->samplerDwOffset[apiSlot] = (startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount) ?
												 (LightweightConstantUpdateEngine::kResourceInUserDataSgpr | startRegister) : extendedRegisterOffsetInDwords;
			lastUserDataResourceSizeInDwords = (startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount) ? 0 : Gnm::kDwordSizeSampler;
			break;

		case Gnm::kShaderInputUsageImmConstBuffer:
			SCE_GNM_ASSERT(apiSlot >= 0 && apiSlot < outTable->constBufferSlotCount);
			outTable->constBufferDwOffset[apiSlot] = (startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount) ?
													 (LightweightConstantUpdateEngine::kResourceInUserDataSgpr | startRegister) : extendedRegisterOffsetInDwords;
			lastUserDataResourceSizeInDwords = (startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount) ? 0 : Gnm::kDwordSizeConstantBuffer;
			break;

		case Gnm::kShaderInputUsageImmVertexBuffer:
			SCE_GNM_ASSERT(apiSlot >= 0 && apiSlot < outTable->vertexBufferSlotCount);
			outTable->vertexBufferDwOffset[apiSlot] = (startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount) ?
													  (LightweightConstantUpdateEngine::kResourceInUserDataSgpr | startRegister) : extendedRegisterOffsetInDwords;
			lastUserDataResourceSizeInDwords = (startRegister < LightweightConstantUpdateEngine::kMaxUserDataCount) ? 0 : Gnm::kDwordSizeVertexBuffer;
			break;
		
		// SRTs will always reside inside the Imm UserData (dwords 0-15), as opposed to the 
		// above resources which can exist in the EUD
		case Gnm::kShaderInputUsageImmShaderResourceTable:
			SCE_GNM_ASSERT(apiSlot >= 0 && apiSlot < LightweightConstantUpdateEngine::kMaxUserDataCount);
			outTable->userSrtDataSgpr = inputUsageSlots[i].m_startRegister;
			outTable->userSrtDataCount = inputUsageSlots[i].m_srtSizeInDWordMinusOne+1;
			break;

// 		case Gnm::kShaderInputUsagePtrSoBufferTable: // Only present in the VS copy-shader that doesn't have a footer
// 			outTable->streamOutPtrSgpr = startRegister;
// 			break;

		}
	}

	// Make sure we can fit a T# (if required) in the last userOffset
	requiredMemorySizeInDwords += lastUserDataResourceSizeInDwords;

	// Now handle only pointers to resource-tables. Items handled below cannot be found more than once
#if SCE_GNM_LCUE_USE_VERTEX_BUFFER_TABLE_MASK_IF_AVAILABLE
	// Note: in order to maintain binary compatibility, we can only put a new chunk mask for kShaderInputUsagePtrVertexBufferTable at the end of all other chunk masks
	bool bUseVertexBufferTableChunkMask = false;
#endif

	for (int32_t i=0; i<inputUsageSlotsCount; ++i)
	{
		uint8_t maskChunks = inputUsageSlots[i].m_chunkMask;
		
		const uint64_t kNibbleToCount = 0x4332322132212110ull;
		uint8_t chunksCount = (kNibbleToCount >> ((maskChunks & 0xF)*4)) & 0xF; SCE_GNM_UNUSED(chunksCount);
		SCE_GNM_ASSERT(usageMasks+chunksCount <= (uint32_t const*)shaderBinaryInfo);
		
		// Lets fill the resource indices first
		uint32_t usedApiSlots[Gnm::kSlotCountResource]; // Use the size of the biggest resource table
		uint32_t usedApiSlotCount;

		// This thing will break if there's more than 1 table for any resource type
		uint8_t startRegister = inputUsageSlots[i].m_startRegister;

		switch (inputUsageSlots[i].m_usageType)
		{
		case Gnm::kShaderInputUsagePtrResourceTable:
		{
			SCE_GNM_ASSERT(inputUsageSlots[i].m_apiSlot == 0);
			outTable->resourcePtrSgpr = startRegister;
			outTable->resourceArrayDwOffset = requiredMemorySizeInDwords;

			if (!(maskChunks & 0xF))
				break;

			SCE_GNM_ASSERT(usageMasks < (uint32_t const*)shaderBinaryInfo);
			uint32_t maskArray[4] = { 0, 0, 0, 0 };	// Max 128 slots are supported in the kShaderInputUsagePtrResourceTable
			if (maskChunks & 1) maskArray[0] = *usageMasks++;	// get slots 0-31 which are set in Chunk 0
			if (maskChunks & 2) maskArray[1] = *usageMasks++;	// get slots 32-63 which are set in Chunk 1
			if (maskChunks & 4) maskArray[2] = *usageMasks++;	// get slots 64-95 which are set in Chunk 2
			if (maskChunks & 8) maskArray[3] = *usageMasks++;	// get slots 96-127 which are set in Chunk 3
			SCE_GNM_ASSERT(usageMasks <= (uint32_t const*)shaderBinaryInfo);

			usedApiSlotCount = getUsedApiSlotsFromMask(&usedApiSlots[0], outTable->resourceSlotCount, maskArray, Gnm::kSlotCountResource);
			SCE_GNM_ASSERT(usedApiSlotCount > 0);

			uint32_t lastUsedApiSlot = usedApiSlots[usedApiSlotCount-1];
			for (uint8_t j=0; j<usedApiSlotCount; j++)
			{
				uint16_t currentApiSlot = static_cast<uint16_t>(usedApiSlots[j]);
				outTable->resourceDwOffset[currentApiSlot] = requiredMemorySizeInDwords + currentApiSlot * Gnm::kDwordSizeResource;
			}
			requiredMemorySizeInDwords += (lastUsedApiSlot+1) * Gnm::kDwordSizeResource;
		}
		break;

		case Gnm::kShaderInputUsagePtrRwResourceTable:
		{
			SCE_GNM_ASSERT(inputUsageSlots[i].m_apiSlot == 0);
			outTable->rwResourcePtrSgpr = startRegister;
			outTable->rwResourceArrayDwOffset = requiredMemorySizeInDwords;

			if (!(maskChunks & 1))
				break;
			SCE_GNM_ASSERT(usageMasks < (uint32_t const*)shaderBinaryInfo);

			usedApiSlotCount = getUsedApiSlotsFromMask(&usedApiSlots[0], outTable->rwResourceSlotCount, *usageMasks++, Gnm::kSlotCountRwResource);
			SCE_GNM_ASSERT(usedApiSlotCount > 0);

			uint32_t lastUsedApiSlot = usedApiSlots[usedApiSlotCount-1];
			for (uint8_t j=0; j<usedApiSlotCount; j++)
			{
				uint16_t currentApiSlot = static_cast<uint16_t>(usedApiSlots[j]);
				outTable->rwResourceDwOffset[currentApiSlot] = requiredMemorySizeInDwords + currentApiSlot * Gnm::kDwordSizeRwResource;
			}
			requiredMemorySizeInDwords += (lastUsedApiSlot+1) * Gnm::kDwordSizeRwResource;
		}
		break;

		case Gnm::kShaderInputUsagePtrConstBufferTable:
		{
			SCE_GNM_ASSERT(inputUsageSlots[i].m_apiSlot == 0);
			outTable->constBufferPtrSgpr = startRegister;
			outTable->constBufferArrayDwOffset = requiredMemorySizeInDwords;
			
			if (!(maskChunks & 1))
				break;
			SCE_GNM_ASSERT(usageMasks < (uint32_t const*)shaderBinaryInfo);

			usedApiSlotCount = getUsedApiSlotsFromMask(&usedApiSlots[0], outTable->constBufferSlotCount, *usageMasks++, Gnm::kSlotCountConstantBuffer);
			SCE_GNM_ASSERT(usedApiSlotCount > 0);

			uint32_t lastUsedApiSlot = usedApiSlots[usedApiSlotCount-1];
			for (uint8_t j=0; j<usedApiSlotCount; j++)
			{
				uint16_t currentApiSlot = static_cast<uint16_t>(usedApiSlots[j]);
				outTable->constBufferDwOffset[currentApiSlot] = requiredMemorySizeInDwords + currentApiSlot * Gnm::kDwordSizeConstantBuffer;
			}
			requiredMemorySizeInDwords += (lastUsedApiSlot+1) * Gnm::kDwordSizeConstantBuffer;
		}
		break;

		case Gnm::kShaderInputUsagePtrSamplerTable:
		{
			SCE_GNM_ASSERT(inputUsageSlots[i].m_apiSlot == 0);
			outTable->samplerPtrSgpr = startRegister;
			outTable->samplerArrayDwOffset = requiredMemorySizeInDwords;

			if (!(maskChunks & 1))
				break;
			SCE_GNM_ASSERT(usageMasks < (uint32_t const*)shaderBinaryInfo);

			usedApiSlotCount = getUsedApiSlotsFromMask(&usedApiSlots[0], outTable->samplerSlotCount, *usageMasks++, Gnm::kSlotCountSampler);
			SCE_GNM_ASSERT(usedApiSlotCount > 0);

			uint32_t lastUsedApiSlot = usedApiSlots[usedApiSlotCount-1];
			for (uint8_t j=0; j<usedApiSlotCount; j++)
			{
				uint16_t currentApiSlot = static_cast<uint16_t>(usedApiSlots[j]);
				outTable->samplerDwOffset[currentApiSlot] = requiredMemorySizeInDwords + currentApiSlot * Gnm::kDwordSizeSampler;
			}
			requiredMemorySizeInDwords += (lastUsedApiSlot+1) * Gnm::kDwordSizeSampler;
		}
		break;

		case Gnm::kShaderInputUsagePtrVertexBufferTable:
		{
			SCE_GNM_ASSERT(shaderStage == Gnm::kShaderStageLs || shaderStage == Gnm::kShaderStageEs || shaderStage == Gnm::kShaderStageVs || shaderStage == Gnm::kShaderStageCs);
			SCE_GNM_ASSERT(inputUsageSlots[i].m_apiSlot == 0);
			outTable->vertexBufferPtrSgpr = startRegister;
			outTable->vertexBufferArrayDwOffset = requiredMemorySizeInDwords;
			
#if SCE_GNM_LCUE_USE_VERTEX_BUFFER_TABLE_MASK_IF_AVAILABLE
			if (maskChunks & 1) 
			{
				// Skip updating for the vertex buffer table below, since we are using the chunk mask at the end
				// we'll update it after everything else.
				bUseVertexBufferTableChunkMask = true;
				continue;
			}
#endif
			SCE_GNM_ASSERT(gnmxShaderStruct != NULL);
			const Gnm::VertexInputSemantic* semanticTable = NULL;
			usedApiSlotCount = 0;
			if (shaderStage == Gnm::kShaderStageVs && isDispatchDraw)
			{
				Gnmx::VsShader const* pVsShader = ((Gnmx::CsVsShader*)gnmxShaderStruct)->getVertexShader();
				usedApiSlotCount = pVsShader->m_numInputSemantics;
				semanticTable = pVsShader->getInputSemanticTable();
			}
			else if (shaderStage == Gnm::kShaderStageVs)
			{
				usedApiSlotCount = ((Gnmx::VsShader*)gnmxShaderStruct)->m_numInputSemantics;
				semanticTable = ((Gnmx::VsShader*)gnmxShaderStruct)->getInputSemanticTable();
			}
			else if (shaderStage == Gnm::kShaderStageLs)
			{
				usedApiSlotCount = ((Gnmx::LsShader*)gnmxShaderStruct)->m_numInputSemantics;
				semanticTable = ((Gnmx::LsShader*)gnmxShaderStruct)->getInputSemanticTable();
			}
			else if (shaderStage == Gnm::kShaderStageEs)
			{
				usedApiSlotCount = ((Gnmx::EsShader*)gnmxShaderStruct)->m_numInputSemantics;
				semanticTable = ((Gnmx::EsShader*)gnmxShaderStruct)->getInputSemanticTable();
			}
			if (shaderStage == Gnm::kShaderStageCs && isDispatchDraw)
			{
				Gnmx::VsShader const* pVsShader = ((Gnmx::CsVsShader*)gnmxShaderStruct)->getVertexShader();
				Gnmx::CsShader const* pCsShader = ((Gnmx::CsVsShader*)gnmxShaderStruct)->getComputeShader();
				if (pCsShader->m_version >= Gnmx::kDdCsShaderVersion_IndependentCsFetchShader)
				{
					usedApiSlotCount = pCsShader->m_numInputSemantics;
					semanticTable = pCsShader->getInputSemanticTable();
				}
				else
				{
					usedApiSlotCount = pVsShader->m_gsModeOrNumInputSemanticsCs;
					semanticTable = pVsShader->getInputSemanticTable();
				}
			}
			else if (shaderStage == Gnm::kShaderStageCs)
			{
				Gnmx::CsShader const* pCsShader = ((Gnmx::CsShader*)gnmxShaderStruct);
				if (pCsShader->m_version >= Gnmx::kDdCsShaderVersion_IndependentCsFetchShader)
				{
					usedApiSlotCount = pCsShader->m_numInputSemantics;
					semanticTable = pCsShader->getInputSemanticTable();
				}
				else
				{
					usedApiSlotCount = 0;
					semanticTable = NULL;
				}
			}
			// Check if the shader uses any API-slot over the maximum count configured for the InputResourceOffset table
			SCE_GNM_ASSERT(usedApiSlotCount > 0 && usedApiSlotCount <= outTable->vertexBufferSlotCount);

			uint32_t usedVertexBufferApiSlots[Gnm::kSlotCountVertexBuffer];

			// First use what the shader generated
			for (uint8_t SlotIndex = 0; SlotIndex < usedApiSlotCount; SlotIndex++)
			{
				uint8_t semanticIndex = semanticTable[SlotIndex].m_semantic;
				SCE_GNM_ASSERT(semanticIndex >= 0 && semanticIndex < outTable->vertexBufferSlotCount);
				usedVertexBufferApiSlots[SlotIndex] = semanticIndex;
			}

			uint32_t firstUsedApiSlot = usedVertexBufferApiSlots[0];
			uint32_t lastUsedApiSlot = usedVertexBufferApiSlots[usedApiSlotCount-1];

			// If a semanticRemapTable has been provided, override the shaders defined usage slots to conform with the remapped layout
			if (semanticRemapTable && numElementsInSemanticRemapTable != 0)
			{
				// Override values defined in the shader binary header above
				SCE_GNM_ASSERT(usedApiSlotCount <= numElementsInSemanticRemapTable);
				usedApiSlotCount = remapVertexBufferOffsetsWithSemanticTable(&usedVertexBufferApiSlots[0], firstUsedApiSlot, lastUsedApiSlot, outTable->vertexBufferSlotCount, 
																			semanticRemapTable, numElementsInSemanticRemapTable);
			}

			// Generate the final dword offsets for the vertex buffer table
			lastUsedApiSlot = usedVertexBufferApiSlots[usedApiSlotCount-1];
			for (uint8_t j=0; j<usedApiSlotCount; j++)
			{
				uint16_t currentApiSlot = static_cast<uint16_t>(usedVertexBufferApiSlots[j]);
				outTable->vertexBufferDwOffset[currentApiSlot] = requiredMemorySizeInDwords + currentApiSlot * Gnm::kDwordSizeVertexBuffer;
			}
			requiredMemorySizeInDwords += (lastUsedApiSlot+1) * Gnm::kDwordSizeVertexBuffer;
		}
		break;
		}
	}

	// Note: this must be called after all other tables are processed above, as the vertex buffer table chunk mask (*usageMasks) 
	// is always stored at the end of the chunk mask table
#if SCE_GNM_LCUE_USE_VERTEX_BUFFER_TABLE_MASK_IF_AVAILABLE
	if (bUseVertexBufferTableChunkMask)
	{
		uint32_t usedApiSlots[Gnm::kSlotCountVertexBuffer];
		uint32_t usedApiSlotCount;

		SCE_GNM_ASSERT(usageMasks < (uint32_t const*)shaderBinaryInfo);

		usedApiSlotCount = getUsedApiSlotsFromMask(&usedApiSlots[0], outTable->vertexBufferSlotCount, *usageMasks++, Gnm::kSlotCountVertexBuffer);
		SCE_GNM_ASSERT(usedApiSlotCount > 0);

		uint32_t firstUsedApiSlot = usedApiSlots[0];
		uint32_t lastUsedApiSlot = usedApiSlots[usedApiSlotCount-1];

		// If a semanticRemapTable has been provided, override the shaders defined usage slots to conform with the remapped layout
		if (semanticRemapTable && numElementsInSemanticRemapTable != 0)
		{
			// Override values defined in the shader binary header above
			SCE_GNM_ASSERT((uint32_t)usedApiSlotCount <= numElementsInSemanticRemapTable);
			usedApiSlotCount = remapVertexBufferOffsetsWithSemanticTable(&usedApiSlots[0], firstUsedApiSlot, lastUsedApiSlot, outTable->vertexBufferSlotCount,
																		semanticRemapTable, numElementsInSemanticRemapTable);
		}

		// Generate the final dword offsets for the vertex buffer table
		lastUsedApiSlot = usedApiSlots[usedApiSlotCount-1];
		for (uint8_t j = 0; j < usedApiSlotCount; j++)
		{
			int32_t currentApiSlot = usedApiSlots[j];
			outTable->vertexBufferDwOffset[currentApiSlot] = requiredMemorySizeInDwords + currentApiSlot * Gnm::kDwordSizeVertexBuffer;
		}
		requiredMemorySizeInDwords += (lastUsedApiSlot+1) * Gnm::kDwordSizeVertexBuffer;
	}
#endif

	// Final amount of memory the shader will use from the scratch and resource buffer
	outTable->requiredBufferSizeInDwords = requiredMemorySizeInDwords;

	// Checking for non handled input data
	for (int32_t i=0; i<inputUsageSlotsCount; ++i)
	{
		switch (inputUsageSlots[i].m_usageType)
		{
		case Gnm::kShaderInputUsageImmResource:
		case Gnm::kShaderInputUsageImmRwResource:
		case Gnm::kShaderInputUsageImmSampler:
		case Gnm::kShaderInputUsageImmConstBuffer:
		case Gnm::kShaderInputUsageImmVertexBuffer:
		case Gnm::kShaderInputUsageImmShaderResourceTable:
		case Gnm::kShaderInputUsageSubPtrFetchShader:
		case Gnm::kShaderInputUsagePtrExtendedUserData:
		case Gnm::kShaderInputUsagePtrResourceTable:
		case Gnm::kShaderInputUsagePtrRwResourceTable:
		case Gnm::kShaderInputUsagePtrConstBufferTable:
		case Gnm::kShaderInputUsagePtrVertexBufferTable:
		case Gnm::kShaderInputUsagePtrSamplerTable:
		case Gnm::kShaderInputUsagePtrInternalGlobalTable:
		case Gnm::kShaderInputUsageImmGdsCounterRange:
		case Gnm::kShaderInputUsageImmGdsMemoryRange:
		case Gnm::kShaderInputUsageImmLdsEsGsSize:
		case Gnm::kShaderInputUsageImmGdsKickRingBufferOffset:
		case Gnm::kShaderInputUsageImmVertexRingBufferOffset:
		case Gnm::kShaderInputUsagePtrDispatchDraw:
		case Gnm::kShaderInputUsageImmDispatchDrawInstances:
//		case Gnm::kShaderInputUsagePtrSoBufferTable:		// Only present in the VS copy-shader that doesn't have a footer
			break;
		default:
			// Not handled yet
			SCE_GNM_ASSERT_MSG(false, "Input Usage Slot type %d is not supported by LCUE sce::Gnmx::generateInputResourceOffsetTable()", inputUsageSlots[i].m_usageType);
			break;
		}
	}
}