// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// Module includes
#include "OnlineSharingFacebook.h"
#include "OnlineIdentityFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"

FOnlineSharingFacebook::FOnlineSharingFacebook(FOnlineSubsystemFacebook* InSubsystem)
	: FOnlineSharingFacebookCommon(InSubsystem)
{
}

FOnlineSharingFacebook::~FOnlineSharingFacebook()
{
}

bool FOnlineSharingFacebook::RequestNewReadPermissions(int32 LocalUserNum, EOnlineSharingCategory NewPermissions)
{
	/** NYI */
	ensure((NewPermissions & ~EOnlineSharingCategory::ReadPermissionMask) == EOnlineSharingCategory::None);
	TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, false);
	return false;
}

bool FOnlineSharingFacebook::RequestNewPublishPermissions(int32 LocalUserNum, EOnlineSharingCategory NewPermissions, EOnlineStatusUpdatePrivacy Privacy)
{
	/** NYI */
	ensure((NewPermissions & ~EOnlineSharingCategory::PublishPermissionMask) == EOnlineSharingCategory::None);

	bool bTriggeredRequest = false;
	TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, false);
	return bTriggeredRequest;
}

bool FOnlineSharingFacebook::ShareStatusUpdate(int32 LocalUserNum, const FOnlineStatusUpdate& StatusUpdate)
{
	/** NYI */
	bool bTriggeredRequest = false;
	TriggerOnSharePostCompleteDelegates(LocalUserNum, false);
	return bTriggeredRequest;
}

bool FOnlineSharingFacebook::ReadNewsFeed(int32 LocalUserNum, int32 NumPostsToRead)
{
	/** NYI */
	bool bTriggeredRequest = false;
	TriggerOnReadNewsFeedCompleteDelegates(LocalUserNum, false);
	return bTriggeredRequest;
}


