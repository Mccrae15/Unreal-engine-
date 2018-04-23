/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx/computecontext.h>
#include <gnm/gpumem.h>
#include <gnm/platform.h>

#include <algorithm>

using namespace sce::Gnm;
using namespace sce::Gnmx;

static bool handleReserveFailed(CommandBuffer *cb, uint32_t dwordCount, void *userData)
{
	ComputeContext &cmpc = *(ComputeContext*)userData;
	// If either command buffer has actually reached the end of its full buffer, then invoke m_bufferFullCallback (if one has been registered)
	if (static_cast<void*>(cb) == static_cast<void*>(&(cmpc.m_dcb)) && cmpc.m_dcb.m_cmdptr + dwordCount > cmpc.m_actualDcbEnd)
	{
		if (cmpc.m_cbFullCallback.m_func != 0)
			return cmpc.m_cbFullCallback.m_func(&cmpc, cb, dwordCount, cmpc.m_cbFullCallback.m_userData);
		else
		{
			SCE_GNM_ERROR("Out of DCB command buffer space, and no callback bound.");
			return false;
		}
	}
	// Register a new submission up to the current DCB command pointer
	if (cmpc.m_submissionCount >= ComputeContext::kMaxNumStoredSubmissions)
	{
		SCE_GNM_ASSERT_MSG(cmpc.m_submissionCount < ComputeContext::kMaxNumStoredSubmissions, "Out of space for stored submissions. More can be added by increasing kMaxNumStoredSubmissions.");
		return false;
	}
	cmpc.m_submissionRanges[cmpc.m_submissionCount].m_dcbStartDwordOffset = (uint32_t)(cmpc.m_currentDcbSubmissionStart - cmpc.m_dcb.m_beginptr);
	cmpc.m_submissionRanges[cmpc.m_submissionCount].m_dcbSizeInDwords     = (uint32_t)(cmpc.m_dcb.m_cmdptr - cmpc.m_currentDcbSubmissionStart);
	cmpc.m_submissionCount++;
	cmpc.m_currentDcbSubmissionStart = cmpc.m_dcb.m_cmdptr;
	// Advance CB end pointers to the next (possibly artificial) boundary -- either current+(4MB-4), or the end of the actual buffer
	cmpc.m_dcb.m_endptr = std::min(cmpc.m_dcb.m_cmdptr+kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)cmpc.m_actualDcbEnd);
	return true;
}

ComputeContext::ComputeContext(void)
{
}

ComputeContext::~ComputeContext(void)
{
}

void ComputeContext::init(void *dcbBuffer, uint32_t dcbSizeInBytes)
{
	m_dcb.init(dcbBuffer, std::min((uint32_t)Gnm::kIndirectBufferMaximumSizeInBytes, dcbSizeInBytes), handleReserveFailed, this);

	for(uint32_t iSub=0; iSub<kMaxNumStoredSubmissions; ++iSub)
	{
		m_submissionRanges[iSub].m_dcbStartDwordOffset = 0;
		m_submissionRanges[iSub].m_dcbSizeInDwords = 0;
	}
	m_submissionCount = 0;
	m_currentDcbSubmissionStart = m_dcb.m_beginptr;
	m_actualDcbEnd = (uint32_t*)dcbBuffer+(dcbSizeInBytes/4);
	m_cbFullCallback.m_func = NULL;
	m_cbFullCallback.m_userData = NULL;

	m_UsingLightweightConstantUpdateEngine = false;
	m_predicationRegionStart = nullptr;
	m_predicationConditionAddr = nullptr;
}

#if defined(__ORBIS__)
void ComputeContext::init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr)
{
	SCE_GNM_ASSERT_MSG(resourceBufferSizeInBytes == 0 || resourceBufferInGarlic != NULL, "resourceBufferInGarlic must not be NULL if resourceBufferSizeInBytes is larger than 0 bytes.");
	SCE_GNM_ASSERT_MSG(resourceBufferSizeInBytes >  0 || resourceBufferInGarlic == NULL, "resourceBufferSizeInBytes must be greater than 0 if resourceBufferInGarlic is not NULL.");

	m_dcb.init(dcbBuffer, std::min((uint32_t)Gnm::kIndirectBufferMaximumSizeInBytes, dcbBufferSizeInBytes), handleReserveFailed, this);

	for(uint32_t iSub=0; iSub<kMaxNumStoredSubmissions; ++iSub)
	{
		m_submissionRanges[iSub].m_dcbStartDwordOffset = 0;
		m_submissionRanges[iSub].m_dcbSizeInDwords = 0;
	}
	m_submissionCount = 0;
	m_currentDcbSubmissionStart = m_dcb.m_beginptr;
	m_actualDcbEnd = (uint32_t*)dcbBuffer + (dcbBufferSizeInBytes/4);
	m_cbFullCallback.m_func = NULL;
	m_cbFullCallback.m_userData = NULL;

	const int32_t kResourceBufferCount = resourceBufferInGarlic != NULL ? 1 : 0; // only one resource buffer per context, if resource buffer is provided
	uint32_t* resourceBuffer = (uint32_t*)resourceBufferInGarlic;
	m_lwcue.init(&resourceBuffer, kResourceBufferCount, resourceBufferSizeInBytes / 4, globalInternalResourceTableAddr);
	m_lwcue.setDispatchCommandBuffer(&m_dcb);

	m_UsingLightweightConstantUpdateEngine = true;
	m_predicationRegionStart = nullptr;
	m_predicationConditionAddr = nullptr;

#if SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
	// first order of business, invalidate the KCache/L1/L2 to rid us of any stale data
	m_dcb.flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionInvalidateKCache);
#endif // SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
}
#endif

void ComputeContext::reset(void)
{
	m_dcb.resetBuffer();
	// Restore end pointer to artificial limits
	m_dcb.m_endptr = std::min(m_dcb.m_cmdptr+Gnm::kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualDcbEnd);

	// Restore submission ranges to default values
	for(uint32_t iSub=0; iSub<kMaxNumStoredSubmissions; ++iSub)
	{
		m_submissionRanges[iSub].m_dcbStartDwordOffset = 0;
		m_submissionRanges[iSub].m_dcbSizeInDwords = 0;
	}
	m_submissionCount = 0;
	m_currentDcbSubmissionStart = m_dcb.m_beginptr;
	m_predicationRegionStart = nullptr;
	m_predicationConditionAddr = nullptr;

#if defined(__ORBIS__)
	// swap buffers for the LCUE
	if ( m_UsingLightweightConstantUpdateEngine )
		m_lwcue.swapBuffers();
#endif
}

void ComputeContext::setPredication(void *condAddr)
{
	uint32_t predPacketSize = Gnm::MeasureDispatchCommandBuffer().setPredication((void *)(m_dcb.m_cmdptr), 16); // dummy args; ignore them
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
