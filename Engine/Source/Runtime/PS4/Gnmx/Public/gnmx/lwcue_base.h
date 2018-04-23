/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/
#pragma once

#if !defined(_SCE_GNMX_LWCUE_BASE_H)
#define _SCE_GNMX_LWCUE_BASE_H

#include <gnm.h>
#include <gnmx/shaderbinary.h>
#include <gnmx/helpers.h>
#include <stdint.h>

#if !defined(DOXYGEN_IGNORE)

/** @brief Validates resource descriptor data (i.e. checks if V#/T# is valid) as they are bound.
 *  @note While this is enabled, the LCUE will run slower. Enabled when linked against libSceGnmx_debug.
 */
#include <gnmx/lwcue_validation.h>
#if SCE_GNM_ENABLE_ASSERTS
	#define SCE_GNM_LCUE_VALIDATE_CONSTANT_BUFFER(lcue, callback, apislot, buffer)	validateConstantBuffer(lcue, callback, apislot, buffer)
	#define SCE_GNM_LCUE_VALIDATE_VERTEX_BUFFER(lcue, callback, apislot, buffer)	validateVertexBuffer(lcue, callback, apislot, buffer) 
	#define SCE_GNM_LCUE_VALIDATE_BUFFER(lcue, callback, apislot, buffer)			validateBuffer(lcue, callback, apislot, buffer)
	#define SCE_GNM_LCUE_VALIDATE_RWBUFFER(lcue, callback, apislot, buffer)			validateRwBuffer(lcue, callback, apislot, buffer)
	#define SCE_GNM_LCUE_VALIDATE_TEXTURE(lcue, callback, apislot, texture)			validateTexture(lcue, callback, apislot, texture)
	#define SCE_GNM_LCUE_VALIDATE_RWTEXTURE(lcue, callback, apislot, texture)		validateRwTexture(lcue, callback, apislot, texture)
	#define SCE_GNM_LCUE_VALIDATE_SAMPLER(lcue, callback, apislot, sampler)			validateSampler(lcue, callback, apislot, sampler)
	#define SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA(lcue, callback, addr, size)	validateShaderPreFetchArea(lcue, callback, addr, size)
#else // SCE_GNM_ENABLE_ASSERTS
	#define SCE_GNM_LCUE_VALIDATE_CONSTANT_BUFFER(lcue, callback, apislot, buffer)
	#define SCE_GNM_LCUE_VALIDATE_VERTEX_BUFFER(lcue, callback, apislot, buffer)	
	#define SCE_GNM_LCUE_VALIDATE_BUFFER(lcue, callback, apislot, buffer)			
	#define SCE_GNM_LCUE_VALIDATE_RWBUFFER(lcue, callback, apislot, buffer)			
	#define SCE_GNM_LCUE_VALIDATE_TEXTURE(lcue, callback, apislot, texture)			
	#define SCE_GNM_LCUE_VALIDATE_RWTEXTURE(lcue, callback, apislot, texture)		
	#define SCE_GNM_LCUE_VALIDATE_SAMPLER(lcue, callback, apislot, sampler)			
	#define SCE_GNM_LCUE_VALIDATE_SHADER_PREFETCH_AREA(lcue, callback, addr, size)
#endif // SCE_GNM_ENABLE_ASSERTS

#if SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING_ENABLED
	#define SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(a) SCE_GNM_ASSERT(a)
#else // SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING_ENABLED
	#define SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING(a)
#endif // SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING_ENABLED

// Validation of complete resource binding
#if SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
	#define SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(a, b) initResourceBindingValidation(a, b)
	#define SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(a, b) SCE_GNM_ASSERT(isResourceBindingComplete(a, b))
	#define SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(a) (a) = true
#else // SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
	#define SCE_GNM_LCUE_VALIDATE_RESOURCE_INIT_TABLE(a, b)
	#define SCE_GNM_LCUE_VALIDATE_RESOURCE_CHECK_TABLE(a, b)
	#define SCE_GNM_LCUE_VALIDATE_RESOURCE_MARK_USED(a)
#endif // SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED

#if !SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW && __ORBIS__
#define SCE_GNM_LCUE_DISPATCH_DRAW_AVAILABILITY() __attribute__((unavailable("To enable dispatch draw, please recompile Gnmx with SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW (can be found in Gnmx/config.h)")))
#else // !SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
#define SCE_GNM_LCUE_DISPATCH_DRAW_AVAILABILITY()
#endif // !SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW

#ifdef __ORBIS__
#define SCE_GNM_LCUE_NOT_SUPPORTED __attribute__((unavailable("method or attribute is not supported by the LightweightConstantUpdateEngine.")))
#define SCE_GNM_LCUE_USED __attribute__((used))
#else // __ORBIS__
#define SCE_GNM_LCUE_NOT_SUPPORTED 
#define SCE_GNM_LCUE_USED
#endif // __ORBIS__

#endif // !defined(DOXYGEN_IGNORE)

namespace sce
{
	namespace Gnmx
	{
		/*	GLOSSARY/DEFINITIONS:
		 *	-------------------------------------------------------------------------------------------------------------------------
		 *	User Data (UD): contains 16 SGPRs s[0:15] in a shader core, which are pre-fetched by the SPI before execution starts.
		 *	UD is set through PM4 packets, thus, you don't need to maintain copies of those resource descriptors in memory.
		 *	 
		 *	User Extended Data (UED): contains up to 48 <c>DWORD</c>s (SW limitation, size can be adjusted see Gnm Library Overview for more details) 
		 *	and does not have a specific type (e.g. V#/T#/S# are supported), and is stored in memory. If the UED is used, 
		 *	the UD will store a pointer to the start of each UED.
	 	 *	 
		 *	Flat-table (or array): up to [N] <c>DWORD</c>s (SW limitation) for a specific resource type, five tables available. Can be seen 
		 *	as a contiguous zero-based array that may (or may not) have gaps, and is stored in memory.
		 *	 
		 *	Flat-tables: resource (textures/buffers), rw-resource (textures/buffers), sampler, constant buffer and vertex buffer.
		 *	 
		 *	Memory based resources: UED or flat-table based resources.
		 */

		/** @brief The Lightweight Constant Update Engine (LCUE), like the CUE, manages dynamic mappings of shader resource descriptors in memory, but does so in a more CPU efficient manner.*/
		namespace LightweightConstantUpdateEngine
		{
#if !defined(DOXYGEN_IGNORE)
			// Internal constants defined for the LCUE

			// kMax* constants define how many resources of each available resource type will be stored and handled by the InputResourceOffsets table,
			// this also defines the maximum available API-slot globally for the LCUE. Default values are based on the average expected usage, so users are encouraged  
			// to modify the defaults to whatever best suits their application. Modifying the values will require Gnmx to be rebuilt. 
			// Note: resource types are textures/buffers, rw_textures/rw_buffers, samplers, vertex_buffers and constant_buffers.
			const int32_t kMaxResourceCount			= 48;	SCE_GNM_STATIC_ASSERT(kMaxResourceCount <= 128);		///< PSSL compiler limit is 128, Default value is 16
			const int32_t kMaxRwResourceCount		= 16;	SCE_GNM_STATIC_ASSERT(kMaxRwResourceCount <= 16);		///< PSSL compiler limit is 16, Default value is 16
			const int32_t kMaxSamplerCount			= 16;	SCE_GNM_STATIC_ASSERT(kMaxSamplerCount <= 16);			///< PSSL compiler limit is 16, Default value is 16
			const int32_t kMaxVertexBufferCount		= 16;	SCE_GNM_STATIC_ASSERT(kMaxVertexBufferCount <= 32);		///< PSSL compiler limit is 32, Default value is 16
			const int32_t kMaxConstantBufferCount	= 20;	SCE_GNM_STATIC_ASSERT(kMaxConstantBufferCount == 20);	///< PSSL compiler limit is 20, Default value is 20 Note: Because API-slots 15-19 are all reserved this value should remain 20
			const int32_t kMaxStreamOutBufferCount	= 4;	SCE_GNM_STATIC_ASSERT(kMaxStreamOutBufferCount == 4);	///< PSSL compiler limit is 4, Default value is 4
			
			const int32_t kMaxUserDataCount			= 16;	///< PSSL compiler limit is 16, count not tracked by the InputResourceOffsets table
			const int32_t kMaxSrtUserDataCount		= 16;	///< PSSL compiler limit is 16, count not tracked by the InputResourceOffsets table

			const int32_t kMaxResourceBufferCount	= 4;	///< Maximum number for supported splits for the resource buffer per LCUE instance
			const int32_t kMaxPsInputUsageCount		= 32;	///< Maximum number of interpolants a PS Stage can receive

			const int32_t kDefaultFetchShaderPtrSgpr			= 0;	///< Default SGPR in PSSL
			const int32_t kDefaultVertexBufferTablePtrSgpr		= 2;	///< Default SGPR in PSSL
			const int32_t kDefaultGlobalInternalTablePtrSgpr	= 0;	///< Default SGPR in PSSL, Note: it has lower priority than FetchPtr (sgpr would be s[4:5], after FetchPtr and VbPtr)
			const int32_t kDefaultStreamOutTablePtrSgpr			= 2;	///< Default SGPR in PSSL, only used by VS copy shader in GS active stage
			const int32_t kDefaultVertexldsEsGsSizeSgpr			= 0;	///< Default SGPR in PSSL, only used by VS copy shader in GS active stage

			const int32_t kResourceInUserDataSgpr	= 0x8000;	///< In User data resource Mask
			const int32_t kResourceIsVSharp			= 0x4000;	///< VSharp resource Mask Note: only used/available for immediate resources
			const int32_t kResourceValueMask		= 0x3FFF;	///< Resource memory offset is stored in the lower 14-bits

			// On-chip GS constants
			const uint32_t kOnChipGsInvalidSignature			= 0xFFFFFFFFU;

			// Tessellation distribution constants (Neo only)
			const uint32_t kTesselationDistrbutionMask			= 0x7FFFFFFFU;					///< Tessellation Distribution mask HS shader stage
			const uint32_t kTesselationDistrbutionEnabledMask	= ~kTesselationDistrbutionMask;	///< Tessellation Distribution enabled for HS shader stage

			// Shader stage constants
			const uint32_t kShaderStageAsynchronousCompute	= Gnm::kShaderStageCount;		///< Additional stage for CsVs dispatch Draw shader
			const uint32_t kNumShaderStages					= Gnm::kShaderStageCount + 1;	///< Number of unique shader stages for resource binding

			// 6KB is enough to store anything you can bind to a GPU shader stage, all counted in <c>DWORD</c>s
			const int32_t kGpuStageBufferSizeInDwords			= (6*1024) / sizeof(uint32_t);	///< Size of Single buffer Stage
			const int32_t kComputeScratchBufferSizeInDwords		= kGpuStageBufferSizeInDwords;	///< Size of the Compute Scratch buffer
			const int32_t kGraphicsScratchBufferSizeInDwords	= kNumShaderStages * kGpuStageBufferSizeInDwords; ///< Size of the Graphics Scratch buffer (encompasses all graphics shader stages)
			const int32_t kGlobalInternalTableSizeInDwords		= sce::Gnm::kShaderGlobalResourceCount * sizeof(sce::Gnm::Buffer) / sizeof(uint32_t); ///< Size of a global resource table

			// Internal constant buffers that are expected at fixed API-slots
			const int32_t kConstantBufferInternalApiSlotForEmbeddedData = 15; ///< Immediate/Embedded constant buffer fixed API-slot (any GPU stage).
			const int32_t kConstantBufferInternalApiSlotReserved0		= 16; ///< Slot 16 is reserved by compiler
			const int32_t kConstantBufferInternalApiSlotReserved1		= 17; ///< Slot 17 is reserved by compiler
			const int32_t kConstantBufferInternalApiSlotReserved2		= 18; ///< Slot 18 is reserved by compiler
			const int32_t kConstantBufferInternalApiSlotForTessellation = 19; ///< Tessellation constant buffer (with strides for LDS data) fixed API-slot (HS,VS/ES GPU stages).

			// Internal constants for ShaderBinaryInfo
			const uint64_t kShaderBinaryInfoSignatureMask		= 0x00ffffffffffffffLL;
			const uint64_t kShaderBinaryInfoSignatureU64		= 0x007264685362724fLL;

			// DispatchDraw constants
			const uint32_t kNumDispatchDrawRingBuffersRolls		= 16; ///< Number of times the dispatch ring buffers can be rolled in a LCUE instance, see setupDispatchDrawRingBuffers()
			
			/** @brief Used to validate shader resource bindings */
			struct ShaderResourceBindingValidation
			{
				bool resourceOffsetIsBound		[kMaxResourceCount];
				bool rwResourceOffsetIsBound	[kMaxRwResourceCount];
				bool samplerOffsetIsBound		[kMaxSamplerCount];
				bool vertexBufferOffsetIsBound	[kMaxVertexBufferCount];
				bool constBufferOffsetIsBound	[kMaxConstantBufferCount];
				bool appendConsumeCounterIsBound;
				bool gdsMemoryRangeIsBound;
// 				bool onChipEsVertsPerSubGroupIsBound;					// only used in graphics, TODO: add check in future
// 				bool onChipEsExportVertexSizeIsBound;					// only used in graphics, TODO: add check in future
//				bool streamOutOffsetIsBound[kMaxStreamOutBufferCount];	// No enough information is available when GS shader is used, stream-outs are in the VS but there's no footer available for VS in a VsGs shader (only GS has a footer).
			};

#endif	// !defined(DOXYGEN_IGNORE)
			/** @brief Defines the type of pipeline the resource buffer can use.
			 * 
			 *  @see LightweightConstantUpdateEngine::computeResourceBufferSize()
			 */
			typedef enum ResourceBufferType
			{
				kResourceBufferGraphics, ///< Configures the resource buffer for graphics
				kResourceBufferCompute	 ///< Configures the resource buffer for async compute
			} ResourceBufferType;


			/** @brief Estimates the size of the resource buffer required to support <c><i>numBatches</i></c>.
			 *
			 *  @param[in] bufferType	Type of pipeline that the resource buffer uses.
			 *  @param[in] numBatches	The number of batches (draw and/or dispatch) that the resource buffer needs to support.
			 *  @param[in] bRequiresStreamOut	Specifies whether the resource buffer supports streamout buffers for GS shader stages.
			 *
			 *  @return The calculated size of the resource buffer for the specified pipeline, expressed as a number of bytes.
			 * 
			 *  @note Specifying <c>kResourceBufferGraphics</c> as the value of the <c>bufferType</c> parameter may return a result that is larger than the size actually required, because <c>kResourceBufferGraphics</c> causes this function to calculate according to the maximum number of
			 *  shader stages that may be required per batch. To obtain a more conservative estimate, reduce <c><i>numBatches</i></c> to a suitable value.
			 */
			static inline uint32_t computeResourceBufferSize(ResourceBufferType bufferType, uint32_t numBatches, bool bRequiresStreamOut)
			{
				// Even though the hardware supports up to 8 different shader stages, technically only 6 stages can ever be active at most for a given batch 
				// (Geometry+Tessellation = kActiveShaderStagesLsHsEsGsVsPs)
				const uint32_t kNumSupportedStagesPerBatch = 6;
				uint32_t resourceBufferSizeInBytes = 0;
				resourceBufferSizeInBytes += kMaxResourceCount * Gnm::kDwordSizeResource * sizeof(uint32_t);
				resourceBufferSizeInBytes += kMaxRwResourceCount * Gnm::kDwordSizeRwResource * sizeof(uint32_t);
				resourceBufferSizeInBytes += kMaxSamplerCount * Gnm::kDwordSizeSampler * sizeof(uint32_t);
				resourceBufferSizeInBytes += kMaxConstantBufferCount * Gnm::kDwordSizeConstantBuffer * sizeof(uint32_t);

				if (bufferType == kResourceBufferGraphics)
				{
					resourceBufferSizeInBytes += kMaxVertexBufferCount * Gnm::kDwordSizeVertexBuffer * sizeof(uint32_t);
					if(bRequiresStreamOut)
						resourceBufferSizeInBytes += kMaxStreamOutBufferCount * Gnm::kDwordSizeStreamoutBuffer * sizeof(uint32_t);
					resourceBufferSizeInBytes *= kNumSupportedStagesPerBatch;
				}

				resourceBufferSizeInBytes *= numBatches;
				return resourceBufferSizeInBytes;
			}


			/** @brief Estimates the size of the resource buffer required to support <c><i>numBatches</i></c>.
			 *
			 *  @param[in] bufferType	The type of pipeline that the resource buffer uses.
			 *  @param[in] numBatches	The number of batches (draw and/or dispatch) that the resource buffer needs to support.
			 *
			 *  @return The calculated size of the resource buffer for the specified pipeline, expressed as a number of bytes.
			 * 
			 *  @note Specifying <c>kResourceBufferGraphics</c> as the value of the <c>bufferType</c> parameter may return a result that is larger than the 
			 *  size actually required, because <c>kResourceBufferGraphics</c> causes this function to calculate according to the maximum number of
			 *  shader stages that may be required per batch. To obtain a more conservative estimate, reduce <c><i>numBatches</i></c> to a suitable value.
			 *  @note This version of <c>computeResourceBufferSize()</c> does not provide a <c>bRequiresStreamOut</c> parameter and does not calculate the space 
			 *  required for streamout buffers. Thus, it always returns a smaller estimated resource buffer size than the overload that provides a <c>bRequiresStreamOut</c> parameter.
			 *  You may find it helpful to use this version of <c>computeResourceBufferSize()</c> when not using geometry shaders or when using on-chip Gs modes.
			 */
			static inline uint32_t computeResourceBufferSize(ResourceBufferType bufferType, uint32_t numBatches)
			{
				return computeResourceBufferSize(bufferType, numBatches, false);
			}

		} // LightweightConstantUpdateEngine

		/** @brief Accelerates the bindings of required shader resources by caching the input information of a shader. */
		struct InputResourceOffsets
		{
			uint16_t requiredBufferSizeInDwords;	///< Specifies how much memory needs to be reserved to store all memory-based resources. These are things not set through PM4.
			bool	 isSrtShader;					///< A flag that specifies whether the shader makes use of SRTs.
			uint8_t	 shaderStage;					///< The shader stage (LS/HS/ES/GS/VS/PS) for the shader resources offsets.
			
			// For each available shader-resource-ptr, store the starting SGPR s[0:254] where it'll be set (<c>0xFF</c> means not used). Pointers take 2 SGPRs (64b) and must be 2DW aligned
			uint8_t	fetchShaderPtrSgpr;				///< The SGPR containing the fetch shader pointer. If this exists, <c>s[0:1]</c> is always used.
			uint8_t	vertexBufferPtrSgpr;			///< The SGPR containing the vertex buffer table pointer. If this exists, <c>s[2:3]</c> is always used, but only in the vertex pipeline.
			uint8_t streamOutPtrSgpr;				///< The SGPR containing the stream out buffer pointer. If this exists, <c>s[2:3]</c> is always used, but only in the Geometry pipeline.
			uint8_t userExtendedData1PtrSgpr;		///< The SGPR containing the user extended data table pointer.
//			uint8_t userInternalSrtDataPtrSgpr;		///< *Note: Not supported for now*.
			uint8_t constBufferPtrSgpr;				///< The SGPR containing the constant buffer table pointer.
			uint8_t resourcePtrSgpr;				///< The SGPR containing the resource buffer table pointer.
			uint8_t rwResourcePtrSgpr;				///< The SGPR containing the read/write resource buffer table pointer.
			uint8_t samplerPtrSgpr;					///< The SGPR containing the sampler buffer table pointer.
			uint8_t globalInternalPtrSgpr;			///< The SGPR containing the global internal pointer, which is either stored in <c>s[0:1]</c> or <c>s[4:5]</c>.
			uint8_t appendConsumeCounterSgpr;		///< The SGPR containing the 32bit value address and size used from GDS.
			uint8_t gdsMemoryRangeSgpr;				///< The SGPR containing the GDS address range for storage.
			uint8_t ldsEsGsSizeSgpr;				///< The SGPR containing the GWS resource base offset.
			uint8_t userSrtDataSgpr;				///< The SGPR containing the start offset of the SRT Data Buffer.
			uint8_t userSrtDataCount;				///< The number of <c>DWORD</c>s in use by the SRT Data Buffer. The size will be between 1-8.

			uint8_t gdsKickRingBufferOffsetSgpr;	///< The SGPR containing the GDS kick ring buffer offset for DispatchDraw.
			uint8_t vertexRingBufferOffsetSgpr;		///< The SGPR containing the Vertex kick ring buffer offset for DispatchDraw.	
			uint8_t dispatchDrawPtrSgpr;			///< The SGPR containing the DispatchDraw data pointer to DispatchDrawTriangleCullData structure.
			uint8_t	dispatchDrawInstancesSgpr;		///< The SGPR containing the number of instances for DispatchDraw.

			// For each available shader-resource-flat-table (aka array), store the memory offset (from the start of the buffer) to the beginning of its flat-table (0xFFFF means it's not used).
			// Note: arrays are 0 indexed but the user can skip/set any index inside the range, allowing gaps at any place. This accelerates setting the pointer to the beginning of flat-tables.
			uint16_t constBufferArrayDwOffset;		///< The constant buffer table offset into the main buffer.
			uint16_t vertexBufferArrayDwOffset;		///< The vertex buffer table offset into the main buffer.
			uint16_t resourceArrayDwOffset;			///< The resource buffer table offset into the main buffer.
			uint16_t rwResourceArrayDwOffset;		///< The read/write resource buffer table offset into the main buffer.
			uint16_t samplerArrayDwOffset;			///< The sampler buffer table offset into the main buffer.
			uint16_t streamOutArrayDwOffset;		///< The stream out buffer table offset into the main buffer. This is only for the Geometry pipeline.

			// For each logical shader API slot, store either: an offset to a memory location, or a User Data (UD) SGPR where the resource should be set.
			// Note: if (item[i]&kResourceInUserDataSgpr) it's set directly into s[0:15] using PM4 packets, otherwise it's copied into the scratch buffer using the offset.
			uint16_t resourceDwOffset		[LightweightConstantUpdateEngine::kMaxResourceCount];		///< The start offset of a resource in the resource buffer table or user data.
			uint16_t rwResourceDwOffset		[LightweightConstantUpdateEngine::kMaxRwResourceCount];		///< The start offset of a resource in the read/write resource buffer table or user data.
			uint16_t samplerDwOffset		[LightweightConstantUpdateEngine::kMaxSamplerCount];		///< The start offset of a sampler in the sampler buffer table or user data.
			uint16_t constBufferDwOffset	[LightweightConstantUpdateEngine::kMaxConstantBufferCount];	///< The start offset of a constant buffer in the constant buffer table or user data.
			uint16_t vertexBufferDwOffset	[LightweightConstantUpdateEngine::kMaxVertexBufferCount];	///< The start offset of a vertex array in the vertex buffer table or user data.
			uint16_t streamOutDwOffset		[LightweightConstantUpdateEngine::kMaxStreamOutBufferCount];///< The start offset of a stream out buffer in the stream out buffer table or user data. This is only for the Geometry pipeline.

			uint8_t resourceSlotCount;			///< The number of resource slots used by the shader.
			uint8_t rwResourceSlotCount;		///< The number of rw resource slots used by the shader.
			uint8_t samplerSlotCount;			///< The number of sampler slots used by the shader.
			uint8_t constBufferSlotCount;		///< The number of constant buffer slots used by the shader.
			uint8_t vertexBufferSlotCount;		///< The number of vertex buffer slots used by the shader.

			uint8_t pad[1];

			
			/** @brief Initializes several resource slots that the shader uses.
			 */
			void initSupportedResourceCounts()
			{
				resourceSlotCount		= LightweightConstantUpdateEngine::kMaxResourceCount;
				rwResourceSlotCount		= LightweightConstantUpdateEngine::kMaxRwResourceCount;
				samplerSlotCount		= LightweightConstantUpdateEngine::kMaxSamplerCount;
				constBufferSlotCount	= LightweightConstantUpdateEngine::kMaxConstantBufferCount;
				vertexBufferSlotCount	= LightweightConstantUpdateEngine::kMaxVertexBufferCount;
			}

//			kShaderInputUsageImmAluFloatConst	// Immediate float const (scalar or vector). *Not Supported*
//			kShaderInputUsageImmAluBool32Const	// 32 immediate Booleans packed into one UINT. *Not Supported*
		};

			
		/** @brief Computes the maximum number of patches per thread-group for a LsShader/HsShader pair and returns a valid VGT and Patch count.
		 *
		 *	@param[out] outVgtPrimitiveCount	The VGT primitive count will be written here. Note that you must subtract 1 from this value before passing it to setVgtControl().
		 *	@param[out] outTgPatchCount			The number of computed patches per HS thread group will be written here. 
		 *	@param[in] desiredTgLdsSizeInBytes	The desired amount of HS-stage local data store to be allocated (the maximum size is 32K). The desired LDS size will be used if higher than the minimum requirement of the HsShader.
		 *	@param[in] desiredTgPatchCount		The desired patch count to use. This will be used if it is higher than the minimum requirement of the HsShader.
		 *	@param[in] localShader				The paired LsShader for the HsShader.
		 *	@param[in] hullShader				The HsShader to generate patch counts for.
		 */
		SCE_GNMX_EXPORT void computeTessellationTgParameters(uint32_t *outVgtPrimitiveCount, uint32_t* outTgPatchCount, uint32_t desiredTgLdsSizeInBytes, uint32_t desiredTgPatchCount, 
															 const Gnmx::LsShader* localShader, const Gnmx::HsShader* hullShader);

			
		/** @brief Fills the in/out InputResourceOffsets structure with data extracted from the GNMX shader struct (LS/HS/ES/VS/PS/CS Shader).
		 *
		 *	The shader code address and size are extracted from the GNMX shader. If the shader code is in Garlic memory, the CPU performance will not be optimal, and 
		 *	it is recommended to call this method before patching the shader code to be in GPU-friendly Garlic memory.
		 *
		 *	@param[out] outTable			The InputResourceOffsets table to which this function writes. Note that this value must not be <c>NULL</c>.
		 *	@param[in] shaderStage		The shader stage that the ShaderResourceOffsets table is being generated for.
		 *	@param[in] gnmxShaderStruct	The Gnmx shader class to be parsed. Note that the <c><i>shaderStage</i></c> and <c><i>gnmxShaderStruct</i></c> shader type must match.
		 *
		 *  @note To maintain high performance of the LCUE, it is only necessary to build the InputResourceOffsets table once. This table can be cached alongside the shader.
		 */
		SCE_GNMX_EXPORT void generateInputResourceOffsetTable(InputResourceOffsets* outTable, sce::Gnm::ShaderStage shaderStage, const void* gnmxShaderStruct);


		/** @brief Fills the in/out InputResourceOffsets structure with data extracted from the GNMX shader struct (LS/HS/ES/VS/PS/CS Shader), with the ability to remap input semantics.
		*
		*	The shader code address and size are extracted from the GNMX shader. If the shader code is in Garlic memory, the CPU performance will not be optimal, and
		*	it is recommended to call this method before patching the shader code to be in GPU-friendly Garlic memory.
		*
		*	@param[out] outTable						The InputResourceOffsets table to which this function writes. Note that this value must not be <c>NULL</c>.
		*	@param[in] shaderStage						The shader stage that the ShaderResourceOffsets table is being generated for.
		*	@param[in] gnmxShaderStruct					The Gnmx shader class to be parsed. Note that the <c><i>shaderStage</i></c> and <c><i>gnmxShaderStruct</i></c> shader type must match.
		*	@param[in] semanticRemapTable				A table specifying how input semantics should be remapped for the fetch shader.
		*	@param[in] numElementsInSemanticRemapTable	The number of elements in <c><i>semanticRemapTable</i></c>.
		* 
		*  @note To maintain high performance of the LCUE, it is only necessary to build the InputResourceOffsets table once. This table can be cached alongside the shader.
		*/
		SCE_GNMX_EXPORT void generateInputResourceOffsetTable(InputResourceOffsets* outTable, sce::Gnm::ShaderStage shaderStage, const void* gnmxShaderStruct, const void *semanticRemapTable, const uint32_t numElementsInSemanticRemapTable);


		/** @brief Fills the in/out InputResourceOffsets structure with data extracted from the GNMX shader struct (CsVs Shader).
		 *
		 *	The shader code address and size are extracted from the GNMX shader. If the shader code is in Garlic, the CPU performance will not be optimal, and
		 *	it is recommended to call this method before patching the shader code to be in GPU-friendly Garlic memory.
		 *
		 *	@param[out] CsOutTable		The Async Cs shader InputResourceOffsets table to which this function writes. Note that this value must not be <c>NULL</c>.
		 *	@param[out] VsOutTable		The Vs shader InputResourceOffsets table that will be written to. Note that this value must not be <c>NULL</c>.
		 *	@param[in] gnmxShaderStruct The Gnmx shader class to be parsed. Note that the <c><i>shaderStage</i></c> and <c><i>gnmxShaderStruct</i></c> shader type must match.
		 *
		 *  @note To maintain high performance of the LCUE, it is only necessary to build the InputResourceOffsets table once. This table can be cached alongside the shader.
		 */
		SCE_GNMX_EXPORT void generateInputResourceOffsetTableForDispatchDraw(InputResourceOffsets* CsOutTable, InputResourceOffsets* VsOutTable, const void* gnmxShaderStruct);


		/** @brief Fills the in/out InputResourceOffsets structure with data extracted from the GNMX shader struct (CsVs Shader), with the ability to remap input semantics.
		*
		*	The shader code address and size are extracted from the GNMX shader. If the shader code is in Garlic, the CPU performance will not be optimal, and
		*	it is recommended to call this method before patching the shader code to be in GPU-friendly Garlic memory.
		*
		*	@param[out] CsOutTable						The Async Cs shader InputResourceOffsets table to which this function writes. Note that this value must not be <c>NULL</c>.
		*	@param[out] VsOutTable						The Vs shader InputResourceOffsets table to which this function writes. Note that this value must not be <c>NULL</c>.
		*	@param[in] gnmxShaderStruct					The Gnmx shader class to be parsed. Note that the <c><i>shaderStage</i></c> and <c><i>gnmxShaderStruct</i></c> shader type must match.
		*	@param[in] semanticRemapTable				A table specifying how input semantics should be remapped for the fetch shader.
		*	@param[in] numElementsInSemanticRemapTable	The number of elements in <c><i>semanticRemapTable</i></c>.
		*
		*  @note To maintain high performance of the LCUE, it is only necessary to build the InputResourceOffsets table once. This table can be cached alongside the shader.
		*/
		SCE_GNMX_EXPORT void generateInputResourceOffsetTableForDispatchDraw(InputResourceOffsets* CsOutTable, InputResourceOffsets* VsOutTable, const void* gnmxShaderStruct, const void *semanticRemapTable, const uint32_t numElementsInSemanticRemapTable);


		/** @brief Fills the in/out InputResourceOffsets structure with data extracted from the GNMX shader structure (LS/HS/ES/VS/PS/CS Shader).
		*
		*  If the shader code is in Garlic, the CPU performance will not be optimal, and it is recommended to call this method before patching the shader code to be in GPU-friendly Garlic memory.
		*
		*  @param[out] outTable							The InputResourceOffsets table to which this function writes. Note that this value must not be <c>NULL</c>.
		*  @param[in] shaderStage						The shader stage that the ShaderResourceOffsets table is being generated for.
		*  @param[in] gnmxShaderStruct					The Gnmx shader class to be parsed. Note that the <c><i>shaderStage</i></c> and <c><i>gnmxShaderStruct</i></c> shader type must match.
		*  @param[in] shaderCode						A pointer to the beginning of the shader microcode.
		*  @param[in] shaderCodeSizeInBytes				The size of the shader microcode in bytes.
		*  @param[in] isSrtUsed							A flag that specifies if the shader uses SRTs (see Gnmx::ShaderCommonData()).
		*  @param[in] isDispatchDraw					A flag that specifies if the shader is part of a CsVs shader stage.
		*  @param[in] semanticRemapTable				A table specifying how input semantics should be remapped for the fetch shader.
		*  @param[in] numElementsInSemanticRemapTable	The number of elements in <c><i>semanticRemapTable</i></c>.
		*
		*  @note To maintain high performance of the LCUE, it is only necessary to build the InputResourceOffsets table once. This table can be cached alongside the shader.
		*/
		SCE_GNMX_EXPORT void generateInputResourceOffsetTable(InputResourceOffsets* outTable, sce::Gnm::ShaderStage shaderStage, const void* gnmxShaderStruct, const void* shaderCode, uint32_t shaderCodeSizeInBytes,
															  bool isSrtUsed, bool isDispatchDraw, const void *semanticRemapTable, const uint32_t numElementsInSemanticRemapTable);


		SCE_GNM_API_DEPRECATED_MSG("Please use generateInputResourceOffsetTable(InputResourceOffsets* outTable, sce::Gnm::ShaderStage shaderStage, const void* gnmxShaderStruct, const void* shaderCode, uint32_t shaderCodeSizeInBytes, bool isSrtUsed, bool isDispatchDraw, const void *semanticRemapTable, const uint32_t numElementsInSemanticRemapTable) instead. shaderCodeSizeInBytes now take an unsigned int")
		SCE_GNMX_EXPORT void generateInputResourceOffsetTable(InputResourceOffsets* outTable, sce::Gnm::ShaderStage shaderStage, const void* gnmxShaderStruct, const void* shaderCode, int32_t shaderCodeSizeInBytes,
															  bool isSrtUsed, bool isDispatchDraw, const void *semanticRemapTable, const uint32_t numElementsInSemanticRemapTable)
		{
			generateInputResourceOffsetTable(outTable, shaderStage, gnmxShaderStruct, shaderCode, static_cast<uint32_t>(shaderCodeSizeInBytes), isSrtUsed, isDispatchDraw, semanticRemapTable, numElementsInSemanticRemapTable);
		}


		class BaseConstantUpdateEngine;

		/** @brief Defines a function that is called when the LCUE main resource buffer runs out of space.
		 *
		 *  @param[in]	lwcue				An instance of the LCUE calling the callback.
		 *  @param[in]	sizeInDwords		The size of space requested for the next allocation in <c>DWORD</c>s.
		 *  @param[out] resultSizeInDwords	The actual size of memory allocation returned in <c>DWORD</c>s.
		 *  @param[in]	userData			The user data passed to the callback function.
		 *
		 *  @return A pointer to a new resource buffer allocation when space has run out.
		 */
		typedef uint32_t* (*AllocResourceBufferCallback)(BaseConstantUpdateEngine* lwcue, uint32_t sizeInDwords, uint32_t* resultSizeInDwords, void* userData);


		/** @brief Represents a callback function pointer and data for the out-of-memory resource callback. */
		class ResourceBufferCallback
		{
		public:
			AllocResourceBufferCallback m_func;		///< A function to call when the resource buffer runs out of space.
			void*						m_userData;	///< The user defined data.
		};


		/** @brief The base class for the Lightweight Constant Update Engine */
		class SCE_GNMX_EXPORT BaseConstantUpdateEngine
		{
		public:

			/** @brief Enables/disables the prefetch of the shader code.
			 *
			 *	@param[in] enable	A flag that specifies whether to enable the prefetch of the shader code.
		 	 *
			 *	@note Shader code prefetching is enabled by default.
				@note  The implementation of the shader prefetching feature uses the prefetchIntoL2() function that the Gnm library provides. See the notes for <c>prefetchIntoL2()</c> regarding a potential stall of the command processor when multiple DMAs are in flight.
			 *  @see  DrawCommandBuffer::prefetchIntoL2(), DispatchCommandBuffer::prefetchIntoL2()
			 */
			SCE_GNM_FORCE_INLINE void setShaderCodePrefetchEnable(bool enable)
			{
				m_prefetchShaderCode = enable;
			}


			/** @brief Sets the address to the global resource table. This is a collection of V#'s which point to global buffers for various shader tasks.
			 *
			 *  @param[in] globalResourceTableAddr	A pointer to the global resource table in memory.
			 *
			 *  @note The address specified in globalResourceTableAddr needs to be in GPU-visible memory and must be set before calling setGlobalDescriptor().
			 */
			SCE_GNM_FORCE_INLINE void setGlobalResourceTableAddr(void* globalResourceTableAddr)
			{
				m_globalInternalResourceTableAddr = (Gnm::Buffer*)globalResourceTableAddr;
			}


			/** @brief Binds a function for the LCUE to call when the main resource buffer runs out of memory.
			 *
			 *  @param[in] callbackFunction A function to call when the resource buffer runs out of memory.
			 *  @param[in] userData			The user data passed to <c><i>callbackFunction</i></c>.
			 */
			SCE_GNM_FORCE_INLINE void setResourceBufferFullCallback(AllocResourceBufferCallback callbackFunction, void* userData)
			{
				m_resourceBufferCallback.m_func = callbackFunction;
				m_resourceBufferCallback.m_userData = userData;
			}


			/** @brief Binds a function for the LCUE to call when a resource buffer error occurs.
			 *
			 *  @param[in] callbackFunction A function to call when a resource buffer error occurs.
			 *  @param[in] userData			The user data passed to <c><i>callbackFunction</i></c>.
			 */
			SCE_GNM_FORCE_INLINE void setResourceBufferErrorCallback(LightweightConstantUpdateEngine::lwcueValidationErrorBufferCallback callbackFunction, void* userData)
			{
				m_resourceErrorCallback.m_func = callbackFunction;
				m_resourceErrorCallback.m_userData = userData;
			}


			/** @brief Gets the number of <c>DWORD</c>s available in the currently bound resource buffer. 
			 *
			 *  @return The remaining space in <c>DWORD</c>s.
			 */
			SCE_GNM_FORCE_INLINE uint32_t getRemainingResourceBufferSpaceInDwords(void)
			{
				return static_cast<uint32_t>(m_bufferCurrentEnd - m_bufferCurrent);
			}

			/** @brief Gets the number of <c>DWORD</c>s that have been used in the currently bound resource buffer.
			 *
			 *  @return The currently used space in <c>DWORD</c>s.
			 */
			SCE_GNM_FORCE_INLINE uint32_t getUsedResourceBufferSpaceInDwords(void)
			{
				return static_cast<uint32_t>(m_bufferCurrent - m_bufferCurrentBegin);
			}


			/** @brief Sets an entry in the global resource table.
			 *
			 *  @param[in] resourceType	The global resource type to bind a buffer for. Each global resource type has its own entry in the global resource table.
			 *  @param[in] buffer		The buffer to bind to the specified entry in the global resource table. The size of the buffer is global-resource-type-specific.
			 *
			 *  @note				This function modifies the global resource table. It is not safe to modify the global resource table unless the GPU is idle.
			 */
			void setGlobalDescriptor(Gnm::ShaderGlobalResourceType resourceType, const Gnm::Buffer* buffer);

#if !defined(DOXYGEN_IGNORE)

			SCE_GNM_FORCE_INLINE bool resourceBufferHasSpace(uint32_t sizeInDwords)
			{
				uint32_t remainingSizeInDwords = static_cast<uint32_t>(m_bufferCurrentEnd - m_bufferCurrent);
				return (sizeInDwords <= remainingSizeInDwords);
			}

			uint32_t* reserveResourceBufferSpace(uint32_t sizeInDwords);
			void init(uint32_t** resourceBuffersInGarlic, int32_t resourceBufferCount, uint32_t resourceBufferSizeInDwords, void* globalInternalResourceTableAddr);
			void swapBuffers();

			ResourceBufferCallback m_resourceBufferCallback;								// A callback that the system invokes when the resource buffer runs out of space.
			LightweightConstantUpdateEngine::ResourceErrorCallback m_resourceErrorCallback;	 				// A callback that the system invokes when a resource-binding error occurs.

			uint32_t*	m_bufferCurrent;															// The current pointer inside the buffer (the start of new allocations).
			uint32_t*	m_bufferCurrentBegin;														// The beginning of the current buffer used to store shader resource data.
			uint32_t*	m_bufferCurrentEnd;															// The end of the current buffer used to store shader resource data.

			uint32_t*	m_bufferBegin	[LightweightConstantUpdateEngine::kMaxResourceBufferCount];	// The beginning of each buffer used to store shader resource data.
			uint32_t*	m_bufferEnd		[LightweightConstantUpdateEngine::kMaxResourceBufferCount];	// The end of each buffer used to store shader resource data.

			sce::Gnm::Buffer*	m_globalInternalResourceTableAddr;		// The cached address to the global resource table shared among shader stages on a <c>preDraw()</c>/<c>preDispatch()</c> basis.

			int32_t		m_bufferCount;																// The number of resource buffers in the N-buffer scheme.
			int32_t		m_bufferIndex;																// The index of the write buffer being used (N-buffer scheme).
			bool		m_prefetchShaderCode;					// A flag that specifies whether to have the LCUE warm up the L2 Cache by prefetching the shaders in <c>preDraw()</c>/<c>preDispatch()</c>.

			uint8_t		m_pad[7];
#endif // !defined(DOXYGEN_IGNORE)
		};
	} // Gnmx
} // sce

#endif // _SCE_GNMX_LWCUE_BASE_H
