/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 07.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
*/

#if defined(__ORBIS__) // LCUE isn't supported offline

#include <gnm/commandbuffer.h>
#include <gnm/measureconstantcommandbuffer.h>
#include <gnm/measuredispatchcommandbuffer.h>
#include <gnmx/lwgfxcontext.h>
#include <algorithm>

using namespace sce;
using namespace Gnmx;

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
	// If the DCB command buffer has actually reached the end of its full buffer, then invoke m_bufferFullCallback (if one has been registered)
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
	return gfxc.splitCommandBuffers();
}


void LightweightGfxContext::init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes,
								 void* globalInternalResourceTableAddr)
{
	SCE_GNM_ASSERT(dcbBuffer != NULL);
	SCE_GNM_ASSERT(dcbBufferSizeInBytes > 0);
	SCE_GNM_ASSERT_MSG(resourceBufferSizeInBytes == 0 || resourceBufferInGarlic != NULL, "resourceBufferInGarlic must not be NULL if resourceBufferSizeInBytes is larger than 0 bytes.");
	SCE_GNM_ASSERT_MSG(resourceBufferSizeInBytes >  0 || resourceBufferInGarlic == NULL, "resourceBufferSizeInBytes must be greater than 0 if resourceBufferInGarlic is not NULL.");

	// only calling base init so we can initialize all the base class members for multiple submits, the real setup for DCB is handled below 
	BaseGfxContext::init(NULL, 0, NULL, 0);

	// init command buffers and override BaseGfxContext DCB settings
	uint32_t* dcbStartAddr = (uint32_t*)dcbBuffer;
	uint32_t  dcbSizeInBytes = ((dcbBufferSizeInBytes / 4)) * sizeof(uint32_t);
	m_dcb.init(dcbStartAddr, std::min((uint32_t)Gnm::kIndirectBufferMaximumSizeInBytes, dcbSizeInBytes), handleReserveFailed, this);
	m_currentDcbSubmissionStart = m_dcb.m_beginptr;
	m_actualDcbEnd = (uint32_t*)dcbStartAddr + (dcbSizeInBytes / 4);

	const int32_t kResourceBufferCount = resourceBufferInGarlic != NULL ? 1 : 0; // only one resource buffer per context, if resource buffer is provided
	uint32_t* resourceBuffer = (uint32_t*)resourceBufferInGarlic;
	m_lwcue.init(&resourceBuffer, kResourceBufferCount, resourceBufferSizeInBytes / 4, globalInternalResourceTableAddr);
	m_lwcue.setDrawCommandBuffer(&m_dcb);

#if SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
	// first order of business, invalidate the KCache/L1/L2 to rid us of any stale data
	m_dcb.flushShaderCachesAndWait(Gnm::kCacheActionWriteBackAndInvalidateL1andL2, Gnm::kExtendedCacheActionInvalidateKCache, Gnm::kStallCommandBufferParserDisable);
#endif // SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
}


void LightweightGfxContext::reset(void)
{
	m_dcb.resetBuffer();
	m_ccb.resetBuffer();

	// Restore end pointers to artificial limits
	m_dcb.m_endptr = std::min(m_dcb.m_cmdptr+Gnm::kIndirectBufferMaximumSizeInBytes/4, (uint32_t*)m_actualDcbEnd);
	
	// Restore buffer size in case it was modified by splitCommandBuffers
	m_dcb.m_bufferSizeInDwords = static_cast<uint32_t>(m_dcb.m_endptr - m_dcb.m_beginptr);

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
	m_submissionCount++;

	if (bAdvanceEndOfBuffer)
	{
		// If end of buffer allocations were made, it is not safe to continue using the existing command pointer address
		// as it will begin to over write embedded data with new commands. So we need to jump passed the embedded data block.
		advanceCmdPtrPastEndBufferAllocation(&m_dcb);

		// Advance CB end pointers to the next (possibly artificial) boundary -- either current+(4MB-4), or the end of the actual buffer
		m_dcb.m_endptr = std::min(m_dcb.m_cmdptr + Gnm::kIndirectBufferMaximumSizeInBytes / 4, (uint32_t*)m_actualDcbEnd);

		m_dcb.m_bufferSizeInDwords = static_cast<uint32_t>(m_dcb.m_endptr - m_dcb.m_beginptr);
	}

	m_currentDcbSubmissionStart = m_dcb.m_cmdptr;
	m_currentCcbSubmissionStart = m_ccb.m_cmdptr;

	return true;
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
	void *dcbGpuAddrs[kMaxNumStoredSubmissions], *ccbGpuAddrs[kMaxNumStoredSubmissions];
	uint32_t dcbSizes[kMaxNumStoredSubmissions], ccbSizes[kMaxNumStoredSubmissions];

	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<m_submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = m_submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = m_dcb.m_beginptr + m_submissionRanges[iSub].m_dcbStartDwordOffset;

		ccbSizes[iSub]    = m_submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
		ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? m_ccb.m_beginptr + m_submissionRanges[iSub].m_ccbStartDwordOffset : 0;
	}

	// Submit anything left over after the final stored range
	dcbSizes[m_submissionCount]    = static_cast<uint32_t>(m_dcb.m_cmdptr - m_currentDcbSubmissionStart)*4;
	dcbGpuAddrs[m_submissionCount] = (void*)m_currentDcbSubmissionStart;

	ccbSizes[m_submissionCount]    = static_cast<uint32_t>(m_ccb.m_cmdptr - m_currentCcbSubmissionStart)*4;
	ccbGpuAddrs[m_submissionCount] = (ccbSizes[m_submissionCount] > 0) ? (void*)m_currentCcbSubmissionStart : 0;

	int32_t submitResult = Gnm::submitCommandBuffers(m_submissionCount + 1, dcbGpuAddrs, dcbSizes, ccbGpuAddrs, ccbSizes);

	return submitResult;
}


int32_t LightweightGfxContext::submit(uint64_t workloadId)
{
	void *dcbGpuAddrs[kMaxNumStoredSubmissions], *ccbGpuAddrs[kMaxNumStoredSubmissions];
	uint32_t dcbSizes[kMaxNumStoredSubmissions], ccbSizes[kMaxNumStoredSubmissions];

	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<m_submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = m_submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = m_dcb.m_beginptr + m_submissionRanges[iSub].m_dcbStartDwordOffset;

		ccbSizes[iSub]    = m_submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
		ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? m_ccb.m_beginptr + m_submissionRanges[iSub].m_ccbStartDwordOffset : 0;
	}

	// Submit anything left over after the final stored range
	dcbSizes[m_submissionCount]    = static_cast<uint32_t>(m_dcb.m_cmdptr - m_currentDcbSubmissionStart)*4;
	dcbGpuAddrs[m_submissionCount] = (void*)m_currentDcbSubmissionStart;

	ccbSizes[m_submissionCount]    = static_cast<uint32_t>(m_ccb.m_cmdptr - m_currentCcbSubmissionStart)*4;
	ccbGpuAddrs[m_submissionCount] = (ccbSizes[m_submissionCount] > 0) ? (void*)m_currentCcbSubmissionStart : 0;

	int32_t submitResult = Gnm::submitCommandBuffers(workloadId, m_submissionCount + 1, dcbGpuAddrs, dcbSizes, ccbGpuAddrs, ccbSizes);

	return submitResult;
}

static int32_t internalSubmitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg, LightweightGfxContext* gfx)
{
	void *dcbGpuAddrs[BaseGfxContext::kMaxNumStoredSubmissions], *ccbGpuAddrs[BaseGfxContext::kMaxNumStoredSubmissions];
	uint32_t dcbSizes[BaseGfxContext::kMaxNumStoredSubmissions], ccbSizes[BaseGfxContext::kMaxNumStoredSubmissions];

	const uint32_t submissionCount = gfx->m_submissionCount;
	const LightweightGfxContext::SubmissionRange *submissionRanges = gfx->m_submissionRanges;
	const uint32_t* currentDcbSubmissionStart = gfx->m_currentDcbSubmissionStart;
	const uint32_t* currentCcbSubmissionStart = gfx->m_currentCcbSubmissionStart;
	const GnmxDrawCommandBuffer &dcb = gfx->m_dcb;
	const GnmxConstantCommandBuffer &ccb = gfx->m_ccb;

	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = dcb.m_beginptr + submissionRanges[iSub].m_dcbStartDwordOffset;

		ccbSizes[iSub]    = submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
		ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? ccb.m_beginptr + submissionRanges[iSub].m_ccbStartDwordOffset : 0;
	}

	// Submit anything left over after the final stored range
	dcbSizes[submissionCount]    = static_cast<uint32_t>(dcb.m_cmdptr - currentDcbSubmissionStart)*4;
	dcbGpuAddrs[submissionCount] = (void*)currentDcbSubmissionStart;

	ccbSizes[submissionCount]    = static_cast<uint32_t>(ccb.m_cmdptr - currentCcbSubmissionStart)*4;
	ccbGpuAddrs[submissionCount] = (ccbSizes[submissionCount] > 0) ? (void*)currentCcbSubmissionStart : 0;

	int32_t submitResult = sce::Gnm::kSubmissionSuccess;
	if (workloadId){
		submitResult = Gnm::submitAndFlipCommandBuffers(workloadId, submissionCount + 1, dcbGpuAddrs, dcbSizes, ccbGpuAddrs, ccbSizes,
														videoOutHandle, displayBufferIndex, flipMode, flipArg);
	}
	else{
		submitResult = Gnm::submitAndFlipCommandBuffers(submissionCount + 1, dcbGpuAddrs, dcbSizes, ccbGpuAddrs, ccbSizes,
														videoOutHandle, displayBufferIndex, flipMode, flipArg);
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
