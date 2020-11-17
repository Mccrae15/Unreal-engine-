// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskPS4QueryTournamentTeamDetails.h"
#include "OnlineSubsystemPS4.h"
#include "../OnlineTournamentInterfacePS4.h"
#include "../OnlineTournamentPS4NpTypes.h"
#include "Serialization/JsonSerializerMacros.h"

struct FPS4NpQueryRosterResponse
	: public FJsonSerializable
{
public:
	~FPS4NpQueryRosterResponse() = default;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("teamDetail", TeamDetail);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("members", Members, FPS4NpEntity);
	END_JSON_SERIALIZER

public:
	FPS4NpEntity TeamDetail;
	TArray<FPS4NpEntity> Members;
};

FOnlineAsyncTaskPS4QueryTournamentTeamDetails::FOnlineAsyncTaskPS4QueryTournamentTeamDetails(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TArray<TSharedRef<const FOnlineTournamentTeamId>>& InTeamIds, const FOnlineTournamentQueryTeamDetailsComplete& InDelegate)
	: FOnlineAsyncWebTaskListPS4(InSubsystem)
	, SonyUserId(InSonyUserId)
	, WebApiUserContext(InWebApiUserContext)
	, NpTournamentServiceLabel(InNpTournamentServiceLabel)
	, TeamIds(InTeamIds)
	, Delegate(InDelegate)
	, Result(true)
{
}

FString FOnlineAsyncTaskPS4QueryTournamentTeamDetails::ToString() const
{
	return FString(TEXT("FOnlineAsyncTaskPS4QueryTournamentTeamDetails"));
}

bool FOnlineAsyncTaskPS4QueryTournamentTeamDetails::WasSuccessful() const
{
	return Result.WasSuccessful() && OptionalTeamDetails.IsSet();
}

TArray<FWebApiPS4Task> FOnlineAsyncTaskPS4QueryTournamentTeamDetails::CreateWebTasks()
{
	TArray<FWebApiPS4Task> Tasks;

	for (const TSharedRef<const FOnlineTournamentTeamId> TeamId : TeamIds)
	{
		TSharedRef<const FOnlineTournamentTeamIdPS4> PS4TeamId = StaticCastSharedRef<const FOnlineTournamentTeamIdPS4>(TeamId);

		FWebApiPS4Task Task(WebApiUserContext);

		FString UriPath = FString::Printf(TEXT("/v1/npServiceLabels/%u/events2/%s/registeredRosters/%s"), NpTournamentServiceLabel, *PS4TeamId->GetTournamentId()->ToString(), *PS4TeamId->GetTeamId());
		TournamentIds.Emplace(PS4TeamId->GetTournamentId());

		Task.SetRequest(ENpApiGroup::Tournament, MoveTemp(UriPath), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);

		// Set our required platform header to signify what platform this team belongs to
		Task.AddRequestHeader(TEXT("X-NP-EVENTS-TEAM-PLATFORM"), PS4TeamId->GetPlatform());

		Tasks.Emplace(MoveTemp(Task));
	}

	return Tasks;
}

void FOnlineAsyncTaskPS4QueryTournamentTeamDetails::ProcessResult(FWebApiPS4Task& CompletedTask, const int32 TaskIndex)
{
	if (CompletedTask.WasSuccessful())
	{
		FPS4NpQueryRosterResponse Response;
		Response.FromJson(CompletedTask.GetResponseBody());

		if (!OptionalTeamDetails.IsSet())
		{
			OptionalTeamDetails = TArray<TSharedRef<const IOnlineTournamentTeamDetails>>();
		}

		check(TournamentIds.IsValidIndex(TaskIndex));
		OptionalTeamDetails->Emplace(MakeShared<const FOnlineTournamentTeamDetailsPS4>(TournamentIds[TaskIndex], MoveTemp(Response.TeamDetail), MoveTemp(Response.Members)));
	}
	else
	{
		Result = CompletedTask.GetErrorResult();
		Result.bSucceeded = false;
	}
}

void FOnlineAsyncTaskPS4QueryTournamentTeamDetails::Finalize()
{
	const FOnlineTournamentPS4Ptr TournamentInt = StaticCastSharedPtr<FOnlineTournamentPS4>(Subsystem->GetTournamentInterface());
	if (TournamentInt.IsValid())
	{
		// Always create the data cache if it doesn't exist
		FOnlineTournamentPS4::FOnlineTournamentPS4Data& UserDataCache = TournamentInt->UserQueriedData.FindOrAdd(SonyUserId);

		// Update cache if we were successful
		if (Result.WasSuccessful() && OptionalTeamDetails.IsSet())
		{
			for (TSharedRef<const IOnlineTournamentTeamDetails> NewTeam : OptionalTeamDetails.GetValue())
			{
				// Check if we already have this Event Id cached
				UserDataCache.QueriedTeamDetails.Emplace(NewTeam->GetTeamId(), NewTeam);
			}
		}
	}
}

void FOnlineAsyncTaskPS4QueryTournamentTeamDetails::TriggerDelegates()
{
	Delegate.ExecuteIfBound(Result, OptionalTeamDetails);
}
