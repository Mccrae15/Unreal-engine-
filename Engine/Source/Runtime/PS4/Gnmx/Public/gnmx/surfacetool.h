/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if !defined(_SCE_GNMX_SURFACETOOL_H)
#define _SCE_GNMX_SURFACETOOL_H

#if defined(__ORBIS__)

#include <gnm/common.h>
#include <gnm/constants.h>
#include <gnm/error.h>
#include <gnmx/common.h>

namespace sce
{
	namespace Gnm
	{
		class DepthRenderTarget;
		class RenderTarget;
		class Texture;
	}
	namespace Gnmx
	{
		class GfxContext;
		class LightweightGfxContext;

		/** @brief A helper function to render a full-screen quad.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Primitive type (function sets to kPrimitiveTypeRectList)
				  - Index size (function sets to kIndexSize16 and kCachePolicyLru)
				  - Vertex shader (function sets to kEmbeddedVsShaderClear)
				  The following render state is assumed to have been set up by the caller:
				  - Pixel shader
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			*/
		void renderFullScreenQuad(GfxContext *gfxc);

		/** @brief A helper function to render a full-screen quad.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Primitive type (function sets to kPrimitiveTypeRectList)
				  - Index size (function sets to kIndexSize16 and kCachePolicyLru)
				  - Vertex shader (function sets to kEmbeddedVsShaderClear)
				  The following render state is assumed to have been set up by the caller:
				  - Pixel shader
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			*/
		void renderFullScreenQuad(LightweightGfxContext *gfxc);

		/** @brief A helper function to render a full-screen quad.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Primitive type (function sets to kPrimitiveTypeRectList)
				  - Index size (function sets to kIndexSize16 and kCachePolicyLru)
				  - Vertex shader (function sets to kEmbeddedVsShaderClear)
				  The following render state is assumed to have been set up by the caller:
				  - Pixel shader
			@param[in,out] dcb The draw command buffer to which this function writes commands. This pointer must not be <c>NULL</c>.
		*/
		void renderFullScreenQuad(GnmxDrawCommandBuffer *dcb);

		/** @brief Resolves an MSAA render target using fixed-function hardware so that it can be subsequently be sampled as a texture.
				   This function modifies render state destructively. See Notes.
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] destTarget The single-sampled render target which will be the destination of the resolve operation. This pointer must not be <c>NULL</c>.
			@param[in] srcTarget The multi-sampled render target which will be the source of the resolve operation. This pointer must not be <c>NULL</c>.
			@note In order for the resolve to succeed, all of the following conditions must be met:
				<ul>
				  <li><c><i>srcTarget</i></c> and <c><i>destTarget</i></c> must have the same MicroTileMode (kMicroTileModeDisplay vs. kMicroTileModeThin). </li>
				  <li><c><i>srcTarget</i></c> and <c><i>destTarget</i></c> must have the same DataFormat. </li>
				  <li><c><i>srcTarget</i></c> and <c><i>destTarget</i></c> must not have a tile mode of kTileModeDisplay_LinearAligned. </li>
				  <li><c><i>destTarget</i></c> must be a single-sample target <c>(getNumFragments() == kNumFragments1)</c>. </li>
				  <li><c><i>destTarget</i></c> must not use CMASK fast-clear. </li>
				</ul>
			@note This function is provided for convenience. Higher-quality resolves can be implemented using custom shaders.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 (function sets to <c><i>srcTarget</i></c>)
				  - Render target slot 1 (function sets to <c><i>destTarget</i></c>)
				  - Depth Render Target (function sets to <c>NULL</c>)
				  - Blend control (function disables blending)
				  - Depth stencil control (function disables zwrite and sets compareFunc=ALWAYS)
				  - Color control (function sets to Normal/Copy)
				  - Primitive type (function sets to <c>kPrimitiveTypeRectList</c>)
				  - Index size (function sets to kIndexSize16 and kCachePolicyLru)
				  - Vertex shader (function sets to <c>kEmbeddedVsShaderClear</c>)
				  - Pixel shader (function sets to <c>kEmbeddedPsShaderDummy</c>)
			*/
		void hardwareMsaaResolve(GfxContext *gfxc, const Gnm::RenderTarget *destTarget, const Gnm::RenderTarget *srcTarget);

		/** @brief Resolves an MSAA render target using fixed-function hardware so that it can be subsequently be sampled as a texture.
					This function modifies render state destructively. See Notes.
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] destTarget The single-sampled render target that will be the destination of the resolve operation. This pointer must not be <c>NULL</c>.
			@param[in] srcTarget The multi-sampled render target that will be the source of the resolve operation. This pointer must not be <c>NULL</c>.
			@note In order for the resolve to succeed, all of the following conditions must be met:
				<ul>
				  <li><c><i>srcTarget</i></c> and <c><i>destTarget</i></c> must have the same MicroTileMode (kMicroTileModeDisplay vs. kMicroTileModeThin). </li>
				  <li><c><i>srcTarget</i></c> and <c><i>destTarget</i></c> must have the same DataFormat. </li>
				  <li><c><i>srcTarget</i></c> and <c><i>destTarget</i></c> must not have a tile mode of kTileModeDisplay_LinearAligned. </li>
				  <li><c><i>destTarget</i></c> must be a single-sample target <c>(getNumFragments() == kNumFragments1)</c>. </li>
				  <li><c><i>destTarget</i></c> must not use CMASK fast-clear. </li>
				</ul>
			@note This function is provided for convenience. Higher-quality resolves can be implemented using custom shaders.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 (function sets to <c><i>srcTarget</i></c>)
				  - Render target slot 1 (function sets to <c><i>destTarget</i></c>)
				  - Depth Render Target (function sets to <c>NULL</c>)
				  - Blend control (function disables blending)
				  - Depth stencil control (function disables zwrite and sets compareFunc=ALWAYS)
				  - Color control (function sets to Normal/Copy)
				  - Primitive type (function sets to <c>kPrimitiveTypeRectList</c>)
				   -Index size (function sets to <c>kIndexSize16</c> and <c>kCachePolicyLru</c>)
				  - Vertex shader (function sets to <c>kEmbeddedVsShaderClear</c>)
				  - Pixel shader (function sets to <c>kEmbeddedPsShaderDummy</c>)
			*/
		void hardwareMsaaResolve(LightweightGfxContext *gfxc, const Gnm::RenderTarget *destTarget, const Gnm::RenderTarget *srcTarget);


		/** @brief Resolves an MSAA render target using fixed-function hardware so that it can be subsequently be sampled as a texture.
				This function modifies render state destructively. See Notes.
			@param[in,out] dcb The command buffer to which this function writes. This pointer must not be <c>NULL</c>.
			@param[in] destTarget The single-sampled render target that will be the destination of the resolve operation. This pointer must not be <c>NULL</c>.
			@param[in] srcTarget The multi-sampled render target that will be the source of the resolve operation. This pointer must not be <c>NULL</c>.
			@note In order for the resolve to succeed, all of the following conditions must be met:
				<ul>
				<li><c><i>srcTarget</i></c> and <c><i>destTarget</i></c> must have the same MicroTileMode (kMicroTileModeDisplay vs. kMicroTileModeThin). </li>
				<li><c><i>srcTarget</i></c> and <c><i>destTarget</i></c> must have the same DataFormat. </li>
				<li><c><i>srcTarget</i></c> and <c><i>destTarget</i></c> must not have a tile mode of kTileModeDisplay_LinearAligned. </li>
				<li><c><i>destTarget</i></c> must be a single-sample target <c>(getNumFragments() == kNumFragments1)</c>. </li>
				<li><c><i>destTarget</i></c> must not use CMASK fast-clear. </li>
				</ul>
			@note This function is provided for convenience. Higher-quality resolves can be implemented using custom shaders.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 (function sets to <c><i>srcTarget</i></c>)
				  - Render target slot 1 (function sets to <c><i>destTarget</i></c>)
				  - Depth Render Target (function sets to <c>NULL</c>)
				  - Blend control (function disables blending)
				  - Depth stencil control (function disables zwrite and sets compareFunc=ALWAYS)
				  - Color control (function sets to Normal/Copy)
				  - Primitive type (function sets to <c>kPrimitiveTypeRectList</c>)
				   -Index size (function sets to <c>kIndexSize16</c> and <c>kCachePolicyLru</c>)
				  - Vertex shader (function sets to <c>kEmbeddedVsShaderClear</c>)
				  - Pixel shader (function sets to <c>kEmbeddedPsShaderDummy</c>)
			*/
		void hardwareMsaaResolve(GnmxDrawCommandBuffer *dcb, const Gnm::RenderTarget *destTarget, const Gnm::RenderTarget *srcTarget);

		/** @brief Applies the second stage of a fast clear operation to a render target: the CMASK
					is examined and all untouched areas of the CB are replaced with the fast clear color.
					If fast clears are enabled, this operation is necessary before binding a color surface as a texture.
					This function modifies render state destructively. See Notes.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 [function sets to target]
				  - Screen viewport [function sets to target's width/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=ALWAYS]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  The following render state is assumed to have been set up by the caller:
				  - Active shader stages [function assumes only VS/PS are enabled]
				  - Fast clear color for MRT slot 0.
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] target The RenderTarget whose fast-clear data should be eliminated. This pointer must not be <c>NULL</c>.
				              The target must have a valid CMASK buffer.
			*/
		void eliminateFastClear(GfxContext *gfxc, const Gnm::RenderTarget *target);

		/** @brief Applies the second stage of a fast clear operation to a render target: the CMASK
					is examined and all untouched areas of the CB are replaced with the fast clear color bound to MRT slot 0.
					If fast clears are enabled, this operation is necessary before binding a color surface as a texture.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 [function sets to target]
				  - Screen viewport [function sets to target's width/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=ALWAYS]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  The following render state is assumed to have been set up by the caller:
				  - Active shader stages [function assumes only VS/PS are enabled]
				  - Fast clear color for MRT slot 0.
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] target The RenderTarget whose fast-clear data should be eliminated. This pointer must not be <c>NULL</c>.
				              The target must have a valid CMASK buffer.
			*/
		void eliminateFastClear(LightweightGfxContext *gfxc, const Gnm::RenderTarget *target);

		/** @brief Applies the second stage of a fast clear operation to a render target: the CMASK
					is examined and all untouched areas of the CB are replaced with the fast clear color bound to MRT slot 0.
					If fast clears are enabled, this operation is necessary before binding a color surface as a texture.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 [function sets to target]
				  - Screen viewport [function sets to target's width/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=ALWAYS]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  The following render state is assumed to have been set up by the caller:
				  - Active shader stages [function assumes only VS/PS are enabled]
				  - Fast clear color for MRT slot 0.
			@param[in,out] dcb The command buffer to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] target The RenderTarget whose fast-clear data should be eliminated. This pointer must not be <c>NULL</c>.
				              The target must have a valid CMASK buffer.
			*/
		void eliminateFastClear(GnmxDrawCommandBuffer *dcb, const Gnm::RenderTarget *target);

		/** @brief Decompresses a RenderTarget's FMASK surface so that all pixels have valid FMASK data. This operation is necessary
			        before binding the FMASK surface as a texture.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 [function sets to target]
				  - Screen viewport [function sets to target's width/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=ALWAYS]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  The following render state is assumed to have been set up by the caller:
				  - Active shader stages [function assumes only VS/PS are enabled]
 				  - Fast clear color for MRT slot 0 (if CMASK is enabled for the provided target).
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] target The RenderTarget whose FMASK surface should be decompressed. This pointer must not be <c>NULL</c>.
				              The target must have a valid FMASK buffer.
			*/
		void decompressFmaskSurface(GfxContext *gfxc, const Gnm::RenderTarget *target);

		/** @brief Decompresses a RenderTarget's FMASK surface so that all pixels have valid FMASK data. This operation is necessary
			        before binding the FMASK surface as a texture.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 [function sets to target]
				  - Screen viewport [function sets to target's width/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=ALWAYS]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  The following render state is assumed to have been set up by the caller:
				  - Active shader stages [function assumes only VS/PS are enabled]
 				  - Fast clear color for MRT slot 0 (if CMASK is enabled for the provided target).
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] target The RenderTarget whose FMASK surface should be decompressed. This pointer must not be <c>NULL</c>.
				              The target must have a valid FMASK buffer.
			*/
		void decompressFmaskSurface(LightweightGfxContext *gfxc, const Gnm::RenderTarget *target);

		/** @brief Decompresses a RenderTarget's FMASK surface so that all pixels have valid FMASK data. This operation is necessary
			        before binding the FMASK surface as a texture.
			@note If CMASK is enabled for the provided target, 
			      this operation will perform an implicit CMASK fast clear elimination pass, as well. .
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 [function sets to target]
				  - Screen viewport [function sets to target's width/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=ALWAYS]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  The following render state is assumed to have been set up by the caller:
				  - Active shader stages [function assumes only VS/PS are enabled]
 				  - Fast clear color for MRT slot 0 (if CMASK is enabled for the provided target).
			@param[in,out] dcb The command buffer to which this function writes. This pointer must not be <c>NULL</c>.
			@param[in] target The RenderTarget whose FMASK surface should be decompressed. This pointer must not be <c>NULL</c>.
				              The target must have a valid FMASK buffer.
			*/
		void decompressFmaskSurface(Gnmx::GnmxDrawCommandBuffer *dcb, const Gnm::RenderTarget *target);

		/** @brief Decompresses a RenderTarget's color surface (which had been compressed using DCC compression). This operation is necessary
			        before binding the color surface as a texture, if DCC compression is enabled.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 [function sets to target]
				  - screen viewport [function sets to target's width/height]
				  - vertex shader / pixel shader [function sets to undefined values]
				  - depth stencil control [function disables zwrite and sets compareFunc=ALWAYS]
				  - blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - color control [function sets to Normal/Copy]
				  - primitive type [function sets to kPrimitiveTypeRectList]
				  - index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - vertex shader [function sets to kEmbeddedVsShaderClear]
				  - pixel shader [function sets to NULL]
				  The following render state is assumed to have been set up by the caller:
				  - active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] target The RenderTarget whose color surface should be decompressed. This pointer must not be <c>NULL</c>.
				              If DCC compression is enabled, the target must have a valid DCC metadata buffer.
		    @note  NEO mode only
			*/
		void decompressDccSurface(GfxContext *gfxc, const Gnm::RenderTarget *target);

		/** @brief Decompresses a RenderTarget's color surface (which had been compressed using DCC compression). This operation is necessary
			        before binding the color surface as a texture, if DCC compression is enabled.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 [function sets to target]
				  - screen viewport [function sets to target's width/height]
				  - vertex shader / pixel shader [function sets to undefined values]
				  - depth stencil control [function disables zwrite and sets compareFunc=ALWAYS]
				  - blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - color control [function sets to Normal/Copy]
				  - primitive type [function sets to kPrimitiveTypeRectList]
				  - index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - vertex shader [function sets to kEmbeddedVsShaderClear]
				  - pixel shader [function sets to NULL]
				  The following render state is assumed to have been set up by the caller:
				  - active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] target The RenderTarget whose color surface should be decompressed. This pointer must not be <c>NULL</c>.
				              If DCC compression is enabled, the target must have a valid DCC metadata buffer.
		    @note  NEO mode only
			*/
		void decompressDccSurface(LightweightGfxContext *gfxc, const Gnm::RenderTarget *target);

		/** @brief Decompresses a RenderTarget's color surface (which had been compressed using DCC compression). This operation is necessary
			        before binding the color surface as a texture, if DCC compression is enabled.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Render target slot 0 [function sets to target]
				  - screen viewport [function sets to target's width/height]
				  - vertex shader / pixel shader [function sets to undefined values]
				  - depth stencil control [function disables zwrite and sets compareFunc=ALWAYS]
				  - blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - color control [function sets to Normal/Copy]
				  - primitive type [function sets to kPrimitiveTypeRectList]
				  - index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - vertex shader [function sets to kEmbeddedVsShaderClear]
				  - pixel shader [function sets to NULL]
				  The following render state is assumed to have been set up by the caller:
				  - active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] dcb The command buffer to which this function writes. This pointer must not be <c>NULL</c>.
			@param[in] target The RenderTarget whose color surface should be decompressed. This pointer must not be <c>NULL</c>.
				              If DCC compression is enabled, the target must have a valid DCC metadata buffer.
 		    @note  NEO mode only
			*/
		void decompressDccSurface(GnmxDrawCommandBuffer *dcb, const Gnm::RenderTarget *target);

		/** @brief Decompresses a DepthRenderTarget's depth surface so that all unwritten pixels have valid depth data.
			        This operation is necessary before binding the depth surface as a texture if depth compression is enabled.
			@note The following render state is modified destructively by this function, and may need to be reset:
				  - Depth render target [function sets to <c><i>depthTarget</i></c>]
				  - Screen viewport [function sets to <c><i>depthTarget</i></c>'s pitch/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=NEVER]
				  - Depth render control [function disables stencil and depth compression]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  The following render state is assumed to have been set up by the caller:
				  - Active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] depthTarget The depth target whose depth data should be decompressed. This pointer must not be <c>NULL</c>.
				                   The target must have a valid HTILE buffer.
			*/
		void decompressDepthSurface(GfxContext *gfxc, const Gnm::DepthRenderTarget *depthTarget);

		/** @brief Decompresses a DepthRenderTarget's depth surface so that all unwritten pixels have valid depth data.
			        This operation is necessary before binding the depth surface as a texture if depth compression is enabled.
			@note The following render state is modified destructively by this function, and may need to be reset:
				  - Depth render target [function sets to <c><i>depthTarget</i></c>]
				  - Screen viewport [function sets to <c><i>depthTarget</i></c>'s pitch/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=NEVER]
				  - Depth render control [function disables stencil and depth compression]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  The following render state is assumed to have been set up by the caller:
				  - active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] depthTarget The depth target whose depth data should be decompressed. This pointer must not be <c>NULL</c>.
				                   The target must have a valid HTILE buffer.
			*/
		void decompressDepthSurface(LightweightGfxContext *gfxc, const Gnm::DepthRenderTarget *depthTarget);

		/** @brief Decompresses a DepthRenderTarget's depth surface so that all unwritten pixels have valid depth data.
			        This operation is necessary before binding the depth surface as a texture if depth compression is enabled.
			@note The following render state is modified destructively by this function, and may need to be reset:
				  - Depth render target [function sets to <c><i>depthTarget</i></c>]
				  - Screen viewport [function sets to <c><i>depthTarget</i></c>'s pitch/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=NEVER]
				  - Depth render control [function disables stencil and depth compression]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  The following render state is assumed to have been set up by the caller:
				  - active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] dcb The command buffer to which this function writes. This pointer must not be <c>NULL</c>.
			@param[in] depthTarget The depth target whose depth data should be decompressed. This pointer must not be <c>NULL</c>.
				                   The target must have a valid HTILE buffer.
			*/
		void decompressDepthSurface(GnmxDrawCommandBuffer *dcb, const Gnm::DepthRenderTarget *depthTarget);

		/** @brief Decompresses a DepthRenderTarget's depth surface to a copy without decompressing the original depth surface.
            @note Fields other than ZMASK and SMEM in the source's HTILE buffer may be modified by this operation.
                  If you require other fields to remain unchanged, please copy the source's HTILE buffer before calling this function.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Depth render target [function sets to <c><i>destination</i></c>]
				  - Screen viewport [function sets to <c><i>destination</i></c>'s pitch/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=NEVER]
				  - Depth render control [function disables stencil and depth compression]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  - Render override control
				  The following render state is assumed to have been set up by the caller:
				  - active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] destination The decompressed depth data from source will be written to this depth target. This pointer must not be <c>NULL</c>.
			@param[in] source The depth target whose depth data should be decompressed. This pointer must not be <c>NULL</c>.
				              The target must have a valid HTILE buffer.
			*/
		void decompressDepthSurfaceToCopy(GfxContext *gfxc, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source);

		/** @brief Decompresses a DepthRenderTarget's depth surface to a copy without decompressing the original depth surface.
            @note Fields other than ZMASK and SMEM in the source's HTILE buffer may be modified by this operation.
                  If you require other fields to remain unchanged, please copy the source's HTILE buffer before calling this function.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Depth render target [function sets to <c><i>destination</i></c>]
				  - Screen viewport [function sets to <c><i>destination</i></c>'s pitch/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=NEVER]
				  - Depth render control [function disables stencil and depth compression]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  - Render override control
				  The following render state is assumed to have been set up by the caller:
				  - active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] destination The decompressed depth data from source will be written to this depth target. This pointer must not be <c>NULL</c>.
			@param[in] source The depth target whose depth data should be decompressed. This pointer must not be <c>NULL</c>.
				              The target must have a valid HTILE buffer.
			*/
		void decompressDepthSurfaceToCopy(LightweightGfxContext *gfxc, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source);

		/** @brief Decompresses a DepthRenderTarget's depth surface to a copy without decompressing the original depth surface.
            @note Fields other than ZMASK and SMEM in the source's HTILE buffer may be modified by this operation.
                  If you require other fields to remain unchanged, please copy the source's HTILE buffer before calling this function.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Depth render target [function sets to <c><i>destination</i></c>]
				  - Screen viewport [function sets to <c><i>destination</i></c>'s pitch/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=NEVER]
				  - Depth render control [function disables stencil and depth compression]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to 0xF]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to kPrimitiveTypeRectList]
				  - Index size [function sets to kIndexSize16 and kCachePolicyLru]
				  - Vertex shader [function sets to kEmbeddedVsShaderClear]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  - Render override control
				  The following render state is assumed to have been set up by the caller:
				  - Active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] dcb The command buffer to which this function writes. This pointer must not be <c>NULL</c>.
			@param[in] destination The decompressed depth data from source will be written to this depth target. This pointer must not be <c>NULL</c>.
			@param[in] source The depth target whose depth data should be decompressed. This pointer must not be <c>NULL</c>.
				              The target must have a valid HTILE buffer.
			*/
		void decompressDepthSurfaceToCopy(GnmxDrawCommandBuffer *dcb, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source);

		/** @brief Copies one compressed depth surface to another without decompressing either surface.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Depth render target [function sets to <c><i>destination</i></c>]
				  - Screen viewport [function sets to <c><i>destination</i></c>'s pitch/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=NEVER]
				  - Depth render control [function disables stencil and depth compression]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to <c>0xF</c>]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to <c>kPrimitiveTypeRectList</c>]
				  - Index size [function sets to <c>kIndexSize16</c> and <c>kCachePolicyLru</c>]
				  - Vertex shader [function sets to <c>kEmbeddedVsShaderClear</c>]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  - Render override control
				  The following render state is assumed to have been set up by the caller:
				  - active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] destination The compressed depth data from source will be written to this depth target. This pointer must not be <c>NULL</c>.
				                   The target must have a valid HTILE buffer.
			@param[in] source The depth target whose compressed depth data should be copied. This pointer must not be <c>NULL</c>.
				              The target must have a valid HTILE buffer.
				*/
		void copyCompressedDepthSurface(GfxContext *gfxc, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source);

		/** @brief Copies one compressed depth surface to another without decompressing either surface.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Depth render target [function sets to <c><i>destination</i></c>]
				  - Screen viewport [function sets to <c><i>destination</i></c>'s pitch/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=NEVER]
				  - Depth render control [function disables stencil and depth compression]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to <c>0xF</c>]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to <c>kPrimitiveTypeRectList</c>]
				  - Index size [function sets to <c>kIndexSize16</c> and <c>kCachePolicyLru</c>]
				  - Vertex shader [function sets to <c>kEmbeddedVsShaderClear</c>]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  - Render override control
				  The following render state is assumed to have been set up by the caller:
				  - active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] gfxc The graphics context to which this function writes commands. This pointer must not be <c>NULL</c>.
			@param[in] destination The compressed depth data from source will be written to this depth target. This pointer must not be <c>NULL</c>.
				                   The target must have a valid HTILE buffer.
			@param[in] source The depth target whose compressed depth data should be copied. This pointer must not be <c>NULL</c>.
				              The target must have a valid HTILE buffer.
				*/
		void copyCompressedDepthSurface(LightweightGfxContext *gfxc, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source);

		/** @brief Copies one compressed depth surface to another without decompressing either surface.
			@note The following render state is modified destructively by this function and may need to be reset:
				  - Depth render target [function sets to <c><i>destination</i></c>]
				  - Screen viewport [function sets to <c><i>destination</i></c>'s pitch/height]
				  - Vertex shader / pixel shader [function sets to undefined values]
				  - Depth stencil control [function disables zwrite and sets compareFunc=NEVER]
				  - Depth render control [function disables stencil and depth compression]
				  - Blend control [function disables blending]
				  - MRT color mask [function sets to <c>0xF</c>]
				  - Color control [function sets to Normal/Copy]
				  - Primitive type [function sets to <c>kPrimitiveTypeRectList</c>]
				  - Index size [function sets to <c>kIndexSize16</c> and <c>kCachePolicyLru</c>]
				  - Vertex shader [function sets to <c>kEmbeddedVsShaderClear</c>]
				  - Pixel shader [function sets to <c>kEmbeddedPsShaderDummy</c>]
				  - Render override control
				  The following render state is assumed to have been set up by the caller:
				  - active shader stages [function assumes only VS/PS are enabled]
			@param[in,out] dcb The command buffer to which this function writes. This pointer must not be <c>NULL</c>.
			@param[in] destination The compressed depth data from source will be written to this depth target. This pointer must not be <c>NULL</c>.
				                   The target must have a valid HTILE buffer.
			@param[in] source The depth target whose compressed depth data should be copied. This pointer must not be <c>NULL</c>.
				              The target must have a valid HTILE buffer.
				*/
		void copyCompressedDepthSurface(Gnmx::GnmxDrawCommandBuffer *dcb, const Gnm::DepthRenderTarget *destination, const Gnm::DepthRenderTarget *source);

		/** @brief Which buffers to copy, when doing a one-fragment copy from a DepthRenderTarget.
		*/
        typedef enum BuffersToCopy
        {
            kBuffersToCopyDepth           = 0x1, ///< Copy only the DEPTH buffer.
            kBuffersToCopyStencil         = 0x2, ///< Copy only the STENCIL buffer.
            kBuffersToCopyDepthAndStencil = 0x3, ///< Copy both the DEPTH and the STENCIL buffers.
        } BuffersToCopy;

        /** @brief Efficiently copies one fragment of a DepthRenderTarget to the COLOR buffer of a RenderTarget, even if the source is compressed.
            @param[in,out]  dcb             The command buffer to which this function writes. This pointer must not be <c>NULL</c>.
            @param[in]      source          The DepthRenderTarget to copy from.
            @param[in,out]  destination     The RenderTarget to copy to. 
            @param[in]      buffersToCopy   Whether to copy the DEPTH buffer, the STENCIL buffer, or both.
            @param[in]      fragmentIndex   The index of the fragment in the DepthRenderTarget to copy from.    
         */
        int copyOneFragment(Gnmx::GnmxDrawCommandBuffer *dcb, Gnm::DepthRenderTarget source, Gnm::RenderTarget destination, BuffersToCopy buffersToCopy, int fragmentIndex);

        /** @brief Efficiently copies one fragment of a DepthRenderTarget to another DepthRenderTarget, even if the source is compressed.
            @param[in,out]  dcb             The command buffer to which this function writes. This pointer must not be <c>NULL</c>.
            @param[in]      source          The DepthRenderTarget to copy from.
            @param[in,out]  destination     The DepthRenderTarget to copy to.
            @param[in]      buffersToCopy   Whether to copy the DEPTH buffer or the STENCIL buffer.
            @param[in]      fragmentIndex   The index of the fragment in the DepthRenderTarget to copy from.    
         */
        int copyOneFragment(Gnmx::GnmxDrawCommandBuffer *dcb, Gnm::DepthRenderTarget source, Gnm::DepthRenderTarget destination, BuffersToCopy buffersToCopy, int fragmentIndex);
    }
}

#endif	// __ORBIS__

#endif	// _SCE_GNMX_SURFACETOOL_H
