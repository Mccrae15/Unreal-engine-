/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if !defined(_SCE_GNMX_GFXCONTEXT_H)
#define _SCE_GNMX_GFXCONTEXT_H

#include <gnmx/basegfxcontext.h>
#include <gnmx/constantupdateengine.h>

#include "shaderbinary.h"

// TEMPORARY FOR DEPRECATE REASONS:
#include <gnm/platform.h>

//#define SCE_GNMX_ENABLE_GFXCONTEXT_CALLCOMMANDBUFFERS

namespace sce
{
	namespace Gnmx
	{

		/** @brief Encapsulates a Gnm::DrawCommandBuffer and Gnm::ConstantCommandBuffer pair and a Constant Update Engine and wraps them in a single higher-level interface.
		 *
		 * This should be a new developer's main entry point into the PlayStation®4 rendering API.
		 * The GfxContext object submits to the graphics ring, and supports both graphics and compute operations; for compute-only tasks, use the ComputeContext class.
		 * @see ComputeContext
		 */
		class SCE_GNMX_EXPORT GfxContext : public BaseGfxContext
		{
		public:
			/** @brief Default constructor. */
			GfxContext(void);
			/** @brief Default destructor. */
			~GfxContext(void);

			/** @brief Initializes a GfxContext with application-provided memory buffers.
				@param[in] cueHeapAddr A buffer in VRAM for use by the Constant Update Engine. Use ConstantUpdateEngine::computeHeapSize()
								   to determine the correct buffer size.
				@param[in] numRingEntries The number of entries in each internal Constant Update Engine ring buffer. At least 64 is recommended.
				@param[in] dcbBuffer A buffer for use by <c>m_dcb</c>.
				@param[in] dcbSizeInBytes The size of <c><i>dcbBuffer</i></c> in bytes.
				@param[in] ccbBuffer A buffer for use by <c>m_ccb</c>.
				@param[in] ccbSizeInBytes The size of <c><i>ccbBuffer</i></c> in bytes.
				*/
			void init(void *cueHeapAddr, uint32_t numRingEntries,
					  void *dcbBuffer, uint32_t dcbSizeInBytes, void *ccbBuffer, uint32_t ccbSizeInBytes);

			/** @brief Initializes a GfxContext with application-provided memory buffers.

				@param[in] cueHeapAddr		A buffer in VRAM for use by the Constant Update Engine. Use ConstantUpdateEngine::computeHeapSize()
											to determine the correct buffer size.
				@param[in] numRingEntries	The number of entries in each internal Constant Update Engine ring buffer. At least 64 is recommended.
				@param[in] ringSetup		The ring configuration for the constant update engine.
				@param[in] dcbBuffer		A buffer for use by <c>m_dcb</c>.
				@param[in] dcbSizeInBytes	The size of <c><i>dcbBuffer</i></c> in bytes.
				@param[in] ccbBuffer		A buffer for use by <c>m_ccb</c>.
				@param[in] ccbSizeInBytes	The size of <c><i>ccbBuffer</i></c> in bytes.
				*/
			void init(void *cueHeapAddr, uint32_t numRingEntries, ConstantUpdateEngine::RingSetup ringSetup,
					  void *dcbBuffer, uint32_t dcbSizeInBytes, void *ccbBuffer, uint32_t ccbSizeInBytes);
			

			/** @brief Resets the ConstantUpdateEngine, Gnm::DrawCommandBuffer, and Gnm::ConstantCommandBuffer for a new frame.
			
				Call this at the beginning of every frame.

				The Gnm::DrawCommandBuffer and Gnm::ConstantCommandBuffer will be reset to empty (<c>m_cmdptr = m_beginptr</c>)
				All shader pointers currently cached in the Constant Update Engine will be set to <c>NULL</c>.
			*/
			void reset(void);

			/** @brief Sets up a default hardware state for the graphics ring.
			 *
			 *	This function performs the following tasks:
			 *	<ul> 
			 *		<li>Causes the GPU to wait until all pending PS outputs are written</li>
			 *		<li>Invalidates all GPU caches</li>
			 *		<li>Resets context registers to their default values</li>
			 *		<li>Rolls the hardware context</li>
			 *      <li>Invalidates the CUE's shader contexts</li>
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
			 *		<li><c>setGsMode(kGsModeDisable, kGsMaxOutputPrimitiveDwordSize1024);</c></li>
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
				BaseGfxContext::initializeDefaultHardwareState();
				m_cue.invalidateAllShaderContexts();
			}
			/** @brief  Sets up a default context state for the graphics ring.
			 *
			 *	This function resets context registers to default values, rolls the hardware context, and invalidates the CUE's shader contexts. It does not include a forced GPU stall,
			 *  or invalidate caches, and may therefore be a more efficient alternative method for resetting GPU state to safe default values than calling the
			 *  initializeDefaultHardwareState() function.
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
				BaseGfxContext::initializeToDefaultContextState();
				m_cue.invalidateAllShaderContexts();
			}

#if defined(__ORBIS__)
			/** @brief Submits the DrawCommandBuffer and ConstantCommandBuffer.
				@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
				@note This function will submit the ConstantCommandBuffer only if <c>m_submitWithCcb</c> had been set in the CUE. If the CUE
					  determines that it has not submitted any meaningful CCB packets, it will not set this variable. If you still want the CCB to be submitted
					  (e.g. because you added your own packets to the <c><i>m_ccb</i></c> directly), you will need to set <c>m_cue.m_submitWithCcb</c> to <c>true</c> before calling submit().
				@return A code indicating the submission status.
				@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
				@see Gnm::areSubmitsAllowed()
				*/
			int32_t submit(void);

			/** @brief Submits the DrawCommandBuffer and ConstantCommandBuffer.
				@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
				@note This function will submit the ConstantCommandBuffer only if <c>m_submitWithCcb</c> had been set in the CUE. If the CUE
					  determines that it has not submitted any meaningful CCB packets, it will not set this variable. If you still want the CCB to be submitted
					  (for example, because you added your own packets to the <c><i>m_ccb</i></c> directly), you will need to set <c>m_cue.m_submitWithCcb</c> to <c>true</c> before calling submit().
				@param[in] workloadId The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
				@return A code indicating the submission status.
				*/
			int32_t submit(uint64_t workloadId);


			/** @brief Runs validation on the DrawCommandBuffer and ConstantCommandBuffer without submitting them.
				@return A code indicating the validation status.
				*/
			int32_t validate(void);

			/** @brief Submits the DrawCommandBuffer and ConstantCommandBuffer. A flip command is appended and will be executed after command buffer execution is complete.
				
				@param[in] videoOutHandle			The video out handle.
				@param[in] displayBufferIndex		The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
				@param[in] flipMode					The flip mode. For valid values, see #SceVideoOutFlipMode.
				@param[in] flipArg					User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely. 
													The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> 
													associated with the most recently completed flip.
				@return A code indicating the error status.
				
				@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
				@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
				@see Gnm::areSubmitsAllowed()
				*/
			int32_t submitAndFlip(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg);

			/** @brief Submits the DrawCommandBuffer and ConstantCommandBuffer, appending a flip command that executes after command buffer execution completes.
				
				@param[in] workloadId				The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
				@param[in] videoOutHandle			The video out handle.
				@param[in] displayBufferIndex		The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
				@param[in] flipMode					The flip mode. For valid values, see #SceVideoOutFlipMode.
				@param[in] flipArg					User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely. 
													The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> 
													associated with the most recently completed flip.
				@return A code indicating the error status.
				
				@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
				*/
			int32_t submitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg);

			/** @brief Submits the DrawCommandBuffer and ConstantCommandBuffer, appending a flip command that executes after command buffer execution completes.
				
				@note This function submits the ConstantCommandBuffer only if <c>m_submitWithCcb</c> was set in the CUE. If the CUE
					  determines that it has not submitted any meaningful CCB packets, it does not set this variable. If you want the CCB to be submitted anyway,
					  (for example, because you added your own packets to the <c><i>m_ccb</i></c> directly), set <c>m_cue.m_submitWithCcb</c> to <c>true</c> before calling submit().
				
				@param[in] videoOutHandle		Video out handle.
				@param[in] displayBufferIndex	The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
				@param[in] flipMode				The flip mode. For valid values, see #SceVideoOutFlipMode.
				@param[in] flipArg				User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely. 
												The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> associated with the most recently completed flip.
				@param[out] labelAddr			The GPU address to be updated when the command buffer has been processed.
				@param[in] value				The value to write to <c><i>labelAddr</i></c>.
				
				@return A code indicating the error status.
				@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
				@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
				@see Gnm::areSubmitsAllowed()
				*/
			int32_t submitAndFlip(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
								  void *labelAddr, uint32_t value);

			/** @brief Submits the DrawCommandBuffer and ConstantCommandBuffer, appending a flip command that executes after command buffer execution completes.
				
				@note This function submits the ConstantCommandBuffer only if <c>m_submitWithCcb</c> was set in the CUE. If the CUE
					  determines that it has not submitted any meaningful CCB packets, it will not set this variable. If you want the CCB to be submitted anyway,
					  (for example, because you added your own packets to the <c><i>m_ccb</i></c> directly), you need to set <c>m_cue.m_submitWithCcb</c> to <c>true</c> before calling submit().
				
				@param[in] workloadId			The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
				@param[in] videoOutHandle		Video out handle.
				@param[in] displayBufferIndex	The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
				@param[in] flipMode				The flip mode. For valid values, see #SceVideoOutFlipMode.
				@param[in] flipArg				User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely. 
												The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> associated with the most recently completed flip.
				@param[out] labelAddr			The GPU address to be updated when the command buffer has been processed.
				@param[in] value				The value to write to <c><i>labelAddr</i></c>.
								
				@return A code indicating the error status.
				@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
				*/
			int32_t submitAndFlip(uint64_t workloadId, uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
								  void *labelAddr, uint32_t value);


			/** @brief Submits the DrawCommandBuffer and ConstantCommandBuffer, appending a flip command that executes after command buffer execution completes.

				@note This function submits the ConstantCommandBuffer only if <c>m_submitWithCcb</c> was set in the CUE. If the CUE
					  determines that it has not submitted any meaningful CCB packets, it will not set this variable. If you want the CCB to be submitted anyway,
					  (for example, because you added your own packets to the <c><i>m_ccb</i></c> directly), you need to set <c>m_cue.m_submitWithCcb</c> to <c>true</c> before calling submit().

				@param[in] videoOutHandle		Video out handle.
				@param[in] displayBufferIndex	The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
				@param[in] flipMode				The flip mode. For valid values, see #SceVideoOutFlipMode.
				@param[in] flipArg				User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely. 
												The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> 
												associated with the most recently completed flip.
				@param[in] eventType            Determines when interrupt will be triggered.
				@param[out] labelAddr			The GPU address to be updated when the command buffer has been processed.
				@param[in] value				The value to write to <c><i>labelAddr</i></c>.
			    @param[in] cacheAction          Cache action to perform.

				@return A code indicating the error status.
				@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
				@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
				@see Gnm::areSubmitsAllowed()
				*/
			int32_t submitAndFlipWithEopInterrupt(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  Gnm::EndOfPipeEventType eventType, void *labelAddr, uint32_t value, Gnm::CacheAction cacheAction);

			/** @brief Submits the DrawCommandBuffer and ConstantCommandBuffer. A flip command is appended and will be executed after command buffer execution is complete.

				@note This function will submit the ConstantCommandBuffer only if <c>m_submitWithCcb</c> had been set in the CUE. If the CUE
					  determines that it has not submitted any meaningful CCB packets, it will not set this variable. If you still want the CCB to be submitted
					  (e.g. because you added your own packets to the <c><i>m_ccb</i></c> directly), you will need to set <c>m_cue.m_submitWithCcb</c> to true before calling submit().

				@param[in] workloadId				The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
				@param[in] videoOutHandle		Video out handle.
				@param[in] displayBufferIndex	The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
				@param[in] flipMode				The flip mode. For valid values, see #SceVideoOutFlipMode.
				@param[in] flipArg				User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely. 
												The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> 
												associated with the most recently completed flip.
				@param[in] eventType            Determines when interrupt will be triggered.
				@param[out] labelAddr			The GPU address to be updated when the command buffer has been processed.
				@param[in] value				The value to write to <c><i>labelAddr</i></c>.
			    @param[in] cacheAction          Cache action to perform.

				@return A code indicating the error status.
				@note To enable auto-validation on submit(), please enable the "GPU Validation" option in the Target Manager Settings.
				*/
			int32_t submitAndFlipWithEopInterrupt(uint64_t workloadId,
												  uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  Gnm::EndOfPipeEventType eventType, void *labelAddr, uint32_t value, Gnm::CacheAction cacheAction);


			/** @brief Submits the DrawCommandBuffer and ConstantCommandBuffer. A flip command is appended and will be executed after command buffer execution is complete.

				@note This function will submit the ConstantCommandBuffer only if <c>m_submitWithCcb</c> had been set in the CUE. If the CUE
					  determines that it has not submitted any meaningful CCB packets, it will not set this variable. If you still want the CCB to be submitted
					  (e.g. because you added your own packets to the <c><i>m_ccb</i></c> directly), you will need to set <c>m_cue.m_submitWithCcb</c> to <c>true</c> before calling submit().

				@param[in] videoOutHandle		Video out handle.
				@param[in] displayBufferIndex	The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
				@param[in] flipMode				The flip mode. For valid values, see #SceVideoOutFlipMode.
				@param[in] flipArg				User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely. 
												The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> 
												associated with the most recently completed flip.
				@param[in] eventType            Determines when interrupt will be triggered.
			    @param[in] cacheAction          Cache action to perform.

				@return A code indicating the error status.
				@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
				@note When called after Gnm::submitDone(), this call may block the CPU until the GPU is idle. You can use Gnm::areSubmitsAllowed() to determine whether a call to submitDone() will block the CPU.
				@see Gnm::areSubmitsAllowed()
				*/
			int32_t submitAndFlipWithEopInterrupt(uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  Gnm::EndOfPipeEventType eventType, Gnm::CacheAction cacheAction);

			/** @brief Submits the DrawCommandBuffer and ConstantCommandBuffer, appending a flip command that executes after command buffer execution completes.

			@note This function submits the ConstantCommandBuffer only if <c>m_submitWithCcb</c> was set in the CUE. If the CUE
			determines that it has not submitted any meaningful CCB packets, it will not set this variable. If you want the CCB to be submitted anyway,
			(for example, because you added your own packets to the <c><i>m_ccb</i></c> directly), you need to set <c>m_cue.m_submitWithCcb</c> to <c>true</c> before calling submit().

				@param[in] workloadId				The id of the workload corresponding to this context. @sa Gnm::beginWorkload()
				@param[in] videoOutHandle		Video out handle.
				@param[in] displayBufferIndex	The index of the display buffer for which this function waits. Use sceVideoOutRegisterBuffers() to register this index.
				@param[in] flipMode				The flip mode. For valid values, see #SceVideoOutFlipMode.
				@param[in] flipArg				User-supplied argument that has no meaning to the SDK. You can use <c>flipArg</c> to identify each flip uniquely. 
												The sceVideoOutGetFlipStatus() function returns an SceVideoOutFlipStatus object that includes the <c><i>flipArg</i></c> 
												associated with the most recently completed flip.
				@param[in] eventType            Determines when interrupt will be triggered.
			    @param[in] cacheAction          Cache action to perform.

				@return A code indicating the error status.
				@note To enable auto-validation on submit(), enable the "GPU Validation" option in the Target Manager Settings.
				*/
			int32_t submitAndFlipWithEopInterrupt(uint64_t workloadId,
												  uint32_t videoOutHandle, uint32_t displayBufferIndex, uint32_t flipMode, int64_t flipArg,
												  Gnm::EndOfPipeEventType eventType, Gnm::CacheAction cacheAction);


			/** @brief Indicates that the context has active commands in the CCB so the CCB has to be submitted along with the DCB.

				@note This is useful if you are not using the context's submit() functions and are submitting its buffers yourself.

				@return <c>true</c> if the CCB needs to be submitted, <c>false</c> if submitting only the DCB is sufficient.
				*/
			bool ccbIsActive() const { return m_cue.m_submitWithCcb || (0 != (m_dispatchDrawFlags & kDispatchDrawFlagIrbValid));}
#endif

			//////////// Constant Update Engine commands

			/**
			 * @brief Binds one or more read-only texture objects to the specified shader stage.
			 * This function never rolls the hardware context.
			 * @param[in] stage The resource(s) will be bound to this shader stage.
			 * @param[in] startSlot The first slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountResource -1]</c>.
			 * @param[in] numSlots The number of consecutive slots to bind.
			 * @param[in] textures The Gnm::Texture objects to bind to the specified slots. <c><i>textures</i>[0]</c> will be bound to startSlot, <c><i>textures</i>[1]</c> to <c><i>startSlot</i> + 1</c>, and so on.
			 *                 The contents of these texture objects are cached locally inside the ConstantUpdateEngine. If this parameter is <c>NULL</c>, the specified slots will
			 *                 be unbound; this is not necessary for regular operation, but may be useful for debugging purposes.
			 * @note Buffers and Textures share the same pool of API slots.
			 */
			void setTextures(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Texture *textures)
			{
				if( numSlots == 1 )
					m_cue.setTexture(stage, startSlot, textures);
				else
					return m_cue.setTextures(stage, startSlot, numSlots, textures);
			}

			/**
			 * @brief Binds one or more read-only buffer objects to the specified shader stage.
			 * This function never rolls the hardware context.
			 * @param[in] stage The resource(s) will be bound to this shader stage.
			 * @param[in] startSlot The first slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountResource -1]</c>.
			 * @param[in] numSlots The number of consecutive slots to bind.
			 * @param[in] buffers The Gnm::Buffer objects to bind to the specified slots. <c>buffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>buffers[1]</c> to <c><i>startSlot</i> +1</c>, and so on.
			 *                The contents of these Gnm::Buffer objects are cached locally inside the ConstantUpdateEngine. If this parameter is <c>NULL</c>, the specified slots will
			 *                be unbound; this is not necessary for regular operation, but may be useful for debugging purposes.
			 * @note Gnm::Buffer and Gnm::Texture objects share the same pool of API slots.
			 */
			void setBuffers(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Buffer *buffers)
			{
				if( numSlots == 1 )
					m_cue.setBuffer(stage, startSlot, buffers);
				else
					return m_cue.setBuffers(stage, startSlot, numSlots, buffers);
			}

			/**
			 * @brief Binds one or more read/write texture objects to the specified shader stage.
			 * This function never rolls the hardware context.
			 * @param[in] stage The resource(s) will be bound to this shader stage.
			 * @param[in] startSlot The first slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountRwResource -1]</c>.
			 * @param[in] numSlots The number of consecutive slots to bind.
			 * @param[in] rwTextures The Gnm::Texture objects to bind to the specified slots. <c>rwTextures[0]</c> will be bound to <c><i>startSlot</i></c>, <c>rwTextures[1]</c> to <c><i>startSlot</i> +1</c>, and so on.
			 *                   The contents of these texture objects are cached locally inside the ConstantUpdateEngine. If this parameter is <c>NULL</c>, the specified slots will
			 *                   be unbound; this is not necessary for regular operation, but may be useful for debugging purposes.
			 * @note <c><i>rwBuffers</i></c> and <c><i>rwTextures</i></c> objects share the same pool of API slots.
			 * @see setRwBuffers()
			 */
			void setRwTextures(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Texture *rwTextures)
			{
                if( numSlots == 1 )
					m_cue.setRwTexture(stage, startSlot, rwTextures);
				else
					return m_cue.setRwTextures(stage, startSlot, numSlots, rwTextures);
			}

			/**
			 * @brief Binds one or more read/write buffer objects to the specified shader stage.
			 * This function never rolls the hardware context.
			 * @param[in] stage The resource(s) will be bound to this shader stage.
			 * @param[in] startSlot The first slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountRwResource -1]</c>.
			 * @param[in] numSlots The number of consecutive slots to bind.
			 * @param[in] rwBuffers The Gnm::Buffer objects to bind to the specified slots. <c>rwBuffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>rwBuffers[1]</c> to <c><i>startSlot</i> +1</c>, and so on.
			 *                  The contents of these buffer objects are cached locally inside the ConstantUpdateEngine. If this parameter is <c>NULL</c>, the specified slots will
			 *                  be unbound; this is not necessary for regular operation, but may be useful for debugging purposes.
			 * @note <c><i>rwBuffers</i></c> and <c><i>rwTextures</i></c> objects share the same pool of API slots.
			 * @see setRwTextures()
			 */
			void setRwBuffers(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Buffer *rwBuffers)
			{
				if( numSlots == 1 )
					m_cue.setRwBuffer(stage, startSlot, rwBuffers);
				else
					return m_cue.setRwBuffers(stage, startSlot, numSlots, rwBuffers);
			}

			/**
			 * @brief Binds one or more sampler objects to the specified shader stage.
			 * This function never rolls the hardware context.
			 * @param[in] stage The resource(s) will be bound to this shader stage.
			 * @param[in] startSlot The first slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountSampler -1]</c>.
			 * @param[in] numSlots The number of consecutive slots to bind.
			 * @param[in] samplers The Gnm::Sampler objects to bind to the specified slots. <c>samplers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>samplers[1]</c> to <c><i>startSlot</i> +1</c>, and so on.
			 *                 The contents of these Sampler objects are cached locally inside the ConstantUpdateEngine. If this parameter is <c>NULL</c>, the specified slots will
			 *                 be unbound; this is not necessary for regular operation, but may be useful for debugging purposes.
			 */
			void setSamplers(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Sampler *samplers)
			{
				if( numSlots == 1 )
					m_cue.setSampler(stage, startSlot, samplers);
				else
					return m_cue.setSamplers(stage, startSlot, numSlots, samplers);
			}

			/** @brief Binds a user SRT buffer.
			*
			*  This function never rolls the hardware context.
			*
			*  @param[in] shaderStage The shader stage to which this function binds the SRT buffer.
			*  @param[in] buffer A pointer to the buffer. If this argument is set to <c>NULL</c>, then <c><i>sizeInDwords</i></c> must be set to <c>0</c>.
			*  @param[in] sizeInDwords The size of the <c><i>buffer</i></c> data, expressed as a number of <c>DWORD</c>s. The valid range is <c>[1..kMaxSrtUserDataCount]</c> if <c><i>buffer</i></c> is non-<c>NULL</c>.
			*/
			void setUserSrtBuffer(sce::Gnm::ShaderStage shaderStage, const void* buffer, uint32_t sizeInDwords)
			{
				m_cue.setUserSrtBuffer(shaderStage, buffer, sizeInDwords);
			}

			/**
			 * @brief Binds one or more constant buffer objects to the specified shader stage.
			 * This function never rolls the hardware context.
			 * @param[in] stage The resource(s) will be bound to this shader stage.
			 * @param[in] startSlot The first slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountConstantBuffer -1]</c>.
			 * @param[in] numSlots The number of consecutive slots to bind.
			 * @param[in] buffers The constant buffer objects to bind to the specified slots. <c>buffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>buffers[1]</c> to <c><i>startSlot</i> +1</c>, and so on.
			 *                The contents of these Gnm::Buffer objects are cached locally inside the ConstantUpdateEngine. If this parameter is <c>NULL</c>, the specified slots will
			 *                be unbound; this is not necessary for regular operation, but may be useful for debugging purposes.
			 */
			void setConstantBuffers(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Buffer *buffers)
			{
				if( numSlots == 1 )
					m_cue.setConstantBuffer(stage, startSlot, buffers);
				else
					return m_cue.setConstantBuffers(stage, startSlot, numSlots, buffers);
			}

			/**
			 * @brief Binds one or more vertex buffer objects to the specified shader stage.
			 * This function never rolls the hardware context.
			 * @param[in] stage The resource(s) will be bound to this shader stage.
			 * @param[in] startSlot The first slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountVertexBuffer -1]</c>.
			 * @param[in] numSlots The number of consecutive slots to bind.
			 * @param[in] buffers The vertex buffer objects to bind to the specified slots. <c>buffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>buffers[1]</c> to <c><i>startSlot</i> +1</c>, and so on.
			 *                The contents of these Gnm::Buffer objects are cached locally inside the ConstantUpdateEngine. If this parameter is <c>NULL</c>, the specified slots will
			 *                be unbound; this is not necessary for regular operation, but may be useful for debugging purposes.
			 */
			void setVertexBuffers(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const Gnm::Buffer *buffers)
			{
				return m_cue.setVertexBuffers(stage, startSlot, numSlots, buffers);
			}

            /**
			 * @brief Binds one or more streamout buffer objects to the specified shader stage.
			 * This function never rolls the hardware context.
			 * @param[in] startSlot The first slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountStreamoutBuffer -1]</c>.
			 * @param[in] numSlots The number of consecutive slots to bind.
			 * @param[in] buffers The streamout buffer objects to bind to the specified slots. <c>buffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>buffers[1]</c> to <c><i>startSlot</i> +1</c>, and so on.
			 *                The contents of these Gnm::Buffer objects are cached locally inside the ConstantUpdateEngine. If this parameter is <c>NULL</c>, the specified slots will
			 *                be unbound; this is not necessary for regular operation, but may be useful for debugging purposes.
			 */
			void setStreamoutBuffers(uint32_t startSlot, uint32_t numSlots, const Gnm::Buffer *buffers)
			{
				return m_cue.setStreamoutBuffers(startSlot, numSlots, buffers);
			}
			/** @brief Sets the mapping from GS streams to the VS streams in streamout.

			This function will roll the hardware context.
			@param[in] mapping A four-element bitfield, in which each four-bit element controls four VS streams for a particular GS stream. This pointer must not be <c>NULL</c>.
			@see setStreamoutBufferDimensions(), writeStreamoutBufferUpdate()
			
			*/
			void setStreamoutMapping(const Gnm::StreamoutBufferMapping* mapping)
			{
				m_cue.m_shaderContextIsValid &= ~(1 << Gnm::kShaderStageGs);
				m_dcb.setStreamoutMapping(mapping);
			}

			/**
			 * @brief Binds one or more Boolean constants to the specified shader stage.
			 * This function never rolls the hardware context.
			 * @param[in] stage The resource(s) will be bound to this shader stage.
			 * @param[in] startSlot The first slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountBoolConstant -1]</c>.
			 * @param[in] numSlots The number of consecutive slots to bind.
			 * @param[in] bits The Boolean constants to bind to the specified slots. Each slot will contain 32 1-bit bools packed together in a single <c>DWORD</c>.
			 *             <c>bits[0]</c> will be bound to <c><i>startSlot</i></c>, <c>bits[1]</c> to <c><i>startSlot</i> +1</c>, and so on.
			 *             The contents of the bits array are cached locally inside the ConstantUpdateEngine. If this parameter is <c>NULL</c>, the specified slots will
			 *             be unbound; this is not necessary for regular operation, but may be useful for debugging purposes.
			 */
			void setBoolConstants(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const uint32_t *bits)
			{
				return m_cue.setBoolConstants(stage, startSlot, numSlots, bits);
			}

			/**
			 * @brief Binds one or more floating-point constants to the specified shader stage.
			 * This function never rolls the hardware context.
			 * @param[in] stage The resource(s) will be bound to this shader stage.
			 * @param[in] startSlot The first slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountResource -1]</c>.
			 * @param[in] numSlots The number of consecutive slots to bind.
			 * @param[in] floats The constants to bind to the specified slots. <c>floats[0]</c> will be bound to <c><i>startSlot</i></c>, <c>floats[1]</c> to <c><i>startSlot</i> +1</c>, and so on.
			 *               The contents of these Gnm::Texture objects are cached locally inside the ConstantUpdateEngine. If this parameter is <c>NULL</c>, the specified slots will
			 *               be unbound; this is not necessary for regular operation, but may be useful for debugging purposes.
			 */
			void setFloatConstants(Gnm::ShaderStage stage, uint32_t startSlot, uint32_t numSlots, const float *floats)
			{
				return m_cue.setFloatConstants(stage, startSlot, numSlots, floats);
			}

#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			// User table management API, placed here to allow inlining
			/**
			 * @brief Sets the user table for textures and buffers.
			 *
			 * When a user table is set for a particular resource type, the CUE uses that table instead of its internal resource tracking.
			 * This allows for faster draw setup and a simpler interface for applications that already manage their resources themselves.
			 *
			 * @param stage		The shader stage to bind the table to.
			 * @param table		The table (array) of resources. Each resource entry takes 16 bytes (the size of Gnm::Texture). Specify a value of <c>NULL</c> to revert to CUE resource tracking.
			 */
			void setUserResourceTable(Gnm::ShaderStage stage, const Gnm::Texture *table)
			{
				m_cue.setUserResourceTable(stage,table);
			}
			/**
			 * @brief Sets the user table for read/write textures and buffers.
			 *
			 * When a user table is set for a particular resource type, the CUE uses that table instead of its internal resource tracking.
			 * This allows for faster draw setup and a simpler interface for applications that already manage their resources themselves.
			 *
			 * @param stage		The shader stage to bind the table to.
			 * @param table		The table (array) of resources. Each resource entry takes 16 bytes (size of Gnm::Texture). Specify a value of <c>NULL</c> to revert to CUE resource tracking.
			 */
			void setUserRwResourceTable(Gnm::ShaderStage stage, const Gnm::Texture *table)
			{
				m_cue.setUserRwResourceTable(stage,table);
			}
			/**
			 * @brief Sets the user table for samplers.
			 *
			 * When a user table is set for a particular resource type, the CUE uses that table instead of its internal resource tracking.
			 * This allows for faster draw setup and a simpler interface for applications that already manage their resources themselves.
			 *
			 * @param stage		The shader stage to bind the table to.
			 * @param table		The table (array) of samplers. Specify a value of <c>NULL</c> to revert to CUE resource tracking.
			 */
			void setUserSamplerTable(Gnm::ShaderStage stage, const Gnm::Sampler *table)
			{
				m_cue.setUserSamplerTable(stage,table);
			}
			/**
			 * @brief Sets the user table for constant buffers.
			 *
			 * When a user table is set for a particular resource type, the CUE uses that table instead of its internal resource tracking.
			 * This allows for faster draw setup and a simpler interface for applications that already manage their resources themselves.
			 *
			 * @param stage		The shader stage to bind the table to.
			 * @param table The table (array) of constant buffers. Specify a value of <c>NULL</c> to revert to CUE resource tracking.
			 */
			void setUserConstantBufferTable(Gnm::ShaderStage stage, const Gnm::Buffer *table)
			{
				m_cue.setUserConstantBufferTable(stage,table);
			}
			
			/**
			 * @brief Sets the user table for vertex buffers.
			 *
			 * When a user table is set for a particular resource type, the CUE uses that table instead of its internal resource tracking.
			 * This allows for faster draw setup and a simpler interface for applications that already manage their resources themselves.
			 *
			 * @param stage		The shader stage to bind the table to.
			 * @param table		The table (array) of vertex buffers. Specify a value of <c>NULL</c> to revert to CUE resource tracking.
			 */
			void setUserVertexTable(Gnm::ShaderStage stage, const Gnm::Buffer *table)
			{
				m_cue.setUserVertexTable(stage,table);
			}
			
			/**
			 * @brief Clears all user tables on all stages.
			 */
			void clearUserTables()
			{
				m_cue.clearUserTables();
			}
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES

		/**
			 * @brief Specifies a range of the Global Data Store to be used by shaders for atomic global counters such as those
			 *        used to implement PSSL <c>AppendRegularBuffer</c> and <c>ConsumeRegularBuffer</c> objects.
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
			 * @param[in] stage The shader stage to bind this counter range to.
			 * @param[in] baseOffsetInBytes The byte offset to the start of the counters in GDS. Must be a multiple of 4. 
			 * @param[in] rangeSizeInBytes The size of the counter range, in bytes. Must be a multiple of 4.
			 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
			 */
			void setAppendConsumeCounterRange(Gnm::ShaderStage stage, uint32_t baseOffsetInBytes, uint32_t rangeSizeInBytes)
			{
				return m_cue.setAppendConsumeCounterRange(stage, baseOffsetInBytes, rangeSizeInBytes);
			}

			/**
			 * @brief Specifies a range of the Global Data Store to be used by shaders.
			 *
			 *  This function never rolls the hardware context.
			 *
			 * @param[in] stage The shader stage to bind this range to.
			 * @param[in] rangeGdsOffsetInBytes The byte offset to the start of the range in the GDS. This must be a multiple of 4. 
			 * @param[in] rangeSizeInBytes The size of the counter range in bytes. This must be a multiple of 4.
			 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes. It is an error to specify a range outside of these bounds.
			 */
			void setGdsMemoryRange(Gnm::ShaderStage stage, uint32_t rangeGdsOffsetInBytes, uint32_t rangeSizeInBytes)
			{
				return m_cue.setGdsMemoryRange(stage, rangeGdsOffsetInBytes, rangeSizeInBytes);
			}

			/**
			 * @brief Specifies a buffer to hold tessellation constants.
			 * This function never rolls the hardware context.
			 * @param[in] tcbAddr		The address of the buffer.
			 * @param[in] domainStage	The stage in which the domain shader will execute.
			 */
			void setTessellationDataConstantBuffer(void *tcbAddr, Gnm::ShaderStage domainStage);

			/**
			 * @brief Specifies a buffer to receive off-chip tessellation factors.
			 * This function never rolls the hardware context.
			 * @note This function modifies the global resource table. It is not safe to modify the global resource table unless the GPU graphics pipe is idle.
			 * @param[out] tessFactorMemoryBaseAddr	The address of the buffer. This address must already have been mapped correctly. Please refer to <c>Gnmx::Toolkit::allocateTessellationFactorRingBuffer()</c>.
			 */
			void setTessellationFactorBuffer(void *tessFactorMemoryBaseAddr);
			
			/**	@brief Sets the active shader stages in the graphics pipeline. 
				Note that the compute-only CS stage is always active for dispatch commands.
				This function will roll the hardware context.
				@param[in] activeStages Indicates which shader stages should be activated.
				@see Gnm::DrawCommandBuffer::setGsMode()
				@cmdsize 3
			 */
			void setActiveShaderStages(Gnm::ActiveShaderStages activeStages)
			{
				m_cue.setActiveShaderStages(activeStages);
				return m_dcb.setActiveShaderStages(activeStages);
			}

			/**
			 * @brief Binds a shader to the VS stage.
			 * This function will roll hardware context if any of the following Gnm::VsStageRegisters set for <c><i>vsb</i></c> are different from current state:
			 - <c>m_spiVsOutConfig</c>
			 - <c>m_spiShaderPosFormat</c>
			 - <c>m_paClVsOutCntl</c>

			 The check is deferred until next draw call.
			 * @param[in] vsb Pointer to the shader to bind to the VS stage.
			 * @param[in] shaderModifier If the shader requires a fetch shader, pass the associated shader modifier value here. Otherwise, pass 0.
			 * @param[in] fetchShaderAddr If the shader requires a fetch shader, pass its GPU address here. Otherwise, pass 0.
			 * @note  Even though this function might not roll hardware context by itself, the subsequent draw*() function will roll context if the CUE is left managing the PS shader inputs table.
			 *		  To prevent this roll, you must manage PS inputs and disable the CUE's management by means of <c>SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE</c>.
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*<i>vsb</i></c> must not change until
			 *        a different shader has been bound to this stage!
			 */
			void setVsShader(const Gnmx::VsShader *vsb, uint32_t shaderModifier, void *fetchShaderAddr)
			{
				return m_cue.setVsShader(vsb, shaderModifier, fetchShaderAddr);
			}

			/** @brief Uses a shader embedded in the Gnm driver to set the shader code that the VS shader stage is to use.

				This function will roll the hardware context.
				@param[in] shaderId         The ID of the embedded shader to set.
				@param[in] shaderModifier	The shader modifier value that generateVsFetchShaderBuildState() returns; if no fetch shader, use <c>0</c>.
				@cmdsize 29
			*/
			void setEmbeddedVsShader(Gnm::EmbeddedVsShader shaderId, uint32_t shaderModifier)
			{
				m_dcb.setEmbeddedVsShader(shaderId, shaderModifier);
				return m_cue.invalidateAllShaderContexts();
			}


			/**
			 * @brief Binds a shader pair to the asynchronous compute and VS stages for use in a dispatchDraw() call.
			 * This function will roll hardware context if any of the following Gnm::VsStageRegisters set for <c><i>csvsb</i></c> are different from current state:
			 *
			 * - <c>m_spiVsOutConfig</c>
			 * - <c>m_spiShaderPosFormat</c>
			 * - <c>m_paClVsOutCntl</c>
			 *
			 * The check is deferred until next draw call.
			 * @param[in] csvsb Pointer to the shaders to bind to the asynchronous compute and VS stages.
			 * @param[in] shaderModifierVs If the VS shader requires a fetch shader, pass the associated shader modifier value here; otherwise pass 0.
			 * @param[in] fetchShaderAddrVs If the VS shader requires a fetch shader, pass its GPU address here; otherwise pass 0.
			 * @param[in] shaderModifierCs If the compute shader requires a fetch shader, pass the associated shader modifier value here; otherwise pass 0.
			 * @param[in] fetchShaderAddrCs If the compute shader requires a fetch shader, pass its GPU address here; otherwise pass 0.
			 * @note  Setting the asynchronous compute shader does not affect the currently set graphics CS stage shader, which is set by setCsShader().
			 *		  Asynchronous compute shaders share the user data constants and CUE ring elements set up for Gnm::kShaderStageVs,
			 *		  while graphics CS stage shaders use the user data constants and CUE ring elements set up for Gnm::kShaderStageCs.
			 * @note  Only the pointers <c>csvsb->getVertexShader()</c> and <c>csvsb->getComputeShader()</c> are cached inside the Gnm::ConstantUpdateEngine; the location 
			 *		  and contents of these pointers should not be changed before all calls to dispatchDraw() which use this shader have been made!
			 */
			void setCsVsShaders(const Gnmx::CsVsShader *csvsb, uint32_t shaderModifierVs, void *fetchShaderAddrVs, uint32_t shaderModifierCs, void *fetchShaderAddrCs)
			{
				return m_cue.setCsVsShaders(csvsb, shaderModifierVs, fetchShaderAddrVs, shaderModifierCs, fetchShaderAddrCs);
			}

			/**
			 * @brief Binds a shader to the asynchronous compute stage, for use in a dispatchDraw call.
			 * Generally, compute shaders for dispatch draw are packed into a CsVsShader, and so will be set by calling setCsVsShaders(), instead.
			 * This function never rolls the hardware context.
			 * @param[in] csb Pointer to the shader to bind to the asynchronous compute stage.
			 * @param[in] shaderModifierCs If the compute shader requires a fetch shader, pass the associated shader modifier value here; otherwise pass 0.
			 * @param[in] fetchShaderAddrCs If the compute shader requires a fetch shader, pass its GPU address here; otherwise pass 0.
			 * @note  Setting the asynchronous compute shader does not affect the currently set graphics CS stage shader, which is set by setCsShader().
			 *		  Asynchronous compute shaders share the user data constants and CUE ring elements set up for Gnm::kShaderStageVs,
			 *		  while graphics CS stage shaders use the user data constants and CUE ring elements set up for Gnm::kShaderStageCs.
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c><i>*csb</i></c> must not change until
			 *        a different shader has been bound to this stage!
			 */
			void setAsynchronousComputeShader(const Gnmx::CsShader *csb, uint32_t shaderModifierCs, void *fetchShaderAddrCs)
			{
				return m_cue.setAsynchronousComputeShader(csb, shaderModifierCs, fetchShaderAddrCs);
			}

			/**
			 * @brief Binds a shader to the PS stage.
			 * This function will roll hardware context if any of the following Gnm::PsStageRegisters set for the shader specified by <c><i>psb</i></c> are different from current state:
			 - <c>m_spiShaderZFormat</c>
			 - <c>m_spiShaderColFormat</c>
			 - <c>m_spiPsInputEna</c>
			 - <c>m_spiPsInputAddr</c>
			 - <c>m_spiPsInControl</c>
			 - <c>m_spiBarycCntl</c>
			 - <c>m_dbShaderControl</c>
			 - <c>m_cbShaderMask</c>

			 The check is deferred until next draw call.
			 * @param[in] psb Pointer to the shader to bind to the PS stage.
			 * @note  Even though this function might not roll hardware context by itself, the subsequent draw*() function will roll context if the CUE is left managing the PS shader inputs table.
			 *		  To prevent this roll, you must manage PS inputs and disable the CUE's management by means of <c>SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE</c>.
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*psb</c> must not change until
			 *        a different shader has been bound to this stage!
			 * @note Passing <c>NULL</c> to this function does not disable pixel shading by itself. The psb pointer may only safely be <c>NULL</c> if pixel shader invocation is
			 *       impossible as is the case when:
			 *  	  - color buffer writes have been disabled with <c>setRenderTargetMask(0)</c>.
			 *  	  - a CbMode other than <c>kCbModeNormal</c> is passed to <c>setCbControl()</c>, such as during a hardware MSAA resolve or fast-clear elimination pass.
			 *  	  - the depth test is enabled and <c>kCompareFuncNever</c> is passed to <c>DepthStencilControl::setDepthControl()</c>.
			 *  	  - the stencil test is enabled and <c>kCompareFuncNever</c> is passed to <c>DepthStencilControl::setStencilFunction()</c> and/or
			 *  	  	<c>DepthStencilControl::setStencilFunctionBack()</c>.
			 *  	  - both front and back face culling are enabled with <c>PrimitiveSetup::setCullFace(kPrimitiveSetupCullFaceFrontAndBack)</c>.
			 *  	  - etc.
			 */
			void setPsShader(const Gnmx::PsShader *psb)
			{
				return m_cue.setPsShader(psb);
			}

			/** @brief Sets the shader code to be used for the PS shader stage using a shader embedded in the Gnm driver.

				This function will roll the hardware context.

				@param[in] shaderId		The ID of the embedded shader to set.
				@cmdsize 40
			*/
			void setEmbeddedPsShader(Gnm::EmbeddedPsShader shaderId)
			{
				m_dcb.setEmbeddedPsShader(shaderId);
				return m_cue.invalidateAllShaderContexts();
			}


			/**
			 * @brief Binds a shader to the CS stage.
			 * This function never rolls the hardware context.
			 * @param[in] csb Pointer to the shader to bind to the CS stage.
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*<i>csb</i></c> must not change until
			 *        a different shader has been bound to this stage!
			 */
			void setCsShader(const Gnmx::CsShader *csb)
			{
				return m_cue.setCsShader(csb);
			}

			/**
			 * @brief Binds a shader to the ES stage.
			 * This function never rolls the hardware context.
			 * @param[in] esb Pointer to the shader to bind to the ES stage.
			 * @param[in] shaderModifier If the shader requires a fetch shader, pass the associated shader modifier value here. Otherwise, pass 0.
			 * @param[in] fetchShaderAddr If the shader requires a fetch shader, pass its GPU address here. Otherwise, pass 0.
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*<i>esb</i></c> must not change until
			 *        a different shader has been bound to this stage!
			 */
			void setEsShader(const Gnmx::EsShader *esb, uint32_t shaderModifier, void *fetchShaderAddr)
			{
				return m_cue.setEsShader(esb, shaderModifier, fetchShaderAddr);
			}

			

			

			/**
			 * @brief Binds shaders to the LS and HS stages, and updates the LS shader's LDS size based on the HS shader's needs.
			 * This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants set in <c><i>hsb</i></c> or the value of <c><i>numPatches</i></c> are different from current state:

			 Gnm::HsStageRegisters
			 - <c>m_vgtTfParam</c>
			 - <c>m_vgtHosMaxTessLevel</c>
			 - <c>m_vgtHosMinTessLevel</c>

			 Gnm::HullStateConstants
			 - <c>m_numThreads</c>
			 - <c>m_numInputCP</c>

			 The check is deferred until next draw call.
			 * @param[in] lsb Pointer to the shader to bind to the LS stage. This must not be set to <c>NULL</c> if <c><i>hsb</i></c> is non-<c>NULL</c>.
			 * @param[in] shaderModifier If the shader requires a fetch shader, pass the associated shader modifier value here. Otherwise, pass 0.
			 * @param[in] fetchShaderAddr If the LS shader requires a fetch shader, pass its GPU address here. Otherwise, pass 0.
			 * @param[in] hsb Pointer to the shader to bind to the HS stage. This must not be set to <c>NULL</c> if <c><i>lsb</i></c> is non-<c>NULL</c>.
			 * @param[in] numPatches Number of patches in the HS shader.
			 * @note Only the shader pointers are cached inside the ConstantUpdateEngine; the location and contents of <c><i>lsb</i></c> and <c><i>hsb</i></c> must not change
			 *       until different shaders are bound to these stages!
			 */
			void setLsHsShaders(Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const Gnmx::HsShader *hsb, uint32_t numPatches);
			/**
			* @brief Binds shaders to the LS and HS stages, and updates the LDS size of the LS shader based on the needs of the HS shader.
			* This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants set in <c><i>hsb</i></c> or the value of <c><i>numPatches</i></c> differ from current state:

			Gnm::HsStageRegisters
			- <c>m_vgtTfParam</c>
			- <c>m_vgtHosMaxTessLevel</c>
			- <c>m_vgtHosMinTessLevel</c>

			Gnm::HullStateConstants
			- <c>m_numThreads</c>
			- <c>m_numInputCP</c>

			The check is deferred until next draw call.
			* @param[in] lsb Pointer to the shader to bind to the LS stage. This value must not be set to <c>NULL</c> if <c><i>hsb</i></c> is non-<c>NULL</c>.
			* @param[in] shaderModifier If the shader requires a fetch shader, pass the associated shader modifier value here. Otherwise, pass <c>0</c>.
			* @param[in] fetchShaderAddr If the LS shader requires a fetch shader, pass its GPU address here. Otherwise, pass <c>0</c>.
			* @param[in] hsb Pointer to the shader to bind to the HS stage. This value must not be set to <c>NULL</c> if <c><i>lsb</i></c> is non-<c>NULL</c>.
			* @param[in] numPatches Number of patches in the HS shader.
			* @param[in] distributionMode Tessellation distribution mode.
			* @note Only the shader pointers are cached inside the ConstantUpdateEngine; the location and contents of <c><i>lsb</i></c> and <c><i>hsb</i></c> must not change
			*       until different shaders are bound to these stages!
			* @note NEO mode only.
			*/
			void setLsHsShaders(Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const Gnmx::HsShader *hsb, uint32_t numPatches, Gnm::TessellationDistributionMode distributionMode);


			/**
			 * @brief Binds a shader to the GS and VS stages.
			 * This function will roll hardware context if any of Gnm::GsStageRegisters entries set for the GsShader specified in <c><i>gsb</i></c>
			 or the Gnm::VsStageRegisters entries set for the copy shader specified in by GsShader::getCopyShader() call to the shader specified in <c><i>gsb</i></c> are different from current state:

			 Gnm::GsStageRegisters
			 - <c>m_vgtStrmoutConfig</c>
			 - <c>m_vgtGsOutPrimType</c>
			 - <c>m_vgtGsInstanceCnt</c>

			 Gnm::VsStageRegisters
			 - <c>m_spiVsOutConfig</c>
			 - <c>m_spiShaderPosFormat</c>
			 - <c>m_paClVsOutCntl</c>

			 The check is deferred until next draw call.
			 * @param[in] gsb Pointer to the shader to bind to the GS/VS stages.
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*<i>gsb</i></c> must not change until
			 *        a different shader has been bound to this stage!
			 */
			void setGsVsShaders(const Gnmx::GsShader *gsb);

			/**
			 * @brief Binds an on-chip-GS vertex shader to the ES stage.
			 *
			 * This function never rolls the hardware context.
			 *
			 * @param[in] esb					A pointer to the shader to bind to the ES stage.
			 * @param[in] ldsSizeIn512Bytes		The size of LDS allocation required for on-chip GS. The size unit is 512 bytes and must be compatible with the GsShader and gsPrimsPerSubGroup value passed to setOnChipGsVsShaders.
			 * @param[in] shaderModifier		The shader modifier. If the shader requires a fetch shader, pass the associated shader modifier value here; otherwise pass a value of 0.
			 * @param[in] fetchShaderAddr		The fetch shader address. If the shader requires a fetch shader, pass its GPU address here; otherwise pass a value of 0.
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*esb</c> must not change until
			 *        a different shader has been bound to this stage.
			 *
			 * @see computeOnChipGsConfiguration()
			 */
			void setOnChipEsShader(const Gnmx::EsShader *esb, uint32_t ldsSizeIn512Bytes, uint32_t shaderModifier, void *fetchShaderAddr)
			{
				m_cue.setOnChipEsShader(esb, ldsSizeIn512Bytes, shaderModifier, fetchShaderAddr);
			}

			/**
			 * @brief Binds an on-chip GS shader to the GS and VS stages and sets up the on-chip GS sub-group size controls.
			 *
			 * This function may roll hardware context. This will occur if either the GS shader or the bundled VS copy shader are
			 * different from the currently-bound shaders in these stages:
			 * 
			 * Gnm::GsStageRegisters
			 * - <c>m_vgtStrmoutConfig</c>
			 * - <c>m_vgtGsOutPrimType</c>
			 * - <c>m_vgtGsInstanceCnt</c>
			 * 
			 * Gnm::VsStageRegisters
			 * - <c>m_spiVsOutConfig</c>
			 * - <c>m_spiShaderPosFormat</c>
			 * - <c>m_paClVsOutCntl</c>
			 * 
			 * This function will also roll the hardware context if the value of gsPrimsPerSubGroup or <c>gsb->m_inputVertexCount</c> changes.
			 * 
			 * The check is deferred until next draw call.
			 * 
			 * @param[in] gsb					A pointer to the shader to bind to the GS/VS stages.
			 * @param[in] gsPrimsPerSubGroup	The number of GS threads which will be launched per on-chip-GS LDS allocation. This must be compatible with the size of LDS allocation passed to setOnChipEsShaders().
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*gsb</c> must not change until
			 *        a different shader has been bound to this stage.
			 *
			 * @see computeOnChipGsConfiguration()
			 */
			void setOnChipGsVsShaders(const Gnmx::GsShader *gsb, uint32_t gsPrimsPerSubGroup);
#if SCE_GNM_CUE2_ENABLE_CACHE
			/**
			 * @brief Binds a shader to the VS stage.
			 * This function will roll the hardware context if any of the following Gnm::VsStageRegisters set for <c><i>vsb</i></c> are different from current state:
			 - <c>m_spiVsOutConfig</c>
			 - <c>m_spiShaderPosFormat</c>
			 - <c>m_paClVsOutCntl</c>

			 The check is deferred until next draw call.
			 * @param[in] vsb				A pointer to the shader to bind to the VS stage.
			 * @param[in] shaderModifier	The associated shader modifier value if the shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] fetchShaderAddr	The GPU address if the shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] cache				The cached shader inputs.
			 *
			 * @note  Even though this function might not roll hardware context by itself, the subsequent draw*() function will roll context if the CUE is left managing the PS shader inputs table.
			 *		  To prevent this roll, you must manage PS inputs and disable the CUE's management by means of <c>SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE</c>.
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*vsb</c> must not change until
			 *        a different shader has been bound to this stage!
			 */
			void setVsShader(const Gnmx::VsShader *vsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *cache)
			{
				return m_cue.setVsShader(vsb, shaderModifier, fetchShaderAddr, cache);
			}
			
			/**
			 * @brief Binds a shader to the PS stage.
			 * This function will roll hardware context if any of the following Gnm::PsStageRegisters set for the shader specified by <c><i>psb</i></c> are different from current state:
			 - <c>m_spiShaderZFormat</c>
			 - <c>m_spiShaderColFormat</c>
			 - <c>m_spiPsInputEna</c>
			 - <c>m_spiPsInputAddr</c>
			 - <c>m_spiPsInControl</c>
			 - <c>m_spiBarycCntl</c>
			 - <c>m_dbShaderControl</c>
			 - <c>m_cbShaderMask</c>

			 The check is deferred until next draw call.
			 * @param[in] psb		A pointer to the shader to bind to the PS stage. This pointer must not be <c>NULL</c> if color buffer writes have been enabled by passing a non-zero mask
							  to GfxContext::setRenderTargetMask().
			 * @param[in] cache		The cached shader inputs.
			 *
			 * @note  Even though this function might not roll hardware context by itself, the subsequent draw*() function will roll context if the CUE is left managing the PS shader inputs table.
			 *		  To prevent this roll, you must manage PS inputs and disable the CUE's management by means of <c>SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE</c>.
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*psb</c> must not change until
			 *        a different shader has been bound to this stage!
			 */
			void setPsShader(const Gnmx::PsShader *psb, const ConstantUpdateEngine::InputParameterCache *cache)
			{
				return m_cue.setPsShader(psb,cache);
			}


			/**
			 * @brief Binds a shader to the CS stage.
			 *
			 * This function never rolls the hardware context.
			 *
			 * @param[in] csb		A pointer to the shader to bind to the CS stage.
			 * @param[in] cache		The cached shader inputs.
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*csb</c> must not change until
			 *        a different shader has been bound to this stage!
			 */
			void setCsShader(const Gnmx::CsShader *csb, const ConstantUpdateEngine::InputParameterCache *cache)
			{
				return m_cue.setCsShader(csb,cache);
			}

			/**
			 * @brief Binds a shader to the ES stage.
			 *
			 * This function never rolls the hardware context.
			 *
			 * @param[in] esb				A pointer to the shader to bind to the ES stage.
			 * @param[in] shaderModifier	The associated shader modifier value if the shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] fetchShaderAddr	The GPU address	of the fetch shader if the shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] cache				The cached shader inputs.
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*esb</c> must not change until
			 *        a different shader has been bound to this stage!
			 */
			void setEsShader(const Gnmx::EsShader *esb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *cache)
			{
				return m_cue.setEsShader(esb, shaderModifier, fetchShaderAddr, cache);
			}

			

			

			/**
			 * @brief Binds shaders to the LS and HS stages and updates the LS shader's LDS size based on the HS shader's needs.
			 *
			 * This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants set in <c><i>hsb</i></c> or the value of <c><i>numPatches</i></c> are different from current state:

			 Gnm::HsStageRegisters
			 - <c>m_vgtTfParam</c>
			 - <c>m_vgtHosMaxTessLevel</c>
			 - <c>m_vgtHosMinTessLevel</c>

			 Gnm::HullStateConstants
			 - <c>m_numThreads</c>
			 - <c>m_numInputCP</c>

			 The check is deferred until next draw call.
			 * @param[in] lsb				A pointer to the shader to bind to the LS stage. This must not be set to <c>NULL</c> if <c><i>hsb</i></c> is non-<c>NULL</c>.
			 * @param[in] shaderModifier	The associated shader modifier value if the shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] fetchShaderAddr	The GPU address of the fetch shader if the LS shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] lsCache			The cached shader inputs for the LS shader.
			 * @param[in] hsb				A pointer to the shader to bind to the HS stage. This must not be set to <c>NULL</c> if <c><i>lsb</i></c> is non-<c>NULL</c>.
			 * @param[in] numPatches		The number of patches in the HS shader.
			 * @param[in] hsCache			The cached shader inputs for the HS shader.
			 *
			 * @note Only the shader pointers are cached inside the ConstantUpdateEngine; the location and contents of <c><i>lsb</i></c> and <c><i>hsb</i></c> must not change
			 *       until different shaders are bound to these stages!
			 */
			void setLsHsShaders(Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache, const Gnmx::HsShader *hsb, uint32_t numPatches, const ConstantUpdateEngine::InputParameterCache *hsCache);
			/**
			* @brief Binds shaders to the LS and HS stages, and updates the LDS size of the LS shader based on the needs of the HS shader.
			*
			* This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants set in <c><i>hsb</i></c> or the value of <c><i>numPatches</i></c> differ from current state:

			Gnm::HsStageRegisters
			- <c>m_vgtTfParam</c>
			- <c>m_vgtHosMaxTessLevel</c>
			- <c>m_vgtHosMinTessLevel</c>

			Gnm::HullStateConstants
			- <c>m_numThreads</c>
			- <c>m_numInputCP</c>

			The check is deferred until next draw call.
			* @param[in] lsb				A pointer to the shader to bind to the LS stage. This value must not be set to <c>NULL</c> if <c><i>hsb</i></c> is non-<c>NULL</c>.
			* @param[in] shaderModifier	The associated shader modifier value if the shader requires a fetch shader. Otherwise pass <c>0</c>.
			* @param[in] fetchShaderAddr	The GPU address of the fetch shader if the LS shader requires a fetch shader. Otherwise pass <c>0</c>.
			* @param[in] lsCache			The cached shader inputs for the LS shader.
			* @param[in] hsb				A pointer to the shader to bind to the HS stage. This vallue must not be set to <c>NULL</c> if <c><i>lsb</i></c> is non-<c>NULL</c>.
			* @param[in] numPatches		The number of patches in the HS shader.
			* @param[in] distributionMode	Tessellation distribution mode.
			* @param[in] hsCache			The cached shader inputs for the HS shader.
			*
			* @note Only the shader pointers are cached inside the ConstantUpdateEngine; the location and contents of <c><i>lsb</i></c> and <c><i>hsb</i></c> must not change
			*       until different shaders are bound to these stages!
			* @note NEO mode only.
			*/
			void setLsHsShaders(Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache, const Gnmx::HsShader *hsb, uint32_t numPatches, Gnm::TessellationDistributionMode distributionMode,const ConstantUpdateEngine::InputParameterCache *hsCache);


			/**
			 * @brief Binds a shader to the GS and VS stages.
			 *
			 * This function will roll the hardware context if any of Gnm::GsStageRegisters entries set for the GsShader specified in <c><i>gsb</i></c>
			 or the Gnm::VsStageRegisters entries set for the copy shader specified in by GsShader::getCopyShader() call to the shader specified in <c><i>gsb</i></c> are different from current state:

			 Gnm::GsStageRegisters
			 - <c>m_vgtStrmoutConfig</c>
			 - <c>m_vgtGsOutPrimType</c>
			 - <c>m_vgtGsInstanceCnt</c>

			 Gnm::VsStageRegisters
			 - <c>m_spiVsOutConfig</c>
			 - <c>m_spiShaderPosFormat</c>
			 - <c>m_paClVsOutCntl</c>

			 The check is deferred until next draw call.
			 * @param[in] gsb		A pointer to the shader to bind to the GS/VS stages.
			 * @param[in] cache		The cached shader inputs.
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*gsb</c> must not change until
			 *        a different shader has been bound to this stage!
			 */
			void setGsVsShaders(const Gnmx::GsShader *gsb, const ConstantUpdateEngine::InputParameterCache *cache);

			/**
			 * @brief Binds an on-chip-GS vertex shader to the ES stage.
			 *
			 * This function never rolls the hardware context.
			 * @param[in] esb					A pointer to the shader to bind to the ES stage.
			 * @param[in] ldsSizeIn512Bytes		The size of LDS allocation required for on-chip GS. The size unit is 512 bytes and must be compatible with the GsShader and gsPrimsPerSubGroup value passed to setOnChipGsVsShaders().
			 * @param[in] shaderModifier		The associated shader modifier value if the shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] fetchShaderAddr		The GPU address of the shader if the shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] cache					The cached shader inputs.
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*esb</c> must not change until
			 *        a different shader has been bound to this stage!
			 * @see computeOnChipGsConfiguration()
			 */
			void setOnChipEsShader(const Gnmx::EsShader *esb, uint32_t ldsSizeIn512Bytes, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *cache)
			{
				m_cue.setOnChipEsShader(esb, ldsSizeIn512Bytes, shaderModifier, fetchShaderAddr, cache);
			}

			/**
			 * @brief Binds an on-chip GS shader to the GS and VS stages and sets up the on-chip GS sub-group size controls.
			 * This function may roll hardware context. This will occur if either the GS shader or the bundled VS copy shader are
			 * different from the currently-bound shaders in these stages:
			 * 
			 * Gnm::GsStageRegisters
			 * - <c>m_vgtStrmoutConfig</c>
			 * - <c>m_vgtGsOutPrimType</c>
			 * - <c>m_vgtGsInstanceCnt</c>
			 * 
			 * Gnm::VsStageRegisters
			 * - <c>m_spiVsOutConfig</c>
			 * - <c>m_spiShaderPosFormat</c>
			 * - <c>m_paClVsOutCntl</c>
			 * 
			 * This function will also roll context if the value of gsPrimsPerSubGroup or <c>gsb->m_inputVertexCount</c> changes.
			 * 
			 * The check is deferred until next draw call.
			 * @param[in] gsb					A pointer to the shader to bind to the GS/VS stages.
			 * @param[in] gsPrimsPerSubGroup	The number of GS threads which will be launched per on-chip-GS LDS allocation, which must be compatible with the size of LDS allocation passed to setOnChipEsShaders.
			 * @param[in] cache					The cached shader inputs.
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*gsb</c> must not change until
			 *        a different shader has been bound to this stage!
			 * @see computeOnChipGsConfiguration
			 */
			void setOnChipGsVsShaders(const Gnmx::GsShader *gsb, uint32_t gsPrimsPerSubGroup, const ConstantUpdateEngine::InputParameterCache *cache);

#endif //SCE_GNM_CUE2_ENABLE_CACHE

			/**
			 * @brief Sets the ring buffer where data will flow from the ES to the GS stages when geometry shading is enabled.
			 * This function will roll hardware context.
			 * @note This function modifies the global resource table. It is not safe to modify the global resource table unless the GPU graphics pipe is idle.
			 * @note This function overwrites the register state set by setOnChipEsGsLayout() and vice versa.
			 *
			 * @param[out] baseAddr						The address of the buffer.
			 * @param[in] ringSize						The size of the buffer.
			 * @param[in] maxExportVertexSizeInDword	The maximum size of a vertex export in <c>DWORD</c>s.
			 */
			void setEsGsRingBuffer(void *baseAddr, uint32_t ringSize, uint32_t maxExportVertexSizeInDword);

			/**
			 * @brief Sets the layout of LDS area where data will flow from the ES to the GS stages when on-chip geometry shading is enabled.
			 * This sets the same context register state as <c>setEsGsRingBuffer(NULL, 0, maxExportVertexSizeInDword)</c> but does not modify the global resource table.
			 *
			 * This function will roll hardware context.
			 * @param[in] maxExportVertexSizeInDword	The stride of an ES-GS vertex in <c>DWORD</c>s, which must match EsShader::m_maxExportVertexSizeInDword.
			 */
			void setOnChipEsGsLdsLayout(uint32_t maxExportVertexSizeInDword);

			/**
			 * @brief Sets the ring buffers where data will flow from the GS to the VS stages when geometry shading is enabled.
			 * This function will roll hardware context.
			 * @note This function modifies the global resource table. It is not safe to modify the global resource table unless the GPU graphics pipe is idle.
			 * @param[out] baseAddr						The address of the buffer. This address must not be set to <c>NULL</c> if <c><i>ringSize</i></c> is non-zero.
			 * @param[in] ringSize						The size of the buffer.
			 * @param[in] vtxSizePerStreamInDword		The vertex size for each of four streams in <c>DWORD</c>s. This pointer must not be set to <c>NULL</c>.
			 * @param[in] maxOutputVtxCount				The maximum number of vertices output from the GS stage.
			 */
			void setGsVsRingBuffers(void *baseAddr, uint32_t ringSize,
									const uint32_t vtxSizePerStreamInDword[4], uint32_t maxOutputVtxCount);

			/** @brief Turns off the Gs Mode.

			The function will roll the hardware context if it is different from current state.

			@note  This function has to be called prior moving back to a non-GS pipeline in addition to <c>setShaderStages()</c>.

			@see Gnm::DrawCommandBuffer::setGsMode()
			*/
			void setGsModeOff()
			{
				m_cue.setGsModeOff();
			}
			/** @brief Enables streamout from a VS shader.

			This allows streamout without a GS shader. After enabling, the streamout has to be set up the same way as the streamout from a GS shader.
			The VS shader has to be compiled with the streamout for this to have any effect.
			The enabled state might be disabled by setting a GS shader and will be disabled with <c>disableGsMode()</c>.
			This function will roll the hardware context.

			@param[in] enable		A flag that specifies whether to enable streamout from VS shaders.

			@see setGsShader(), disableGsMode(), setGsModeOff()

			*/
			void setVsShaderStreamoutEnable(bool enable)
			{
				m_cue.m_shaderContextIsValid &= ~(1 << Gnm::kShaderStageGs); // purge cached GS in the CUE
				m_dcb.setVsShaderStreamoutEnable(enable);
			}

			/**
			 * @brief Sets the address of the system's global resource table: a collection of <c>V#</c>s which point to global buffers for various shader tasks.
			 * This function never rolls the hardware context.
			 * @param[out] addr The GPU-visible address of the global resource table. The minimum size of this buffer is given by
			 *             Gnm::SCE_GNM_SHADER_GLOBAL_TABLE_SIZE, and its minimum alignment is Gnm::kMinimumBufferAlignmentInBytes.
			 */
			void setGlobalResourceTableAddr(void *addr)
			{
				return m_cue.setGlobalResourceTableAddr(addr);
			}

			/**
			 * @brief Sets an entry in the global resource table.
			 * This function never rolls the hardware context.
			 * @note This function modifies the global resource table. It is not safe to modify the global resource table unless the GPU is idle.
			 * @param[in] resType The global resource type to bind a buffer for. Each global resource type has its own entry in the global resource table.
			 * @param[in] res The buffer to bind to the specified entry in the global resource table. The size of the buffer is global-resource-type-specific.
			 */
			void setGlobalDescriptor(Gnm::ShaderGlobalResourceType resType, const Gnm::Buffer *res)
			{
				return m_cue.setGlobalDescriptor(resType, res);
			}
#if SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR
			/**
			 * @brief Sets a user provided function to allocate EUD data.
			 * The memory for EUD has to be allocated in the GPU-visible memory (preferably Garlic) and be aligned to 16 bytes. 
			 * There is no corresponding free() callback, and it is up to the user to re-cycle/free this memory after the GPU has finished processing the context
			 * where the memory had been allocated.
			 * 
			 * @param allocator	A pointer to the function that allocates memory for the EUD. Its first argument is the size of the allocation in <c>DWORD</c>s and the second is arbitrary user data.
			 * @param userData	The data to be passed as the second parameter to the function specified for <c><i>allocator</i></c>.
			 */
			void setUserEudAllocator(void* (*allocator)(uint32_t sizeInDword, void* userData), void* userData)
			{
				m_cue.setUserEudAllocator(allocator,userData);
			}
#endif //SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR


			
			//////////// Draw commands


			/** @brief Inserts a draw call using auto-generated indices.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.

				@param[in] indexCount  The index up to which this function generates indices automatically.
				
				@see Gnm::DrawCommandBuffer::setPrimitiveType()
			  */
			void drawIndexAuto(uint32_t indexCount)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexAuto can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_cue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawIndexAuto(indexCount, Gnm::kDrawModifierDefault);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}
			
			/** @brief Inserts a draw call using auto-generated indices, while adding an offset to the vertex and instance indices.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				@note For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader must be compiled with the <c>-indirect-draw</c> flag.

				@param[in] indexCount		The index up to which to auto-generate the indices.
				@param[in] vertexOffset		The offset to add to each vertex index.
				@param[in] instanceOffset	The offset to add to each instance index.
				@param[in] modifier A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				
				@see Gnm::DrawCommandBuffer::setPrimitiveType()
			  */
			void drawIndexAuto(uint32_t indexCount, uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexAuto can't be called between beginDispatchDraw and endDispatchDraw");
				m_cue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawIndexAuto(indexCount, modifier);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @deprecated This function now requires a DrawModifier argument.
				@brief Inserts a draw call using auto-generated indices, while adding an offset to the vertex and instance indices.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				@note For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader must be compiled with the <c>-indirect-draw</c> flag.

				@param[in] indexCount		The index up to which to auto-generate the indices.
				@param[in] vertexOffset		The offset to add to each vertex index.
				@param[in] instanceOffset	The offset to add to each instance index.
				
				@see Gnm::DrawCommandBuffer::setPrimitiveType()
			  */
			SCE_GNM_API_CHANGED
			void drawIndexAuto(uint32_t indexCount, uint32_t vertexOffset, uint32_t instanceOffset)
			{
				return drawIndexAuto(indexCount, vertexOffset, instanceOffset, Gnm::kDrawModifierDefault);
			}

			
			/** @brief Inserts a draw call using streamout output.

				Draw commands never roll the hardware context, but use the current context such that the next command that sets context state will roll context.
				@note In NEO mode this function is incompatible with the kWdSwitchOnlyOnEopDisable in setVgtControlForNeo().

				@see Gnm::DrawCommandBuffer::setPrimitiveType()
			  */
			void drawOpaque()
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawOpaque can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_cue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawOpaqueAuto(Gnm::kDrawModifierDefault);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}
			
			/** @brief Inserts a draw call using streamout output while adding an offset to the vertex and instance indices.

				Draw commands never roll the hardware context, but use the current context such that the next command that sets context state will roll context.
				@note For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader must be compiled with the <c>-indirect-draw</c> flag.

				@param[in] vertexOffset		The offset to add to each vertex index.
				@param[in] instanceOffset	The offset to add to each instance index.
				@param[in] modifier			A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				
				@see Gnm::DrawCommandBuffer::setPrimitiveType()
			  */
			void drawOpaque(uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawOpaque can't be called between beginDispatchDraw and endDispatchDraw");
				m_cue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawOpaqueAuto(modifier);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @deprecated This function now requires a DrawModifier argument.
				@brief Inserts a draw call using streamout output, while adding an offset to the vertex and instance indices.

				Draw commands never roll the hardware context, but use the current context such that the next command that sets context state will roll context.
				@note For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader must be compiled with the <c>-indirect-draw</c> flag.

				@param[in] vertexOffset		The offset to add to each vertex index.
				@param[in] instanceOffset	The offset to add to each instance index.
				
				@see Gnm::DrawCommandBuffer::setPrimitiveType()
			  */
			SCE_GNM_API_CHANGED
			void drawOpaque(uint32_t vertexOffset, uint32_t instanceOffset)
			{
				return drawOpaque(vertexOffset, instanceOffset, Gnm::kDrawModifierDefault);
			}

			/** @brief Inserts a draw call using provided indices, which are inserted into the command buffer.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.

				@param[in] indexCount  Number of indices to insert. The maximum count is either 32764 (for 16-bit indices) or 16382 (for 32-bit indices).
				@param[in] indices     Pointer to first index in buffer containing <c><i>indexCount</i></c> indices. Pointer should be 4-byte aligned.
				@param[in] indicesSizeInBytes Size of the buffer pointed to by <c><i>indices</i></c>, expressed as a number of bytes. To specify the size of individual indices, use setIndexSize().
				                              The valid range is [0..65528] bytes.
			  
				@see Gnm::DrawCommandBuffer::setPrimitiveType(), Gnm::DrawCommandBuffer::setIndexSize()
			  */
			void drawIndexInline(uint32_t indexCount, const void *indices, uint32_t indicesSizeInBytes)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexInline can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_cue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawIndexInline(indexCount, indices, indicesSizeInBytes, Gnm::kDrawModifierDefault);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @brief Inserts a draw call using provided indices, which are inserted into the command buffer, while adding an offset to each of the vertex and instance indices.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				@note For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader must be compiled with the <c>-indirect-draw</c> flag.
			  
				@param[in] indexCount			The number of indices to insert.
				@param[in] indices				A pointer to the first index in the buffer containing <c><i>indexCount</i></c> indices. The pointer should be 4-byte aligned.
				@param[in] indicesSizeInBytes	The size (in bytes) of the <c><i>indices</i></c> buffer. To specify the size of an individual index, use setIndexSize().
				@param[in] vertexOffset			The offset to add to each vertex index.
				@param[in] instanceOffset		The offset to add to each instance index.
				@param[in] modifier				A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				
				@see Gnm::DrawCommandBuffer::setPrimitiveType(), Gnm::DrawCommandBuffer::setIndexSize()
			  */
			void drawIndexInline(uint32_t indexCount, const void *indices, uint32_t indicesSizeInBytes, uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexInline can't be called between beginDispatchDraw and endDispatchDraw");
				m_cue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawIndexInline(indexCount, indices, indicesSizeInBytes, modifier);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @deprecated This function now requires a DrawModifier argument.
				@brief Inserts a draw call using specified indices, which are inserted into the command buffer, while adding an offset to each of the vertex and instance indices.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				@note For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader must be compiled with the <c>-indirect-draw</c> flag.
			  
				@param[in] indexCount			The number of indices to insert.
				@param[in] indices				A pointer to the first index in the buffer containing <c><i>indexCount</i></c> indices. The pointer should be 4-byte aligned.
				@param[in] indicesSizeInBytes	The size of the buffer pointed to by <c><i>indices</i></c> in bytes. To specify the size of individual indices, use setIndexSize().
				@param[in] vertexOffset			The offset to add to each vertex index.
				@param[in] instanceOffset		The offset to add to each instance index.
				
				@see Gnm::DrawCommandBuffer::setPrimitiveType(), Gnm::DrawCommandBuffer::setIndexSize()
			  */
			SCE_GNM_API_CHANGED
			void drawIndexInline(uint32_t indexCount, const void *indices, uint32_t indicesSizeInBytes, uint32_t vertexOffset, uint32_t instanceOffset)
			{
				return drawIndexInline(indexCount, indices, indicesSizeInBytes, vertexOffset, instanceOffset, Gnm::kDrawModifierDefault);
			}

			/** @brief Inserts a draw call using indices which are located in memory.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
			  
				@param[in] indexCount			The number of indices to insert.
				@param[in] indexAddr			The GPU address of the index buffer.
				
				@see Gnm::DrawCommandBuffer::setPrimitiveType(), Gnm::DrawCommandBuffer::setIndexSize()
			  */
			void drawIndex(uint32_t indexCount, const void *indexAddr)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndex can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_cue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawIndex(indexCount, indexAddr, Gnm::kDrawModifierDefault);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @brief Inserts a draw call using indices that are located in memory, while adding an offset to the vertex and instance indices.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				@note For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader must be compiled with -indirect-draw flag.
			  
				@param[in] indexCount			The number of indices to insert.
				@param[in] indexAddr			The GPU address of the index buffer.
				@param[in] vertexOffset			The offset to add to each vertex index.
				@param[in] instanceOffset		The offset to add to each instance index.
				@param[in] modifier				A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				
				@see Gnm::DrawCommandBuffer::setPrimitiveType(), Gnm::DrawCommandBuffer::setIndexSize()
			  */
			void drawIndex(uint32_t indexCount, const void *indexAddr, uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndex can't be called between beginDispatchDraw and endDispatchDraw");
				m_cue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawIndex(indexCount, indexAddr, modifier);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @brief Inserts a draw call using indices located in memory, while adding an offset to each of the vertex and instance indices.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				@note For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader must be compiled with the <c>-indirect-draw</c> flag.
			  
				@param[in] indexCount			The number of indices to insert.
				@param[in] indexAddr			The GPU address of the index buffer.
				@param[in] vertexOffset			The offset to add to each vertex index.
				@param[in] instanceOffset		The offset to add to each instance index.
				
				@see Gnm::DrawCommandBuffer::setPrimitiveType(), Gnm::DrawCommandBuffer::setIndexSize()
			  */
			void drawIndex(uint32_t indexCount, const void *indexAddr, uint32_t vertexOffset, uint32_t instanceOffset)
			{
				return drawIndex(indexCount, indexAddr, vertexOffset, instanceOffset, Gnm::kDrawModifierDefault);
			}


			/** @brief Inserts an instanced, indexed draw call that sources object ID values from the specified buffer.
				@param[in] indexCount		The number of entries to process from the index buffer.
				@param[in] instanceCount    The number of instances to render.
				@param[in] indexAddr		The address of the index buffer. This must not be set to <c>NULL</c>.
				@param[in] objectIdAddr     The address of the object ID buffer. This must not be set to <c>NULL</c>.
				@note  NEO mode only
				@note instanceCount has to match the number of instances set with the setNumInstances().
				@cmdsize 11
			*/
			void drawIndexMultiInstanced(uint32_t indexCount, uint32_t instanceCount, const void *indexAddr, const void *objectIdAddr)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexMultiInstanced can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_cue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawIndexMultiInstanced(indexCount, instanceCount, indexAddr, objectIdAddr, Gnm::kDrawModifierDefault);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @brief Inserts an instanced, indexed draw call that sources object ID values from the specified buffer.
				@param[in] indexCount		The number of entries to process from the index buffer.
				@param[in] instanceCount    The number of instances to render.
				@param[in] indexAddr		The address of the index buffer. This must not be set to <c>NULL</c>.
				@param[in] objectIdAddr     The address of the object ID buffer. This must not be set to <c>NULL</c>.
				@param[in] vertexOffset		The offset to add to each vertex index.
				@param[in] instanceOffset	The offset to add to each instance index.
				@param[in] modifier			A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				@note  NEO mode only
				@note instanceCount has to match the number of instances set with the setNumInstances().
				@cmdsize 11
			*/
			void drawIndexMultiInstanced(uint32_t indexCount, uint32_t instanceCount, const void *indexAddr, const void *objectIdAddr, uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexMultiInstanced can't be called between beginDispatchDraw and endDispatchDraw");
				m_cue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawIndexMultiInstanced(indexCount, instanceCount, indexAddr, objectIdAddr, modifier);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @brief Inserts an indirect draw call, which reads its parameters from a specified address in GPU memory.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				
				@param[in] dataOffsetInBytes		The offset (in bytes) into the indirect arguments buffer. This function reads arguments from the buffer at this offset. 
													The data at this offset should be a Gnm::DrawIndirectArgs structure.
				@param[in] modifier					A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				
				@note The buffer containing the indirect arguments must already have been set using setBaseIndirectArgs().
				@note The <c><i>instanceCount</i></c> value specified in the DrawIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			         is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
				
				@see Gnm::DrawIndirectArgs, setBaseIndirectArgs()
			*/
			void drawIndirect(uint32_t dataOffsetInBytes, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndirect can't be called between beginDispatchDraw and endDispatchDraw");
				// no need to set vertex and instance offsets here, they are coming from the draw indirect structure.
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				switch( m_cue.getActiveShaderStages())
				{
				case Gnm::kActiveShaderStagesVsPs:
					{
						const Gnmx::VsShader *pvs = m_cue.m_currentVSB;
						m_dcb.drawIndirect(dataOffsetInBytes,Gnm::kShaderStageVs,(uint8_t)pvs->getVertexOffsetUserRegister(),(uint8_t)pvs->getInstanceOffsetUserRegister(), modifier);
					}
					break;
				case Gnm::kActiveShaderStagesEsGsVsPs:
					{
						const Gnmx::EsShader *pes = m_cue.m_currentESB;
						m_dcb.drawIndirect(dataOffsetInBytes,Gnm::kShaderStageEs,(uint8_t)pes->getVertexOffsetUserRegister(),(uint8_t)pes->getInstanceOffsetUserRegister(), modifier);
					}
					break;
				case Gnm::kActiveShaderStagesLsHsVsPs:
				case Gnm::kActiveShaderStagesLsHsEsGsVsPs:
				case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
				case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
					{
						const Gnmx::LsShader *pls = m_cue.m_currentLSB;
						m_dcb.drawIndirect(dataOffsetInBytes,Gnm::kShaderStageLs,(uint8_t)pls->getVertexOffsetUserRegister(),(uint8_t)pls->getInstanceOffsetUserRegister(), modifier);
					}
					break;
				case Gnm::kActiveShaderStagesDispatchDrawVsPs:
					SCE_GNM_ERROR_INLINE("drawIndirect does not support DispatchDaw");
				}
				
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}
			/** @brief Inserts an indirect draw call, which reads its parameters from a specified address in GPU memory.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				
				@param[in] dataOffsetInBytes		The offset (in bytes) into the indirect arguments buffer. This function reads arguments from the buffer at this offset.
													The data at this offset should be a Gnm::DrawIndirectArgs structure.
				
				@note The buffer containing the indirect arguments should already have been set using setBaseIndirectArgs().
				@note The <c><i>instanceCount</i></c> value specified in the DrawIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			         is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
				
				@see Gnm::DrawIndirectArgs, setBaseIndirectArgs()
			*/
			void drawIndirect(uint32_t dataOffsetInBytes)
			{
				return drawIndirect(dataOffsetInBytes, Gnm::kDrawModifierDefault);
			}

			/** @brief Inserts an indirect multi-draw call that optionally reads the draw count from GPU memory, and which has a similar effect to calling drawIndirect() several times
						with multiple DrawIndirectArgs objects packed tightly in memory.

				Draw commands never roll the hardware context, but use the current context such that the next command that sets context state will roll context.

				Each draw reads a new DrawIndirectArgs object from the indirect arguments buffer.

				@param[in] dataOffsetInBytes	Offset (in bytes) into the buffer that contains the first draw call's indirect arguments, set using setBaseIndirectArgs().
												The data at this offset should be a Gnm::DrawIndirectArgs structure.
				@param[in] count				If <c><i>countAddress</i></c> is <c>NULL</c>, this value defines the number of draw calls to launch. If <c>countAddress</c> is non-<c>NULL</c>,
												then this value will be used to clamp the draw count read from <c>countAddress</c>. 
				@param[in] countAddress			If non-<c>NULL</c>, this is a <c>DWORD</c>-aligned address from which the GPU will read the draw count, prior to clamping using the value provided in <c>count</c>.
				@param[in] modifier				A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.

				@note The buffer containing the indirect arguments should already have been set using setBaseIndirectArgs().
				@note The <c><i>instanceCount</i></c> value specified in the DrawIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			         is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.

				@see Gnm::DrawIndirectArgs, setBaseIndirectArgs()
			*/
			void drawIndirectCountMulti(uint32_t dataOffsetInBytes, uint32_t count, void *countAddress, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndirectCountMulti can't be called between beginDispatchDraw and endDispatchDraw");
				// no need to set vertex and instance offsets here, they are coming from the draw indirect structure.
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				switch( m_cue.getActiveShaderStages())
				{
				case Gnm::kActiveShaderStagesVsPs:
					{
						const Gnmx::VsShader *pvs = m_cue.m_currentVSB;
						m_dcb.drawIndirectCountMulti(dataOffsetInBytes,count,countAddress,Gnm::kShaderStageVs,(uint8_t)pvs->getVertexOffsetUserRegister(),(uint8_t)pvs->getInstanceOffsetUserRegister(), modifier);
					}
					break;
				case Gnm::kActiveShaderStagesEsGsVsPs:
					{
						const Gnmx::EsShader *pes = m_cue.m_currentESB;
						m_dcb.drawIndirectCountMulti(dataOffsetInBytes,count,countAddress,Gnm::kShaderStageEs,(uint8_t)pes->getVertexOffsetUserRegister(),(uint8_t)pes->getInstanceOffsetUserRegister(), modifier);
					}
					break;
				case Gnm::kActiveShaderStagesLsHsVsPs:
				case Gnm::kActiveShaderStagesLsHsEsGsVsPs:
				case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
				case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
					{
						const Gnmx::LsShader *pls = m_cue.m_currentLSB;
						m_dcb.drawIndirectCountMulti(dataOffsetInBytes,count,countAddress,Gnm::kShaderStageLs,(uint8_t)pls->getVertexOffsetUserRegister(),(uint8_t)pls->getInstanceOffsetUserRegister(), modifier);
					}
					break;
				default:
					SCE_GNM_ERROR_INLINE("drawIndirectCountMulti does not support DispatchDaw");
				}
				
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @brief Inserts an indirect multi-draw call that optionally reads the draw count from GPU memory, and which has a similar effect to calling drawIndirect() several times
						with multiple DrawIndirectArgs objects packed tightly in memory.

				Draw commands never roll the hardware context, but use the current context such that the next command that sets context state will roll context.

				Each draw reads a new DrawIndirectArgs object from the indirect arguments buffer.

				@param[in] dataOffsetInBytes	Offset (in bytes) into the buffer that contains the first draw call's indirect arguments, set using setBaseIndirectArgs().
												The data at this offset should be a Gnm::DrawIndirectArgs structure.
				@param[in] count				If <c><i>countAddress</i></c> is NULL, the value of the <c>count</c> parameter defines the number of draw calls to launch. If <c><i>countAddress</i></c> is non-<c>NULL</c>,
												then the value of the <c>count</c> parameter will be used to clamp the draw count read from <c>countAddress</c>. 
				@param[in] countAddress			If non-<c>NULL</c>, this value is a <c>DWORD</c>-aligned address from which the GPU will read the draw count, prior to clamping using the value provided in <c>count</c>.

				@note The buffer containing the indirect arguments should already have been set using setBaseIndirectArgs().
				@note The <c><i>instanceCount</i></c> value specified in the DrawIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
			         is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.

				@see Gnm::DrawIndexIndirectArgs, setBaseIndirectArgs()
			*/
			void drawIndirectCountMulti(uint32_t dataOffsetInBytes, uint32_t count, void *countAddress)
			{
				return drawIndirectCountMulti(dataOffsetInBytes, count, countAddress, Gnm::kDrawModifierDefault);
			}

			/** @brief Inserts an indirect drawIndex call, which reads its parameters from a specified address in the GPU memory.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				
				@param[in] dataOffsetInBytes	The offset (in bytes) into the buffer that contains the indirect arguments, which is set using setBaseIndirectArgs().
												The data at this offset should be a Gnm::DrawIndexIndirectArgs structure.
				@param[in] modifier				A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				
				@note	The index buffer should already have been set up using setIndexBuffer() and setIndexCount(),
						and the buffer containing the indirect arguments should have been set using setBaseIndirectArgs().
						The index count in <c>args->m_indexCountPerInstance</c> will be clamped to the value passed to setIndexCount().
				@note	The <c><i>instanceCount</i></c> value specified in the DrawIndexIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
						is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
				@see Gnm::DrawIndexIndirectArgs, setBaseIndirectArgs(), setIndexBuffer(), setIndexCount()
			*/
			void drawIndexIndirect(uint32_t dataOffsetInBytes, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexIndirect can't be called between beginDispatchDraw and endDispatchDraw");
				// no need to set vertex and instance offsets here, they are coming from the draw indirect structure.
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				switch( m_cue.getActiveShaderStages())
				{
				case Gnm::kActiveShaderStagesVsPs:
					{
						const Gnmx::VsShader *pvs = m_cue.m_currentVSB;
						m_dcb.drawIndexIndirect(dataOffsetInBytes,Gnm::kShaderStageVs,pvs->m_fetchControl&0xf,(pvs->m_fetchControl>>4)&0xf, modifier);
					}
					break;
				case Gnm::kActiveShaderStagesEsGsVsPs:
					{
						const Gnmx::EsShader *pes = m_cue.m_currentESB;
						m_dcb.drawIndexIndirect(dataOffsetInBytes,Gnm::kShaderStageEs,pes->m_fetchControl&0xf,(pes->m_fetchControl>>4)&0xf, modifier);
					}
					break;
				case Gnm::kActiveShaderStagesLsHsVsPs:
				case Gnm::kActiveShaderStagesLsHsEsGsVsPs:
				case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
				case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
					{
						const Gnmx::LsShader *pls = m_cue.m_currentLSB;
						m_dcb.drawIndexIndirect(dataOffsetInBytes,Gnm::kShaderStageLs,pls->m_fetchControl&0xf,(pls->m_fetchControl>>4)&0xf, modifier);
					}
					break;
				case Gnm::kActiveShaderStagesDispatchDrawVsPs:
					SCE_GNM_ERROR_INLINE("drawIndexIndirect does not support DispatchDaw");
				}
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @brief Inserts an indirect drawIndex call, which reads its parameters from a specified address in the GPU memory.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				
				@param[in] dataOffsetInBytes	The offset (in bytes) into the buffer that contains the indirect arguments, which is set using setBaseIndirectArgs().
												The data at this offset should be a Gnm::DrawIndexIndirectArgs structure.
				
				@note	The index buffer should already have been set up using setIndexBuffer() and setIndexCount(),
						and the buffer containing the indirect arguments should have been set using setBaseIndirectArgs().
						The index count in <c>args->m_indexCountPerInstance</c> will be clamped to the value passed to setIndexCount().
				@note	The <c><i>instanceCount</i></c> value specified in the DrawIndexIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
						is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
				@see Gnm::DrawIndexIndirectArgs, setBaseIndirectArgs(), setIndexBuffer(), setIndexCount()
			*/
			void drawIndexIndirect(uint32_t dataOffsetInBytes)
			{
				return drawIndexIndirect(dataOffsetInBytes, Gnm::kDrawModifierDefault);
			}


			/** @brief Inserts an indirect multi-drawIndex call that optionally reads the draw count from GPU memory, and which has a similar effect to calling drawIndexndirect() several times
							with multiple DrawIndexIndirectArgs objects packed tightly in memory.	

				Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll context.

				Each draw reads a new DrawIndexIndirectArgs object from the indirect arguments buffer.
				
				@param[in] dataOffsetInBytes	The offset (in bytes) into the buffer that contains the indirect arguments, which is set using setBaseIndirectArgs().
												The data at this offset should be a Gnm::DrawIndexIndirectArgs structure.
				@param[in] count				If <c><i>countAddress</i></c> is <c>NULL</c>, the value of the <c>count</c> parameter defines the number of draw calls to launch. If <c><i>countAddress</i></c> is non-NULL,
												then the value of the <c>count</c> parameter will be used to clamp the draw count read from <c>countAddress</c>. 
				@param[in] countAddress			If non-<c>NULL</c>, this value is a <c>DWORD</c>-aligned address from which the GPU will read the draw count, prior to clamping using the value provided in <c>count</c>.
				@param[in] modifier				A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				
				@note	The index buffer and the buffer containing the indirect arguments should already have been set up using setIndexBuffer() and setIndexCount().
						The index count in <c><i>args</i>-><i>m_indexCountPerInstance</i></c> will be clamped to the value passed to setIndexCount().
				@note	The <c><i>instanceCount</i></c> value specified in the DrawIndexIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
						is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
				
				@see Gnm::DrawIndexIndirectArgs, setBaseIndirectArgs(), setIndexBuffer(), setIndexCount()
			*/
			void drawIndexIndirectCountMulti(uint32_t dataOffsetInBytes, uint32_t count, void *countAddress, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexIndirectCountMulti can't be called between beginDispatchDraw and endDispatchDraw");
				// no need to set vertex and instance offsets here, they are coming from the draw indirect structure.
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				switch( m_cue.getActiveShaderStages())
				{
				case Gnm::kActiveShaderStagesVsPs:
					{
						const Gnmx::VsShader *pvs = m_cue.m_currentVSB;
						m_dcb.drawIndexIndirectCountMulti(dataOffsetInBytes,count,countAddress,Gnm::kShaderStageVs,pvs->m_fetchControl&0xf,(pvs->m_fetchControl>>4)&0xf, modifier);
					}
					break;
				case Gnm::kActiveShaderStagesEsGsVsPs:
					{
						const Gnmx::EsShader *pes = m_cue.m_currentESB;
						m_dcb.drawIndexIndirectCountMulti(dataOffsetInBytes,count,countAddress,Gnm::kShaderStageEs,pes->m_fetchControl&0xf,(pes->m_fetchControl>>4)&0xf, modifier);
					}
					break;
				case Gnm::kActiveShaderStagesLsHsVsPs:
				case Gnm::kActiveShaderStagesLsHsEsGsVsPs:
				case Gnm::kActiveShaderStagesOffChipLsHsVsPs:
				case Gnm::kActiveShaderStagesOffChipLsHsEsGsVsPs:
					{
						const Gnmx::LsShader *pls = m_cue.m_currentLSB;
						m_dcb.drawIndexIndirectCountMulti(dataOffsetInBytes,count,countAddress,Gnm::kShaderStageLs,pls->m_fetchControl&0xf,(pls->m_fetchControl>>4)&0xf, modifier);
					}
					break;
				default:
					SCE_GNM_ERROR_INLINE("drawIndexIndirectCountMulti does not support DispatchDaw");
				}
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}


			/** @brief Inserts an indirect multi-drawIndex call that optionally reads the draw count from GPU memory, and which has a similar effect to calling drawIndexndirect() several times
							with multiple DrawIndexIndirectArgs objects packed tightly in memory.	

				Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll context.

				Each draw reads a new DrawIndexIndirectArgs object from the indirect arguments buffer.
				
				@param[in] dataOffsetInBytes	The offset (in bytes) into the buffer that contains the indirect arguments, which is set using setBaseIndirectArgs().
												The data at this offset should be a Gnm::DrawIndexIndirectArgs structure.
				@param[in] count				If <c><i>countAddress</i></c> is <c>NULL</c>, the value of the <c>count</c> parameter defines the number of draw calls to launch. If <c><i>countAddress</i></c> is non-<c>NULL</c>,
												then the value of the <c>count</c> parameter will be used to clamp the draw count read from <c>countAddress</c>. 
				@param[in] countAddress			If non-<c>NULL</c>, this is a <c>DWORD</c>-aligned address from which the GPU will read the draw count, prior to clamping using the value provided in <c>count</c>.
				
				@note	The index buffer and the buffer containing the indirect arguments should already have been set up using setIndexBuffer() and setIndexCount().
						The index count in <c><i>args</i>-><i>m_indexCountPerInstance</i></c> will be clamped to the value passed to setIndexCount().
				@note	The <c><i>instanceCount</i></c> value specified in the DrawIndexIndirectArgs structure will continue to affect subsequent draw calls. Therefore, it is recommended strongly that this command
						is always followed immediately by a call to setNumInstances(), in order to restore the instance count to a known value.
				
				@see Gnm::DrawIndexIndirectArgs, setBaseIndirectArgs(), setIndexBuffer(), setIndexCount()
			*/
			void drawIndexIndirectCountMulti(uint32_t dataOffsetInBytes, uint32_t count, void *countAddress)
			{
				return drawIndexIndirectCountMulti(dataOffsetInBytes, count, countAddress, Gnm::kDrawModifierDefault);
			}

			/** @brief Inserts a draw call using indices which are located in memory and whose base, size, and element size were set previously.

				Draw commands never roll the hardware context but instead use the current context such that the next command that sets context state will roll the context.
				
				@param[in] indexOffset			The starting index number in the index buffer.
				@param[in] indexCount			The number of indices to insert.
				
				@see Gnm::DrawCommandBuffer::setIndexBuffer(), Gnm::DrawCommandBuffer::setIndexCount(), Gnm::DrawCommandBuffer::setIndexSize()
			*/
			void drawIndexOffset(uint32_t indexOffset, uint32_t indexCount)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexOffset can't be called between beginDispatchDraw and endDispatchDraw");
				SCE_GNM_ASSERT_MSG_INLINE(!m_cue.isVertexOrInstanceOffsetEnabled(), "Using a shader that is expecting a vertex and/or instance offset without specifing them");
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawIndexOffset(indexOffset, indexCount, Gnm::kDrawModifierDefault);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @brief Using indices located in memory and previously set values for base, size and element size, inserts a draw call while adding an offset to the supplied vertex and instance indices.

				Draw commands never roll the hardware context, but use the current context such that the next command that sets context state will roll the context.
				@note For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader must be compiled with the <c>-indirect-draw</c> flag.
				
				@param[in] indexOffset			The starting index number in the index buffer.
				@param[in] indexCount			The number of indices to insert.
				@param[in] vertexOffset			The offset to add to each vertex index.
				@param[in] instanceOffset		The offset to add to each instance index.
				@param[in] modifier				A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				
				@see Gnm::DrawCommandBuffer::setIndexBuffer(), Gnm::DrawCommandBuffer::setIndexCount(), Gnm::DrawCommandBuffer::setIndexSize()
			*/
			void drawIndexOffset(uint32_t indexOffset, uint32_t indexCount, uint32_t vertexOffset, uint32_t instanceOffset, Gnm::DrawModifier modifier)
			{
				SCE_GNM_ASSERT_MSG_INLINE((m_dispatchDrawFlags & kDispatchDrawFlagInDispatchDraw) == 0, "drawIndexOffset can't be called between beginDispatchDraw and endDispatchDraw");
				m_cue.setVertexAndInstanceOffset(vertexOffset,instanceOffset);
				m_cue.preDraw();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.drawIndexOffset(indexOffset, indexCount, modifier);
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDraw();
			}

			/** @deprecated This function now requires a DrawModifier argument.
				@brief Using indices located in memory and previously set values for base, size and element size, inserts a draw call while adding an offset to the supplied vertex and instance indices.

				Draw commands never roll the hardware context, but use the current context such that the next command that sets context state will roll the context.
				@note For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader must be compiled with the <c>-indirect-draw</c> flag.
				
				@param[in] indexOffset			The starting index number in the index buffer.
				@param[in] indexCount			The number of indices to insert.
				@param[in] vertexOffset			The offset to add to each vertex index.
				@param[in] instanceOffset		The offset to add to each instance index.
				
				@see Gnm::DrawCommandBuffer::setIndexBuffer(), Gnm::DrawCommandBuffer::setIndexCount(), Gnm::DrawCommandBuffer::setIndexSize()
			*/
			SCE_GNM_API_CHANGED
			void drawIndexOffset(uint32_t indexOffset, uint32_t indexCount, uint32_t vertexOffset, uint32_t instanceOffset)
			{
				return drawIndexOffset(indexOffset, indexCount, vertexOffset, instanceOffset, Gnm::kDrawModifierDefault);
			}

			////////////// Dispatch commands

			/** @brief Inserts a compute shader dispatch with the indicated number of thread groups.
				This function never rolls the hardware context.
				@param[in] threadGroupX Number of thread groups dispatched along the X dimension. This must be in the range [1..2,147,483,647].
				@param[in] threadGroupY Number of thread groups dispatched along the Y dimension. This must be in the range [1..2,147,483,647].
				@param[in] threadGroupZ Number of thread groups dispatched along the Z dimension. This must be in the range [1..2,147,483,647].
			*/
			void dispatch(uint32_t threadGroupX, uint32_t threadGroupY, uint32_t threadGroupZ)
			{
				m_cue.preDispatch();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.dispatchWithOrderedAppend(threadGroupX, threadGroupY, threadGroupZ, m_cue.getCsShaderOrderedAppendMode());
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDispatch();
			}

			/** @brief Inserts an indirect compute shader dispatch, whose parameters are read from GPU memory.
			This function never rolls the hardware context.
			@param[in] dataOffsetInBytes Offset (in bytes) into the buffer that contains the indirect arguments (set using <c>setBaseIndirectArgs()</c>).
			                  The data at this offset should be a Gnm::DispatchIndirectArgs structure.
			@note The buffer containing the indirect arguments should already have been set using <c>setBaseIndirectArgs()</c>.
			@see Gnm::DispatchCommandBuffer::setBaseIndirectArgs()
			*/
			void dispatchIndirect(uint32_t dataOffsetInBytes)
			{
				m_cue.preDispatch();
#if SCE_GNMX_RECORD_LAST_COMPLETION
				const uint32_t offset = beginRecordLastCompletion();
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_dcb.dispatchIndirectWithOrderedAppend(dataOffsetInBytes, m_cue.getCsShaderOrderedAppendMode());
#if SCE_GNMX_RECORD_LAST_COMPLETION
				endRecordLastCompletion(offset);
#endif //SCE_GNMX_RECORD_LAST_COMPLETION
				m_cue.postDispatch();
			}

			/** @brief Inserts an asynchronous compute shader Gnm::DispatchCommandBuffer::dispatchDraw() and a paired vertex shader Gnm::DrawCommandBuffer::dispatchDraw().

				Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll the context.

				@note dispatchDraw() also has the following state requirements to function properly in addition to those set directly by beginDispatchDraw() and dispatchDraw():
				      <c>setActiveShaderStages(Gnm::kActiveShaderStagesDispatchDrawVsPs)</c> : Always required by dispatch draw hardware.
				      <c>setNumInstances(1)</c> : Dispatch draw is not compatible with hardware instancing. Instancing is implemented in software via setDispatchDrawNumInstances().
				
				@note Unless the compute dispatches are guaranteed not to fill the ring buffers (and halt) or are limited to no more than 1 thread group per CU,
				      tessellation or on-chip GS draw commands cannot be overlapped by DispatchCommandBuffer::dispatchDraw() compute dispatches.
				      A GPU hang can result if a tessellation or on-chip GS thread group launches on a CU that is already hosting more than 1 thread group
				      of compute, which is waiting for space in the dispatch draw ring buffers. In general, if any dispatchDraw commands will follow, it is safer
					  to prevent all overlap of dispatch draw compute with tessellation or on-chip GS draw commands by calling
					  GfxContext::dispatchDrawComputeWaitForEndOfPipe() after the last tessellation or on-chip GS draw command.			   

				@note If a dispatchDraw() call is still likely to be vertex-bound after any primitive culling performed by compute, adjusting the sub-draw flush
					  thresholding parameters <c>primGroupThreshold</c> and <c>pollIntervalThreshold</c> may improve performance by packing the output vertices into fewer VS
					  wavefronts, at some cost in additional command-processor and vertex-wavefront-launching latency.

				@param[in] pDispatchDrawIndexData	The input data to a dispatch draw asynchronous compute shader, which should be generated by Gnmx::createDispatchDrawInputData().
				@param[in] primGroupThreshold The output threshold for a sub-draw to be generated, expressed in multiples of <c><i>primGroupIndexCount</i></c>.  A sub-draw will be flushed whenever the available output from compute shaders exceeds this limit.
				@param[in] pollIntervalThreshold The timeout for a sub-draw to be generated, expressed in multiples of polling intervals (approximately 50us each.)  A sub-draw will be flushed after this timeout once there are also at least <c><i>primGroupIndexCount</i></c> indices available.
				@param[in] modifier A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
			   */
			void dispatchDraw(DispatchDrawTriangleCullIndexData const *pDispatchDrawIndexData, uint32_t primGroupThreshold, uint32_t pollIntervalThreshold, Gnm::DrawModifier modifier);

			/** @brief Inserts an asynchronous compute shader Gnm::DispatchCommandBuffer::dispatchDraw() and a paired vertex shader Gnm::DrawCommandBuffer::dispatchDraw().

				Draw commands never roll the hardware context but use the current context such that the next command that sets context state will roll the context.

				@note dispatchDraw() also has the following state requirements to function properly, in addition to those set directly by beginDispatchDraw() and dispatchDraw():
				      /li <c>setActiveShaderStages(Gnm::kActiveShaderStagesDispatchDrawVsPs)</c> : Always required by dispatch draw hardware.
				      /li <c>setNumInstances(1)</c> : Dispatch draw is not compatible with hardware instancing. Instancing is implemented in software by means of setDispatchDrawNumInstances().
				
				@note Unless the compute dispatches are guaranteed not to fill the ring buffers (and halt) or are limited to no more than 1 thread group per CU,
				      tessellation or on-chip GS draw commands cannot be overlapped by DispatchCommandBuffer::dispatchDraw() compute dispatches.
				      A GPU hang can result if a tessellation or on-chip GS thread group launches on a CU that is already hosting more than 1 thread group
				      of compute that is waiting for space in the dispatch draw ring buffers. In general, if any dispatchDraw commands will follow, it is safer
					  to prevent all overlap of dispatch draw compute with tessellation or on-chip GS draw commands by calling
					  GfxContext::dispatchDrawComputeWaitForEndOfPipe() after the last tessellation or on-chip GS draw command.

				@note If a dispatchDraw() call is still likely to be vertex-bound after any primitive culling performed by compute, adjusting the sub-draw flush
					  thresholding parameters <c>primGroupThreshold</c> and <c>pollIntervalThreshold</c> may improve performance by packing the output vertices into fewer VS
					  wavefronts, at some cost in additional command-processor and vertex-wavefront-launching latency.

				@param[in] pDispatchDrawIndexData	The input data to a dispatch draw asynchronous compute shader, which should be generated by Gnmx::createDispatchDrawInputData().
			   */
			inline void dispatchDraw(DispatchDrawTriangleCullIndexData const *pDispatchDrawIndexData)
			{
				dispatchDraw(pDispatchDrawIndexData, 0, 0, Gnm::kDrawModifierDefault);
			}

#ifdef SCE_GNMX_ENABLE_GFXCONTEXT_CALLCOMMANDBUFFERS // Disabled by default until a future release
			/** @brief Calls another command buffer pair. When the called command buffers end, execution will continue
			           in the current command buffer.
					   
				@param dcbBaseAddr			The address of the destination dcb. This pointer must not be <c>NULL</c> if <c><i>dcbSizeInDwords</i></c> is non-zero.
				@param dcbSizeInDwords		The size of the destination dcb in <c>DWORD</c>s. This may be 0 if no dcb is required.
				@param ccbBaseAddr			The address of the destination ccb. This pointer must not be <c>NULL</c> if <c><i>ccbSizeInDwords</i></c> is non-zero.
				@param ccbSizeInDwords		The size of the destination ccb in <c>DWORD</c>s. This may be 0 if no ccb is required.
				
				@note After this function is called, all previous resource/shader bindings and render state are undefined.
				@note  Calls can recurse one level deep only. The command buffer submitted to the GPU can call another command buffer, but the callee cannot contain
				       any call commands of its own.
			*/
			void callCommandBuffers(void *dcbAddr, uint32_t dcbSizeInDwords, void *ccbAddr, uint32_t ccbSizeInDwords);
#endif

			// auto-generated method forwarding to m_dcb
//			#include "gfxcontext_methods.h"

		public:
			
			ConstantUpdateEngine       m_cue; ///< Constant update engine. Access directly at your own risk!
		};
	}
}
#endif
