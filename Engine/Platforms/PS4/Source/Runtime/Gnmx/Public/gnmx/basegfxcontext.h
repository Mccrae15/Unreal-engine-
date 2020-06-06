/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 07.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
*/
#ifndef _SCE_GNMX_BASEGFXCONTEXT_H
#define _SCE_GNMX_BASEGFXCONTEXT_H

#include <gnm/constantcommandbuffer.h>
#include <gnm/drawcommandbuffer.h>
#include <gnmx/config.h>
#include <gnmx/helpers.h>
#include <gnmx/computequeue.h>
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

			/** @brief Sets up a default hardware state for the graphics ring.
			 *
			 *	This function performs the following tasks:
			 *	<ul> 
			 *		<li>Causes the GPU to wait until all pending PS outputs are written.</li>
			 *		<li>Invalidates all GPU caches.</li>
			 *		<li>Resets context registers to their default values.</li>
			 *		<li>Rolls the hardware context.</li>
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
			 *		<li><c>setGraphicsShaderControl( Gnm::GraphicsShaderControl.init() );</c></li>
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
			 *  @see Gnm::DrawCommandBuffer::initializeToDefaultContextState()
			 *
 			 *	@cmdsize 256
			 */
			void initializeDefaultHardwareState()
			{
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

				@note  Changes to the shader control will remain until the next call to this function.
				@cmdsize (((uint32_t)broadcast < (uint32_t)kBroadcastAll) ? 6 : 3) + 21
				@see Gnm::GraphicsShaderControl, setComputeShaderControl
				*/			
			void setGraphicsShaderControl(Gnm::GraphicsShaderControl control)
			{
				return m_dcb.setGraphicsShaderControl(control);
			}

			/** @brief Resets the ConstantUpdateEngine, Gnm::DrawCommandBuffer, and Gnm::ConstantCommandBuffer for a new frame.
			
				This function should be called at the beginning of every frame.

				The Gnm::DrawCommandBuffer and Gnm::ConstantCommandBuffer will be reset to empty (<c>m_cmdptr = m_beginptr</c>)
				All shader pointers currently cached in the Constant Update Engine will be set to <c>NULL</c>.
			*/
			void reset(void);

			/** @brief Sets the index size (16 or 32 bits).

				All subsequent draw calls that reference index buffers will use this index size to interpret their contents.
				This function will roll the hardware context.
				@param[in] indexSize		The index size to set.
				
				@cmdsize 2
			*/
			void setIndexSize(Gnm::IndexSize indexSize)
			{
				return m_dcb.setIndexSize(indexSize, Gnm::kCachePolicyBypass);
			}

			/** @brief Sets the base address where the indices are located for functions that do not specify their own indices.
				This function never rolls the hardware context.
				@param[in] indexAddr The address of the index buffer. This must be 2-byte aligned.

				@see drawIndexIndirect(), drawIndexOffset()
				@cmdsize 3
			*/
			void setIndexBuffer(const void * indexAddr)
			{
				return m_dcb.setIndexBuffer(indexAddr);
			}

			/** @brief Sets the VGT (vertex geometry tessellator) primitive type.
							All subsequent draw calls will use this primitive type.
							This function will roll the hardware context.
							@param[in] primType    The primitive type to set.
							@cmdsize 3
			*/
			void setPrimitiveType(Gnm::PrimitiveType primType)
			{
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
				return m_dcb.setNumInstances(numInstances);
			}

			/** @brief Resets multiple vertex geometry tessellator (VGT) configurations to default values.

			This function will roll the hardware context.
			@cmdsize 3
			*/
			void resetVgtControl()
			{
				return m_dcb.resetVgtControl();
			}

			/** @brief Specifies information for multiple vertex geometry tessellator (VGT) configurations.

				This function will roll the hardware context.

				@param[in] primGroupSizeMinus1		The number of primitives sent to one VGT block before switching to the next block. This argument has an implied +1 value. That is, a value of <c>0</c> means 1 primitive per group, and a value of <c>255</c> means 256 primitives per group.
													For tessellation, set <c><i>primGroupSizeMinusOne</i></c> to (number of patches per thread group) - <c>1</c>. In NEO mode, the recommended value of <c>primGroupSizeMinus1</c> is 127.
				@param[in] partialVsWaveMode		Specifies whether to enable partial VS waves. If enabled, then the VGT will issue a VS-stage wavefront as soon as a primitive group is finished; otherwise, the VGT will continue a VS-stage wavefront from one primitive group to the next
			                         				primitive group within a draw call. Partial VS waves must be enabled for streamout.
				@cmdsize 3
			*/
			SCE_GNM_API_CHANGED_NOINLINE
			void setVgtControl(uint8_t primGroupSizeMinus1, Gnm::VgtPartialVsWaveMode partialVsWaveMode)
			{
				SCE_GNM_CALL_DEPRECATED(return m_dcb.setVgtControl(primGroupSizeMinus1, partialVsWaveMode));
			}

			/** @brief Specifies information for multiple vertex geometry tessellator (VGT) configurations.
				This function will roll the hardware context.
				@param[in] primGroupSizeMinusOne	Number of primitives sent to one VGT block before switching to the next block. This argument has an implied +1 value. That is, a value of 0 means 1 primitive per group, and a value of 255 means 256 primitives per group.
													For tessellation, set <c><i>primGroupSizeMinusOne</i></c> to (number of patches per thread group) - <c>1</c>.
				@cmdsize 3
			*/
			void setVgtControl(uint8_t primGroupSizeMinusOne)
			{
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
				return m_dcb.setVgtControlForBase(primGroupSizeMinusOne, partialVsWaveMode);
			}

			/** @brief Specifies information for multiple vertex geometry tessellator (VGT) configurations.

			This function will roll the hardware context.

			@param[in] primGroupSizeMinusOne	Number of primitives sent to one VGT block before switching to the next block. This argument has an implied +1 value. That is, a value of 0 means 1 primitive per group, and a value of 255 means 256 primitives per group.
												For tessellation, set <c><i>primGroupSizeMinusOne</i></c> to (number of patches per thread group) - <c>1</c>.
			@param[in] partialVsWaveMode		Specifies whether to enable partial VS waves. If enabled, then the VGT will issue a VS-stage wavefront as soon as a primitive group is finished; otherwise, the VGT will continue a VS-stage wavefront from one primitive group to the next
												primitive group within a draw call. Partial VS waves must be enabled for streamout.
			@param[in] wdSwitchOnlyOnEopMode	If enabled (<c>kVgtSwitchOnEopEnable</c>), then the WD will send an entire draw call to one IA-plus-VGT pair, which will then behave as in base mode. If disabled, then the WD will split the draw call on instance boundaries and then send <c><c><i>primGroupSize</i></c>*2</c> (or the remainder) primitives to each IA.

			@note When using tessellation and the geometry shader at the same time in an application running in NEO mode, call setVgtControlForNeo() with <c>kVgtPartialVsWaveEnable</c>
			as the value of the <c>partialVsWaveMode</c> parameter; otherwise, the GPU hangs upon rendering.

			@cmdsize 3
			*/
			void setVgtControlForNeo(uint8_t primGroupSizeMinusOne, Gnm::WdSwitchOnlyOnEopMode wdSwitchOnlyOnEopMode, Gnm::VgtPartialVsWaveMode partialVsWaveMode)
			{
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
						<c>kRecordLastCompletionSynchronous</c><br>Write command buffer offset of draw or dispatch at EOP to known offset in the <c>DrawCommandBuffer</c>, and wait for this value to be written before proceeding to the next draw or dispatch. In this mode, only one draw or dispatch will be issued at a time; this feature makes it easier to isolate problematic commands, but it may change the applicationâ€™s behavior and timing.
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
				@param[in] numBytes				The number of bytes to transfer from <c><i>srcGpuAddr</i></c> to <c><i>dstGpuAddr</i></c>.
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
				@see DrawCommandBuffer::setAaSampleCount()
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
#ifdef SCE_GNMX_ENABLE_GFXCONTEXT_CALLCOMMANDBUFFERS // Disabled by default
			/** @brief Calls another command buffer pair. When the called command buffers end, execution will continue
			           in the current command buffer.
					   
				@param dcbBaseAddr			The address of the destination dcb. This pointer must not be <c>NULL</c> if <c><i>dcbSizeInDwords</i></c> is non-zero.
				@param dcbSizeInDwords		The size of the destination dcb in <c>DWORD</c>s. This may be set to 0 if no dcb is required.
				@param ccbBaseAddr			The address of the destination ccb. This pointer must not be <c>NULL</c> if <c><i>ccbSizeInDwords</i></c> is non-zero.
				@param ccbSizeInDwords		The size of the destination ccb in <c>DWORD</c>s. This may be 0 if no ccb is required.

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
			#include "gnmx/gfxcontext_methods.h"



			/** @brief Splits the command buffer into multiple submission ranges.
					   
				This function will cause the GfxContext to stop adding commands to the active submission range and create a new one.
				This is necessary for when chainCommandBuffer() is called, which must be the last packet in a DCB range,
				or if the DCB has reached its 4MB limit (Gnm::kIndirectBufferMaximumSizeInBytes).

				@return <c>true</c> if the split was successful, or <c>false</c> if an error occurred.
				@note Command buffers ranges are always expected to be submitted together, therefore it is not safe to submit
				only a set of ranges in the command buffer after splitCommandBuffers() has been called.
			*/
			bool splitCommandBuffers(bool bAdvanceEndOfBuffer = true);

			GnmxDrawCommandBuffer     m_dcb; ///< The draw command buffer. Access directly at your own risk!
			GnmxConstantCommandBuffer m_ccb; ///< The constant command buffer. Access directly at your own risk!

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

#if !defined(DOXYGEN_IGNORE)
			// The following code/data is used to work around the hardware's 4 MB limit on individual command buffers. We use the m_callback
			// fields of m_dcb and m_ccb to detect when either buffer crosses a 4 MB boundary, and save off the start/size of both buffers
			// into the m_submissionRanges array. When submit() is called, the m_submissionRanges array is used to submit each <4MB chunk individually.
			//
			// In order for this code to function properly, users of this class must not modify m_dcb.m_callback or m_ccb.m_callback!
			// To register a callback that triggers when m_dcb/m_ccb run out of space, use m_bufferFullCallback.
			static const uint32_t kMaxNumStoredSubmissions = 16; // Maximum number of <4MB submissions that can be recorded. Make this larger if you want more; it just means GfxContext objects get larger.
			const uint32_t *m_currentDcbSubmissionStart; // Beginning of the submit currently being constructed in the DCB
			const uint32_t *m_currentCcbSubmissionStart; // Beginning of the submit currently being constructed in the CCB
			const uint32_t *m_actualDcbEnd; // Actual end of the DCB's data buffer (that is, dcbBuffer+dcbSizeInBytes/4)
			const uint32_t *m_actualCcbEnd; // Actual end of the CCB's data buffer (that is, ccbBuffer+ccbSizeInBytes/4)

			uint32_t *m_predicationRegionStart; // Beginning of the current predication region. Only valid between calls to setPredication() and endPredication().
			void *m_predicationConditionAddr; // Address of the predication condition that controls whether the current predication region will be skipped.

			uint8_t m_pad[4];

			class SubmissionRange
			{
			public:
				uint32_t m_dcbStartDwordOffset, m_dcbSizeInDwords;
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
