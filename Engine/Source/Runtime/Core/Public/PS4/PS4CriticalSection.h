// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PThreadCriticalSection.h"
#include "Misc/Timespan.h"
#include "HAL/PThreadRWLock.h"

/**
 * This is the PS4 version of a critical section. It uses an adaptive pthread mutex, which spins for a short
 * period when a lock is contended to avoid the kernel syscall in cases where the lock is not held for long.
 * The spin count is adaptive, based on measured spin times from previous lock attempts.
 */
class FPS4CriticalSection
{
	ScePthreadMutex Mutex;
	ScePthread LockingThread;

	/*
	 * Adaptive pthread mutexes are not recursive, so
	 * we need a counter to handle this ourselves.
	 */
	uint64 LockCount;

public:

	FORCEINLINE FPS4CriticalSection(void)
		: LockingThread(nullptr)
		, LockCount(0)
	{
		ScePthreadMutexattr MutexAttributes;
		scePthreadMutexattrInit(&MutexAttributes);
		scePthreadMutexattrSettype(&MutexAttributes, PTHREAD_MUTEX_ADAPTIVE_NP);
		scePthreadMutexInit(&Mutex, &MutexAttributes, nullptr);
		scePthreadMutexattrDestroy(&MutexAttributes);
	}

	FORCEINLINE ~FPS4CriticalSection(void)
	{
		scePthreadMutexDestroy(&Mutex);
	}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock(void)
	{
		if (scePthreadMutexLock(&Mutex) == SCE_OK)
		{
			LockingThread = scePthreadSelf();
		}

		LockCount++;
	}

	/**
	 * Attempt to take a lock and returns whether or not a lock was taken.
	 *
	 * @return true if a lock was taken, false otherwise.
	 */
	FORCEINLINE bool TryLock()
	{
		ScePthread ThisThread = scePthreadSelf();

		if (scePthreadMutexTrylock(&Mutex) == SCE_OK)
		{
			LockingThread = ThisThread;
			LockCount++;
			return true;
		}
		else if (LockingThread == ThisThread)
		{
			LockCount++;
			return true;
		}

		return false;
	}

	/**
	 * Releases the lock on the critical section
	 */
	FORCEINLINE void Unlock(void)
	{
		if ((--LockCount) == 0)
		{
			LockingThread = nullptr;
			scePthreadMutexUnlock(&Mutex);
		}
	}

private:
	FPS4CriticalSection(const FPS4CriticalSection&);
	FPS4CriticalSection& operator=(const FPS4CriticalSection&);
};

/** System-Wide Critical Section for PS4 (wrapper for FCriticalSection since PS4 runs one process at a time) */
class CORE_API FPS4SystemWideCriticalSection
{
public:
	/** Construct a named, system-wide critical section and attempt to get access/ownership of it */
	explicit FPS4SystemWideCriticalSection(const FString& InName, FTimespan InTimeout = FTimespan::Zero());

	/** Destructor releases system-wide critical section if it is currently owned */
	~FPS4SystemWideCriticalSection();

	/**
	* Does the calling thread have ownership of the system-wide critical section?
	*
	* @return True if obtained. WARNING: Returns true for an owned but previously abandoned locks so shared resources can be in undetermined states. You must handle shared data robustly.
	*/
	bool IsValid() const;

	/** Releases system-wide critical section if it is currently owned */
	void Release();

private:
	FPS4SystemWideCriticalSection(const FPS4SystemWideCriticalSection&);
	FPS4SystemWideCriticalSection& operator=(const FPS4SystemWideCriticalSection&);

	bool TryLock(const FString& InName);

private:
	TCHAR Name[PS4_MAX_PATH];
	bool bIsLocked;
};

typedef FPS4CriticalSection FCriticalSection;
typedef FPS4SystemWideCriticalSection FSystemWideCriticalSection;
typedef FPThreadsRWLock FRWLock;
