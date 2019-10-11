// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Clang/ClangPlatformMath.h"
#include "PS4/PS4SystemIncludes.h"
#include <x86intrin.h>
#include "Math/UnrealPlatformMathSSE.h"


/**
 * PS4 implementation of the Math OS functions.
 */
struct FPS4PlatformMath
	: public FClangPlatformMath
{
	static FORCEINLINE int32 TruncToInt(float F)
	{
		return (int32)truncf(F);
	}

	static FORCEINLINE float TruncToFloat(float F)
	{
		return truncf(F);
	}

	static FORCEINLINE int32 RoundToInt(float F)
	{
		// Note: the x2 is to workaround the rounding-to-nearest-even-number issue when the fraction is .5
		return _mm_cvt_ss2si(_mm_set_ss(F + F + 0.5f)) >> 1;
	}

	static FORCEINLINE float RoundToFloat(float F)
	{
		return (float)RoundToInt(F);
	}

	static FORCEINLINE int32 FloorToInt(float F)
	{
		return (int32)floorf(F);
	}

	static FORCEINLINE float FloorToFloat(float F)
	{
		return floorf(F);
	}

	static FORCEINLINE int32 CeilToInt(float F)
	{		
		return (int32)ceilf(F);
	}

	static FORCEINLINE float CeilToFloat(float F)
	{
		return ceilf(F);
	}

#if PLATFORM_ENABLE_VECTORINTRINSICS
	static FORCEINLINE float InvSqrt(float F)
	{
		return UnrealPlatformMathSSE::InvSqrt(F);
	}

	static FORCEINLINE float InvSqrtEst( float F )
	{
		return UnrealPlatformMathSSE::InvSqrtEst(F);
	}
#endif

	static FORCEINLINE uint64 CeilLogTwo64(uint64 Arg)
	{
		int64 Bitmask = ((int64)(CountLeadingZeros64(Arg) << 57)) >> 63;
		return (64 - CountLeadingZeros64(Arg - 1)) & (~Bitmask);
	}

	static FORCEINLINE uint32 FloorLogTwo64(uint64 V)
	{
		return (V == 0) ? 0 : (63 - __builtin_clzll(V));
	}

	static FORCEINLINE uint64 RoundUpToPowerOfTwo64(uint64 V)
	{
		return uint64(1) << CeilLogTwo64(V);
	}

	static FORCEINLINE uint64 RoundDownToPowerOfTwo64(uint64 V)
	{
		return uint64(1) << FloorLogTwo64(V);
	}
};


typedef FPS4PlatformMath FPlatformMath;
