// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineIdentityGoogle.h"
#include "OnlineSubsystemGooglePrivate.h"

FOnlineIdentityGoogle::FOnlineIdentityGoogle(FOnlineSubsystemGoogle* InSubsystem)
	: FOnlineIdentityGoogleCommon(InSubsystem)
{
}

bool FOnlineIdentityGoogle::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	// Not implemented
	FString ErrorStr = TEXT("NotImplemented");
	GoogleSubsystem->ExecuteNextTick([this, LocalUserNum, ErrorStr]()
	{
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, GetEmptyUniqueId(), ErrorStr);
	});
	return false;
}

bool FOnlineIdentityGoogle::Logout(int32 LocalUserNum)
{
	// Not implemented
	GoogleSubsystem->ExecuteNextTick([this, LocalUserNum]()
	{
		TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
	});
	return false;
}

