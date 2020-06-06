// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PThreadCriticalSection.h"
#include "Misc/Timespan.h"
#include "HAL/PThreadRWLock.h"

/**
 * This is the PS4 version of a critical section. It uses an adaptive pthread mutex, which spins for a short
 * period when a lock is contended to avoid the kernel syscall in cases where the lock is not held for long.
 * The spin count is adaptive, based on measured spin times from previous lock attempts.
 */
class FSonyCriticalSection
{
	ScePthreadMutex Mutex;
	ScePthread LockingThread;

	/*
	 * Adaptive pthread mutexes are not recursive, so
	 * we need a counter to handle this ourselves.
	 */
	uint64 LockCount;

public:

	FORCEINLINE FSonyCriticalSection(void)
		: LockingThread(nullptr)
		, LockCount(0)
	{
		ScePthreadMutexattr MutexAttributes;
		scePthreadMutexattrInit(&MutexAttributes);
		scePthreadMutexattrSettype(&MutexAttributes, PTHREAD_MUTEX_ADAPTIVE_NP);
		scePthreadMutexInit(&Mutex, &MutexAttributes, nullptr);
		scePthreadMutexattrDestroy(&MutexAttributes);
	}

	FORCEINLINE ~FSonyCriticalSection(void)
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
	FSonyCriticalSection(const FSonyCriticalSection&);
	FSonyCriticalSection& operator=(const FSonyCriticalSection&);
};

/** System-Wide Critical Section for Sony (wrapper for FCriticalSection since Sony runs one process at a time) */
class CORE_API FSonySystemWideCriticalSection
{
public:
	/** Construct a named, system-wide critical section and attempt to get access/ownership of it */
	explicit FSonySystemWideCriticalSection(const FString& InName, FTimespan InTimeout = FTimespan::Zero());

	/** Destructor releases system-wide critical section if it is currently owned */
	~FSonySystemWideCriticalSection();

	/**
	* Does the calling thread have ownership of the system-wide critical section?
	*
	* @return True if obtained. WARNING: Returns true for an owned but previously abandoned locks so shared resources can be in undetermined states. You must handle shared data robustly.
	*/
	bool IsValid() const;

	/** Releases system-wide critical section if it is currently owned */
	void Release();

private:
	FSonySystemWideCriticalSection(const FSonySystemWideCriticalSection&);
	FSonySystemWideCriticalSection& operator=(const FSonySystemWideCriticalSection&);

	bool TryLock(const FString& InName);

private:
	TCHAR Name[SONY_MAX_PATH];
	bool bIsLocked;
};

typedef FSonyCriticalSection FCriticalSection;
typedef FSonySystemWideCriticalSection FSystemWideCriticalSection;
typedef FPThreadsRWLock FRWLock;
