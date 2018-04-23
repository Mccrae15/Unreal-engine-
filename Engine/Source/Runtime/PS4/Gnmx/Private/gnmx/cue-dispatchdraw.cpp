/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifdef __ORBIS__
#include <x86intrin.h>
#endif // __ORBIS__
#include <gnm.h>

#include <gnmx/common.h>

#include <gnmx/cue.h>
#include <gnmx/cue-helper.h>

using namespace sce::Gnm;
using namespace sce::Gnmx;

namespace
{
	const uint32_t kWaitOnDeCounterDiffSizeInDword = 2;
}

void ConstantUpdateEngine::preDispatchDraw(sce::Gnm::DispatchOrderedAppendMode *pOutOrderedAppendMode, uint32_t *pOutDispatchDrawIndexDeallocationMask, uint32_t *pOutSgprKrbLoc, uint32_t *pOutSgprInstanceCs,
										   sce::Gnm::DispatchDrawMode *pOutDispatchDrawMode, uint32_t *pOutSgprVrbLoc, uint32_t *pOutSgprInstanceVs)
{
	SCE_GNM_ASSERT_MSG(m_activeShaderStages == kActiveShaderStagesDispatchDrawVsPs, "setActiveShaderStages must be DispatchDrawVsPs");

	// These sync is needed only when one of the ring buffers crosses a sync point (either 1/2 or last chunk), if this has not happened it's going to be replaced with a NOP

	m_usingCcb = false;

	m_ccb->waitOnDeCounterDiff(m_numRingEntries/4);
	GnmxConstantCommandBuffer savedCcb = *m_ccb;
	savedCcb.m_cmdptr -= kWaitOnDeCounterDiffSizeInDword;
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

	*pOutSgprVrbLoc = (uint32_t)-1;
	*pOutDispatchDrawMode = Gnm::kDispatchDrawModeIndexRingBufferOnly;
	if (pOutSgprInstanceVs != NULL)
		*pOutSgprInstanceVs = (uint32_t)-1;
	do
	{
		uint32_t inputUsageTableSize = m_stageInfo[currentStage].inputUsageTableSize;
		if ( inputUsageTableSize )
		{
			if ( m_prefetchShaderCode && (m_dirtyShader & currentStageMask) )
			{
				// Should we consider only pre-fetching up to some maximum number of bytes from the start of the shader, in case there are very large shaders?
				m_dcb->prefetchIntoL2((void*)((uintptr_t)m_stageInfo[currentStage].shaderBaseAddr256<<8),
									  m_stageInfo[currentStage].shaderCodeSizeInBytes);
			}

			if ( (m_dirtyStage & currentStageMask) )
			{
				applyInputUsageData(currentStage);
			}

			if (currentStage == Gnm::kShaderStageVs) 
			{
				Gnm::InputUsageSlot const* inputUsageTable = m_stageInfo[currentStage].inputUsageTable;
				bool bFoundImmVertexRingBufferOffset = false, bFoundImmDispatchDrawInstances = false;
				for (uint32_t iUsage = 0; iUsage < inputUsageTableSize && !(bFoundImmVertexRingBufferOffset && bFoundImmDispatchDrawInstances); ++iUsage) {
					if (inputUsageTable[iUsage].m_startRegister >= 16)
						break;
					if (!bFoundImmVertexRingBufferOffset && inputUsageTable[iUsage].m_usageType == Gnm::kShaderInputUsageImmVertexRingBufferOffset) {
						*pOutSgprVrbLoc = inputUsageTable[iUsage].m_startRegister;
						bFoundImmVertexRingBufferOffset = true;
					}
					if (!bFoundImmDispatchDrawInstances && inputUsageTable[iUsage].m_usageType == Gnm::kShaderInputUsageImmDispatchDrawInstances) {
						if (pOutSgprInstanceVs != NULL)
							*pOutSgprInstanceVs = inputUsageTable[iUsage].m_startRegister;
						else
							m_dcb->setUserData(Gnm::kShaderStageVs, inputUsageTable[iUsage].m_startRegister, 0);
						bFoundImmDispatchDrawInstances = true;
					}
				}
				if (bFoundImmVertexRingBufferOffset)
					*pOutDispatchDrawMode = Gnm::kDispatchDrawModeIndexAndVertexRingBuffer;
			}
			m_dirtyShader = m_dirtyShader & ~currentStageMask;
			m_dirtyStage = m_dirtyStage & ~currentStageMask;
		}

		// Next active stage in { Ps, Vs }:
		currentStage = (Gnm::ShaderStage)(currentStage + 1);
		currentStageMask <<= 1;
	} while ( currentStage < Gnm::kShaderStageGs );

	*pOutSgprKrbLoc = (uint32_t)-1;
	if (pOutSgprInstanceCs != NULL)
		*pOutSgprInstanceCs = (uint32_t)-1;
	SCE_GNM_ASSERT_MSG(m_asyncCsShaderOrderedAppendMode == Gnm::kDispatchOrderedAppendModeIndexPerWavefront || m_asyncCsShaderOrderedAppendMode == Gnm::kDispatchOrderedAppendModeIndexPerThreadgroup, "dispatchDraw expects a CS shader with m_orderedAppendMode != kDispatchOrderedAppendModeDisabled");
	SCE_GNM_ASSERT_MSG(m_dispatchDrawIndexDeallocNumBits >= 1 && m_dispatchDrawIndexDeallocNumBits <= 15, "dispatchDraw expects a CS shader with m_dispatchDrawIndexDeallocNumBits in the range [1:15]");
	*pOutOrderedAppendMode = (Gnm::DispatchOrderedAppendMode)m_asyncCsShaderOrderedAppendMode;
	*pOutDispatchDrawIndexDeallocationMask = 0x0000FFFF & (0xFFFF << m_dispatchDrawIndexDeallocNumBits);
	{
		currentStage = (Gnm::ShaderStage)kShaderStageAsynchronousCompute;
		uint32_t inputUsageTableSize = m_stageInfo[currentStage].inputUsageTableSize;
		if ( inputUsageTableSize )
		{
			if (m_prefetchShaderCode && (m_dirtyShader & (1 << kShaderStageAsynchronousCompute)))
			{
				// Should we consider only pre-fetching up to some maximum number of bytes from the start of the shader, in case there are very large shaders?
				// the compute shader code must be prefetched by the ACB:
			
				m_acb->prefetchIntoL2((void*)((uintptr_t)m_stageInfo[currentStage].shaderBaseAddr256<<8),
					m_stageInfo[currentStage].shaderCodeSizeInBytes);
			}
			// also explicitly prefetch the dispatch draw data into L2 for the compute shaders:
			if (m_pDispatchDrawData && m_sizeofDispatchDrawData)
			{
				m_acb->prefetchIntoL2((void*)m_pDispatchDrawData, m_sizeofDispatchDrawData);
			}

			applyInputUsageDataForDispatchDrawCompute();

			m_dirtyShader = m_dirtyShader & ~(1 << kShaderStageAsynchronousCompute);
			
			Gnm::InputUsageSlot const* inputUsageTable = m_stageInfo[currentStage].inputUsageTable;
			bool bFoundImmGdsKickRingBufferOffset = false, bFoundImmDispatchDrawInstances = false;

			for (uint32_t iUsage = 0; iUsage < inputUsageTableSize && !(bFoundImmGdsKickRingBufferOffset && bFoundImmDispatchDrawInstances); ++iUsage) 
			{
				if (inputUsageTable[iUsage].m_startRegister >= 16)
					break;
				if (!bFoundImmGdsKickRingBufferOffset && inputUsageTable[iUsage].m_usageType == Gnm::kShaderInputUsageImmGdsKickRingBufferOffset) 
				{
					*pOutSgprKrbLoc = inputUsageTable[iUsage].m_startRegister;
					bFoundImmGdsKickRingBufferOffset = true;
				}
				if (!bFoundImmDispatchDrawInstances && inputUsageTable[iUsage].m_usageType == Gnm::kShaderInputUsageImmDispatchDrawInstances) 
				{
					if (pOutSgprInstanceCs != NULL)
						*pOutSgprInstanceCs = inputUsageTable[iUsage].m_startRegister;
					else
						m_acb->setUserData(inputUsageTable[iUsage].m_startRegister, 0);
					bFoundImmDispatchDrawInstances = true;
				}
			}
		}
	}
	SCE_GNM_ASSERT_MSG(*pOutSgprKrbLoc != (uint32_t)-1, "dispatchDraw expects a CS shader with an InputUsageSlot of m_usageType kShaderInputUsageImmGdsKickRingBufferOffset in a real user sgpr");

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
		m_ccb->incrementCeCounterForDispatchDraw();
		m_dcb->waitOnCe();
		m_acb->waitOnCe();
	}

	if(!m_anyWrapped)
	{
		savedCcb.insertNop(kWaitOnDeCounterDiffSizeInDword);
	}
	m_anyWrapped = false;
}


void ConstantUpdateEngine::postDispatchDraw()
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


