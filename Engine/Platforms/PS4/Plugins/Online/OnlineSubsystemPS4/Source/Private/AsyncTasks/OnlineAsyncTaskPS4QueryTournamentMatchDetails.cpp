// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskPS4QueryTournamentMatchDetails.h"
#include "OnlineSubsystemPS4.h"
#include "../OnlineTournamentInterfacePS4.h"
#include "../OnlineTournamentPS4NpTypes.h"

EOnlineTournamentMatchState FPS4NpQueryMatchDetailsMatchDetail::GetState() const
{
	const FDateTime CurrentTime = FDateTime::UtcNow();

	if (Duration.RegisterStartDate > CurrentTime)
	{
		return EOnlineTournamentMatchState::Created;
	}

	return !bClosed ? EOnlineTournamentMatchState::InProgress : EOnlineTournamentMatchState::Finished;
}

struct FPS4NpQueryMatchDetailsResponse
	: public FJsonSerializable
{
public:
	virtual ~FPS4NpQueryMatchDetailsResponse() = default;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("eventId", EventId);
		JSON_SERIALIZE("eventType", EventType);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("matchEventDetail", MatchDetails, FPS4NpQueryMatchDetailsMatchDetail);
	END_JSON_SERIALIZER

public:
	FString EventId;
	FString EventType;
	TArray<FPS4NpQueryMatchDetailsMatchDetail> MatchDetails;
};

FOnlineAsyncTaskPS4QueryTournamentMatchDetails::FOnlineAsyncTaskPS4QueryTournamentMatchDetails(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TArray<TSharedRef<const FOnlineTournamentMatchId>>& InTournamentMatchIds, const FOnlineTournamentQueryMatchDetailsComplete& InDelegate)
	: FOnlineAsyncWebTaskListPS4(InSubsystem)
	, SonyUserId(InSonyUserId)
	, WebApiUserContext(InWebApiUserContext)
	, NpTournamentServiceLabel(InNpTournamentServiceLabel)
	, TournamentMatchIds(InTournamentMatchIds)
	, Delegate(InDelegate)
	, Result(true)
{
}

FString FOnlineAsyncTaskPS4QueryTournamentMatchDetails::ToString() const
{
	return FString(TEXT("FOnlineAsyncTaskPS4QueryTournamentMatchDetails"));
}

bool FOnlineAsyncTaskPS4QueryTournamentMatchDetails::WasSuccessful() const
{
	return Result.WasSuccessful() && OptionalMatchDetails.IsSet();
}

TArray<FWebApiPS4Task> FOnlineAsyncTaskPS4QueryTournamentMatchDetails::CreateWebTasks()
{
	TArray<FWebApiPS4Task> Tasks;

	for (const TSharedRef<const FOnlineTournamentMatchId> MatchId : TournamentMatchIds)
	{
		FWebApiPS4Task Task(WebApiUserContext);

		FString UriPath = FString::Printf(TEXT("/v1/npServiceLabels/%u/events2/%s"), NpTournamentServiceLabel, *MatchId->ToString());

		Task.SetRequest(ENpApiGroup::Tournament, MoveTemp(UriPath), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);

		Tasks.Emplace(MoveTemp(Task));
	}

	return Tasks;
}

void FOnlineAsyncTaskPS4QueryTournamentMatchDetails::ProcessResult(FWebApiPS4Task& CompletedTask, const int32 /*UnusedTaskIndex*/)
{
	if (CompletedTask.WasSuccessful())
	{
		FPS4NpQueryMatchDetailsResponse Response;
		Response.FromJson(CompletedTask.GetResponseBody());

		if (!OptionalMatchDetails.IsSet())
		{
			OptionalMatchDetails = TArray<TSharedRef<const IOnlineTournamentMatchDetails>>();
			OptionalMatchDetails->Reset(Response.MatchDetails.Num());
		}
		for (FPS4NpQueryMatchDetailsMatchDetail& Details : Response.MatchDetails)
		{
			OptionalMatchDetails->Emplace(MakeShared<FOnlineTournamentMatchDetailsPS4>(CopyTemp(Response.EventId), MoveTemp(Details)));
		}
	}
	else
	{
		Result = CompletedTask.GetErrorResult();
		Result.bSucceeded = false;
	}
}

void FOnlineAsyncTaskPS4QueryTournamentMatchDetails::Finalize()
{
	const FOnlineTournamentPS4Ptr TournamentInt = StaticCastSharedPtr<FOnlineTournamentPS4>(Subsystem->GetTournamentInterface());
	if (TournamentInt.IsValid())
	{
		// Always create the data cache if it doesn't exist
		FOnlineTournamentPS4::FOnlineTournamentPS4Data& UserDataCache = TournamentInt->UserQueriedData.FindOrAdd(SonyUserId);

		// Update cache if we were successful
		if (Result.WasSuccessful() && OptionalMatchDetails.IsSet())
		{
			for (TSharedRef<const IOnlineTournamentMatchDetails> NewMatch : OptionalMatchDetails.GetValue())
			{
				// Check if we already have this Event Id cached
				UserDataCache.QueriedMatchDetails.Emplace(NewMatch->GetMatchId(), NewMatch);
			}
		}
	}
}

void FOnlineAsyncTaskPS4QueryTournamentMatchDetails::TriggerDelegates()
{
	Delegate.ExecuteIfBound(Result, OptionalMatchDetails);
}
