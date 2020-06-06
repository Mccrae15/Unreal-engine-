// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskPS4QueryTournamentParticipantList.h"
#include "OnlineSubsystemPS4.h"
#include "../OnlineTournamentInterfacePS4.h"
#include "../OnlineTournamentPS4NpTypes.h"
#include "Serialization/JsonSerializerMacros.h"

struct FOnlineAsyncTaskPS4QueryTournamentParticipantListResponse
	: public FJsonSerializable
{
public:
	~FOnlineAsyncTaskPS4QueryTournamentParticipantListResponse() = default;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("registeredRosters", RegisteredTeams, FPS4NpRegisteredRoster);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("registeredUsers", RegisteredPlayers, FPS4NpRegisteredUser);

		JSON_SERIALIZE("start", Start);
		JSON_SERIALIZE("size", Size);
		JSON_SERIALIZE("totalResults", TotalResults);
	END_JSON_SERIALIZER

public:
	TArray<FPS4NpRegisteredRoster> RegisteredTeams;
	TArray<FPS4NpRegisteredUser> RegisteredPlayers;

	int32 Start;
	int32 Size;
	int32 TotalResults;
};

FOnlineAsyncTaskPS4QueryTournamentParticipantList::FOnlineAsyncTaskPS4QueryTournamentParticipantList(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TSharedRef<const FOnlineTournamentId> InTournamentId, const FOnlineTournamentParticipantQueryFilter& InQueryFilter, const FOnlineTournamentQueryParticipantListComplete& InDelegate)
	: FOnlineAsyncWebTaskPS4(InSubsystem)
	, SonyUserId(InSonyUserId)
	, WebApiUserContext(InWebApiUserContext)
	, NpTournamentServiceLabel(InNpTournamentServiceLabel)
	, TournamentId(InTournamentId)
	, QueryFilter(InQueryFilter)
	, Delegate(InDelegate)
	, Result(true)
{
}

FString FOnlineAsyncTaskPS4QueryTournamentParticipantList::ToString() const
{
	return FString(TEXT("FOnlineAsyncTaskPS4QueryTournamentParticipantList"));
}

bool FOnlineAsyncTaskPS4QueryTournamentParticipantList::WasSuccessful() const
{
	return Result.WasSuccessful() && OptionalParticipantDetails.IsSet();
}

TOptional<FWebApiPS4Task> FOnlineAsyncTaskPS4QueryTournamentParticipantList::CreateWebTask()
{
	FWebApiPS4Task Task(WebApiUserContext);

	const TCHAR* const ParticipantTypeString = QueryFilter.ParticipantType == EOnlineTournamentParticipantType::Individual ? TEXT("registeredUsers") : TEXT("registeredRosters");

	FString UriPath = FString::Printf(TEXT("/v1/npServiceLabels/%u/events2/%s/%s"), NpTournamentServiceLabel, *TournamentId->ToString(), ParticipantTypeString);

	const TCHAR* QueryStringPrefix = TEXT("?");
	if (QueryFilter.Limit.IsSet())
	{
		UriPath += FString::Printf(TEXT("%slimit=%u"), QueryStringPrefix, QueryFilter.Limit.GetValue());
		QueryStringPrefix = TEXT("&");
	}
	if (QueryFilter.Offset.IsSet())
	{
		UriPath += FString::Printf(TEXT("%soffset=%u"), QueryStringPrefix, QueryFilter.Offset.GetValue());
		QueryStringPrefix = TEXT("&");
	}

	Task.SetRequest(ENpApiGroup::Tournament, MoveTemp(UriPath), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);

	return Task;
}

void FOnlineAsyncTaskPS4QueryTournamentParticipantList::ProcessResult(FWebApiPS4Task& CompletedTask)
{
	if (CompletedTask.WasSuccessful())
	{
		FOnlineAsyncTaskPS4QueryTournamentParticipantListResponse Response;
		Response.FromJson(CompletedTask.GetResponseBody());

		OptionalParticipantDetails = TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>();

		if (QueryFilter.ParticipantType == EOnlineTournamentParticipantType::Individual)
		{
			TArray<FPS4NpRegisteredUser>& Participants = Response.RegisteredPlayers;
			OptionalParticipantDetails->Reset(Participants.Num());
			for (FPS4NpRegisteredUser& Participant : Participants)
			{
				TSharedRef<const FOnlineTournamentParticipantDetailsPS4> ParticipantDetails = MakeShared<FOnlineTournamentParticipantDetailsPS4>(TournamentId, MoveTemp(Participant));
				OptionalParticipantDetails->Add(ParticipantDetails);
			}
		}
		else if (QueryFilter.ParticipantType == EOnlineTournamentParticipantType::Team)
		{
			TArray<FPS4NpRegisteredRoster>& Participants = Response.RegisteredTeams;
			OptionalParticipantDetails->Reset(Participants.Num());
			for (FPS4NpRegisteredRoster& Participant : Participants)
			{
				TSharedRef<const FOnlineTournamentParticipantDetailsPS4> ParticipantDetails = MakeShared<FOnlineTournamentParticipantDetailsPS4>(TournamentId, MoveTemp(Participant));
				OptionalParticipantDetails->Add(ParticipantDetails);
			}
		}
	}
	else
	{
		Result = CompletedTask.GetErrorResult();
		Result.bSucceeded = false;
	}
}

void FOnlineAsyncTaskPS4QueryTournamentParticipantList::Finalize()
{
	const FOnlineTournamentPS4Ptr TournamentInt = StaticCastSharedPtr<FOnlineTournamentPS4>(Subsystem->GetTournamentInterface());
	if (TournamentInt.IsValid())
	{
		// Always create the data cache if it doesn't exist
		FOnlineTournamentPS4::FOnlineTournamentPS4Data& UserDataCache = TournamentInt->UserQueriedData.FindOrAdd(SonyUserId);

		// Update cache if we were successful
		if (Result.WasSuccessful() && OptionalParticipantDetails.IsSet())
		{
			TMap<EOnlineTournamentParticipantType, TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>& DataMap = UserDataCache.QueriedParticipantList.FindOrAdd(TournamentId);
			DataMap.Emplace(QueryFilter.ParticipantType, OptionalParticipantDetails.GetValue());
		}
	}
}

void FOnlineAsyncTaskPS4QueryTournamentParticipantList::TriggerDelegates()
{
	Delegate.ExecuteIfBound(Result, OptionalTotalResults, OptionalParticipantDetails);
}
