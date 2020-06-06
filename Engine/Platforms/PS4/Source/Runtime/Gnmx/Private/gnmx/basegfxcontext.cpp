/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 07.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
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

	for(uint32_t iSub=0; iSub<kMaxNumStoredSubmissions; ++iSub)
	{
		m_submissionRanges[iSub].m_dcbStartDwordOffset = 0;
		m_submissionRanges[iSub].m_dcbSizeInDwords = 0;
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
	m_predicationRegionStart = nullptr;
	m_predicationConditionAddr = nullptr;
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
	m_submissionCount++;

	if (bAdvanceEndOfBuffer)
	{
		advanceCmdPtrPastEndBufferAllocation(&m_dcb);
		advanceCmdPtrPastEndBufferAllocation(&m_ccb);

		// Advance CB end pointers to the next (possibly artificial) boundary -- either current+(4MB-4), or the end of the actual buffer
		m_dcb.m_endptr = std::min(m_dcb.m_cmdptr+kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualDcbEnd);
		m_ccb.m_endptr = std::min(m_ccb.m_cmdptr+kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualCcbEnd);

		m_dcb.m_bufferSizeInDwords = (uint32_t)(m_dcb.m_endptr - m_dcb.m_beginptr);
		m_ccb.m_bufferSizeInDwords = (uint32_t)(m_ccb.m_endptr - m_ccb.m_beginptr);
	}

	m_currentDcbSubmissionStart = m_dcb.m_cmdptr;
	m_currentCcbSubmissionStart = m_ccb.m_cmdptr;

	return true;
}

void BaseGfxContext::reset(void)
{
	m_dcb.resetBuffer();
	m_ccb.resetBuffer();
	// Restore end pointers to artificial limits
	m_dcb.m_endptr = std::min(m_dcb.m_cmdptr+Gnm::kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualDcbEnd);
	m_ccb.m_endptr = std::min(m_ccb.m_cmdptr+Gnm::kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualCcbEnd);
	m_dcb.m_bufferSizeInDwords = (uint32_t)(m_dcb.m_endptr - m_dcb.m_beginptr);
	m_ccb.m_bufferSizeInDwords = (uint32_t)(m_ccb.m_endptr - m_ccb.m_beginptr);

	// Restore submit ranges to default values
	for(uint32_t iSub=0; iSub<kMaxNumStoredSubmissions; ++iSub)
	{
		m_submissionRanges[iSub].m_dcbStartDwordOffset = 0;
		m_submissionRanges[iSub].m_dcbSizeInDwords = 0;
		m_submissionRanges[iSub].m_ccbStartDwordOffset = 0;
		m_submissionRanges[iSub].m_ccbSizeInDwords = 0;
	}
	m_submissionCount = 0;
	m_currentDcbSubmissionStart = m_dcb.m_beginptr;
	m_currentCcbSubmissionStart = m_ccb.m_beginptr;

	m_predicationRegionStart = nullptr;
	m_predicationConditionAddr = nullptr;
}
void BaseGfxContext::setOnChipGsVsLdsLayout(const uint32_t vtxSizePerStreamInDword[4], uint32_t maxOutputVtxCount)
{
	m_dcb.setupGsVsRingRegisters(vtxSizePerStreamInDword, maxOutputVtxCount);
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
