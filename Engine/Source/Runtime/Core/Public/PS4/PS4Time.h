// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformTime.h"
#include "PS4/PS4SystemIncludes.h"

/**
 * PS4 implementation of the Time OS functions
 */
struct CORE_API FPS4Time : public FGenericPlatformTime
{
	enum
	{
		CycleShift = 10,
	};
	static double InitTiming();

	static FORCEINLINE double Seconds()
	{
		// @todo PS4: should we add some arbitrary large number like windows does? (look at values from this)
		//Cycles() is normalized to microseconds, so don't use that for 'Seconds' since it will roll over in an hour.
		uint64 Time = sceKernelGetProcessTimeCounter();
		return (double)Time * InvFrequency;
	}

	static FORCEINLINE uint32 Cycles()
	{
		uint64 Time = sceKernelGetProcessTimeCounter() >> CycleShift;
		return Time;
	}
	static FORCEINLINE uint64 Cycles64()
	{
		return sceKernelReadTsc();
	}

	static double InvFrequency;

	static void SystemTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec );
	static void UtcTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec );
};


typedef FPS4Time FPlatformTime;
