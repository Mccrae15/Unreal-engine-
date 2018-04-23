/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if !defined(_SCE_GNMX_LWCUE_VALIDATION_H)
#define _SCE_GNMX_LWCUE_VALIDATION_H

#include <stdint.h>
#include <gnm.h>

namespace sce
{
	namespace Gnmx
	{
		namespace LightweightConstantUpdateEngine
		{
			class BaseConstantUpdateEngine;

			/** @brief Error codes returned when resource bindings fail */
			typedef enum ResourceBindingError
			{
				kErrorOk							= 0x00000000,	///< No error found.

				// Validation method parameters
				kErrorInvalidParameters				= 0x00000001,	///< Parameters passed to the validation function are not valid.

				// Memory errors
				kErrorMemoryNotMapped				= 0x00000100,	///< Used memory is not mapped.
				kErrorMemoryProtectionMismatch		= 0x00000200,	///< Used memory is mapped but with different protection flags from the specified ones.
				kErrorResourceMemoryTypeMismatch	= 0x00000400,	///< Used memory type in resource is not compatible with the resource type (e.g. using RO memory type with a RWTexture).
				kErrorShaderPrefetchArea			= 0x00000800,	///< Used memory is not safe for shader prefetching into L2. Disable setShaderCodePrefetchEnable() or map the area to GPU read/write.
		
				// V# errors
				kErrorVSharpDataFormatIsNotValid	= 0x00001000,	///< V# has an invalid data format.
				kErrorVSharpStrideIsNotValid		= 0x00002000,	///< V# has an invalid stride.
				kErrorVSharpNumElementsIsNotValid	= 0x00004000,	///< V# has an invalid number of elements.
				kErrorVSharpInvalidBufferType		= 0x00010000,	///< V# used as either Byte/Data/RegularBuffer but invalid for all of those types.

				// # not valid
				kErrorVSharpIsNotValid				= 0x01000000,	///< V# is not valid (likely not initialized).
				kErrorTSharpIsNotValid				= 0x02000000,	///< T# is not valid (likely not initialized).
				kErrorSSharpIsNotValid				= 0x04000000,	///< S# is not valid (likely not initialized).

			} ResourceBindingError;

			/** @brief Type of resource binding for LCUE validation error reports */
			typedef enum ResourceBindingType
			{
				kResourceBuffer			= 0x00000001, 
				kResourceRwBuffer		= 0x00000002,
				kResourceVertexBuffer	= 0x00000003,
				kResourceConstantBuffer = 0x00000004,
				kResourceTexture		= 0x00000005,
				kResourceRwTexture		= 0x00000006,
				kResourceSampler		= 0x00000007,
				kResourceGlobalTable	= 0x00000008,
				kResourceGlobalResource	= 0x00000009,
				kResourceShader			= 0x0000000a,
				kResourceUnknown		= 0xFFFFFFFF
			} ResourceBindingType;

#if !defined(DOXYGEN_IGNORE)

			/** @brief Defines a function to call when the LCUE detects a resource binding error.
			 *
			 *  @param[in]	lwcue			An instance of the LCUE calling the callback.
			 *  @param[in]	userData		User data passed to the callback function.
			 *  @param[in]	apiSlot			Gnm API slot of the binding error, note if <c>bindingType</c> equals kResourceShader, <c>apiSlot</c> will always be 0.
			 *  @param[in]	bindingType		Resource type for binding error.
			 *  @param[in]	bindingError	Error the LCUE detected.
			 */
			typedef void (*lwcueValidationErrorBufferCallback)(const BaseConstantUpdateEngine* lwcue, void* userData, uint32_t apiSlot, ResourceBindingType bindingType, ResourceBindingError bindingError);


			/** @brief Represents a callback function pointer and data for LCUE resource binding errors */
			class ResourceErrorCallback
			{
			public:
				lwcueValidationErrorBufferCallback m_func;		///< A function to call when a resource binding error occurs.
				void*							   m_userData;	///< The user defined data.
			};

			
			/** @brief Checks the memory area used by the resource is mapped and has the correct protection flags
			 *
			 *  @param[in] buffer The buffer (V#) object to validate.
			 *  @param[in] memoryProtectionFlag	The memory protection flag of the resource.
			 *
			 *  @return Returns kErrorOk (=0) if no error is found; otherwise it returns a combination of errors defined by ResourceBindingError.
			 */
			ResourceBindingError validateResourceMemoryArea(const sce::Gnm::Buffer* buffer, int32_t memoryProtectionFlag);

			
			/** @brief Checks the memory area used by the resource is mapped and has the correct protection flags.
			 *
			 *  @param[in] texture				The Texture (T#) object to validate
			 *  @param[in] memoryProtectionFlag	The memory protection flag of the resource
			 *
			 *  @return kErrorOk (=0) if no error is found, otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateResourceMemoryArea(const sce::Gnm::Texture* texture, int32_t memoryProtectionFlag);

			
			/** @brief Check that the memory area used by the resource is mapped and has the correct protection flags.
			 *
			 *  @param[in] resourceBeginAddr	The base address of the resource.
			 *  @param[in] resourceSizeInBytes	The size of the resource in bytes
			 *  @param[in] memoryProtectionFlag	The memory protection flag of the resource.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateResourceMemoryArea(const void* resourceBeginAddr, uint64_t resourceSizeInBytes, int32_t memoryProtectionFlag);

			
			/** @brief Checks for valid parameters specific to all V#'s.
			 *
			 *  @param[in] buffer	The Generic V# to be checked.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateGenericVsharp(const sce::Gnm::Buffer* buffer);

			
			/** @brief Checks that a valid constant buffer is bound.
			 *
			 *  @param[in] buffer	The constant buffer object to be checked.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateConstantBuffer(const sce::Gnm::Buffer* buffer);
		

			/** @brief Checks that a valid constant buffer is bound, and calls a user registered callback if available.
			 *
			 *  @param[in] lwcue			Instance of the LCUE calling the validator.
			 *  @param[in] resourceCallback Registered callback for resource error reports.
			 *  @param[in] apiSlot			Gnm API slot of the resource.
			 *  @param[in] buffer			Resource buffer to check.
			 *
			 *  @see BaseConstantUpdateEngine::setResourceBufferErrorCallback()
			 */
			void validateConstantBuffer(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Buffer* buffer);

			
			/** @brief Checks that a valid vertex buffer is bound.
			 *
			 *  @param[in] buffer	The vertex buffer object to be checked.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateVertexBuffer(const sce::Gnm::Buffer* buffer);


			/** @brief Checks that a valid vertex buffer is bound, and calls a user registered callback if available.
			 *
			 *  @param[in] lwcue			Instance of the LCUE calling the validator.
			 *  @param[in] resourceCallback Registered callback for resource error reports.
			 *  @param[in] apiSlot			Gnm API slot of the resource.
			 *  @param[in] buffer			Resource buffer to check.
			 *
			 *  @see BaseConstantUpdateEngine::setResourceBufferErrorCallback()
			 */
			void validateVertexBuffer(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Buffer* buffer);

			
			/** @brief Check that a valid buffer is bound.
			 *
			 *  @param[in] buffer	The buffer object to be checked.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateBuffer(const sce::Gnm::Buffer* buffer);


			/** @brief Checks that a valid buffer is bound, and calls a user registered callback if available.
			 *
			 *  @param[in] lwcue			Instance of the LCUE calling the validator.
			 *  @param[in] resourceCallback Registered callback for resource error reports.
			 *  @param[in] apiSlot			Gnm API slot of the resource.
			 *  @param[in] buffer			Resource buffer to check.
			 *
			 *  @see BaseConstantUpdateEngine::setResourceBufferErrorCallback()
			 */
			void validateBuffer(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Buffer* buffer);


			/** @brief Checks that a valid read/write buffer is bound.
			 *
			 *  @param[in] buffer	The read/write object to be checked.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateRwBuffer(const sce::Gnm::Buffer* buffer);


			/** @brief Checks that a valid RW buffer is bound, and calls a user registered callback if available.
			 *
			 *  @param[in] lwcue			Instance of the LCUE calling the validator.
			 *  @param[in] resourceCallback Registered callback for resource error reports.
			 *  @param[in] apiSlot			Gnm API slot of the resource.
			 *  @param[in] buffer			Resource buffer to check.
			 *
			 *  @see BaseConstantUpdateEngine::setResourceBufferErrorCallback()
			 */
			void validateRwBuffer(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Buffer* buffer);

			
			/** @brief Checks that a valid texture is bound.
			 *
			 *  @param[in] texture The Texture object to be checked
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateTexture(const sce::Gnm::Texture* texture);


			/** @brief Checks that a valid texture is bound, and calls a user registered callback if available.
			 *
			 *  @param[in] lwcue			Instance of the LCUE calling the validator.
			 *  @param[in] resourceCallback Registered callback for resource error reports.
			 *  @param[in] apiSlot			Gnm API slot of the resource.
			 *  @param[in] texture			Resource texture to check.
			 *
			 *  @see BaseConstantUpdateEngine::setResourceBufferErrorCallback()
			 */
			SCE_GNM_FORCE_INLINE void validateTexture(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Texture* texture);

			
			/** @brief Checks that a valid read/write texture is bound.
			 *
			 *  @param[in] texture The read/write texture buffer object to be checked.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateRwTexture(const sce::Gnm::Texture* texture);


			/** @brief Checks that a valid RW texture is bound, and calls a user registered callback if available.
			 *
			 *  @param[in] lwcue			Instance of the LCUE calling the validator.
			 *  @param[in] resourceCallback Registered callback for resource error reports.
			 *  @param[in] apiSlot			Gnm API slot of the resource.
			 *  @param[in] texture			Resource texture to check.
			 *
			 *  @see BaseConstantUpdateEngine::setResourceBufferErrorCallback()
			 */
			SCE_GNM_FORCE_INLINE void validateRwTexture(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Texture* texture);


			/** @brief Checks that a valid sampler is bound.
			 *
			 *  @param[in] sampler The sampler object to be checked.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateSampler(const sce::Gnm::Sampler* sampler);


			/** @brief Checks that a valid sampler is bound, and calls a user registered callback if available.
			 *
			 *  @param[in] lwcue			Instance of the LCUE calling the validator.
			 *  @param[in] resourceCallback Registered callback for resource error reports.
			 *  @param[in] apiSlot			Gnm API slot of the resource.
			 *  @param[in] sampler			Resource sampler to check.
			 *
			 *  @see BaseConstantUpdateEngine::setResourceBufferErrorCallback()
			 */
			void validateSampler(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Sampler* sampler);

			
			/** @brief Checks a single entry in the Global Internal Descriptor Table.
			 *
			 *  @param[in] resourceType					The global resource type.
			 *  @param[in] globalInternalTableMemory	A pointer to the global internal table.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateGlobalInternalTableResource(sce::Gnm::ShaderGlobalResourceType resourceType, const void* globalInternalTableMemory);

			
			/** @brief Checks all items in the Global Internal Table that are used by the specified shader.
			 *
			 *  @param[in] globalInternalTableMemory	A pointer to the global internal table.
			 *  @param[in] gnmxShaderStruct				A pointer to the gnmx shader structure.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 */
			ResourceBindingError validateGlobalInternalResourceTableForShader(const void* globalInternalTableMemory, const void* gnmxShaderStruct);

			
			/** @brief Checks the location of the shader binary is located in GPU read/write memory for prefetching.
			 *
			 *  @param[in] binaryAddr	A pointer to the shader binary ucode.
			 *  @param[in] binarySize	The size of the shader binary.
			 *
			 *  @return kErrorOk (=0) if no error is found; otherwise a combination of errors defined by ResourceBindingError is returned.
			 *
			 *  @note By default shader prefetching is enabled. Call setShaderCodePrefetchEnable() to disable this feature 
			 *        or ensure the GPU has read and write access to the memory region.
			 */
			ResourceBindingError validateShaderPreFetchArea(const void* binaryAddr, uint32_t binarySize);


			/** @brief Checks that a valid shader is bound for prefetching, and calls a user registered callback if available.
			 *
			 *  @param[in] lwcue			Instance of the LCUE calling the validator.
			 *  @param[in] resourceCallback Registered callback for resource error reports.
			 *  @param[in] binaryAddr		Base address of shader microcode to prefetch.
			 *  @param[in] binarySize		Shader microcode size.
			 *
			 *  @see BaseConstantUpdateEngine::setResourceBufferErrorCallback()
			 */
			void validateShaderPreFetchArea(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, const void* binaryAddr, uint32_t binarySize);


#endif // !defined(DOXYGEN_IGNORE) 
		} // LightweightConstantUpdateEngine
	} // Gnmx
} // sce

#endif // _SCE_GNMX_LWCUE_VALIDATION_H
