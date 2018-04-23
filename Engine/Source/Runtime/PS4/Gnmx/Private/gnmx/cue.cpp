/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx/common.h>

#include <cstring>
#ifdef __ORBIS__
#include <x86intrin.h>
#else //__ORBIS__
#include <intrin.h>
#endif //__ORBIS__
#include <stddef.h>
#include <gnmx/cue.h>
#include <gnmx/gfxcontext.h>
#include <gnmx/cue-helper.h>

#ifdef _MSC_VER
#pragma warning(disable:4127) // conditional is constant
#endif

using namespace sce::Gnm;
using namespace sce::Gnmx;


//////////////////////////////////////

namespace
{
	const uint32_t kWaitOnDeCounterDiffSizeInDword = 2;
#if SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR
	void* defaultUserEudAllocator(uint32_t sizeInDword, void* data)
	{
		ConstantUpdateEngine* cue = reinterpret_cast<ConstantUpdateEngine*>(data);
		const uint32_t size = sizeInDword * 4;
#if SCE_GNM_CUE2_EUD_ALLOCATION_IN_CCB
#if SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
		return cue->m_ccb->allocateFromTheEndOfTheBuffer(size, kEmbeddedDataAlignment16);
#else //SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
		return cue->m_ccb->allocateFromCommandBuffer(size, kEmbeddedDataAlignment16);
#endif //SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
#else //SCE_GNM_CUE2_EUD_ALLOCATION_IN_CCB
#if SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
		return cue->m_dcb->allocateFromTheEndOfTheBuffer(size, kEmbeddedDataAlignment16);
#else //SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
		return cue->m_dcb->allocateFromCommandBuffer(size, kEmbeddedDataAlignment16);
#endif //SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
#endif //SCE_GNM_CUE2_EUD_ALLOCATION_IN_CCB
	}
#endif //SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR
}

SCE_GNM_STATIC_ASSERT((sizeof(ConstantUpdateEngine) & 0xf) == 0);
#ifdef __ORBIS__
SCE_GNM_STATIC_ASSERT(sizeof(ConstantUpdateEngine::StageInfo::activeResource) * 8 >= kSlotCountResource);
SCE_GNM_STATIC_ASSERT(sizeof(ConstantUpdateEngine::StageInfo::eudResourceSet) == sizeof(ConstantUpdateEngine::StageInfo::activeResource));
SCE_GNM_STATIC_ASSERT(sizeof(ConstantUpdateEngine::StageInfo::activeSampler) * 8 >= kSlotCountSampler);
SCE_GNM_STATIC_ASSERT(sizeof(ConstantUpdateEngine::StageInfo::eudSamplerSet) == sizeof(ConstantUpdateEngine::StageInfo::activeSampler));
SCE_GNM_STATIC_ASSERT(sizeof(ConstantUpdateEngine::StageInfo::activeConstantBuffer) * 8 >= kSlotCountConstantBuffer);
SCE_GNM_STATIC_ASSERT(sizeof(ConstantUpdateEngine::StageInfo::eudConstantBufferSet) == sizeof(ConstantUpdateEngine::StageInfo::activeConstantBuffer));
SCE_GNM_STATIC_ASSERT(sizeof(ConstantUpdateEngine::StageInfo::activeRwResource) * 8 >= kSlotCountRwResource);
SCE_GNM_STATIC_ASSERT(sizeof(ConstantUpdateEngine::StageInfo::eudRwResourceSet) == sizeof(ConstantUpdateEngine::StageInfo::activeRwResource));
SCE_GNM_STATIC_ASSERT(sizeof(ConstantUpdateEngine::StageInfo::activeVertexBuffer) * 8 >= kSlotCountVertexBuffer);
#endif //__ORBIS__



ConstantUpdateEngine::ConstantUpdateEngine(void)
{
}


ConstantUpdateEngine::~ConstantUpdateEngine(void)
{
}


void ConstantUpdateEngine::init(void *heapAddr, uint32_t numRingEntries)
{
	RingSetup ringSetup;
	ringSetup.numResourceSlots	   = Gnm::kSlotCountResource;
	ringSetup.numRwResourceSlots   = Gnm::kSlotCountRwResource;
	ringSetup.numSampleSlots	   = Gnm::kSlotCountSampler;
	ringSetup.numVertexBufferSlots = Gnm::kSlotCountVertexBuffer;

	init(heapAddr, numRingEntries, ringSetup);
}


void ConstantUpdateEngine::init(void *heapAddr, uint32_t numRingEntries, RingSetup ringSetup)
{
	SCE_GNM_ASSERT_MSG(heapAddr != NULL, "heapAddr must not be NULL.");
	SCE_GNM_ASSERT_MSG((uintptr_t(heapAddr) & 0x3) == 0, "heapAddr (0x%p) must be aligned to a 4-byte boundary.", heapAddr);
	SCE_GNM_ASSERT_MSG(numRingEntries>=4, "If hardware kcache invalidation is enabled, numRingEntries (%u) must be at least 4.", numRingEntries);

	m_ringSetup = ringSetup;
	ConstantUpdateEngineHelper::validateRingSetup(&m_ringSetup);

	m_dcb					= 0;
	m_ccb					= 0;
	m_activeShaderStages	= Gnm::kActiveShaderStagesVsPs;
	m_numRingEntries		= numRingEntries;
	m_shaderUsesSrt			= 0;
	m_anyWrapped			= true;
	m_psInputs				= NULL;
	m_dirtyStage            = 0;
	m_dirtyShader			= 0;

	m_currentVSB = NULL;
	m_currentPSB = NULL;
	m_currentLSB = NULL;
	m_currentESB = NULL;
	m_currentHSB = NULL;
	m_currentGSB = NULL;
	m_currentAcbCSB = NULL;

	m_shaderContextIsValid = 0;
	m_currentDistributionMode = Gnm::kTessellationDistributionNone;

	m_prefetchShaderCode = true; // The L2 prefetch is currently disabled -- No need to insert unnecessary nops.

	memset(m_stageInfo, 0, sizeof(m_stageInfo));

	m_streamoutBufferStage.usedChunk = 0;
	m_streamoutBufferStage.curChunk  = 0;
	m_streamoutBufferStage.curSlots  = 0;
	m_streamoutBufferStage.usedSlots = 0;
	m_eudReferencesStreamoutBuffers	 = false;

#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	m_globalTableStage.usedChunk	= 0;
	m_globalTableStage.curChunk		= 0;
	m_globalTableStage.curSlots		= 0;
	m_globalTableStage.usedSlots	= 0;
	m_eudReferencesGlobalTable		= 0;

#else //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	memset(m_globalTablePtr, 0, SCE_GNM_SHADER_GLOBAL_TABLE_SIZE);
	m_globalTableNeedsUpdate = false;
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE

	m_onChipEsVertsPerSubGroup        = 0;
	m_onChipEsExportVertexSizeInDword = 0;
	m_onChipGsSignature				  = kOnChipGsInvalidSignature;

#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	// Divvy up the heap between the ring buffers for each shader stage.
	// This is a duplicate of the body of ConstantUpdateEngine::computeHeapSize(). We don't
	// just call computeHeapSize() here because init() needs the intermediate k*RingBufferBytes values.
	const uint32_t kResourceRingBufferBytes	        = ConstantUpdateEngineHelper::computeShaderResourceRingSize(m_ringSetup.numResourceSlots        * Gnm::kDwordSizeResource,         numRingEntries);
	const uint32_t kRwResourceRingBufferBytes	    = ConstantUpdateEngineHelper::computeShaderResourceRingSize(m_ringSetup.numRwResourceSlots      * Gnm::kDwordSizeRwResource,       numRingEntries);
	const uint32_t kSamplerRingBufferBytes		    = ConstantUpdateEngineHelper::computeShaderResourceRingSize(m_ringSetup.numSampleSlots          * Gnm::kDwordSizeSampler,          numRingEntries);
	const uint32_t kVertexBufferRingBufferBytes     = ConstantUpdateEngineHelper::computeShaderResourceRingSize(m_ringSetup.numVertexBufferSlots    * Gnm::kDwordSizeVertexBuffer,     numRingEntries);
	const uint32_t kConstantBufferRingBufferBytes   = ConstantUpdateEngineHelper::computeShaderResourceRingSize(Gnm::kSlotCountConstantBuffer       * Gnm::kDwordSizeConstantBuffer,   numRingEntries);

	m_heapSize = computeHeapSize(numRingEntries, m_ringSetup);


	// Initialize ring buffers
	void *freeAddr = heapAddr;
	for(int iStage=0; iStage<Gnm::kShaderStageCount; ++iStage)
	{
		freeAddr = ConstantUpdateEngineHelper::initializeRingBuffer(&m_stageInfo[iStage].ringBuffers[kRingBuffersIndexResource],		 freeAddr, kResourceRingBufferBytes,		 m_ringSetup.numResourceSlots        * Gnm::kDwordSizeResource,         m_numRingEntries);
		freeAddr = ConstantUpdateEngineHelper::initializeRingBuffer(&m_stageInfo[iStage].ringBuffers[kRingBuffersIndexRwResource],		 freeAddr, kRwResourceRingBufferBytes,		 m_ringSetup.numRwResourceSlots      * Gnm::kDwordSizeRwResource,       m_numRingEntries);
		freeAddr = ConstantUpdateEngineHelper::initializeRingBuffer(&m_stageInfo[iStage].ringBuffers[kRingBuffersIndexSampler],			 freeAddr, kSamplerRingBufferBytes,			 m_ringSetup.numSampleSlots          * Gnm::kDwordSizeSampler,          m_numRingEntries);
		freeAddr = ConstantUpdateEngineHelper::initializeRingBuffer(&m_stageInfo[iStage].ringBuffers[kRingBuffersIndexVertexBuffer],	 freeAddr, kVertexBufferRingBufferBytes,	 m_ringSetup.numVertexBufferSlots    * Gnm::kDwordSizeVertexBuffer,     m_numRingEntries);
		freeAddr = ConstantUpdateEngineHelper::initializeRingBuffer(&m_stageInfo[iStage].ringBuffers[kRingBuffersIndexConstantBuffer],	 freeAddr, kConstantBufferRingBufferBytes,	 Gnm::kSlotCountConstantBuffer       * Gnm::kDwordSizeConstantBuffer,   m_numRingEntries);
	}

	const uint32_t actualUsed = static_cast<uint32_t>(uintptr_t(freeAddr) - uintptr_t(heapAddr));
	SCE_GNM_ASSERT(actualUsed == m_heapSize); // sanity check
	SCE_GNM_UNUSED(actualUsed);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	
	m_submitWithCcb = false;

	m_dispatchDrawIndexDeallocNumBits = 0;
	m_asyncCsShaderOrderedAppendMode = m_gfxCsShaderOrderedAppendMode = 0;

#if SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR
	setUserEudAllocator(defaultUserEudAllocator,this);
#endif //SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR

#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	clearUserTables();
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	memset(m_currentFetchShader,0,sizeof(m_currentFetchShader));
#endif //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER

}

//------


void ConstantUpdateEngine::setGlobalResourceTableAddr(void *addr)
{
#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	SCE_GNM_UNUSED(addr);
	SCE_GNM_ASSERT_MSG(false, "Setting the global resource table address when SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE is enabled has no effect");
#else
	m_globalTableAddr = addr;
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
}


void ConstantUpdateEngine::setGlobalDescriptor(sce::Gnm::ShaderGlobalResourceType resType, const sce::Gnm::Buffer *res)
{
#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	const uint16_t slotMask   = 1 << (16 - resType - 1);

	// Check for a conflict between previous draw and current draw
	if( !m_globalTableStage.usedChunk && 
		(m_globalTableStage.curSlots & slotMask) )
	{
		// Conflict
		m_globalTableStage.usedSlots = m_globalTableStage.curSlots;
		m_globalTableStage.usedChunk = m_globalTableStage.curChunk;
		m_globalTableStage.curChunk = 0;
	}
	// No chunk allocated for the current draw (due to conflict, or because it was never used before)
	if ( !m_globalTableStage.curChunk )
	{
		m_globalTableStage.curSlots = 0;
		m_globalTableStage.curChunk = (__int128_t*)m_dcb->allocateFromCommandBuffer(SCE_GNM_SHADER_GLOBAL_TABLE_SIZE, Gnm::kEmbeddedDataAlignment16);

		// Check if we need to dirty the EUD:
		m_shaderDirtyEud |= m_eudReferencesGlobalTable;
	}
	if( res ) 
	{
		m_globalTableStage.curChunk[resType] = *(__int128_t*)res;
		m_globalTableStage.curSlots |= slotMask;
	}
	else
	{
		m_globalTableStage.curSlots  &= ~slotMask;
		m_globalTableStage.usedSlots &= ~slotMask; // to avoid keeping older version of this resource
	}
	
#else //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	reinterpret_cast<__int128_t*>(m_globalTablePtr)[resType] = *reinterpret_cast<const __int128_t*>(res);
	m_globalTableNeedsUpdate = true;
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
}


//------



void ConstantUpdateEngine::updateSrtIUser(uint32_t usageSlot, ApplyUsageDataState* state)
{
	const uint8_t		currentStageMask = 1 << state->currentStage;
	const uint32_t		usesSrt			 = m_shaderUsesSrt & currentStageMask;
	const uint32_t		destUsgpr		 = state->inputUsageTable[usageSlot].m_startRegister;
	const uint32_t		dwCount			 = state->inputUsageTable[usageSlot].m_srtSizeInDWordMinusOne+1;

	SCE_GNM_ASSERT_MSG(usesSrt, "non-SRT shaders should not be looking for Shader Resource Tables!");
	SCE_GNM_ASSERT_MSG(dwCount <= state->stageInfo->userSrtBufferSizeInDwords, "SRT user data size mismatch: shader expected %u DWORDS, but caller provided %u.",
					   dwCount, state->stageInfo->userSrtBufferSizeInDwords);
	m_dcb->setUserDataRegion(state->currentStage, destUsgpr, state->stageInfo->userSrtBuffer, dwCount);
}

void ConstantUpdateEngine::updateLdsEsGsSizeUser(uint32_t usageSlot, ApplyUsageDataState* state)
{
	const uint32_t destUsgpr  = state->inputUsageTable[usageSlot].m_startRegister;
	m_dcb->setUserData(state->currentStage, destUsgpr, m_onChipEsVertsPerSubGroup * m_onChipEsExportVertexSizeInDword * 4);
}

void ConstantUpdateEngine::updateLdsEsGsSizeEud(uint32_t usageSlot, ApplyUsageDataState* state)
{
	const uint32_t		destEudNdx = state->inputUsageTable[usageSlot].m_startRegister - 16;
	uint32_t * const	eud32	   = (uint32_t *)(state->newEud + destEudNdx);
	*eud32 = m_onChipEsVertsPerSubGroup * m_onChipEsExportVertexSizeInDword * 4;
}

void ConstantUpdateEngine::updateEudUser(uint32_t usageSlot, ApplyUsageDataState* state)
{
	const uint32_t		destUsgpr		 = state->inputUsageTable[usageSlot].m_startRegister;
	
#if !SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS
	const uint8_t		currentStageMask = 1 << state->currentStage;
	const uint32_t		usesSrt			 = m_shaderUsesSrt & currentStageMask;
	
	if ( usesSrt )
	{
		m_dcb->setPointerInUserData(state->currentStage, destUsgpr, state->stageInfo->internalSrtBuffer);
	}
	else
#endif //!SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS
	if ( state->newEud )
	{
		SCE_GNM_ASSERT_MSG(state->newEud, "Invalid pointer.\n");
		m_dcb->setPointerInUserData(state->currentStage, destUsgpr, state->newEud);
	}
}

void ConstantUpdateEngine::updateGlobalTableUser(uint32_t usageSlot, ApplyUsageDataState* state)
{
	const uint32_t destUsgpr  = state->inputUsageTable[usageSlot].m_startRegister;
#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	updateChunkState128(&m_globalTableStage,1);
	m_dcb->setPointerInUserData(state->currentStage, destUsgpr,m_globalTableStage.curChunk);
#else //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	SCE_GNM_ASSERT_MSG(m_globalTableAddr, "Global table pointer not specified. Call setGlobalResourceTableAddr() first!");
	m_dcb->setPointerInUserData(state->currentStage, destUsgpr, m_globalTableAddr);
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
}

void ConstantUpdateEngine::updateGlobalTableEud(uint32_t usageSlot, ApplyUsageDataState* state)
{
	const uint32_t destEudNdx = state->inputUsageTable[usageSlot].m_startRegister - 16;
	void **eud64 = reinterpret_cast<void **>(state->newEud + destEudNdx);
#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	m_eudReferencesGlobalTable |= 1 << state->currentStage;
	updateChunkState128(&m_globalTableStage,1);
	(*eud64) = m_globalTableStage.curChunk;
#else //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	SCE_GNM_ASSERT_MSG(m_globalTableAddr, "Global table pointer not specified. Call setGlobalResourceTableAddr() first!");
	(*eud64) = m_globalTableAddr;
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
}

void ConstantUpdateEngine::updateDispatchDrawUser(uint32_t usageSlot, ApplyUsageDataState* state)
{
	const uint32_t destUsgpr  = state->inputUsageTable[usageSlot].m_startRegister;

	SCE_GNM_ASSERT_MSG(m_pDispatchDrawData, "DispatchDrawData pointer not specified. Call setDispatchDrawData() first!");
	m_dcb->setPointerInUserData(state->currentStage, destUsgpr, (void*)m_pDispatchDrawData);
}

void ConstantUpdateEngine::updateSoBufferTableUser(uint32_t usageSlot, ApplyUsageDataState* state)
{
	const uint32_t destUsgpr  = state->inputUsageTable[usageSlot].m_startRegister;

	updateChunkState128(&m_streamoutBufferStage, 1);
	m_dcb->setPointerInUserData(state->currentStage, destUsgpr, m_streamoutBufferStage.curChunk);
}

void ConstantUpdateEngine::updateSoBufferTableEud(uint32_t usageSlot, ApplyUsageDataState* state)
{
	const uint32_t destEudNdx = state->inputUsageTable[usageSlot].m_startRegister - 16;

	void **eud64 = reinterpret_cast<void **>(state->newEud + destEudNdx);
	updateChunkState128(&m_streamoutBufferStage, 1);
	(*eud64) = m_streamoutBufferStage.curChunk;
	m_eudReferencesStreamoutBuffers = true;
}


//------


static const uint32_t nullData[8] = {};

void ConstantUpdateEngine::initializeInputsCache(InputParameterCache *cache, const InputUsageSlot * inputUsageTable, uint32_t inputUsageTableSize)
{
#if SCE_GNM_CUE2_ENABLE_CACHE
	StageInfo * const nullStageInfo = 0; // this pointer is used only to compute offsets of the StageInfo's fields

	cache->userPointerTypesMask = 0;
	cache->eudPointerTypesMask	= 0;

	//
	// Initial pass: calculate the offsets for the dfferent inputs in the 'cache'
	//

	uint32_t	numUserImmediate = 0;
	uint32_t	numUserScalar	 = 0;
	uint32_t	numEudImmediate	 = 0;
	uint32_t	numEudScalar	 = 0;
	uint32_t	numDelegates	 = 0;

	for (uint32_t usageSlot = 0; usageSlot != inputUsageTableSize; ++usageSlot)
	{
		const uint32_t	destUsgpr = inputUsageTable[usageSlot].m_startRegister;
		const uint8_t	usageType = inputUsageTable[usageSlot].m_usageType;
		const uint64_t	usageBit  = (1ULL << usageType);
		const bool		usesEud	  = destUsgpr > 15;

		if( ConstantUpdateEngineHelper::kImmediateTypesMask & usageBit )
		{
			if( usesEud )
				++numEudImmediate;
			else
				++numUserImmediate;
		}
		else if( ConstantUpdateEngineHelper::kDwordTypesMask & usageBit )
		{
			if( usesEud )
				++numEudScalar;
			else
				++numUserScalar;
		}
		else if( ConstantUpdateEngineHelper::kDelegatedTypesMask & usageBit )
		{
			++numDelegates;
		}
	}

	const uint32_t totalSlots = numUserImmediate + numUserScalar + numEudImmediate + numEudScalar + numDelegates;
	SCE_GNM_ASSERT_MSG( totalSlots <= InputParameterCache::kMaxCacheSlots, "Too many input slots (%d), only no more than %d is supported", totalSlots, InputParameterCache::kMaxCacheSlots);

	cache->scalarUserStart	 = numUserImmediate;
	cache->immediateEudStart = numUserScalar   + cache->scalarUserStart;
	cache->scalarEudStart	 = numEudImmediate + cache->immediateEudStart;
	cache->delegateStart	 = numEudScalar    + cache->scalarEudStart;
	cache->delegateEnd		 = numDelegates    + cache->delegateStart;

	//
	// Second pass: fill up the 'cache' data
	//

	uint32_t	userImmediateWrite = 0;
	uint32_t	userScalarWrite	   = cache->scalarUserStart;
	uint32_t	eudImmeidateWrite  = cache->immediateEudStart;
	uint32_t	eudScalarWrite	   = cache->scalarEudStart;
	uint32_t	delegateWrite	   = cache->delegateStart;

	for (uint32_t usageSlot = 0; usageSlot != inputUsageTableSize; ++usageSlot)
	{
		const uint32_t	destUsgpr = inputUsageTable[usageSlot].m_startRegister;
		const uint8_t	usageType = inputUsageTable[usageSlot].m_usageType;

		const uint64_t	usageBit   = (1ULL << usageType);
		const bool		usesEud	   = destUsgpr > 15;
		const uint8_t	srcApiSlot = inputUsageTable[usageSlot].m_apiSlot;

		if ( ConstantUpdateEngineHelper::kImmediateTypesMask & usageBit )
		{
			uint32_t									*pindex = usesEud? &eudImmeidateWrite : &userImmediateWrite;
			InputParameterCache::DescriptorParam		*pparam = &cache->slots[(*pindex)++].descriptor;
			const uint8_t				 dqWordSize	   = ConstantUpdateEngineHelper::chunkSizes[usageType].dqw;
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			const uint8_t				 chunkSize	   = ConstantUpdateEngineHelper::chunkSizes[usageType].size;
			const uint8_t				 chunkSizeMask = chunkSize - 1;
			const uint32_t				 chunkSlot	   = (srcApiSlot & chunkSizeMask);
			const uint8_t				 chunkId	   = srcApiSlot / chunkSize;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6011) // NULL pointer dereference.
#endif
			const StageChunkState*		 stageChunks   = &(nullStageInfo->*(ConstantUpdateEngineHelper::chunkStatePointers[usageType]));
			const uint64_t				*setPtr		   = &(nullStageInfo->*(ConstantUpdateEngineHelper::eudSetPointers[usageType]));
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			pparam->regCount		 = (inputUsageTable[usageSlot].m_registerCount*4) + 4; // 1 -> 8 DW; 0 -> 4 DW
			pparam->destination		 = usesEud ? destUsgpr - 16 : destUsgpr;
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			pparam->setMask			 = 1ULL << (srcApiSlot & 63);
			pparam->chunkStateOffset = (uint32_t)((uintptr_t)(stageChunks + chunkId) - (uintptr_t)nullStageInfo);
			pparam->setBitsOffset	 = (uint32_t)((uintptr_t)(setPtr + (srcApiSlot >> 6)) - (uintptr_t)nullStageInfo);
			pparam->chunkSlot		 = chunkSlot * dqWordSize;
			pparam->chunkSlotMask	 = 1 << (15-chunkSlot);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_PARAMETER_HAS_SRC_RESOURCE_TRACKING
			pparam->srcApiSlot		 = srcApiSlot;
			pparam->activeBitsOffset = (uint32_t)((uintptr_t)&(nullStageInfo->*(ConstantUpdateEngineHelper::activeBitsPointers[usageType])) - (uintptr_t)nullStageInfo);
#endif //SCE_GNM_CUE2_PARAMETER_HAS_SRC_RESOURCE_TRACKING
#if SCE_GNM_CUE2_PARAMETER_HAS_USER_TABLE
			pparam->userTableId = ConstantUpdateEngineHelper::stageUserTableOffsets[usageType];
			pparam->userTableIndex	= srcApiSlot * dqWordSize;
#endif // SCE_GNM_CUE2_PARAMETER_HAS_USER_TABLE
		}
		else if ( ConstantUpdateEngineHelper::kPointerTypesMask & usageBit )
		{
			InputParameterCache::PointerType pointerType = InputParameterCache::kPtrResourceTable;
			switch ( usageType )
			{
			case kShaderInputUsagePtrResourceTable:
				pointerType = InputParameterCache::kPtrResourceTable;
				break;
			case kShaderInputUsagePtrSamplerTable:
				pointerType = InputParameterCache::kPtrSamplerTable;
				break;
			case kShaderInputUsagePtrConstBufferTable:
				pointerType = InputParameterCache::kPtrConstBufferTable;
				break;
			case kShaderInputUsagePtrVertexBufferTable:
				pointerType = InputParameterCache::kPtrVertexTable;
				break;
			case kShaderInputUsagePtrRwResourceTable:
				pointerType = InputParameterCache::kPtrRwResourceTable;
				break;
			default:
				SCE_GNM_ERROR("Unknown usage type: %u", usageType);
			}
			uint32_t * const	pointerTypesMask = usesEud ? &cache->eudPointerTypesMask : &cache->userPointerTypesMask;
			uint8_t  * const	pointers		 = usesEud ? cache->eudPointers : cache->userPointers;

			(*pointerTypesMask) |= 1<<pointerType;
			pointers[pointerType] = (uint8_t)(usesEud ? destUsgpr - 16 : destUsgpr);
		}
		else if ( ConstantUpdateEngineHelper::kDwordTypesMask & usageBit )
		{
			uint32_t							*pindex = usesEud? &eudScalarWrite : &userScalarWrite;
			InputParameterCache::DwordParam		*pparam = &cache->slots[(*pindex)++].scalar;

			pparam->destination		= (uint8_t)(usesEud ? destUsgpr - 16 : destUsgpr);
			pparam->referenceOffset = ConstantUpdateEngineHelper::dwordValueOffsets[usageType];
		}
		else if ( ConstantUpdateEngineHelper::kDelegatedTypesMask & usageBit )
		{
			InputParameterCache::DelegateParam *pparam = &cache->slots[delegateWrite++].special;

			SCE_GNM_ASSERT_MSG(usageSlot<256, "Sanity check failed!");
			pparam->delegateIndex = usageType*2 + usesEud; // offsetof(updateDelegates[usageType][usesEud]);
			pparam->slot		  = (uint8_t)usageSlot;
			SCE_GNM_ASSERT_MSG(ConstantUpdateEngineHelper::updateDelegates[usageType][usesEud], "Invalid delegate");
		}
	}
#else // SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_UNUSED(cache);
	SCE_GNM_UNUSED(inputUsageTable);
	SCE_GNM_UNUSED(inputUsageTableSize);
#endif // SCE_GNM_CUE2_ENABLE_CACHE
}
namespace
{
#if SCE_GNM_CUE2_ENABLE_CACHE
	const uint32_t* getParameterData(const ConstantUpdateEngine::InputParameterCache::DescriptorParam * pparam, ConstantUpdateEngine::StageInfo * const stageInfo)
	{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		const __int128_t* userTablePtr = reinterpret_cast<const __int128_t*>(stageInfo->userTable[pparam->userTableId]);
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		
		if( userTablePtr )
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			SCE_GNM_ASSERT_MSG(userTablePtr, "User table is not set");
			return reinterpret_cast<const uint32_t*>(userTablePtr+pparam->userTableIndex);
		}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES		
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		uintptr_t stageInfoPtr = reinterpret_cast<uintptr_t>(stageInfo);
		const ConstantUpdateEngine::StageChunkState* chunkState = (const ConstantUpdateEngine::StageChunkState*)(stageInfoPtr+pparam->chunkStateOffset);
		const uint32_t				 usedCur	= chunkState->curSlots & pparam->chunkSlotMask;
		const __int128_t			*chunk		= usedCur ? chunkState->curChunk : chunkState->usedChunk;
		const uint32_t				*dataPtr	= (const uint32_t*)(chunk+pparam->chunkSlot);
		return dataPtr;
#endif
	}
#endif //SCE_GNM_CUE2_ENABLE_CACHE
}
void ConstantUpdateEngine::applyInputUsageData(sce::Gnm::ShaderStage currentStage)
{
	const uint8_t		currentStageMask = 1 << currentStage;
	StageInfo * const	stageInfo		 = m_stageInfo+currentStage;

	const Gnm::InputUsageSlot	*inputUsageTable     = stageInfo->inputUsageTable;
	const uint32_t				 inputUsageTableSize = stageInfo->inputUsageTableSize;


	

	SCE_GNM_ASSERT_MSG(inputUsageTableSize, "This function should not be called if inputUsageTableSize is 0.");

	// Do we need to allocate the EUD?
	uint32_t * newEud = 0;
	if ( stageInfo->eudSizeInDWord && (m_shaderDirtyEud & currentStageMask) )
	{
#if SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR
		SCE_GNM_ASSERT_MSG(m_userEudAllocator,"SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR is enabled but the allocator had not been set.");
		stageInfo->eudAddr = (uint32_t*)m_userEudAllocator(stageInfo->eudSizeInDWord,m_userEudAllocatorData);
		SCE_GNM_ASSERT_MSG(0 == (reinterpret_cast<uintptr_t>(stageInfo->eudAddr) & 0xFULL), "User EUD allocator returned a pointer that is not 16 byte aligned (%p)",stageInfo->eudAddr);
#else //SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR
#if SCE_GNM_CUE2_EUD_ALLOCATION_IN_CCB
#if SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
		stageInfo->eudAddr = (uint32_t*)m_ccb->allocateFromTheEndOfTheBuffer(stageInfo->eudSizeInDWord*4, Gnm::kEmbeddedDataAlignment16);
#else //SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
		stageInfo->eudAddr = (uint32_t*)m_ccb->allocateFromCommandBuffer(stageInfo->eudSizeInDWord*4, Gnm::kEmbeddedDataAlignment16);
#endif //SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
#else //SCE_GNM_CUE2_EUD_ALLOCATION_IN_CCB
#if SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
		stageInfo->eudAddr = (uint32_t*)m_dcb->allocateFromTheEndOfTheBuffer(stageInfo->eudSizeInDWord*4, Gnm::kEmbeddedDataAlignment16);
#else //SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
		stageInfo->eudAddr = (uint32_t*)m_dcb->allocateFromCommandBuffer(stageInfo->eudSizeInDWord*4, Gnm::kEmbeddedDataAlignment16);
#endif //SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
#endif //SCE_GNM_CUE2_EUD_ALLOCATION_IN_CCB
#endif // SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR
		ConstantUpdateEngineHelper::cleanEud(this, currentStage);
		newEud = stageInfo->eudAddr;
	}

#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	// Build the ring address tables:
	uint16_t ringBufferActiveSize[kNumRingBuffersPerStage];
	uint32_t usedRingBuffer = 0;		// Bitfield representing which ring are actually used by the shaders.
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_ENABLE_CACHE
	uintptr_t           stageInfoPtr     = (uintptr_t)stageInfo;
	const InputParameterCache *pcache = stageInfo->activeCache;

	uint32_t index = 0;

	//
	// Setup Immediate in user sgprs:
	//
	for( ; index != pcache->scalarUserStart; ++index)
	{
		const InputParameterCache::DescriptorParam *pparam = &pcache->slots[index].descriptor;

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		const uint64_t *activeBits = (const uint64_t*)(stageInfoPtr+pparam->activeBitsOffset);
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		const bool ignoreActiveBits = stageInfo->userTable[pparam->userTableId];
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		const bool isActive = ignoreActiveBits || activeBits[ pparam->srcApiSlot >> 6 ] & (1ULL << (pparam->srcApiSlot &63) );
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		const bool isActive = true;
#endif //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		if( isActive )
		{
			const uint32_t				*dataPtr	= getParameterData(pparam,stageInfo);
#if SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			if( ! stageInfo->userTable[pparam->userTableId] )
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			{
				const uint64_t *activeBits = (const uint64_t*)(stageInfoPtr+pparam->activeBitsOffset);
				SCE_GNM_UNUSED(activeBits);
				SCE_GNM_ASSERT_MSG(activeBits[ pparam->srcApiSlot >> 6 ] & (1ULL << (pparam->srcApiSlot &63) ),
					"The shader requires a resource in slot %i which hasn't been set.\n"
					"If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", pparam->srcApiSlot);
			}
#endif // SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES

			m_dcb->setUserDataRegion(currentStage, pparam->destination, dataPtr, pparam->regCount);
		}
		else
		{
			m_dcb->setUserDataRegion(currentStage,pparam->destination,nullData, pparam->regCount);
		}
	}

	//
	// Setup scalar in user sgprs:
	//
	for ( ; index != pcache->immediateEudStart; ++index )
	{
		const InputParameterCache::DwordParam *pparam = &pcache->slots[index].scalar;
		const uint32_t reference = *(uint32_t*)(stageInfoPtr+pparam->referenceOffset);
		m_dcb->setUserData(currentStage, pparam->destination, reference );
	}

	//
	// Setup pointers in user sgrps:
	//
	if ( pcache->userPointerTypesMask & (1 << InputParameterCache::kPtrResourceTable) )
	{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		void* userTable = const_cast<void*>(stageInfo->userTable[kUserTableResource]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		SCE_GNM_ASSERT_MSG(userTable,"User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		if(userTable)
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			m_dcb->setPointerInUserData(currentStage,pcache->userPointers[InputParameterCache::kPtrResourceTable],userTable);
		}
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		else
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			if ( stageInfo->dirtyRing & (1u << (31-kRingBuffersIndexResource)) )
				updateChunkState256(stageInfo->resourceStage, kResourceNumChunks);

			void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexResource);
			m_dcb->setPointerInUserData(currentStage, pcache->userPointers[InputParameterCache::kPtrResourceTable], ringBufferAddr);
			usedRingBuffer |= 1u << (31-kRingBuffersIndexResource);
			ringBufferActiveSize[kRingBuffersIndexResource] = 128 - ConstantUpdateEngineHelper::lzcnt128(stageInfo->activeResource);
		}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	}
	if ( pcache->userPointerTypesMask & (1 << InputParameterCache::kPtrSamplerTable) )
	{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		void* userTable = const_cast<void*>(stageInfo->userTable[kUserTableSampler]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		SCE_GNM_ASSERT_MSG(userTable,"User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		if(userTable)
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			m_dcb->setPointerInUserData(currentStage,pcache->userPointers[InputParameterCache::kPtrSamplerTable],userTable);
		}
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		else
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			if ( stageInfo->dirtyRing & (1 << (31-kRingBuffersIndexSampler)) )
				updateChunkState128(stageInfo->samplerStage, kSamplerNumChunks);

			void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexSampler);
			m_dcb->setPointerInUserData(currentStage, pcache->userPointers[InputParameterCache::kPtrSamplerTable], ringBufferAddr);
			usedRingBuffer |= 1<< (31-kRingBuffersIndexSampler);
			ringBufferActiveSize[kRingBuffersIndexSampler] = 64 - (uint16_t)__lzcnt64(stageInfo->activeSampler);
		}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	}
	if ( pcache->userPointerTypesMask & (1 << InputParameterCache::kPtrConstBufferTable) )
	{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		void* userTable = const_cast<void*>(stageInfo->userTable[kUserTableConstantBuffer]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		SCE_GNM_ASSERT_MSG(userTable,"User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		if(userTable)
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			m_dcb->setPointerInUserData(currentStage,pcache->userPointers[InputParameterCache::kPtrConstBufferTable],userTable);
		}
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		else
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexConstantBuffer)) )
				updateChunkState128(stageInfo->constantBufferStage	, kConstantBufferNumChunks);

			void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexConstantBuffer);
			m_dcb->setPointerInUserData(currentStage, pcache->userPointers[InputParameterCache::kPtrConstBufferTable], ringBufferAddr);
			usedRingBuffer |= 1<< (31-kRingBuffersIndexConstantBuffer);
			ringBufferActiveSize[kRingBuffersIndexConstantBuffer] = 64 - (uint16_t)__lzcnt64(stageInfo->activeConstantBuffer);
		}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	}
	if ( pcache->userPointerTypesMask & (1 << InputParameterCache::kPtrRwResourceTable) )
	{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		void* userTable = const_cast<void*>(stageInfo->userTable[kUserTableRwResource]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		SCE_GNM_ASSERT_MSG(userTable,"User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		if(userTable)
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			m_dcb->setPointerInUserData(currentStage,pcache->userPointers[InputParameterCache::kPtrRwResourceTable],userTable);
		}
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		else
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexRwResource)) )
				updateChunkState256(stageInfo->rwResourceStage, kRwResourceNumChunks);

			void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexRwResource);
			m_dcb->setPointerInUserData(currentStage, pcache->userPointers[InputParameterCache::kPtrRwResourceTable], ringBufferAddr);
			usedRingBuffer |= 1<< (31-kRingBuffersIndexRwResource);
			ringBufferActiveSize[kRingBuffersIndexRwResource] = 64 - (uint16_t)__lzcnt64(stageInfo->activeRwResource);
		}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	}
	if ( pcache->userPointerTypesMask & (1 << InputParameterCache::kPtrVertexTable) )
	{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
		void* userTable = const_cast<void*>(stageInfo->userTable[kUserTableVertexBuffer]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		SCE_GNM_ASSERT_MSG(userTable,"User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		if(userTable)
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			m_dcb->setPointerInUserData(currentStage,pcache->userPointers[InputParameterCache::kPtrVertexTable],userTable);
		}
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		else
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		{
			ringBufferActiveSize[kRingBuffersIndexVertexBuffer] = (uint16_t)(64 - __lzcnt64(stageInfo->activeVertexBuffer));

			if ( ringBufferActiveSize[kRingBuffersIndexVertexBuffer] <= 8 )
			{
				if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexVertexBuffer)) )
					updateChunkState128(stageInfo->vertexBufferStage, 1);

				SCE_GNM_ASSERT_MSG(stageInfo->vertexBufferStage[0].curChunk, "Invalid pointer.\n");
				m_dcb->setPointerInUserData(currentStage, pcache->userPointers[InputParameterCache::kPtrVertexTable], stageInfo->vertexBufferStage[0].curChunk);
			}
			else
			{
				if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexVertexBuffer)) )
					updateChunkState128(stageInfo->vertexBufferStage, kVertexBufferNumChunks);

				void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexVertexBuffer);
				m_dcb->setPointerInUserData(currentStage, pcache->userPointers[InputParameterCache::kPtrVertexTable], ringBufferAddr);
				usedRingBuffer |= 1<< (31-kRingBuffersIndexVertexBuffer);
			}
		}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	}


	if ( newEud ) 
	{
		//
		// Setup Immediate in the EUD:
		//
		for ( ; index != pcache->scalarEudStart; ++index )
		{
			const InputParameterCache::DescriptorParam *pparam = &pcache->slots[index].descriptor;
			const __int128_t		*dataPtr;
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			const uint64_t *activeBits = (const uint64_t*)(stageInfoPtr+pparam->activeBitsOffset);
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			const bool ignoreActiveBits = stageInfo->userTable[pparam->userTableId];
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			const bool isActive = ignoreActiveBits || activeBits[ pparam->srcApiSlot >> 6 ] & (1ULL << (pparam->srcApiSlot &63) );
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			const bool isActive = true;
#endif //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			if( isActive )
			{
#if SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				if( ! stageInfo->userTable[pparam->userTableId] )
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				{
					const uint64_t *activeBits = (const uint64_t*)(stageInfoPtr+pparam->activeBitsOffset);
					SCE_GNM_UNUSED(activeBits);
					SCE_GNM_ASSERT_MSG(activeBits[ pparam->srcApiSlot >> 6 ] & (1ULL << (pparam->srcApiSlot &63) ),
						"The shader requires a resource in slot %i which hasn't been set.\n"
						"If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", pparam->srcApiSlot);
				}
#endif // SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES

				dataPtr   = reinterpret_cast<const __int128_t*>(getParameterData(pparam,stageInfo));
			}
			else
			{
				dataPtr = reinterpret_cast<const __int128_t *>(nullData);
			}

			__int128_t *eud128 = reinterpret_cast<__int128_t *>(newEud + pparam->destination);
			eud128[0] = dataPtr[0];
			if ( pparam->regCount == 8 )
				eud128[1] = dataPtr[1];

#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			//*pparam->setPtr |= pparam->setMask;
			uint64_t *setPtr = (uint64_t *)(stageInfoPtr+pparam->setBitsOffset);
			*setPtr |= pparam->setMask;
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

		}

		//
		// Setup scalar in the EUD:
		//
		for( ; index != pcache->delegateStart; ++index)
		{
			const InputParameterCache::DwordParam *pparam = &pcache->slots[index].scalar;
			uint32_t *eud32 = (uint32_t *)(newEud + pparam->destination);
			const uint32_t reference = *(uint32_t*)(stageInfoPtr+pparam->referenceOffset);
			*eud32 = reference;
		}

		//
		// Setup pointers in the EUD:
		//
		if( pcache->eudPointerTypesMask & (1 << InputParameterCache::kPtrResourceTable) )
		{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			void* userTable = const_cast<void*>(stageInfo->userTable[kUserTableResource]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			SCE_GNM_ASSERT_MSG(userTable,"User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			if(userTable)
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			{
				void **eud64 = reinterpret_cast<void **>(newEud + pcache->eudPointers[InputParameterCache::kPtrResourceTable]);
				(*eud64) = userTable;
			}
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			else
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			{
				if ( stageInfo->dirtyRing & (1u << (31-kRingBuffersIndexResource)) )
					updateChunkState256(stageInfo->resourceStage, kResourceNumChunks);

				// Need to mark the entire resource table as used.
				stageInfo->eudResourceSet[0] = stageInfo->activeResource[0];
				stageInfo->eudResourceSet[1] = stageInfo->activeResource[1];

				void **eud64 = reinterpret_cast<void **>(newEud + pcache->eudPointers[InputParameterCache::kPtrResourceTable]);
				(*eud64) = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexResource);

				usedRingBuffer |= 1u << (31-kRingBuffersIndexResource);
				ringBufferActiveSize[kRingBuffersIndexResource] = 128 - ConstantUpdateEngineHelper::lzcnt128(stageInfo->activeResource);
			}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		}
		if( pcache->eudPointerTypesMask & (1 << InputParameterCache::kPtrSamplerTable) )
		{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			void* userTable = const_cast<void*>(stageInfo->userTable[kUserTableSampler]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			SCE_GNM_ASSERT_MSG(userTable,"User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			if(userTable)
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			{
				void **eud64 = reinterpret_cast<void **>(newEud + pcache->eudPointers[InputParameterCache::kPtrSamplerTable]);
				(*eud64) = userTable;
			}
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			else
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			{
				if ( stageInfo->dirtyRing & (1 << (31-kRingBuffersIndexSampler)) )
					updateChunkState128(stageInfo->samplerStage, kSamplerNumChunks);

				// Need to mark the entire resource table as used.
				stageInfo->eudSamplerSet = stageInfo->activeSampler;

				void **eud64 = reinterpret_cast<void **>(newEud + pcache->eudPointers[InputParameterCache::kPtrSamplerTable]);
				(*eud64) = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexSampler);

				usedRingBuffer |= 1<< (31-kRingBuffersIndexSampler);
				ringBufferActiveSize[kRingBuffersIndexSampler] = 64 - (uint16_t)__lzcnt64(stageInfo->activeSampler);
			}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		}
		if( pcache->eudPointerTypesMask & (1 << InputParameterCache::kPtrConstBufferTable) )
		{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			void* userTable = const_cast<void*>(stageInfo->userTable[kUserTableConstantBuffer]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			SCE_GNM_ASSERT_MSG(userTable,"User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			if(userTable)
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			{
				void **eud64 = reinterpret_cast<void **>(newEud + pcache->eudPointers[InputParameterCache::kPtrConstBufferTable]);
				(*eud64) = userTable;
			}
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			else
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			{
				if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexConstantBuffer)) )
					updateChunkState128(stageInfo->constantBufferStage	, kConstantBufferNumChunks);

				// Need to mark the entire resource table as used.
				stageInfo->eudConstantBufferSet = stageInfo->activeConstantBuffer;

				void **eud64 = reinterpret_cast<void **>(newEud + pcache->eudPointers[InputParameterCache::kPtrConstBufferTable]);
				(*eud64) = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexConstantBuffer);

				usedRingBuffer |= 1<< (31-kRingBuffersIndexConstantBuffer);
				ringBufferActiveSize[kRingBuffersIndexConstantBuffer] = 64 - (uint16_t)__lzcnt64(stageInfo->activeConstantBuffer);
			}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		}
		if( pcache->eudPointerTypesMask & (1 << InputParameterCache::kPtrRwResourceTable) )
		{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			void* userTable = const_cast<void*>(stageInfo->userTable[kUserTableRwResource]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			SCE_GNM_ASSERT_MSG(userTable,"User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			if(userTable)
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			{
				void **eud64 = reinterpret_cast<void **>(newEud + pcache->eudPointers[InputParameterCache::kPtrRwResourceTable]);
				(*eud64) = userTable;
			}
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			else
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			{
				if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexRwResource)) )
					updateChunkState256(stageInfo->rwResourceStage, kRwResourceNumChunks);

				// Need to mark the entire resource table as used.
				stageInfo->eudRwResourceSet = stageInfo->activeRwResource;

				void **eud64 = reinterpret_cast<void **>(newEud + pcache->eudPointers[InputParameterCache::kPtrRwResourceTable]);
				(*eud64) = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexRwResource);

				usedRingBuffer |= 1<< (31-kRingBuffersIndexRwResource);
				ringBufferActiveSize[kRingBuffersIndexRwResource] = 64 - (uint16_t)__lzcnt64(stageInfo->activeRwResource);
			}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		}
		if( pcache->eudPointerTypesMask & (1 << InputParameterCache::kPtrVertexTable) )
		{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			void* userTable = const_cast<void*>(stageInfo->userTable[kUserTableVertexBuffer]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			SCE_GNM_ASSERT_MSG(userTable,"User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			if(userTable)
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			{
				void **eud64 = reinterpret_cast<void **>(newEud + pcache->eudPointers[InputParameterCache::kPtrVertexTable]);
				(*eud64) = userTable;
			}
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			else
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			{
				void **eud64 = reinterpret_cast<void **>(newEud + pcache->eudPointers[InputParameterCache::kPtrVertexTable]);
				ringBufferActiveSize[kRingBuffersIndexVertexBuffer] = 64 - (uint16_t)__lzcnt64(stageInfo->activeVertexBuffer);

				if ( ringBufferActiveSize[kRingBuffersIndexVertexBuffer] <= 8 )
				{
					if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexVertexBuffer)) )
						updateChunkState128(stageInfo->vertexBufferStage, 1);

					SCE_GNM_ASSERT_MSG(stageInfo->vertexBufferStage[0].curChunk, "Invalid pointer.\n");
					(*eud64) = stageInfo->vertexBufferStage[0].curChunk;
				}
				else
				{
					if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexVertexBuffer)) )
						updateChunkState128(stageInfo->vertexBufferStage, kVertexBufferNumChunks);

					(*eud64) = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexVertexBuffer);
					usedRingBuffer |= 1<< (31-kRingBuffersIndexVertexBuffer);
				}
			}
#endif // SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		}
	}
	else
	{
		index = pcache->delegateStart; // if we did not populate the EUD then the param index has not advanced to the delegates portion of the cache
	}

	//
	// Execute delegates:
	//
	ApplyUsageDataState state;
	state.currentStage		   = currentStage;
	state.inputUsageTable	   = inputUsageTable;
	state.newEud			   = newEud;
	state.stageInfo			   = stageInfo;

	for ( ; index != pcache->delegateEnd; ++index )
	{
		SCE_GNM_ASSERT_MSG(pcache->slots[index].special.delegateIndex, "Invalid delegate");

		const InputParameterCache::DelegateParam				*pparam	   = &pcache->slots[index].special;
		const ConstantUpdateEngineHelper::UpdateDelegate		*tableBase = &ConstantUpdateEngineHelper::updateDelegates[0][0];

		//(this->*(pcache->delegateParams[i].fn))(pcache->delegateParams[i].slot,&state);
		(this->*(tableBase[pparam->delegateIndex]))(pparam->slot,&state);
	}


#else // SCE_GNM_CUE2_ENABLE_CACHE
	const uint32_t					 usesSrt			 = m_shaderUsesSrt & currentStageMask;
	// Populate the USGPRs:
	uint32_t iUsageSlot = 0;
	while ( iUsageSlot < inputUsageTableSize )
	{
		const uint32_t destUsgpr  = inputUsageTable[iUsageSlot].m_startRegister;

		// EUD? Early exit.
		if ( destUsgpr > 15 )
			break;

		const uint32_t srcApiSlot = inputUsageTable[iUsageSlot].m_apiSlot;
		const uint32_t regCount   = (inputUsageTable[iUsageSlot].m_registerCount*4) + 4; // 1 -> 8 DW; 0 -> 4 DW

		switch ( inputUsageTable[iUsageSlot].m_usageType )
		{
		 case Gnm::kShaderInputUsageImmResource:
			 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableResource];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 {
					 const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kResourceSizeInDqWord);
					 m_dcb->setUserDataRegion(currentStage, destUsgpr, dataPtr, regCount);
					 break;
				 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool isActive = ignoreActiveBits || ConstantUpdateEngineHelper::isBitSet(stageInfo->activeResource, srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				 const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				 if ( isActive )
				 {
					 const uint32_t			 chunkId   = srcApiSlot / kResourceChunkSize;
					 const uint32_t			 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kResourceChunkSizeMask);
					 const uint32_t			 usedCur   = stageInfo->resourceStage[chunkId].curSlots & (1 << (15-chunkSlot));
					 const __int128_t		*chunk	   = usedCur ? stageInfo->resourceStage[chunkId].curChunk : stageInfo->resourceStage[chunkId].usedChunk;
					 const uint32_t			*dataPtr   = (const uint32_t*)(chunk+chunkSlot*ConstantUpdateEngineHelper::kResourceSizeInDqWord);

					 SCE_GNM_ASSERT_MSG(ConstantUpdateEngineHelper::isBitSet(stageInfo->activeResource, srcApiSlot),
									  "The shader requires a resource in slot %u which hasn't been set.\n"
									  "If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					 m_dcb->setUserDataRegion(currentStage, destUsgpr, dataPtr, regCount);
				 }
				 else
				 {
					 m_dcb->setUserDataRegion(currentStage, destUsgpr, nullData, regCount);
				 }
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			 }
			 break;
		 case Gnm::kShaderInputUsageImmSampler:
			 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableSampler];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 {
					 const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kSamplerSizeInDqWord);
					 m_dcb->setSsharpInUserData(currentStage, destUsgpr, (Gnm::Sampler*)dataPtr);
					 break;
				 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool isActive = ignoreActiveBits || stageInfo->activeSampler & (1<<srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				 const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				 if ( isActive )
				 {
					 const uint32_t			 chunkId   = srcApiSlot / kSamplerChunkSize;
					 const uint32_t			 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kSamplerChunkSizeMask);
					 const uint32_t			 usedCur   = stageInfo->samplerStage[chunkId].curSlots & (1 << (15-chunkSlot));
					 const __int128_t		*chunk	   = usedCur ? stageInfo->samplerStage[chunkId].curChunk : stageInfo->samplerStage[chunkId].usedChunk;
					 const uint32_t			*dataPtr   = (const uint32_t*)(chunk+chunkSlot*ConstantUpdateEngineHelper::kSamplerSizeInDqWord);

					 SCE_GNM_ASSERT_MSG(stageInfo->activeSampler & (1ULL<<srcApiSlot),
									  "The shader requires a sampler in slot %u which hasn't been set.\n"
									  "If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					 m_dcb->setSsharpInUserData(currentStage, destUsgpr, (Gnm::Sampler*)dataPtr);
				 }
				 else
				 {
					 m_dcb->setSsharpInUserData(currentStage, destUsgpr, (Gnm::Sampler*)nullData);
				 }
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			 }
			 break;
		 case Gnm::kShaderInputUsageImmConstBuffer:
			 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableConstantBuffer];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 {
					 const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kConstantBufferSizeInDqWord);
					 m_dcb->setVsharpInUserData(currentStage, destUsgpr, (Gnm::Buffer*)dataPtr);
					 break;
				 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool isActive = ignoreActiveBits || stageInfo->activeConstantBuffer & (1<<srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				 const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				 if ( isActive )
				 {
					 const uint32_t			 chunkId   = srcApiSlot / kConstantBufferChunkSize;
					 const uint32_t			 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kConstantBufferChunkSizeMask);
					 const uint32_t			 usedCur   = stageInfo->constantBufferStage[chunkId].curSlots & (1 << (15-chunkSlot));
					 const __int128_t		*chunk	   = usedCur ? stageInfo->constantBufferStage[chunkId].curChunk : stageInfo->constantBufferStage[chunkId].usedChunk;
					 const uint32_t			*dataPtr   = (const uint32_t*)(chunk+chunkSlot*ConstantUpdateEngineHelper::kConstantBufferSizeInDqWord);

					 SCE_GNM_ASSERT_MSG(stageInfo->activeConstantBuffer & (1ULL<<srcApiSlot),
									  "The shader requires a constant buffer in slot %u which hasn't been set.\n"
									  "If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					 m_dcb->setVsharpInUserData(currentStage, destUsgpr, (Gnm::Buffer*)dataPtr);
				 }
				 else
				 {
					 m_dcb->setVsharpInUserData(currentStage, destUsgpr, (Gnm::Buffer*)nullData);
				 }
#endif // !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			 }
			 break;
		 case Gnm::kShaderInputUsageImmVertexBuffer:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			break;
		 case Gnm::kShaderInputUsageImmRwResource:
			 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableRwResource];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 {
					 const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kRwResourceSizeInDqWord);
					 m_dcb->setUserDataRegion(currentStage, destUsgpr, dataPtr, regCount);
					 break;
				 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 const bool isActive = ignoreActiveBits || stageInfo->activeRwResource & (1<<srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				 const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				 if ( isActive )
				 {
					 const uint32_t			 chunkId   = srcApiSlot / kRwResourceChunkSize;
					 const uint32_t			 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kRwResourceChunkSizeMask);
					 const uint32_t			 usedCur   = stageInfo->rwResourceStage[chunkId].curSlots & (1 << (15-chunkSlot));
					 const __int128_t		*chunk	   = usedCur ? stageInfo->rwResourceStage[chunkId].curChunk : stageInfo->rwResourceStage[chunkId].usedChunk;
					 const uint32_t			*dataPtr   = (const uint32_t*)(chunk+chunkSlot*ConstantUpdateEngineHelper::kRwResourceSizeInDqWord);

					 SCE_GNM_ASSERT_MSG(stageInfo->activeRwResource & (1ULL<<srcApiSlot),
									  "The shader requires a rw resource in slot %u which hasn't been set.\n"
									  "If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					 m_dcb->setUserDataRegion(currentStage, destUsgpr, dataPtr, regCount);
				 }
				 else
				 {
					 m_dcb->setUserDataRegion(currentStage, destUsgpr, nullData, regCount);
				 }
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			 }
			 break;
		 case Gnm::kShaderInputUsageImmAluFloatConst:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			break;
		 case Gnm::kShaderInputUsageImmAluBool32Const:
			m_dcb->setUserData(currentStage, destUsgpr, stageInfo->boolValue);
			break;
		 case Gnm::kShaderInputUsageImmGdsCounterRange:
			m_dcb->setUserData(currentStage, destUsgpr, stageInfo->appendConsumeDword);
			break;
		 case Gnm::kShaderInputUsageImmGdsMemoryRange:
			m_dcb->setUserData(currentStage, destUsgpr, stageInfo->gdsMemoryRangeDword);
			break;
		 case Gnm::kShaderInputUsageImmGwsBase:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			break;
		 case Gnm::kShaderInputUsageImmShaderResourceTable:
			 {
				 SCE_GNM_ASSERT_MSG(usesSrt, "non-SRT shaders should not be looking for Shader Resource Tables!");
				 const uint32_t dwCount = inputUsageTable[iUsageSlot].m_srtSizeInDWordMinusOne+1;
				 SCE_GNM_ASSERT_MSG(dwCount <= stageInfo->userSrtBufferSizeInDwords, "SRT user data size mismatch: shader expected %u DWORDS, but caller provided %u.",
									dwCount, stageInfo->userSrtBufferSizeInDwords);
				 m_dcb->setUserDataRegion(currentStage, destUsgpr, stageInfo->userSrtBuffer, dwCount);
			 }
			break;
		 case Gnm::kShaderInputUsageImmLdsEsGsSize:
			m_dcb->setUserData(currentStage, destUsgpr, m_onChipEsVertsPerSubGroup * m_onChipEsExportVertexSizeInDword * 4);
			break;
		 case Gnm::kShaderInputUsageSubPtrFetchShader:
			SCE_GNM_ASSERT_MSG(destUsgpr == 0, "Fetch shader are expected to be set in usgpr 0 -- Please check input data.");
			break;
		 case Gnm::kShaderInputUsagePtrResourceTable:
			 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 void		*userTable	=  const_cast<void*>(stageInfo->userTable[kUserTableResource]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 {
					 m_dcb->setPointerInUserData(currentStage, destUsgpr, userTable);
					 break;
				 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if ( stageInfo->dirtyRing & (1 << (31-kRingBuffersIndexResource)) )
					updateChunkState256(stageInfo->resourceStage, kResourceNumChunks);

				 void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexResource);
				 m_dcb->setPointerInUserData(currentStage, destUsgpr, ringBufferAddr);
				 usedRingBuffer |= 1<< (31-kRingBuffersIndexResource);
				 ringBufferActiveSize[kRingBuffersIndexResource] = 128 - ConstantUpdateEngineHelper::lzcnt128(stageInfo->activeResource);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			 }
			 break;
		 case Gnm::kShaderInputUsagePtrInternalResourceTable:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			break;
		 case Gnm::kShaderInputUsagePtrSamplerTable:
			 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 void		*userTable	= const_cast<void*>(stageInfo->userTable[kUserTableSampler]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 {
					 m_dcb->setPointerInUserData(currentStage, destUsgpr, userTable);
					 break;
				 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if ( stageInfo->dirtyRing & (1 << (31-kRingBuffersIndexSampler)) )
					updateChunkState128(stageInfo->samplerStage, kSamplerNumChunks);

				 void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexSampler);
				 m_dcb->setPointerInUserData(currentStage, destUsgpr, ringBufferAddr);
				 usedRingBuffer |= 1<< (31-kRingBuffersIndexSampler);
				 ringBufferActiveSize[kRingBuffersIndexSampler] = 64 - (uint16_t)__lzcnt64(stageInfo->activeSampler);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			 }
			 break;
		 case Gnm::kShaderInputUsagePtrConstBufferTable:
			 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 void	*userTable	= const_cast<void*>(stageInfo->userTable[kUserTableConstantBuffer]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 {
					 m_dcb->setPointerInUserData(currentStage, destUsgpr, userTable);
					 break;
				 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexConstantBuffer)) )
					 updateChunkState128(stageInfo->constantBufferStage	, kConstantBufferNumChunks);

				 void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexConstantBuffer);
				 m_dcb->setPointerInUserData(currentStage, destUsgpr, ringBufferAddr);
				 usedRingBuffer |= 1<< (31-kRingBuffersIndexConstantBuffer);
				 ringBufferActiveSize[kRingBuffersIndexConstantBuffer] = 64 - (uint16_t)__lzcnt64(stageInfo->activeConstantBuffer);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			 }
			 break;
		 case Gnm::kShaderInputUsagePtrVertexBufferTable:
			 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 void	*userTable	= const_cast<void*>(stageInfo->userTable[kUserTableVertexBuffer]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 {
					 m_dcb->setPointerInUserData(currentStage, destUsgpr, userTable);
					 break;
				 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 ringBufferActiveSize[kRingBuffersIndexVertexBuffer] = 64 - (uint16_t)__lzcnt64(stageInfo->activeVertexBuffer);

				 if ( ringBufferActiveSize[kRingBuffersIndexVertexBuffer] <= 8 )
				 {
					 if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexVertexBuffer)) )
						 updateChunkState128(stageInfo->vertexBufferStage, 1);

					 SCE_GNM_ASSERT_MSG(stageInfo->vertexBufferStage[0].curChunk, "Invalid pointer.");
					 m_dcb->setPointerInUserData(currentStage, destUsgpr, stageInfo->vertexBufferStage[0].curChunk);
				 }
				 else
				 {
					 if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexVertexBuffer)) )
						 updateChunkState128(stageInfo->vertexBufferStage, kVertexBufferNumChunks);

					 void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexVertexBuffer);
					 m_dcb->setPointerInUserData(currentStage, destUsgpr, ringBufferAddr);
					 usedRingBuffer |= 1<< (31-kRingBuffersIndexVertexBuffer);
				 }
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			 }
			 break;
		 case Gnm::kShaderInputUsagePtrSoBufferTable:
			 {
				 updateChunkState128(&m_streamoutBufferStage, 1);
				 m_dcb->setPointerInUserData(currentStage, destUsgpr, m_streamoutBufferStage.curChunk);
			 }
			break;
		 case Gnm::kShaderInputUsagePtrRwResourceTable:
			 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				 void	*userTable	= const_cast<void*>(stageInfo->userTable[kUserTableRwResource]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 {
					 m_dcb->setPointerInUserData(currentStage, destUsgpr, userTable);
					 break;
				 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexRwResource)) )
					 updateChunkState256(stageInfo->rwResourceStage, kRwResourceNumChunks);

				 void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexRwResource);
				 m_dcb->setPointerInUserData(currentStage, destUsgpr, ringBufferAddr);
				 usedRingBuffer |= 1<< (31-kRingBuffersIndexRwResource);
				 ringBufferActiveSize[kRingBuffersIndexRwResource] = 64 - (uint16_t)__lzcnt64(stageInfo->activeRwResource);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			 }
			 break;
		 case Gnm::kShaderInputUsagePtrInternalGlobalTable:
			 {
#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
				 updateChunkState128(&m_globalTableStage,1);
				 m_dcb->setPointerInUserData(currentStage,destUsgpr,m_globalTableStage.curChunk);
#else //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
				 SCE_GNM_ASSERT_MSG(m_globalTableAddr, "Global table pointer not specified. Call setGlobalResourceTableAddr() first!");
				 m_dcb->setPointerInUserData(currentStage, destUsgpr, m_globalTableAddr);
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
			 }
			 break;

		 case Gnm::kShaderInputUsagePtrExtendedUserData:
			 {
				 if(newEud)
				 {
					 m_dcb->setPointerInUserData(currentStage, destUsgpr, newEud);
				 }
			 }
			 break;

		 case Gnm::kShaderInputUsageImmGdsKickRingBufferOffset:
		 case Gnm::kShaderInputUsageImmVertexRingBufferOffset:
			 //nothing to do
			 break;
		 case Gnm::kShaderInputUsagePtrDispatchDraw:
			 {
				 SCE_GNM_ASSERT_MSG(m_pDispatchDrawData, "DispatchDrawData pointer not specified. Call setDispatchDrawData() first!");
				 m_dcb->setPointerInUserData(currentStage, destUsgpr, (void*)m_pDispatchDrawData);
			 }
			 break;
		 case Gnm::kShaderInputUsageImmDispatchDrawInstances:
			 //nothing to do
			 break;

		 case Gnm::kShaderInputUsagePtrIndirectResourceTable:
		 case Gnm::kShaderInputUsagePtrIndirectInternalResourceTable:
		 case Gnm::kShaderInputUsagePtrIndirectRwResourceTable:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			break;
		 default:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
		}

		iUsageSlot++;
	}

	// EUD Population:
	if ( newEud )
	{
		while ( iUsageSlot < inputUsageTableSize)
		{
			const uint32_t destEudNdx = inputUsageTable[iUsageSlot].m_startRegister - 16;
			const uint32_t srcApiSlot = inputUsageTable[iUsageSlot].m_apiSlot;
			const uint32_t regCount   = (inputUsageTable[iUsageSlot].m_registerCount+1) * 4; // 1 -> 8 DW; 0 -> 4 DW

			switch ( inputUsageTable[iUsageSlot].m_usageType )
			{
			 case Gnm::kShaderInputUsageImmResource:
				 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableResource];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 {
						 const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kResourceSizeInDqWord);
						 __int128_t *eud128 = (__int128_t *)(newEud + destEudNdx);
						 eud128[0] = dataPtr[0];
						 if ( regCount == 8 )
							 eud128[1] = dataPtr[1];
						 break;
					 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 const __int128_t *dataPtr;
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool isActive = ignoreActiveBits || ConstantUpdateEngineHelper::isBitSet(stageInfo->activeResource, srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
					 const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
					 if ( isActive )
					 {
						 const uint32_t		 chunkId   = srcApiSlot / kResourceChunkSize;
						 const uint32_t		 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kResourceChunkSizeMask);
						 const uint32_t		 usedCur   = stageInfo->resourceStage[chunkId].curSlots & (1 << (15-chunkSlot));
						 const __int128_t	*chunk	   = usedCur ? stageInfo->resourceStage[chunkId].curChunk : stageInfo->resourceStage[chunkId].usedChunk;
						 dataPtr   = chunk+chunkSlot*ConstantUpdateEngineHelper::kResourceSizeInDqWord;
						 SCE_GNM_ASSERT_MSG(ConstantUpdateEngineHelper::isBitSet(stageInfo->activeResource, srcApiSlot),
										  "The shader requires a resource in slot %u which hasn't been set.\n"
										  "If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					 }
					 else
					 {
						 dataPtr = (const __int128_t *)nullData;
					 }

					 __int128_t *eud128 = (__int128_t *)(newEud + destEudNdx);
					 eud128[0] = dataPtr[0];
					 if ( regCount == 8 )
						 eud128[1] = dataPtr[1];

					 stageInfo->eudResourceSet[srcApiSlot>>6] |= (((uint64_t)1) << (srcApiSlot & 63));
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 }
				 break;
			 case Gnm::kShaderInputUsageImmSampler:
				 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableSampler];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 {
						 const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kSamplerSizeInDqWord);
						 __int128_t *eud128 = (__int128_t *)(newEud + destEudNdx);
						 eud128[0] = dataPtr[0];
						 break;
					 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 const __int128_t *dataPtr;
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool isActive = ignoreActiveBits || stageInfo->activeSampler & (1<<srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
					 const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
					 if ( isActive )
					 {
						 const uint32_t		 chunkId   = srcApiSlot / kSamplerChunkSize;
						 const uint32_t		 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kSamplerChunkSizeMask);
						 const uint32_t		 usedCur   = stageInfo->samplerStage[chunkId].curSlots & (1 << (15-chunkSlot));
						 const __int128_t	*chunk	   = usedCur ? stageInfo->samplerStage[chunkId].curChunk : stageInfo->samplerStage[chunkId].usedChunk;
						 dataPtr   = chunk+chunkSlot*ConstantUpdateEngineHelper::kSamplerSizeInDqWord;
						 SCE_GNM_ASSERT_MSG(stageInfo->activeSampler & (1ULL<<srcApiSlot),
										  "The shader requires a sampler in slot %u which hasn't been set.\n"
										  "If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					 }
					 else
					 {
						 dataPtr = (const __int128_t *)nullData;
					 }

					 __int128_t *eud128 = (__int128_t *)(newEud + destEudNdx);
					 eud128[0] = dataPtr[0];

					 stageInfo->eudSamplerSet |= (1ULL << srcApiSlot);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 }
				 break;
			 case Gnm::kShaderInputUsageImmConstBuffer:
				 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableConstantBuffer];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 {
						 const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kConstantBufferSizeInDqWord);
						 __int128_t *eud128 = (__int128_t *)(newEud + destEudNdx);
						 eud128[0] = dataPtr[0];
						 break;
					 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 const __int128_t *dataPtr;
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool isActive = ignoreActiveBits || stageInfo->activeConstantBuffer & (1<<srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
					 const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
					 if ( isActive )
					 {
						 const uint32_t		 chunkId   = srcApiSlot / kConstantBufferChunkSize;
						 const uint32_t		 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kConstantBufferChunkSizeMask);
						 const uint32_t		 usedCur   = stageInfo->constantBufferStage[chunkId].curSlots & (1 << (15-chunkSlot));
						 const __int128_t	*chunk	   = usedCur ? stageInfo->constantBufferStage[chunkId].curChunk : stageInfo->constantBufferStage[chunkId].usedChunk;
						 dataPtr   = chunk+chunkSlot*ConstantUpdateEngineHelper::kConstantBufferSizeInDqWord;
						 SCE_GNM_ASSERT_MSG(stageInfo->activeConstantBuffer & (1ULL<<srcApiSlot),
										  "The shader requires a constant buffer in slot %u which hasn't been set.\n"
										  "If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					 }
					 else
					 {
						 dataPtr = (const __int128_t *)nullData;
					 }

					 __int128_t *eud128 = (__int128_t *)(newEud + destEudNdx);
					 eud128[0] = dataPtr[0];

					 stageInfo->eudConstantBufferSet |= (1ULL << srcApiSlot);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 }
				 break;
			 case Gnm::kShaderInputUsageImmVertexBuffer:
				SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
				break;
			 case Gnm::kShaderInputUsageImmRwResource:
				 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableRwResource];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 {
						 const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kRwResourceSizeInDqWord);
						 __int128_t *eud128 = (__int128_t *)(newEud + destEudNdx);
						 eud128[0] = dataPtr[0];
						 if ( regCount == 8 )
							 eud128[1] = dataPtr[1];
						 break;
					 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 const __int128_t *dataPtr;
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 const bool isActive = ignoreActiveBits || stageInfo->activeRwResource & (1<<srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
					 const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
					 if ( isActive )
					 {
						 const uint32_t		 chunkId   = srcApiSlot / kRwResourceChunkSize;
						 const uint32_t		 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kRwResourceChunkSizeMask);
						 const uint32_t		 usedCur   = stageInfo->rwResourceStage[chunkId].curSlots & (1 << (15-chunkSlot));
						 const __int128_t	*chunk	   = usedCur ? stageInfo->rwResourceStage[chunkId].curChunk : stageInfo->rwResourceStage[chunkId].usedChunk;
						 dataPtr   = chunk+chunkSlot*ConstantUpdateEngineHelper::kRwResourceSizeInDqWord;
						 SCE_GNM_ASSERT_MSG(stageInfo->activeRwResource & (1ULL<<srcApiSlot),
										  "The shader requires a rw resource in slot %u which hasn't been set.\n"
										  "If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					 }
					 else
					 {
						 dataPtr = (const __int128_t *)nullData;
					 }

					 __int128_t *eud128 = (__int128_t *)(newEud + destEudNdx);
					 eud128[0] = dataPtr[0];
					 if ( regCount == 8 )
						 eud128[1] = dataPtr[1];

					 stageInfo->eudRwResourceSet |= (1ULL << srcApiSlot);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 }
				 break;
			 case Gnm::kShaderInputUsageImmAluFloatConst:
				SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
				break;
			 case Gnm::kShaderInputUsageImmAluBool32Const:
				newEud[destEudNdx] = stageInfo->boolValue;
				break;
			 case Gnm::kShaderInputUsageImmGdsCounterRange:
				{
					uint32_t *eud32 = (uint32_t *)(newEud + destEudNdx);
					*eud32 = stageInfo->appendConsumeDword;
				}
				break;
			 case Gnm::kShaderInputUsageImmGdsMemoryRange:
				{
					uint32_t *eud32 = (uint32_t *)(newEud + destEudNdx);
					*eud32 = stageInfo->gdsMemoryRangeDword;
				}
				break;
			 case Gnm::kShaderInputUsageImmGwsBase:
				SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
				break;
			 case Gnm::kShaderInputUsageImmShaderResourceTable:
				SCE_GNM_ERROR("non-SRT shaders should not be looking for Shader Resource Tables!");
				break;

			 case Gnm::kShaderInputUsageImmLdsEsGsSize:
				{
					uint32_t *eud32 = (uint32_t *)(newEud + destEudNdx);
					*eud32 = m_onChipEsVertsPerSubGroup * m_onChipEsExportVertexSizeInDword * 4;
				}
				break;

			 case Gnm::kShaderInputUsageSubPtrFetchShader:
				SCE_GNM_ERROR("Fetch shader are expected to be set in usgpr 0 -- Please check input data.");
				break;

			 case Gnm::kShaderInputUsagePtrResourceTable:
				 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 void	*userTable	= const_cast<void*>(stageInfo->userTable[kUserTableResource]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 {
						 void **eud64 = (void **)(newEud + destEudNdx);
						 (*eud64) = userTable;
						 break;
					 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if ( stageInfo->dirtyRing & (1 << (31-kRingBuffersIndexResource)) )
						 updateChunkState256(stageInfo->resourceStage, kResourceNumChunks);

					 // Need to mark the entire resource table as used.
					 stageInfo->eudResourceSet[0] = stageInfo->activeResource[0];
					 stageInfo->eudResourceSet[1] = stageInfo->activeResource[1];

					 void **eud64 = (void **)(newEud + destEudNdx);
					 (*eud64) = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexResource);

					 usedRingBuffer |= 1<< (31-kRingBuffersIndexResource);
					 ringBufferActiveSize[kRingBuffersIndexResource] = 128 - ConstantUpdateEngineHelper::lzcnt128(stageInfo->activeResource);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 }
				 break;
			 case Gnm::kShaderInputUsagePtrInternalResourceTable:
				SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
				break;
			 case Gnm::kShaderInputUsagePtrSamplerTable:
				 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 void *userTable	= const_cast<void*>(stageInfo->userTable[kUserTableSampler]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 {
						 void **eud64 = (void **)(newEud + destEudNdx);
						 (*eud64) = userTable;
						 break;
					 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if ( stageInfo->dirtyRing & (1 << (31-kRingBuffersIndexSampler)) )
						 updateChunkState128(stageInfo->samplerStage, kSamplerNumChunks);

					 // Need to mark the entire resource table as used.
					 stageInfo->eudSamplerSet = stageInfo->activeSampler;

					 void **eud64 = (void **)(newEud + destEudNdx);
					 (*eud64) = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexSampler);

					 usedRingBuffer |= 1<< (31-kRingBuffersIndexSampler);
					 ringBufferActiveSize[kRingBuffersIndexSampler] = 64 - (uint16_t)__lzcnt64(stageInfo->activeSampler);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 }
				 break;
			 case Gnm::kShaderInputUsagePtrConstBufferTable:
				 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 void *userTable	= const_cast<void*>(stageInfo->userTable[kUserTableConstantBuffer]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 {
						 void **eud64 = (void **)(newEud + destEudNdx);
						 (*eud64) = userTable;
						 break;
					 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexConstantBuffer)) )
						 updateChunkState128(stageInfo->constantBufferStage	, kConstantBufferNumChunks);

					 // Need to mark the entire resource table as used.
					 stageInfo->eudConstantBufferSet = stageInfo->activeConstantBuffer;

					 void **eud64 = (void **)(newEud + destEudNdx);
					 (*eud64) = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexConstantBuffer);

					 usedRingBuffer |= 1<< (31-kRingBuffersIndexConstantBuffer);
					 ringBufferActiveSize[kRingBuffersIndexConstantBuffer] = 64 - (uint16_t)__lzcnt64(stageInfo->activeConstantBuffer);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 }
				 break;
			 case Gnm::kShaderInputUsagePtrVertexBufferTable:
				 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 void *userTable	= const_cast<void*>(stageInfo->userTable[kUserTableVertexBuffer]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 {
						 void **eud64 = (void **)(newEud + destEudNdx);
						 (*eud64) = userTable;
						 break;
					 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 void **eud64 = (void **)(newEud + destEudNdx);
					 ringBufferActiveSize[kRingBuffersIndexVertexBuffer] = 64 - (uint16_t)__lzcnt64(stageInfo->activeVertexBuffer);

					 if ( ringBufferActiveSize[kRingBuffersIndexVertexBuffer] <= 8 )
					 {
						 if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexVertexBuffer)) )
							 updateChunkState128(stageInfo->vertexBufferStage, 1);

						 SCE_GNM_ASSERT_MSG(stageInfo->vertexBufferStage[0].curChunk, "Invalid pointer.");
						 (*eud64) = stageInfo->vertexBufferStage[0].curChunk;
					 }
					 else
					 {
						 if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexVertexBuffer)) )
							 updateChunkState128(stageInfo->vertexBufferStage, kVertexBufferNumChunks);

						 (*eud64) = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexVertexBuffer);
						 usedRingBuffer |= 1<< (31-kRingBuffersIndexVertexBuffer);
					 }
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 }
				 break;
			 case Gnm::kShaderInputUsagePtrSoBufferTable:
				 {
					 void **eud64 = (void **)(newEud + destEudNdx);
					 updateChunkState128(&m_streamoutBufferStage, 1);
					 (*eud64) = m_streamoutBufferStage.curChunk;
					 m_eudReferencesStreamoutBuffers = true;
				 }
				 break;
			 case Gnm::kShaderInputUsagePtrRwResourceTable:
				 {
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
					 void *userTable	= const_cast<void*>(stageInfo->userTable[kUserTableRwResource]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 {
						 void **eud64 = (void **)(newEud + destEudNdx);
						 (*eud64) = userTable;
						 break;
					 }
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					 if ( stageInfo->dirtyRing & (1<< (31-kRingBuffersIndexRwResource)) )
						 updateChunkState256(stageInfo->rwResourceStage, kRwResourceNumChunks);

					 // Need to mark the entire resource table as used.
					 stageInfo->eudRwResourceSet = stageInfo->activeRwResource;

					 void **eud64 = (void **)(newEud + destEudNdx);
					 (*eud64) = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexRwResource);

					 usedRingBuffer |= 1<< (31-kRingBuffersIndexRwResource);
					 ringBufferActiveSize[kRingBuffersIndexRwResource] = (uint16_t)(64 - __lzcnt64(stageInfo->activeRwResource));
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				 }
				 break;
			 case Gnm::kShaderInputUsagePtrInternalGlobalTable:
				 {
					 void **eud64 = (void **)(newEud + destEudNdx);
#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
					 updateChunkState128(&m_globalTableStage,1);
					 m_eudReferencesGlobalTable |= (1<<currentStage);
					 (*eud64) = m_globalTableStage.curChunk;
#else //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
					 SCE_GNM_ASSERT_MSG(m_globalTableAddr, "Global table pointer not specified. Call setGlobalResourceTableAddr() first!");
					 (*eud64) = m_globalTableAddr;
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
				 }
				 break;
			 case Gnm::kShaderInputUsagePtrExtendedUserData:
				SCE_GNM_ERROR("EUD withing EUD is not supported.");
				break;
			 case Gnm::kShaderInputUsagePtrDispatchDraw:
				SCE_GNM_ERROR("PtrDispatchDraw withing EUD is not supported.");
				break;
			 case Gnm::kShaderInputUsagePtrIndirectResourceTable:
			 case Gnm::kShaderInputUsagePtrIndirectInternalResourceTable:
			 case Gnm::kShaderInputUsagePtrIndirectRwResourceTable:
				SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
				break;
			 default:
				SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			}
			iUsageSlot++;
		}
	}
#endif // SCE_GNM_CUE2_ENABLE_CACHE

#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	// Update the ring buffer as needed:
	const uint16_t cpRamStageOffset = (uint16_t)(currentStage*ConstantUpdateEngineHelper::kPerStageDwordSize);
	uint32_t ringToUpdate = usedRingBuffer & stageInfo->dirtyRing;

	if ( ringToUpdate )
	{
		while ( ringToUpdate )
		{
			const uint32_t ndx = __lzcnt32(ringToUpdate); // 0 represent the high bit (0x8000 0000)
			const uint32_t bit = 1 << (31 - ndx);
			const uint32_t clr = ~bit;

			//

			const uint16_t numResourcesToCopy = ringBufferActiveSize[ndx];

			const uint16_t cpRamOffset = cpRamStageOffset + ConstantUpdateEngineHelper::ringBufferOffsetPerStageInCpRam[ndx];
			const uint16_t sizeInDw    = ConstantUpdateEngineHelper::slotSizeInDword[ndx] * numResourcesToCopy;
			void           *ringAddr   = ConstantUpdateEngineHelper::getRingBuffersNextHead(&stageInfo->ringBuffers[ndx]);

			m_ccb->cpRamDump(ringAddr, cpRamOffset*4, sizeInDw);
			const bool hasWrapped = ConstantUpdateEngineHelper::advanceRingBuffersHead(&stageInfo->ringBuffers[ndx]);
			m_anyWrapped = m_anyWrapped || hasWrapped;

			//

			stageInfo->dirtyRing &= clr;
			ringToUpdate &= clr;
		}
		m_usingCcb = true;
	}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::applyInputUsageDataForDispatchDrawCompute()
{
	// dispatch draw asynchronous compute always shares data in rings or other memory with graphics VS stage:
	Gnm::ShaderStage currentStage = Gnm::kShaderStageVs;
	StageInfo * const		stageInfo		 = m_stageInfo+currentStage;
	const uint8_t			currentStageMask = 1 << currentStage;
	const uint32_t			usesSrtVs = m_shaderUsesSrt & currentStageMask;

	StageInfo * const		stageInfoCs		 = m_stageInfo+kShaderStageAsynchronousCompute;
	const Gnm::InputUsageSlot		*inputUsageTable     = stageInfoCs->inputUsageTable;
	const uint32_t					 inputUsageTableSize = stageInfoCs->inputUsageTableSize;
	const uint32_t					 usesSrt			 = m_shaderUsesSrt & (1 << kShaderStageAsynchronousCompute);

	SCE_GNM_ASSERT_MSG( inputUsageTableSize, "This function should not be called if inputUsageTableSize is 0.");
	SCE_GNM_ASSERT_MSG( stageInfoCs->eudSizeInDWord <= stageInfo->eudSizeInDWord, "Mismatch between dispatch draw asynchronous compute shader and VS shader EUD size" );
	void *newEud = stageInfo->eudAddr;

#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	static const uint32_t nullData[8] = {};
#endif // !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

	// Build the ring address tables:
//	uint16_t ringBufferActiveSize[kNumRingBuffersPerStage];

	// Populate the USGPRs:
	uint32_t iUsageSlot = 0;
	while ( iUsageSlot < inputUsageTableSize )
	{
		const uint32_t destUsgpr  = inputUsageTable[iUsageSlot].m_startRegister;

		// EUD? Early exit.
		if ( destUsgpr > 15 )
			break;

		const uint32_t srcApiSlot = inputUsageTable[iUsageSlot].m_apiSlot;
		const uint32_t regCount   = (inputUsageTable[iUsageSlot].m_registerCount*4) + 4; // 1 -> 8 DW; 0 -> 4 DW

		switch ( inputUsageTable[iUsageSlot].m_usageType )
		{
		case Gnm::kShaderInputUsageImmResource:
			{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableResource];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				{
					const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kResourceSizeInDqWord);
					m_acb->setUserDataRegion(destUsgpr, dataPtr, regCount);
					break;
				}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool isActive = ignoreActiveBits || ConstantUpdateEngineHelper::isBitSet(stageInfo->activeResource, srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				if ( isActive )
				{
					const uint32_t			 chunkId   = srcApiSlot / kResourceChunkSize;
					const uint32_t			 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kResourceChunkSizeMask);
					const uint32_t			 usedCur   = stageInfo->resourceStage[chunkId].curSlots & (1 << (15-chunkSlot));
					const __int128_t		*chunk	   = usedCur ? stageInfo->resourceStage[chunkId].curChunk : stageInfo->resourceStage[chunkId].usedChunk;
					const uint32_t			*dataPtr   = (const uint32_t*)(chunk+chunkSlot*ConstantUpdateEngineHelper::kResourceSizeInDqWord);

					SCE_GNM_ASSERT_MSG(ConstantUpdateEngineHelper::isBitSet(stageInfo->activeResource, srcApiSlot),
						"The shader requires a resource in VS slot %u which hasn't been set.\n"
						"If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					m_acb->setUserDataRegion(destUsgpr, dataPtr, regCount);
				}
				else
				{
					m_acb->setUserDataRegion(destUsgpr, nullData, regCount);
				}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			}
			break;
		case Gnm::kShaderInputUsageImmSampler:
			{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableSampler];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				{
					const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kSamplerSizeInDqWord);
					m_acb->setSsharpInUserData(destUsgpr, (Gnm::Sampler*)dataPtr);
					break;
				}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool isActive = ignoreActiveBits || stageInfo->activeSampler & (1<<srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				if ( isActive )
				{
					const uint32_t			 chunkId   = srcApiSlot / kSamplerChunkSize;
					const uint32_t			 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kSamplerChunkSizeMask);
					const uint32_t			 usedCur   = stageInfo->samplerStage[chunkId].curSlots & (1 << (15-chunkSlot));
					const __int128_t		*chunk	   = usedCur ? stageInfo->samplerStage[chunkId].curChunk : stageInfo->samplerStage[chunkId].usedChunk;
					const uint32_t			*dataPtr   = (const uint32_t*)(chunk+chunkSlot*ConstantUpdateEngineHelper::kSamplerSizeInDqWord);

					SCE_GNM_ASSERT_MSG(stageInfo->activeSampler & (1ULL<<srcApiSlot),
						"The shader requires a sampler in VS slot %u which hasn't been set.\n"
						"If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					m_acb->setSsharpInUserData(destUsgpr, (Gnm::Sampler*)dataPtr);
				}
				else
				{
					m_acb->setSsharpInUserData(destUsgpr, (Gnm::Sampler*)nullData);
				}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			}
			break;
		case Gnm::kShaderInputUsageImmConstBuffer:
			{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableConstantBuffer];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				{
					const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kConstantBufferSizeInDqWord);
					m_acb->setVsharpInUserData(destUsgpr, (Gnm::Buffer*)dataPtr);
					break;
				}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool isActive = ignoreActiveBits || stageInfo->activeConstantBuffer & (1<<srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				if ( isActive )
				{
					const uint32_t			 chunkId   = srcApiSlot / kConstantBufferChunkSize;
					const uint32_t			 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kConstantBufferChunkSizeMask);
					const uint32_t			 usedCur   = stageInfo->constantBufferStage[chunkId].curSlots & (1 << (15-chunkSlot));
					const __int128_t		*chunk	   = usedCur ? stageInfo->constantBufferStage[chunkId].curChunk : stageInfo->constantBufferStage[chunkId].usedChunk;
					const uint32_t			*dataPtr   = (const uint32_t*)(chunk+chunkSlot*ConstantUpdateEngineHelper::kConstantBufferSizeInDqWord);

					SCE_GNM_ASSERT_MSG(stageInfo->activeConstantBuffer & (1ULL<<srcApiSlot),
						"The shader requires a constant buffer in VS slot %u which hasn't been set.\n"
						"If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					m_acb->setVsharpInUserData(destUsgpr, (Gnm::Buffer*)dataPtr);
				}
				else
				{
					m_acb->setVsharpInUserData(destUsgpr, (Gnm::Buffer*)nullData);
				}
#endif // !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			}
			break;
		case Gnm::kShaderInputUsageImmVertexBuffer:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			break;
		case Gnm::kShaderInputUsageImmRwResource:
			{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const __int128_t		*userTable	= (const __int128_t*) stageInfo->userTable[kUserTableRwResource];
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				{
					const uint32_t			*dataPtr   = (const uint32_t*)(userTable+srcApiSlot*ConstantUpdateEngineHelper::kRwResourceSizeInDqWord);
					m_acb->setUserDataRegion(destUsgpr, dataPtr, regCount);
					break;
				}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool ignoreActiveBits = userTable;
#else //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool ignoreActiveBits = false;
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const bool isActive = ignoreActiveBits || stageInfo->activeRwResource & (1<<srcApiSlot);
#else //SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				const bool isActive = true;
#endif//SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
				if ( isActive )
				{
					const uint32_t			 chunkId   = srcApiSlot / kRwResourceChunkSize;
					const uint32_t			 chunkSlot = srcApiSlot & (ConstantUpdateEngineHelper::kRwResourceChunkSizeMask);
					const uint32_t			 usedCur   = stageInfo->rwResourceStage[chunkId].curSlots & (1 << (15-chunkSlot));
					const __int128_t		*chunk	   = usedCur ? stageInfo->rwResourceStage[chunkId].curChunk : stageInfo->rwResourceStage[chunkId].usedChunk;
					const uint32_t			*dataPtr   = (const uint32_t*)(chunk+chunkSlot*ConstantUpdateEngineHelper::kRwResourceSizeInDqWord);

					SCE_GNM_ASSERT_MSG(stageInfo->activeRwResource & (1ULL<<srcApiSlot),
						"The shader requires a rw resource in VS slot %u which hasn't been set.\n"
						"If this is an expected behavior, please define: SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES.", srcApiSlot);
					m_acb->setUserDataRegion(destUsgpr, dataPtr, regCount);
				}
				else
				{
					m_acb->setUserDataRegion(destUsgpr, nullData, regCount);
				}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			}
			break;
		case Gnm::kShaderInputUsageImmAluFloatConst:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			break;
		case Gnm::kShaderInputUsageImmAluBool32Const:
			m_acb->setUserData(destUsgpr, stageInfo->boolValue);
			break;
		case Gnm::kShaderInputUsageImmGdsCounterRange:
			m_acb->setUserData(destUsgpr, stageInfo->appendConsumeDword);
			break;
		case Gnm::kShaderInputUsageImmGdsMemoryRange:
			m_acb->setUserData(destUsgpr, stageInfo->gdsMemoryRangeDword);
			break;
		case Gnm::kShaderInputUsageImmGwsBase:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			break;
		case Gnm::kShaderInputUsageImmShaderResourceTable:
			{
				SCE_GNM_ASSERT_MSG(usesSrt, "non-SRT shaders should not be looking for Shader Resource Tables!");
				const uint32_t dwCount = inputUsageTable[iUsageSlot].m_srtSizeInDWordMinusOne+1;
				SCE_GNM_ASSERT_MSG(!usesSrt || usesSrtVs, "Mismatch between dispatch draw asynchronous compute shader and VS shader InputUsage table SRT usage");
				SCE_GNM_ASSERT_MSG(dwCount <= stageInfo->userSrtBufferSizeInDwords, "SRT user data size mismatch: dispatch draw CS shader expected %u DWORDS, but caller provided %u to VS stage.", dwCount, stageInfoCs->userSrtBufferSizeInDwords);
				m_acb->setUserDataRegion(destUsgpr, stageInfo->userSrtBuffer, dwCount);
			}
			break;
		case Gnm::kShaderInputUsageImmLdsEsGsSize:
			SCE_GNM_ERROR("OnChip Gs is not supported by DispatchDraw!");
			break;
		case Gnm::kShaderInputUsageSubPtrFetchShader:
			SCE_GNM_ASSERT_MSG(destUsgpr == 0, "Fetch shader are expected to be set in usgpr 0 -- Please check input data.");
			break;
		case Gnm::kShaderInputUsagePtrResourceTable:
			{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				void		*userTable	=  const_cast<void*>(stageInfo->userTable[kUserTableResource]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				{
					m_acb->setPointerInUserData( destUsgpr, userTable);
					break;
				}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				//FIXME: should validate that CS doesn't use any chunks that VS does not?
				void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexResource);
				m_acb->setPointerInUserData(destUsgpr, ringBufferAddr);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			}
			break;
		case Gnm::kShaderInputUsagePtrInternalResourceTable:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			break;
		case Gnm::kShaderInputUsagePtrSamplerTable:
			{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				void		*userTable	= const_cast<void*>(stageInfo->userTable[kUserTableSampler]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				{
					m_acb->setPointerInUserData(destUsgpr, userTable);
					break;
				}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				//FIXME: should validate that CS doesn't use any chunks that VS does not?
				void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexSampler);
				m_acb->setPointerInUserData(destUsgpr, ringBufferAddr);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			}
			break;
		case Gnm::kShaderInputUsagePtrConstBufferTable:
			{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				void	*userTable	= const_cast<void*>(stageInfo->userTable[kUserTableConstantBuffer]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				{
					m_acb->setPointerInUserData(destUsgpr, userTable);
					break;
				}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				//FIXME: should validate that CS doesn't use any chunks that VS does not?
				void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexConstantBuffer);
				m_acb->setPointerInUserData(destUsgpr, ringBufferAddr);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			}
			break;
		case Gnm::kShaderInputUsagePtrVertexBufferTable:
			{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				void	*userTable	= const_cast<void*>(stageInfo->userTable[kUserTableVertexBuffer]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				{
					m_acb->setPointerInUserData(destUsgpr, userTable);
					break;
				}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				//FIXME: should validate that stageInfo doesn't use any chunks that stageInfoVs does not?
				uint32_t ringBufferActiveSize_VertexBuffer = 64 - (uint16_t)__lzcnt64(stageInfo->activeVertexBuffer);

				if ( ringBufferActiveSize_VertexBuffer <= 8 )
				{
					SCE_GNM_ASSERT_MSG(stageInfo->vertexBufferStage[0].curChunk, "Invalid pointer.");
					m_acb->setPointerInUserData(destUsgpr, stageInfo->vertexBufferStage[0].curChunk);
				}
				else
				{
					void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexVertexBuffer);
					m_acb->setPointerInUserData(destUsgpr, ringBufferAddr);
				}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			}
			break;
		case Gnm::kShaderInputUsagePtrSoBufferTable:
			{
				//FIXME: should validate that CS doesn't use any chunks that VS does not?
				m_acb->setPointerInUserData(destUsgpr, m_streamoutBufferStage.curChunk);
			}
			break;
		case Gnm::kShaderInputUsagePtrRwResourceTable:
			{
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				void	*userTable	= const_cast<void*>(stageInfo->userTable[kUserTableRwResource]);
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				SCE_GNM_ASSERT_MSG( userTable, "User table is not set");
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				if(userTable)
#endif//SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				{
					m_acb->setPointerInUserData(destUsgpr, userTable);
					break;
				}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				//FIXME: should validate that CS doesn't use any chunks that VS does not?
				void * const ringBufferAddr = ConstantUpdateEngineHelper::getRingAddress(stageInfo, kRingBuffersIndexRwResource);
				m_acb->setPointerInUserData(destUsgpr, ringBufferAddr);
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
			}
			break;
		case Gnm::kShaderInputUsagePtrInternalGlobalTable:
			{
#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
				m_acb->setPointerInUserData(destUsgpr, m_globalTableStage.curChunk);
#else //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
				SCE_GNM_ASSERT_MSG(m_globalTableAddr, "Global table pointer not specified. Call setGlobalResourceTableAddr() first!");
				m_acb->setPointerInUserData(destUsgpr, m_globalTableAddr);
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
			}
			break;

		case Gnm::kShaderInputUsagePtrExtendedUserData:
			{
				SCE_GNM_ASSERT_MSG(newEud, "EUD is required, but the pointer is NULL");
				m_acb->setPointerInUserData(destUsgpr, newEud);
			}
			break;

		case Gnm::kShaderInputUsageImmGdsKickRingBufferOffset:
		case Gnm::kShaderInputUsageImmVertexRingBufferOffset:
			//nothing to do
			break;
		case Gnm::kShaderInputUsagePtrDispatchDraw:
			{
				SCE_GNM_ASSERT_MSG(m_pDispatchDrawData, "DispatchDrawData pointer not specified. Call setDispatchDrawData() first!");
				m_acb->setPointerInUserData(destUsgpr, (void*)m_pDispatchDrawData);
			}
			break;
		case Gnm::kShaderInputUsageImmDispatchDrawInstances:
			//nothing to do
			break;

		case Gnm::kShaderInputUsagePtrIndirectResourceTable:
		case Gnm::kShaderInputUsagePtrIndirectInternalResourceTable:
		case Gnm::kShaderInputUsagePtrIndirectRwResourceTable:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
			break;
		default:
			SCE_GNM_ERROR("Unsupported shader input usage type 0x%02X.", inputUsageTable[iUsageSlot].m_usageType); // not supported yet
		}

		iUsageSlot++;
	}

#if SCE_GNM_CUE2_VALIDATE_DISPATCH_DRAW_INPUT_USAGE_TABLE
	// EUD Validation:
	if ( newEud )
	{
		const Gnm::InputUsageSlot		*inputUsageTableVs     = stageInfo->inputUsageTable;
		const uint32_t					 inputUsageTableSizeVs = stageInfo->inputUsageTableSize;
		uint32_t iUsageSlotVs = 0;
		while ( iUsageSlotVs < inputUsageTableSizeVs && inputUsageTableVs[iUsageSlotVs].m_startRegister < 16 )
			++iUsageSlotVs;

		while ( iUsageSlot < inputUsageTableSize)
		{
			const uint32_t destEudNdx = inputUsageTable[iUsageSlot].m_startRegister - 16;
			const uint32_t srcApiSlot = inputUsageTable[iUsageSlot].m_apiSlot;
			const uint32_t regCount   = (inputUsageTable[iUsageSlot].m_registerCount+1) * 4; // 1 -> 8 DW; 0 -> 4 DW

			while ( iUsageSlotVs < inputUsageTableSizeVs && inputUsageTableVs[iUsageSlotVs].m_startRegister < 16 + destEudNdx )
				++iUsageSlotVs;
			SCE_GNM_ASSERT_MSG( iUsageSlotVs < inputUsageTableSizeVs
				&& inputUsageTable[iUsageSlot].m_startRegister == inputUsageTableVs[iUsageSlotVs].m_startRegister
				&& inputUsageTable[iUsageSlot].m_usageType == inputUsageTableVs[iUsageSlotVs].m_usageType
				&& inputUsageTable[iUsageSlot].m_apiSlot == inputUsageTableVs[iUsageSlotVs].m_apiSlot
				&& inputUsageTable[iUsageSlot].m_registerCount == inputUsageTableVs[iUsageSlotVs].m_registerCount );
			iUsageSlot++;
		}
	}
#endif //#if SCE_GNM_CUE2_VALIDATE_DISPATCH_DRAW_INPUT_USAGE_TABLE
}

#if SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE
void ConstantUpdateEngine::setPsInputUsageTable(uint32_t *psInput)
{
	if (m_currentPSB != NULL)
		m_dcb->setPsShaderUsage(psInput, m_currentPSB->m_numInputSemantics);
}
#endif //!SCE_GNM_CUE2_USER_ALWAYS_SETS_PS_USAGE

void ConstantUpdateEngine::preDraw()
{
	const uint32_t esgs_en = (m_activeShaderStages>>5)&1;
	const uint32_t lshs_en = (m_activeShaderStages&1);
	SCE_GNM_ASSERT_MSG(m_activeShaderStages != kActiveShaderStagesDispatchDrawVsPs, "setActiveShaderStages must be VsPs, EsGsVsPs, (OffChip)LsHsVsPs, or (OffChip)LsHsEsGsVsPs");
	if(m_activeShaderStages == kActiveShaderStagesEsGsVsPs)
	{
		SCE_GNM_ASSERT_MSG(m_currentESB && m_currentGSB, "When ES and GS stages are enabled both ES and GS shaders should be non-NULL");
		SCE_GNM_ASSERT_MSG(!(m_currentESB->isOnChip() ^ m_currentGSB->isOnChip()),"Both ES and GS should be on-chip or off-chip simultaneously");
		SCE_GNM_ASSERT_MSG(!m_currentGSB->isOnChip() || m_currentESB->m_memExportVertexSizeInDWord == m_currentGSB->m_inputVertexSizeInDWord, "For on-chip GS the ES output vertex size has to match the GS input size");
	}


	// These sync is needed only when one of the ring buffers crosses a sync point (either 1/2 or last chunk), if this has not happened it's going to be replaced with a NOP

	m_usingCcb = false;

	m_ccb->waitOnDeCounterDiff(m_numRingEntries/4);
	GnmxConstantCommandBuffer savedCcb = *m_ccb;
	
#if !SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE
	// Update PS usage table, generating it from scratch if the user didn't pass one in.
	if (m_currentPSB != NULL && (m_dirtyShader&ConstantUpdateEngineHelper::kDirtyVsOrPsMask) && m_currentPSB->m_numInputSemantics != 0)
	{
		if ( m_psInputs )
		{
			m_dcb->setPsShaderUsage(m_psInputs, m_currentPSB->m_numInputSemantics);
		}
		else
		{
			uint32_t psInputs[32];
			Gnm::generatePsShaderUsageTable(psInputs,
											m_currentVSB->getExportSemanticTable(), m_currentVSB->m_numExportSemantics,
											m_currentPSB->getPixelInputSemanticTable(), m_currentPSB->m_numInputSemantics);
			m_dcb->setPsShaderUsage(psInputs, m_currentPSB->m_numInputSemantics);
		}
	}
#endif //!SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE

	Gnm::ShaderStage currentStage = Gnm::kShaderStagePs;
	uint32_t currentStageMask = (1<<currentStage);

	do
	{
		if ( m_prefetchShaderCode && (m_dirtyShader & currentStageMask) )
		{
			m_dcb->prefetchIntoL2((void*)((uintptr_t)m_stageInfo[currentStage].shaderBaseAddr256<<8),
								  m_stageInfo[currentStage].shaderCodeSizeInBytes);
		}

		if ( (m_dirtyStage & currentStageMask) && m_stageInfo[currentStage].inputUsageTableSize )
		{
			applyInputUsageData(currentStage);
		}

		m_dirtyShader = m_dirtyShader & ~currentStageMask;
		m_dirtyStage = m_dirtyStage & ~currentStageMask;

		// Next active stage:
		currentStage = (Gnm::ShaderStage)(currentStage + 1);
		currentStageMask <<= 1;

		if ( currentStage == Gnm::kShaderStageGs && !esgs_en )
		{
			currentStage = Gnm::kShaderStageHs; // Skip Gs/Es
			currentStageMask = (1<<currentStage);
		}
		if ( currentStage == Gnm::kShaderStageHs && !lshs_en )
		{
			currentStage = Gnm::kShaderStageCount; // Skip Ls/Hs
			currentStageMask = (1<<currentStage);
		}

	} while ( currentStage < kShaderStageCount );

#if !SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	// Check if we need to update the global table:
	if ( m_globalTableNeedsUpdate )
	{
		SCE_GNM_ASSERT_MSG(m_globalTableAddr, "Global table pointer not specified. Call setGlobalResourceTableAddr() first!");
		m_dcb->writeDataInlineThroughL2(m_globalTableAddr, m_globalTablePtr, SCE_GNM_SHADER_GLOBAL_TABLE_SIZE/4, Gnm::kCachePolicyLru, Gnm::kWriteDataConfirmEnable);
		// Invalidate KCache:
		m_dcb->flushShaderCachesAndWait(kCacheActionNone,kExtendedCacheActionInvalidateKCache, kStallCommandBufferParserDisable);
		m_globalTableNeedsUpdate = false;
	}
#endif //!SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	if ( m_usingCcb )
	{
		m_ccb->incrementCeCounter();
		m_dcb->waitOnCe();
	}

	if(!m_anyWrapped)
	{
		// replace the waitOnDeCounterDiff() with a NOP
		savedCcb.m_cmdptr -= kWaitOnDeCounterDiffSizeInDword;
		savedCcb.insertNop(kWaitOnDeCounterDiffSizeInDword);
	}
	m_anyWrapped = false;
}


void ConstantUpdateEngine::postDraw()
{
	if ( m_usingCcb )
	{
		// Inform the constant engine that this draw has ended so that the constant engine can reuse
		// the constant memory allocated to this draw call
		m_dcb->incrementDeCounter();
		m_usingCcb = false;
		m_submitWithCcb = true;
	}
}


void ConstantUpdateEngine::preDispatch()
{
	m_usingCcb = false;
	m_ccb->waitOnDeCounterDiff(m_numRingEntries/4);
	GnmxConstantCommandBuffer savedCcb = *m_ccb;
	
	if ( m_prefetchShaderCode && (m_dirtyShader & (1<<kShaderStageCs)) )
	{
		m_dcb->prefetchIntoL2((void*)((uintptr_t)m_stageInfo[kShaderStageCs].shaderBaseAddr256<<8),
							  m_stageInfo[kShaderStageCs].shaderCodeSizeInBytes);
	}
	if ( (m_dirtyStage & (1<<kShaderStageCs)) && m_stageInfo[kShaderStageCs].inputUsageTableSize )
	{
		applyInputUsageData(kShaderStageCs);
	}

	m_dirtyShader = m_dirtyShader & ~(1<<sce::Gnm::kShaderStageCs);
	m_dirtyStage = m_dirtyStage & ~(1<<sce::Gnm::kShaderStageCs);

#if !SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	// Check if we need to update the global table:
	if ( m_globalTableNeedsUpdate )
	{
		SCE_GNM_ASSERT_MSG(m_globalTableAddr, "Global table pointer not specified. Call setGlobalResourceTableAddr() first!");
		m_dcb->writeDataInlineThroughL2(m_globalTableAddr, m_globalTablePtr, SCE_GNM_SHADER_GLOBAL_TABLE_SIZE/4, Gnm::kCachePolicyLru, Gnm::kWriteDataConfirmEnable);
		// Invalidate KCache:
		m_dcb->flushShaderCachesAndWait(kCacheActionNone,kExtendedCacheActionInvalidateKCache, kStallCommandBufferParserDisable);
		m_globalTableNeedsUpdate = false;
	}
#endif //!SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	if ( m_usingCcb )
	{
		m_ccb->incrementCeCounter();
		m_dcb->waitOnCe();
	}

	if(!m_anyWrapped)
	{
		// replace the waitOnDeCounterDiff() with a NOP
		savedCcb.m_cmdptr -= kWaitOnDeCounterDiffSizeInDword;
		savedCcb.insertNop(kWaitOnDeCounterDiffSizeInDword);
	}

	m_anyWrapped = false;
}


void ConstantUpdateEngine::postDispatch()
{
	if ( m_usingCcb )
	{
		// Inform the constant engine that this dispatch has ended so that the constant engine can reuse
		// the constant memory allocated to this dispatch
		m_dcb->incrementDeCounter();
		m_usingCcb = false;
		m_submitWithCcb = true;
	}
}


void ConstantUpdateEngine::advanceFrame(void)
{
	m_anyWrapped = true;
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	for (uint32_t iStage = 0; iStage < kShaderStageCount; ++iStage)
	{
		for (uint32_t iRing = 0; iRing < kNumRingBuffersPerStage; ++iRing)
		{
			ConstantUpdateEngineHelper::updateRingBuffersStatePostSubmission(&m_stageInfo[iStage].ringBuffers[iRing]);
		}
	}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	invalidateAllBindings();
	m_submitWithCcb = false;
}

void ConstantUpdateEngine::invalidateAllBindings(void)
{
	invalidateAllShaderContexts();
	for (uint32_t iStage = 0; iStage < kShaderStageCount; ++iStage)
	{
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
		for (uint32_t iResource = 0; iResource < kResourceNumChunks; ++iResource)
		{
			m_stageInfo[iStage].resourceStage[iResource].usedChunk = 0;
			m_stageInfo[iStage].resourceStage[iResource].curChunk  = 0;
			m_stageInfo[iStage].resourceStage[iResource].curSlots  = 0;
			m_stageInfo[iStage].resourceStage[iResource].usedSlots = 0;
		}

		for (uint32_t iSampler = 0; iSampler < kSamplerNumChunks; ++iSampler)
		{
			m_stageInfo[iStage].samplerStage[iSampler].usedChunk = 0;
			m_stageInfo[iStage].samplerStage[iSampler].curChunk	 = 0;
			m_stageInfo[iStage].samplerStage[iSampler].curSlots	 = 0;
			m_stageInfo[iStage].samplerStage[iSampler].usedSlots = 0;
		}

		for (uint32_t iConstantBuffer = 0; iConstantBuffer < kConstantBufferNumChunks; ++iConstantBuffer)
		{
			m_stageInfo[iStage].constantBufferStage[iConstantBuffer].usedChunk = 0;
			m_stageInfo[iStage].constantBufferStage[iConstantBuffer].curChunk  = 0;
			m_stageInfo[iStage].constantBufferStage[iConstantBuffer].curSlots  = 0;
			m_stageInfo[iStage].constantBufferStage[iConstantBuffer].usedSlots = 0;
		}

		for (uint32_t iVertexBuffer = 0; iVertexBuffer < kVertexBufferNumChunks; ++iVertexBuffer)
		{
			m_stageInfo[iStage].vertexBufferStage[iVertexBuffer].usedChunk = 0;
			m_stageInfo[iStage].vertexBufferStage[iVertexBuffer].curChunk  = 0;
			m_stageInfo[iStage].vertexBufferStage[iVertexBuffer].curSlots  = 0;
			m_stageInfo[iStage].vertexBufferStage[iVertexBuffer].usedSlots = 0;
		}

		for (uint32_t iRwResource = 0; iRwResource < kRwResourceNumChunks; ++iRwResource)
		{
			m_stageInfo[iStage].rwResourceStage[iRwResource].usedChunk = 0;
			m_stageInfo[iStage].rwResourceStage[iRwResource].curChunk  = 0;
			m_stageInfo[iStage].rwResourceStage[iRwResource].curSlots  = 0;
			m_stageInfo[iStage].rwResourceStage[iRwResource].usedSlots = 0;
		}

		m_stageInfo[iStage].eudResourceSet[0]	 = 0;
		m_stageInfo[iStage].eudResourceSet[1]	 = 0;
		m_stageInfo[iStage].eudSamplerSet		 = 0;
		m_stageInfo[iStage].eudConstantBufferSet = 0;
		m_stageInfo[iStage].eudRwResourceSet	 = 0;

		m_stageInfo[iStage].activeResource[0]	 = 0;
		m_stageInfo[iStage].activeResource[1]	 = 0;
		m_stageInfo[iStage].activeSampler		 = 0;
		m_stageInfo[iStage].activeConstantBuffer = 0;
		m_stageInfo[iStage].activeVertexBuffer	 = 0;
		m_stageInfo[iStage].activeRwResource	 = 0;
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

		m_stageInfo[iStage].internalSrtBuffer		  = 0;
		m_stageInfo[iStage].userSrtBufferSizeInDwords = 0;

		m_stageInfo[iStage].inputUsageTableSize = 0;
		m_stageInfo[iStage].eudSizeInDWord		= 0;
		m_stageInfo[iStage].dirtyRing			= 0;
		m_stageInfo[iStage].appendConsumeDword	= 0;

		m_stageInfo[iStage].shaderBaseAddr256   = 0;
	}

	m_streamoutBufferStage.usedChunk = 0;
	m_streamoutBufferStage.curChunk  = 0;
	m_streamoutBufferStage.curSlots  = 0;
	m_streamoutBufferStage.usedSlots = 0;
	m_eudReferencesStreamoutBuffers	 = false;
#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
	m_globalTableStage.usedChunk	= 0;
	m_globalTableStage.curChunk		= 0;
	m_globalTableStage.curSlots		= 0;
	m_globalTableStage.usedSlots	= 0;
	m_eudReferencesGlobalTable		= 0;
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE

	m_onChipEsVertsPerSubGroup        = 0;
	m_onChipEsExportVertexSizeInDword = 0;

	m_activeShaderStages = Gnm::kActiveShaderStagesVsPs;
	m_shaderUsesSrt		 = 0;
	m_anyWrapped		 = true;
	m_psInputs			 = NULL;
	m_dirtyStage         = 0;
	m_dirtyShader		 = 0;
	m_dispatchDrawIndexDeallocNumBits = 0;
	m_asyncCsShaderOrderedAppendMode = m_gfxCsShaderOrderedAppendMode = 0;

	m_currentVSB	= 0;
	m_currentPSB	= 0;
	m_currentCSB	= 0;
	m_currentLSB	= 0;
	m_currentHSB	= 0;
	m_currentESB	= 0;
	m_currentGSB	= 0;
	m_currentAcbCSB	= 0;
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	clearUserTables();
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#if SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	memset(m_currentFetchShader,0,sizeof(m_currentFetchShader));
#endif //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
}
void ConstantUpdateEngine::setGsModeOff()
{
	m_shaderContextIsValid &= ~(1 << Gnm::kShaderStageGs);
	m_onChipGsSignature = kOnChipGsInvalidSignature;
	m_dcb->disableGsMode();
}

#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
void ConstantUpdateEngine::clearUserTables()
{
	for( int stage = 0; stage != sizeof(m_stageInfo)/sizeof(m_stageInfo[0]); ++stage)
	{
		memset(m_stageInfo[stage].userTable,0,sizeof(m_stageInfo[stage].userTable));
	}
}
#endif // SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
