/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/
#pragma once

#if !defined(_SCE_GNMX_LWGFXCONTEXT_H)
#define _SCE_GNMX_LWGFXCONTEXT_H

#include <gnmx/basegfxcontext.h>
#include <gnmx/lwcue_graphics.h>

namespace sce
{
	namespace Gnmx
	{
		/** @brief Encapsulates a Gnm::DrawCommandBuffer and Gnm::ConstantCommandBuffer pair with the LightweightGraphicsConstantUpdateEngine and wraps them in a single higher-level interface.
		 * 
		 *  The LightweightGfxContext object submits to the graphics ring and supports both graphics and compute operations. For compute-only tasks, use the ComputeContext class.
		 *  @see ComputeContext
		 */
		class SCE_GNMX_EXPORT LightweightGfxContext : public BaseGfxContext
		{
		public:

#if !defined(DOXYGEN_IGNORE)
			// These methods are silently ignored as they do not apply to the LCUE
			SCE_GNM_FORCE_INLINE void setActiveResourceSlotCount(Gnm::ShaderStage stage, uint32_t count) {SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(count);}
			SCE_GNM_FORCE_INLINE void setActiveRwResourceSlotCount(Gnm::ShaderStage stage, uint32_t count) {SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(count);}
			SCE_GNM_FORCE_INLINE void setActiveSamplerSlotCount(Gnm::ShaderStage stage, uint32_t count) {SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(count);}
			SCE_GNM_FORCE_INLINE void setActiveVertexBufferSlotCount(Gnm::ShaderStage stage, uint32_t count) {SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(count);}
			SCE_GNM_FORCE_INLINE void setActiveConstantBufferSlotCount(Gnm::ShaderStage stage, uint32_t count) {SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(count);}
			SCE_GNM_FORCE_INLINE void setActiveStreamoutBufferSlotCount(uint32_t count) {SCE_GNM_UNUSED(count);}

			// GsMode is handled automatically by the LightweightConstantUpdateEngine, see GraphicsConstantUpdateEngine::setActiveShaderStages() for more details.
			SCE_GNM_FORCE_INLINE void setGsModeOff() {} 

			// LCUE does not support user resource tables
			SCE_GNM_LCUE_NOT_SUPPORTED void setUserResourceTable(Gnm::ShaderStage stage, const Gnm::Texture *table){SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(table);}
			SCE_GNM_LCUE_NOT_SUPPORTED void setUserRwResourceTable(Gnm::ShaderStage stage, const Gnm::Texture *table){SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(table);}
			SCE_GNM_LCUE_NOT_SUPPORTED void setUserSamplerTable(Gnm::ShaderStage stage, const Gnm::Sampler *table){SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(table);}
			SCE_GNM_LCUE_NOT_SUPPORTED void setUserConstantBufferTable(Gnm::ShaderStage stage, const Gnm::Buffer *table){SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(table);}
			SCE_GNM_LCUE_NOT_SUPPORTED void setUserVertexTable(Gnm::ShaderStage stage, const Gnm::Buffer *table){SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(table);}
			SCE_GNM_LCUE_NOT_SUPPORTED void clearUserTables();
			
			//LCUE does not support VS streamout only shaders
			//SCE_GNM_LCUE_NOT_SUPPORTED void setVsShaderStreamoutEnable(bool enable){SCE_GNM_UNUSED(enable);}
#endif // !defined(DOXYGEN_IGNORE)

		public:

			/** @brief Initializes a LightweightGfxContext with application-provided memory buffers, including LCUE buffers.
			 *
			 *  @param[in] dcbBuffer						A buffer for use by the Draw Command Buffer.
			 *  @param[in] dcbBufferSizeInBytes				The size of <c><i>dcbBuffer</i></c> in bytes.
			 *  @param[in] resourceBufferInGarlic			A resource buffer for use by the LCUE; can be <c>NULL</c> if <c><i>resourceBufferSizeInBytes</i></c> is equal to <c>0</c>.
			 *  @param[in] resourceBufferSizeInBytes		The size of <c><i>resourceBufferInGarlic</i></c> in bytes; can be <c>0</c>.
			 *  @param[in] globalInternalResourceTableAddr	A pointer to the global resource table in memory; can be  <c>NULL</c>.
			 *  @param[in] numDispatchDrawRingBuffersRolls	The number of allowed dispatch ring buffer rolls for the lcue instance (see setupDispatchDrawRingBuffers() for more details).
			 *
			 *  @note The ability to initialize the LightweightGfxContext with no resource buffer (<c>resourceBufferInGarlic = NULL</c>) relies on your registering a "resource buffer full" callback function
			 *  that handles out-of-memory conditions.
			 *
			 *  @see LightweightGfxContext::setResourceBufferFullCallback()
			 */
			void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes,
					  void* globalInternalResourceTableAddr, uint32_t numDispatchDrawRingBuffersRolls);


			/** @brief Initializes a LightweightGfxContext with application-provided memory buffers, including LCUE buffers.
			 *
			 *  @param[in] dcbBuffer						A buffer for use by the Draw Command Buffer.
			 *  @param[in] dcbBufferSizeInBytes				The size of <c><i>dcbBuffer</i></c> in bytes.
			 *  @param[in] resourceBufferInGarlic			A resource buffer for use by the LCUE; can be <c>NULL</c> if <c><i>resourceBufferSizeInBytes</i></c> is equal to <c>0</c>.
			 *  @param[in] resourceBufferSizeInBytes		The size of <c><i>resourceBufferInGarlic</i></c> in bytes; can be <c>0</c>.
			 *  @param[in] globalInternalResourceTableAddr	A pointer to the global resource table in memory; can be  <c>NULL</c>.
			 *
			 *  @note The ability to initialize the LightweightGfxContext with no resource buffer (<c>resourceBufferInGarlic = NULL</c>) relies on your registering a "resource buffer full" callback function
			 *  that handles out-of-memory conditions.
			 *
			 *  @see LightweightGfxContext::setResourceBufferFullCallback()
			 */
			void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr)
			{
				init(dcbBuffer, dcbBufferSizeInBytes, resourceBufferInGarlic, resourceBufferSizeInBytes, globalInternalResourceTableAddr,
					 LightweightConstantUpdateEngine::kNumDispatchDrawRingBuffersRolls);
			}


			/** @brief Binds a function for the LCUE to call when the main resource buffer runs out of memory.
			 *
			 *  @param[in] callbackFunction		A function to call when the resource buffer runs out of memory.
			 *  @param[in] userData				The user data passed to <c><i>callbackFunction</i></c>.
			 */
			void setResourceBufferFullCallback(AllocResourceBufferCallback callbackFunction, void* userData)
			{
				m_lwcue.setResourceBufferFullCallback(callbackFunction, userData);
			}


			/** @brief Initializes a GfxContext with application-provided memory buffers to enable dispatch draw.
			 *
			 *	@note This function can only be called after init() has been called.
			 *
			 *	@param[in] acbBuffer		A buffer for use by the Dispatch Command Buffer.
			 *	@param[in] acbSizeInBytes	The size of <c><i>acbBuffer</i></c> in bytes.
			 */
			SCE_GNM_LCUE_DISPATCH_DRAW_AVAILABILITY()
			void initDispatchDrawCommandBuffer(void *acbBuffer, uint32_t acbSizeInBytes);


			/** @brief Resets the Gnm::DrawCommandBuffer and swaps buffers in the LCUE for a new frame.
			 *
			 *	This function should be called at the beginning of every frame.
			 *	The Gnm::DrawCommandBuffer, Gnm::ConstantCommandBuffer and asynchronous Gnm::DispatchCommandBuffer
			 *	will be reset to empty (<c>m_cmdptr = m_beginptr</c>).
			 */
			void reset(void);


			/** @brief Splits the command buffers.
			 *	This function will cause the GfxContext to stop adding commands to the active command buffers and create new ones.
			 *	This is most often necessary when chainCommandBuffer() is called, which must be the last packet in its DCB.
			 */
			bool splitCommandBuffers(bool bAdvanceEndOfBuffer = true);


			/** @brief An LCUE wrapper to set the active shader stages in the graphics pipeline.
			 *
			 *	Note that the compute-only CS stage is always active.
			 *	This function will roll the hardware context.
			 *
			 *	@param[in] activeStages Indicates which shader stages should be activated.
			 *
			 *  @note setGsMode() will be automatically set by the LCUE
			 */
			void setActiveShaderStages(Gnm::ActiveShaderStages activeStages)
			{
				m_lwcue.setActiveShaderStages(activeStages);
				return m_dcb.setActiveShaderStages(activeStages);
			}

		   /** @brief Enables/disables the prefetch of the shader code.
			 *
			 *	@param[in] enable	A flag that specifies whether to enable the prefetch of the shader code.
		 	 *
			 *	@note Shader code prefetching is enabled by default.
				@note  The implementation of the shader prefetching feature uses the prefetchIntoL2() function that the Gnm library provides. See the notes for <c>prefetchIntoL2()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			 *  @see  DrawCommandBuffer::prefetchIntoL2()
			 */
			SCE_GNM_FORCE_INLINE void setShaderCodePrefetchEnable(bool enable)
			{
				m_lwcue.setShaderCodePrefetchEnable(enable);
			}


			/** @brief Enables/disables automatic generation and binding of the reserved tessellation constant buffer
			 *
			 *	@param[in] enable	A flag that specifies whether to enable automatic generation and binding of the reserved tessellation constant buffer
		 	 *
			 *	@note This feature is enabled by default.
			 *	@note If enabled, it is not necessary to call LightweightGfxContext::setTessellationDataConstantBuffer().
			 *	 
 			 */
			SCE_GNM_FORCE_INLINE void setTessellationAutoManageReservedCBEnable(bool enable)
			{
				m_lwcue.setTessellationAutoManageReservedCBEnable(enable);
			}


			///////////// Shader bindings

			/** @brief Binds an ES shader to the ES stage.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] esb				A pointer to the ES shader.
			 *  @param[in] shaderModifier	The shader modifier value generated by generateEsFetchShaderBuildState(). Specify a value of 0 if there is no fetch shader.
			 *  @param[in] fetchShader		If the shader requires a fetch shader, pass its GPU address here; otherwise, pass <c>NULL</c>.
			 *  @param[in] table			The matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*esb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setEsShader() is called again, all resource bindings for the stage will need to be rebound.
			 */
			SCE_GNM_FORCE_INLINE void setEsShader(const sce::Gnmx::EsShader* esb, uint32_t shaderModifier, const void* fetchShader, const InputResourceOffsets* table)
			{
				m_lwcue.setEsShader(esb, shaderModifier, fetchShader, table);
			}


			/** @brief Binds an ES shader to the ES stage without a fetch shader.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] esb				A pointer to the ES shader.
			 *  @param[in] table			The matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*esb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setEsShader() is called again, all resource bindings for the stage will need to be rebound.
			 */
			SCE_GNM_FORCE_INLINE void setEsShader(const sce::Gnmx::EsShader* esb, const InputResourceOffsets* table)
			{ 
				m_lwcue.setEsShader(esb, table); 
			}


			/** @brief Binds an ES shader to the ES stage.
			 *
			 *  This function never rolls the hardware context.
			 *  @param[in] esb					A pointer to the ES shader.
			 *  @param[in] ldsSizeIn512Bytes	The LDS allocation size in 512-byte granularity allocation units. Internally, this value will be passed to
			 *									EsStageRegisters::updateLdsSize() before the EsShader is bound. If this parameter is 0, the function behaves
			 *									identically to ConstantUpdateEngine::setEsShader().
			 *  @param[in] shaderModifier		The shader modifier value generated by generateEsFetchShaderBuildState(). Specify a value of 0 if there is no fetch shader.
			 *  @param[in] fetchShader			If the shader requires a fetch shader, pass its GPU address here; otherwise, pass <c>NULL</c>.
			 *  @param[in] table				A matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*esb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setEsShader() is called again, all resource bindings for the stage will need to be rebound.
			 *
			 *  @see Gnmx::computeOnChipGsConfiguration()
			 */
			SCE_GNM_FORCE_INLINE void setOnChipEsShader(const Gnmx::EsShader *esb, uint32_t ldsSizeIn512Bytes, uint32_t shaderModifier, const void *fetchShader, const InputResourceOffsets* table)
			{
				m_lwcue.setOnChipEsShader(esb, ldsSizeIn512Bytes, shaderModifier, fetchShader, table);
			}


			/** @brief Binds an ES shader to the ES stage without a fetch shader.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] esb					A pointer to the ES shader.
			 *  @param[in] ldsSizeIn512Bytes	The LDS allocation size in 512-byte granularity allocation units. Internally, this value will be passed to
			 *									EsStageRegisters::updateLdsSize() before the EsShader is bound. If this parameter is 0, the function behaves
			 *									identically to ConstantUpdateEngine::setEsShader().
			 *  @param[in] table				The matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*esb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setEsShader() is called again, all resource bindings for the stage will need to be rebound.
			 *
			 *  @see Gnmx::computeOnChipGsConfiguration()
			 */
			SCE_GNM_FORCE_INLINE void setOnChipEsShader(const Gnmx::EsShader *esb, uint32_t ldsSizeIn512Bytes, const InputResourceOffsets* table)
			{ 
				m_lwcue.setOnChipEsShader(esb, ldsSizeIn512Bytes, table); 
			}


			/** @brief Binds a fetch shader separately for the ES stage. Use for late fetch shader binding or updating the fetch shader.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] shaderModifier		The shader modifier value generated by generateEsFetchShaderBuildState().
			 *  @param[in] fetchShader			A pointer to the fetch shader.
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*fetchShader</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 */
			SCE_GNM_FORCE_INLINE void setEsFetchShader(uint32_t shaderModifier, const void* fetchShader)
			{
				m_lwcue.setEsFetchShader(shaderModifier, fetchShader);
			}

			
			/** @brief Binds a GS shader to the GS and VS stages.
			 *
			 *  This function will roll hardware context if any of the Gnm::GsStageRegisters entries set in the GsShader (specified in <c><i>gsb</i></c>),
			 *  or the Gnm::VsStageRegisters entries set for the copy shader (specified in GsShader::getCopyShader() from <c><i>gsb</i></c>) are different from current state:
			 *
			 *  Gnm::GsStageRegisters
			 *  - <c>m_vgtStrmoutConfig</c>
			 *  - <c>m_vgtGsOutPrimType</c>
			 *  - <c>m_vgtGsInstanceCnt</c>
			 *
			 *  Gnm::VsStageRegisters
			 *  - <c>m_spiVsOutConfig</c>
			 *  - <c>m_spiShaderPosFormat</c>
			 *  - <c>m_paClVsOutCntl</c>
			 * 
			 *  @param[in] gsb		A pointer to a GS shader to bind to the GS/VS stages.
			 *  @param[in] gsTable	The matching InputResourceOffsets table created by generateShaderResourceOffsetTable.
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*gsb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setGsVsShaders() is called again, all resource bindings for the stage will need to be rebound.
			 */
			SCE_GNM_FORCE_INLINE void setGsVsShaders(const sce::Gnmx::GsShader* gsb, const InputResourceOffsets* gsTable)
			{
				m_lwcue.setGsVsShaders(gsb, gsTable);
			}


			/** @brief Binds an on-chip GS shader to the GS and VS stages and sets up the on-chip GS sub-group size controls.
			 *
			 * This function may roll hardware context. This will occur if either the GS shader or the bundled VS copy shader are
			 * different from the currently-bound shaders in these stages:
			 *
			 *
			 *  Gnm::GsStageRegisters
			 *  - <c>m_vgtStrmoutConfig</c>
			 *  - <c>m_vgtGsOutPrimType</c>
			 *  - <c>m_vgtGsInstanceCnt</c>
			 *
			 *  Gnm::VsStageRegisters
			 *  - <c>m_spiVsOutConfig</c>
			 *  - <c>m_spiShaderPosFormat</c>
			 *  - <c>m_paClVsOutCntl</c>
			 * 
			 *  This function will also roll context if the value of gsPrimsPerSubGroup or <c>gsb->m_inputVertexCount</c> changes.
			 *
			 *  @param[in] gsb					A pointer to a GS shader to bind to the GS/VS stages.
			 *  @param[in] gsPrimsPerSubGroup	The number of GS threads which will be launched per on-chip-GS LDS allocation, which must be compatible with the size of LDS allocation passed to setOnChipEsShaders().
			 *  @param[in] gsTable				The matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*gsb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setGsVsShaders() is called again, all resource bindings for the stage will need to be rebound.
			 *
			 *  @see Gnmx::computeOnChipGsConfiguration()
			 */
			SCE_GNM_FORCE_INLINE void setOnChipGsVsShaders(const sce::Gnmx::GsShader* gsb, uint32_t gsPrimsPerSubGroup, const InputResourceOffsets* gsTable)
			{
				m_lwcue.setOnChipGsVsShaders(gsb, gsPrimsPerSubGroup, gsTable);
			}


			/** @brief Binds a LS shader to the LS stage.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] lsb				A pointer to the LS shader.
			 *  @param[in] shaderModifier	The shader modifier value generated by generateLsFetchShaderBuildState(). Specify a value of 0 if there is no fetch shader.
			 *  @param[in] fetchShader		The GPU address of the fetch shader if one is required; otherwise specify a value of <c>NULL</c>.
			 *  @param[in] table			The matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*lsb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setLsShader() is called again, all resource bindings for the stage will need to be rebound.
			 */
			SCE_GNM_FORCE_INLINE void setLsShader(const sce::Gnmx::LsShader* lsb, uint32_t shaderModifier, const void* fetchShader, const InputResourceOffsets* table)
			{
				m_lwcue.setLsShader(lsb, shaderModifier, fetchShader, table);
			}


			/** @brief Binds a LS shader to the LS stage without a fetch shader.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] lsb			A pointer to the LS shader.
			 *  @param[in] table		The matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*lsb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setLsShader() is called again, all resource bindings for the stage will need to be rebound.
			 */
			SCE_GNM_FORCE_INLINE void setLsShader(const sce::Gnmx::LsShader* lsb, const InputResourceOffsets* table)
			{ 
				m_lwcue.setLsShader(lsb, table); 
			}


			/** @brief Binds a fetch shader separately for the LS stage. Use for late fetch shader binding or updating the fetch shader.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] shaderModified	The shader modifier value generated by generateLsFetchShaderBuildState().
			 *  @param[in] fetchShader		A pointer to the fetch shader.
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*fetchShader</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 */
			SCE_GNM_FORCE_INLINE void setLsFetchShader(uint32_t shaderModified, const void* fetchShader)
			{
				m_lwcue.setLsFetchShader(shaderModified, fetchShader); 
			}


			/** @brief Binds a HS shader to the HS stage.
			 *
			 *  This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants, which are set in <c><i>hsb</i></c>, or the value of <c><i>tgPatchCount</i></c> are different from current state.
			 *
			 *  @param[in] hsb				A pointer to the HS shader.
			 *  @param[in] table			The matching InputResourceOffsets table created by generateShaderResourceOffsetTable.
			 *  @param[in] tgPatchCount		The user desired patch count per thread group.
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*hsb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setHsShader() is called again, all resource bindings for the stage will need to be rebound.
			 */
			SCE_GNM_FORCE_INLINE void setHsShader(const sce::Gnmx::HsShader* hsb, const InputResourceOffsets* table, uint32_t tgPatchCount)
			{
				m_lwcue.setHsShader(hsb, table, tgPatchCount);
			}

			/** @brief Binds an HS shader to the HS stage.
			 *
			 *  This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants (which are set in <c><i>hsb</i></c>) or the value of <c><i>tgPatchCount</i></c> differ from current state.
			 *
			 *  @param[in] hsb				A pointer to the HS shader.
			 *  @param[in] table			The matching InputResourceOffsets table that generateShaderResourceOffsetTable() created.
			 *  @param[in] tgPatchCount		The requested patch count per thread group.
			 *  @param[in] distributionMode Tessellation distribution mode.
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*hsb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first, before any resource-binding calls. If setHsShader() is called again, all resource bindings for the stage will need to be rebound.
			 *  @note NEO mode only.
			 */
			SCE_GNM_FORCE_INLINE void setHsShader(const sce::Gnmx::HsShader* hsb, const InputResourceOffsets* table, uint32_t tgPatchCount, Gnm::TessellationDistributionMode distributionMode)
			{
				m_lwcue.setHsShader(hsb, table, tgPatchCount, distributionMode);
			}
					

			/** @brief Binds shaders to the LS and HS stages, and updates the LDS size of the LS shader based on the needs of the HS shader.
			*
			*  This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants (which are set in <c><i>hsb</i></c>) or the value of <c><i>tgPatchCount</i></c> differ from current state:
			*
			*  Gnm::HsStageRegisters
			*  - <c>m_vgtTfParam</c>
			*  - <c>m_vgtHosMaxTessLevel</c>
			*  - <c>m_vgtHosMinTessLevel</c>
			*
			*  Gnm::HullStateConstants
			*  - <c>m_numThreads</c>
			*  - <c>m_numInputCP</c>
			*
			*  The check is deferred until the next draw call.
			*
			*  @param[in] lsb					A pointer to the shader to bind to the LS stage. This value must not be set to <c>NULL</c> if <c><i>hsb</i></c> is non-<c>NULL</c>.
			*  @param[in] shaderModifier		The associated shader modifier value if the shader requires a fetch shader. Specify <c>0</c> if no fetch shader is required.
			*  @param[in] fetchShader			The GPU address of the fetch shader if the LS shader requires a fetch shader. Specify <c>NULL</c> if no fetch shader is required.
			*  @param[in] lsTable				The matching InputResourceOffsets table created by generateShaderResourceOffsetTable() for the LS stage.
			*  @param[in] hsb					A pointer to the shader to bind to the HS stage. This value must not be set to <c>NULL</c> if <c><i>lsb</i></c> is non-<c>NULL</c>.
			*  @param[in] tgPatchCount			The number of patches in the HS shader.
			*  @param[in] hsTable				The matching InputResourceOffsets table created by generateShaderResourceOffsetTable() for the HS stage.
			*
			*  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*lsb</c> and <c>*hsb</c> must not change until the GPU has completed the draw!
			*/
			SCE_GNM_FORCE_INLINE void setLsHsShaders(Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShader, const InputResourceOffsets* lsTable, const Gnmx::HsShader *hsb, uint32_t tgPatchCount, const InputResourceOffsets* hsTable)
			{
				SCE_GNM_ASSERT_MSG_INLINE((lsb && hsb) || (!lsb && !hsb), "lsb (0x%010llX) and hsb (0x%010llX) must either both be NULL, or both be non-NULL.", (unsigned long long)lsb, (unsigned long long)hsb);
				if (lsb && hsb)
				{
					if (hsb->m_hsStageRegisters.isOffChip())
					{
						lsb->m_lsStageRegisters.updateLdsSizeOffChip(&hsb->m_hullStateConstants, lsb->m_lsStride, tgPatchCount);	// set TG LDS size
					}
					else
					{
						lsb->m_lsStageRegisters.updateLdsSize(&hsb->m_hullStateConstants, lsb->m_lsStride, tgPatchCount);	// set TG LDS size
					}
				}

				m_lwcue.setLsShader(lsb, shaderModifier, fetchShader, lsTable);
				m_lwcue.setHsShader(hsb, hsTable, tgPatchCount);
			}

			/** @brief Binds shaders to the LS and HS stages, and updates the LDS size of the LS shader based on the needs of the HS shader.
			*
			*  This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants (which are set in <c><i>hsb</i></c>) or the value of <c><i>tgPatchCount</i></c> differ from current state:
			*
			*  Gnm::HsStageRegisters
			*  - <c>m_vgtTfParam</c>
			*  - <c>m_vgtHosMaxTessLevel</c>
			*  - <c>m_vgtHosMinTessLevel</c>
			*
			*  Gnm::HullStateConstants
			*  - <c>m_numThreads</c>
			*  - <c>m_numInputCP</c>
			*
			*  The check is deferred until the next draw call.
			*
			*  @param[in] lsb					A pointer to the shader to bind to the LS stage. This value must not be set to <c>NULL</c> if <c><i>hsb</i></c> is non-<c>NULL</c>.
			*  @param[in] shaderModifier		The associated shader modifier value if the shader requires a fetch shader. Specify <c>0</c> if no fetch shader is required.
			*  @param[in] fetchShader			The GPU address of the fetch shader if the LS shader requires a fetch shader. Specify <c>NULL</c> if no fetch shader is required.
			*  @param[in] lsTable				The matching InputResourceOffsets table created by generateShaderResourceOffsetTable() for the LS stage.
			*  @param[in] hsb					A pointer to the shader to bind to the HS stage. This must not be set to <c>NULL</c> if <c><i>lsb</i></c> is non-<c>NULL</c>.
			*  @param[in] tgPatchCount			The number of patches in the HS shader.
			*  @param[in] distributionMode		Tessellation distribution mode.
			*  @param[in] hsTable				The matching InputResourceOffsets table created by generateShaderResourceOffsetTable() for the HS stage.
			*
			*  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*lsb</c> and <c>*hsb</c> must not change until the GPU has completed the draw!
			*  @note NEO mode only.
			*/
			SCE_GNM_FORCE_INLINE void setLsHsShaders(Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShader, const InputResourceOffsets* lsTable, const Gnmx::HsShader *hsb, uint32_t tgPatchCount, sce::Gnm::TessellationDistributionMode distributionMode, const InputResourceOffsets* hsTable)
			{
				SCE_GNM_ASSERT_MSG_INLINE((lsb && hsb) || (!lsb && !hsb), "lsb (0x%010llX) and hsb (0x%010llX) must either both be NULL, or both be non-NULL.", (unsigned long long)lsb, (unsigned long long)hsb);
				if (lsb && hsb)
				{
					if (hsb->m_hsStageRegisters.isOffChip())
					{
						lsb->m_lsStageRegisters.updateLdsSizeOffChip(&hsb->m_hullStateConstants, lsb->m_lsStride, tgPatchCount);	// set TG LDS size
					}
					else
					{
						lsb->m_lsStageRegisters.updateLdsSize(&hsb->m_hullStateConstants, lsb->m_lsStride, tgPatchCount);	// set TG LDS size
					}
				}

				m_lwcue.setLsShader(lsb, shaderModifier, fetchShader, lsTable);
				m_lwcue.setHsShader(hsb, hsTable, tgPatchCount, distributionMode);
			}


			/** @brief Binds a VS shader to the VS stage.
			 *
			 *  This function will roll hardware context if any of the following Gnm::VsStageRegisters set for <c><i>vsb</i></c> are different from current state:
			 *  - <c>m_spiVsOutConfig</c>
			 *  - <c>m_spiShaderPosFormat</c>
			 *  - <c>m_paClVsOutCntl</c>
			 * 
			 *  @param[in] vsb					A pointer to the VS shader.
			 *  @param[in] shaderModifier		The shader modifier value generated by generateVsFetchShaderBuildState(). Specify 0 if no fetch shader is required.
			 *  @param[in] fetchShader			The GPU address of the fetch shader if the LS shader requires a fetch shader. Specify <c>NULL</c> if no fetch shader is required.
			 *  @param[in] table				The matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*vsb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setVsShader() is called again, all resource bindings for the stage will need to be rebound.
			 */
			SCE_GNM_FORCE_INLINE void setVsShader(const sce::Gnmx::VsShader* vsb, uint32_t shaderModifier, const void* fetchShader, const InputResourceOffsets* table)
			{
				m_lwcue.setVsShader(vsb, shaderModifier, fetchShader, table);
			}


			/** @brief Binds a VS shader to the VS stage without a fetch shader.
			 *
			 *  This function will roll hardware context if any of the following Gnm::VsStageRegisters set for <c><i>shader</i></c> are different from current state:
			 *  - <c>m_spiVsOutConfig</c>
			 *  - <c>m_spiShaderPosFormat</c>
			 *  - <c>m_paClVsOutCntl</c>
			 * 
			 *  @param[in] vsb					A pointer to the VS shader.
			 *  @param[in] table				The matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*vsb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setVsShader() is called again, all resource bindings for the stage will need to be rebound.
			 */
			SCE_GNM_FORCE_INLINE void setVsShader(const sce::Gnmx::VsShader* vsb, const InputResourceOffsets* table)
			{ 
				m_lwcue.setVsShader(vsb, table); 
			}


			/** @brief Binds a fetch shader separately for the VS shader stage. Use for late fetch shader binding or updating the fetch shader.
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] shaderModified		The shader modifier value generated by generateVsFetchShaderBuildState().
			 *  @param[in] fetchShader			A pointer to the fetch shader.
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c><i>*fetchShader</i></c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 */
			SCE_GNM_FORCE_INLINE void setVsFetchShader(uint32_t shaderModified, const void* fetchShader)
			{
				m_lwcue.setVsFetchShader(shaderModified, fetchShader);
			}


			/** @brief Sets the shader code to be used for the VS shader stage using a shader embedded in the Gnm driver.
			 * This function will roll the hardware context.
			 * 
			 * @param[in] shaderId			The ID of the embedded shader to set.
			 * @param[in] shaderModifier	The shader modifier value generated by generateVsFetchShaderBuildState(); use 0 if there is no fetch shader.
			 *
			 * @note This function will invalidate the existing VS shader state in the LCUE. Therefore, it is required to rebind a pixel shader along with its
			 *		 InputResourceOffsets table for subsequent draw calls.
			 *
			 * @see  LightweightGraphicsConstantUpdateEngine::invalidateShaderStage()
			 */
			SCE_GNM_FORCE_INLINE void setEmbeddedVsShader(Gnm::EmbeddedVsShader shaderId, uint32_t shaderModifier)
			{
				m_dcb.setEmbeddedVsShader(shaderId, shaderModifier);
 				m_lwcue.invalidateShaderStage(Gnm::kShaderStageVs);
			}


			/** @brief Binds a PS shader to the PS stage.
			 *
			 *  This function will roll hardware context if any of the following Gnm::PsStageRegisters set for the shader specified by <c><i>psb</i></c> are different from current state:
			 *  - <c>m_spiShaderZFormat</c>
			 *  - <c>m_spiShaderColFormat</c>
			 *  - <c>m_spiPsInputEna</c>
			 *  - <c>m_spiPsInputAddr</c>
			 *  - <c>m_spiPsInControl</c>
			 *  - <c>m_spiBarycCntl</c>
			 *  - <c>m_dbShaderControl</c>
			 *  - <c>m_cbShaderMask</c>
			 *  
			 *  @param[in] psb					A pointer to the PS shader.
			 *  @param[in] table				The matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*psb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setPsShader() is called again, all resource bindings for the stage will need to be rebound.
			 *  @note Passing <c>NULL</c> to this function does not disable pixel shading, by itself. The psb pointer may safely be <c>NULL</c> only if pixel shader invocation is
			 *        impossible, as is the case under circumstances such as the following:
			 *  	  - color buffer writes have been disabled with <c>setRenderTargetMask(0)</c>.
			 *  	  - a CbMode other than <c>kCbModeNormal</c> is passed to <c>setCbControl()</c>, such as during a hardware MSAA resolve or fast-clear elimination pass.
			 *  	  - the depth test is enabled and <c>kCompareFuncNever</c> is passed to <c>DepthStencilControl::setDepthControl()</c>.
			 *  	  - the stencil test is enabled and <c>kCompareFuncNever</c> is passed to <c>DepthStencilControl::setStencilFunction()</c> and/or
			 *  	  	<c>DepthStencilControl::setStencilFunctionBack()</c>.
			 *  	  - both front and back face culling are enabled with <c>PrimitiveSetup::setCullFace(kPrimitiveSetupCullFaceFrontAndBack)</c>.
			 *  	  - etc.
			 */
			SCE_GNM_FORCE_INLINE void setPsShader(const sce::Gnmx::PsShader* psb, const InputResourceOffsets* table)
			{
				m_lwcue.setPsShader(psb, table);
			}


			/** @brief Sets the shader code to be used for the PS shader stage using a shader embedded in the Gnm driver.
			* This function will roll the hardware context.
			*
			* @param[in] shaderId			The ID of the embedded shader to set.
			*
			* @note This function will invalidate the existing PS shader state in the LCUE. Therefore, it is required to rebind a pixel shader along with its  
			*		InputResourceOffsets table for subsequent draw calls.
			*
			* @see  LightweightGraphicsConstantUpdateEngine::invalidateShaderStage()
			*/
			SCE_GNM_FORCE_INLINE void setEmbeddedPsShader(Gnm::EmbeddedPsShader shaderId)
			{
				m_dcb.setEmbeddedPsShader(shaderId);
				m_lwcue.invalidateShaderStage(Gnm::kShaderStagePs);
			}


			/** @brief Binds a CS shader to the CS stage.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] csb				A pointer to the CS shader.
			 *  @param[in] table			The matching InputResourceOffsets table created by generateShaderResourceOffsetTable().
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*csb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDispatch() is called.
			 *  @note This function must be called first before any resource bindings calls. If setCsShader() is called again, all resource bindings for the stage will need to be rebound.
			 */
			SCE_GNM_FORCE_INLINE void setCsShader(const sce::Gnmx::CsShader* csb, const InputResourceOffsets* table)
			{
				m_lwcue.setCsShader(csb, table);
			}


			/** @brief Binds a shader pair to the asynchronous compute and VS stages for use in a dispatchDraw() call.
			 *
			 *  This function will roll hardware context if any of the following Gnm::VsStageRegisters set for <c><i>csvsb</i></c> are different from current state:
			 *  - <c>m_spiVsOutConfig</c>
			 *  - <c>m_spiShaderPosFormat</c>
			 *  - <c>m_paClVsOutCntl</c>
			 *
			 *  The check is deferred until next draw call.
			 *
			 *  @param[in] csvsb						A pointer to the shaders to bind to the asynchronous compute and VS stages.
			 *  @param[in] shaderModifierVs				The associated shader modifier value if the VS shader requires a fetch shader. Specify 0 if no fetch shader is required.
			 *  @param[in] fetchShaderAddrVs			The GPU address of the fetch shader if the VS shader requires a fetch shader. Specify 0 if no fetch shader is required.
			 *  @param[in] vsTable						The matching InputResourceOffsets table created by generateCsVsShaderResourceOffsetTable().
			 *  @param[in] shaderModifierCs				The associated shader modifier value if the compute shader requires a fetch shader. Specify 0 if no fetch shader is required.
			 *  @param[in] fetchShaderAddrCs			The GPU address of the fetch shader if the compute shader requires a fetch shader. Specify 0 if no fetch shader is required.
			 *  @param[in] csTable						The matching InputResourceOffsets table created by generateCsVsShaderResourceOffsetTable.
			 *
			 *  @note  Setting the asynchronous compute shader does not affect the currently set graphics CS stage shader, which is set by setCsShader().
			 *		   Asynchronous compute shaders share the user data constants and CUE ring elements set up for Gnm::kShaderStageVs.
			 *		   Graphics CS stage shaders use the user data constants and CUE ring elements set up for kShaderStageAsynchronousCompute.
			 *  @note  Only the pointers <c>csvsb->getVertexShader()</c> and <c>csvsb->getComputeShader()</c> are cached inside the LCUE; the location 
			 *		   and contents of these pointers should not be changed before all calls to dispatchDraw() which use this shader have been made!
			 *  @note This binding will not take effect on the GPU until preDispatchDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setCsVsShaders() is called again, all resource bindings 
			 *        for the Vs and kShaderStageAsynchronousCompute stage will need to be rebound.
			 */
			SCE_GNM_LCUE_DISPATCH_DRAW_AVAILABILITY()
			SCE_GNM_FORCE_INLINE void setCsVsShaders(const Gnmx::CsVsShader *csvsb, uint32_t shaderModifierVs, void *fetchShaderAddrVs, const InputResourceOffsets* vsTable,
																					uint32_t shaderModifierCs, void *fetchShaderAddrCs, const InputResourceOffsets* csTable)
			{
				m_lwcue.setCsVsShaders(csvsb, shaderModifierVs, fetchShaderAddrVs, vsTable, shaderModifierCs, fetchShaderAddrCs, csTable);
			}


			/** @brief Binds a shader to the asynchronous compute stage for use in a dispatchDraw() call.
			 *
			 *  Generally, compute shaders for dispatch draw are packed into a CsVsShader and so instead will be set by calling setCsVsShaders().
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] csb					A pointer to the shader to bind to the asynchronous compute stage.
			 *  @param[in] shaderModifierCs		The associated shader modifier value if the compute shader requires a fetch shader. Specify 0 if no fetch shader is required.
			 *  @param[in] fetchShaderAddrCs	The GPU address of the fetch shader if the compute shader requires a fetch shader. Specify 0 if no fetch shader is required.
			 *  @param[in] table				The matching InputResourceOffsets table created by generateCsVsShaderResourceOffsetTable().
			 *
			 *  @note Setting the asynchronous compute shader does not affect the currently set graphics CS stage shader, which is set by setCsShader().
			 *		  Asynchronous compute shaders share the user data constants and CUE ring elements set up for Gnm::kShaderStageVs.
			 *		  Graphics CS stage shaders use the user data constants and CUE ring elements set up for Gnm::kShaderStageCs.
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*csb</c> must not change until the GPU has completed the draw!
			 *  @note This binding will not take effect on the GPU until preDispatchDraw() is called.
			 *  @note This function must be called first before any resource bindings calls. If setAsynchronousComputeShader() is called again, all resource bindings 
			 *        for the kShaderStageAsynchronousCompute stage will need to be rebound.
			 */
			SCE_GNM_LCUE_DISPATCH_DRAW_AVAILABILITY()
			SCE_GNM_FORCE_INLINE void setAsynchronousComputeShader(const Gnmx::CsShader *csb, uint32_t shaderModifierCs, void *fetchShaderAddrCs, const InputResourceOffsets* table)
			{
				m_lwcue.setAsynchronousComputeShader(csb, shaderModifierCs, fetchShaderAddrCs, table);
			}

			///////////// Resource binding commands

			/** @brief Sets the layout of LDS area where data will flow from the ES to the GS stages when on-chip geometry shading is enabled.
			 *  This sets the same context register state as <c>setEsGsRingBuffer(NULL, 0, maxExportVertexSizeInDword)</c>, but does not modify the global resource table.
			 *  This function will roll hardware context.
			 *
			 *  @param[in] maxExportVertexSizeInDword		The stride of an ES-GS vertex in <c>DWORD</c>s, which must match EsShader::m_maxExportVertexSizeInDword.
			 *
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 *  @note This binding requires setOnChipEsShader() be called prior to binding.
			 */
			SCE_GNM_FORCE_INLINE void setOnChipEsGsLdsLayout(uint32_t maxExportVertexSizeInDword)
			{
				m_lwcue.setOnChipEsGsLdsLayout(maxExportVertexSizeInDword);
			}

		
			/** @brief Binds one or more streamout buffer objects to the specified shader stage.
			 *
			 * This function never rolls the hardware context.
			 *
			 *  @param[in] startSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxStreamOutBufferCount-1]</c>.     
			 *  @param[in] numSlots			The number of consecutive API slots to bind.
			 *  @param[in] buffers			The buffer objects to bind to the specified slots.
			 *							<c>buffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>buffers[1]</c> to <c>startSlot+1</c> and so on. 
			 *							The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
			 *  @note This binding will not take effect on the GPU until preDraw() is called.
			 */
			SCE_GNM_FORCE_INLINE void setStreamoutBuffers(int32_t startSlot, int32_t numSlots, const sce::Gnm::Buffer* buffers)
			{
				m_lwcue.setStreamoutBuffers(startSlot, numSlots, buffers);
			}


			/** @brief Specifies a range of the Global Data Store to be used by shaders for atomic global counters such as those
			 *  used to implement PSSL <c>AppendRegularBuffer</c> and <c>ConsumeRegularBuffer</c> objects.
			 *
			 *  Each counter is a 32-bit integer. The counters for each shader stage may have a different offset in GDS. For example:
			 *  @code
			 *     setAppendConsumeCounterRange(kShaderStageVs, 0x0100, 12) // Set up 3 counters for the VS stage starting at offset 0x100.
			 *     setAppendConsumeCounterRange(kShaderStageCs, 0x0400, 4)  // Set up 1 counter for the CS stage at offset 0x400.
			 *	@endcode
			 *
			 *  The index is defined by the chosen slot in the PSSL shader. For example:
			 *  @code
			 *     AppendRegularBuffer<uint> appendBuf : register(u3) // Will access the 4th counter starting at the base offset provided to this function.
			 *  @endcode
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] shaderStage				The shader stage to bind this counter range to.
			 *  @param[in] gdsMemoryBaseInBytes		The byte offset to the start of the counters in GDS. This must be a multiple of 4.
			 *  @param[in] countersSizeInBytes		The size of the counter range in bytes. This must be a multiple of 4.
			 *
			 *  @note GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes. 
			 */
			SCE_GNM_FORCE_INLINE void setAppendConsumeCounterRange(sce::Gnm::ShaderStage shaderStage, uint32_t gdsMemoryBaseInBytes, uint32_t countersSizeInBytes)
			{
				m_lwcue.setAppendConsumeCounterRange(shaderStage, gdsMemoryBaseInBytes, countersSizeInBytes);
			}


			/** @brief Specifies a range of the Global Data Store to be used by shaders.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] shaderStage			The shader stage to bind this range to.
			 *  @param[in] gdsMemoryBaseInBytes	The byte offset to the start of the range in GDS. This must be a multiple of 4. 
			 *  @param[in] countersSizeInBytes	The size of the counter range in bytes. This must be a multiple of 4.
			 *
			 *  @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes. It is an error to specify a range outside these bounds.
			 */
			SCE_GNM_FORCE_INLINE void setGdsMemoryRange(sce::Gnm::ShaderStage shaderStage, uint32_t gdsMemoryBaseInBytes, uint32_t countersSizeInBytes)
			{
				m_lwcue.setGdsMemoryRange(shaderStage, gdsMemoryBaseInBytes, countersSizeInBytes);
			}


			/** @brief Binds one or more constant buffer objects to the specified shader stage.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] stage			The resource(s) will be bound to this shader stage.
			 *  @param[in] startSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxConstantBufferCount-1]</c>.
			 *  @param[in] numSlots			The number of consecutive API slots to bind.
			 *  @param[in] buffers			The constant buffer objects to bind to the specified slots.
			 *							<c>buffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>buffers[1]</c> to <c>startSlot+1</c> and so on.
			 *							The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
			 */
			SCE_GNM_FORCE_INLINE void setConstantBuffers(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Buffer *buffers)
			{
				return m_lwcue.setConstantBuffers(stage, startSlot, numSlots, buffers);
			}


			/** @brief Binds one or more vertex buffer objects to the specified shader stage.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] stage			The resource(s) will be bound to this shader stage.
			 *  @param[in] startSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxVertexBufferCount-1]</c>.
			 *  @param[in] numSlots			The number of consecutive API slots to bind.
			 *  @param[in] buffers			The vertex buffer objects to bind to the specified slots.
			 *							<c>buffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>buffers[1]</c> to <c>startSlot+1</c> and so on.
			 *							The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
			 */
			SCE_GNM_FORCE_INLINE void setVertexBuffers(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Buffer *buffers)
			{
				return m_lwcue.setVertexBuffers(stage, startSlot, numSlots, buffers);
			}


			/** @brief Binds one or more read-only buffer objects to the specified shader stage.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] stage		The resource(s) will be bound to this shader stage.
			 *  @param[in] startSlot	The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxResourceCount-1]</c>.
			 *  @param[in] numSlots		The number of consecutive API slots to bind.
			 *  @param[in] buffers		The buffer objects to bind to the specified slots.
			 *						<c>buffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>buffers[1]</c> to <c>startSlot+1</c> and so on.
			 *						The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
			 *
			 *	@note Buffers and Textures share the same pool of API slots.
			 */
			SCE_GNM_FORCE_INLINE void setBuffers(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Buffer *buffers)
			{
				return m_lwcue.setBuffers(stage, startSlot, numSlots, buffers);
			}


			/** @brief Binds one or more read/write buffer objects to the specified shader stage.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] stage		The resource(s) will be bound to this shader stage.
			 *  @param[in] startSlot	The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxRwResourceCount-1]</c>.
			 *  @param[in] numSlots		The number of consecutive API slots to bind.
			 *  @param[in] rwBuffers	The read/write buffer objects to bind to the specified slots.
			 *						<c>rwBuffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>rwBuffers[1]</c> to <c>startSlot+1</c> and so on.
			 *						The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
			 *
			 *  @note Read/write buffers and read/write textures share the same pool of API slots.
			 */
			SCE_GNM_FORCE_INLINE void setRwBuffers(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Buffer *rwBuffers)
			{
				return m_lwcue.setRwBuffers(stage, startSlot, numSlots, rwBuffers);
			}


			/** @brief Binds one or more read-only texture objects to the specified shader stage.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] stage		The resource(s) will be bound to this shader stage.
			 *  @param[in] startSlot	The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxResourceCount-1]</c>.
			 *  @param[in] numSlots		The number of consecutive API slots to bind.
			 *  @param[in] textures		The texture objects to bind to the specified slots.
			 *						<c>textures[0]</c> will be bound to <c><i>startSlot</i></c>, <c>textures[1]</c> to <c>startSlot+1</c> and so on.
			 *						The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
			 *
			 *  @note Buffers and Textures share the same pool of API slots.
			 */
			SCE_GNM_FORCE_INLINE void setTextures(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Texture *textures)
			{
				return m_lwcue.setTextures(stage, startSlot, numSlots, textures);
			}


			/** @brief Binds one or more read/write texture objects to the specified shader stage.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] stage		The resource(s) will be bound to this shader stage.
			 *  @param[in] startSlot	The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxRwResourceCount-1]</c>.
			 *  @param[in] numSlots		The number of consecutive API slots to bind.
			 *  @param[in] rwTextures	The read/write texture objects to bind to the specified slots.
			 *						<c>rwTextures[0]</c> will be bound to <c><i>startSlot</i></c>, <c>texture[1]</c> to <c>startSlot+1</c> and so on.
			 *						The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
			 *
			 *  @note Read/write buffers and read/write textures share the same pool of API slots.
			 */
			SCE_GNM_FORCE_INLINE void setRwTextures(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Texture *rwTextures)
			{
				return m_lwcue.setRwTextures(stage, startSlot, numSlots, rwTextures);
			}


			/** @brief Binds one or more sampler objects to the specified shader stage.
 			 *
			 * This function never rolls the hardware context.
			 *
			 *  @param[in] stage	The resource(s) will be bound to this shader stage.
			 *  @param[in] startSlot	The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxSamplerCount-1]</c>.
			 *  @param[in] numSlots		The number of consecutive API slots to bind.
			 *  @param[in] samplers		The sampler objects to bind to the specified slots. 
			 *						<c>samplers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>samplers[1]</c> to <c>startSlot+1</c> and so on. 
			 *						The contents of these Sampler objects are cached locally inside the LCUE's scratch buffer.
			 */
			SCE_GNM_FORCE_INLINE void setSamplers(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Sampler *samplers)
			{
				return m_lwcue.setSamplers(stage, startSlot, numSlots, samplers);
			}


			/** @brief Binds a user SRT buffer.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] shaderStage	The shader stage to bind the SRT buffer to.
			 *  @param[in] buffer		A pointer to the buffer. If this argument is set to <c>NULL</c>, <c><i>sizeInDwords</i></c> must be set to 0.
			 *  @param[in] sizeInDwords	The size of the data pointed to by <c><i>buffer</i></c> in <c>DWORD</c>s. The valid range is <c>[1..kMaxSrtUserDataCount]</c> if <c><i>buffer</i></c> is non-<c>NULL</c>.
			 */
			SCE_GNM_FORCE_INLINE void setUserSrtBuffer(sce::Gnm::ShaderStage shaderStage, const void* buffer, uint32_t sizeInDwords)
			{
				m_lwcue.setUserSrtBuffer(shaderStage, buffer, sizeInDwords);
			}


			/** @brief Sets the address to the global resource table. This is a collection of V#'s which point to global buffers for various shader tasks.
			 *
			 *  @param[in] globalResourceTableAddr	A pointer to the global resource table in memory.
			 *
			 *  @note The address specified in <c><i>globalResourceTableAddr</i></c> needs to be in GPU-visible memory and must be set before calling setGlobalDescriptor().
			 */
			SCE_GNM_FORCE_INLINE void setGlobalResourceTableAddr(void* globalResourceTableAddr)
			{
				m_lwcue.setGlobalResourceTableAddr(globalResourceTableAddr);
			}


			/** @brief Sets an entry in the global resource table.
			 * 
			 * This function never rolls the hardware context.
			 * @param[in] resType The global resource type to bind a buffer for. Each global resource type has its own entry in the global resource table.
			 * @param[in] res The buffer to bind to the specified entry in the global resource table. The size of the buffer is global-resource-type-specific.
			 *
			 * @note This function modifies the global resource table. It is not safe to modify the global resource table unless the GPU is idle.
			 */
			SCE_GNM_FORCE_INLINE void setGlobalDescriptor(Gnm::ShaderGlobalResourceType resType, const Gnm::Buffer *res)
			{
				return m_lwcue.setGlobalDescriptor(resType, res);
			}


#if !defined(DOXYGEN_IGNORE)
			SCE_GNM_LCUE_NOT_SUPPORTED void setBoolConstants(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const uint32_t *bits)
			{
				// Not implemented yet
				SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(startSlot); SCE_GNM_UNUSED(numSlots); SCE_GNM_UNUSED(bits);
			}

			SCE_GNM_LCUE_NOT_SUPPORTED void setFloatConstants(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const float *floats)
			{
				// Not implemented yet
				SCE_GNM_UNUSED(stage); SCE_GNM_UNUSED(startSlot); SCE_GNM_UNUSED(numSlots); SCE_GNM_UNUSED(floats);
			}
#endif	// !defined(DOXYGEN_IGNORE)

			//////////// Draw commands

			/** @brief Inserts a draw call using auto generated indices.
			 *
			 *	Draw commands never roll the hardware context; instead they use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@param[in] indexCount  The index up to which to auto generate the indices.
			 *	@see Gnm::DrawCommandBuffer::setPrimitiveType()
			 */
			SCE_GNM_FORCE_INLINE void drawIndexAuto(uint32_t indexCount)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexAuto can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_lwcue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");

				m_lwcue.preDraw();
				m_dcb.drawIndexAuto(indexCount, Gnm::kDrawModifierDefault);
			}


			/** @brief Inserts a draw call using auto generated indices while adding an offset to the vertex and instance indices.
			 *
			 *  Draw commands never roll the hardware context; instead they use the current context such that the next command that sets context state will roll the context.
			 *
			 *  @param[in] indexCount		The index up to which to auto generate the indices.
			 *  @param[in] vertexOffset		The offset added to each vertex index.
			 *	@param[in] instanceOffset	The offset added to each instance index.
			 *  @param[in] modifier			A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			 *
			 *	@see Gnm::DrawCommandBuffer::setPrimitiveType()
			 */
			SCE_GNM_FORCE_INLINE void drawIndexAuto(uint32_t indexCount, uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexAuto can't be called between beginDispatchDraw and endDispatchDraw");
				m_lwcue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);
					
				m_lwcue.preDraw();
				m_dcb.drawIndexAuto(indexCount, modifier);
			}
			SCE_GNM_API_CHANGED	void drawIndexAuto(uint32_t indexCount, uint32_t vertexOffset, uint32_t instanceOffset)
			{
				return drawIndexAuto(indexCount, vertexOffset, instanceOffset, Gnm::kDrawModifierDefault);
			}


			/** @brief Inserts a draw call using streamout output.
			 *	
			 *	Draw commands never roll the hardware context, but use the current context such that the next command that sets context state will roll context.
			 *	@note In NEO mode this function is incompatible with the kWdSwitchOnlyOnEopDisable in setVgtControlForNeo().
			 *	
			 *	@see Gnm::DrawCommandBuffer::setPrimitiveType()
			 */
			SCE_GNM_FORCE_INLINE void drawOpaque()
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawOpaque can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_lwcue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");

				m_lwcue.preDraw();
				m_dcb.drawOpaqueAuto(Gnm::kDrawModifierDefault);
			}


			/** @brief Inserts a draw call using streamout output while adding an offset to the vertex and instance indices.
			 *
			 *	Draw commands never roll the hardware context, but use the current context such that the next command that sets context state will roll context.
			 *
			 *	@param[in] vertexOffset		The offset added to each vertex index.
			 *	@param[in] instanceOffset	The offset added to each instance index.
			 *  @param[in] modifier			A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			 *
			 *	@see Gnm::DrawCommandBuffer::setPrimitiveType()
			 */
			SCE_GNM_FORCE_INLINE void drawOpaque(uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawOpaque can't be called between beginDispatchDraw and endDispatchDraw");
				m_lwcue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);

				m_lwcue.preDraw();
				m_dcb.drawOpaqueAuto(modifier);
			}
			SCE_GNM_API_CHANGED
			void drawOpaque(uint32_t vertexOffset, uint32_t instanceOffset)
			{
				return drawOpaque(vertexOffset, instanceOffset, Gnm::kDrawModifierDefault);
			}


			/** @brief Inserts a draw call using specified indices that are inserted into the command buffer.
			 *
			 *	Draw commands never roll the hardware context; instead they use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@param[in] indexCount  Number of indices to insert. The maximum count is either 32764 (for 16-bit indices) or 16382 (for 32-bit indices).
			 *	@param[in] indices     Pointer to first index in buffer containing <c><i>indexCount</i></c> indices. Pointer must be 4-byte aligned.
			 *	@param[in] indicesSizeInBytes Size of the buffer pointed to by <c><i>indices</i></c>, expressed as a number of bytes. To specify the size of an individual index, use setIndexSize().
			 *                                The valid range is [0..65528] bytes.
			 *
			 *	@see Gnm::DrawCommandBuffer::setPrimitiveType(), Gnm::DrawCommandBuffer::setIndexSize()
			 */
			SCE_GNM_FORCE_INLINE void drawIndexInline(uint32_t indexCount, const void *indices, uint32_t indicesSizeInBytes)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexInline can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_lwcue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");

				m_lwcue.preDraw();
				m_dcb.drawIndexInline(indexCount, indices, indicesSizeInBytes, Gnm::kDrawModifierDefault);
			}


			/** @brief Inserts a draw call using provided indices, which are inserted into the command buffer while adding an offset to each of the vertex and instance indexes.
			 *
			 *	Draw commands never roll the hardware context; instead they use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@param[in] indexCount			Number of indices to insert.The maximum count is either 32764 (for 16-bit indices) or 16382 (for 32-bit indices).
			 *	@param[in] indices				Pointer to first index in buffer containing <c><i>indexCount</i></c> indices. Pointer must be 4-byte aligned.
			 *	@param[in] indicesSizeInBytes	Size (in bytes) of the <c><i>indices</i></c> buffer. To specify the size of an individual index, use setIndexSize().The valid range is <c>[0..65528]</c> bytes.
			 *	@param[in] vertexOffset			Offset added to each vertex index.
			 *	@param[in] instanceOffset		Offset added to each instance index.
			 *  @param[in] modifier				A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			 *
			 *	@see Gnm::DrawCommandBuffer::setPrimitiveType(), Gnm::DrawCommandBuffer::setIndexSize()
			 */
			SCE_GNM_FORCE_INLINE void drawIndexInline(uint32_t indexCount, const void *indices, uint32_t indicesSizeInBytes, uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexInline can't be called between beginDispatchDraw and endDispatchDraw");
				m_lwcue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);

				m_lwcue.preDraw();
				m_dcb.drawIndexInline(indexCount, indices, indicesSizeInBytes, modifier);
			}
			SCE_GNM_API_CHANGED
			void drawIndexInline(uint32_t indexCount, const void *indices, uint32_t indicesSizeInBytes, uint32_t vertexOffset, uint32_t instanceOffset)
			{
				return drawIndexInline(indexCount, indices, indicesSizeInBytes, vertexOffset, instanceOffset, Gnm::kDrawModifierDefault);
			}


			/** @brief Inserts a draw call using indices which are located in memory.
			 *
			 *	Draw commands never roll the hardware context; instead they use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@param[in] indexCount	The number of indices to insert.
			 *	@param[in] indexAddr	The GPU address of the index buffer.
			 *
			 *	@see Gnm::DrawCommandBuffer::setPrimitiveType(), Gnm::DrawCommandBuffer::setIndexSize()
			 */
			SCE_GNM_FORCE_INLINE void drawIndex(uint32_t indexCount, const void *indexAddr)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndex can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_lwcue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");

				m_lwcue.preDraw();
				m_dcb.drawIndex(indexCount, indexAddr, Gnm::kDrawModifierDefault);
			}

			/** @brief Using indices located in memory, inserts a draw call while adding an offset to the vertex and instance indices.
			 *
			 *	Draw commands never roll the hardware context; instead they use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@param[in] indexCount		The number of indices to insert.
			 *	@param[in] indexAddr		The GPU address of the index buffer.
			 *	@param[in] vertexOffset		The offset to add to each vertex index.
			 *	@param[in] instanceOffset	The offset to add to each instance index.
			 *  @param[in] modifier			A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			 *
			 *	@see Gnm::DrawCommandBuffer::setPrimitiveType(), Gnm::DrawCommandBuffer::setIndexSize()
			 */
			SCE_GNM_FORCE_INLINE void drawIndex(uint32_t indexCount, const void *indexAddr, uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndex can't be called between beginDispatchDraw and endDispatchDraw");
				m_lwcue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);

				m_lwcue.preDraw();
				m_dcb.drawIndex(indexCount, indexAddr, modifier);
			}
			SCE_GNM_API_CHANGED
			void drawIndex(uint32_t indexCount, const void *indexAddr, uint32_t vertexOffset, uint32_t instanceOffset)
			{
				return drawIndex(indexCount, indexAddr, vertexOffset, instanceOffset, Gnm::kDrawModifierDefault);
			}

			/** @brief Inserts an instanced, indexed draw call that sources object ID values from the <c>objectIdAddr</c> buffer.
				@param[in] indexCount		The number of entries to process from the index buffer.
				@param[in] instanceCount    The number of instances to render.
				@param[in] indexAddr		The address of the index buffer. Do not set to <c>NULL</c>.
				@param[in] objectIdAddr     The address of the object ID buffer. Do not set to <c>NULL</c>.
				@note  NEO mode only
				@note instanceCount has to match the number of instances set with the setNumInstances().
			*/
			SCE_GNM_FORCE_INLINE void drawIndexMultiInstanced(uint32_t indexCount, uint32_t instanceCount, const void *indexAddr, const void *objectIdAddr)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexMultiInstanced can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_lwcue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");

				m_lwcue.preDraw();
				m_dcb.drawIndexMultiInstanced(indexCount, instanceCount, indexAddr, objectIdAddr, Gnm::kDrawModifierDefault);
			}

			/** @brief Inserts an instanced, indexed draw call that sources object ID values from the <c>objectIdAddr</c> buffer.
				@param[in] indexCount		The number of entries to process from the index buffer.
				@param[in] instanceCount    The number of instances to render.
				@param[in] indexAddr		The address of the index buffer. Do not set to <c>NULL</c>.
				@param[in] objectIdAddr     The address of the object ID buffer. Do not set to <c>NULL</c>.
				@param[in] vertexOffset		The offset to add to each vertex index.
				@param[in] instanceOffset	The offset to add to each instance index.
				@param[in] modifier			A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				@note  NEO mode only
				@note instanceCount has to match the number of instances set with the setNumInstances().
			*/
			SCE_GNM_FORCE_INLINE void drawIndexMultiInstanced(uint32_t indexCount, uint32_t instanceCount, const void *indexAddr, const void *objectIdAddr, uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexMultiInstanced can't be called between beginDispatchDraw and endDispatchDraw");
				m_lwcue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);

				m_lwcue.preDraw();
				m_dcb.drawIndexMultiInstanced(indexCount, instanceCount, indexAddr, objectIdAddr, modifier);
			}

			/** @brief Using indices located in memory and previously set values for base, size and element size, inserts a draw call while adding an offset to the supplied vertex and instance indices.
			 *
			 *	Draw commands never roll the hardware context; instead they use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@param[in] indexOffset	The starting index number in the index buffer.
			 *	@param[in] indexCount	The number of indices to insert.
			 *
			 *	@see Gnm::DrawCommandBuffer::setIndexBuffer(), Gnm::DrawCommandBuffer::setIndexCount(), Gnm::DrawCommandBuffer::setIndexSize()
			 */
			SCE_GNM_FORCE_INLINE void drawIndexOffset(uint32_t indexOffset, uint32_t indexCount)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexOffset can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_lwcue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");

				m_lwcue.preDraw();
				m_dcb.drawIndexOffset(indexOffset, indexCount, Gnm::kDrawModifierDefault);
			}


			/** @brief Using indices located in memory and previously set values for base, size and element size, inserts a draw call while adding an offset to the supplied vertex and instance indices.
			 *
			 *	Draw commands never roll the hardware context, but use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@param[in] indexOffset		The starting index number in the index buffer.
			 *	@param[in] indexCount		The number of indices to insert.
			 *	@param[in] vertexOffset		The offset to add to each vertex index.
			 *	@param[in] instanceOffset	The offset to add to each instance index.
			 *  @param[in] modifier			A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			 *
			 *	@see Gnm::DrawCommandBuffer::setIndexBuffer(), Gnm::DrawCommandBuffer::setIndexCount(), Gnm::DrawCommandBuffer::setIndexSize()
			 */
			SCE_GNM_FORCE_INLINE void drawIndexOffset(uint32_t indexOffset, uint32_t indexCount, uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexOffset can't be called between beginDispatchDraw and endDispatchDraw");
				m_lwcue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);

				m_lwcue.preDraw();
				m_dcb.drawIndexOffset(indexOffset, indexCount, modifier);
			}


			SCE_GNM_API_CHANGED
			void drawIndexOffset(uint32_t indexOffset, uint32_t indexCount, uint32_t vertexOffset, uint32_t instanceOffset)
			{
				return drawIndexOffset(indexOffset, indexCount, vertexOffset, instanceOffset, Gnm::kDrawModifierDefault);
			}


			/** @brief Inserts an indirect draw call, which reads its parameters from a specified address in GPU memory.
			 *
			 *	Draw commands never roll the hardware context; instead they use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@param[in] dataOffsetInBytes	The offset (in bytes) into the indirect arguments buffer. This function reads arguments from the buffer at this offset.
			 *									The data at this offset must be a Gnm::DrawIndirectArgs structure.
			 *  @param[in] modifier				A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			 *
			 *	@note The buffer containing the indirect arguments should already have been set using setBaseIndirectArgs().
			 *  @note The <c><i>instanceCount</i></c> value specified in the DrawIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			 *        is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
			 *
			 *	@see Gnm::DrawIndirectArgs, Gnm::DrawCommandBuffer::setBaseIndirectArgs()
			 */
			SCE_GNM_FORCE_INLINE void drawIndirect(uint32_t dataOffsetInBytes, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndirect can't be called between beginDispatchDraw and endDispatchDraw");
				// no need to set vertex and instance offsets here, they are coming from the draw indirect structure.
				m_lwcue.preDraw();

				switch (m_lwcue.getActiveShaderStages())
				{
					case sce::Gnm::kActiveShaderStagesVsPs:
					{
						const Gnmx::VsShader *pvs = (const Gnmx::VsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageVs);
						m_dcb.drawIndirect(dataOffsetInBytes, Gnm::kShaderStageVs, (uint8_t)pvs->getVertexOffsetUserRegister(), (uint8_t)pvs->getInstanceOffsetUserRegister(), modifier);
						break;
					}
					case sce::Gnm::kActiveShaderStagesEsGsVsPs:
					{
						const Gnmx::EsShader *pes = (const Gnmx::EsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageEs);
						m_dcb.drawIndirect(dataOffsetInBytes, Gnm::kShaderStageEs, (uint8_t)pes->getVertexOffsetUserRegister(), (uint8_t)pes->getInstanceOffsetUserRegister(), modifier);
						break;
					}
					case sce::Gnm::kActiveShaderStagesLsHsVsPs:
					case sce::Gnm::kActiveShaderStagesLsHsEsGsVsPs:
					case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
					case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
					{
						const Gnmx::LsShader *pls = (const Gnmx::LsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageLs);
						m_dcb.drawIndirect(dataOffsetInBytes, Gnm::kShaderStageLs, (uint8_t)pls->getVertexOffsetUserRegister(), (uint8_t)pls->getInstanceOffsetUserRegister(), modifier);
						break;
					}
					case sce::Gnm::kActiveShaderStagesDispatchDrawVsPs:
					{
						SCE_GNM_ERROR_INLINE("drawIndirect does not support DispatchDaw");
						break;
					}
					default:
					{
						SCE_GNM_ERROR_INLINE("Unknown active shader stage is set, please ensure you have called setActiveShaderStages() prior to draw");
						break;
					}
				}
			}
			SCE_GNM_FORCE_INLINE void drawIndirect(uint32_t dataOffsetInBytes)
			{
				return drawIndirect(dataOffsetInBytes, Gnm::kDrawModifierDefault);
			}

			/** @brief Inserts an indirect multi-draw call that optionally reads the draw count from GPU memory, and which has a similar effect to calling drawIndirect() several times
			 *		   with multiple DrawIndirectArgs objects packed tightly in memory.
			 *
			 *  Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll context.
			 *
			 *	Each draw reads a new DrawIndirectArgs object from the indirect arguments buffer.
			 *
			 *  @param[in] dataOffsetInBytes		The offset (in bytes) into the buffer that contains the first draw call's indirect arguments, which are set using setBaseIndirectArgs().
			 *										The data at this offset must be a Gnm::DrawIndirectArgs structure.
			 *	@param[in] count					If <c><i>countAddress</i></c> is <c>NULL</c>, this defines the number of draw calls to launch. If <c><i>countAddress</i></c> is non-<c>NULL</c>,
			 *										then this value will be used to clamp the draw count read from <c><i>countAddress</i></c>. 
			 *	@param[in] countAddress				If non-<c>NULL</c>, this is a <c>DWORD</c>-aligned address from which  the GPU will read the draw count, prior to clamping using the value provided in <c>count</c>.
			 *
			 *	@note The buffer containing the indirect arguments should already have been set using setBaseIndirectArgs().
			 *  @note The <c>instanceCount</c> value specified in the DrawIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			 *        is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
			 *
			 *	@see Gnm::DrawIndexIndirectArgs, Gnm::DrawCommandBuffer::setBaseIndirectArgs()
			 */
			SCE_GNM_FORCE_INLINE void drawIndirectCountMulti(uint32_t dataOffsetInBytes, uint32_t count, void *countAddress)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndirectCountMulti can't be called between beginDispatchDraw and endDispatchDraw");
				// no need to set vertex and instance offsets here, they are coming from the draw indirect structure.
				m_lwcue.preDraw();

				switch (m_lwcue.getActiveShaderStages())
				{
					case sce::Gnm::kActiveShaderStagesVsPs:
					{
						const Gnmx::VsShader *pvs = (const Gnmx::VsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageVs);
						m_dcb.drawIndirectCountMulti(dataOffsetInBytes, count, countAddress, Gnm::kShaderStageVs, (uint8_t)pvs->getVertexOffsetUserRegister(), (uint8_t)pvs->getInstanceOffsetUserRegister(), Gnm::kDrawModifierDefault);
						break;
					}
					case sce::Gnm::kActiveShaderStagesEsGsVsPs:
					{
						const Gnmx::EsShader *pes = (const Gnmx::EsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageEs);
						m_dcb.drawIndirectCountMulti(dataOffsetInBytes, count, countAddress, Gnm::kShaderStageEs, (uint8_t)pes->getVertexOffsetUserRegister(), (uint8_t)pes->getInstanceOffsetUserRegister(), Gnm::kDrawModifierDefault);
						break;
					}
					case sce::Gnm::kActiveShaderStagesLsHsVsPs:
					case sce::Gnm::kActiveShaderStagesLsHsEsGsVsPs:
					case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
					case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
					{
						const Gnmx::LsShader *pls = (const Gnmx::LsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageLs);
						m_dcb.drawIndirectCountMulti(dataOffsetInBytes, count, countAddress, Gnm::kShaderStageLs, (uint8_t)pls->getVertexOffsetUserRegister(), (uint8_t)pls->getInstanceOffsetUserRegister(), Gnm::kDrawModifierDefault);
						break;
					}
					case sce::Gnm::kActiveShaderStagesDispatchDrawVsPs:
					{
						SCE_GNM_ERROR_INLINE("drawIndexIndirectCountMulti does not support DispatchDaw");
						break;
					}
					default:
					{
						SCE_GNM_ERROR_INLINE("Unknown active shader stage is set, please ensure you have called setActiveShaderStages() prior to draw");
						break;
					}
				}
			}


			/** @deprecated Please use drawIndirectCountMulti() instead.
			 *	@brief Inserts an indirect multi-draw call that optionally reads the draw count from GPU memory, and which has a similar effect to calling drawIndirect() several times
			 *		   with multiple DrawIndirectArgs objects packed tightly in memory.
			 *
			 *  Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll context.
			 *
			 *	Each draw reads a new DrawIndirectArgs object from the indirect arguments buffer.
			 *
			 *  @param[in] dataOffsetInBytes		The offset (in bytes) into the buffer that contains the first draw call's indirect arguments, which are set using setBaseIndirectArgs().
			 *										The data at this offset must be a Gnm::DrawIndirectArgs structure.
			 *	@param[in] count					If <c><i>countAddress</i></c> is <c>NULL</c>, the value of the <c>count</c> parameter defines the number of draw calls to launch. If <c>countAddress</c> is non-NULL,
			 *										then the value of the <c>count</c> parameter will be used to clamp the draw count read from <c><i>countAddress</i></c>. 
			 *	@param[in] countAddress				If non-<c>NULL</c>, this is a <c>DWORD</c>-aligned address from which the GPU will read the draw count, prior to clamping using the value provided in <c>count</c>.
			 *  @param[in] stage					The shader stage that contains the fetch shader.
			 *  @param[in] vertexOffsetUserSgpr		The user SGRPR from which the fetch shader is expecting the vertex offset, or <c>0</c> if not used.
			 *  @param[in] instanceOffsetUserSgpr	The user SGPR from which the fetch shader is expecting the instance offset, or <c>0</c> if not used.
			 *  @param[in] modifier					A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			 *
			 *  @note The buffer containing the indirect arguments should already have been set using setBaseIndirectArgs().
			 *  @note The <c>instanceCount</c> value specified in the DrawIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			 *        is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
			 *
			 *  @see Gnm::DrawIndirectArgs, Gnm::DrawCommandBuffer::setBaseIndirectArgs()
			 */
			SCE_GNM_FORCE_INLINE void drawIndirectCountMulti(uint32_t dataOffsetInBytes, uint32_t count, void *countAddress, Gnm::ShaderStage stage, uint8_t vertexOffsetUserSgpr, uint8_t instanceOffsetUserSgpr, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndirectCountMulti can't be called between beginDispatchDraw and endDispatchDraw");
				// no need to set vertex and instance offsets here, they are coming from the draw indirect structure.
				m_lwcue.preDraw();
				m_dcb.drawIndirectCountMulti(dataOffsetInBytes, count, countAddress, stage, vertexOffsetUserSgpr, instanceOffsetUserSgpr, modifier);
			}


			/** @brief Inserts an indirect drawIndex call, which reads its parameters from a specified address in the GPU memory.
			 *
			 *	Draw commands never roll the hardware context; instead they use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@param[in] dataOffsetInBytes	The offset (in bytes) into the buffer that contains the indirect arguments, which is set using setBaseIndirectArgs().
			 *									The data at this offset must be a Gnm::DrawIndexIndirectArgs structure.
			 *
			 *	@note	The index buffer should already have been set up using setIndexBuffer() and setIndexCount(),
			 *			and the buffer containing the indirect arguments should have been set using setBaseIndirectArgs().
			 *			The index count in <c>args->m_indexCountPerInstance</c> will be clamped to the value passed to setIndexCount().
			 *  @note The <c><i>instanceCount</i></c> value specified in the DrawIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			 *        is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
			 *
			 *	@see Gnm::DrawIndexIndirectArgs, Gnm::DrawCommandBuffer::setBaseIndirectArgs(), Gnm::DrawCommandBuffer::setIndexBuffer(), Gnm::DrawCommandBuffer::setIndexCount()
			 */
			SCE_GNM_FORCE_INLINE void drawIndexIndirect(uint32_t dataOffsetInBytes)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexIndirect can't be called between beginDispatchDraw and endDispatchDraw");
				// no need to set vertex and instance offsets here, they are coming from the draw indirect structure.
				m_lwcue.preDraw();

				switch (m_lwcue.getActiveShaderStages())
				{
					case sce::Gnm::kActiveShaderStagesVsPs:
					{
						const Gnmx::VsShader *pvs = (const Gnmx::VsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageVs);
						m_dcb.drawIndexIndirect(dataOffsetInBytes, Gnm::kShaderStageVs, pvs->m_fetchControl&0xf, (pvs->m_fetchControl>>4)&0xf, Gnm::kDrawModifierDefault);
						break;
					}
					case sce::Gnm::kActiveShaderStagesEsGsVsPs:
					{
						const Gnmx::EsShader *pes = (const Gnmx::EsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageEs);
						m_dcb.drawIndexIndirect(dataOffsetInBytes, Gnm::kShaderStageEs, pes->m_fetchControl&0xf, (pes->m_fetchControl>>4)&0xf, Gnm::kDrawModifierDefault);
						break;
					}
					case sce::Gnm::kActiveShaderStagesLsHsVsPs:
					case sce::Gnm::kActiveShaderStagesLsHsEsGsVsPs:
					case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
					case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
					{
						const Gnmx::LsShader *pls = (const Gnmx::LsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageLs);
						m_dcb.drawIndexIndirect(dataOffsetInBytes, Gnm::kShaderStageLs, pls->m_fetchControl&0xf, (pls->m_fetchControl>>4)&0xf, Gnm::kDrawModifierDefault);
						break;
					}
					case sce::Gnm::kActiveShaderStagesDispatchDrawVsPs:
					{
						SCE_GNM_ERROR_INLINE("drawIndexIndirect does not support DispatchDaw");
						break;
					}
					default:
					{
						SCE_GNM_ERROR_INLINE("Unknown active shader stage is set, please ensure you have called setActiveShaderStages() prior to draw");
						break;
					}
				}
			}


			/** @brief Inserts an indirect multi-drawIndex call that optionally reads the draw count from GPU memory, and which has a similar effect to calling drawIndexIndirect() several times
			 *		   with multiple DrawIndexIndirectArgs objects packed tightly in memory.
			 *
			 *  Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll context.
			 *
			 *	Each draw reads a new DrawIndexIndirectArgs object from the indirect arguments buffer.
			 *
			 *  @param[in] dataOffsetInBytes		The offset (in bytes) into the buffer that contains the first draw call's indirect arguments, which are set using setBaseIndirectArgs().
			 *										The data at this offset must be a Gnm::DrawIndexIndirectArgs structure.
			 *	@param[in] count					If <c><i>countAddress</i></c> is <c>NULL</c>, the value of the <c>count</c> parameter defines the number of draw calls to launch. If <c><i>countAddress</i></c> is non-<c>NULL</c>,
			 *										then the value of the <c>count</c> parameter will be used to clamp the draw count read from <c>countAddress</c>. 
			 *	@param[in] countAddress				If non-<c>NULL</c>, this value is a <c>DWORD</c>-aligned address from which the GPU will read the draw count, prior to clamping using the value provided in <c>count</c>.
			 *  @param[in] modifier					A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			 *
			 *	@note The index buffer and the buffer containing the indirect arguments should already have been set up using setIndexBuffer() and setIndexCount().
			 *		  The index count in <c><i>args</i>-><i>m_indexCountPerInstance</i></c> will be clamped to the value passed to setIndexCount().
			 *  @note The <c>instanceCount</c> value specified in the DrawIndexIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			 *        is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
			 *
			 *	@see Gnm::DrawIndexIndirectArgs, Gnm::DrawCommandBuffer::setBaseIndirectArgs(), Gnm::DrawCommandBuffer::setIndexBuffer(), Gnm::DrawCommandBuffer::setIndexCount()
			 */
			void drawIndexIndirectCountMulti(uint32_t dataOffsetInBytes, uint32_t count, void *countAddress, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexIndirectCountMulti can't be called between beginDispatchDraw and endDispatchDraw");
				// no need to set vertex and instance offsets here, they are coming from the draw indirect structure.
				m_lwcue.preDraw();

				switch (m_lwcue.getActiveShaderStages())
				{
					case sce::Gnm::kActiveShaderStagesVsPs:
					{
						const Gnmx::VsShader *pvs = (const Gnmx::VsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageVs);
						m_dcb.drawIndexIndirectCountMulti(dataOffsetInBytes, count, countAddress, Gnm::kShaderStageVs, pvs->m_fetchControl&0xf, (pvs->m_fetchControl>>4)&0xf, modifier);
						break;
					}
					case sce::Gnm::kActiveShaderStagesEsGsVsPs:
					{
						const Gnmx::EsShader *pes = (const Gnmx::EsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageEs);
						m_dcb.drawIndexIndirectCountMulti(dataOffsetInBytes, count, countAddress, Gnm::kShaderStageEs, pes->m_fetchControl&0xf, (pes->m_fetchControl>>4)&0xf, modifier);
						break;
					}
					case sce::Gnm::kActiveShaderStagesLsHsVsPs:
					case sce::Gnm::kActiveShaderStagesLsHsEsGsVsPs:
					case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
					case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
					{
						const Gnmx::LsShader *pls = (const Gnmx::LsShader*)m_lwcue.getBoundShader(Gnm::kShaderStageLs);
						m_dcb.drawIndexIndirectCountMulti(dataOffsetInBytes, count, countAddress, Gnm::kShaderStageLs, pls->m_fetchControl&0xf, (pls->m_fetchControl>>4)&0xf, modifier);
						break;
					}
					case sce::Gnm::kActiveShaderStagesDispatchDrawVsPs:
					{
						SCE_GNM_ERROR_INLINE("drawIndexIndirectCountMulti does not support DispatchDaw");
						break;
					}
					default:
					{
						SCE_GNM_ERROR_INLINE("Unknown active shader stage is set, please ensure you have called setActiveShaderStages() prior to draw");
						break;
					}
				}
			}


			/** @brief Inserts an indirect multi-drawIndex call that optionally reads the draw count from GPU memory, and which has a similar effect to calling drawIndexndirect() several times
			 *		   with multiple DrawIndexIndirectArgs objects packed tightly in memory.
			 *
			 *	Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll context.
			 *	
			 *	Each draw reads a new DrawIndexIndirectArgs object from the indirect arguments buffer.
			 *
			 *	@param[in] dataOffsetInBytes	The offset (in bytes) into the buffer that contains the indirect arguments, which is set using setBaseIndirectArgs().
			 *									The data at this offset should be a Gnm::DrawIndexIndirectArgs structure.
			 *	@param[in] count				If <c>countAddress</c> is NULL, this defines the number of draw calls to launch. If <c>countAddress</c> is non-NULL,
			 *									then this value will be used to clamp the draw count read from <c>countAddress</c>.
			 *	@param[in] countAddress			If non-NULL, this is a DWORD-aligned address that the GPU will read the draw count from, prior to clamping using the value provided in <c>count</c>.
			 *
			 *	@note	The index buffer and the buffer containing the indirect arguments should already have been set up using setIndexBuffer() and setIndexCount().
			 *			The index count in <c>args->m_indexCountPerInstance</c> will be clamped to the value passed to setIndexCount().
			 *	@note	The <c><i>instanceCount</i></c> value specified in the DrawIndexIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			 *			is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
			 *
			 *	@see Gnm::DrawIndexIndirectArgs, setBaseIndirectArgs(), setIndexBuffer(), setIndexCount()
			 */
			SCE_GNM_FORCE_INLINE void drawIndexIndirectCountMulti(uint32_t dataOffsetInBytes, uint32_t count, void *countAddress)
			{
				return drawIndexIndirectCountMulti(dataOffsetInBytes, count, countAddress, Gnm::kDrawModifierDefault);
			}


			/** @brief Inserts an indirect multi-drawIndex call that optionally reads the draw count from GPU memory, and which has a similar effect to calling drawIndexIndirect() several times
			 *		   with multiple DrawIndexIndirectArgs objects packed tightly in memory.
			 *
			 *  Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll context.
			 *
			 *	Each draw reads a new DrawIndexIndirectArgs object from the indirect arguments buffer.
			 *
			 *  @param[in] dataOffsetInBytes		The offset (in bytes) into the buffer that contains the first draw call's indirect arguments, which are set using setBaseIndirectArgs().
			 *										The data at this offset must be a Gnm::DrawIndexIndirectArgs structure.
			 *	@param[in] count					If <c><i>countAddress</i></c> is <c>NULL</c>, the value of the <c>count</c> parameter defines the number of draw calls to launch. If <c><i>countAddress</i></c> is non-<c>NULL</c>,
			 *										then the value of the <c>count</c> parameter will be used to clamp the draw count read from <c>countAddress</c>.
			 *	@param[in] countAddress				If non-<c>NULL</c>, this value is a <c>DWORD</c>-aligned address from which the GPU will read the draw count, prior to clamping using the value provided in <c>count</c>.
			 *	@param[in] stage					The shader stage that contains the fetch shader.
			 *  @param[in] vertexOffsetUserSgpr		The user SGRPR from which the fetch shader is expecting the vertex offset, or <c>0</c> if not used.
			 *  @param[in] instanceOffsetUserSgpr	The user SGPR from which the fetch shader is expecting the instance offset, or <c>0</c> if not used.
			 *  @param[in] modifier					A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			 *
			 *	@note The index buffer and the buffer containing the indirect arguments should already have been set up using setIndexBuffer() and setIndexCount(),
			 *		  and the buffer containing the indirect arguments should have been set using setBaseIndirectArgs().
			 *		  The index count in <c><i>args</i>-><i>m_indexCountPerInstance</i></c> will be clamped to the value passed to setIndexCount().
			 *  @note The <c>instanceCount</c> value specified in the DrawIndexIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			 *        is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
			 *
			 *	@see Gnm::DrawIndexIndirectArgs, Gnm::DrawCommandBuffer::setIndexBuffer(), Gnm::DrawCommandBuffer::setIndexCount(), Gnm::DrawCommandBuffer::setBaseIndirectArgs()
			 */
			SCE_GNM_FORCE_INLINE void drawIndexIndirectCountMulti(uint32_t dataOffsetInBytes, uint32_t count, void *countAddress, Gnm::ShaderStage stage, uint8_t vertexOffsetUserSgpr, uint8_t instanceOffsetUserSgpr, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexIndirectCountMulti can't be called between beginDispatchDraw and endDispatchDraw");
				// no need to set vertex and instance offsets here, they are coming from the draw indirect structure.
				m_lwcue.preDraw();
				m_dcb.drawIndexIndirectCountMulti(dataOffsetInBytes, count, countAddress, stage, vertexOffsetUserSgpr, instanceOffsetUserSgpr, modifier);
			}


			////////////// Dispatch commands

			/** @brief Inserts a compute shader dispatch with the indicated number of thread groups.
		 	 *
			 * 	This function never rolls the hardware context.
			 *
			 *	@param[in] threadGroupX The number of thread groups dispatched along the X dimension. This must be in the range <c>[0..2,147,483,647]</c>.
			 *	@param[in] threadGroupY The number of thread groups dispatched along the Y dimension. This must be in the range <c>[0..2,147,483,647]</c>.
			 *	@param[in] threadGroupZ The number of thread groups dispatched along the Z dimension. This must be in the range <c>[0..2,147,483,647]</c>.
			 */
			SCE_GNM_FORCE_INLINE void dispatch(uint32_t threadGroupX, uint32_t threadGroupY, uint32_t threadGroupZ)
			{
				m_lwcue.preDispatch();
				m_dcb.dispatchWithOrderedAppend(threadGroupX, threadGroupY, threadGroupZ, m_lwcue.getCsShaderOrderedAppendMode());
			}


			/** @brief Inserts an indirect compute shader dispatch, whose parameters are read from GPU memory.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] dataOffsetInBytes The offset (in bytes) into the buffer that contains the indirect arguments (set using <c>setBaseIndirectArgs()</c>).
			 *
			 *	The data at this offset should be a Gnm::DispatchIndirectArgs structure.
			 *
			 *  @note The buffer containing the indirect arguments should already have been set using <c>setBaseIndirectArgs()</c>.
			 *
			 *  @see Gnm::DispatchCommandBuffer::setBaseIndirectArgs
			 */
			SCE_GNM_FORCE_INLINE void dispatchIndirect(uint32_t dataOffsetInBytes)
			{
				m_lwcue.preDispatch();
				m_dcb.dispatchIndirectWithOrderedAppend(dataOffsetInBytes, m_lwcue.getCsShaderOrderedAppendMode());
			}


			/** @brief Inserts an asynchronous compute shader Gnm::DispatchCommandBuffer::dispatchDraw() and a paired vertex shader Gnm::DrawCommandBuffer::dispatchDraw().
			 *
			 *	Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@note dispatchDraw() also has the following state requirements to function properly in addition to those set directly by beginDispatchDraw() and dispatchDraw():
			 *	      <c>setActiveShaderStages(Gnm::kActiveShaderStagesDispatchDrawVsPs)</c> : Always required by dispatch draw hardware.
			 *	      <c>setNumInstances(1)</c> : Dispatch draw is not compatible with hardware instancing. Instancing is implemented in software via setDispatchDrawNumInstances().
			 *	
			 *	@note Unless the compute dispatches are guaranteed not to fill the ring buffers (and halt) or are limited to no more than 1 thread group per CU,
			 *	      tessellation or on-chip GS draw commands cannot be overlapped by DispatchCommandBuffer::dispatchDraw() compute dispatches.
			 *	      A GPU hang can result if a tessellation or on-chip GS thread group launches on a CU that is already hosting more than 1 thread group of compute, 
			 *		  which is waiting for space in the dispatch draw ring buffers. In general, if any dispatchDraw() commands will follow, it is safer
			 *		  to prevent all overlap of dispatch draw compute with tessellation or on-chip GS draw commands by calling
			 *		  GfxContext::dispatchDrawComputeWaitForEndOfPipe() after the last tessellation or on-chip GS draw command.			   
			 *
			 *	@note If a dispatchDraw call is still likely to be vertex-bound after any primitive culling performed by compute, adjusting the sub-draw flush
			 *		  thresholding parameters <c>primGroupThreshold</c> and <c>pollIntervalThreshold</c> may improve performance by packing the output vertices into fewer VS
			 *		  wavefronts, at some cost in additional command-processor and vertex-wavefront-launching latency.
			 *	
			 *	@param[in] pDispatchDrawIndexData	The input data to a dispatch draw asynchronous compute shader, which should be generated by Gnmx::createDispatchDrawInputData().
			 *	@param[in] primGroupThreshold The output threshold for a sub-draw to be generated; expressed in multiples of <c><i>primGroupIndexCount</i></c>.  A sub-draw will be flushed whenever the available output from compute shaders exceeds this limit.
			 *	@param[in] pollIntervalThreshold The timeout for a sub-draw to be generated; expressed in multiples of polling intervals (approximately 50us each.)  A sub-draw will be flushed after this timeout once there are also at least <c><i>primGroupIndexCount</i></c> indices available.
			 *	@param[in] modifier A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			 */

			void dispatchDraw(DispatchDrawTriangleCullIndexData const *pDispatchDrawIndexData, uint32_t primGroupThreshold, uint32_t pollIntervalThreshold, Gnm::DrawModifier modifier);


			/** @brief Inserts an asynchronous compute shader Gnm::DispatchCommandBuffer::dispatchDraw() and a paired vertex shader Gnm::DrawCommandBuffer::dispatchDraw().
			 *
			 *	Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll the context.
			 *
			 *	@note dispatchDraw() also has the following state requirements to function properly, in addition to those set directly by beginDispatchDraw() and dispatchDraw():
			 *	<c>setActiveShaderStages(Gnm::kActiveShaderStagesDispatchDrawVsPs)</c> : Always required by dispatch draw hardware.
			 *	<c>setNumInstances(1)</c> : Dispatch draw is not compatible with hardware instancing. Instancing is implemented in software via setDispatchDrawNumInstances().
			 *
			 *	@note Unless the compute dispatches are guaranteed not to fill the ring buffers (and halt) or are limited to no more than 1 thread group per CU,
			 *	tessellation or on-chip GS draw commands cannot be overlapped by DispatchCommandBuffer::dispatchDraw() compute dispatches.
			 *	A GPU hang can result if a tessellation or on-chip GS thread group launches on a CU that is already hosting more than 1 thread group
			 *	of compute, which is waiting for space in the dispatch draw ring buffers. In general, if any dispatchDraw commands will follow, it is safer
			 *	to prevent all overlap of dispatch draw compute with tessellation or on-chip GS draw commands by calling
			 *	GfxContext::dispatchDrawComputeWaitForEndOfPipe() after the last tessellation or on-chip GS draw command.
			 *
			 *	@note If a dispatchDraw call is still likely to be vertex-bound after any primitive culling performed by compute, adjusting the sub-draw flush
			 *	thresholding parameters <c>primGroupThreshold</c> and <c>pollIntervalThreshold</c> may improve performance by packing the output vertices into fewer VS
			 *	wavefronts, at some cost in additional command-processor and vertex-wavefront-launching latency.
			 *
			 *	@param[in] pDispatchDrawIndexData	The input data to a dispatch draw asynchronous compute shader, which should be generated by Gnmx::createDispatchDrawInputData().
			 */
			SCE_GNM_FORCE_INLINE void dispatchDraw(DispatchDrawTriangleCullIndexData const *pDispatchDrawIndexData)
			{
				dispatchDraw(pDispatchDrawIndexData, 0, 0, Gnm::kDrawModifierDefault);
			}


			/** @brief Sets the ring buffer where data will flow from the ES to the GS stages when geometry shading is enabled.
			 *
			 *  This function will roll hardware context.
			 *
			 *  @note This function modifies the global resource table. It is not safe to modify the global resource table unless the GPU graphics pipe is idle.
			 *
			 *  @param[in] ringBuffer The address of the buffer.
			 *  @param[in] ringSizeInBytes The size of the buffer in bytes.
			 *  @param[in] maxExportVertexSizeInDword The maximum size of a vertex export in <c>DWORD</c>s.
			 */
			void setEsGsRingBuffer(void* ringBuffer, uint32_t ringSizeInBytes, uint32_t maxExportVertexSizeInDword);

				
			/** @brief Sets the ring buffers where data will flow from the GS to the VS stages when geometry shading is enabled.
			 *
			 *  This function will roll hardware context.
			 *
			 *  @note This function modifies the global resource table. It is not safe to modify the global resource table unless the GPU graphics pipe is idle.
			 *
			 *  @param[in] ringBuffer					The address of the buffer.
			 *  @param[in] ringSizeInBytes				The size of the buffer in bytes.
			 *  @param[in] vertexSizePerStreamInDword	The vertex size for each of four streams in <c>DWORD</c>s.
			 *  @param[in] maxOutputVertexCount			The maximum number of vertices output from the GS stage.
			 */
			void setGsVsRingBuffers(void* ringBuffer, uint32_t ringSizeInBytes, const uint32_t vertexSizePerStreamInDword[4], uint32_t maxOutputVertexCount);

				
			/** @brief Specifies a buffer to hold tessellation constants.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] tcbAddr		The address of the buffer.
			 *  @param[in] domainStage	The stage in which the domain shader will execute.
			 */
			SCE_GNM_FORCE_INLINE void setTessellationDataConstantBuffer(void *tcbAddr, Gnm::ShaderStage domainStage)
			{
				SCE_GNM_ASSERT_MSG(domainStage == Gnm::kShaderStageEs || domainStage == Gnm::kShaderStageVs, "domainStage (%d) must be kShaderStageEs or kShaderStageVs.", domainStage);

				Gnm::Buffer tessellationCb;
				tessellationCb.initAsConstantBuffer(tcbAddr, sizeof(Gnm::TessellationDataConstantBuffer));
				m_lwcue.setConstantBuffers(Gnm::kShaderStageHs, LightweightConstantUpdateEngine::kConstantBufferInternalApiSlotForTessellation, 1, &tessellationCb);
				m_lwcue.setConstantBuffers(domainStage, LightweightConstantUpdateEngine::kConstantBufferInternalApiSlotForTessellation, 1, &tessellationCb);
			}

				
			/** @brief Specifies a buffer to receive off-chip tessellation factors.
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @note This function modifies the global resource table. It is not safe to modify the global resource table unless the GPU graphics pipe is idle.
			 *
			 *  @param[in] tessFactorMemoryBaseAddr	The address of the buffer. This address must already have been mapped correctly. Please refer to <c>Gnmx::Toolkit::allocateTessellationFactorRingBuffer()</c>.
			 */
			SCE_GNM_FORCE_INLINE void setTessellationFactorBuffer(void *tessFactorMemoryBaseAddr)
			{
				Gnm::Buffer tessFactorBuffer;
				tessFactorBuffer.initAsTessellationFactorBuffer(tessFactorMemoryBaseAddr, Gnm::kTfRingSizeInBytes);
				m_lwcue.setGlobalDescriptor(Gnm::kShaderGlobalResourceTessFactorBuffer, &tessFactorBuffer);
			}


			/** @brief Submits the DrawCommandBuffer.
			 *
			 *	@return A code indicating the submission status.
			 *
			 *	@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
			 *  @note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
			 *  @see Gnm::areSubmitsAllowed()
			 */
			int32_t submit(void);


			/** @brief Submits the DrawCommandBuffer with a workload ID.
			 *
			 *	@param[in] workloadId The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
			 *
			 *	@return A code indicating the submission status.
			 *
			 *	@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
			 *  @see Gnm::areSubmitsAllowed()
			 */
			int32_t submit(uint64_t workloadId);


			/** @brief Submits the DrawCommandBuffer, appending a flip command to execute after command buffer execution completes.
			 *
			 *	@param[in] videoOutHandle		The video out handle.
			 *	@param[in] displayBufferIndex	The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
			 *	@param[in] flipMode				The flip mode. For valid values, see SceVideoOutFlipMode.
			 *	@param[in] flipArg				User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely. 
			 *									The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> 
			 *									associated with the most recently completed flip.
			 *
			 * 	@return A code indicating the error status.
			 *
			 *	@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
			 *  @note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
			 *  @see Gnm::areSubmitsAllowed()
			 */
			int32_t submitAndFlip(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg);

			/** @brief Submits the DrawCommandBuffer, appending a flip command to execute after command buffer execution completes.
			 *
			 *	@param[in] workloadId				The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
			 *	@param[in] videoOutHandle			The video out handle.
			 *	@param[in] displayBufferIndex		The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
			 *	@param[in] flipMode					The flip mode. For valid values, see SceVideoOutFlipMode.
			 *	@param[in] flipArg					User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely.
			 *										The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> associated with the most recently completed flip.
			 *
			 *	@return A code indicating the error status.
			 *	
			 *	@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
			 *	@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
			 *  @see Gnm::areSubmitsAllowed()
			 */
			int32_t submitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg);

			/** @brief Submits the DrawCommandBuffer, appending a flip command to execute after command buffer execution completes.
			 *
			 *	@note This function will submit the ConstantCommandBuffer only if <c>m_submitWithCcb</c> had been set in the CUE. If the CUE
			 *		  determines that it has not submitted any meaningful CCB packets, it will not set this variable. If you still want the CCB to be submitted
			 *		  (e.g. because you added your own packets to the <c><i>m_ccb</i></c> directly), you will need to set <c>m_cue.m_submitWithCcb</c> to <c>true</c> before calling submit().
			 *	
			 *	@param[in] videoOutHandle		Video out handle.
			 *	@param[in] displayBufferIndex	The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
			 *	@param[in] flipMode				The flip mode. For valid values, see SceVideoOutFlipMode.
			 *	@param[in] flipArg				User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely. 
			 *									The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> associated with the most recently completed flip.
			 *	@param[out] labelAddr			The GPU address to be updated when the command buffer has been processed.
			 *	@param[in]  value				The value to write to <c><i>labelAddr</i></c>.
			 *	
			 *	@return A code indicating the error status.
			 *	
			 *	@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
			 *  @note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
			 *  @see Gnm::areSubmitsAllowed()
			 */
			int32_t submitAndFlip(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
								  void *labelAddr, uint32_t value);


			/** @brief Submits the DrawCommandBuffer, appending a flip command to execute after command buffer execution completes.
			 *	
			 *	@param[in] workloadId			The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
			 *	@param[in] videoOutHandle		Video out handle.
			 *	@param[in] displayBufferIndex	The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
			 *	@param[in] flipMode				The flip mode. For valid values, see SceVideoOutFlipMode.
			 *	@param[in] flipArg				User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely.
			 *									The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> associated with the most recently completed flip.
			 *	@param[out] labelAddr			The GPU address to update when the command buffer has been processed.
			 *	@param[out] labelAddr			The GPU address to update when the command buffer has been processed.
			 *	@param[in] value				The value to write to <c><i>labelAddr</i></c>.
			 *					
			 *	@return A code indicating the error status.
			 *	@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
			 *	@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
			 *  @see Gnm::areSubmitsAllowed()
			 */
			int32_t submitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
								  void *labelAddr, uint32_t value);


			/** @brief Submits the DrawCommandBuffer. A flip command is appended and will be executed after command buffer execution is complete.
			 *
			 *	@param[in] videoOutHandle		Video out handle.
			 *	@param[in] displayBufferIndex	The index of the display buffer to wait for. This index is registered with <c>sceVideoOutRegisterBuffers()</c>.
			 *	@param[in] flipMode				The flip mode. Please see <c>SceVideoOutFlipMode</c> for possible values.
			 *	@param[in] flipArg				A user-provided argument with no internal meaning. The <c><i>flipArg</i></c> associated with the most recently completed flip is
			 *									included in the <c>SceVideoOutFlipStatus</c> object retrieved by <c>sceVideoOutGetFlipStatus()</c>; it could therefore
			 *									be used to uniquely identify each flip.
			 *	@param[in] eventType            Determines when interrupt will be triggered.
			 *	@param[out] labelAddr			The GPU address to be updated when the command buffer has been processed.
			 *	@param[in] value				The value to write to <c><i>labelAddr</i></c>.
			 *	@param[in] cacheAction          Cache action to perform.
			 *
			 *	@return A code indicating the error status.
			 *	@note To enable auto-validation on submit(), please enable the "GPU Validation" option in the Target Manager Settings.
			 *	@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
			 *  @see Gnm::areSubmitsAllowed()
			 */
			int32_t submitAndFlipWithEopInterrupt(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  Gnm::EndOfPipeEventType eventType, void *labelAddr, uint32_t value, Gnm::CacheAction cacheAction);


			/** @brief Submits the DrawCommandBuffer. A flip command is appended and will be executed after command buffer execution is complete.
			 *	
			 *	@param[in] workloadId			The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
			 *	@param[in] videoOutHandle		Video out handle.
			 *	@param[in] displayBufferIndex	The index of the display buffer to wait for. This index is registered with <c>sceVideoOutRegisterBuffers()</c>.
			 *	@param[in] flipMode				The flip mode. Please see <c>SceVideoOutFlipMode</c> for possible values.
			 *	@param[in] flipArg				A user-provided argument with no internal meaning. The <c><i>flipArg</i></c> associated with the most recently completed flip is
			 *									included in the <c>SceVideoOutFlipStatus</c> object retrieved by <c>sceVideoOutGetFlipStatus()</c>; it could therefore
			 *									be used to uniquely identify each flip.
			 *	@param[in] eventType            Determines when interrupt will be triggered.
			 *	@param[out] labelAddr			The GPU address to be updated when the command buffer has been processed.
			 *	@param[in] value				The value to write to <c><i>labelAddr</i></c>.
			 *	@param[in] cacheAction          Cache action to perform.
			 *	
			 *	@return A code indicating the error status.
			 *	@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
			 *	@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
			 *	@see Gnm::areSubmitsAllowed()
			 */
			int32_t submitAndFlipWithEopInterrupt(uint64_t workloadId,
												  uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  Gnm::EndOfPipeEventType eventType, void *labelAddr, uint32_t value, Gnm::CacheAction cacheAction);


			/** @brief Submits the DrawCommandBuffer. A flip command is appended and will be executed after command buffer execution is complete.
			 *
			 *	@param[in] videoOutHandle		Video out handle.
			 *	@param[in] displayBufferIndex	The index of the display buffer to wait for. This index is registered with <c>sceVideoOutRegisterBuffers()</c>.
			 *	@param[in] flipMode				The flip mode. Please see <c>SceVideoOutFlipMode</c> for possible values.
			 *	@param[in] flipArg				A user-provided argument with no internal meaning. The <c><i>flipArg</i></c> associated with the most recently completed flip is
			 *									included in the <c>SceVideoOutFlipStatus</c> object retrieved by <c>sceVideoOutGetFlipStatus()</c>; it could therefore
			 *									be used to uniquely identify each flip.
			 *	@param[in] eventType            Determines when interrupt will be triggered.
			 *	@param[in] cacheAction          Cache action to perform.
			 * 
			 *	@return A code indicating the error status.
			 *	@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
			 *	@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
			 *	@see Gnm::areSubmitsAllowed()
			 */
			int32_t submitAndFlipWithEopInterrupt(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  Gnm::EndOfPipeEventType eventType, Gnm::CacheAction cacheAction);


			/** @brief Submits the DrawCommandBuffer. A flip command is appended and will be executed after command buffer execution is complete.
			 *	
			 *	@param[in] workloadId			The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
			 *	@param[in] videoOutHandle		Video out handle.
			 *	@param[in] displayBufferIndex	The index of the display buffer to wait for. This index is registered with <c>sceVideoOutRegisterBuffers()</c>.
			 *	@param[in] flipMode				The flip mode. Please see <c>SceVideoOutFlipMode</c> for possible values.
			 *	@param[in] flipArg				A user-provided argument with no internal meaning. The <c><i>flipArg</i></c> associated with the most recently completed flip is
			 *									included in the <c>SceVideoOutFlipStatus</c> object retrieved by <c>sceVideoOutGetFlipStatus()</c>; it could therefore
			 *									be used to uniquely identify each flip.
			 *	@param[in] eventType            Determines when interrupt will be triggered.
			 *	@param[in] cacheAction          Cache action to perform.
			 *   
			 *	@return A code indicating the error status.
			 *	@note To enable auto-validation on submit(), please enable the "GPU Validation" option in the Target Manager Settings.
			 *	@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
			 *  @see Gnm::areSubmitsAllowed()
			*/
			int32_t submitAndFlipWithEopInterrupt(uint64_t workloadId,
												  uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  Gnm::EndOfPipeEventType eventType, Gnm::CacheAction cacheAction);


#if defined(__ORBIS__)
			/** @brief Runs validation on the DrawCommandBuffer without submitting it.
			 *
			 *	@return A code indicating the validation status.
			 */
			int32_t validate(void)
			{
				void* dcbGpuAddress = m_dcb.m_beginptr;
				uint32_t dcbBufferSize = uint32_t(m_dcb.m_cmdptr - m_dcb.m_beginptr) * sizeof(uint32_t);

				// CCB is only necessary for dispatch draw initial setup, so it isn't necessary to pass to validation
				return Gnm::validateDrawCommandBuffers(1, &dcbGpuAddress, &dcbBufferSize, 0, 0);
			}
#endif

			public:

				LightweightGraphicsConstantUpdateEngine m_lwcue; ///< Light-weight constant update engine. Access directly at your own risk!
		};
	} // Gnmx
} // sce


#endif // // _SCE_GNMX_LWGFXCONTEXT_H
