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


void ConstantUpdateEngine::setVsShader(const sce::Gnmx::VsShader *vsb, uint32_t shaderModifier, void *fetchShaderAddr, const InputParameterCache* cache)
{
#if SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	if( m_currentVSB == vsb && m_currentFetchShader[Gnm::kShaderStageVs] == fetchShaderAddr && (m_shaderContextIsValid & (1 << Gnm::kShaderStageVs))) // can skip setting shader only if the shader is the same AND the context has not been reset
	{
		return;
	}
	m_currentFetchShader[Gnm::kShaderStageVs] = fetchShaderAddr;
#endif //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	

	StageInfo *vsStage = m_stageInfo+Gnm::kShaderStageVs;
#if SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	const bool canUpdate = (m_shaderContextIsValid & (1 << Gnm::kShaderStageVs)) && m_currentVSB && vsb && vsb->m_vsStageRegisters.isSharingContext(m_currentVSB->m_vsStageRegisters);
#endif //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	m_currentVSB  = vsb;
#if !SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE
	m_psInputs	  = NULL;	// new VS or PS -> new PS inputs table (unless the user passes one in with setPsInputTable)
#endif //!SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE

	if ( !vsb )
	{
		vsStage->shaderBaseAddr256   = 0;
		vsStage->inputUsageTable	 = 0;
		vsStage->inputUsageTableSize = 0;
		m_dirtyShader &= ~(1 << Gnm::kShaderStageVs);
		return;
	}

	m_dirtyStage |= (1 << Gnm::kShaderStageVs);
	m_dirtyShader |= (1 << Gnm::kShaderStageVs);
	m_shaderContextIsValid |= (1 << Gnm::kShaderStageVs);

	vsStage->shaderBaseAddr256	   = vsb->m_vsStageRegisters.m_spiShaderPgmLoVs;
	vsStage->shaderCodeSizeInBytes = vsb->m_common.m_shaderSize;
#if SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	if(canUpdate)
		m_dcb->updateVsShader(&vsb->m_vsStageRegisters,shaderModifier);
	else
		m_dcb->setVsShader(&vsb->m_vsStageRegisters, shaderModifier);
#else //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	m_dcb->setVsShader(&vsb->m_vsStageRegisters, shaderModifier);
#endif
	if ( fetchShaderAddr )
	{
		m_dcb->setPointerInUserData(Gnm::kShaderStageVs, 0, fetchShaderAddr);
		// TODO: Need a warning, not an assert.
		//SCE_GNM_ASSERT_MSG(vsb->m_numInputSemantics &&
		//				 vsb->getInputUsageSlotTable()[0].m_usageType == Gnm::kShaderInputUsageSubPtrFetchShader,
		//				 "VsShader vsb [0x%p] doesn't expect a fetch shader, but got one passed in fetchShaderAddr==[0x%p]",
		//				 vsb, fetchShaderAddr);
	}
	else
	{
		SCE_GNM_ASSERT_MSG(!vsb->m_numInputSemantics ||
						 vsb->getInputUsageSlotTable()[0].m_usageType != Gnm::kShaderInputUsageSubPtrFetchShader,
						 "VsShader vsb [0x%p] expects a fetch shader, but fetchShaderAddr==0", vsb);
	}

	vsStage->inputUsageTable	 = vsb->getInputUsageSlotTable();
	vsStage->inputUsageTableSize = vsb->m_common.m_numInputUsageSlots;
#if SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_ASSERT_MSG(cache, "Pointer to cached inputs cannot be NULL");		
	vsStage->activeCache = cache;
#else //SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_UNUSED(cache);
	SCE_GNM_ASSERT_MSG(!cache, "Passing a non-NULL cache when shader input caching is disabled has no effect");
#endif //SCE_GNM_CUE2_ENABLE_CACHE


	m_shaderUsesSrt &= ~(1 << kShaderStageVs);
	m_shaderUsesSrt |= vsb->m_common.m_shaderIsUsingSrt << kShaderStageVs;

	// EUD is automatically dirtied
	m_shaderDirtyEud |= (1 << kShaderStageVs);
	vsStage->eudSizeInDWord = ConstantUpdateEngineHelper::calculateEudSizeInDWord(vsStage->inputUsageTable, vsStage->inputUsageTableSize);

#if SCE_GNMX_TRACK_EMBEDDED_CB
	// Handle embedded constant buffer
	if ( vsb->m_common.m_embeddedConstantBufferSizeInDQW )
	{
		// Set up the internal constants:
		Gnm::Buffer vsEmbeddedConstBuffer;
		void *shaderAddr = (void*)( ((uintptr_t)vsb->m_vsStageRegisters.m_spiShaderPgmHiVs << 32) + vsb->m_vsStageRegisters.m_spiShaderPgmLoVs );
		vsEmbeddedConstBuffer.initAsConstantBuffer((void*)( (uintptr_t(shaderAddr)<<8) + vsb->m_common.m_shaderSize ),
												   vsb->m_common.m_embeddedConstantBufferSizeInDQW*16);

		// TODO: Add a special case function for this buffer
		setConstantBuffers(kShaderStageVs, kShaderSlotEmbeddedConstantBuffer, 1, &vsEmbeddedConstBuffer);
	}
	else
	{
		// TODO: Add a special case function for this buffer
		setConstantBuffers(kShaderStageVs, kShaderSlotEmbeddedConstantBuffer, 1, NULL);
	}
#endif //SCE_GNMX_TRACK_EMBEDDED_CB
}

void ConstantUpdateEngine::setPsShader(const sce::Gnmx::PsShader *psb, const InputParameterCache *cache)
{
#if SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	if( m_currentPSB == psb && (m_shaderContextIsValid & (1 << Gnm::kShaderStagePs)) ) // can skip setting the shader if the shader is the same AND the context has not been reset
	{
		return;
	}
#endif //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER

	StageInfo *psStage = m_stageInfo+Gnm::kShaderStagePs;

#if SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	const bool canUpdate = (m_shaderContextIsValid & (1 << Gnm::kShaderStagePs)) && ((psb && m_currentPSB && psb->m_psStageRegisters.isSharingContext(m_currentPSB->m_psStageRegisters)) || (m_currentPSB == NULL && psb == NULL));
#endif //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	
	m_currentPSB  = psb;	// note: PS is allowed to be NULL, unlike most stages
#if !SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE
	m_psInputs	  = NULL;	// new VS or PS -> new PS inputs table (unless the user passes one in with setPsInputTable)
#endif //!SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE

	if ( !psb )
	{
#if SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
		if (canUpdate)
			m_dcb->updatePsShader(0);
		else
			m_dcb->setPsShader(0);
#else //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
		m_dcb->setPsShader(0);
#endif //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
		
		psStage->shaderBaseAddr256   = 0;
		psStage->inputUsageTable	 = 0;
		psStage->inputUsageTableSize = 0;
		m_dirtyShader &= ~(1 << Gnm::kShaderStagePs);
		return;
	}

	m_dirtyStage |= (1 << Gnm::kShaderStagePs);
	m_dirtyShader |= (1 << Gnm::kShaderStagePs);
	m_shaderContextIsValid |= (1 << Gnm::kShaderStagePs);

	psStage->shaderBaseAddr256	   = psb->m_psStageRegisters.m_spiShaderPgmLoPs;
	psStage->shaderCodeSizeInBytes = psb->m_common.m_shaderSize;

#if SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	if(canUpdate)
		m_dcb->updatePsShader(&psb->m_psStageRegisters);
	else
		m_dcb->setPsShader(&psb->m_psStageRegisters);
#else //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	m_dcb->setPsShader(&psb->m_psStageRegisters);
#endif //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	
	psStage->inputUsageTable	 = psb->getInputUsageSlotTable();
	psStage->inputUsageTableSize = psb->m_common.m_numInputUsageSlots;
#if SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_ASSERT_MSG(cache, "Pointer to cached inputs cannot be NULL");
	psStage->activeCache = cache;
#else //SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_UNUSED(cache);
	SCE_GNM_ASSERT_MSG(!cache, "Passing a non-NULL cache when shader input caching is disabled has no effect");
#endif //SCE_GNM_CUE2_ENABLE_CACHE
	

	m_shaderUsesSrt &= ~(1 << kShaderStagePs);
	m_shaderUsesSrt |= psb->m_common.m_shaderIsUsingSrt << kShaderStagePs;

	// EUD is automatically dirtied
	m_shaderDirtyEud |= (1 << kShaderStagePs);
	psStage->eudSizeInDWord = ConstantUpdateEngineHelper::calculateEudSizeInDWord(psStage->inputUsageTable, psStage->inputUsageTableSize);
#if SCE_GNMX_TRACK_EMBEDDED_CB
	if ( psb->m_common.m_embeddedConstantBufferSizeInDQW )
	{
		// Setup the internal constants
		Gnm::Buffer psEmbeddedConstBuffer;
		void *shaderAddr = (void*)( ((uintptr_t)psb->m_psStageRegisters.m_spiShaderPgmHiPs << 32) + psb->m_psStageRegisters.m_spiShaderPgmLoPs );
		psEmbeddedConstBuffer.initAsConstantBuffer((void*)( (uintptr_t(shaderAddr)<<8) + psb->m_common.m_shaderSize ),
												   psb->m_common.m_embeddedConstantBufferSizeInDQW*16);

		// TODO: Add a special case function for this buffer
		setConstantBuffers(kShaderStagePs, kShaderSlotEmbeddedConstantBuffer, 1, &psEmbeddedConstBuffer);
	}
	else
	{
		// TODO: Add a special case function for this buffer
		setConstantBuffers(kShaderStagePs, kShaderSlotEmbeddedConstantBuffer, 1, NULL);
	}
#endif //SCE_GNMX_TRACK_EMBEDDED_CB
}

void ConstantUpdateEngine::setCsShader(const sce::Gnmx::CsShader *csb, const InputParameterCache *cache)
{
#if SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	if( m_currentCSB == csb ) // there are no context registers for the CS shader so always can skip if it's the same
	{
		return;
	}
#endif //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	
	StageInfo *csStage = m_stageInfo+Gnm::kShaderStageCs;

	m_currentCSB = csb;

	if ( !csb )
	{
		csStage->shaderBaseAddr256   = 0;
		csStage->inputUsageTable	 = 0;
		csStage->inputUsageTableSize = 0;
		m_dirtyShader &= ~(1 << Gnm::kShaderStageCs);
		m_gfxCsShaderOrderedAppendMode = 0;
		return;
	}

	m_dirtyStage |= (1 << Gnm::kShaderStageCs);
	m_dirtyShader |= (1 << Gnm::kShaderStageCs);

	csStage->shaderBaseAddr256	   = csb->m_csStageRegisters.m_computePgmLo;
	csStage->shaderCodeSizeInBytes = csb->m_common.m_shaderSize;
	m_gfxCsShaderOrderedAppendMode = csb->m_orderedAppendMode;

	m_dcb->setCsShader(&csb->m_csStageRegisters);
	csStage->inputUsageTable	 = csb->getInputUsageSlotTable();
	csStage->inputUsageTableSize = csb->m_common.m_numInputUsageSlots;
#if SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_ASSERT_MSG(cache, "Pointer to cached inputs cannot be NULL");
	csStage->activeCache = cache;
#else //SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_UNUSED(cache);
	SCE_GNM_ASSERT_MSG(!cache, "Passing a non-NULL cache when shader input caching is disabled has no effect");
#endif //SCE_GNM_CUE2_ENABLE_CACHE

	m_shaderUsesSrt &= ~(1 << kShaderStageCs);
	m_shaderUsesSrt |= csb->m_common.m_shaderIsUsingSrt << kShaderStageCs;

	// EUD is automatically dirtied
	m_shaderDirtyEud |= (1 << kShaderStageCs);
	csStage->eudSizeInDWord = ConstantUpdateEngineHelper::calculateEudSizeInDWord(csStage->inputUsageTable, csStage->inputUsageTableSize);
#if SCE_GNMX_TRACK_EMBEDDED_CB
	if (csb->m_common.m_embeddedConstantBufferSizeInDQW)
	{
		// Setup the internal constants
		Gnm::Buffer csEmbeddedConstBuffer;
		void *shaderAddr = (void*)( ((uintptr_t)csb->m_csStageRegisters.m_computePgmHi << 32) + csb->m_csStageRegisters.m_computePgmLo );
		csEmbeddedConstBuffer.initAsConstantBuffer((void*)( (uintptr_t(shaderAddr)<<8) + csb->m_common.m_shaderSize ),
												   csb->m_common.m_embeddedConstantBufferSizeInDQW*16);

		// TODO: Add a special case function for this buffer
		setConstantBuffers(kShaderStageCs, kShaderSlotEmbeddedConstantBuffer, 1, &csEmbeddedConstBuffer);
	}
	else
	{
		// TODO: Add a special case function for this buffer
		setConstantBuffers(kShaderStageCs, kShaderSlotEmbeddedConstantBuffer, 1, NULL);
	}
#endif //SCE_GNMX_TRACK_EMBEDDED_CB
}
static bool isRedundantLs(const ConstantUpdateEngine *cue, const LsShader *lsb, void *fetchShaderAddr)
{
#if SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	return (cue->m_currentLSB == lsb &&
		cue->m_currentFetchShader[sce::Gnm::kShaderStageLs] == fetchShaderAddr);
#else //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	SCE_GNM_UNUSED(cue);
	SCE_GNM_UNUSED(lsb);
	SCE_GNM_UNUSED(fetchShaderAddr);
	return false;
#endif //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
}
static void setLsShaderInternal(ConstantUpdateEngine *cue, const LsShader *lsb, const sce::Gnm::LsStageRegisters *patchedLs, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache)
{
	ConstantUpdateEngine::StageInfo *lsStage = cue->m_stageInfo + sce::Gnm::kShaderStageLs;
	cue->m_currentLSB = lsb;

	if (!lsb)
	{
		lsStage->shaderBaseAddr256 = 0;
		lsStage->inputUsageTable = 0;
		lsStage->inputUsageTableSize = 0;

		cue->m_dirtyShader &= ~(1 << sce::Gnm::kShaderStageLs);
		return;
	}
	cue->m_dirtyStage |= 1 << sce::Gnm::kShaderStageLs;
	cue->m_dirtyShader |= 1 << sce::Gnm::kShaderStageLs;

	lsStage->shaderBaseAddr256 = lsb->m_lsStageRegisters.m_spiShaderPgmLoLs;
	lsStage->shaderCodeSizeInBytes = lsb->m_common.m_shaderSize;

	cue->m_dcb->setLsShader(patchedLs, shaderModifier);
	if (fetchShaderAddr)
	{
		cue->m_dcb->setPointerInUserData(sce::Gnm::kShaderStageLs, 0, fetchShaderAddr);
		// TODO: Need a warning, not an assert.
		//SCE_GNM_ASSERT_MSG(lsb->m_numInputSemantics &&
		//				 lsb->getInputUsageSlotTable()[0].m_usageType == Gnm::kShaderInputUsageSubPtrFetchShader,
		//				 "LsShader lsb [0x%p] doesn't expect a fetch shader, but got one passed in fetchShaderAddr==[0x%p]",
		//				 lsb, fetchShaderAddr);
	}
	else
	{
		SCE_GNM_ASSERT_MSG(!lsb->m_numInputSemantics ||
			lsb->getInputUsageSlotTable()[0].m_usageType != sce::Gnm::kShaderInputUsageSubPtrFetchShader,
			"LsShader lsb [0x%p] expects a fetch shader, but fetchShaderAddr==0", lsb);
	}

	lsStage->inputUsageTable = lsb->getInputUsageSlotTable();
	lsStage->inputUsageTableSize = lsb->m_common.m_numInputUsageSlots;
#if SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_ASSERT_MSG(lsCache, "Pointer to cached LS inputs cannot be NULL");
	lsStage->activeCache = lsCache;
#else //SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_UNUSED(lsCache);
	SCE_GNM_ASSERT_MSG(!lsCache, "Passing a non-NULL LS cache when shader input caching is disabled has no effect");
#endif //SCE_GNM_CUE2_ENABLE_CACHE


	cue->m_shaderUsesSrt &= ~(1 << sce::Gnm::kShaderStageHs);
	cue->m_shaderUsesSrt |= lsb->m_common.m_shaderIsUsingSrt << sce::Gnm::kShaderStageLs;

	// EUD is automatically dirtied
	cue->m_shaderDirtyEud |= 1 << kShaderStageLs;
	lsStage->eudSizeInDWord = ConstantUpdateEngineHelper::calculateEudSizeInDWord(lsStage->inputUsageTable, lsStage->inputUsageTableSize);
#if SCE_GNMX_TRACK_EMBEDDED_CB
	// Handle embedded constant buffer
	if (lsb->m_common.m_embeddedConstantBufferSizeInDQW)
	{
		// Set up the internal constants:
		sce::Gnm::Buffer lsEmbeddedConstBuffer;
		void *shaderAddr = (void*)(((uintptr_t)lsb->m_lsStageRegisters.m_spiShaderPgmHiLs << 32) + lsb->m_lsStageRegisters.m_spiShaderPgmLoLs);
		lsEmbeddedConstBuffer.initAsConstantBuffer((void*)((uintptr_t(shaderAddr) << 8) + lsb->m_common.m_shaderSize),
			lsb->m_common.m_embeddedConstantBufferSizeInDQW * 16);

		// TODO: Add a special case function for this buffer
		cue->setConstantBuffers(sce::Gnm::kShaderStageLs, sce::Gnm::kShaderSlotEmbeddedConstantBuffer, 1, &lsEmbeddedConstBuffer);
	}
	else
	{
		// TODO: Add a special case function for this buffer
		cue->setConstantBuffers(sce::Gnm::kShaderStageLs, sce::Gnm::kShaderSlotEmbeddedConstantBuffer, 1, NULL);
	}
#endif //SCE_GNMX_TRACK_EMBEDDED_CB

}
void ConstantUpdateEngine::setLsShader(const sce::Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const InputParameterCache *cache)
{
	setLsShaderInternal(this, lsb, &lsb->m_lsStageRegisters, shaderModifier, fetchShaderAddr, cache);
}

#define SCE_GNM_PASS_DISTRIBUTION_MODE(x) ,x

static void setHsShaderInternal(ConstantUpdateEngine *cue, const sce::Gnmx::HsShader *hsb, const sce::Gnm::HsStageRegisters *copyRegs, const sce::Gnm::TessellationRegisters *tessRegs, const ConstantUpdateEngine::InputParameterCache *cache, bool canUpdate)
{
	ConstantUpdateEngine::StageInfo *hsStage = cue->m_stageInfo + sce::Gnm::kShaderStageHs;

#if SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	if (tessRegs)
		cue->m_currentTessRegs = *tessRegs;
#else //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	SCE_GNM_UNUSED(canUpdate);
#endif //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	cue->m_currentHSB = hsb;

	if (!hsb)
	{
		hsStage->shaderBaseAddr256 = 0;
		hsStage->inputUsageTable = 0;
		hsStage->inputUsageTableSize = 0;
		cue->m_dirtyShader &= ~(1 << sce::Gnm::kShaderStageHs);
		return;
	}

	cue->m_dirtyStage |= (1 << sce::Gnm::kShaderStageHs);
	cue->m_dirtyShader |= (1 << sce::Gnm::kShaderStageHs);
	cue->m_shaderContextIsValid |= (1 << sce::Gnm::kShaderStageHs);

	hsStage->shaderBaseAddr256 = hsb->m_hsStageRegisters.m_spiShaderPgmLoHs;
	hsStage->shaderCodeSizeInBytes = hsb->m_common.m_shaderSize;

#if SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	if (canUpdate)
		cue->m_dcb->updateHsShader(copyRegs, tessRegs);
	else
		cue->m_dcb->setHsShader(copyRegs, tessRegs);
#else //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	cue->m_dcb->setHsShader(copyRegs, tessRegs);
#endif //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER


	hsStage->inputUsageTable = hsb->getInputUsageSlotTable();
	hsStage->inputUsageTableSize = hsb->m_common.m_numInputUsageSlots;
#if SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_ASSERT_MSG(cache, "Pointer to cached inputs cannot be NULL");
	hsStage->activeCache = cache;
#else //SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_UNUSED(cache);
	SCE_GNM_ASSERT_MSG(!cache, "Passing a non-NULL cache when shader input caching is disabled has no effect");
#endif //SCE_GNM_CUE2_ENABLE_CACHE


	cue->m_shaderUsesSrt &= ~(1 << kShaderStageHs);
	cue->m_shaderUsesSrt |= hsb->m_common.m_shaderIsUsingSrt << kShaderStageHs;

	// EUD is automatically dirtied
	cue->m_shaderDirtyEud |= (1 << kShaderStageHs);
	hsStage->eudSizeInDWord = ConstantUpdateEngineHelper::calculateEudSizeInDWord(hsStage->inputUsageTable, hsStage->inputUsageTableSize);
#if SCE_GNMX_TRACK_EMBEDDED_CB
	if (hsb->m_common.m_embeddedConstantBufferSizeInDQW)
	{
		// Setup the internal constants
		Gnm::Buffer hsEmbeddedConstBuffer;
		void *shaderAddr = (void*)(((uintptr_t)hsb->m_hsStageRegisters.m_spiShaderPgmHiHs << 32) + hsb->m_hsStageRegisters.m_spiShaderPgmLoHs);
		hsEmbeddedConstBuffer.initAsConstantBuffer((void*)((uintptr_t(shaderAddr) << 8) + hsb->m_common.m_shaderSize),
			hsb->m_common.m_embeddedConstantBufferSizeInDQW * 16);

		// TODO: Add a special case function for this buffer
		cue->setConstantBuffers(kShaderStageHs, kShaderSlotEmbeddedConstantBuffer, 1, &hsEmbeddedConstBuffer);
	}
	else
	{
		// TODO: Add a special case function for this buffer
		cue->setConstantBuffers(kShaderStageHs, kShaderSlotEmbeddedConstantBuffer, 1, NULL);
	}
#endif //SCE_GNMX_TRACK_EMBEDDED_CB
}
static 
#ifdef __ORBIS__
constexpr
#endif
 bool canUpdateHs(const ConstantUpdateEngine* cue, 
	const sce::Gnmx::HsShader *hsb, 
	const sce::Gnm::TessellationRegisters *tessRegs
	,sce::Gnm::TessellationDistributionMode distributionMode
	)
{
	return (cue->m_shaderContextIsValid & (1 << sce::Gnm::kShaderStageHs)) && tessRegs && hsb &&
		cue->m_currentHSB && hsb->m_hsStageRegisters.isSharingContext(cue->m_currentHSB->m_hsStageRegisters) &&
		cue->m_currentTessRegs.m_vgtLsHsConfig == tessRegs->m_vgtLsHsConfig
		&& cue->m_currentDistributionMode == distributionMode
		;
}
// this cannot be a constexpr because of SCE_GNM_UNUSED macros.
static bool isRedundantHs(const ConstantUpdateEngine* cue, 
	const sce::Gnmx::HsShader *hsb, 
	const sce::Gnm::TessellationRegisters *tessRegs
	, sce::Gnm::TessellationDistributionMode distributionMode
	)
{
#if SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	return hsb == cue->m_currentHSB &&
		(!hsb || (tessRegs && tessRegs->m_vgtLsHsConfig == cue->m_currentTessRegs.m_vgtLsHsConfig)) &&
		(cue->m_shaderContextIsValid & (1 << sce::Gnm::kShaderStageHs))
		&& (cue->m_currentDistributionMode == distributionMode)
		;
#else //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	SCE_GNM_UNUSED(cue); 
	SCE_GNM_UNUSED(hsb);
	SCE_GNM_UNUSED(tessRegs);
	SCE_GNM_UNUSED(distributionMode);
	
	return false;
#endif //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
}

void ConstantUpdateEngine::setHsShader(const sce::Gnmx::HsShader *hsb, const sce::Gnm::TessellationRegisters *tessRegs,const InputParameterCache *cache)
{
	if (isRedundantHs(this,hsb,tessRegs SCE_GNM_PASS_DISTRIBUTION_MODE(Gnm::kTessellationDistributionNone)))
	{
		return;
	}
	setHsShaderInternal(this, hsb, &hsb->m_hsStageRegisters, tessRegs, cache, canUpdateHs(this, hsb, tessRegs SCE_GNM_PASS_DISTRIBUTION_MODE(Gnm::kTessellationDistributionNone)));
	m_currentDistributionMode = Gnm::kTessellationDistributionNone;
}
void ConstantUpdateEngine::setHsShader(const sce::Gnmx::HsShader *hsb, const sce::Gnm::TessellationRegisters *tessRegs, sce::Gnm::TessellationDistributionMode distributionMode, const InputParameterCache *cache)
{

	if (isRedundantHs(this, hsb, tessRegs, distributionMode))
	{
		return;
	}
	Gnm::HsStageRegisters copyRegs = hsb->m_hsStageRegisters;
	copyRegs.setTessellationDistributionMode(distributionMode);
	setHsShaderInternal(this, hsb, &copyRegs, tessRegs, cache, canUpdateHs(this, hsb, tessRegs, distributionMode));
	m_currentDistributionMode = distributionMode;
}

void ConstantUpdateEngine::setLsHsShaders(const LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache, const HsShader *hsb, uint32_t numPatches, const ConstantUpdateEngine::InputParameterCache *hsCache)
{
	Gnm::TessellationRegisters tessRegs = { 0 };

	if (hsb)
	{
		tessRegs.init(&hsb->m_hullStateConstants, numPatches);
	}
	if (isRedundantLs(this, lsb, fetchShaderAddr) && isRedundantHs(this, hsb, &tessRegs SCE_GNM_PASS_DISTRIBUTION_MODE(Gnm::kTessellationDistributionNone)))
		return;
	if (!hsb && !lsb)
	{
		ConstantUpdateEngine::StageInfo *hsStage = m_stageInfo + sce::Gnm::kShaderStageHs;
		ConstantUpdateEngine::StageInfo *lsStage = m_stageInfo + sce::Gnm::kShaderStageLs;

		lsStage->shaderBaseAddr256 = 0;
		lsStage->inputUsageTable = 0;
		lsStage->inputUsageTableSize = 0;

		hsStage->shaderBaseAddr256 = 0;
		hsStage->inputUsageTable = 0;
		hsStage->inputUsageTableSize = 0;
		m_dirtyShader &= ~(1 << sce::Gnm::kShaderStageHs) & ~(1 << sce::Gnm::kShaderStageLs);

		m_currentLSB = lsb;
		m_currentHSB = hsb;
		return;
	}

	Gnm::LsStageRegisters patchedLs = lsb->m_lsStageRegisters;
	if (hsb->m_hsStageRegisters.isOffChip())
		patchedLs.updateLdsSizeOffChip(&hsb->m_hullStateConstants, lsb->m_lsStride, numPatches);
	else
		patchedLs.updateLdsSize(&hsb->m_hullStateConstants, lsb->m_lsStride, numPatches);
	setLsShaderInternal(this, lsb, &patchedLs, shaderModifier, fetchShaderAddr, lsCache);
	setHsShaderInternal(this, hsb, &hsb->m_hsStageRegisters, &tessRegs, hsCache, canUpdateHs(this, hsb, &tessRegs SCE_GNM_PASS_DISTRIBUTION_MODE(Gnm::kTessellationDistributionNone)));
	m_currentDistributionMode = Gnm::kTessellationDistributionNone;
}
void ConstantUpdateEngine::setLsHsShaders(const LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache, const HsShader *hsb, uint32_t numPatches, sce::Gnm::TessellationDistributionMode distributionMode, const ConstantUpdateEngine::InputParameterCache *hsCache)
{
	Gnm::TessellationRegisters tessRegs = { 0 };
	
	if (hsb)
	{
		tessRegs.init(&hsb->m_hullStateConstants, numPatches);
	}
	if (isRedundantLs(this, lsb, fetchShaderAddr) && isRedundantHs(this, hsb, &tessRegs, distributionMode))
		return;
	if (!hsb && !lsb)
	{
		ConstantUpdateEngine::StageInfo *hsStage = m_stageInfo + sce::Gnm::kShaderStageHs;
		ConstantUpdateEngine::StageInfo *lsStage = m_stageInfo + sce::Gnm::kShaderStageLs;

		lsStage->shaderBaseAddr256 = 0;
		lsStage->inputUsageTable = 0;
		lsStage->inputUsageTableSize = 0;

		hsStage->shaderBaseAddr256 = 0;
		hsStage->inputUsageTable = 0;
		hsStage->inputUsageTableSize = 0;
		m_dirtyShader &= ~(1 << sce::Gnm::kShaderStageHs) & ~(1 << sce::Gnm::kShaderStageLs);

		m_currentLSB = lsb;
		m_currentHSB = hsb;
		return;
	}

	Gnm::LsStageRegisters patchedLs = lsb->m_lsStageRegisters;
	if (hsb->m_hsStageRegisters.isOffChip())
		patchedLs.updateLdsSizeOffChip(&hsb->m_hullStateConstants, lsb->m_lsStride, numPatches);
	else
		patchedLs.updateLdsSize(&hsb->m_hullStateConstants, lsb->m_lsStride, numPatches);
	Gnm::HsStageRegisters copyRegs = hsb->m_hsStageRegisters;
	copyRegs.setTessellationDistributionMode(distributionMode);
	setLsShaderInternal(this, lsb, &patchedLs, shaderModifier, fetchShaderAddr, lsCache);
	setHsShaderInternal(this, hsb, &copyRegs, &tessRegs, hsCache, canUpdateHs(this, hsb, &tessRegs, distributionMode));

	m_currentDistributionMode = distributionMode;
}

void ConstantUpdateEngine::setOnChipEsShader(const sce::Gnmx::EsShader *esb, uint32_t ldsSizeIn512Bytes, uint32_t shaderModifier, void *fetchShaderAddr, const InputParameterCache *cache)
{
#if SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	if( m_currentESB == esb && m_currentFetchShader[Gnm::kShaderStageEs] == fetchShaderAddr)
	{
		return;
	}
	m_currentFetchShader[Gnm::kShaderStageEs] = fetchShaderAddr;
#endif //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	StageInfo *esStage = m_stageInfo+Gnm::kShaderStageEs;

	m_currentESB  = esb;

	if ( !esb )
	{
		esStage->shaderBaseAddr256   = 0;
		esStage->inputUsageTable	 = 0;
		esStage->inputUsageTableSize = 0;
		m_dirtyShader &= ~(1 << Gnm::kShaderStageEs);
		return;
	}

	m_dirtyStage |= (1 << Gnm::kShaderStageEs);
	m_dirtyShader |= (1 << Gnm::kShaderStageEs);

	esStage->shaderBaseAddr256	   = esb->m_esStageRegisters.m_spiShaderPgmLoEs;
	esStage->shaderCodeSizeInBytes = esb->m_common.m_shaderSize;

	if ( ldsSizeIn512Bytes )
	{
		sce::Gnm::EsStageRegisters esCopy = esb->m_esStageRegisters;
		esCopy.updateLdsSize(ldsSizeIn512Bytes);
		m_dcb->setEsShader(&esCopy, shaderModifier);
	}
	else
	{
		m_dcb->setEsShader(&esb->m_esStageRegisters, shaderModifier);
	}

	if ( fetchShaderAddr )
	{
		m_dcb->setPointerInUserData(Gnm::kShaderStageEs, 0, fetchShaderAddr);
		// TODO: Need a warning, not an assert.
		//SCE_GNM_ASSERT_MSG(esb->m_numInputSemantics &&
		//				 esb->getInputUsageSlotTable()[0].m_usageType == Gnm::kShaderInputUsageSubPtrFetchShader,
		//				 "EsShader esb [0x%p] doesn't expect a fetch shader, but got one passed in fetchShaderAddr==[0x%p]",
		//				 esb, fetchShaderAddr);
	}
	else
	{
		SCE_GNM_ASSERT_MSG(!esb->m_numInputSemantics ||
						 esb->getInputUsageSlotTable()[0].m_usageType != Gnm::kShaderInputUsageSubPtrFetchShader,
						 "EsShader esb [0x%p] expects a fetch shader, but fetchShaderAddr==0", esb);
	}

	esStage->inputUsageTable	 = esb->getInputUsageSlotTable();
	esStage->inputUsageTableSize = esb->m_common.m_numInputUsageSlots;
#if SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_ASSERT_MSG(cache, "Pointer to cached inputs cannot be NULL");
	esStage->activeCache = cache;
#else //SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_UNUSED(cache);
	SCE_GNM_ASSERT_MSG(!cache, "Passing a non-NULL cache when shader input caching is disabled has no effect");
#endif //SCE_GNM_CUE2_ENABLE_CACHE


	m_shaderUsesSrt &= ~(1 << kShaderStageEs);
	m_shaderUsesSrt |= esb->m_common.m_shaderIsUsingSrt << kShaderStageEs;

	// EUD is automatically dirtied
	m_shaderDirtyEud |= (1 << kShaderStageEs);
	esStage->eudSizeInDWord = ConstantUpdateEngineHelper::calculateEudSizeInDWord(esStage->inputUsageTable, esStage->inputUsageTableSize);
#if SCE_GNMX_TRACK_EMBEDDED_CB
	// Handle embedded constant buffer
	if ( esb->m_common.m_embeddedConstantBufferSizeInDQW )
	{
		// Set up the internal constants:
		Gnm::Buffer esEmbeddedConstBuffer;
		void *shaderAddr = (void*)( ((uintptr_t)esb->m_esStageRegisters.m_spiShaderPgmHiEs << 32) + esb->m_esStageRegisters.m_spiShaderPgmLoEs );
		esEmbeddedConstBuffer.initAsConstantBuffer((void*)( (uintptr_t(shaderAddr)<<8) + esb->m_common.m_shaderSize ),
												   esb->m_common.m_embeddedConstantBufferSizeInDQW*16);

		// TODO: Add a special case function for this buffer
		setConstantBuffers(kShaderStageEs, kShaderSlotEmbeddedConstantBuffer, 1, &esEmbeddedConstBuffer);
	}
	else
	{
		// TODO: Add a special case function for this buffer
		setConstantBuffers(kShaderStageEs, kShaderSlotEmbeddedConstantBuffer, 1, NULL);
	}
#endif //SCE_GNMX_TRACK_EMBEDDED_CB
}

void ConstantUpdateEngine::setGsVsShaders(const GsShader *gsb, const InputParameterCache *cache)
{
#if SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	if( m_currentGSB == gsb && (m_shaderContextIsValid & (1 << Gnm::kShaderStageGs)))
	{
		return;
	}
#endif //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	StageInfo *gsStage = m_stageInfo+Gnm::kShaderStageGs;

#if SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	bool canUpdate = (m_shaderContextIsValid & (1 << Gnm::kShaderStageGs)) && m_currentGSB && gsb && gsb->m_gsStageRegisters.isSharingContext(gsb->m_gsStageRegisters) && gsb->getCopyShader()->m_vsStageRegisters.isSharingContext(m_currentGSB->getCopyShader()->m_vsStageRegisters);
#endif //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER

	m_currentGSB = gsb;

	if ( !gsb )
	{
		gsStage->shaderBaseAddr256   = 0;
		gsStage->inputUsageTable	 = 0;
		gsStage->inputUsageTableSize = 0;
		m_dirtyShader &= ~((1 << Gnm::kShaderStageGs) | (1 << Gnm::kShaderStageVs));
		return;
	}

	m_dirtyStage |= (1 << Gnm::kShaderStageGs);
	m_dirtyShader |= (1 << Gnm::kShaderStageGs);
	m_shaderContextIsValid |= (1 << Gnm::kShaderStageGs);

	gsStage->shaderBaseAddr256	   = gsb->m_gsStageRegisters.m_spiShaderPgmLoGs;
	gsStage->shaderCodeSizeInBytes = gsb->m_common.m_shaderSize;

#if SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	if(canUpdate)
		m_dcb->updateGsShader(&gsb->m_gsStageRegisters);
	else
		m_dcb->setGsShader(&gsb->m_gsStageRegisters);
#else //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	m_dcb->setGsShader(&gsb->m_gsStageRegisters);
#endif //SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
	

	gsStage->inputUsageTable	 = gsb->getInputUsageSlotTable();
	gsStage->inputUsageTableSize = gsb->m_common.m_numInputUsageSlots;
#if SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_ASSERT_MSG(cache, "Pointer to cached inputs cannot be NULL");
	gsStage->activeCache = cache;
#else //SCE_GNM_CUE2_ENABLE_CACHE
	SCE_GNM_UNUSED(cache);
	SCE_GNM_ASSERT_MSG(!cache, "Passing a non-NULL cache when shader input caching is disabled has no effect");
#endif //SCE_GNM_CUE2_ENABLE_CACHE

	m_shaderUsesSrt &= ~(1 << kShaderStageGs);
	m_shaderUsesSrt |= gsb->m_common.m_shaderIsUsingSrt << kShaderStageGs;

	// EUD is automatically dirtied
	m_shaderDirtyEud |= (1 << kShaderStageGs);
	gsStage->eudSizeInDWord = ConstantUpdateEngineHelper::calculateEudSizeInDWord(gsStage->inputUsageTable, gsStage->inputUsageTableSize);
#if SCE_GNMX_TRACK_EMBEDDED_CB
	if ( gsb->m_common.m_embeddedConstantBufferSizeInDQW )
	{
		// Setup the internal constants
		Gnm::Buffer gsEmbeddedConstBuffer;
		void *shaderAddr = (void*)( ((uintptr_t)gsb->m_gsStageRegisters.m_spiShaderPgmHiGs << 32) + gsb->m_gsStageRegisters.m_spiShaderPgmLoGs );
		gsEmbeddedConstBuffer.initAsConstantBuffer((void*)( (uintptr_t(shaderAddr)<<8) + gsb->m_common.m_shaderSize ),
												   gsb->m_common.m_embeddedConstantBufferSizeInDQW*16);

		// TODO: Add a special case function for this buffer
		setConstantBuffers(kShaderStageGs, kShaderSlotEmbeddedConstantBuffer, 1, &gsEmbeddedConstBuffer);
	}
	else
	{
		// TODO: Add a special case function for this buffer
		setConstantBuffers(kShaderStageGs, kShaderSlotEmbeddedConstantBuffer, 1, NULL);
	}
#endif //SCE_GNMX_TRACK_EMBEDDED_CB
	setVsShader(gsb->getCopyShader(), 0, (void*)0); // Setting the GS also set the VS copy shader
}

void ConstantUpdateEngine::setCsVsShaders(const sce::Gnmx::CsVsShader *csvsb, uint32_t shaderModifierVs, void *fetchShaderAddrVs, uint32_t shaderModifierCs, void *fetchShaderAddrCs)
{
	if (csvsb == NULL) {
		setAsynchronousComputeShader(NULL, 0, NULL);
		setVsShader(NULL, 0, NULL);
		return;
	}

	m_dirtyStage |= (1 << kShaderStageAsynchronousCompute) | (1 << Gnm::kShaderStageVs);
	m_dirtyShader |= (1 << kShaderStageAsynchronousCompute) | (1 << Gnm::kShaderStageVs);

	const Gnmx::CsShader *csb = csvsb->getComputeShader();
	const Gnmx::VsShader *vsb = csvsb->getVertexShader();
	if ( fetchShaderAddrCs )
	{
		// TODO: Need a warning, not an assert.
		//SCE_GNM_ASSERT_MSG(vsb->m_gsModeOrNumInputSemanticsCs &&
		//				 csb->getInputUsageSlotTable()[0].m_usageType == Gnm::kShaderInputUsageSubPtrFetchShader,
		//				 "CsVsShader::getComputeShader() csb [0x%p] doesn't expect a fetch shader, but got one passed in fetchShaderAddr==[0x%p]",
		//				 csb, fetchShaderAddrCs);
	}
	else
	{
		SCE_GNM_ASSERT_MSG(!vsb->m_gsModeOrNumInputSemanticsCs || csb->getInputUsageSlotTable()[0].m_usageType != Gnm::kShaderInputUsageSubPtrFetchShader,
			"CsVsShader::getComputeShader() csb [0x%p] expects a fetch shader, but fetchShaderAddr==0", csb);
	}

	setAsynchronousComputeShader(csb, shaderModifierCs, fetchShaderAddrCs);
	setVsShader(vsb, shaderModifierVs, fetchShaderAddrVs);
}

void ConstantUpdateEngine::setAsynchronousComputeShader(const sce::Gnmx::CsShader *csb, uint32_t shaderModifier, void *fetchShaderAddr)
{
#if SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	if( m_currentAcbCSB == csb && m_currentFetchShader[kShaderStageAsynchronousCompute] == fetchShaderAddr)
	{
		return;
	}
	m_currentFetchShader[kShaderStageAsynchronousCompute] = fetchShaderAddr;
#endif //SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
	//FIXME: this function is actually dispatch draw specific and should probably be renamed, 
	// because it must assume that the asynchronous compute stage shares all data in memory with the VS stage.

	StageInfo *acsStage = m_stageInfo+kShaderStageAsynchronousCompute;

	if ( !csb )
	{
		m_currentAcbCSB = NULL;
		acsStage->inputUsageTable    = 0;
		acsStage->inputUsageTableSize= 0;
		m_dispatchDrawIndexDeallocNumBits = 0;
		m_asyncCsShaderOrderedAppendMode = 0;
		m_dirtyShader &= ~(1 << kShaderStageAsynchronousCompute);

		return;
	}

	m_dirtyStage |= (1 << kShaderStageAsynchronousCompute);
	m_dirtyShader |= (1 << kShaderStageAsynchronousCompute);

	m_currentAcbCSB = csb;

	acsStage->shaderBaseAddr256	   = csb->m_csStageRegisters.m_computePgmLo;
	acsStage->shaderCodeSizeInBytes = csb->m_common.m_shaderSize;

	m_acb->setCsShader(m_currentAcbCSB ? &csb->m_csStageRegisters : NULL, shaderModifier);
	if ( fetchShaderAddr )
		m_acb->setPointerInUserData(0, fetchShaderAddr);

	acsStage->inputUsageTable	  = csb->getInputUsageSlotTable();
	acsStage->inputUsageTableSize = csb->m_common.m_numInputUsageSlots;

	m_shaderUsesSrt &= ~(1 << kShaderStageAsynchronousCompute);
	m_shaderUsesSrt |= csb->m_common.m_shaderIsUsingSrt << kShaderStageAsynchronousCompute;

	// EUD is automatically dirtied
	m_shaderDirtyEud |= (1 << kShaderStageAsynchronousCompute);
	acsStage->eudSizeInDWord = ConstantUpdateEngineHelper::calculateEudSizeInDWord(acsStage->inputUsageTable, acsStage->inputUsageTableSize);

	m_dispatchDrawIndexDeallocNumBits = csb->m_dispatchDrawIndexDeallocNumBits;
	m_asyncCsShaderOrderedAppendMode = csb->m_orderedAppendMode;

	// Handle embedded constant buffer
	SCE_GNM_ASSERT_MSG( csb->m_common.m_embeddedConstantBufferSizeInDQW == 0, "asynchronous compute shaders do not support embedded constant buffers");
/*
	if ( csb->m_common.m_embeddedConstantBufferSizeInDQW )
	{
		// Set up the internal constants:
		Gnm::Buffer csEmbeddedConstBuffer;
		void *shaderAddr = (void*)( ((uintptr_t)csb->m_csStageRegisters.m_computePgmHi << 32) + csb->m_csStageRegisters.m_computePgmLo );
		csEmbeddedConstBuffer.initAsConstantBuffer((void*)( (uintptr_t(shaderAddr)<<8) + csb->m_common.m_shaderSize ),
			csb->m_common.m_embeddedConstantBufferSizeInDQW*16);

		// TODO: Add a special case function for this buffer
		m_acb->setConstantBuffers(kShaderSlotEmbeddedConstantBuffer, 1, &csEmbeddedConstBuffer);
	}
	else
	{
		// TODO: Add a special case function for this buffer
		m_acb->setConstantBuffers(kShaderSlotEmbeddedConstantBuffer, 1, NULL);
	}
*/
}


//-----------------------------------------------------------------------------


void ConstantUpdateEngine::setVertexAndInstanceOffset(uint32_t vertexOffset, uint32_t instanceOffset)
{
	ShaderStage fetchShaderStage	   = Gnm::kShaderStageCount;		// initialize with an invalid value
	uint32_t	vertexOffsetUserSlot   = 0;
	uint32_t	instanceOffsetUserSlot = 0;

	// find where the fetch shader is
	switch(m_activeShaderStages)
	{
		case Gnm::kActiveShaderStagesVsPs:
			SCE_GNM_ASSERT(m_currentVSB);
			fetchShaderStage	   = Gnm::kShaderStageVs;
			vertexOffsetUserSlot   = m_currentVSB->getVertexOffsetUserRegister();
			instanceOffsetUserSlot = m_currentVSB->getInstanceOffsetUserRegister();
			break;
		case Gnm::kActiveShaderStagesEsGsVsPs:
			SCE_GNM_ASSERT(m_currentESB);
			fetchShaderStage	   = Gnm::kShaderStageEs;
			vertexOffsetUserSlot   = m_currentESB->getVertexOffsetUserRegister();
			instanceOffsetUserSlot = m_currentESB->getInstanceOffsetUserRegister();
			break;
		case Gnm::kActiveShaderStagesLsHsEsGsVsPs:
		case Gnm::kActiveShaderStagesLsHsVsPs:
		case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
		case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
			SCE_GNM_ASSERT(m_currentLSB);
			fetchShaderStage	   = Gnm::kShaderStageLs;
			vertexOffsetUserSlot   = m_currentLSB->getVertexOffsetUserRegister();
			instanceOffsetUserSlot = m_currentLSB->getInstanceOffsetUserRegister();
			break;
		case kActiveShaderStagesDispatchDrawVsPs:
			SCE_GNM_ERROR("Current shader configuration does not support vertex/instance offset");
	}
	if (fetchShaderStage != kShaderStageCount)
	{
		if( vertexOffsetUserSlot )
			m_dcb->setUserData(fetchShaderStage,vertexOffsetUserSlot,vertexOffset);
		if( instanceOffsetUserSlot )
			m_dcb->setUserData(fetchShaderStage,instanceOffsetUserSlot, instanceOffset);
	}
}


bool ConstantUpdateEngine::isVertexOrInstanceOffsetEnabled() const
{
	uint32_t	vertexOffsetUserSlot   = 0;
	uint32_t	instanceOffsetUserSlot = 0;

	if (!m_shaderContextIsValid)
		return false;
	// find where the fetch shader is
	switch(m_activeShaderStages)
	{
	 case Gnm::kActiveShaderStagesVsPs:
		SCE_GNM_ASSERT(m_currentVSB);
		vertexOffsetUserSlot   = m_currentVSB->getVertexOffsetUserRegister();
		instanceOffsetUserSlot = m_currentVSB->getInstanceOffsetUserRegister();
		break;
	 case Gnm::kActiveShaderStagesEsGsVsPs:
		SCE_GNM_ASSERT(m_currentESB);
		vertexOffsetUserSlot   = m_currentESB->getVertexOffsetUserRegister();
		instanceOffsetUserSlot = m_currentESB->getInstanceOffsetUserRegister();
		break;
	 case Gnm::kActiveShaderStagesLsHsEsGsVsPs:
	 case Gnm::kActiveShaderStagesLsHsVsPs:
	 case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
	 case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
		SCE_GNM_ASSERT(m_currentLSB);
		vertexOffsetUserSlot   = m_currentLSB->getVertexOffsetUserRegister();
		instanceOffsetUserSlot = m_currentLSB->getInstanceOffsetUserRegister();
		break;
	 case kActiveShaderStagesDispatchDrawVsPs:
		return false;
	}
	return vertexOffsetUserSlot || instanceOffsetUserSlot;
}

void ConstantUpdateEngine::enableGsModeOnChip(GsMaxOutputPrimitiveDwordSize maxPrimDwordSize, uint32_t esVerticesPerSubGroup, uint32_t gsInputPrimitivesPerSubGroup)
{
	SCE_GNM_ASSERT_MSG((uint32_t)maxPrimDwordSize < 4, "maxPrimDwordSize (0x%02X) is invalid enum value.", maxPrimDwordSize);
	SCE_GNM_ASSERT_MSG(esVerticesPerSubGroup < 2048, "esVerticesPerSubGroup (%u) must be in the range [0..2047].", esVerticesPerSubGroup);
	SCE_GNM_ASSERT_MSG(gsInputPrimitivesPerSubGroup < 2048, "gsInputPrimitivesPerSubGroup (%u) must be in the range [0..2047].", gsInputPrimitivesPerSubGroup);

	const uint32_t signature = (((uint32_t)maxPrimDwordSize) << 22) | (esVerticesPerSubGroup << 11) | (gsInputPrimitivesPerSubGroup);
	if ( signature != m_onChipGsSignature )
	{
		// the on-chip GS signature has changed 
		if ( m_onChipGsSignature != kOnChipGsInvalidSignature )
		{
			// the on-chip GS mode has been set previously so need to turn it off before changing
			m_dcb->disableGsMode();
		}
		m_dcb->enableGsModeOnChip(maxPrimDwordSize, esVerticesPerSubGroup, gsInputPrimitivesPerSubGroup);

		m_onChipGsSignature = signature;
		setOnChipEsVertsPerSubGroup((uint16_t)esVerticesPerSubGroup);
	}
}

