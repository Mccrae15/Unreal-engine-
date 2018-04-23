/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if !defined(_SCE_GNMX_LWCUE_COMPUTE_H)
#define _SCE_GNMX_LWCUE_COMPUTE_H

#include <gnmx/lwcue_base.h>

namespace sce
{
	namespace Gnmx
	{
			/** @brief Lightweight Constant Update Engine for async-compute context */
			class SCE_GNMX_EXPORT ComputeConstantUpdateEngine : public BaseConstantUpdateEngine
			{
			public:
#if defined(__ORBIS__) // LCUE isn't supported offline

				/** @brief Initializes the resource areas for the Computes constant updates.
				 *
				 *  @param[in] resourceBuffersInGarlic			An array of resource buffers to be used by the ComputeCUE.
				 *  @param[in] resourceBufferCount				The number of resource buffers created.
				 *  @param[in] resourceBufferSizeInDwords		The size of each resource buffer in <c>DWORD</c>s.
				 *  @param[in] globalInternalResourceTableAddr	A pointer to the global resource table in memory.
				 */
				void init(uint32_t** resourceBuffersInGarlic, int32_t resourceBufferCount, uint32_t resourceBufferSizeInDwords, void* globalInternalResourceTableAddr);

				
				/** @brief Swaps LCUE's ComputeCUE resource buffers for the next frame and invalidates all bindings.
				 *
				 *  @note If <c>SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE</c> is enabled, any calls to swap buffers will additionally insert a command for <c>flushShaderCachesAndWait()</c> 
				 *        to invalidate both K$ and L2 caches.
				 */
				void swapBuffers();


				/** @brief Clears all existing bindings. */
				void invalidateAllBindings();

				
				/** @brief Sets the pointer to the dispatch command buffer.
				 *
				 *  @param[in] dcb	A pointer to the dispatch command buffer.
				 */
				SCE_GNM_FORCE_INLINE void setDispatchCommandBuffer(GnmxDispatchCommandBuffer* dcb) { m_dispatchCommandBuffer = dcb; };

				
				/** @brief Binds a CS shader to the CS stage.
				 *  @param[in] shader A pointer to the CS shader.
				 *  @param[in] table A matching ShaderResourceOffsets table created by generateShaderResourceOffsetTable().
				 *
				 *  @note Only the pointer is cached inside the LCUE; the location and contents of <c>*shader</c> must not change until the GPU has completed the dispatch!
				 *	@note This binding will not take effect on the GPU until preDispatch() is called.
				 *  @note This function must be called first before any resource bindings calls. If setCsShader() is called again, all resource bindings for the stage will need to be rebound.
				 */
				void setCsShader(const sce::Gnmx::CsShader* shader, const InputResourceOffsets* table);

	
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
				 *  @note GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes. 
				 */
				void setAppendConsumeCounterRange(uint32_t gdsMemoryBaseInBytes, uint32_t countersSizeInBytes);

				
				/** @brief Specifies a range of the Global Data Store to be used by the CS shader.
				 *
				 *  This function never rolls the hardware context.
				 *
				 *  @param[in] baseOffsetInBytes		The byte offset to the start of the range in the GDS. This must be a multiple of 4. 
				 *  @param[in] rangeSizeInBytes			The size of the counter range in bytes. This must be a multiple of 4.
				 *
				 *  @note  The GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes. It is an error to specify a range outside these bounds.
				 */
				void setGdsMemoryRange(uint32_t baseOffsetInBytes, uint32_t rangeSizeInBytes);


				/** @brief Binds one or more constant buffer objects to the CS Shader stage.
				 *
				 *  @param[in] startApiSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxConstantBufferCount-1]</c>.
				 *  @param[in] apiSlotCount		The number of consecutive API slots to bind.
				 *  @param[in] buffer			The constant buffer objects to bind to the specified slots.
				 *							
				 *	<c>buffer[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>buffer[1]</c> to <c>startApiSlot+1</c> and so on.
				 *	The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
				 *
				 *  @note This binding will not take effect on the GPU until preDispatch() is called.
				 */
				void setConstantBuffers(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Buffer* buffer);

				
				/** @brief Binds one or more read-only buffer objects to the CS Shader stage. 
				 *
				 *  @param[in] startApiSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxResourceCount-1]</c>.
				 *  @param[in] apiSlotCount		The number of consecutive API slots to bind.
				 *  @param[in] buffer			The buffer objects to bind to the specified slots.
				 *
				 *	<c>buffer[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>buffer[1]</c> to <c>startApiSlot+1</c>, and so on. 
				 *	The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
				 *
				 *  @note Buffers and Textures share the same pool of API slots.
				 *  @note This binding will not take effect on the GPU until preDispatch() is called.
				 */
				void setBuffers(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Buffer* buffer);

				
				/** @brief Binds one or more read/write buffer objects to the CS Shader stage.
				 *
				 *  @param[in] startApiSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxRwResourceCount-1]</c>.
				 *  @param[in] apiSlotCount		The number of consecutive API slots to bind.
				 *  @param[in] buffer			The read/write buffer objects to bind to the specified slots. 
				 *
				 *	<c>buffer[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>buffer[1]</c> to <c>startApiSlot+1</c> and so on. 
				 *	The contents of these buffer objects are cached locally inside the LCUE's scratch buffer.
				 *
				 *  @note Read/write buffers and read/write textures share the same pool of API slots.
				 *  @note This binding will not take effect on the GPU until preDispatch() is called.
				 */
				void setRwBuffers(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Buffer* buffer);

				
				/** @brief Binds one or more read-only texture objects to the CS Shader stage. 
				 *
				 *  @param[in] startApiSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxResourceCount-1]</c>.
				 *  @param[in] apiSlotCount		The number of consecutive API slots to bind.
				 *  @param[in] texture			The texture objects to bind to the specified slots. 
				 *
				 *  <c>texture[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>texture[1]</c> to <c>startApiSlot+1</c> and so on. 
				 *  The contents of these texture objects are cached locally inside the LCUE's scratch buffer.
				 *
				 *  @note Buffers and Textures share the same pool of API slots.
				 *  @note This binding will not take effect on the GPU until preDispatch() is called.
				 */
				void setTextures(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Texture* texture);


				/** @brief Binds one or more read/write texture objects to the CS Shader stage. 
				 *
				 *  @param[in] startApiSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxRwResourceCount-1]</c>.     
				 *  @param[in] apiSlotCount		The number of consecutive API slots to bind.
				 *  @param[in] texture			The read/write texture objects to bind to the specified slots. 
				 *
				 *  <c>texture[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>texture[1]</c> to <c>startApiSlot+1</c> and so on. 
				 *  The contents of these texture objects are cached locally inside the LCUE's scratch buffer.
				 *
				 *  @note Read/write buffers and read/write textures share the same pool of API slots.
				 *  @note This binding will not take effect on the GPU until preDispatch() is called.
				 */
				void setRwTextures(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Texture* texture);

				
				/** @brief Binds one or more sampler objects to the CS Shader stage.
				 *
				 *  @param[in] startApiSlot		The first API slot to bind to. Valid slots are <c>[0..LCUE::kMaxSamplerCount-1]</c>.
				 *  @param[in] apiSlotCount		The number of consecutive API slots to bind.
				 *  @param[in] sampler			The sampler objects to bind to the specified slots. 
				 *
				 *  <c>sampler[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>sampler[1]</c> to <c>startApiSlot+1</c>, and so on. 
				 *
				 *  The contents of these Sampler objects are cached locally inside the LCUE's scratch buffer.
				 *  @note This binding will not take effect on the GPU until preDispatch() is called.
				 */
				void setSamplers(int32_t startApiSlot, int32_t apiSlotCount, const sce::Gnm::Sampler* sampler);


				/** @brief Binds a user SRT buffer to the CS Shader stage.
				 *
				 *  @param[in] buffer			The pointer to the buffer. If this is set to <c>NULL</c>, then <c><i>bufSizeInDwords</i></c> must be 0.
				 *  @param[in] sizeInDwords		The size of the data pointed to by <c><i>buffer</i></c> in <c>DWORD</c>s. The valid range is <c>[1..kMaxSrtUserDataCount]</c> if <c><i>buffer</i></c> is not set to <c>NULL</c>.
				 *
				 *  @note This binding will not take effect on the GPU until preDispatch() is called.
				 */
				void setUserSrtBuffer(const void* buffer, uint32_t sizeInDwords);

				
				////////////// Dispatch commands
				
				/** @brief Inserts a compute shader dispatch with the indicated number of thread groups.
				 *
				 * 	This function never rolls the hardware context.
				 *
				 *	@param[in] threadGroupX		The number of thread groups dispatched along the X dimension. This must be in the range <c>[0..2,147,483,647]</c>.
				 *	@param[in] threadGroupY		The number of thread groups dispatched along the Y dimension. This must be in the range <c>[0..2,147,483,647]</c>.
				 *	@param[in] threadGroupZ		The number of thread groups dispatched along the Z dimension. This must be in the range <c>[0..2,147,483,647]</c>.
				 */
				SCE_GNM_FORCE_INLINE void dispatch(uint32_t threadGroupX, uint32_t threadGroupY, uint32_t threadGroupZ)
				{
					#if defined(__ORBIS__)
					preDispatch();
					#endif
					m_dispatchCommandBuffer->dispatch(threadGroupX, threadGroupY, threadGroupZ);
				}

				/** @brief Inserts an indirect compute shader dispatch whose parameters are read from GPU memory.
				This function never rolls the hardware context.
				@param[in] args A pointer to the arguments structure for this dispatch whose contents must be filled in before this packet is processed.
								This must be 4-byte aligned and must not be <c>NULL</c>.
				*/
				SCE_GNM_FORCE_INLINE void dispatchIndirect(Gnm::DispatchIndirectArgs *args)
				{
					#if defined(__ORBIS__)
					preDispatch();
					#endif
					m_dispatchCommandBuffer->dispatchIndirect(args);
				}

				/** @brief Executes all previous enqueued resource and shader bindings in preparation for a dispatch call.
				 *
				 *  Dirty resource bindings will be flushed from the internal scratch buffer and committed to the resource buffer.
				 *
				 *  @note When using the Lightweight Constant Update Engine to manage shaders and shader resources, this function must be called 
				 *  immediately before every dispatch call.
				 */
				void preDispatch();

				/**
				* @brief Returns the DispatchOrderedAppendMode setting that the the currently set graphics pipe CS shader requires.
				*
				* @return The DispatchOrderedAppendMode setting that the currently set graphics pipe CS shader requires. If non-zero,you must call DrawCommandBuffer::dispatchWithOrderedAppend().
				*/
				Gnm::DispatchOrderedAppendMode getCsShaderOrderedAppendMode() const { return (Gnm::DispatchOrderedAppendMode)m_csShaderOrderedAppendMode; }
			
	#if !defined(DOXYGEN_IGNORE)

				SCE_GNM_LCUE_NOT_SUPPORTED
				void setInternalSrtBuffer(const void* buffer){SCE_GNM_UNUSED(buffer);};
				
			public:

				GnmxDispatchCommandBuffer*	m_dispatchCommandBuffer;
				uint32_t					m_scratchBuffer[LightweightConstantUpdateEngine::kComputeScratchBufferSizeInDwords];
				const void*					m_boundShader;
				uint32_t					m_boundShaderAppendConsumeCounterRange;
				uint32_t					m_boundShaderGdsMemoryRange;
				const InputResourceOffsets*	m_boundShaderResourceOffsets;
				
				bool m_dirtyShader;
				bool m_dirtyShaderResources;

				mutable LightweightConstantUpdateEngine::ShaderResourceBindingValidation m_boundShaderResourcesValidation;

				uint8_t m_csShaderOrderedAppendMode;
				uint8_t m_pad[7];

			public:

				SCE_GNM_FORCE_INLINE uint32_t* flushScratchBuffer();
				SCE_GNM_FORCE_INLINE void updateCommonPtrsInUserDataSgprs(const uint32_t* resourceBufferFlushedAddress);
				SCE_GNM_FORCE_INLINE void updateEmbeddedCb(const sce::Gnmx::ShaderCommonData* shaderCommon);

				void setUserData(uint32_t startSgpr, uint32_t sgprCount, const uint32_t* data);
				void setPtrInUserData(uint32_t startSgpr, const void* gpuAddress);
	#endif // !defined(DOXYGEN_IGNORE)

#endif

			};
	} // Gnmx
} // sce

#endif // _SCE_GNMX_LWCUE_COMPUTE_H
