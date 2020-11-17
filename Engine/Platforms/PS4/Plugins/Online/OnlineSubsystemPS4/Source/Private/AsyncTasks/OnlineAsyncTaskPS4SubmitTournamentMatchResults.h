// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../OnlineTournamentPS4InterfaceTypes.h"

class FOnlineAsyncTaskPS4SubmitTournamentMatchResults
	: public FOnlineAsyncWebTaskPS4
{
public:
	FOnlineAsyncTaskPS4SubmitTournamentMatchResults(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TSharedRef<const FOnlineTournamentMatchId> InMatchId, const FOnlineTournamentMatchResults& InMatchResults, const FOnlineTournamentSubmitMatchResultsComplete& InDelegate);

	virtual FString ToString() const override;
	virtual bool WasSuccessful() const override;

	virtual TOptional<FWebApiPS4Task> CreateWebTask() override;
	virtual void ProcessResult(FWebApiPS4Task& CompletedTask) override;
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	SceUserServiceUserId SonyUserId;
	FNpWebApiUserContext WebApiUserContext;
	SceNpServiceLabel NpTournamentServiceLabel;
	TSharedRef<const FOnlineTournamentMatchId> MatchId;
	FOnlineTournamentMatchResults MatchResults;
	FOnlineTournamentSubmitMatchResultsComplete Delegate;

	FOnlineError Result;
};
