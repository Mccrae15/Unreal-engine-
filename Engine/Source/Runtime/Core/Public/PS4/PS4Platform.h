// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * PS4 specific types
 */
struct FPS4Types : public FGenericPlatformTypes
{
	typedef unsigned long long	SIZE_T;
	typedef long long			SSIZE_T;
	typedef char16_t			CHAR16;
};


typedef FPS4Types FPlatformTypes;


// Platform defines, overrides from the default (except first 3)
#define PLATFORM_DESKTOP									0
#define PLATFORM_64BITS										1
#define PLATFORM_LITTLE_ENDIAN								1
#define PLATFORM_SUPPORTS_UNALIGNED_INT_LOADS				1
#define PLATFORM_SUPPORTS_PRAGMA_PACK						1
#define PLATFORM_USE_LS_SPEC_FOR_UNICODE					1
#define PLATFORM_HAS_BSD_TIME								0
#define PLATFORM_USE_PTHREADS								1
#define PLATFORM_MAX_FILEPATH_LENGTH						PATH_MAX
#define PLATFORM_SUPPORTS_TEXTURE_STREAMING					1
#define PLATFORM_HAS_NO_GETHOSTBYNAME						1
#define PLATFORM_REQUIRES_FILESERVER						1
#define PLATFORM_ENABLE_VECTORINTRINSICS					1
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_GETHOSTNAME			0
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_GETADDRINFO			0
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_SELECT				0
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_IOCTL				0
#define PLATFORM_HAS_BSD_SOCKET_FEATURE_MSG_DONTWAIT		1
#define PLATFORM_SUPPORTS_STACK_SYMBOLS						1
#define PLATFORM_HAS_128BIT_ATOMICS							1
#define PLATFORM_USES_FIXED_RHI_CLASS						1
#define PLATFORM_RHITHREAD_DEFAULT_BYPASS					0
#define PLATFORM_USES_STACKBASED_MALLOC_CRASH				1

//This is a workaround for a compiler bug that should be removed. Tracked in UE-42829.
#define PLATFORM_VECTOR_CUBIC_INTERP_SSE					1

// Function type macros.
#if UE_BUILD_DEBUG
#define FORCEINLINE		inline								/* Don't force code to be inlined in debug builds */
#else
#define FORCEINLINE		inline __attribute__((__always_inline__))		/* Force code to be inline */
#endif
#define FORCENOINLINE	__attribute__((noinline))			/* Force code to NOT be inline */

#define FUNCTION_CHECK_RETURN_END __attribute__ ((warn_unused_result))	/* Warn that callers should not ignore the return value. */
#define FUNCTION_NO_RETURN_END __attribute__ ((noreturn))				/* Indicate that the function never returns. */

// Optimization macros
#define PRAGMA_DISABLE_OPTIMIZATION_ACTUAL _Pragma("clang optimize off")
#define PRAGMA_ENABLE_OPTIMIZATION_ACTUAL _Pragma("clang optimize on")

// Disable optimization of a specific function
#define DISABLE_FUNCTION_OPTIMIZATION	__attribute__((optnone))

// the default of inline makes them still undebuggable
#define FORCEINLINE_DEBUGGABLE_ACTUAL inline

#define ABSTRACT

// Alignment.
#define GCC_PACK(n)			__attribute__((packed,aligned(n)))
#define GCC_ALIGN(n)		__attribute__((aligned(n)))

#define MAX_PATH 1024


// Prefetch
#define PLATFORM_CACHE_LINE_SIZE	64

// operator new/delete operators
// As of 10.9 we need to use _NOEXCEPT & cxx_noexcept compatible definitions
#if __has_feature(cxx_noexcept)
#define OPERATOR_NEW_THROW_SPEC
#else
#define OPERATOR_NEW_THROW_SPEC throw (std::bad_alloc)
#endif
#define OPERATOR_DELETE_THROW_SPEC noexcept
#define OPERATOR_NEW_NOTHROW_SPEC  noexcept
#define OPERATOR_DELETE_NOTHROW_SPEC  noexcept

// Allocator selection

#define FORCE_ANSI_ALLOCATOR 0
#define USE_MALLOC_BINNED2 1

#ifndef PLATFORM_USES_FIXED_GMalloc_CLASS
#define PLATFORM_USES_FIXED_GMalloc_CLASS ((UE_BUILD_SHIPPING || UE_BUILD_TEST) && !FORCE_ANSI_ALLOCATOR && USE_MALLOC_BINNED2 && !FORCE_USE_STATS)
#endif