/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/
// This file was auto-generated from dispatchcommandbuffer.h -- do not edit by hand!
// This file should NOT be included directly by any non-Gnmx source files!

#if !defined(_SCE_GNMX_COMPUTECONTEXT_METHODS_H)
#define _SCE_GNMX_COMPUTECONTEXT_METHODS_H

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable:4996) // deprecated functions
#endif

/** @brief Sets up a default hardware state for the compute ring.
*
*	This function flushes the K$ and I$.
*
*	@cmdsize 256
*/
void initializeDefaultHardwareState()
{
	return m_dcb.initializeDefaultHardwareState();
}

/** @brief For the compute queue on which this command buffer is launched, sets a dispatch mask that determines which compute units are active in the specified shader engine.
* 
*  All masks are logical masks indexed from 0 to Gnm::kNumCusPerSe <c>- 1</c>, regardless of which physical compute units are working and enabled by the driver.
*
*  @param[in] engine The shader engine to configure.
*  @param[in] mask A mask enabling compute units for the CS shader stage.
*
*  @see Gnm::DrawCommandBuffer::setGraphicsResourceManagement(), Gnm::DrawCommandBuffer::setComputeResourceManagement, Gnmx::GfxContext::setGraphicsResourceManagement()
*
*  @cmdsize 6
*/
SCE_GNM_API_CHANGED
void setComputeResourceManagement(Gnm::ShaderEngine engine, uint16_t mask)
{
	return m_dcb.setComputeResourceManagement(engine, mask);
}

/** @brief For dispatches on the graphics ring, sets a dispatch mask that determines the compute units that are active for compute shaders in the specified shader engine.
			All masks are logical masks indexed from 0 to Gnm::kNumCusPerSe <c>- 1</c>, regardless of which physical compute units are working and enabled by the driver.
			This function never rolls the hardware context.
			
			@param[in] engine			The shader engine to configure.
			@param[in] mask				A mask enabling compute units for the CS shader stage.
			
			@see setGraphicsShaderControl(), Gnmx::GfxContext::setGraphicsShaderControl()
			
			@cmdsize 3
*/
void setComputeResourceManagementForBase(Gnm::ShaderEngine engine, uint16_t mask)
{
	return m_dcb.setComputeResourceManagementForBase(engine, mask);
}

/** @brief For dispatches on the graphics ring, sets a dispatch mask that determines the compute units that are active for compute shaders in the specified shader engine.
			All masks are logical masks indexed from 0 to Gnm::kNumCusPerSe <c>- 1</c>, regardless of which physical compute units are working and enabled by the driver.
			This function never rolls the hardware context.
			
			@param[in] engine			The shader engine to configure.
			@param[in] mask				A mask enabling compute units for the CS shader stage.
			
			@see setGraphicsShaderControl(), Gnmx::GfxContext::setGraphicsShaderControl()
			
			@cmdsize 3
*/
void setComputeResourceManagementForNeo(Gnm::ShaderEngine engine, uint16_t mask)
{
	return m_dcb.setComputeResourceManagementForNeo(engine, mask);
}

/** @brief Sets the shader code to be used for the CS shader stage.
*
*  @param[in] computeData   Pointer to structure containing memory address (256-byte aligned) of the shader code and additional
*                           registers to set as determined by the shader compiler. Must not be <c>NULL</c>.
*	@param[in] shaderModifier Shader Modifier value that generateCsFetchShaderBuildState() returns; if no fetch shader, use <c>0</c>.
*  @cmdsize 25
*/
void setCsShader(const Gnm::CsStageRegisters * computeData, uint32_t shaderModifier)
{
	return m_dcb.setCsShader(computeData, shaderModifier);
}

/** @brief Sets the shader code to use for the CS shader stage.
*
*  @param[in] computeData   Pointer to structure containing memory address (256-byte aligned) of the shader code and additional
*                           registers to set as determined by the shader compiler. Must not be <c>NULL</c>.
*  @cmdsize 25
*/
SCE_GNM_FORCE_INLINE void setCsShader(const Gnm::CsStageRegisters * computeData)
{
	return m_dcb.setCsShader(computeData);
}

/** @deprecated The bulky bit for CS shader does not work properly and may cause the system to hang; use setCsShader() instead.
				@brief Sets the shader code to be used for the CS shader stage as a bulky shader.
				Threadgroups with bulky shaders are scheduled to separate CUs.

				Specify a shader to be bulky to prevent a situation in which only few threadgroups can fit into single CU because each uses a significant number of registers and/or LDS.
			  @param[in] computeData   Pointer to structure containing the memory address (256-byte aligned) of the shader code and additional
			                                   registers to set as determined by the shader compiler.
			  @cmdsize 25
*/
SCE_GNM_API_DEPRECATED_NOINLINE("The bulky bit for CS shader does not work properly and may cause the system to hung; please use setCsShader() instead.")
void setBulkyCsShader(const Gnm::CsStageRegisters * computeData)
{
	return m_dcb.setBulkyCsShader(computeData);
}

/** @brief Reads data from global data store (GDS). This command implicitly waits for the CS stage to complete before triggering the read.
*
*  @param[out] dstGpuAddr The destination address to which this method writes the GDS data. This pointer must be 4-byte aligned and must not be set to <c>NULL</c>.
*  @param[in] gdsOffsetInDwords The <c>DWORD</c> offset into GDS to read from.
*  @param[in] gdsSizeInDwords The number of <c>DWORD</c>s to read.
*
*  @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes. Reads beyond this range will simply return 0.
*			When using this function, the data transfer is not performed by the CP or CP DMA, but rather it is done by the GPU back end
*			since this function is actually implemented as a “release memory” event. It means the CP will not wait for the transfer
*			to start or end before moving on with the processing of further commands, but this function conveniently allows the waiting of
*			previously executing compute or pixel processing before initiating the transfer of data from GDS.
*	
*  @cmdsize 7
*/
void readDataFromGds(void * dstGpuAddr, uint32_t gdsOffsetInDwords, uint32_t gdsSizeInDwords)
{
	return m_dcb.readDataFromGds(dstGpuAddr, gdsOffsetInDwords, gdsSizeInDwords);
}

/** @brief Allocates space for user data directly inside the command buffer, and returns a CPU pointer to the space.
*
*  @param[in] sizeInBytes	Size of the data in bytes. Note that the maximum size of a single command packet is 2^16 bytes,
*							and the effective maximum value of <c><i>sizeInBytes</i></c> will be slightly less than that due to packet headers
*							and padding.
*  @param[in] alignment    Alignment from the start of the command buffer.
* 
*  @return Returns a pointer to the memory just allocated. If the requested size is larger than the maximum packet size (64KB),
*			or if there is insufficient memory remaining in the command buffer, this function returns 0.
*
*	@cmdsize 2 + (sizeInBytes + sizeof(uint32_t) - 1)/sizeof(uint32_t) + (uint32_t)(1<<alignment)/sizeof(uint32_t)
*/
void * allocateFromCommandBuffer(uint32_t sizeInBytes, Gnm::EmbeddedDataAlignment alignment)
{
	return m_dcb.allocateFromCommandBuffer(sizeInBytes, alignment);
}

/** @brief Sets a buffer resource (<c>V#</c>) in the appropriate shader user data registers.
*
*	@param[in] startUserDataSlot The first user data slot to write to. There are 16 <c>DWORD</c>-sized user data slots available per shader stage.
*	 A <c>V#</c> occupies four consecutive slots, so the valid range for <c><i>startUserDataSlot</i></c> is <c>[0..12]</c>.
*	@param[in] buffer Pointer to a Buffer resource definition.
*
*	@cmdsize 8
*/
void setVsharpInUserData(uint32_t startUserDataSlot, const Gnm::Buffer * buffer)
{
	return m_dcb.setVsharpInUserData(startUserDataSlot, buffer);
}

/** @brief Sets a texture resource (<c>T#</c>) in the appropriate shader user data registers.
*
*	@param[in] startUserDataSlot Starting user data slot. There are 16 <c>DWORD</c>-sized user data slots available per shader stage.
*	 A <c>T#</c> occupies eight consecutive slots, so the valid range for <c><i>startUserDataSlot</i></c> is <c>[0..8]</c>.
*	@param[in] tex Pointer to a Texture resource definition.
*
*	@cmdsize 12
*/
void setTsharpInUserData(uint32_t startUserDataSlot, const Gnm::Texture * tex)
{
	return m_dcb.setTsharpInUserData(startUserDataSlot, tex);
}

/** @brief Sets a sampler resource (<c>S#</c>) in the appropriate shader user data registers.
* 
*	@param[in] startUserDataSlot Starting user data slot. There are 16 <c>DWORD</c>-sized user data slots available per shader stage.
*	 An <c>S#</c> occupies four consecutive slots, so the valid range for <c><i>startUserDataSlot</i></c> is <c>[0..12]</c>.
*	@param[in] sampler Pointer to a Sampler resource definition.
*
*	@cmdsize 8
*/
void setSsharpInUserData(uint32_t startUserDataSlot, const Gnm::Sampler * sampler)
{
	return m_dcb.setSsharpInUserData(startUserDataSlot, sampler);
}

/** @brief Sets a GPU pointer in the appropriate shader user data registers.
*
*	@param[in] startUserDataSlot Starting user data slot. There are 16 <c>DWORD</c>-sized user data slots available per shader stage.
*	 A GPU address occupies two consecutive slots, so the valid range for <c><i>startUserDataSlot</i></c> is <c>[0..14]</c>.
*	@param[in] gpuAddr GPU address to write to the specified slot.
*
*	@cmdsize 4
*/
void setPointerInUserData(uint32_t startUserDataSlot, void * gpuAddr)
{
	return m_dcb.setPointerInUserData(startUserDataSlot, gpuAddr);
}

/** @brief Copies an arbitrary 32-bit integer constant into the user data register of a CS shader stage.
*
* @param[in] userDataSlot Destination user data slot. There are 16 <c>DWORD</c>-sized user data slots available per shader stage.
* 				This function sets a single slot, so the valid range for <c><i>userDataSlot</i></c> is <c>[0..15]</c>.
* @param[in] data		The source data to copy into the user data register.
*
* @cmdsize 3
*/
void setUserData(uint32_t userDataSlot, uint32_t data)
{
	return m_dcb.setUserData(userDataSlot, data);
}

/** @brief Copies several contiguous <c>DWORD</c>s into the user data registers of the CS shader stage.
*
*	@param[in] startUserDataSlot	The first slot to write. The valid range is <c>[0..15]</c>.
*	@param[in] userData				The source data to copy into the user data registers. If <c><i>numDwords</i></c> is greater than 0, this pointer must not be <c>NULL</c>.
*	@param[in] numDwords			The number of <c>DWORD</c>s to copy into the user data registers. The sum of <c><i>startUserDataSlot</i></c> and <c><i>numDwords</i></c> must not exceed 16.
*
*	@cmdsize 4+numDwords
*/
void setUserDataRegion(uint32_t startUserDataSlot, const uint32_t * userData, uint32_t numDwords)
{
	return m_dcb.setUserDataRegion(startUserDataSlot, userData, numDwords);
}

/** @brief Specifies how the scratch buffer (compute only) should be subdivided between the executing wavefronts.
*  	
*  Each Shader Engine supports dynamic scratch buffer offset allocation for up to 320 compute wavefronts, so setting 
*  maxNumWaves to less than <c>2*320=640</c> will reduce the limit for each Shader Engine. Setting <c><i>maxNumWaves</i></c> to a value higher 
*  than 640 sets the limit to 640. Note, however, that the scratch buffer base offsets used by each Shader Engine are always 
*  0 for SE0 and <c>(maxNumWaves/2 * num1KByteChunksPerWave * 1024)</c> for SE1. Therefore, setting <c><i>maxNumWaves</i></c> higher than 640 
*  will result in unused addresses in the middle and at the end of the scratch buffer. For this reason, 640 is the 
*  recommended upper limit for <c><i>maxNumWaves</i></c>.
*  
*  @param[in] maxNumWaves The maximum number of wavefronts that could be using the scratch buffer simultaneously.
*                         This value can range from <c>[0..1024]</c>, but the recommended upper limit is 640.
*  @param[in] num1KByteChunksPerWave The amount of scratch buffer space for use by each wavefront. This is specified in units of 1024-byte chunks. Must be < 8192.
*  @cmdsize 3
*
*  @note It is not recommended to share scratch buffers across compute queues or coexisting dispatches
*/
void setScratchSize(uint32_t maxNumWaves, uint32_t num1KByteChunksPerWave)
{
	return m_dcb.setScratchSize(maxNumWaves, num1KByteChunksPerWave);
}

/** @brief Configures a GDS ordered append unit internal counter to enable special ring buffer counter handling of <c>ds_ordered_count</c> shader operations targeting a specific GDS address.
*
*	The GDS ordered append unit supports Gnm::kGdsAccessibleOrderedAppendCounters (currently 8) special allocation counters which, like the GDS, are a
*	global resource that must be managed by the application.
*
*	This method configures the GDS OA counter <c><i>oaCounterIndex</i></c> (<c>[0:7]</c>) to detect <c>ds_ordered_count</c> operations targeting an 
*	allocation position counter at GDS address <c><i>gdsDwOffsetOfCounter</i></c>. These specific operations are then modified in order 
*	to prevent ring buffer data overwrites in addition to the wavefront ordering already enforced by <c>ds_ordered_count</c> in 
*	general. The ring buffer total size in arbitrary allocation units, specified by <c><i>spaceInAllocationUnits</i></c>, is initially 
*	stored in two internal GDS registers associated with the GDS OA counter: <c>WRAP_SIZE</c>, which stays constant, and <c>SPACE_AVAILABLE</c> 
*	which subsequently varies as buffer space is allocated and deallocated.
*
*	For a given dispatch, the global ordering of <c>ds_ordered_count</c> operations is partially determined by an ordered wavefront 
*	index, which can be set to increment as each new wavefront launches or as each new threadgroup launches, and an ordered 
*	operation index, which increments for a given wavefront or threadgroup each time a <c>ds_ordered_count</c> is issued with the 
*	<c>wave_release</c> flag set. Using these two indices, <c>ds_ordered_count</c> operations are always processed in strict wavefront index 
*	order for a given operation index and in strict operation index order for a given wavefront index. Note, however, that there is no order
*	constraint between two wavefronts when one has the earlier wavefront index and the other has the earlier operation index; many wavefront
*	indices may be executed for one operation index before any are executed for the following operation index. Alternatively, a single wavefront
*	index may execute all of its operation indices before the following wavefront index executes any operations.
*
*	Dispatches create wavefronts ordered by a thread group ID and iterate over X in the inner loop, then Y, and then Z. To enable 
*	generation of an ordered wavefront index passed to each wavefront, compute shaders must be dispatched with a method that 
*	provides a <c>DispatchOrderedAppendMode</c> argument (<c>dispatchWithOrderedAppend()</c> or <c>dispatchDraw()</c> for example). The value of this argument 
*	determines whether the index is incremented once per wavefront (<c>kDispatchOrderedAppendModeIndexPerWavefront</c>) or once per 
*	threadgroup (<c>kDispatchOrderedAppendModeIndexPerThreadgroup</c>).
*
*	Any <c>ds_ordered_count</c> operation whose GDS <c>DWORD</c> address (<c>M0[16:31]>>2 + offset0>>2</c>) matches <c><i>gdsDwOffsetOfCounter</i></c> will be 
*	intercepted for special processing by the GDS ordered append unit. If the current operation index (that is, the number of
*	previous wave releases issued by <c>ds_ordered_count</c> operations for this wavefront or threadgroup) is equal to <c><i>oaOpIndex</i></c> 
*	and the wavefront originated from the same asynchronous compute <c>pipe_id</c> that enabled the GDS OA counter, the operation 
*	is converted into an allocation operation. If the operation matches the address, but <c><i>oaOpIndex</i></c> or the compute <c>pipe_id</c> (or 
*	both) do not match, it is converted to a deallocation operation. Operations matching the address but originating from 
*	graphics pipe shader wavefronts are also converted to deallocation operations.
*
*	Any <c>ds_ordered_count</c> operation converted to an allocation operation will first wait until the internal <c>SPACE_AVAILABLE</c> 
*	register value is larger than the requested allocation size before decrementing the internal register value. In addition, 
*	some specific <c>ds_ordered_count</c> operations, such as <c>WRAP</c> and <c>CONDXCHG32</c>, receive some additional pre-processing of the 
*	allocation count to produce source operands to the internal atomic operations. See the GPU Shader Core ISA documentation 
*	for more details about <c>ds_ordered_count</c>.
*
*	Any <c>ds_ordered_count</c> operation converted to a deallocation operation will increment the internal <c>SPACE_AVAILABLE</c> register 
*	value but does not update the target counter in GDS.
*
*	Used together, these allocation and deallocation operations allow a shader running in one compute pipe or graphics stage to 
*	write data into a ring buffer while another shader running in a different compute pipe or graphics stage consumes this data. 
*	Allocations and writes are performed in wavefront (or threadgroup) launch order for the source pipe or stage, and reads and 
*	deallocations are performed in wavefront launch order for the destination pipe or stage. Typically, the source pipe also writes 
*	to another counter in GDS in wavefront (or threadgroup) launch order to indicate the completion of writes, which requires 
*	another <c>ds_ordered_count</c> but not another OA counter.
*
*	Dispatch draw requires one counter to be configured for the index ring buffer and, if enabled, a second counter to be configured 
*	for the vertex ring buffer.
*
*	@param[in] oaCounterIndex			The index of one of the 8 available GDS ordered append unit internal counters that should be configured; range <c>[0:7]</c>.
*	@param[in] gdsDwOffsetOfCounter		The <c>DWORD</c> offset of the allocation offset counter in GDS which will be matched against the address used by <c>ds_ordered_count</c>, i.e. <c>M0[16:31]>>2 + offset0>>2</c>.
*	@param[in] oaOpIndex				The <c>ds_ordered_count</c> operation index which will be interpreted as an allocation operation if also issued from this compute <c>pipe_id</c>.
*	@param[in] spaceInAllocationUnits	The size of the ring buffer in arbitrary units (elements).
*
*	@cmdsize 5
*  @see disableOrderedAppendAllocationCounter()
*/
void enableOrderedAppendAllocationCounter(uint32_t oaCounterIndex, uint32_t gdsDwOffsetOfCounter, uint32_t oaOpIndex, uint32_t spaceInAllocationUnits)
{
	return m_dcb.enableOrderedAppendAllocationCounter(oaCounterIndex, gdsDwOffsetOfCounter, oaOpIndex, spaceInAllocationUnits);
}

/** @brief Disables a GDS ordered append unit internal counter.
*
*	@param[in] oaCounterIndex	The index of the GDS ordered append unit internal counter to disable; range [0:7].
*
*	@cmdsize 5
*
*  @see enableOrderedAppendAllocationCounter()
*/
void disableOrderedAppendAllocationCounter(uint32_t oaCounterIndex)
{
	return m_dcb.disableOrderedAppendAllocationCounter(oaCounterIndex);
}

/** @brief Copies data inline into the command buffer and uses the command processor's DMA engine to transfer it to a destination GPU address.
*
*	@param[out] dstGpuAddr     Destination address to which this function writes. Must be 4-byte aligned.
*	@param[in] data            Pointer to data to be copied inline. This pointer must not be <c>NULL</c>.
*	@param[in] sizeInDwords    Number of <c>DWORD</c>s of data to copy. The valid range is <c>[1..16380]</c>.
*	@param[in] writeConfirm    Enables/disables write confirmation for this memory write.
*
*	@note Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
*
*	@cmdsize 4+sizeInDwords
*/
void writeDataInline(void * dstGpuAddr, const void * data, uint32_t sizeInDwords, Gnm::WriteDataConfirmMode writeConfirm)
{
	return m_dcb.writeDataInline(dstGpuAddr, data, sizeInDwords, writeConfirm);
}

/** @brief Copies data inline into the command buffer and uses the command processor's DMA engine to transfer it to a destination GPU address through the GPU L2 cache.
*
*  @param[out] dstGpuAddr     Destination address to which this function writes. Must be 4-byte aligned.
*  @param[in] data            Pointer to data to be copied inline. This pointer must not be <c>NULL</c>.
*  @param[in] sizeInDwords    Number of <c>DWORD</c>s of data to copy. The valid range is <c>[1..16380]</c>.
*  @param[in] cachePolicy	   Specifies the cache policy to use if the data is written to the GPU's L2 cache.
*  @param[in] writeConfirm    Enables or disables write confirmation for this memory write.
*
*  @note Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
*
*  @cmdsize 4+sizeInDwords
*/
void writeDataInlineThroughL2(void * dstGpuAddr, const void * data, uint32_t sizeInDwords, Gnm::CachePolicy cachePolicy, Gnm::WriteDataConfirmMode writeConfirm)
{
	return m_dcb.writeDataInlineThroughL2(dstGpuAddr, data, sizeInDwords, cachePolicy, writeConfirm);
}

/** @brief Triggers an event on the GPU.
*
*  @param[in] eventType Type of event for which the command processor waits.
*
*  @cmdsize 2
*/
void triggerEvent(Gnm::EventType eventType)
{
	return m_dcb.triggerEvent(eventType);
}

/** @brief Requests the GPU to trigger an interrupt upon release memory event.
*
*	@param[in] eventType   Type of memory event that triggers the interrupt.
*	@param[in] cacheAction Cache action to perform when <c><i>eventType</i></c> event occurs.
*
*	@cmdsize 7
*/
void triggerReleaseMemEventInterrupt(Gnm::ReleaseMemEventType eventType, Gnm::CacheAction cacheAction)
{
	return m_dcb.triggerReleaseMemEventInterrupt(eventType, cacheAction);
}

/** @brief Writes the specified value to the given location in memory and triggers an interrupt upon release memory event.
*
*  @param[in] eventType   Determines the type of shader to wait for before writing <c><i>immValue</i></c> to <c>*<i>dstGpuAddr</i></c>.
*  @param[in] dstSelector Specifies which levels of the memory hierarchy to write to.
*  @param[out] dstGpuAddr GPU address to which <c><i>immValue</i></c> will be written. Must be 4-byte aligned (or 8-byte aligned, if <c><i>srcSelector</i></c> refers to a 64-bit quantity). This pointer must not be <c>NULL</c>.
*  @param[in] srcSelector Specifies the type of data to write -- either the provided <c><i>immValue</i></c>, or an internal GPU counter.
*  @param[in] immValue    Value that will be written to <c><i>dstGpuAddr</i></c>. If <c><i>srcSelect</i></c> specifies a GPU counter, this argument
*						   will be ignored.
*  @param[in] cacheAction      Specifies which caches to flush and invalidate after the specified write is complete.
*  @param[in] writePolicy		Specifies the cache policy to use, if the data is written to the GPU's L2 cache.
*
*  @note While the GPU supports writing to 4-byte-aligned addresses from any EventWriteSource, significant performance issues can arise on the CPU when accessing an 8-byte
*		  value that isn't 8-byte-aligned (e.g. 64-bit atomic operations whose destination crosses a cache line boundary). It is strongly recommended that 8-byte values
* 	      be written to 8-byte-aligned addresses.
*
*  @cmdsize 7
*/
void writeReleaseMemEventWithInterrupt(Gnm::ReleaseMemEventType eventType, Gnm::EventWriteDest dstSelector, void * dstGpuAddr, Gnm::EventWriteSource srcSelector, uint64_t immValue, Gnm::CacheAction cacheAction, Gnm::CachePolicy writePolicy)
{
	return m_dcb.writeReleaseMemEventWithInterrupt(eventType, dstSelector, dstGpuAddr, srcSelector, immValue, cacheAction, writePolicy);
}

/** @brief Writes the specified value to the given location in memory.
*
*  @param[in] eventType   Determines the type of shader to wait for before writing <c><i>immValue</i></c> to <c>*<i>dstGpuAddr</i></c>.
*  @param[in] dstSelector Specifies which levels of the memory hierarchy to write to.
*  @param[out] dstGpuAddr GPU address to which <c><i>immValue</i></c> will be written. Must be 4-byte aligned (or 8-byte aligned, if <c><i>srcSelector</i></c> refers to a 64-bit quantity). This pointer must not be <c>NULL</c>.
*  @param[in] srcSelector Specifies the type of data to write -- either the provided <c><i>immValue</i></c>, or an internal GPU counter.
*  @param[in] immValue    Value that will be written to <c><i>dstGpuAddr</i></c>. If <c><i>srcSelect</i></c> specifies a GPU counter, this argument
*						   will be ignored.
*  @param[in] cacheAction      Specifies which caches to flush and invalidate after the specified write is complete.
*  @param[in] writePolicy		Specifies the cache policy to use, if the data is written to the GPU's L2 cache.
*
*  @note While the GPU supports writing to 4-byte-aligned addresses from any EventWriteSource, significant performance issues can arise on the CPU when accessing an 8-byte
*		  value that isn't 8-byte-aligned (e.g. 64-bit atomic operations whose destination crosses a cache line boundary). It is strongly recommended that 8-byte values
*		  be written to 8-byte-aligned addresses.
*
*  @cmdsize 7
*/
void writeReleaseMemEvent(Gnm::ReleaseMemEventType eventType, Gnm::EventWriteDest dstSelector, void * dstGpuAddr, Gnm::EventWriteSource srcSelector, uint64_t immValue, Gnm::CacheAction cacheAction, Gnm::CachePolicy writePolicy)
{
	return m_dcb.writeReleaseMemEvent(eventType, dstSelector, dstGpuAddr, srcSelector, immValue, cacheAction, writePolicy);
}

/** @brief Stalls command processor until specified test passes.
*
*  The 32-bit value at the specified GPU address is tested against the reference value with the test qualified by the specified function and mask.
*  Basically, this function tells the GPU to stall until <c><i>compareFunc</i>((*<i>gpuAddr</i> and <i>mask</i>), <i>refValue</i>) == true</c>.
*
*  @param[in] gpuAddr		Address to poll. Must be 4-byte aligned.
*	@param[in] mask			Mask to be applied to <c>*<i>gpuAddr</i></c> before comparing to <c><i>refValue</i></c>.
*	@param[in] compareFunc	Specifies the type of comparison to be done between (<c>*<i>gpuAddr</i></c> and <c><i>mask</i></c>) and the <c><i>refValue</i></c>.
*	@param[in] refValue		Expected value of <c>*<i>gpuAddr</i></c>.
* 
*	@cmdsize 7
*/
void waitOnAddress(void * gpuAddr, uint32_t mask, Gnm::WaitCompareFunc compareFunc, uint32_t refValue)
{
	return m_dcb.waitOnAddress(gpuAddr, mask, compareFunc, refValue);
}

/** @brief Waits for all PS shader output to one or more targets to complete and for the contexts using them to roll over.
*
*  When you use this method to specify the various render target slots (color and/or depth) to be checked within 
*  the provided base address and size, the command processor waits for all active contexts associated with those target to roll over.
*  This differs from the function of the DrawCommandBuffer, which does not require a context roll to determine that the shader output has finished. 
*  
*  Optionally, the caller may also specify that certain caches be flushed.
*
*  @param[in] baseAddr256			Starting base address (256-byte aligned) of the surface to be synchronized to (high 32 bits of a 40-bit
* 									virtual GPU address).
*  @param[in] sizeIn256ByteBlocks	Size of the surface. Has a granularity of 256 bytes. This must not be 0 if <c><i>targetMask</i></c> contains kWaitTargetSlotDb.
*  @param[in] targetMask			Configures which of the source and destination caches should be enabled for coherency. This field is
*									composed of individual flags from the <c>#WaitTargetSlot</c> enum.
*  @param[in] cacheAction			Specifies which caches to flush and invalidate after the specified writes are complete.
*  @param[in] extendedCacheMask	Specifies additional caches to flush and invalidate. This field is composed of individual flags from Gnm::ExtendedCacheAction.
*									<c>kExtendedCacheActionFlushAndInvalidateCbCache</c> and <c>kExtendedCacheActionFlushAndInvalidateDbCache</c> flags are not supported under DispatchCommandBuffer.
*
*  @note This command waits only on output written by graphics shaders - not compute shaders!
*  @note Unlike the corresponding DrawCommandBuffer::waitForGraphicsWrites() function, this variant does not "wake up" until the graphics pipe rolls its context.
*        To prevent delays and potential deadlocks, a graphics job that will be waited on by this function should roll its context immediately once the job is complete. For example,
*        writing to any GPU context register is one way to trigger a context roll.
*
*  @see Gnm::WaitTargetSlot, Gnm::ExtendedCacheAction, flushShaderCachesAndWait()
*
*  @cmdsize 7
*/
void waitForGraphicsWrites(uint32_t baseAddr256, uint32_t sizeIn256ByteBlocks, uint32_t targetMask, Gnm::CacheAction cacheAction, uint32_t extendedCacheMask)
{
	return m_dcb.waitForGraphicsWrites(baseAddr256, sizeIn256ByteBlocks, targetMask, cacheAction, extendedCacheMask);
}

/** @brief Requests a flush of the specified data cache(s), and waits for the flush operation(s) to complete.
*
*	@param[in] cacheAction       Specifies which caches to flush and invalidate.
*	@param[in] extendedCacheMask Specifies additional caches to flush and invalidate. This field is composed of individual flags from Gnm::ExtendedCacheAction.
*	                             <c>kExtendedCacheActionFlushAndInvalidateCbCache</c> and <c>kExtendedCacheActionFlushAndInvalidateDbCache</c> flags are not supported under DispatchCommandBuffer.
*
*	@note This function is equivalent to calling <c>waitForGraphicsWrites(0,1,0,<i>cacheAction</i>,<i>extendedCacheMask</i>)</c>. For information on avoiding a potential synchronization pitfall,
*		  see the description of the waitForGraphicsWrites() function.
*
*	@see Gnm::ExtendedCacheAction, waitForGraphicsWrites()
*
*	@cmdsize 7
*/
void flushShaderCachesAndWait(Gnm::CacheAction cacheAction, uint32_t extendedCacheMask)
{
	return m_dcb.flushShaderCachesAndWait(cacheAction, extendedCacheMask);
}

/** @brief Inserts the specified number of <c>DWORD</c>s in the command buffer as a <c>NOP</c> packet.
*
*  @param[in] numDwords   Number of <c>DWORD</c>s to insert. The entire packet (including the PM4 header) will be <c><i>numDwords</i></c>.
*						   Valid range is <c>[0..16384]</c>.
*
*  @cmdsize numDwords
*/
void insertNop(uint32_t numDwords)
{
	return m_dcb.insertNop(numDwords);
}

/** @brief Sets a marker command in the command buffer that will be used by performance analysis and debug tools.
*
*  The marker command created by this function will be handled as a standalone marker. For a scoped marker block, use pushMarker() and popMarker().
*
*  @param[in] debugString   String to be embedded into the command buffer. This pointer must not be <c>NULL</c>, and <c>strlen(<i>debugString</i>) < 32756</c>.
*
*  @see pushMarker(), popMarker()
* 	
*  @cmdsize 2 + 2*((strlen(debugString)+1+4+7)/(2*sizeof(uint32_t))) + (strlen(debugString)+1+4+3)/sizeof(uint32_t)
*/
void setMarker(const char * debugString)
{
	return m_dcb.setMarker(debugString);
}

/** @brief Sets a colored marker command in the command buffer that will be used by the PA/Debug tools.
*
*  The marker command created by this function will be handled as a standalone marker. For a scoped marker block,
*  use pushMarker() and popMarker().
*
*  @param[in]	debugString		String to be embedded into the command buffer. This pointer must not be <c>NULL</c>, and <c>strlen(<i>debugString</i>) < 32756</c>.
*  @param[in]	argbColor		The color of the marker.
*	
*  @see pushMarker(), popMarker()
*
*  @cmdsize 2 + 2*((strlen(debugString)+1+8+7)/(2*sizeof(uint32_t))) + (strlen(debugString)+1+8+3)/sizeof(uint32_t)
*/
void setMarker(const char * debugString, uint32_t argbColor)
{
	return m_dcb.setMarker(debugString, argbColor);
}

/** @brief Sets a marker command in the command buffer that will be used by the Performance Analysis and Debug tools.
* 
*  The marker command created by this function is handled as the beginning of a scoped marker block.
*  Close the block with a matching call to popMarker(). Marker blocks can be nested.
*
*  @param[in] debugString   String to be embedded into the command buffer. This pointer must not be <c>NULL</c>, and <c>strlen(<i>debugString</i>) < 32756</c>.
*	
*  @see popMarker()
*
*  @cmdsize 2 + 2*((strlen(debugString)+1+4+7)/(2*sizeof(uint32_t))) + (strlen(debugString)+1+4+3)/sizeof(uint32_t)
*/
void pushMarker(const char * debugString)
{
	return m_dcb.pushMarker(debugString);
}

/** @brief Sets a colored marker command in the command buffer that will be used by the Performance Analysis and Debug tools.
* 
*  The marker command created by this function is handled as the beginning of a scoped marker block.
*  Close the block with a matching call to popMarker(). Marker blocks can be nested.
*
*  @param[in]	debugString		The string to be embedded into the command buffer. This pointer must not be <c>NULL</c>, and <c>strlen(<i>debugString</i>) < 32756</c>.
*  @param[in]	argbColor		The color of the marker.
*
*  @see popMarker()
*
*  @cmdsize 2 + 2*((strlen(debugString)+1+8+7)/(2*sizeof(uint32_t))) + (strlen(debugString)+1+8+3)/sizeof(uint32_t)
*/
void pushMarker(const char * debugString, uint32_t argbColor)
{
	return m_dcb.pushMarker(debugString, argbColor);
}

/** @brief Closes the marker block opened by the most recent pushMarker() command.
*
*	@see pushMarker()
*
*	@cmdsize 6
*/
void popMarker()
{
	return m_dcb.popMarker();
}

/** @brief Prefetches data into L2$
*
*	@param[in] dataAddr      The GPU address of the buffer to be preloaded in L2$. Must be non-<c>NULL</c>.
*	@param[in] sizeInBytes   The size of the buffer in bytes. The transfer size must be less than or equal to #kDmaMaximumSizeInBytes.
*		
*	@note  Do not use this function for buffers to which you intend to write.
*	@note  This function is currently disabled; it ignores its parameters and writes a <c>NOP</c> packet to the command buffer.
*	@note  The GPU supports a maximum of two DMA transfers in flight concurrently. If a third DMA is issued, the CP will stall until one of the previous two completes. The following
*	       Gnm commands are implemented using CP DMA transfers, and are thus subject to this restriction:
*		   <ul>
*			 <li>dmaData()</li>
*			 <li>prefetchIntoL2()</li>
*			 <li>writeOcclusionQuery()</li>
*		   </ul>
*
*	@cmdsize 7
*/
void prefetchIntoL2(void * dataAddr, uint32_t sizeInBytes)
{
	return m_dcb.prefetchIntoL2(dataAddr, sizeInBytes);
}

/** @brief Signals a semaphore.
*
*	@param[in] semAddr Address of the semaphore's mailbox (must be 8-byte aligned). This pointer must not be <c>NULL</c>.
*	@param[in] behavior Selects between incrementing the mailbox value and setting the mailbox value to 1.
*	@param[in] updateConfirm If enabled, the packet waits for the mailbox to be written.
*
*	@cmdsize 3
*/
void signalSemaphore(uint64_t * semAddr, Gnm::SemaphoreSignalBehavior behavior, Gnm::SemaphoreUpdateConfirmMode updateConfirm)
{
	return m_dcb.signalSemaphore(semAddr, behavior, updateConfirm);
}

/** @brief Waits on a semaphore.
*
*	Waits until the value in the mailbox is not 0.
*	While waiting on a semaphore, the compute queue is idle and can be preempted by other queues.
*
*	@param[in] semAddr  Address of the semaphore's mailbox (must be 8 bytes aligned). This pointer must not be <c>NULL</c>.
*	@param[in] behavior Selects the action to perform when the semaphore opens (mailbox becomes non-zero): either decrement or do nothing.
*
*	@cmdsize 3
*/
void waitSemaphore(uint64_t * semAddr, Gnm::SemaphoreWaitBehavior behavior)
{
	return m_dcb.waitSemaphore(semAddr, behavior);
}

/** @brief Sets compute queue priority.
*	Queue priority can be from 0 through 15 with 15 being the highest priority and 0 the lowest.
* 
*	@param[in] queueId The ID of the queue returned by Gnm::mapComputeQueue().
*	@param[in] priority The queue priority.
*
*	@cmdsize 3
*/
void setQueuePriority(uint32_t queueId, uint32_t priority)
{
	return m_dcb.setQueuePriority(queueId, priority);
}

/** @deprecated This function has no effect, and will be removed.
				@brief Enables a compute queue's quantum timer.

				The quantum timer starts counting down whenever the queue is activated by the scheduler. After it reaches 0, the queue yields to other queues that have the same priority.
				The next time the queue becomes active, its quantum timer is restarted with the same initial value.
				This is useful for ensuring the fair distribution of compute time among queues that have the same priority.
				@param[in] queueId The ID of the queue returned by Gnm::mapComputeQueue().
				@param[in] quantumScale The units of time used.
				@param[in] quantumDuration The initial timer value. Valid range is <c>0-63</c>.
				@cmdsize 3
*/
SCE_GNM_API_DEPRECATED_MSG("Quantum setting from the command buffer has no effect.")
void enableQueueQuantumTimer(uint32_t queueId, Gnm::QuantumScale quantumScale, uint32_t quantumDuration)
{
	return m_dcb.enableQueueQuantumTimer(queueId, quantumScale, quantumDuration);
}

/** @deprecated This function has no effect, and will be removed.
				@brief Disables a compute queue's quantum timer.
				@param[in] queueId The ID of the queue returned by Gnm::mapComputeQueue().
				@cmdsize 3
*/
SCE_GNM_API_DEPRECATED_MSG("Quantum setting from the command buffer has no effect.")
void disableQueueQuantumTimer(uint32_t queueId)
{
	return m_dcb.disableQueueQuantumTimer(queueId);
}

/** @brief Inserts a pause command to stall DCB processing and reserves a specified number of <c>DWORD</c>s after it.
*
*	@param[in] reservedDWs      The number of <c>DWORD</c>s to reserve.
*
*	@return The address of the beginning of the "hole" in the command buffer following the pause command. To release the pause command, pass this address to the resume() method.
*	
*	@note The pause command in the Async Compute pauses the entire pipe, not just the queue that executes it. Therefore, it is possible for this command to stall up to 8 queues.
*
*  @see DispatchCommandBuffer::resume(), DispatchCommandBuffer::fillAndResume(), DispatchCommandBuffer::chainCommandBufferAndResume()
*
*	@cmdsize 2 + reservedDWs
*/
uint64_t pause(uint32_t reservedDWs)
{
	return m_dcb.pause(reservedDWs);
}

/** @brief Immediately releases a previously written pause command.
*	
*	@param[in]	holeAddr		The address of the "hole" following a pause command. The pause() method returned this value when it created the pause command.
*	
*	@note Because the CPU modifies the pause command directly, this method releases the pause immediately. 
*
*  @see DispatchCommandBuffer::pause()
*
*	@cmdsize 0
*/
void resume(uint64_t holeAddr)
{
	return m_dcb.resume(holeAddr);
}

/** @brief Fills the allocated "hole" by copying the <c><i>commandStream</i></c> into it and then releases the pause command immediately.
*	
*	@param[in]	holeAddr		The address of the "hole" following a pause command. The pause() method returned this value when it created the pause command.
*	@param[in]	commandStream	The command stream data to copy into the "hole".
*	@param[in]	sizeInDW		The size of the hole in <c>DWORD</c>s. 
*	
*	@note Because the CPU modifies the pause command directly, this method releases the pause immediately. 
*
*  @see DispatchCommandBuffer::pause()
*
*	@cmdsize 0
*/
void fillAndResume(uint64_t holeAddr, void * commandStream, uint32_t sizeInDW)
{
	return m_dcb.fillAndResume(holeAddr, commandStream, sizeInDW);
}

/** @brief Inserts a reference to another command buffer into a previously allocated "hole" and then releases the pause command immediately.
*	
*	@param[in]	holeAddr		The address of the "hole" following a pause command. The pause() method returned this value when it created the pause command.
*	@param[in]	nextIbBaseAddr		A pointer to the command buffer that is the destination of the reference this method creates. This pointer must not be <c>NULL</c>.
*	@param[in]	nextIbSizeInDW		The size of the <c><i>nextIbBaseAddr</i></c> buffer in DWORDs. The valid range is <c>[1..kIndirectBufferMaximumSizeInBytes/4]</c>.
*
*	@note The size of the allocated "hole" must be exactly four <c>DWORD</c>s.
*	@note This method must be the final command in any command buffer that uses it.
*	@note Because the CPU modifies the pause command directly, this method releases the pause immediately. 
*
*  @see DispatchCommandBuffer::pause()
*
*	@cmdsize 0 
*/
void chainCommandBufferAndResume(uint64_t holeAddr, void * nextIbBaseAddr, uint64_t nextIbSizeInDW)
{
	return m_dcb.chainCommandBufferAndResume(holeAddr, nextIbBaseAddr, nextIbSizeInDW);
}

/** @brief Queues the release of a pause command at the end of a shader.
*	
*	This method is functionally equivalent to resume(), except that it waits for the previously queued shader to finish before resuming.
*               
*	@param[in] eventType		The type of shader for which this function waits before writing <c><i>immValue</i></c> to <c>*dstGpuAddr</c>.
*	@param[in] holeAddr			The address of the "hole" following a pause command. The pause() method returned this value when it created the pause command.
*	@param[in] cacheAction      Specifies the caches to flush and invalidate after the specified write completes.
*	@param[in] writePolicy		Specifies the cache policy to use if the data is written to the GPU's L2 cache.
*
* 	@note  Because the CP modifies the pause command, this method releases the pause only when the 
*		 CP executes the writeResumeEvent() function.
*
*  @see DispatchCommandBuffer::pause()
*
*	@cmdsize 7
*/
void writeResumeEvent(Gnm::ReleaseMemEventType eventType, uint64_t holeAddr, Gnm::CacheAction cacheAction, Gnm::CachePolicy writePolicy)
{
	return m_dcb.writeResumeEvent(eventType, holeAddr, cacheAction, writePolicy);
}

/** @brief Queues the release of a pause command at the end of a shader and triggers an interrupt upon resuming.
* 					
*	This method is functionally equivalent to resume(), except that it waits for the previously queued shader to finish before resuming.
*
*	@param[in] eventType		The type of shader for which this function waits before writing <c><i>immValue</i></c> to <c>*dstGpuAddr</c>.
*	@param[in] holeAddr			The address of the "hole" following a pause command. The pause() method returned this value when it created the pause command.
*	@param[in] cacheAction      Specifies the caches to flush and invalidate after the specified write completes.
*	@param[in] writePolicy		Specifies the cache policy to use if the data is written to the GPU's L2 cache.
*
* 	@note Because the CP modifies the pause command, this method releases the pause only when the 
*		  CP excutes the writeResumeEvent() function.
*
*  @see DispatchCommandBuffer::pause()
*
*	@cmdsize 7
*/
void writeResumeEventWithInterrupt(Gnm::ReleaseMemEventType eventType, uint64_t holeAddr, Gnm::CacheAction cacheAction, Gnm::CachePolicy writePolicy)
{
	return m_dcb.writeResumeEventWithInterrupt(eventType, holeAddr, cacheAction, writePolicy);
}

/** @brief Waits for a previously written pause command to be released.
*	
*	@param[in]	holeAddr	The address of the "hole" following a pause packet. This value was returned by pause() when the packet was created.
*
*  @see DispatchCommandBuffer::pause()
*
*	@cmdsize 7
*/
void waitForResume(uint64_t holeAddr)
{
	return m_dcb.waitForResume(holeAddr);
}

/** @brief Writes a command that will resume a previously written pause command.
*
*	The resume command that this function writes does not take effect until the CP executes the resume command. 
*	In contrast, the resume() function releases the pause immediately.
*	
*	@param[in]	holeAddr		The address of the "hole" following a pause command. The pause() method returned this value when it created the pause command.
*
*  @see DispatchCommandBuffer::pause()
*
*	@cmdsize 5
*/
void writeResume(uint64_t holeAddr)
{
	return m_dcb.writeResume(holeAddr);
}

/** @brief Inserts a "call" to another command buffer.
*
*	@param[in] cbBaseAddr Address of the command buffer to call. This pointer must not be <c>NULL</c>.
*	@param[in] cbSizeInDW Size of the called command buffer in DWORDs. The valid range is <c>[1..kIndirectBufferMaximumSizeInBytes/4]</c>.
*
*	@note  Calls are only allowed off the primary ring.
*
*	@cmdsize 4
*/
void callCommandBuffer(void * cbBaseAddr, uint64_t cbSizeInDW)
{
	return m_dcb.callCommandBuffer(cbBaseAddr, cbSizeInDW);
}

/** @brief Inserts a reference to another command buffer.
*
*	@param[in]	cbBaseAddr	The address of the command buffer that is the destination of the reference that this method creates. This pointer must not be <c>NULL</c>.
*	@param[in]	cbSizeInDW	The size of the <c><i>cbBaseAddr</i></c> buffer in <c>DWORD</c>s. The valid range is <c>[1..kIndirectBufferMaximumSizeInBytes/4]</c>.
*
*	@note This function must be the final command in any command buffer that uses it.
*	
*	@cmdsize 4
*/
void chainCommandBuffer(void * cbBaseAddr, uint64_t cbSizeInDW)
{
	return m_dcb.chainCommandBuffer(cbBaseAddr, cbSizeInDW);
}

#ifdef _MSC_VER
#	pragma warning(pop)
#endif

#endif // !defined(_SCE_GNMX_COMPUTECONTEXT_METHODS_H)
