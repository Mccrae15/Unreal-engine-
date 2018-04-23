// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4Time.h"
#include "Misc/Timespan.h"
#include "Misc/DateTime.h"
#include <kernel.h>
#include <rtc.h>

double FPS4Time::InvFrequency = 0.0000000006274579;

namespace /*unnamed*/{
	inline void GetTimeInline(const SceRtcDateTime& DateTime, int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec)
	{
		Year = DateTime.year;
		Month = DateTime.month;
		Day = DateTime.day;
		Hour = DateTime.hour;
		Min = DateTime.minute;
		Sec = DateTime.second;
		MSec = DateTime.microsecond / 1000;
		FTimespan hms(Hour, Min, Sec);
		FDateTime ymd(Year, Month, Day);
		FDateTime Date = ymd + hms;
		DayOfWeek = static_cast<int32>(Date.GetDayOfWeek());
	}
}


double FPS4Time::InitTiming()
{	
	uint64 Frequency = sceKernelGetProcessTimeCounterFrequency();
	InvFrequency = 1.0 / (double)Frequency;
	SecondsPerCycle = InvFrequency * (1 << CycleShift);
	SecondsPerCycle64 = InvFrequency;
	return FPlatformTime::Seconds();
}


void FPS4Time::SystemTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec )
{
	//https://ps4.scedev.net/resources/documents/SDK/3.500/Rtc-Reference/0005.html
	SceRtcDateTime LocalTime;
	sceRtcGetCurrentClockLocalTime(&LocalTime);

	GetTimeInline(LocalTime, Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
}


void FPS4Time::UtcTime( int32& Year, int32& Month, int32& DayOfWeek, int32& Day, int32& Hour, int32& Min, int32& Sec, int32& MSec )
{
	//https://ps4.scedev.net/resources/documents/SDK/3.500/Rtc-Reference/0003.html
	//current time (UTC) in Tick format
	SceRtcTick Ticks;
	sceRtcGetCurrentTick(&Ticks);

	//https://ps4.scedev.net/resources/documents/SDK/3.500/Rtc-Reference/0015.html
	//Convert SceRtcTick to SceRtcDateTime
	SceRtcDateTime UniversalTime;
	sceRtcSetTick(&UniversalTime, &Ticks);

	GetTimeInline(UniversalTime, Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec);
}
