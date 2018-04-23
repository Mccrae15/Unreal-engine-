/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if defined(__ORBIS__) // LCUE isn't supported offline

#include <gnm/commandbuffer.h>
#include <gnm/measureconstantcommandbuffer.h>
#include <gnm/measuredispatchcommandbuffer.h>
#include <gnmx/lwgfxcontext.h>
#include <algorithm>

using namespace sce;
using namespace Gnmx;

static bool dispatchDrawCcbOutOfSpace(sce::Gnm::CommandBuffer *cb, uint32_t dwordCount, void *userData)
{
	SCE_GNM_UNUSED(cb);
	SCE_GNM_UNUSED(dwordCount);
	SCE_GNM_UNUSED(userData);

	SCE_GNM_ERROR("CCB required for DispatchDraw has run out of space, increase the default size of kNumDispatchDrawRingBuffersRolls (%d) or "
				  "specify a larger value for numDispatchDrawRingBuffersRolls at init time.", LightweightConstantUpdateEngine::kNumDispatchDrawRingBuffersRolls);

	// CCB is always initialized to its full size and is fixed, so we don't split here. Instead the user is required to increase the number of supported 
	// numDispatchDrawRingBuffersRolls at init time.
	return false;
}
static const uint32_t* cmdPtrAfterIncrement(const sce::Gnm::CommandBuffer *cb, uint32_t dwordCount)
{
	if (cb->sizeOfEndBufferAllocationInDword())
		return cb->m_beginptr + cb->m_bufferSizeInDwords + dwordCount;
	else
		return cb->m_cmdptr + dwordCount;
}
static bool handleReserveFailed(sce::Gnm::CommandBuffer *cb, uint32_t dwordCount, void *userData)
{
	LightweightGfxContext &gfxc = *(LightweightGfxContext*)userData;
	// If either (DCB/ACB) command buffer has actually reached the end of its full buffer, then invoke m_bufferFullCallback (if one has been registered)
	if (static_cast<void*>(cb) == static_cast<void*>(&(gfxc.m_dcb)) && cmdPtrAfterIncrement(cb, dwordCount) > gfxc.m_actualDcbEnd)
	{
		if (gfxc.m_cbFullCallback.m_func != 0)
			return gfxc.m_cbFullCallback.m_func(&gfxc, cb, dwordCount, gfxc.m_cbFullCallback.m_userData);
		else
		{
			SCE_GNM_ERROR("Out of DCB command buffer space, and no callback bound.");
			return false;
		}
	}
	if (static_cast<void*>(cb) == static_cast<void*>(&(gfxc.m_acb)) && cmdPtrAfterIncrement(cb, dwordCount) > gfxc.m_actualAcbEnd)
	{
		if (gfxc.m_cbFullCallback.m_func != 0)
			return gfxc.m_cbFullCallback.m_func(&gfxc, cb, dwordCount, gfxc.m_cbFullCallback.m_userData);
		else
		{
			SCE_GNM_ERROR("Out of ACB command buffer space, and no callback bound.");
			return false;
		}
	}
	// CCB reserve failed is handled by dispatchDrawCcbOutOfSpace() above
	return gfxc.splitCommandBuffers();
}


void LightweightGfxContext::init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes,
								 void* globalInternalResourceTableAddr, uint32_t numDispatchDrawRingBuffersRolls)
{
	SCE_GNM_ASSERT(dcbBuffer != NULL);
	SCE_GNM_ASSERT(dcbBufferSizeInBytes > 0);
	SCE_GNM_ASSERT_MSG(resourceBufferSizeInBytes == 0 || resourceBufferInGarlic != NULL, "resourceBufferInGarlic must not be NULL if resourceBufferSizeInBytes is larger than 0 bytes.");
	SCE_GNM_ASSERT_MSG(resourceBufferSizeInBytes >  0 || resourceBufferInGarlic == NULL, "resourceBufferSizeInBytes must be greater than 0 if resourceBufferInGarlic is not NULL.");

	// only calling base init so we can initialize all the base class members for multiple submits and dispatch draw, the real setup for DCB/CCB/ACB is handled below 
	BaseGfxContext::init(NULL, 0, NULL, 0);

	// init command buffers and override BaseGfxContext DCB/CCB/ACB settings
	sce::Gnm::MeasureConstantCommandBuffer m_mccb;
	uint32_t ccbReserveSizeInDwords = m_mccb.incrementCeCounterForDispatchDraw() * numDispatchDrawRingBuffersRolls;

	// we only need a small CCB for dispatch draw (see setupDispatchDrawRingBuffers), so pull memory in from the actual end of the entire dcb allocation
	// the ccb size will never exceed Gnm::kIndirectBufferMaximumSizeInBytes
	uint32_t* ccbStartAddr = (uint32_t*)dcbBuffer + ((dcbBufferSizeInBytes / 4) - ccbReserveSizeInDwords);
	uint32_t  ccbSizeInBytes = ccbReserveSizeInDwords * sizeof(uint32_t);
	m_ccb.init(ccbStartAddr, std::min((uint32_t)Gnm::kIndirectBufferMaximumSizeInBytes, ccbSizeInBytes), dispatchDrawCcbOutOfSpace, this);
	m_currentCcbSubmissionStart = m_ccb.m_beginptr;
	m_actualCcbEnd = (uint32_t*)ccbStartAddr + (ccbSizeInBytes / 4);

	uint32_t* dcbStartAddr = (uint32_t*)dcbBuffer;
	uint32_t  dcbSizeInBytes = ((dcbBufferSizeInBytes / 4) - ccbReserveSizeInDwords) * sizeof(uint32_t);
	m_dcb.init(dcbStartAddr, std::min((uint32_t)Gnm::kIndirectBufferMaximumSizeInBytes, dcbSizeInBytes), handleReserveFailed, this);
	m_currentDcbSubmissionStart = m_dcb.m_beginptr;
	m_actualDcbEnd = (uint32_t*)dcbStartAddr + (dcbSizeInBytes / 4);

	// m_acb is initalized through initDispatchDrawCommandBuffer()
	m_acb.m_beginptr = m_acb.m_cmdptr = m_acb.m_endptr = NULL;

	const int32_t kResourceBufferCount = resourceBufferInGarlic != NULL ? 1 : 0; // only one resource buffer per context, if resource buffer is provided
	uint32_t* resourceBuffer = (uint32_t*)resourceBufferInGarlic;
	m_lwcue.init(&resourceBuffer, kResourceBufferCount, resourceBufferSizeInBytes / 4, globalInternalResourceTableAddr);
	m_lwcue.setDrawCommandBuffer(&m_dcb);

#if SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
	// first order of business, invalidate the KCache/L1/L2 to rid us of any stale data
	m_dcb.flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionInvalidateKCache, Gnm::kStallCommandBufferParserDisable);
#endif // SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
}


void LightweightGfxContext::initDispatchDrawCommandBuffer(void *acbBuffer, uint32_t acbSizeInBytes)
{
	m_acb.init(acbBuffer, std::min((uint32_t)Gnm::kIndirectBufferMaximumSizeInBytes, acbSizeInBytes), handleReserveFailed, this);
	m_currentAcbSubmissionStart = m_acb.m_beginptr;
	m_actualAcbEnd = (uint32_t*)acbBuffer+(acbSizeInBytes/4);
	if (m_acb.m_beginptr != NULL && (m_acb.m_endptr - m_acb.m_beginptr) > sce::Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker())
		m_acb.pushDispatchDrawAcbSubmitMarker();

	m_lwcue.setDispatchDrawCommandBuffer(&m_acb);
}


void LightweightGfxContext::reset(void)
{
	m_dcb.resetBuffer();
	m_ccb.resetBuffer();
	m_acb.resetBuffer();

	// Restore end pointers to artificial limits
	m_dcb.m_endptr = std::min(m_dcb.m_cmdptr+Gnm::kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualDcbEnd);
	m_acb.m_endptr = std::min(m_acb.m_cmdptr+Gnm::kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualAcbEnd);
	
	// Restore buffer size in case it was modified by splitCommandBuffers
	m_dcb.m_bufferSizeInDwords = static_cast<uint32_t>(m_dcb.m_endptr - m_dcb.m_beginptr);
	m_acb.m_bufferSizeInDwords = static_cast<uint32_t>(m_acb.m_endptr - m_acb.m_beginptr);

	// Under the LCUE the CCB usage is limited for dispatchDraw, 
	// so size of buffer is fixed
	SCE_GNM_ASSERT(m_ccb.m_endptr == m_actualCcbEnd);

	// Restore submit ranges to default values
	for(uint32_t iSub=0; iSub<kMaxNumStoredSubmissions; ++iSub)
	{
		m_submissionRanges[iSub].m_dcbStartDwordOffset = 0;
		m_submissionRanges[iSub].m_dcbSizeInDwords = 0;
		m_submissionRanges[iSub].m_acbStartDwordOffset = 0;
		m_submissionRanges[iSub].m_acbSizeInDwords = 0;
		m_submissionRanges[iSub].m_ccbStartDwordOffset = 0;
		m_submissionRanges[iSub].m_ccbSizeInDwords = 0;
	}
	m_submissionCount = 0;
	m_currentDcbSubmissionStart = m_dcb.m_beginptr;
	m_currentAcbSubmissionStart = m_acb.m_beginptr;
	m_currentCcbSubmissionStart = m_ccb.m_beginptr;
	if (m_acb.m_beginptr != NULL && (m_acb.m_endptr - m_acb.m_beginptr) > sce::Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker())
		m_acb.pushDispatchDrawAcbSubmitMarker();
	m_pDispatchDrawSharedData = NULL;
	m_dispatchDrawFlags = 0;
	m_dispatchDrawNumInstancesMinus1 = 0;	//setNumInstances(1)
	m_dispatchDrawIndexDeallocMask = 0;
	m_lwcue.swapBuffers();
}


bool LightweightGfxContext::splitCommandBuffers(bool bAdvanceEndOfBuffer)
{
	// Register a new submit up to the current DCB/CCB command pointers
	if (m_submissionCount >= BaseGfxContext::kMaxNumStoredSubmissions)
	{
		SCE_GNM_ASSERT_MSG(m_submissionCount < BaseGfxContext::kMaxNumStoredSubmissions, "Out of space for stored submissions. More can be added by increasing kMaxNumStoredSubmissions.");
		return false;
	}
	if (m_dcb.m_cmdptr == m_currentDcbSubmissionStart) // we cannot create an empty DCB submission because submitCommandBuffers() will fail
	{
		if (!m_dcb.getRemainingBufferSpaceInDwords())
			return false; // there is no room in the dcb to insert a NOP, something went very wrong.
		m_dcb.insertNop(1); // insert a NOP so the DCB range is not empty.
	}
	// Store the current submit range
	m_submissionRanges[m_submissionCount].m_dcbStartDwordOffset = static_cast<uint32_t>(m_currentDcbSubmissionStart - m_dcb.m_beginptr);
	m_submissionRanges[m_submissionCount].m_dcbSizeInDwords     = static_cast<uint32_t>(m_dcb.m_cmdptr - m_currentDcbSubmissionStart);
	m_submissionRanges[m_submissionCount].m_ccbStartDwordOffset = static_cast<uint32_t>(m_currentCcbSubmissionStart - m_ccb.m_beginptr);
	m_submissionRanges[m_submissionCount].m_ccbSizeInDwords     = static_cast<uint32_t>(m_ccb.m_cmdptr - m_currentCcbSubmissionStart);
	m_submissionRanges[m_submissionCount].m_acbStartDwordOffset = static_cast<uint32_t>(m_currentAcbSubmissionStart - m_acb.m_beginptr);
	m_submissionRanges[m_submissionCount].m_acbSizeInDwords     = static_cast<uint32_t>(m_acb.m_cmdptr - m_currentAcbSubmissionStart);
	m_submissionCount++;

	if (bAdvanceEndOfBuffer)
	{
		// If end of buffer allocations were made, it is not safe to continue using the existing command pointer address
		// as it will begin to over write embedded data with new commands. So we need to jump passed the embedded data block.
		advanceCmdPtrPastEndBufferAllocation(&m_dcb);
		advanceCmdPtrPastEndBufferAllocation(&m_acb);

		// Advance CB end pointers to the next (possibly artificial) boundary -- either current+(4MB-4), or the end of the actual buffer
		m_dcb.m_endptr = std::min(m_dcb.m_cmdptr + Gnm::kIndirectBufferMaximumSizeInBytes / 4, (uint32_t*)m_actualDcbEnd);
		m_acb.m_endptr = std::min(m_acb.m_cmdptr + Gnm::kIndirectBufferMaximumSizeInBytes / 4, (uint32_t*)m_actualAcbEnd);

		m_dcb.m_bufferSizeInDwords = static_cast<uint32_t>(m_dcb.m_endptr - m_dcb.m_beginptr);
		m_acb.m_bufferSizeInDwords = static_cast<uint32_t>(m_acb.m_endptr - m_acb.m_beginptr);
		SCE_GNM_ASSERT(m_ccb.m_endptr == m_actualCcbEnd);
	}

	m_currentDcbSubmissionStart = m_dcb.m_cmdptr;
	m_currentCcbSubmissionStart = m_ccb.m_cmdptr;
	m_currentAcbSubmissionStart = m_acb.m_cmdptr;

	if (m_acb.m_beginptr != NULL && (m_acb.m_endptr - m_acb.m_beginptr) > sce::Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker())
		m_acb.pushDispatchDrawAcbSubmitMarker();

	return true;
}


void LightweightGfxContext::dispatchDraw(DispatchDrawTriangleCullIndexData const *pDispatchDrawIndexData, uint32_t primGroupThreshold, uint32_t pollIntervalThreshold, Gnm::DrawModifier modifier)
{
	//To prevent dispatch draw deadlocks, this must match the value set to setVgtControl primGroupSize
	static const uint32_t kMinPrimgroupSize = 129;
	const uint32_t numPrimsPerVgt = kMinPrimgroupSize + ((m_dispatchDrawFlags & kDispatchDrawFlagsMaskPrimGroupSizeMinus129) >> kDispatchDrawFlagsShiftPrimGroupSizeMinus129);

	// The primGroupIndexCount must be a multiple of 2*numPrimsPerVgt primitives worth of indices
	// (numPrimsPerVgt*3*2 for triangle primitives) to prevent deadlocks related to VGT switching 
	// and dispatch draw IRB deallocation.
	uint32_t primGroupIndexCount = numPrimsPerVgt*3*2;
	// To prevent deadlocks caused by insufficient IRB space to ensure that at least one IRB
	// deallocation will fit and be processed by the VGT, we must ensure that the IRB has at least 
	// enough space for 3 maximal sized compute output groups and has space for at least 2 maximum
	// sized sub-draw flushes plus room to allocate a maximal sized compute output group.
	static const uint32_t kMaxComputeWorkGroupOutput = 513*3;
	uint32_t maxSubDrawIndexCount = primGroupIndexCount*primGroupThreshold;
	SCE_GNM_ASSERT_MSG(2*(maxSubDrawIndexCount > kMaxComputeWorkGroupOutput ? maxSubDrawIndexCount : kMaxComputeWorkGroupOutput) + kMaxComputeWorkGroupOutput <= m_dispatchDrawSharedData.m_bufferIrb.getNumElements(), "maxSubDrawIndexCount (%u) = primGroupThreshold (%u) * numPrimsPerVgt (%u) *3*2 exceeds the deadlock condition for IRB size in indices %u",
		maxSubDrawIndexCount, primGroupThreshold, numPrimsPerVgt, m_dispatchDrawSharedData.m_bufferIrb.getNumElements());

	uint32_t indexOffset = 0;		//Do we need to be able to set the index offset for dispatch draw?

	SCE_GNM_ASSERT_MSG((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) != 0, "dispatchDraw can only be called between beginDispatchDraw and endDispatchDraw");
	SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_magic == Gnmx::kDispatchDrawTriangleCullIndexDataMagic, "dispatchDraw: index data magic is wrong");
	SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_versionMajor == Gnmx::kDispatchDrawTriangleCullIndexDataVersionMajor, "dispatchDraw: index data version major index does not match");
	SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_numIndexDataBlocks > 0 && pDispatchDrawIndexData->m_numIndexDataBlocks <= 0xFFFF, "dispatchDraw: m_numIndexDataBlocks must be in the range [1:65536]");
	SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_numIndexBits >= 1 && pDispatchDrawIndexData->m_numIndexBits <= 16, "dispatchDraw: m_numIndexBits must be in the range [1:16]");
	SCE_GNM_ASSERT_MSG(pDispatchDrawIndexData->m_numIndexSpaceBits < pDispatchDrawIndexData->m_numIndexBits, "dispatchDraw: m_numIndexSpaceBits must be less than m_numIndexBits");

	// Store bufferInputData and numBlocksTotal to kShaderInputUsagePtrDispatchDraw data
	sce::Gnmx::CsShader const* pDdCsShader = ((sce::Gnmx::CsShader const*)m_lwcue.getBoundShader((sce::Gnm::ShaderStage)sce::Gnmx::LightweightConstantUpdateEngine::kShaderStageAsynchronousCompute));
	if (pDdCsShader->m_version >= 1)
	{
		if (m_pDispatchDrawSharedData == NULL) {
			m_pDispatchDrawSharedData = (DispatchDrawSharedData*)m_acb.allocateFromCommandBuffer(sizeof(DispatchDrawSharedData), Gnm::kEmbeddedDataAlignment4);
			memcpy(m_pDispatchDrawSharedData, &m_dispatchDrawSharedData, sizeof(DispatchDrawSharedData));
		}
		bool const bIsInstancing = true;
		const size_t sizeofDispatchDrawData = bIsInstancing ? Gnmx::kSizeofDispatchDrawTriangleCullV1DataInstancingEnabled : Gnmx::kSizeofDispatchDrawTriangleCullV1DataInstancingDisabled;
		DispatchDrawData *pDispatchDrawData = (DispatchDrawData*)m_acb.allocateFromCommandBuffer(sizeofDispatchDrawData, Gnm::kEmbeddedDataAlignment4);
		pDispatchDrawData->m_pShared = m_pDispatchDrawSharedData;
		pDispatchDrawData->m_bufferInputIndexData = pDispatchDrawIndexData->m_bufferInputIndexData;
		pDispatchDrawData->m_numIndexDataBlocks = (uint16_t)pDispatchDrawIndexData->m_numIndexDataBlocks;
		pDispatchDrawData->m_numIndexBits = pDispatchDrawIndexData->m_numIndexBits;
		pDispatchDrawData->m_numInstancesPerTgMinus1 = pDispatchDrawIndexData->m_numInstancesPerTgMinus1;
		if (bIsInstancing) {
			//NOTE: only required by instancing shader
			pDispatchDrawData->m_instanceStepRate0Minus1 = m_dispatchDrawInstanceStepRate0Minus1;
			pDispatchDrawData->m_instanceStepRate1Minus1 = m_dispatchDrawInstanceStepRate1Minus1;
		}

		// Pass a pointer to the dispatch draw data for the CUE to pass to shaders as kShaderInputUsagePtrDispatchDraw.
		// Its contents must not be modified until after this dispatchDraw call's shaders have finished running.
		m_lwcue.setDispatchDrawData(pDispatchDrawData, sizeofDispatchDrawData);
	}
	else
	{
		uint32_t firstBlock = 0;		//Do we need to be able to render a partial set of blocks?
		// We do not use the CCB to prefetch the dispatch draw control data.
		// Instead we allocate copies from the ACB which the CUE prefetches explicitly.
		// As the current version of DispatchDrawTriangleCullData contains data which changes with every draw call,
		// we must currently allocate and copy 84 bytes for every dispatchDraw call.
		// In the future, we could put data which changes infrequently DispatchDrawTriangleCullCommonData in a 
		// separate structure with a pointer in the base data, which would generally reduce this copy and command 
		// buffer allocation to 52 bytes per dispatchDraw call.
		DispatchDrawV0Data* pDispatchDrawData = (DispatchDrawV0Data*)m_acb.allocateFromCommandBuffer(sizeof(DispatchDrawV0Data), Gnm::kEmbeddedDataAlignment4);
		pDispatchDrawData->m_bufferIrb = m_dispatchDrawSharedData.m_bufferIrb;
		pDispatchDrawData->m_bufferInputIndexData = pDispatchDrawIndexData->m_bufferInputIndexData;
		pDispatchDrawData->m_numIndexDataBlocks = (uint16_t)pDispatchDrawIndexData->m_numIndexDataBlocks;
		pDispatchDrawData->m_gdsOffsetOfIrbWptr = m_dispatchDrawSharedData.m_gdsOffsetOfIrbWptr;
		pDispatchDrawData->m_sizeofIrbInIndices = m_dispatchDrawSharedData.m_bufferIrb.m_regs[2];
		pDispatchDrawData->m_clipCullSettings = m_dispatchDrawSharedData.m_cullSettings;
		pDispatchDrawData->m_numIndexBits = pDispatchDrawIndexData->m_numIndexBits;
		pDispatchDrawData->m_numInstancesPerTgMinus1 = pDispatchDrawIndexData->m_numInstancesPerTgMinus1;
		pDispatchDrawData->m_firstIndexDataBlock = (uint16_t)firstBlock;
		pDispatchDrawData->m_reserved = 0;
		pDispatchDrawData->m_quantErrorScreenX = m_dispatchDrawSharedData.m_quantErrorScreenX;
		pDispatchDrawData->m_quantErrorScreenY = m_dispatchDrawSharedData.m_quantErrorScreenY;
		pDispatchDrawData->m_gbHorizClipAdjust = m_dispatchDrawSharedData.m_gbHorizClipAdjust;
		pDispatchDrawData->m_gbVertClipAdjust = m_dispatchDrawSharedData.m_gbVertClipAdjust;
		//NOTE: only required by instancing shader
		pDispatchDrawData->m_instanceStepRate0Minus1 = m_dispatchDrawInstanceStepRate0Minus1;
		pDispatchDrawData->m_instanceStepRate1Minus1 = m_dispatchDrawInstanceStepRate1Minus1;
		//NOTE: only required by VRB shader
		pDispatchDrawData->m_bufferVrb = m_dispatchDrawSharedData.m_bufferVrb;

		// Pass a pointer to the dispatch draw data for the CUE to pass to shaders as kShaderInputUsagePtrDispatchDraw.
		// Its contents must not be modified until after this dispatchDraw call's shaders have finished running.
		m_lwcue.setDispatchDrawData(pDispatchDrawData, sizeof(DispatchDrawV0Data));
	}

	// m_cue.preDispatchDraw looks up settings from the currently set CsVsShader
	// NOTE: For the standard triangle culling dispatch draw shaders, many of these settings are fixed:
	//		orderedAppendMode = kDispatchOrderedAppendModeIndexPerThreadgroup
	//		dispatchDrawMode = kDispatchDrawModeIndexRingBufferOnly
	//		dispatchDrawIndexDeallocMask = 0xFC00  (CsShader.m_dispatchDrawIndexDeallocNumBits = 10)
	//		user SGPR locations may vary depending on the vertex shader source which is compiled
	Gnm::DispatchOrderedAppendMode orderedAppendMode;	//from CsShader.m_orderedAppendMode
	Gnm::DispatchDrawMode dispatchDrawMode = Gnm::kDispatchDrawModeIndexRingBufferOnly;	//from VsShader inputUsageSlots (kDispatchDrawModeIndexAndVertexRingBuffers if sgprVrbLoc is found)
	uint32_t dispatchDrawIndexDeallocMask = 0;	//from CsShader.m_dispatchDrawIndexDeallocNumBits
	uint32_t sgprKrbLoc = 0;	//get the user SGPR index from the CsShader inputUsageSlots kShaderInputUsageImmGdsKickRingBufferOffset, which must always be present
	uint32_t sgprVrbLoc = 0;	//get the user SGPR index from the VsShader inputUsageSlots kShaderInputUsageImmVertexRingBufferOffset, if any, or (uint32_t)-1 if not found; dispatchDrawMode returns kDispatchDrawModeIndexAndVertexRingBuffer if found.
	uint32_t sgprInstancesCs = 0;	//get the user SGPR index from the CsShader inputUsageSlots kShaderInputUsageImmDispatchDrawInstances, if any, or (uint32_t)-1 if not found
	uint32_t sgprInstancesVs = 0;	//get the user SGPR index from the VsShader inputUsageSlots kShaderInputUsageImmDispatchDrawInstances, if any, or (uint32_t)-1 if not found

	// Tell the LCUE to set up CCB constant data for the current set shaders:
	m_lwcue.preDispatchDraw(&orderedAppendMode, &dispatchDrawIndexDeallocMask, &sgprKrbLoc, &sgprInstancesCs, &dispatchDrawMode, &sgprVrbLoc, &sgprInstancesVs);
	// Notify the GPU of the dispatchDrawIndexDeallocMask required by the current CsShader:
	if (dispatchDrawIndexDeallocMask != m_dispatchDrawIndexDeallocMask)
	{
		m_dispatchDrawIndexDeallocMask = dispatchDrawIndexDeallocMask;
		m_dcb.setDispatchDrawIndexDeallocationMask(dispatchDrawIndexDeallocMask);
	}

	uint32_t maxInstancesPerCall = (dispatchDrawIndexDeallocMask >> pDispatchDrawIndexData->m_numIndexBits) + ((dispatchDrawIndexDeallocMask & (0xFFFFU << pDispatchDrawIndexData->m_numIndexSpaceBits)) != dispatchDrawIndexDeallocMask ? 1 : 0);
	if (maxInstancesPerCall == 0)
	{
		// For the standard triangle culling dispatch draw shaders, dispatchDrawIndexDeallocMask = 0xFC00,
		// which limits dispatchDraw calls to use no more than 63K (0xFC00) indices:
		uint32_t mask = dispatchDrawIndexDeallocMask, dispatchDrawIndexDeallocNumBits = 0;
		if (!(mask & 0xFF))	mask >>= 8, dispatchDrawIndexDeallocNumBits |= 8;
		if (!(mask & 0xF))	mask >>= 4, dispatchDrawIndexDeallocNumBits |= 4;
		if (!(mask & 0x3))	mask >>= 2, dispatchDrawIndexDeallocNumBits |= 2;
		dispatchDrawIndexDeallocNumBits |= (0x1 &~ mask);
		SCE_GNM_ASSERT_MSG(maxInstancesPerCall > 0, "dispatchDraw requires numIndexBits (%u) < 16 or numIndexSpaceBits (%u) > m_dispatchDrawIndexDeallocNumBits (%u) for the asynchronous compute shader", pDispatchDrawIndexData->m_numIndexBits, pDispatchDrawIndexData->m_numIndexSpaceBits, dispatchDrawIndexDeallocNumBits);
		return;
	}

	uint32_t numInstancesMinus1 = (m_dispatchDrawNumInstancesMinus1 & 0xFFFF);
	uint32_t numCalls = 1, numTgY = 1, numInstancesPerCall = 1;
	uint32_t numTgYLastCall = 1, numInstancesLastCall = 1;
	if (numInstancesMinus1 != 0)
	{
		// To implement instancing in software in an efficient way, the CS shader can pack some of the bits
		// of the instanceId into the output index data, provided the output indices are using a small enough
		// range of the available 63K index space.
		// This allows each dispatchDraw command buffer command to render multiple instances, reducing the
		// command processing and dispatch/draw overhead by a corresponding factor.  
		// If the total number of instances is greater than the number which can be rendered within a single 
		// dispatch, only a change to the kShaderInputUsageImmDispatchDrawInstances user SGPRs is required
		// between each dispatch, which keeps the overhead of additional instances to a minimum.
		// In addition, if an object is smaller than 256 triangles, it becomes possible to render multiple
		// instances of that object in a single thread group (each of which processes up to 512 triangles),
		// which correspondingly reduces the number of thread groups launched.

		// Here, we calculate how many dispatches and how many thread groups we will have to launch to
		// render the required total number of instances:
		uint32_t numInstances = numInstancesMinus1 + 1;
		uint32_t maxInstancesPerTg = pDispatchDrawIndexData->m_numInstancesPerTgMinus1+1;
		numInstancesLastCall = numInstances;
		numInstancesPerCall = numInstances;
		if (numInstances > maxInstancesPerCall)
		{
			numCalls = (numInstances + maxInstancesPerCall-1)/maxInstancesPerCall;
			numInstancesPerCall = maxInstancesPerCall;
			numInstancesLastCall -= (numCalls - 1) * maxInstancesPerCall;
		}
		if (numInstancesPerCall > maxInstancesPerTg)
			numTgY = (numInstancesPerCall + maxInstancesPerTg-1)/maxInstancesPerTg;
		if (numInstancesLastCall > maxInstancesPerTg)
			numTgYLastCall = (numInstancesLastCall + maxInstancesPerTg-1)/maxInstancesPerTg;
#ifdef SCE_GNM_DEBUG
		SCE_GNM_ASSERT_MSG(numInstancesPerCall*(numCalls-1) + numInstancesLastCall == numInstances && numTgY*(numCalls-1) + numTgYLastCall == (numInstances + maxInstancesPerTg-1)/maxInstancesPerTg, "dispatchDraw instancing internal error");
#endif
	}

	SCE_GNM_ASSERT_MSG(sgprKrbLoc < 16, "dispatchDraw requires an asynchronous compute shader with a kShaderInputUsageImmKickRingBufferOffset userdata sgpr");
	SCE_GNM_ASSERT_MSG(dispatchDrawMode == Gnm::kDispatchDrawModeIndexRingBufferOnly || sgprVrbLoc < 16, "dispatchDraw with a vertex ring buffer requires a VS shader with a kShaderInputUsageImmVertexRingBufferOffset userdata sgpr");
	SCE_GNM_ASSERT_MSG(dispatchDrawMode == Gnm::kDispatchDrawModeIndexRingBufferOnly || (m_dispatchDrawFlags & kDispatchDrawFlagVrbValid), "dispatchDraw with a vertex ring buffer requires setupDispatchDrawRingBuffers to be called with a valid vertex ring buffer");
	SCE_GNM_ASSERT_MSG((numInstancesMinus1 == 0) || (sgprInstancesCs < 16 && sgprInstancesVs < 16), "dispatchDraw with instancing requires asynchronous compute and VS shaders with kShaderInputUsageImmDispatchDrawInstances userdata sgprs");

	// Here we iterate over however many dispatchDraw commands are required to render all requested instances,
	// adjusting the kShaderInputUsageImmDispatchDrawInstances user SGPRs for each call:
	uint32_t firstInstance = 0;
	for (uint32_t nCall = 0; nCall+1 < numCalls; ++nCall, firstInstance += numInstancesPerCall)
	{
		uint32_t dispatchDrawInstances = (firstInstance<<16)|(numInstancesPerCall-1);
		m_acb.setUserData(sgprInstancesCs, dispatchDrawInstances);
		m_dcb.setUserData(Gnm::kShaderStageVs, sgprInstancesVs, dispatchDrawInstances);
		m_acb.dispatchDraw(pDispatchDrawIndexData->m_numIndexDataBlocks, numTgY, 1, orderedAppendMode, sgprKrbLoc);
		m_dcb.dispatchDraw(Gnm::kPrimitiveTypeTriList, indexOffset, primGroupIndexCount, primGroupThreshold, pollIntervalThreshold, dispatchDrawMode, sgprVrbLoc, modifier);
	}
	{
		uint32_t dispatchDrawInstances = (firstInstance<<16)|(numInstancesLastCall-1);
		if (sgprInstancesCs < 16)
			m_acb.setUserData(sgprInstancesCs, dispatchDrawInstances);
		if (sgprInstancesVs < 16)
			m_dcb.setUserData(Gnm::kShaderStageVs, sgprInstancesVs, dispatchDrawInstances);
	}
	m_acb.dispatchDraw(pDispatchDrawIndexData->m_numIndexDataBlocks, numTgYLastCall, 1, orderedAppendMode, sgprKrbLoc);
	m_dcb.dispatchDraw(Gnm::kPrimitiveTypeTriList, indexOffset, primGroupIndexCount, primGroupThreshold, pollIntervalThreshold, dispatchDrawMode, sgprVrbLoc, modifier);
}


void LightweightGfxContext::setEsGsRingBuffer(void* ringBuffer, uint32_t ringSizeInBytes, uint32_t maxExportVertexSizeInDword)
{
	SCE_GNM_ASSERT(ringBuffer != NULL && ringSizeInBytes != 0);

	Gnm::Buffer ringReadDescriptor;
	Gnm::Buffer ringWriteDescriptor;
	ringReadDescriptor.initAsEsGsReadDescriptor(ringBuffer, ringSizeInBytes);
	ringWriteDescriptor.initAsEsGsWriteDescriptor(ringBuffer, ringSizeInBytes);

	m_lwcue.setGlobalDescriptor(Gnm::kShaderGlobalResourceEsGsReadDescriptor, &ringReadDescriptor);
	m_lwcue.setGlobalDescriptor(Gnm::kShaderGlobalResourceEsGsWriteDescriptor, &ringWriteDescriptor);

	m_dcb.setupEsGsRingRegisters(maxExportVertexSizeInDword);
}


void LightweightGfxContext::setGsVsRingBuffers(void* ringBuffer, uint32_t ringSizeInBytes, const uint32_t vertexSizePerStreamInDword[4], uint32_t maxOutputVertexCount)
{
	Gnm::Buffer ringReadDescriptor;
	Gnm::Buffer ringWriteDescriptor[4];

	ringReadDescriptor.initAsGsVsReadDescriptor(ringBuffer, ringSizeInBytes);
	ringWriteDescriptor[0].initAsGsVsWriteDescriptor(ringBuffer, 0, vertexSizePerStreamInDword, maxOutputVertexCount);
	ringWriteDescriptor[1].initAsGsVsWriteDescriptor(ringBuffer, 1, vertexSizePerStreamInDword, maxOutputVertexCount);
	ringWriteDescriptor[2].initAsGsVsWriteDescriptor(ringBuffer, 2, vertexSizePerStreamInDword, maxOutputVertexCount);
	ringWriteDescriptor[3].initAsGsVsWriteDescriptor(ringBuffer, 3, vertexSizePerStreamInDword, maxOutputVertexCount);

	m_lwcue.setGlobalDescriptor(Gnm::kShaderGlobalResourceGsVsReadDescriptor, &ringReadDescriptor);
	m_lwcue.setGlobalDescriptor(Gnm::kShaderGlobalResourceGsVsWriteDescriptor0, &ringWriteDescriptor[0]);
	m_lwcue.setGlobalDescriptor(Gnm::kShaderGlobalResourceGsVsWriteDescriptor1, &ringWriteDescriptor[1]);
	m_lwcue.setGlobalDescriptor(Gnm::kShaderGlobalResourceGsVsWriteDescriptor2, &ringWriteDescriptor[2]);
	m_lwcue.setGlobalDescriptor(Gnm::kShaderGlobalResourceGsVsWriteDescriptor3, &ringWriteDescriptor[3]);

	m_dcb.setupGsVsRingRegisters(vertexSizePerStreamInDword, maxOutputVertexCount);
}

int32_t LightweightGfxContext::submit(void)
{
	SCE_GNM_ASSERT_MSG((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "endDispatchDraw was not called before submit");

	void *dcbGpuAddrs[kMaxNumStoredSubmissions], *acbGpuAddrs[kMaxNumStoredSubmissions], *ccbGpuAddrs[kMaxNumStoredSubmissions];
	uint32_t dcbSizes[kMaxNumStoredSubmissions], acbSizes[kMaxNumStoredSubmissions], ccbSizes[kMaxNumStoredSubmissions];
	uint32_t numAcbSubmits = 0;

	// CCB is only necessary for dispatch draw, is most cases the ccbSize = 0
	// If setupDispatchDrawRingBuffers is called, the m_ccb.incrementCeCounterForDispatchDraw() in the CCB must be executed for dispatch draw setup synchronization to work
	const bool dispatchDrawActive = ((m_dispatchDrawFlags & kDispatchDrawFlagIrbValid) != 0 );
	uint32_t pipeId = 0, queueId = 0;
	if (dispatchDrawActive && m_pQueue != NULL) {
		pipeId = m_pQueue->m_pipeId;
		queueId = m_pQueue->m_queueId;
	}

	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<m_submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = m_submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = m_dcb.m_beginptr + m_submissionRanges[iSub].m_dcbStartDwordOffset;

		ccbSizes[iSub]    = m_submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
		ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? m_ccb.m_beginptr + m_submissionRanges[iSub].m_ccbStartDwordOffset : 0;

		if (m_submissionRanges[iSub].m_acbSizeInDwords) {
			uint32_t sizeofAcbInDwords = m_submissionRanges[iSub].m_acbSizeInDwords;
			uint32_t *acbGpuAddr = m_acb.m_beginptr + m_submissionRanges[iSub].m_acbStartDwordOffset;
			// skip submission ranges which contain only the pushDispatchDrawAcbSubmitMarker() inserted at the start of every acb range:
			if (!(sizeofAcbInDwords == Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker() && Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr))) {
				acbSizes[numAcbSubmits] = sizeofAcbInDwords*sizeof(uint32_t);
				acbGpuAddrs[numAcbSubmits] = (void*)acbGpuAddr;
				SCE_GNM_ASSERT_MSG(Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr), "dispatch draw ACB submission %u does not start with pushDispatchDrawAcbSubmitMarker()", iSub);
				//NOTE: if the first ACB buffer (m_acb.m_beginptr) was eliminated because it contain only the pushDispatchDrawAcbSubmitMarker(), 
				// patch m_acb.m_beginptr to point to the first ACB because the DCB only has a pointer to m_acb.m_beginptr and won't be able to find the chain otherwise:
				if (numAcbSubmits != 0 || acbGpuAddr != m_acb.m_beginptr)
					Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker(numAcbSubmits != 0 ? (uint32_t*)acbGpuAddrs[numAcbSubmits-1] : m_acb.m_beginptr, numAcbSubmits != 0 ? acbSizes[numAcbSubmits-1] : Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker()*4, pipeId, queueId, acbGpuAddr);
				++numAcbSubmits;
			}
		}
	}

	// Submit anything left over after the final stored range
	dcbSizes[m_submissionCount]    = static_cast<uint32_t>(m_dcb.m_cmdptr - m_currentDcbSubmissionStart)*4;
	dcbGpuAddrs[m_submissionCount] = (void*)m_currentDcbSubmissionStart;

	ccbSizes[m_submissionCount]    = static_cast<uint32_t>(m_ccb.m_cmdptr - m_currentCcbSubmissionStart)*4;
	ccbGpuAddrs[m_submissionCount] = (ccbSizes[m_submissionCount] > 0) ? (void*)m_currentCcbSubmissionStart : 0;

	if (m_acb.m_cmdptr > m_currentAcbSubmissionStart) {
		uint32_t sizeofAcbInDwords = static_cast<uint32_t>(m_acb.m_cmdptr - m_currentAcbSubmissionStart);
		uint32_t *acbGpuAddr = const_cast<uint32_t*>(m_currentAcbSubmissionStart);
		// skip submission ranges which contain only the pushDispatchDrawAcbSubmitMarker() inserted at the start of every acb range:
		if (!(sizeofAcbInDwords == Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker() && Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr))) {
			acbSizes[numAcbSubmits] = sizeofAcbInDwords*sizeof(uint32_t);
			acbGpuAddrs[numAcbSubmits] = acbGpuAddr;
			SCE_GNM_ASSERT_MSG(Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr), "dispatch draw ACB submission %u does not start with pushDispatchDrawAcbSubmitMarker()", m_submissionCount);
			//NOTE: if the first ACB buffer (m_acb.m_beginptr) was eliminated because it contain only the pushDispatchDrawAcbSubmitMarker(), 
			// patch m_acb.m_beginptr to point to the first ACB because the DCB only has a pointer to m_acb.m_beginptr and won't be able to find the chain otherwise:
			if (numAcbSubmits != 0 || acbGpuAddr != m_acb.m_beginptr)
				Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker(numAcbSubmits != 0 ? (uint32_t*)acbGpuAddrs[numAcbSubmits-1] : m_acb.m_beginptr, numAcbSubmits != 0 ? acbSizes[numAcbSubmits-1] : Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker()*4, pipeId, queueId, acbGpuAddr);
			++numAcbSubmits;
		}
	}
	if (numAcbSubmits != 0) {
		Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker((uint32_t*)acbGpuAddrs[numAcbSubmits-1], acbSizes[numAcbSubmits-1], pipeId, queueId, NULL);
		SCE_GNM_ASSERT_MSG(!dispatchDrawActive || (m_pQueue != NULL && m_pQueue->isMapped()), "ComputeQueue was not %s before submitting for dispatch draw", m_pQueue == NULL ? "set" : "mapped");
	}

	int32_t submitResult = Gnm::submitCommandBuffers(m_submissionCount + 1, dcbGpuAddrs, dcbSizes, ccbGpuAddrs, ccbSizes);

	if (submitResult == sce::Gnm::kSubmissionSuccess && dispatchDrawActive && numAcbSubmits)
	{
		ComputeQueue::SubmissionStatus err = m_pQueue->submit(numAcbSubmits, acbGpuAddrs, acbSizes);
		if (err == ComputeQueue::kSubmitFailQueueIsFull)
		{
			if (sce::Gnm::getErrorResponseLevel() != sce::Gnm::kErrorResponseLevelIgnore)
				sce::Gnm::printErrorMessage(__FILE__, __LINE__, __FUNCTION__, "ComputeQueue for dispatch draw is full; waiting for space...");
			do 
			{
				err = m_pQueue->submit(numAcbSubmits, acbGpuAddrs, acbSizes);
			} while (err == ComputeQueue::kSubmitFailQueueIsFull);
		}
		if (err != ComputeQueue::kSubmitOK)
			return static_cast<int32_t>(err);
	}

	return submitResult;
}


int32_t LightweightGfxContext::submit(uint64_t workloadId)
{
	SCE_GNM_ASSERT_MSG((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "endDispatchDraw was not called before submit");

	void *dcbGpuAddrs[kMaxNumStoredSubmissions], *acbGpuAddrs[kMaxNumStoredSubmissions], *ccbGpuAddrs[kMaxNumStoredSubmissions];
	uint32_t dcbSizes[kMaxNumStoredSubmissions], acbSizes[kMaxNumStoredSubmissions], ccbSizes[kMaxNumStoredSubmissions];
	uint32_t numAcbSubmits = 0;

	// CCB is only necessary for dispatch draw, is most cases the ccbSize = 0
	// If setupDispatchDrawRingBuffers is called, the m_ccb.incrementCeCounterForDispatchDraw() in the CCB must be executed for dispatch draw setup synchronization to work
	const bool dispatchDrawActive = ((m_dispatchDrawFlags & kDispatchDrawFlagIrbValid) != 0 );
	uint32_t pipeId = 0, queueId = 0;
	if (dispatchDrawActive && m_pQueue != NULL) {
		pipeId = m_pQueue->m_pipeId;
		queueId = m_pQueue->m_queueId;
	}

	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<m_submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = m_submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = m_dcb.m_beginptr + m_submissionRanges[iSub].m_dcbStartDwordOffset;

		ccbSizes[iSub]    = m_submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
		ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? m_ccb.m_beginptr + m_submissionRanges[iSub].m_ccbStartDwordOffset : 0;

		if (m_submissionRanges[iSub].m_acbSizeInDwords) {
			uint32_t sizeofAcbInDwords = m_submissionRanges[iSub].m_acbSizeInDwords;
			uint32_t *acbGpuAddr = m_acb.m_beginptr + m_submissionRanges[iSub].m_acbStartDwordOffset;
			// skip submission ranges which contain only the pushDispatchDrawAcbSubmitMarker() inserted at the start of every acb range:
			if (!(sizeofAcbInDwords == Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker() && Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr))) {
				acbSizes[numAcbSubmits] = sizeofAcbInDwords*sizeof(uint32_t);
				acbGpuAddrs[numAcbSubmits] = (void*)acbGpuAddr;
				SCE_GNM_ASSERT_MSG(Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr), "dispatch draw ACB submission %u does not start with pushDispatchDrawAcbSubmitMarker()", iSub);
				//NOTE: if the first ACB buffer (m_acb.m_beginptr) was eliminated because it contain only the pushDispatchDrawAcbSubmitMarker(), 
				// patch m_acb.m_beginptr to point to the first ACB because the DCB only has a pointer to m_acb.m_beginptr and won't be able to find the chain otherwise:
				if (numAcbSubmits != 0 || acbGpuAddr != m_acb.m_beginptr)
					Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker(numAcbSubmits != 0 ? (uint32_t*)acbGpuAddrs[numAcbSubmits-1] : m_acb.m_beginptr, numAcbSubmits != 0 ? acbSizes[numAcbSubmits-1] : Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker()*4, pipeId, queueId, acbGpuAddr);
				++numAcbSubmits;
			}
		}
	}

	// Submit anything left over after the final stored range
	dcbSizes[m_submissionCount]    = static_cast<uint32_t>(m_dcb.m_cmdptr - m_currentDcbSubmissionStart)*4;
	dcbGpuAddrs[m_submissionCount] = (void*)m_currentDcbSubmissionStart;

	ccbSizes[m_submissionCount]    = static_cast<uint32_t>(m_ccb.m_cmdptr - m_currentCcbSubmissionStart)*4;
	ccbGpuAddrs[m_submissionCount] = (ccbSizes[m_submissionCount] > 0) ? (void*)m_currentCcbSubmissionStart : 0;

	if (m_acb.m_cmdptr > m_currentAcbSubmissionStart) {
		uint32_t sizeofAcbInDwords = static_cast<uint32_t>(m_acb.m_cmdptr - m_currentAcbSubmissionStart);
		uint32_t *acbGpuAddr = const_cast<uint32_t*>(m_currentAcbSubmissionStart);
		// skip submission ranges which contain only the pushDispatchDrawAcbSubmitMarker() inserted at the start of every acb range:
		if (!(sizeofAcbInDwords == Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker() && Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr))) {
			acbSizes[numAcbSubmits] = sizeofAcbInDwords*sizeof(uint32_t);
			acbGpuAddrs[numAcbSubmits] = acbGpuAddr;
			SCE_GNM_ASSERT_MSG(Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr), "dispatch draw ACB submission %u does not start with pushDispatchDrawAcbSubmitMarker()", m_submissionCount);
			//NOTE: if the first ACB buffer (m_acb.m_beginptr) was eliminated because it contain only the pushDispatchDrawAcbSubmitMarker(), 
			// patch m_acb.m_beginptr to point to the first ACB because the DCB only has a pointer to m_acb.m_beginptr and won't be able to find the chain otherwise:
			if (numAcbSubmits != 0 || acbGpuAddr != m_acb.m_beginptr)
				Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker(numAcbSubmits != 0 ? (uint32_t*)acbGpuAddrs[numAcbSubmits-1] : m_acb.m_beginptr, numAcbSubmits != 0 ? acbSizes[numAcbSubmits-1] : Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker()*4, pipeId, queueId, acbGpuAddr);
			++numAcbSubmits;
		}
	}
	if (numAcbSubmits != 0) {
		Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker((uint32_t*)acbGpuAddrs[numAcbSubmits-1], acbSizes[numAcbSubmits-1], pipeId, queueId, NULL);
		SCE_GNM_ASSERT_MSG(!dispatchDrawActive || (m_pQueue != NULL && m_pQueue->isMapped()), "ComputeQueue was not %s before submitting for dispatch draw", m_pQueue == NULL ? "set" : "mapped");
	}

	int32_t submitResult = Gnm::submitCommandBuffers(workloadId, m_submissionCount + 1, dcbGpuAddrs, dcbSizes, ccbGpuAddrs, ccbSizes);

	if (submitResult == sce::Gnm::kSubmissionSuccess && dispatchDrawActive && numAcbSubmits)
	{
		ComputeQueue::SubmissionStatus err = m_pQueue->submit(numAcbSubmits, acbGpuAddrs, acbSizes);
		if (err == ComputeQueue::kSubmitFailQueueIsFull)
		{
			if (sce::Gnm::getErrorResponseLevel() != sce::Gnm::kErrorResponseLevelIgnore)
				sce::Gnm::printErrorMessage(__FILE__, __LINE__, __FUNCTION__, "ComputeQueue for dispatch draw is full; waiting for space...");
			do 
			{
				err = m_pQueue->submit(numAcbSubmits, acbGpuAddrs, acbSizes);
			} while (err == ComputeQueue::kSubmitFailQueueIsFull);
		}
		if (err != ComputeQueue::kSubmitOK)
			return static_cast<int32_t>(err);
	}

	return submitResult;
}

static int32_t internalSubmitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg, LightweightGfxContext* gfx)
{
	SCE_GNM_ASSERT_MSG((gfx->m_dispatchDrawFlags & BaseGfxContext::kDispatchDrawFlagInDispatchDraw) == 0, "endDispatchDraw was not called before submit");

	void *dcbGpuAddrs[BaseGfxContext::kMaxNumStoredSubmissions], *acbGpuAddrs[BaseGfxContext::kMaxNumStoredSubmissions], *ccbGpuAddrs[BaseGfxContext::kMaxNumStoredSubmissions];
	uint32_t dcbSizes[BaseGfxContext::kMaxNumStoredSubmissions], acbSizes[BaseGfxContext::kMaxNumStoredSubmissions], ccbSizes[BaseGfxContext::kMaxNumStoredSubmissions];
	uint32_t numAcbSubmits = 0;

	// CCB is only necessary for dispatch draw, is most cases the ccbSize = 0
	// If setupDispatchDrawRingBuffers is called, the m_ccb.incrementCeCounterForDispatchDraw() in the CCB must be executed for dispatch draw setup synchronization to work
	Gnmx::ComputeQueue  *pQueue = gfx->m_pQueue;
	const bool dispatchDrawActive = ((gfx->m_dispatchDrawFlags & BaseGfxContext::kDispatchDrawFlagIrbValid) != 0 );
	uint32_t pipeId = 0, queueId = 0;
	if (dispatchDrawActive && pQueue != NULL) {
		pipeId = pQueue->m_pipeId;
		queueId = pQueue->m_queueId;
	}

	const uint32_t submissionCount = gfx->m_submissionCount;
	const LightweightGfxContext::SubmissionRange *submissionRanges = gfx->m_submissionRanges;
	const uint32_t* currentDcbSubmissionStart = gfx->m_currentDcbSubmissionStart;
	const uint32_t* currentCcbSubmissionStart = gfx->m_currentCcbSubmissionStart;
	const uint32_t* currentAcbSubmissionStart = gfx->m_currentAcbSubmissionStart;
	const GnmxDrawCommandBuffer &dcb = gfx->m_dcb;
	const GnmxConstantCommandBuffer &ccb = gfx->m_ccb;
	const GnmxDispatchCommandBuffer &acb = gfx->m_acb;

	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = dcb.m_beginptr + submissionRanges[iSub].m_dcbStartDwordOffset;

		ccbSizes[iSub]    = submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
		ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? ccb.m_beginptr + submissionRanges[iSub].m_ccbStartDwordOffset : 0;

		if (submissionRanges[iSub].m_acbSizeInDwords) {
			uint32_t sizeofAcbInDwords = submissionRanges[iSub].m_acbSizeInDwords;
			uint32_t *acbGpuAddr = acb.m_beginptr + submissionRanges[iSub].m_acbStartDwordOffset;
			// skip submission ranges which contain only the pushDispatchDrawAcbSubmitMarker() inserted at the start of every acb range:
			if (!(sizeofAcbInDwords == Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker() && Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr))) {
				acbSizes[numAcbSubmits] = sizeofAcbInDwords*sizeof(uint32_t);
				acbGpuAddrs[numAcbSubmits] = (void*)acbGpuAddr;
				SCE_GNM_ASSERT_MSG(Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr), "dispatch draw ACB submission %u does not start with pushDispatchDrawAcbSubmitMarker()", iSub);
				//NOTE: if the first ACB buffer (m_acb.m_beginptr) was eliminated because it contain only the pushDispatchDrawAcbSubmitMarker(), 
				// patch m_acb.m_beginptr to point to the first ACB because the DCB only has a pointer to m_acb.m_beginptr and won't be able to find the chain otherwise:
				if (numAcbSubmits != 0 || acbGpuAddr != acb.m_beginptr)
					Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker(numAcbSubmits != 0 ? (uint32_t*)acbGpuAddrs[numAcbSubmits-1] : acb.m_beginptr, numAcbSubmits != 0 ? acbSizes[numAcbSubmits-1] : Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker()*4, pipeId, queueId, acbGpuAddr);
				++numAcbSubmits;
			}
		}
	}

	// Submit anything left over after the final stored range
	dcbSizes[submissionCount]    = static_cast<uint32_t>(dcb.m_cmdptr - currentDcbSubmissionStart)*4;
	dcbGpuAddrs[submissionCount] = (void*)currentDcbSubmissionStart;

	ccbSizes[submissionCount]    = static_cast<uint32_t>(ccb.m_cmdptr - currentCcbSubmissionStart)*4;
	ccbGpuAddrs[submissionCount] = (ccbSizes[submissionCount] > 0) ? (void*)currentCcbSubmissionStart : 0;

	if (acb.m_cmdptr > currentAcbSubmissionStart) {
		uint32_t sizeofAcbInDwords = static_cast<uint32_t>(acb.m_cmdptr - currentAcbSubmissionStart);
		uint32_t *acbGpuAddr = const_cast<uint32_t*>(currentAcbSubmissionStart);
		// skip submission ranges which contain only the pushDispatchDrawAcbSubmitMarker() inserted at the start of every acb range:
		if (!(sizeofAcbInDwords == Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker() && Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr))) {
			acbSizes[numAcbSubmits] = sizeofAcbInDwords*sizeof(uint32_t);
			acbGpuAddrs[numAcbSubmits] = acbGpuAddr;
			SCE_GNM_ASSERT_MSG(Gnm::DispatchCommandBuffer::isDispatchDrawAcbSubmitMarker(acbGpuAddr), "dispatch draw ACB submission %u does not start with pushDispatchDrawAcbSubmitMarker()", submissionCount);
			//NOTE: if the first ACB buffer (m_acb.m_beginptr) was eliminated because it contain only the pushDispatchDrawAcbSubmitMarker(), 
			// patch m_acb.m_beginptr to point to the first ACB because the DCB only has a pointer to m_acb.m_beginptr and won't be able to find the chain otherwise:
			if (numAcbSubmits != 0 || acbGpuAddr != acb.m_beginptr)
				Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker(numAcbSubmits != 0 ? (uint32_t*)acbGpuAddrs[numAcbSubmits-1] : acb.m_beginptr, numAcbSubmits != 0 ? acbSizes[numAcbSubmits-1] : Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker()*4, pipeId, queueId, acbGpuAddr);
			++numAcbSubmits;
		}
	}
	if (numAcbSubmits != 0) {
		Gnm::DispatchCommandBuffer::patchDispatchDrawAcbSubmitMarker((uint32_t*)acbGpuAddrs[numAcbSubmits-1], acbSizes[numAcbSubmits-1], pipeId, queueId, NULL);
		SCE_GNM_ASSERT_MSG(!dispatchDrawActive || (pQueue != NULL && pQueue->isMapped()), "ComputeQueue was not %s before submitting for dispatch draw", pQueue == NULL ? "set" : "mapped");
	}

	int32_t submitResult = sce::Gnm::kSubmissionSuccess;
	if (workloadId){
		submitResult = Gnm::submitAndFlipCommandBuffers(workloadId, submissionCount + 1, dcbGpuAddrs, dcbSizes, ccbGpuAddrs, ccbSizes,
														videoOutHandle, displayBufferIndex, flipMode, flipArg);
	}
	else{
		submitResult = Gnm::submitAndFlipCommandBuffers(submissionCount + 1, dcbGpuAddrs, dcbSizes, ccbGpuAddrs, ccbSizes,
														videoOutHandle, displayBufferIndex, flipMode, flipArg);
	}

	if (submitResult == sce::Gnm::kSubmissionSuccess && dispatchDrawActive && numAcbSubmits)
	{
		ComputeQueue::SubmissionStatus err = pQueue->submit(numAcbSubmits, acbGpuAddrs, acbSizes);
		if (err == ComputeQueue::kSubmitFailQueueIsFull)
		{
			if (sce::Gnm::getErrorResponseLevel() != sce::Gnm::kErrorResponseLevelIgnore)
				sce::Gnm::printErrorMessage(__FILE__, __LINE__, __FUNCTION__, "ComputeQueue for dispatch draw is full; waiting for space...");
			do 
			{
				err = pQueue->submit(numAcbSubmits, acbGpuAddrs, acbSizes);
			} while (err == ComputeQueue::kSubmitFailQueueIsFull);
		}
		if (err != ComputeQueue::kSubmitOK)
			return static_cast<int32_t>(err);
	}

	return submitResult;
}


int32_t LightweightGfxContext::submitAndFlip(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg)
{
	m_dcb.prepareFlip();
	return internalSubmitAndFlip(0, videoOutHandle, displayBufferIndex, flipMode, flipArg, this);
}


int32_t LightweightGfxContext::submitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg)
{
	m_dcb.prepareFlip();
	return internalSubmitAndFlip(workloadId, videoOutHandle, displayBufferIndex, flipMode, flipArg, this);
}


int32_t LightweightGfxContext::submitAndFlip(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
											 void *labelAddr, uint32_t value)
{
	m_dcb.prepareFlip(labelAddr, value);
	return internalSubmitAndFlip(0, videoOutHandle, displayBufferIndex, flipMode, flipArg, this);
}


int32_t LightweightGfxContext::submitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg, 
											void *labelAddr, uint32_t value)
{
	m_dcb.prepareFlip(labelAddr, value);
	return internalSubmitAndFlip(workloadId, videoOutHandle, displayBufferIndex, flipMode, flipArg, this);
}


int32_t LightweightGfxContext::submitAndFlipWithEopInterrupt(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
															 Gnm::EndOfPipeEventType eventType, void *labelAddr, uint32_t value, Gnm::CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, labelAddr, value, cacheAction);
	return internalSubmitAndFlip(0, videoOutHandle, displayBufferIndex, flipMode, flipArg, this);
}


int32_t LightweightGfxContext::submitAndFlipWithEopInterrupt(uint64_t workloadId,
															 uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
															 Gnm::EndOfPipeEventType eventType, void *labelAddr, uint32_t value, Gnm::CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, labelAddr, value, cacheAction);
	return internalSubmitAndFlip(workloadId, videoOutHandle, displayBufferIndex, flipMode, flipArg, this);
}


int32_t LightweightGfxContext::submitAndFlipWithEopInterrupt(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
															 Gnm::EndOfPipeEventType eventType, Gnm::CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, cacheAction);
	return internalSubmitAndFlip(0, videoOutHandle, displayBufferIndex, flipMode, flipArg, this);
}


int32_t LightweightGfxContext::submitAndFlipWithEopInterrupt(uint64_t workloadId,
															 uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
															 Gnm::EndOfPipeEventType eventType, Gnm::CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, cacheAction);
	return internalSubmitAndFlip(workloadId, videoOutHandle, displayBufferIndex, flipMode, flipArg, this);
}


#endif // defined(__ORBIS__)
