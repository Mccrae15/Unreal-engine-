/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx/fetchshaderhelper.h>

using namespace sce::Gnm;
using namespace sce::Gnmx;

uint32_t sce::Gnmx::computeVsFetchShaderSize(const VsShader *vsb)
{
	SCE_GNM_ASSERT_MSG(vsb != 0, "lsb must not be NULL.");
	Gnm::FetchShaderBuildState fb = {0};

	Gnm::generateVsFetchShaderBuildState(&fb, &vsb->m_vsStageRegisters, vsb->m_numInputSemantics, nullptr, 0, vsb->getVertexOffsetUserRegister(), vsb->getInstanceOffsetUserRegister());

	return fb.m_fetchShaderBufferSize;
}

void sce::Gnmx::generateVsFetchShader(void *fs, uint32_t *shaderModifier, const VsShader *vsb)
{
	generateVsFetchShader(fs, shaderModifier, vsb, nullptr, 0, nullptr, 0);
}

void sce::Gnmx::generateVsFetchShader(void *fs, uint32_t *shaderModifier, const VsShader *vsb, const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData)
{
	generateVsFetchShader(fs, shaderModifier, vsb, instancingData, numElementsInInstancingData, nullptr, 0);
}

void sce::Gnmx::generateVsFetchShader(void *fs, uint32_t *shaderModifier, const VsShader *vsb, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	generateVsFetchShader(fs, shaderModifier, vsb, nullptr, 0, semanticRemapTable, numElementsInRemapTable);
}

void sce::Gnmx::generateVsFetchShader(void *fs,
									  uint32_t *shaderModifier, 
									  const VsShader *vsb,
									  const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData,
									  const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	SCE_GNM_ASSERT_MSG(shaderModifier != 0, "shaderModifier must not be NULL.");
	SCE_GNM_ASSERT_MSG(vsb != 0, "vsb must not be NULL.");
	Gnm::FetchShaderBuildState fb = {0};

	Gnm::generateVsFetchShaderBuildState(&fb, &vsb->m_vsStageRegisters, vsb->m_numInputSemantics, instancingData, numElementsInInstancingData, vsb->getVertexOffsetUserRegister(), vsb->getInstanceOffsetUserRegister());

	fb.m_numInputSemantics		 = vsb->m_numInputSemantics;
	fb.m_inputSemantics			 = vsb->getInputSemanticTable();
	fb.m_numInputUsageSlots      = vsb->m_common.m_numInputUsageSlots;
	fb.m_inputUsageSlots		 = vsb->getInputUsageSlotTable();
	fb.m_numElementsInRemapTable = numElementsInRemapTable;
	fb.m_semanticsRemapTable	 = static_cast<const uint32_t*>(semanticRemapTable);
	
	Gnm::generateFetchShader(fs, &fb);
	*shaderModifier = fb.m_shaderModifier;
}

uint32_t sce::Gnmx::computeLsFetchShaderSize(const LsShader *lsb)
{
	SCE_GNM_ASSERT_MSG(lsb != 0, "lsb must not be NULL.");
	Gnm::FetchShaderBuildState fb = {0};

	Gnm::generateLsFetchShaderBuildState(&fb, &lsb->m_lsStageRegisters, lsb->m_numInputSemantics, nullptr, 0, lsb->getVertexOffsetUserRegister(), lsb->getInstanceOffsetUserRegister());

	return fb.m_fetchShaderBufferSize;
}

void sce::Gnmx::generateLsFetchShader(void *fs, uint32_t *shaderModifier, const LsShader *lsb)
{
	generateLsFetchShader(fs, shaderModifier, lsb, nullptr, 0, nullptr, 0);
}

void sce::Gnmx::generateLsFetchShader(void *fs, uint32_t *shaderModifier, const LsShader *lsb, const Gnm::FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData)
{
	generateLsFetchShader(fs, shaderModifier, lsb, instancingData, numElementsInInstancingData, nullptr, 0);
}

void sce::Gnmx::generateLsFetchShader(void *fs,
									  uint32_t *shaderModifier,
									  const LsShader *lsb,
									  const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	generateLsFetchShader(fs, shaderModifier, lsb, nullptr, 0, semanticRemapTable, numElementsInRemapTable);
}

void sce::Gnmx::generateLsFetchShader(void *fs,
									  uint32_t *shaderModifier,
									  const LsShader *lsb,
									  const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData,
									  const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	SCE_GNM_ASSERT_MSG(shaderModifier != 0, "shaderModifier must not be NULL.");
	SCE_GNM_ASSERT_MSG(lsb != 0, "lsb must not be NULL.");
	Gnm::FetchShaderBuildState fb = {0};

	Gnm::generateLsFetchShaderBuildState(&fb, &lsb->m_lsStageRegisters, lsb->m_numInputSemantics, instancingData, numElementsInInstancingData, lsb->getVertexOffsetUserRegister(), lsb->getInstanceOffsetUserRegister());

	fb.m_numInputSemantics		 = lsb->m_numInputSemantics;
	fb.m_inputSemantics			 = lsb->getInputSemanticTable();
	fb.m_numInputUsageSlots      = lsb->m_common.m_numInputUsageSlots;
	fb.m_inputUsageSlots		 = lsb->getInputUsageSlotTable();
	fb.m_numElementsInRemapTable = numElementsInRemapTable;
	fb.m_semanticsRemapTable	 = static_cast<const uint32_t*>(semanticRemapTable);
	
	Gnm::generateFetchShader(fs, &fb);
	*shaderModifier = fb.m_shaderModifier;
}


uint32_t sce::Gnmx::computeEsFetchShaderSize(const EsShader *esb)
{
	SCE_GNM_ASSERT_MSG(esb != 0, "lsb must not be NULL.");
	Gnm::FetchShaderBuildState fb = {0};

	Gnm::generateEsFetchShaderBuildState(&fb, &esb->m_esStageRegisters, esb->m_numInputSemantics, nullptr, 0, esb->getVertexOffsetUserRegister(), esb->getInstanceOffsetUserRegister());

	return fb.m_fetchShaderBufferSize;
}

void sce::Gnmx::generateEsFetchShader(void *fs, uint32_t *shaderModifier, const EsShader *esb)
{
	generateEsFetchShader(fs, shaderModifier, esb, nullptr, 0, nullptr, 0);
}

void sce::Gnmx::generateEsFetchShader(void *fs, uint32_t *shaderModifier, const EsShader *esb, const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData)
{
	generateEsFetchShader(fs, shaderModifier, esb, instancingData, numElementsInInstancingData, nullptr, 0);
}

void sce::Gnmx::generateEsFetchShader(void *fs, uint32_t *shaderModifier, const EsShader *esb, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	generateEsFetchShader(fs, shaderModifier, esb, nullptr, 0, semanticRemapTable, numElementsInRemapTable);
}

void sce::Gnmx::generateEsFetchShader(void *fs,
									  uint32_t *shaderModifier,
									  const EsShader *esb,
									  const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData,
									  const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	SCE_GNM_ASSERT_MSG(shaderModifier != 0, "shaderModifier must not be NULL.");
	SCE_GNM_ASSERT_MSG(esb != 0, "esb must not be NULL.");
	Gnm::FetchShaderBuildState fb = {0};

	Gnm::generateEsFetchShaderBuildState(&fb, &esb->m_esStageRegisters, esb->m_numInputSemantics, instancingData, numElementsInInstancingData, esb->getVertexOffsetUserRegister(), esb->getInstanceOffsetUserRegister());

	fb.m_numInputSemantics		 = esb->m_numInputSemantics;
	fb.m_inputSemantics			 = esb->getInputSemanticTable();
	fb.m_numInputUsageSlots      = esb->m_common.m_numInputUsageSlots;
	fb.m_inputUsageSlots		 = esb->getInputUsageSlotTable();
	fb.m_numElementsInRemapTable = numElementsInRemapTable;
	fb.m_semanticsRemapTable	 = static_cast<const uint32_t*>(semanticRemapTable);

	Gnm::generateFetchShader(fs, &fb);
	*shaderModifier = fb.m_shaderModifier;
}

uint32_t sce::Gnmx::computeVsFetchShaderSize(const CsVsShader *csvsb)
{
	SCE_GNM_ASSERT_MSG(csvsb != 0, "lsb must not be NULL.");
	Gnm::FetchShaderBuildState fb = {0};
	Gnm::generateVsFetchShaderBuildState(&fb, &csvsb->getVertexShader()->m_vsStageRegisters, csvsb->getVertexShader()->m_numInputSemantics, nullptr, 0, 0, 0);

	return fb.m_fetchShaderBufferSize;
}

void sce::Gnmx::generateVsFetchShader(void *fs, uint32_t *shaderModifier, const CsVsShader *csvsb)
{
	generateVsFetchShader(fs, shaderModifier, csvsb, nullptr, 0, nullptr, 0);
}

void sce::Gnmx::generateVsFetchShader(void *fs, uint32_t *shaderModifier, const CsVsShader *csvsb, const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData)
{
	generateVsFetchShader(fs, shaderModifier, csvsb, instancingData, numElementsInInstancingData, nullptr, 0);
}

void sce::Gnmx::generateVsFetchShader(void *fs, uint32_t *shaderModifier, const CsVsShader *csvsb, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	generateVsFetchShader(fs, shaderModifier, csvsb, nullptr, 0, semanticRemapTable, numElementsInRemapTable);
}

void sce::Gnmx::generateVsFetchShader(void *fs,
	uint32_t *shaderModifier, 
	const CsVsShader *csvsb,
	const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData,
	const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	SCE_GNM_ASSERT_MSG(shaderModifier != 0, "shaderModifier must not be NULL.");
	SCE_GNM_ASSERT_MSG(csvsb != 0, "csvsb must not be NULL.");
	Gnm::FetchShaderBuildState fb = {0};
	const uint8_t numInputSemantics = csvsb->getVertexShader()->m_numInputSemantics;
	Gnm::VertexInputSemantic const* pInputSemantics = csvsb->getVertexShader()->getInputSemanticTable();

	Gnm::generateVsFetchShaderBuildState(&fb, &csvsb->getVertexShader()->m_vsStageRegisters, numInputSemantics, instancingData, numElementsInInstancingData, 0, 0);

	fb.m_numInputSemantics		 = numInputSemantics;
	fb.m_inputSemantics			 = pInputSemantics;
	fb.m_numInputUsageSlots      = csvsb->getVertexShader()->m_common.m_numInputUsageSlots;
	fb.m_inputUsageSlots		 = csvsb->getVertexShader()->getInputUsageSlotTable();
	fb.m_numElementsInRemapTable = numElementsInRemapTable;
	fb.m_semanticsRemapTable	 = static_cast<const uint32_t*>(semanticRemapTable);

	Gnm::generateFetchShader(fs, &fb);
	*shaderModifier = fb.m_shaderModifier;
}

uint32_t sce::Gnmx::computeCsFetchShaderSize(const CsVsShader *csvsb)
{
	SCE_GNM_ASSERT_MSG(csvsb != 0, "csvsb must not be NULL.");
	Gnm::FetchShaderBuildState fb = {0};
	Gnmx::CsShader const* csb = csvsb->getComputeShader();
	if (csb->m_version >= Gnmx::kDdCsShaderVersion_IndependentCsFetchShader)
		Gnm::generateCsFetchShaderBuildState(&fb, &csb->m_csStageRegisters, csb->m_numInputSemantics, nullptr, 0, 0, 0);
	else
		Gnm::generateCsFetchShaderBuildState(&fb, &csb->m_csStageRegisters, csvsb->getVertexShader()->m_gsModeOrNumInputSemanticsCs, nullptr, 0, 0, 0);

	return fb.m_fetchShaderBufferSize;
}

void sce::Gnmx::generateCsFetchShader(void *fs, uint32_t *shaderModifier, const CsVsShader *csvsb)
{
	generateCsFetchShader(fs, shaderModifier, csvsb, nullptr, 0, nullptr, 0);
}

void sce::Gnmx::generateCsFetchShader(void *fs, uint32_t *shaderModifier, const CsVsShader *csvsb, const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData)
{
	generateCsFetchShader(fs, shaderModifier, csvsb, instancingData, numElementsInInstancingData, nullptr, 0);
}

void sce::Gnmx::generateCsFetchShader(void *fs, uint32_t *shaderModifier, const CsVsShader *csvsb, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	generateCsFetchShader(fs, shaderModifier, csvsb, nullptr, 0, semanticRemapTable, numElementsInRemapTable);
}

void sce::Gnmx::generateCsFetchShader(void *fs,
	uint32_t *shaderModifier, 
	const CsVsShader *csvsb,
	const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData,
	const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	SCE_GNM_ASSERT_MSG(shaderModifier != 0, "shaderModifier must not be NULL.");
	SCE_GNM_ASSERT_MSG(csvsb != 0, "csvsb must not be NULL.");
	uint8_t numInputSemantics;
	Gnm::VertexInputSemantic const *pInputSemantics;
	Gnmx::CsShader const* csb = csvsb->getComputeShader();
	if (csb->m_version >= Gnmx::kDdCsShaderVersion_IndependentCsFetchShader) {
		numInputSemantics	= csb->m_numInputSemantics;
		pInputSemantics		= csb->getInputSemanticTable();
	} else {
		numInputSemantics	= csvsb->getVertexShader()->m_gsModeOrNumInputSemanticsCs;
		pInputSemantics		= csvsb->getVertexShader()->getInputSemanticTable();
	}

	Gnm::FetchShaderBuildState fb = {0};
	Gnm::generateCsFetchShaderBuildState(&fb, &csb->m_csStageRegisters, numInputSemantics, instancingData, numElementsInInstancingData, 0, 0);

	fb.m_numInputSemantics	= numInputSemantics;
	fb.m_inputSemantics		= pInputSemantics;
	fb.m_numInputUsageSlots = csb->m_common.m_numInputUsageSlots;
	fb.m_inputUsageSlots	= csb->getInputUsageSlotTable();
	fb.m_numElementsInRemapTable = numElementsInRemapTable;
	fb.m_semanticsRemapTable	 = static_cast<const uint32_t*>(semanticRemapTable);

	Gnm::generateFetchShader(fs, &fb);
	*shaderModifier = fb.m_shaderModifier;
}

uint32_t sce::Gnmx::computeCsFetchShaderSize(const CsShader *csb)
{
	SCE_GNM_ASSERT_MSG(csb != 0, "csvsb must not be NULL.");
	SCE_GNM_ASSERT_MSG(csb->m_version >= Gnmx::kDdCsShaderVersion_IndependentCsFetchShader, "csb must have m_version >= %u.", (uint32_t)Gnmx::kDdCsShaderVersion_IndependentCsFetchShader);
	Gnm::FetchShaderBuildState fb = {0};
	Gnm::generateCsFetchShaderBuildState(&fb, &csb->m_csStageRegisters, csb->m_numInputSemantics, nullptr, 0, 0, 0);

	return fb.m_fetchShaderBufferSize;
}

void sce::Gnmx::generateCsFetchShader(void *fs, uint32_t *shaderModifier, const CsShader *csb)
{
	generateCsFetchShader(fs, shaderModifier, csb, nullptr, 0, nullptr, 0);
}

void sce::Gnmx::generateCsFetchShader(void *fs, uint32_t *shaderModifier, const CsShader *csb, const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData)
{
	generateCsFetchShader(fs, shaderModifier, csb, instancingData, numElementsInInstancingData, nullptr, 0);
}

void sce::Gnmx::generateCsFetchShader(void *fs, uint32_t *shaderModifier, const CsShader *csb, const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	generateCsFetchShader(fs, shaderModifier, csb, nullptr, 0, semanticRemapTable, numElementsInRemapTable);
}

void sce::Gnmx::generateCsFetchShader(void *fs,
	uint32_t *shaderModifier, 
	const CsShader *csb,
	const FetchShaderInstancingMode *instancingData, const uint32_t numElementsInInstancingData,
	const void *semanticRemapTable, const uint32_t numElementsInRemapTable)
{
	SCE_GNM_ASSERT_MSG(shaderModifier != 0, "shaderModifier must not be NULL.");
	SCE_GNM_ASSERT_MSG(csb != 0, "csvsb must not be NULL.");
	SCE_GNM_ASSERT_MSG(csb->m_version >= Gnmx::kDdCsShaderVersion_IndependentCsFetchShader, "csb must have m_version >= %u.", (uint32_t)Gnmx::kDdCsShaderVersion_IndependentCsFetchShader);
	Gnm::FetchShaderBuildState fb = {0};
	Gnm::generateCsFetchShaderBuildState(&fb, &csb->m_csStageRegisters, csb->m_numInputSemantics, instancingData, numElementsInInstancingData, 0, 0);

	fb.m_numInputSemantics	= csb->m_numInputSemantics;
	fb.m_inputSemantics		= csb->getInputSemanticTable();
	fb.m_numInputUsageSlots = csb->m_common.m_numInputUsageSlots;
	fb.m_inputUsageSlots	= csb->getInputUsageSlotTable();
	fb.m_numElementsInRemapTable = numElementsInRemapTable;
	fb.m_semanticsRemapTable	 = static_cast<const uint32_t*>(semanticRemapTable);

	Gnm::generateFetchShader(fs, &fb);
	*shaderModifier = fb.m_shaderModifier;
}
