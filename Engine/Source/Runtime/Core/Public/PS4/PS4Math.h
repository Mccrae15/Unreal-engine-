// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformMath.h"
#include "PS4/PS4SystemIncludes.h"
#include <x86intrin.h>
#include "Math/UnrealPlatformMathSSE.h"


/**
 * PS4 implementation of the Math OS functions.
 */
struct FPS4PlatformMath
	: public FGenericPlatformMath
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

	/**
	 * Counts the number of leading zeros in the bit representation of the value
	 *
	 * @param Value the value to determine the number of leading zeros for
	 *
	 * @return the number of zeros before the first "on" bit
	 */
	static FORCEINLINE uint32 CountLeadingZeros(uint32 Value)
	{
		if (Value == 0)
		{
			return 32;
		}

		return __builtin_clz(Value);
	}

	/**
	 * Counts the number of trailing zeros in the bit representation of the value
	 *
	 * @param Value the value to determine the number of trailing zeros for
	 *
	 * @return the number of zeros after the last "on" bit
	 */
	static FORCEINLINE uint32 CountTrailingZeros(uint32 Value)
	{
		if (Value == 0)
		{
			return 32;
		}

		return __builtin_ctz(Value);
	}

	static FORCEINLINE uint64 CountTrailingZeros64(uint64 Value)
	{
		if (Value == 0)
		{
			return 64;
		}

		return __builtin_ctzll(Value);
	}

	static FORCEINLINE uint64 CountLeadingZeros64(uint64 Value)
	{
		if (Value == 0)
		{
			return 64;
		}

		return __builtin_clzll(Value);
	}

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
