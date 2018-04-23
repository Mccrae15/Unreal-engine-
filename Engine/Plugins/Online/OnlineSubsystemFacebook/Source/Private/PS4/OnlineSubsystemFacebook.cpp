// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"
#include "OnlineIdentityFacebook.h"
#include "OnlineSharingFacebook.h"

FOnlineSubsystemFacebook::FOnlineSubsystemFacebook()
{
}

FOnlineSubsystemFacebook::FOnlineSubsystemFacebook(FName InInstanceName)
	: FOnlineSubsystemFacebookCommon(InInstanceName)
{
}

FOnlineSubsystemFacebook::~FOnlineSubsystemFacebook()
{
}

bool FOnlineSubsystemFacebook::Init()
{
	if (FOnlineSubsystemFacebookCommon::Init())
	{
		FacebookIdentity = MakeShareable(new FOnlineIdentityFacebook(this));
		FacebookSharing = MakeShareable(new FOnlineSharingFacebook(this));
		return true;
	}

	return false;
}

bool FOnlineSubsystemFacebook::Shutdown()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemFacebook::Shutdown()"));
	return FOnlineSubsystemFacebookCommon::Shutdown();
}

bool FOnlineSubsystemFacebook::IsEnabled() const
{
	return FOnlineSubsystemFacebookCommon::IsEnabled();
}
