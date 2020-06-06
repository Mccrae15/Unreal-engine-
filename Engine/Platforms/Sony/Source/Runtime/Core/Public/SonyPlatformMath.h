// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Clang/ClangPlatformMath.h"
#include "SonySystemIncludes.h"
#include <x86intrin.h>
#include "Math/UnrealPlatformMathSSE4.h"


/**
 * Sony implementation of the Math OS functions.
 */
struct FSonyPlatformMath	: public TUnrealPlatformMathSSE4Base<FClangPlatformMath>
{
	static FORCEINLINE uint64 CeilLogTwo64(uint64 Arg)
	{
		int64 Bitmask = ((int64)(CountLeadingZeros64(Arg) << 57)) >> 63;
		return (64 - CountLeadingZeros64(Arg - 1)) & (~Bitmask);
	}

	static FORCEINLINE uint32 FloorLogTwo64(uint64 V)
	{
		return (V == 0) ? 0u : (63u - __builtin_clzll(V));
	}

	static FORCEINLINE uint64 RoundUpToPowerOfTwo64(uint64 V)
	{
		return uint64(1) << CeilLogTwo64(V);
	}

	static FORCEINLINE uint64 RoundDownToPowerOfTwo64(uint64 V)
	{
		return uint64(1) << FloorLogTwo64(V);
	}

	static FORCEINLINE int32 CountBits(uint64 Bits)
	{
		return __builtin_popcountll(Bits);
	}
};


typedef FSonyPlatformMath FPlatformMath;
