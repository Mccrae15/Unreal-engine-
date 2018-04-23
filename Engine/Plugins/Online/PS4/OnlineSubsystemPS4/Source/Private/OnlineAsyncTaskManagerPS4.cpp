// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineSubsystemPS4.h"


void FOnlineAsyncTaskManagerPS4::OnlineTick()
{
	check(PS4Subsystem);
	check(FPlatformTLS::GetCurrentThreadId() == OnlineThreadId);
}
