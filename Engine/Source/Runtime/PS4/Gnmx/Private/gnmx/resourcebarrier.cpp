/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx/resourcebarrier.h>
#include <gnmx/surfacetool.h>
#include <gnm/buffer.h>
#include <gnm/depthrendertarget.h>
#include <gnm/dispatchcommandbuffer.h>
#include <gnm/drawcommandbuffer.h>
#include <gnm/unsafedispatchcommandbuffer.h>
#include <gnm/unsafedrawcommandbuffer.h>
#include <gnm/rendertarget.h>
#include <gnm/texture.h>
#include <immintrin.h>
#if defined(__ORBIS__) || defined(GNM_INTERNAL)
#include <gnm/platform.h>
#endif // defined(__ORBIS__) || defined(GNM_INTERNAL)
using namespace sce;


void Gnmx::ResourceBarrier::init(const Gnm::RenderTarget *target, Usage oldUsage, Usage newUsage)
{
	SCE_GNM_ASSERT_MSG(target != NULL, "target must not be NULL.");
	SCE_GNM_ASSERT_MSG(oldUsage < kNumUsages, "Invalid enum value (%d) for oldUsage", oldUsage);
	SCE_GNM_ASSERT_MSG(newUsage < kNumUsages, "Invalid enum value (%d) for newUsage", newUsage);

	m_oldUsage = oldUsage;
	m_newUsage = newUsage;

	m_flags = 0;
	m_resourceBaseAddr256 = target->getBaseAddress256ByteBlocks();
	int32_t currentNumSlices = 1 + target->getLastArraySliceIndex() - target->getBaseArraySliceIndex();
	m_resourceSize256 = currentNumSlices * target->getSliceSizeInBytes() / 256;

    m_type = Type::kRenderTarget;
    m_renderTarget = *target;
}

void Gnmx::ResourceBarrier::init(const Gnm::DepthRenderTarget *depthTarget, Usage oldUsage, Usage newUsage)
{
	SCE_GNM_ASSERT_MSG(depthTarget != NULL, "depthTarget must not be NULL.");
	SCE_GNM_ASSERT_MSG(oldUsage < kNumUsages, "Invalid enum value (%d) for oldUsage", oldUsage);
	SCE_GNM_ASSERT_MSG(newUsage < kNumUsages, "Invalid enum value (%d) for newUsage", newUsage);

	m_oldUsage = oldUsage;
	m_newUsage = newUsage;

	m_flags = 0;
	int32_t currentNumSlices = 1 + depthTarget->getLastArraySliceIndex() - depthTarget->getBaseArraySliceIndex();
	if (oldUsage == kUsageStencilSurface)
	{
		m_resourceBaseAddr256 = depthTarget->getStencilWriteAddress256ByteBlocks();
		m_resourceSize256 = currentNumSlices * depthTarget->getStencilSliceSizeInBytes() / 256;
	}
	else
	{
		m_resourceBaseAddr256 = depthTarget->getZWriteAddress256ByteBlocks();
		m_resourceSize256 = currentNumSlices * depthTarget->getZSliceSizeInBytes() / 256;
	}

    m_type = Type::kDepthRenderTarget;
    m_depthRenderTarget = *depthTarget;
}
void Gnmx::ResourceBarrier::init(const Gnm::Texture *texture, Usage oldUsage, Usage newUsage)
{
	SCE_GNM_ASSERT_MSG(texture != NULL, "texture must not be NULL.");
	SCE_GNM_ASSERT_MSG(oldUsage < kNumUsages, "Invalid enum value (%d) for oldUsage", oldUsage);
	SCE_GNM_ASSERT_MSG(newUsage < kNumUsages, "Invalid enum value (%d) for newUsage", newUsage);

	m_oldUsage = oldUsage;
	m_newUsage = newUsage;

	uint64_t tiledTextureSize = 0;
	Gnm::AlignmentType textureAlign;
	GpuAddress::computeTotalTiledTextureSize(&tiledTextureSize, &textureAlign, texture);
	SCE_GNM_UNUSED(textureAlign);
	m_flags = 0;
	m_resourceBaseAddr256    = texture->getBaseAddress256ByteBlocks();
	m_resourceSize256        = (uint32_t)(tiledTextureSize/256);

    m_type = Type::kTexture;
    m_texture = *texture;
}
void Gnmx::ResourceBarrier::init(const Gnm::Buffer *buffer, Usage oldUsage, Usage newUsage)
{
	SCE_GNM_ASSERT_MSG(buffer != NULL, "buffer must not be NULL.");
	SCE_GNM_ASSERT_MSG(oldUsage < kNumUsages, "Invalid enum value (%d) for oldUsage", oldUsage);
	SCE_GNM_ASSERT_MSG(newUsage < kNumUsages, "Invalid enum value (%d) for newUsage", newUsage);

	m_oldUsage = oldUsage;
	m_newUsage = newUsage;

	m_flags = 0;
	m_resourceBaseAddr256    = (uint32_t)( (uintptr_t)(buffer->getBaseAddress()) / 256 );
	m_resourceSize256        = buffer->getSize();

    m_type = Type::kBuffer;
    m_buffer = *buffer;
}

void Gnmx::ResourceBarrier::enableDestinationCacheFlushAndInvalidate(bool enable)
{
	m_flags &= ~kFlagFlushDestCache;
	m_flags |= (enable ? kFlagFlushDestCache : 0);
}

void Gnmx::ResourceBarrier::write(GnmxDrawCommandBuffer *dcb) const
{    
	Gnm::CacheAction cacheAction = Gnm::kCacheActionNone;
	int32_t extendedCacheAction = 0;
	if (m_flags & kFlagFlushDestCache)
	{
		// Based on the new resource usage, make sure the appropriate cache is flushed and invalidated as well
		// (so the new usage doesn't see stale data in its cache).
		switch(m_newUsage)
		{
		case kUsageRenderTarget:
			extendedCacheAction |= Gnm::kExtendedCacheActionFlushAndInvalidateCbCache;
			break;
		case kUsageRoTexture:
		case kUsageRoBuffer:
			cacheAction = Gnm::kCacheActionWriteBackAndInvalidateL1andL2;
			break;
		case kUsageRwTexture:
		case kUsageRwBuffer:
			cacheAction = Gnm::kCacheActionWriteBackAndInvalidateL1andL2;
			break;
		case kUsageDepthSurface:
		case kUsageStencilSurface:
			extendedCacheAction |= Gnm::kExtendedCacheActionFlushAndInvalidateDbCache;
			break;
		default:
			SCE_GNM_ERROR("newUsage (%d) has invalid ResourceUsage enum value", m_newUsage);
			break;
		}
	}

	switch(m_oldUsage)
	{
	case kUsageRenderTarget:
		cacheAction          = (Gnm::CacheAction)( (uint32_t)cacheAction | Gnm::kCacheActionNone);
		extendedCacheAction |= Gnm::kExtendedCacheActionFlushAndInvalidateCbCache;
        if((kRenderTarget == m_type) && m_renderTarget.getDccCompressionEnable())
        {
			volatile uint32_t* label = (volatile uint32_t*)dcb->allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
			uint32_t zero = 0;
			dcb->writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
			dcb->writeAtEndOfPipe(Gnm::kEopFlushAndInvalidateCbDbCaches, Gnm::kEventWriteDestMemory, const_cast<uint32_t*>(label), Gnm::kEventWriteSource32BitsImmediate, 0x1, cacheAction, Gnm::kCachePolicyLru);
			dcb->waitOnAddress(const_cast<uint32_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
    		dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbMeta);
		    dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbPixelData);
        }
        else
        {
		    // Wait for outstanding PS writes to the surface to complete.
		    dcb->waitForGraphicsWrites(m_resourceBaseAddr256, m_resourceSize256,
			    Gnm::kWaitTargetSlotCb0 | 
                Gnm::kWaitTargetSlotCb1 | 
                Gnm::kWaitTargetSlotCb2 | 
                Gnm::kWaitTargetSlotCb3 |
			    Gnm::kWaitTargetSlotCb4 | 
                Gnm::kWaitTargetSlotCb5 | 
                Gnm::kWaitTargetSlotCb6 | 
                Gnm::kWaitTargetSlotCb7,
			    cacheAction, extendedCacheAction, Gnm::kStallCommandBufferParserDisable);
		    dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbPixelData); // flush DCC metadata
        }
#ifdef __ORBIS__
        if((kRenderTarget == m_type) && (m_flags & kFlagResolveMetadata))
        {
            if(m_renderTarget.getDccCompressionEnable())
                Gnmx::decompressDccSurface(dcb, &m_renderTarget); // this always also does eliminateFastClear and decompressFmaskSurface.
            else
            {
                Gnmx::eliminateFastClear(dcb, &m_renderTarget);
                Gnmx::decompressFmaskSurface(dcb, &m_renderTarget);
            }
        }
#endif
		break;
	case kUsageDepthSurface:
	case kUsageStencilSurface:
		cacheAction          = (Gnm::CacheAction)( (uint32_t)cacheAction | Gnm::kCacheActionNone);
		extendedCacheAction |= Gnm::kExtendedCacheActionFlushAndInvalidateDbCache;
		// Wait for outstanding PS writes to the surface to complete.
		dcb->waitForGraphicsWrites(m_resourceBaseAddr256, m_resourceSize256, Gnm::kWaitTargetSlotDb, 
            cacheAction, extendedCacheAction, Gnm::kStallCommandBufferParserDisable);
        if((kDepthRenderTarget == m_type) && m_depthRenderTarget.getHtileTextureCompatible())
        {
		    dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateDbMeta);
        }
#ifdef __ORBIS__
        if((kDepthRenderTarget == m_type) && (m_flags & kFlagResolveMetadata))
            Gnmx::decompressDepthSurface(dcb, &m_depthRenderTarget);
#endif
		break;
	case kUsageRoTexture:
	case kUsageRoBuffer:
		{
			if (m_newUsage == kUsageRoBuffer || m_newUsage == kUsageRoTexture)
				return; // Read-only to read-only transition; nothing to do
			cacheAction          = (Gnm::CacheAction)( (uint32_t)cacheAction | Gnm::kCacheActionWriteBackAndInvalidateL1andL2);
			extendedCacheAction |= 0;
			volatile uint32_t* label = (volatile uint32_t*)dcb->allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
			uint32_t zero = 0;
			dcb->writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
			// NOTE: kEopCbDbReadsDone and kEopCsDone are two names for the same value, so this EOP event does cover both graphics and compute
			// use cases.
			dcb->writeAtEndOfPipe(Gnm::kEopCbDbReadsDone, Gnm::kEventWriteDestMemory, const_cast<uint32_t*>(label),
				Gnm::kEventWriteSource32BitsImmediate, 0x1, cacheAction, Gnm::kCachePolicyLru);
			dcb->waitOnAddress(const_cast<uint32_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
		}
		break;
	case kUsageRwTexture:
	case kUsageRwBuffer:
		{
			cacheAction          = (Gnm::CacheAction)( (uint32_t)cacheAction | Gnm::kCacheActionWriteBackAndInvalidateL1andL2);
			extendedCacheAction |= 0;
			volatile uint32_t* label = (volatile uint32_t*)dcb->allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
			uint32_t zero = 0;
			dcb->writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
			// NOTE: kEopCbDbReadsDone and kEopCsDone are two names for the same value, so this EOP event does cover both graphics and compute
			// use cases.
			dcb->writeAtEndOfPipe(Gnm::kEopCbDbReadsDone, Gnm::kEventWriteDestMemory, const_cast<uint32_t*>(label),
				Gnm::kEventWriteSource32BitsImmediate, 0x1, cacheAction, Gnm::kCachePolicyLru);
			dcb->waitOnAddress(const_cast<uint32_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
		}
		break;
	default:
		SCE_GNM_ERROR("oldUsage (%d) has invalid ResourceUsage enum value", m_oldUsage);
		break;
	}
}
void Gnmx::ResourceBarrier::write(GnmxDispatchCommandBuffer *dcb) const
{
	Gnm::CacheAction cacheAction = Gnm::kCacheActionNone;
	int32_t extendedCacheAction = 0;

	if (m_flags & kFlagFlushDestCache)
	{
		// Based on the new resource usage, make sure the appropriate cache is flushed and invalidated as well
		// (so the new usage doesn't see stale data in its cache).
		switch(m_newUsage)
		{
		case kUsageRenderTarget:
			extendedCacheAction |= Gnm::kExtendedCacheActionFlushAndInvalidateCbCache;
			break;
		case kUsageRoTexture:
		case kUsageRoBuffer:
			cacheAction = Gnm::kCacheActionWriteBackAndInvalidateL1andL2;
			break;
		case kUsageRwTexture:
		case kUsageRwBuffer:
			cacheAction = Gnm::kCacheActionWriteBackAndInvalidateL1andL2;
			break;
		case kUsageDepthSurface:
		case kUsageStencilSurface:
			extendedCacheAction |= Gnm::kExtendedCacheActionFlushAndInvalidateDbCache;
			break;
		default:
			SCE_GNM_ERROR("newUsage (%d) has invalid ResourceUsage enum value", m_newUsage);
			break;
		}
	}

	switch(m_oldUsage)
	{
	case kUsageRenderTarget:
		cacheAction          = (Gnm::CacheAction)( (uint32_t)cacheAction | Gnm::kCacheActionNone);
		extendedCacheAction |= Gnm::kExtendedCacheActionFlushAndInvalidateCbCache;
		// Wait for outstanding PS writes to the surface to complete.
		dcb->waitForGraphicsWrites(m_resourceBaseAddr256, m_resourceSize256,
			Gnm::kWaitTargetSlotCb0 | 
			Gnm::kWaitTargetSlotCb1 |
			Gnm::kWaitTargetSlotCb2 |
			Gnm::kWaitTargetSlotCb3 |
			Gnm::kWaitTargetSlotCb4 |
			Gnm::kWaitTargetSlotCb5 |
			Gnm::kWaitTargetSlotCb6 |
			Gnm::kWaitTargetSlotCb7,
			cacheAction, extendedCacheAction);
		dcb->triggerEvent(Gnm::kEventTypeFlushAndInvalidateCbPixelData); // flush DCC metadata
		break;
	case kUsageDepthSurface:
	case kUsageStencilSurface:
		cacheAction          = (Gnm::CacheAction)( (uint32_t)cacheAction | Gnm::kCacheActionNone);
		extendedCacheAction |= Gnm::kExtendedCacheActionFlushAndInvalidateDbCache;
		// Wait for outstanding PS writes to the surface to complete.
		dcb->waitForGraphicsWrites(m_resourceBaseAddr256,
			m_resourceSize256, Gnm::kWaitTargetSlotDb,
			cacheAction, extendedCacheAction);
		break;
	case kUsageRoTexture:
	case kUsageRoBuffer:
		{
			if (m_newUsage == kUsageRoBuffer || m_newUsage == kUsageRoTexture)
				return; // Read-only to read-only transition; nothing to do
			cacheAction          = (Gnm::CacheAction)( (uint32_t)cacheAction | Gnm::kCacheActionWriteBackAndInvalidateL1andL2);
			extendedCacheAction |= 0;
			volatile uint32_t* label = (volatile uint32_t*)dcb->allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
			uint32_t zero = 0;
			dcb->writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
			dcb->writeReleaseMemEvent(Gnm::kReleaseMemEventCsDone, Gnm::kEventWriteDestMemory, const_cast<uint32_t*>(label),
				Gnm::kEventWriteSource32BitsImmediate, 0x1, cacheAction, Gnm::kCachePolicyLru);
			dcb->waitOnAddress(const_cast<uint32_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
		}
		break;
	case kUsageRwTexture:
	case kUsageRwBuffer:
		{
			cacheAction          = (Gnm::CacheAction)( (uint32_t)cacheAction | Gnm::kCacheActionWriteBackAndInvalidateL1andL2);
			extendedCacheAction |= 0;
			volatile uint32_t* label = (volatile uint32_t*)dcb->allocateFromCommandBuffer(sizeof(uint32_t), Gnm::kEmbeddedDataAlignment8);
			uint32_t zero = 0;
			dcb->writeDataInline((void*)label, &zero, 1, Gnm::kWriteDataConfirmEnable);
			dcb->writeReleaseMemEvent(Gnm::kReleaseMemEventCsDone, Gnm::kEventWriteDestMemory, const_cast<uint32_t*>(label),
				Gnm::kEventWriteSource32BitsImmediate, 0x1, cacheAction, Gnm::kCachePolicyLru);
			dcb->waitOnAddress(const_cast<uint32_t*>(label), 0xffffffff, Gnm::kWaitCompareFuncEqual, 0x1);
		}
		break;
	default:
		SCE_GNM_ERROR("oldUsage (%d) has invalid ResourceUsage enum value", m_oldUsage);
		break;
	}
}

void Gnmx::ResourceBarrier::enableResolveMetadata(bool enable)
{
	m_flags &= ~kFlagResolveMetadata;
	m_flags |= (enable ? kFlagResolveMetadata : 0);
}
