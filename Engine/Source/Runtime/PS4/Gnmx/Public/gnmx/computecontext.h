/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if !defined(_SCE_GNMX_COMPUTECONTEXT_H)
#define _SCE_GNMX_COMPUTECONTEXT_H

#include <gnm/constantcommandbuffer.h>
#include <gnm/dispatchcommandbuffer.h>
#include <gnmx/constantupdateengine.h>
#include <gnmx/helpers.h>
#include <gnmx/lwcue_compute.h>
#include <gnmx/resourcebarrier.h>

namespace sce
{
	namespace Gnmx
	{
		/** @brief Encapsulates a Gnm::DispatchCommandBuffer in a higher-level interface.
		 *
		 *	This should be a new developer's main entry point into the PlayStation®4 rendering API.
		 *	The ComputeContext object submits to the specified compute ring and only supports compute operations; for graphics tasks, use the GfxContext class.
		 *	@see GfxContext
		 */
		class SCE_GNMX_EXPORT ComputeContext 
		{
		public:
			/** @brief Default constructor. */
			ComputeContext(void);
			/** @brief Default destructor. */
			~ComputeContext(void);

			/** @brief Initializes a ComputeContext with application-provided memory buffers.
				@param[out] dcbBuffer A buffer for use by the Gnm::DispatchCommandBuffer.
				@param[in] dcbSizeInBytes The size of <c><i>dcbBuffer</i></c>, in bytes.
				*/
			void init(void *dcbBuffer, uint32_t dcbSizeInBytes);


#if defined(__ORBIS__)
			/** @brief Initializes a ComputeContext with application-provided memory buffers including an instance of the LCUE to manage resource bindings.
			 *
			 *  @param[in] dcbBuffer						A buffer for use by the Gnm::DispatchCommandBuffer.
			 *  @param[in] dcbBufferSizeInBytes				The size of <c><i>dcbBuffer</i></c> in bytes.
			 *  @param[in] resourceBufferInGarlic			A resource buffer for use by the LCUE, can be <c>NULL</c> if <c><i>resourceBufferSizeInBytes</i></c> is equal to <c>0</c>.
			 *  @param[in] resourceBufferSizeInBytes		The size of <c><i>resourceBufferInGarlic</i></c> in bytes, can be <c>0</c>.
			 *  @param[in] globalInternalResourceTableAddr	A pointer to the global resource table in Garlic memory, can be <c>NULL</c>.
			 *
			 *  @note The ability to initialize the ComputeContext with no resource buffer (<c>resourceBufferInGarlic = NULL</c>) relies on your registration of a "resource buffer full" callback function
			 *		  that handles out-of-memory conditions.
			 *
			 *  @see ComputeContext::setResourceBufferFullCallback()
			 */
			void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr);
#endif

			/** @brief Resets the Gnm::DispatchCommandBuffer for a new frame. 
			
				Call this at the beginning of every frame. The Gnm::DispatchCommandBuffer will be reset to empty (<c>m_cmdptr = m_beginptr</c>) when not using the LCUE.

				@note In-order to ensure correct functionality with the LCUE, if the ComputeContext has been initialized to use the LCUE, the command buffer will not be empty after it has been reset. 
				reset() will additionally insert a call to flushShaderCachesAndWait().
			*/
			void reset(void);

#if defined(__ORBIS__)

			/** @brief Writes a resource barrier to the command stream.
			 *
			 *	@param barrier The barrier to write. This pointer must not be <c>NULL</c>.
			 */
			void writeResourceBarrier(const ResourceBarrier *barrier)
			{
				return barrier->write(&m_dcb);
			}

			/** @brief Binds a function for the LCUE to call when the main resource buffer runs out of memory.
			 *
			 *	@param callbackFunction	Developer-supplied function to call when the resource buffer runs out of memory.
			 *	@param userData			The user data passed to <c><i>callbackFunction</i></c>.
			 *
			 *	@note The ability to call ComputeContext::init() without supplying a resource buffer requires a developer-supplied callback function.
			 *
			 *	@see ComputeContext::init()
			 */
			void setResourceBufferFullCallback(AllocResourceBufferCallback callbackFunction, void* userData)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				m_lwcue.setResourceBufferFullCallback(callbackFunction, userData);
			}

			/** @brief Binds a CS shader to the CS stage.
			 *
			 *  @param[in] shader			The pointer to the CS shader.
			 *
			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*shader</c> must not change until the GPU has completed the dispatch!
			 *	@note This binding will not take effect on the GPU until <c>preDispatch()</c> is called.
			 *  @note This function must be called first before any resource bindings calls. If setCsShader() is called again, all resource bindings for the stage will need to be rebound.
			 *  @note This function will regenerate the InputResourceOffsets table on every binding. It is not recommended to call this function regularly; instead cache the table and call setCsShader() with 
			 *  an InputResourceOffsets table explicitly.
			 */
			void setCsShader(const sce::Gnmx::CsShader* shader)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				generateInputResourceOffsetTable(&m_boundInputResourceOffsets, Gnm::kShaderStageCs, shader);
				setCsShader(shader, &m_boundInputResourceOffsets);
			}

			/** @brief Binds a CS shader to the CS stage using an explicitly specified InputResourceOffsets table.
			 *
			 *  @param[in] shader			The pointer to the CS shader.
			 *	@param[in] table Matching	The ShaderResourceOffsets table created by generateShaderResourceOffsetTable().

			 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*shader</c> must not change until the GPU has completed the dispatch!
			 *	@note This binding will not take effect on the GPU until <c>preDispatch()</c> is called.
			 *  @note This function must be called first before any resource bindings calls. If setCsShader() is called again, all resource bindings for the stage will need to be rebound.
			 */
			void setCsShader(const sce::Gnmx::CsShader* shader, const InputResourceOffsets* table)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				m_lwcue.setCsShader(shader, table);
			}

			/** @brief Specifies a range of the Global Data Store to be used by shaders for atomic global counters such as those
			 *  used to implement PSSL <c>AppendRegularBuffer</c> and <c>ConsumeRegularBuffer</c> objects.
			 *
			 *  Each counter is a 32-bit integer. The counters for this CS shader stage may have a different offset in GDS. For example:
			 *
			 *  @code
			 *     setAppendConsumeCounterRange(0x0400, 4)  // Set up 1 counter for the CS stage at offset 0x400.
			 *	@endcode
			 *
			 *  The index is defined by the chosen slot in the PSSL shader. For example:
			 *
			 *  @code
			 *     AppendRegularBuffer<uint> appendBuf : register(u3) // Will access the 4th counter starting at the base offset provided to this function.
			 *  @endcode
			 *
			 *  This function never rolls the hardware context.
			 *
			 *  @param[in] gdsMemoryBaseInBytes		The byte offset to the start of the counters in GDS. This must be a multiple of 4.
			 *  @param[in] countersSizeInBytes		The size of the counter range in bytes. This must be a multiple of 4.
			 *
			 *  @note GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes(). 
			 *  @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
			 */
			void setAppendConsumeCounterRange(uint32_t gdsMemoryBaseInBytes, uint32_t countersSizeInBytes)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				m_lwcue.setAppendConsumeCounterRange(gdsMemoryBaseInBytes, countersSizeInBytes);
			}

			/** @brief Binds one or more constant buffer objects to the CS Shader stage.
			 *
			 *  @param startApiSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxConstantBufferCount-1]</c>.
			 *  @param apiSlotCount		The number of consecutive API slots to bind.
			 *  @param buffer			The constant buffer objects to bind to the specified slots. <c>buffer[0]</c> will be bound to <c><i>startApiSlot</i></c>,
			 *							<c>buffer[1]</c> to <c>startApiSlot+1</c> and so on. The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
			 */
			void setConstantBuffers(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Buffer* buffer)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				m_lwcue.setConstantBuffers(startApiSlot, apiSlotCount, buffer);
			}

			/** @brief Binds one or more read-only buffer objects to the CS Shader stage.
			 *
			 *  @param[in] startApiSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxResourceCount-1]</c>.
			 *  @param[in] apiSlotCount		The number of consecutive API slots to bind.
			 *  @param[in] buffer			The read-only buffer objects to bind to the specified slots. <c>buffer[0]</c> will be bound to <c><i>startApiSlot</i></c>,
			 *							<c>buffer[1]</c> to <c>startApiSlot+1</c> and so on. The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
			 *
			 *  @note Buffers and Textures share the same pool of API slots.
			 */
			void setBuffers(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Buffer* buffer)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				m_lwcue.setBuffers(startApiSlot, apiSlotCount, buffer);
			}
				
			/** @brief Binds one or more read/write buffer objects to the CS Shader stage.
			 *
			 *  @param[in] startApiSlot			The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxRwResourceCount-1]</c>.
			 *  @param[in] apiSlotCount			The number of consecutive API slots to bind.
			 *  @param[in] buffer				The read/write buffer objects to bind to the specified slots. <c>buffer[0]</c> will be bound to <c><i>startApiSlot</i></c>,
			 *								<c>buffer[1]</c> to <c>startApiSlot+1</c> and so on. The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
			 *
			 *  @note Buffers and Textures share the same pool of API slots.
			 */
			void setRwBuffers(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Buffer* buffer)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				m_lwcue.setRwBuffers(startApiSlot, apiSlotCount, buffer);
			}

			/** @brief Binds one or more read-only texture objects to the CS Shader stage.
			 *
			 *  @param[in] startApiSlot			The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxResourceCount-1]</c>.
			 *  @param[in] apiSlotCount			The number of consecutive API slots to bind.
			 *  @param[in] texture				The read-only texture objects to bind to the specified slots. <c>texture[0]</c> will be bound to <c><i>startApiSlot</i></c>,
			 *								<c>texture[1]</c> to <c>startApiSlot+1</c> and so on. The contents of these texture objects are cached locally inside the LCUE's scratch buffer.
			 *
			 *  @note Buffers and Textures share the same pool of API slots.
			 */
			void setTextures(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Texture* texture)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				m_lwcue.setTextures(startApiSlot, apiSlotCount, texture);
			}

			/** @brief Binds one or more read/write texture objects to the CS Shader stage.
			 *
			 *  @param[in] startApiSlot			The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxRwResourceCount-1]</c>.     
			 *  @param[in] apiSlotCount			The number of consecutive API slots to bind.
			 *  @param[in] texture				The read/write texture objects to bind to the specified slots. <c>texture[0]</c> will be bound to <c><i>startApiSlot</i></c>,
			 *								<c>texture[1]</c> to <c>startApiSlot+1</c> and so on. The contents of these texture objects are cached locally inside the LCUE's scratch buffer.
			 *
			 *  @note Buffers and Textures share the same pool of API slots.
			 */
			void setRwTextures(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Texture* texture)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				m_lwcue.setRwTextures(startApiSlot, apiSlotCount, texture);
			}

			/** @brief Binds one or more sampler objects to the CS Shader stage.
			 *
			 *  @param[in] startApiSlot			The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxSamplerCount-1]</c>.
			 *  @param[in] apiSlotCount			The number of consecutive API slots to bind.
			 *  @param[in] sampler				The sampler objects to bind to the specified slots. <c>sampler[0]</c> will be bound to <c><i>startApiSlot</i></c>,
			 *								<c>sampler[1]</c> to <c>startApiSlot+1</c> and so on. The contents of these Sampler objects are cached locally inside the LCUE's scratch buffer.
			 */	
			void setSamplers(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Sampler* sampler)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				m_lwcue.setSamplers(startApiSlot, apiSlotCount, sampler);
			}

			/** @brief Binds a user SRT buffer to the CS Shader stage.
			 *
			 *  @param[in] buffer				The pointer to the buffer. If this is set to <c>NULL</c>, <c><i>sizeInDwords</i></c> must be 0.
			 *  @param[in] sizeInDwords			The size of the data pointed to by <c><i>buffer</i></c> in <c>DWORD</c>s.
			 *								The valid range is <c>[1..kMaxSrtUserDataCount]</c> if <c><i>buffer</i></c> is non-<c>NULL</c>.
			 */
			void setUserSrtBuffer(const void* buffer, uint32_t sizeInDwords)
			{
				SCE_GNM_ASSERT_MSG_INLINE(m_UsingLightweightConstantUpdateEngine, "In order to use this method, you must call void init(void* dcbBuffer, uint32_t dcbBufferSizeInBytes, void* resourceBufferInGarlic, uint32_t resourceBufferSizeInBytes, void* globalInternalResourceTableAddr).");
				m_lwcue.setUserSrtBuffer(buffer, sizeInDwords);
			}
#endif
			
			/** @brief Inserts a compute shader dispatch with the indicated number of thread groups.
			    
				@param[in] threadGroupX			The number of thread groups dispatched along the X dimension. This must be in the range <c>[0..2,147,483,647]</c>.
			    @param[in] threadGroupY			The number of thread groups dispatched along the Y dimension. This must be in the range <c>[0..2,147,483,647]</c>.
			    @param[in] threadGroupZ			The number of thread groups dispatched along the Z dimension. This must be in the range <c>[0..2,147,483,647]</c>.
			    
				@cmdsize 7
			*/
			void dispatch(uint32_t threadGroupX, uint32_t threadGroupY, uint32_t threadGroupZ)
			{
				Gnm::DispatchOrderedAppendMode orderedAppendMode = Gnm::kDispatchOrderedAppendModeDisabled;
				#if defined(__ORBIS__)
				if (m_UsingLightweightConstantUpdateEngine)
				{
					m_lwcue.preDispatch();
					orderedAppendMode = m_lwcue.getCsShaderOrderedAppendMode();
				}
				#endif

				m_dcb.dispatchWithOrderedAppend(threadGroupX, threadGroupY, threadGroupZ, orderedAppendMode);
			}

			/** @brief Inserts an indirect compute shader dispatch, whose parameters are read from GPU memory.
			This function never rolls the hardware context.
				@param[in] args A pointer to the arguments structure for this dispatch, whose contents must be filled in before this packet is processed.
								This must be 4-byte aligned and must not be <c>NULL</c>.
			*/
			void dispatchIndirect(Gnm::DispatchIndirectArgs *args)
			{
				Gnm::DispatchOrderedAppendMode orderedAppendMode = Gnm::kDispatchOrderedAppendModeDisabled;
				#if defined(__ORBIS__)
				if (m_UsingLightweightConstantUpdateEngine)
				{
					m_lwcue.preDispatch();
					orderedAppendMode = m_lwcue.getCsShaderOrderedAppendMode();
				}
				#endif

				m_dcb.dispatchIndirectWithOrderedAppend(args, orderedAppendMode);
			}

			//////////// Dispatch commands

			/** @brief Uses the command processor's DMA engine to clear a buffer to specified value (such as a GPU memset).
				@param[out] dstGpuAddr   The destination address to which this function writes data. Must not be <c>NULL</c>. 
				@param[in] srcData       The value with which to fill the destination buffer.
				@param[in] numBytes      The size of the destination buffer.  This value must be a multiple of 4.
				@param[in] isBlocking    If <c>true</c>, the command processor will block while the transfer is active.
				@note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
				@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
				@note  This feature does not affect the GPU <c>L2$</c> cache.
				@see  DispatchCommandBuffer::dmaData()
			*/
			void fillData(void *dstGpuAddr, uint32_t srcData, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
			{
				return Gnmx::fillData(&m_dcb, dstGpuAddr, srcData, numBytes, isBlocking);
			}

			/** @brief Uses the command processor's DMA engine to transfer data from a source address to a destination address.
				@param[out] dstGpuAddr         The destination address to which this function writes data. Must not be <c>NULL</c>.
				@param[in] srcGpuAddr          The source address from which this function reads data. Must not be <c>NULL</c>.
				@param[in] numBytes            The number of bytes to transfer from <c>srcGpuAddr</c> to <c>dstGpuAddr</c>. Must be a multiple of 4.
				@param[in] isBlocking          If <c>true</c>, the CP waits for the DMA to be complete before performing any more processing.
				@note  The implementation of this feature uses the <c>dmaData()</c> function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
				@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
				@note  This feature does not affect the GPU <c>L2$</c> cache.
				@see   DispatchCommandBuffer::dmaData()
			*/
			void copyData(void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
			{
				return Gnmx::copyData(&m_dcb, dstGpuAddr, srcGpuAddr, numBytes, isBlocking);
			}

			/** @brief Inserts user data directly inside the command buffer returning a locator for later reference.
						  @param[in] dataStream    Pointer to the data.
						  @param[in] sizeInDword   Size of the data in stride of 4. Note that the maximum size of a single command packet is 2^16 bytes,
											   and the effective maximum value of <c><i>sizeInDword</i></c> will be slightly less than that due to packet headers
											   and padding.
						  @param[in] alignment     Alignment of the embedded copy in the command buffer.
						  @return Returns a pointer to the allocated buffer.
			*/
			void* embedData(const void *dataStream, uint32_t sizeInDword, Gnm::EmbeddedDataAlignment alignment)
			{
				return Gnmx::embedData(&m_dcb, dataStream, sizeInDword, alignment);
			}

			/**
			* @brief Wrapper around <c>dmaData()</c> to clear the values of one or more append or consume buffer counters to the specified value.
			 * @param[in] destRangeByteOffset Byte offset in GDS to the beginning of the counter range to update. Must be a multiple of 4.
			 * @param[in] startApiSlot Index of the first <c>RW_Buffer</c> API slot whose counter should be updated. Valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>.
			 * @param[in] numApiSlots Number of consecutive slots to update. <c><i>startApiSlot</i> + <i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
			 * @param[in] clearValue The value to set the specified counters to.
			 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
			 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
			 * @note  The implementation of this feature uses the <c>dmaData()</c> function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			 * @note  To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
			 */
			void clearAppendConsumeCounters(uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, uint32_t clearValue)
			{
				return Gnmx::clearAppendConsumeCounters(&m_dcb, destRangeByteOffset, startApiSlot, numApiSlots, clearValue);
			}

			/**
			 * @brief Wrapper around <c>dmaData()</c> to update the values of one or more append or consume buffer counters, using values sourced from the provided GPU-visible address.
			 * @param[in] destRangeByteOffset Byte offset in GDS to the beginning of the counter range to update. Must be a multiple of 4.
			 * @param[in] startApiSlot Index of the first <c>RW_Buffer</c> API slot whose counter should be updated. Valid range is <c>[0..Gnm::kSlotCountRwResource-1]</c>.
			 * @param[in] numApiSlots Number of consecutive slots to update. <c><i>startApiSlot</i> + <i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
			 * @param[in] srcGpuAddr GPU-visible address to read the new counter values from.
			 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
			 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
			 * @note  The implementation of this feature uses the <c>dmaData()</c> function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			 * @note  To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
			 */
			void writeAppendConsumeCounters(uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, const void *srcGpuAddr)
			{
				return Gnmx::writeAppendConsumeCounters(&m_dcb, destRangeByteOffset, startApiSlot, numApiSlots, srcGpuAddr);
			}

			/**
			 * @brief Wrapper around <c>dmaData()</c> to retrieve the values of one or more append or consume buffer counters and store them in a GPU-visible address.
			 * @param[out] destGpuAddr GPU-visible address to write the counter values to.
			 * @param[in] srcRangeByteOffset Byte offset in GDS to the beginning of the counter range to read. Must be a multiple of 4.
			 * @param[in] startApiSlot Index of the first <c>RW_Buffer</c> API slot whose counter should be read. Valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>.
			 * @param[in] numApiSlots Number of consecutive slots to read. <c><i>startApiSlot</i> + <i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
			 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
			 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
			 * @note  The implementation of this feature uses the <c>dmaData()</c> function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			 * @note  To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
			 */
			void readAppendConsumeCounters(void *destGpuAddr, uint32_t srcRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots)
			{
				return Gnmx::readAppendConsumeCounters(&m_dcb, destGpuAddr, srcRangeByteOffset, startApiSlot, numApiSlots);
			}

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
			#include "computecontext_methods.h"

		public:
			/** @brief Defines a callback function that the system calls when a ComputeContext command buffer is out of space. 

				@param[in,out] gfxc			A pointer to the ComputeContext object whose command buffer is out of space.
				@param[in,out] cb			A pointer to the CommandBuffer object that is out of space. This will be <c>cmpc.m_dcb</c>.
				@param[in] sizeInDwords		The size of the unfulfilled CommandBuffer request in <c>DWORD</c>s.
				@param[in] userData			The user data.
				
				@return  <c>true</c> if the requested space is available in cb when the function returns; otherwise, returns <c>false</c>.
			 */
			typedef bool (*BufferFullCallbackFunc)(ComputeContext *gfxc, Gnm::CommandBuffer *cb, uint32_t sizeInDwords, void *userData);

			/** @brief Represents a "command buffer is out of space" callback function and the data to pass to it. */
			class BufferFullCallback
			{
			public:
				BufferFullCallbackFunc m_func;	///< A pointer to the function to call when the command buffer is out of space.
				void *m_userData;				///< The user data to pass to the <c><i>m_func</i></c> function.
			};

			BufferFullCallback m_cbFullCallback; ///< A "command buffer is out of space" callback function and its user data. The system calls this function when <c>m_dcb</c>
												 ///< actually runs out of space (as opposed to crossing a 4 MB boundary).
			GnmxDispatchCommandBuffer m_dcb;		///< Dispatch command buffer. Access directly at your own risk!

			ComputeConstantUpdateEngine m_lwcue;	///< Compute based Lightweight Constant Update Engine. Access directly at your own risk!
			InputResourceOffsets m_boundInputResourceOffsets; ///< Offsets of the currently bound input resources.
			

#if !defined(DOXYGEN_IGNORE)
			// The following code/data is used to work around the hardware's 4 MB limit on individual command buffers. We use the m_callback
			// fields of m_dcb to detect when either buffer crosses a 4 MB boundary, and save off the start/size of both buffers
			// into the m_submissionRanges array. When submitted, the m_submissionRanges array is used to submit each <4MB chunk individually.
			//
			// In order for this code to function properly, users of this class must not modify m_dcb.m_callback!
			// To register a callback that triggers when m_dcb run out of space, use m_bufferFullCallback.
			static const uint32_t kMaxNumStoredSubmissions = 16; // Maximum number of <4MB submissions that can be recorded. Make this larger if you want more; it just means ComputeContext objects get larger.
			const uint32_t *m_currentDcbSubmissionStart; // Beginning of the submission currently being constructed in the DCB
			const uint32_t *m_actualDcbEnd; // Actual end of the m_dcb's data buffer (i.e. dcbBuffer+dcbSizeInBytes/4)

			uint32_t *m_predicationRegionStart; // Beginning of the current predication region. Only valid between calls to setPredication() and endPredication().
			void *m_predicationConditionAddr; // Address of the predication condition that controls whether the current predication region will be skipped.

			class SubmissionRange
			{
			public:
				uint32_t m_dcbStartDwordOffset, m_dcbSizeInDwords;
			};
			SubmissionRange m_submissionRanges[kMaxNumStoredSubmissions]; // Stores the range of each previously-constructed submission (not including the one currently under construction)
			uint32_t m_submissionCount; // The current number of stored submissions in m_submissionRanges (again, not including the one currently under construction)

			bool m_UsingLightweightConstantUpdateEngine;
			uint8_t m_pad[3];

#endif // !defined(DOXYGEN_IGNORE)

		};
	}
}

#endif
