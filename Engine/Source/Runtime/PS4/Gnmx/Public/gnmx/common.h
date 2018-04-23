/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/
#if !defined(_SCE_GNMX_COMMON_H)
#define _SCE_GNMX_COMMON_H

#include <gnmx/config.h>

namespace sce
{
	namespace Gnm
	{
#if SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
		class UnsafeConstantCommandBuffer;
		class UnsafeDispatchCommandBuffer;
		class UnsafeDrawCommandBuffer;
#else // SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
		class ConstantCommandBuffer;
		class DispatchCommandBuffer;
		class DrawCommandBuffer;
#endif // SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
	}
	namespace Gnmx
	{
#if SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
		typedef Gnm::UnsafeConstantCommandBuffer GnmxConstantCommandBuffer; ///< Gnmx local typedef for Gnm's ConstantCommandBuffer
		typedef Gnm::UnsafeDispatchCommandBuffer GnmxDispatchCommandBuffer;	///< Gnmx local typedef for Gnm's DispatchCommandBuffer
		typedef Gnm::UnsafeDrawCommandBuffer     GnmxDrawCommandBuffer;		///< Gnmx local typedef for Gnm's DrawCommandBuffer
#else // SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
		typedef Gnm::ConstantCommandBuffer       GnmxConstantCommandBuffer; ///< Gnmx local typedef for Gnm's ConstantCommandBuffer
		typedef Gnm::DispatchCommandBuffer       GnmxDispatchCommandBuffer;	///< Gnmx local typedef for Gnm's DispatchCommandBuffer
		typedef Gnm::DrawCommandBuffer           GnmxDrawCommandBuffer;		///< Gnmx local typedef for Gnm's DrawCommandBuffer
#endif // SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS
	}
}

#if !defined(DOXYGEN_IGNORE)
#if defined(__ORBIS__) || defined(SCE_GNMX_LIBRARY_STATIC_EXPORT)
#	define SCE_GNMX_EXPORT   
#else
#	if defined( SCE_GNMX_DLL_EXPORT )
#		define SCE_GNMX_EXPORT __declspec( dllexport )
#	else
#		define SCE_GNMX_EXPORT __declspec( dllimport )
#	endif
#endif
#endif
#endif // _SCE_GNMX_COMMON_H
