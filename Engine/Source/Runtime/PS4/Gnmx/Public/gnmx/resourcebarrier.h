/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNMX_RESOURCEBARRIER_H
#define _SCE_GNMX_RESOURCEBARRIER_H

#include <gnmx/common.h>
#include <gnm/common.h> // must be first, to define uint32_t and friends!
#include <gnm/rendertarget.h>
#include <gnm/buffer.h>
#include <gnm/depthrendertarget.h>

namespace sce
{
	namespace Gnmx
	{
		/** @brief Simplifies the necessary GPU synchronization when using the same data as two or more different kinds of resource.
				   
				   For example, after rendering to a RenderTarget, you must flush certain caches and introduce stalls to make it
				   safe to use the RenderTarget data as a Texture in a subsequent draw call. This class wraps the Gnm class
				   to enable the expression of these transitions in terms of the desired high-level effect (such as "RenderTarget to
				   read-only Texture"), sparing you from interaction with cryptic lower-level Gnm synchronization primitives.
			*/
		class SCE_GNMX_EXPORT ResourceBarrier
		{
		public:
			/** @brief Specifies the old and new usage types for a particular resource barrier.
				*/
			typedef enum Usage
			{
				kUsageRenderTarget          =  0, ///< Resource used as the color surface of a Gnm RenderTarget.
				kUsageDepthSurface          =  1, ///< Resource used as the depth surface of a Gnm DepthRenderTarget.
				kUsageStencilSurface        =  2, ///< Resource used as the stencil surface of a Gnm DepthRenderTarget.
				kUsageRoTexture             =  3, ///< Resource used as a read-only Gnm Texture.
				kUsageRoBuffer              =  4, ///< Resource used as a read-only Gnm Buffer.
				kUsageRwTexture             =  5, ///< Resource used as a read/write Gnm Texture.
				kUsageRwBuffer              =  6, ///< Resource used as a read/write Gnm Buffer.
				kNumUsages,                        ///< Enum value count
			} Usage;

			/** @brief Uses an existing RenderTarget object to initialize a ResourceBarrier.
				@param target The RenderTarget that this function uses to determine the address and size of the memory to share between multiple resources.
				@param oldUsage Specifies the usage type of the resource before this barrier is processed.
				@param newUsage Specifies the usage type of the resource after this barrier is processed.
				*/
			void init(const Gnm::RenderTarget *target, Usage oldUsage, Usage newUsage);

			/** @brief Uses an existing DepthRenderTarget object to initialize a ResourceBarrier.
				@param depthTarget The DepthRenderTarget that this function uses to determine the address and size of the memory to share between multiple resources.
				@param oldUsage Specifies the usage type of the resource before this barrier is processed.
				@param newUsage Specifies the usage type of the resource after this barrier is processed.
				*/
			void init(const Gnm::DepthRenderTarget *depthTarget, Usage oldUsage, Usage newUsage);

			/** @brief Uses an existing Texture object to initialize a ResourceBarrier.
				@param texture The Texture that this function uses to determine the address and size of the memory to share between multiple resources.
				@param oldUsage Specifies the usage type of the resource before this barrier is processed.
				@param newUsage Specifies the usage type of the resource after this barrier is processed.
				*/
			void init(const Gnm::Texture *texture, Usage oldUsage, Usage newUsage);

			/** @brief Uses an existing Buffer object to initialize a ResourceBarrier.
				@param buffer The Buffer that this function uses to determine the address and size of the memory to share between multiple resources.
				@param oldUsage Specifies the usage type of the resource before this barrier is processed.
				@param newUsage Specifies the usage type of the resource after this barrier is processed.
				*/
			void init(const Gnm::Buffer *buffer, Usage oldUsage, Usage newUsage);

			/** @brief Specifies whether the appropriate caches for the "new" resource usage should be flushed and invalidated when the barrier
			           is written. 
					   
					   This feature is disabled by default. You may need to enable this setting if a previous resource transition did not flush the appropriate caches. For
					   example, consider a resource that was used as a read-only Texture and then written to as a RenderTarget. It is assumed that this was done without
					   using appropriate synchronization first -- specifically, without invalidating the GPU's texture cache. When using a ResourceBarrier
					   to convert the RenderTarget back to a Texture, the  CB cache of the GPU will always be flushed and invalidated. In this case, however,
					   the stale data in the "destination" cache (the texture cache) must also be invalidated.
				@param enable Pass <c>true</c> to enable destination cache flush/invalidate.
				@note The "invalidate" operation here is usually implemented as a "flush and invalidate". This would not be correct if the "destination"
				      cache contains dirty data that hasn't been written back to memory yet. The only way to correct this scenario would be to perform
					  the appropriate synchronization in the first place.
				@note A ResourceBarrier will always flush and invalidate the caches for the "old" resource usage; this function controls only whether
				      the "new" usage caches will be invalidated, as well.
				*/
			void enableDestinationCacheFlushAndInvalidate(bool enable);

			/** @brief Specifies whether buffers should be resolved for later use by the texture unit. This feature is disabled by default.

					   Buffers sometimes exist in a 'compressed' state which saves bandwidth during rasterization, but which makes the buffer unusable by the texture unit.
                       The act of resolving a buffer prepares it for subsequent use by the texture unit. This is an expensive operation, however, and should be avoided unless necessary.
				@param enable A flag that specifies whether to enable the resolving of metadata.
				*/
			void enableResolveMetadata(bool enable);
			
			/** @brief Writes a ResourceBarrier to a command buffer. 
			
				This function emits the low-level Gnm commands necessary to perform the requested resource transition safely.
				@param dcb The DrawCommandBuffer to which this function writes synchronization commands.
				*/
			void write(GnmxDrawCommandBuffer *dcb) const;

			/** @brief Writes a ResourceBarrier to a command buffer. 
			
				This function emits the low-level Gnm commands necessary to perform the requested resource transition safely.
				@param dcb The DispatchCommandBuffer to which this function writes synchronization commands.
				@note You must avoid a potentially serious synchronization issue that can occur when using ResourceBarriers in a DispatchCommandBuffer to wait for a surface being
				      modified by the graphics pipe. For more information, see DispatchCommandBuffer::waitForGraphicsWrites().
				*/
			void write(GnmxDispatchCommandBuffer *dcb) const;

		protected:
#ifndef DOXYGEN_IGNORE
			Usage m_oldUsage, m_newUsage;

			uint32_t m_resourceBaseAddr256;
			uint32_t m_resourceSize256;
			Gnm::CacheAction m_cacheAction;
			int32_t m_extendedCacheAction;

			enum
			{
				kFlagFlushDestCache = (1<<0),
                kFlagResolveMetadata = (1<<1),
			};
			uint32_t m_flags;

            union 
            {
                Gnm::Buffer            m_buffer;
                Gnm::Texture           m_texture;
                Gnm::RenderTarget      m_renderTarget;
                Gnm::DepthRenderTarget m_depthRenderTarget;
            };
            enum Type
            {
                kBuffer,            
                kTexture,
                kRenderTarget,
                kDepthRenderTarget
            };
            Type m_type;
#endif
		};
	}
}

#endif
