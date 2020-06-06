// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../OnlineTournamentPS4InterfaceTypes.h"

class FOnlineAsyncTaskPS4QueryTournamentList : public FOnlineAsyncWebTaskPS4
{
public:
	FOnlineAsyncTaskPS4QueryTournamentList(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const FOnlineTournamentQueryFilter& InQueryFilter, const FOnlineTournamentQueryTournamentListComplete& InDelegate);

	virtual FString ToString() const override;
	virtual bool WasSuccessful() const override;

	virtual TOptional<FWebApiPS4Task> CreateWebTask() override;
	virtual void ProcessResult(FWebApiPS4Task& CompletedTask) override;
	virtual void Finalize() override;
	virtual void TriggerDelegates() override;

private:
	static FString BuildQueryStringFromQueryFilter(const FOnlineTournamentQueryFilter& QueryFilter);

private:
	SceUserServiceUserId SonyUserId;
	FNpWebApiUserContext WebApiUserContext;
	SceNpServiceLabel NpTournamentServiceLabel;
	FOnlineTournamentQueryFilter QueryFilter;
	FOnlineTournamentQueryTournamentListComplete Delegate;

	FOnlineError Result;
	TOptional<TArray<TSharedRef<const FOnlineTournamentId>>> OptionalTournamentIds;
};