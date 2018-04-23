// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4/PS4CriticalSection.h"
#include <libdbg.h>
#include <user_service.h>
#include <system_service.h>
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Misc/ScopeLock.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformProcess.h"

namespace
{
	FCriticalSection ListLock;
	TSet<FString> SystemWideCriticalSections;
}

FPS4SystemWideCriticalSection::FPS4SystemWideCriticalSection(const FString& InName, FTimespan InTimeout)
	: bIsLocked(false)
{
	check(InName.Len() > 0)
	check(InName.Len() < MAX_PATH)
	FCString::Strcpy(Name, MAX_PATH, *InName);

	FDateTime ExpireTime = FDateTime::UtcNow() + InTimeout;
	const float RetrySeconds = FMath::Min((float)InTimeout.GetTotalSeconds(), 0.25f);

	bIsLocked = TryLock(InName);

	while (!bIsLocked && (InTimeout.IsZero() || (FDateTime::UtcNow() < ExpireTime)))
	{
		FPS4PlatformProcess::Sleep(RetrySeconds);
		bIsLocked = TryLock(InName);
	}
}

FPS4SystemWideCriticalSection::~FPS4SystemWideCriticalSection()
{
	Release();
}

bool FPS4SystemWideCriticalSection::IsValid() const
{
	return bIsLocked;
}

void FPS4SystemWideCriticalSection::Release()
{
	if (bIsLocked)
	{
		FScopeLock Sentry(&ListLock);

		SystemWideCriticalSections.Remove(FString(Name));
		bIsLocked = false;
	}
}

bool FPS4SystemWideCriticalSection::TryLock(const FString& InName)
{
	FScopeLock Sentry(&ListLock);

	FString* CritSectionPtr = SystemWideCriticalSections.Find(InName);
	if (CritSectionPtr == nullptr)
	{
		SystemWideCriticalSections.Add(InName);
		return true;
	}

	return false;
}
