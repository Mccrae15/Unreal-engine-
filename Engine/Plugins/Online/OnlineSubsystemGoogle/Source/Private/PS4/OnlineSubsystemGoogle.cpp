// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGoogle.h"
#include "OnlineSubsystemGooglePrivate.h"
#include "OnlineIdentityGoogle.h"

FOnlineSubsystemGoogle::FOnlineSubsystemGoogle(FName InInstanceName)
	: FOnlineSubsystemGoogleCommon(InInstanceName)
{
}

FOnlineSubsystemGoogle::~FOnlineSubsystemGoogle()
{
}

bool FOnlineSubsystemGoogle::Init()
{
	if (FOnlineSubsystemGoogleCommon::Init())
	{
		GoogleIdentity = MakeShareable(new FOnlineIdentityGoogle(this));
		return true;
	}

	return false;
}

bool FOnlineSubsystemGoogle::Shutdown()
{
	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineSubsystemGoogle::Shutdown()"));
	return FOnlineSubsystemGoogleCommon::Shutdown();
}

