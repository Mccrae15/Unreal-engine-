// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../OnlineTournamentPS4InterfaceTypes.h"

class FOnlineAsyncTaskPS4QueryTournamentTeamDetails
	: public FOnlineAsyncWebTaskListPS4
{
public:
	FOnlineAsyncTaskPS4QueryTournamentTeamDetails(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TArray<TSharedRef<const FOnlineTournamentTeamId>>& InTeamIds, const FOnlineTournamentQueryTeamDetailsComplete& InDelegate);

	virtual FString ToString() const override;
	virtual bool WasSuccessful() const override;

	virtual TArray<FWebApiPS4Task> CreateWebTasks() override;
	virtual void ProcessResult(FWebApiPS4Task& CompletedTask, const int32 TaskIndex) override;
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	SceUserServiceUserId SonyUserId;
	FNpWebApiUserContext WebApiUserContext;
	SceNpServiceLabel NpTournamentServiceLabel;
	TArray<TSharedRef<const FOnlineTournamentTeamId>> TeamIds;
	FOnlineTournamentQueryTeamDetailsComplete Delegate;

	TArray<TSharedRef<const FOnlineTournamentId>> TournamentIds;

	FOnlineError Result;
	TOptional<TArray<TSharedRef<const IOnlineTournamentTeamDetails>>> OptionalTeamDetails;
};
