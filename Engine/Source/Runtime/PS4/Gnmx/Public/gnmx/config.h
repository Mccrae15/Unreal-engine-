/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNMX_CONFIG_H
#define _SCE_GNMX_CONFIG_H

#ifndef DOXYGEN_IGNORE
//
//
// This file contains a set of user available #define allowing tuning of GfxContext
//
//



//-----------------------------------------------------------------------------
//
// Global Gnmx Configurations:
//
//-----------------------------------------------------------------------------


/** @brief If set, Gnmx will use the Gnm's Unsafe command buffer variants.
 *
 *  These variants do not check for space in the command buffer before writing commands.
 *  This can significantly increase CPU performance, but will clearly lead to out-of-bounds
 *  array accesses if used irresponsibly!
 *
 *  If set, the support for larger than 4MB command buffer through the GfxContext and through
 *  ComputeContext will be disabled.  Any system relying on the callback system will stop working.
 *
 *  Default value: 0
 */
#ifndef SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
#define SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS 0
#endif // SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS


/** @brief If set, Gnmx will set up the embedded constant buffer in an API slot as expected by the PSSLC-compiled shaders. 
 *   
 *  If you are using <c>orbis-wave-psslc</c> to compile your shaders, then set this option to 0 for a performance gain, unless compiled with <c>-disable-pic-for-literal-buffer</c> option.
 *  If you are using <c>orbis-psslc</c> to compile your shaders, then this option should be set to 1 (the default value.)
 * 
 *  Default value: 0 (changed from 1 to 0 with SDK 3.500)
 */
#ifndef SCE_GNMX_TRACK_EMBEDDED_CB
#define SCE_GNMX_TRACK_EMBEDDED_CB 0
#endif //SCE_GNMX_TRACK_EMBEDDED_CB

#define SCE_GNMX_ENABLE_CUE_V2 1

#endif /* DOXYGEN_IGNORE */

//-----------------------------------------------------------------------------
//
// GfxContext Configurations:
//
//-----------------------------------------------------------------------------

/** @brief If non-zero at compile time, Gnmx can record in memory the command buffer offset of the last draw or dispatch that completed successfully. 
 *  See sce::Gnmx::BaseGfxContext::initializeRecordLastCompletion() for details.
 *
 *  Default value: 0
 */
#ifndef SCE_GNMX_RECORD_LAST_COMPLETION
#define SCE_GNMX_RECORD_LAST_COMPLETION 0
#endif // SCE_GNMX_RECORD_LAST_COMPLETION

#ifndef DOXYGEN_IGNORE

//-----------------------------------------------------------------------------
//
// CUE v2 Configurations:
//
//-----------------------------------------------------------------------------



/** @brief If set, when a shader requests a resource which hasn't been set prior to the call to the draw function,
 *  an assert will trigger.
 *
 *  Modifying this value will affect the size of the ConstantUpdateEngine and GfxContext if SCE_GNM_CUE2_ENABLE_CACHE is set.
 *
 *  Default value: 1
 */
#ifndef SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES
#define SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES	1
#endif //!SCE_GNM_CUE2_CHECK_UNINITIALIZE_SHADER_RESOURCES


/** @brief If set, the contents of a shader's input usage table will be fully parsed to compute the required size of
 *  its Extended User Data (EUD) table.
 *
 *  This may be necessary when using older versions of PSSLC, which do not guarantee a sensible ordering of elements within the input usage table.
 *  If zero (default), the CUE assumes that the shader's input usage table is sorted by slot index, and can quickly compute the EUD table size by inspecting the final
 *  table element.
 *
 *  It is highly recommended that this value remain unset, unless an assert triggers that tells you otherwise.
 *
 *  Default value: 0
 */
#ifndef SCE_GNM_CUE2_PARSE_INPUTS_TO_COMPUTE_EUD_SIZE
#define SCE_GNM_CUE2_PARSE_INPUTS_TO_COMPUTE_EUD_SIZE		0
#endif //!SCE_GNM_CUE2_PARSE_INPUTS_TO_COMPUTE_EUD_SIZE


/** @brief If set, use an accelerator data structure (cache) for the shader's inputs.
 *
 *  Modifying this value will affect the size of ConstantUpdateEngine and GfxContext.
 *
 *  Default value: 1
 */
#ifndef SCE_GNM_CUE2_ENABLE_CACHE
#define SCE_GNM_CUE2_ENABLE_CACHE 1
#endif //!SCE_GNM_CUE2_ENABLE_CACHE


/** @brief If set, the EUD allocations are made in the CCB instead of DCB.
 *
 *  Default value: 0
 */
#ifndef SCE_GNM_CUE2_EUD_ALLOCATION_IN_CCB
#define SCE_GNM_CUE2_EUD_ALLOCATION_IN_CCB 0
#endif //!SCE_GNM_CUE2_EUD_ALLOCATION_IN_CCB


/** @brief If set, the EUD allocations are made with the allocateFromTheEndOfTheBuffer() instead of allocateFromCommandBuffer().
 *
 *  This will improve GPU performance and should be enabled unless you are relying on continuously writing into DCB/CCB (e.g. doing a continuous submit scheme).
 *
 *  Default value: 0
 */
#ifndef SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END
#define SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END 0
#endif //!SCE_GNM_CUE2_EUD_ALLOCATION_AT_THE_END


/** @brief If set, enable user allocator for the EUD allocations
 *
 *  Default value: 0
 */
#ifndef SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR
#define SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR 0
#endif //!SCE_GNM_CUE2_ENABLE_USER_EUD_ALLOCATOR

/** @brief If set, the CUE will detect and skip redundant set*Shader() calls.
 *  
 *  Default value: 0
 */
#ifndef SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER
#define SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER 0
#endif //!SCE_GNM_CUE2_ENABLE_SKIP_REDUNDANT_SHADER

/** @brief If set, the code will try to avoid context rolls when setting shaders
 *
 *  Default value: 1
 */
#ifndef SCE_GNM_CUE2_ENABLE_UPDATE_SHADER
#define SCE_GNM_CUE2_ENABLE_UPDATE_SHADER	1
#endif //!SCE_GNM_CUE2_ENABLE_UPDATE_SHADER


/** @brief If set, enable APIs to set entire resource table and bypass CUE's resource management
 *
 *  User resource tables is an alternative to managing shader resources through setTextures/setBuffers/setSaplers/etc APIs.
 *  Even though you can set all stage's textures, for example, with a single call to setTextures it's not much faster than setting each texture 
 *  individually because of the amount of CUE state being affected. 
 *  Using setUserResourceTable, on the other hand, will set all stage's textures and buffers at the cost of updating a pointer i.e. it's very fast.
 *  Conceptually and performance-wise, the User Resource Tables are similar to SRTs but do not require shader modifications. This is possible because, unlike the SRTs,
 *  the URTs rely on the resource layout already specified in the shader.
 *
 *  Modifying this value will affect the size of ConstantUpdateEngine and GfxContext.
 *
 *  Default value: 1
 */
#ifndef SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#define SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES 1
#endif //!SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES


/** @brief If set, disables the CUE's resource tracking and relies only on the user tables
 *
 *  Modifying this value will affect the size of ConstantUpdateEngine and GfxContext.
 *
 *  Default value: 0
 */
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#ifndef SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#define SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY 0
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_ONLY
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES


/** @brief If set, warn when a resource is being set while a resource table is active for the same resource type and shader stage
 *
 *  Default value: 1
 */
#if SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES
#ifndef SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET
#define SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET 1
#endif //!SCE_GNM_CUE2_USER_RESOURCE_TABLES_WARN_UNUSED_SET
#endif //SCE_GNM_CUE2_ENABLE_USER_RESOURCE_TABLES

/** @brief If set, enable the support for both SRT and global resources in shaders. 
 *  @note If you are using ConstantUpdateEngine::setInternalSrtBuffer() then you need to disable this option.
 *
 *  Default value: 1
 */
#ifndef SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS
#define SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS 1
#endif //!SCE_GNM_CUE2_ENABLE_MIXED_MODE_SRT_SHADERS

/** @brief If set (disabled by default), the CUE will skip binding the pixel usage table in preDraw()/preDispatchDraw().
*	This should be disabled if the application chooses to call Gnm::DrawCommandBuffer::setPsShaderUsage() manually.
*
*  Default: disabled
*/
#ifndef SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE
#define SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE 0
#endif //!SCE_GNM_CUE2_SKIP_VS_PS_SEMANTIC_TABLE



//-----------------------------------------------------------------------------
//
// Compatibility with old CUE:
//
//-----------------------------------------------------------------------------

/** @brief If set, binding a <c>NULL</c>pointer as a shader resource will be ignored.
 *
 *  This matches the default behavior of the original ConstantUpdateEngine, and may be required for compatibility.
 *  If zero, binding a <c>NULL</c> pointer to a resource tells the CUE it can stop tracking the resource slot,
 *  which can significantly improve performance. It is highly recommended that this value remains undefined,
 *  and that unused resources be unbound (set to <c>NULL</c>).
 *
 *  Default value: 0
 */
#ifndef SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL
#define SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL	0
#endif //!SCE_GNM_CUE2_IGNORE_SHADER_RESOURCES_SET_TO_NULL


/** @brief If set, all shader resource definitions are initialized to zero by default.
 *
 *  This matches the default behavior of the original ConstantUpdateEngine, and may be required for compatibility.
 *  If unset, the initial contents of shader resource definitions are undefined; attempts to use an unbound resource
 *  will trigger an assert.
 *
 *  It is highly recommended that this value remains undefined, and that all shader resources are properly initialized
 *  and bound prior to their first use.
 *
 *  Modifying this value will affect the size of the GfxContext if SCE_GNM_CUE2_ENABLE_CACHE is set.
 *
 *  Default value: 0
 */
#ifndef SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES
#define SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES	0
#endif //!SCE_GNM_CUE2_SUPPORT_UNINITIALIZE_SHADER_RESOURCES








//-----------------------------------------------------------------------------
//
// LCUE Configurations:
//
//-----------------------------------------------------------------------------

/** @brief If defined, (disabled by default), switches all sample Gnmx code to use the LightweightGfxContext.
 *  All subsequent shader resource bindings will be handled by the LCUE. 
 *  For more information, see the <em>Gnm Library Overview</em> and the <em>Gnm Library Reference</em> 
 *
 *  Default: disabled
 */
#ifndef SCE_GNMX_ENABLE_GFX_LCUE
#define SCE_GNMX_ENABLE_GFX_LCUE	0
#endif // SCE_GNMX_ENABLE_GFX_LCUE


/** @brief If defined (enabled by default), clears the HW KCache/L1/L2 caches at init time and at the end of each LCUE buffer swap in swapBuffers().
 *
 *  @note This will result in the flushing and invalidation of the GPU KCache/L1/L2 caches, which may slightly affect performance of any asynchronous compute jobs.
 *  In this case you can disable this feature, but it's advised that you manually flush and invalidate the GPU KCache/L1/L2 caches before the pending LCUE buffer
 *  is consumed by the GPU.
 *
 *  Default: enabled
 */
#ifndef SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE
#define SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE	1
#endif // SCE_GNM_LCUE_CLEAR_HARDWARE_KCACHE


/** @brief If defined (enabled by default), the Dispatch Draw feature will be enabled. 
 *
 *  @note Dispatch Draw requires additional CPU overhead in order to correctly bind and track state necessary for Dispatch Draw to work.
 *	In cases where Dispatch Draw is not requires, the feature can be globally disabled to help improve CPU performance.
 *
 *  Default: enabled
 */
#ifndef SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW
#define SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW	1
#endif // SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW


/** @brief If defined (enabled by default), the LCUE will reduce context rolls when setting PS, VS, GS and HS shaders 
 *  (but may incur additional CPU overhead).
 * 
 *  Default: enabled
 */
#ifndef SCE_GNM_LCUE_ENABLE_UPDATE_SHADER
#define SCE_GNM_LCUE_ENABLE_UPDATE_SHADER	1
#endif //!SCE_GNM_LCUE_ENABLE_UPDATE_SHADER


/** @brief If defined (disabled by default), the LCUE will cache the pixel usage table when preDraw()/preDispatchDraw() is called
 *  and only binds Gnm::DrawCommandBuffer::setPsShaderUsage() when cached state changes (but will incur additional CPU overhead).
 *  
 *  @note Only necessary to enable if Gnm::DrawCommandBuffer::setPsShaderUsage() context roll is affecting performance
 *  @note If SCE_GNM_LCUE_SKIP_VS_PS_SEMANTIC_TABLE is enabled, this flag will be ignored.
 *  @note To enable, you will need to recompile Gnmx library with this flag enabled.
 *
 *  Default: disabled
 */
#ifndef SCE_GNM_LCUE_ENABLE_CACHED_PS_SEMANTIC_TABLE
#define SCE_GNM_LCUE_ENABLE_CACHED_PS_SEMANTIC_TABLE	0
#endif // SCE_GNM_LCUE_ENABLE_CACHED_PS_SEMANTIC_TABLE


/** @brief If defined (disabled by default), the LCUE will skip binding the pixel usage table in preDraw()/preDispatchDraw().
 *	
 *  @note This should be enabled if the application chooses to call Gnm::DrawCommandBuffer::setPsShaderUsage() manually.
 *  @note To enable, you will need to recompile Gnmx library with this flag enabled.
 *
 *  Default: disabled
 */
#ifndef SCE_GNM_LCUE_SKIP_VS_PS_SEMANTIC_TABLE
#define SCE_GNM_LCUE_SKIP_VS_PS_SEMANTIC_TABLE	0
#endif // SCE_GNM_LCUE_SKIP_VS_PS_SEMANTIC_TABLE


/** @brief If defined (disabled by default), an assert will be thrown if a resource that is not used by the current shader is bound.
 *  By default the LCUE will skip resources if the binding does not exist in the ShaderResourceOffsetTable.  
 *  Use this assert to catch unused bindings your engine makes to reduce LCUE call overhead or to catch out of order bindings 
 *  that may occur before shader is set.
 * 
 *  @note To enable, you will need to recompile Gnmx library (Debug) with this flag enabled.
 *
 *  Default: disabled
 */
#ifndef SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING_ENABLED
#define SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING_ENABLED	0
#endif // SCE_GNM_LCUE_ASSERT_ON_MISSING_RESOURCE_BINDING_ENABLED


/** @brief If defined, checks if all resources expected by the current bound shaders are bound when a "draw", "dispatch" or "dispatchDraw" is issued.
 *	Will also check to ensure if the shader uses any API-slot over the maximum count configured to be handled by the LCUE.
 *
 *  @note To enable, you will need to recompile Gnmx library (Debug) with this flag enabled.
 *
 *  Default: disabled
 */
#ifndef SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED
#define SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED	0
#endif // SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED








//-----------------------------------------------------------------------------
//
// ComputeQueue Configurations:
//
//-----------------------------------------------------------------------------

/** If set, extra code will be added to potentially add nop if needed.
  * Unless the root queue is used outside of this class, this test is unnecessary.
  *
  * Default value: 0
  */
#ifndef SCE_GNM_SUPPORT_FOR_NOP_AT_END_OF_COMPUTEQUEUE
#define SCE_GNM_SUPPORT_FOR_NOP_AT_END_OF_COMPUTEQUEUE 0
#endif // SCE_GNM_SUPPORT_FOR_NOP_AT_END_OF_COMPUTEQUEUE

#endif /* DOXYGEN_IGNORE */

#endif /* _SCE_GNMX_CONFIG_H */
