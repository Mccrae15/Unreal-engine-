/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if !defined(_SCE_GNMX_LWGFX_CUE_TO_LCUE_H)
#define _SCE_GNMX_LWGFX_CUE_TO_LCUE_H

#if defined(__ORBIS__)	// LCUE isn't supported offline
/*
 * Constant-Update-Engine (CUE) to Lightweight-Constant-Update-Engine (LCUE) helper.
 *
 * Provides a new Gnmx::GfxContext that maps all of its methods to the Gnmx::LightweightGfxContext. This should serve as
 * a quick first step to integrate and test your code with the LCUE.
 *
 * IMPORTANT NOTES / MUST-READ:
 * -------------------------------------------------------------------------------------------------------------------------
 * This header is NOT intended to be used in a shipping product, as performance will be very low
 * due to the need to generate structures used by the LCUE on-the-fly.
 * 
 * This header redefines Gnmx::GfxContext as Gnmx::GnmxGfxContext, and than declares its own class Gnmx::GfxContext. Thus,
 * depending on where you include this header or how you link your code, you are likely to find issues. To prevent issues, 
 * we recommend you:
 *	(a) Make sure you include <gnmx/lwgfxcontext_cuetolcue.h> as the first line in any source file that uses Gnmx::GfxContext and or includes
 *      gfxcontext.h directly. An easy way to achieve this is to enable SCE_GNMX_ENABLE_GFX_LCUE <gnmx/config.h>. Note that this has the 
 *		drawback of affecting any other code that also includes <gnmx/gnmx.h>.
 *  (b) Make sure you rebuild your solution and all the libraries it uses. It's possible the compiler
 *		 will incorrectly link code with previously built snippets of the original Gnmx::GfxContext.
 */

#define GfxContext IgnoreContext

#include <gnmx.h>
// This is in case we are including it from inside the gnmx.h itself
#include <gnmx/computecontext.h>
#include <gnmx/constantupdateengine.h>
#include <gnmx/fetchshaderhelper.h>
#include <gnmx/gfxcontext.h>
#include <gnmx/helpers.h>
#include <gnmx/shaderbinary.h>
#include <gnmx/computequeue.h>

#undef GfxContext

#include <gnmx/lwgfxcontext.h>
namespace sce
{
	namespace Gnmx
	{
	
#if !defined(DOXYGEN_IGNORE)

		// Remap CUE based GfxContext to the LCUE based LightweightGfxContext
		class SCE_GNM_API_DEPRECATED_CLASS_MSG("Remapped GfxContext from lwgfxcontext_cuetolcue.h is no longer supported, please call and initalize the LightweightGfxContext directly in your own code") SCE_GNMX_EXPORT GfxContext : public LightweightGfxContext
		{
		public:
			void init(void *cueCpRamShadowBuffer, void *cueHeapAddr, uint32_t numRingEntries,
				      void *dcbBuffer, uint32_t dcbSizeInBytes, void *ccbBuffer, uint32_t ccbSizeInBytes)
			{
				SCE_GNM_UNUSED(cueCpRamShadowBuffer);
				SCE_GNM_UNUSED(ccbBuffer);
				SCE_GNM_UNUSED(ccbSizeInBytes);

				uint32_t heapSizeInBytes = Gnmx::ConstantUpdateEngine::computeHeapSize(numRingEntries);
				LightweightGfxContext::init((uint32_t*)dcbBuffer, dcbSizeInBytes/4, (uint32_t*)cueHeapAddr, heapSizeInBytes/4, NULL);
			}

			SCE_GNM_FORCE_INLINE void init(void *cueHeapAddr, uint32_t numRingEntries,
										   void *dcbBuffer, uint32_t dcbSizeInBytes, void *ccbBuffer, uint32_t ccbSizeInBytes)
			{
				SCE_GNM_UNUSED(ccbBuffer);
				SCE_GNM_UNUSED(ccbSizeInBytes);

				// using the cueHeap address allocated from garlic memory, as our resource buffer memory. The LCUE will directly write to the resource buffer memory
				uint32_t heapSizeInBytes = Gnmx::ConstantUpdateEngine::computeHeapSize(numRingEntries);
				LightweightGfxContext::init(dcbBuffer, dcbSizeInBytes, cueHeapAddr, heapSizeInBytes, NULL);
			}

			SCE_GNM_FORCE_INLINE void setResourceBufferFullCallback(AllocResourceBufferCallback callbackFunction, void* userData)
			{
				LightweightGfxContext::setResourceBufferFullCallback(callbackFunction, userData);
			}

			SCE_GNM_FORCE_INLINE void setVsShader(const Gnmx::VsShader *vsb, uint32_t shaderModifier, void *fetchShaderAddr)
			{
				sce::Gnmx::generateInputResourceOffsetTable(&m_boundShaderResourceOffsets[Gnm::kShaderStageVs], Gnm::kShaderStageVs, vsb);
				LightweightGfxContext::setVsShader(vsb, shaderModifier, fetchShaderAddr, &m_boundShaderResourceOffsets[Gnm::kShaderStageVs]);
			}


			SCE_GNM_FORCE_INLINE void setPsShader(const Gnmx::PsShader *psb)
			{
				if (psb != NULL)
				{
					sce::Gnmx::generateInputResourceOffsetTable(&m_boundShaderResourceOffsets[Gnm::kShaderStagePs], Gnm::kShaderStagePs, psb);
					LightweightGfxContext::setPsShader(psb, &m_boundShaderResourceOffsets[Gnm::kShaderStagePs]);
				}
				else
				{
					memset(&m_boundShaderResourceOffsets[Gnm::kShaderStagePs], 0, sizeof(InputResourceOffsets));
					LightweightGfxContext::setPsShader(psb, NULL);
				}
			}

			SCE_GNM_FORCE_INLINE void setCsShader(const Gnmx::CsShader *csb)
			{
				sce::Gnmx::generateInputResourceOffsetTable(&m_boundShaderResourceOffsets[Gnm::kShaderStageCs], Gnm::kShaderStageCs, csb);
				LightweightGfxContext::setCsShader(csb, &m_boundShaderResourceOffsets[Gnm::kShaderStageCs]);
			}

			SCE_GNM_FORCE_INLINE void setCsVsShaders(const Gnmx::CsVsShader *csvsb, uint32_t shaderModifierVs, void *fetchShaderAddrVs, uint32_t shaderModifierCs, void *fetchShaderAddrCs)
			{
				sce::Gnmx::generateInputResourceOffsetTableForDispatchDraw(&m_boundShaderResourceOffsets[kShaderStageACs], 
																		   &m_boundShaderResourceOffsets[Gnm::kShaderStageVs], csvsb);
				LightweightGfxContext::setCsVsShaders(csvsb, shaderModifierVs, fetchShaderAddrVs, &m_boundShaderResourceOffsets[Gnm::kShaderStageVs],
				 											 shaderModifierCs, fetchShaderAddrCs, &m_boundShaderResourceOffsets[kShaderStageACs]);
			}

			SCE_GNM_FORCE_INLINE void setEsShader(const Gnmx::EsShader *esb, uint32_t shaderModifier, void *fetchShaderAddr)
			{
				sce::Gnmx::generateInputResourceOffsetTable(&m_boundShaderResourceOffsets[Gnm::kShaderStageEs], Gnm::kShaderStageEs, esb);
				LightweightGfxContext::setEsShader(esb, shaderModifier, fetchShaderAddr, &m_boundShaderResourceOffsets[Gnm::kShaderStageEs]);
			}

			SCE_GNM_FORCE_INLINE void setOnChipEsShader(const Gnmx::EsShader *esb, uint32_t ldsSizeIn512Bytes, uint32_t shaderModifier, const void *fetchShaderAddr)
			{
				sce::Gnmx::generateInputResourceOffsetTable(&m_boundShaderResourceOffsets[Gnm::kShaderStageEs], Gnm::kShaderStageEs, esb);
				LightweightGfxContext::setOnChipEsShader(esb, ldsSizeIn512Bytes, shaderModifier, fetchShaderAddr, &m_boundShaderResourceOffsets[Gnm::kShaderStageEs]);
			}

			SCE_GNM_FORCE_INLINE void setGsVsShaders(const Gnmx::GsShader *gsb)
			{
				sce::Gnmx::generateInputResourceOffsetTable(&m_boundShaderResourceOffsets[Gnm::kShaderStageGs], Gnm::kShaderStageGs, gsb);
				LightweightGfxContext::setGsVsShaders(gsb, &m_boundShaderResourceOffsets[Gnm::kShaderStageGs]);
			}

			SCE_GNM_FORCE_INLINE void setOnChipGsVsShaders(const Gnmx::GsShader *gsb, uint32_t gsPrimsPerSubGroup)
			{
				sce::Gnmx::generateInputResourceOffsetTable(&m_boundShaderResourceOffsets[Gnm::kShaderStageGs], Gnm::kShaderStageGs, gsb);
				LightweightGfxContext::setOnChipGsVsShaders(gsb, gsPrimsPerSubGroup, &m_boundShaderResourceOffsets[Gnm::kShaderStageGs]);
			}

			SCE_GNM_FORCE_INLINE void setLsHsShaders(Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const Gnmx::HsShader *hsb, uint32_t numPatches)
			{
				sce::Gnmx::generateInputResourceOffsetTable(&m_boundShaderResourceOffsets[Gnm::kShaderStageLs], Gnm::kShaderStageLs, lsb);
				sce::Gnmx::generateInputResourceOffsetTable(&m_boundShaderResourceOffsets[Gnm::kShaderStageHs], Gnm::kShaderStageHs, hsb);
				LightweightGfxContext::setLsHsShaders(lsb, shaderModifier, fetchShaderAddr, &m_boundShaderResourceOffsets[Gnm::kShaderStageLs], hsb, &m_boundShaderResourceOffsets[Gnm::kShaderStageHs], numPatches);
			}

		private:

			static const uint32_t kShaderStageACs = sce::Gnm::kShaderStageCount;
			InputResourceOffsets m_boundShaderResourceOffsets[sce::Gnm::kShaderStageCount+1];
		};

	#endif	// !defined(DOXYGEN_IGNORE)
	}
}

#endif  // defined(__ORBIS__)

#endif // _SCE_GNMX_LWGFX_CUE_TO_LCUE_H
