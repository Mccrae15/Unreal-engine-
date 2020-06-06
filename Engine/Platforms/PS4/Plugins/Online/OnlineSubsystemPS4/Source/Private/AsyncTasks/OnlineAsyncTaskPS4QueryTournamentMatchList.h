// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../OnlineTournamentPS4InterfaceTypes.h"

class FOnlineAsyncTaskPS4QueryTournamentMatchList
	: public FOnlineAsyncWebTaskPS4
{
public:
	FOnlineAsyncTaskPS4QueryTournamentMatchList(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TSharedRef<const FOnlineTournamentId> InTournamentId, const FOnlineTournamentQueryMatchListComplete& InDelegate);

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
	TSharedRef<const FOnlineTournamentId> TournamentId;
	FOnlineTournamentQueryMatchListComplete Delegate;

	FOnlineError Result;
	TOptional<TArray<TSharedRef<const FOnlineTournamentMatchId>>> OptionalMatchIds;
};