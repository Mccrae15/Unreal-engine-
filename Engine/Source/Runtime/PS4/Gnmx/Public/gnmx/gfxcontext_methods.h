/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/
// This file was auto-generated from drawcommandbuffer.h -- do not edit by hand!
// This file should NOT be included directly by any non-Gnmx source files!

#if !defined(_SCE_GNMX_GFXCONTEXT_METHODS_H)
#define _SCE_GNMX_GFXCONTEXT_METHODS_H

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable:4996) // deprecated functions
#endif

/** @deprecated This function is no longer necessary and will be removed in the next release.
*  @brief Sets the PM4 packet type to Graphics or Compute shader type.
*
*  This function is no longer necessary and will be removed in the next release. The only
*  affected function is DrawCommandBuffer::setBaseIndirectArgs(), which now accepts shaderType to
*  specify whether given indirect arguments are for use with dispatchIndirect() or drawIndirect().
*
*  This function never rolls the hardware context.
*
*  @param[in] shaderType  Specifies the use of Graphics or Compute mode for DrawCommandBuffer commands.
*
*  @cmdsize 0
*/
SCE_GNM_API_REMOVED("This function is no longer necessary and will be removed in the next release.")
void setShaderType(Gnm::ShaderType shaderType)
{
	return m_dcb.setShaderType(shaderType);
}

/** @brief Flushes the streamout pipeline.
				This function should be called before changing or querying streamout state with writeStreamoutBufferUpdate().
				This function will flush the entire GPU pipeline and wait for it to be idle. Use extremely sparingly!
				@cmdsize 21
*/
void flushStreamout()
{
	return m_dcb.flushStreamout();
}

/** @brief Sets the streamout buffer's parameters.
				The VGT (vertex geometry tessellator) needs to know the size and stride of the streamout buffers to correctly update buffer offset.
				This function will roll the hardware context.
				@param[in] bufferId The streamout buffer to update.
				@param[in] bufferSizeInDW The size of the buffer.
				@param[in] bufferStrideInDW The size of an element in the buffer.
				@see setStreamoutMapping(), writeStreamoutBufferUpdate()
				@cmdsize 4
*/
void setStreamoutBufferDimensions(Gnm::StreamoutBufferId bufferId, uint32_t bufferSizeInDW, uint32_t bufferStrideInDW)
{
	return m_dcb.setStreamoutBufferDimensions(bufferId, bufferSizeInDW, bufferStrideInDW);
}

/** @brief Sets the mapping from GS streams to the VS streams in streamout.
				This function will roll the hardware context.
				@param[in] mapping A four-element bitfield, in which each four-bit element controls four VS streams for a particular GS stream. This pointer must not be <c>NULL</c>.
				@see setStreamoutBufferDimensions(), writeStreamoutBufferUpdate()
				@cmdsize 3
*/
void setStreamoutMapping(const Gnm::StreamoutBufferMapping * mapping)
{
	return m_dcb.setStreamoutMapping(mapping);
}

/** @brief Writes a streamout update packet.
				The packet that this function writes must be preceded by flushStreamout().
				@param[in] buffer The ID of the streamout buffer to update.
				@param[in] sourceSelect The operation to perform on the buffer offset register.
				@param[in] updateMemory The operation to perform with the current buffer filled size.
				@param[out] dstAddr If Gnm::kStreamoutBufferUpdateSaveFilledSize is specified, then <c><i>dstAddr</i></c> provides the memory offset for the saved value. 
					If <c><i>updateMemory</i></c> is <c>kStreamoutBufferUpdateSaveFilledSize</c>, then <c><i>dstAddr</i></c> must not be <c>NULL</c>, and it must be aligned to a 4-byte boundary.
				@param[in] srcAddrOrImm Specifies the parameter for the <c><i>sourceSelect</i></c> operation. 
					If <c><i>sourceSelect</i></c> is <c>kStreamoutBufferUpdateWriteIndirect</c>, then <c><i>srcAddrOrImm</i></c> must not be <c>0</c>, and it must be aligned to a 4-byte boundary.
				@see setStreamoutBufferDimensions(), setStreamoutMapping()
				@cmdsize 6
*/
void writeStreamoutBufferUpdate(Gnm::StreamoutBufferId buffer, Gnm::StreamoutBufferUpdateWrite sourceSelect, Gnm::StreamoutBufferUpdateSaveFilledSize updateMemory, void * dstAddr, uint64_t srcAddrOrImm)
{
	return m_dcb.writeStreamoutBufferUpdate(buffer, sourceSelect, updateMemory, dstAddr, srcAddrOrImm);
}

/** @brief Writes the immediate offset into a buffer.
				This function is a convenience wrapper around writeStreamoutBufferUpdate().
				@param[in] buffer The ID of the streamout buffer to update.
				@param[in] offset The offset to write into the buffer offset register.
				@cmdsize 6
*/
void writeStreamoutBufferOffset(Gnm::StreamoutBufferId buffer, uint32_t offset)
{
	return m_dcb.writeStreamoutBufferOffset(buffer, offset);
}

/** @brief Enables streamout from a VS shader.
				This allows streamout without a GS shader. After enabling, the streamout has to be set up the same way as the streamout from a GS shader.
				The VS shader has to be compiled with the streamout for this to have any effect. 
				The enabled state might be disabled by setting a GS shader and will be disabled with <c>disableGsMode()</c>.
				This function will roll the hardware context.
				
				@param[in] enable		A flag that specifies whether to enable streamout from VS shaders.
				@cmdsize 3
				@see setGsShader(), setGsMode()
*/
void setVsShaderStreamoutEnable(bool enable)
{
	return m_dcb.setVsShaderStreamoutEnable(enable);
}

/** @brief Sets up the buffer parameters for the opaque draw.
				
				This function sets up parameters needed to compute the number of elements involved in the drawOpaqueAuto() call. The formula for the number of elements is <c>(BUFFER SIZE - OFFSET)/ STRIDE</c>.
				Since the size is read from a memory location, it is useful when the number of elements to draw is not known at the time of a command buffer's creation. For example, this could occur
				when drawing elements from a streamout buffer.
				This function will roll the hardware context.
				@param sizeLocation			The pointer to a <c>DWORD</c> that contains the opaque buffer size in bytes.
				@param stride				The size of a buffer's element in <c>DWORD</c>.
				@param offset				The value in <c>DWORD</c> to be subtracted from the buffer's size.
				@see drawOpaqueAuto()
				@cmdsize 12
*/
void setupDrawOpaqueParameters(void * sizeLocation, uint32_t stride, uint32_t offset)
{
	return m_dcb.setupDrawOpaqueParameters(sizeLocation, stride, offset);
}

/** @brief For dispatches on the graphics ring, sets dispatch limits that control how many compute wavefronts will be allowed to run simultaneously in the GPU.
				This function never rolls the hardware context.
				@param[in] wavesPerSh			The wavefront limit per shader engine. The range is <c>[1:1023]</c>. Set 0 to disable the limit.
				@param[in] threadgroupsPerCu	The threadgroup limit per compute unit. The range is <c>[1:15]</c>. Set <c>0</c> to disable the limit.
				@param[in] lockThreshold		The per-shader-engine low threshold for locking. The granularity is 4. The range is <c>[1:63]</c> corresponding to <c>[4:252]</c> wavefronts. Set to 0 to disable locking.
				@cmdsize 3
				@see setGraphicsShaderControl(), Gnmx::GfxContext::setComputeShaderControl()
*/
void setComputeShaderControl(uint32_t wavesPerSh, uint32_t threadgroupsPerCu, uint32_t lockThreshold)
{
	return m_dcb.setComputeShaderControl(wavesPerSh, threadgroupsPerCu, lockThreshold);
}

/** @brief For dispatches on the graphics ring, sets a dispatch mask that determines the active compute units for compute shaders in the specified shader engine.
			All masks are logical masks indexed from 0 to Gnm::kNumCusPerSe <c>- 1</c>, regardless of which physical compute units are working and enabled by the driver.
			This function never rolls the hardware context.
			
			@param[in] engine			The shader engine to configure.
			@param[in] mask				A mask that enables compute units for the CS shader stage.
			
			@see setGraphicsShaderControl(), Gnmx::GfxContext::setGraphicsShaderControl()
			
			@cmdsize 6
*/
SCE_GNM_API_CHANGED
void setComputeResourceManagement(Gnm::ShaderEngine engine, uint16_t mask)
{
	return m_dcb.setComputeResourceManagement(engine, mask);
}

/** @brief For dispatches on the graphics ring, sets a dispatch mask that determines the active compute units for compute shaders in the specified shader engine.
			All masks are logical masks indexed from <c>0</c> to Gnm::kNumCusPerSe <c>- 1</c>, regardless of which physical compute units are working and enabled by the driver.
			This function never rolls the hardware context.
			
			@param[in] engine			The shader engine to configure.
			@param[in] mask				A mask that enables compute units for the CS shader stage.
			
			@see setGraphicsShaderControl(), Gnmx::GfxContext::setGraphicsShaderControl()
			
			@cmdsize 3
*/
void setComputeResourceManagementForBase(Gnm::ShaderEngine engine, uint16_t mask)
{
	return m_dcb.setComputeResourceManagementForBase(engine, mask);
}

/** @brief For dispatches on the graphics ring, sets a dispatch mask that determines the active compute units for compute shaders in the specified shader engine.
			All masks are logical masks indexed from <c>0</c> to Gnm::kNumCusPerSe <c>- 1</c>, regardless of which physical compute units are working and enabled by the driver.
			This function never rolls the hardware context.
			
			@param[in] engine			The shader engine to configure.
			@param[in] mask				A mask that enables compute units for the CS shader stage.
			
			@see setGraphicsShaderControl(), Gnmx::GfxContext::setGraphicsShaderControl()
			
			@cmdsize 3
*/
void setComputeResourceManagementForNeo(Gnm::ShaderEngine engine, uint16_t mask)
{
	return m_dcb.setComputeResourceManagementForNeo(engine, mask);
}

/** @brief Specifies how the scratch buffer (graphics only) should be subdivided between the executing wavefronts for graphics shaders (all stages except CS).
*
* <c><i>maxNumWaves</i> * <i>num1KByteChunksPerWave</i> * 1024</c> must be less than or equal to the total size of the scratch buffer.
* This function will roll the hardware context.
*  @param[in] maxNumWaves Maximum number of wavefronts that could be using the scratch buffer simultaneously.
*                     This value can range from <c>[0..4095]</c>, but the recommended upper limit is getNumShaderEngines()<c>*320</c>. For more information, see setComputeScratchSize().
*  @param[in] num1KByteChunksPerWave The amount of scratch buffer space for use by each wavefront. Specified in units of 1024-byte chunks. Must be < <c>8192</c>.
*  @see setComputeScratchSize()
*  @cmdsize 3
*/
void setGraphicsScratchSize(uint32_t maxNumWaves, uint32_t num1KByteChunksPerWave)
{
	return m_dcb.setGraphicsScratchSize(maxNumWaves, num1KByteChunksPerWave);
}

/** @brief Specifies how the scratch buffer (compute only) should be subdivided between the executing wavefronts for compute (CS stage) shaders.
*
* <c><i>maxNumWaves</i> * <i>num1KByteChunksPerWave</i> * 1024</c> must be less than or equal to the total size of the scratch buffer.
*
* Each shader engine supports dynamic scratch buffer offset allocation for up to 32 compute wavefronts per compute unit (CU), so setting <c>maxNumWaves</c>
* to less than getNumShaderEngines()<c>*320</c> (640 in base mode, 1280 in NEO mode) will reduce the limit for each shader engine. Setting 
* <c>maxNumWaves</c> to a value greater than this will not allow more wavefronts to run, but will result in additional unused space being allocated
* in the scratch buffer, as the base offset for the Nth SE is always <c>N*(<i>maxNumWaves</i>/</c>getNumShaderEngines()<c> * <i>num1KByteChunksPerWave</i> * 1024</c>.
* For this reason, getNumShaderEngines()<c>*320</c> is the recommended upper limit for <c><i>maxNumWaves</i></c>.
*
* This function will roll the hardware context.
*  @param[in] maxNumWaves Maximum number of wavefronts that could be using the scratch buffer simultaneously.
*                     This value can range from <c> [0..4095] </c>, but the recommended upper limit is <c> (getGpuMode() == kGpuModeNeo ? 1280 : 640) </c>.
*  @param[in] num1KByteChunksPerWave The amount of scratch buffer space for use by each wavefront. Specified in units of 1024-byte chunks.  Must be <c> < 8192</c>.
*  @see setGraphicsScratchSize()
*  @cmdsize 3
*/
void setComputeScratchSize(uint32_t maxNumWaves, uint32_t num1KByteChunksPerWave)
{
	return m_dcb.setComputeScratchSize(maxNumWaves, num1KByteChunksPerWave);
}

/** @brief Sets the parameters for the Viewport Transform Engine.
				This function will roll the hardware context.
				@param[in] vportControl Register contents.  See ViewportTransformControl structure definition for details.
				@see ViewportTransformControl
				@cmdsize 3
*/
void setViewportTransformControl(Gnm::ViewportTransformControl vportControl)
{
	return m_dcb.setViewportTransformControl(vportControl);
}

/** @brief Sets the parameters for controlling the polygon clipper.
				This function will roll the hardware context.
			  
				@param[in] reg Value to write to the clip control register.
			  
				@see Gnm::ClipControl
				@cmdsize 3
*/
void setClipControl(Gnm::ClipControl reg)
{
	return m_dcb.setClipControl(reg);
}

/** @brief Sets the parameters of one of the eight user clip planes.
				Any vertex that lies on the negative half space of the plane are determined to be outside the clip plane.
				This function will roll the hardware context.
			  @param[in] clipPlane  Indicates which clip plane to set the values for. Range is [0-5].
			  @param[in] x          X component of the plane equation.
			  @param[in] y          Y component of the plane equation.
			  @param[in] z          Z component of the plane equation.
			  @param[in] w          W component of the plane equation.
			  @cmdsize 6
*/
void setUserClipPlane(uint32_t clipPlane, float x, float y, float z, float w)
{
	return m_dcb.setUserClipPlane(clipPlane, x, y, z, w);
}

/** @brief Specifies the top-left and bottom-right coordinates of the four clip rectangles.
			This function will roll the hardware context.
			@param[in] rectId    Index of the clip rectangle to modify (0..3).
			@param[in] left      Left x value of clip rectangle.  15-bit unsigned.  Valid range 0-16383.
			@param[in] top       Top y value of clip rectangle.  15-bit unsigned.  Valid range 0-16383.
			@param[in] right     Right x value of clip rectangle.  15-bit unsigned.  Valid range 0-16384.
			@param[in] bottom    Bottom y value of clip rectangle.  15-bit unsigned.  Valid range 0-16384.
			@cmdsize 4
*/
void setClipRectangle(uint32_t rectId, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom)
{
	return m_dcb.setClipRectangle(rectId, left, top, right, bottom);
}

/**	@brief Sets the clip rule to use for the OpenGL Clip Boolean function.
			This function will roll the hardware context.
			@param[in] clipRule		The inside flags for each of the four clip rectangles form a 4-bit binary number arranged as 3210.
										These 4 bits are taken as a 4-bit index and the corresponding bit in the 16-bit <c><i>clipRule</i></c> specifies whether the pixel is visible.
										Common values include:
										– <c>0xFFFE</c> ("Inside any of the four rectangles -> visible").
										– <c>0x8000</c> ("Outside any of the four rectangles -> not visible").
										– <c>0xFFFF</c> ("Always visible; ignore clip rectangles").
										– <c>0x0000</c> ("Always invisible" -- not very useful).
			@cmdsize 3
*/
void setClipRectangleRule(uint16_t clipRule)
{
	return m_dcb.setClipRectangleRule(clipRule);
}

/** @brief Configures the Primitive Setup register, which provides control for facedness, culling, polygon offset, window offset, provoking vertex, and so on.
				This function will roll the hardware context.
			  @param[in] reg    Value to write to the Primitive Setup register.
			  @see Gnm::PrimitiveSetup
			  @cmdsize 3
*/
void setPrimitiveSetup(Gnm::PrimitiveSetup reg)
{
	return m_dcb.setPrimitiveSetup(reg);
}

/** @brief Enables/disables the use of the primitive reset index, used with strip-type primitives to begin a new strip in the middle of a draw call.
				The reset index is specified with setPrimitiveResetIndex().
				This function will roll the hardware context.
			  @param[in] enable If <c>true</c>, primitive reset index functionality is enabled.  If <c>false</c>, it is disabled; the reset index is treated just like any other index buffer entry.
			  @see setPrimitiveResetIndex()
			  @cmdsize 3
*/
void setPrimitiveResetIndexEnable(bool enable)
{
	return m_dcb.setPrimitiveResetIndexEnable(enable);
}

/** @brief Sets the reset index: the value that starts a new primitive (strip/fan/polygon) when it is encountered in the index buffer.
				For this function to work, the reset index feature must be enabled with setPrimitiveResetIndexEnable().
				This function will roll the hardware context.
				@param[in] resetIndex The new restart index.
				@see setPrimitiveResetIndexEnable()
				@cmdsize 3
*/
void setPrimitiveResetIndex(uint32_t resetIndex)
{
	return m_dcb.setPrimitiveResetIndex(resetIndex);
}

/** @brief Sets the vertex quantization behavior, which describes how floating-point X,Y vertex coordinates are converted to fixed-point values.
				This function will roll the hardware context.
				@param[in] quantizeMode Controls the precision of the destination fixed-point vertex coordinate values.
				@param[in] roundMode Controls the rounding behavior when converting to fixed-point.
				@param[in] centerMode Controls the location of the pixel center: 0,0 (Direct3D-style) or 0.5,0.5 (OpenGL-style).
				@cmdsize 3
*/
void setVertexQuantization(Gnm::VertexQuantizationMode quantizeMode, Gnm::VertexQuantizationRoundMode roundMode, Gnm::VertexQuantizationCenterMode centerMode)
{
	return m_dcb.setVertexQuantization(quantizeMode, roundMode, centerMode);
}

/** @brief Specifies the offset from screen coordinates to window coordinates.
				Vertices will be offset by these values if <c><i>windowOffsetEnable</i></c> is <c>true</c> in
				setPrimitiveSetup(). The window scissor and generic scissor will be offset by these values if the <c><i>windowOffsetDisable</i></c> is <c>false</c> in
				setWindowScissor() / setGenericScissor().
				This function will roll the hardware context.
			  @param[in] offsetX  Offset in x-direction from screen to window coordinates. 16-bit signed.
			  @param[in] offsetY  Offset in y-direction from screen to window coordinates. 16-bit signed.
			  @see Gnm::PrimitiveSetup::setVertexWindowOffsetEnable()
			  @cmdsize 3
*/
void setWindowOffset(int16_t offsetX, int16_t offsetY)
{
	return m_dcb.setWindowOffset(offsetX, offsetY);
}

/** @brief Specifies the screen scissor rectangle parameters.
			This is the basic, global, always-enabled, scissor rectangle that applies to every render call.
			This scissor is not affected by the window offset specified by setWindowOffset(); it is specified in absolute coordinates.
				Will roll the hardware context.
			  @param[in] left                 Left hand edge of scissor rectangle.  Valid range -32768-16383.
			  @param[in] top                  Upper edge of scissor rectangle.  Valid range -32768-16383.
			  @param[in] right                Right hand edge of scissor rectangle. Valid range -32768-16384.
			  @param[in] bottom               Lower edge of scissor rectangle.  Valid range -32768-16384.
			  @cmdsize 4
*/
void setScreenScissor(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
	return m_dcb.setScreenScissor(left, top, right, bottom);
}

/** @brief Specifies the window scissor rectangle parameters.
				The window scissor is a global, auxiliary scissor rectangle that can be
				specified in either absolute or window-relative coordinates.
				This function will roll the hardware context.
			  @param[in] left                 Left hand edge of scissor rectangle.  Valid range 0-16383.
			  @param[in] top                  Upper edge of scissor rectangle.  Valid range 0-16383.
			  @param[in] right                Right hand edge of scissor rectangle. Valid range 0-16384.
			  @param[in] bottom               Lower edge of scissor rectangle.  Valid range 0-16384.
			  @param[in] windowOffsetEnable   Enables/disables the extra window offset provided by setWindowOffset(). If disabled, the first four arguments are interpreted as
			                              absolute coordinates.
			  @see setWindowOffset()
			  @cmdsize 4
*/
void setWindowScissor(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, Gnm::WindowOffsetMode windowOffsetEnable)
{
	return m_dcb.setWindowScissor(left, top, right, bottom, windowOffsetEnable);
}

/** @brief Specifies the generic scissor rectangle parameters.
				The generic scissor is an additional global, auxiliary scissor rectangle that can be
				specified in either absolute or window-relative coordinates.
				This function will roll the hardware context.
			  @param[in] left                 Left hand edge of scissor rectangle.  Valid range 0-16383.
			  @param[in] top                  Upper edge of scissor rectangle.  Valid range 0-16383.
			  @param[in] right                Right hand edge of scissor rectangle. Valid range 0-16384.
			  @param[in] bottom               Lower edge of scissor rectangle.  Valid range 0-16384.
			  @param[in] windowOffsetEnable   Enables/disables the extra window offset provided by setWindowOffset(). If disabled, the first four arguments are interpreted as
			                              absolute coordinates.
			  @see setWindowOffset()
			  @cmdsize 4
*/
void setGenericScissor(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, Gnm::WindowOffsetMode windowOffsetEnable)
{
	return m_dcb.setGenericScissor(left, top, right, bottom, windowOffsetEnable);
}

/** @brief Specifies the viewport scissor rectangle parameters associated with a viewport ID.
				Geometry shaders can select any of the 16
				viewports; all other shaders can only select viewport 0.
				
				This function will roll the hardware context.
			  @param[in] viewportId           ID of the viewport whose scissor should be updated (ranges from 0 through 15).
			  @param[in] left                 Left hand edge of scissor rectangle.  Valid range 0-32383.
			  @param[in] top                  Upper edge of scissor rectangle.  Valid range 0-32383.
			  @param[in] right                Right hand edge of scissor rectangle. Valid range 0-32384.
			  @param[in] bottom               Lower edge of scissor rectangle.  Valid range 0-32384.
			  @param[in] windowOffsetEnable   Enables/disables the extra window offset provided by setWindowOffset(). If disabled, the first four arguments are interpreted as
			                              absolute coordinates.
			  @see setWindowOffset(), setScanModeControl()
			  @note The viewport scissor will be ignored unless the feature is enabled with setScanModeControl().
			  @cmdsize 4
*/
void setViewportScissor(uint32_t viewportId, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, Gnm::WindowOffsetMode windowOffsetEnable)
{
	return m_dcb.setViewportScissor(viewportId, left, top, right, bottom, windowOffsetEnable);
}

/** @brief Sets the viewport parameters associated with a viewport ID.
				This function will roll the hardware context.
			  @param[in] viewportId           ID of the viewport (ranges from 0 through 15).
			  @param[in] dmin                 Minimum Z Value from Viewport Transform.  Z values will be clamped by the DB to this value.
			  @param[in] dmax                 Maximum Z Value from Viewport Transform.  Z values will be clamped by the DB to this value.
			  @param[in] scale                Array containing the x, y and z scales.
			  @param[in] offset               Array containing the x, y and z offsets.
			  @cmdsize 12
*/
void setViewport(uint32_t viewportId, float dmin, float dmax, const float scale[3], const float offset[3])
{
	return m_dcb.setViewport(viewportId, dmin, dmax, scale, offset);
}

/** @brief Specifies the boundaries beyond which rasterization resolution is reduced to 1/2 or 1/4 of its usual value.
			           Generally, this function is used to reduce the rendering detail in the periphery of a render target. To specify a region of the screen
					   that is always to be rendered at full resolution, use setFoveatedWindow().
				@param[in] xAxisLeftEye  The coordinate boundaries that affect resolution scale along the X axis. In stereo rendering, these boundaries are used only by the left eye.
				@param[in] xAxisRightEye The coordinate boundaries that affect resolution scale along the X axis. In stereo rendering, these boundaries are used only by the right eye.
				@param[in] yAxisBothEyes The coordinate boundaries that affect resolution scale along the Y axis. In stereo rendering, these boundaries are used by both eyes.
				@see setFoveatedWindow()
				@note  NEO mode only
				@cmdsize 5
*/
void setScaledResolutionGrid(const Gnm::ScaledResolutionGridAxis xAxisLeftEye, const Gnm::ScaledResolutionGridAxis xAxisRightEye, const Gnm::ScaledResolutionGridAxis yAxisBothEyes)
{
	return m_dcb.setScaledResolutionGrid(xAxisLeftEye, xAxisRightEye, yAxisBothEyes);
}

/** @brief Reset the window to its default value.
	@cmdsize 4
	@note  NEO mode only
*/
void resetFoveatedWindow()
{
	return m_dcb.resetFoveatedWindow();
}

/** @brief Specifies a rectangular window in screen-space to be rendered at full resolution, overriding the settings specified with setScaledResolutionGrid().
			
				This technique is known as "foveated imaging". Generally, it is used to maintain maximum image quality around the focal point of the player's gaze.
				@param[in] xMinLeftEye  The minimum boundary (inclusive) of the foveated window region along the X axis when rendering the left eye. Specified in 32-pixel units. Valid range is [0..255].
				@param[in] xMaxLeftEye  The maximum boundary (inclusive) of the foveated window region along the X axis when rendering the left eye. Specified in 32-pixel units. Valid range is [0..255].
				@param[in] xMinRightEye The minimum boundary (inclusive) of the foveated window region along the X axis when rendering the right eye. Specified in 32-pixel units. Valid range is [0..255].
				@param[in] xMaxRightEye The maximum boundary (inclusive) of the foveated window region along the X axis when rendering the right eye. Specified in 32-pixel units. Valid range is [0..255].
				@param[in] yMinBothEyes The minimum boundary (inclusive) of the foveated window region along the Y axis when rendering both eyes. Specified in 32-pixel units. Valid range is [0..255].
				@param[in] yMaxBothEyes The maximum boundary (inclusive) of the foveated window region along the Y axis when rendering both eyes. Specified in 32-pixel units. Valid range is [0..255].
				@cmdsize 4
				@note  NEO mode only
*/
void setFoveatedWindow(uint8_t xMinLeftEye, uint8_t xMaxLeftEye, uint8_t xMinRightEye, uint8_t xMaxRightEye, uint8_t yMinBothEyes, uint8_t yMaxBothEyes)
{
	return m_dcb.setFoveatedWindow(xMinLeftEye, xMaxLeftEye, xMinRightEye, xMaxRightEye, yMinBothEyes, yMaxBothEyes);
}

/** @brief Enables the MSAA and viewport scissoring settings in the Scan Mode control register.
				This function will roll the hardware context.
			  @param[in] msaa             Enables multisampling anti-aliasing.
			  @param[in] viewportScissor  Enables the scissor rectangle that setViewportScissor() specified.
			  @see setViewportScissor()
			  @cmdsize 3
*/
void setScanModeControl(Gnm::ScanModeControlAa msaa, Gnm::ScanModeControlViewportScissor viewportScissor)
{
	return m_dcb.setScanModeControl(msaa, viewportScissor);
}

/** @brief Controls Multisampling aliasing.
				This function will roll the hardware context.
				@param[in] logNumSamples				The number of samples to use for MSAA.
				@param[in] maxSampleDistance			The distance (in subpixels) from the pixel center to the outermost sample location.
				           Pass zero if <c><i>logNumSamples</i></c> is kNumSamples1. The valid range is <c>[0..15]</c>.
				@cmdsize 3
*/
void setAaSampleCount(Gnm::NumSamples logNumSamples, uint32_t maxSampleDistance)
{
	return m_dcb.setAaSampleCount(logNumSamples, maxSampleDistance);
}

/** @brief Specifies how often the PS shader is run: once per pixel, or once per sample.
				This function will roll the hardware context.
				@param[in] rate The PS shader execution rate.
				@sa Gnm::DepthEqaaControl::setPsSampleIterationCount(), setPsShaderSampleExclusionMask()
				@cmdsize 3
*/
void setPsShaderRate(Gnm::PsShaderRate rate)
{
	return m_dcb.setPsShaderRate(rate);
}

/** @brief Specifies sample locations that are not to trigger a PS shader invocation, even when covered by a rasterized primitive.
				@param[in] mask If bit N of this mask is set, sample location N will not trigger a PS shader invocation if a primitive covers it.
				@note The value passed to setAaSampleCount() limits the total number of samples per pixel.
				@note setAaSampleLocationControl() specifies the mapping of sample indices to sample locations.
				@note This feature affects PS shader invocation only; it does not affect Z/stencil processing.
				@note This feature takes effect only when the Ps shader is configured for "EarlyZ." (For details, see PsStageRegisters::getExpectedGpuZBehavior().)
				      Any other PsZBehavior will force the GPU into legacy mode, in which every covered sample triggers a PS shader invocation.
				@sa setPsShaderRate()
				@note  NEO mode only
				@cmdsize 3
*/
void setPsShaderSampleExclusionMask(uint16_t mask)
{
	return m_dcb.setPsShaderSampleExclusionMask(mask);
}

/** @brief Specifies a set of adjustment factors that the GPU uses when computing texture gradients.
			
	The factors are specified as unsigned 2.6 fixed-point values. See Gnmx::convertF32ToU2_6() for simple conversion code.
	The adjusted gradients are computed using the following expressions:  

	@image html matrixmult.jpg width=325px
	  
	| d[uvw]_dx'|    | factor00 factor01 |   | d[uvw]_dx |
	|           | =  |                   | * |           |
	| d[uvw]_dy'|    | factor10 factor11 |   | d[uvw]_dy |
	@param[in] factor00 Adjustment factor applied to du_dx, dv_dx, and dw_dx, to compute du_dx', dv_dx', and dw_dx'. Its format is unsigned 2.6 fixed-point.
	@param[in] factor01 Adjustment factor applied to du_dy, dv_dy, and dw_dy, to compute du_dx', dv_dx', and dw_dx'. Its format is unsigned 2.6 fixed-point.
	@param[in] factor10 Adjustment factor applied to du_dx, dv_dx, and dw_dx, to compute du_dy', dv_dy', and dw_dy'. Its format is unsigned 2.6 fixed-point.
	@param[in] factor11 Adjustment factor applied to du_dy, dv_dy, and dw_dy, to compute du_dy', dv_dy', and dw_dy'. Its format is unsigned 2.6 fixed-point.
	@param[in] factor01sb When enabled, factor01 will be negated if directed by the shader SampleAdjust instruction.  Generally, this approach is used in shaders that read textures
				as part of writing depth. For example, in addition to the usual texture gradient matrix for samples that render color in this frame,
				alpha tests require a flipped texture gradient matrix for samples that do not render color in this frame.
	@param[in] factor10sb When enabled, factor10 will be negated if directed by the shader SampleAdjust instruction.  Generally, this approach is used in shaders that read textures
				as part of writing depth. For example, in addition to the usual texture gradient matrix for samples that render color in this frame,
				alpha tests require a flipped texture gradient matrix for samples that do not render color in this frame.
	@note  NEO mode only
	@cmdsize 3
*/
void setTextureGradientFactors(uint8_t factor00, uint8_t factor01, uint8_t factor10, uint8_t factor11, 
							   Gnm::TextureGradientFactor01SignNegationBehavior factor01sb, Gnm::TextureGradientFactor10SignNegationBehavior factor10sb)
{
	return m_dcb.setTextureGradientFactors(factor00, factor01, factor10, factor11, factor01sb, factor10sb);
}

/** @brief Specifies a set of adjustment factors used by the GPU when computing texture gradients.
	
	The factors are specified as unsigned 2.6 fixed-point values. See Gnmx::convertF32ToU2_6() for simple conversion code.
	The adjusted gradients are computed using the following expressions:  

	@image html matrixmult.jpg width=325px
	  
	| d[uvw]_dx'|    | factor00 factor01 |   | d[uvw]_dx |
	|           | =  |                   | * |           |
	| d[uvw]_dy'|    | factor10 factor11 |   | d[uvw]_dy |
	@param[in] factor00 Adjustment factor applied to du_dx, dv_dx, and dw_dx, to compute du_dx', dv_dx', and dw_dx'. Its format is unsigned 2.6 fixed-point.
	@param[in] factor01 Adjustment factor applied to du_dy, dv_dy, and dw_dy, to compute du_dx', dv_dx', and dw_dx'. Its format is unsigned 2.6 fixed-point.
	@param[in] factor10 Adjustment factor applied to du_dx, dv_dx, and dw_dx, to compute du_dy', dv_dy', and dw_dy'. Its format is unsigned 2.6 fixed-point.
	@param[in] factor11 Adjustment factor applied to du_dy, dv_dy, and dw_dy, to compute du_dy', dv_dy', and dw_dy'. Its format is unsigned 2.6 fixed-point.
	@note  NEO mode only
	@cmdsize 3
*/
SCE_GNM_API_CHANGED
void setTextureGradientFactors(uint8_t factor00, uint8_t factor01, uint8_t factor10, uint8_t factor11)
{
	return m_dcb.setTextureGradientFactors(factor00, factor01, factor10, factor11);
}

/** @brief Sets the render override control.
				This function will roll the hardware context.
				@param[in] renderOverrideControl The render override control state.
				@cmdsize 3
*/
void setRenderOverrideControl(Gnm::RenderOverrideControl renderOverrideControl)
{
	return m_dcb.setRenderOverrideControl(renderOverrideControl);
}

/** @brief Sets the render override2 control.
				This function will roll the hardware context.
				@param[in] renderOverride2Control The render override2 control state.
				@cmdsize 3
*/
void setRenderOverride2Control(Gnm::RenderOverride2Control renderOverride2Control)
{
	return m_dcb.setRenderOverride2Control(renderOverride2Control);
}

/** @brief Sets the multisample AA mask.
				This function will roll the hardware context.
			  @param[in] mask 	Mask is a 64-bit quantity that is treated as 4 16-bit masks. LSB is Sample0, MSB is Sample15.
			  						The 4 masks are applied to each 2x2 screen-aligned pixels as follows:
									– Upper Left Corner   15:0
									– Upper Right Corner 31:16
									– Lower Left Corner  47:32
									– Lower Right Corner 63:48
			  @cmdsize 6
*/
void setAaSampleMask(uint64_t mask)
{
	return m_dcb.setAaSampleMask(mask);
}

/** @brief Sets the sample locations and centroid priority as configured in the provided AaSampleLocationControl object.
				This function will roll the hardware context.
				@param[in] control The sample locations and centroid priority will be set from the values stored in this object.
				@cmdsize 22
*/
void setAaSampleLocationControl(const Gnm::AaSampleLocationControl * control)
{
	return m_dcb.setAaSampleLocationControl(control);
}

/** @brief Specifies the width of the line.
			This function will roll the hardware context.
			@param[in] widthIn8ths    The width of the line in 1/8ths of a pixel.
			@cmdsize 3
*/
void setLineWidth(uint16_t widthIn8ths)
{
	return m_dcb.setLineWidth(widthIn8ths);
}

/** @brief Specifies the dimensions of the point primitives.
			This function will roll the hardware context.
			@param[in] halfWidth    Half width (horizontal radius) of point; fixed point (12.4), 12 bits integer, 4 bits fractional pixels.
			@param[in] halfHeight   Half height (vertical radius) of point; fixed point (12.4), 12 bits integer, 4 bits fractional pixels.
			@cmdsize 3
*/
void setPointSize(uint16_t halfWidth, uint16_t halfHeight)
{
	return m_dcb.setPointSize(halfWidth, halfHeight);
}

/** @brief Specifies the minimum and maximum radius of point primitives and point sprites.
				This function will roll the hardware context.
				@param[in] minRadius Minimum radius; fixed point (12.4), 12 bits integer, 4 bits fractional pixels.
				@param[in] maxRadius Maximum radius; fixed point (12.4), 12 bits integer, 4 bits fractional pixels.
				@cmdsize 3
*/
void setPointMinMax(uint16_t minRadius, uint16_t maxRadius)
{
	return m_dcb.setPointMinMax(minRadius, maxRadius);
}

/** @brief Sets the clamp value for polygon offset.
				This function will roll the hardware context.
				@param[in] clamp Specifies the maximum (if <c><i>clamp</i></c> is positive) or minimum (if <c><i>clamp</i></c> is negative) value clamp for the polygon offset result.
				@note Polygon offset is <c>max(|dzdx|,|dzdy|) * scale + offset * 2^(exponent(max_z_in_primitive) - mantissa_bits_in_z_format)</c>, with clamp applied.
				@see setPolygonOffsetZFormat(), setPolygonOffsetFront(), setPolygonOffsetBack(), Gnm::PrimitiveSetup::setPolygonOffsetEnable()
				@cmdsize 3
*/
void setPolygonOffsetClamp(float clamp)
{
	return m_dcb.setPolygonOffsetClamp(clamp);
}

/** @brief Sets information about the Z-buffer format needed for polygon offset.
				This function will roll the hardware context.
				@param[in] format Z-buffer format.
				@see setPolygonOffsetClamp(), setPolygonOffsetFront(), setPolygonOffsetBack(), Gnm::PrimitiveSetup::setPolygonOffsetEnable()
				@cmdsize 3
*/
void setPolygonOffsetZFormat(Gnm::ZFormat format)
{
	return m_dcb.setPolygonOffsetZFormat(format);
}

/** @brief Sets the front face polygon scale and offset.
				This function will roll the hardware context.
				@param[in] scale   Specifies polygon-offset scale for front-facing polygons; 32-bit IEEE <c>float</c> format. Scale must be specified in 1/16th units (e.g. pass <c>16.0f</c> for a scale of <c>1.0</c>).
				@param[in] offset  Specifies polygon-offset offset for front-facing polygons; 32-bit IEEE <c>float</c> format.
				@note Polygon offset is <c>max(|dzdx|,|dzdy|) * scale + offset * 2^(exponent(max_z_in_primitive) - mantissa_bits_in_z_format)</c>, with clamp applied.
				@see setPolygonOffsetClamp(), setPolygonOffsetZFormat(), setPolygonOffsetBack(), Gnm::PrimitiveSetup::setPolygonOffsetEnable()
				@cmdsize 4
*/
void setPolygonOffsetFront(float scale, float offset)
{
	return m_dcb.setPolygonOffsetFront(scale, offset);
}

/** @brief Sets the back face polygon scale and offset.
				This function will roll the hardware context.
				@param[in] scale   Specifies polygon-offset scale for back-facing polygons; 32-bit IEEE float format. Scale must be specified in 1/16th units (e.g. pass 16.0f for a scale of 1.0).
				@param[in] offset  Specifies polygon-offset offset for back-facing polygons; 32-bit IEEE float format.
				@note Polygon offset is <c>max(|dzdx|,|dzdy|) * scale + offset * 2^(exponent(max_z_in_primitive) - mantissa_bits_in_z_format)</c>, with clamp applied.
				@see setPolygonOffsetClamp(), setPolygonOffsetZFormat(), setPolygonOffsetFront(), Gnm::PrimitiveSetup::setPolygonOffsetEnable()
				@cmdsize 4
*/
void setPolygonOffsetBack(float scale, float offset)
{
	return m_dcb.setPolygonOffsetBack(scale, offset);
}

/** @brief Sets the hardware screen offset to center guard band.
				This function will roll the hardware context.
				@param[in] offsetX The hardware screen offset in X from 0 to 8176. This is specified in units of 16 pixels and the range is <c>[0..508]</c> in increments of 4.
				                   A value of 200 would correspond to an offset of 3200 pixels.
				@param[in] offsetY The hardware screen offset in Y from 0 to 8176. This is specified in units of 16 pixels and the range is <c>[0..508]</c> in increments of 4.
				                   A value of 200 would correspond to an offset of 3200 pixels.
				@sa setGuardBands()
				@cmdsize 3
*/
void setHardwareScreenOffset(uint32_t offsetX, uint32_t offsetY)
{
	return m_dcb.setHardwareScreenOffset(offsetX, offsetY);
}

/** @brief Sets the size of the clip and discard guard bands.
				This function will roll the hardware context.
				@param[in] horzClip Adjusts the horizontal clipping guard band size. 32-bit floating point value greater than or equal to 1.0f.
				@param[in] vertClip Adjusts the vertical clipping guard band size. 32-bit floating point value greater than or equal to 1.0f.
				@param[in] horzDiscard Adjusts the horizontal discard guard band size. 32-bit floating point value >= 1.0f.
				@param[in] vertDiscard Adjusts the vertical discard guard band size. 32-bit floating point value >= 1.0f.
				@note The parameter values are multipliers on the w term of the vertex coordinates. Set them to 1.0f to disable the corresponding guard band.
				@sa setHardwareScreenOffset()
				@cmdsize 6
*/
void setGuardBands(float horzClip, float vertClip, float horzDiscard, float vertDiscard)
{
	return m_dcb.setGuardBands(horzClip, vertClip, horzDiscard, vertDiscard);
}

/** @brief Sets the two instance step rates that the hardware uses to divide the instance ID by.
				The instance is divided by these two step rates and the result is provided in registers that can be used in the fetch shader.
				This function will roll the hardware context.
				@param[in] step0  Instance step rate 0.
				@param[in] step1  Instance step rate 1.
				@cmdsize 4
*/
void setInstanceStepRate(uint32_t step0, uint32_t step1)
{
	return m_dcb.setInstanceStepRate(step0, step1);
}

/** @brief Sets Pixel Shader interpolator settings for each of the parameters.
				This function will roll the hardware context.
				@param[in] inputTable    Array containing the various interpolator settings. This pointer must not be <c>NULL</c>.
				@param[in] numItems      Number of items in the table. Must be in the range <c>[1..32]</c>.
				@note This is set automatically by Gnmx::ConstantUpdateEngine::setPsShader.
				@cmdsize 2+numItems
*/
void setPsShaderUsage(const uint32_t * inputTable, uint32_t numItems)
{
	return m_dcb.setPsShaderUsage(inputTable, numItems);
}

/** @brief Informs the hardware of the maximum number of ES and GS threads that can be launched per on-chip GS sub-group.
				An on-chip GS sub-group consists of a set of ES, GS and VS wavefronts that share a single LDS allocation.
				
				Use this function when on-chip GS mode is enabled with setGsMode().
			    @param[in] esVerticesPerSubGroup			The number of ES vertices (i.e. threads) needed to create the GS primitives specified in <c><i>gsInputPrimitivesPerSubGroup</i></c> based on the input primitive type. Must be in the range [0..2047].
				@param[in] gsInputPrimitivesPerSubGroup		The number of GS primitives (i.e. threads) that can fit in LDS. This is based LDS usage for input vertices plus all output streams. Must be in the range [0..2047].
				@see setGsMode(), setGsModeOff(), computeOnChipGsConfiguration(), setOnChipGsVsLdsLayout()
				@cmdsize 3
*/
SCE_GNM_API_DEPRECATED(setGsOnChipControl)
void setGsOnChipControl(uint32_t esVerticesPerSubGroup, uint32_t gsInputPrimitivesPerSubGroup)
{
	return m_dcb.setGsOnChipControl(esVerticesPerSubGroup, gsInputPrimitivesPerSubGroup);
}

/** @brief Update the shader registers for the PS shader stage.
				It will only set the shader's GPU address as well as <c><i>m_spiShaderPgmRsrc1Ps</i></c> and <c><i>m_spiShaderPgmRsrc2Ps</i></c>.
				It is important to make sure the other registers are already properly set; otherwise the behavior is undefined.
				This function never rolls the hardware context.
				@param[in] psRegs       A pointer to the structure containing the memory address (256-byte aligned) of the shader code and additional registers to set as
											determined by the shader compiler.
				@cmdsize 40
*/
void updatePsShader(const Gnm::PsStageRegisters * psRegs)
{
	return m_dcb.updatePsShader(psRegs);
}

/** @brief Updates the shader registers for the VS shader stage.
				It will only set the shader's GPU address as well as <c><i>m_spiShaderPgmRsrc1Vs</i></c> and <c><i>m_spiShaderPgmRsrc2Vs</i></c>.
				It is important to make sure the other registers are already properly set; otherwise the behavior is undefined.
				This function never rolls the hardware context.
				@param[in] vsRegs         A pointer to the structure containing the memory address (256-byte aligned) of the shader code and additional registers to set as determined by the shader compiler.
				@param[in] shaderModifier The shader modifier value generated by generateVsFetchShaderBuildState(); use 0 if no fetch shader.
				@cmdsize 29
*/
void updateVsShader(const Gnm::VsStageRegisters * vsRegs, uint32_t shaderModifier)
{
	return m_dcb.updateVsShader(vsRegs, shaderModifier);
}

/** @brief Update the shader registers for the GS shader stage.
				
				It will only set the shader's GPU address as well as <c><i>m_spiShaderPgmRsrc1Gs</i></c> and <c><i>m_spiShaderPgmRsrc2Gs</i></c>.
				It is important to make sure the other registers are already properly set; otherwise the behavior is undefined.
				This function never rolls the hardware context.
				@param[in] gsRegs      A pointer to structure containing memory address (256-byte-aligned) of the shader code and additional registers to set as determined by the shader compiler.
				@sa setGsMode()
				@cmdsize 29
*/
void updateGsShader(const Gnm::GsStageRegisters * gsRegs)
{
	return m_dcb.updateGsShader(gsRegs);
}

/** @brief Update the shader registers for the HS shader stage.
				
				It will only set the shader's GPU address as well as <c><i>m_spiShaderPgmRsrc1Hs</i></c> and <c><i>m_spiShaderPgmRsrc2Hs</i></c>.
				It is important to make sure the other registers are already properly set; otherwise the behavior is undefined.
				This function never rolls the hardware context.
				@param[in] hsRegs       A pointer to the structure containing memory address (256-byte aligned) of the shader code and additional registers to set as determined by the shader compiler.
				@param[in] tessRegs     A pointer to the structure containing tessellation registers.
				@cmdsize 30
*/
void updateHsShader(const Gnm::HsStageRegisters * hsRegs, const Gnm::TessellationRegisters * tessRegs)
{
	return m_dcb.updateHsShader(hsRegs, tessRegs);
}

/** @brief Sets the address of the border color table.
				This table is unique for each graphics context. It should contain no more than 4096 entries,
				each of which is a 16-byte float4 in RGBA channel order. The total maximum size of the table is thus 64 KB. It is recommended that the entire
				64 KB be allocated as a precaution even if unused, as any reference by a sampler to an out-of-bounds table entry will most likely crash the GPU.
				This function will roll the hardware context.
*  @param[in] tableAddr 256-byte-aligned address of the table of border colors.
*  @cmdsize 3
*/
void setBorderColorTableAddr(void * tableAddr)
{
	return m_dcb.setBorderColorTableAddr(tableAddr);
}

/**
* @brief Reads data from global data store (GDS)and writes it to the specified GPU address.
			 	This function never rolls the hardware context.
			 
				@param[in]  eventType			Specifies the event used to trigger the GDS read.
				@param[out] dstGpuAddr			The address to which this function writes the GDS data that it reads. This pointer must be 4-byte aligned and must not be <c>NULL</c>.
				@param[in]  gdsOffsetInDwords	The <c>DWORD</c> offset into GDS to read from.
				@param[in]  gdsSizeInDwords		The number of <c>DWORD</c>s to read.
				
				@note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes. Reads beyond this range will simply return 0.
						When using this function, the data transfer is not performed by the CP or CP DMA, but rather it is done by the GPU back
						end since this function is actually implemented as an “end-of-shader write” event. This means the CP will not wait for the
						transfer to start or end before moving on with the processing of further commands. However, this function conveniently
						will wait on previously executing compute or pixel processing before initiating the transfer of data from GDS.
				
				@cmdsize 5
*/
void readDataFromGds(Gnm::EndOfShaderEventType eventType, void * dstGpuAddr, uint32_t gdsOffsetInDwords, uint32_t gdsSizeInDwords)
{
	return m_dcb.readDataFromGds(eventType, dstGpuAddr, gdsOffsetInDwords, gdsSizeInDwords);
}

/** @brief Allocates space for user data directly inside the command buffer and returns a CPU pointer to the space.
				This function never rolls the hardware context.
			  @param[in] sizeInBytes  Size of the data in bytes. Note that the maximum size of a single command packet is 2^16 bytes,
			                      and the effective maximum value of <c><i>sizeInBytes</i></c> will be slightly less than that due to packet headers
								  and padding.
			  @param[in] alignment    Alignment of the pointer from the start of memory, not from the start of the command buffer.
			  @return Returns a pointer to the memory just allocated. If the requested size is larger than the maximum packet size (64 KB),
			          or if there is insufficient memory remaining in the command buffer, this function returns 0.
			  @cmdsize 2 + (sizeInBytes + sizeof(uint32_t) - 1)/sizeof(uint32_t) + (uint32_t)(1<<alignment)/sizeof(uint32_t)
*/
void * allocateFromCommandBuffer(uint32_t sizeInBytes, Gnm::EmbeddedDataAlignment alignment)
{
	return m_dcb.allocateFromCommandBuffer(sizeInBytes, alignment);
}

/** @brief Copies several contiguous <c>DWORD</c>s into the user data registers of the specified shader stage.
* 
* 	@param[in] stage				The shader stage whose user data registers should be updated.
*	@param[in] startUserDataSlot			The first slot to write. The valid range is <c>[0..15]</c>.
*	@param[in] userData				The source data to copy into the user data registers. If <c><i>numDwords</i></c> is greater than 0, this pointer must not be <c>NULL</c>.
*	@param[in] numDwords				The number of <c>DWORD</c>s to copy into the user data registers. The sum of <c><i>startUserDataSlot</i></c> and <c><i>numDwords</i></c> must not exceed 16.
*
*	@cmdsize 4+numDwords
*/
void setUserDataRegion(Gnm::ShaderStage stage, uint32_t startUserDataSlot, const uint32_t * userData, uint32_t numDwords)
{
	return m_dcb.setUserDataRegion(stage, startUserDataSlot, userData, numDwords);
}

/** @brief Sets the depth render target.
			This function will roll the hardware context.
			@note If the previously set depth render target has the same read/write address, this function might have no effect and retain the previous target even though some other
			parameters could have changed in the new target.
			@param[in] depthTarget  Pointer to a Gnm::DepthRenderTarget struct. To disable the depth buffer, pass <c>NULL</c> as this value.
			@cmdsize ( depthTarget && (depthTarget->getZReadAddress256ByteBlocks() || depthTarget->getStencilReadAddress256ByteBlocks()) ) ? 24 : 6
*/
void setDepthRenderTarget(Gnm::DepthRenderTarget const * depthTarget)
{
	return m_dcb.setDepthRenderTarget(depthTarget);
}

/** @brief Sets the depth clear value.
				When depth clears are enabled, all depth buffer/HTILE reads and writes are replaced by the <c><i>clearValue</i></c> value.
				This function will roll the hardware context.
				@param[in] clearValue	A 32-bit floating point value that must be in the range of 0.0 to 1.0.
				@see Gnm::DbRenderControl::setDepthClearEnable()
				@cmdsize 3
*/
void setDepthClearValue(float clearValue)
{
	return m_dcb.setDepthClearValue(clearValue);
}

/** @brief Sets the stencil clear value.
				Stencil value used when an HTILE buffer entry's <c>SMEM</c> field is 0, which specifies that the tile is cleared to background stencil values.
				This function will roll the hardware context.
				@param[in] clearValue	Sets the 8-bit stencil clear value.
				@see Gnm::DbRenderControl::setStencilClearEnable()
				@cmdsize 3
*/
void setStencilClearValue(uint8_t clearValue)
{
	return m_dcb.setStencilClearValue(clearValue);
}

/** @deprecated CMASK clear colors are now stored in the RenderTarget, and are set automatically when the target is bound. See RenderTarget::setCmaskClearColor().
				@brief Sets the fast clear color for the corresponding render target slot.
				This function will roll the hardware context.
				@param[in] rtSlot			The slot index of the Gnm::RenderTarget for which the fast clear color is to be set <c>(0..7)</c>.
				@param[in] clearColor		The fast clear color. See the description for which bits of <c>clearColor[0]</c> and <c>clearColor[1]</c> are used for each bit depth.
				           The target surface's DataFormat is used to interpret the relevant bits as a color -- for example, a 32-bit BGRA8888 surface would interpret
						   <c>clearColor[0]</c> bits <c>[31:0]</c> as a 32-bit BGRA color value. Refer to the #Gnm::SurfaceFormat enum description for details on the layout of each format.
				
				<table>
				<tr><td>	  Pixel size       </td><td>          Relevant bits                               </td></tr>
				<tr><td>	    8 bpp          </td><td>         <c>clearColor[0]</c> bits <c>[7:0]</c></td></tr>
				<tr><td>	   16 bpp         </td><td>          <c>clearColor[0]</c> bits <c>[15:0]</c></td></tr>
				<tr><td>	   32 bpp         </td><td>          <c>clearColor[0]</c> bits <c>[31:0]</c></td></tr>
				<tr><td>	   64 bpp         </td><td>          <c>clearColor[0]</c> and <c>clearColor[1]</c> bits <c>[31:0]</c></td></tr>
				<tr><td>	  128 bpp         </td><td>          Unsupported</td></tr>
				</table>
				
				@see Gnm::SurfaceFormat
				@cmdsize 0
*/
SCE_GNM_API_DEPRECATED_MSG("CMASK clear colors are now stored in the RenderTarget, and set automatically when the target is bound. See RenderTarget::setCmaskClearColor().")
void setCmaskClearColor(uint32_t rtSlot, const uint32_t clearColor[2])
{
	return m_dcb.setCmaskClearColor(rtSlot, clearColor);
}

/** @brief Controls which of the color channels are written into the color surface for the eight Gnm::RenderTarget slots.
				This function will roll the hardware context.
				@param[in] mask        Contains color channel mask fields for writing to Gnm::RenderTarget slots. <c><i>mask</i></c> is treated as a set of eight 4-bit values:
										one for each corresponding render target slot starting from the low bits. Red, green, blue and alpha are channels 0, 1, 2 and 3 in the pixel shader and are
										enabled by bits 0, 1, 2 and 3 in each field. The low order bit corresponds to the red channel. A zero bit disables writing
										to that channel and a one bit enables writing to that channel.
				@note The channels may be in a different order in the frame buffer, depending on the Gnm::RenderTargetChannelOrder
				field; the bits in <c><i>mask</i></c> correspond to the order of channels after blending and before Gnm::RenderTargetChannelOrder is applied.
				When rendering to MRTs, pixel shader exports must be contiguous across the first and last MRT index being used. For example, if the application is rendering only to MRT1 and MRT3, the pixel shader must still export a value to MRT2; this is required even if the render target mask for MRT2 has been set to 0, and/or the render target slot for MRT2 has been set to <c>NULL</c>.
				@see Gnm::RenderTargetChannelOrder
				@cmdsize 3
*/
void setRenderTargetMask(uint32_t mask)
{
	return m_dcb.setRenderTargetMask(mask);
}

/** @brief Sets the blending settings for the specified render target slot.
				This function will roll the hardware context.
				@param[in] rtSlot Slot index of the render target to which the new blend settings should be applied <c>[0..7]</c>.
				@param[in] blendControl The new blend settings to apply.
				@cmdsize 3
*/
void setBlendControl(uint32_t rtSlot, Gnm::BlendControl blendControl)
{
	return m_dcb.setBlendControl(rtSlot, blendControl);
}

/** @brief Sets the channels of the constant blend color.
				Certain Gnm::BlendMultiplier values refer to a "constant color" or "constant alpha"; this function specifies these constants.
				This function will roll the hardware context.
				@param[in] red        Red channel of constant blend color as a float.
				@param[in] green      Green channel of constant blend color as a float.
				@param[in] blue       Blue channel of constant blend color as a float.
				@param[in] alpha      Alpha channel of constant blend color as a float.
				@see Gnm::BlendMultiplier
				@cmdsize 6
*/
void setBlendColor(float red, float green, float blue, float alpha)
{
	return m_dcb.setBlendColor(red, green, blue, alpha);
}

/** @brief Sets the stencil test value, stencil mask, stencil write mask and stencil op value for front- and back-facing primitives together.
				This function will roll the hardware context.
				@param[in] stencilControl Structure in which to specify a mask for stencil buffer values on read and on write and values for stencil test and stencil operation.
				@see Gnm::DepthStencilControl::setStencilFunction(), Gnm::StencilOpControl::setStencilOps()
				@cmdsize 4
*/
void setStencil(Gnm::StencilControl stencilControl)
{
	return m_dcb.setStencil(stencilControl);
}

/** @brief Sets the stencil reference value, stencil mask, stencil write mask and stencil operation value for front- and back-facing primitives separately.
				This function will roll the hardware context.
				@param[in] front	Specifies the stencil test and operation values and read and write masks for front-facing primitives.
				@param[in] back     Specifies the stencil test and operation values and read and write masks for back-facing primitives.
				@see Gnm::DepthStencilControl::setStencilFunctionBack(), Gnm::DepthStencilControl::setSeparateStencilEnable(), Gnm::StencilOpControl::setStencilOpsBack(),
				@cmdsize 4
*/
void setStencilSeparate(Gnm::StencilControl front, Gnm::StencilControl back)
{
	return m_dcb.setStencilSeparate(front, back);
}

/** @brief Controls alpha-to-mask and sets the dither values if desired.
				
				This function will roll the hardware context.
				
				@param[in] alphaToMaskControl The alpha-to-mask control register.
				@cmdsize 3
*/
void setAlphaToMaskControl(Gnm::AlphaToMaskControl alphaToMaskControl)
{
	return m_dcb.setAlphaToMaskControl(alphaToMaskControl);
}

/** @brief Controls HTILE stencil 0.
				HTILE stencil (known as Hi-S) is like a mini stencil buffer, stored as a "may pass" and "may fail" bit in each HTILE buffer entry.
				Stencil buffer writes for which the "may pass" or "may fail" results can be determined for an entire tile will cause HTILE stencil to be updated.
				Subsequent stencil buffer tests for which results are logically deducible from HTILE stencil can be tile-wise trivially accepted or rejected.
				The application workflow is as follows:
				<ol>
					<li> Set HTILE stencil and clear stencil buffer. Do not change HTILE stencil until before next clear.</li>
					<li> Write to the stencil buffer in ways that maximize HTILE stencil's ability to tile-wise accelerate later stencil tests.</li>
					<li> Render with stencil testing in ways that maximize HTILE stencil's ability to trivially accept or reject tiles.</li>
				</ol>
				@param[in] htileStencilControl Specifies an HTILE stencil for both front- and back-facing primitives.
				@cmdsize 3
*/
void setHtileStencil0(Gnm::HtileStencilControl htileStencilControl)
{
	return m_dcb.setHtileStencil0(htileStencilControl);
}

/** @brief Controls HTILE stencil 1.
				HTILE stencil (known as Hi-S) is like a mini stencil buffer, stored as a "may pass" and "may fail" bit in each HTILE buffer entry.
				Stencil buffer writes for which the "may pass" or "may fail" results can be determined for an entire tile will cause HTILE stencil to be updated.
				Subsequent stencil buffer tests for which results are logically deducible from HTILE stencil can be tile-wise trivially accepted or rejected.
				The application workflow is as follows:
				<ul>
				<li>Set HTILE stencil and clear stencil buffer. Do not change HTILE stencil until before next clear.</li>
				<li>Write to the stencil buffer in ways that maximize HTILE stencil's ability to tile-wise accelerate later stencil tests.</li>
				<li>Render with stencil testing in ways that maximize HTILE stencil's ability to trivially accept or reject tiles.</li>
				</ul>
				@param[in] htileStencilControl Specifies an HTILE stencil for both front- and back-facing primitives.
				@cmdsize 3
*/
void setHtileStencil1(Gnm::HtileStencilControl htileStencilControl)
{
	return m_dcb.setHtileStencil1(htileStencilControl);
}

/** @brief Controls general CB behavior across all render targets.
				This function will roll the hardware context.
				@param[in] mode This field selects standard color processing or one of several major operation modes.
				@param[in] op This field specifies the Boolean bitwise operation to apply to the source (shader output) and destination
			            (the color buffer).
				@note The value of op must be set to <c>kRasterOpCopy</c> if blending is enabled with <c>BlendControl::setBlendEnable()</c>
				@see Gnm::RasterOp
				@cmdsize 3
*/
void setCbControl(Gnm::CbMode mode, Gnm::RasterOp op)
{
	return m_dcb.setCbControl(mode, op);
}

/** @brief Writes the Gnm::DepthStencilControl, which controls depth and stencil tests.
				This function will roll the hardware context.
				@param[in] depthControl   Value to write to the Gnm::DepthStencilControl register.
				@see setDepthStencilDisable()
				@cmdsize 3
*/
void setDepthStencilControl(Gnm::DepthStencilControl depthControl)
{
	return m_dcb.setDepthStencilControl(depthControl);
}

/** @brief Convenient alternative to setDepthStencilControl(), which disables depth/stencil writes and depth/stencil tests.
				This function will roll the hardware context.
				@see setDepthStencilControl()
				@cmdsize 3
*/
void setDepthStencilDisable()
{
	return m_dcb.setDepthStencilDisable();
}

/** @brief Sets the minimum and maximum values used by the depth bounds test.
				This test must be enabled using Gnm::DepthStencilControl::setDepthBoundsEnable().
				If the test is disabled, or if no depth render target is bound, these values are ignored.
				This function will roll the hardware context.
				@param[in] depthBoundsMin The minimum depth that will pass the depth bounds test. Must be less than or equal to <c><i>depthBoundsMax</i></c>. [range: 0..1].
				@param[in] depthBoundsMax The maximum depth that will pass the depth bounds test. Must be greater than <c><i>depthBoundsMin</i></c>. [range: 0..1].
				@see Gnm::DepthStencilControl::setDepthBoundsEnable()
				@cmdsize 4
*/
void setDepthBoundsRange(float depthBoundsMin, float depthBoundsMax)
{
	return m_dcb.setDepthBoundsRange(depthBoundsMin, depthBoundsMax);
}

/** @brief Writes the Gnm::StencilOpControl object, which controls stencil operations.
				This function will roll the hardware context.
				@param[in] stencilControl   Value to write to the Gnm::StencilOpControl register.
				@cmdsize 3
*/
void setStencilOpControl(Gnm::StencilOpControl stencilControl)
{
	return m_dcb.setStencilOpControl(stencilControl);
}

/** @brief Controls enabling depth and stencil clear and configuring various parameters for depth and stencil copy.
				This function will roll the hardware context.
				@param[in] reg      Value to write to the Gnm::DbRenderControl register.
				@cmdsize 3
*/
void setDbRenderControl(Gnm::DbRenderControl reg)
{
	return m_dcb.setDbRenderControl(reg);
}

/** @brief Configures the ZPass count behavior.
				This function will roll the hardware context.
				@param[in] perfectZPassCounts If enabled, ZPass counts are forced to be accurate (by disabling no-op culling optimizations which could otherwise lead incorrect counts).
										This is usually enabled when issuing an occlusion query.
				@param[in] log2SampleRate     Sets how many samples are counter for ZPass counts. Normally set to the number of anti-aliased samples.
				@cmdsize 3
*/
void setDbCountControl(Gnm::DbCountControlPerfectZPassCounts perfectZPassCounts, uint32_t log2SampleRate)
{
	return m_dcb.setDbCountControl(perfectZPassCounts, log2SampleRate);
}

/** @brief Sets the depth EQAA parameters.
			This function will roll the hardware context.
			@param[in] depthEqaa  Value to write to the DB EQAA register.
			@cmdsize 3
*/
void setDepthEqaaControl(Gnm::DepthEqaaControl depthEqaa)
{
	return m_dcb.setDepthEqaaControl(depthEqaa);
}

/** @brief Enables primitive ID generation which is incremented for each new primitive.
				This function will roll the hardware context.
				@param[in] enable Enables or disables primitive ID generation.
				@cmdsize 3
*/
void setPrimitiveIdEnable(bool enable)
{
	return m_dcb.setPrimitiveIdEnable(enable);
}

/** @brief Controls vertex reuse in the VGT (vertex geometry tessellator).
				Reuse is turned off for streamout and viewports.
				This function will roll the hardware context.
				@param[in] enable If <c>true</c>, VGT vertex reuse is enabled.
				@cmdsize 3
*/
void setVertexReuseEnable(bool enable)
{
	return m_dcb.setVertexReuseEnable(enable);
}

/** @brief Sets the index offset.
				This offset is added to every index rendered, including those generated by drawIndexAuto(). The default value is <c>0</c>.
				The <c>S_VERTEX_ID</c> value always includes this offset.
				@param[in] offset The offset to set.
				@cmdsize 3
*/
void setIndexOffset(uint32_t offset)
{
	return m_dcb.setIndexOffset(offset);
}

/** @brief Inserts an instanced, indexed draw call that sources object ID values from the <c>objectIdAddr</c> buffer.
				@param[in] indexCount		The number of entries to process from the index buffer.
				@param[in] instanceCount    The number of instances to render.
				@param[in] indexAddr		The address of the index buffer. Do not set to <c>NULL</c>.
				@param[in] objectIdAddr     The address of the object ID buffer. Do not set to <c>NULL</c>.
				@param[in] modifier A collection of values that modify the draw call operation in various ways. For a safe default, use <c>kDrawModifierDefault</c>.
				@note  NEO mode only
				@cmdsize 13
*/
void drawIndexMultiInstanced(uint32_t indexCount, uint32_t instanceCount, const void * indexAddr, const void * objectIdAddr, Gnm::DrawModifier modifier)
{
	return m_dcb.drawIndexMultiInstanced(indexCount, instanceCount, indexAddr, objectIdAddr, modifier);
}

/** @brief Inserts an instanced, indexed draw call that sources object ID values from the <c>objectIdAddr</c> buffer.
				@param[in] indexCount		The number of entries to process from the index buffer.
				@param[in] instanceCount    The number of instances to render.
				@param[in] indexAddr		The address of the index buffer. Do not set to <c>NULL</c>.
				@param[in] objectIdAddr     The address of the object ID buffer. Do not set to <c>NULL</c>.
				@note  NEO mode only
				@cmdsize 13
*/
void drawIndexMultiInstanced(uint32_t indexCount, uint32_t instanceCount, const void * indexAddr, const void * objectIdAddr)
{
	return m_dcb.drawIndexMultiInstanced(indexCount, instanceCount, indexAddr, objectIdAddr);
}

/** @brief Sets the number of elements in the index buffer. This count is used as an upper bound when fetching indices for indirect draw calls.
				If an indirect draw call attempts to fetch an element beyond these bounds, the fetch will not occur, and a value of 0 will be used instead.
				This function never rolls the hardware context.
				@note The consequence of setting this count lower than the actual index buffer size is that any read attempts beyond the provided count will be discarded and treated as if the elements were 0.
				      The consequence of setting this count higher than the actual index buffer size is that any read attempts beyond the provided count will read undefined data and possibly trigger a
					  memory access exception.
				@param[in] indexCount Count of indices in the index buffer.
				@note This command is not needed by non-indirect draw calls, which set the appropriate size automatically based on the draw parameters.
				@cmdsize 2
*/
void setIndexCount(uint32_t indexCount)
{
	return m_dcb.setIndexCount(indexCount);
}

/** @deprecated This function now requires a ShaderType argument.
				@brief Sets the buffer that contains the arguments for the following indirect calls: drawIndexIndirect(), drawIndirect() and dispatchIndirect().
				This function never rolls the hardware context.
				@param[in] indirectBaseAddr Address of the buffer containing arguments for use by the indirect draw/dispatch. Must be 8-byte aligned.
				@see drawIndexIndirect(), drawIndirect(), drawIndexIndirectCountMulti(), drawIndirectCountMulti(), dispatchIndirect()
				@cmdsize 4
*/
SCE_GNM_API_DEPRECATED_MSG("This function now takes a ShaderType argument.")
void setBaseIndirectArgs(void * indirectBaseAddr)
{
	return m_dcb.setBaseIndirectArgs(indirectBaseAddr);
}

/** @brief Sets the buffer that contains the arguments for the following indirect calls: drawIndexIndirect(), drawIndirect() and dispatchIndirect().
				This function never rolls the hardware context.
				@param[in] shaderType Specifies whether to use <c><i>indirectBaseAddr</i></c> for dispatchIndirect() or for drawIndexIndirect()/drawIndirect().
				@param[in] indirectBaseAddr Address of the buffer containing arguments for use by the indirect draw/dispatch. Must be 8-byte aligned.
				@see drawIndexIndirect(), drawIndirect(), drawIndexIndirectCountMulti(), drawIndirectCountMulti(), dispatchIndirect()
				@cmdsize 4
*/
void setBaseIndirectArgs(Gnm::ShaderType shaderType, void * indirectBaseAddr)
{
	return m_dcb.setBaseIndirectArgs(shaderType, indirectBaseAddr);
}

/** @brief Configures a GDS ordered append unit internal counter to enable special ring buffer counter handling of <c>ds_ordered_count</c> shader operations targeting a specific GDS address. 
				The GDS ordered append unit supports Gnm::kGdsAccessibleOrderedAppendCounters (currently 8) special allocation counters which, like GDS, are a 
				global resource that must be managed by the application.
				This method configures the GDS OA counter <c><i>oaCounterIndex</i></c> (<c>[0:7]</c>) to detect <c>ds_ordered_count</c> operations targeting an allocation 
				position counter at GDS address <c><i>gdsDwOffsetOfCounter</i></c>. These specific operations are then modified in order to prevent ring buffer 
				data overwrites in addition to the wavefront ordering already enforced by <c>ds_ordered_count</c> in general. The ring buffer total 
				size in arbitrary allocation units, specified by <c><i>spaceInAllocationUnits</i></c>, is initially stored in two internal GDS registers 
				associated with the GDS OA counter: <c>WRAP_SIZE</c>, which stays constant, and <c>SPACE_AVAILABLE</c> which subsequently varies as buffer 
				space is allocated and deallocated.
				For a given shader stage, the global ordering of <c>ds_ordered_count</c> operations is partially determined by an ordered wavefront 
				index, which increments as each new wavefront launches or optionally per-threadgroup for a CS stage, and an ordered operation 
				index which increments for a given wavefront (or threadgroup) each time a <c>ds_ordered_count</c> is issued with the <c>wave_release</c> 
				flag set. Using these two indices, <c>ds_ordered_count</c> operations are always processed in strict wavefront index order relative 
				to a given operation index and in strict operation index order relative to a given wavefront index. Note, however, that there is no order
				constraint between two wavefronts when one has the earlier wavefront index and the other has the earlier operation index; many wavefront
				indices may be executed for one operation index before any are executed for the following operation index. Alternatively, a single wavefront index may
				execute all of its operation indices before the following wavefront index executes any operations.
				Graphics pipe CS dispatches create wavefronts ordered by thread group ID and iterate over X in the inner loop, then Y, and then Z. 
				To enable generation of an ordered wavefront index passed to each wavefront, compute shaders must be dispatched with a method 
				that provides a <c>DispatchOrderedAppendMode</c> argument (<c>dispatchWithOrderedAppend()</c> or <c>dispatchDraw()</c> for example). The value of this 
				argument determines whether the index is incremented once per wavefront (<c>kDispatchOrderedAppendModeIndexPerWavefront</c>) or once 
				per threadgroup (<c>kDispatchOrderedAppendModeIndexPerThreadgroup</c>).
				VS wavefront launch order is simply in vertex order (post-reuse determination). VS stage generation of an ordered wavefront 
				index is enabled only by <c>dispatchDraw()</c>.
				PS wavefronts are launched as two independent streams on PlayStation®4 hardware with one from each shader engine. As a result, 
				<c>ds_ordered_count</c> operations for PS always implicitly split atomic operations between 2 sequential counters based on the source 
				shader engine index (referred to as <c>packer_id</c> in the GDS address description). 
				Any <c>ds_ordered_count</c> operation whose GDS <c>DWORD</c> address (<c>M0[16:31]>>2 + offset0>>2 + packer_id</c> for PS stage, or <c>M0[16:31]>>2 + offset0>>2</c> 
				for VS or CS stage) matches <c><i>gdsDwOffsetOfCounter</i></c> will be intercepted for special processing by the GDS ordered append unit. If the 
				current operation index (i.e. the number of previous wave releases issued by <c>ds_ordered_count</c> operations for this wavefront or 
				threadgroup) is equal to <c><i>oaOpIndex</i></c>, and the wavefront originated from the graphics shader stage <c><i>stage</i></c>, the operation is converted 
				into an allocation operation. If the operation matches the address but <c><i>oaOpIndex</i></c> or <c><i>stage</i></c> (or both) do not match, it is converted 
				to a deallocation operation. Operations matching the address but originating from asynchronous compute pipes are also converted 
				into deallocation operations.
				Any <c>ds_ordered_count</c> operation converted to an allocation operation will first wait until the internal <c>SPACE_AVAILABLE</c> register 
				value is larger than the requested allocation size before decrementing the internal register value. In addition, some specific 
				<c>ds_ordered_count</c> operations such as <c>WRAP</c> and <c>CONDXCHG32</c> receive some additional pre-processing of the allocation count to produce 
				source operands to the internal atomic operations. See the GPU Shader Core ISA documentation for more details about <c>ds_ordered_count</c>.
				Any <c>ds_ordered_count</c> operation converted to a deallocation operation will increment the internal <c>SPACE_AVAILABLE</c> register value 
				but does not update the target counter in GDS.
				Used together, these allocation and deallocation operations allow a shader running in one compute pipe or graphics stage to write 
				data into a ring buffer while another shader, running in a different compute pipe or graphics stage, consumes this data. Allocations 
				and writes are performed in wavefront launch order for the source pipe or stage, and reads and deallocations are performed in wavefront 
				launch order for the destination pipe or stage. Typically, the source pipe also writes to another counter in the GDS in wavefront launch 
				order to indicate the completion of writes, which requires another <c>ds_ordered_count</c> but not another OA counter.
				Dispatch draw requires one counter to be configured for the index ring buffer and, if enabled, a second counter to be configured for 
				the vertex ring buffer.

				@param[in] oaCounterIndex			The index of one of the 8 available GDS ordered append unit internal counters that should be configured; range <c>[0:7]</c>.
				@param[in] gdsDwOffsetOfCounter		The <c>DWORD</c> offset of the allocation offset counter in GDS which will be matched against the address used by <c>ds_ordered_count</c>, i.e. <c>M0[16:31]>>2 + offset0>>2 + packer_id</c>, where <c>packer_id</c> can be non-zero only for PS stage shaders.
				@param[in] stage					The graphic pipe shader stage to issue allocation operations. Only <c>kShaderStageCs</c>, <c>kShaderStagePs</c> and <c>kShaderStageVs</c> are supported.
				@param[in] oaOpIndex				The <c>ds_ordered_count</c> operation index which will be interpreted as an allocation operation if also issued from the matching graphic pipe shader stage.
				@param[in] spaceInAllocationUnits	The size of the ring buffer in arbitrary units (elements).
				
				@cmdsize 5
				@see disableOrderedAppendAllocationCounter()
*/
void enableOrderedAppendAllocationCounter(uint32_t oaCounterIndex, uint32_t gdsDwOffsetOfCounter, Gnm::ShaderStage stage, uint32_t oaOpIndex, uint32_t spaceInAllocationUnits)
{
	return m_dcb.enableOrderedAppendAllocationCounter(oaCounterIndex, gdsDwOffsetOfCounter, stage, oaOpIndex, spaceInAllocationUnits);
}

/** @brief Disables a GDS ordered append unit internal counter.
				@param[in] oaCounterIndex			The index of the GDS ordered append unit internal counter to disable; range [0:7].
				
				@cmdsize 5
				@see enableOrderedAppendAllocationCounter()
*/
void disableOrderedAppendAllocationCounter(uint32_t oaCounterIndex)
{
	return m_dcb.disableOrderedAppendAllocationCounter(oaCounterIndex);
}

/** @brief Inserts an occlusion query to count the number of pixels which have passed the depth test.
				Generally, two queries are necessary: one at the beginning and one at the end of the command sequence to measure.
				This function never rolls the hardware context.
				
				@param[in] queryOp			Specifies whether this query marks the beginning of a measurement or the end.
				@param[out] queryResults	Receives the ZPass counts.  If <c><i>queryOp</i></c> is kOcclusionQueryOpBegin or kOcclusionQueryOpClearAndBegin,
											the results are automatically reset to zero with a GPU DMA before the query is written. The contents of this address are described by the
											Gnm::OcclusionQueryResults structure. The contents of this object will be written by the GPU, so the object itself must be
											in GPU-visible memory. This pointer must not be <c>NULL</c>.
				
				@note If the CPU is waiting on query results, and if results objects are being reused within the same command buffer,
						using kOcclusionQueryOpBegin or kOcclusionQueryOpClearAndBegin can lead to a race condition. It is strongly recommended that query results be cleared to zero
						before submission, and that a separate results object is used for each query.
				@note The CP supports a maximum of two DMA transfers in flight concurrently. If a third DMA is issued, the CP will stall until one of the previous two completes. The following
				      Gnm commands are implemented using CP DMA transfers, and are thus subject to this restriction:
					  <ul>
						<li>dmaData()</li>
						<li>prefetchIntoL2()</li>
						<li>writeOcclusionQuery()</li>
					  </ul>
				
				@see setZPassPredicationEnable(), Gnm::OcclusionQueryResults, setDbCountControl()
				
				@cmdsize queryOp == sce::Gnm::kOcclusionQueryOpClearAndBegin ? 22 : 8
*/
void writeOcclusionQuery(Gnm::OcclusionQueryOp queryOp, Gnm::OcclusionQueryResults * queryResults)
{
	return m_dcb.writeOcclusionQuery(queryOp, queryResults);
}

/** @brief Enables and configures conditional rendering based on ZPass results.
				When ZPass predication is active, certain packets (such as draws) will be skipped based on the results of an occlusion / ZPass query.
				This function never rolls the hardware context.
				@param[in] queryResults The results of a previously-issued occlusion query. It is not necessary for the
										application to wait for the results to be ready before enabling predication.
										This pointer must not be <c>NULL</c>.
				@param[in] hint Indicates how to handle draw packets that occur before the appropriate query results are ready. The GPU can
							either wait until the results are ready, or just execute the draw packet as if it were unpredicated.
				@param[in] action Specifies the relation between the query results and whether the predicated draw packets are skipped or executed.
				@see writeOcclusionQuery(), setZPassPredicationDisable(), Gnm::OcclusionQueryResults()
				@cmdsize 3
*/
void setZPassPredicationEnable(Gnm::OcclusionQueryResults * queryResults, Gnm::PredicationZPassHint hint, Gnm::PredicationZPassAction action)
{
	return m_dcb.setZPassPredicationEnable(queryResults, hint, action);
}

/** @brief Disables conditional rendering. Draw call packets will proceed regardless of ZPass results.
				This function never rolls the hardware context.
				@see setZPassPredicationEnable()
				@cmdsize 3
*/
void setZPassPredicationDisable()
{
	return m_dcb.setZPassPredicationDisable();
}

/** @brief Copies data inline into the command buffer and uses the command processor's DMA engine to transfer it to a destination GPU address.
*
*	This function never rolls the hardware context.
*
*	@param[in] dstGpuAddr	Destination address to which this function writes. Must be 4-byte aligned and must not be <c>NULL</c>.
*	@param[in] data         Pointer to data to be copied inline. This pointer must not be <c>NULL</c>.
*	@param[in] sizeInDwords Number of <c>DWORD</c>s of data to copy. The valid range is <c>[1..16380]</c>.
*	@param[in] writeConfirm Enables/disables write confirmation for this memory write.
*
*	@note Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
*
*	@see writeDataInlineThroughL2()
*
*	@cmdsize 4+sizeInDwords
*/
void writeDataInline(void * dstGpuAddr, const void * data, uint32_t sizeInDwords, Gnm::WriteDataConfirmMode writeConfirm)
{
	return m_dcb.writeDataInline(dstGpuAddr, data, sizeInDwords, writeConfirm);
}

/** @brief Copies data inline into the command buffer and uses the command processor's DMA engine to transfer it to a destination GPU address through the GPU L2 cache.
*
*	This function never rolls the hardware context.
*
*  @param[in] dstGpuAddr   Destination address to which this function writes. Must be 4-byte aligned and must not be <c>NULL</c>.
*  @param[in] data         Pointer to data to be copied inline. This pointer must not be <c>NULL</c>.
*  @param[in] sizeInDwords	Number of <c>DWORD</c>s of data to copy. Valid range is <c>[1..16380]</c>.
*  @param[in] cachePolicy	Specifies the cache policy to use if the data is written to the GPU's L2 cache.
*  @param[in] writeConfirm	Enables or disables write confirmation for this memory write.
*
*  @note Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
*
*  @see writeDataInline()
*
*  @cmdsize 4+sizeInDwords
*/
void writeDataInlineThroughL2(void * dstGpuAddr, const void * data, uint32_t sizeInDwords, Gnm::CachePolicy cachePolicy, Gnm::WriteDataConfirmMode writeConfirm)
{
	return m_dcb.writeDataInlineThroughL2(dstGpuAddr, data, sizeInDwords, cachePolicy, writeConfirm);
}

/** @brief Triggers an event on the GPU.
* 
*  This function never rolls the hardware context.
*
*  @param[in] eventType Type of the event for which the command processor is to wait.
*
*  @see writeEventStats()
*
*  @cmdsize 2
*/
void triggerEvent(Gnm::EventType eventType)
{
	return m_dcb.triggerEvent(eventType);
}

/** @brief Writes the specified 64-bit value to the given location in memory when this command reaches the end of the processing pipe (EOP).
* 
*  This function never rolls the hardware context.
* 
*  @param[in] eventType	Determines when <c><i>immValue</i></c> will be written to the specified address.
*  @param[in] dstSelector	Specifies which levels of the memory hierarchy to write to.
*  @param[in] dstGpuAddr	GPU relative address to which the given value will be written. If <c><i>srcSelector</i></c> is kEventWriteSource32BitsImmediate,
*							this pointer must be 4-byte aligned; otherwise, it must be 8-byte aligned. Either way, it must not be <c>NULL</c>.
*  @param[in] srcSelector	Specifies the type of data to write -- either the provided <c><i>immValue</i></c>, or an internal GPU counter.
*  @param[in] immValue		Value that will be written to <c><i>dstGpuAddr</i></c>. If <c><i>srcSelector</i></c> specifies a GPU counter, this argument
*							will be ignored. If <c><i>srcSelector</i></c> is kEventWriteSource32BitsImmediate, only the low 32 bits of this value will be written.
*  @param[in] cacheAction	Specifies which caches to flush and invalidate after the specified write is complete.
*  @param[in] cachePolicy	Specifies the cache policy to use if the data is written to the GPU's L2 cache. This is enabled only when <c><i>dstSelector</i></c> has been set to anything other than <c>kEventWriteDestMemory</c>.
*	
*  @note This command will not stall the command processor until the action occurs. It merely directs the system to perform the action at the moment that all pipeline outputs have already settled into their respective caches.
*		  To perform the action at the moment that all shader wavefronts have retired, use writeAtEndOfShader().
*
*  @see writeAtEndOfPipeWithInterrupt(), writeAtEndOfShader()
*
*  @cmdsize 6
*/
void writeAtEndOfPipe(Gnm::EndOfPipeEventType eventType, Gnm::EventWriteDest dstSelector, void * dstGpuAddr, Gnm::EventWriteSource srcSelector, uint64_t immValue, Gnm::CacheAction cacheAction, Gnm::CachePolicy cachePolicy)
{
	return m_dcb.writeAtEndOfPipe(eventType, dstSelector, dstGpuAddr, srcSelector, immValue, cacheAction, cachePolicy);
}

/** @brief Writes the specified 64-bit value to the specified location in memory and triggers an interrupt when this command reaches the end of the processing pipe (EOP).
* 
*  This function never rolls the hardware context.
*	
*  @param[in] eventType	Determines when <c><i>immValue</i></c> will be written to the specified address.
*  @param[in] dstSelector	Specifies which levels of the memory hierarchy to write to.
*  @param[in] dstGpuAddr	GPU relative address to which <c><i>immValue</i></c> will be written. If <c><i>srcSelector</i></c> is kEventWriteSource32BitsImmediate,
*							this pointer must be 4-byte aligned; otherwise, it must be 8-byte aligned. Either way, it must not be <c>NULL</c>.
*  @param[in] srcSelector	Specifies the type of data to write -- either the provided <c><i>immValue</i></c>, or an internal GPU counter.
*  @param[in] immValue		Value that will be written to <c><i>dstGpuAddr</i></c>. If <c><i>srcSelector</i></c> specifies a GPU counter, this argument
*							will be ignored. If <c><i>srcSelector</i></c> is kEventWriteSource32BitsImmediate, only the low 32 bits of this value will be written.
*  @param[in] cacheAction	Specifies which caches to flush and invalidate after the specified data is written and the interrupt triggers.
*  @param[in] cachePolicy	Specifies the cache policy to use if the data is written to the GPU's L2 cache. This is enabled only when <c><i>dstSelector</i></c> has been set to anything other than <c>kEventWriteDestMemory</c>.
*
*  @note Applications can use <c>SceKernelEqueue</c> and Gnm::addEqEvent() to handle interrupts.
*	@note This command will not stall the command processor until the action occurs. It merely directs the system to perform the action at the moment that all pipeline outputs have already settled into their respective caches.
*		  To perform the action at the moment that all shader wavefronts have retired, use writeAtEndOfShader().
*		
* @see  writeAtEndOfPipe(), writeAtEndOfShader()
*
* @cmdsize 6
*/
void writeAtEndOfPipeWithInterrupt(Gnm::EndOfPipeEventType eventType, Gnm::EventWriteDest dstSelector, void * dstGpuAddr, Gnm::EventWriteSource srcSelector, uint64_t immValue, Gnm::CacheAction cacheAction, Gnm::CachePolicy cachePolicy)
{
	return m_dcb.writeAtEndOfPipeWithInterrupt(eventType, dstSelector, dstGpuAddr, srcSelector, immValue, cacheAction, cachePolicy);
}

/** @brief Requests the GPU to trigger an interrupt upon EOP event.
			This function never rolls the hardware context.
			@param[in] eventType   Determines when interrupt will be triggered.
			@param[in] cacheAction Cache action to perform.
			@note Applications can use <c>SceKernelEqueue</c> and sce::Gnm::addEqEvent() to handle interrupts.
			@cmdsize 6
*/
void triggerEndOfPipeInterrupt(Gnm::EndOfPipeEventType eventType, Gnm::CacheAction cacheAction)
{
	return m_dcb.triggerEndOfPipeInterrupt(eventType, cacheAction);
}

/** @brief Writes the specified value to the given location in memory when the specified shader stage becomes idle.
			This function never rolls the hardware context.
			@param[in] eventType   Determines the type of shader event to wait for before writing  <c><i>immValue</i></c> to <c>*dstGpuAddr</c> (PS or CS).
								   If kEosPsDone is passed, this command will not wait for previous dispatches to complete.
								   If kEosCsDone is passed, this command will not wait for previous draws to complete.
			@param[out] dstGpuAddr     GPU address to which <c><i>immValue</i></c> will be written. Must be 4-byte aligned. Must not be <c>NULL</c>.
			@param[in] immValue       Value that will be written to <c><i>dstGpuAddr</i></c>.
			
			@note When <c><i>eventType</i></c> is set to <c>kEosCsDone</c>, this function should be called right after the call to 'dispatch'.
			@note This command must be written immediately after the corresponding draw command, otherwise GPU behavior is undefined.
			
			@cmdsize 5
*/
void writeAtEndOfShader(Gnm::EndOfShaderEventType eventType, void * dstGpuAddr, uint32_t immValue)
{
	return m_dcb.writeAtEndOfShader(eventType, dstGpuAddr, immValue);
}

/** @brief Blocks command processor until specified test passes.
*	
*  The 32-bit value at the specified GPU address is tested against the
*  reference value with the test qualified by the specified function and mask.
*  Basically, this function tells the GPU to stall until <c><i>compareFunc</i>((*<i>gpuAddr</i> and <i>mask</i>), <i>refValue</i>) == true</c>.
*	
*  This function never rolls the hardware context.
*
*  @param[in] gpuAddr		Address to poll. Must be 4-byte aligned and must not be <c>NULL</c>.
*  @param[in] mask			Mask to be applied to <c>*<i>gpuAddr</i></c> before comparing to <c><i>refValue</i></c>.
*  @param[in] compareFunc	Specifies the type of comparison to be done between (<c>*<i>gpuAddr</i></c> AND <c><i>mask</i></c>) and the <c><i>refValue</i></c>.
*  @param[in] refValue		Expected value of <c>*<i>gpuAddr</i></c>.
*
*  @see waitOnAddressAndStallCommandBufferParser()
* 
*  @cmdsize 7
*/
void waitOnAddress(void * gpuAddr, uint32_t mask, Gnm::WaitCompareFunc compareFunc, uint32_t refValue)
{
	return m_dcb.waitOnAddress(gpuAddr, mask, compareFunc, refValue);
}

/** @brief Stalls command-processor parsing of the command buffer until all previous commands have started executing.
* 
*  This function never rolls the hardware context.
*
*  @note Commands may not necessarily be finished executing. For example, draw commands
*		  may have been launched but not necessarily finished and committed to memory.
*	@note In addition to fetching command buffer data, the CP prefetches the index data referenced by subsequent draw calls.
*		  Stalling the command buffer parsing will also prevent this index prefetching, which may be necessary if the index data is
*		  dynamically generated or uploaded just-in-time by a previous command.
*
*  @see waitOnAddressAndStallCommandBufferParser()
*
*  @cmdsize 2
*/
void stallCommandBufferParser()
{
	return m_dcb.stallCommandBufferParser();
}

/** @brief Blocks command processor until the specified test passes and all previous commands have started executing.
*
*  The 32-bit value at the specified GPU address is tested against the reference value with the test qualified
*  by the specified function and mask. This function tells the GPU to stall until:
*
*  <c><i>compareFunc</i>((*<i>gpuAddr</i> and <i>mask</i>), <i>refValue</i>) == true</c>
*
*  Unlike waitOnAddress(), this variant only supports a compare function of Gnm::kWaitCompareFuncGreaterEqual (which is hard-coded).
*
*  This function never rolls the hardware context.
*
*  @param[in] gpuAddr		Address to poll. Must be 4-byte aligned and must not be <c>NULL</c>. 
*  @param[in] mask			Mask to be applied to <c>*<i>gpuAddr</i></c> before comparing to <c><i>refValue</i></c>.
*  @param[in] refValue		Expected value of <c>*<i>gpuAddr</i></c>.
*
*  @note Commands may not necessarily be finished executing. For example, draw commands
*		  may have been launched but not necessarily finished and committed to memory.
*  @note In addition to fetching command buffer data, the CP prefetches the index data referenced by subsequent draw calls.
*		  Stalling the command buffer parsing will also prevent this index prefetching, which may be necessary if the index data is
*		  dynamically generated or uploaded just-in-time by a previous command.
*
*  @see waitOnAddress()
* 
*  @cmdsize 7
*/
void waitOnAddressAndStallCommandBufferParser(void * gpuAddr, uint32_t mask, uint32_t refValue)
{
	return m_dcb.waitOnAddressAndStallCommandBufferParser(gpuAddr, mask, refValue);
}

/** @brief Blocks command-processor processing until specified test passes.
* 
*  The 32-bit value at the specified GPU register is tested against the
*  reference value with the test qualified by the specified function and mask.
*  Basically, this function tells the GPU to stall until <c><i>compareFunc</i>((*<i>gpuAddr</i> and <i>mask</i>), <i>refValue</i>) == true</c>.
* 
*  This function never rolls the hardware context.
*
*  @param[in] gpuReg		Register offset to poll. Must not be <c>NULL</c>. 
*  @param[in] mask			Mask to be applied to <c>*<i>gpuAddr</i></c> before comparing to <c><i>refValue</i></c>.
*  @param[in] compareFunc	Specifies the type of comparison to be done between (<c>*<i>gpuAddr</i></c> and <c><i>mask</i></c>) and the <c><i>refValue</i></c>.
*  @param[in] refValue		Expected value of <c>*<i>gpuAddr</i></c>.
*
*  @cmdsize 7
*/
void waitOnRegister(uint16_t gpuReg, uint32_t mask, Gnm::WaitCompareFunc compareFunc, uint32_t refValue)
{
	return m_dcb.waitOnRegister(gpuReg, mask, compareFunc, refValue);
}

/** @brief Waits for all PS shader output to one or more targets to complete.
			One can specify the various render target slots
			(color and/or depth,) to be checked within the provided base address and size: all active contexts associated with
			those target can then be waited for. The caller may also optionally specify that certain caches be flushed.
			This function may roll the hardware context.
			@note This command will only wait on output written by graphics shaders, not compute shaders!
			@param[in] baseAddr256     Starting base address (256-byte aligned) of the surface to be synchronized to (high 32 bits of a 40-bit
								   virtual GPU address).
			@param[in] sizeIn256ByteBlocks        Size of the surface. Has a granularity of 256 bytes. This must not be set to 0 if <c><i>targetMask</i></c> contains kWaitTargetSlotDb.
			@param[in] targetMask		Configures which of the source and destination caches should be enabled for coherency. This field is
											composed of individual flags from the #Gnm::WaitTargetSlot enum.
			@param[in] cacheAction      Specifies which caches to flush and invalidate after the specified writes are complete.
			@param[in] extendedCacheMask Specifies additional caches to flush and invalidate. This field is composed of individual flags from the #Gnm::ExtendedCacheAction enum.
			@param[in] commandBufferStallMode Specifies whether to stall further parsing of the command buffer until the wait condition is complete.
			@see Gnm::WaitTargetSlot, Gnm::ExtendedCacheAction, flushShaderCachesAndWait()
			@cmdsize 7
*/
void waitForGraphicsWrites(uint32_t baseAddr256, uint32_t sizeIn256ByteBlocks, uint32_t targetMask, Gnm::CacheAction cacheAction, uint32_t extendedCacheMask, Gnm::StallCommandBufferParserMode commandBufferStallMode)
{
	return m_dcb.waitForGraphicsWrites(baseAddr256, sizeIn256ByteBlocks, targetMask, cacheAction, extendedCacheMask, commandBufferStallMode);
}

/** @brief Requests a flush of the specified data cache(s) and waits for the flush operation(s) to complete.
				This function may roll the hardware context.
				@note This function is equivalent to calling <c>waitForGraphicsWrites(0,1,0,<i>cacheAction</i>,<i>extendedCacheMask</i>)</c>.
				@param[in] cacheAction      Specifies which caches to flush and invalidate.
				@param[in] extendedCacheMask Specifies additional caches to flush and invalidate. This field is composed of individual flags from the #Gnm::ExtendedCacheAction enum.
				@param[in] commandBufferStallMode Specifies whether to stall further parsing of the command buffer until the wait condition is complete.
				@see waitForGraphicsWrites(), Gnm::ExtendedCacheAction
				@cmdsize (extendedCacheMask & kExtendedCacheActionFlushAndInvalidateDbCache) ? 9 : 7
*/
void flushShaderCachesAndWait(Gnm::CacheAction cacheAction, uint32_t extendedCacheMask, Gnm::StallCommandBufferParserMode commandBufferStallMode)
{
	return m_dcb.flushShaderCachesAndWait(cacheAction, extendedCacheMask, commandBufferStallMode);
}

/** @brief Signals a semaphore.
*
*	@param[in] semAddr Address of the semaphore's mailbox (must be 8-byte aligned). This pointer must not be <c>NULL</c>.
*	@param[in] behavior Selects between incrementing the mailbox value and setting the mailbox value to 1.
*	@param[in] updateConfirm If enabled, the packet waits for the mailbox to be written.
*
*  @see waitSemaphore()
*
*	@cmdsize 3
*/
void signalSemaphore(uint64_t * semAddr, Gnm::SemaphoreSignalBehavior behavior, Gnm::SemaphoreUpdateConfirmMode updateConfirm)
{
	return m_dcb.signalSemaphore(semAddr, behavior, updateConfirm);
}

/** @brief Waits on a semaphore.
*	This function waits until the value in the mailbox is not 0.
*
*	@param[in] semAddr  Address of the semaphore's mailbox (must be 8-byte aligned). This pointer must not be <c>NULL</c>.
*	@param[in] behavior Selects the action to perform when the semaphore opens (mailbox becomes non-zero): either decrement or do nothing.
*
*  @see signalSemaphore()
*
*	@cmdsize 3
*/
void waitSemaphore(uint64_t * semAddr, Gnm::SemaphoreWaitBehavior behavior)
{
	return m_dcb.waitSemaphore(semAddr, behavior);
}

/** @brief Writes out the statistics for the specified event to memory.
				This function never rolls the hardware context.
				@param[in] eventStats  	Type of event to get the statistics for.
				@param[out] dstGpuAddr	GPU-relative address to which the stats will be written. This pointer must not be <c>NULL</c> and it must be 8-byte aligned. 
				@see Gnm::EventStats, triggerEvent()
				@cmdsize 4
*/
void writeEventStats(Gnm::EventStats eventStats, void * dstGpuAddr)
{
	return m_dcb.writeEventStats(eventStats, dstGpuAddr);
}

/** @brief Inserts the specified number of <c>DWORD</c>s in the command buffer as a <c>NOP</c> packet.
*
*	This function never rolls the hardware context.
*
*	@param[in] numDwords   Number of <c>DWORD</c>s to insert. The entire packet (including the PM4 header) will be <c><i>numDwords</i></c>. The valid range is [0..16384].
*
*	@cmdsize numDwords
*/
void insertNop(uint32_t numDwords)
{
	return m_dcb.insertNop(numDwords);
}

/** @brief Sets a marker command in the command buffer that will be used by the PA/Debug tools.
			The marker command created by this function will be handled as a standalone marker. For a scoped marker block,
			use pushMarker() and popMarker().
			This function never rolls the hardware context.
			@param[in] debugString   String to be embedded into the command buffer. This pointer must not be <c>NULL</c>, and <c>strlen(<i>debugString</i>) < 32756</c>.
			
			@see pushMarker(), popMarker()
			@cmdsize 2 + 2*((strlen(debugString)+1+4+7)/(2*sizeof(uint32_t))) + (strlen(debugString)+1+4+3)/sizeof(uint32_t)
*/
void setMarker(const char * debugString)
{
	return m_dcb.setMarker(debugString);
}

/** @brief Sets a colored marker command in the command buffer that will be used by the PA/Debug tools.
			The marker command created by this function will be handled as a standalone marker. For a scoped marker block,
			use pushMarker() and popMarker().
			This function never rolls the hardware context.
			
			@param debugString		String to be embedded into the command buffer. This pointer must not be <c>NULL</c>, and <c>strlen(<i>debugString</i>) < 32756</c>.
			@param argbColor		The color of the marker.
			
			@see pushMarker(), popMarker()
			
			@cmdsize 2 + 2*((strlen(debugString)+1+8+7)/(2*sizeof(uint32_t))) + (strlen(debugString)+1+8+3)/sizeof(uint32_t)
*/
void setMarker(const char * debugString, uint32_t argbColor)
{
	return m_dcb.setMarker(debugString, argbColor);
}

/** @brief Sets a marker command in the command buffer that will be used by the Performance Analysis and Debug tools.
			The marker command created by this function is handled as the beginning of a scoped marker block.
			Close the block with a matching call to popMarker(). Marker blocks can be nested.
			This function never rolls the hardware context.
			
			@param[in] debugString   The string to be embedded into the command buffer. This value must not be <c>NULL</c>, and <c>strlen(debugString) < 32756</c>.
			
			@see popMarker()
			@cmdsize 2 + 2*((strlen(debugString)+1+4+7)/(2*sizeof(uint32_t))) + (strlen(debugString)+1+4+3)/sizeof(uint32_t)
*/
void pushMarker(const char * debugString)
{
	return m_dcb.pushMarker(debugString);
}

/** @brief Sets a colored marker command in the command buffer that will be used by the Performance Analysis and Debug tools.
			The marker command created by this function is handled as the beginning of a scoped marker block.
			Close the block with a matching call to popMarker(). Marker blocks can be nested.
			This function never rolls the hardware context.
			@param debugString  The string to be embedded into the command buffer. This value must not be <c>NULL</c>, and <c>strlen(debugString) < 32756</c>.
			@param argbColor	The color of the marker.
			
			@see popMarker()
			@cmdsize 2 + 2*((strlen(debugString)+1+8+7)/(2*sizeof(uint32_t))) + (strlen(debugString)+1+8+3)/sizeof(uint32_t)
*/
void pushMarker(const char * debugString, uint32_t argbColor)
{
	return m_dcb.pushMarker(debugString, argbColor);
}

/** @brief Closes the marker block opened by the most recent pushMarker() command.
				This function never rolls the hardware context.
				@see pushMarker()
				@cmdsize 6
*/
void popMarker()
{
	return m_dcb.popMarker();
}

/** @brief Requests an immediate MIP Stats report, then resets the counters.
				@param[out] outputBuffer Buffer in which the GPU reports the MIP stats. (Must be 64-byte aligned.)
				@param[in] sizeInByte   Size of the <c>outputBuffer</c> in bytes. Must be equal to <c>n*2048+64</c> bytes. Minimum valid value is <c>(4096+64)</c> bytes.
										<i>n</i> is an integer value of <c>2</c> or more.  Thus, valid values of <c>sizeInByte</c> are (2*2048+64), (3*2048+64), (4*2048+64), and so on.
				@cmdsize 4
*/
void requestMipStatsReportAndReset(void * outputBuffer, uint32_t sizeInByte)
{
	return m_dcb.requestMipStatsReportAndReset(outputBuffer, sizeInByte);
}

/** @brief Prefetches data into L2$
				@param dataAddr     The GPU address of the buffer to be preloaded in L2$. This value must not be <c>NULL</c>.
				@param sizeInBytes	The size of the buffer in bytes. The maximum size allowed is <c>kDmaMaximumSizeInBytes</c>.
				
				@note Do not use this function for buffers to which you intend to write.
				@note The GPU supports a maximum of two DMA transfers in flight concurrently. If a third DMA is issued, the CP will stall until one of the previous two completes. The following
				      Gnm commands are implemented using CP DMA transfers, and are thus subject to this restriction:
					  <ul>
						<li>dmaData()</li>
						<li>prefetchIntoL2()</li>
						<li>writeOcclusionQuery()</li>
					  </ul>
				@cmdsize 7
*/
void prefetchIntoL2(void * dataAddr, uint32_t sizeInBytes)
{
	return m_dcb.prefetchIntoL2(dataAddr, sizeInBytes);
}

#ifdef __ORBIS__
/** @brief Blocks command processor until the specified display buffer is no longer displayed.
*
*	This function never rolls the hardware context.
*
*  @param[in] videoOutHandle		The video output handle obtained from <c>sceVideoOutOpen()</c>.
*  @param[in] displayBufferIndex	The index of the display buffer, which is registered with <c>sceVideoOutRegisterBuffers()</c>, to wait for.
*
*  @note This function does not block CPU processing; rather, it blocks the CP from processing further commands
*		  until the specified display buffer is ready to be overwritten.
*
*  @cmdsize 7
*/
void waitUntilSafeForRendering(uint32_t videoOutHandle, uint32_t displayBufferIndex)
{
	return m_dcb.waitUntilSafeForRendering(videoOutHandle, displayBufferIndex);
}

/** @brief Prepares the system to flip command buffers.
				This function is intended to be used in conjunction with Gnm::submitAndFlipCommandBuffers().
				This function MUST be the last command to be inserted in the command buffer before a call to Gnm::submitAndFlipCommandBuffers().
				When this command buffer has been processed by the CP, it will emit the specified <c>eventType</c> and take the specified <c>cacheAction</c>.
				@param[in] eventType   Determines when interrupt will be triggered.
				@param[in] cacheAction Cache action to perform.
				@note Applications can use SceKernelEqueue and sce::Gnm::addEqEvent() to handle interrupts.
				@see Gnm::submitAndFlipCommandBuffers()
				@cmdsize 64
*/
void prepareFlipWithEopInterrupt(Gnm::EndOfPipeEventType eventType, Gnm::CacheAction cacheAction)
{
	return m_dcb.prepareFlipWithEopInterrupt(eventType, cacheAction);
}

/** @brief Inserts a pause command to stall DCB processing and reserves a specified number of <c>DWORD</c>s after it.
*	
*	@param[in] reservedDWs	The number of <c>DWORD</c>s to reserve.
*
*	@return The address of the beginning of the "hole" in the command buffer following the pause command. To release the pause command, pass this address to the resume() method.
*	
*  @see resume(), fillAndResume(), chainCommandBufferAndResume()
*
*	@cmdsize 2 + reservedDWs
*/
uint64_t pause(uint32_t reservedDWs)
{
	return m_dcb.pause(reservedDWs);
}

/** @brief Immediately releases a previously written pause command.
*	
*	@param[in]	holeAddr	The address of the "hole" following a pause command. The pause() method returned this value when it created the pause command.
*	
*	@note Because the CPU modifies the pause command directly, this method releases the pause immediately.
*
*  @see pause()
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
*	@param[in]	commandStream	The command stream data to copy into the "hole."
*	@param[in]	sizeInDW		The size of the hole in <c>DWORD</c>s. 
*	
*	@note Because the CPU modifies the pause command directly, this method releases the pause immediately. 
*
*  @see pause()
*
*  @cmdsize 0
*/
void fillAndResume(uint64_t holeAddr, void * commandStream, uint32_t sizeInDW)
{
	return m_dcb.fillAndResume(holeAddr, commandStream, sizeInDW);
}

/** @brief Inserts a reference to another command buffer into a previously allocated "hole" and then releases the pause command immediately.
*
*	@param[in]	holeAddr			The address of the "hole" following a pause command. The pause() method returned this value when it created the pause command.
*	@param[in]	nextIbBaseAddr		A pointer to the command buffer that is the destination of the reference this method creates. This pointer must not be <c>NULL</c>.
*	@param[in]	nextIbSizeInDW		The size of the <c><i>nextIbBaseAddr</i></c> buffer in <c>DWORD</c>s.  The valid range is <c>[1..kIndirectBufferMaximumSizeInBytes/4]</c>.
*
*	@note The size of the allocated "hole" must be exactly four <c>DWORD</c>s.
*	@note This method must be the final command in any command buffer that uses it.
*	@note Because the CPU modifies the pause command directly, this method releases the pause immediately. 
*	
*  @see pause()
*
*	@cmdsize 0 
*/
void chainCommandBufferAndResume(uint64_t holeAddr, void * nextIbBaseAddr, uint64_t nextIbSizeInDW)
{
	return m_dcb.chainCommandBufferAndResume(holeAddr, nextIbBaseAddr, nextIbSizeInDW);
}

/** @brief Inserts a reference to another command buffer.
*
*	@param[in]	cbBaseAddr		The address of the command buffer that is the destination of the reference this method creates. This pointer must not be <c>NULL</c>.
*	@param[in]	cbSizeInDW		The size of the <c><i>cbBaseAddr</i></c> buffer in <c>DWORD</c>s. The valid range is <c>[1..kIndirectBufferMaximumSizeInBytes/4]</c>.
*
*	@note This function must be the final command in any command buffer that uses it.
*
*	@cmdsize 4
*/
void chainCommandBuffer(void * cbBaseAddr, uint64_t cbSizeInDW)
{
	return m_dcb.chainCommandBuffer(cbBaseAddr, cbSizeInDW);
}
#endif // __ORBIS__
/** @brief Sets draw payload options.
			@sa DrawPayloadControl
			@param[in] cntrl A DrawPayloadControl configured with appropriate options.
			This function rolls the graphics context.
			@note  NEO mode only
			@cmdsize 3
*/
void setDrawPayloadControl(Gnm::DrawPayloadControl cntrl)
{
	return m_dcb.setDrawPayloadControl(cntrl);
}

/** @brief Configures the generation of Object ID values.
				
				@param[in] objIdSource Specifies whether to read a draw call's object ID from a GPU register or from the shader output.
				@param[in] addPrimitiveId Optionally enables addition of the pipelined primitive ID to the object ID.
				@note  NEO mode only
				@sa setObjectId()
				@cmdsize 3
*/
void setObjectIdMode(Gnm::ObjectIdSource objIdSource, Gnm::AddPrimitiveId addPrimitiveId)
{
	return m_dcb.setObjectIdMode(objIdSource, addPrimitiveId);
}

/** @brief Set the object id for subsequent draw calls.
			           In order to use this value, you must set the Object ID source to <c>kObjectIdSourceReg</c>.
			@param[in] id The object id.
			This function does not roll the graphics context.
			@note  NEO mode only
			@sa setObjectIdMode()
			@cmdsize 3
*/
void setObjectId(uint32_t id)
{
	return m_dcb.setObjectId(id);
}

/** @brief Sets tessellation distribution thresholds.
			@param[in] thresholds The TessellationDistributionThresholds structure with the thresholds settings. 
			@note  NEO mode only
			@sa TessellationDistributionThresholds
			@cmdsize 3
*/
void setTessellationDistributionThresholds(Gnm::TessellationDistributionThresholds thresholds)
{
	return m_dcb.setTessellationDistributionThresholds(thresholds);
}

#ifdef _MSC_VER
#	pragma warning(pop)
#endif

#endif // !defined(_SCE_GNMX_GFXCONTEXT_METHODS_H)
