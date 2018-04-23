/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if defined(__ORBIS__) // Surface tools isn't supported offline

#include <gnmx.h>
#include <gnmx/lwgfxcontext.h>
#include <gnmx/surfacetool.h>

#include <gnm/rendertarget.h>
#include <gnm/depthrendertarget.h>
#include <gnm/texture.h>

#include <algorithm>
#include <string.h>

using namespace sce;

static bool isTileModeLinear(Gnm::TileMode tileMode)
{
	return (tileMode == Gnm::kTileModeDisplay_LinearAligned);
}

static bool tileModesAreSafeForMsaaResolve(Gnm::TileMode destMode, Gnm::TileMode srcMode)
{
	Gnm::MicroTileMode mtmSrc = Gnm::kMicroTileModeRotated, mtmDest = Gnm::kMicroTileModeRotated;
	if ( GpuAddress::kStatusSuccess != GpuAddress::getMicroTileMode(&mtmSrc,  srcMode) )
		return false;
	if ( GpuAddress::kStatusSuccess != GpuAddress::getMicroTileMode(&mtmDest, destMode) )
		return false;
	return mtmSrc == mtmDest && isTileModeLinear(destMode) == isTileModeLinear(srcMode);
}

enum DecompressMethod
{
	kDecompressInPlace,
	kDecompressToCopy,
	kCopyCompressed,
};
static void copyFromCompressedDepthSurface(Gnmx::GnmxDrawCommandBuffer *dcb, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source, DecompressMethod method)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	SCE_GNM_ASSERT_MSG(destination, "destination must not be NULL.");
	SCE_GNM_ASSERT_MSG(source, "source must not be NULL.");

	dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta); 


	dcb->setDepthStencilDisable();
	dcb->setCbControl(Gnm::kCbModeDisable, Gnm::kRasterOpCopy);
	{	
		Gnm::DepthRenderTarget depthRenderTarget = *source;	
		depthRenderTarget.setZWriteAddress(destination->getZReadAddress());
		depthRenderTarget.setStencilWriteAddress(destination->getStencilReadAddress());
		dcb->setDepthRenderTarget(&depthRenderTarget);
		Gnmx::setupScreenViewport(dcb, 0, 0, depthRenderTarget.getWidth(), depthRenderTarget.getHeight(), 0.5f, 0.5f);
	}
	{
		Gnm::DbRenderControl dbRenderControl;
		dbRenderControl.init();
		const Gnm::DbTileWriteBackPolicy dbTileWriteBackPolicy = (method == kCopyCompressed) ? Gnm::kDbTileWriteBackPolicyCompressionAllowed : Gnm::kDbTileWriteBackPolicyCompressionForbidden;
		dbRenderControl.setStencilTileWriteBackPolicy(dbTileWriteBackPolicy);
		dbRenderControl.setDepthTileWriteBackPolicy(dbTileWriteBackPolicy);
		dcb->setDbRenderControl(dbRenderControl);
	}
	Gnm::RenderOverrideControl renderOverrideControl;
	if(method != kDecompressInPlace)
	{
		renderOverrideControl.init();
		renderOverrideControl.setForceZValid(true);       // makes it read all Z tiles
		renderOverrideControl.setForceStencilValid(true); // makes it read all STENCIL tiles
		renderOverrideControl.setForceZDirty(true);       // makes it write all Z tiles
		renderOverrideControl.setForceStencilDirty(true); // makes it write all STENCIL tiles
		switch(method)
		{
		case kDecompressToCopy:			
			renderOverrideControl.setPreserveCompression(true); // preserves the HTILE buffer for later use
			break;
		case kCopyCompressed:
			renderOverrideControl.setPreserveCompression(false); // allows the HTILE buffer to be re-summarized
			break;
		default:
			SCE_GNM_ERROR("Compressed depth buffer copy method %d is not handled here.", method);
			break;
		}
		dcb->setRenderOverrideControl(renderOverrideControl);
	}

	dcb->setPsShader(nullptr);
	Gnmx::renderFullScreenQuad(dcb);

	{
		volatile uint32_t *label = (uint32_t*)dcb->allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
		uint32_t zero = 0;
		dcb->writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
		dcb->writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, (void*)label,
			Gnm::kEventWriteSource32BitsImmediate, 1,
			Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
		dcb->waitOnAddress((void*)label, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);
	}

	dcb->setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);

	if(method != kDecompressInPlace)
	{
		renderOverrideControl.init();
		dcb->setRenderOverrideControl(renderOverrideControl);
	}
}

void sce::Gnmx::renderFullScreenQuad(GfxContext *gfxc)
{
	SCE_GNM_ASSERT_MSG(gfxc, "gfxc must not be NULL.");
	// invalidate the VS shader binding since the renderFullScreenQuad() is going to use its own shader without updating the CUE
	gfxc->setVsShader(NULL, 0, NULL);
	gfxc->m_cue.preDraw();
	renderFullScreenQuad(&gfxc->m_dcb);
	gfxc->m_cue.postDraw();
	gfxc->m_cue.invalidateAllShaderContexts();
}
void sce::Gnmx::renderFullScreenQuad(LightweightGfxContext *gfxc)
{
	SCE_GNM_ASSERT_MSG(gfxc, "gfxc must not be NULL.");
	gfxc->m_lwcue.preDraw();
	renderFullScreenQuad(&gfxc->m_dcb);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStageVs);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStagePs);
}
void sce::Gnmx::renderFullScreenQuad(GnmxDrawCommandBuffer *dcb)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	dcb->setEmbeddedVsShader(Gnm::kEmbeddedVsShaderFullScreen, 0);
	dcb->setPrimitiveType(Gnm::kPrimitiveTypeRectList);
	dcb->setIndexSize(Gnm::kIndexSize16, Gnm::kCachePolicyLru);
	dcb->drawIndexAuto(3, Gnm::kDrawModifierDefault);
}

void sce::Gnmx::hardwareMsaaResolve(GfxContext *gfxc, const Gnm::RenderTarget *destTarget, const Gnm::RenderTarget *srcTarget)
{
	SCE_GNM_ASSERT_MSG(gfxc, "gfxc must not be NULL.");
	hardwareMsaaResolve(&gfxc->m_dcb, destTarget, srcTarget);
	gfxc->m_cue.invalidateAllShaderContexts();
}
void sce::Gnmx::hardwareMsaaResolve(LightweightGfxContext *gfxc, const Gnm::RenderTarget *destTarget, const Gnm::RenderTarget *srcTarget)
{
	SCE_GNM_ASSERT_MSG(gfxc, "gfxc must not be NULL.");
	hardwareMsaaResolve(&gfxc->m_dcb, destTarget, srcTarget);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStageVs);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStagePs);
}
void sce::Gnmx::hardwareMsaaResolve(GnmxDrawCommandBuffer *dcb, const Gnm::RenderTarget *destTarget, const Gnm::RenderTarget *srcTarget)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	SCE_GNM_ASSERT_MSG(destTarget, "destTarget must not be NULL.");
	SCE_GNM_ASSERT_MSG(srcTarget, "srcTarget must not be NULL.");
	SCE_GNM_ASSERT_MSG(tileModesAreSafeForMsaaResolve(destTarget->getTileMode(), srcTarget->getTileMode()), "srcTarget and destTarget must both be tiled, or both be linear, and must have matching MicroTileModes (Display, Thin, etc.)");
	SCE_GNM_ASSERT_MSG(!srcTarget->getFmaskCompressionEnable() || (srcTarget->getFmaskAddress() != nullptr), "srcTarget (0x%p) uses FMASK compression, but its FMASK surface is NULL.", srcTarget);
	SCE_GNM_ASSERT_MSG(srcTarget->getDataFormat().m_asInt == destTarget->getDataFormat().m_asInt, "srcTarget and destTarget must have the same DataFormat");
	SCE_GNM_ASSERT_MSG(destTarget->getNumFragments() == Gnm::kNumFragments1, "destTarget->getNumFragments() must be kNumFragments1");
	SCE_GNM_ASSERT_MSG(!destTarget->getCmaskFastClearEnable(), "destTarget CMASK fast clear must be disabled");

	// 1. Flush and invalidate CB color cache (invalidation is required due to the way the color cache is configured while in resolve mode).
	// 2. Fully enable slot 0 with setRenderTargetMask(0xF).  Output to all other MRTs must be disabled.
	// 3. Bind the multi-sampled source RenderTarget to MRT slot 0. MRT0 must be in a blend-capable format. The source surface MUST have numFragments > kNumFragments1;
	//    it is illegal to resolve a 1-fragment-per-pixel surface.
	// 4. Bind the single-sampled destination RenderTarget to MRT slot 1, but do not enable it in the shader output mask. The following fields must match
	//    between the source and destination targets:
	//    - CB_COLOR1_INFO.ENDIAN
	//    - CB_COLOR1_INFO.FORMAT
	//    - CB_COLOR1_INFO.NUMBER_TYPE
	//    - CB_COLOR1_INFO.COMP_SWAP
	//    - Any field that is deterministic based on the surface format.
	//    The surface tile modes must also be compatible; see tileModesAreSafeForMsaaResolve().
	// 5. Disable blending.
	// 6. Set the CB mode to kCbModeFmaskDecompress, with a rasterOp of kRasterOpCopy.
	// 7. Bind a pixel shader that outputs to MRT slot 0. Do *NOT* use setPsShader(NULL). The shader code will not be run by this pass,
	//    but the shader state must be configured correctly. The recommended approach is to use setEmbeddedPsShader(kEmbeddedPsShaderDummy).
	// 8. Draw over the screen region(s) that should be resolved.  Usually, this will just be a large rectangle the size of the surface.  Do not draw the same pixel twice.
	// 9. Flush the color cache for all the modes.

	SCE_GNM_ASSERT_MSG(sce::Gnm::getGpuMode() != sce::Gnm::kGpuModeNeo || !destTarget->getDccCompressionEnable(), "destTarget DCC compression must be disabled");
	//dcb->pushMarker("Gnmx::hardwareMsaaResolve");

	dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta);
	dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbPixelData);


	dcb->setRenderTarget(0, srcTarget);
	dcb->setRenderTarget(1, destTarget);
	dcb->setDepthRenderTarget((Gnm::DepthRenderTarget*)nullptr);
	dcb->setEmbeddedPsShader(Gnm::kEmbeddedPsShaderDummy);

	Gnm::BlendControl blendControl;
	blendControl.init();
	blendControl.setBlendEnable(false);
	dcb->setBlendControl(0, blendControl);
	dcb->setRenderTargetMask(0x0000000F);

	Gnm::DepthStencilControl dsc;
	dsc.init();
	dsc.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways);
	dsc.setDepthEnable(false);
	dcb->setDepthStencilControl(dsc);
	Gnmx::setupScreenViewport(dcb, 0, 0, destTarget->getWidth(), destTarget->getHeight(), 0.5f, 0.5f);
	dcb->setDepthStencilDisable();

	dcb->setCbControl(Gnm::kCbModeResolve, Gnm::kRasterOpCopy);
	renderFullScreenQuad(dcb);
	dcb->setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);

	dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta);
	// This post-resolve pixel flush is necessary in case the resolved surface will be bound as a texture input.
	dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbPixelData);

	//dcb->popMarker();
}

void sce::Gnmx::eliminateFastClear(GfxContext *gfxc, const Gnm::RenderTarget *target)
{
	SCE_GNM_ASSERT_MSG(gfxc, "gfxc must not be NULL.");
	eliminateFastClear(&gfxc->m_dcb, target);
	gfxc->m_cue.invalidateAllShaderContexts();
}
void sce::Gnmx::eliminateFastClear(LightweightGfxContext *gfxc, const Gnm::RenderTarget *target)
{
	SCE_GNM_ASSERT_MSG(gfxc, "gfxc must not be NULL.");
	eliminateFastClear(&gfxc->m_dcb, target);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStageVs);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStagePs);
}
void sce::Gnmx::eliminateFastClear(GnmxDrawCommandBuffer *dcb, const Gnm::RenderTarget *target)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	SCE_GNM_ASSERT_MSG(target, "target must not be NULL.");
	if (!target->getCmaskFastClearEnable())
		return; // Nothing to do.

	SCE_GNM_ASSERT_MSG(target->getCmaskAddress() != nullptr, "Can not eliminate fast clear from a target with no CMASK.");
	//dcb->pushMarker("Gnmx::eliminateFastClear");

	// Required sequence of events for fast clear elimination:
	// 1. Fully enable slot 0 with setRenderTargetMask(0xF).  Output to all other MRTs must be disabled.
	// 2. Bind the render target whose CMASK should be eliminated to slot 0.
	// 3. Disable blending.
	// 4. Set the CB mode to kCbModeEliminateFastClear, with a rasterOp of kRasterOpCopy.
	// 5. Bind a pixel shader that outputs to MRT slot 0. Do *NOT* use setPsShader(NULL). The shader code will not be run by this pass,
	//    but the shader state must be configured correctly. The recommended approach is to use setEmbeddedPsShader(kEmbeddedPsShaderDummy).
	// 6. Draw over the screen region(s) that should be modified.  Usually, this will just be a large rectangle the size of the surface.  Do not draw the same pixel twice.
	// 7. Flush the color cache for all the modes.

	// CB surface data/metadata must be fully flushed/invalidated before the surface operation begins.

	volatile uint32_t *cbLabel = (uint32_t*)dcb->allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
	*cbLabel = 0;

	// Needs to flush the CMask data
	dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta);


	dcb->setRenderTarget(0, target);
	//dcb.setDepthRenderTarget((Gnm::DepthRenderTarget *)NULL);

	// Validation
	SCE_GNM_ASSERT_MSG(target->getCmaskAddress() != nullptr, "Must have a CMASK surface to eliminate fast clear data");

	Gnm::BlendControl blendControl;
	blendControl.init();
	blendControl.setBlendEnable(false);
	dcb->setBlendControl(0, blendControl);
	dcb->setRenderTargetMask(0x0000000F); // enable MRT0 output only
	dcb->setCbControl(Gnm::kCbModeEliminateFastClear, Gnm::kRasterOpCopy);
	Gnm::DepthStencilControl dsc;
	dsc.init();
	dsc.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways);
	dsc.setDepthEnable(true);
	dcb->setDepthStencilControl(dsc);
	Gnmx::setupScreenViewport(dcb, 0, 0, target->getWidth(), target->getHeight(), 0.5f, 0.5f);

	// draw full screen quad.
	dcb->setPsShader(nullptr);
	renderFullScreenQuad(dcb);

	// Wait for the draw to be finished, and flush the Cb/Db caches:
	dcb->writeAtEndOfPipe(Gnm::kEopFlushCbDbCaches, Gnm::kEventWriteDestMemory, (void*)cbLabel,
		Gnm::kEventWriteSource32BitsImmediate, 1,
		Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
	dcb->waitOnAddress((void*)cbLabel, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);
	//gfxc.triggerEvent(Gnm::kEventCacheFlush); // FLUSH_AND_INV_CB_PIXEL_DATA

	// Restore CB mode to normal
	dcb->setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);

	//dcb->popMarker();
}

void sce::Gnmx::decompressFmaskSurface(GfxContext *gfxc, const Gnm::RenderTarget *target)
{
	SCE_GNM_ASSERT_MSG(gfxc, "gfxc must not be NULL.");
	decompressFmaskSurface(&gfxc->m_dcb, target);
	gfxc->m_cue.invalidateAllShaderContexts();
}
void sce::Gnmx::decompressFmaskSurface(LightweightGfxContext *gfxc, const Gnm::RenderTarget *target)
{
	SCE_GNM_ASSERT_MSG(gfxc, "gfxc must not be NULL.");
	decompressFmaskSurface(&gfxc->m_dcb, target);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStageVs);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStagePs);
}
void sce::Gnmx::decompressFmaskSurface(GnmxDrawCommandBuffer *dcb, const Gnm::RenderTarget *target)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	SCE_GNM_ASSERT_MSG(target, "target must not be NULL.");
	if (!target->getFmaskCompressionEnable())
		return; // nothing to do.
	SCE_GNM_ASSERT_MSG(target->getFmaskAddress() != nullptr, "Compressed surface must have an FMASK surface");
	//dcb->pushMarker("Gnmx::decompressFmaskSurface");

	// Required sequence of events for FMASK decompression:
	// 1. Flush the CB data and metadata caches.
	// 2. Fully enable slot 0 with setRenderTargetMask(0xF).  Output to all other MRTs must be disabled.
	// 3. Bind the render target whose FMASK should be decompressed to slot 0.
	// 4. Disable blending.
	// 5. Set the CB mode to kCbModeFmaskDecompress, with a rasterOp of kRasterOpCopy.
	// 6. Bind a pixel shader that outputs to MRT slot 0. Do *NOT* use setPsShader(NULL). The shader code will not be run by this pass,
	//    but the shader state must be configured correctly. The recommended approach is to use setEmbeddedPsShader(kEmbeddedPsShaderDummy).
	// 7. Draw over the screen region(s) that should be modified.  Usually, this will just be a large rectangle the size of the surface.  Do not draw the same pixel twice.
	// 8. Flush the color cache for all the modes.

	dcb->setRenderTarget(0, target);
	//dcb->setDepthRenderTarget((Gnm::DepthRenderTarget *)NULL);

	// Validation
	dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta); // FLUSH_AND_INV_CB_META		// Flush the FMask cache


	Gnm::BlendControl blendControl;
	blendControl.init();
	blendControl.setBlendEnable(false);
	dcb->setBlendControl(0, blendControl);
	dcb->setRenderTargetMask(0x0000000F); // enable MRT0 output only
	dcb->setCbControl(Gnm::kCbModeFmaskDecompress, Gnm::kRasterOpCopy);
	Gnm::DepthStencilControl dsc;
	dsc.init();
	dsc.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways);
	dsc.setDepthEnable(true);
	dcb->setDepthStencilControl(dsc);
	Gnmx::setupScreenViewport(dcb, 0, 0, target->getWidth(), target->getHeight(), 0.5f, 0.5f);

	// draw full screen quad.
	dcb->setPsShader(nullptr);
	renderFullScreenQuad(dcb);

	// Restore CB mode to normal
	dcb->setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);

	// Flush caches again
	if (target->getCmaskAddress() == nullptr)
	{
		// Flush the CB color cache
		dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbPixelData); // FLUSH_AND_INV_CB_PIXEL_DATA
	}
	dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta); // FLUSH_AND_INV_CB_META
	//dcb->popMarker();
}

void sce::Gnmx::decompressDccSurface(GfxContext *gfxc, const Gnm::RenderTarget *target)
{
	SCE_GNM_NEO_MODE_ONLY;
	SCE_GNM_ASSERT_MSG(gfxc, "gfxc must not be NULL.");
	decompressDccSurface(&gfxc->m_dcb, target);
	gfxc->m_cue.invalidateAllShaderContexts();
}
void sce::Gnmx::decompressDccSurface(LightweightGfxContext *gfxc, const Gnm::RenderTarget *target)
{
	SCE_GNM_NEO_MODE_ONLY;
	SCE_GNM_ASSERT_MSG(gfxc, "gfxc must not be NULL.");
	decompressDccSurface(&gfxc->m_dcb, target);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStageVs);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStagePs);
}
void sce::Gnmx::decompressDccSurface(GnmxDrawCommandBuffer *dcb, const Gnm::RenderTarget *target)
{
	SCE_GNM_NEO_MODE_ONLY;
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	SCE_GNM_ASSERT_MSG(target, "target must not be NULL.");
	if (!target->getDccCompressionEnable())
		return; // nothing to do.
	SCE_GNM_ASSERT_MSG(target->getDccAddress() != nullptr, "Compressed surface must have an DCC metadata surface");
	//dcb->pushMarker("Gnmx::decompressDccSurface");

	dcb->setRenderTarget(0, target);
	//dcb->setDepthRenderTarget((Gnm::DepthRenderTarget *)NULL);

	// CB surface data/metadata must be fully flushed/invalidated before the surface operation begins.
	uint32_t *flushLabelAddr = (uint32_t*)dcb->allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment4);
	*flushLabelAddr = 0;
	dcb->writeAtEndOfPipe(Gnm::kEopFlushAndInvalidateCbDbCaches, Gnm::kEventWriteDestMemory, flushLabelAddr,
		Gnm::kEventWriteSource32BitsImmediate, 1, Gnm::kCacheActionNone, Gnm::kCachePolicyLru);
	dcb->waitOnAddress(flushLabelAddr, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, 1);

	Gnm::BlendControl blendControl;
	blendControl.init();
	blendControl.setBlendEnable(false);
	dcb->setBlendControl(0, blendControl);
	dcb->setRenderTargetMask(0x0000000F); // enable MRT0 output only
	dcb->setCbControl(Gnm::kCbModeDccDecompress, Gnm::kRasterOpCopy);
	Gnm::DepthStencilControl dsc;
	dsc.init();
	dsc.setDepthControl(Gnm::kDepthControlZWriteDisable, Gnm::kCompareFuncAlways);
	dsc.setDepthEnable(true);
	dcb->setDepthStencilControl(dsc);
	Gnmx::setupScreenViewport(dcb, 0, 0, target->getWidth(), target->getHeight(), 0.5f, 0.5f);

	// draw full screen quad.
	dcb->setPsShader(nullptr);
	renderFullScreenQuad(dcb);

	// Restore CB mode to normal
	dcb->setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);

	// Must surface-sync before using the surface again, as well as flushing/invalidating the CB metadata cache to avoid using
	// stale DCC metadata in future passes.
	dcb->waitForGraphicsWrites(target->getBaseAddress256ByteBlocks(), target->getSliceSizeInBytes()/256, Gnm::kWaitTargetSlotCb0,
		Gnm::kCacheActionNone, 0, Gnm::kStallCommandBufferParserDisable);
	dcb->triggerEvent(Gnm::kEventTypeCacheFlushAndInvEvent);

	//dcb->popMarker();
}

void sce::Gnmx::decompressDepthSurface(GfxContext *gfxc, const Gnm::DepthRenderTarget *depthTarget)
{
	if(!depthTarget->getHtileAccelerationEnable())
		return; // If the DepthRenderTarget isn't compressed, decompressing it in-place would be a no-op.
	//gfxc->pushMarker("Gnmx::decompressDepthSurface");
	copyFromCompressedDepthSurface(&gfxc->m_dcb, depthTarget, depthTarget, kDecompressInPlace);
	gfxc->m_cue.invalidateAllShaderContexts();
	//gfxc->popMarker();
}
void sce::Gnmx::decompressDepthSurface(LightweightGfxContext *gfxc, const Gnm::DepthRenderTarget *depthTarget)
{
	if(!depthTarget->getHtileAccelerationEnable())
		return; // If the DepthRenderTarget isn't compressed, decompressing it in-place would be a no-op.
	//gfxc->pushMarker("Gnmx::decompressDepthSurface");
	copyFromCompressedDepthSurface(&gfxc->m_dcb, depthTarget, depthTarget, kDecompressInPlace);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStageVs);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStagePs);
	//gfxc->popMarker();
}
void sce::Gnmx::decompressDepthSurface(GnmxDrawCommandBuffer *dcb, const Gnm::DepthRenderTarget *depthTarget)
{
	if(!depthTarget->getHtileAccelerationEnable())
		return; // If the DepthRenderTarget isn't compressed, decompressing it in-place would be a no-op.
	//dcb->pushMarker("Gnmx::decompressDepthSurface");
	copyFromCompressedDepthSurface(dcb, depthTarget, depthTarget, kDecompressInPlace);
	//dcb->popMarker();
}

void sce::Gnmx::decompressDepthSurfaceToCopy(GfxContext *gfxc, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source)
{
	//gfxc->pushMarker("Gnmx::decompressDepthSurfaceToCopy");
	copyFromCompressedDepthSurface(&gfxc->m_dcb, destination, source, kDecompressToCopy);
	gfxc->m_cue.invalidateAllShaderContexts();
	//gfxc->popMarker();
}
void sce::Gnmx::decompressDepthSurfaceToCopy(LightweightGfxContext *gfxc, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source)
{
	//gfxc->pushMarker("Gnmx::decompressDepthSurfaceToCopy");
	copyFromCompressedDepthSurface(&gfxc->m_dcb, destination, source, kDecompressToCopy);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStageVs);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStagePs);
	//gfxc->popMarker();
}
void sce::Gnmx::decompressDepthSurfaceToCopy(GnmxDrawCommandBuffer *dcb, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source)
{
	//dcb->pushMarker("Gnmx::decompressDepthSurfaceToCopy");
	copyFromCompressedDepthSurface(dcb, destination, source, kDecompressToCopy);
	//dcb->popMarker();
}

void sce::Gnmx::copyCompressedDepthSurface(GfxContext *gfxc, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source)
{
	//gfxc->pushMarker("Gnmx::copyCompressedDepthSurface");
	copyFromCompressedDepthSurface(&gfxc->m_dcb, destination, source, kCopyCompressed);
	gfxc->m_cue.invalidateAllShaderContexts();
	//gfxc->popMarker();
}
void sce::Gnmx::copyCompressedDepthSurface(LightweightGfxContext *gfxc, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source)
{
	//gfxc->pushMarker("Gnmx::copyCompressedDepthSurface");
	copyFromCompressedDepthSurface(&gfxc->m_dcb, destination, source, kCopyCompressed);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStageVs);
	gfxc->m_lwcue.invalidateShaderStage(Gnm::kShaderStagePs);
	//gfxc->popMarker();
}
void sce::Gnmx::copyCompressedDepthSurface(GnmxDrawCommandBuffer *dcb, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source)
{
	//dcb->pushMarker("Gnmx::copyCompressedDepthSurface");
	copyFromCompressedDepthSurface(dcb, destination, source, kCopyCompressed);
	//dcb->popMarker();
}

int sce::Gnmx::copyOneFragment(sce::Gnmx::GnmxDrawCommandBuffer *dcb, Gnm::DepthRenderTarget source, Gnm::RenderTarget destination, BuffersToCopy buffersToCopy, int fragmentIndex)
{
    Gnmx::setupScreenViewport(dcb, 0, 0, destination.getWidth(), destination.getHeight(), 0.5f, 0.5f);
	dcb->setEmbeddedPsShader(sce::Gnm::kEmbeddedPsShaderDummyG32R32);

    const bool copyDepthToColor = (buffersToCopy & Gnmx::kBuffersToCopyDepth) != 0;
    const bool copyStencilToColor = (buffersToCopy & Gnmx::kBuffersToCopyStencil) != 0;

	Gnm::BlendControl blendControl;
	blendControl.init();
	blendControl.setBlendEnable(false);
	dcb->setBlendControl(0, blendControl);

    Gnm::DepthStencilControl depthStencilControl;
    depthStencilControl.init();
    depthStencilControl.setDepthEnable(false);
    depthStencilControl.setStencilEnable(false);        
    dcb->setDepthStencilControl(depthStencilControl);

    Gnm::DbRenderControl dbRenderControl;
    dbRenderControl.init();
    dbRenderControl.setCopySampleIndex(fragmentIndex);
    dbRenderControl.setCopyCentroidEnable(true);
    dbRenderControl.setCopyDepthToColor(copyDepthToColor);
    dbRenderControl.setCopyStencilToColor(copyStencilToColor);
    dcb->setDbRenderControl(dbRenderControl);

    dcb->setRenderTarget(0, &destination);
    dcb->setDepthRenderTarget(&source);
    dcb->setRenderTargetMask(buffersToCopy);
	Gnmx::renderFullScreenQuad(dcb);
    dcb->waitForGraphicsWrites(
        destination.getBaseAddress256ByteBlocks(),
        destination.getSliceSizeInBytes() >> 8, 
        Gnm::kWaitTargetSlotCb0,
        Gnm::kCacheActionWriteBackAndInvalidateL1andL2,
        Gnm::kExtendedCacheActionFlushAndInvalidateCbCache,
        Gnm::kStallCommandBufferParserDisable
    );

    dbRenderControl.init();
    dcb->setDbRenderControl(dbRenderControl);
    return 0;
}

static const sce::Gnm::DataFormat    S8 = {{{sce::Gnm::kSurfaceFormat8,    sce::Gnm::kTextureChannelTypeUInt, sce::Gnm::kTextureChannelY, sce::Gnm::kTextureChannelX, sce::Gnm::kTextureChannelConstant0, sce::Gnm::kTextureChannelConstant0, 0}}};
static const sce::Gnm::DataFormat Z16   = Gnm::kDataFormatR16Unorm;
static const sce::Gnm::DataFormat Z24S8 = {{{sce::Gnm::kSurfaceFormat8_24, sce::Gnm::kTextureChannelTypeUInt, sce::Gnm::kTextureChannelX, sce::Gnm::kTextureChannelY, sce::Gnm::kTextureChannelConstant0, sce::Gnm::kTextureChannelConstant0, 0}}};
static const sce::Gnm::DataFormat Z32   = Gnm::kDataFormatR32Float;
static const sce::Gnm::DataFormat zFormat[] = {Gnm::kDataFormatInvalid, Z16, Z24S8, Z32};

int sce::Gnmx::copyOneFragment(sce::Gnmx::GnmxDrawCommandBuffer *dcb, Gnm::DepthRenderTarget source, Gnm::DepthRenderTarget destination, BuffersToCopy buffersToCopy, int fragmentIndex)
{
    SCE_GNM_ASSERT_MSG(destination.getNumFragments() == sce::Gnm::kNumFragments1, "destination must have exactly one fragment per pixel.");
    SCE_GNM_ASSERT_MSG(destination.getTileMode() == sce::Gnm::kTileModeDepth_2dThin_256, "Destination must have TileMode of kTileModeDepth_2dThin_256.");

    Gnm::RenderTargetSpec spec;
    spec.init();
    spec.m_width             = destination.getWidth();
    spec.m_height            = destination.getHeight();
    spec.m_pitch             = destination.getPitch();
    spec.m_numSlices         = destination.getLastArraySliceIndex() + 1;        
    spec.m_colorTileModeHint = sce::Gnm::kTileModeThin_2dThin; 
    spec.m_numSamples        = sce::Gnm::kNumSamples1;
    spec.m_numFragments      = sce::Gnm::kNumFragments1;      
    spec.m_minGpuMode        = destination.getMinimumGpuMode();
    int error = 0;
    if(buffersToCopy & kBuffersToCopyDepth)
    {
        SCE_GNM_ASSERT_MSG(destination.getZWriteAddress(), "destination must have a DEPTH buffer.");
        spec.m_colorFormat = zFormat[destination.getZFormat()];
        Gnm::RenderTarget renderTarget;
        error = renderTarget.init(&spec);        
        if(0 != error)
            return error;
        renderTarget.setBaseAddress(destination.getZWriteAddress());
        error = copyOneFragment(dcb, source, renderTarget, buffersToCopy, fragmentIndex);
        if(0 != error)
            return error;
    }
    if(buffersToCopy & kBuffersToCopyStencil)
    {
        SCE_GNM_ASSERT_MSG(destination.getStencilWriteAddress(), "destination must have a STENCIL buffer.");
        spec.m_colorFormat = S8;
        Gnm::RenderTarget renderTarget;
        error = renderTarget.init(&spec);
        if(0 != error)
            return error;
        renderTarget.setBaseAddress(destination.getStencilWriteAddress());
        error = copyOneFragment(dcb, source, renderTarget, buffersToCopy, fragmentIndex);
        if(0 != error)
            return error;
    }
    return error;
}

#endif // __ORBIS__
