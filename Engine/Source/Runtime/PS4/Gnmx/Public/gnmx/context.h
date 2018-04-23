/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if !defined(_SCE_GNMX_CONTEXT_H)
#define _SCE_GNMX_CONTEXT_H

#include <gnmx/config.h>
#include <gnmx/gfxcontext.h>

namespace sce
{
	namespace Gnmx
	{
		// Remap graphics context and shader binding methods so they can be shared between CUE and LCUE
		#if SCE_GNMX_ENABLE_GFX_LCUE

			typedef LightweightGfxContext	GnmxGfxContext;
			typedef InputResourceOffsets	InputOffsetsCache;
			#define generateInputOffsetsCache(inputTable, shaderType, shader) generateInputResourceOffsetTable(inputTable, shaderType, shader)
			#define generateInputOffsetsCacheForDispatchDraw(inputTableCs, inputTableVs, shader) generateInputResourceOffsetTableForDispatchDraw(inputTableCs, inputTableVs, shader)

			#define setDispatchDrawShader(shader, shaderModifierVS, fetchShaderVS, inputTableVS, shaderModifierCS, fetchShaderCS, inputTableCS)	setCsVsShaders(shader, shaderModifierVS, fetchShaderVS, inputTableVS, shaderModifierCS, fetchShaderCS, inputTableCS)

		#else // SCE_GNMX_ENABLE_GFX_LCUE

			typedef GfxContext									GnmxGfxContext;
			typedef ConstantUpdateEngine::InputParameterCache	InputOffsetsCache;

			#define generateInputOffsetsCache(inputTable, shaderType, shader) ConstantUpdateEngine::initializeInputsCache(inputTable, shader->getInputUsageSlotTable(), shader->m_common.m_numInputUsageSlots)
			#define generateInputOffsetsCacheForDispatchDraw(inputTableCs, inputTableVs, shader) SCE_GNM_UNUSED(inputTableCs); SCE_GNM_UNUSED(inputTableVs);  // don't do anything for now

			#define setDispatchDrawShader(shader, shaderModifierVS, fetchShaderVS, inputTableVS, shaderModifierCS, fetchShaderCS, inputTableCS)	setCsVsShaders(shader, shaderModifierVS, fetchShaderVS, shaderModifierCS, fetchShaderCS)

		#endif // SCE_GNMX_ENABLE_GFX_LCUE
	}
}
#endif // _SCE_GNMX_CONTEXT_H
