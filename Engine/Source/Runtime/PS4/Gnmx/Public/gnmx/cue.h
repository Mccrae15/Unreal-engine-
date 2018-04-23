/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#if !defined(_SCE_GNMX_CUE_H)
#define _SCE_GNMX_CUE_H

#include <gnmx/common.h>

#include <gnm/common.h> // must be first, to define uint32_t and friends!
#include <gnm/constants.h>
#include <gnm/tessellation.h>
#include <gnm/streamout.h>
#include <gnmx/shaderbinary.h>


//

// If nonzero, the global table is handled like any other resource and can be changed without stopping the GPU 
// but it will break some old code that relies on swapping pointers to the global table.
#ifndef SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE 
#define SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE 0
#endif //!SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE 

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY && !SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#error "Forced to use User Resource Tables only without enabling user resource tables"
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY && !SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES

#if SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES
#warning "SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES does not do anything when only User Resource Tables are enabled"
#endif //SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES
#endif //SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY

// 


namespace sce
{
	// Forward Declarations
	namespace Gnm
	{
		class Buffer;
		class Sampler;
		class Texture;
		class ConstantCommandBuffer;
		class DrawCommandBuffer;
		class DispatchCommandBuffer;
		struct InputUsageData;
	}

    /** @brief Higher-level layer on top of the %Gnm library.
		In the %Gnmx library, higher level functions for driving the GPU are defined using the Gnm interface. Many of
		the Gnmx API calls are transferred directly to corresponding low-level Gnm calls. However, methods for
		generating command buffers and managing shader resources, etc. are implemented so that they can be called
		from a GfxContext class instance, which contains state variables. There are differences between the Gnmx layer
		and the Gnm layer in that, for example, an instance of a class in one Gnmx namespace is not guaranteed to be
		usable from another Gnmx namespace, and a method in one Gnmx instance cannot, in general, be called
		from another Gnmx namespace.

		The two main entry points to Gnmx functions are the GfxContext class and the ComputeContext class. To realize
		a comprehensible rendering API suitable for users who already have knowledge of OpenGL, Direct3D or PlayStation®3,
		the GfxContext class uses a command buffer pair together with a ConstantUpdateEngine instance, which manages shader
		resources across multiple draw calls. However, one should bear in mind that these implementations are offered
		primarily as a usable reference and that, as a whole, the %Gnmx API layer is still similar to the Gnm interface in
		the sense that results are not necessarily guaranteed. Problems might occur if API calls are re-ordered, and care
		must still be taken with: hardware resources such as caches, memory access ordering when programming.
		
		The %Gnmx library should be used as the starting point for initial development on PlayStation®4. Note that the
		Gnmx source code has been written to provide comprehensible working examples of how to call sequences of Gnm
		functions and is not necessarily optimized. In many cases, it will be necessary to modify the source code of
		the %Gnmx library for optimization purposes. An optimized library may be provided by SIE in the future together
		with source code.
     */
	namespace Gnmx
	{
		class CsShader;
		class EsShader;
		class GsShader;
		class HsShader;
		class LsShader;
		class PsShader;
		class VsShader;
		class CsVsShader;

		//

		class SCE_GNMX_EXPORT ConstantUpdateEngine
		{
		public:

			/** @brief Configures a ring buffer for the ConstantUpdateEngine. */
			struct RingSetup
			{
				uint8_t numResourceSlots;     ///< Maximum number of resource slots      (must be a multiple of kResourceChunkSize and must not exceed Gnm::kSlotCountResource)
				uint8_t numRwResourceSlots;   ///< Maximum number of RW resource slots   (must be a multiple of kResourceChunkSize and must not exceed Gnm::kSlotCountRwResource)
				uint8_t numSampleSlots;       ///< Maximum number of smapler slots       (must be a multiple of kSamplerChunkSize and must not exceed Gnm::kSlotCountSampler)
				uint8_t numVertexBufferSlots; ///< Maximum number of vertex buffer slots (must be a multiple of kVertexBufferChunkSize and must not exceed Gnm::kSlotCountVertexBuffer)
			};

			/** @brief Computes the required heap size for a ConstantUpdateEngine instance. Use this value to compute the size of the heap passed to ConstantUpdateEngine::init().
				       This variant uses the default counts for each resource type.
				@param[in] numRingEntries The number of entries in each ring buffer. At least 64 is recommended.
				@return The size of the ConstantUpdateEngine heap buffer in bytes.
				*/
			static uint32_t computeHeapSize(uint32_t numRingEntries);
			/** @brief Computes the required heap size for a ConstantUpdateEngine instance. Use this value to compute the size of the heap passed to ConstantUpdateEngine::init().
				@param[in] numRingEntries Specifies the number of entries in each ring buffer. At least 64 is recommended.
				@param[in] setup Specifies the per-ring-buffer counts for each resource type.
				@return The size of the ConstantUpdateEngine heap buffer in bytes.
				*/
			static uint32_t computeHeapSize(uint32_t numRingEntries, RingSetup setup);

		public:

			/** @brief Default constructor */
			ConstantUpdateEngine(void);
			/** @brief Default destructor */
			~ConstantUpdateEngine(void);

			/** @brief Associates a DrawCommandBuffer/ConstantCommandBuffer pair with this CUE. Any CUE functions
			 *         that need to write GPU commands will use these buffers.
			 *
			 *  @param dcb		The DrawCommandBuffer where draw commands will be written.
			 *  @param ccb		The ConstantCommandBuffer where shader resource definition management commands will be written.
			 *  @param acb		The DispatchCommandBuffer for a dispatch draw asynchronous compute. Specify a value of <c>NULL</c> if this is not required.
			 */
			void bindCommandBuffers(GnmxDrawCommandBuffer *dcb, GnmxConstantCommandBuffer *ccb, GnmxDispatchCommandBuffer *acb)
			{
				m_dcb = dcb;
				m_ccb = ccb;
				m_acb = acb;
			}

			/** @brief Initializes the Constant Update Engine with a memory heap used to allocate the different ring buffers.

				@param heapAddr		A pointer to the heap memory to use for the resource ring buffers
				@param numRingEntries	The number of entries in each ring buffer. At least 64 is recommended.
				
				@see computeHeapSize()
			 */
			void init(void *heapAddr, uint32_t numRingEntries);

			/** @brief Initialize the Constant Update Engine with a memory heap used to allocate the different ring buffers.

				@param heapAddr			A pointer to the heap memory to use for the resource ring buffers
				@param numRingEntries	The size of each ring buffers. At least 64 is recommended.
				@param ringSetup		The ring setup configuration.
				
				@see computeHeapSize()
			 */
			void init(void *heapAddr, uint32_t numRingEntries, RingSetup ringSetup);

			/**
			 * @brief Sets the address of the system's global resource table - a collection of <c>V#</c>s which point to global buffers for various shader tasks.
			 *
			 * @param addr The GPU-visible address of the global resource table. The minimum size of this buffer is given by
			 *             Gnm::SCE_GNM_SHADER_GLOBAL_TABLE_SIZE, and its minimum alignment is Gnm::kMinimumBufferAlignmentInBytes. This buffer is accessed by both the CPU and GPU.
			 */

			void setGlobalResourceTableAddr(void *addr);

			/**
			 * @brief Sets an entry in the global resource table.
			 *
			 * @note This function modifies the global resource table. It is not safe to modify the global resource table unless the GPU is idle.
			 *
			 * @param resType		The global resource type to bind a buffer for. Each global resource type has its own entry in the global resource table.
			 * @param res			The buffer to bind to the specified entry in the global resource table. The size of the buffer is global-resource-type-specific.
			 */
			void setGlobalDescriptor(Gnm::ShaderGlobalResourceType resType, const Gnm::Buffer *res);

			/**
			 * @brief Sets the active shader stages in the graphics pipeline.
			 *
			 * Note that the compute-only CS stage is always active.
			 *
			 * This function will roll the hardware context.
			 *
			 * @param activeStages A flag that indicates which shader stages should be activated.
			 */
			void setActiveShaderStages(Gnm::ActiveShaderStages activeStages)
			{
				m_activeShaderStages = activeStages;
			}

			/** @brief Gets the last value set using setActiveShaderStages().
			 *
			 * @return The shader stages that will be considered activated by preDraw() calls.
			 */
			Gnm::ActiveShaderStages getActiveShaderStages(void) const
			{
				return m_activeShaderStages;
			}

			/** @brief Enables/Disables the prefetch of the shader code.

				@param[in] enable		A flag that specifies whether to enable the prefetch of the shader code.
				
				@note Shader code prefetching is enabled by default.
				@note The implementation of shader prefetching uses the prefetchIntoL2() function that Gnm provides. See the notes for DrawCommandBuffer::prefetchIntoL2() regarding a potential
				      stall of the command processor when multiple DMAs are in flight.
				@see  DrawCommandBuffer::prefetchIntoL2()
			 */
			void setShaderCodePrefetchEnable(bool enable)
			{
				m_prefetchShaderCode = enable;
			}


			///////////////////////
			// Functions to bind shader resources
			///////////////////////

			/**
			 * @brief Binds one or more read-only texture objects to the specified shader stage.
			 *
			 * @param stage			The resource(s) to bind to this shader stage.
			 * @param startApiSlot	The first API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountResource-1]</c>.
			 * @param numApiSlots	The number of consecutive API slots to bind.
			 * @param textures		The texture objects to bind to the specified slots. <c>textures[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>textures[1]</c> to <c>startApiSlot+1</c> and so on.
			 *						The contents of these texture objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *						be unbound.
			 *
			 * @note Buffers and textures share the same pool of API slots.
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setTextures(Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const Gnm::Texture *textures);

			/**
			 * @brief Binds one read-only texture objects to the specified shader stage.
			 *
			 * @param stage		The resource will be bound to this shader stage.
			 * @param apiSlot	The API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountResource-1]</c>.
			 * @param texture	The texture object to bind to the specified slots. <c>textures[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>textures[1]</c> to <c>startApiSlot+1</c> and so on.
			 *					The contents of these texture objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *					be unbound.
			 *
			 * @note Buffers and textures share the same pool of API slots.
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setTexture(Gnm::ShaderStage stage, uint32_t apiSlot, const Gnm::Texture *texture);

			/**
			 * @brief Binds one or more read-only buffer objects to the specified shader stage.
			 *
			 * @param stage			The resource(s) will be bound to this shader stage.
			 * @param startApiSlot	The first API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountResource-1]</c>.
			 * @param numApiSlots	The number of consecutive API slots to bind.
			 * @param buffers		The buffer objects to bind to the specified slots. <c>buffers[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>buffers[1]</c> to <c>startApiSlot+1</c> and so on.
			 *						The contents of these buffer objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *						be unbound.
			 *
			 * @note Buffers and textures share the same pool of API slots.
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setBuffers(Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const Gnm::Buffer *buffers);

			/**
			 * @brief Binds one read-only buffer object to the specified shader stage.
			 *
			 * @param stage		The resource will be bound to this shader stage.
			 * @param apiSlot	The API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountResource-1]</c>.
			 * @param buffer	The buffer object to bind to the specified slots. <c>buffers[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>buffers[1]</c> to <c>startApiSlot+1</c> and so on.
			 *					The contents of these buffer objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *					be unbound.
			 *
			 * @note Buffers and textures share the same pool of API slots.
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setBuffer(Gnm::ShaderStage stage, uint32_t apiSlot, const Gnm::Buffer *buffer);

			/**
			 * @brief Binds one or more read/write texture objects to the specified shader stage.
			 * @param stage			The resource(s) will be bound to this shader stage.
			 * @param startApiSlot	The first API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountRwResource-1]</c>.
			 * @param numApiSlots	The number of consecutive API slots to bind.
			 * @param rwTextures	The texture objects to bind to the specified slots. <c>rwTextures[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>rwTextures[1]</c> to <c>startApiSlot+1</c> and so on.
			 *						The contents of these texture objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *						be unbound.
			 *
			 * @note Read/write buffers and read/write textures share the same pool of API slots.
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setRwTextures(Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const Gnm::Texture *rwTextures);

			/**
			 * @brief Binds one read/write texture objects to the specified shader stage.
			 *
			 * @param stage			The resource to bind to this shader stage.
			 * @param apiSlot		The API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountRwResource-1]</c>.
			 * @param rwTexture		The texture object to bind to the specified slots. <c>rwTextures[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>rwTextures[1]</c> to <c>startApiSlot+1</c> and so on.
			 *						The contents of these texture objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *						be unbound.
			 *
			 * @note Read/write buffers and read/write textures share the same pool of API slots.
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setRwTexture(Gnm::ShaderStage stage, uint32_t apiSlot, const Gnm::Texture *rwTexture);

			/**
			 * @brief Binds one or more read/write buffer objects to the specified shader stage.
			 *
			 * @param stage			The resource(s) will be bound to this shader stage.
			 * @param startApiSlot	The first API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountRwResource-1]</c>.
			 * @param numApiSlots	The number of consecutive API slots to bind.
			 * @param rwBuffers		The buffer objects to bind to the specified slots. <c>rwBuffers[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>rwBuffers[1]</c> to <c>startApiSlot+1</c> and so on.
			 *						The contents of these buffer objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *						be unbound.
			 *
			 * @note Read/write buffers and read/write textures share the same pool of API slots.
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setRwBuffers(Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const Gnm::Buffer *rwBuffers);

			/**
			 * @brief Binds one read/write buffer objects to the specified shader stage.
			 *
			 * @param stage		The resource will be bound to this shader stage.
			 * @param apiSlot	The API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountRwResource-1]</c>.
			 * @param rwBuffer	The buffer object to bind to the specified slots. <c>rwBuffers[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>rwBuffers[1]</c> to <c>startApiSlot+1</c> and so on.
			 *                  The contents of these buffer objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *                  be unbound.
			 *
			 * @note Read/write buffers and read/write textures share the same pool of API slots.
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setRwBuffer(Gnm::ShaderStage stage, uint32_t apiSlot, const Gnm::Buffer *rwBuffer);

			/**
			 * @brief Binds one or more sampler objects to the specified shader stage.
			 *
			 * @param stage			The resource(s) will be bound to this shader stage.
			 * @param startApiSlot	The first API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountSampler-1]</c>.
			 * @param numApiSlots	The number of consecutive API slots to bind.
			 * @param samplers		The sampler objects to bind to the specified slots. <c>samplers[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>samplers[1]</c> to <c>startApiSlot+1</c> and so on.
			 *						The contents of these sampler objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *						be unbound.
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setSamplers(Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const Gnm::Sampler *samplers);

			/**
			 * @brief Binds one sampler objects to the specified shader stage.
			 *
			 * @param stage			The resource will be bound to this shader stage.
			 * @param apiSlot		The API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountSampler-1]</c>.
			 * @param sampler		The sampler object to bind to the specified slots. <c>samplers[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>samplers[1]</c> to <c>startApiSlot+1</c> and so on.
			 *						The contents of these Sampler objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *						be unbound.
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setSampler(Gnm::ShaderStage stage, uint32_t apiSlot, const Gnm::Sampler *sampler);

			/**
			 * @brief Binds one or more constant buffer objects to the specified shader stage.
			 *
			 * @param stage			The resource(s) will be bound to this shader stage.
			 * @param startApiSlot	The first API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountConstantBuffer-1]</c>.
			 * @param numApiSlots	The number of consecutive API slots to bind.
			 * @param buffers		The constant buffer objects to bind to the specified slots. <c>buffers[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>buffers[1]</c> to <c>startApiSlot+1</c> and so on.
			 *						The contents of these buffer objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *						be unbound.
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setConstantBuffers(Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const Gnm::Buffer *buffers);

			/**
			 * @brief Binds one constant buffer objects to the specified shader stage.
			 *
			 * @param stage		The resource will be bound to this shader stage.
			 * @param apiSlot	API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountConstantBuffer-1]</c>.
			 * @param buffer	The constant buffer object to bind to the specified slots. <c>buffers[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>buffers[1]</c> to <c>startApiSlot+1</c> and so on.
			 *					The contents of these buffer objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *					be unbound.
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setConstantBuffer(Gnm::ShaderStage stage, uint32_t apiSlot, const Gnm::Buffer *buffer);

			/**
			 * @brief Binds one or more vertex buffer objects to the specified shader stage.
			 *
			 * @param stage			The resource(s) will be bound to this shader stage.
			 * @param startApiSlot	The first API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountVertexBuffer-1]</c>.
			 * @param numApiSlots	The number of consecutive API slots to bind.
			 * @param buffers		The vertex buffer objects to bind to the specified slots. <c>buffers[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>buffers[1]</c> to <c>startApiSlot+1</c> and so on.
			 *						The contents of these buffer objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *						be unbound.
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setVertexBuffers(Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const Gnm::Buffer *buffers);

			/**
			 * @brief Binds one or more Boolean constants to the specified shader stage.
			 *
			 * @param stage			The resource(s) will be bound to this shader stage.
			 * @param maskAnd       A mask that will be bitwise-ANDed with the previous value. The new value will be <c>"(<i>oldValue</i> & <i>maskAnd</i>) | <i>maskOr</i>"</c>
			 * @param maskOr        A mask that will be bitwise-ORed with the previous value. The new value will be <c>"(<i>oldValue</i> & <i>maskAnd</i>) | <i>maskOr</i>"</c>
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setBoolConstants(Gnm::ShaderStage stage, uint32_t maskAnd, uint32_t maskOr);

			/**
			 * @brief Binds one or more floating-point constants to the specified shader stage.
			 *
			 * @param stage			The resource(s) will be bound to this shader stage.
			 * @param startApiSlot	The first API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountResource-1]</c>.
			 * @param numApiSlots	The number of consecutive API slots to bind.
			 *
			 * @param floats		The constants to bind to the specified slots. <c>floats[0]</c> will be bound to <c><i>startApiSlot</i></c>, <c>floats[1]</c> to <c>startApiSlot+1</c> and so on.
			 *						The contents of these texture objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *						be unbound.
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called
			 */
			void setFloatConstants(Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const float *floats);

			/**
			 * @brief Specifies a range of the Global Data Store to be used by shaders for atomic global counters such as those
			 *        used to implement PSSL <c>AppendRegularBuffer</c> and <c>ConsumeRegularBuffer</c> objects.
			 *
			 *  Each counter is a 32-bit integer. The counters for each shader stage may have a different offset in GDS. For example:
			 *  @code
			 *     setAppendConsumeCounterRange(kShaderStageVs, 0x0100, 12) // Set up 3 counters for the VS stage starting at offset 0x100.
			 *     setAppendConsumeCounterRange(kShaderStageCs, 0x0400, 4)  // Set up 1 counter for the CS stage at offset 0x400.
			 *	@endcode
			 *
			 *  The index is defined by the chosen slot in the PSSL shader. For example:
			 *  @code
			 *     AppendRegularBuffer<uint> appendBuf : register(u3) // Will access the 4th counter starting at the base offset provided to this function.
			 *  @endcode
			 *
			 *  This function never rolls the hardware context.
			 *
			 * @param stage						The shader stage to bind this counter range to.
			 * @param rangeGdsOffsetInBytes		The byte offset to the start of the counters in GDS. This must be a multiple of 4.
			 * @param rangeSizeInBytes			The size of the counter range in bytes. This must be a multiple of 4.
			 *
			 * @note  The GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes.
			 */
			void setAppendConsumeCounterRange(Gnm::ShaderStage stage, uint32_t rangeGdsOffsetInBytes, uint32_t rangeSizeInBytes);

			/**
			 * @brief Specifies a range of the Global Data Store to be used by shaders.
			 *
			 *  This function never rolls the hardware context.
			 *
			 * @param[in] stage						The shader stage to bind this range to.
			 * @param[in] rangeGdsOffsetInBytes		The byte offset to the start of the range in GDS. This must be a multiple of 4.
			 * @param[in] rangeSizeInBytes			The size of the counter range in bytes. This must be a multiple of 4.
			 *
			 * @note  GDS accessible size is provided by sce::Gnm::kGdsAccessibleMemorySizeInBytes. It is an error to specify a range outside these bounds.
			 */
			void setGdsMemoryRange(Gnm::ShaderStage stage, uint32_t rangeGdsOffsetInBytes, uint32_t rangeSizeInBytes);

            /**
			 * @brief Binds one or more streamout buffer objects to the specified shader stage.
			 *
			 * @param startApiSlot		The first API slot to bind to. Valid slots are <c>[0..Gnm::kSlotCountStreamoutBuffer-1]</c>.
			 * @param numApiSlots		The number of consecutive API slots to bind.
			 *
			 * @param buffers			The streamout buffer objects to bind to the specified slots. <c>buffers[0]</c> will be bound to <c><i>startSlot</i></c>, <c>buffers[1]</c> to <c>startSlot+1</c> and so on.
			 *							The contents of these buffer objects are cached locally inside the Constant Update Engine. If this parameter is <c>NULL</c>, the specified slots will
			 *							be unbound.
			 */
			void setStreamoutBuffers(uint32_t startApiSlot, uint32_t numApiSlots, const Gnm::Buffer *buffers);

			//////////////////////
			// Functions to bind shaders
			//////////////////////
			/**
			 * @brief An opaque structure that caches the shader input table.
			 *
			 * InputParameterCache is a run-time only structure. You should not save it in a file as it is not guaranteed to work if any changes had been made to the 
			 * ConstantUpdateEngine. 
			 * When given to a <c>set*Shader()</c> function it accelerates the CUE functions.
			 *
			 * @see initializeInputsCache()
			 */
			struct InputParameterCache; 

			/**
			 * @brief Initializes the shader's inputs cache.
			 *
			 * @param cache					The cache to be initialized.
			 * @param inputUsageTable		A pointer to the shader inputs table.
			 * @param inputUsageTableSize	The number of entries in the shader inputs table.
			 */
			static void initializeInputsCache(InputParameterCache *cache, const Gnm::InputUsageSlot *inputUsageTable, uint32_t inputUsageTableSize);

			/**
			 * @brief Binds a shader to the VS stage.
			 *
			 * @param vsb				A pointer to the shader to bind to the VS stage.
			 * @param shaderModifier	The shader modifier value generated by generateVsFetchShaderBuildState(). Specify a value of 0 if no fetch shader is required.
			 * @param fetchShaderAddr	The GPU address of a fetch shader if the shader requires it; otherwise specify a value of 0.
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*vsb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setVsShader(const Gnmx::VsShader *vsb, uint32_t shaderModifier, void *fetchShaderAddr)
			{
#if SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = m_stageInputCache+Gnm::kShaderStageVs;
				if ( vsb && m_currentVSB != vsb )
					initializeInputsCache(cache, vsb->getInputUsageSlotTable(), vsb->m_common.m_numInputUsageSlots);
#else //SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = nullptr;
#endif //SCE_GNM_CUE2_ENABLE_CACHE
				setVsShader(vsb, shaderModifier, fetchShaderAddr,cache);
			}

			/**
			 * @brief Binds a shader to the VS stage.
			 *
			 * @param vsb				A pointer to the shader to bind to the VS stage.
			 * @param shaderModifier	The shader modifier value generated by generateVsFetchShaderBuildState(). Specify a value of 0 if no fetch shader is required.
			 * @param fetchShaderAddr	The GPU address of a fetch shader if the shader requires it; otherwise specify a value of 0.
			 * @param cachedInputs		The shader inputs cache.
			 *
			 * @see initializeInputsCache()
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*vsb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setVsShader(const Gnmx::VsShader *vsb, uint32_t shaderModifier, void *fetchShaderAddr, const InputParameterCache *cachedInputs);


			/**
			 * @brief Binds a shader pair to the asynchronous compute and VS stages for use in a dispatchDraw() call.
			 *
			 * @param csvsb					A pointer to the shaders to bind to the asynchronous compute and VS stage.
			 * @param shaderModifierVs		The associated shader modifier value if the VS shader requires a fetch shader; otherwise specify a value of 0.
			 * @param fetchShaderAddrVs		The GPU address of the fetch shader if the VS shader requires a fetch shader; otherwise specify a value of 0.
			 * @param shaderModifierCs		The associated shader modifier value if the CS shader requires a fetch shader; otherwise specify a value of 0.
			 * @param fetchShaderAddrCs		The GPU address of the fetch shader if the CS shader requires a fetch shader; otherwise specify a value of 0.
			 *
			 * @note  Setting the asynchronous compute shader does not affect the currently set graphics CS stage shader, which is set by setCsShader().
			 *		  Asynchronous compute shaders share the user data constants and CUE ring elements set up for Gnm::kShaderStageVs,
			 *		  Graphics CS stage shaders use the user data constants and CUE ring elements set up for Gnm::kShaderStageCs.
			 * @note  Only the pointers <c>csvsb->getVertexShader()</c> and <c>csvsb->getComputeShader()</c> are cached inside the Gnm::ConstantUpdateEngine; the location
			 *		  and contents of these pointers should not be changed before all calls to dispatchDraw() which use this shader have been made!
			 * @note This binding will not take effect on the GPU until preDispatchDraw() or preDraw() is called.
			 */
			void setCsVsShaders(const Gnmx::CsVsShader *csvsb, uint32_t shaderModifierVs, void *fetchShaderAddrVs, uint32_t shaderModifierCs, void *fetchShaderAddrCs);

			/**
			 * @brief Binds a shader to the asynchronous compute stage, for use in a dispatchDraw() call.
			 * Generally, compute shaders for dispatch draw are packed into a CsVsShader and so will instead be set by calling setCsVsShaders().
			 * This function never rolls the hardware context.
			 *
			 * @param csb					A pointer to the shader to bind to the asynchronous compute stage.
			 * @param shaderModifierCs		The associated shader modifier value if the CS shader requires a fetch shader; otherwise specify a value of 0.
			 * @param fetchShaderAddrCs		The GPU address of the fetch shader if the CS shader requires a fetch shader; otherwise specify a value of 0.
			 *
			 * @note  Setting the asynchronous compute shader does not affect the currently set graphics CS stage shader, which is set by setCsShader().
			 *		  Asynchronous compute shaders share the user data constants and CUE ring elements set up for Gnm::kShaderStageVs,
			 *		  while graphics CS stage shaders use the user data constants and CUE ring elements set up for Gnm::kShaderStageCs.
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*csb</c> must not change until
			 *        a different shader has been bound to this stage!
			 * @note This binding will not take effect on the GPU until preDispatchDraw() is called.
			 */
			void setAsynchronousComputeShader(const Gnmx::CsShader *csb, uint32_t shaderModifierCs, void *fetchShaderAddrCs);

			/**
			 * @brief Binds a shader to the PS stage.
			 *
			 * @param psb			A pointer to the shader to bind to the PS stage.
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*psb</c> must not change until
			 *        a different shader has been bound to this stage!
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 * @note Passing <c>NULL</c>to this function does not disable pixel shading by itself. The psRegs pointer may only safely be <c>NULL</c> if pixel shader invocation is
			 *       impossible, as is the case when:
			 *  	  - color buffer writes have been disabled with <c>setRenderTargetMask(0)</c>.
			 *  	  - a CbMode other than <c>kCbModeNormal</c> is passed to <c>setCbControl()</c>, such as during a hardware MSAA resolve or fast-clear elimination pass.
			 *  	  - the depth test is enabled and <c>kCompareFuncNever</c> is passed to <c>DepthStencilControl::setDepthControl()</c>.
			 *  	  - the stencil test is enabled and <c>kCompareFuncNever</c> is passed to <c>DepthStencilControl::setStencilFunction()</c> and/or
			 *  	  	<c>DepthStencilControl::setStencilFunctionBack()</c>.
			 *  	  - both front and back face culling are enabled with <c>PrimitiveSetup::setCullFace(kPrimitiveSetupCullFaceFrontAndBack)</c>.
			 *  	  - etc.
			 */
			void setPsShader(const Gnmx::PsShader *psb)
			{
#if SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = m_stageInputCache+Gnm::kShaderStagePs;
				if ( psb && m_currentPSB != psb )
					initializeInputsCache(cache, psb->getInputUsageSlotTable(), psb->m_common.m_numInputUsageSlots);
#else //SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = nullptr;
#endif //SCE_GNM_CUE2_ENABLE_CACHE
				setPsShader(psb, cache);
			}

			/**
			 * @brief Binds a shader to the PS stage.
			 *
			 * @param psb	A pointer to the shader to bind to the PS stage. This pointer must not be <c>NULL</c> if color buffer writes have been enabled by passing a non-zero mask
							to GfxContext::setRenderTargetMask().
			 * @param cache The shader inputs cache.
			 *
			 * @see initializeInputsCache()
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*psb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setPsShader(const Gnmx::PsShader *psb, const InputParameterCache *cache);

			/**
			 * @brief Binds a shader to the CS stage.
			 *
			 * @param csb		A pointer to the shader to bind to the CS stage.
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*csb</c> must not change until
			 *        a different shader has been bound to this stage!
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setCsShader(const Gnmx::CsShader *csb)
			{
#if SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = m_stageInputCache+Gnm::kShaderStageCs;
				if ( csb && m_currentCSB != csb )
					initializeInputsCache(cache, csb->getInputUsageSlotTable(), csb->m_common.m_numInputUsageSlots);
#else //SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = nullptr;
#endif //SCE_GNM_CUE2_ENABLE_CACHE
				setCsShader(csb, cache);
			}


			/**
			 * @brief Binds a shader to the CS stage.
			 *
			 * @param csb		A pointer to the shader to bind to the CS stage.
			 * @param cache		The shader inputs cache.
			 *
			 * @see initializeInputsCache()
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*csb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setCsShader(const Gnmx::CsShader *csb, const InputParameterCache *cache);

			/**
			 * @brief Binds a shader to the LS stage.
			 *
			 * @param lsb				A pointer to the shader to bind to the LS stage.
			 * @param shaderModifier	The shader modifier value generated by generateLsFetchShaderBuildState(). Specify a value of 0 if no fetch shader is required.
			 * @param fetchShaderAddr	The GPU address of a fetch shader if the shader requires it; otherwise specify a value of 0.
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*<i>lsb</i></c> must not change until
			 *        a different shader has been bound to this stage!
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setLsShader(const Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr)
			{
#if SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = m_stageInputCache+Gnm::kShaderStageLs;
				if ( lsb && m_currentLSB != lsb )
					initializeInputsCache(cache, lsb->getInputUsageSlotTable(), lsb->m_common.m_numInputUsageSlots);
#else //SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = nullptr;
#endif //SCE_GNM_CUE2_ENABLE_CACHE
				setLsShader(lsb, shaderModifier, fetchShaderAddr, cache);
			}


			/**
			 * @brief Binds a shader to the LS stage.
			 *
			 * @param lsb Pointer to the shader to bind to the LS stage.
			 * @param shaderModifier	The shader modifier value generated by generateLsFetchShaderBuildState(). Specify a value of 0 if no fetch shader is required.
			 * @param fetchShaderAddr	The GPU address of a fetch shader if the shader requires it; otherwise specify a value of 0.
			 * @param cache				The shader inputs cache.
			 *
			 * @see initializeInputsCache()
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*lsb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setLsShader(const Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const InputParameterCache *cache);

			/**
			 * @brief Binds a shader to the HS stage.
			 *
			 * @param hsb		A pointer to the shader to bind to the HS stage.
			 * @param tessRegs	The tessellation register settings for this HS shader. The register contents will be cached inside the Constant Update Engine.
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*hsb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setHsShader(const Gnmx::HsShader *hsb, const Gnm::TessellationRegisters *tessRegs)
			{
#if SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = m_stageInputCache+Gnm::kShaderStageHs;
				if ( hsb && m_currentHSB != hsb )
					initializeInputsCache(cache, hsb->getInputUsageSlotTable(), hsb->m_common.m_numInputUsageSlots);
#else //SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = nullptr;
#endif //SCE_GNM_CUE2_ENABLE_CACHE
				setHsShader(hsb, tessRegs, cache);
			}


			/**
			 * @brief Binds a shader to the HS stage.
			 *
			 * @param hsb			A pointer to the shader to bind to the HS stage.
			 * @param tessRegs		The tessellation register settings for this HS shader. The register contents will be cached inside the Constant Update Engine.
			 * @param cache			The shader inputs cache.
			 *
			 * @see initializeInputsCache()
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*hsb</c> must not change until
			 *        a different shader has been bound to this stage!
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setHsShader(const Gnmx::HsShader *hsb, const Gnm::TessellationRegisters *tessRegs, const InputParameterCache *cache);
			/**
			* @brief Binds a shader to the HS stage.
			*
			* @param hsb			A pointer to the shader to bind to the HS stage.
			* @param tessRegs		The tessellation register settings for this HS shader. The register contents will be cached inside the Constant Update Engine.
			* @param distributionMode The tessellation distribution mode.
			* @param cache			The shader inputs cache.
			*
			* @see initializeInputsCache()
			*
			* @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*hsb</c> must not change until
			*        a different shader has been bound to this stage!
			* @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			* @note NEO mode only.
			*/
			void setHsShader(const Gnmx::HsShader *hsb, const Gnm::TessellationRegisters *tessRegs, Gnm::TessellationDistributionMode distributionMode, const InputParameterCache *cache);

			/**
			 * @brief Binds shaders to the LS and HS stages and updates the LS shader's LDS size based on the HS shader's needs.
			 *
			 * This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants set in <c><i>hsb</i></c> or the value of <c><i>numPatches</i></c> are different from current state:

			 Gnm::HsStageRegisters
			 - <c>m_vgtTfParam</c>
			 - <c>m_vgtHosMaxTessLevel</c>
			 - <c>m_vgtHosMinTessLevel</c>

			 Gnm::HullStateConstants
			 - <c>m_numThreads</c>
			 - <c>m_numInputCP</c>

			 The check is deferred until next draw call.
			 * @param[in] lsb				A pointer to the shader to bind to the LS stage. This must not be set to <c>NULL</c> if <c><i>hsb</i></c> is non-<c>NULL</c>.
			 * @param[in] shaderModifier	The associated shader modifier value if the shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] fetchShaderAddr	The GPU address of the fetch shader if the LS shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] hsb				A pointer to the shader to bind to the HS stage. This must not be set to <c>NULL</c> if <c><i>lsb</i></c> is non-<c>NULL</c>.
			 * @param[in] numPatches		The number of patches in the HS shader.
			 *
			 * @note Only the shader pointers are cached inside the ConstantUpdateEngine; the location and contents of <c><i>lsb</i></c> and <c><i>hsb</i></c> must not change
			 *       until different shaders are bound to these stages!
			 */
			void setLsHsShaders(const Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const Gnmx::HsShader *hsb, uint32_t numPatches)
			{
#if SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *hsCache = m_stageInputCache+Gnm::kShaderStageHs;
				if ( hsb && m_currentHSB != hsb )
					initializeInputsCache(hsCache, hsb->getInputUsageSlotTable(), hsb->m_common.m_numInputUsageSlots);
				InputParameterCache *lsCache = m_stageInputCache+Gnm::kShaderStageLs;
				if ( lsb && m_currentLSB != lsb)
					initializeInputsCache(lsCache, lsb->getInputUsageSlotTable(), lsb->m_common.m_numInputUsageSlots);
#else //SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *hsCache = nullptr;
				InputParameterCache *lsCache = nullptr;
#endif //SCE_GNM_CUE2_ENABLE_CACHE
				setLsHsShaders(lsb,shaderModifier,fetchShaderAddr,lsCache,hsb,numPatches,hsCache);
			}
			/**
			* @brief Binds shaders to the LS and HS stages and updates the LS shader's LDS size based on the HS shader's needs.
			*
			* This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants set in <c><i>hsb</i></c> or the value of <c><i>numPatches</i></c> are different from current state:

			Gnm::HsStageRegisters
			- <c>m_vgtTfParam</c>
			- <c>m_vgtHosMaxTessLevel</c>
			- <c>m_vgtHosMinTessLevel</c>

			Gnm::HullStateConstants
			- <c>m_numThreads</c>
			- <c>m_numInputCP</c>

			The check is deferred until next draw call.
			* @param[in] lsb				A pointer to the shader to bind to the LS stage. This must not be set to <c>NULL</c> if <c><i>hsb</i></c> is non-<c>NULL</c>.
			* @param[in] shaderModifier	The associated shader modifier value if the shader requires a fetch shader; otherwise, pass a value of <c>0</c>.
			* @param[in] fetchShaderAddr	The GPU address of the fetch shader if the LS shader requires a fetch shader; otherwise, pass a value of <c>0</c>.
			* @param[in] hsb				A pointer to the shader to bind to the HS stage. This parameter must not be set to <c>NULL</c> if <c><i>lsb</i></c> is non-<c>NULL</c>.
			* @param[in] numPatches		The number of patches in the HS shader.
			* @param[in] distributionMode Tessellation distribution mode.
			*
			* @note Only the shader pointers are cached inside the ConstantUpdateEngine; the location and contents of <c><i>lsb</i></c> and <c><i>hsb</i></c> must not change
			*       until different shaders are bound to these stages!
			* @note NEO mode only.
			*/
			void setLsHsShaders(const Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const Gnmx::HsShader *hsb, uint32_t numPatches, Gnm::TessellationDistributionMode distributionMode)
			{
#if SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *hsCache = m_stageInputCache + Gnm::kShaderStageHs;
				if (hsb && m_currentHSB != hsb)
					initializeInputsCache(hsCache, hsb->getInputUsageSlotTable(), hsb->m_common.m_numInputUsageSlots);
				InputParameterCache *lsCache = m_stageInputCache + Gnm::kShaderStageLs;
				if (lsb && m_currentLSB != lsb)
					initializeInputsCache(lsCache, lsb->getInputUsageSlotTable(), lsb->m_common.m_numInputUsageSlots);
#else //SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *hsCache = nullptr;
				InputParameterCache *lsCache = nullptr;
#endif //SCE_GNM_CUE2_ENABLE_CACHE
				setLsHsShaders(lsb, shaderModifier, fetchShaderAddr, lsCache, hsb, numPatches, distributionMode, hsCache);
			}

			/**
			 * @brief Binds shaders to the LS and HS stages and updates the LS shader's LDS size based on the HS shader's needs.
			 *
			 * This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants set in <c><i>hsb</i></c> or the value of <c><i>numPatches</i></c> are different from current state:

			 Gnm::HsStageRegisters
			 - <c>m_vgtTfParam</c>
			 - <c>m_vgtHosMaxTessLevel</c>
			 - <c>m_vgtHosMinTessLevel</c>

			 Gnm::HullStateConstants
			 - <c>m_numThreads</c>
			 - <c>m_numInputCP</c>

			 The check is deferred until next draw call.
			 * @param[in] lsb				A pointer to the shader to bind to the LS stage. This must not be set to <c>NULL</c> if <c><i>hsb</i></c> is non-<c>NULL</c>.
			 * @param[in] shaderModifier	The associated shader modifier value if the shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] fetchShaderAddr	The GPU address of the fetch shader if the LS shader requires a fetch shader; otherwise pass a value of 0.
			 * @param[in] lsCache			The cached shader inputs for the LS shader.
			 * @param[in] hsb				A pointer to the shader to bind to the HS stage. This must not be set to <c>NULL</c> if <c><i>lsb</i></c> is non-<c>NULL</c>.
			 * @param[in] numPatches		The number of patches in the HS shader.
			 * @param[in] hsCache			The cached shader inputs for the HS shader.
			 *
			 * @note Only the shader pointers are cached inside the ConstantUpdateEngine; the location and contents of <c><i>lsb</i></c> and <c><i>hsb</i></c> must not change
			 *       until different shaders are bound to these stages!
			 */
			void setLsHsShaders(const Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache, const Gnmx::HsShader *hsb, uint32_t numPatches, const ConstantUpdateEngine::InputParameterCache *hsCache);
			/**
			* @brief Binds shaders to the LS and HS stages and updates the LS shader's LDS size based on the HS shader's needs.
			*
			* This function will roll hardware context if any of the Gnm::HsStageRegisters or Gnm::HullStateConstants set in <c><i>hsb</i></c> or the value of <c><i>numPatches</i></c> are different from current state:

			Gnm::HsStageRegisters
			- <c>m_vgtTfParam</c>
			- <c>m_vgtHosMaxTessLevel</c>
			- <c>m_vgtHosMinTessLevel</c>

			Gnm::HullStateConstants
			- <c>m_numThreads</c>
			- <c>m_numInputCP</c>

			The check is deferred until next draw call.
			* @param[in] lsb				A pointer to the shader to bind to the LS stage. This parameter must not be set to <c>NULL</c> if <c><i>hsb</i></c> is non-<c>NULL</c>.
			* @param[in] shaderModifier	The associated shader modifier value if the shader requires a fetch shader; otherwise, pass a value of <c>0</c>.
			* @param[in] fetchShaderAddr	The GPU address of the fetch shader if the LS shader requires a fetch shader; otherwise, pass a value of <c>0</c>.
			* @param[in] lsCache			The cached shader inputs for the LS shader.
			* @param[in] hsb				A pointer to the shader to bind to the HS stage. This must not be set to <c>NULL</c> if <c><i>lsb</i></c> is non-<c>NULL</c>.
			* @param[in] numPatches		The number of patches in the HS shader.
			* @param[in] distributionMode The tessellation distribution mode.
			* @param[in] hsCache			The cached shader inputs for the HS shader.
			*
			* @note Only the shader pointers are cached inside the ConstantUpdateEngine; the location and contents of <c><i>lsb</i></c> and <c><i>hsb</i></c> must not change
			*       until different shaders are bound to these stages!
			* @note NEO mode only.
			*/
			void setLsHsShaders(const Gnmx::LsShader *lsb, uint32_t shaderModifier, void *fetchShaderAddr, const ConstantUpdateEngine::InputParameterCache *lsCache, const Gnmx::HsShader *hsb, uint32_t numPatches, Gnm::TessellationDistributionMode distributionMode, const ConstantUpdateEngine::InputParameterCache *hsCache);
			/**
			 * @brief Binds a shader to the ES stage.
			 *
			 * @param esb				A pointer to the shader to bind to the ES stage.
			 * @param shaderModifier	The shader modifier value generated by generateEsFetchShaderBuildState(). Specify a value of 0 if no fetch shader is required.
			 * @param fetchShaderAddr	The GPU address of the fetch shader if the shader requires it; otherwise specify a value of 0.
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*esb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setEsShader(const Gnmx::EsShader *esb, uint32_t shaderModifier, void *fetchShaderAddr)
			{
				setOnChipEsShader(esb, 0, shaderModifier, fetchShaderAddr);
			}

			/**
			 * @brief Binds a shader to the ES stage.
			 *
			 * @param esb				A pointer to the shader to bind to the ES stage.
			 * @param shaderModifier	The shader modifier value generated by generateEsFetchShaderBuildState(). Specify a value of 0 if no fetch shader is required.
			 * @param fetchShaderAddr	The GPU address of the fetch shader if the shader requires it; otherwise specify a value of 0.
			 * @param cache				The shader inputs cache. 
			 *
			 * @see initializeInputsCache()
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*esb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setEsShader(const Gnmx::EsShader *esb, uint32_t shaderModifier, void *fetchShaderAddr, const InputParameterCache *cache)
			{
				setOnChipEsShader(esb, 0, shaderModifier, fetchShaderAddr, cache);
			}
			

			/**
			 * @brief Binds an on-chip-GS vertex shader to the ES stage.
			 *
			 * This function never rolls the hardware context.
			 *
			 * @param[in] esb					A pointer to the shader to bind to the ES stage.
			 * @param[in] ldsSizeIn512Bytes		The size of LDS allocation required for on-chip GS here. This must be a multiple of 512 bytes and must be compatible with the GsShader and gsPrimsPerSubGroup value passed to setOnChipGsVsShaders().
			 * @param[in] shaderModifier		The shader modifier value if the shader requires a fetch shader. Specify a value of 0 if no fetch shader is required.
			 * @param[in] fetchShaderAddr		The GPU address of the fetch shader if the shader requires it; otherwise specify a value of 0.
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*esb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @see computeOnChipGsConfiguration()
			 */
			void setOnChipEsShader(const sce::Gnmx::EsShader *esb, uint32_t ldsSizeIn512Bytes, uint32_t shaderModifier, void *fetchShaderAddr)
			{
#if SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = m_stageInputCache+Gnm::kShaderStageEs;
				if ( esb && m_currentESB != esb )
					initializeInputsCache(cache, esb->getInputUsageSlotTable(), esb->m_common.m_numInputUsageSlots);
#else //SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = nullptr;
#endif //SCE_GNM_CUE2_ENABLE_CACHE
				setOnChipEsShader(esb, ldsSizeIn512Bytes, shaderModifier, fetchShaderAddr, cache);
			}


			/**
			 * @brief Binds an on-chip-GS vertex shader to the ES stage.
			 *
			 * This function never rolls the hardware context.
			 *
			 * @param[in] esb					A pointer to the shader to bind to the ES stage.
			 * @param[in] ldsSizeIn512Bytes		The size of LDS allocation required for on-chip GS here. This must be a multiple of 512 bytes and must be compatible with the GsShader and gsPrimsPerSubGroup value passed to setOnChipGsVsShaders().
			 * @param[in] shaderModifier		The shader modifier value if the shader requires a fetch shader. Specify a value of 0 if no fetch shader is required.
			 * @param[in] fetchShaderAddr		The GPU address of the fetch shader if the shader requires it; otherwise specify a value of 0.
			 * @param cache						The shader inputs cache.
			 *
			 * @see initializeInputsCache()
			 *
			 * @note  Only the pointer is cached inside the ConstantUpdateEngine; the location and contents of <c>*esb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @see computeOnChipGsConfiguration
			 */
			void setOnChipEsShader(const sce::Gnmx::EsShader *esb, uint32_t ldsSizeIn512Bytes, uint32_t shaderModifier, void *fetchShaderAddr, const InputParameterCache *cache);

			/**
			 * @brief Binds a shader to the GS and VS stages.
			 *
			 * @param gsb A pointer to the shader to bind to the GS/VS stages.
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*gsb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setGsVsShaders(const GsShader *gsb)
			{
#if SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = m_stageInputCache+Gnm::kShaderStageGs;
				if ( gsb && m_currentGSB != gsb )
					initializeInputsCache(cache, gsb->getInputUsageSlotTable(), gsb->m_common.m_numInputUsageSlots);
#else //SCE_GNM_CUE2_ENABLE_CACHE
				InputParameterCache *cache = nullptr;
#endif //SCE_GNM_CUE2_ENABLE_CACHE
				setGsVsShaders(gsb, cache);
			}


			/**
			 * @brief Binds a shader to the GS and VS stages.
			 *
			 * @param gsb		A pointer to the shader to bind to the GS/VS stages.
			 * @param cache		The shader inputs cache.
			 *
			 * @see initializeInputsCache()
			 *
			 * @note  Only the pointer is cached inside the Constant Update Engine; the location and contents of <c>*gsb</c> must not change until
			 *        a different shader has been bound to this stage!
			 *
			 * @note This binding will not take effect on the GPU until preDraw() or preDispatch() is called.
			 */
			void setGsVsShaders(const GsShader *gsb, const InputParameterCache *cache);

			/** @brief Updates the CUE's information about the current GS mode.

			*/
			void setGsModeOff();
			

#if SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE
			/**
			* @brief Sets the PS Input Table.
			*
			* @param[in] psInput PS Input table.
			*
			* @note This function should be called after calls to setPsShader() and setVsShader().
			*       
			*/
			void setPsInputUsageTable(uint32_t *psInput);
#else //SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE
			/**
			 * @brief Sets the PS Input Table.
			 *
			 * @param[in] psInput PS Input table.
			 *
			 * @note This function should be called after calls to setPsShader() and setVsShader().
			 *       Calls to setPsShader() or setVsShader() will invalidate this table.
			 *       If not specified, an appropriate table will be auto-generated in preDraw().
			 */
			void setPsInputUsageTable(uint32_t *psInput)
			{
				m_psInputs = psInput;
			}
#endif //SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE
			/**
			 * @brief	Sets the vertex and instance offset for the current shader configuration.
			 *			For the <c><i>vertexOffset</i></c> and <c><i>instanceOffset</i></c> to have any effect, the VS shader has to be compiled with -indirect-draw flag.
			 *
			 * @param vertexOffset		The offset added to each vertex index.
			 * @param instanceOffset	The offset added to instance index.
			 */
			void setVertexAndInstanceOffset(uint32_t vertexOffset, uint32_t instanceOffset);

			/**
			 * @brief Checks if the current shader configuration is expecting vertex or instance offset.
			 */
			bool isVertexOrInstanceOffsetEnabled() const;

			void setOnChipEsVertsPerSubGroup(uint16_t onChipEsVertsPerSubGroup)
			{
				m_onChipEsVertsPerSubGroup = onChipEsVertsPerSubGroup;
			}

			void setOnChipEsExportVertexSizeInDword(uint16_t onChipEsExportVertexSizeInDword)
			{
				m_onChipEsExportVertexSizeInDword = onChipEsExportVertexSizeInDword;
			}
			
			void enableGsModeOnChip(Gnm::GsMaxOutputPrimitiveDwordSize maxPrimDwordSize, uint32_t esVerticesPerSubGroup, uint32_t gsInputPrimitivesPerSubGroup);

			////////////////////////////////
			// Draw/Dispatch related functions
			////////////////////////////////

			/**
			 * @brief Executes all previous enqueued resource and shader bindings in preparation for a draw call.
			 *
			 * This may involve waiting on a previous draw to complete before kicking off a DMA to copy
			 * dirty resource chunks to/from CPRAM.
			 *
			 * @note When using the Constant Update Engine to manage shaders and shader resources, this function must be called immediately before every draw call,
			 * and postDraw() must be called immediately afterwards.
			 *
			 * @see postDraw()
			 */
			void preDraw();

			/**
			 * @brief Inserts GPU commands to indicate that the resources used by the most recent draw call can be safely overwritten in the Constant Update Engine ring buffers.
			 *
			 * @note When using the Constant Update Engine to manage shaders and shader resources, preDraw() must be called immediately before every draw call,
			 * and this function must be called immediately afterwards.
			 * @see preDraw()
			 */
			void postDraw();

			/**
			 * @brief Executes all previous enqueued resource and shader bindings in preparation for a dispatch call.
			 *
			 * This may involve waiting on a previous draw to complete before kicking off a DMA to copy
			 * dirty resource chunks to/from CPRAM.
			 *
			 * @note When using the Constant Update Engine to manage shaders and shader resources, this function must be called immediately before every dispatch call,
			 * and postDispatch() must be called immediately afterwards.
			 *
			 * @see postDispatch()
			 */
			void preDispatch();

			/**
			 * @brief Inserts GPU commands to indicate that the resources used by the most recent dispatch call can be safely overwritten in the Constant Update Engine ring buffers.
			 *
			 * @note When using the Constant Update Engine to manage shaders and shader resources, preDispatch() must be called immediately before every dispatch call,
			 * and this function must be called immediately afterwards.
			 *
			 * @see preDispatch()
			 */
			void postDispatch();

			/**
			 * @brief Returns the DispatchOrderedAppendMode setting that the currently set graphics pipe CS shader requires.
			 *
			 * @return The DispatchOrderedAppendMode setting that the currently set graphics pipe CS shader requires. If non-zero, you must call the DrawCommandBuffer::dispatchWithOrderedAppend() function.
			 */
			Gnm::DispatchOrderedAppendMode getCsShaderOrderedAppendMode() const { return (Gnm::DispatchOrderedAppendMode)m_gfxCsShaderOrderedAppendMode; }

			//

			/**
			 * @brief Advances the frame and should be called at the beginning of every frame.
			 */
			void advanceFrame(void);

			/**
			 * @brief Invalidates all current shader and resource bindings.
			 */
			void invalidateAllBindings(void);

			/**
			 * @brief Invalidate all current shader context bindings.
			 */
			void invalidateAllShaderContexts(void) { m_shaderContextIsValid = 0; }

			/**
			 * @brief Ensures that all needed constant state packets in the CCB are allocated after the provided address.
			 * 
			 *  This function will check the current constant state in the CCB and will move chunks if necessary to ensure that all the needed CCB packets are 
			 *  allocated after the provided address. This could be useful to implement a ring buffer CCB for example.
			 * @note This only deals with the chunk state in the CCB without affecting the EUD.
			 *
			 * @param[in] fence The lowest possible GPU address for constant state packets.
			 * @return the address 
			 */
			const void* moveConstantStateAfterAddress(const void* fence);
			/**
			 * @brief Gets the size of the CUE heap.
			 *
			 * @return			The size of the CUE heap in bytes.
			 *
			 * @note To compute the required size of a CUE heap before the object is initialized, use computeHeapSize().
			 *
			 * @see computeHeapSize()
			 */
			uint32_t getHeapSize(void) const
			{
				return m_heapSize;
			}
#if SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR
			/**
			 * @brief Sets a user provided function to allocate EUD data.
			 *
			 * The memory for EUD has to be allocated in the GPU-visible memory (preferably Garlic) and aligned at 16 bytes. 
			 * There is no corresponding free() callback and it is up to the user to re-cycle/free this memory after the GPU has finished processing the context
			 * where the memory had been allocated.
			 * 
			 * @param allocator		A pointer to the function that allocates memory for the EUD. Its first argument is the size of allocation in <c>DWORD</c>s and the second is arbitrary user data.
			 * @param userData		The value to be passed as a second parameter to the allocator function.
			 */
			void setUserEudAllocator(void* (*allocator)(uint32_t sizeInDword, void* userData), void* userData)
			{
				m_userEudAllocator = allocator;
				m_userEudAllocatorData = userData;
			}
#endif //SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR

		public:

			//--------------------------------------------------------------------------------
			// Adjustable Constants:
			//--------------------------------------------------------------------------------

			static const uint32_t		kResourceChunkSize		 =  8;
			static const uint32_t		kResourceNumChunks		 = 16;
			static const uint32_t		kSamplerChunkSize		 =  8;
			static const uint32_t		kSamplerNumChunks		 =  2;
			static const uint32_t		kConstantBufferChunkSize =  8;
			static const uint32_t		kConstantBufferNumChunks =  3;
			static const uint32_t		kVertexBufferChunkSize	 =  8;
			static const uint32_t		kVertexBufferNumChunks	 =  4;
			static const uint32_t		kRwResourceChunkSize	 =  8;
			static const uint32_t		kRwResourceNumChunks	 =  2;

#ifndef DOXYGEN_IGNORE
			//--------------------------------------------------------------------------------
			// Internal Constants:
			//--------------------------------------------------------------------------------

			// Ring buffer indices for each chunked resource type.
			enum
			{
				kRingBuffersIndexResource,
				kRingBuffersIndexRwResource,
				kRingBuffersIndexSampler,
				kRingBuffersIndexVertexBuffer,
				kRingBuffersIndexConstantBuffer,

				kNumRingBuffersPerStage
			};
			// Invalid on-chip GS shader mode signature
			enum
			{
				kOnChipGsInvalidSignature = 0xFFFFFFFFU
			};
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			// User resource tables indices for each resource type
			enum
			{
				kUserTableResource,
				kUserTableRwResource,
				kUserTableSampler,
				kUserTableVertexBuffer,
				kUserTableConstantBuffer,

				kNumUserTablesPerStage
			};
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES

			//--------------------------------------------------------------------------------
			// Internal Objects:
			//--------------------------------------------------------------------------------

			typedef struct StageChunkState
			{
				uint16_t  curSlots;    // Bitfield representing the resource which have been set for the upcoming draw.
				uint16_t  usedSlots;   // Bitfield representing the resource set for the previous draw.
				uint32_t  reserved;

				__int128_t *curChunk;  // Pointer to the current constructed chunk of resource for the next draw.
				__int128_t *usedChunk; // Pointer to the previously use chunk of resource from the last draw.
			} StageChunkState;

			// Different states for StageChunkState:
			// - Init: nothing allocated:
			//   . m_newResources  = 0
			//   . m_usedResources = 0
			//   . curChunk      = 0
			//   . m_prevChunk     = 0
			// - Just after draw:
			//   . m_newResources  = resource "used" by a previous draw
			//   . m_usedResources = 0
			//   . curChunk      = current resource set
			//   . m_prevChunk     = 0
			// - Between draws (no conflict):
			//   . m_newResources  = resource "used" by a previous draws and incoming draw
			//   . m_usedResources = 0
			//   . curChunk      = current resource set
			//   . m_prevChunk     = 0
			// - Between draws (w/ conflicts):
			//   . m_newResources  = resource "used" for incoming draw
			//   . m_usedResources = resource "used" by a previous draws and incoming draw
			//   . curChunk      = new set of resource
			//   . m_prevChunk     = previous resource set

			struct ShaderResourceRingBuffer
			{
				uint32_t  headElemIndex;
				uint32_t  reserved;
				void     *elementsAddr;   // GPU address of the array of elements.
				uint32_t  elemSizeDwords; // Size of each element, in <c>DWORD</c>s.
				uint32_t  elemCount;	  // Capacity of the buffer, in elements.
				uint64_t  wrappedIndex;
				uint64_t  halfPointIndex;
			};

			typedef struct InputParameterCache
			{
#define SCE_GNM_CUE2_PARAMETER_HAS_SRC_RESOURCE_TRACKING (SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES || SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES)
#define SCE_GNM_CUE2_PARAMETER_HAS_USER_TABLE SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				struct DescriptorParam
				{
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
					uint64_t setMask;
					uint32_t chunkStateOffset;
					uint32_t setBitsOffset;
					uint32_t chunkSlot;
					uint32_t chunkSlotMask;
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY					
					uint32_t destination;
					uint32_t regCount;
#if SCE_GNM_CUE2_PARAMETER_HAS_SRC_RESOURCE_TRACKING
					uint32_t activeBitsOffset;
					uint32_t srcApiSlot;
#endif //SCE_GNM_CUE2_PARAMETER_HAS_SRC_RESOURCE_TRACKING
#if SCE_GNM_CUE2_PARAMETER_HAS_USER_TABLE
					uint32_t userTableId;
					uint32_t userTableIndex;
#endif //SCE_GNM_CUE2_PARAMETER_HAS_USER_TABLE
				};
				struct DwordParam
				{
					uint16_t referenceOffset;
					uint8_t  destination;
					uint8_t  pad;
				};
				struct DelegateParam
				{
					uint16_t delegateIndex;
					uint8_t  slot;
					uint8_t	 pad;
				};
				union Param
				{
					DescriptorParam descriptor;
					DwordParam      scalar;
					DelegateParam   special;
				};
				enum PointerType 
				{
					kPtrResourceTable,
					kPtrSamplerTable,
					kPtrConstBufferTable,
					kPtrRwResourceTable,
					kPtrVertexTable,
					kNumPointerTypes
				};

				uint32_t userPointerTypesMask;
				uint32_t eudPointerTypesMask;
				uint8_t  userPointers[kNumPointerTypes];
				uint8_t  eudPointers[kNumPointerTypes];
			    uint8_t  pad[2];
				// new
				uint32_t scalarUserStart;
				uint32_t immediateEudStart;
				uint32_t scalarEudStart;
				uint32_t delegateStart;
				uint32_t delegateEnd;

				enum {kMaxCacheSlots = 128 };
				Param slots[kMaxCacheSlots];

			} InputParameterCache;

			typedef struct StageInfo
			{
#if !SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
				StageChunkState resourceStage[kResourceNumChunks];
				StageChunkState samplerStage[kSamplerNumChunks];
				StageChunkState constantBufferStage[kConstantBufferNumChunks];
				StageChunkState vertexBufferStage[kVertexBufferNumChunks];
				StageChunkState rwResourceStage[kRwResourceNumChunks];

				ShaderResourceRingBuffer ringBuffers[kNumRingBuffersPerStage];
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				const void*		userTable[kNumUserTablesPerStage];
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
				void      *internalSrtBuffer;

				// Bitfield tracking which the resources have been set and
				// Bitfields tracking which resources have been set within the EUD:
				uint64_t   activeResource[2];
				uint64_t   eudResourceSet[2];

				uint64_t   activeSampler;
				uint64_t   eudSamplerSet;

				uint64_t   activeConstantBuffer;
				uint64_t   eudConstantBufferSet;

				uint64_t   activeRwResource;
				uint64_t   eudRwResourceSet;

				uint64_t   activeVertexBuffer;

				//

				uint32_t   boolValue;
				

				uint32_t   userSrtBuffer[16];
				uint32_t   userSrtBufferSizeInDwords;
#if SCE_GNM_CUE2_ENABLE_CACHE
				const InputParameterCache*		activeCache;
#endif //SCE_GNM_CUE2_ENABLE_CACHE

				const Gnm::InputUsageSlot	*inputUsageTable;
				uint32_t				 	*eudAddr;
				uint32_t				 	 shaderBaseAddr256;
				uint32_t				 	 shaderCodeSizeInBytes;
				uint16_t					 inputUsageTableSize;
				uint16_t                     eudSizeInDWord; // 0 means: no EUD.
				uint32_t                     dirtyRing; // Bitfield of input to upload to the ring.
				uint32_t                     appendConsumeDword;
				uint32_t                     gdsMemoryRangeDword;
			} StageInfo;

			////////////////////////////////
			// Compatibility API added. These functions are either no-ops, or wrappers around
			// other CUE functions.
			////////////////////////////////

			void setBoolConstants(Gnm::ShaderStage stage, uint32_t startApiSlot, uint32_t numApiSlots, const uint32_t *bits)
			{
				SCE_GNM_ASSERT_MSG_INLINE(startApiSlot < Gnm::kSlotCountBoolConstant, "startApiSlot (%u) must be in the range [0..%d].", startApiSlot, Gnm::kSlotCountBoolConstant-1);
				SCE_GNM_ASSERT_MSG_INLINE(numApiSlots <= Gnm::kSlotCountBoolConstant, "numApiSlots (%u) must be in the range [0..%d].", numApiSlots, Gnm::kSlotCountBoolConstant);
				SCE_GNM_ASSERT_MSG_INLINE(startApiSlot+numApiSlots <= Gnm::kSlotCountBoolConstant, "startApiSlot (%u) + numApiSlots (%u) must be in the range [0..%d].", startApiSlot, numApiSlots, Gnm::kSlotCountBoolConstant);
				if ( numApiSlots == 0 )
					return;
				const uint32_t andMask = m_stageInfo[stage].boolValue;
				uint32_t orMask  = 0;
				for(uint32_t iSlot=0; iSlot<numApiSlots; ++iSlot)
					orMask |= (bits[iSlot] ? 1U : 0U) << iSlot;
				orMask = orMask << startApiSlot;
				setBoolConstants(stage, andMask, orMask);
			}

#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
			// User table management API, placed here to allow inlining
			/**
			 * @brief Set the user table for textures and buffers.
			 * When a user table is set for a particular resource type the CUE uses that table instead of its internal resource tracking.
			 * This allows for faster draw setup and simpler interface for the applications that already manage their resources themselves.
			 * Setting it to <c>NULL</c> reverts to the CUE resource tracking.
			 * @param stage The table will be bound to this shader stage.
			 * @param table The table (array) of resources. Each resource entry takes 16 bytes (size of Gnm::Texture).
			 */
			void setUserResourceTable(Gnm::ShaderStage stage, const Gnm::Texture *table)
			{
				m_stageInfo[stage].userTable[kUserTableResource] = table;
			}
			/**
			 * @brief Set the user table for read/write textures and buffers.
			 * When a user table is set for a particular resource type the CUE uses that table instead of its internal resource tracking.
			 * This allows for faster draw setup and simpler interface for the applications that already manage their resources themselves.
			 * Setting it to <c>NULL</c>  reverts to the CUE resource tracking.
			 * @param stage The table will be bound to this shader stage.
			 * @param table The table (array) of resources. Each resource entry takes 16 bytes (size of Gnm::Texture).
			 */
			void setUserRwResourceTable(Gnm::ShaderStage stage, const Gnm::Texture *table)
			{
				m_stageInfo[stage].userTable[kUserTableRwResource] = table;
			}
			/**
			 * @brief Set the user table for samplers.
			 * When a user table is set for a particular resource type the CUE uses that table instead of its internal resource tracking.
			 * This allows for faster draw setup and simpler interface for the applications that already manage their resources themselves.
			 * Setting it to <c>NULL</c> reverts to the CUE resource tracking.
			 * @param stage The table will be bound to this shader stage.
			 * @param table The table (array) of samplers.
			 */
			void setUserSamplerTable(Gnm::ShaderStage stage, const Gnm::Sampler *table)
			{
				m_stageInfo[stage].userTable[kUserTableSampler] = table;
			}
			/**
			 * @brief Set the user table for constant buffers.
			 * When a user table is set for a particular resource type the CUE uses that table instead of its internal resource tracking.
			 * This allows for faster draw setup and simpler interface for the applications that already manage their resources themselves.
			 * Setting it to <c>NULL</c> reverts to the CUE resource tracking.
			 * @param stage The table will be bound to this shader stage.
			 * @param table The table (array) of constant buffers.
			 */
			void setUserConstantBufferTable(Gnm::ShaderStage stage, const Gnm::Buffer *table)
			{
				m_stageInfo[stage].userTable[kUserTableConstantBuffer] = table;
			}
			/**
			 * @brief Set the user table for vertex buffers
			 * When a user table is set for a particular resource type the CUE uses that table instead of its internal resource tracking.
			 * This allows for faster draw setup and simpler interface for the applications that already manage their resources themselves.
			 * Setting it to <c>NULL</c> reverts to the CUE resource tracking.
			 * @param stage The table will be bound to this shader stage.
			 * @param table The table (array) of vertex buffers.
			 */
			void setUserVertexTable(Gnm::ShaderStage stage, const Gnm::Buffer *table)
			{
				m_stageInfo[stage].userTable[kUserTableVertexBuffer] = table;
			}
			/**
			 * @brief Clear all user tables on all stages.
			 */
			void clearUserTables();
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES

			//--------------------------------------------------------------------------------
			// DispatchDraw API:
			//--------------------------------------------------------------------------------
			void setDispatchDrawData(void const* pDispatchDrawData, uint32_t sizeofDispatchDrawData)
			{
				m_pDispatchDrawData = pDispatchDrawData;
				m_sizeofDispatchDrawData = sizeofDispatchDrawData;
			}

			void preDispatchDraw(Gnm::DispatchOrderedAppendMode *pOutOrderedAppendMode, uint32_t *pOutDispatchDrawIndexDeallocationMask, uint32_t *pOutSgprKrbLoc, uint32_t *pOutSgprInstanceCs,
				Gnm::DispatchDrawMode *pOutDispatchDrawMode, uint32_t *pOutSgprVrbLoc, uint32_t *pOutSgprInstanceVs);
			void postDispatchDraw();

			// deprecated:
			void preDispatchDraw(GnmxDrawCommandBuffer *dcb, GnmxDispatchCommandBuffer *acb, GnmxConstantCommandBuffer *ccb,
				Gnm::DispatchOrderedAppendMode *pOutOrderedAppendMode, uint32_t *pOutDispatchDrawIndexDeallocationMask, uint32_t *pOutSgprKrbLoc, uint32_t *pOutSgprInstanceCs,
				Gnm::DispatchDrawMode *pOutDispatchDrawMode, uint32_t *pOutSgprVrbLoc, uint32_t *pOutSgprInstanceVs);
			void postDispatchDraw(GnmxDrawCommandBuffer *dcb, GnmxDispatchCommandBuffer *acb, GnmxConstantCommandBuffer *ccb);

			//--------------------------------------------------------------------------------
			// Placeholder stuff to be implemented later
			//--------------------------------------------------------------------------------
#if SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS
			SCE_GNM_API_DEPRECATED_MSG_NOINLINE("setInternalSrtBuffer() has no effect in the mixed mode SRT shaders, if you need to use this function please disable SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS option in the gnmx/config.h")
			void setInternalSrtBuffer(Gnm::ShaderStage stage, void *buf);
#else //SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS
			void setInternalSrtBuffer(Gnm::ShaderStage stage, void *buf);
#endif //SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS
			void setUserSrtBuffer(Gnm::ShaderStage stage, const void *buf, uint32_t bufSizeInDwords);

			//--------------------------------------------------------------------------------
			// Internal API:
			//--------------------------------------------------------------------------------
			static void updateChunkState128(StageChunkState *chunkState, uint32_t numChunks);
			static void updateChunkState256(StageChunkState *chunkState, uint32_t numChunks);
			const void* copyChunkState(Gnm::ShaderStage stage, uint32_t baseResourceOffset, uint32_t chunkSizeInDw, uint32_t numChunks, StageChunkState* chunkState, const void* fence );
			void applyInputUsageData(Gnm::ShaderStage currentStage);
			void applyInputUsageDataForDispatchDrawCompute();

			// Internal structure used to pass current state between callbacks
			struct ApplyUsageDataState
			{
				const Gnm::InputUsageSlot *inputUsageTable;
				Gnm::ShaderStage currentStage;
				uint8_t  pad[4];
				StageInfo *stageInfo;
				uint32_t *newEud;
			};

			// Internal handlers
			void updateSrtIUser(uint32_t usageSlot, ApplyUsageDataState* state);
			void updateLdsEsGsSizeUser(uint32_t usageSlot, ApplyUsageDataState* state);
			void updateLdsEsGsSizeEud(uint32_t usageSlot, ApplyUsageDataState* state);
			void updateSoBufferTableUser(uint32_t usageSlot, ApplyUsageDataState* state);
			void updateSoBufferTableEud(uint32_t usageSlot, ApplyUsageDataState* state);
			void updateGlobalTableUser(uint32_t usageSlot, ApplyUsageDataState* state);
			void updateGlobalTableEud(uint32_t usageSlot, ApplyUsageDataState* state);
			void updateEudUser(uint32_t usageSlot, ApplyUsageDataState* state);
			void updateDispatchDrawUser(uint32_t usageSlot, ApplyUsageDataState* state);

			//--------------------------------------------------------------------------------
			// Internal Variables:
			//--------------------------------------------------------------------------------

			GnmxDrawCommandBuffer     *m_dcb;
			GnmxConstantCommandBuffer *m_ccb;
			GnmxDispatchCommandBuffer *m_acb;

			RingSetup m_ringSetup;

			uint32_t  m_heapSize;
			uint32_t  m_numRingEntries;

			

			

			Gnm::ActiveShaderStages m_activeShaderStages;

			static const uint32_t kShaderStageAsynchronousCompute = Gnm::kShaderStageCount;

			

			StageInfo m_stageInfo[Gnm::kShaderStageCount+1];
#if SCE_GNM_CUE2_ENABLE_CACHE
			InputParameterCache m_stageInputCache[Gnm::kShaderStageCount+1];
#endif //SCE_GNM_CUE2_ENABLE_CACHE
			StageChunkState m_streamoutBufferStage;


			//////////////////////
			// Cached shader state
			//////////////////////
			const Gnmx::VsShader * m_currentVSB; // TODO: must currently be cached for compatibility with old CUE's drawIndirect()
			const Gnmx::PsShader * m_currentPSB; // TODO: must currently be cached to allow the PS inputs to be calculated before every draw, once we know what the VSB is.
			const Gnmx::CsShader * m_currentCSB;
			const Gnmx::LsShader * m_currentLSB; // TODO: must currently be cached for compatibility with old CUE's drawIndirect()
			const Gnmx::HsShader * m_currentHSB;
			const Gnmx::EsShader * m_currentESB; // TODO: must currently be cached for compatibility with old CUE's drawIndirect()
			const Gnmx::GsShader * m_currentGSB;
			const Gnmx::CsShader * m_currentAcbCSB; // For dispatch draw
			void				 * m_currentFetchShader[Gnm::kShaderStageCount+1];
			const uint32_t *m_psInputs;
			// Global Table Support:
#if SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
			StageChunkState m_globalTableStage;
			uint8_t m_eudReferencesGlobalTable;
			
#else //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE

			void     *m_globalTableAddr;
			uint32_t m_globalTablePtr[SCE_GNM_SHADER_GLOBAL_TABLE_SIZE / sizeof(uint32_t)];
			bool     m_globalTableNeedsUpdate;
#endif //SCE_GNM_CUE2_DYNAMIC_GLOBAL_TABLE
			uint8_t m_pad0[3];
			uint32_t m_shaderContextIsValid;
			Gnm::TessellationDistributionMode m_currentDistributionMode;

			Gnm::TessellationRegisters m_currentTessRegs;
			

			bool m_anyWrapped;
			bool m_usingCcb;
			bool m_submitWithCcb;
			bool m_eudReferencesStreamoutBuffers;

			// OnChip Gs Support:
			uint16_t m_onChipEsVertsPerSubGroup;
			uint16_t m_onChipEsExportVertexSizeInDword;
			uint32_t m_onChipGsSignature;

			// Bitfields indexed by shader stage
			uint8_t m_shaderUsesSrt;	 // One bit per shader stage. If a stage's bit is set, the bound shader uses Shader Resource Tables.
			uint8_t m_shaderDirtyEud;	 // One bit per shader stage. If a stage's bit is set, its EUD needs to be allocated and setup.
			uint8_t m_dirtyStage;
			uint8_t m_dirtyShader;

			

			uint8_t m_asyncCsShaderOrderedAppendMode;
			uint8_t m_gfxCsShaderOrderedAppendMode;
			uint8_t m_pad1[2];

			

			void*	(*m_userEudAllocator)(uint32_t,void*);
			void*	m_userEudAllocatorData;

			void const *m_pDispatchDrawData;
			
			uint32_t m_sizeofDispatchDrawData;

			bool     m_prefetchShaderCode;

			uint8_t m_dispatchDrawIndexDeallocNumBits;

			
			uint8_t m_pad2[3];
#endif // DOXYGEN_IGNORE
		};
	}
}

#endif
