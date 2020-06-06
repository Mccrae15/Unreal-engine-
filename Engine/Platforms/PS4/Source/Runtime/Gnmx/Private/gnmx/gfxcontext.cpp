/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 07.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
*/

#include <gnmx/gfxcontext.h>

#include <gnm/buffer.h>
#include <gnm/gpumem.h>
#include <gnm/platform.h>
#include <gnm/rendertarget.h>
#include <gnm/tessellation.h>
#include <gnm/measuredispatchcommandbuffer.h>

#include <algorithm>

using namespace sce::Gnm;
using namespace sce::Gnmx;

GfxContext::GfxContext(void)
{
}

GfxContext::~GfxContext(void)
{
}

void GfxContext::init(void *cueHeapAddr, uint32_t numRingEntries,
					  void *dcbBuffer, uint32_t dcbSizeInBytes, void *ccbBuffer, uint32_t ccbSizeInBytes)
{
	BaseGfxContext::init(dcbBuffer,dcbSizeInBytes,ccbBuffer,ccbSizeInBytes);
	m_cue.init( cueHeapAddr, numRingEntries);
	m_cue.bindCommandBuffers(&m_dcb, &m_ccb);
}


void GfxContext::init(void *cueHeapAddr, uint32_t numRingEntries, ConstantUpdateEngine::RingSetup ringSetup,
					  void *dcbBuffer, uint32_t dcbSizeInBytes, void *ccbBuffer, uint32_t ccbSizeInBytes)
{
	BaseGfxContext::init(dcbBuffer,dcbSizeInBytes,ccbBuffer,ccbSizeInBytes);
	m_cue.init(cueHeapAddr, numRingEntries, ringSetup);
	m_cue.bindCommandBuffers(&m_dcb, &m_ccb);
}


void GfxContext::reset(void)
{
	m_cue.advanceFrame();
	// Unbind all shaders in the CUE
	BaseGfxContext::reset();
}

#if defined(__ORBIS__)
int32_t GfxContext::submit(void)
{
	return submit(0);
}
int32_t GfxContext::submit(uint64_t workloadId)
{
	const bool submitCcbs = ccbIsActive();
	void *dcbGpuAddrs[kMaxNumStoredSubmissions+1], *ccbGpuAddrs[kMaxNumStoredSubmissions+1];
	uint32_t dcbSizes[kMaxNumStoredSubmissions+1], ccbSizes[kMaxNumStoredSubmissions+1];

	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<m_submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = m_submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = m_dcb.m_beginptr + m_submissionRanges[iSub].m_dcbStartDwordOffset;
		if (submitCcbs) {
			ccbSizes[iSub]    = m_submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
			ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? m_ccb.m_beginptr + m_submissionRanges[iSub].m_ccbStartDwordOffset : 0;
		}
	}
	// Submit anything left over after the final stored range
	dcbSizes[m_submissionCount]    = static_cast<uint32_t>(m_dcb.m_cmdptr - m_currentDcbSubmissionStart)*4;
	dcbGpuAddrs[m_submissionCount] = (void*)m_currentDcbSubmissionStart;
	if (submitCcbs) {
		ccbSizes[m_submissionCount]    = static_cast<uint32_t>(m_ccb.m_cmdptr - m_currentCcbSubmissionStart)*4;
		ccbGpuAddrs[m_submissionCount] = (ccbSizes[m_submissionCount] > 0) ? (void*)m_currentCcbSubmissionStart : nullptr;
	}
	
	void** submitCcbGpuAddrs = NULL;
	uint32_t* submitCcbSizes = NULL;
	if (submitCcbs) {
		submitCcbGpuAddrs = ccbGpuAddrs;
		submitCcbSizes = ccbSizes;
	}

	int errSubmit;
	if(workloadId)
		errSubmit = Gnm::submitCommandBuffers(workloadId,m_submissionCount+1, dcbGpuAddrs, dcbSizes, submitCcbGpuAddrs, submitCcbSizes);
	else
		errSubmit = Gnm::submitCommandBuffers(m_submissionCount+1, dcbGpuAddrs, dcbSizes, submitCcbGpuAddrs, submitCcbSizes);

	return errSubmit;
}

int32_t GfxContext::validate(void)
{
	void *dcbGpuAddrs[kMaxNumStoredSubmissions+1], *ccbGpuAddrs[kMaxNumStoredSubmissions+1];
	uint32_t dcbSizes[kMaxNumStoredSubmissions+1], ccbSizes[kMaxNumStoredSubmissions+1];

	const bool submitCcbs = ccbIsActive();

	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<m_submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = m_submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = m_dcb.m_beginptr + m_submissionRanges[iSub].m_dcbStartDwordOffset;
		if (submitCcbs) {
			ccbSizes[iSub]    = m_submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
			ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? m_ccb.m_beginptr + m_submissionRanges[iSub].m_ccbStartDwordOffset : nullptr;
		}
		else {
			ccbSizes[iSub]    = 0;
			ccbGpuAddrs[iSub] = nullptr;
		}
	}
	// Submit anything left over after the final stored range
	dcbSizes[m_submissionCount]    = static_cast<uint32_t>(m_dcb.m_cmdptr - m_currentDcbSubmissionStart)*4;
	dcbGpuAddrs[m_submissionCount] = (void*)m_currentDcbSubmissionStart;
	if (submitCcbs) {
		ccbSizes[m_submissionCount]    = static_cast<uint32_t>(m_ccb.m_cmdptr - m_currentCcbSubmissionStart)*4;
		ccbGpuAddrs[m_submissionCount] = (ccbSizes[m_submissionCount] > 0) ? (void*)m_currentCcbSubmissionStart : nullptr;
	}
	else
	{
		ccbSizes[m_submissionCount]    = 0;
		ccbGpuAddrs[m_submissionCount] = nullptr;
	}

	return Gnm::validateDrawCommandBuffers(m_submissionCount+1, dcbGpuAddrs, dcbSizes, ccbGpuAddrs, ccbSizes);
}
static int internalSubmitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
								  GfxContext* gfx)
{
	const bool submitCcbs = gfx->ccbIsActive();
	
	void *dcbGpuAddrs[GfxContext::kMaxNumStoredSubmissions+1], *ccbGpuAddrs[GfxContext::kMaxNumStoredSubmissions+1];
	uint32_t dcbSizes[GfxContext::kMaxNumStoredSubmissions+1], ccbSizes[GfxContext::kMaxNumStoredSubmissions+1];

	const uint32_t submissionCount = gfx->m_submissionCount;
	const GfxContext::SubmissionRange *submissionRanges = gfx->m_submissionRanges;
	const uint32_t* currentDcbSubmissionStart = gfx->m_currentDcbSubmissionStart;
	const uint32_t* currentCcbSubmissionStart = gfx->m_currentCcbSubmissionStart;
	const GnmxDrawCommandBuffer &dcb = gfx->m_dcb;
	const GnmxConstantCommandBuffer &ccb = gfx->m_ccb;
	
	// Submit each previously stored range
	for(uint32_t iSub=0; iSub<submissionCount; ++iSub)
	{
		dcbSizes[iSub]    = submissionRanges[iSub].m_dcbSizeInDwords*sizeof(uint32_t);
		dcbGpuAddrs[iSub] = dcb.m_beginptr + submissionRanges[iSub].m_dcbStartDwordOffset;
		if (submitCcbs) {
			ccbSizes[iSub]    = submissionRanges[iSub].m_ccbSizeInDwords*sizeof(uint32_t);
			ccbGpuAddrs[iSub] = (ccbSizes[iSub] > 0) ? ccb.m_beginptr + submissionRanges[iSub].m_ccbStartDwordOffset : 0;
		}
	}
	// Submit anything left over after the final stored range
	dcbSizes[submissionCount]    = static_cast<uint32_t>(dcb.m_cmdptr - currentDcbSubmissionStart)*4;
	dcbGpuAddrs[submissionCount] = (void*)currentDcbSubmissionStart;
	if (submitCcbs) {
		ccbSizes[submissionCount]    = static_cast<uint32_t>(ccb.m_cmdptr - currentCcbSubmissionStart)*4;
		ccbGpuAddrs[submissionCount] = (ccbSizes[submissionCount] > 0) ? (void*)currentCcbSubmissionStart : nullptr;
	}

	void** submitCcbGpuAddrs = NULL;
	uint32_t* submitCcbSizes = NULL;
	if (submitCcbs) {
		submitCcbGpuAddrs = ccbGpuAddrs;
		submitCcbSizes = ccbSizes;
	}

	int errSubmit;
	if( workloadId ) {
		errSubmit = sce::Gnm::submitAndFlipCommandBuffers(workloadId, submissionCount+1, dcbGpuAddrs, dcbSizes, submitCcbGpuAddrs, submitCcbSizes,
			videoOutHandle, displayBufferIndex, flipMode, flipArg);
	}
	else {
		errSubmit = sce::Gnm::submitAndFlipCommandBuffers( submissionCount+1, dcbGpuAddrs, dcbSizes, submitCcbGpuAddrs, submitCcbSizes,
			videoOutHandle, displayBufferIndex, flipMode, flipArg);
	}

	return errSubmit;

}
int32_t GfxContext::submitAndFlip(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg)
{
	m_dcb.prepareFlip();
	return internalSubmitAndFlip(0,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);
}
int32_t GfxContext::submitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg)
{
	m_dcb.prepareFlip();
	return internalSubmitAndFlip(workloadId,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);
}


int32_t GfxContext::submitAndFlip(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
								  void *labelAddr, uint32_t value)
{
	m_dcb.prepareFlip(labelAddr, value);
	return internalSubmitAndFlip(0,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);

}
int32_t GfxContext::submitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
								  void *labelAddr, uint32_t value)
{
	m_dcb.prepareFlip(labelAddr, value);
	return internalSubmitAndFlip(workloadId,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);
}

int32_t GfxContext::submitAndFlipWithEopInterrupt(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  EndOfPipeEventType eventType, CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, cacheAction);
	return internalSubmitAndFlip(0,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);

}
int32_t GfxContext::submitAndFlipWithEopInterrupt(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  EndOfPipeEventType eventType, CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, cacheAction);
	return internalSubmitAndFlip(workloadId,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);
}

int32_t GfxContext::submitAndFlipWithEopInterrupt(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  EndOfPipeEventType eventType, void *labelAddr, uint32_t value, CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, labelAddr, value, cacheAction);
	return internalSubmitAndFlip(0,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);

}
int32_t GfxContext::submitAndFlipWithEopInterrupt(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  EndOfPipeEventType eventType, void *labelAddr, uint32_t value, CacheAction cacheAction)
{
	m_dcb.prepareFlipWithEopInterrupt(eventType, labelAddr, value, cacheAction);
	return internalSubmitAndFlip(workloadId,videoOutHandle,displayBufferIndex,flipMode,flipArg,this);
}
#endif // defined(__ORBIS__)

void GfxContext::setTessellationDataConstantBuffer(void *tcbAddr, ShaderStage domainStage)
{
	SCE_GNM_ASSERT_MSG(domainStage == Gnm::kShaderStageEs || domainStage == Gnm::kShaderStageVs, "domainStage (%d) must be kShaderStageEs or kShaderStageVs.", domainStage);

	Gnm::Buffer tcbdef;
	tcbdef.initAsConstantBuffer(tcbAddr, sizeof(Gnm::TessellationDataConstantBuffer));

	// Slot 19 is currently reserved by the compiler for the tessellation data cb:
	this->setConstantBuffers(Gnm::kShaderStageHs, Gnm::kShaderSlotTessellationDataConstantBuffer,1, &tcbdef);
	this->setConstantBuffers(domainStage, Gnm::kShaderSlotTessellationDataConstantBuffer,1, &tcbdef);
}

void GfxContext::setTessellationFactorBuffer(void *tessFactorMemoryBaseAddr)
{
	Gnm::Buffer tfbdef;
	tfbdef.initAsTessellationFactorBuffer(tessFactorMemoryBaseAddr, Gnm::kTfRingSizeInBytes);
	m_cue.setGlobalDescriptor(Gnm::kShaderGlobalResourceTessFactorBuffer, &tfbdef);
}


void GfxContext::setLsHsShaders(LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const HsShader *hsb, uint32_t numPatches)
{
	m_cue.setLsHsShaders(lsb,shaderModifier,fetchShaderAddr,hsb,numPatches);
}

void GfxContext::setLsHsShaders(LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const HsShader *hsb, uint32_t numPatches, sce::Gnm::TessellationDistributionMode distributionMode)
{
	m_cue.setLsHsShaders(lsb, shaderModifier, fetchShaderAddr, hsb, numPatches, distributionMode);
}

void GfxContext::setGsVsShaders(const GsShader *gsb)
{
	if ( gsb )
	{
		SCE_GNM_ASSERT_MSG(!gsb->isOnChip(), "setGsVsShaders called with an on-chip GS shader; use setOnChipGsVsShaders instead");
		m_dcb.enableGsMode(gsb->getGsMaxOutputPrimitiveDwordSize()); // TODO: use the entire gdb->copyShader->gsMode value
	}
	else
	{
		m_cue.setGsModeOff();
	}

	m_cue.setGsVsShaders(gsb);
}

void GfxContext::setOnChipGsVsShaders(const GsShader *gsb, uint32_t gsPrimsPerSubGroup)
{
	if ( gsb )
	{
		uint16_t esVertsPerSubGroup = (uint16_t)((gsb->m_inputVertexCountMinus1+1)*gsPrimsPerSubGroup);
		SCE_GNM_ASSERT_MSG(gsb->isOnChip(), "setOnChipGsVsShaders called with an off-chip GS shader; use setGsVsShaders instead");
		SCE_GNM_ASSERT_MSG(gsPrimsPerSubGroup > 0, "gsPrimsPerSubGroup must be greater than 0");
		SCE_GNM_ASSERT_MSG(gsPrimsPerSubGroup*gsb->getSizePerPrimitiveInBytes() <= 64*1024, "gsPrimsPerSubGroup*gsb->getSizePerPrimitiveInBytes() will not fit in 64KB LDS");
		SCE_GNM_ASSERT_MSG(esVertsPerSubGroup <= 2047, "gsPrimsPerSubGroup*(gsb->m_inputVertexCountMinus1+1) can't be greater than 2047");
		m_cue.enableGsModeOnChip(gsb->getGsMaxOutputPrimitiveDwordSize(), 
								 esVertsPerSubGroup, gsPrimsPerSubGroup);
	}
	else
	{
		m_cue.setGsModeOff();
	}

	m_cue.setGsVsShaders(gsb);
}

#if SCE_GNM_CUE2_ENABLE_CACHE
void GfxContext::setLsHsShaders(LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache, const HsShader *hsb, uint32_t numPatches, const ConstantUpdateEngine::InputParameterCache *hsCache)
{
	m_cue.setLsHsShaders(lsb, shaderModifier, fetchShaderAddr, lsCache,
						hsb,numPatches,hsCache);
}
void GfxContext::setLsHsShaders(LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache, const HsShader *hsb, uint32_t numPatches, sce::Gnm::TessellationDistributionMode distributionMode, const ConstantUpdateEngine::InputParameterCache *hsCache)
{
	m_cue.setLsHsShaders(lsb, shaderModifier, fetchShaderAddr, lsCache,
		hsb, numPatches, distributionMode, hsCache);
}

void GfxContext::setGsVsShaders(const GsShader *gsb, const ConstantUpdateEngine::InputParameterCache *cache)
{
	if ( gsb )
	{
		SCE_GNM_ASSERT_MSG(!gsb->isOnChip(), "setGsVsShaders called with an on-chip GS shader; use setOnChipGsVsShaders instead");
		m_dcb.enableGsMode(gsb->getGsMaxOutputPrimitiveDwordSize()); // TODO: use the entire gdb->copyShader->gsMode value
	}
	else
	{
		m_cue.setGsModeOff();
	}

	m_cue.setGsVsShaders(gsb,cache);
}

void GfxContext::setOnChipGsVsShaders(const GsShader *gsb, uint32_t gsPrimsPerSubGroup, const ConstantUpdateEngine::InputParameterCache *cache)
{
	if ( gsb )
	{
		uint16_t esVertsPerSubGroup = (uint16_t)((gsb->m_inputVertexCountMinus1+1)*gsPrimsPerSubGroup);
		SCE_GNM_ASSERT_MSG(gsb->isOnChip(), "setOnChipGsVsShaders called with an off-chip GS shader; use setGsVsShaders instead");
		SCE_GNM_ASSERT_MSG(gsPrimsPerSubGroup > 0, "gsPrimsPerSubGroup must be greater than 0");
		SCE_GNM_ASSERT_MSG(gsPrimsPerSubGroup*gsb->getSizePerPrimitiveInBytes() <= 64*1024, "gsPrimsPerSubGroup*gsb->getSizePerPrimitiveInBytes() will not fit in 64KB LDS");
		SCE_GNM_ASSERT_MSG(esVertsPerSubGroup <= 2047, "gsPrimsPerSubGroup*(gsb->m_inputVertexCountMinus1+1) can't be greater than 2047");
		m_cue.enableGsModeOnChip(gsb->getGsMaxOutputPrimitiveDwordSize(),
								 esVertsPerSubGroup, gsPrimsPerSubGroup);
	}
	else
	{
		m_cue.setGsModeOff();
	}

	m_cue.setGsVsShaders(gsb,cache);
}
#endif //SCE_GNM_CUE2_ENABLE_CACHE

void GfxContext::setEsGsRingBuffer(void *baseAddr, uint32_t ringSize, uint32_t maxExportVertexSizeInDword)
{
	SCE_GNM_ASSERT_MSG(baseAddr != NULL || ringSize == 0, "if baseAddr is NULL, ringSize must be 0.");
	Gnm::Buffer ringReadDescriptor;
	Gnm::Buffer ringWriteDescriptor;

	ringReadDescriptor.initAsEsGsReadDescriptor(baseAddr, ringSize);
	ringWriteDescriptor.initAsEsGsWriteDescriptor(baseAddr, ringSize);

	m_cue.setGlobalDescriptor(Gnm::kShaderGlobalResourceEsGsReadDescriptor, &ringReadDescriptor);
	m_cue.setGlobalDescriptor(Gnm::kShaderGlobalResourceEsGsWriteDescriptor, &ringWriteDescriptor);

	m_dcb.setupEsGsRingRegisters(maxExportVertexSizeInDword);
}

void GfxContext::setOnChipEsGsLdsLayout(uint32_t maxExportVertexSizeInDword)
{
	m_cue.setOnChipEsExportVertexSizeInDword((uint16_t)maxExportVertexSizeInDword);
	m_dcb.setupEsGsRingRegisters(maxExportVertexSizeInDword);
}

void GfxContext::setGsVsRingBuffers(void *baseAddr, uint32_t ringSize,
									const uint32_t vtxSizePerStreamInDword[4], uint32_t maxOutputVtxCount)
{
	SCE_GNM_ASSERT_MSG(baseAddr != NULL || ringSize == 0, "if baseAddr is NULL, ringSize must be 0.");
	SCE_GNM_ASSERT_MSG(vtxSizePerStreamInDword != NULL, "vtxSizePerStreamInDword must not be NULL.");
	Gnm::Buffer ringReadDescriptor;
	Gnm::Buffer ringWriteDescriptor;

	ringReadDescriptor.initAsGsVsReadDescriptor(baseAddr, ringSize);
	m_cue.setGlobalDescriptor(Gnm::kShaderGlobalResourceGsVsReadDescriptor, &ringReadDescriptor);

	for (uint32_t iStream = 0; iStream < 4; ++iStream)
	{
		ringWriteDescriptor.initAsGsVsWriteDescriptor(baseAddr, iStream,
													  vtxSizePerStreamInDword, maxOutputVtxCount);
		m_cue.setGlobalDescriptor((Gnm::ShaderGlobalResourceType)(Gnm::kShaderGlobalResourceGsVsWriteDescriptor0 + iStream),
								  &ringWriteDescriptor);
	}

	m_dcb.setupGsVsRingRegisters(vtxSizePerStreamInDword, maxOutputVtxCount);
}


#ifdef SCE_GNMX_ENABLE_GFXCONTEXT_CALLCOMMANDBUFFERS
void GfxContext::callCommandBuffers(void *dcbAddr, uint32_t dcbSizeInDwords, void *ccbAddr, uint32_t ccbSizeInDwords)
{
	BaseGfxContext::callCommandBuffers(dcbAddr,dcbSizeInDwords,ccbAddr,ccbSizeInDwords);
	m_cue.invalidateAllBindings();
}
#endif
