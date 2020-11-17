// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskPS4QueryTournamentDetails.h"
#include "OnlineSubsystemPS4.h"
#include "../OnlineTournamentInterfacePS4.h"
#include "../OnlineTournamentPS4NpTypes.h"
#include "../WebApiPS4Types.h"

EOnlineTournamentState FPS4NpQueryTournamentDetailsTournamentDetail::GetState() const
{
	const FDateTime CurrentTime = FDateTime::UtcNow();

	if (Duration.RegisterStartDate > CurrentTime)
	{
		return EOnlineTournamentState::Created;
	}
	else if (Duration.RegisterEndDate > CurrentTime && !bClosed)
	{
		return EOnlineTournamentState::OpenRegistration;
	}
	else if (Duration.EventStartDate > CurrentTime)
	{
		return EOnlineTournamentState::ClosedRegistration;
	}
	else if (Duration.EventEndDate > CurrentTime)
	{
		return EOnlineTournamentState::InProgress;
	}

	return EOnlineTournamentState::Finished;
}

struct FPS4NpQueryTournamentDetailsResponse
	: public FJsonSerializable
{
public:
	virtual ~FPS4NpQueryTournamentDetailsResponse() = default;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("eventId", EventId);
		JSON_SERIALIZE("eventType", EventType);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("tournamentEventDetail", TournamentDetail);
	END_JSON_SERIALIZER

public:
	FString EventId;
	FString EventType;
	FPS4NpQueryTournamentDetailsTournamentDetail TournamentDetail;
};

FOnlineAsyncTaskPS4QueryTournamentDetails::FOnlineAsyncTaskPS4QueryTournamentDetails(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TArray<TSharedRef<const FOnlineTournamentId>>& InTournamentIds, const FOnlineTournamentQueryTournamentDetailsComplete& InDelegate)
	: FOnlineAsyncWebTaskListPS4(InSubsystem)
	, SonyUserId(InSonyUserId)
	, WebApiUserContext(InWebApiUserContext)
	, NpTournamentServiceLabel(InNpTournamentServiceLabel)
	, TournamentIds(InTournamentIds)
	, Delegate(InDelegate)
	, Result(true)
{
}

FString FOnlineAsyncTaskPS4QueryTournamentDetails::ToString() const
{
	return FString(TEXT("FOnlineAsyncTaskPS4QueryTournamentDetails"));
}

bool FOnlineAsyncTaskPS4QueryTournamentDetails::WasSuccessful() const
{
	return Result.WasSuccessful() && OptionalTournamentDetails.IsSet();
}

TArray<FWebApiPS4Task> FOnlineAsyncTaskPS4QueryTournamentDetails::CreateWebTasks()
{
	TArray<FWebApiPS4Task> Tasks;

	for (const TSharedRef<const FOnlineTournamentId> TournamentId : TournamentIds)
	{
		FWebApiPS4Task Task(WebApiUserContext);

		FString UriPath = FString::Printf(TEXT("/v1/npServiceLabels/%u/events2/%s"), NpTournamentServiceLabel, *TournamentId->ToString());

		Task.SetRequest(ENpApiGroup::Tournament, MoveTemp(UriPath), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);

		Tasks.Emplace(MoveTemp(Task));
	}

	return Tasks;
}

void FOnlineAsyncTaskPS4QueryTournamentDetails::ProcessResult(FWebApiPS4Task& CompletedTask, const int32 /*UnusedTaskIndex*/)
{
	if (CompletedTask.WasSuccessful())
	{
		FPS4NpQueryTournamentDetailsResponse Response;
		Response.FromJson(CompletedTask.GetResponseBody());

		if (!OptionalTournamentDetails.IsSet())
		{
			OptionalTournamentDetails = TArray<TSharedRef<const IOnlineTournamentDetails>>();
		}

		OptionalTournamentDetails->Emplace(MakeShared<FOnlineTournamentDetailsPS4>(CopyTemp(Response.EventId), MoveTemp(Response.TournamentDetail)));
	}
	else
	{
		Result = CompletedTask.GetErrorResult();
		Result.bSucceeded = false;
	}
}

void FOnlineAsyncTaskPS4QueryTournamentDetails::Finalize()
{
	const FOnlineTournamentPS4Ptr TournamentInt = StaticCastSharedPtr<FOnlineTournamentPS4>(Subsystem->GetTournamentInterface());
	if (TournamentInt.IsValid())
	{
		// Always create the data cache if it doesn't exist
		FOnlineTournamentPS4::FOnlineTournamentPS4Data& UserDataCache = TournamentInt->UserQueriedData.FindOrAdd(SonyUserId);

		// Update cache if we were successful
		if (Result.WasSuccessful() && OptionalTournamentDetails.IsSet())
		{
			for (TSharedRef<const IOnlineTournamentDetails> NewTournament : OptionalTournamentDetails.GetValue())
			{
				// Check if we already have this Event Id cached
				UserDataCache.QueriedTournamentDetails.Emplace(NewTournament->GetTournamentId(), NewTournament);
			}
		}
	}
}

void FOnlineAsyncTaskPS4QueryTournamentDetails::TriggerDelegates()
{
	Delegate.ExecuteIfBound(Result, OptionalTournamentDetails);
}
