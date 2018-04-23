// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlinePresenceInterfacePS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "Misc/App.h"

#include "WebApiPS4Task.h"

void FPresenceTask:: DoWork()
{
	// do work serially since we're already on a thread
	Data.Task->StartSynchronousTask();

	// Queue up an event in the async task manager so that the delegate can safely trigger in the game thread.
	if (PS4Subsystem->GetAsyncTaskManager())
	{
		auto NewEvent = new FAsyncEventPresenceTaskCompleted(PS4Subsystem, Data);
		PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
	}
}

void FOnlinePresencePS4::SetPresence(const FUniqueNetId& User, const FOnlineUserPresenceStatus& Status, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	if(!PS4Subsystem)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlinePresencePS4::SetPresence - No PS4Subsystem, returning false."));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	// These constants come from the call rates limit documentation
	CleanTaskList(CurrentSetPresenceTasks, 15 * 60);
	if (CurrentSetPresenceTasks.Num() >= 45)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlinePresencePS4::SetPresence - Call rate limit has been exceeded, returning false."));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	FUniqueNetIdPS4 const& PS4User = FUniqueNetIdPS4::Cast(User);
	
	FString ApiGroup = TEXT("sdk:userProfile");
	FString BaseURL = TEXT("/v1/users/");
	BaseURL += PS4User.ToString();
	BaseURL += TEXT("/presence/inGamePresence");

	FString GameStatusStr = Status.StatusStr;

	const FVariantData* GameStatus = Status.Properties.Find(DefaultPresenceKey);
	if (!GameStatus || GameStatus->GetType() != EOnlineKeyValuePairDataType::String)
	{
		GameStatus = nullptr;
	}

	const FVariantData* GameData = Status.Properties.Find(CustomPresenceDataKey);
	if (!GameData || GameData->GetType() != EOnlineKeyValuePairDataType::String)
	{
		GameData = nullptr;
	}

	if(!GameStatus && !GameData && GameStatusStr.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlinePresencePS4::SetPresence - Empty presence string, returning false."));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	// update cached entry
	TSharedRef<FOnlineUserPresence>* UserPresence = CachedPresenceData.Find(User.ToString());
	if (UserPresence == nullptr)
	{
		UserPresence = &CachedPresenceData.Add(User.ToString(), MakeShareable(new FOnlineUserPresence()));
	}
	(*UserPresence)->Status = Status;

	// this json is simple enough to write manually
	bool NeedComma = false;
	FString Body = TEXT("{");
	if (GameStatus && GameStatusStr.IsEmpty())
	{
		GameStatus->GetValue(GameStatusStr);
	}
	Body += FString::Printf(TEXT("\"gameStatus\" : \"%s\""), *GameStatusStr);
	NeedComma = true;

	if (GameData)
	{
		FString GameDataStr;
		GameData->GetValue(GameDataStr);
		if(NeedComma)
		{
			Body += TEXT(",");
		}
		Body += FString::Printf(TEXT("\"gameData\" : \"%s\""), *GameDataStr);
	}
	Body += TEXT("}");

	const int32 LocalUserWebApiContext = PS4Subsystem->GetUserWebApiContext(FUniqueNetIdPS4::Cast(User));
	BackgroundWebApiTask * WebApiTask = new BackgroundWebApiTask(LocalUserWebApiContext);
	WebApiTask->GetTask().SetRequest(ApiGroup, BaseURL, SCE_NP_WEBAPI_HTTP_METHOD_PUT);
	WebApiTask->GetTask().SetRequestBody(Body);

	TaskData Data(PS4User);
	Data.Task = MakeShareable(WebApiTask);
	Data.TaskType = ETaskType::Set;
	Data.Delegate = Delegate;

	BackgroundPresenceTask * PresenceTask = new BackgroundPresenceTask(PS4Subsystem, Data);
	PresenceTask->StartBackgroundTask();

	CurrentSetPresenceTasks.Add(FPresenceTaskBackgroundWrapper(MakeShareable(PresenceTask), FApp::GetCurrentTime()));
}

void FOnlinePresencePS4::QueryPresence(const FUniqueNetId& User, const FOnPresenceTaskCompleteDelegate& Delegate)
{
	if (!PS4Subsystem)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlinePresencePS4::QueryPresence - No PS4Subsystem, returning false."));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	// These constants come from the call rates limit documentation
	CleanTaskList(CurrentQueryPresenceTasks, (15 * 60) + 10);
	if (CurrentQueryPresenceTasks.Num() >= 150)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlinePresencePS4::QueryPresence - Call rate limit has been exceeded, returning false."));
		Delegate.ExecuteIfBound(User, false);
		return;
	}

	FUniqueNetIdPS4 const& PS4User = FUniqueNetIdPS4::Cast(User);

	FString ApiGroup = TEXT("sdk:userProfile");
	FString BaseURL = TEXT("/v1/users/");
	BaseURL += PS4User.ToString();
	BaseURL += TEXT("/presence?type=incontext");

	IOnlineIdentityPtr Identity = PS4Subsystem->GetIdentityInterface();
	const int32 LocalUserWebApiContext = PS4Subsystem->GetUserWebApiContext(PS4User);
	BackgroundWebApiTask * WebApiTask = new BackgroundWebApiTask(LocalUserWebApiContext);
	WebApiTask->GetTask().SetRequest(ApiGroup, BaseURL, SCE_NP_WEBAPI_HTTP_METHOD_GET);

	TaskData Data(PS4User);
	Data.Task = MakeShareable(WebApiTask);
	Data.TaskType = ETaskType::Query;
	Data.Delegate = Delegate;
	
	BackgroundPresenceTask * PresenceTask = new BackgroundPresenceTask(PS4Subsystem, Data);
	PresenceTask->StartBackgroundTask();

	CurrentQueryPresenceTasks.Add(FPresenceTaskBackgroundWrapper(MakeShareable(PresenceTask), FApp::GetCurrentTime()));
}

EOnlineCachedResult::Type FOnlinePresencePS4::GetCachedPresence(const FUniqueNetId& User, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	FUniqueNetIdPS4 const& PS4User = FUniqueNetIdPS4::Cast(User);

	// yes, this is a pointer to a shared pointer
	TSharedRef<FOnlineUserPresence>* Presence = CachedPresenceData.Find(PS4User.ToString());
	if (Presence)
	{
		OutPresence = *Presence;
		return EOnlineCachedResult::Success;
	}

	return EOnlineCachedResult::NotFound;
}

EOnlineCachedResult::Type FOnlinePresencePS4::GetCachedPresenceForApp(const FUniqueNetId& /*LocalUserId*/, const FUniqueNetId& User, const FString& AppId, TSharedPtr<FOnlineUserPresence>& OutPresence)
{
	EOnlineCachedResult::Type Result = EOnlineCachedResult::NotFound;

	if (PS4Subsystem->GetAppId() == AppId)
	{
		Result = GetCachedPresence(User, OutPresence);
	}

	return Result;
}

void FOnlinePresencePS4::CleanTaskList(TArray<FPresenceTaskBackgroundWrapper>& CurrentTasks, double CallRateLimitDuration)
{
	const double CurrentTime = FApp::GetCurrentTime();
	CurrentTasks.RemoveAllSwap([&CallRateLimitDuration, &CurrentTime](const FPresenceTaskBackgroundWrapper& Task)
	{
		const bool bIsTaskDone = Task.BackgroundPresenceTask->IsDone();
		if (!bIsTaskDone)
		{
			// Do not delete this task
			return false;
		}

		const bool bIsTaskOld = CurrentTime - Task.RequestCreationTime > CallRateLimitDuration;
		if (!bIsTaskOld)
		{
			// Do not delete this task
			return false;
		}

		// Delete this task
		UE_LOG_ONLINE(VeryVerbose, TEXT("Removing Presence Task from call-rate list"));
		return true;
	});
}

bool FOnlinePresencePS4::SetPresenceResponse(const TaskData & Data)
{
	// sanity check
	if(Data.Task.IsValid() && Data.Task->IsDone())
	{
		return true;
	}

	return false;
}

bool FOnlinePresencePS4::QueryPresenceResponse(const TaskData & Data)
{
	// sanity check
	if (Data.Task.IsValid() && Data.Task->IsDone())
	{
		TSharedRef<FOnlineUserPresence> NewPresence(MakeShareable(new FOnlineUserPresence()));

		sce::Json::Value Root;

		const FString& Response = Data.Task->GetTask().GetResponseBody();

		int ret = sce::Json::Parser::parse(Root, TCHAR_TO_ANSI(*Response), Response.Len());
		if(ret < 0)
		{
			// failed to parse for some reason
			return false;
		}

		size_t count = Root["presence"]["incontextInfoList"].count();
		for (int i = 0; i < count; ++i)
		{
			if (Root["presence"]["incontextInfoList"][i]["platform"].getString() == "PS4")
			{
				sce::Json::String GameStatus = Root["presence"]["incontextInfoList"][i]["gameStatus"].getString();
				sce::Json::String GameData = Root["presence"]["incontextInfoList"][i]["gameData"].getString();

				if(!GameStatus.empty())
				{
					NewPresence->Status.Properties.Add(DefaultPresenceKey, FVariantData(FString(GameStatus.c_str())));
				}

				if(!GameData.empty())
				{
					NewPresence->Status.Properties.Add(CustomPresenceDataKey, FVariantData(FString(GameData.c_str())));
				}
			}

			NewPresence->bIsOnline = NewPresence->bIsOnline || Root["presence"]["incontextInfoList"][i]["onlineStatus"].getString() == "online";
			NewPresence->bIsPlaying = NewPresence->bIsPlaying || Root["presence"]["incontextInfoList"][i]["gameTitleInfo"]["npTitleId"].getString().size() > 0;
			NewPresence->bIsPlayingThisGame = NewPresence->bIsPlayingThisGame || Root["presence"]["incontextInfoList"][i]["gameTitleInfo"]["npTitleId"].getString() == PS4Subsystem->GetNpTitleId().id;
		}

		CachedPresenceData.Add(Data.User->ToString(), NewPresence);

		return true;
	}

	return false;
}

void FAsyncEventPresenceTaskCompleted::TriggerDelegates()
{
	// this must be called from the main thread
	check(IsInGameThread());

	// feels like there should be a better way to do this
	FOnlinePresencePS4 * PS4Presence = static_cast<FOnlinePresencePS4*>(PS4Subsystem->GetPresenceInterface().Get());
	bool bWasSuccessful = false;

	if (PS4Presence)
	{
		if(Data.TaskType == ETaskType::Set)
		{
			bWasSuccessful = PS4Presence->SetPresenceResponse(Data);
		}
		else if(Data.TaskType == ETaskType::Query)
		{
			bWasSuccessful = PS4Presence->QueryPresenceResponse(Data);

			if (bWasSuccessful)
			{
				TSharedPtr<FOnlineUserPresence> Presence;
				if (PS4Presence->GetCachedPresence(Data.User.Get(), Presence) == EOnlineCachedResult::Success &&
					Presence.IsValid())
				{
					PS4Presence->TriggerOnPresenceReceivedDelegates(Data.User.Get(), Presence.ToSharedRef());
				}
			}
		}
		else
		{
			//something bad happened
			bWasSuccessful = false;
		}
	}

	FOnlineAsyncEvent::TriggerDelegates();
	Data.Delegate.ExecuteIfBound(Data.User.Get(), bWasSuccessful);
}
