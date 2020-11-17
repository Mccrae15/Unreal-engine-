// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskPS4SubmitTournamentMatchResults.h"
#include "Serialization/JsonSerializerMacros.h"

struct FPS4NpPlayerScore
	: public FJsonSerializable
{
public:
	FPS4NpPlayerScore() = default;
	FPS4NpPlayerScore(FString&& InAccountId, FString&& InScore)
		: AccountId(MoveTemp(InAccountId))
		, Score(MoveTemp(InScore))
	{
	}

	virtual ~FPS4NpPlayerScore() = default;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("accountId", AccountId);
		JSON_SERIALIZE("score", Score);
	END_JSON_SERIALIZER

public:
	FString AccountId;
	FString Score;

};

struct FPS4NpPlayerScoreSubmissionBody
	: public FJsonSerializable
{
public:
	virtual ~FPS4NpPlayerScoreSubmissionBody() = default;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("version", Version);
		JSON_SERIALIZE("tournamentType", TournamentType);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("results", PlayerScores, FPS4NpPlayerScore);
	END_JSON_SERIALIZER

public:
	FString Version = TEXT("2");
	FString TournamentType = TEXT("oneOnOne");
	TArray<FPS4NpPlayerScore> PlayerScores;
};

struct FPS4NpTeamScore
	: public FJsonSerializable
{
public:
	FPS4NpTeamScore() = default;
	FPS4NpTeamScore(const FString& InTeamId, const FString& InPlatform, FString&& InScore)
		: TeamId(InTeamId)
		, Platform(InPlatform)
		, Score(MoveTemp(InScore))
	{
	}

	virtual ~FPS4NpTeamScore() = default;

	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("teamId", TeamId);
		JSON_SERIALIZE("platform", Platform);
		JSON_SERIALIZE("score", Score);
	END_JSON_SERIALIZER

public:
	FString TeamId;
	FString Platform;
	FString Score;

};

struct FPS4NpTeamScoreSubmissionBody
	: public FJsonSerializable
{
public:
	virtual ~FPS4NpTeamScoreSubmissionBody() = default;
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("version", Version);
		JSON_SERIALIZE("tournamentType", TournamentType);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("results", TeamScores, FPS4NpTeamScore);
	END_JSON_SERIALIZER

public:
	FString Version = TEXT("2");
	FString TournamentType = TEXT("teamOnTeam");
	TArray<FPS4NpTeamScore> TeamScores;
};

FOnlineAsyncTaskPS4SubmitTournamentMatchResults::FOnlineAsyncTaskPS4SubmitTournamentMatchResults(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TSharedRef<const FOnlineTournamentMatchId> InMatchId, const FOnlineTournamentMatchResults& InMatchResults, const FOnlineTournamentSubmitMatchResultsComplete& InDelegate)
	: FOnlineAsyncWebTaskPS4(InSubsystem)
	, SonyUserId(InSonyUserId)
	, WebApiUserContext(InWebApiUserContext)
	, NpTournamentServiceLabel(InNpTournamentServiceLabel)
	, MatchId(InMatchId)
	, MatchResults(InMatchResults)
	, Delegate(InDelegate)
	, Result(true)
{
}

FString FOnlineAsyncTaskPS4SubmitTournamentMatchResults::ToString() const
{
	return FString(TEXT("FOnlineAsyncTaskPS4SubmitTournamentMatchResults"));
}

bool FOnlineAsyncTaskPS4SubmitTournamentMatchResults::WasSuccessful() const
{
	return Result.WasSuccessful();
}

TOptional<FWebApiPS4Task> FOnlineAsyncTaskPS4SubmitTournamentMatchResults::CreateWebTask()
{
	TOptional<FWebApiPS4Task> Task;

	TOptional<EOnlineTournamentParticipantType> ParticipantType;

	for (const FOnlineTournamentScore& Score : MatchResults.ScoresToSubmit)
	{
		if (ParticipantType.IsSet())
		{
			// Ensure all types are the same
			if (ParticipantType.GetValue() != Score.ParticipantType)
			{
				Result.bSucceeded = false;
				return Task;
			}
		}
		else
		{
			ParticipantType = Score.ParticipantType;
		}
	}

	if (!ParticipantType.IsSet())
	{
		Result.bSucceeded = false;
		return Task;
	}

	Task = FWebApiPS4Task(WebApiUserContext);

	FString ResourceName;
	if (ParticipantType.GetValue() == EOnlineTournamentParticipantType::Individual)
	{
		ResourceName = TEXT("me");

		FPS4NpPlayerScoreSubmissionBody ScoreSubmissionData;
		for (const FOnlineTournamentScore& Score : MatchResults.ScoresToSubmit)
		{
			ScoreSubmissionData.PlayerScores.Emplace(Score.ParticipantId->ToString(), Score.Score.ToString());
		}

		Task->SetRequestBody(ScoreSubmissionData.ToJson());
	}
	else
	{
		{
			// Extract team/platform details from the first participant
			check(MatchResults.ScoresToSubmit.IsValidIndex(0)); // We've previously checked that we have > 0 scores
			TSharedRef<const FOnlineTournamentTeamIdPS4> TeamId = StaticCastSharedRef<const FOnlineTournamentTeamIdPS4>(MatchResults.ScoresToSubmit[0].ParticipantId);
			ResourceName = TeamId->GetTeamId();

			Task->AddRequestHeader(TEXT("X-NP-EVENTS-TEAM-PLATFORM"), TeamId->GetPlatform());
		}

		FPS4NpTeamScoreSubmissionBody ScoreSubmissionData;
		for (const FOnlineTournamentScore& Score : MatchResults.ScoresToSubmit)
		{
			TSharedRef<const FOnlineTournamentTeamIdPS4> TeamId = StaticCastSharedRef<const FOnlineTournamentTeamIdPS4>(Score.ParticipantId);
			ScoreSubmissionData.TeamScores.Emplace(TeamId->GetTeamId(), TeamId->GetPlatform(), Score.Score.ToString());
		}

		Task->SetRequestBody(ScoreSubmissionData.ToJson());
	}

	FString UriPath = FString::Printf(TEXT("/v1/npServiceLabels/%u/events2/%s/reports/%s"), NpTournamentServiceLabel, *MatchId->ToString(), *ResourceName);

	Task->SetRequest(ENpApiGroup::Tournament, MoveTemp(UriPath), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_POST);

	return Task;
}

void FOnlineAsyncTaskPS4SubmitTournamentMatchResults::ProcessResult(FWebApiPS4Task& CompletedTask)
{
	if (Result.WasSuccessful() && !CompletedTask.WasSuccessful())
	{
		Result = CompletedTask.GetErrorResult();
	}
}

void FOnlineAsyncTaskPS4SubmitTournamentMatchResults::Finalize()
{
	// No-Op
}

void FOnlineAsyncTaskPS4SubmitTournamentMatchResults::TriggerDelegates()
{
	Delegate.ExecuteIfBound(Result);
}
