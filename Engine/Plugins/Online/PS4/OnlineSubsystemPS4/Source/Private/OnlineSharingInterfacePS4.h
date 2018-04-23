// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once


// Module includes
#include "OnlineSubsystemPS4Private.h"
#include "OnlineSharingInterface.h"
#include "OnlineSubsystemPS4Package.h"


/**
 * PS4 implementation of the Online Sharing Interface
 */
class FOnlineSharingPS4 : public IOnlineSharing
{
public:

	//~ Begin IOnlineSharing Interface
	virtual void RequestCurrentPermissions(int32 LocalUserNum, FOnRequestCurrentPermissionsComplete& CompletionDelegate) override;
	virtual void GetCurrentPermissions(int32 LocalUserNum, TArray<FSharingPermission>& OutPermissions) override;
	virtual bool ReadNewsFeed(int32 LocalUserNum, int32 NumPostsToRead) override;
	virtual bool RequestNewReadPermissions(int32 LocalUserNum, EOnlineSharingCategory NewPermissions) override;
	virtual bool ShareStatusUpdate(int32 LocalUserNum, const FOnlineStatusUpdate& StatusUpdate) override;
	virtual bool RequestNewPublishPermissions(int32 LocalUserNum, EOnlineSharingCategory NewPermissions, EOnlineStatusUpdatePrivacy Privacy) override;
	virtual EOnlineCachedResult::Type GetCachedNewsFeed(int32 LocalUserNum, int32 NewsFeedIdx, FOnlineStatusUpdate& OutNewsFeed) override;
	virtual EOnlineCachedResult::Type GetCachedNewsFeeds(int32 LocalUserNum, TArray<FOnlineStatusUpdate>& OutNewsFeeds) override;
	//~ End IOnlineSharing Interface

	
public:

	/**
	 * Constructor used to indicate which OSS we are a part of
	 */
	explicit FOnlineSharingPS4(class FOnlineSubsystemPS4* InSubsystem);
	
	/**
	 * Default destructor
	 */
	virtual ~FOnlineSharingPS4();

private:

	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

	/** List of current permissions and their states */
	TArray<FSharingPermission> CurrentPermissions;

	/** 
	 * Mapped array of status updates that have been read in. 
	 * Call ReadNewsFeed() first to fill out the list. 
	 */
	TMap<int32, TArray<FOnlineStatusUpdate>> PlayerStatusUpdates;

	/** Friendship to async tasks that help fill out data in interface */
	friend class FOnlineAsyncTaskPS4ReadNewsFeed;
};


typedef TSharedPtr<FOnlineSharingPS4, ESPMode::ThreadSafe> FOnlineSharingPS4Ptr;