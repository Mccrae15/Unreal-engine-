/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if !defined(_SCE_GNMX_HELPERS_H)
#define _SCE_GNMX_HELPERS_H

#include <gnm/common.h>
#include <gnm/constants.h>
#include <gnm/error.h>
#include <gnmx/common.h>

namespace sce
{
	namespace Gnm
	{
		class AaSampleLocationControl;
		class DataFormat;
		class DispatchCommandBuffer;
		class MeasureDispatchCommandBuffer;
		class DrawCommandBuffer;
		class MeasureDrawCommandBuffer;
	}
	namespace Gnmx
	{
		class EsShader; // see gnmx/shaderbinary.h
		class GsShader; // see gnmx/shaderbinary.h
		class LsShader; // see gnmx/shaderbinary.h
		class HsShader; // see gnmx/shaderbinary.h

		/**
		 * @brief Converts a single-precision IEEE <c>float</c> value to an unsigned 2.6 fixed-point value.
		 * @param[in] f The <c>float</c> to convert. Must be in the range <c>[0..2)</c> or the function will assert.
		 * @return The nearest U2.6 value to <c><i>f</i></c>.
		 */
		static inline uint8_t convertF32ToU2_6(float f)
		{
			SCE_GNM_ASSERT_MSG_INLINE(f >= 0.0f && f < 2.f, "f (%f) must be in the range [0..2)", f);
			uint8_t retVal = static_cast<uint8_t>(f*64.f);
			return f < 0.0f ? 0x00 : (retVal > 0xFF ? 0xFF : retVal);
		}

		/**
		 * @brief Converts a single-precision IEEE <c>float</c> value to a signed 2.6 fixed-point value.
		 * @param[in] f The <c>float</c> to convert. Must be in the range <c>(-2..2]</c> or the function will assert.
		 * @return The nearest S2.6 value to <c><i>f</i></c>.
		 */
		static inline int32_t convertF32ToS2_6(float f)
		{
			SCE_GNM_ASSERT_MSG_INLINE(f > -2.0f && f < 2.f, "f (%f) must be in the range (-2..2)", f);
			int32_t retVal = static_cast<int32_t>(f*64.f);
			retVal = retVal < -2*64 ? (-2*64) : (retVal >= 2*64 ? (2*64-1) : retVal);
			return retVal & 0x00FF; // mask off high bits for negative values
		}

		/**
		 * @brief Converts a single-precision IEEE <c>float</c> value to an unsigned 4.8 fixed-point value.
		 * @param[in] f The <c>float</c> to convert. Must be in the range <c>[0..15]</c> or the function will assert.
		 * @return The nearest U4.8 value to <c><i>f</i></c>.
		 */
		static inline uint32_t convertF32ToU4_8(float f)
		{
			SCE_GNM_ASSERT_MSG_INLINE(f >= 0.0f && f < 16.f, "f (%f) must be in the range [0..16)", f);
			uint32_t retVal = static_cast<uint32_t>(f*256.f);
			return f < 0.0f ? 0x000 : (retVal > 0xFFF ? 0xFFF : retVal);
		}

		/**
		 * @brief Converts a single-precision IEEE float value to an unsigned 12.4 fixed-point value.
		 * @param[in] f The float to convert. Must be in the range <c>[0..4095]</c>, or the function will assert.
		 * @return The nearest U12.4 value to <c><i>f</i></c>.
		 */
		static inline uint16_t convertF32ToU12_4(float f)
		{
			SCE_GNM_ASSERT_MSG_INLINE(f >= 0.0f && f < 4096.f, "f (%f) must be in the range [0..4096)", f);
			uint16_t retVal = static_cast<uint16_t>(f*16.f);
			return f < 0.0f ? 0x0000 : retVal;
		}

		/**
		 * @brief Converts a single-precision IEEE float value to a signed 6.8 fixed-point value.
		 * @param[in] f The float to convert. Must be in the range <c>(-32..32)</c>, or the function will assert.
		 * @return The nearest S6.8 value to <c><i>f</i></c>.
		 */
		static inline int32_t convertF32ToS6_8(float f)
		{
			SCE_GNM_ASSERT_MSG_INLINE(f > -32.0f && f < 32.f, "f (%f) must be in the range (-32..32)", f);
			int32_t retVal = static_cast<int32_t>(f*256.f);
			retVal = retVal < -32*256 ? (-32*256) : (retVal >= 32*256 ? (32*256-1) : retVal);
			return retVal & 0x3FFF; // mask off high bits for negative values
		}

		/**
		 * @brief Converts a single-precision IEEE float value to a signed 2.4 fixed-point value.
		 * @param[in] f The float to convert. Must be in the range <c>(-2..2)</c>, or the function will assert.
		 * @return The nearest S2.4 value to <c><i>f</i></c>.
		 */
		static inline int32_t convertF32ToS2_4(float f)
		{
			SCE_GNM_ASSERT_MSG_INLINE(f > -2.0f && f < 2.f, "f (%f) must be in the range (-2..2)", f);
			int32_t retVal = static_cast<int32_t>(f*16.f);
			retVal = retVal < -2*16 ? (-2*16) : (retVal >= 2*16 ? (2*16-1) : retVal);
			return retVal & 0x003F; // mask off high bits for negative values
		}

		/**
		 * @brief Converts a single-precision IEEE float value to the nearest half-precision IEEE float.
		 * @param[in] f32 The float to convert.
		 * @return The nearest F16 value to <c><i>f32</i></c>.
		 */
		static inline uint16_t convertF32ToF16(const float f32)
		{
			union F32toU32
			{
				uint32_t u;
				float    f;
			} val;
			val.f = f32;

			uint32_t uRes32 = val.u;

			const int16_t  iExp      = (const int16_t)(((const int32_t) ((uRes32>>23)&0xff))-127+15);
			const uint16_t  expClamp = iExp < 0 ? 0 : (iExp>31 ? 31 : (uint16_t)iExp);

			const uint16_t sign = (const uint16_t) ((uRes32>>16)&0x8000);
			const uint16_t mant = (const uint16_t) ((uRes32>>13)&0x3ff);
			const uint16_t exp  = (const uint16_t) (expClamp << 10);

			return sign | exp | mant;
		}

		/**
		* @brief Converts a half-precision IEEE float value to the nearest single-precision IEEE float.
		 * @param[in] f16 The float to convert.
		 * @return The nearest F32 value to <c><i>f16</i></c>.
		 */
		static inline float convertF16ToF32(const uint16_t f16)
		{
			union U32toF32
			{
				uint32_t u;
				float    f;
			} val;

			const uint32_t sign = (((const uint32_t) f16)&0x8000)<<16;
			const uint32_t mant = (f16&0x3ffu)<<13; // 13 bits are zeros
			const uint32_t exp  = (f16&0x7fff)==0 ? 0 : (((((const uint32_t) f16)>>10)&0x1f)-15+127)<<23;

			val.u = sign | exp | mant;
			return val.f;
		}

		/**
		 * @brief Performs some basic sanity checks on a Gnm DataFormat object.
		 * @param format The data format to validate.
		 * @return A value of <c>false</c> if the DataFormat object fails some basic validation tests; otherwise <c>true</c> is returned.
		 * @note This function is much more permissive than the Gnm::DataFormat::isValid() function it replaces; it only checks
		 *       whether the individual fields are within the defined range of their corresponding enums. A return value of <c>true</c>
		 *       does not necessarily mean that the format is correct (or useful) but a return value of <c>false</c> is always a bad sign.
		 */
		SCE_GNMX_EXPORT bool isDataFormatValid(const Gnm::DataFormat &format);

		/** @brief Determines the recommended number of patches and the number of primitives per VGT, used as a parameter to Gnm::DrawCommandBuffer::setVgtControl().
		 *
		 * These values are relevant when using hardware tessellation. Ideally, there should be no more than 256 vertices worth of patches per VGT, but you also do not want any partially filled HS thread groups.
		 * @param[out] outVgtPrimCount The VGT primitive count will be written here. Note that you must subtract 1 from this value before passing it to setVgtControl(). This pointer must not be <c>NULL</c>.
		 * @param[out] outPatchCount The number of patches per HS thread group will be written here. This value is used as an input to Gnm::TessellationDataConstantBuffer::init(), Gnm::TessellationRegisters::init(),
		 *                      and GfxContext::setLsHsShaders(). This pointer must not be <c>NULL</c>.
		 * @param[in] maxHsVgprCount The maximum number of HS-stage general-purpose registers to allocate to patches for each threadgroup.
		 * @param[in] maxHsLdsBytes The maximum amount of HS-stage local data store to allocate for patches for each threadgroup. This must also include space for the outputs of the LS stage.
		 * @param[in] lsb The LsShader which will feed into <c><i>hsb</i></c>. This pointer must not be <c>NULL</c>.
		 * @param[in] hsb The HsShader to generate patch counts for. This pointer must not be <c>NULL</c>.
		 * @see Gnm::DrawCommandBuffer::setVgtControl(), Gnm::TessellationDataConstantBuffer::init(), Gnm::TessellationRegisters::init(), GfxContext::setLsHsShaders()
		 */
		SCE_GNMX_EXPORT void computeVgtPrimitiveAndPatchCounts(uint32_t *outVgtPrimCount, uint32_t *outPatchCount, uint32_t maxHsVgprCount, uint32_t maxHsLdsBytes, const LsShader *lsb, const HsShader *hsb);

		/** @brief Computes the number of primitives per VGT.
		 *
		 * Output from this function is used as an argument to pass to Gnm::DrawCommandBuffer::setVgtControl() and may adjust the requested number of patches.
		 * These values are relevant when using hardware tessellation. Ideally, there should be no more than 256 vertices worth of patches per VGT, but you also
		 * do not want any partially filled HS thread groups either.
		 *
		 * @param outVgtPrimCount	Receives the VGT primitive count. Note that 1 must be subtracted from this value before passing it to <c>setVgtControl()</c>.
		 * @param inoutPatchCount	An input/output parameter. The input should contain the requested number of patches, and this value can be calculated using sce::Gnm::computeNumPatches().
		 *							The value supplied must be greater than 0.	It may be adjusted by this function and is used as an argument to pass to 
		 *							Gnm::TessellationDataConstantBuffer::init(), Gnm::TessellationRegisters::init() and GfxContext::setLsHsShaders().
		 * @param hsb				The HsShader to generate patch counts for.
		 *
		 * @see Gnm::DrawCommandBuffer::setVgtControl(), Gnm::TessellationDataConstantBuffer::init(), Gnm::TessellationRegisters::init(), GfxContext::setLsHsShaders()
		 */
		SCE_GNMX_EXPORT void computeVgtPrimitiveCountAndAdjustNumPatches(uint32_t *outVgtPrimCount, uint32_t *inoutPatchCount, const HsShader *hsb);

		/** @brief Computes LDS allocation size and number of GS threads per LDS allocation for on-chip GS.
		 *
		 * These values may be passed to GfxContext::setOnChipEsShader() and GfxContext::setOnChipGsVsShaders() respectively.
		 * They are optimized to maximize GS and ES thread utilization within the constraints placed by running in no more than <c><i>maxLdsUsage</i></c> bytes of LDS.
		 *
		 * @param[out] outLdsSizeIn512Bytes		Receives the ldsSize in 512 bytes.
		 * @param[out] outGsPrimsPerSubGroup	Receives the gsPrimsPerSubGroup value to be passed to GfxContext::setOnChipGsVsShaders().
		 * @param[in] esb						The on-chip-GS EsShader binary, which will be passed to GfxContext::setOnChipEsShader().
		 * @param[in] gsb						The on-chip GsShader binary, which will be passed to GfxContext::setOnChipGsVsShaders().
		 * @param[in] maxLdsUsage				The maximum size in bytes, which should be returned in <c><i>outLdsSizeIn512Bytes</i></c>. 64*1024 is the maximum size, but smaller values can be used to insist that more than 1 on-chip GS sub-group should be able to run per compute unit.
		 *
	     * @return				A value of <c>true</c> if at least one GS thread (primitive) can be run in <c><i>maxLdsUsage</i></c> bytes of LDS; otherwise a value of <c>false</c> is returned.
		 */
		SCE_GNMX_EXPORT bool computeOnChipGsConfiguration(uint32_t *outLdsSizeIn512Bytes, uint32_t *outGsPrimsPerSubGroup, const EsShader *esb, const GsShader *gsb, uint32_t maxLdsUsage);

		/////////////// DrawCommandBuffer helper functions

		/** @brief Uses the command processor's DMA engine to clear a buffer to specified value (like a GPU memset) using a draw command buffer.

			@param[in,out] dcb				The draw command buffer to which this method writes GPU commands.
			@param[out] dstGpuAddr			The destination address to which this method writes. Must not be <c>NULL</c>.
			@param[in] srcData				The value with which this method fills the destination buffer.
			@param[in] numBytes				Size of the destination buffer. Must be a multiple of 4.
			@param[in] isBlocking    		Flag that specifies whether the CP will block while the transfer is active.
			@note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DrawCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
			@note  This feature does not affect the GPU L2$ cache.
			@see   DrawCommandBuffer::dmaData()
		*/
		void fillData(GnmxDrawCommandBuffer *dcb, void *dstGpuAddr, uint32_t srcData, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking);

		/** @brief Uses the command processor's DMA engine to clear a buffer to specified value (like a GPU memset) using a measure draw command buffer.

			
			@param[in,out] dcb				The measure draw command buffer to which this method writes GPU commands.
			@param[out] dstGpuAddr			The destination address to which this method writes. Must not be <c>NULL</c>.
			@param[in] srcData				The value with which this method fills the destination buffer.
			@param[in] numBytes				Size of the destination buffer. Must be a multiple of 4.
			@param[in] isBlocking			A flag that specifies whether the CP will block while the transfer is active.
			@note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DrawCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
			@note  This feature does not affect the GPU L2$ cache.
			@see   DrawCommandBuffer::dmaData()
		*/
		void fillData(Gnm::MeasureDrawCommandBuffer *dcb, void *dstGpuAddr, uint32_t srcData, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking);
		
		/** @brief Uses the command processor's DMA engine to transfer data from a source address to a destination address in a draw command buffer.
			
			@param[in,out] dcb				The draw command buffer to which this method writes GPU commands. Must not be <c>NULL</c>.
			@param[out] dstGpuAddr			Destination address to which this method writes. Must not be <c>NULL</c>.
			@param[in] srcGpuAddr			Source address from which this method reads data.
			@param[in] numBytes				The number of bytes to transfer.
			@param[in] isBlocking			Flag that specifies whether the CP will block while the transfer is active.
			@note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DrawCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
			@note  This feature does not affect the GPU L2$ cache.
			@see   DrawCommandBuffer::dmaData()
		*/
		void copyData(GnmxDrawCommandBuffer *dcb, void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking);
		
		/** @brief Uses the command processor's DMA engine to transfer data from a source address to a destination address in a measure draw command buffer.
			
			@param[in,out] dcb				The measure draw command buffer to which this method writes GPU commands.
			@param[out] dstGpuAddr			Destination address to which this method writes. Must not be <c>NULL</c>
			@param[in] srcGpuAddr			Source address from which this method reads data. Must not be <c>NULL</c>.
			@param[in] numBytes				The number of bytes to transfer.
			@param[in] isBlocking			Flag that specifies whether the CP will block while the transfer is active.
			@note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DrawCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
			@note  This feature does not affect the GPU L2$ cache.
			@see   DrawCommandBuffer::dmaData()
		*/
		void copyData(Gnm::MeasureDrawCommandBuffer *dcb, void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking);
		
		/** @brief Inserts user data directly inside the draw command buffer returning a locator for later reference.
			
			@param[in,out] dcb		The draw command buffer to write GPU commands to.
			@param[in] dataStream	A pointer to the data to embed.
			@param[in] sizeInDword	The size of the data in a stride of 4. Note that the maximum size of a single command packet is 2^16 bytes,
									and the effective maximum value of <c><i>sizeInDword</i></c> will be slightly less than that due to packet headers
									and padding.
			@param[in] alignment	The alignment of the embedded copy in the CB.
			
			@return					A pointer to the allocated buffer.
		*/
		void* embedData(GnmxDrawCommandBuffer *dcb, const void *dataStream, uint32_t sizeInDword, Gnm::EmbeddedDataAlignment alignment);
		
		/** @brief Inserts user data directly inside the measure command buffer returning a locator for later reference.

			@param[in,out] dcb		The measure draw command buffer to write GPU commands to.
			@param[in] dataStream	A pointer to the data to embed.
			@param[in] sizeInDword	The size of the data in a stride of 4. Note that the maximum size of a single command packet is 2^16 bytes,
									and the effective maximum value of <c><i>sizeInDword</i></c> will be slightly less than that due to packet headers
									and padding.
			@param[in] alignment	The alignment of the embedded copy in the CB.
			
			@return					A pointer to the allocated buffer.
		*/		
		void* embedData(Gnm::MeasureDrawCommandBuffer *dcb, const void *dataStream, uint32_t sizeInDword, Gnm::EmbeddedDataAlignment alignment);

		/** @brief Initializes AaSampleLocationControl for the specified number of AA samples.

			@param[in,out] locationControl	AaSampleLocationControl to initialize with default MSAA sample locations
			@param[out] maxSampleDistance	Maximum sample distance.
			@param[in] numAASamples			The number of samples to use while multisampling.
			@see DrawCommandBuffer::setAaSampleCount()
		*/
		void fillAaDefaultSampleLocations(Gnm::AaSampleLocationControl *locationControl, uint32_t *maxSampleDistance, Gnm::NumSamples numAASamples);

		/** @brief Sets the multisampling sample locations to default values in a draw command buffer.
				   This function also calls DrawCommandBuffer::setAaSampleCount() to set the sample count and maximum sample distance.

			@param[in,out] dcb			The Gnm::DrawCommandBuffer to write commands to.
			@param[in] numAASamples		The number of samples used while multisampling.
			@see DrawCommandBuffer::setAaSampleCount()
		*/
		void setAaDefaultSampleLocations(GnmxDrawCommandBuffer *dcb, Gnm::NumSamples numAASamples);

		/** @brief Sets the multisampling sample locations to default values in a measure draw command buffer.
				   This function also calls DrawCommandBuffer::setAaSampleCount() to set the sample count and maximum sample distance.

			@param[in,out] dcb			The Gnm::MeasureDrawCommandBuffer to write commands to.
			@param[in] numAASamples		The number of samples used while multisampling.
			@see DrawCommandBuffer::setAaSampleCount()
		*/
		void setAaDefaultSampleLocations(Gnm::MeasureDrawCommandBuffer *dcb, Gnm::NumSamples numAASamples);

		/** @brief A utility function that configures (for draw command buffers) the viewport, scissor, and guard band for the provided viewport dimensions.
			
			If more control is required, users can call the underlying functions manually.
			
			@param[in,out] dcb		The draw command buffer to write GPU commands to.
			@param[in] left			The X coordinate of the left edge of the rendering surface in pixels.
			@param[in] top			The Y coordinate of the top edge of the rendering surface in pixels.
			@param[in] right		The X coordinate of the right edge of the rendering surface in pixels.
			@param[in] bottom		The Y coordinate of the bottom edge of the rendering surface in pixels.
			@param[in] zScale		The scale value for the Z transform from clip-space to screen-space. The correct value depends on which
								convention you are following in your projection matrix. For OpenGL-style matrices, use <c><i>zScale</i></c> = 0.5. For Direct3D-style
								matrices, use <c><i>zScale</i></c> = 1.0.
			@param[in] zOffset		The offset value for the Z transform from clip-space to screen-space. The correct value depends on which
								convention you are following in your projection matrix. For OpenGL-style matrices, use <c><i>zOffset</i></c> = 0.5. For Direct3D-style
								matrices, use <c><i>zOffset</i></c> = 0.0.
		*/
		void setupScreenViewport(GnmxDrawCommandBuffer *dcb, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, float zScale, float zOffset);

		/** @brief Utility function that configures (for measure draw command buffers) the viewport, scissor, and guard band for the provided viewport dimensions.
			
			If more control is required, users can call the underlying functions manually.
			
			@param[in,out] dcb      The measure draw command buffer to write GPU commands to.
			@param[in] left			The X coordinate of the left edge of the rendering surface in pixels.
			@param[in] top			The Y coordinate of the top edge of the rendering surface in pixels.
			@param[in] right		The X coordinate of the right edge of the rendering surface in pixels.
			@param[in] bottom		The Y coordinate of the bottom edge of the rendering surface in pixels.
			@param[in] zScale		The scale value for the Z transform from clip-space to screen-space. The correct value depends on which
								convention you are following in your projection matrix. For OpenGL-style matrices, use <c><i>zScale</i></c> = 0.5. For Direct3D-style
								matrices, use <c><i>zScale</i></c> = 1.0.
			@param[in] zOffset		The offset value for the Z transform from clip-space to screen-space. The correct value depends on which
								convention you are following in your projection matrix. For OpenGL-style matrices, use <c><i>zOffset</i></c> = 0.5. For Direct3D-style
								matrices, use <c><i>zOffset</i></c> = 0.0.
		*/
		void setupScreenViewport(Gnm::MeasureDrawCommandBuffer *dcb, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, float zScale, float zOffset);

		/** @brief A wrapper around <c>dmaData()</c> to clear the values of one or more append/consume buffer counters (draw command) to the specified value.
		 *
		 * @param[in,out] dcb					The draw command buffer to write GPU commands to.
		 * @param[in] destRangeByteOffset		The byte offset in GDS to the beginning of the counter range to clear. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be updated. The valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>.
		 * @param[in] numApiSlots				The number of consecutive slots to update. <c><i>startApiSlot</i></c> + <c><i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
		 * @param[in] clearValue				The value to set the specified counters to.
		 *
		 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
		 *
		 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
		 * @note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DrawCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		*/
		void clearAppendConsumeCounters(GnmxDrawCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, uint32_t clearValue);

		/** @brief A wrapper around <c>dmaData()</c> to clear the values of one or more append/consume buffer counters (measure draw command) to the specified value.
		 *
		 * @param[in,out] dcb					The measure draw command buffer to write GPU commands to.
		 * @param[in] destRangeByteOffset		The byte offset in GDS to the beginning of the counter range to clear. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be updated. The valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>.
		 * @param[in] numApiSlots				The number of consecutive slots to update. <c><i>startApiSlot</i></c> + <c><i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
		 * @param[in] clearValue				The value to set the specified counters to.
		 *
		 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
 		 *
		 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		*/
		void clearAppendConsumeCounters(Gnm::MeasureDrawCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, uint32_t clearValue);

		/** @brief A wrapper around <c>dmaData()</c> to update the values of one or more append/consume buffer counters (draw command) using values sourced from the provided GPU-visible address.
		 *
		 * @param[in,out] dcb					The draw command buffer to write GPU commands to.
		 * @param[in] destRangeByteOffset		The byte offset in GDS to the beginning of the counter range to update. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be updated. The valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>
		 * @param[in] numApiSlots				The number of consecutive slots to update. <c><i>startApiSlot</i></c> + <c><i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
		 * @param[in] srcGpuAddr				The GPU-visible address to read the new counter values from.
		 *
		 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
		 *
		 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
		 * @note The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DrawCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		 */
		void writeAppendConsumeCounters(GnmxDrawCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, const void *srcGpuAddr);
		
		/** @brief A wrapper around <c>dmaData()</c> to update the values of one or more append/consume buffer counters (measure draw command) using values sourced from the provided GPU-visible address.
		 *
		 * @param[in,out] dcb					The measure draw command buffer to write GPU commands to.
		 * @param[in] destRangeByteOffset		The byte offset in GDS to the beginning of the counter range to update. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be updated. The valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>
		 * @param[in] numApiSlots				The number of consecutive slots to update. <c><i>startApiSlot</i></c> + <c><i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
		 * @param[in] srcGpuAddr				The GPU-visible address to read the new counter values from.
		 *
		 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
		 *
		 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		 */
		void writeAppendConsumeCounters(Gnm::MeasureDrawCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, const void *srcGpuAddr);

		/** @brief A wrapper around <c>dmaData()</c> to retrieve the values of one or more append/consume buffer counters (draw command) and store them in a GPU-visible address.
		 *
		 * @param[in,out] dcb					The draw command buffer to write GPU commands to.
		 * @param[out] destGpuAddr				The GPU-visible address to write the counter values to.
		 * @param[in] srcRangeByteOffset		The byte offset in GDS to the beginning of the counter range to read. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be read. The valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>.
		 * @param[in] numApiSlots				The number of consecutive slots to read. <c><i>startApiSlot</i></c> + <c><i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
		 *
		 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()
		 *
		 * @note  GDS accessible size is provided by Gnm::kGdsAccessibleMemorySizeInBytes.
		 * @note The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DrawCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		 */
		void readAppendConsumeCounters(GnmxDrawCommandBuffer *dcb, void *destGpuAddr, uint32_t srcRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots);
		
		/** @brief A wrapper around <c>dmaData()</c> to retrieve the values of one or more append/consume buffer counters (measure draw command) and store them in a GPU-visible address.
		 *
		 * @param[in,out] dcb					The measure command buffer to write GPU commands to.
		 * @param[out] destGpuAddr				The GPU-visible address to write the counter values to.
		 * @param[in] srcRangeByteOffset		The byte offset in GDS to the beginning of the counter range to read. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be read. The valid range is <c>[0..Gnm::kSlotCountRwResource -1]</c>.
		 * @param[in] numApiSlots				The number of consecutive slots to read. <c><i>startApiSlot</i></c> + <c><i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
		 *
		 * @see Gnm::DispatchCommandBuffer::dmaData(), Gnm::DrawCommandBuffer::dmaData()		 
		 *
		 * @note  GDS accessible size is provided by Gnm::kGdsAccessibleMemorySizeInBytes.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		 */		
		void readAppendConsumeCounters(Gnm::MeasureDrawCommandBuffer *dcb, void *destGpuAddr, uint32_t srcRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots);

		////////////// DispatchCommandBuffer helper functions

		/** @brief Uses the command processor's DMA engine to clear a buffer to specified value (like a GPU memset) using a dispatch command buffer.

			@param[in,out] dcb			The dispatch command buffer to which this method writes GPU commands.
			@param[out] dstGpuAddr		Destination address to which this method writes. Must not be <c>NULL</c>.
			@param[in] srcData			The value with which this method fills the destination buffer.
			@param[in] numBytes			Size of the destination buffer. Must be a multiple of 4.
			@param[in] isBlocking		Flag that specifies whether the CP will block while the transfer is active.
			@note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
			@note  This feature does not affect the GPU L2$ cache.
			@see   DispatchCommandBuffer::dmaData()
		*/
		void fillData(GnmxDispatchCommandBuffer *dcb, void *dstGpuAddr, uint32_t srcData, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking);
		
		/** @brief Uses the command processors DMA engine to clear a buffer to specified value (like a GPU memset) using a measure dispatch command buffer.

			@param[in,out] dcb			The measure dispatch command buffer to which this method writes GPU commands.
			@param[out] dstGpuAddr		Destination address to which this method writes. Must not be <c>NULL</c>.
			@param[in] srcData			The value with which this method fills the destination buffer.
			@param[in] numBytes			Size of the destination buffer. Must be a multiple of 4.
			@param[in] isBlocking		Flag that specifies whether the CP will block while the transfer is active.
			@note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
			@note  This feature does not affect the GPU L2$ cache.
			@see   DispatchCommandBuffer::dmaData()
		*/
		void fillData(Gnm::MeasureDispatchCommandBuffer *dcb, void *dstGpuAddr, uint32_t srcData, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking);

		/** @brief Uses the command processor's DMA engine to transfer data from a source address to a destination address in a dispatch command buffer.

			@param[in,out] dcb			The dispatch command buffer to which this method writes GPU commands.
			@param[out] dstGpuAddr		Destination address to which this method writes. Must not be <c>NULL</c>.
			@param[in] srcGpuAddr		Source address from which this method reads the data to copy. Must not be <c>NULL</c>.
			@param[in] numBytes         Number of bytes to transfer.
			@param[in] isBlocking       Flag that specifies whether the CP will block while the transfer is active.
			@note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
			@note  This feature does not affect the GPU L2$ cache.
			@see   DispatchCommandBuffer::dmaData()
		*/
		void copyData(GnmxDispatchCommandBuffer *dcb, void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking);
		
		/** @brief Uses the command processor's DMA engine to transfer data from a source address to a destination address in a measure dispatch command buffer.
			
			@param[in,out] dcb			The measure dispatch command buffer to which this method writes GPU commands.
			@param[out] dstGpuAddr		Destination address to which this method writes. Must not be <c>NULL</c>.
			@param[in] srcGpuAddr		Source address from which this method reads the data to copy. Must not be <c>NULL</c>.
			@param[in] numBytes			Number of bytes to transfer.
			@param[in] isBlocking		Flag that specifies whether the CP will block while the transfer is active.
			@note  Although command processor DMAs are launched in order, they are asynchronous to CU execution and will complete out-of-order to issued batches.
			@note  This feature does not affect the GPU L2$ cache.
			@see   DispatchCommandBuffer::dmaData()
		*/
		void copyData(Gnm::MeasureDispatchCommandBuffer *dcb, void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking);
		
		/** @brief Inserts user data directly inside the dispatch command buffer returning a locator for later reference.
			
			@param[in,out] dcb			The dispatch command buffer to write GPU commands to.
			@param[in] dataStream		A pointer to the data to embed.
			@param[in] sizeInDword		The size of the data in a stride of 4. Note that the maximum size of a single command packet is 2^16 bytes,
										and the effective maximum value of <c><i>sizeInDword</i></c> will be slightly less than that due to packet headers
										and padding.
			@param[in] alignment		The alignment of the embedded copy in the CB.

			@return						A pointer to the allocated buffer. 
		*/
		void* embedData(GnmxDispatchCommandBuffer *dcb, const void *dataStream, uint32_t sizeInDword, Gnm::EmbeddedDataAlignment alignment);

		/** @brief Inserts user data directly inside the measure dispatch command buffer returning a locator for later reference.

			@param[in,out] dcb			The measure dispatch command buffer to write GPU commands to.
			@param[in] dataStream		A pointer to the data to embed.
			@param[in] sizeInDword		The size of the data in a stride of 4. Note that the maximum size of a single command packet is 2^16 bytes,
										and the effective maximum value of <c><i>sizeInDword</i></c> will be slightly less than that due to packet headers
										and padding.
			@param[in] alignment		The alignment of the embedded copy in the CB.

			@return						A pointer to the allocated buffer. 
		*/		
		void* embedData(Gnm::MeasureDispatchCommandBuffer *dcb, const void *dataStream, uint32_t sizeInDword, Gnm::EmbeddedDataAlignment alignment);

		/** @brief A wrapper around <c>dmaData()</c> to clear the values of one or more append/consume buffer counters (dispatch command) to the specified value.
		 *
		 * @param[in,out] dcb					The dispatch command buffer to write GPU commands to.
		 * @param[in] destRangeByteOffset		The byte offset in GDS to the beginning of the counter range to clear. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be updated. The valid range is <c>[0..kSlotCountRwResource-1]</c>.
		 * @param[in] numApiSlots				The number of consecutive slots to update. <c><i>startApiSlot</i></c> + <c><i>numApiSlots</i></c> must be <c><= kSlotCountRwResource</c>.
		 * @param[in] clearValue				The value to set the specified counters to.
 		 *
		 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
		 * @note  The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		 */
		void clearAppendConsumeCounters(GnmxDispatchCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, uint32_t clearValue);
		
		/** @brief A wrapper around <c>dmaData()</c> to clear the values of one or more append/consume buffer counters (measure dispatch command) to the specified value.
		 *
		 * @param[in,out] dcb					The measure dispatch command buffer to write GPU commands to.
		 * @param[in] destRangeByteOffset		The byte offset in GDS to the beginning of the counter range to clear. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be updated. The valid range is <c>[0..kSlotCountRwResource-1]</c>.
		 * @param[in] numApiSlots				The number of consecutive slots to update. <c><i>startApiSlot</i></c> + <c><i>numApiSlots</i></c> must be <c><= kSlotCountRwResource</c>.
		 * @param[in] clearValue				The value to set the specified counters to.
 		 *
		 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		*/		
		void clearAppendConsumeCounters(Gnm::MeasureDispatchCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, uint32_t clearValue);

		/** @brief A wrapper around <c>dmaData()</c> to update the values of one or more append/consume buffer counters (dispatch command) using values source from the provided GPU-visible address.
		 *
		 * @param[in,out] dcb					The dispatch command buffer to write GPU commands to.
		 * @param[in] destRangeByteOffset		The byte offset in GDS to the beginning of the counter range to update. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be updated. The valid range is <c>[0..kSlotCountRwResource-1]</c>.
		 * @param[in] numApiSlots				The number of consecutive slots to update. <c><i>startApiSlot</i></c>+<c><i>numApiSlots</i></c> must be <c><= kSlotCountRwResource</c>.
		 * @param[in] srcGpuAddr				The GPU-visible address to read the new counter values from.
		 * @note The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		 */
		void writeAppendConsumeCounters(GnmxDispatchCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, const void *srcGpuAddr);
		
		/** @brief A wrapper around <c>dmaData()</c> to update the values of one or more append/consume buffer counters (measure dispatch command), using values source from the provided GPU-visible address.
		 *
		 * @param[in,out] dcb					The measure dispatch command buffer to write GPU commands to.
		 * @param[in] destRangeByteOffset		The byte offset in GDS to the beginning of the counter range to update. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be updated. The valid range is <c>[0..kSlotCountRwResource-1]</c>.
		 * @param[in] numApiSlots				The number of consecutive slots to update. <c><i>startApiSlot</i></c>+<c><i>numApiSlots</i></c> must be <c><= kSlotCountRwResource</c>.
		 * @param[in] srcGpuAddr				The GPU-visible address to read the new counter values from.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		 */
		void writeAppendConsumeCounters(Gnm::MeasureDispatchCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, const void *srcGpuAddr);

		/** @brief A wrapper around <c>dmaData()</c> to retrieve the values of one or more append/consume buffer counters (dispatch command) and store them in a GPU-visible address.
		 *
		 * @param[in,out] dcb						The dispatch command buffer to write GPU commands to.
		 * @param[out] destGpuAddr				The GPU-visible address to write the counter values to.
		 * @param[in] srcRangeByteOffset		The byte offset in GDS to the beginning of the counter range to read. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be read. The valid range is <c>[0..kSlotCountRwResource-1]</c>.
		 * @param[in] numApiSlots				The number of consecutive slots to read. <c><i>startApiSlot</i></c>+<c><i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
		 *
		 * @note  GDS accessible size is provided by Gnm::kGdsAccessibleMemorySizeInBytes.
		 * @note The implementation of this feature uses the dmaData() function that the Gnm library provides. See the notes for <c>DispatchCommandBuffer::dmaData()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		 */
		void readAppendConsumeCounters(GnmxDispatchCommandBuffer *dcb, void *destGpuAddr, uint32_t srcRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots);

		/** @brief A wrapper around <c>dmaData()</c> to retrieve the values of one or more append/consume buffer counters (measure dispatch command) and store them in a GPU-visible address.
		 *
		 * @param[in,out] dcb						The measure dispatch command buffer to write GPU commands to.
		 * @param[out] destGpuAddr				The GPU-visible address to write the counter values to.
		 * @param[in] srcRangeByteOffset		The byte offset in GDS to the beginning of the counter range to read. This must be a multiple of 4.
		 * @param[in] startApiSlot				The index of the first <c>RW_Buffer</c> API slot whose counter should be read. The valid range is <c>[0..kSlotCountRwResource-1]</c>.
		 * @param[in] numApiSlots				The number of consecutive slots to read. <c><i>startApiSlot</i></c>+<c><i>numApiSlots</i></c> must be less than or equal to Gnm::kSlotCountRwResource.
		 *
		 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
		 * @note To avoid unintended data corruption, ensure that this function does not use GDS ranges that overlap other, unrelated GDS ranges.
		 */		
		void readAppendConsumeCounters(Gnm::MeasureDispatchCommandBuffer *dcb, void *destGpuAddr, uint32_t srcRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots);

	}
}

#endif // !defined(_SCE_GNMX_HELPERS_H)
