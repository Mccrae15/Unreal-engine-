/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 07.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
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

		#else // SCE_GNMX_ENABLE_GFX_LCUE

			typedef GfxContext									GnmxGfxContext;
			typedef ConstantUpdateEngine::InputParameterCache	InputOffsetsCache;

			#define generateInputOffsetsCache(inputTable, shaderType, shader) ConstantUpdateEngine::initializeInputsCache(inputTable, shader->getInputUsageSlotTable(), shader->m_common.m_numInputUsageSlots)

		#endif // SCE_GNMX_ENABLE_GFX_LCUE
	}
}
#endif // _SCE_GNMX_CONTEXT_H
