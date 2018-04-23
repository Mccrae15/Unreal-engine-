/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if defined(__ORBIS__) // LCUE isn't supported offline

#include <sys/dmem.h>
#include <sys/errno.h>
#include <sceerror.h>

#include <gnmx/lwcue_validation.h>

using namespace sce::Gnm;
using namespace sce::Gnmx;
using namespace sce::Gnmx::LightweightConstantUpdateEngine;

ResourceBindingError LightweightConstantUpdateEngine::validateResourceMemoryArea(const Gnm::Buffer* buffer, int32_t memoryProtectionFlag)
{
	// Calculate the size of the memory region of a buffer we are validating, it is possible the buffer contains interleaved data
	void* pBeginAddr = buffer->getBaseAddress();

	void* pAddrForElementAfterEnd;
	// Stride of 0 is a special case
	if (buffer->getStride() == 0)
	{
		pAddrForElementAfterEnd = (void*)((uint64_t)pBeginAddr + buffer->getSize());
	}
	else
	{
		void* pStartOfLastElement = (void*)((uint64_t)pBeginAddr + buffer->getSize() - buffer->getStride());
		pAddrForElementAfterEnd = (void*)((uint64_t)pStartOfLastElement + buffer->getDataFormat().getBytesPerElement());
	}

	return validateResourceMemoryArea(pBeginAddr, (uint64_t)pAddrForElementAfterEnd - (uint64_t)pBeginAddr, memoryProtectionFlag);
}


ResourceBindingError LightweightConstantUpdateEngine::validateResourceMemoryArea(const Gnm::Texture* texture, int32_t memoryProtectionFlag)
{
	uint32_t err = kErrorOk;

	// Account for T# mip range clamping
	uint32_t baseMipLevel	= texture->getBaseMipLevel();
	uint32_t baseSliceIndex = texture->getBaseArraySliceIndex();
	uint32_t lastMipLevel	= texture->getLastMipLevel();
	uint32_t lastSliceIndex = texture->getLastArraySliceIndex();

	uint64_t baseMipOffset = 0, baseMipSizeInBytes = 0;
	uint64_t lastMipOffset = 0, lastMipSizeInBytes = 0;

	GpuAddress::computeTextureSurfaceOffsetAndSize(&baseMipOffset, &baseMipSizeInBytes, texture, baseMipLevel, baseSliceIndex);
	GpuAddress::computeTextureSurfaceOffsetAndSize(&lastMipOffset, &lastMipSizeInBytes, texture, lastMipLevel, lastSliceIndex);

	void* pTextureBaseAddr		= texture->getBaseAddress();
	void* pBeginMipRangeAddr	= (uint8_t*)(pTextureBaseAddr) + baseMipOffset;
	void* pEndMipRangeAddr		= (uint8_t*)(pTextureBaseAddr) + lastMipOffset + lastMipSizeInBytes - 1;

	bool bPrtBaseAddr = false;
	bool bPrtLastAddr = false;
	for (int i = 0; i < SCE_KERNEL_MAX_PRT_APERTURE_INDEX; i++)
	{
		void *pPrtAddr;
		size_t nPrtLength;
		sceKernelGetPrtAperture(i, &pPrtAddr, &nPrtLength);

		bPrtBaseAddr |= (((uint64_t)pBeginMipRangeAddr	>= (uint64_t)pPrtAddr) && ((uint64_t)pBeginMipRangeAddr <= ((uint64_t)pPrtAddr + nPrtLength)));
		bPrtLastAddr |= (((uint64_t)pEndMipRangeAddr	>= (uint64_t)pPrtAddr) && ((uint64_t)pEndMipRangeAddr	<= ((uint64_t)pPrtAddr + nPrtLength)));
	}

	// If the texture resides in a PRT aperture, ignore memory checks as they will always fail
	if (!bPrtBaseAddr && !bPrtLastAddr)
		err = validateResourceMemoryArea(pBeginMipRangeAddr, (uint64_t)pEndMipRangeAddr - (uint64_t)pBeginMipRangeAddr, memoryProtectionFlag);

	return (ResourceBindingError)err;
}


ResourceBindingError LightweightConstantUpdateEngine::validateResourceMemoryArea(const void* resourceBeginAddr, uint64_t resourceSizeInBytes, int32_t memoryProtectionFlag)
{
//	SCE_GNM_ASSERT(resourceSizeInBytes > 0);	// zero sized resources can be valid, if a user wants to bind a NULL buffer (only valid if the GPU will not read the resource)

	uint32_t err = kErrorOk;
	if (resourceBeginAddr == NULL || memoryProtectionFlag == 0)
		err = kErrorInvalidParameters;

	if(err == kErrorOk)
	{
		void* resourceEndAddr = (void*)((uint64_t)resourceBeginAddr + resourceSizeInBytes);
		do 
		{
			void* blockStart;
			void* blockEnd;
			int32_t blockProtection;
			int32_t status = sceKernelQueryMemoryProtection((void*)resourceBeginAddr, &blockStart, &blockEnd, &blockProtection);
			if (status != SCE_OK)
			{
				err = kErrorMemoryNotMapped;
				break;
			}
			if ((blockProtection & memoryProtectionFlag) != memoryProtectionFlag)
			{
				// Memory protection doesn't match the desired settings
				err = kErrorMemoryProtectionMismatch;
				break;
			}

			// Go to the next block
			resourceBeginAddr = (void*)( (uint64_t)blockEnd + 1 );

		} while (resourceBeginAddr < resourceEndAddr);
	}

	return (ResourceBindingError)err;
}


ResourceBindingError LightweightConstantUpdateEngine::validateGenericVsharp(const sce::Gnm::Buffer* buffer)
{
	uint32_t err = kErrorOk;
	
	if (!buffer->isBuffer())
		err |= kErrorVSharpIsNotValid;

	const Gnm::DataFormat dataFormat = buffer->getDataFormat();
	const uint32_t stride = buffer->getStride();

	// Check if data format is valid
	if (!dataFormat.supportsBuffer())
		err |= kErrorVSharpDataFormatIsNotValid;

	bool isStrideValid = (stride == 0 || stride >= dataFormat.getBytesPerElement());
	if (!isStrideValid)
		err |= kErrorVSharpStrideIsNotValid;

	return (ResourceBindingError)err;
}


ResourceBindingError LightweightConstantUpdateEngine::validateConstantBuffer(const sce::Gnm::Buffer* buffer)
{
	uint32_t err = validateGenericVsharp(buffer);
	err |= validateResourceMemoryArea(buffer, SCE_KERNEL_PROT_GPU_READ);

	// Stride and format are fixed for constant buffers
	if (buffer->getStride() != 16)
		err |= kErrorVSharpStrideIsNotValid;
	if (buffer->getDataFormat().m_asInt != Gnm::kDataFormatR32G32B32A32Float.m_asInt)
		err |= kErrorVSharpDataFormatIsNotValid;

	return (ResourceBindingError)err;
}


void LightweightConstantUpdateEngine::validateConstantBuffer(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Buffer* buffer)
{
	ResourceBindingError err = kErrorOk; 
	err = validateConstantBuffer(buffer);

	if (resourceCallback->m_func)
		resourceCallback->m_func(lwcue, resourceCallback->m_userData, apiSlot, kResourceConstantBuffer, err);
	else
		SCE_GNM_ASSERT_MSG(err == LightweightConstantUpdateEngine::kErrorOk, "Constant Buffer binding at apiSlot %d is invalid, binding error 0x%08X", apiSlot, err);
}


ResourceBindingError LightweightConstantUpdateEngine::validateVertexBuffer(const sce::Gnm::Buffer* buffer)
{
	uint32_t err = validateGenericVsharp(buffer);
	err |= validateResourceMemoryArea(buffer, SCE_KERNEL_PROT_GPU_READ);

	const Gnm::DataFormat dataFormat = buffer->getDataFormat();
	const uint32_t stride = buffer->getStride();
	const uint32_t numElements = buffer->getNumElements();

	// If stride is zero, numElements == dataFormat.getBytesPerElement
	bool isNumberElementsValid = (stride > 0 || numElements == 0 || numElements == dataFormat.getBytesPerElement());
	if (!isNumberElementsValid)
		err |= kErrorVSharpNumElementsIsNotValid;

	return (ResourceBindingError)err;
}


void LightweightConstantUpdateEngine::validateVertexBuffer(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Buffer* buffer)
{
	ResourceBindingError err = kErrorOk; 
	err = validateVertexBuffer(buffer);

	if (resourceCallback->m_func)
		resourceCallback->m_func(lwcue, resourceCallback->m_userData, apiSlot, kResourceVertexBuffer, err);
	else
		SCE_GNM_ASSERT_MSG(err == LightweightConstantUpdateEngine::kErrorOk, "Vertex Buffer binding at apiSlot %d is invalid, binding error 0x%08X", apiSlot, err);
}


ResourceBindingError LightweightConstantUpdateEngine::validateBuffer(const sce::Gnm::Buffer* buffer)
{
	uint32_t err = validateGenericVsharp(buffer);
	err |= validateResourceMemoryArea(buffer, SCE_KERNEL_PROT_GPU_READ);

	// DataBuffer, RegularBuffer, ByteBuffer
	const Gnm::DataFormat dataFormat = buffer->getDataFormat();
	bool isValidByteBuffer = (buffer->getStride() == 0 && dataFormat.m_asInt == Gnm::kDataFormatR8Uint.m_asInt);
	bool isValidDataBuffer = (buffer->getStride() > 0);
	bool isValidRegularBuffer = (dataFormat.m_asInt == Gnm::kDataFormatR32Float.m_asInt);
	if (!isValidByteBuffer && !isValidDataBuffer && !isValidRegularBuffer)
		err |= kErrorVSharpInvalidBufferType;

	return (ResourceBindingError)err;
}


void LightweightConstantUpdateEngine::validateBuffer(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Buffer* buffer)
{
	ResourceBindingError err = kErrorOk; 
	err = validateBuffer(buffer);

	if (resourceCallback->m_func)
		resourceCallback->m_func(lwcue, resourceCallback->m_userData, apiSlot, kResourceBuffer, err);
	else
		SCE_GNM_ASSERT_MSG(err == LightweightConstantUpdateEngine::kErrorOk, "Buffer binding at apiSlot %d is invalid, binding error 0x%08X", apiSlot, err);
}


ResourceBindingError LightweightConstantUpdateEngine::validateRwBuffer(const sce::Gnm::Buffer* buffer)
{
	uint32_t err = validateGenericVsharp(buffer);
	err |= validateResourceMemoryArea(buffer, SCE_KERNEL_PROT_GPU_READ|SCE_KERNEL_PROT_GPU_WRITE);
	
	if (buffer->getResourceMemoryType() == Gnm::kResourceMemoryTypeRO)
		err |=  kErrorResourceMemoryTypeMismatch;

	// ByteBuffer, DataBuffer, RegularBuffer
	const Gnm::DataFormat dataFormat = buffer->getDataFormat();
	bool isValidByteBuffer = (buffer->getStride() == 0 && dataFormat.m_asInt == Gnm::kDataFormatR8Uint.m_asInt);
	bool isValidDataBuffer = (buffer->getStride() > 0);
	bool isValidRegularBuffer = (dataFormat.m_asInt == Gnm::kDataFormatR32Float.m_asInt);
	if (!isValidByteBuffer && !isValidDataBuffer && !isValidRegularBuffer)
		err |= kErrorVSharpInvalidBufferType;

	return (ResourceBindingError)err;
}


void LightweightConstantUpdateEngine::validateRwBuffer(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Buffer* buffer)
{
	ResourceBindingError err = kErrorOk; 
	err = validateRwBuffer(buffer);

	if (resourceCallback->m_func)
		resourceCallback->m_func(lwcue, resourceCallback->m_userData, apiSlot, kResourceRwBuffer, err);
	else
		SCE_GNM_ASSERT_MSG(err == LightweightConstantUpdateEngine::kErrorOk, "RW Buffer binding at apiSlot %d is invalid, binding error 0x%08X", apiSlot, err);
}


ResourceBindingError LightweightConstantUpdateEngine::validateTexture(const sce::Gnm::Texture* texture)
{
	uint32_t err = kErrorOk;

	if (!texture->isTexture())
		err |= kErrorTSharpIsNotValid;

	err |= validateResourceMemoryArea(texture, SCE_KERNEL_PROT_GPU_READ);
	return (ResourceBindingError)err;
}


void LightweightConstantUpdateEngine::validateTexture(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Texture* texture)
{
	ResourceBindingError err = kErrorOk; 
	err = validateTexture(texture);

	if (resourceCallback->m_func)
		resourceCallback->m_func(lwcue, resourceCallback->m_userData, apiSlot, kResourceTexture, err);
	else
		SCE_GNM_ASSERT_MSG(err == LightweightConstantUpdateEngine::kErrorOk, "Texture binding at apiSlot %d is invalid, binding error 0x%08X", apiSlot, err);
}


ResourceBindingError LightweightConstantUpdateEngine::validateRwTexture(const sce::Gnm::Texture* texture)
{
	uint32_t err = kErrorOk;

	if (!texture->isTexture())
		err |= kErrorTSharpIsNotValid;

	err |= validateResourceMemoryArea(texture, SCE_KERNEL_PROT_GPU_READ|SCE_KERNEL_PROT_GPU_WRITE);
	if (texture->getResourceMemoryType() == Gnm::kResourceMemoryTypeRO)
		err |= kErrorResourceMemoryTypeMismatch;

	return (ResourceBindingError)err;
}


void LightweightConstantUpdateEngine::validateRwTexture(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Texture* texture)
{
	ResourceBindingError err = kErrorOk; 
	err = validateRwTexture(texture);

	if (resourceCallback->m_func)
		resourceCallback->m_func(lwcue, resourceCallback->m_userData, apiSlot, kResourceRwTexture, err);
	else
		SCE_GNM_ASSERT_MSG(err == LightweightConstantUpdateEngine::kErrorOk, "RW Texture binding at apiSlot %d is invalid, binding error 0x%08X", apiSlot, err);
}


ResourceBindingError LightweightConstantUpdateEngine::validateSampler(const sce::Gnm::Sampler* sampler)
{
	// There is no good way to validate a sampler, all values and combinations are valid
	(void)sampler;
	uint32_t err = kErrorOk;
	return (ResourceBindingError)err;
}


void LightweightConstantUpdateEngine::validateSampler(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, uint32_t apiSlot, const sce::Gnm::Sampler* sampler)
{
	ResourceBindingError err = kErrorOk; 
	err = validateSampler(sampler);

	if (resourceCallback->m_func)
		resourceCallback->m_func(lwcue, resourceCallback->m_userData, apiSlot, kResourceSampler, err);
	else
		SCE_GNM_ASSERT_MSG(err == LightweightConstantUpdateEngine::kErrorOk, "Sampler binding at apiSlot %d is invalid, binding error 0x%08X", apiSlot, err);
}


ResourceBindingError LightweightConstantUpdateEngine::validateGlobalInternalTableResource(Gnm::ShaderGlobalResourceType resourceType, const void* globalInternalTableMemory)
{
	// TODO
	(void)resourceType; (void)globalInternalTableMemory;
	SCE_GNM_ASSERT(false);

	uint32_t err = kErrorOk;
	return (ResourceBindingError)err;
}


ResourceBindingError LightweightConstantUpdateEngine::validateGlobalInternalResourceTableForShader(const void* globalInternalTableMemory, const void* gnmxShaderStruct)
{
	// TODO
	(void)globalInternalTableMemory; (void)gnmxShaderStruct;
	SCE_GNM_ASSERT(false);

	uint32_t err = kErrorOk;
	return (ResourceBindingError)err;
}


ResourceBindingError LightweightConstantUpdateEngine::validateShaderPreFetchArea(const void* binaryAddr, uint32_t binarySize)
{
	uint32_t err = kErrorOk;
	err |= validateResourceMemoryArea(binaryAddr, binarySize, SCE_KERNEL_PROT_GPU_READ|SCE_KERNEL_PROT_GPU_WRITE);
	if (err != kErrorOk)
		err |= kErrorShaderPrefetchArea;

	return (ResourceBindingError)err;
}


void LightweightConstantUpdateEngine::validateShaderPreFetchArea(const BaseConstantUpdateEngine* lwcue, ResourceErrorCallback* resourceCallback, const void* binaryAddr, uint32_t binarySize)
{
	ResourceBindingError err = kErrorOk; 
	err = validateShaderPreFetchArea(binaryAddr, binarySize);

	if (resourceCallback->m_func)
		resourceCallback->m_func(lwcue, resourceCallback->m_userData, 0, kResourceShader, err);
	else
		SCE_GNM_ASSERT_MSG(err == LightweightConstantUpdateEngine::kErrorOk, "Shader prefetch area is invalid, binding error 0x%08X", err);
}

#endif // defined(__ORBIS__)