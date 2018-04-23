// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSharingInterfacePS4.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "WebApiPS4Task.h"

/**
 *	Task object sharing the user's status update
 */
class FOnlineAsyncTaskPS4ShareStatusUpdate : public FOnlineAsyncTaskPS4
{
public:

	/** Constructor */
	FOnlineAsyncTaskPS4ShareStatusUpdate(FOnlineSubsystemPS4* InSubsystem, int32 InLocalUserNum, int32 InLocalUserWebApiContext, const FString& InUserName, const FOnlineStatusUpdate& InStatusUpdate)
		:	FOnlineAsyncTaskPS4(InSubsystem)
		,	LocalUserNum(InLocalUserNum)
		,	WebApiTask(InLocalUserWebApiContext)
	{
		StartTask(InUserName, InStatusUpdate);
	}


	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4ShareStatusUpdate bWasSuccessful: %d"), bWasSuccessful);
	}

	void StartTask(const FString& UserName, const FOnlineStatusUpdate& StatusUpdate)
	{
		// Might just rename the activity feeds, and maybe put them in a separate folder with a matching name.

		// Load a template for the Json string we will send to the PSN WebApi
		// The 'Type' of the StatusUpdate controls which template we use.
		// This allows to have multiple activity feeds (for beating a level, a boss, consuming an item, etc.)
		FString JsonTemplateFullPath = FPaths::ProjectContentDir() + FString::Printf(TEXT("OSS/PS4/ActivityFeeds/%s.json"), *StatusUpdate.Type);
		FString JsonStr;
		if (!FFileHelper::LoadFileToString(JsonStr, *JsonTemplateFullPath))
		{
			UE_LOG_ONLINE(Error, TEXT("Could not read Json file for activity feed. File = %s"), *JsonTemplateFullPath);
			bWasSuccessful = false;
			bIsComplete = true;
			return;
		}

		// Replace the {{ONLINE_ID}} marker of ther user updating his status
		JsonStr.ReplaceInline(TEXT("{{ONLINE_ID}}"), *UserName, ESearchCase::CaseSensitive);
		
		// Replace the {{TITLE_ID}} marker
		const SceNpTitleId& NpTitleId = Subsystem->GetNpTitleId();
		JsonStr.ReplaceInline(TEXT("{{TITLE_ID}}"), ANSI_TO_TCHAR(NpTitleId.id), ESearchCase::CaseSensitive);
		
		// Replace the {{COMMENT}} marker. This is the actual message we want to send
		JsonStr.ReplaceInline(TEXT("{{COMMENT}}"), *StatusUpdate.Message, ESearchCase::CaseSensitive);

		UE_LOG_ONLINE(Display, TEXT("POST user feed body = \n%s"), *JsonStr);

		WebApiTask.GetTask().SetRequestBody(JsonStr);
		WebApiTask.GetTask().SetRequest(TEXT("activityFeed"), TEXT("/v1/users/me/feed"), SCE_NP_WEBAPI_HTTP_METHOD_POST);
		WebApiTask.StartBackgroundTask();
	}

	virtual void Tick() override
	{
		if (!bIsComplete)
		{
			if (WebApiTask.IsDone())
			{
				// Http Code 201 is success
				bWasSuccessful = WebApiTask.GetTask().GetHttpStatusCode() == 201;
				bIsComplete = true;
			}
		}
	}

	virtual void TriggerDelegates() override
	{
		FOnlineAsyncTaskPS4::TriggerDelegates();

		UE_LOG_ONLINE(Display, TEXT("POST USER FEED response body = %s"), *WebApiTask.GetTask().GetResponseBody());
		Subsystem->GetSharingInterface()->TriggerOnSharePostCompleteDelegates(LocalUserNum, bWasSuccessful);
	}


private:

	int32 LocalUserNum;
	FAsyncTask<FWebApiPS4Task> WebApiTask;
};


/**
 *	Task object reading the user's news feed
 */
class FOnlineAsyncTaskPS4ReadNewsFeed : public FOnlineAsyncTaskPS4
{
public:

	/** Constructor */
	FOnlineAsyncTaskPS4ReadNewsFeed(FOnlineSubsystemPS4* InSubsystem, FOnlineSharingPS4* InPS4Sharing, int32 InLocalUserNum, int32 InLocalUserWebApiContext, int InNumPostsToRead)
		:	FOnlineAsyncTaskPS4(InSubsystem)
		,	PS4Sharing(InPS4Sharing)
		,	LocalUserNum(InLocalUserNum)
		,	LocalUserWebApiContext(InLocalUserWebApiContext)
		,	NumPostsToRead(InNumPostsToRead)
		,	PostCounter(0)
		,	BlockCounter(0)
		,	CurrentWebApiTask()
	{
	}


	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4ReadNewsFeed bWasSuccessful: %d"), bWasSuccessful);
	}

	virtual void Tick() override
	{
		if (CurrentWebApiTask.IsValid() && CurrentWebApiTask->IsDone())
		{
			const int HttpStatusCode = CurrentWebApiTask->GetTask().GetHttpStatusCode();

			if (HttpStatusCode == 200)
			{
				// If we're here than we have a response from the server to read that contains the user posts
				// Consume the response from the WebApi request
				// It may contain dozens of post updates
				ConsumeResponse(CurrentWebApiTask->GetTask().GetResponseBody());

				// We are done with this task reading in a block of feeds
				CurrentWebApiTask = NULL;
				++BlockCounter;

				// Have we read in the max number of feed blocks?
				if (PostCounter == NumPostsToRead)
				{
					bWasSuccessful = true;
					bIsComplete = true;
					return;
				}
			}
			else if (HttpStatusCode == 204)
			{
				// We've run out of blocks to read. Success depends on reading at least one block.
				// Expected to read at least one block
				bWasSuccessful = (BlockCounter > 0);
				bIsComplete = true;
				return;
			}
			else
			{
				// We've hit some kind of error
				bWasSuccessful = false;
				bIsComplete = true;
				return;
			}
		}

		// Create a background test requesting a user feed if we don't already have on in motion
		if (!CurrentWebApiTask.IsValid())
		{
			FString Url = FString::Printf(TEXT("/v1/users/me/feed/%d"), BlockCounter);
			
			CurrentWebApiTask = MakeShareable(new BackgroundWebApiTask(LocalUserWebApiContext));
			CurrentWebApiTask->GetTask().SetRequest(TEXT("activityFeed"), Url, SCE_NP_WEBAPI_HTTP_METHOD_GET);
			CurrentWebApiTask->StartBackgroundTask();
		}
	}

	virtual void TriggerDelegates() override
	{
		FOnlineAsyncTaskPS4::TriggerDelegates();
		Subsystem->GetSharingInterface()->TriggerOnReadNewsFeedCompleteDelegates(LocalUserNum, bWasSuccessful);
	}

private:

	void ConsumeResponse(const FString& ResponseStr)
	{
		UE_LOG_ONLINE(Display, TEXT("GET USER FEED %d, response body = %s"), PostCounter, *ResponseStr);

		// Create the Json parser
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ResponseStr);

		if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			// The array of status updates for the local user we will be adding to
			TArray<FOnlineStatusUpdate>& StatusUpdateArray = PS4Sharing->PlayerStatusUpdates.FindOrAdd(LocalUserNum);

			TArray<TSharedPtr<FJsonValue>> JsonFeedArray = JsonObject->GetArrayField(TEXT("feed"));
			for (TArray<TSharedPtr<FJsonValue>>::TConstIterator It(JsonFeedArray); It; ++It)
			{
				const TSharedPtr<FJsonValue>& JsonValue = *It;
				const TSharedPtr<FJsonObject>& JsonFeed = JsonValue->AsObject();
				const TSharedPtr<FJsonValue>& JsonStoryComment = JsonFeed->GetField<EJson::String>(TEXT("storyComment"));
				if (JsonStoryComment.IsValid())
				{
					// We have a post update to add to our collection
					FOnlineStatusUpdate* StatusUpdate = new (StatusUpdateArray) FOnlineStatusUpdate();
					StatusUpdate->Message = JsonStoryComment->AsString();
					++PostCounter;
				}
			}
		}
	}

private:

	/** PS4 sharing interface to manipulate */
	FOnlineSharingPS4* PS4Sharing;

	/** The local user number requesting profile data */
	int32 LocalUserNum;

	/** The WebApi context for the local user */
	int32 LocalUserWebApiContext;

	int32 NumPostsToRead;
	int32 PostCounter;
	int32 BlockCounter;

	typedef FAsyncTask<FWebApiPS4Task> BackgroundWebApiTask;
	TSharedPtr<BackgroundWebApiTask> CurrentWebApiTask;
};


FOnlineSharingPS4::FOnlineSharingPS4(class FOnlineSubsystemPS4* InSubsystem)
	:	PS4Subsystem(InSubsystem)
{
	static TArray<EOnlineSharingCategory> AllCategories = {
		EOnlineSharingCategory::ReadPosts,
		EOnlineSharingCategory::Friends,
		EOnlineSharingCategory::Mailbox,
		EOnlineSharingCategory::OnlineStatus,
		EOnlineSharingCategory::ProfileInfo,
		EOnlineSharingCategory::LocationInfo,
		EOnlineSharingCategory::SubmitPosts,
		EOnlineSharingCategory::ManageFriends,
		EOnlineSharingCategory::AccountAdmin,
		EOnlineSharingCategory::Events
	};

	for (EOnlineSharingCategory Cat : AllCategories)
	{
		FSharingPermission NewPerm(ToString(Cat), Cat);
		NewPerm.Status = EOnlineSharingPermissionState::Granted;
		CurrentPermissions.Add(NewPerm);
	}
}

FOnlineSharingPS4::~FOnlineSharingPS4()
{
}

void FOnlineSharingPS4::RequestCurrentPermissions(int32 LocalUserNum, FOnRequestCurrentPermissionsComplete& CompletionDelegate)
{
	CompletionDelegate.ExecuteIfBound(LocalUserNum, true, CurrentPermissions);
}

void FOnlineSharingPS4::GetCurrentPermissions(int32 LocalUserNum, TArray<FSharingPermission>& OutPermissions)
{
	OutPermissions.Empty(CurrentPermissions.Num());
	OutPermissions = CurrentPermissions;
}

bool FOnlineSharingPS4::RequestNewReadPermissions(int32 LocalUserNum, EOnlineSharingCategory NewPermissions)
{
	ensure((NewPermissions & ~EOnlineSharingCategory::ReadPermissionMask) == EOnlineSharingCategory::None);
	// Note: We automatically grant read permissions for posts
	TriggerOnRequestNewReadPermissionsCompleteDelegates(LocalUserNum, true);
	return true;
}

bool FOnlineSharingPS4::RequestNewPublishPermissions(int32 LocalUserNum, EOnlineSharingCategory NewPermissions, EOnlineStatusUpdatePrivacy Privacy)
{
	ensure((NewPermissions & ~EOnlineSharingCategory::PublishPermissionMask) == EOnlineSharingCategory::None);
	// Note: We automatically grant publish permissions for posts
	TriggerOnRequestNewPublishPermissionsCompleteDelegates(LocalUserNum, true);
	return true;
}

bool FOnlineSharingPS4::ShareStatusUpdate(int32 LocalUserNum, const FOnlineStatusUpdate& StatusUpdate)
{
	bool bIsSharingStatusUpdate = false;

	const FString& UserName = PS4Subsystem->GetIdentityInterface()->GetPlayerNickname(LocalUserNum);
	if (!UserName.IsEmpty())
	{
		TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum));
		const int32 LocalUserWebApiContext = PS4Subsystem->GetUserWebApiContext(*LocalUserId);
		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(new FOnlineAsyncTaskPS4ShareStatusUpdate(PS4Subsystem, LocalUserNum, LocalUserWebApiContext, UserName, StatusUpdate));
		bIsSharingStatusUpdate = true;
	}

	if (!bIsSharingStatusUpdate)
	{
		TriggerOnSharePostCompleteDelegates(LocalUserNum, false);
	}

	return bIsSharingStatusUpdate;
}


bool FOnlineSharingPS4::ReadNewsFeed(int32 LocalUserNum, int32 NumPostsToRead)
{
	check(NumPostsToRead > 0);

	bool bIsReadingNewsFeed = false;

	TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum));
	const int32 LocalUserWebApiContext = PS4Subsystem->GetUserWebApiContext(*LocalUserId);
	if (LocalUserWebApiContext > 0)
	{
		PlayerStatusUpdates.FindOrAdd(LocalUserNum).Empty();

		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(new FOnlineAsyncTaskPS4ReadNewsFeed(PS4Subsystem, this, LocalUserNum, LocalUserWebApiContext, NumPostsToRead));
		bIsReadingNewsFeed = true;
	}
	else
	{
		TriggerOnReadNewsFeedCompleteDelegates(LocalUserNum, false);
		bIsReadingNewsFeed = false;
	}

	return bIsReadingNewsFeed;
}

EOnlineCachedResult::Type FOnlineSharingPS4::GetCachedNewsFeed(int32 LocalUserNum, int32 NewsFeedIdx, FOnlineStatusUpdate& OutNewsFeed)
{
	check(NewsFeedIdx >= 0);

	const TArray<FOnlineStatusUpdate>* StatusArray = PlayerStatusUpdates.Find(LocalUserNum);
	if (StatusArray && NewsFeedIdx < StatusArray->Num())
	{
		OutNewsFeed = PlayerStatusUpdates[LocalUserNum][NewsFeedIdx];
		return EOnlineCachedResult::Success;
	}

	return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FOnlineSharingPS4::GetCachedNewsFeeds(int32 LocalUserNum, TArray<FOnlineStatusUpdate>& OutNewsFeeds)
{
	OutNewsFeeds.Empty();

	const TArray<FOnlineStatusUpdate>* StatusArray = PlayerStatusUpdates.Find(LocalUserNum);
	if (StatusArray)
	{
		for (TArray<FOnlineStatusUpdate>::TConstIterator It(*StatusArray); It; ++It)
		{
			OutNewsFeeds.Add(*It);
		}
		return (OutNewsFeeds.Num() > 0) ? EOnlineCachedResult::Success : EOnlineCachedResult::NotFound;
	}

	return EOnlineCachedResult::NotFound;
}
