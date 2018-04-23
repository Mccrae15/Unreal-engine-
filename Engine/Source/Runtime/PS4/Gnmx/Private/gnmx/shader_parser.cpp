/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/


#define SCE_SHADER_BINARY_DLL_EXPORT

#include <shader/binary.h>
#include <gnmx/shader_parser.h>
#include <gnmx/shaderbinary.h>

using namespace sce;



void sce::Gnmx::parseShader(sce::Gnmx::ShaderInfo *shaderInfo, const void* data)
{
	SCE_GNM_ASSERT_MSG(shaderInfo, "shaderInfo must not be NULL.");
	SCE_GNM_ASSERT_MSG(data, "data must not be NULL.");
	const Shader::Binary::Header		*binaryHeader = (const Shader::Binary::Header*)(data);
	const Gnmx::ShaderFileHeader		*header		  = (const Gnmx::ShaderFileHeader*)(binaryHeader + 1);
	const Gnmx::ShaderCommonData		*shaderCommon = (const Gnmx::ShaderCommonData*)(header + 1);
	const uint32_t						*sbAddress	  = (const uint32_t*)(shaderCommon + 1);

#ifdef __ORBIS__ // host-side DLLs can't use SCE_GNM_ASSERT_MSG, since it requires statically linking with Gnm
	SCE_GNM_ASSERT_MSG(header->m_fileHeader == Gnmx::kShaderFileHeaderId, "The shader file header is invalid.");
	SCE_GNM_ASSERT_MSG(header->m_type != Gnmx::kComputeShader || header->m_majorVersion == 7, "The compute shader version doesn't match the runtime version, please rebuild your shader for v7.*");
	SCE_GNM_ASSERT_MSG(header->m_type == Gnmx::kComputeShader || header->m_majorVersion == 7 || header->m_majorVersion == 6, "The graphics shader version doesn't match the runtime version, please rebuild your shader for v6.* or v7.*");

	SCE_GNM_ASSERT_MSG(sbAddress[1] == 0xFFFFFFFF, "The shader has already been patched.");
	SCE_GNM_ASSERT_MSG((sbAddress[0] & 3) == 0, "The shader offset should be 4 bytes aligned.");
#endif // __ORBIS__
	const uint32_t		sbOffsetInDW  = sbAddress[0] >> 2;
	
	shaderInfo->m_shaderStruct	    = (void*)shaderCommon;
	shaderInfo->m_gpuShaderCode	    = (uint32_t*)shaderCommon + sbOffsetInDW;
	shaderInfo->m_gpuShaderCodeSize = shaderCommon->computeShaderCodeSizeInBytes();
}

void sce::Gnmx::parseGsShader(sce::Gnmx::ShaderInfo* gsShaderInfo, sce::Gnmx::ShaderInfo* vsShaderInfo, const void* data)
{
	SCE_GNM_ASSERT_MSG(gsShaderInfo, "gsShaderInfo must not be NULL.");
	SCE_GNM_ASSERT_MSG(vsShaderInfo, "vsShaderInfo must not be NULL.");
	SCE_GNM_ASSERT_MSG(data, "data must not be NULL.");
	parseShader(gsShaderInfo, data);

	const Gnmx::VsShader*		vsbShader		  = gsShaderInfo->m_gsShader->getCopyShader();
	const uint32_t				vsbOffsetInDW     = vsbShader->m_vsStageRegisters.m_spiShaderPgmLoVs >> 2;

	SCE_GNM_ASSERT_MSG((vsbShader->m_vsStageRegisters.m_spiShaderPgmLoVs & 3) == 0, "The shader offset should be 4 bytes aligned.");

	vsShaderInfo->m_vsShader			= (Gnmx::VsShader*)vsbShader;
	vsShaderInfo->m_gpuShaderCode		= (uint32_t*)gsShaderInfo->m_gsShader + vsbOffsetInDW;
	vsShaderInfo->m_gpuShaderCodeSize   = vsbShader->m_common.computeShaderCodeSizeInBytes();
}

void sce::Gnmx::parseCsVsShader(sce::Gnmx::ShaderInfo* csvsShaderInfo, sce::Gnmx::ShaderInfo* csShaderInfo, const void* data)
{
	SCE_GNM_ASSERT_MSG(csvsShaderInfo, "csvsShaderInfo must not be NULL.");
	SCE_GNM_ASSERT_MSG(csShaderInfo, "csShaderInfo must not be NULL.");
	SCE_GNM_ASSERT_MSG(data, "data must not be NULL.");
	parseShader(csvsShaderInfo, data);

	const Gnmx::CsShader*		csbShader		  = csvsShaderInfo->m_csvsShader->getComputeShader();
	const uint32_t				csbOffsetInDW     = csbShader->m_csStageRegisters.m_computePgmLo >> 2;

	SCE_GNM_ASSERT_MSG((csbShader->m_csStageRegisters.m_computePgmLo & 3) == 0, "The shader offset should be 4 bytes aligned.");

	csShaderInfo->m_csShader			= (Gnmx::CsShader*)csbShader;
	csShaderInfo->m_gpuShaderCode		= (uint32_t*)csvsShaderInfo->m_csvsShader + csbOffsetInDW;
	csShaderInfo->m_gpuShaderCodeSize   = csbShader->m_common.computeShaderCodeSizeInBytes();
}
