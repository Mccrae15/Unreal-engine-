/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/
#ifndef _SCE_GNMX_BASEGFXCONTEXT_H
#define _SCE_GNMX_BASEGFXCONTEXT_H

#include <gnm/constantcommandbuffer.h>
#include <gnm/drawcommandbuffer.h>
#include <gnmx/config.h>
#include <gnmx/helpers.h>
#include <gnmx/computequeue.h>
#include <gnmx/dispatchdraw.h>
#include <gnmx/resourcebarrier.h>

namespace sce
{
	namespace Gnmx
	{
		class RenderTarget;

		/** @brief The base class from which sce::Gnmx::LightweightGfxContext and sce::Gnmx::GfxContext are derived. 
		*
		* This class provides default methods for initializing and managing command buffers and contexts.
		*/
		class SCE_GNMX_EXPORT BaseGfxContext
		{
		public:
			/** @brief The default constructor for the BaseGfxContext class. */
			BaseGfxContext(void);
			/** @brief The default destructor for the BaseGfxContext class. */
			~BaseGfxContext(void);

			/** @brief Initializes a GfxContext with application-provided memory buffers.

				@param[out] dcbBuffer		A buffer for use by <c>m_dcb</c>.
				@param[in] dcbSizeInBytes	The size of <c><i>dcbBuffer</i></c> in bytes.
				@param[out] ccbBuffer		A buffer for use by <c>m_ccb</c>.
				@param[in] ccbSizeInBytes	The size of <c><i>ccbBuffer</i></c> in bytes.
				*/
			void init(void *dcbBuffer, uint32_t dcbSizeInBytes, void *ccbBuffer, uint32_t ccbSizeInBytes);

			/** @brief Initializes a GfxContext with application-provided memory buffers to enable dispatch draw.

				@note initDispatchDrawCommandBuffer() must only be called after init()

				@param[out] acbBuffer		A buffer for use by the Dispatch Command Buffer.
				@param[in] acbSizeInBytes	The size of <c><i>acbBuffer</i></c> in bytes.
				*/
			void initDispatchDrawCommandBuffer(void *acbBuffer, uint32_t acbSizeInBytes);

			/** @brief Sets the compute queue the GfxContext should use for dispatch draw asynchronous compute dispatches.
				
				@param[in,out]  pQueue   The ComputeQueue object to use for dispatch draw asynchronous compute dispatches.
				*/
			void setDispatchDrawComputeQueue(ComputeQueue *pQueue)
			{
				m_pQueue = pQueue;
			}

			/** @brief Sets up a default hardware state for the graphics ring.
			 *
			 *	This function performs the following tasks:
			 *	<ul> 
			 *		<li>Causes the GPU to wait until all pending PS outputs are written</li>
			 *		<li>Invalidates all GPU caches</li>
			 *		<li>Resets context registers to their default values</li>
			 *		<li>Rolls the hardware context</li>
			 *	</ul>
			 * 
			 *  Some of the render states that this function resets include the following. The order of this listing is not significant.
			 *	<ul>
			 *		<li><c>setVertexQuantization(kVertexQuantizationMode16_8, kVertexQuantizationRoundModeRoundToEven, kVertexQuantizationCenterAtHalf);</c></li>
			 *		<li><c>setLineWidth(8);</c></li>
			 *		<li><c>setPointSize(0x0008, 0x0008);</c></li>
			 *		<li><c>setPointMinMax(0x0000, 0xFFFF);</c></li>
			 *		<li><c>setClipControl( Gnm::ClipControl.init() );</c></li>
			 *		<li><c>setViewportTransformControl( Gnm::ViewportTransformControl.init() );</c></li>
			 *		<li><c>setClipRectangleRule(0xFFFF);</c></li>
			 *		<li><c>setGuardBands(1.0f, 1.0f, 1.0f, 1.0f);</c></li>
			 *		<li><c>setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);</c></li>
			 *		<li><c>setAaSampleMask((uint64_t)(-1LL));</c></li>
			 *		<li><c>setNumInstances(1);</c></li>
			 *		<li><c>GraphicsShaderControl graphicsShaderControl;</c></li>
			 *		<li><c>graphicsShaderControl.init();</c></li>
			 *		<li><c>setGraphicsShaderControl(graphicsShaderControl);</c></li>
			 *		<li><c>setPrimitiveResetIndex(0)</c></li>
			 *		<li><c>disableGsMode();</c></li>
			 *		<li><c>setScanModeControl(kScanModeControlAaDisable, kScanModeControlViewportScissorDisable);</c></li>
			 *		<li><c>setPsShaderRate(kPsShaderRatePerPixel);</c></li>
			 *		<li><c>setAaSampleCount(kNumSamples1, 0);</c></li>
			 *		<li><c>setPolygonOffsetZFormat(kZFormat32Float);</c></li>
			 *	</ul>
			 *
			 *	@note Call this function only when the graphics pipeline is idle. Calling this function after the beginning of the frame may cause the GPU to hang.
			 *  @note This function is called internally by the OS after every call to submitDone(); generally, it is not necessary for developers to call it explicitly at the beginning
			 *        of every frame, as was previously documented.
			 *  @see DrawCommandBuffer::initializeDefaultContextState()
			 *
 			 *	@cmdsize 256
			 */
			void initializeDefaultHardwareState()
			{
				if (m_acb.m_beginptr != NULL)
					m_acb.initializeDefaultHardwareState();
				m_dcb.initializeDefaultHardwareState();
			}
			/** @brief  Sets up a default context state for the graphics ring.
			 *
			 *	This function resets context registers to default values and rolls the hardware context. It does not include a forced GPU stall
			 *  or invalidate caches, and may therefore be a more efficient alternative method for resetting GPU state to safe default values than
			 *  calling the initializeDefaultHardwareState() function.
			 *
			 *  This function resets the following render states as described. The order of this listing is not significant.
			 *	<ul>
			 *		<li><c>setCbControl(Gnm::kCbModeNormal, Gnm::kRasterOpCopy);</c></li>
			 *      <li><c>setIndexOffset(0);</c></li>
			 *		<li><c>setNumInstances(1);</c></li>
			 *	</ul>
			 *	@cmdsize 256
			 */
			void initializeToDefaultContextState()
			{
				m_dcb.initializeToDefaultContextState();
			}

			/** @brief Sets per graphics stage limits controlling which compute units will be enabled and how many wavefronts will be allowed to run. This is broadcast to both shader engines (SEs).
			
				There is a detailed description of defaults and limitations in class GraphicsShaderControl.

				This function never rolls the hardware context.

				@param[in] control		A GraphicsShaderControl configured with compute unit masks and wavefront limits for all shader stages.

				@note  In order to prevent dispatch draw GPU hangs, we additionally require that VS must enable compute unit 8, which we require to be disabled for dispatch draw compute.
				@note  Changes to the shader control will remain until the next call to this function.
				@cmdsize (((uint32_t)broadcast < (uint32_t)kBroadcastAll) ? 6 : 3) + 21
				@see Gnm::GraphicsShaderControl, setComputeShaderControl
				*/			
			void setGraphicsShaderControl(Gnm::GraphicsShaderControl control)
			{
				if ((control.m_regPgmRsrc3[Gnm::kShaderStageVs-1] & 0x100) == 0)
					control.m_regPgmRsrc3[Gnm::kShaderStageVs-1] |= 0x100;
				return m_dcb.setGraphicsShaderControl(control);
			}

			/** @brief For dispatches on the compute queue that is executing <c>m_acb</c>, sets dispatch limits that control the maximum number of asynchronous compute wavefronts (including dispatch draw compute wavefronts) that can run simultaneously in the GPU. 
				
				This function can also be used to disable this dispatch limit. 
				This function never rolls the hardware context.
				
				@param[in] wavesPerSh        The wavefront limit per shader engine. The range is <c>[1:1023]</c>. Specify a value of 0 to disable the limit.
				@param[in] threadgroupsPerCu The threadgroup limit per compute unit. The range is <c>[1:15]</c>. Specify a value of 0 to disable the limit.
				@param[in] lockThreshold     The per-shader-engine low threshold for locking. The granularity is 4. The range is <c>[1:63]</c> corresponding to <c>[4:252]</c> wavefronts. To disable locking, specify a value of <c>0</c>.

				@note  It is always advisable to limit the number of dispatch draw compute thread groups to approximately balance compute throughput against VS-PS throughput, and ensure
				       that dispatch draw compute resource usage does not crowd out VS-PS resource usage.
				       As soon as the GPU becomes VS-PS throughput bound, back-pressure will cause dispatch compute thread groups to stall waiting for IRB space. This will fill the GPU to the
					   extent allowed by the CU mask above and the thread group per CU limit.  This in turn restricts resources available to launch VS-PS wavefronts, which, if 
					   throughput is bounded by VS or PS shader resource availability, results in the GPU ending up locked into a lower performance state. In this state VS-PS is resource 
					   limited by an unnecessarily large complement of stalled dispatch draw compute wavefronts.
				@cmdsize 3

				@see Gnm::DispatchCommandBuffer::setComputeShaderControl()
			*/
			void setAsynchronousComputeShaderControl(uint32_t wavesPerSh, uint32_t threadgroupsPerCu, uint32_t lockThreshold)
			{
				return m_acb.setComputeShaderControl(wavesPerSh, threadgroupsPerCu, lockThreshold);
			}

			/** @brief On the compute queue that is executing this command buffer, sets a dispatch mask that determines which compute units are active in the specified shader engine for asynchronous compute (including dispatch draw compute.)

				All masks are logical masks indexed from <c>0</c> to Gnm::kNumCusPerSe <c>- 1</c>, regardless of which physical compute units are working and enabled by the driver.
				This function never rolls the hardware context.

				@param[in] engine h		The shader engine to be configured.
				@param[in] mask			A mask to enable compute units for the CS shader stage.

				@note  In order to prevent dispatch draw GPU hangs, dispatch draw compute must disable compute units 1 (on which PS must run and VS can't run) and 8 (on which VS must run.)
				@see setGraphicsShaderControl(), Gnmx::GfxContext::setGraphicsShaderControl()
				@cmdsize 6
			*/
			SCE_GNM_API_CHANGED
			void setAsynchronousComputeResourceManagement(Gnm::ShaderEngine engine, uint16_t mask)
			{
				SCE_GNM_ASSERT_MSG_INLINE(mask == 0 || (mask & 0x00FD) != 0, "mask (0x%04X) for asynchronous compute must include at least one CU in 0x%04x", mask, 0x0FD);
				mask &= 0x0FD;
				if (mask == 0)
					mask = 0x0FD;

				if (sce::Gnm::getGpuMode() == sce::Gnm::kGpuModeBase)
				{
					m_acb.setComputeResourceManagementForBase(engine, mask);
					m_acb.insertNop(3);
				}
				else
				{
					m_acb.setComputeResourceManagementForNeo(engine, mask);
					m_acb.setComputeResourceManagementForNeo(engine == sce::Gnm::kShaderEngine0 ? sce::Gnm::kShaderEngine2 : sce::Gnm::kShaderEngine3, mask);
				}
			}

			/** @brief On the compute queue that is executing this command buffer, sets a dispatch mask that determines which compute units are active in the specified shader engine for asynchronous compute (including dispatch draw compute.)

				All masks are logical masks indexed from <c>0</c> to Gnm::kNumCusPerSe <c>- 1</c>, regardless of which physical compute units are working and enabled by the driver.
				This function never rolls the hardware context.

				@param[in] engine h		The shader engine to be configured.
				@param[in] mask			A mask to enable compute units for the CS shader stage.

				@note  In order to prevent dispatch draw GPU hangs, dispatch draw compute must disable compute units 1 (on which PS must run and VS can't run) and 8 (on which VS must run.)
				@see setGraphicsShaderControl(), Gnmx::GfxContext::setGraphicsShaderControl()
				@cmdsize 3
			*/
			void setAsynchronousComputeResourceManagementForBase(Gnm::ShaderEngine engine, uint16_t mask)
			{
				SCE_GNM_ASSERT_MSG_INLINE(mask == 0 || (mask & 0x00FD) != 0, "mask (0x%04X) for asynchronous compute must include at least one CU in 0x%04x", mask, 0x0FD);
				mask &= 0x0FD;
				if (mask == 0)
					mask = 0x0FD;
				return m_acb.setComputeResourceManagementForBase(engine, mask);
			}

			/** @brief Sets a dispatch mask that determines which compute units are active in the specified shader engine for asynchronous compute (including dispatch draw compute), on the compute queue this command buffer is executing on.

				All masks are logical masks indexed from 0 to Gnm::kNumCusPerSe <c>- 1</c>, regardless of which physical compute units are working and enabled by the driver.
				This function never rolls the hardware context.

				@param[in] engine h		The shader engine to be configured.
				@param[in] mask			A mask to enable compute units for the CS shader stage.

				@note  In order to prevent dispatch draw GPU hangs, we additionally require that dispatch draw compute must disable compute units 1 (on which PS must run and VS can't run) and 8 (on which VS must run).
				@see setGraphicsShaderControl(), Gnmx::GfxContext::setGraphicsShaderControl()
				@cmdsize 3
			*/
			void setAsynchronousComputeResourceManagementForNeo(Gnm::ShaderEngine engine, uint16_t mask)
			{
				SCE_GNM_ASSERT_MSG_INLINE(mask == 0 || (mask & 0x00FD) != 0, "mask (0x%04X) for asynchronous compute must include at least one CU in 0x%04x", mask, 0x0FD);
				mask &= 0x0FD;
				if (mask == 0)
					mask = 0x0FD;
				return m_acb.setComputeResourceManagementForNeo(engine, mask);
			}

			/** @brief Gets the required size (in bytes) of a GDS area that the GPU uses to track outstanding dispatchDraw() calls.

			    The GDS area consists of a kick ring buffer with elements that are 3 <c>DWORD</c>s in size, and a number of <c>DWORD</c> counters required to track index and vertex ring buffer allocations.

				@param[in] numKickRingBufferElems			The size of the GDS kick ring buffer in elements, which must be between 1 and 1023 (kMaxDispatchDrawKickRingBufferElements) elements in size. This determines how many dispatchDraw() calls can be simultaneously generating index data; 64 elements is probably more than sufficient.
				@return The required size in bytes.
			*/
			uint32_t getRequiredSizeOfGdsDispatchDrawArea(uint32_t numKickRingBufferElems);

			/** @brief Configures and zeroes the dispatch draw GDS kick ring buffer, configures the index ring buffer and, optionally, configures the vertex ring buffer.
				
				@param[in] pIndexRingBuffer			A pointer to memory to use for the index ring buffer. This memory must be 256-byte aligned. Generally, it should reside in GARLIC memory.
				@param[in] sizeofIndexRingBufferAlign256B	The size of the index ring buffer, which must be a multiple of 256 bytes. This buffer must be large enough to hold <c>kDispatchDrawIndexRingBufferMinimumSizeInIndices</c> indices of output; for 16-bit indices, this requirement translates to at least 9472B (9KB + 256B). This limits the total amount of index data in flight between compute and vertex shaders; optimal performance is probably achieved somewhere between 64KB and 256KB, depending on the characteristics of the rendering load.
				@param[in] pVertexRingBuffer			A pointer to memory to use for the vertex ring buffer. This memory must be 4-byte aligned. Generally, it should reside in GARLIC memory. This value is unused if <c><i>sizeofVertexRingBufferInBytes</i></c> is <c>0</c>.
				@param[in] sizeofVertexRingBufferInBytes	The size of the vertex ring buffer, which must be a power of 2 bytes in size and must be at least <c>256*3*(<c><i>maxVrbVertexSizeInBytes</i></c>+8)</c>. If this member is set to <c>0</c>, dispatchDraw VRB support will be disabled. To support <c>vrbVertexSizeInBytes=16</c> (float4 position, typically the minimum likely VRB output), the VRB must be at least 18KB in size or 32KB to meet the power of 2 alignment requirement. To match the IRB, the size should be approximately <c><c><i>sizeofIndexRingBufferAlign256B</i></c>*(256*(<c><i>maxVrbVertexSizeInBytes</i></c>+8))/(513*2)</c> or roughly <c>6*<c><i>sizeofIndexRingBufferAlign256B</i></c></c> to support <c>vrbVertexSizeInBytes=16</c>.
				@param[in] numKickRingBufferElems			The size of the GDS kick ring buffer, expressed as a number of elements. Each element is 12 bytes in size; 64 elements is probably more than sufficient for most usage. This value must be between <c>1</c> and <c>1023</c> (<c>kMaxDispatchDrawKickRingBufferElements</c>) elements in size. This value determines the number of dispatchDraw() calls that can generate index data simultaneously.
				@param[in] gdsOffsetDispatchDrawArea		The offset of the GDS area to use for tracking dispatchDraw() calls. This area must be 8-byte aligned and it must start at an address no larger than <c>kGdsMaxAddressForDispatchDrawKickRingBufferCounters</c> (<c>0xFFC</c>). The region must be at least <c>getRequiredSizeOfGdsDispatchDrawArea(numKickRingBufferElems)</c> bytes in size.
				@param[in] gdsOaCounterForDispatchDrawIrb	The index of an uninitialized GDS ordered append unit internal counter to configure for index ring buffer allocation tracking (range <c>[0:7]</c>).
				@param[in] gdsOaCounterForDispatchDrawVrb	The index of an uninitialized GDS ordered append unit internal counter to configure for vertex ring buffer allocation tracking (range <c>[0:7]</c>). This value is unused if <c><i>sizeofVertexRingBufferInDwords</i></c> is <c>0</c>. This value must not conflict with <c><i>gdsOaCounterForDispatchDrawIrb</i></c> if <c><i>sizeofVertexRingBufferInDwords</i></c> is not <c>0</c>.
			*/
			void setupDispatchDrawRingBuffers(void *pIndexRingBuffer, uint32_t sizeofIndexRingBufferAlign256B, void *pVertexRingBuffer, uint32_t sizeofVertexRingBufferInBytes, uint32_t numKickRingBufferElems, uint32_t gdsOffsetDispatchDrawArea, uint32_t gdsOaCounterForDispatchDrawIrb, uint32_t gdsOaCounterForDispatchDrawVrb);

			/** @brief Configures and zeroes the dispatch draw GDS kick ring buffer and configures the index ring buffer.
				@note This variant assumes a <c>NULL</c> vertex buffer.
				@param[in] pIndexRingBuffer					A pointer to memory to use for the index ring buffer. This memory must be 256-byte aligned. Generally, it should reside in GARLIC memory.
				@param[in] sizeofIndexRingBufferAlign256B	The size of the index ring buffer, which must be a multiple of 256 bytes. This value must be large enough to hold <c>kDispatchDrawIndexRingBufferMinimumSizeInIndices</c> indices of output; for 16-bit indices, this translates to at least 9472B (9KB + 256B). This value limits the total amount of index data in flight between compute and vertex shaders; optimal performance is probably achieved somewhere between 64KB and 256KB, depending on the characteristics of the rendering load.
				@param[in] numKickRingBufferElems			The size of the GDS kick ring buffer, expressed as a number of elements. Each element is 12 bytes in size; 64 elements is probably more than sufficient for most usage. This value must be between <c>1</c> and <c>1023</c> (<c>kMaxDispatchDrawKickRingBufferElements</c>) elements in size. This value determines the number of dispatchDraw() calls that can generate index data simultaneously.
				@param[in] gdsOffsetDispatchDrawArea		The offset of the GDS area to use for tracking dispatchDraw() calls. This area must be 8-byte aligned and it must start at an address no larger than <c>kGdsMaxAddressForDispatchDrawKickRingBufferCounters</c> (<c>0xFFC</c>). The region must be at least <c>getRequiredSizeOfGdsDispatchDrawArea(<c><i>numKickRingBufferElems</i></c>)</c> bytes in size.
				@param[in] gdsOaCounterForDispatchDrawIrb	The index of an uninitialized GDS ordered append unit internal counter to configure for index ring buffer allocation tracking (range <c>[0:7]</c>).
			*/
			inline void setupDispatchDrawRingBuffers(void *pIndexRingBuffer, uint32_t sizeofIndexRingBufferAlign256B, uint32_t numKickRingBufferElems, uint32_t gdsOffsetDispatchDrawArea, uint32_t gdsOaCounterForDispatchDrawIrb)
			{
				setupDispatchDrawRingBuffers(pIndexRingBuffer, sizeofIndexRingBufferAlign256B, NULL, 0, numKickRingBufferElems, gdsOffsetDispatchDrawArea, gdsOaCounterForDispatchDrawIrb, (uint32_t)-1);
			}

			/** @brief Resets the ConstantUpdateEngine, Gnm::DrawCommandBuffer, and Gnm::ConstantCommandBuffer for a new frame.
			
				This function should be called at the beginning of every frame.

				The Gnm::DrawCommandBuffer and Gnm::ConstantCommandBuffer will be reset to empty (<c>m_cmdptr = m_beginptr</c>)
				All shader pointers currently cached in the Constant Update Engine will be set to <c>NULL</c>.
			*/
			void reset(void);

			/** @brief Sets the index size (16 or 32 bits).

				All future draw calls that reference index buffers will use this index size to interpret their contents.
				This function will roll the hardware context.
				@param[in] indexSize		The index size to set.
				
				@note This setting is not used by dispatchDraw(), which overrides the setting to Gnm::kIndexSize16ForDispatchDraw.  The last set value is restored by the first draw command after a dispatchDraw() call .
				@cmdsize 2
			*/
			void setIndexSize(Gnm::IndexSize indexSize)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "setIndexSize can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE((indexSize & kIndexSizeFlagDispatchDraw) == 0, "GfxContext::setIndexSize should only be called to set non-dispatchDraw IndexSize values");
				return m_dcb.setIndexSize(indexSize, Gnm::kCachePolicyBypass);
			}

			/** @brief Sets the base address where the indices are located for functions that do not specify their own indices.
				This function never rolls the hardware context.
				@param[in] indexAddr The address of the index buffer. This must be 2-byte aligned.
				@note This setting is not used by dispatchDraw(), which overrides the setting to point to the index ring buffer passed to setupDispatchDrawRingBuffers(). The last set value is restored by the first draw command after a dispatchDraw().

				@see drawIndexIndirect(), drawIndexOffset()
				@cmdsize 3
			*/
			void setIndexBuffer(const void * indexAddr)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "setIndexBuffer can't be called between beginDispatchDraw and endDispatchDraw");
				return m_dcb.setIndexBuffer(indexAddr);
			}

			/** @brief Sets the VGT (vertex geometry tessellator) primitive type.
							All future draw calls will use this primitive type.
							This function will roll the hardware context.
							@param[in] primType    The primitive type to set.
							@cmdsize 3
			*/
			void setPrimitiveType(Gnm::PrimitiveType primType)
			{
				SCE_GNM_ASSERT_MSG_INLINE(primType == Gnm::kPrimitiveTypeTriList || (m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "setPrimitiveType can't be called between beginDispatchDraw and endDispatchDraw");
				return m_dcb.setPrimitiveType(primType);
			}

			/** @brief Sets the number of instances for subsequent draw commands.
				This function never rolls the hardware context.
				@param[in] numInstances The number of instances to render for subsequent draw commands.
								  The minimum value is 1; if 0 is passed, it will be treated as 1.
				@cmdsize 2
			*/
			void setNumInstances(uint32_t numInstances)
			{
				SCE_GNM_ASSERT_MSG_INLINE(numInstances <= 1 || (m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "setNumInstances can't be called between beginDispatchDraw and endDispatchDraw");
				return m_dcb.setNumInstances(numInstances);
			}

			/** @brief Resets multiple vertex geometry tessellator (VGT) configurations to default values.

			This function will roll the hardware context.
			@cmdsize 3
			*/
			void resetVgtControl()
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "resetVgtControl can't be called between beginDispatchDraw and endDispatchDraw");
				return m_dcb.resetVgtControl();
			}

			/** @brief Specifies information for multiple vertex geometry tessellator (VGT) configurations.

				This function will roll the hardware context.

				@param[in] primGroupSizeMinus1		The number of primitives sent to one VGT block before switching to the next block. This argument has an implied +1 value. That is, a value of <c>0</c> means 1 primitive per group, and a value of <c>255</c> means 256 primitives per group.
													For tessellation, set <c><i>primGroupSizeMinusOne</i></c> to (number of patches per thread group) - <c>1</c>. In NEO mode, the recommended value of <c>primGroupSizeMinus1</c> is 127.
				@param[in] partialVsWaveMode		Specifies whether to enable partial VS waves. If enabled, then the VGT will issue a VS-stage wavefront as soon as a primitive group is finished; otherwise, the VGT will continue a VS-stage wavefront from one primitive group to the next
			                         				primitive group within a draw call. Partial VS waves must be enabled for streamout.
				@note This setting is not used by dispatchDraw(), which overrides the setting to <c>(129, kVgtPartialVsWaveEnable)</c>.  The last set value is restored by the first draw command after a dispatchDraw() call.
				@cmdsize 3
			*/
			SCE_GNM_API_CHANGED_NOINLINE
			void setVgtControl(uint8_t primGroupSizeMinus1, Gnm::VgtPartialVsWaveMode partialVsWaveMode)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "setVgtControl can't be called between beginDispatchDraw and endDispatchDraw");
				return m_dcb.setVgtControl(primGroupSizeMinus1, partialVsWaveMode);
			}

			/** @brief Specifies information for multiple vertex geometry tessellator (VGT) configurations.
				This function will roll the hardware context.
				@param[in] primGroupSizeMinusOne	Number of primitives sent to one VGT block before switching to the next block. This argument has an implied +1 value. That is, a value of 0 means 1 primitive per group, and a value of 255 means 256 primitives per group.
													For tessellation, set <c><i>primGroupSizeMinusOne</i></c> to (number of patches per thread group) - <c>1</c>.
				@note This setting is not used by dispatchDraw(), which overrides the setting to <c>(129)</c>.  The first draw command after a dispatchDraw() call restores the value set previously.
				@cmdsize 3
			*/
			void setVgtControl(uint8_t primGroupSizeMinusOne)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "setVgtControl can't be called between beginDispatchDraw and endDispatchDraw");
				return m_dcb.setVgtControl(primGroupSizeMinusOne);
			}

			/** @deprecated The <c>partialVsWaveMode</c> parameter has been removed.
			@brief Specifies information for multiple vertex geometry tessellator (VGT) configurations.

			This function will roll the hardware context.
				@param[in] primGroupSizeMinusOne	Number of primitives sent to one VGT block before switching to the next block. This argument has an implied +1 value. That is, a value of 0 means 1 primitive per group, and a value of 255 means 256 primitives per group.
													For tessellation, set <c><i>primGroupSizeMinusOne</i></c> to (number of patches per thread group) - <c>1</c>.
				@param[in] partialVsWaveMode		Specifies whether to enable partial VS waves. If enabled, then the VGT will issue a VS-stage wavefront as soon as a primitive group is finished; otherwise, the VGT will continue a VS-stage wavefront from one primitive group to the next
													primitive group within a draw call. Partial VS waves must be enabled for streamout.
				@cmdsize 3
			*/
			void setVgtControlForBase(uint8_t primGroupSizeMinusOne, Gnm::VgtPartialVsWaveMode partialVsWaveMode)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "setVgtControl can't be called between beginDispatchDraw and endDispatchDraw");
				return m_dcb.setVgtControlForBase(primGroupSizeMinusOne, partialVsWaveMode);
			}

			/** @brief Specifies information for multiple vertex geometry tessellator (VGT) configurations.

			This function will roll the hardware context.

			@param[in] primGroupSizeMinusOne	Number of primitives sent to one VGT block before switching to the next block. This argument has an implied +1 value. That is, a value of 0 means 1 primitive per group, and a value of 255 means 256 primitives per group.
												For tessellation, set <c><i>primGroupSizeMinusOne</i></c> to (number of patches per thread group) - <c>1</c>.
			@param[in] partialVsWaveMode		Specifies whether to enable partial VS waves. If enabled, then the VGT will issue a VS-stage wavefront as soon as a primitive group is finished; otherwise, the VGT will continue a VS-stage wavefront from one primitive group to the next
												primitive group within a draw call. Partial VS waves must be enabled for streamout.
			@param[in] wdSwitchOnlyOnEopMode	If enabled (<c>kVgtSwitchOnEopEnable</c>), then the WD will send an entire draw call to one IA-plus-VGT pair, which will then behave as in base mode. If disabled, then the WD will split the draw call on instance boundaries and then send <c><c><i>primGroupSize</i></c>*2</c> (or the remainder) primitives to each IA.
			@cmdsize 3
			*/
			void setVgtControlForNeo(uint8_t primGroupSizeMinusOne, Gnm::WdSwitchOnlyOnEopMode wdSwitchOnlyOnEopMode, Gnm::VgtPartialVsWaveMode partialVsWaveMode)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "setVgtControl can't be called between beginDispatchDraw and endDispatchDraw");
				return m_dcb.setVgtControlForNeo(primGroupSizeMinusOne, wdSwitchOnlyOnEopMode, partialVsWaveMode);
			}

			/** @brief Writes the specified 64-bit value to the given location in memory when this command reaches the end of the processing pipe (EOP).

				This function never rolls the hardware context.

				@param[in] eventType			Determines when <c><i>immValue</i></c> will be written to the specified address.
				@param[out] dstGpuAddr			The GPU address to which the given value will be written. This address must be 8-byte aligned and must not be set to <c>NULL</c>.
				@param[in] immValue				The value that will be written to <c><i>dstGpuAddr</i></c>.
				@param[in] cacheAction			Specifies which caches to flush and invalidate after the specified write is complete.

				@sa Gnm::DrawCommandBuffer::writeAtEndOfPipe()
				@cmdsize 6
			*/
			void writeImmediateAtEndOfPipe(Gnm::EndOfPipeEventType eventType, void *dstGpuAddr, uint64_t immValue, Gnm::CacheAction cacheAction)
			{
				return m_dcb.writeAtEndOfPipe(eventType,
												 Gnm::kEventWriteDestMemory, dstGpuAddr,
												 Gnm::kEventWriteSource64BitsImmediate, immValue,
												 cacheAction, Gnm::kCachePolicyLru);
			}

			/** @brief Writes the specified 32-bit value to the given location in memory when this command reaches the end of the processing pipe (EOP).

				This function never rolls the hardware context.

				@param[in] eventType		Determines when <c><i>immValue</i></c> will be written to the specified address.
				@param[out] dstGpuAddr		The GPU address to which the given value will be written. This address must be 4-byte aligned and must not be set to <c>NULL</c>.
				@param[in] immValue			The value that will be written to <c><i>dstGpuAddr</i></c>. This must be in the range <c>[0..0xFFFFFFFF]</c>.
				@param[in] cacheAction      Specifies which caches to flush and invalidate after the specified write is complete.
			
				@sa Gnm::DrawCommandBuffer::writeAtEndOfPipe()
				@cmdsize 6
			*/
			void writeImmediateDwordAtEndOfPipe(Gnm::EndOfPipeEventType eventType, void *dstGpuAddr, uint64_t immValue, Gnm::CacheAction cacheAction)
			{
				SCE_GNM_ASSERT_MSG_INLINE(immValue <= 0xFFFFFFFF, "immValue (0x%016lX) must be in the range [0..0xFFFFFFFF].", immValue);
				return m_dcb.writeAtEndOfPipe(eventType,
												 Gnm::kEventWriteDestMemory, dstGpuAddr,
												 Gnm::kEventWriteSource32BitsImmediate, immValue,
												 cacheAction, Gnm::kCachePolicyLru);
			}

			/** @brief Writes a resource barrier to the command stream.
				@param barrier The barrier to write. This pointer must not be <c>NULL</c>.
				*/
			void writeResourceBarrier(const ResourceBarrier *barrier)
			{
				return barrier->write(&m_dcb);
			}
			
			/** @brief Writes the GPU core clock counter to the given location in memory when this command reaches the end of the processing pipe (EOP).

				This function never rolls the hardware context.
	
				@param[in] eventType		Determines when the counter value will be written to the specified address.
				@param[out] dstGpuAddr		The GPU address to which the counter value will be written. This address must be 8-byte aligned and must not be set to <c>NULL</c>.
				@param[in] cacheAction      Specifies which caches to flush and invalidate after the specified write is complete.
			
				@sa Gnm::DrawCommandBuffer::writeAtEndOfPipe()
				@cmdsize 6
			*/
			void writeTimestampAtEndOfPipe(Gnm::EndOfPipeEventType eventType, void *dstGpuAddr, Gnm::CacheAction cacheAction)
			{
				return m_dcb.writeAtEndOfPipe(eventType,
											  Gnm::kEventWriteDestMemory, dstGpuAddr,
											  Gnm::kEventWriteSourceGpuCoreClockCounter, 0,
											  cacheAction, Gnm::kCachePolicyLru);
			}

			/** @brief Writes the specified 64-bit value to the given location in memory and triggers an interrupt when this command reaches the end of the processing pipe (EOP).

				This function never rolls the hardware context.

				@param[in] eventType		Determines when <c><i>immValue</i></c> will be written to the specified address.
				@param[out] dstGpuAddr		The GPU address to which the given value will be written. This address must be 8-byte aligned and must not be set to <c>NULL</c>.
				@param[in] immValue			The value that will be written to <c><i>dstGpuAddr</i></c>.
				@param[in] cacheAction      Specifies which caches to flush and invalidate after the specified write is complete.

				@sa Gnm::DrawCommandBuffer::writeAtEndOfPipeWithInterrupt()
				@cmdsize 6
			*/
			void writeImmediateAtEndOfPipeWithInterrupt(Gnm::EndOfPipeEventType eventType, void *dstGpuAddr, uint64_t immValue, Gnm::CacheAction cacheAction)
			{
				return m_dcb.writeAtEndOfPipeWithInterrupt(eventType,
															  Gnm::kEventWriteDestMemory, dstGpuAddr,
															  Gnm::kEventWriteSource64BitsImmediate, immValue,
															  cacheAction, Gnm::kCachePolicyLru);
			}

			/** @brief Writes the GPU core clock counter to the given location in memory and triggers an interrupt when this command reaches the end of the processing pipe (EOP).

				This function never rolls the hardware context.

				@param[in] eventType		Determines when the counter value will be written to the specified address.
				@param[out] dstGpuAddr		The GPU address to which the counter value will be written. This address must be 8-byte aligned and must not be set to <c>NULL</c>.
				@param[in] cacheAction      Specifies which caches to flush and invalidate after the specified write is complete.

				@sa Gnm::DrawCommandBuffer::writeAtEndOfPipeWithInterrupt()
				@cmdsize 6
			*/
			void writeTimestampAtEndOfPipeWithInterrupt(Gnm::EndOfPipeEventType eventType, void *dstGpuAddr, Gnm::CacheAction cacheAction)
			{
				return m_dcb.writeAtEndOfPipeWithInterrupt(eventType,
															  Gnm::kEventWriteDestMemory, dstGpuAddr,
															  Gnm::kEventWriteSourceGpuCoreClockCounter, 0,
															  cacheAction, Gnm::kCachePolicyLru);
			}

			/**
			 * @brief Sets the layout of LDS area where data will flow from the GS to the VS stages when on-chip geometry shading is enabled.
			 *
			 * This sets the same context register state as <c>setGsVsRingBuffers(NULL, 0, vtxSizePerStreamInDword, maxOutputVtxCount)</c>, but does not modify the global resource table.
			 *
			 * This function will roll hardware context.
			 *
			 * @param[in] vtxSizePerStreamInDword		The stride of GS-VS vertices for each of four streams in <c>DWORD</c>s, which must match <c>GsShader::m_memExportVertexSizeInDWord[]</c>. This pointer must not be <c>NULL</c>.
			 * @param[in] maxOutputVtxCount				The maximum number of vertices output from the GS stage, which must match GsShader::m_maxOutputVertexCount. Must be in the range [0..1024].
			 * @see setGsMode(), setGsModeOff(), computeOnChipGsConfiguration(), setGsOnChipControl(), Gnm::DrawCommandBuffer::setupGsVsRingRegisters()
			 */
			void setOnChipGsVsLdsLayout(const uint32_t vtxSizePerStreamInDword[4], uint32_t maxOutputVtxCount);

			/** @brief Turns off the Gs Mode.
				
				The function will roll the hardware context if it is different from current state.
				
				@note  Prior to moving back to a non-GS pipeline, you must call this function in addition to calling setShaderStages().

				@see Gnm::DrawCommandBuffer::setGsMode() 
			 */
			void setGsModeOff()
			{
				return m_dcb.disableGsMode();
			}
#if SCE_GNMX_RECORD_LAST_COMPLETION
			protected:
			/** @brief Values that initialize the command-completion recording feature and specify recording options.
					These values specify whether to record the last successfully completed draw or dispatch completed. Those values which enable recording also specify recording options. 
					Command-completion recording is useful for debugging GPU crashes.
				@sa initializeRecordLastCompletion()
			*/
			typedef enum RecordLastCompletionMode
			{
				kRecordLastCompletionDisabled,     ///< Do not record the offset of the last draw or dispatch that completed.
				kRecordLastCompletionAsynchronous, ///< Write the command buffer offset of the draw or dispatch at EOP to a known offset in the DrawCommandBuffer.
				kRecordLastCompletionSynchronous,  ///< Write the command buffer offset of the draw or dispatch at EOP to a known offset in the DrawCommandBuffer and wait for this value to be written before proceeding to the next draw or dispatch.
			} RecordLastCompletionMode;
			RecordLastCompletionMode m_recordLastCompletionMode; ///< Values that initialize the command-completion recording feature and specify recording options.

			/** @brief Enables or disables command-completion recording and specifies recording options. You can use command-completion recording to investigate the cause of a GPU crash. 
			
				The <c>level</c> command-completion recording mode specifies whether to record commands or not; those values which enable recording also specify how to record 
				the most recently completed draw or dispatch command. The default instrumentation level is <c>kRecordLastCompletionDisabled</c>. 
				
				If <c><i>level</i></c> is not <c>kRecordLastCompletionDisabled</c>, you must call this function after <c>reset()</c> and before the first draw or dispatch. 
				If the instrumentation <c><i>level</i></c> is not <c>kRecordLastCompletionDisabled</c>, each draw or dispatch call is instrumented to write its own command buffer offset to a label upon completion. 
				Optionally, each write can stall the CP until the label has been updated. 

				@note The command-completion recording feature is available only in applications that were compiled with the <c>SCE_GNMX_RECORD_LAST_COMPLETION</c> define 
				set to a non-zero value.  If the <c>SCE_GNMX_RECORD_LAST_COMPLETION</c> define is non-zero at compile time (default value is <c>0</c>), Gnmx becomes capable 
				of recording in memory the command buffer offset of the last draw or dispatch that completed successfully. This feature is useful for debugging 
				GPU crashes, because the command that is likely to have crashed the GPU is the one following the last command that that completed successfully.
				@note Enabling command-completion recording has the consequence that every draw or dispatch will be preceded and followed by several extra CPU instructions, 
				including branches. Simply enabling the <c>#define</c>, however, does not cause Gnmx to record the command buffer offset of the last successfully completed draw or dispatch. 
				To enable command-completion recording, you must also call GfxContext::initializeRecordLastCompletion(), passing to it a <c>level</c> value that enables recording. 

				@param[in] level The record last completion mode to set. One of the following <c>RecordLastCompletionMode</c> enumerated values:<br>
						<c>kRecordLastCompletionDisabled</c><br>Do not record the offset of the last draw or dispatch that completed. This is the default behavior.<br>
						<c>kRecordLastCompletionAsynchronous</c><br>Write command buffer offset of each successful draw or dispatch to a known offset in the <c>DrawCommandBuffer</c>. In this mode, draws and dispatches are still issued asynchronously, and may overlap.<br>
						<c>kRecordLastCompletionSynchronous</c><br>Write command buffer offset of draw or dispatch at EOP to known offset in the <c>DrawCommandBuffer</c>, and wait for this value to be written before proceeding to the next draw or dispatch. In this mode, only one draw or dispatch will be issued at a time; this feature makes it easier to isolate problematic commands, but it may change the application’s behavior and timing.
			*/
			void initializeRecordLastCompletion(RecordLastCompletionMode level);

			/** @brief Begins the instrumentation process for a draw or a dispatch.

				This function calculates the byte offset of a subsequent draw or dispatch so that you can instrument it later.
				@return The byte offset from the beginning of the draw command buffer to the draw or dispatch to be instrumented.
			*/
			uint32_t beginRecordLastCompletion() const
			{
				return static_cast<uint32_t>( (m_dcb.m_cmdptr - m_dcb.m_beginptr) * sizeof(uint32_t) );
			}

			/** @brief Ends the instrumentation process for a draw or a dispatch.
				
				       This function adds commands to write the byte offset of a draw or dispatch in the draw command buffer to a space near the start of the draw command buffer.
				       This function may stall the GPU until this value is written.
				@param[in] offset The byte offset from the beginning of the draw command buffer to the draw or dispatch to be instrumented.
			*/
			void endRecordLastCompletion(uint32_t offset)
			{
				if(m_recordLastCompletionMode != kRecordLastCompletionDisabled)
				{
					writeImmediateDwordAtEndOfPipe(Gnm::kEopFlushCbDbCaches, m_addressOfOffsetOfLastCompletion, offset, Gnm::kCacheActionWriteBackAndInvalidateL1andL2);
					if(m_recordLastCompletionMode == kRecordLastCompletionSynchronous)
					{
						waitOnAddress(m_addressOfOffsetOfLastCompletion, 0xFFFFFFFF, Gnm::kWaitCompareFuncEqual, offset);
					}
				}
			}
			
			/** @brief The address at which the command buffer offset of the most recently completed draw/dispatch in the DCB is stored when that draw/dispatch completes.
			
				       This value is helpful for identifying the call that caused the GPU to crash.
				@sa RecordLastCompletionMode
			*/
			uint32_t *m_addressOfOffsetOfLastCompletion;
			public:
#endif // SCE_GNMX_RECORD_LAST_COMPLETION
			/** @brief Sets up context state and synchronization for dispatch draw and must be called before issuing a block of dispatchDraw() calls.

				This function inserts a CP label write into the DrawCommandBuffer and a corresponding wait into the asynchronous compute
				DispatchCommandBuffer. This is done to prevent dispatchDraw asynchronous compute wavefronts from being launched long before the DrawCommandBuffer
				launches the vertex wavefronts to consume their output data.  If the index ring buffer has insufficient space for all of the output from
				all launched compute wavefronts, any wavefronts which do not find available space will stall and tie up shading resources until 
				the consumer wavefronts launch.

				This function will roll context and modifies context state associated with setIndexSize(), setIndexBuffer(), setNumInstances(), setPrimitiveType(), and setVgtControl().
				setIndexSize(), setIndexBuffer(), setPrimitiveType(), setNumInstances(), setVgtControl(), or any non-dispatchDraw draw functions may not be called between beginDispatchDraw()
				and endDispatchDraw().

				@param[in] primGroupSizeMinus1 One less than the primitive group size passed to setVgtControl(). In NEO mode, the recommended value of <c>primGroupSizeMinus1</c> is 127.
				@note <c><i>m_acb</i>.setComputeResourceManagement()</c> should also be used to ensure that there is at least one compute unit for System Software shaders
				      to use in the event that System Software interrupts in the middle of a dispatchDraw() call.  Dispatch draw compute shader wavefronts are 
					  not guaranteed to complete once launched. This is because they may stall until the consumer vertex shader wavefronts have launched
					  to free up space if the index ring buffer is full. Limiting dispatchDraw() computes to run on 8 of the 9 compute units in 
					  each SE is sufficient to prevent deadlocks against System Software rendering.
				@note Unless the compute dispatches are guaranteed not to fill the ring buffers (and halt) or are limited to no more than 1 thread group per CU,
				      tessellation or on-chip GS draw commands cannot be overlapped by DispatchCommandBuffer::dispatchDraw() compute dispatches.
				      A GPU hang can result if a tessellation or on-chip GS thread group launches on a CU that is already hosting more than 1 thread group
				      of compute, which is waiting for space in the dispatch draw ring buffers. In general, if any dispatchDraw commands will follow, it is safer
					  to prevent all overlap of dispatch draw compute with tessellation or on-chip GS draw commands by calling
					  GfxContext::dispatchDrawComputeWaitForEndOfPipe() after the last tessellation or on-chip GS draw command.
			 */
			void beginDispatchDraw(uint8_t primGroupSizeMinus1);

			/** @brief Sets up context state and synchronization for dispatch draw; must be called before issuing a block of dispatchDraw() calls.

				This function inserts a CP label write into the DrawCommandBuffer and a corresponding wait into the asynchronous compute
				DispatchCommandBuffer. Doing so prevents dispatchDraw() asynchronous compute wavefronts from being launched before the DrawCommandBuffer
				launches the vertex wavefronts that consume their output data.  If the index ring buffer has insufficient space for all of the output from
				all launched compute wavefronts, any wavefronts that do not find available space will stall, which ties up shading resources until 
				the consumer wavefronts launch.

				This function will roll context. It modifies context state associated with setIndexSize(), setIndexBuffer(), setNumInstances(), setPrimitiveType() and setVgtControl().
				Do not call setIndexSize(), setIndexBuffer(), setPrimitiveType(), setNumInstances(), setVgtControl() or any non-dispatchDraw draw functions between calls to beginDispatchDraw()
				and endDispatchDraw().

				@note This variant assumes a <c><i>primGroupSizeMinus1</i></c> value of <c>127</c>.
				@note <c><i>m_acb</i>.setComputeResourceManagement()</c> must also be used to ensure that there is at least one compute unit for System Software shaders
					  to use in the event that System Software interrupts in the middle of a dispatchDraw() call.  Dispatch draw compute shader wavefronts are 
					  not guaranteed to complete once launched. If the index ring buffer is full, dispatch draw compute shader wavefronts may stall until the launch of consumer vertex shader wavefronts
					  frees space . Limiting dispatchDraw() computes to run on 8 of the 9 compute units in 
					  each SE is sufficient to prevent deadlocks against System Software rendering.
				@note Unless the compute dispatches are guaranteed not to fill the ring buffers (and halt) or are limited to no more than one thread group per CU,
					  DispatchCommandBuffer::dispatchDraw() compute dispatches cannot overlap tessellation or on-chip GS draw commands.
					  A GPU hang can result if a tessellation or on-chip GS thread group launches on a CU that is already hosting more than one thread group
					  of compute that is waiting for space in the dispatch draw ring buffers. In general, if any dispatchDraw commands will follow, it is safer
					  to prevent all overlap of dispatch draw compute with tessellation or on-chip GS draw commands by calling
					  GfxContext::dispatchDrawComputeWaitForEndOfPipe() after the last tessellation or on-chip GS draw command.
			 */
			inline void beginDispatchDraw() { beginDispatchDraw(128); }

			/** @brief Restores the context state and must be called after issuing a block of dispatchDraw() calls.

				This function will roll context and modifies the context state associated with setIndexSize(), setIndexBuffer(), and setVgtControl().
				After a beginDispatchDraw/endDispatchDraw block, the value of setPrimitiveType() is always left set to kPrimitiveTypeTriList and 
				setNumInstances() is always left set to 1.

				@param[in] indexSize				The Gnm::IndexSize value to restore.
				@param[in] indexAddr				The index buffer pointer to restore.
				@param[in] primGroupSizeMinus1		The setVgtControl() value to restore; the default setting for normal rendering is 255. The default setting for NEO mode is 127.
				@param[in] partialVsWaveMode		The setVgtControl() value to restore; the default setting for normal rendering is kVgtPartialVsWaveDisable.
			 */
			void endDispatchDraw(Gnm::IndexSize indexSize, const void *indexAddr, uint8_t primGroupSizeMinus1, Gnm::VgtPartialVsWaveMode partialVsWaveMode);

			/** @brief Restores context state with default setVgtControl() settings and must be called after issuing a block of dispatchDraw() to restore context state.

				This function will roll context and modifies the context state associated with setIndexSize(), setIndexBuffer(), and setVgtControl(). It restores state to
				<c>setIndexSize(indexSize)</c>, <c>setIndexBuffer(indexAddr)</c> and <c>setVgtControl(255, Gnm::kVgtPartialVsWaveDisable)</c>.
				After a beginDispatchDraw/endDispatchDraw block, the value of setPrimitiveType() is always left set to kPrimitiveTypeTriList and 
				setNumInstances() is always left set to 1.

				@param[in] indexSize				The Gnm::IndexSize value to restore.
				@param[in] indexAddr				The index buffer pointer to restore.
			 */
			inline void endDispatchDraw(Gnm::IndexSize indexSize, const void *indexAddr)
			{
				endDispatchDraw(indexSize, indexAddr, 255, Gnm::kVgtPartialVsWaveDisable);
			}

			/** @brief Restores context state with default setVgtControl() settings and must be called after issuing a block of dispatchDraw() to restore context state.

				This function will roll context and modifies the context state associated with setIndexSize(), setIndexBuffer() and setVgtControl(). It restores state to
				<c>setIndexSize(kIndexSize16)</c>, <c>setIndexBuffer(NULL)</c> and <c>setVgtControl(255, Gnm::kVgtPartialVsWaveDisable)</c>.
				After a beginDispatchDraw/endDispatchDraw block, the value of setPrimitiveType() is always left set to kPrimitiveTypeTriList and 
				setNumInstances() is always left set to 1.
			 */
			inline void endDispatchDraw()
			{
				endDispatchDraw(Gnm::kIndexSize16, NULL, 255, Gnm::kVgtPartialVsWaveDisable);
			}

			/** @brief Inserts an EOP label write in the DrawCommandBuffer and a corresponding DispatchCommandBuffer::waitOnAddress() in the asynchronous compute buffer.
						This function never rolls the hardware context.
			 
				@note Unless the compute dispatches are guaranteed not to fill the ring buffers (and halt) or are limited to no more than 1 thread group per CU,
				      tessellation or on-chip GS draw commands cannot be overlapped by DispatchCommandBuffer::dispatchDraw() compute dispatches.
				      A GPU hang can result if a tessellation or on-chip GS thread group launches on a CU that is already hosting more than 1 thread group
				      of compute, which is waiting for space in the dispatch draw ring buffers. In general, if any dispatchDraw commands will follow, it is safer
					  to prevent all overlap of dispatch draw compute with tessellation or on-chip GS draw commands by calling
					  GfxContext::dispatchDrawComputeWaitForEndOfPipe() after the last tessellation or on-chip GS draw command.			   
				
				@see dispatchDraw()
			 */
			void dispatchDrawComputeWaitForEndOfPipe();

			//////////// Wrappers around Gnmx helper functions

			/** @brief Sets the render target for the specified RenderTarget slot.

				This wrapper automatically works around a rare hardware quirk involving CMASK cache
				corruption with multiple render targets that have identical FMASK pointers (for example, if all of them are <c>NULL</c>).

				This function will roll the hardware context.

				@param[in] rtSlot The render target slot index to which this render target is set to (0-7).
				@param[in] target A pointer to a Gnm::RenderTarget structure. If <c>NULL</c>is passed, then the color buffer for this slot is disabled.
				@sa Gnm::RenderTarget::disableFmaskCompressionForMrtWithCmask()
			*/
			void setRenderTarget(uint32_t rtSlot, Gnm::RenderTarget const *target);

			/** @brief Uses the command processor's DMA engine to clear a buffer to specified value (such as a GPU <c>memset()</c>).
				
				This function never rolls the hardware context.
				
				@param[out] dstGpuAddr		The destination address to which this function writes data. Must not be <c>NULL</c>.
				@param[in] srcData			The value with which to fill the destination buffer.
				@param[in] numBytes			The size of the destination buffer. This value must be a multiple of 4.
				@param[in] isBlocking		If <c>true</c>, the command processor will block while the transfer is active
				@note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for DrawCommandBuffer::dmaData() regarding a potential stall of the command processor that may occur when multiple DMAs are in flight.
				@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
				@note  This feature does not affect the GPU <c>L2$</c> cache.
				@see  DrawCommandBuffer::dmaData()
			*/
			void fillData(void *dstGpuAddr, uint32_t srcData, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
			{
				return Gnmx::fillData(&m_dcb, dstGpuAddr, srcData, numBytes, isBlocking);
			}

			/** @brief Uses the command processor's DMA engine to transfer data from a source address to a destination address.
				
				This function never rolls the hardware context.
				
				@param[out] dstGpuAddr			The destination address to which this function writes data. Must not be <c>NULL</c>. 
				@param[in] srcGpuAddr			The source address from which this function reads data. Must not be <c>NULL</c>. 
				@param[in] numBytes				The number of bytes to transfer from <c><i>srcGpuAddr</i></c> to <c><i>dstGpuAddr</i></c>. Must be a multiple of <c>4</c>.
				@param[in] isBlocking			If <c>true</c>, the CP waits for the DMA to be complete before performing any more processing.
				@note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for DrawCommandBuffer::dmaData() regarding a potential stall of the command processor that may occur when multiple DMAs are in flight.
				@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
				@note  This feature does not affect the GPU <c>L2$</c> cache.
				@see   DrawCommandBuffer::dmaData()
			*/
			void copyData(void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
			{
				return Gnmx::copyData(&m_dcb, dstGpuAddr, srcGpuAddr, numBytes, isBlocking);
			}

			/** @brief Inserts user data directly inside the command buffer and returns a locator for later reference.

				This function never rolls the hardware context.

				@param[in] dataStream		A pointer to the data.
				@param[in] sizeInDword		The size of the data in a stride of 4. Note that the maximum size of a single command packet is 2^16 bytes,
											and the effective maximum value of <c><i>sizeInDword</i></c> will be slightly less than that due to packet headers
											and padding.
				@param[in] alignment		An alignment of the embedded copy in the command buffer.

				@return            A pointer to the allocated buffer.
			*/
			void* embedData(const void *dataStream, uint32_t sizeInDword, Gnm::EmbeddedDataAlignment alignment)
			{
				return Gnmx::embedData(&m_dcb, dataStream, sizeInDword, alignment);
			}

			/** @brief Sets the multisampling sample locations to default values.
					   This function also calls setAaSampleCount() to set the sample count and maximum sample distance.
				This function will roll hardware context.
				
				@param[in] numSamples The number of samples used while multisampling.
				DrawCommandBuffer::setAaSampleCount()
			*/
			void setAaDefaultSampleLocations(Gnm::NumSamples numSamples)
			{
				return Gnmx::setAaDefaultSampleLocations(&m_dcb, numSamples);
			}

			/** @brief A utility function that configures the viewport, scissor, and guard band for the provided viewport dimensions.

				If more control is required, users can call the underlying functions manually.

				This function will roll hardware context.

				@param[in] left		The X coordinate of the left edge of the rendering surface in pixels.
				@param[in] top		The Y coordinate of the top edge of the rendering surface in pixels.
				@param[in] right	The X coordinate of the right edge of the rendering surface in pixels.
				@param[in] bottom	The Y coordinate of the bottom edge of the rendering surface in pixels.
				@param[in] zScale	The scale value for the Z transform from clip-space to screen-space. The correct value depends on which
									convention you are following in your projection matrix. For OpenGL-style matrices, use <c>zScale=0.5</c>. For Direct3D-style
									matrices, use <c>zScale=1.0</c>.
				@param[in] zOffset	The offset value for the Z transform from clip-space to screen-space. The correct value depends on which
									convention you're following in your projection matrix. For OpenGL-style matrices, use <c>zOffset=0.5</c>. For Direct3D-style
									matrices, use <c>zOffset=0.0</c>.
			*/
			void setupScreenViewport(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, float zScale, float zOffset)
			{
				return Gnmx::setupScreenViewport(&m_dcb, left, top, right, bottom, zScale, zOffset);
			}
			
			/** @brief A utility function that configures dispatch draw shaders for the given viewport dimensions and guard band.

				This function will not hardware context.

				@param[in] left			The X coordinate of the left edge of the rendering surface in pixels.
				@param[in] top			The Y coordinate of the top edge of the rendering surface in pixels.
				@param[in] right		The X coordinate of the right edge of the rendering surface in pixels.
				@param[in] bottom		The Y coordinate of the bottom edge of the rendering surface in pixels.
			*/
			void setupDispatchDrawScreenViewport(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);

			/** @brief A helper function that configures DispatchDraw shaders for the given clip and cull settings.

				This function will not roll the hardware context.

				@param[in] primitiveSetup The value currently set to setPrimitiveSetup().
				@param[in] clipControl The value currently set to setClipControl().
			*/
			void setupDispatchDrawClipCullSettings(Gnm::PrimitiveSetup primitiveSetup, Gnm::ClipControl clipControl);

			/** A helper function that configures DispatchDraw shaders for the given clip and cull settings.

				This function will not roll the hardware context.
				@param[in] dispatchDrawClipCullFlags A union of <c>Gnm::kDispatchDrawClipCullFlag*</c> flags controlling DispatchDraw shader clipping and culling. 
			*/
			void setupDispatchDrawClipCullSettings(uint32_t dispatchDrawClipCullFlags)
			{
				if ((m_dispatchDrawSharedData.m_cullSettings & kDispatchDrawClipCullMask) != (dispatchDrawClipCullFlags & kDispatchDrawClipCullMask)) {
					m_dispatchDrawSharedData.m_cullSettings = (uint16_t)((m_dispatchDrawSharedData.m_cullSettings &~kDispatchDrawClipCullMask) | (dispatchDrawClipCullFlags & kDispatchDrawClipCullMask));
					m_pDispatchDrawSharedData = NULL;
				}
			}

			/** Sets the number of instances for subsequent dispatchDraw() calls and is analogous to setNumInstances() for standard draws.

				Vertex input attributes using index kFetchShaderUseInstanceId will read their vertex buffers using the instance ID rather than the
				vertex index. The instance ID is an alternate index for vertex buffer reads that increments from 0 to <c>numInstances-1</c> with all
				vertices in each instance passed the same instance ID.

				Dispatch draw is limited to 2^16 instances per GfxContext::dispatchDraw() call. GfxContext::dispatchDraw() may issue many pairs of 
				DrawCommandBuffer::dispatchDraw() and DispatchCommandBuffer::dispatchDraw() calls in order to render the total requested number of
				instances, but it will only update the constant update engine (CUE) once for all of the calls.

				This function will not roll the hardware context.
				
				@param[in] numInstances		The number of instances to render in the range <c>[0:65536]</c> with 0 interpreted as 1.
			*/
			void setDispatchDrawNumInstances(uint32_t numInstances)
			{
				SCE_GNM_ASSERT_MSG_INLINE(numInstances <= 0x10000, "setDispatchDrawNumInstances: numInstances must be in the range [0:1<<16] (0:65536)");
				if (numInstances == 0 || numInstances > 0x10000)
					numInstances = 1;
				m_dispatchDrawNumInstancesMinus1 = (uint16_t)(numInstances-1);
			}

			/** Sets the instance step rates for subsequent dispatchDraw calls and is analogous to setInstanceStepRate() for standard draws.

				Vertex input attributes using index kFetchShaderUseInstanceIdOverStepRate0 or kFetchShaderUseInstanceIdOverStepRate1
				will read their vertex buffers using the instance ID divided by <c><i>stepRate0</i></c> or <c><i>stepRate1</i></c> rather than the
				vertex index. The instance ID is an alternate index for vertex buffer reads that increments from 0 to <c>numInstances-1</c> with all
				vertices in each instance passed the same instance ID.

				This function will not roll the hardware context.

				@param[in] stepRate0		The step rate for vertex input attributes using index kFetchShaderUseInstanceIdOverStepRate0 in the range <c>[0:65536]</c> with 0 interpreted as 1.
				@param[in] stepRate1		The step rate for vertex input attributes using index kFetchShaderUseInstanceIdOverStepRate1 in the range <c>[0:65536]</c> with 0 interpreted as 1.
			*/
			void setDispatchDrawInstanceStepRate(uint32_t stepRate0, uint32_t stepRate1)
			{
				SCE_GNM_ASSERT_MSG_INLINE(stepRate0 <= 0x10000 && stepRate1 <= 0x10000, "setDispatchDrawInstanceStepRate: stepRate0 and stepRate1 must be in the range [0:1<<16] (0:65536)");
				if (stepRate0 == 0 || stepRate0 > 0x10000)
					stepRate0 = 1;
				if (stepRate1 == 0 || stepRate1 > 0x10000)
					stepRate1 = 1;
				m_dispatchDrawInstanceStepRate0Minus1 = (uint16_t)(stepRate0-1);
				m_dispatchDrawInstanceStepRate1Minus1 = (uint16_t)(stepRate1-1);
			}

			/**
			 * @brief A wrapper around <c>dmaData()</c> to clear the values of one or more append or consume buffer counters to the specified value.
			 *
			 *	This function never rolls the hardware context.
			 *
			 * @param[in] destRangeByteOffset		The byte offset in GDS to the beginning of the counter range to clear. This must be a multiple of 4.
			 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be updated. The valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>.
			 * @param[in] numApiSlots				The number of consecutive slots to update. <c>startApiSlot + numApiSlots</c> must be less than or equal to Gnm::kSlotCountRwResource.
			 * @param[in] clearValue				The value to set the specified counters to.
			 *
			 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
			 *
			 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
			 * @note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for DrawCommandBuffer::dmaData() regarding a potential stall of the command processor that may occur when multiple DMAs are in flight.
			 * @note  To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
			 */
			void clearAppendConsumeCounters(uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, uint32_t clearValue)
			{
				return Gnmx::clearAppendConsumeCounters(&m_dcb, destRangeByteOffset, startApiSlot, numApiSlots, clearValue);
			}

			/**
			 * @brief A wrapper around <c>dmaData()</c> to update the values of one or more append or consume buffer counters using values sourced from the provided GPU-visible address.
			 *
			 * This function never rolls the hardware context.
			 *
			 * @param[in] destRangeByteOffset				The byte offset in the GDS to the beginning of the counter range to update. This must be a multiple of 4.
			 * @param[in] startApiSlot						The index of the first <c>RW_Buffer</c> API slot whose counter should be updated. The valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>.
			 * @param[in] numApiSlots						The number of consecutive slots to update. <c>startApiSlot + numApiSlots</c> must be less than or equal to Gnm::kSlotCountRwResource.
			 * @param[in] srcGpuAddr						The GPU-visible address to read the new counter values from.
			 *
			 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
			 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
			 * @note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for DrawCommandBuffer::dmaData() regarding a potential stall of the command processor that may occur when multiple DMAs are in flight.
			 * @note  To avoid unintended data corruption, ensure that the GDS ranges this function uses do not overlap other, unrelated GDS ranges.
			 */
			void writeAppendConsumeCounters(uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, const void *srcGpuAddr)
			{
				return Gnmx::writeAppendConsumeCounters(&m_dcb, destRangeByteOffset, startApiSlot, numApiSlots, srcGpuAddr);
			}

			/**
			 * @brief A wrapper around <c>dmaData()</c> to retrieve the values of one or more append or consume buffer counters and store them in a GPU-visible address.
			 *
			 * This function never rolls the hardware context.
			 *
			 * @param[out] destGpuAddr			The GPU-visible address to write the counter values to.
			 * @param[in] srcRangeByteOffset	The byte offset in GDS to the beginning of the counter range to read. This must be a multiple of 4.
			 * @param[in] startApiSlot			The index of the first RW_Buffer API slot whose counter should be read. The valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>.
			 * @param[in] numApiSlots			The number of consecutive slots to read. <c>startApiSlot + numApiSlots</c> must be less than or equal to Gnm::kSlotCountRwResource.
			 *
			 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
			 *
			 * @note  The GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
			 * @note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for DrawCommandBuffer::dmaData() regarding a potential stall of the command processor that may occur when multiple DMAs are in flight.
			 * @note  To avoid unintended data corruption, ensure that the GDS ranges this function uses do not overlap other, unrelated GDS ranges.
			 */
			void readAppendConsumeCounters(void *destGpuAddr, uint32_t srcRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots)
			{
				return Gnmx::readAppendConsumeCounters(&m_dcb, destGpuAddr, srcRangeByteOffset, startApiSlot, numApiSlots);
			}
#ifdef SCE_GNMX_ENABLE_GFXCONTEXT_CALLCOMMANDBUFFERS // Disabled by default until a future release
			/** @brief Calls another command buffer pair. When the called command buffers end, execution will continue
			           in the current command buffer.
					   
				@param dcbBaseAddr			The address of the destination dcb. This pointer must not be <c>NULL</c> if <c><i>dcbSizeInDwords</i></c> is non-zero.
				@param dcbSizeInDwords		The size of the destination dcb in DWORDs. This may be set to 0 if no dcb is required.
				@param ccbBaseAddr			The address of the destination ccb. This pointer must not be <c>NULL</c> if <c><i>ccbSizeInDwords</i></c> is non-zero.
				@param ccbSizeInDwords		The size of the destination ccb in DWORDs. This may be 0 if no ccb is required.

				@note After this function is called, all previous resource/shader bindings and render state are undefined.
				@note  Calls can only recurse one level deep. The command buffer submitted to the GPU can call another command buffer, but the callee cannot contain
				       any call commands of its own.
			*/
			void callCommandBuffers(void *dcbAddr, uint32_t dcbSizeInDwords, void *ccbAddr, uint32_t ccbSizeInDwords);
#endif

			/** @brief Indicates the beginning or end of a predication region of command data that the GPU skips if the value at the <c>condAddr</c> address is <c>0</c>.

			@param[in] condAddr The address of the 32-bit predication condition. Must be 4-byte aligned and must not be <c>NULL</c>. The GPU reads the contents of this address when it executes a packet.
			<ul>
			<li>A non-<c>NULL</c> <c>condAddr</c> value identifies the beginning of the current packet's predication region. </li>
			<li>A <c>NULL</c>-value <c>condAddr</c> identifies the end of the current predication region.</li>
			<li>If <c>*condAddr == 0</c>, the GPU skips all command data in this packet's predication region.</li>
			<li>If <c>*condAddr != 0</c>, the GPU executes all command data in this packet's predication region.</li>
			</ul>

			@note You must end each predication region explicitly by calling <c>setPredication(NULL)</c>.
			A predication region cannot straddle a command-buffer boundary.
			It is illegal to begin a new predication region inside of an active predication region.
			@cmdsize 5
			*/
			void setPredication(void *condAddr);

			// auto-generated method forwarding to m_dcb
			#include "gfxcontext_methods.h"



			/** @brief Splits the command buffer into multiple submission ranges.
					   
				This function will cause the GfxContext to stop adding commands to the active submission range and create a new one.
				This is necessary for when chainCommandBuffer() is called, which must be the last packet in a DCB range,
				or if the DCB has reached its 4MB limit (Gnm::kIndirectBufferMaximumSizeInBytes).

				@return <c>true</c> if the split was successful, or <c>false</c> if an error occurred.
				@note Command buffers ranges are always expected to be submitted together, therefore it is not safe to submit
				only a set of ranges in the command buffer after splitCommandBuffers() has been called.
			*/
			bool splitCommandBuffers(bool bAdvanceEndOfBuffer = true);

			typedef Gnmx::DispatchDrawTriangleCullV1Data DispatchDrawData; ///< Dispatch draw triangle culling data.
			typedef Gnmx::DispatchDrawTriangleCullV1SharedDataWithVrb DispatchDrawSharedData; ///< Dispatch draw triangle culling data.
			typedef Gnmx::DispatchDrawVrbTriangleCullData DispatchDrawV0Data; ///< Dispatch draw VRB triangle culling data.

			GnmxDrawCommandBuffer     m_dcb; ///< The draw command buffer. Access directly at your own risk!
			GnmxDispatchCommandBuffer m_acb; ///< The asynchronous compute dispatch command buffer. Access directly at your own risk!
			GnmxConstantCommandBuffer m_ccb; ///< The constant command buffer. Access directly at your own risk!
			Gnmx::ComputeQueue     *m_pQueue; ///< The compute queue for <c>m_acb</c>. Access directly at your own risk!

			/** @brief Defines a type of function that is called when one of the GfxContext command buffers is out of space. 

				@param[in,out] gfxc					A pointer to the GfxContext object whose command buffer is out of space.
				@param[in,out] cb					A pointer to the CommandBuffer object that is out of space. This will be one of gfxc's CommandBuffer members.
				@param[in] sizeInDwords				The size of the unfulfilled CommandBuffer request in <c>DWORD</c>s.
				@param[in] userData					The user data passed to the function.
				
				@return <c>true</c> if the requested space is available in <c><i>cb</i></c> when the function returns; otherwise, returns <c>false</c>.
			 */
			typedef bool (*BufferFullCallbackFunc)(BaseGfxContext *gfxc, Gnm::CommandBuffer *cb, uint32_t sizeInDwords, void *userData);

			/** @brief Bundles a callback function pointer and the data that will be passed to it. */
			class BufferFullCallback
			{
			public:
				BufferFullCallbackFunc m_func; ///< Function to call when the command buffer is out of space.
				void *m_userData; ///< User data passed to <c><i>m_func</i></c>.
			};

			BufferFullCallback m_cbFullCallback; ///< Invoked when m_dcb or m_ccb actually runs out of space (as opposed to crossing a 4 MB boundary).

			DispatchDrawSharedData* m_pDispatchDrawSharedData;			///< Pointer to last copy of <c>m_dispatchDrawSharedData</c> in command buffer if valid.
			DispatchDrawSharedData m_dispatchDrawSharedData;			///< The current state of dispatch draw data.
			
			uint32_t m_dispatchDrawIndexDeallocMask;					///< The last dispatch draw compute shader index deallocation mask value.
			uint16_t m_dispatchDrawNumInstancesMinus1;					///< The setDispatchDrawNumIndices() value.
			uint16_t m_dispatchDrawInstanceStepRate0Minus1;				///< The setDispatchDrawInstanceStepRate() values.
			uint16_t m_dispatchDrawInstanceStepRate1Minus1;				///< The setDispatchDrawInstanceStepRate() values.
			uint16_t m_dispatchDrawFlags;								///< A union of <c>kDispatchDrawFlag*</c>.

			static const uint16_t kDispatchDrawFlagInDispatchDraw	= 0x0001;	///< Between beginDispatchDraw() and endDispatchDraw().
			static const uint16_t kDispatchDrawFlagIrbValid			= 0x0002;	///< A flag indicating that setupDispatchDrawRingBuffers() was called with valid inputs.
			static const uint16_t kDispatchDrawFlagVrbValid			= 0x0004;	///< A flag indicating that setupDispatchDrawRingBuffers() was called with valid inputs to enable the vertex ring buffer.
			static const uint16_t kDispatchDrawFlagsMaskPrimGroupSizeMinus129 =	0x07F0;	///< Stores value of (primGroupSizeMinus1-128) passed to beginDispatchDraw().
			static const int kDispatchDrawFlagsShiftPrimGroupSizeMinus129 =		4; ///< Shift value applied to <c>kDispatchDrawFlagsMaskPrimGroupSizeMinus129</c>.

			static const uint32_t kIndexSizeFlagDispatchDraw				= 0x00000010;	///< Bit 4 of Gnm::IndexSize indicates dispatch draw is enabled.
#if !defined(DOXYGEN_IGNORE)
			// The following code/data is used to work around the hardware's 4 MB limit on individual command buffers. We use the m_callback
			// fields of m_dcb and m_ccb to detect when either buffer crosses a 4 MB boundary, and save off the start/size of both buffers
			// into the m_submissionRanges array. When submit() is called, the m_submissionRanges array is used to submit each <4MB chunk individually.
			//
			// In order for this code to function properly, users of this class must not modify m_dcb.m_callback or m_ccb.m_callback!
			// To register a callback that triggers when m_dcb/m_ccb run out of space, use m_bufferFullCallback.
			static const uint32_t kMaxNumStoredSubmissions = 16; // Maximum number of <4MB submissions that can be recorded. Make this larger if you want more; it just means GfxContext objects get larger.
			const uint32_t *m_currentDcbSubmissionStart; // Beginning of the submit currently being constructed in the DCB
			const uint32_t *m_currentAcbSubmissionStart; // Beginning of the submit currently being constructed in the ACB
			const uint32_t *m_currentCcbSubmissionStart; // Beginning of the submit currently being constructed in the CCB
			const uint32_t *m_actualDcbEnd; // Actual end of the DCB's data buffer (i.e. dcbBuffer+dcbSizeInBytes/4)
			const uint32_t *m_actualAcbEnd; // Actual end of the ACB's data buffer (i.e. acbBuffer+acbSizeInBytes/4)
			const uint32_t *m_actualCcbEnd; // Actual end of the CCB's data buffer (i.e. ccbBuffer+ccbSizeInBytes/4)

			uint32_t *m_predicationRegionStart; // Beginning of the current predication region. Only valid between calls to setPredication() and endPredication().
			void *m_predicationConditionAddr; // Address of the predication condition that controls whether the current predication region will be skipped.

			uint8_t m_pad[4];

			class SubmissionRange
			{
			public:
				uint32_t m_dcbStartDwordOffset, m_dcbSizeInDwords;
				uint32_t m_acbStartDwordOffset, m_acbSizeInDwords;
				uint32_t m_ccbStartDwordOffset, m_ccbSizeInDwords;
			};
			SubmissionRange m_submissionRanges[kMaxNumStoredSubmissions]; // Stores the range of each previously-constructed submission (not including the one currently under construction).
			uint32_t m_submissionCount; // The current number of stored submissions in m_submissionRanges (again, not including the one currently under construction).

			SCE_GNM_FORCE_INLINE static void advanceCmdPtrPastEndBufferAllocation(Gnm::CommandBuffer * buffer)
			{
				if( buffer->sizeOfEndBufferAllocationInDword())
				{
					buffer->m_cmdptr = buffer->m_beginptr + buffer->m_bufferSizeInDwords;
				}
			}

#endif // !defined(DOXYGEN_IGNORE)

		};
	}
}
#endif	// !_SCE_GNMX_BASEGFXCONTEXT_H
