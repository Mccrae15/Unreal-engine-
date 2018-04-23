// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformTLS.h"
#include "CoreTypes.h"
#include "PS4/PS4SystemIncludes.h"

/**
 * PS4 implementation of the TLS OS functions.
 */
struct CORE_API FPS4TLS
	: public FGenericPlatformTLS
{
	/**
	 * Returns the currently executing thread's id.
	 *
	 * @return The identifier of the current thread.
	 */
	static FORCEINLINE uint32 GetCurrentThreadId()
	{
		return scePthreadGetthreadid();
	}

	/**
	 * Allocates a thread local store slot.
	 *
	 * @return The index of the allocated slot.
	 */
	static FORCEINLINE uint32 AllocTlsSlot()
	{
		// create a thread key
		ScePthreadKey Key;
		scePthreadKeyCreate(&Key, nullptr);
		return (uint32)Key;
	}

	/**
	 * Sets a value in the specified TLS slot.
	 *
	 * @param SlotIndex the TLS index to store it in.
	 * @param Value the value to store in the slot.
	 */
	static FORCEINLINE void SetTlsValue(uint32 SlotIndex,void* Value)
	{
		// set the void* value
		ScePthreadKey Key = (ScePthreadKey)SlotIndex;
		scePthreadSetspecific(Key, Value);
	}

	/**
	 * Reads the value stored at the specified TLS slot.
	 *
	 * @param SlotIndex The index of the slot to read.
	 * @return the value stored in the slot.
	 */
	static FORCEINLINE void* GetTlsValue(uint32 SlotIndex)
	{
		// look up the void* value
		ScePthreadKey Key = (ScePthreadKey)SlotIndex;
		return scePthreadGetspecific(Key);
	}

	/**
	 * Frees a previously allocated TLS slot.
	 *
	 * @param SlotIndex the TLS index to store it in.
	 */
	static FORCEINLINE void FreeTlsSlot(uint32 SlotIndex)
	{
		// free the key
		ScePthreadKey Key = (ScePthreadKey)SlotIndex;
		scePthreadKeyDelete(Key);
	}
};


typedef FPS4TLS FPlatformTLS;
