// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskPS4QueryTournamentMatchList.h"
#include "OnlineSubsystemPS4.h"
#include "../OnlineTournamentInterfacePS4.h"
#include "Serialization/JsonSerializerMacros.h"

struct FPS4NpQueryMatchesMatch
	: public FJsonSerializable
{
public:
	~FPS4NpQueryMatchesMatch() = default;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("eventId", MatchId);
	END_JSON_SERIALIZER

public:
	FString MatchId;
};

struct FPS4NpQueryMatchesResponse
	: public FJsonSerializable
{
public:
	~FPS4NpQueryMatchesResponse() = default;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("matches", Matches, FPS4NpQueryMatchesMatch);
	END_JSON_SERIALIZER

public:
	TArray<FPS4NpQueryMatchesMatch> Matches;
};

FOnlineAsyncTaskPS4QueryTournamentMatchList::FOnlineAsyncTaskPS4QueryTournamentMatchList(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TSharedRef<const FOnlineTournamentId> InTournamentId, const FOnlineTournamentQueryMatchListComplete& InDelegate)
	: FOnlineAsyncWebTaskPS4(InSubsystem)
	, SonyUserId(InSonyUserId)
	, WebApiUserContext(InWebApiUserContext)
	, NpTournamentServiceLabel(InNpTournamentServiceLabel)
	, TournamentId(InTournamentId)
	, Delegate(InDelegate)
	, Result(true)
{
}

FString FOnlineAsyncTaskPS4QueryTournamentMatchList::ToString() const
{
	return FString(TEXT("FOnlineAsyncTaskPS4QueryTournamentMatchList"));
}

bool FOnlineAsyncTaskPS4QueryTournamentMatchList::WasSuccessful() const
{
	return Result.WasSuccessful() && OptionalMatchIds.IsSet();
}

TOptional<FWebApiPS4Task> FOnlineAsyncTaskPS4QueryTournamentMatchList::CreateWebTask()
{
	FWebApiPS4Task Task(WebApiUserContext);

	FString UriPath = FString::Printf(TEXT("/v1/npServiceLabels/%u/events2/%s/bracket"), NpTournamentServiceLabel, *TournamentId->ToString());

	Task.SetRequest(ENpApiGroup::Tournament, MoveTemp(UriPath), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);

	return Task;
}

void FOnlineAsyncTaskPS4QueryTournamentMatchList::ProcessResult(FWebApiPS4Task& CompletedTask)
{
	if (CompletedTask.WasSuccessful())
	{
		FPS4NpQueryMatchesResponse Response;
		Response.FromJson(CompletedTask.GetResponseBody());

		OptionalMatchIds = TArray<TSharedRef<const FOnlineTournamentMatchId>>();
		OptionalMatchIds->Reset(Response.Matches.Num());
		for (FPS4NpQueryMatchesMatch& Event : Response.Matches)
		{
			OptionalMatchIds->Add(MakeShared<FOnlineTournamentMatchIdPS4>(MoveTemp(Event.MatchId)));
		}
	}
	else
	{
		Result = CompletedTask.GetErrorResult();
		Result.bSucceeded = false;
	}
}

void FOnlineAsyncTaskPS4QueryTournamentMatchList::Finalize()
{
	const FOnlineTournamentPS4Ptr TournamentInt = StaticCastSharedPtr<FOnlineTournamentPS4>(Subsystem->GetTournamentInterface());
	if (TournamentInt.IsValid())
	{
		// Always create the data cache if it doesn't exist
		FOnlineTournamentPS4::FOnlineTournamentPS4Data& UserDataCache = TournamentInt->UserQueriedData.FindOrAdd(SonyUserId);

		// Update cache if we were successful
		if (Result.WasSuccessful() && OptionalMatchIds.IsSet())
		{
			UserDataCache.QueriedTournamentMatchIds.Emplace(TournamentId, OptionalMatchIds.GetValue());
		}
	}
}

void FOnlineAsyncTaskPS4QueryTournamentMatchList::TriggerDelegates()
{
	Delegate.ExecuteIfBound(Result, OptionalMatchIds);
}
