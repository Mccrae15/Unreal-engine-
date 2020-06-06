// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskPS4QueryTournamentList.h"
#include "OnlineSubsystemPS4.h"
#include "../OnlineTournamentInterfacePS4.h"
#include "../OnlineTournamentPS4NpTypes.h"
#include "Serialization/JsonSerializerMacros.h"

struct FPS4NpQueryEventsEvent
	: public FJsonSerializable
{
public:
	~FPS4NpQueryEventsEvent() = default;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("eventId", EventId);
	END_JSON_SERIALIZER

public:
	FString EventId;
};

struct FPS4NpQueryEventsResponse
	: public FJsonSerializable
{
public:
	~FPS4NpQueryEventsResponse() = default;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("events", Events, FPS4NpQueryEventsEvent);
	END_JSON_SERIALIZER

public:
	TArray<FPS4NpQueryEventsEvent> Events;
};

FOnlineAsyncTaskPS4QueryTournamentList::FOnlineAsyncTaskPS4QueryTournamentList(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const FOnlineTournamentQueryFilter& InQueryFilter, const FOnlineTournamentQueryTournamentListComplete& InDelegate)
	: FOnlineAsyncWebTaskPS4(InSubsystem)
	, SonyUserId(InSonyUserId)
	, WebApiUserContext(InWebApiUserContext)
	, NpTournamentServiceLabel(InNpTournamentServiceLabel)
	, QueryFilter(InQueryFilter)
	, Delegate(InDelegate)
	, Result(true)
{
}

FString FOnlineAsyncTaskPS4QueryTournamentList::ToString() const
{
	return FString(TEXT("FOnlineAsyncTaskPS4QueryTournamentList"));
}

bool FOnlineAsyncTaskPS4QueryTournamentList::WasSuccessful() const
{
	return Result.WasSuccessful() && OptionalTournamentIds.IsSet();
}

TOptional<FWebApiPS4Task> FOnlineAsyncTaskPS4QueryTournamentList::CreateWebTask()
{
	FWebApiPS4Task Task(WebApiUserContext);

	FString UriPath = FString::Printf(TEXT("/v1/npServiceLabels/%u/events2%s"), NpTournamentServiceLabel, *BuildQueryStringFromQueryFilter(QueryFilter));

	Task.SetRequest(ENpApiGroup::Tournament, MoveTemp(UriPath), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);

	return Task;
}

void FOnlineAsyncTaskPS4QueryTournamentList::ProcessResult(FWebApiPS4Task& CompletedTask)
{
	if (CompletedTask.WasSuccessful())
	{
		FPS4NpQueryEventsResponse Response;
		Response.FromJson(CompletedTask.GetResponseBody());

		OptionalTournamentIds = TArray<TSharedRef<const FOnlineTournamentId>>();
		OptionalTournamentIds->Reset(Response.Events.Num());
		for (FPS4NpQueryEventsEvent& Event : Response.Events)
		{
			OptionalTournamentIds->Add(MakeShared<FOnlineTournamentIdPS4>(MoveTemp(Event.EventId)));
		}
	}
	else
	{
		Result = CompletedTask.GetErrorResult();
		Result.bSucceeded = false;
	}
}

void FOnlineAsyncTaskPS4QueryTournamentList::Finalize()
{
	const FOnlineTournamentPS4Ptr TournamentInt = StaticCastSharedPtr<FOnlineTournamentPS4>(Subsystem->GetTournamentInterface());
	if (TournamentInt.IsValid())
	{
		// Always create the data cache if it doesn't exist
		FOnlineTournamentPS4::FOnlineTournamentPS4Data& UserDataCache = TournamentInt->UserQueriedData.FindOrAdd(SonyUserId);

		// Update cache if we were successful
		if (Result.WasSuccessful() && OptionalTournamentIds.IsSet())
		{
			for (TSharedRef<const FOnlineTournamentId> NewTournamentId : OptionalTournamentIds.GetValue())
			{
				// Cast once here, to avoid casting a bunch of times in loop
				TSharedRef<const FOnlineTournamentIdPS4> NewTournamentIdPS4 = StaticCastSharedRef<const FOnlineTournamentIdPS4>(NewTournamentId);

				// Check if we already have this Event Id cached
				TSharedRef<const FOnlineTournamentId>* FoundTournamentId = UserDataCache.QueriedTournamentIds.FindByPredicate([&NewTournamentIdPS4](const TSharedRef<const FOnlineTournamentId> OldTournamentId)
				{
					return *NewTournamentIdPS4 == *StaticCastSharedRef<const FOnlineTournamentIdPS4>(OldTournamentId);
				});

				if (FoundTournamentId == nullptr)
				{
					UserDataCache.QueriedTournamentIds.Add(NewTournamentId);
				}
			}
		}
	}
}

void FOnlineAsyncTaskPS4QueryTournamentList::TriggerDelegates()
{
	Delegate.ExecuteIfBound(Result, OptionalTournamentIds);
}

/*static*/
FString FOnlineAsyncTaskPS4QueryTournamentList::BuildQueryStringFromQueryFilter(const FOnlineTournamentQueryFilter& QueryFilter)
{
	FString QueryString(TEXT("?eventType=tournament"));

	if (QueryFilter.ParticipantType.IsSet())
	{
		switch (QueryFilter.ParticipantType.GetValue())
		{
		case EOnlineTournamentParticipantType::Individual:
			QueryString += TEXT("&tournamentType=oneOnOne");
			break;
		case EOnlineTournamentParticipantType::Team:
			QueryString += TEXT("&tournamentType=teamOnTeam");
			break;
		}
	}

	if (QueryFilter.PlayerId.IsSet())
	{
		QueryString += FString::Printf(TEXT("&registeredAccountId=%s"), *QueryFilter.PlayerId.GetValue()->ToString());
	}
	else if (QueryFilter.TeamId.IsSet())
	{
		const TSharedRef<const FOnlineTournamentTeamIdPS4> PS4TeamId = StaticCastSharedRef<const FOnlineTournamentTeamIdPS4>(QueryFilter.TeamId.GetValue());
		QueryString += FString::Printf(TEXT("&registeredTeam=%s&registeredTeamPlatform=%s"), *PS4TeamId->GetTeamId(), *PS4TeamId->GetPlatform());
	}

	if (QueryFilter.SortDirection.IsSet())
	{
		switch (QueryFilter.SortDirection.GetValue())
		{
		case FOnlineTournamentQueryFilter::EOnlineTournamentSortDirection::Ascending:
			QueryString += TEXT("&direction=asc");
			break;
		case FOnlineTournamentQueryFilter::EOnlineTournamentSortDirection::Descending:
			QueryString += TEXT("&direction=desc");
			break;
		}
	}

	if (QueryFilter.Limit.IsSet())
	{
		QueryString += FString::Printf(TEXT("&limit=%d"), FMath::Clamp(1, 100, static_cast<int32>(QueryFilter.Limit.GetValue())));
	}

	if (QueryFilter.Offset.IsSet())
	{
		QueryString += FString::Printf(TEXT("&offset=%d"), FMath::Clamp(0, TNumericLimits<int32>::Max(), static_cast<int32>(QueryFilter.Offset.GetValue())));
	}

	return QueryString;
}
