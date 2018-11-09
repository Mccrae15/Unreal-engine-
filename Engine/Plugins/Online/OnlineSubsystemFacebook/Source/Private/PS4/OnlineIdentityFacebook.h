// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
 
// Module includes
#include "OnlineIdentityFacebookCommon.h"
#include "OnlineSharingFacebook.h"
#include "OnlineAccountFacebookCommon.h"
#include "OnlineSubsystemFacebookPackage.h"

class FOnlineSubsystemFacebook;


/** Xbox implementation of a Facebook user account */
class FUserOnlineAccountFacebook : public FUserOnlineAccountFacebookCommon
{
public:

	explicit FUserOnlineAccountFacebook(const FString& InUserId = FString(), const FString& InAuthTicket = FString())
		: FUserOnlineAccountFacebookCommon(InUserId, InAuthTicket)
	{
	}

	virtual ~FUserOnlineAccountFacebook()
	{
	}

	friend class FOnlineIdentityFacebook;
};

/**
 * Facebook service implementation of the online identity interface
 */
class FOnlineIdentityFacebook :
	public FOnlineIdentityFacebookCommon
{

public:

	//~ Begin IOnlineIdentity Interface	
	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	//~ End IOnlineIdentity Interface

public:

	/**
	 * Default constructor
	 */
	FOnlineIdentityFacebook(FOnlineSubsystemFacebook* InSubsystem);

	/**
	 * Destructor
	 */
	virtual ~FOnlineIdentityFacebook()
	{
	}
};

typedef TSharedPtr<FOnlineIdentityFacebook, ESPMode::ThreadSafe> FOnlineIdentityFacebookPtr;
