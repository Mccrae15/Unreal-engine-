/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx/dispatchdraw.h>

#define DISPATCH_DRAW_OPTIMIZE_INDEX_COMPARES_BY_KEEPING_RANGE 1

int64_t sce::Gnmx::getMaxSizeofDispatchDrawInputData(DispatchDrawTriangleCullIndexData *pDispatchDrawTriangleCullIndexData, Gnm::PrimitiveType primType, uint32_t numIndicesIn)
{
	if ((primType != Gnm::kPrimitiveTypeTriList && primType != Gnm::kPrimitiveTypeTriStrip) || pDispatchDrawTriangleCullIndexData == NULL)
		return kDispatchDrawErrorInvalidArguments;
	memset(pDispatchDrawTriangleCullIndexData, 0, sizeof(DispatchDrawTriangleCullIndexData));
	pDispatchDrawTriangleCullIndexData->m_magic = kDispatchDrawTriangleCullIndexDataMagic;
	pDispatchDrawTriangleCullIndexData->m_versionMajor = kDispatchDrawTriangleCullIndexDataVersionMajor;
	pDispatchDrawTriangleCullIndexData->m_versionMinor = kDispatchDrawTriangleCullIndexDataVersionMinor;
	if (numIndicesIn < 3)
		return 0;
	uint32_t numBlocksMax = 0;
	size_t sizeofIndexDataOutMax = 0;
	if (primType == Gnm::kPrimitiveTypeTriList) {
		// Worst case scenario for triangle list is no vertex reuse at all
		// In this case, we end up with each 85 triangles using 255 vertices forming a block
		// plus one final block with numTri <= 85 and numTri*3 vertices:
		uint32_t numTrianglesIn = (numIndicesIn / 3);
		numBlocksMax = numTrianglesIn/85;
		sizeofIndexDataOutMax = ((4 + 255*2 + 85*3 + 1 + 0x3) &~0x3)*numBlocksMax;
		if (numTrianglesIn > numBlocksMax*85) {
			uint32_t numTriLastBlock = numTrianglesIn - numBlocksMax*85;
			++numBlocksMax;
			sizeofIndexDataOutMax += ((4 + numTriLastBlock*3*2 + numTriLastBlock*3 + (numTriLastBlock & 1) + 0x3) &~0x3);
		}
		sizeofIndexDataOutMax += numBlocksMax*4;
	} else
	if (primType == Gnm::kPrimitiveTypeTriStrip) {
		// Worst case scenario for triangle strip is no vertex reuse except normal strip reuse, with no strip restarts - 
		// each strip restart decreases both the total number of blocks (by decreasing the total triangle count) and 
		// the size of the blocks (because a restart decreases reuse and therefore the number of triangles which will
		// fit in a block which uses 256 vertices).
		// In this case, we end up with each 254 triangles using 256 vertices forming a block
		// plus one final block with numTri <= 254 and numTri+2 vertices:
		uint32_t numTrianglesIn = (numIndicesIn - 2);
		numBlocksMax = numTrianglesIn/254;
		sizeofIndexDataOutMax = ((4 + 256*2 + 254*3 + 0x3) &~0x3)*numBlocksMax;
		if (numTrianglesIn > numBlocksMax*254) {
			uint32_t numTriLastBlock = numTrianglesIn - numBlocksMax*254;
			++numBlocksMax;
			sizeofIndexDataOutMax += ((4 + (numTriLastBlock + 2)*2 + numTriLastBlock*3 + (numTriLastBlock & 1) + 0x3) &~0x3);
		}
		sizeofIndexDataOutMax += numBlocksMax*4;
	}
	pDispatchDrawTriangleCullIndexData->m_numIndexDataBlocks = numBlocksMax;
	return sizeofIndexDataOutMax;
}

int64_t sce::Gnmx::getSizeofDispatchDrawInputData(DispatchDrawTriangleCullIndexData *pDispatchDrawTriangleCullIndexData, Gnm::PrimitiveType primType, uint32_t numIndicesIn, uint16_t const* pIndicesIn)
{
	if ((primType != Gnm::kPrimitiveTypeTriList && primType != Gnm::kPrimitiveTypeTriStrip) || pIndicesIn == NULL || pDispatchDrawTriangleCullIndexData == NULL)
		return kDispatchDrawErrorInvalidArguments;
	uint16_t aVtx[256];
	size_t sizeofIndexDataOut = 0;
	uint32_t numBlocks = 0;
	uint32_t numVtx = 0;
	uint32_t numTri = 0;
	uint16_t maxIndex = 0, maxIndexInBlock = 0;
#if DISPATCH_DRAW_OPTIMIZE_INDEX_COMPARES_BY_KEEPING_RANGE
	uint16_t minIndexInBlock = 0xFFFF;
#endif
	
	uint16_t const* pTriIn = pIndicesIn;
	uint16_t const* pTriInEnd = pIndicesIn + numIndicesIn;
	uint32_t nWindingXor = 0;	// 0 or 3 alternating
	while (pTriIn+2 < pTriInEnd) {
		if (!(pTriIn[0] == pTriIn[1] || pTriIn[0] == pTriIn[2] || pTriIn[1] == pTriIn[2])) {	// skip degenerate triangles
			uint16_t aVtxNew[3];
			uint32_t numVtxNew = 0;
			uint32_t nWindingXorInTri = 0;
			for (uint32_t nVertInTri = 0; nVertInTri < 3; ++nVertInTri) {
				uint16_t index = pTriIn[nVertInTri ^ nWindingXorInTri];
				nWindingXorInTri = nWindingXor;
				uint32_t nVertInVtx;
#if DISPATCH_DRAW_OPTIMIZE_INDEX_COMPARES_BY_KEEPING_RANGE
				// use a range check to avoid iterating over all previously seen indices if the new index is not in [minIndexInBlock:maxIndexInBlock]
				if (index < minIndexInBlock || index > maxIndexInBlock)
					nVertInVtx = numVtx;
				else
#endif
				{
					for (nVertInVtx = 0; nVertInVtx < numVtx; ++nVertInVtx)
						if (aVtx[nVertInVtx] == index)
							break;
				}
				if (nVertInVtx == numVtx)
					aVtxNew[numVtxNew++] = index;
			}
			if (numVtx + numVtxNew > 256 || numTri+1 > 512) {
				// no room for the new unique vertices or no room for another triangle, so dump block:
				numBlocks++;
				sizeofIndexDataOut += (2*sizeof(uint16_t) + numVtx*sizeof(uint16_t) + numTri*3 + 3) & ~0x3;
				// track overall maxIndex
				if (maxIndex < maxIndexInBlock)
					maxIndex = maxIndexInBlock;
				// advance to next block
				numTri = numVtx = 0;
				// write the new vertices which didn't fit as the first vertices in the next block
				aVtxNew[0] = pTriIn[0], aVtxNew[1] = pTriIn[1 ^ nWindingXor], aVtxNew[2] = pTriIn[2 ^ nWindingXor];
				numVtxNew = 3;
#if DISPATCH_DRAW_OPTIMIZE_INDEX_COMPARES_BY_KEEPING_RANGE
				// reset [minIndexInBlock:maxIndexInBlock] to empty for new block
				minIndexInBlock = 0xFFFF;
#endif
				maxIndexInBlock = 0;
			}
			for (uint32_t nVtxNew = 0; nVtxNew < numVtxNew; ++nVtxNew) {
				uint16_t index = aVtxNew[nVtxNew];
				aVtx[numVtx++] = index;
#if DISPATCH_DRAW_OPTIMIZE_INDEX_COMPARES_BY_KEEPING_RANGE
				// track [minIndexInBlock:maxIndexInBlock], range of indices in aVtx[]
				if (minIndexInBlock > index)
					minIndexInBlock = index;
#endif
				if (maxIndexInBlock < index)
					maxIndexInBlock = index;
			}
			++numTri;
		}
		if (primType == Gnm::kPrimitiveTypeTriStrip) {
			nWindingXor ^= 3;
			++pTriIn;
		} else
			pTriIn += 3;
	}
	if (numTri > 0) {
		// dump final partial block if there are any triangles:
		numBlocks++;
		sizeofIndexDataOut += (2*sizeof(uint16_t) + numVtx*sizeof(uint16_t) + numTri*3 + 3) & ~0x3;
		// track overall maxIndex
		if (maxIndex < maxIndexInBlock)
			maxIndex = maxIndexInBlock;
	} else if (numBlocks == 0 && numIndicesIn >= 3) {
		// if there are any input primitives, but all input primitives are degenerate triangles, output one empty block
		numBlocks++;
		sizeofIndexDataOut += 2*sizeof(uint16_t);
	}

	pDispatchDrawTriangleCullIndexData->m_magic = kDispatchDrawTriangleCullIndexDataMagic;
	pDispatchDrawTriangleCullIndexData->m_versionMajor = kDispatchDrawTriangleCullIndexDataVersionMajor;
	pDispatchDrawTriangleCullIndexData->m_versionMinor = kDispatchDrawTriangleCullIndexDataVersionMinor;
	pDispatchDrawTriangleCullIndexData->m_numIndexDataBlocks = numBlocks;
	if (numBlocks > 1 || numVtx > 128 || numTri > 256) {
		pDispatchDrawTriangleCullIndexData->m_numInstancesPerTgMinus1 = 0;
	} else if (numTri > 0) {
		uint32_t maxInstancesPerThreadGroupVtx = 256/numVtx;
		uint32_t maxInstancesPerThreadGroupTri = 512/numTri;
		pDispatchDrawTriangleCullIndexData->m_numInstancesPerTgMinus1 = (uint8_t)( (maxInstancesPerThreadGroupVtx < maxInstancesPerThreadGroupTri ? maxInstancesPerThreadGroupVtx : maxInstancesPerThreadGroupTri) - 1 );
	} else {
		pDispatchDrawTriangleCullIndexData->m_numInstancesPerTgMinus1 = 0;
	}
	{
		uint32_t bits = maxIndex, numIndexBits = 0;
		if (bits) {
			if (bits & 0xFF00)	numIndexBits |= 0x8, bits >>= 8;
			if (bits & 0xF0)	numIndexBits |= 0x4, bits >>= 4;
			if (bits & 0xC)		numIndexBits |= 0x2, bits >>= 2;
			if (bits & 0x2)		numIndexBits |= 0x1;
			++numIndexBits;
		}
		pDispatchDrawTriangleCullIndexData->m_numIndexBits = (uint8_t)numIndexBits;
		uint32_t numIndexSpaceBits = 0;
		bits = (0xFFFF>>(16-numIndexBits)) &~ maxIndex;
		if (bits) {
			if (bits & 0xFF00)	numIndexSpaceBits |= 0x8, bits >>= 8;
			if (bits & 0xF0)	numIndexSpaceBits |= 0x4, bits >>= 4;
			if (bits & 0xC)		numIndexSpaceBits |= 0x2, bits >>= 2;
			if (bits & 0x2)		numIndexSpaceBits |= 0x1;
			++numIndexSpaceBits;
		}
		pDispatchDrawTriangleCullIndexData->m_numIndexSpaceBits = (uint8_t)numIndexSpaceBits;
	}
	pDispatchDrawTriangleCullIndexData->m_reserved = 0;
	pDispatchDrawTriangleCullIndexData->m_reserved2 = 0;
	pDispatchDrawTriangleCullIndexData->m_bufferInputIndexData.initAsByteBuffer((void*)0, (uint32_t)(numBlocks * sizeof(uint32_t) + sizeofIndexDataOut));
	pDispatchDrawTriangleCullIndexData->m_bufferInputIndexData.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);	// input index data is read-only
	return numBlocks * sizeof(uint32_t) + sizeofIndexDataOut;
}

static inline uint16_t* dumpDispatchDrawInputBlockTriangles(uint16_t* pIdxBlk, uint8_t const* aTriIdxIn, uint32_t numVtx, uint32_t numTri)
{
	pIdxBlk[0] = (uint16_t)numTri;
	pIdxBlk[1] = (uint16_t)numVtx;
	uint8_t *aTriIdx = (uint8_t*)(pIdxBlk + 2 + numVtx);
	uint32_t nTriIdx = 0;
	for (uint32_t nTri = 0; nTri < numTri; ++nTri, nTriIdx += 3) {
		aTriIdx[nTri + 0*numTri] = aTriIdxIn[nTriIdx + 0];
		aTriIdx[nTri + 1*numTri] = aTriIdxIn[nTriIdx + 1];
		aTriIdx[nTri + 2*numTri] = aTriIdxIn[nTriIdx + 2];
	}
	// advance to next block, aligning to a dword boundary:
	if (numTri & 1)
		aTriIdx[nTriIdx++] = 0;
	pIdxBlk = reinterpret_cast<uint16_t*>(aTriIdx + nTriIdx);
	if (((uintptr_t)pIdxBlk) & 0x3)
		*pIdxBlk++ = 0;
	return pIdxBlk;
}

int64_t sce::Gnmx::createDispatchDrawInputData(DispatchDrawTriangleCullIndexData *pDispatchDrawTriangleCullIndexData, Gnm::PrimitiveType primType, uint32_t const numIndicesIn, uint16_t const*const pIndicesIn, void*const pIndexDataOut, size_t const sizeofIndexDataOut)
{
	if ((primType != Gnm::kPrimitiveTypeTriList && primType != Gnm::kPrimitiveTypeTriStrip) || pIndicesIn == NULL || pIndexDataOut == NULL || pDispatchDrawTriangleCullIndexData == NULL)
		return kDispatchDrawErrorInvalidArguments;
	if (((uintptr_t)pIndexDataOut & 0x3) != 0)
		return kDispatchDrawErrorInvalidArguments;
	if (sizeofIndexDataOut < pDispatchDrawTriangleCullIndexData->m_bufferInputIndexData.getSize())
		return kDispatchDrawErrorInvalidArguments;
	uint32_t numBlocksMax = pDispatchDrawTriangleCullIndexData->m_numIndexDataBlocks;

	// pIndexDataOut ->
	//	U32 aOffsetToBlock[numBlocks];
	//	IdxBlk[i] @(pIndexDataOut + offsetToBlock[i]) {
	//		U16 triCount	//<= 512
	//		U16 vtxCount	//<= 256
	//		U16 aVtx[vtxCount]		// original vertex indices
	//		U8 aTriIdx[triCount*3]	// local indices into vtx[vtxCount] in SOA order
	//		if (triCount&1) { U8 pad = 0; }
	//	}
	uint8_t aTriIdxTmp[512*3];		// have to write triangles (3 local indices each) to a temporary buffer, as their final placement will be determined by how many unique vertices we find
	uint16_t const*const pIndexDataOutEnd = (uint16_t const*)((uintptr_t)pIndexDataOut + sizeofIndexDataOut);
//	memset(aTriIdxTmp, 0xFF, 512*3*sizeof(uint8_t));
		
	uint32_t *aOffsetToBlock = (uint32_t*)pIndexDataOut;
	uint16_t *pIdxBlk = (uint16_t*)(aOffsetToBlock + numBlocksMax);
	uint16_t *aVtx = pIdxBlk+2;		// write vertices (unique input indices) in place as we go
	uint32_t numVtxMax = (aVtx + 256 <= pIndexDataOutEnd) ? 256 : (aVtx <= pIndexDataOutEnd) ? (uint32_t)(pIndexDataOutEnd - aVtx) : 0;
	uint32_t numBlocks = 0;
	uint32_t numVtx = 0;
	uint32_t numTri = 0;
	uint32_t numTriIdx = 0;
	uint16_t maxIndex = 0, maxIndexInBlock = 0;
#if DISPATCH_DRAW_OPTIMIZE_INDEX_COMPARES_BY_KEEPING_RANGE
	uint16_t minIndexInBlock = 0xFFFF;
#endif
		
	uint16_t const* pTriIn = pIndicesIn;
	uint16_t const* pTriInEnd = pIndicesIn + numIndicesIn;
	uint32_t nWindingXor = 0;	// 0 or 3 alternating
	while (pTriIn+2 < pTriInEnd) {
		if (!(pTriIn[0] == pTriIn[1] || pTriIn[0] == pTriIn[2] || pTriIn[1] == pTriIn[2])) {	// skip degenerate triangles
			uint8_t aTriIdxNew[3];
			uint16_t aVtxNew[3];
			uint32_t numVtxNew = 0;
			uint32_t nWindingXorInTri = 0;
			for (uint32_t nVertInTri = 0; nVertInTri < 3; ++nVertInTri) {
				uint16_t index = pTriIn[nVertInTri ^ nWindingXorInTri];
				nWindingXorInTri = nWindingXor;
				uint32_t nVertInVtx;
#if DISPATCH_DRAW_OPTIMIZE_INDEX_COMPARES_BY_KEEPING_RANGE
				// use a range check to avoid iterating over all previously seen indices if the new index is not in [minIndexInBlock:maxIndexInBlock]
				if (index < minIndexInBlock || index > maxIndexInBlock)
					nVertInVtx = numVtx;
				else
#endif
				{
					for (nVertInVtx = 0; nVertInVtx < numVtx; ++nVertInVtx)
						if (aVtx[nVertInVtx] == index)
							break;
				}
				if (nVertInVtx == numVtx) {
					aTriIdxNew[nVertInTri] = (uint8_t)(nVertInVtx + numVtxNew);
					aVtxNew[numVtxNew++] = index;
				} else
					aTriIdxNew[nVertInTri] = (uint8_t)nVertInVtx;
				SCE_GNM_ASSERT(aTriIdxNew[nVertInTri] < numVtx + numVtxNew);
			}
			if (numVtx + numVtxNew > numVtxMax || numTri+1 > 512) {
				if (aVtx + numVtx + numVtxNew + (numTriIdx+1)/2 > pIndexDataOutEnd)
					return kDispatchDrawErrorOutOfSpaceForIndexData;
				if (numBlocks+1 > numBlocksMax)
					return kDispatchDrawErrorOutOfSpaceForBlockOffset;
				if (((uintptr_t)pIdxBlk - (uintptr_t)pIndexDataOut) > 0xFFFFFFFF)
					return kDispatchDrawErrorUnrepresentableOffset;
				// no room for the new unique vertices or no room for another triangle, so dump block and advance to next block:
				aOffsetToBlock[numBlocks++] = (uint32_t)((uintptr_t)pIdxBlk - (uintptr_t)pIndexDataOut);
				pIdxBlk = dumpDispatchDrawInputBlockTriangles(pIdxBlk, aTriIdxTmp, numVtx, numTri);
				aVtx = pIdxBlk + 2;
				numVtxMax = (aVtx + 256 <= pIndexDataOutEnd) ? 256 : (aVtx <= pIndexDataOutEnd) ? (uint32_t)(pIndexDataOutEnd - aVtx) : 0;
				numTri = numTriIdx = numVtx = 0;
				// track overall maxIndex
				if (maxIndex < maxIndexInBlock)
					maxIndex = maxIndexInBlock;
				// write the new vertices which didn't fit as the first vertices in the next block
				if (numVtxMax < 5)	// 3 vertices plus 3 indices requires 9 bytes of space, which rounds up to 5 vertices
					return kDispatchDrawErrorOutOfSpaceForIndexData;
				aVtxNew[0] = pTriIn[0], aVtxNew[1] = pTriIn[1 ^ nWindingXor], aVtxNew[2] = pTriIn[2 ^ nWindingXor];
				aTriIdxNew[0] = 0, aTriIdxNew[1] = 1, aTriIdxNew[2] = 2;
				numVtxNew = 3;
#if DISPATCH_DRAW_OPTIMIZE_INDEX_COMPARES_BY_KEEPING_RANGE
				// reset [minIndexInBlock:maxIndexInBlock] to empty for new block
				minIndexInBlock = 0xFFFF;
#endif
				maxIndexInBlock = 0;
			}
			for (uint32_t nVtxNew = 0; nVtxNew < numVtxNew; ++nVtxNew) {
				uint16_t index = aVtxNew[nVtxNew];
				aVtx[numVtx++] = index;
#if DISPATCH_DRAW_OPTIMIZE_INDEX_COMPARES_BY_KEEPING_RANGE
				// track [minIndexInBlock:maxIndexInBlock], range of indices in aVtx[]
				if (minIndexInBlock > index)
					minIndexInBlock = index;
#endif
				if (maxIndexInBlock < index)
					maxIndexInBlock = index;
			}
			aTriIdxTmp[numTriIdx + 0] = aTriIdxNew[0];
			aTriIdxTmp[numTriIdx + 1] = aTriIdxNew[1];
			aTriIdxTmp[numTriIdx + 2] = aTriIdxNew[2];
			++numTri;
			numTriIdx += 3;
		}
		if (primType == Gnm::kPrimitiveTypeTriStrip) {
			nWindingXor ^= 3;
			++pTriIn;
		} else
			pTriIn += 3;
	}
	if (numTri > 0) {
		if (aVtx + numVtx + (numTri*3+1)/2 > pIndexDataOutEnd)
			return kDispatchDrawErrorOutOfSpaceForIndexData;
		if (numBlocks+1 > numBlocksMax)
			return kDispatchDrawErrorOutOfSpaceForBlockOffset;
		if (((uintptr_t)pIdxBlk - (uintptr_t)pIndexDataOut) > 0xFFFFFFFF)
			return kDispatchDrawErrorUnrepresentableOffset;
		// dump final partial block if there are any triangles:
		aOffsetToBlock[numBlocks++] = (uint32_t)((uintptr_t)pIdxBlk - (uintptr_t)pIndexDataOut);
		pIdxBlk = dumpDispatchDrawInputBlockTriangles(pIdxBlk, aTriIdxTmp, numVtx, numTri);
		// track overall maxIndex
		if (maxIndex < maxIndexInBlock)
			maxIndex = maxIndexInBlock;
	} else if (numBlocks == 0 && numIndicesIn >= 3) {
		// if there are any input primitives, but all input primitives are degenerate triangles, output one empty block
		if (numBlocks+1 > numBlocksMax)
			return kDispatchDrawErrorOutOfSpaceForBlockOffset;
		aOffsetToBlock[numBlocks++] = (uint32_t)((uintptr_t)pIdxBlk - (uintptr_t)pIndexDataOut);
		pIdxBlk = dumpDispatchDrawInputBlockTriangles(pIdxBlk, aTriIdxTmp, 0, 0);
	}
	if (numBlocks < numBlocksMax) {
		if (pIdxBlk + 2 > pIndexDataOutEnd)
			return kDispatchDrawErrorOutOfSpaceForIndexData;	//ERROR: out of space for empty block, which we shouldn't have if numBlocksMax were set correctly
		if (((uintptr_t)pIdxBlk - (uintptr_t)pIndexDataOut) > 0xFFFFFFFF)
			return kDispatchDrawErrorUnrepresentableOffset;
		pIdxBlk[0] = 0;
		pIdxBlk[1] = 0;
		uint32_t offsetEmptyBlock = (uint32_t)((uintptr_t)pIdxBlk - (uintptr_t)pIndexDataOut);
		for (uint32_t iBlock = numBlocks; iBlock < numBlocksMax; ++iBlock)
			aOffsetToBlock[iBlock] = offsetEmptyBlock;
		pIdxBlk += 2;
	}
	size_t sizeofIndexDataWritten = ((uintptr_t)pIdxBlk - (uintptr_t)pIndexDataOut);

	//Fill out pDispatchDrawTriangleCullIndexData entirely, in case getMaxSizeofDispatchDrawInputData was called instead of getSizeofDispatchDrawInputData:
	pDispatchDrawTriangleCullIndexData->m_magic = kDispatchDrawTriangleCullIndexDataMagic;
	pDispatchDrawTriangleCullIndexData->m_versionMajor = kDispatchDrawTriangleCullIndexDataVersionMajor;
	pDispatchDrawTriangleCullIndexData->m_versionMinor = kDispatchDrawTriangleCullIndexDataVersionMinor;
	pDispatchDrawTriangleCullIndexData->m_numIndexDataBlocks = numBlocks;
	if (numBlocks > 1 || numVtx > 128 || numTri > 256) {
		pDispatchDrawTriangleCullIndexData->m_numInstancesPerTgMinus1 = 0;
	} else if (numTri > 0) {
		uint32_t maxInstancesPerThreadGroupVtx = 256/numVtx;
		uint32_t maxInstancesPerThreadGroupTri = 512/numTri;
		pDispatchDrawTriangleCullIndexData->m_numInstancesPerTgMinus1 = (uint8_t)( (maxInstancesPerThreadGroupVtx < maxInstancesPerThreadGroupTri ? maxInstancesPerThreadGroupVtx : maxInstancesPerThreadGroupTri) - 1 );
	} else {
		pDispatchDrawTriangleCullIndexData->m_numInstancesPerTgMinus1 = 0;
	}
	{
		uint32_t bits = maxIndex, numIndexBits = 0;
		if (bits) {
			if (bits & 0xFF00)	numIndexBits |= 0x8, bits >>= 8;
			if (bits & 0xF0)	numIndexBits |= 0x4, bits >>= 4;
			if (bits & 0xC)		numIndexBits |= 0x2, bits >>= 2;
			if (bits & 0x2)		numIndexBits |= 0x1;
			++numIndexBits;
		}
		pDispatchDrawTriangleCullIndexData->m_numIndexBits = (uint8_t)numIndexBits;
		uint32_t numIndexSpaceBits = 0;
		bits = (0xFFFF>>(16-numIndexBits)) &~ maxIndex;
		if (bits) {
			if (bits & 0xFF00)	numIndexSpaceBits |= 0x8, bits >>= 8;
			if (bits & 0xF0)	numIndexSpaceBits |= 0x4, bits >>= 4;
			if (bits & 0xC)		numIndexSpaceBits |= 0x2, bits >>= 2;
			if (bits & 0x2)		numIndexSpaceBits |= 0x1;
			++numIndexSpaceBits;
		}
		pDispatchDrawTriangleCullIndexData->m_numIndexSpaceBits = (uint8_t)numIndexSpaceBits;
	}
	pDispatchDrawTriangleCullIndexData->m_reserved = 0;
	pDispatchDrawTriangleCullIndexData->m_reserved2 = 0;
	pDispatchDrawTriangleCullIndexData->m_bufferInputIndexData.initAsByteBuffer((void*)pIndexDataOut, (uint32_t)sizeofIndexDataWritten);
	pDispatchDrawTriangleCullIndexData->m_bufferInputIndexData.setResourceMemoryType(Gnm::kResourceMemoryTypeRO);	// input index data is read-only
	return (int64_t)sizeofIndexDataWritten;
}
