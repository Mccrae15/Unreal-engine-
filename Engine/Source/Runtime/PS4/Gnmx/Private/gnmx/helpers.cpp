/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/
#include <gnmx/helpers.h>

#ifndef SCE_GNM_HP3D
#include <gnm/dispatchcommandbuffer.h>
#include <gnm/measuredispatchcommandbuffer.h>
#include <gnm/unsafedispatchcommandbuffer.h>
#endif // SCE_GNM_HP3D
#include <gnm/drawcommandbuffer.h>
#include <gnm/measuredrawcommandbuffer.h>
#include <gnm/unsafedrawcommandbuffer.h>
#include <gnm/constants.h>
#include <gnmx/shaderbinary.h>

#include <cmath>
#include <cstring>

using namespace sce::Gnm;
using namespace sce::Gnmx;

bool sce::Gnmx::isDataFormatValid(const Gnm::DataFormat &format)
{
	if (format.m_bits.m_surfaceFormat == kSurfaceFormatInvalid || format.m_bits.m_surfaceFormat > kSurfaceFormat1Reversed)
		return false; // invalid SurfaceFormat
	if (format.m_bits.m_channelType > kTextureChannelTypeUBScaled || format.m_bits.m_channelType == 8)
		return false; // invalid channel type
	int32_t numComponents = (int32_t)format.getNumComponents();
	if (numComponents < 0 || numComponents > 4)
		return false; // invalid (reserved) SurfaceFormat
	for(uint32_t iChan=0; iChan<4; ++iChan)
	{
		TextureChannel chan = format.getChannel(iChan);
		if (chan == 2 || chan == 3) // reserved, unused enum values
			return false; // invalid TextureChannel
	}
	return true;
}

void sce::Gnmx::computeVgtPrimitiveAndPatchCounts(uint32_t *outVgtPrimCount, uint32_t *outPatchCount, uint32_t maxHsVgprCount, uint32_t maxHsLdsBytes, const LsShader *lsb, const HsShader *hsb)
{
	SCE_GNM_ASSERT_MSG(lsb != 0, "lsb must not be NULL.");
	SCE_GNM_ASSERT_MSG(hsb != 0, "hsb must not be NULL.");
	SCE_GNM_ASSERT_MSG(outVgtPrimCount, "outVgtPrimCount must not be NULL.");
	SCE_GNM_ASSERT_MSG(outPatchCount, "outPatchCount must not be NULL.");
	
	// Determine numPatches per TG given the limits on HS resource usage
	uint32_t tempNumPatches = computeNumPatches(&hsb->m_hsStageRegisters, &hsb->m_hullStateConstants, lsb->m_lsStride, maxHsVgprCount, maxHsLdsBytes);
	SCE_GNM_ASSERT_MSG(tempNumPatches > 0 , 
					   "The requested VGPR and LDS limits cannot be satisfied for the given shaders. Please adjust maxHsVgprCount and maxHsLdsBytes accordingly.\n"
					   "  - maxHsVgprCount must be greater or equal than: %u\n"
					   "  - maxHsLdsBytes must be greater or equal than:  %u\n",
					   hsb->getNumVgprs(),
					   hsb->m_hsStageRegisters.isOffChip() ? sce::Gnm::computeLdsUsagePerPatchInBytesPerThreadGroupOffChip(&hsb->m_hullStateConstants, lsb->m_lsStride) : sce::Gnm::computeLdsUsagePerPatchInBytesPerThreadGroup(&hsb->m_hullStateConstants, lsb->m_lsStride));

	// The GPU contains two VGTs and for good results we wish to toggle between the two
	// for every (approximately) 256 vertices. Vertex caching is disabled when using tessellation
	// so we can determine the max. number of input patches to 256 vertices by the following
	uint32_t vgtPrimCount = 256 / hsb->m_hullStateConstants.m_numInputCP;

	// It is most efficient to send a number of patches through a VGT which is a multiple
	// of numPatches since the TGs are formed by the VGTs. Otherwise we get
	// a partially filled TG just before each toggle between VGTs.
	if(vgtPrimCount > tempNumPatches)
	{
		vgtPrimCount -= (vgtPrimCount % tempNumPatches); // closest multiple
		*outPatchCount = tempNumPatches; // patch count is unchanged
	}
	else
	{
		*outPatchCount = vgtPrimCount; // if numPatches>vgtPrimCount, reduse LDS consumption by adjusting patch count
	}

	if (Gnm::getGpuMode() == kGpuModeNeo)
		vgtPrimCount = (vgtPrimCount+1) & ~1; // must be a multiple of 2 in NEO mode.
	*outVgtPrimCount = vgtPrimCount;
}


void sce::Gnmx::computeVgtPrimitiveCountAndAdjustNumPatches(uint32_t *outVgtPrimCount, uint32_t *inoutPatchCount, const HsShader *hsb)
{
	SCE_GNM_ASSERT_MSG(hsb != 0, "hsb must not be NULL.");
	SCE_GNM_ASSERT_MSG(outVgtPrimCount, "outVgtPrimCount must not be NULL.");
	SCE_GNM_ASSERT_MSG(inoutPatchCount, "inoutPatchCount must not be NULL.");
	SCE_GNM_ASSERT_MSG(*inoutPatchCount > 0 , "The input value of inoutPatchCount must greater than 0.");

	const uint32_t requestedNumPatchCount = *inoutPatchCount;

	// The GPU contains two VGTs and for good results we wish to toggle between the two
	// for every (approximately) 256 vertices. Vertex caching is disabled when using tessellation
	// so we can determine the max. number of input patches to 256 vertices by the following
	uint32_t vgtPrimCount = 256 / hsb->m_hullStateConstants.m_numInputCP;

	// It is most efficient to send a number of patches through a VGT which is a multiple
	// of numPatches since the TGs are formed by the VGTs. Otherwise we get
	// a partially filled TG just before each toggle between VGTs.
	if ( vgtPrimCount > requestedNumPatchCount )
	{
		vgtPrimCount -= (vgtPrimCount % requestedNumPatchCount); // closest multiple
	}
	else
	{
		*inoutPatchCount = vgtPrimCount; // if numPatches>vgtPrimCount, reduse LDS consumption by adjusting patch count
	}

	if (Gnm::getGpuMode() == kGpuModeNeo)
		vgtPrimCount = (vgtPrimCount+1) & ~1; // must be a multiple of 2 in NEO mode
	*outVgtPrimCount = vgtPrimCount;
}

bool sce::Gnmx::computeOnChipGsConfiguration(uint32_t *outLdsSizeIn512Bytes, uint32_t *outGsPrimsPerSubGroup, EsShader const* esb, GsShader const* gsb, uint32_t maxLdsUsage)
{
	SCE_GNM_ASSERT_MSG(esb != NULL, "esb must not be NULL.");
	SCE_GNM_ASSERT_MSG(gsb != NULL, "gsb must not be NULL.");
	SCE_GNM_ASSERT_MSG(maxLdsUsage <= 64*1024, "maxLdsUsage must be no greater than 64KB.");
	SCE_GNM_ASSERT_MSG(outLdsSizeIn512Bytes != NULL && outGsPrimsPerSubGroup != NULL, "outLdsSizeIn512Bytes and outGsPrimsPerSubGroup must not be NULL.");
	uint32_t gsSizePerPrim = gsb->getSizePerPrimitiveInBytes();
	if (gsSizePerPrim > maxLdsUsage) {
		*outGsPrimsPerSubGroup = *outLdsSizeIn512Bytes = 0;
		return false;
	}
	uint32_t gsPrimsPerSubGroup = maxLdsUsage / gsSizePerPrim;
	// Creating on-chip GS sub-groups with a multiple of 64 will ensure that all ES and GS
	// wavefronts will all have full thread utilization.
	// Creating on-chip-GS sub-groups with greater than 64 primitives might slightly
	// increase the thread utilization of VS wavefronts, but it is usually better to create
	// the smallest possible sub-groups that can reach maximum thread occupancy, as
	// smaller sub-groups will do a better job of finding holes and filling GPU resources.

	// For the specific case of Patch32 input topology, 64 GS prims * 32 input vertices/prim =
	// 2048 ES vertex would just exceed the GPU maximum of 2047 ES verts per sub-group, and
	// we must reduce the GS prims per sub-group to 63.
	uint32_t maxGsPrimsPerSubGroupForInputVertexCount = (gsb->m_inputVertexCountMinus1 == 31) ? 63 : 64;

	if (gsPrimsPerSubGroup > maxGsPrimsPerSubGroupForInputVertexCount)
		gsPrimsPerSubGroup = maxGsPrimsPerSubGroupForInputVertexCount;
	*outGsPrimsPerSubGroup = gsPrimsPerSubGroup;
	uint32_t ldsSizeInBytes = (gsPrimsPerSubGroup * gsSizePerPrim);
	*outLdsSizeIn512Bytes = (ldsSizeInBytes + 511) / 512;
	return true;
}

void sce::Gnmx::fillData(GnmxDrawCommandBuffer *dcb, void *dstGpuAddr, uint32_t srcData, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL");
	SCE_GNM_ASSERT_MSG(numBytes % 4 == 0, "numBytes (0x%08X) must be a multiple of 4 bytes.", numBytes);

	if ( numBytes == 0 )
		return;

	const uint32_t	maximizedDmaCount = numBytes / Gnm::kDmaMaximumSizeInBytes;
	const uint32_t	partialDmaSize    = numBytes - maximizedDmaCount * Gnm::kDmaMaximumSizeInBytes;

	const uint32_t	numNonBlockingDmas = maximizedDmaCount - (partialDmaSize? 0 : 1);
	const uint32_t	finalDmaSize	   = partialDmaSize ? partialDmaSize : Gnm::kDmaMaximumSizeInBytes;

	for (uint32_t iNonBlockingDma = 0; iNonBlockingDma < numNonBlockingDmas; ++iNonBlockingDma)
	{
		dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
					 Gnm::kDmaDataSrcData, (uint64_t)srcData,
					 Gnm::kDmaMaximumSizeInBytes, Gnm::kDmaDataBlockingDisable); // only the final transfer is blocking
		dstGpuAddr = (uint8_t*)dstGpuAddr + Gnm::kDmaMaximumSizeInBytes;
	}

	dcb->dmaData(Gnm::kDmaDataDstMemory,  (uint64_t)dstGpuAddr,
				 Gnm::kDmaDataSrcData, (uint64_t)srcData,
				 finalDmaSize, isBlocking);
}
void sce::Gnmx::fillData(MeasureDrawCommandBuffer *dcb, void *dstGpuAddr, uint32_t srcData, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL");
	SCE_GNM_ASSERT_MSG(numBytes % 4 == 0, "numBytes (0x%08X) must be a multiple of 4 bytes.", numBytes);

	if ( numBytes == 0 )
		return;

	const uint32_t	maximizedDmaCount = numBytes / Gnm::kDmaMaximumSizeInBytes;
	const uint32_t	partialDmaSize    = numBytes - maximizedDmaCount * Gnm::kDmaMaximumSizeInBytes;

	const uint32_t	numNonBlockingDmas = maximizedDmaCount - (partialDmaSize? 0 : 1);
	const uint32_t	finalDmaSize	   = partialDmaSize ? partialDmaSize : Gnm::kDmaMaximumSizeInBytes;

	for (uint32_t iNonBlockingDma = 0; iNonBlockingDma < numNonBlockingDmas; ++iNonBlockingDma)
	{
		dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
			Gnm::kDmaDataSrcData, (uint64_t)srcData,
			Gnm::kDmaMaximumSizeInBytes, Gnm::kDmaDataBlockingDisable); // only the final transfer is blocking
		dstGpuAddr = (uint8_t*)dstGpuAddr + Gnm::kDmaMaximumSizeInBytes;
	}

	dcb->dmaData(Gnm::kDmaDataDstMemory,  (uint64_t)dstGpuAddr,
		Gnm::kDmaDataSrcData, (uint64_t)srcData,
		finalDmaSize, isBlocking);
}

void sce::Gnmx::copyData(GnmxDrawCommandBuffer *dcb, void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL");
	if ( numBytes == 0 )
		return;

	const uint32_t		maximizedDmaCount  = numBytes / Gnm::kDmaMaximumSizeInBytes;
	const uint32_t		partialDmaSize	   = numBytes - maximizedDmaCount * Gnm::kDmaMaximumSizeInBytes;

	const uint32_t		numNonBlockingDmas = maximizedDmaCount - (partialDmaSize? 0 : 1);
	const uint32_t		finalDmaSize	   = partialDmaSize ? partialDmaSize : Gnm::kDmaMaximumSizeInBytes;

	for (uint32_t iNonBlockingDma = 0; iNonBlockingDma < numNonBlockingDmas; ++iNonBlockingDma)
	{
		dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
			Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
			Gnm::kDmaMaximumSizeInBytes, Gnm::kDmaDataBlockingDisable); // only the final transfer is blocking
		dstGpuAddr = (uint8_t*)dstGpuAddr + Gnm::kDmaMaximumSizeInBytes;
		srcGpuAddr = (uint8_t*)srcGpuAddr + Gnm::kDmaMaximumSizeInBytes;
	}

	dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
		Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
		finalDmaSize, isBlocking);
}
void sce::Gnmx::copyData(MeasureDrawCommandBuffer *dcb, void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL");
	if ( numBytes == 0 )
		return;

	const uint32_t		maximizedDmaCount  = numBytes / Gnm::kDmaMaximumSizeInBytes;
	const uint32_t		partialDmaSize	   = numBytes - maximizedDmaCount * Gnm::kDmaMaximumSizeInBytes;

	const uint32_t		numNonBlockingDmas = maximizedDmaCount - (partialDmaSize? 0 : 1);
	const uint32_t		finalDmaSize	   = partialDmaSize ? partialDmaSize : Gnm::kDmaMaximumSizeInBytes;

	for (uint32_t iNonBlockingDma = 0; iNonBlockingDma < numNonBlockingDmas; ++iNonBlockingDma)
	{
		dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
			Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
			Gnm::kDmaMaximumSizeInBytes, Gnm::kDmaDataBlockingDisable); // only the final transfer is blocking
		dstGpuAddr = (uint8_t*)dstGpuAddr + Gnm::kDmaMaximumSizeInBytes;
		srcGpuAddr = (uint8_t*)srcGpuAddr + Gnm::kDmaMaximumSizeInBytes;
	}

	dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
		Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
		finalDmaSize, isBlocking);
}

void* sce::Gnmx::embedData(GnmxDrawCommandBuffer *dcb, const void *dataStream, uint32_t sizeInDword, EmbeddedDataAlignment alignment)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL");
	SCE_GNM_ASSERT_MSG(sizeInDword <= UINT32_MAX/sizeof(uint32_t), "sizeInDword (%u) is too large.", sizeInDword);
	const uint32_t sizeInByte = sizeInDword*sizeof(uint32_t);
	void* dest = dcb->allocateFromCommandBuffer(sizeInByte,alignment);
	if (dest != NULL)
	{
		memcpy(dest, dataStream, sizeInByte);
	}
	return dest;
}
void* sce::Gnmx::embedData(MeasureDrawCommandBuffer *dcb, const void *, uint32_t sizeInDword, EmbeddedDataAlignment alignment)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL");
	SCE_GNM_ASSERT_MSG(sizeInDword <= UINT32_MAX/sizeof(uint32_t), "sizeInDword (%u) is too large.", sizeInDword);
	const uint32_t sizeInByte = sizeInDword*sizeof(uint32_t);
	dcb->allocateFromCommandBuffer(sizeInByte,alignment);
	return NULL;
	
}

void sce::Gnmx::fillAaDefaultSampleLocations(AaSampleLocationControl *locationControl, uint32_t *maxSampleDistance, Gnm::NumSamples numAASamples)
{
	SCE_GNM_ASSERT_MSG(locationControl, "locationControl must not be NULL");
	SCE_GNM_ASSERT_MSG(maxSampleDistance, "maxSampleDistance must not be NULL");

	uint32_t msaaSampleLocs[16] = {0};
	uint64_t centroidPriority = 0;
	if (numAASamples == Gnm::kNumSamples16)
	{
		msaaSampleLocs[0]  = 0x5BB137D9;
		msaaSampleLocs[1]  = 0x1FF5739D;
		msaaSampleLocs[2]  = 0x6E8224A8;
		msaaSampleLocs[3]  = 0x0AC640EC;
		msaaSampleLocs[4]  = 0x5BB137D9;
		msaaSampleLocs[5]  = 0x1FF5739D;
		msaaSampleLocs[6]  = 0x6E8224A8;
		msaaSampleLocs[7]  = 0x0AC640EC;
		msaaSampleLocs[8]  = 0x5BB137D9;
		msaaSampleLocs[9]  = 0x1FF5739D;
		msaaSampleLocs[10] = 0x6E8224A8;
		msaaSampleLocs[11] = 0x0AC640EC;
		msaaSampleLocs[12] = 0x5BB137D9;
		msaaSampleLocs[13] = 0x1FF5739D;
		msaaSampleLocs[14] = 0x6E8224A8;
		msaaSampleLocs[15] = 0x0AC640EC;
		centroidPriority   = 0xA85410FEB632DC97ULL;
		*maxSampleDistance = 8;
	}
	else if (numAASamples == Gnm::kNumSamples8)
	{
		msaaSampleLocs[0]  = 0x5BB137D9;
		msaaSampleLocs[1]  = 0x1FF5739D;
		msaaSampleLocs[2]  = 0x00000000;
		msaaSampleLocs[3]  = 0x00000000;
		msaaSampleLocs[4]  = 0x5BB137D9;
		msaaSampleLocs[5]  = 0x1FF5739D;
		msaaSampleLocs[6]  = 0x00000000;
		msaaSampleLocs[7]  = 0x00000000;
		msaaSampleLocs[8]  = 0x5BB137D9;
		msaaSampleLocs[9]  = 0x1FF5739D;
		msaaSampleLocs[10] = 0x00000000;
		msaaSampleLocs[11] = 0x00000000;
		msaaSampleLocs[12] = 0x5BB137D9;
		msaaSampleLocs[13] = 0x1FF5739D;
		msaaSampleLocs[14] = 0x00000000;
		msaaSampleLocs[15] = 0x00000000;
		centroidPriority   = 0x0000000054106327LL;
		*maxSampleDistance = 7;
	}
	else if (numAASamples == Gnm::kNumSamples4)
	{
		msaaSampleLocs[0]  = 0x22EEA66A;
		msaaSampleLocs[1]  = 0x00000000;
		msaaSampleLocs[2]  = 0x00000000;
		msaaSampleLocs[3]  = 0x00000000;
		msaaSampleLocs[4]  = 0x22EEA66A;
		msaaSampleLocs[5]  = 0x00000000;
		msaaSampleLocs[6]  = 0x00000000;
		msaaSampleLocs[7]  = 0x00000000;
		msaaSampleLocs[8]  = 0x22EEA66A;
		msaaSampleLocs[9]  = 0x00000000;
		msaaSampleLocs[10] = 0x00000000;
		msaaSampleLocs[11] = 0x00000000;
		msaaSampleLocs[12] = 0x22EEA66A;
		msaaSampleLocs[13] = 0x00000000;
		msaaSampleLocs[14] = 0x00000000;
		msaaSampleLocs[15] = 0x00000000;
		centroidPriority   = 0x0000000000001032LL;
		*maxSampleDistance = 6;
	}
	else if (numAASamples == Gnm::kNumSamples2)
	{
		msaaSampleLocs[0]  = 0x0000C44C;
		msaaSampleLocs[1]  = 0x00000000;
		msaaSampleLocs[2]  = 0x00000000;
		msaaSampleLocs[3]  = 0x00000000;
		msaaSampleLocs[4]  = 0x0000C44C;
		msaaSampleLocs[5]  = 0x00000000;
		msaaSampleLocs[6]  = 0x00000000;
		msaaSampleLocs[7]  = 0x00000000;
		msaaSampleLocs[8]  = 0x0000C44C;
		msaaSampleLocs[9]  = 0x00000000;
		msaaSampleLocs[10] = 0x00000000;
		msaaSampleLocs[11] = 0x00000000;
		msaaSampleLocs[12] = 0x0000C44C;
		msaaSampleLocs[13] = 0x00000000;
		msaaSampleLocs[14] = 0x00000000;
		msaaSampleLocs[15] = 0x00000000;
		centroidPriority   = 0x0000000000000010LL;
		*maxSampleDistance = 4;
	}
	else if (numAASamples == Gnm::kNumSamples1)
	{
		centroidPriority = 0;
		*maxSampleDistance = 0;
	}

	locationControl->init(msaaSampleLocs, centroidPriority);
}

void sce::Gnmx::setAaDefaultSampleLocations(GnmxDrawCommandBuffer *dcb, NumSamples numAASamples)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL");

	uint32_t maxSampleDistance = 0;
	AaSampleLocationControl locationControl;
	Gnmx::fillAaDefaultSampleLocations(&locationControl, &maxSampleDistance, numAASamples);

	dcb->setAaSampleLocationControl(&locationControl);
	dcb->setAaSampleCount(numAASamples, maxSampleDistance);

}
void sce::Gnmx::setAaDefaultSampleLocations(MeasureDrawCommandBuffer *dcb, NumSamples )
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL");

	Gnm::AaSampleLocationControl locationControl;
	dcb->setAaSampleLocationControl(&locationControl);
	dcb->setAaSampleCount(Gnm::kNumSamples1, 0);
}

void sce::Gnmx::setupScreenViewport(GnmxDrawCommandBuffer *dcb, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, float zScale, float zOffset)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	int32_t width = right - left;
	int32_t height = bottom - top;
	SCE_GNM_ASSERT_MSG(width > 0 && width <= 16384, "right (%u) - left (%u) must be in the range [1..16384].", right, left);
	SCE_GNM_ASSERT_MSG(height > 0 && height <= 16384, "bottom (%u) - top (%u) must be in the range [1..16384].", bottom, top);

	// viewport transform
	// Set viewport for to the entire render surface, with a screen-space Z range of 0 to 1
	float scale[3] = {(float)(width)*0.5f, - (float)(height)*0.5f, zScale};
	float offset[3] = {(float)(left + width*0.5f), (float)(top + height*0.5f), zOffset};
	dcb->setViewport(0, 0.0f, 1.0f, scale, offset);
	// Disable viewport pass-through (which would prevent the viewport transform from occurring at all)
	Gnm::ViewportTransformControl vpc;
	vpc.init();
	vpc.setPassThroughEnable(false);
	dcb->setViewportTransformControl(vpc);

	// Set screen scissor to cover the entire render surface. It is CRITICAL that the scissor region not
	// extend beyond the bounds of the render surface, or else memory corruption and crashes will occur.
	dcb->setScreenScissor(left, top, right, bottom);

	// Set the guard band offset so that the guard band is centered around the viewport region.
	// 10xx limits hardware offset to multiples of 16 pixels.
	// Primitive filtering further restricts this offset to a multiple of 64 pixels.
	int hwOffsetX = SCE_GNM_MIN(508, (int)floor(offset[0]/16.0f + 0.5f)) & ~0x3;
	int hwOffsetY = SCE_GNM_MIN(508, (int)floor(offset[1]/16.0f + 0.5f)) & ~0x3;
	dcb->setHardwareScreenOffset(hwOffsetX, hwOffsetY);

	// Set the guard band clip distance to the maximum possible values by calculating the minimum distance
	// from the closest viewport edge to the edge of the hardware's coordinate space
	float hwMin = -(float)((1<<23) - 0) / (float)(1<<8);
	float hwMax =  (float)((1<<23) - 1) / (float)(1<<8);
	float gbMaxX = SCE_GNM_MIN(hwMax - fabsf(scale[0]) - offset[0] + hwOffsetX*16, -fabsf(scale[0]) + offset[0] - hwOffsetX*16 - hwMin);
	float gbMaxY = SCE_GNM_MIN(hwMax - fabsf(scale[1]) - offset[1] + hwOffsetY*16, -fabsf(scale[1]) + offset[1] - hwOffsetY*16 - hwMin);
	float gbHorizontalClipAdjust = gbMaxX / fabsf(scale[0]);
	float gbVerticalClipAdjust   = gbMaxY / fabsf(scale[1]);
	dcb->setGuardBands(gbHorizontalClipAdjust, gbVerticalClipAdjust, 1.0f, 1.0f);
}

void sce::Gnmx::setupScreenViewport(MeasureDrawCommandBuffer *dcb, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, float zScale, float zOffset)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	int32_t width = right - left;
	int32_t height = bottom - top;
	SCE_GNM_ASSERT_MSG(width  > 0 && width  <= 16384, "right (%u) - left (%u) must be in the range [1..16384].", right, left);
	SCE_GNM_ASSERT_MSG(height > 0 && height <= 16384, "bottom (%u) - top (%u) must be in the range [1..16384].", bottom, top);

	// viewport transform
	// Set viewport for to the entire render surface, with a screen-space Z range of 0 to 1
	float scale[3] = {(float)(width)*0.5f, - (float)(height)*0.5f, zScale};
	float offset[3] = {(float)(left + width*0.5f), (float)(top + height*0.5f), zOffset};
	dcb->setViewport(0, 0.0f, 1.0f, scale, offset);
	// Disable viewport pass-through (which would prevent the viewport transform from occurring at all)
	Gnm::ViewportTransformControl vpc;
	vpc.init();
	vpc.setPassThroughEnable(false);
	dcb->setViewportTransformControl(vpc);

	// Set screen scissor to cover the entire render surface. It is CRITICAL that the scissor region not
	// extend beyond the bounds of the render surface, or else memory corruption and crashes will occur.
	dcb->setScreenScissor(left, top, right, bottom);

	// Set the guard band offset so that the guard band is centered around the viewport region.
	// 10xx limits hardware offset to multiples of 16 pixels.
	// Primitive filtering further restricts this offset to a multiple of 64 pixels.
	int hwOffsetX = SCE_GNM_MIN(508, (int)floor(offset[0]/16.0f + 0.5f)) & ~0x3;
	int hwOffsetY = SCE_GNM_MIN(508, (int)floor(offset[1]/16.0f + 0.5f)) & ~0x3;
	dcb->setHardwareScreenOffset(hwOffsetX, hwOffsetY);

	// Set the guard band clip distance to the maximum possible values by calculating the minimum distance
	// from the closest viewport edge to the edge of the hardware's coordinate space
	float hwMin = -(float)((1<<23) - 0) / (float)(1<<8);
	float hwMax =  (float)((1<<23) - 1) / (float)(1<<8);
	float gbMaxX = SCE_GNM_MIN(hwMax - fabsf(scale[0]) - offset[0] + hwOffsetX*16, -fabsf(scale[0]) + offset[0] - hwOffsetX*16 - hwMin);
	float gbMaxY = SCE_GNM_MIN(hwMax - fabsf(scale[1]) - offset[1] + hwOffsetY*16, -fabsf(scale[1]) + offset[1] - hwOffsetY*16 - hwMin);
	float gbHorizontalClipAdjust = gbMaxX / fabsf(scale[0]);
	float gbVerticalClipAdjust   = gbMaxY / fabsf(scale[1]);
	dcb->setGuardBands(gbHorizontalClipAdjust, gbVerticalClipAdjust, 1.0f, 1.0f);
}

void sce::Gnmx::clearAppendConsumeCounters(GnmxDrawCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, uint32_t clearValue)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
// 	SCE_GNM_ASSERT_MSG(startApiSlot < sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, sce::Gnm::kSlotCountRwResource-1);
// 	SCE_GNM_ASSERT_MSG(numApiSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numApiSlots, sce::Gnm::kSlotCountRwResource);
// 	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%d].", startApiSlot, numApiSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	return dcb->dmaData(Gnm::kDmaDataDstGds, destRangeByteOffset + startApiSlot*sizeof(uint32_t),
		Gnm::kDmaDataSrcData, clearValue,
		numApiSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}
void sce::Gnmx::clearAppendConsumeCounters(MeasureDrawCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, uint32_t clearValue)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
// 	SCE_GNM_ASSERT_MSG(startApiSlot < sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, sce::Gnm::kSlotCountRwResource-1);
// 	SCE_GNM_ASSERT_MSG(numApiSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numApiSlots, sce::Gnm::kSlotCountRwResource);
// 	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%d].", startApiSlot, numApiSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	dcb->dmaData(Gnm::kDmaDataDstGds, destRangeByteOffset + startApiSlot*sizeof(uint32_t),
		Gnm::kDmaDataSrcData, clearValue,
		numApiSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}

void sce::Gnmx::writeAppendConsumeCounters(GnmxDrawCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, const void *srcGpuAddr)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
// 	SCE_GNM_ASSERT_MSG(startApiSlot < sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, sce::Gnm::kSlotCountRwResource-1);
// 	SCE_GNM_ASSERT_MSG(numApiSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numApiSlots, sce::Gnm::kSlotCountRwResource);
// 	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%d].", startApiSlot, numApiSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	return dcb->dmaData(Gnm::kDmaDataDstGds, destRangeByteOffset + startApiSlot*sizeof(uint32_t),
		Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
		numApiSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}
void sce::Gnmx::writeAppendConsumeCounters(MeasureDrawCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots, const void *srcGpuAddr)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
// 	SCE_GNM_ASSERT_MSG(startApiSlot < sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, sce::Gnm::kSlotCountRwResource-1);
// 	SCE_GNM_ASSERT_MSG(numApiSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numApiSlots, sce::Gnm::kSlotCountRwResource);
// 	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%d].", startApiSlot, numApiSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	dcb->dmaData(Gnm::kDmaDataDstGds, destRangeByteOffset + startApiSlot*sizeof(uint32_t),
		Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
		numApiSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}

void sce::Gnmx::readAppendConsumeCounters(GnmxDrawCommandBuffer *dcb, void *destGpuAddr, uint32_t srcRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
// 	SCE_GNM_ASSERT_MSG(startApiSlot < sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, sce::Gnm::kSlotCountRwResource-1);
// 	SCE_GNM_ASSERT_MSG(numApiSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numApiSlots, sce::Gnm::kSlotCountRwResource);
// 	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%d].", startApiSlot, numApiSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	return dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)destGpuAddr,
		Gnm::kDmaDataSrcGds, srcRangeByteOffset + startApiSlot*sizeof(uint32_t),
		numApiSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}
void sce::Gnmx::readAppendConsumeCounters(MeasureDrawCommandBuffer *dcb, void *destGpuAddr, uint32_t srcRangeByteOffset, uint32_t startApiSlot, uint32_t numApiSlots)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
// 	SCE_GNM_ASSERT_MSG(startApiSlot < sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, sce::Gnm::kSlotCountRwResource-1);
// 	SCE_GNM_ASSERT_MSG(numApiSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numApiSlots, sce::Gnm::kSlotCountRwResource);
// 	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= sce::Gnm::kSlotCountRwResource, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%d].", startApiSlot, numApiSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)destGpuAddr,
		Gnm::kDmaDataSrcGds, srcRangeByteOffset + startApiSlot*sizeof(uint32_t),
		numApiSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef SCE_GNM_HP3D
void sce::Gnmx::fillData(GnmxDispatchCommandBuffer *dcb, void *dstGpuAddr, uint32_t srcData, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	SCE_GNM_ASSERT_MSG(numBytes % 4 == 0, "numBytes (%u) must be a multiple of 4 bytes.", numBytes);

	const uint32_t	maximizedDmaCount = numBytes / Gnm::kDmaMaximumSizeInBytes;
	const uint32_t	partialDmaSize    = numBytes - maximizedDmaCount * Gnm::kDmaMaximumSizeInBytes;

	const uint32_t	numNonBlockingDmas = maximizedDmaCount - (partialDmaSize? 0 : 1);
	const uint32_t	finalDmaSize	   = partialDmaSize ? partialDmaSize : maximizedDmaCount;

	for (uint32_t iNonBlockingDma = 0; iNonBlockingDma < numNonBlockingDmas; ++iNonBlockingDma)
	{
		dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
						Gnm::kDmaDataSrcData, srcData,
						Gnm::kDmaMaximumSizeInBytes, Gnm::kDmaDataBlockingDisable);
		dstGpuAddr = (uint8_t*)dstGpuAddr + Gnm::kDmaMaximumSizeInBytes;
	}

	dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
					Gnm::kDmaDataSrcData, srcData,
					finalDmaSize, isBlocking);
}
void sce::Gnmx::fillData(MeasureDispatchCommandBuffer *dcb, void *dstGpuAddr, uint32_t srcData, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	SCE_GNM_ASSERT_MSG(numBytes % 4 == 0, "numBytes (%u) must be a multiple of 4 bytes.", numBytes);

	const uint32_t	maximizedDmaCount = numBytes / Gnm::kDmaMaximumSizeInBytes;
	const uint32_t	partialDmaSize    = numBytes - maximizedDmaCount * Gnm::kDmaMaximumSizeInBytes;

	const uint32_t	numNonBlockingDmas = maximizedDmaCount - (partialDmaSize? 0 : 1);
	const uint32_t	finalDmaSize	   = partialDmaSize ? partialDmaSize : maximizedDmaCount;

	for (uint32_t iNonBlockingDma = 0; iNonBlockingDma < numNonBlockingDmas; ++iNonBlockingDma)
	{
		dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
			Gnm::kDmaDataSrcData, srcData,
			Gnm::kDmaMaximumSizeInBytes, Gnm::kDmaDataBlockingDisable);
		dstGpuAddr = (uint8_t*)dstGpuAddr + Gnm::kDmaMaximumSizeInBytes;
	}

	dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
		Gnm::kDmaDataSrcData, srcData,
		finalDmaSize, isBlocking);
}
void sce::Gnmx::copyData(GnmxDispatchCommandBuffer *dcb, void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");

	const uint32_t		maximizedDmaCount  = numBytes / Gnm::kDmaMaximumSizeInBytes;
	const uint32_t		partialDmaSize	   = numBytes - maximizedDmaCount * Gnm::kDmaMaximumSizeInBytes;

	const uint32_t		numNonBlockingDmas = maximizedDmaCount - (partialDmaSize? 0 : 1);
	const uint32_t		finalDmaSize	   = partialDmaSize ? partialDmaSize : maximizedDmaCount;

	for (uint32_t iNonBlockingDma = 0; iNonBlockingDma < numNonBlockingDmas; ++iNonBlockingDma)
	{
		dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
						Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
						Gnm::kDmaMaximumSizeInBytes, Gnm::kDmaDataBlockingDisable);
		dstGpuAddr = (uint8_t*)dstGpuAddr + Gnm::kDmaMaximumSizeInBytes;
		srcGpuAddr = (uint8_t*)srcGpuAddr + Gnm::kDmaMaximumSizeInBytes;
	}
	dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
					Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
					finalDmaSize, isBlocking);
}
void sce::Gnmx::copyData(MeasureDispatchCommandBuffer *dcb, void *dstGpuAddr, const void *srcGpuAddr, uint32_t numBytes, Gnm::DmaDataBlockingMode isBlocking)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");

	const uint32_t		maximizedDmaCount  = numBytes / Gnm::kDmaMaximumSizeInBytes;
	const uint32_t		partialDmaSize	   = numBytes - maximizedDmaCount * Gnm::kDmaMaximumSizeInBytes;

	const uint32_t		numNonBlockingDmas = maximizedDmaCount - (partialDmaSize? 0 : 1);
	const uint32_t		finalDmaSize	   = partialDmaSize ? partialDmaSize : maximizedDmaCount;

	for (uint32_t iNonBlockingDma = 0; iNonBlockingDma < numNonBlockingDmas; ++iNonBlockingDma)
	{
		dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
			Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
			Gnm::kDmaMaximumSizeInBytes, Gnm::kDmaDataBlockingDisable);
		dstGpuAddr = (uint8_t*)dstGpuAddr + Gnm::kDmaMaximumSizeInBytes;
		srcGpuAddr = (uint8_t*)srcGpuAddr + Gnm::kDmaMaximumSizeInBytes;
	}
	dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)dstGpuAddr,
		Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
		finalDmaSize, isBlocking);
}
void* sce::Gnmx::embedData(GnmxDispatchCommandBuffer *dcb, const void *dataStream, uint32_t sizeInDword, sce::Gnm::EmbeddedDataAlignment alignment)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	const uint32_t sizeInByte = sizeInDword*sizeof(uint32_t);
	void* dest = dcb->allocateFromCommandBuffer(sizeInByte,alignment);
	if (dest != NULL)
	{
		memcpy(dest, dataStream, sizeInByte);
	}

	return dest;
}
void* sce::Gnmx::embedData(MeasureDispatchCommandBuffer *dcb, const void *, uint32_t sizeInDword, sce::Gnm::EmbeddedDataAlignment alignment)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
	const uint32_t sizeInByte = sizeInDword*sizeof(uint32_t);
	dcb->allocateFromCommandBuffer(sizeInByte,alignment);
	
	return NULL;
}
void sce::Gnmx::clearAppendConsumeCounters(GnmxDispatchCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startSlot, uint32_t numSlots, uint32_t clearValue)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
//	SCE_GNM_ASSERT_MSG(startSlot < sce::Gnm::kSlotCountRwResource, "startSlot (%u) must be in the range [0..%d].", startSlot, sce::Gnm::kSlotCountRwResource-1);
//	SCE_GNM_ASSERT_MSG(numSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numSlots, sce::Gnm::kSlotCountRwResource);
//	SCE_GNM_ASSERT_MSG(startSlot+numSlots <= sce::Gnm::kSlotCountRwResource, "startSlot (%u) + numSlots (%u) must be in the range [0..%d].", startSlot, numSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	return dcb->dmaData(Gnm::kDmaDataDstGds, destRangeByteOffset + startSlot*sizeof(uint32_t),
						   Gnm::kDmaDataSrcData, clearValue,
						   numSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}
void sce::Gnmx::clearAppendConsumeCounters(MeasureDispatchCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startSlot, uint32_t numSlots, uint32_t clearValue)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
//	SCE_GNM_ASSERT_MSG(startSlot < sce::Gnm::kSlotCountRwResource, "startSlot (%u) must be in the range [0..%d].", startSlot, sce::Gnm::kSlotCountRwResource-1);
//	SCE_GNM_ASSERT_MSG(numSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numSlots, sce::Gnm::kSlotCountRwResource);
//	SCE_GNM_ASSERT_MSG(startSlot+numSlots <= sce::Gnm::kSlotCountRwResource, "startSlot (%u) + numSlots (%u) must be in the range [0..%d].", startSlot, numSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	dcb->dmaData(Gnm::kDmaDataDstGds, destRangeByteOffset + startSlot*sizeof(uint32_t),
		Gnm::kDmaDataSrcData, clearValue,
		numSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}
void sce::Gnmx::writeAppendConsumeCounters(GnmxDispatchCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startSlot, uint32_t numSlots, const void *srcGpuAddr)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
//	SCE_GNM_ASSERT_MSG(startSlot < sce::Gnm::kSlotCountRwResource, "startSlot (%u) must be in the range [0..%d].", startSlot, sce::Gnm::kSlotCountRwResource-1);
//	SCE_GNM_ASSERT_MSG(numSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numSlots, sce::Gnm::kSlotCountRwResource);
//	SCE_GNM_ASSERT_MSG(startSlot+numSlots <= sce::Gnm::kSlotCountRwResource, "startSlot (%u) + numSlots (%u) must be in the range [0..%d].", startSlot, numSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	return dcb->dmaData(Gnm::kDmaDataDstGds, destRangeByteOffset + startSlot*sizeof(uint32_t),
						   Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
						   numSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}
void sce::Gnmx::writeAppendConsumeCounters(MeasureDispatchCommandBuffer *dcb, uint32_t destRangeByteOffset, uint32_t startSlot, uint32_t numSlots, const void *srcGpuAddr)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
//	SCE_GNM_ASSERT_MSG(startSlot < sce::Gnm::kSlotCountRwResource, "startSlot (%u) must be in the range [0..%d].", startSlot, sce::Gnm::kSlotCountRwResource-1);
//	SCE_GNM_ASSERT_MSG(numSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numSlots, sce::Gnm::kSlotCountRwResource);
//	SCE_GNM_ASSERT_MSG(startSlot+numSlots <= sce::Gnm::kSlotCountRwResource, "startSlot (%u) + numSlots (%u) must be in the range [0..%d].", startSlot, numSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	dcb->dmaData(Gnm::kDmaDataDstGds, destRangeByteOffset + startSlot*sizeof(uint32_t),
		Gnm::kDmaDataSrcMemory, (uint64_t)srcGpuAddr,
		numSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}

void sce::Gnmx::readAppendConsumeCounters(GnmxDispatchCommandBuffer *dcb, void *destGpuAddr, uint32_t srcRangeByteOffset, uint32_t startSlot, uint32_t numSlots)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
//	SCE_GNM_ASSERT_MSG(startSlot < sce::Gnm::kSlotCountRwResource, "startSlot (%u) must be in the range [0..%d].", startSlot, sce::Gnm::kSlotCountRwResource-1);
//	SCE_GNM_ASSERT_MSG(numSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numSlots, sce::Gnm::kSlotCountRwResource);
//	SCE_GNM_ASSERT_MSG(startSlot+numSlots <= sce::Gnm::kSlotCountRwResource, "startSlot (%u) + numSlots (%u) must be in the range [0..%d].", startSlot, numSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	return dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)destGpuAddr,
						   Gnm::kDmaDataSrcGds, srcRangeByteOffset + startSlot*sizeof(uint32_t),
						   numSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}
void sce::Gnmx::readAppendConsumeCounters(MeasureDispatchCommandBuffer *dcb, void *destGpuAddr, uint32_t srcRangeByteOffset, uint32_t startSlot, uint32_t numSlots)
{
	SCE_GNM_ASSERT_MSG(dcb, "dcb must not be NULL.");
//	SCE_GNM_ASSERT_MSG(startSlot < sce::Gnm::kSlotCountRwResource, "startSlot (%u) must be in the range [0..%d].", startSlot, sce::Gnm::kSlotCountRwResource-1);
//	SCE_GNM_ASSERT_MSG(numSlots <= sce::Gnm::kSlotCountRwResource, "numSlots (%u) must be in the range [0..%d].", numSlots, sce::Gnm::kSlotCountRwResource);
//	SCE_GNM_ASSERT_MSG(startSlot+numSlots <= sce::Gnm::kSlotCountRwResource, "startSlot (%u) + numSlots (%u) must be in the range [0..%d].", startSlot, numSlots, sce::Gnm::kSlotCountRwResource);
	// dmaData knows what ring it's running on, and addresses the appropriate segment of GDS accordingly.
	dcb->dmaData(Gnm::kDmaDataDstMemory, (uint64_t)destGpuAddr,
		Gnm::kDmaDataSrcGds, srcRangeByteOffset + startSlot*sizeof(uint32_t),
		numSlots*sizeof(uint32_t), Gnm::kDmaDataBlockingEnable);
}
#endif // SCE_GNM_HP3D
