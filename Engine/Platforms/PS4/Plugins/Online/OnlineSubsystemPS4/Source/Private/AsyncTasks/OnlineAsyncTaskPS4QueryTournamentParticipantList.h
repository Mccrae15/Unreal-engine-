// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../OnlineTournamentPS4InterfaceTypes.h"

class FOnlineAsyncTaskPS4QueryTournamentParticipantList
	: public FOnlineAsyncWebTaskPS4
{
public:
	FOnlineAsyncTaskPS4QueryTournamentParticipantList(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TSharedRef<const FOnlineTournamentId> InTournamentId, const FOnlineTournamentParticipantQueryFilter& InQueryFilter, const FOnlineTournamentQueryParticipantListComplete& InDelegate);

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
	FOnlineTournamentParticipantQueryFilter QueryFilter;
	FOnlineTournamentQueryParticipantListComplete Delegate;

	FOnlineError Result;
	TOptional<uint32> OptionalTotalResults;
	TOptional<TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>> OptionalParticipantDetails;
};
