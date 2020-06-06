// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyCriticalSection.h"
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

FSonySystemWideCriticalSection::FSonySystemWideCriticalSection(const FString& InName, FTimespan InTimeout)
	: bIsLocked(false)
{
	check(InName.Len() > 0)
	check(InName.Len() < SONY_MAX_PATH)
	FCString::Strcpy(Name, SONY_MAX_PATH, *InName);

	FDateTime ExpireTime = FDateTime::UtcNow() + InTimeout;
	const float RetrySeconds = FMath::Min((float)InTimeout.GetTotalSeconds(), 0.25f);

	bIsLocked = TryLock(InName);

	while (!bIsLocked && (InTimeout.IsZero() || (FDateTime::UtcNow() < ExpireTime)))
	{
		FSonyPlatformProcess::Sleep(RetrySeconds);
		bIsLocked = TryLock(InName);
	}
}

FSonySystemWideCriticalSection::~FSonySystemWideCriticalSection()
{
	Release();
}

bool FSonySystemWideCriticalSection::IsValid() const
{
	return bIsLocked;
}

void FSonySystemWideCriticalSection::Release()
{
	if (bIsLocked)
	{
		FScopeLock Sentry(&ListLock);

		SystemWideCriticalSections.Remove(FString(Name));
		bIsLocked = false;
	}
}

bool FSonySystemWideCriticalSection::TryLock(const FString& InName)
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
