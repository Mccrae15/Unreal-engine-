// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformProcess.h"

/**
 * Dummy process handle for platforms that use generic implementation.
 */
struct FProcHandle
	: public TProcHandle<void*, nullptr>
{
public:

	/** Default constructor. */
	FORCEINLINE FProcHandle()
		: TProcHandle()
	{ }

	/** Initialization constructor. */
	FORCEINLINE explicit FProcHandle( HandleType Other )
		: TProcHandle( Other )
	{ }
};


/**
 * PS4 implementation of the Process OS functions
 */
struct CORE_API FPS4PlatformProcess
	: public FGenericPlatformProcess
{
	static void SetThreadAffinityMask( uint64 AffinityMask );
	static void SetupRenderThread();

	static const TCHAR* ComputerName();
	static const TCHAR* UserName(bool bOnlyAlphaNumeric = true);
	static const TCHAR* BaseDir();
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static const TCHAR* ApplicationSettingsDir();
	static void Sleep( float Seconds );
	static bool CanLaunchURL(const TCHAR* URL);
	static void LaunchURL(const TCHAR* URL, const TCHAR* Parms, FString* Error);


	/**
	 * Sleep function that doesn't add to the thread's wait time.
	 *
	 * @param Seconds The number of seconds to sleep.
	 */
	static void SleepNoStats( float Seconds );

	static void SleepInfinite();

	static void* GetDllHandle( const TCHAR* Filename );
	static void FreeDllHandle( void* DllHandle );

	static FRunnableThread* CreateRunnableThread();
};


typedef FPS4PlatformProcess FPlatformProcess;
