// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformAtomics.h"
#include "CoreTypes.h"
#include "PS4/PS4SystemIncludes.h"
#include <sce_atomic.h>


/**
 * PS4 implementation of the Atomics OS functions.
 */
struct CORE_API FPS4PlatformAtomics
	: public FGenericPlatformAtomics
{
	static FORCEINLINE int8 InterlockedIncrement( volatile int8* Value )
	{
		return sceAtomicIncrement8AcqRel(Value) + 1;
	}

	static FORCEINLINE int16 InterlockedIncrement( volatile int16* Value )
	{
		return sceAtomicIncrement16AcqRel(Value) + 1;
	}

	static FORCEINLINE int32 InterlockedIncrement( volatile int32* Value )
	{
		return sceAtomicIncrement32AcqRel(Value) + 1;
	}

	static FORCEINLINE int64 InterlockedIncrement( volatile int64* Value )
	{
		return sceAtomicIncrement64AcqRel((volatile int64_t*)Value) + 1;
	}

	static FORCEINLINE int8 InterlockedDecrement( volatile int8* Value )
	{
		return sceAtomicDecrement8AcqRel(Value) - 1;
	}

	static FORCEINLINE int16 InterlockedDecrement( volatile int16* Value )
	{
		return sceAtomicDecrement16AcqRel(Value) - 1;
	}

	static FORCEINLINE int32 InterlockedDecrement( volatile int32* Value )
	{
		return sceAtomicDecrement32AcqRel(Value) - 1;
	}

	static FORCEINLINE int64 InterlockedDecrement( volatile int64* Value )
	{
		return sceAtomicDecrement64AcqRel((volatile int64_t*)Value) - 1;
	}

	static FORCEINLINE int8 InterlockedAdd( volatile int8* Value, int8 Amount )
	{
		return sceAtomicAdd8AcqRel(Value, Amount);
	}

	static FORCEINLINE int16 InterlockedAdd( volatile int16* Value, int16 Amount )
	{
		return sceAtomicAdd16AcqRel(Value, Amount);
	}

	static FORCEINLINE int32 InterlockedAdd( volatile int32* Value, int32 Amount )
	{
		return sceAtomicAdd32AcqRel(Value, Amount);
	}

	static FORCEINLINE int64 InterlockedAdd( volatile int64* Value, int64 Amount )
	{
		return sceAtomicAdd64AcqRel((volatile int64_t*)Value, Amount);
	}

	static FORCEINLINE int8 InterlockedExchange( volatile int8* Value, int8 Exchange )
	{
		return sceAtomicExchange8AcqRel(Value, Exchange);
	}

	static FORCEINLINE int16 InterlockedExchange( volatile int16* Value, int16 Exchange )
	{
		return sceAtomicExchange16AcqRel(Value, Exchange);
	}

	static FORCEINLINE int32 InterlockedExchange( volatile int32* Value, int32 Exchange )
	{
		return sceAtomicExchange32AcqRel(Value, Exchange);
	}

	static FORCEINLINE int64 InterlockedExchange( volatile int64* Value, int64 Exchange )
	{
		return sceAtomicExchange64AcqRel((volatile int64_t*)Value, Exchange);
	}

	static FORCEINLINE void* InterlockedExchangePtr( void** Dest, void* Exchange )
	{
		return (void*)sceAtomicExchange64AcqRel((volatile int64_t*)Dest, (int64_t)Exchange);
	}

	static FORCEINLINE int8 InterlockedCompareExchange( volatile int8* Dest, int8 Exchange, int8 Comperand )
	{
		return sceAtomicCompareAndSwap8AcqRel(Dest, Comperand, Exchange);
	}

	static FORCEINLINE int16 InterlockedCompareExchange( volatile int16* Dest, int16 Exchange, int16 Comperand )
	{
		return sceAtomicCompareAndSwap16AcqRel(Dest, Comperand, Exchange);
	}

	static FORCEINLINE int32 InterlockedCompareExchange( volatile int32* Dest, int32 Exchange, int32 Comperand )
	{
		return sceAtomicCompareAndSwap32AcqRel(Dest, Comperand, Exchange);
	}

	static FORCEINLINE int64 InterlockedCompareExchange( volatile int64* Dest, int64 Exchange, int64 Comperand )
	{
		return sceAtomicCompareAndSwap64AcqRel((volatile int64_t*)Dest, Comperand, Exchange);
	}

	static FORCEINLINE int8 AtomicRead(volatile const int8* Src)
	{
		return sceAtomicLoad8AcqRel((const volatile int8_t*)Src);
	}

	static FORCEINLINE int16 AtomicRead(volatile const int16* Src)
	{
		return sceAtomicLoad16AcqRel((const volatile int16_t*)Src);
	}

	static FORCEINLINE int32 AtomicRead(volatile const int32* Src)
	{
		return sceAtomicLoad32AcqRel((const volatile int32_t*)Src);
	}

	static FORCEINLINE int64 AtomicRead(volatile const int64* Src)
	{
		return sceAtomicLoad64AcqRel((const volatile int64_t*)Src);
	}

	static FORCEINLINE int8 AtomicRead_Relaxed(volatile const int8* Src)
	{
		return *Src;
	}

	static FORCEINLINE int16 AtomicRead_Relaxed(volatile const int16* Src)
	{
		return *Src;
	}

	static FORCEINLINE int32 AtomicRead_Relaxed(volatile const int32* Src)
	{
		return *Src;
	}

	static FORCEINLINE int64 AtomicRead_Relaxed(volatile const int64* Src)
	{
		return *Src;
	}

	static FORCEINLINE void AtomicStore(volatile int8* Src, int8 Val)
	{
		InterlockedExchange(Src, Val);
	}

	static FORCEINLINE void AtomicStore(volatile int16* Src, int16 Val)
	{
		InterlockedExchange(Src, Val);
	}

	static FORCEINLINE void AtomicStore(volatile int32* Src, int32 Val)
	{
		InterlockedExchange(Src, Val);
	}

	static FORCEINLINE void AtomicStore(volatile int64* Src, int64 Val)
	{
		InterlockedExchange(Src, Val);
	}

	static FORCEINLINE void AtomicStore_Relaxed(volatile int8* Src, int8 Val)
	{
		*Src = Val;
	}

	static FORCEINLINE void AtomicStore_Relaxed(volatile int16* Src, int16 Val)
	{
		*Src = Val;
	}

	static FORCEINLINE void AtomicStore_Relaxed(volatile int32* Src, int32 Val)
	{
		*Src = Val;
	}

	static FORCEINLINE void AtomicStore_Relaxed(volatile int64* Src, int64 Val)
	{
		*Src = Val;
	}

	DEPRECATED(4.19, "AtomicRead64 has been deprecated, please use AtomicRead's overload instead")
	static FORCEINLINE int64 AtomicRead64(volatile const int64* Src)
	{
		return AtomicRead(Src);
	}

	static FORCEINLINE bool InterlockedCompareExchange128( volatile FInt128* Dest, const FInt128& Exchange, FInt128* Comparand )
	{
		return sceAtomicCompareAndSwap128ByPointerAcqRel((volatile void*)Dest, (void*)Comparand, (void*)&Exchange, (void*)Comparand );
	}

	static FORCEINLINE void AtomicRead128(const FInt128* Src, FInt128* OutResult)
	{
		sceAtomicLoad128_64AcqRel((volatile int64_t*)Src, (int64_t *)OutResult);
	}


	static FORCEINLINE void* InterlockedCompareExchangePointer( void** Dest, void* Exchange, void* Comperand )
	{
		return (void*)sceAtomicCompareAndSwap64AcqRel((volatile int64_t*)Dest, (int64_t)Comperand, (int64_t)Exchange);
	}

	static FORCEINLINE bool CanUseCompareExchange128()
	{
		return true;
	}
};



typedef FPS4PlatformAtomics FPlatformAtomics;
