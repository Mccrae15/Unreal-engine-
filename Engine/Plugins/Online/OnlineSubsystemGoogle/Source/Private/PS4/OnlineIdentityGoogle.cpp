// Copyright Epic Games, Inc. All Rights Reserved.

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
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdGoogle::EmptyId(), ErrorStr);
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

