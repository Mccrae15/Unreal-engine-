/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifdef __ORBIS__
#include <x86intrin.h>
#endif // __ORBIS__
#include <gnm.h>

#include <gnmx/common.h>

#include <gnmx/cue.h>
#include <gnmx/cue-helper.h>

using namespace sce::Gnm;
using namespace sce::Gnmx;

void ConstantUpdateEngine::setTextures(sce::Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const sce::Gnm::Texture *textures)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(startApiSlot);
	SCE_GNM_UNUSED(numApiSlots);
	SCE_GNM_UNUSED(textures);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableResource], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES

	SCE_GNM_ASSERT_MSG(startApiSlot < m_ringSetup.numResourceSlots, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, m_ringSetup.numResourceSlots-1);
	SCE_GNM_ASSERT_MSG(numApiSlots <= m_ringSetup.numResourceSlots, "numApiSlots (%u) must be in the range [0..%u].", numApiSlots, m_ringSetup.numResourceSlots);
	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= m_ringSetup.numResourceSlots, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%u].", startApiSlot, numApiSlots, m_ringSetup.numResourceSlots);
	if ( numApiSlots == 0 )
		return;

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !textures ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// The user may set N T# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t startChunk = startApiSlot / kResourceChunkSize;
	const uint32_t endChunk   = (startApiSlot+numApiSlots-1) / kResourceChunkSize;

	const uint16_t initialSlot = (startApiSlot & ConstantUpdateEngineHelper::kResourceChunkSizeMask);
	const uint16_t finalSlot   = ((startApiSlot+numApiSlots-1) & ConstantUpdateEngineHelper::kResourceChunkSizeMask);

	StageChunkState *chunkState = m_stageInfo[stage].resourceStage;


	uint32_t  curChunk = startChunk;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(textures);

	while ( curChunk <= endChunk )
	{
		const uint16_t startSlot = curChunk == startChunk ? initialSlot : 0;
		const uint16_t endSlot   = curChunk == endChunk   ? finalSlot   : ConstantUpdateEngineHelper::kResourceChunkSizeMask; // aka: kResourceChunkSize-1
		const uint16_t range     = static_cast<uint16_t>(((1 << (16-endSlot-1))-1) ^ ((1 << (16-startSlot))-1));
		//const uint16_t range     = ((1 << (endSlot+1))-1) ^ ((1 << startSlot)-1);

		//

		// Check for a resource conflict between previous draw and current draw:
		// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
		if ( !chunkState[curChunk].usedChunk &&
			(chunkState[curChunk].curSlots & range) )
		{
			// Conflict
			chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
			chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
			chunkState[curChunk].curChunk  = 0;
			chunkState[curChunk].curSlots  = 0;
		}

		if ( curData )
		{
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
				SCE_GNM_ASSERT_MSG(chunkState[curChunk].curChunk != NULL, "allocateRegionToCopyToCpRam() failed");
			}

			// Copy the user resources in the current chunk:
			// TODO: Check is unrolling the loop would help.
			for (uint32_t iResource = startSlot; iResource <= endSlot; ++iResource)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kResourceSizeInDqWord == 2);

				chunkState[curChunk].curChunk[2*iResource]   = *(curData+0);
				chunkState[curChunk].curChunk[2*iResource+1] = *(curData+1);
				curData += 2;
			}
			//
			chunkState[curChunk].curSlots |= range;
		}
		else
		{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
			}

			for (uint32_t iResource = startSlot; iResource <= endSlot; ++iResource)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kResourceSizeInDqWord == 2);
				chunkState[curChunk].curChunk[2*iResource]   = 0;
				chunkState[curChunk].curChunk[2*iResource+1] = 0;
			}
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

			chunkState[curChunk].curSlots  &= ~range;
			chunkState[curChunk].usedSlots &= ~range; // to avoid keeping older version of this resource

			if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
			}
		}

		// Next chunk:
		curChunk++;
	}

	uint64_t apiMask[2];

	if ( startApiSlot+numApiSlots < 64 )
	{
		uint64_t halfMask = ((((uint64_t)1)<<(startApiSlot+numApiSlots))-1) ^ ((((uint64_t)1)<<startApiSlot)-1);
		apiMask[0] = halfMask;
		apiMask[1] = 0;
	}
	else if ( startApiSlot >= 64 )
	{
		uint64_t halfMask = ((((uint64_t)1)<<(startApiSlot+numApiSlots-64))-1) ^ ((((uint64_t)1)<<(startApiSlot-64))-1);
		apiMask[0] = 0;
		apiMask[1] = halfMask;
	}
	else if( startApiSlot + numApiSlots < 128)
	{
		uint64_t upperHalfMask = ((((uint64_t)1)<<(startApiSlot+numApiSlots-64))-1);
		uint64_t lowerHalfMask = ~((((uint64_t)1)<<startApiSlot)-1);

		apiMask[0] = lowerHalfMask;
		apiMask[1] = upperHalfMask;
	}
	else // startApisSlot + numApisSlots == 128 => startApiSlot = 0 && numApiSlots = 128
	{
		apiMask[0] = 0xFFFFFFFFFFFFFFFFULL;
		apiMask[1] = 0xFFFFFFFFFFFFFFFFULL;
	}

	// Check if we need to dirty the EUD:
	if ( (m_stageInfo[stage].eudResourceSet[0] & apiMask[0]) || (m_stageInfo[stage].eudResourceSet[1] & apiMask[1]) )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeResource[0] |= apiMask[0];
		m_stageInfo[stage].activeResource[1] |= apiMask[1];

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1u<<(31-kRingBuffersIndexResource);
	}
	else
	{
		m_stageInfo[stage].activeResource[0] &= ~apiMask[0];
		m_stageInfo[stage].activeResource[1] &= ~apiMask[1];

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexResource);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setTexture(sce::Gnm::ShaderStage stage, uint32_t apiSlot, const sce::Gnm::Texture *texture)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(apiSlot);
	SCE_GNM_UNUSED(texture);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY


#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableResource], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES

	SCE_GNM_ASSERT_MSG(apiSlot < m_ringSetup.numResourceSlots, "startApiSlot (%u) must be in the range [0..%d].", apiSlot, m_ringSetup.numResourceSlots-1);

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !texture ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.


	//
	// Here the user set only 1 T#
	//

	const uint32_t	curChunk = apiSlot / kResourceChunkSize;
	const uint16_t	slot	 = (apiSlot & ConstantUpdateEngineHelper::kResourceChunkSizeMask);
	const uint16_t	slotMask = static_cast<uint16_t>(1 << (16-slot-1));

	StageChunkState *chunkState = m_stageInfo[stage].resourceStage;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(texture);

	//

	// Check for a resource conflict between previous draw and current draw:
	// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
	if ( !chunkState[curChunk].usedChunk &&
		(chunkState[curChunk].curSlots & slotMask) )
	{
		// Conflict
		chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
		chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
		chunkState[curChunk].curChunk  = 0;
		chunkState[curChunk].curSlots  = 0;
	}

	if ( curData )
	{
		// No chunk allocated for the current draw (due to conflict, or because it was never used before)
		if ( !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
		}

		SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kResourceSizeInDqWord == 2);
		chunkState[curChunk].curChunk[2*slot]   = *(curData+0);
		chunkState[curChunk].curChunk[2*slot+1] = *(curData+1);
		//
		chunkState[curChunk].curSlots |= slotMask;
	}
	else
	{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// No chunk allocated for the current draw (due to conflict, or because it was never used before)
		if ( !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
		}

		chunkState[curChunk].curChunk[2*slot]   = 0;
		chunkState[curChunk].curChunk[2*slot+1] = 0;
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

		chunkState[curChunk].curSlots  &= ~slotMask;
		chunkState[curChunk].usedSlots &= ~slotMask; // to avoid keeping older version of this resource

		if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
		}
	}

	uint64_t apiMask[2];

	if ( apiSlot < 64 )
	{
		uint64_t halfMask = ((uint64_t)1)<<(apiSlot);
		apiMask[0] = halfMask;
		apiMask[1] = 0;
	}
	else
	{
		uint64_t halfMask = (((uint64_t)1)<<(apiSlot-64));
		apiMask[0] = 0;
		apiMask[1] = halfMask;
	}

	// Check if we need to dirty the EUD:
	if ( (m_stageInfo[stage].eudResourceSet[0] & apiMask[0]) || (m_stageInfo[stage].eudResourceSet[1] & apiMask[1]) )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeResource[0] |= apiMask[0];
		m_stageInfo[stage].activeResource[1] |= apiMask[1];

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1u<<(31-kRingBuffersIndexResource);
	}
	else
	{
		m_stageInfo[stage].activeResource[0] &= ~apiMask[0];
		m_stageInfo[stage].activeResource[1] &= ~apiMask[1];

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexResource);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setBuffers(sce::Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const sce::Gnm::Buffer *buffers)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(startApiSlot);
	SCE_GNM_UNUSED(numApiSlots);
	SCE_GNM_UNUSED(buffers);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableResource], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(startApiSlot < m_ringSetup.numResourceSlots, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, m_ringSetup.numResourceSlots-1);
	SCE_GNM_ASSERT_MSG(numApiSlots <= m_ringSetup.numResourceSlots, "numApiSlots (%u) must be in the range [0..%u].", numApiSlots, m_ringSetup.numResourceSlots);
	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= m_ringSetup.numResourceSlots, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%u].", startApiSlot, numApiSlots, m_ringSetup.numResourceSlots);
	if ( numApiSlots == 0 )
		return;

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !buffers ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// The user may set N V# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t startChunk = startApiSlot / kResourceChunkSize;
	const uint32_t endChunk   = (startApiSlot+numApiSlots-1) / kResourceChunkSize;

	const uint16_t initialSlot = (startApiSlot & ConstantUpdateEngineHelper::kResourceChunkSizeMask);
	const uint16_t finalSlot   = ((startApiSlot+numApiSlots-1) & ConstantUpdateEngineHelper::kResourceChunkSizeMask);

	StageChunkState *chunkState = m_stageInfo[stage].resourceStage;

	uint32_t  curChunk = startChunk;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(buffers);

	while ( curChunk <= endChunk )
	{
		const uint16_t startSlot = curChunk == startChunk ? initialSlot : 0;
		const uint16_t endSlot   = curChunk == endChunk   ? finalSlot   : ConstantUpdateEngineHelper::kResourceChunkSizeMask; // aka: kResourceChunkSize-1
		const uint16_t range = static_cast<uint16_t>(((1 << (16 - endSlot - 1)) - 1) ^ ((1 << (16 - startSlot)) - 1));
		//const uint16_t range     = ((1 << (endSlot+1))-1) ^ ((1 << startSlot)-1);

		//

		// Check for a resource conflict between previous draw and current draw:
		// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
		if ( !chunkState[curChunk].usedChunk &&
			(chunkState[curChunk].curSlots & range) )
		{
			// Conflict
			chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
			chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
			chunkState[curChunk].curChunk  = 0;
			chunkState[curChunk].curSlots  = 0;
		}

		if ( curData )
		{
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
				SCE_GNM_ASSERT_MSG(chunkState[curChunk].curChunk != NULL, "allocateRegionToCopyToCpRam() failed");
			}

			// Copy the user resources in the current chunk:
			// TODO: Check is unrolling the loop would help.
			for (uint32_t iResource = startSlot; iResource <= endSlot; ++iResource)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kResourceSizeInDqWord == 2);

				chunkState[curChunk].curChunk[2*iResource]   = *curData;
				chunkState[curChunk].curChunk[2*iResource+1] = 0;
				curData += 1;
			}
			//
			chunkState[curChunk].curSlots |= range;
		}
		else
		{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
			}

			for (uint32_t iResource = startSlot; iResource <= endSlot; ++iResource)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kResourceSizeInDqWord == 2);
				chunkState[curChunk].curChunk[2*iResource]   = 0;
				chunkState[curChunk].curChunk[2*iResource+1] = 0;
			}
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

			chunkState[curChunk].curSlots  &= ~range;
			chunkState[curChunk].usedSlots &= ~range; // to avoid keeping older version of this resource

			if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
			}
		}

		// Next chunk:
		curChunk++;
	}

	uint64_t apiMask[2];

	if ( startApiSlot+numApiSlots < 64 )
	{
		uint64_t halfMask = ((((uint64_t)1)<<(startApiSlot+numApiSlots))-1) ^ ((((uint64_t)1)<<startApiSlot)-1);
		apiMask[0] = halfMask;
		apiMask[1] = 0;
	}
	else if ( startApiSlot >= 64 )
	{
		uint64_t halfMask = ((((uint64_t)1)<<(startApiSlot+numApiSlots-64))-1) ^ ((((uint64_t)1)<<(startApiSlot-64))-1);
		apiMask[0] = 0;
		apiMask[1] = halfMask;
	}
	else if( startApiSlot + numApiSlots < 128 )
	{
		uint64_t upperHalfMask = ((((uint64_t)1)<<(startApiSlot+numApiSlots-64))-1);
		uint64_t lowerHalfMask = ~((((uint64_t)1)<<startApiSlot)-1);

		apiMask[0] = lowerHalfMask;
		apiMask[1] = upperHalfMask;
	}
	else
	{
		apiMask[0] = 0xFFFFFFFFFFFFFFFFULL;
		apiMask[1] = 0xFFFFFFFFFFFFFFFFULL;
	}

	// Check if we need to dirty the EUD:
	if ( (m_stageInfo[stage].eudResourceSet[0] & apiMask[0]) || (m_stageInfo[stage].eudResourceSet[1] & apiMask[1]) )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeResource[0] |= apiMask[0];
		m_stageInfo[stage].activeResource[1] |= apiMask[1];

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1u<<(31-kRingBuffersIndexResource);
	}
	else
	{
		m_stageInfo[stage].activeResource[0] &= ~apiMask[0];
		m_stageInfo[stage].activeResource[1] &= ~apiMask[1];

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexResource);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setBuffer(sce::Gnm::ShaderStage stage, uint32_t apiSlot, const sce::Gnm::Buffer *buffer)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(apiSlot);
	SCE_GNM_UNUSED(buffer);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableResource], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(apiSlot < m_ringSetup.numResourceSlots, "startApiSlot (%u) must be in the range [0..%d].", apiSlot, m_ringSetup.numResourceSlots-1);

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !buffer ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.


	//
	// Here the user set only 1 T#
	//

	const uint32_t	curChunk = apiSlot / kResourceChunkSize;
	const uint16_t	slot	 = (apiSlot & ConstantUpdateEngineHelper::kResourceChunkSizeMask);
	const uint16_t	slotMask = static_cast<uint16_t>(1 << (16-slot-1));

	StageChunkState *chunkState = m_stageInfo[stage].resourceStage;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(buffer);

	//

	// Check for a resource conflict between previous draw and current draw:
	// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
	if ( !chunkState[curChunk].usedChunk &&
		(chunkState[curChunk].curSlots & slotMask) )
	{
		// Conflict
		chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
		chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
		chunkState[curChunk].curChunk  = 0;
		chunkState[curChunk].curSlots  = 0;
	}

	if ( curData )
	{
		// No chunk allocated for the current draw (due to conflict, or because it was never used before)
		if ( !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
		}

		SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kResourceSizeInDqWord == 2);
		chunkState[curChunk].curChunk[2*slot]   = *(curData+0);
		chunkState[curChunk].curChunk[2*slot+1] = 0;
		//
		chunkState[curChunk].curSlots |= slotMask;
	}
	else
	{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// No chunk allocated for the current draw (due to conflict, or because it was never used before)
		if ( !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
		}

		chunkState[curChunk].curChunk[2*slot]   = 0;
		chunkState[curChunk].curChunk[2*slot+1] = 0;
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

		chunkState[curChunk].curSlots  &= ~slotMask;
		chunkState[curChunk].usedSlots &= ~slotMask; // to avoid keeping older version of this resource

		if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetResource, curChunk, ConstantUpdateEngineHelper::kResourceChunkSizeInDWord);
		}

	}

	uint64_t apiMask[2];

	if ( apiSlot < 64 )
	{
		uint64_t halfMask = ((uint64_t)1)<<(apiSlot);
		apiMask[0] = halfMask;
		apiMask[1] = 0;
	}
	else
	{
		uint64_t halfMask = (((uint64_t)1)<<(apiSlot-64));
		apiMask[0] = 0;
		apiMask[1] = halfMask;
	}

	// Check if we need to dirty the EUD:
	if ( (m_stageInfo[stage].eudResourceSet[0] & apiMask[0]) || (m_stageInfo[stage].eudResourceSet[1] & apiMask[1]) )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeResource[0] |= apiMask[0];
		m_stageInfo[stage].activeResource[1] |= apiMask[1];

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1u<<(31-kRingBuffersIndexResource);
	}
	else
	{
		m_stageInfo[stage].activeResource[0] &= ~apiMask[0];
		m_stageInfo[stage].activeResource[1] &= ~apiMask[1];

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexResource);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setSamplers(sce::Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const sce::Gnm::Sampler *samplers)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(startApiSlot);
	SCE_GNM_UNUSED(numApiSlots);
	SCE_GNM_UNUSED(samplers);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableSampler], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(startApiSlot < m_ringSetup.numSampleSlots, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, m_ringSetup.numSampleSlots-1);
	SCE_GNM_ASSERT_MSG(numApiSlots <= m_ringSetup.numSampleSlots, "numApiSlots (%u) must be in the range [0..%u].", numApiSlots, m_ringSetup.numSampleSlots);
	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= m_ringSetup.numSampleSlots, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%u].", startApiSlot, numApiSlots, m_ringSetup.numSampleSlots);
	if ( numApiSlots == 0 )
		return;

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !samplers ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// The user may set N T# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t startChunk = startApiSlot / kSamplerChunkSize;
	const uint32_t endChunk   = (startApiSlot+numApiSlots-1) / kSamplerChunkSize;

	const uint16_t initialSlot = (startApiSlot & ConstantUpdateEngineHelper::kSamplerChunkSizeMask);
	const uint16_t finalSlot   = ((startApiSlot+numApiSlots-1) & ConstantUpdateEngineHelper::kSamplerChunkSizeMask);

	StageChunkState *chunkState = m_stageInfo[stage].samplerStage;

	uint32_t  curChunk = startChunk;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(samplers);

	while ( curChunk <= endChunk )
	{
		const uint16_t startSlot = curChunk == startChunk ? initialSlot : 0;
		const uint16_t endSlot   = curChunk == endChunk   ? finalSlot   : ConstantUpdateEngineHelper::kSamplerChunkSizeMask; // aka: kSamplerChunkSize-1
		const uint16_t range = static_cast<uint16_t>(((1 << (16 - endSlot - 1)) - 1) ^ ((1 << (16 - startSlot)) - 1));
		//const uint16_t range     = ((1 << (endSlot+1))-1) ^ ((1 << startSlot)-1);

		//

		// Check for a resource conflict between previous draw and current draw:
		// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
		if ( !chunkState[curChunk].usedChunk &&
			(chunkState[curChunk].curSlots & range) )
		{
			// Conflict
			chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
			chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
			chunkState[curChunk].curChunk  = 0;
			chunkState[curChunk].curSlots  = 0;
		}

		if ( curData )
		{
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetSampler, curChunk, ConstantUpdateEngineHelper::kSamplerChunkSizeInDWord);
				SCE_GNM_ASSERT_MSG(chunkState[curChunk].curChunk != NULL, "allocateRegionToCopyToCpRam() failed");
			}

			// Copy the user resources in the current chunk:
			// TODO: Check is unrolling the loop would help.
			for (uint32_t iSampler = startSlot; iSampler <= endSlot; ++iSampler)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kSamplerSizeInDqWord == 1);

				chunkState[curChunk].curChunk[iSampler]   = *(curData+0);
				curData += 1;
			}
			//
			chunkState[curChunk].curSlots |= range;
		}
		else
		{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetSampler, curChunk, ConstantUpdateEngineHelper::kSamplerChunkSizeInDWord);
			}

			for (uint32_t iSampler = startSlot; iSampler <= endSlot; ++iSampler)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kSamplerSizeInDqWord == 1);
				chunkState[curChunk].curChunk[iSampler]   = 0;
			}
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

			chunkState[curChunk].curSlots  &= ~range;
			chunkState[curChunk].usedSlots &= ~range; // to avoid keeping older version of this resource

			if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetSampler, curChunk, ConstantUpdateEngineHelper::kSamplerChunkSizeInDWord);
			}
		}

		// Next chunk:
		curChunk++;
	}

	const uint16_t apiMask = static_cast<uint16_t>(((1 << (startApiSlot + numApiSlots)) - 1) ^ ((1 << startApiSlot) - 1));

	// Check if we need to dirty the EUD:
	if ( m_stageInfo[stage].eudSamplerSet & apiMask )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeSampler |= apiMask;

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexSampler);
	}
	else
	{
		m_stageInfo[stage].activeSampler &= ~apiMask;

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexSampler);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setSampler(sce::Gnm::ShaderStage stage, uint32_t apiSlot, const sce::Gnm::Sampler *sampler)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(apiSlot);
	SCE_GNM_UNUSED(sampler);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableSampler], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(apiSlot < m_ringSetup.numSampleSlots, "startApiSlot (%u) must be in the range [0..%d].", apiSlot, m_ringSetup.numSampleSlots-1);
	
#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !sampler ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// The user may set N T# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t curChunk = apiSlot / kSamplerChunkSize;
	const uint32_t slot = (apiSlot & ConstantUpdateEngineHelper::kSamplerChunkSizeMask);
	const uint16_t slotMask = static_cast<uint16_t>(1 << (16 - slot - 1));

	StageChunkState *chunkState = m_stageInfo[stage].samplerStage;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(sampler);

		//

		// Check for a resource conflict between previous draw and current draw:
		// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
		if ( !chunkState[curChunk].usedChunk &&
			(chunkState[curChunk].curSlots & slotMask) )
		{
			// Conflict
			chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
			chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
			chunkState[curChunk].curChunk  = 0;
			chunkState[curChunk].curSlots  = 0;
		}

		if ( curData )
		{
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetSampler, curChunk, ConstantUpdateEngineHelper::kSamplerChunkSizeInDWord);
			}

			SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kSamplerSizeInDqWord == 1);

			chunkState[curChunk].curChunk[slot]   = *(curData+0);
			
			chunkState[curChunk].curSlots |= slotMask;
		}
		else
		{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetSampler, curChunk, ConstantUpdateEngineHelper::kSamplerChunkSizeInDWord);
			}

			chunkState[curChunk].curChunk[slot]   = 0;
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

			chunkState[curChunk].curSlots  &= ~slotMask;
			chunkState[curChunk].usedSlots &= ~slotMask; // to avoid keeping older version of this resource

			if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetSampler, curChunk, ConstantUpdateEngineHelper::kSamplerChunkSizeInDWord);
			}
	}

	const uint16_t apiMask = static_cast<uint16_t>(1 << apiSlot);

	// Check if we need to dirty the EUD:
	if ( m_stageInfo[stage].eudSamplerSet & apiMask )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeSampler |= apiMask;

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexSampler);
	}
	else
	{
		m_stageInfo[stage].activeSampler &= ~apiMask;

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexSampler);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setConstantBuffers(sce::Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const sce::Gnm::Buffer *buffers)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(startApiSlot);
	SCE_GNM_UNUSED(numApiSlots);
	SCE_GNM_UNUSED(buffers);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableConstantBuffer], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(startApiSlot < sce::Gnm::kSlotCountConstantBuffer, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, sce::Gnm::kSlotCountConstantBuffer-1);
	SCE_GNM_ASSERT_MSG(numApiSlots <= sce::Gnm::kSlotCountConstantBuffer, "numApiSlots (%u) must be in the range [0..%d].", numApiSlots, sce::Gnm::kSlotCountConstantBuffer);
	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= sce::Gnm::kSlotCountConstantBuffer, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%d].", startApiSlot, numApiSlots, sce::Gnm::kSlotCountConstantBuffer);
	if ( numApiSlots == 0 )
		return;

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !buffers ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.
	
	//
	// The user may set N T# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t startChunk = startApiSlot / kConstantBufferChunkSize;
	const uint32_t endChunk   = (startApiSlot+numApiSlots-1) / kConstantBufferChunkSize;

	const uint16_t initialSlot = (startApiSlot & ConstantUpdateEngineHelper::kConstantBufferChunkSizeMask);
	const uint16_t finalSlot   = ((startApiSlot+numApiSlots-1) & ConstantUpdateEngineHelper::kConstantBufferChunkSizeMask);

	StageChunkState *chunkState = m_stageInfo[stage].constantBufferStage;

	uint32_t  curChunk = startChunk;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(buffers);

	while ( curChunk <= endChunk )
	{
		const uint16_t startSlot = curChunk == startChunk ? initialSlot : 0;
		const uint16_t endSlot   = curChunk == endChunk   ? finalSlot   : ConstantUpdateEngineHelper::kConstantBufferChunkSizeMask; // aka: kConstantBufferChunkSize-1
		const uint16_t range = static_cast<uint16_t>(((1 << (16 - endSlot - 1)) - 1) ^ ((1 << (16 - startSlot)) - 1));
		//const uint16_t range     = ((1 << (endSlot+1))-1) ^ ((1 << startSlot)-1);

		//

		// Check for a resource conflict between previous draw and current draw:
		// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
		if ( !chunkState[curChunk].usedChunk &&
			 (chunkState[curChunk].curSlots & range) )
		{
			// Conflict
			chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
			chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
			chunkState[curChunk].curChunk  = 0;
			chunkState[curChunk].curSlots  = 0;
		}

		if ( curData )
		{
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				const uint32_t allocSizeInDWords = (curChunk == sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks - 1) ?
												   sce::Gnm::kDwordSizeConstantBuffer * (sce::Gnm::kSlotCountConstantBuffer - sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize*(sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks-1)) :
												   ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord;
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRamForConstantBuffer(m_ccb, stage, curChunk, ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord, allocSizeInDWords);
			}

			// Copy the user resources in the current chunk:
			// TODO: Check is unrolling the loop would help.
			for (uint32_t iConstant = startSlot; iConstant <= endSlot; ++iConstant)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kConstantBufferSizeInDqWord == 1);
				SCE_GNM_ASSERT(chunkState[curChunk].curChunk != NULL);

				chunkState[curChunk].curChunk[iConstant]   = *(curData+0);
				curData += 1;
			}
			//
			chunkState[curChunk].curSlots |= range;
		}
		else
		{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			if ( !chunkState[curChunk].curChunk )
			{
				const uint32_t allocSizeInDWords = (curChunk == sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks - 1) ?
												   sce::Gnm::kDwordSizeConstantBuffer * (sce::Gnm::kSlotCountConstantBuffer - sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize*(sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks-1)) :
												   ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord;
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRamForConstantBuffer(m_ccb, stage, curChunk, ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord, allocSizeInDWords);
			}

			for (uint32_t iConstant = startSlot; iConstant <= endSlot; ++iConstant)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kConstantBufferSizeInDqWord == 1);
				chunkState[curChunk].curChunk[iConstant]   = 0;
			}
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

			chunkState[curChunk].curSlots  &= ~range;
			chunkState[curChunk].usedSlots &= ~range; // to avoid keeping older version of this resource

			if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
			{
				const uint32_t allocSizeInDWords = (curChunk == sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks - 1) ?
												   sce::Gnm::kDwordSizeConstantBuffer * (sce::Gnm::kSlotCountConstantBuffer - sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize*(sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks-1)) :
												   ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord;
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRamForConstantBuffer(m_ccb, stage, curChunk, ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord, allocSizeInDWords);
			}
		}

		// Next chunk:
		curChunk++;
	}

	const uint32_t apiMask = ((1u<<(startApiSlot+numApiSlots))-1) ^ ((1u<<startApiSlot)-1);

	// Check if we need to dirty the EUD:
	if ( m_stageInfo[stage].eudConstantBufferSet & apiMask )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeConstantBuffer |= apiMask;

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexConstantBuffer);
	}
	else
	{
		m_stageInfo[stage].activeConstantBuffer &= ~apiMask;

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexConstantBuffer);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setConstantBuffer(sce::Gnm::ShaderStage stage, uint32_t apiSlot, const sce::Gnm::Buffer *buffer)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(apiSlot);
	SCE_GNM_UNUSED(buffer);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableConstantBuffer], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(apiSlot < sce::Gnm::kSlotCountConstantBuffer, "startApiSlot (%u) must be in the range [0..%d].", apiSlot, sce::Gnm::kSlotCountConstantBuffer-1);

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !buffer ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// The user may set N T# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t curChunk = apiSlot / kConstantBufferChunkSize;
	const uint32_t slot = (apiSlot & ConstantUpdateEngineHelper::kConstantBufferChunkSizeMask);
	const uint16_t slotMask = static_cast<uint16_t>(1 << (16 - slot - 1));

	StageChunkState *chunkState = m_stageInfo[stage].constantBufferStage;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(buffer);

		
	//

	// Check for a resource conflict between previous draw and current draw:
	// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
	if ( !chunkState[curChunk].usedChunk &&
		 (chunkState[curChunk].curSlots & slotMask) )
	{
		// Conflict
		chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
		chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
		chunkState[curChunk].curChunk  = 0;
		chunkState[curChunk].curSlots  = 0;
	}

	if ( curData )
	{
		// No chunk allocated for the current draw (due to conflict, or because it was never used before)
		if ( !chunkState[curChunk].curChunk )
		{
			const uint32_t allocSizeInDWords = (curChunk == sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks - 1) ?
											   sce::Gnm::kDwordSizeConstantBuffer * (sce::Gnm::kSlotCountConstantBuffer - sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize*(sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks-1)) :
											   ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord;
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRamForConstantBuffer(m_ccb, stage, curChunk, ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord, allocSizeInDWords);
		}

		// Copy the user resources in the current chunk:
		// TODO: Check is unrolling the loop would help.
			
		SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kConstantBufferSizeInDqWord == 1);

		chunkState[curChunk].curChunk[slot]   = *(curData+0);
			
		//
		chunkState[curChunk].curSlots |= slotMask;
	}
	else
	{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		if ( !chunkState[curChunk].curChunk )
		{
			const uint32_t allocSizeInDWords = (curChunk == sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks - 1) ?
											   sce::Gnm::kDwordSizeConstantBuffer * (sce::Gnm::kSlotCountConstantBuffer - sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize*(sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks-1)) :
											   ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord;
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRamForConstantBuffer(m_ccb, stage, curChunk, ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord, allocSizeInDWords);
		}

			
		SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kConstantBufferSizeInDqWord == 1);
		chunkState[curChunk].curChunk[slot]   = 0;
			
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

		chunkState[curChunk].curSlots  &= ~slotMask;
		chunkState[curChunk].usedSlots &= ~slotMask; // to avoid keeping older version of this resource

		if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
		{
			const uint32_t allocSizeInDWords = (curChunk == sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks - 1) ?
											   sce::Gnm::kDwordSizeConstantBuffer * (sce::Gnm::kSlotCountConstantBuffer - sce::Gnmx::ConstantUpdateEngine::kConstantBufferChunkSize*(sce::Gnmx::ConstantUpdateEngine::kConstantBufferNumChunks-1)) :
											   ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord;
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRamForConstantBuffer(m_ccb, stage, curChunk, ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord, allocSizeInDWords);
		}
	}


	const uint32_t apiMask = 1u << apiSlot;

	// Check if we need to dirty the EUD:
	if ( m_stageInfo[stage].eudConstantBufferSet & apiMask )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeConstantBuffer |= apiMask;

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexConstantBuffer);
	}
	else
	{
		m_stageInfo[stage].activeConstantBuffer &= ~apiMask;

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexConstantBuffer);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setRwTextures(sce::Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const sce::Gnm::Texture *rwTextures)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(startApiSlot);
	SCE_GNM_UNUSED(numApiSlots);
	SCE_GNM_UNUSED(rwTextures);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableRwResource], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(startApiSlot < m_ringSetup.numRwResourceSlots, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, m_ringSetup.numRwResourceSlots-1);
	SCE_GNM_ASSERT_MSG(numApiSlots <= m_ringSetup.numRwResourceSlots, "numApiSlots (%u) must be in the range [0..%u].", numApiSlots, m_ringSetup.numRwResourceSlots);
	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= m_ringSetup.numRwResourceSlots, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%u].", startApiSlot, numApiSlots, m_ringSetup.numRwResourceSlots);
	if ( numApiSlots == 0 )
		return;

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !rwTextures ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// The user may set N T# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t startChunk = startApiSlot / kRwResourceChunkSize;
	const uint32_t endChunk   = (startApiSlot+numApiSlots-1) / kRwResourceChunkSize;

	const uint16_t initialSlot = (startApiSlot & ConstantUpdateEngineHelper::kRwResourceChunkSizeMask);
	const uint16_t finalSlot   = ((startApiSlot+numApiSlots-1) & ConstantUpdateEngineHelper::kRwResourceChunkSizeMask);

	StageChunkState *chunkState = m_stageInfo[stage].rwResourceStage;

	uint32_t  curChunk = startChunk;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(rwTextures);

	while ( curChunk <= endChunk )
	{
		const uint16_t startSlot = curChunk == startChunk ? initialSlot : 0;
		const uint16_t endSlot   = curChunk == endChunk   ? finalSlot   : ConstantUpdateEngineHelper::kRwResourceChunkSizeMask; // aka: kRwResourceChunkSize-1
		const uint16_t range = static_cast<uint16_t>(((1 << (16 - endSlot - 1)) - 1) ^ ((1 << (16 - startSlot)) - 1));
		//const uint16_t range     = ((1 << (endSlot+1))-1) ^ ((1 << startSlot)-1);

		//

		// Check for a resource conflict between previous draw and current draw:
		// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
		if ( !chunkState[curChunk].usedChunk &&
			 (chunkState[curChunk].curSlots & range) )
		{
			// Conflict
			chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
			chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
			chunkState[curChunk].curChunk  = 0;
			chunkState[curChunk].curSlots  = 0;
		}

		if ( curData )
		{
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
				SCE_GNM_ASSERT_MSG(chunkState[curChunk].curChunk != NULL, "allocateRegionToCopyToCpRam() failed");
			}

			// Copy the user resources in the current chunk:
			// TODO: Check is unrolling the loop would help.
			for (uint32_t iRwResource = startSlot; iRwResource <= endSlot; ++iRwResource)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kRwResourceSizeInDqWord == 2);

				chunkState[curChunk].curChunk[2*iRwResource]   = *(curData+0);
				chunkState[curChunk].curChunk[2*iRwResource+1] = *(curData+1);
				curData += 2;
			}
			//
			chunkState[curChunk].curSlots |= range;
		}
		else
		{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
			}

			for (uint32_t iRwResource = startSlot; iRwResource <= endSlot; ++iRwResource)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kRwResourceSizeInDqWord == 2);
				chunkState[curChunk].curChunk[2*iRwResource]   = 0;
				chunkState[curChunk].curChunk[2*iRwResource+1] = 0;
			}
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

			chunkState[curChunk].curSlots  &= ~range;
			chunkState[curChunk].usedSlots &= ~range; // to avoid keeping older version of this resource

			if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
			}
		}

		// Next chunk:
		curChunk++;
	}

	const uint16_t apiMask = static_cast<uint16_t>(((1 << (startApiSlot + numApiSlots)) - 1) ^ ((1 << startApiSlot) - 1));

	// Check if we need to dirty the EUD:
	if ( m_stageInfo[stage].eudRwResourceSet & apiMask )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeRwResource |= apiMask;

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexRwResource);
	}
	else
	{
		m_stageInfo[stage].activeRwResource &= ~apiMask;

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexRwResource);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setRwTexture(sce::Gnm::ShaderStage stage, uint32_t apiSlot, const sce::Gnm::Texture *rwTexture)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(apiSlot);
	SCE_GNM_UNUSED(rwTexture);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableRwResource], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(apiSlot < m_ringSetup.numRwResourceSlots, "startApiSlot (%u) must be in the range [0..%d].", apiSlot, m_ringSetup.numRwResourceSlots-1);

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !rwTexture ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// The user may set N T# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t curChunk = apiSlot / kRwResourceChunkSize;

	const uint32_t slot = (apiSlot & ConstantUpdateEngineHelper::kRwResourceChunkSizeMask);
	const uint16_t slotMask = static_cast<uint16_t>(1 << (16 - slot - 1));

	StageChunkState *chunkState = m_stageInfo[stage].rwResourceStage;


	const __int128_t *curData = reinterpret_cast<const __int128_t*>(rwTexture);

	//

	// Check for a resource conflict between previous draw and current draw:
	// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
	if ( !chunkState[curChunk].usedChunk &&
		(chunkState[curChunk].curSlots & slotMask) )
	{
		// Conflict
		chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
		chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
		chunkState[curChunk].curChunk  = 0;
		chunkState[curChunk].curSlots  = 0;
	}

	if ( curData )
	{
		// No chunk allocated for the current draw (due to conflict, or because it was never used before)
		if ( !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
		}

		// Copy the user resources in the current chunk:
		// TODO: Check is unrolling the loop would help.
		SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kRwResourceSizeInDqWord == 2);

		chunkState[curChunk].curChunk[2*slot]   = *(curData+0);
		chunkState[curChunk].curChunk[2*slot+1] = *(curData+1);


		chunkState[curChunk].curSlots |= slotMask;
	}
	else
	{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// No chunk allocated for the current draw (due to conflict, or because it was never used before)
		if ( !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
		}


		SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kRwResourceSizeInDqWord == 2);
		chunkState[curChunk].curChunk[2*slot]   = 0;
		chunkState[curChunk].curChunk[2*slot+1] = 0;

#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

		chunkState[curChunk].curSlots  &= ~slotMask;
		chunkState[curChunk].usedSlots &= ~slotMask; // to avoid keeping older version of this resource

		if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
		}
	}

	const uint16_t apiMask = static_cast<uint16_t>(1 << apiSlot);

	// Check if we need to dirty the EUD:
	if ( m_stageInfo[stage].eudRwResourceSet & apiMask )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeRwResource |= apiMask;

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexRwResource);
	}
	else
	{
		m_stageInfo[stage].activeRwResource &= ~apiMask;

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexRwResource);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setRwBuffers(sce::Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const sce::Gnm::Buffer *rwBuffers)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(startApiSlot);
	SCE_GNM_UNUSED(numApiSlots);
	SCE_GNM_UNUSED(rwBuffers);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableRwResource], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(startApiSlot < m_ringSetup.numRwResourceSlots, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, m_ringSetup.numRwResourceSlots-1);
	SCE_GNM_ASSERT_MSG(numApiSlots <= m_ringSetup.numRwResourceSlots, "numApiSlots (%u) must be in the range [0..%u].", numApiSlots, m_ringSetup.numRwResourceSlots);
	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= m_ringSetup.numRwResourceSlots, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%u].", startApiSlot, numApiSlots, m_ringSetup.numRwResourceSlots);
	if ( numApiSlots == 0 )
		return;

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !rwBuffers ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// The user may set N T# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t startChunk = startApiSlot / kRwResourceChunkSize;
	const uint32_t endChunk   = (startApiSlot+numApiSlots-1) / kRwResourceChunkSize;

	const uint16_t initialSlot = (startApiSlot & ConstantUpdateEngineHelper::kRwResourceChunkSizeMask);
	const uint16_t finalSlot   = ((startApiSlot+numApiSlots-1) & ConstantUpdateEngineHelper::kRwResourceChunkSizeMask);

	StageChunkState *chunkState = m_stageInfo[stage].rwResourceStage;

	uint32_t  curChunk = startChunk;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(rwBuffers);

	while ( curChunk <= endChunk )
	{
		const uint16_t startSlot = curChunk == startChunk ? initialSlot : 0;
		const uint16_t endSlot   = curChunk == endChunk   ? finalSlot   : ConstantUpdateEngineHelper::kRwResourceChunkSizeMask; // aka: kRwResourceChunkSize-1
		const uint16_t range = static_cast<uint16_t>(((1 << (16 - endSlot - 1)) - 1) ^ ((1 << (16 - startSlot)) - 1));
		//const uint16_t range     = ((1 << (endSlot+1))-1) ^ ((1 << startSlot)-1);

		//

		// Check for a resource conflict between previous draw and current draw:
		// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
		if ( !chunkState[curChunk].usedChunk &&
			 (chunkState[curChunk].curSlots & range) )
		{
			// Conflict
			chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
			chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
			chunkState[curChunk].curChunk  = 0;
			chunkState[curChunk].curSlots  = 0;
		}

		if ( curData )
		{
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
				SCE_GNM_ASSERT_MSG(chunkState[curChunk].curChunk != NULL, "allocateRegionToCopyToCpRam() failed");
			}

			// Copy the user resources in the current chunk:
			// TODO: Check is unrolling the loop would help.
			for (uint32_t iRwResource = startSlot; iRwResource <= endSlot; ++iRwResource)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kRwResourceSizeInDqWord == 2);

				chunkState[curChunk].curChunk[2*iRwResource]   = *(curData+0);
				chunkState[curChunk].curChunk[2*iRwResource+1] = 0;
				curData += 1;
			}
			//
			chunkState[curChunk].curSlots |= range;
		}
		else
		{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
			}

			for (uint32_t iRwResource = startSlot; iRwResource <= endSlot; ++iRwResource)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kRwResourceSizeInDqWord == 2);
				chunkState[curChunk].curChunk[2*iRwResource]   = 0;
				chunkState[curChunk].curChunk[2*iRwResource+1] = 0;
			}
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

			chunkState[curChunk].curSlots  &= ~range;
			chunkState[curChunk].usedSlots &= ~range; // to avoid keeping older version of this resource

			if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
			}
		}

		// Next chunk:
		curChunk++;
	}

	const uint16_t apiMask = static_cast<uint16_t>(((1 << (startApiSlot + numApiSlots)) - 1) ^ ((1 << startApiSlot) - 1));

	// Check if we need to dirty the EUD:
	if ( m_stageInfo[stage].eudRwResourceSet & apiMask )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeRwResource |= apiMask;

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexRwResource);
	}
	else
	{
		m_stageInfo[stage].activeRwResource &= ~apiMask;

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexRwResource);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setRwBuffer(sce::Gnm::ShaderStage stage, uint32_t apiSlot, const sce::Gnm::Buffer *rwBuffer)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(apiSlot);
	SCE_GNM_UNUSED(rwBuffer);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableRwResource], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(apiSlot < m_ringSetup.numRwResourceSlots, "startApiSlot (%u) must be in the range [0..%d].", apiSlot, m_ringSetup.numRwResourceSlots-1);
	
#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !rwBuffer ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// The user may set N T# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t curChunk = apiSlot / kRwResourceChunkSize;
	const uint32_t slot = (apiSlot & ConstantUpdateEngineHelper::kRwResourceChunkSizeMask);
	const uint16_t slotMask = static_cast<uint16_t>(1 << (16 - slot - 1));

	StageChunkState *chunkState = m_stageInfo[stage].rwResourceStage;

	const __int128_t *curData = reinterpret_cast<const __int128_t*>(rwBuffer);


	//

	// Check for a resource conflict between previous draw and current draw:
	// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
	if ( !chunkState[curChunk].usedChunk &&
		 (chunkState[curChunk].curSlots & slotMask) )
	{
		// Conflict
		chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
		chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
		chunkState[curChunk].curChunk  = 0;
		chunkState[curChunk].curSlots  = 0;
	}

	if ( curData )
	{
		// No chunk allocated for the current draw (due to conflict, or because it was never used before)
		if ( !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
		}

		// Copy the user resources in the current chunk:
			
		SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kRwResourceSizeInDqWord == 2);

		chunkState[curChunk].curChunk[2*slot]   = *(curData+0);
		chunkState[curChunk].curChunk[2*slot+1] = 0;
			
		//
		chunkState[curChunk].curSlots |= slotMask;
	}
	else
	{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// No chunk allocated for the current draw (due to conflict, or because it was never used before)
		if ( !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
		}

			
		SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kRwResourceSizeInDqWord == 2);
		chunkState[curChunk].curChunk[2*slot]   = 0;
		chunkState[curChunk].curChunk[2*slot+1] = 0;
			
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

		chunkState[curChunk].curSlots  &= ~slotMask;
		chunkState[curChunk].usedSlots &= ~slotMask; // to avoid keeping older version of this resource

		if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
		{
			chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetRwResource, curChunk, ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord);
		}
	}

	const uint16_t apiMask = static_cast<uint16_t>(1 << apiSlot);

	// Check if we need to dirty the EUD:
	if ( m_stageInfo[stage].eudRwResourceSet & apiMask )
		m_shaderDirtyEud |= (1 << stage);

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeRwResource |= apiMask;

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexRwResource);
	}
	else
	{
		m_stageInfo[stage].activeRwResource &= ~apiMask;

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexRwResource);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setVertexBuffers(sce::Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const sce::Gnm::Buffer *buffers)
{
#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(startApiSlot);
	SCE_GNM_UNUSED(numApiSlots);
	SCE_GNM_UNUSED(buffers);
	return;
#else //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG( !m_stageInfo[stage].userTable[kUserTableVertexBuffer], "Setting an individual resource while a user resource table is active");
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET && SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
	SCE_GNM_ASSERT_MSG(startApiSlot < m_ringSetup.numVertexBufferSlots, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, m_ringSetup.numVertexBufferSlots-1);
	SCE_GNM_ASSERT_MSG(numApiSlots <= m_ringSetup.numVertexBufferSlots, "numApiSlots (%u) must be in the range [0..%u].", numApiSlots, m_ringSetup.numVertexBufferSlots);
	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= m_ringSetup.numVertexBufferSlots, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%u].", startApiSlot, numApiSlots, m_ringSetup.numVertexBufferSlots);
	if ( numApiSlots == 0 )
		return;

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !buffers ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// The user may set N T# in a row; which could overlap multiple chunk,
	// compute start and end for
	//

	const uint32_t startChunk = startApiSlot / kVertexBufferChunkSize;
	const uint32_t endChunk   = (startApiSlot+numApiSlots-1) / kVertexBufferChunkSize;

	const uint16_t initialSlot = (startApiSlot & ConstantUpdateEngineHelper::kVertexBufferChunkSizeMask);
	const uint16_t finalSlot   = ((startApiSlot+numApiSlots-1) & ConstantUpdateEngineHelper::kVertexBufferChunkSizeMask);

	StageChunkState *chunkState = m_stageInfo[stage].vertexBufferStage;

	uint32_t  curChunk = startChunk;
	const __int128_t *curData = reinterpret_cast<const __int128_t*>(buffers);

	while ( curChunk <= endChunk )
	{
		const uint16_t startSlot = curChunk == startChunk ? initialSlot : 0;
		const uint16_t endSlot   = curChunk == endChunk   ? finalSlot   : ConstantUpdateEngineHelper::kVertexBufferChunkSizeMask; // aka: kVertexBufferChunkSize-1
		const uint16_t range = static_cast<uint16_t>(((1 << (16 - endSlot - 1)) - 1) ^ ((1 << (16 - startSlot)) - 1));
		//const uint16_t range     = ((1 << (endSlot+1))-1) ^ ((1 << startSlot)-1);

		//

		// Check for a resource conflict between previous draw and current draw:
		// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
		if ( !chunkState[curChunk].usedChunk &&
			 (chunkState[curChunk].curSlots & range) )
		{
			// Conflict
			chunkState[curChunk].usedSlots = chunkState[curChunk].curSlots;
			chunkState[curChunk].usedChunk = chunkState[curChunk].curChunk;
			chunkState[curChunk].curChunk  = 0;
			chunkState[curChunk].curSlots  = 0;
		}

		if ( curData )
		{
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetVertexBuffer, curChunk, ConstantUpdateEngineHelper::kVertexBufferChunkSizeInDWord);
				SCE_GNM_ASSERT_MSG(chunkState[curChunk].curChunk != NULL, "allocateRegionToCopyToCpRam() failed");
			}

			// Copy the user resources in the current chunk:
			// TODO: Check is unrolling the loop would help.
			for (uint32_t iVertexBuffer = startSlot; iVertexBuffer <= endSlot; ++iVertexBuffer)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kVertexBufferSizeInDqWord == 1);

				chunkState[curChunk].curChunk[iVertexBuffer] = *(curData+0);
				curData += 1;
			}
			//
			chunkState[curChunk].curSlots |= range;
		}
		else
		{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
			// No chunk allocated for the current draw (due to conflict, or because it was never used before)
			if ( !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetVertexBuffer, curChunk, ConstantUpdateEngineHelper::kVertexBufferChunkSizeInDWord);
			}

			for (uint32_t iVertexBuffer = startSlot; iVertexBuffer <= endSlot; ++iVertexBuffer)
			{
				SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kVertexBufferSizeInDqWord == 1);
				chunkState[curChunk].curChunk[iVertexBuffer] = 0;
			}
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

			chunkState[curChunk].curSlots  &= ~range;
			chunkState[curChunk].usedSlots &= ~range; // to avoid keeping older version of this resource

			if ( chunkState[curChunk].usedSlots && !chunkState[curChunk].curChunk )
			{
				chunkState[curChunk].curChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb, stage, ConstantUpdateEngineHelper::kDwordOffsetVertexBuffer, curChunk, ConstantUpdateEngineHelper::kVertexBufferChunkSizeInDWord);
			}
		}

		// Next chunk:
		curChunk++;
	}

	const uint32_t apiMask = uint32_t( ((1ULL<<(startApiSlot+numApiSlots))-1) ^ ((1ULL<<startApiSlot)-1) );

	// TODO: Dirty the EUD if the vertex buffer ptr is set in the EUD

	// Keep track of used APIs:
	if ( curData )
	{
		m_stageInfo[stage].activeVertexBuffer |= apiMask;

		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexVertexBuffer);
	}
	else
	{
		m_stageInfo[stage].activeVertexBuffer &= ~apiMask;

#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		// Mark the resource ptr dirty:
		m_stageInfo[stage].dirtyRing |= 1<<(31-kRingBuffersIndexVertexBuffer);
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
	}
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
}

void ConstantUpdateEngine::setBoolConstants(sce::Gnm::ShaderStage stage, uint32_t maskAnd, uint32_t maskOr)
{
	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.
	m_stageInfo[stage].boolValue = (m_stageInfo[stage].boolValue & maskAnd) | maskOr;
}

void ConstantUpdateEngine::setFloatConstants(sce::Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const float *floats)
{
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(startApiSlot);
	SCE_GNM_UNUSED(numApiSlots);
	SCE_GNM_UNUSED(floats);
	SCE_GNM_ERROR("Not yet implemented in CUE2!");
	SCE_GNM_STATIC_ASSERT(!CUE2_SHOW_UNIMPLEMENTED);
}

void ConstantUpdateEngine::setAppendConsumeCounterRange(sce::Gnm::ShaderStage stage, uint32_t rangeGdsOffsetInBytes, uint32_t rangeSizeInBytes)
{
	SCE_GNM_ASSERT_MSG(((rangeGdsOffsetInBytes | rangeSizeInBytes)&3) == 0, "baseOffsetInBytes (%u) as well as baseOffsetInBytes (%u) must be multiple of 4s.", rangeGdsOffsetInBytes, rangeSizeInBytes);
	SCE_GNM_ASSERT_MSG(rangeGdsOffsetInBytes < kGdsAccessibleMemorySizeInBytes, "baseOffsetInBytes (%u) must be less than %i.", rangeGdsOffsetInBytes, kGdsAccessibleMemorySizeInBytes);
	SCE_GNM_ASSERT_MSG(rangeSizeInBytes <= kGdsAccessibleMemorySizeInBytes, "rangeSizeInBytes (%u) must not exceed %i.", rangeSizeInBytes, kGdsAccessibleMemorySizeInBytes);
	SCE_GNM_ASSERT_MSG((rangeGdsOffsetInBytes+rangeSizeInBytes) <= kGdsAccessibleMemorySizeInBytes, "baseOffsetInBytes (%u) + rangeSizeInBytes (%u) must not exceed %i.", rangeGdsOffsetInBytes, rangeSizeInBytes, kGdsAccessibleMemorySizeInBytes);
	m_stageInfo[stage].appendConsumeDword = (rangeGdsOffsetInBytes << 16) | rangeSizeInBytes;
}

void ConstantUpdateEngine::setGdsMemoryRange(sce::Gnm::ShaderStage stage, uint32_t rangeGdsOffsetInBytes, uint32_t rangeSizeInBytes)
{
	SCE_GNM_ASSERT_MSG(((rangeGdsOffsetInBytes | rangeSizeInBytes)&3) == 0, "baseOffsetInBytes (%u) as well as baseOffsetInBytes (%u) must be multiple of 4s.", rangeGdsOffsetInBytes, rangeSizeInBytes);
	SCE_GNM_ASSERT_MSG(rangeGdsOffsetInBytes < kGdsAccessibleMemorySizeInBytes, "baseOffsetInBytes (%u) must be less than %i.", rangeGdsOffsetInBytes, kGdsAccessibleMemorySizeInBytes);
	SCE_GNM_ASSERT_MSG(rangeSizeInBytes <= kGdsAccessibleMemorySizeInBytes, "rangeSizeInBytes (%u) must not exceed %i.", rangeSizeInBytes, kGdsAccessibleMemorySizeInBytes);
	SCE_GNM_ASSERT_MSG((rangeGdsOffsetInBytes+rangeSizeInBytes) <= kGdsAccessibleMemorySizeInBytes, "baseOffsetInBytes (%u) + rangeSizeInBytes (%u) must not exceed %i.", rangeGdsOffsetInBytes, rangeSizeInBytes, kGdsAccessibleMemorySizeInBytes);
	m_stageInfo[stage].gdsMemoryRangeDword = (rangeGdsOffsetInBytes << 16) | rangeSizeInBytes;
}

void ConstantUpdateEngine::setStreamoutBuffers(uint32_t startApiSlot, uint32_t numApiSlots, const sce::Gnm::Buffer *buffers)
{
	SCE_GNM_ASSERT_MSG(startApiSlot < sce::Gnm::kSlotCountStreamoutBuffer, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, sce::Gnm::kSlotCountStreamoutBuffer-1);
	SCE_GNM_ASSERT_MSG(numApiSlots <= sce::Gnm::kSlotCountStreamoutBuffer, "numApiSlots (%u) must be in the range [0..%d].", numApiSlots, sce::Gnm::kSlotCountStreamoutBuffer);
	SCE_GNM_ASSERT_MSG(startApiSlot+numApiSlots <= sce::Gnm::kSlotCountStreamoutBuffer, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%d].", startApiSlot, numApiSlots, sce::Gnm::kSlotCountConstantBuffer);
	if ( numApiSlots == 0 )
		return;

#if SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
	if ( !buffers ) return;
#endif // SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL

	m_dirtyStage |= (1 << sce::Gnm::kShaderStageGs); // technically we don't need to dirty the stage if the resource is NULL.

	//
	// Only 4 streamout buffers.
	// Only one chunk
	//

	const uint32_t	stage	  = Gnm::kShaderStageVs;
	const uint32_t	startSlot = (startApiSlot);
	const uint32_t	endSlot   = (startApiSlot+numApiSlots-1);
	const uint16_t	range = static_cast<uint16_t>(((1 << (16 - endSlot - 1)) - 1) ^ ((1 << (16 - startSlot)) - 1));
	//const uint16_t range     = ((1 << (endSlot+1))-1) ^ ((1 << startSlot)-1);

	StageChunkState *chunkState = &m_streamoutBufferStage;

	const __int128_t *curData = reinterpret_cast<const __int128_t*>(buffers);

	// Check for a resource conflict between previous draw and current draw:
	// -> a conflict may happen if a texture used in the previous draw is set again for the current one.
	if ( !chunkState->usedChunk &&
		 (chunkState->curSlots & range) )
	{
		// Conflict
		chunkState->usedSlots = chunkState->curSlots;
		chunkState->usedChunk = chunkState->curChunk;
		chunkState->curChunk  = 0;
	}

	// No chunk allocated for the current draw (due to conflict, or because it was never used before)
	if ( !chunkState->curChunk )
	{
		chunkState->curSlots = 0;
		chunkState->curChunk = (__int128_t*)m_dcb->allocateFromCommandBuffer(ConstantUpdateEngineHelper::kStreamoutChunkSizeInBytes, Gnm::kEmbeddedDataAlignment16);

		// Check if we need to dirty the EUD:
		if ( m_eudReferencesStreamoutBuffers )
			m_shaderDirtyEud |= (1 << stage);
	}

	if ( curData )
	{
		// Copy the user resources in the current chunk:
		// TODO: Check is unrolling the loop would help.
		for (uint32_t iSoBuffer = startSlot; iSoBuffer <= endSlot; ++iSoBuffer)
		{
			SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kStreamoutBufferSizeInDqWord == 1);
			chunkState->curChunk[iSoBuffer] = *(curData+0);
			curData += 1;
		}
		//
		chunkState->curSlots |= range;
	}
	else
	{
#if SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
		for (uint32_t iSoBuffer = startSlot; iSoBuffer <= endSlot; ++iSoBuffer)
		{
			SCE_GNM_STATIC_ASSERT(ConstantUpdateEngineHelper::kStreamoutBufferSizeInDqWord == 1);
			chunkState->curChunk[iSoBuffer]   = 0;
		}
#endif // SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES

		chunkState->curSlots  &= ~range;
		chunkState->usedSlots &= ~range; // to avoid keeping older version of this resource
	}
}

void ConstantUpdateEngine::setInternalSrtBuffer(ShaderStage stage, void *buf)
{
#if SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS
	SCE_GNM_ERROR("setInternalSrtBuffer() has no effect in the mixed mode SRT shaders, if you need to use this function please disable SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS option in the gnmx/config.h");
	SCE_GNM_UNUSED(stage);
	SCE_GNM_UNUSED(buf);
#else //SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS
	m_stageInfo[stage].internalSrtBuffer = buf;
	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.
#endif //SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS
}

void ConstantUpdateEngine::setUserSrtBuffer(ShaderStage stage, const void *buf, uint32_t bufSizeInDwords)
{
	SCE_GNM_ASSERT_MSG(bufSizeInDwords<=16, "The SRT buffer size is limited to the number of USER SGPRs availabe (i.e: 16 DWs)");

	const uint32_t		*bufDwords	 = (const uint32_t *)buf;
	uint32_t			*stageDwords = m_stageInfo[stage].userSrtBuffer;

	m_dirtyStage |= (1 << stage); // technically we don't need to dirty the stage if the resource is NULL.

	switch(bufSizeInDwords)
	{
	case 16: stageDwords[15] = bufDwords[15]; SCE_GNM_FALL_THROUGH;
	case 15: stageDwords[14] = bufDwords[14]; SCE_GNM_FALL_THROUGH;
	case 14: stageDwords[13] = bufDwords[13]; SCE_GNM_FALL_THROUGH;
	case 13: stageDwords[12] = bufDwords[12]; SCE_GNM_FALL_THROUGH;
	case 12: stageDwords[11] = bufDwords[11]; SCE_GNM_FALL_THROUGH;
	case 11: stageDwords[10] = bufDwords[10]; SCE_GNM_FALL_THROUGH;
	case 10: stageDwords[9] = bufDwords[9]; SCE_GNM_FALL_THROUGH;
	case  9: stageDwords[8] = bufDwords[8]; SCE_GNM_FALL_THROUGH;
	case  8: stageDwords[7] = bufDwords[7]; SCE_GNM_FALL_THROUGH;
	case  7: stageDwords[6] = bufDwords[6]; SCE_GNM_FALL_THROUGH;
	case  6: stageDwords[5] = bufDwords[5]; SCE_GNM_FALL_THROUGH;
	case  5: stageDwords[4] = bufDwords[4]; SCE_GNM_FALL_THROUGH;
	case  4: stageDwords[3] = bufDwords[3]; SCE_GNM_FALL_THROUGH;
	case  3: stageDwords[2] = bufDwords[2]; SCE_GNM_FALL_THROUGH;
	case  2: stageDwords[1] = bufDwords[1]; SCE_GNM_FALL_THROUGH;
	case  1: stageDwords[0] = bufDwords[0]; 
	}

	m_stageInfo[stage].userSrtBufferSizeInDwords = bufSizeInDwords;
}



//------


// Copy un-overwritten resource from the used by a previous draw to the current chunk.
// e.g: updateChunkState(m_resourceStage[stage], kTextureNumChunks)
void ConstantUpdateEngine::updateChunkState128(StageChunkState *chunkState, uint32_t numChunks)
{
	// This should only be done if the bound shaders request it.

	// TODO: Check is unrolling the loop would help.
	for (uint32_t iChunk = 0; iChunk < numChunks ; ++iChunk)
	{
		// Check which resources needs to be copy from chunk of the previous draw to the current one.
		uint16_t maskToUpdate = chunkState[iChunk].usedSlots & (~chunkState[iChunk].curSlots);

		while ( maskToUpdate )
		{
			const uint16_t ndx = __lzcnt16(maskToUpdate); // 0 represent the high bit (0x8000)
			const uint16_t bit = static_cast<uint16_t>(1 << (15 - ndx));
			const uint16_t clr = ~bit;

			// Copy the current:
			chunkState[iChunk].curChunk[ndx]   = chunkState[iChunk].usedChunk[ndx];
			maskToUpdate &= clr;
		}

		if ( chunkState[iChunk].curChunk )
			chunkState[iChunk].usedChunk = 0;
		chunkState[iChunk].curSlots |= chunkState[iChunk].usedSlots;
	}
}

void ConstantUpdateEngine::updateChunkState256(StageChunkState *chunkState, uint32_t numChunks)
{
	// This should only be done if the bound shaders request it.

	// TODO: Check is unrolling the loop would help.
    for (uint32_t iChunk = 0; iChunk < numChunks ; ++iChunk)
	{
	    // Check which resources needs to be copy from chunk of the previous draw to the current one.
		uint16_t maskToUpdate = chunkState[iChunk].usedSlots & (~chunkState[iChunk].curSlots);

		while ( maskToUpdate )
		{
			const uint16_t ndx = __lzcnt16(maskToUpdate); // 0 represent the high bit (0x8000)
			const uint16_t bit = static_cast<uint16_t>(1 << (15 - ndx));
			const uint16_t clr = ~bit;

			// Copy the current:
			chunkState[iChunk].curChunk[2*ndx]   = chunkState[iChunk].usedChunk[2*ndx];
			chunkState[iChunk].curChunk[2*ndx+1] = chunkState[iChunk].usedChunk[2*ndx+1];
			maskToUpdate &= clr;
		}

		if ( chunkState[iChunk].curChunk )
			chunkState[iChunk].usedChunk = 0;
		chunkState[iChunk].curSlots |= chunkState[iChunk].usedSlots;
	}
}

const void* ConstantUpdateEngine::copyChunkState(ShaderStage stage, uint32_t baseResourceOffset, uint32_t chunkSizeInDw, uint32_t numChunks, StageChunkState* chunkState, const void* fence )
{
	const void *lowest = fence;
	for (uint32_t iChunk = 0; iChunk < numChunks ; ++iChunk)
	{
		StageChunkState *pChunk = chunkState + iChunk;
		// the usedChunk always preceedes the curChunk in the CCB 
		// so we need to copy them in the same order here, otherwise the old data will stomp over the new
		if( pChunk->usedChunk && pChunk->usedChunk < fence)
		{
			void* newChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb,stage,baseResourceOffset,iChunk,chunkSizeInDw);
			memcpy(newChunk,pChunk->usedChunk,chunkSizeInDw*sizeof(uint32_t));
			pChunk->usedChunk = reinterpret_cast<__int128_t *>(newChunk);
		}
		if( pChunk->curChunk && pChunk->curChunk < fence)
		{
			void* newChunk = ConstantUpdateEngineHelper::allocateRegionToCopyToCpRam(m_ccb,stage,baseResourceOffset,iChunk,chunkSizeInDw);
			memcpy(newChunk,pChunk->curChunk,chunkSizeInDw*sizeof(uint32_t));
			pChunk->curChunk = reinterpret_cast<__int128_t *>(newChunk);
		}
		
	}
	return lowest;
}
const void* ConstantUpdateEngine::moveConstantStateAfterAddress(const void* fence)
{

	SCE_GNM_ASSERT_MSG(fence, "The moveConstantStateAfterAddress' argument cannot be NULL");
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY	
	for(int stage = 0; stage != kShaderStageCount; ++stage)
	{
		StageInfo *pChunkState = m_stageInfo + stage;

		copyChunkState(static_cast<ShaderStage>(stage),ConstantUpdateEngineHelper::kDwordOffsetResource,ConstantUpdateEngineHelper::kResourceChunkSizeInDWord,kResourceNumChunks,pChunkState->resourceStage,fence);
		copyChunkState(static_cast<ShaderStage>(stage),ConstantUpdateEngineHelper::kDwordOffsetRwResource,ConstantUpdateEngineHelper::kRwResourceChunkSizeInDWord,kRwResourceNumChunks,pChunkState->rwResourceStage,fence);
		copyChunkState(static_cast<ShaderStage>(stage),ConstantUpdateEngineHelper::kDwordOffsetSampler,ConstantUpdateEngineHelper::kSamplerChunkSizeInDWord,kSamplerNumChunks,pChunkState->samplerStage,fence);
		copyChunkState(static_cast<ShaderStage>(stage),ConstantUpdateEngineHelper::kDwordOffsetVertexBuffer,ConstantUpdateEngineHelper::kVertexBufferChunkSizeInDWord,kVertexBufferNumChunks,pChunkState->vertexBufferStage,fence);
		copyChunkState(static_cast<ShaderStage>(stage),ConstantUpdateEngineHelper::kDwordOffsetConstantBuffer,ConstantUpdateEngineHelper::kConstantBufferChunkSizeInDWord,kConstantBufferNumChunks,pChunkState->constantBufferStage,fence);
	}
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY	
	return fence;

}
