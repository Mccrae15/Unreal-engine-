/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/
#include <gnmx/basegfxcontext.h>
#include <gnm/measuredispatchcommandbuffer.h>

#include <algorithm>

using namespace sce::Gnm;
using namespace sce::Gnmx;

static const uint32_t* cmdPtrAfterIncrement(const CommandBuffer *cb, uint32_t dwordCount)
{
	if (cb->sizeOfEndBufferAllocationInDword())
		return cb->m_beginptr + cb->m_bufferSizeInDwords + dwordCount;
	else
		return cb->m_cmdptr + dwordCount;
}
static bool handleReserveFailed(CommandBuffer *cb, uint32_t dwordCount, void *userData)
{
	BaseGfxContext &gfxc = *(BaseGfxContext*)userData;
	// If either command buffer has actually reached the end of its full buffer, then invoke m_bufferFullCallback (if one has been registered)
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
	if (static_cast<void*>(cb) == static_cast<void*>(&(gfxc.m_ccb)) && cmdPtrAfterIncrement(cb, dwordCount) > gfxc.m_actualCcbEnd)
	{
		if (gfxc.m_cbFullCallback.m_func != 0)
			return gfxc.m_cbFullCallback.m_func(&gfxc, cb, dwordCount, gfxc.m_cbFullCallback.m_userData);
		else
		{
			SCE_GNM_ERROR("Out of CCB command buffer space, and no callback bound.");
			return false;
		}
	}
	if (static_cast<void*>(cb) == static_cast<void*>(&(gfxc.m_acb)) && cmdPtrAfterIncrement(cb,dwordCount) > gfxc.m_actualAcbEnd)
	{
		if (gfxc.m_cbFullCallback.m_func != 0)
			return gfxc.m_cbFullCallback.m_func(&gfxc, cb, dwordCount, gfxc.m_cbFullCallback.m_userData);
		else
		{
			SCE_GNM_ERROR("Out of ACB command buffer space, and no callback bound.");
			return false;
		}
	}
	return gfxc.splitCommandBuffers();
}

BaseGfxContext::BaseGfxContext()
{

}

BaseGfxContext::~BaseGfxContext()
{

}

void BaseGfxContext::init(void *dcbBuffer, uint32_t dcbSizeInBytes, void *ccbBuffer, uint32_t ccbSizeInBytes)
{
	m_dcb.init(dcbBuffer, std::min((uint32_t)Gnm::kIndirectBufferMaximumSizeInBytes, dcbSizeInBytes), handleReserveFailed, this);
	m_ccb.init(ccbBuffer, std::min((uint32_t)Gnm::kIndirectBufferMaximumSizeInBytes, ccbSizeInBytes), handleReserveFailed, this);
	m_acb.m_beginptr = m_acb.m_cmdptr = m_acb.m_endptr = NULL;
	m_currentAcbSubmissionStart = m_actualAcbEnd = NULL;

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
	m_currentCcbSubmissionStart = m_ccb.m_beginptr;
	m_actualDcbEnd = (uint32_t*)dcbBuffer+(dcbSizeInBytes/4);
	m_actualCcbEnd = (uint32_t*)ccbBuffer+(ccbSizeInBytes/4);
	m_cbFullCallback.m_func = NULL;
	m_cbFullCallback.m_userData = NULL;

#if SCE_GNMX_RECORD_LAST_COMPLETION
	m_recordLastCompletionMode = kRecordLastCompletionDisabled;
	m_addressOfOffsetOfLastCompletion = 0;
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
	memset(&m_dispatchDrawSharedData, 0, sizeof(m_dispatchDrawSharedData));
	m_pDispatchDrawSharedData = NULL;
	// Also initialize default back-face culling settings matching the Gnm defaults for hardware culling
	m_dispatchDrawSharedData.m_cullSettings = kDispatchDrawClipCullFlagClipSpaceOGL;
	m_dispatchDrawNumInstancesMinus1 = 0;
	m_dispatchDrawIndexDeallocMask = 0;
	m_dispatchDrawInstanceStepRate0Minus1 = 0;
	m_dispatchDrawInstanceStepRate1Minus1 = 0;
	m_dispatchDrawFlags = 0;
	m_pQueue = nullptr;
	m_predicationRegionStart = nullptr;
	m_predicationConditionAddr = nullptr;
}

void BaseGfxContext::initDispatchDrawCommandBuffer(void *acbBuffer, uint32_t acbSizeInBytes)
{
	m_acb.init(acbBuffer, std::min((uint32_t)Gnm::kIndirectBufferMaximumSizeInBytes, acbSizeInBytes), handleReserveFailed, this);
	m_currentAcbSubmissionStart = m_acb.m_beginptr;
	m_actualAcbEnd = (uint32_t*)acbBuffer+(acbSizeInBytes/4);
	if (m_acb.m_beginptr != NULL && (m_acb.m_endptr - m_acb.m_beginptr) > sce::Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker())
		m_acb.pushDispatchDrawAcbSubmitMarker();
}

bool BaseGfxContext::splitCommandBuffers(bool bAdvanceEndOfBuffer)
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
	m_submissionRanges[m_submissionCount].m_dcbStartDwordOffset = (uint32_t)(m_currentDcbSubmissionStart - m_dcb.m_beginptr);
	m_submissionRanges[m_submissionCount].m_dcbSizeInDwords     = (uint32_t)(m_dcb.m_cmdptr - m_currentDcbSubmissionStart);
	m_submissionRanges[m_submissionCount].m_ccbStartDwordOffset = (uint32_t)(m_currentCcbSubmissionStart - m_ccb.m_beginptr);
	m_submissionRanges[m_submissionCount].m_ccbSizeInDwords     = (uint32_t)(m_ccb.m_cmdptr - m_currentCcbSubmissionStart);
	m_submissionRanges[m_submissionCount].m_acbStartDwordOffset = (uint32_t)(m_currentAcbSubmissionStart - m_acb.m_beginptr);
	m_submissionRanges[m_submissionCount].m_acbSizeInDwords     = (uint32_t)(m_acb.m_cmdptr - m_currentAcbSubmissionStart);
	m_submissionCount++;

	if (bAdvanceEndOfBuffer)
	{
		advanceCmdPtrPastEndBufferAllocation(&m_dcb);
		advanceCmdPtrPastEndBufferAllocation(&m_ccb);
		advanceCmdPtrPastEndBufferAllocation(&m_acb);

		// Advance CB end pointers to the next (possibly artificial) boundary -- either current+(4MB-4), or the end of the actual buffer
		m_dcb.m_endptr = std::min(m_dcb.m_cmdptr+kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualDcbEnd);
		m_ccb.m_endptr = std::min(m_ccb.m_cmdptr+kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualCcbEnd);
		m_acb.m_endptr = std::min(m_acb.m_cmdptr+kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualAcbEnd);

		m_dcb.m_bufferSizeInDwords = (uint32_t)(m_dcb.m_endptr - m_dcb.m_beginptr);
		m_ccb.m_bufferSizeInDwords = (uint32_t)(m_ccb.m_endptr - m_ccb.m_beginptr);
		m_acb.m_bufferSizeInDwords = (uint32_t)(m_acb.m_endptr - m_acb.m_beginptr);
	}

	m_currentDcbSubmissionStart = m_dcb.m_cmdptr;
	m_currentCcbSubmissionStart = m_ccb.m_cmdptr;
	m_currentAcbSubmissionStart = m_acb.m_cmdptr;

	if (m_acb.m_beginptr != NULL && (m_acb.m_endptr - m_acb.m_beginptr) > sce::Gnm::MeasureDispatchCommandBuffer().pushDispatchDrawAcbSubmitMarker())
		m_acb.pushDispatchDrawAcbSubmitMarker();
	return true;
}

uint32_t BaseGfxContext::getRequiredSizeOfGdsDispatchDrawArea(uint32_t numKickRingBufferElems)
{
	uint32_t const kNumDispatchDrawCountersIf2Ses = 8;	// Currently using 4, but will need 4 more for VRB support
	// Base mode:	{ KRB_WPTR, KRB_RPTR, KRB_FREE, IRB_WPTR [, VRB_DEALLOC, VRB_DEALLOC_PREV, VRB_DEALLOC_SE0, VRB_DEALLOC_SE1] }	(8-byte alignment required by VRB_DEALLOC_SE0 with 2 SEs)
	uint32_t const kNumDispatchDrawCountersIf4Ses = 12;	// Currently using 4, but will need 8 more on a 4 SE system for VRB support
	// NEO mode:	{ KRB_WPTR, KRB_RPTR, KRB_FREE, IRB_WPTR [, VRB_DEALLOC, VRB_DEALLOC_PREV, <unused>, <unused>, VRB_DEALLOC_SE0, VRB_DEALLOC_SE1, VRB_DEALLOC_SE2, VRB_DEALLOC_SE3] }	(16-byte alignment required by VRB_DEALLOC_SE0 with 4 SEs)
	uint32_t const numShaderEngines = sce::Gnm::getNumShaderEngines();
	uint32_t const kNumDispatchDrawCounters = (numShaderEngines == 4 ? kNumDispatchDrawCountersIf4Ses : kNumDispatchDrawCountersIf2Ses);
	SCE_GNM_ASSERT_MSG(numKickRingBufferElems >= 1 && numKickRingBufferElems <= sce::Gnm::kMaxDispatchDrawKickRingBufferElements, "numKickRingBufferElems (%u) must be in the range [1:%u]", numKickRingBufferElems, sce::Gnm::kMaxDispatchDrawKickRingBufferElements);
	return (kNumDispatchDrawCounters + numKickRingBufferElems * 3) * 4;
}

void BaseGfxContext::setupDispatchDrawRingBuffers(void *pIndexRingBuffer, uint32_t sizeofIndexRingBufferAlign256B, void *pVertexRingBuffer, uint32_t sizeofVertexRingBufferInBytes, uint32_t numKickRingBufferElems, uint32_t gdsOffsetDispatchDrawArea, uint32_t gdsOaCounterForDispatchDrawIrb, uint32_t gdsOaCounterForDispatchDrawVrb)
{
	uint32_t const kNumDispatchDrawCountersIf2Ses = 8;	// Currently using 4, but will need 4 more for VRB support
	// Base mode:	{ KRB_WPTR, KRB_RPTR, KRB_FREE, IRB_WPTR [, VRB_DEALLOC, VRB_DEALLOC_PREV, VRB_DEALLOC_SE0, VRB_DEALLOC_SE1] }	(8-byte alignment required by VRB_DEALLOC_SE0 with 2 SEs)
	uint32_t const kNumDispatchDrawCountersIf4Ses = 12;	// Currently using 4, but will need 8 more on a 4 SE system for VRB support
	// NEO mode:	{ KRB_WPTR, KRB_RPTR, KRB_FREE, IRB_WPTR [, VRB_DEALLOC, VRB_DEALLOC_PREV, <unused>, <unused>, VRB_DEALLOC_SE0, VRB_DEALLOC_SE1, VRB_DEALLOC_SE2, VRB_DEALLOC_SE3] }	(16-byte alignment required by VRB_DEALLOC_SE0 with 4 SEs)
	uint32_t const numShaderEngines = sce::Gnm::getNumShaderEngines();
	uint32_t const kNumDispatchDrawCounters = (numShaderEngines == 4 ? kNumDispatchDrawCountersIf4Ses : kNumDispatchDrawCountersIf2Ses);

	uint32_t krbCount = numKickRingBufferElems, gdsDwOffsetKrbCounters = gdsOffsetDispatchDrawArea / 4, gdsDwOffsetKrb = gdsDwOffsetKrbCounters + kNumDispatchDrawCounters, gdsDwOffsetIrbWptr = gdsDwOffsetKrbCounters + 3;
	uint32_t sizeofIndexRingBufferInIndices = sizeofIndexRingBufferAlign256B>>1;
	uint32_t sizeofVertexRingBufferInDwords = sizeofVertexRingBufferInBytes>>2;
	SCE_GNM_ASSERT_MSG(gdsOffsetDispatchDrawArea <= sce::Gnm::kGdsMaxAddressForDispatchDrawKickRingBufferCounters, "gdsOffsetDispatchDrawArea (0x%04X) must not be greater than kGdsMaxAddressForDispatchDrawKickRingBufferCounters (0x%04X)", gdsOffsetDispatchDrawArea, sce::Gnm::kGdsMaxAddressForDispatchDrawKickRingBufferCounters);
	SCE_GNM_ASSERT_MSG(!(gdsOffsetDispatchDrawArea & (numShaderEngines * 4 - 1)), "gdsOffsetDispatchDrawArea (0x%04X) must be %u byte aligned", gdsOffsetDispatchDrawArea, numShaderEngines * 4);
	SCE_GNM_ASSERT_MSG(numKickRingBufferElems >= 1 && numKickRingBufferElems <= sce::Gnm::kMaxDispatchDrawKickRingBufferElements, "numKickRingBufferElems (%u) must be in the range [1:%u]", numKickRingBufferElems, sce::Gnm::kMaxDispatchDrawKickRingBufferElements);
	//The following is not possible because sce::Gnm::kGdsMaxAddressForDispatchDrawKickRingBuffer + sce::Gnm::kMaxDispatchDrawKickRingBufferElements*12 <= sce::Gnm::kGdsAccessibleMemorySizeInBytes
	//SCE_GNM_ASSERT_MSG((gdsDwOffsetKrb + numKickRingBufferElems*3)*4 <= sce::Gnm::kGdsAccessibleMemorySizeInBytes, "gdsOffsetDispatchDrawArea [0x%04x:0x%04x] does not fit in user accessible GDS area [0x%04x:0x%04x]", gdsOffsetDispatchDrawArea, (gdsDwOffsetKrb + numKickRingBufferElems*3)*4, 0, sce::Gnm::kGdsAccessibleMemorySizeInBytes);
	SCE_GNM_ASSERT_MSG(m_acb.m_beginptr != NULL || m_acb.m_callback.m_func != NULL, "initDispatchDrawCommandBuffer must be called before setupDispatchDrawRingBuffers");
	SCE_GNM_ASSERT_MSG(!((uintptr_t)pIndexRingBuffer & 0xFF), "pIndexRingBuffer must be 256 byte aligned");
	SCE_GNM_ASSERT_MSG(gdsOaCounterForDispatchDrawIrb < sce::Gnm::kGdsAccessibleOrderedAppendCounters, "gdsOaCounterForDispatchDrawIrb must be in the range [0:%u]", sce::Gnm::kGdsAccessibleOrderedAppendCounters-1);
	SCE_GNM_ASSERT_MSG(sizeofIndexRingBufferAlign256B >= kDispatchDrawIndexRingBufferMinimumSizeInIndices*2 && !(sizeofIndexRingBufferAlign256B & 0xFF), "sizeofIndexRingBufferAlign256B must be a multiple of 256 bytes greater than %u", sce::Gnmx::kDispatchDrawIndexRingBufferMinimumSizeInIndices*2);
	SCE_GNM_ASSERT_MSG(sizeofVertexRingBufferInBytes == 0 || (!(sizeofVertexRingBufferInBytes & (sizeofVertexRingBufferInBytes-1)) && sizeofVertexRingBufferInBytes >= kDispatchDrawVertexRingBufferMinimumSizeInVertices*24), "sizeofVertexRingBufferInBytes must be a power of 2 bytes and at least 18KB, or 0 (to disable VRB support)");
	SCE_GNM_ASSERT_MSG(sizeofVertexRingBufferInBytes == 0 || (!((uintptr_t)pVertexRingBuffer & 0x3)), "pVertexRingBuffer must be 4 byte aligned, if VRB support is enabled (sizeofVertexRingBufferInBytes != 0)");
	SCE_GNM_ASSERT_MSG(sizeofVertexRingBufferInBytes == 0 || (gdsOaCounterForDispatchDrawVrb < sce::Gnm::kGdsAccessibleOrderedAppendCounters && gdsOaCounterForDispatchDrawVrb != gdsOaCounterForDispatchDrawIrb), "gdsOaCounterForDispatchDrawVrb must be in the range [0:%u] and not equal to gdsOaCounterForDispatchDrawIrb, if VRB support is enabled (sizeofVertexRingBufferInBytes != 0)", sce::Gnm::kGdsAccessibleOrderedAppendCounters-1);

	// TODO: Jason -- Need to iterate over shader engines / neo case.
	if (sce::Gnm::getGpuMode() == sce::Gnm::kGpuModeBase)
	{
		m_acb.setComputeResourceManagementForBase(Gnm::kShaderEngine0, 0x0FD);	// ensure that CS LDS and GPR allocations can't block out PS (requires CU 1 be free of dispatch draw CS), VS (requires one of CU[0,2:8] be free of dispatch draw CS), or vshell (requires one of CU[2:8] be free of dispatch draw CS).
		m_acb.setComputeResourceManagementForBase(Gnm::kShaderEngine1, 0x0FD);	// ensure that CS LDS and GPR allocations can't block out PS (requires CU 1 be free of dispatch draw CS), VS (requires one of CU[0,2:8] be free of dispatch draw CS), or vshell (requires one of CU[2:8] be free of dispatch draw CS).
	}
	else
	{
		m_acb.setComputeResourceManagementForNeo(Gnm::kShaderEngine0, 0x0FD);	// ensure that CS LDS and GPR allocations can't block out PS (requires CU 1 be free of dispatch draw CS), VS (requires one of CU[0,2:8] be free of dispatch draw CS), or vshell (requires one of CU[2:8] be free of dispatch draw CS).
		m_acb.setComputeResourceManagementForNeo(Gnm::kShaderEngine1, 0x0FD);	// ensure that CS LDS and GPR allocations can't block out PS (requires CU 1 be free of dispatch draw CS), VS (requires one of CU[0,2:8] be free of dispatch draw CS), or vshell (requires one of CU[2:8] be free of dispatch draw CS).
		m_acb.setComputeResourceManagementForNeo(Gnm::kShaderEngine2, 0x0FD);	// ensure that CS LDS and GPR allocations can't block out PS (requires CU 1 be free of dispatch draw CS), VS (requires one of CU[0,2:8] be free of dispatch draw CS), or vshell (requires one of CU[2:8] be free of dispatch draw CS).
		m_acb.setComputeResourceManagementForNeo(Gnm::kShaderEngine3, 0x0FD);	// ensure that CS LDS and GPR allocations can't block out PS (requires CU 1 be free of dispatch draw CS), VS (requires one of CU[0,2:8] be free of dispatch draw CS), or vshell (requires one of CU[2:8] be free of dispatch draw CS).
	}

	// Clear the KRB counters to zero.
	// It is not necessary to clear the KRB entries, as the CP clears them as it allocates KRB entries to dispatchDraw calls, 
	// but it can make debugging simpler if the initial state is all zero, and the cost of doing so is minimal.
	//	m_acb.dmaData(Gnm::kDmaDataDstGds, gdsDwOffsetKrbCounters*sizeof(uint32_t), Gnm::kDmaDataSrcData, (uint64_t)0, kNumDispatchDrawCounters*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
	uint32_t dwsizeofDmaDataCmd = Gnm::MeasureDispatchCommandBuffer().dmaData(Gnm::kDmaDataDstGds, gdsOffsetDispatchDrawArea, Gnm::kDmaDataSrcData, (uint64_t)0, (kNumDispatchDrawCounters + krbCount*3)*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
	m_acb.dmaData(Gnm::kDmaDataDstGds, gdsOffsetDispatchDrawArea, Gnm::kDmaDataSrcData, (uint64_t)0, (kNumDispatchDrawCounters + krbCount*3)*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);

	// insert a special marker indicating the current dispatch draw ACB address associated with this DCB address and the start of the entire ACB
	SCE_GNM_ASSERT(m_acb.m_cmdptr >= m_acb.m_beginptr + dwsizeofDmaDataCmd);
	m_dcb.markDispatchDrawAcbAddress(m_acb.m_cmdptr - dwsizeofDmaDataCmd, m_acb.m_beginptr);

	// Configure the GDS ordered append unit counter[gdsOaCounterForDispatchDrawIrb] to watch for ds_ordered_count operations 
	// targeting GDS address gdsDwOffsetIrbWptr.  The first ds_ordered_count operation issued by each shader wavefront in this 
	// compute pipe is treated as an index ring buffer allocation and is stalled if adding the allocated size would exceed the 
	// specified ring size.  Operations targeting this address from the VS stage (or any other compute pipe or graphics stage) 
	// are treated as ring buffer frees, which will return space to the space available counter and allow more compute waves 
	// to allocate.
	m_acb.enableOrderedAppendAllocationCounter(gdsOaCounterForDispatchDrawIrb, gdsDwOffsetIrbWptr, 0, sizeofIndexRingBufferInIndices);

	// Configure the GDS ordered append unit counter[gdsOaCounterForDispatchDrawVrb] to watch for ds_ordered_count operations 
	// targeting GDS address gdsDwOffsetIrbVrbWptr = gdsDwOffsetIrbWptr+1.  The third ds_ordered_count operation issued by 
	// each shader wavefront in this compute pipe are treated as a vertex ring buffer allocation and is stalled if adding the
	// allocated size would exceed the specified ring size.  Operations targeting this address from the the VS stage (or any 
	// other compute pipe or graphics stage) are treated as ring buffer frees, which will return space to the space available 
	// counter and allow more compute waves to allocate.
	if (sizeofVertexRingBufferInBytes)
		m_acb.enableOrderedAppendAllocationCounter(gdsOaCounterForDispatchDrawVrb, gdsDwOffsetIrbWptr+1, 2, sizeofVertexRingBufferInDwords);

	// m_acb.setupDispatchDrawKickRingBuffer implicitly waits for one increment of the CS counter.
	// incrementCeCounterForDispatchDraw increments both the CE counter (for the CUE) and CS counter
	// (for dispatchDraw).
	m_ccb.incrementCeCounterForDispatchDraw();

	// Configure the compute command processor with the dispatch draw index ring buffer parameters and the GDS layout of the 
	// KRB and counters.  This sets up internal registers in the compute command processor used to handle dispatchDraw 
	// commands later in the frame.  The compute command processor will allocate a KRB entry per dispatchDraw and free KRB 
	// entries as dispatchDraw commands complete.
	m_acb.setupDispatchDrawKickRingBuffer(krbCount, gdsDwOffsetKrb, gdsDwOffsetKrbCounters);
	// As both the CS and CE counters were incremented, but m_acb.setupDispatchDrawKickRingBuffer only consumed the CS 
	// counter, the ACB must also wait for the CE counter here to keep the command processor counters in sync.
	m_acb.waitOnCe();

	// Configures the draw command processor with the dispatch draw index ring buffer parameters and the GDS layout of the 
	// KRB and counters.  This sets up internal registers in the draw command processor used to handle dispatchDraw 
	// commands later in the frame.  The draw command processor watches the active KRB entry for notifications that index 
	// data is available for processing, and issues a series of dispatch draw sub-draws which launch VS waves to consume
	// the index data out of the index ring buffer.
	m_dcb.waitForSetupDispatchDrawKickRingBuffer(krbCount, gdsDwOffsetKrb, gdsDwOffsetKrbCounters, pIndexRingBuffer, sizeofIndexRingBufferAlign256B);
	// The CE counter was incremented, so the DCB must also wait for the CE counter and increment the DE counter to keep 
	// the command processor counters in sync, much as if a draw call had been issued.
	m_dcb.waitOnCe();
	m_dcb.incrementDeCounter();

	// Set the IRB configuration and GDS layout relevant to dispatch draw shaders in the dispatch draw data
	m_dispatchDrawSharedData.m_bufferIrb.initAsDataBuffer(pIndexRingBuffer, Gnm::kDataFormatR16Uint, sizeofIndexRingBufferInIndices);
	m_dispatchDrawSharedData.m_bufferIrb.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);	// the index ring buffer must be writeable and cached in the GPU L2 for performance.  Caching it in the GPU L1 is unnecessary as the writes are streaming, but wouldn't hurt coherency as the index read up by the input assembly (IA) unit has no L1 cache and so is always effectively "GC".
	if (sizeofVertexRingBufferInBytes) {
		m_dispatchDrawSharedData.m_bufferVrb.initAsDataBuffer(pVertexRingBuffer, Gnm::kDataFormatR32Uint, sizeofVertexRingBufferInDwords);
		m_dispatchDrawSharedData.m_bufferVrb.setResourceMemoryType(Gnm::kResourceMemoryTypeGC);	// the vertex ring buffer must be writeable and cached in the GPU L2 for performance.
		m_dispatchDrawFlags |= kDispatchDrawFlagVrbValid;
	} else
		m_dispatchDrawSharedData.m_bufferVrb.m_regs[0] = m_dispatchDrawSharedData.m_bufferVrb.m_regs[1] = m_dispatchDrawSharedData.m_bufferVrb.m_regs[2] = m_dispatchDrawSharedData.m_bufferVrb.m_regs[3] = 0;
	m_dispatchDrawSharedData.m_gdsOffsetOfIrbWptr = (uint16_t)(gdsDwOffsetIrbWptr<<2);
	m_pDispatchDrawSharedData = NULL;
	m_dispatchDrawNumInstancesMinus1 = 0;	//setDispatchDrawNumInstances(1)
	m_dispatchDrawIndexDeallocMask = 0;		//reset state tracking to ensure that setDispatchDrawIndexDeallocationMask is called
	m_dispatchDrawFlags |= kDispatchDrawFlagIrbValid;
}

void BaseGfxContext::reset(void)
{
	m_dcb.resetBuffer();
	m_acb.resetBuffer();
	m_ccb.resetBuffer();
	// Restore end pointers to artificial limits
	m_dcb.m_endptr = std::min(m_dcb.m_cmdptr+Gnm::kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualDcbEnd);
	m_acb.m_endptr = std::min(m_acb.m_cmdptr+Gnm::kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualAcbEnd);
	m_ccb.m_endptr = std::min(m_ccb.m_cmdptr+Gnm::kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualCcbEnd);
	m_dcb.m_bufferSizeInDwords = (uint32_t)(m_dcb.m_endptr - m_dcb.m_beginptr);
	m_acb.m_bufferSizeInDwords = (uint32_t)(m_acb.m_endptr - m_acb.m_beginptr);
	m_ccb.m_bufferSizeInDwords = (uint32_t)(m_ccb.m_endptr - m_ccb.m_beginptr);

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
	m_dispatchDrawNumInstancesMinus1 = 0;	//setDispatchDrawNumInstances(1)
	m_dispatchDrawIndexDeallocMask = 0;		//reset state tracking to ensure that setDispatchDrawIndexDeallocationMask is called
	m_predicationRegionStart = nullptr;
	m_predicationConditionAddr = nullptr;
}
void BaseGfxContext::setOnChipGsVsLdsLayout(const uint32_t vtxSizePerStreamInDword[4], uint32_t maxOutputVtxCount)
{
	m_dcb.setupGsVsRingRegisters(vtxSizePerStreamInDword, maxOutputVtxCount);
}
void BaseGfxContext::beginDispatchDraw(uint8_t primGroupSizeMinus1)
{
	//To prevent dispatch draw deadlocks, numPrimsPerVgt must be set to a value greater than that of the internal config register VGT_VTX_VECT_EJECT_REG + 2.  VGT_VTX_VECT_EJECT_REG = 127, so numPrimsPerVgt must be at least 129 for dispatch draw
	static const uint32_t kMinPrimGroupSizeMinus1 = 128, kMaxPrimGroupSizeMinus1 = 255;
	SCE_GNM_ASSERT_MSG((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "beginDispatchDraw can't be called between beginDispatchDraw and endDispatchDraw");
	SCE_GNM_ASSERT_MSG((m_dispatchDrawFlags & kDispatchDrawFlagIrbValid) != 0, "setupDispatchDrawRingBuffers must be called before beginDispatchDraw");
	SCE_GNM_ASSERT_MSG(primGroupSizeMinus1 >= kMinPrimGroupSizeMinus1 && primGroupSizeMinus1 <= kMaxPrimGroupSizeMinus1, "beginDispatchDraw primGroupSizeMinus1 must be in the range [%u:%u]", kMinPrimGroupSizeMinus1, kMaxPrimGroupSizeMinus1);

	// The standard triangle culling dispatch draw shaders are hard coded to produce 16-bit indices:
	m_dcb.setIndexSize(Gnm::kIndexSize16ForDispatchDraw, Gnm::kCachePolicyLru);
	m_dcb.setIndexBuffer(m_dispatchDrawSharedData.m_bufferIrb.getBaseAddress());
	// Dispatch draw only supports triangle lists in the index ring buffer
	m_dcb.setPrimitiveType(Gnm::kPrimitiveTypeTriList);
	// VGT_NUM_INSTANCES must be set to 1 while dispatch draw is running; instancing is instead implemented in software controlled by setDispatchDrawNumInstances().
	m_dcb.setNumInstances(1);
	// Gnm::kVgtPartialVsWaveDisable is required to allow vertices across primitive groups to combine into the same VS wavefront.  
	// The new sub-draw flushing thresholds (the primGroupThreshold dispatchDraw() parameter) will not reduce the VS wavefront count, otherwise.
	m_dcb.setVgtControl(primGroupSizeMinus1);

	// If the previous draw was not dispatch draw, make sure the dispatch draw compute dispatches wait until the dispatch draw draw command is processed to start,
	// in order to make sure the dispatch draw compute waves do not fill up the GPU before the associated graphics waves can start consuming their output:
	uint32_t labelDispatchDraw = 0xDDC0DDC0;
	uint64_t *pLabelDispatchDraw = (uint64_t*)m_dcb.allocateFromCommandBuffer(sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8);
	*pLabelDispatchDraw = 0;
	m_dcb.writeDataInline(pLabelDispatchDraw, &labelDispatchDraw, 1, Gnm::kWriteDataConfirmDisable);
	m_acb.waitSemaphore(pLabelDispatchDraw, Gnm::kSemaphoreWaitBehaviorNone);
	m_acb.insertNop(1);

	m_dispatchDrawFlags = (m_dispatchDrawFlags &~kDispatchDrawFlagsMaskPrimGroupSizeMinus129) | (((primGroupSizeMinus1 - kMinPrimGroupSizeMinus1)<<kDispatchDrawFlagsShiftPrimGroupSizeMinus129) & kDispatchDrawFlagsMaskPrimGroupSizeMinus129) | kDispatchDrawFlagInDispatchDraw;
}

void BaseGfxContext::endDispatchDraw(IndexSize indexSize, const void* indexAddr, uint8_t primGroupSizeMinus1, VgtPartialVsWaveMode partialVsWaveMode)
{
	SCE_GNM_UNUSED(partialVsWaveMode);
	SCE_GNM_ASSERT_MSG((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) != 0, "endDispatchDraw can only be called after a corresponding beginDispatchDraw");
	SCE_GNM_ASSERT_MSG(indexSize == Gnm::kIndexSize16 || indexSize == Gnm::kIndexSize32, "indexSize can only be restored to kIndexSize16 or kIndexSize32");

	static const uint32_t kMinPrimGroupSizeMinus1 = 128;
	const uint32_t dispatchDraw_primGroupSizeMinus1 = kMinPrimGroupSizeMinus1 + ((m_dispatchDrawFlags & kDispatchDrawFlagsMaskPrimGroupSizeMinus129) >> kDispatchDrawFlagsShiftPrimGroupSizeMinus129);

	// Restore state for non-dispatchDraw rendering:
	m_dcb.setIndexSize(indexSize, Gnm::kCachePolicyBypass);
	m_dcb.setIndexBuffer(indexAddr);
	if (primGroupSizeMinus1 != dispatchDraw_primGroupSizeMinus1)
		m_dcb.setVgtControl(primGroupSizeMinus1);

	m_dispatchDrawFlags &=~kDispatchDrawFlagInDispatchDraw;
}
#if SCE_GNMX_RECORD_LAST_COMPLETION
void BaseGfxContext::initializeRecordLastCompletion(RecordLastCompletionMode mode)
{
	m_recordLastCompletionMode = mode;
	// Allocate space in the command buffer to store the byte offset of the most recently executed draw command,
	// for debugging purposes.
	m_addressOfOffsetOfLastCompletion = static_cast<uint32_t*>(allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8));
	*m_addressOfOffsetOfLastCompletion = 0;
	fillData(m_addressOfOffsetOfLastCompletion, 0xFFFFFFFF, sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}
#endif //SCE_GNMX_RECORD_LAST_COMPLETION

void BaseGfxContext::dispatchDrawComputeWaitForEndOfPipe()
{
	uint32_t labelDispatchDraw = 0xDDCEDDCE;
	uint64_t *pLabelDispatchDraw = (uint64_t*)m_dcb.allocateFromCommandBuffer(sizeof(uint64_t), Gnm::kEmbeddedDataAlignment8);
	*pLabelDispatchDraw = 0;
	m_dcb.writeAtEndOfPipe(Gnm::kEopCbDbReadsDone, Gnm::kEventWriteDestMemory, pLabelDispatchDraw, Gnm::kEventWriteSource32BitsImmediate, labelDispatchDraw, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
	m_acb.waitSemaphore(pLabelDispatchDraw, Gnm::kSemaphoreWaitBehaviorNone);
	m_acb.insertNop(1);
}
void BaseGfxContext::setRenderTarget(uint32_t rtSlot, sce::Gnm::RenderTarget const *target)
{
	if (target == NULL)
		return m_dcb.setRenderTarget(rtSlot, NULL);
	// Workaround for multiple render target bug with CMASKs but no FMASKs
	Gnm::RenderTarget rtCopy = *target;
	if (!rtCopy.getFmaskCompressionEnable()        && rtCopy.getCmaskFastClearEnable() &&
		rtCopy.getFmaskAddress256ByteBlocks() == 0 && rtCopy.getCmaskAddress256ByteBlocks() != 0)
	{
		rtCopy.disableFmaskCompressionForMrtWithCmask();
	}
	return m_dcb.setRenderTarget(rtSlot, &rtCopy);
}

void BaseGfxContext::setupDispatchDrawScreenViewport(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
{
	// The standard dispatch draw triangle culling CS shader implements frustum culling in software, 
	// and so viewport settings must be passed to the shader in Gnmx::DispatchDrawTriangleCullingData.
	// The CS shader is slightly conservative in it's culling where quantization might affect the 
	// outcome, relying on the standard clipping/culling hardware to make the final determination for 
	// borderline cases and to clip triangles that require clipping.

	// Unfortunately, we have to duplicate much of the work done in Gnmx::setupScreenViewport here:
	int32_t width = right - left;
	int32_t height = bottom - top;
	SCE_GNM_ASSERT_MSG(width  > 0 &&  width <= 16384, "right (%u) - left (%u) must be in the range [1..16384].", right, left);
	SCE_GNM_ASSERT_MSG(height > 0 && height <= 16384, "bottom (%u) - top (%u) must be in the range [1..16384].", bottom, top);

	float scale[2] = {(float)(width)*0.5f, - (float)(height)*0.5f};
	float offset[2] = {(float)(left + width*0.5f), (float)(top + height*0.5f)};

	// Set the guard band offset so that the guard band is centered around the viewport region.
	// 10xx limits hardware offset to multiples of 16 pixels
	// Primitive filtering further restricts this offset to a multiple of 64 pixels.
	int hwOffsetX = SCE_GNM_MIN(508, (int)floor(offset[0]/16.0f + 0.5f)) & ~0x3;
	int hwOffsetY = SCE_GNM_MIN(508, (int)floor(offset[1]/16.0f + 0.5f)) & ~0x3;

	// Set the guard band clip distance to the maximum possible values by calculating the minimum distance
	// from the closest viewport edge to the edge of the hardware's coordinate space
	float hwMin = -(float)((1<<23) - 0) / (float)(1<<8);
	float hwMax =  (float)((1<<23) - 1) / (float)(1<<8);
	float gbMaxX = SCE_GNM_MIN(hwMax - fabsf(scale[0]) - offset[0] + hwOffsetX*16, -fabsf(scale[0]) + offset[0] - hwOffsetX*16 - hwMin);
	float gbMaxY = SCE_GNM_MIN(hwMax - fabsf(scale[1]) - offset[1] + hwOffsetY*16, -fabsf(scale[1]) + offset[1] - hwOffsetY*16 - hwMin);
	float gbHorizontalClipAdjust = gbMaxX / fabsf(scale[0]);
	float gbVerticalClipAdjust   = gbMaxY / fabsf(scale[1]);

	// Quantization to screen quantizes to 1/256 pixel resolution, with a max error of +/- 0.5/256 pixel.
	// This translates back to a max error in (x/w) of (+/- 0.5/256) / viewport_scale.x and (y/w) of (+/- 0.5/256) / viewport_scale.y.
	float fQuantErrorX = 0.5f / (fabsf(scale[0]) * 256.0f);
	float fQuantErrorY = 0.5f / (fabsf(scale[1]) * 256.0f);

	m_dispatchDrawSharedData.m_quantErrorScreenX = fQuantErrorX;
	m_dispatchDrawSharedData.m_quantErrorScreenY = fQuantErrorY;
	m_dispatchDrawSharedData.m_gbHorizClipAdjust = gbHorizontalClipAdjust;
	m_dispatchDrawSharedData.m_gbVertClipAdjust = gbVerticalClipAdjust;
	m_pDispatchDrawSharedData = NULL;
}

void BaseGfxContext::setupDispatchDrawClipCullSettings(PrimitiveSetup primitiveSetup, ClipControl clipControl)
{
	// The standard dispatch draw triangle culling CS shader implements back-face culling in software, 
	// and so settings which affect back-face culling must be passed to the shader in Gnmx::DispatchDrawTriangleCullingData.
	// The CS shader is slightly conservative in it's culling where quantization might affect the 
	// outcome, relying on the standard clipping/culling hardware to make the final determination for 
	// borderline cases.

	Gnm::ClipControlClipSpace clipSpace = (Gnm::ClipControlClipSpace)((clipControl.m_reg >> 19) & 1);	//SCE_GNM_GET_FIELD(clipControl.m_reg, PA_CL_CLIP_CNTL, DX_CLIP_SPACE_DEF);

	uint32_t cullFront = primitiveSetup.m_reg & 1;			//SCE_GNM_GET_FIELD(primitiveSetup.m_reg, PA_SU_SC_MODE_CNTL, CULL_FRONT);
	uint32_t cullBack = (primitiveSetup.m_reg >> 1) & 1;	//SCE_GNM_GET_FIELD(primitiveSetup.m_reg, PA_SU_SC_MODE_CNTL, CULL_BACK);
	Gnm::PrimitiveSetupFrontFace frontFace = (Gnm::PrimitiveSetupFrontFace)((primitiveSetup.m_reg >> 2) & 1);	//SCE_GNM_GET_FIELD(primitiveSetup.m_reg, PA_SU_SC_MODE_CNTL, FACE);

	uint32_t dispatchDrawClipCullFlags = (clipSpace == Gnm::kClipControlClipSpaceOGL) ? Gnmx::kDispatchDrawClipCullFlagClipSpaceOGL : Gnmx::kDispatchDrawClipCullFlagClipSpaceDX;
	if (frontFace == Gnm::kPrimitiveSetupFrontFaceCcw) {
		if (cullBack)	dispatchDrawClipCullFlags |= Gnmx::kDispatchDrawClipCullFlagCullCW;
		if (cullFront)	dispatchDrawClipCullFlags |= Gnmx::kDispatchDrawClipCullFlagCullCCW;
	} else {
		if (cullBack)	dispatchDrawClipCullFlags |= Gnmx::kDispatchDrawClipCullFlagCullCCW;
		if (cullFront)	dispatchDrawClipCullFlags |= Gnmx::kDispatchDrawClipCullFlagCullCW;
	}
	setupDispatchDrawClipCullSettings(dispatchDrawClipCullFlags);
}
#ifdef SCE_GNMX_ENABLE_GFXCONTEXT_CALLCOMMANDBUFFERS
void BaseGfxContext::callCommandBuffers(void *dcbAddr, uint32_t dcbSizeInDwords, void *ccbAddr, uint32_t ccbSizeInDwords)
{
	if (dcbSizeInDwords > 0)
	{
		SCE_GNM_ASSERT_MSG(dcbAddr != NULL, "dcbAddr must not be NULL if dcbSizeInDwords > 0");
		m_dcb.chainCommandBuffer(dcbAddr, dcbSizeInDwords);
	}
	if (ccbSizeInDwords > 0)
	{
		SCE_GNM_ASSERT_MSG(ccbAddr != NULL, "ccbAddr must not be NULL if ccbSizeInDwords > 0");
		m_ccb.chainCommandBuffer(ccbAddr, ccbSizeInDwords);
	}
	if (dcbSizeInDwords == 0 && ccbSizeInDwords == 0)
	{
		return;
	}
	splitCommandBuffers();
}
#endif //SCE_GNMX_ENABLE_GFXCONTEXT_CALLCOMMANDBUFFERS

void BaseGfxContext::setPredication(void *condAddr)
{
	uint32_t predPacketSize = Gnm::MeasureDrawCommandBuffer().setPredication((void *)(m_dcb.m_cmdptr), 16); // dummy args; ignore them
	if (condAddr != nullptr)
	{
		// begin predication region
		SCE_GNM_ASSERT_MSG(m_predicationRegionStart == NULL, "setPredication() can not be called inside an open predication region; call endPredication() first.");
		m_dcb.insertNop(predPacketSize); // placeholder; will be overwritten when the region is closed
		m_predicationRegionStart = m_dcb.m_cmdptr; // first address after the predication packet, since the packet may have caused a new command buffer allocation.
		m_predicationConditionAddr = condAddr;
	}
	else
	{
		// end predication region
		SCE_GNM_ASSERT_MSG(m_predicationRegionStart != NULL, "endPredication() must be called inside an open predication region; call setPredication() first.");
		SCE_GNM_ASSERT_MSG(m_predicationRegionStart >= m_currentDcbSubmissionStart, "predication region [0x%p-0x%p] must not cross a command buffer boundary (0x%p)",
			m_predicationRegionStart, m_dcb.m_cmdptr, m_currentDcbSubmissionStart);
		uint32_t regionSizeDw = uint32_t(m_dcb.m_cmdptr - m_predicationRegionStart);
		// rewrite previous packet, now that we know the proper size
		uint32_t *currentCmdPtr = m_dcb.m_cmdptr;
		m_dcb.m_cmdptr = m_predicationRegionStart - predPacketSize;
		m_dcb.setPredication(m_predicationConditionAddr, regionSizeDw);
		m_dcb.m_cmdptr = currentCmdPtr;
		m_predicationRegionStart = nullptr;
		m_predicationConditionAddr = nullptr;
	}
}
