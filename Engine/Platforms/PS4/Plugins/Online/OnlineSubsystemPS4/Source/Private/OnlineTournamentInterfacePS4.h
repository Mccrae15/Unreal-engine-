// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineTournamentInterface.h"
#include "OnlineSubsystemPS4Package.h"
#include "OnlineTournamentPS4InterfaceTypes.h"

class FOnlineSubsystemPS4;
class FUniqueNetIdPS4;

struct SceNpEventJoinEventParam;
struct SceNpEventJoinMatchEventParam;
struct SceNpEventJoinTeamOnTeamMatchEventParam;
typedef uint32_t SceNpServiceLabel;
typedef int32_t SceUserServiceUserId;

class FOnlineTournamentPS4
	: public IOnlineTournament
{
public:
	FOnlineTournamentPS4(FOnlineSubsystemPS4& InSubsystem);
	virtual ~FOnlineTournamentPS4() = default;

	/**
	 * Initialize the interface
	 */
	void Init();

	//~ Begin IOnlineTournament Interface
	virtual void QueryTournamentList(const TSharedRef<const FUniqueNetId> UserId, const FOnlineTournamentQueryFilter& QueryFilter, const FOnlineTournamentQueryTournamentListComplete& Delegate) override final;
	virtual TArray<TSharedRef<const FOnlineTournamentId>> GetTournamentList(const TSharedRef<const FUniqueNetId> UserId) const override final;
	virtual void QueryTournamentDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentId>>& TournamentIds, const FOnlineTournamentQueryTournamentDetailsComplete& Delegate) override final;
	virtual TSharedPtr<const IOnlineTournamentDetails> GetTournamentDetails(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentId> TournamentId) const override final;
	virtual TArray<TSharedPtr<const IOnlineTournamentDetails>> GetTournamentDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentId>>& TournamentIds) const override final;
	virtual void QueryMatchList(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentId> TournamentId, const FOnlineTournamentQueryMatchListComplete& Delegate) override final;
	virtual TArray<TSharedRef<const FOnlineTournamentMatchId>> GetMatchList(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentId> TournamentId) const override final;
	virtual void QueryMatchDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentMatchId>>& MatchIds, const FOnlineTournamentQueryMatchDetailsComplete& Delegate) override final;
	virtual TSharedPtr<const IOnlineTournamentMatchDetails> GetMatchDetails(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentMatchId> MatchId) const override final;
	virtual TArray<TSharedPtr<const IOnlineTournamentMatchDetails>> GetMatchDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentMatchId>>& MatchIds) const override final;
	virtual void QueryParticipantList(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentId> TournamentId, const FOnlineTournamentParticipantQueryFilter& QueryFilter, const FOnlineTournamentQueryParticipantListComplete& Delegate) override final;
	virtual TArray<TSharedRef<const IOnlineTournamentParticipantDetails>> GetParticipantList(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentId> TournamentId, const EOnlineTournamentParticipantType ParticipantType) const override final;
	virtual void QueryTeamDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentTeamId>>& TeamIds, const FOnlineTournamentQueryTeamDetailsComplete& Delegate) override final;
	virtual TSharedPtr<const IOnlineTournamentTeamDetails> GetTeamDetails(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentTeamId> TeamId) const override final;
	virtual TArray<TSharedPtr<const IOnlineTournamentTeamDetails>> GetTeamDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentTeamId>>& TeamIds) const override final;
	virtual void SubmitMatchResults(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentMatchId> MatchId, const FOnlineTournamentMatchResults& MatchResults, const FOnlineTournamentSubmitMatchResultsComplete& Delegate) override final;
	virtual FDelegateHandle AddOnOnlineTournamentTournamentJoined(const FOnOnlineTournamentTournamentJoinedDelegate& Delegate) override final;
	virtual void RemoveOnOnlineTournamentTournamentJoined(const FDelegateHandle& DelegateHandle) override final;
	virtual FDelegateHandle AddOnOnlineTournamentMatchJoinedDelegate(const FOnOnlineTournamentMatchJoinedDelegate& Delegate) override final;
	virtual void RemoveOnOnlineTournamentMatchJoinedDelegate(const FDelegateHandle& DelegateHandle) override final;

#if !UE_BUILD_SHIPPING
	virtual void DumpCachedTournamentInfo(const TSharedRef<const FUniqueNetId> UserId) const override;
	virtual void DumpCachedMatchInfo(const TSharedRef<const FUniqueNetId> UserId) const override;
	virtual void DumpCachedParticipantInfo(const TSharedRef<const FUniqueNetId> UserId) const override;
	virtual void DumpCachedTeamInfo(const TSharedRef<const FUniqueNetId> UserId) const override;
#endif // !UE_BUILD_SHIPPING
	//~ End IOnlineTournament Interface

PACKAGE_SCOPE:
	void OnTournamentJoinEvent(const SceNpEventJoinEventParam& Param);
	void OnTournamentJoinMatchEvent(const SceNpEventJoinMatchEventParam& Param);
	void OnTournamentJoinTeamMatchEvent(const SceNpEventJoinTeamOnTeamMatchEventParam& Param);

	void TestQueryTournamentInfo(const TSharedRef<const FUniqueNetId> UserId);

private:
	void OnTestQueryTournamentListComplete(const FOnlineError& ResultStatus, const TOptional<TArray<TSharedRef<const FOnlineTournamentId>>>& TournamentIds, const TSharedRef<const FUniqueNetId> UserId);
	void OnTestQueryTournamentTournamentDetailsComplete(const FOnlineError& ResultStatus, const TOptional<TArray<TSharedRef<const IOnlineTournamentDetails>>>& TournamentDetails, const TSharedRef<const FUniqueNetId> UserId);
	void OnTestQueryTournamentMatchListComplete(const FOnlineError& ResultStatus, const TOptional<TArray<TSharedRef<const FOnlineTournamentMatchId>>>& MatchIds, const TSharedRef<const FUniqueNetId> UserId);
	void OnTestQueryTournamentMatchDetailsComplete(const FOnlineError& ResultStatus, const TOptional<TArray<TSharedRef<const IOnlineTournamentMatchDetails>>>& MatchDetails, const TSharedRef<const FUniqueNetId> UserId);
	void OnTestQueryTournamentParticipantListComplete(const FOnlineError& ResultStatus, const TOptional<uint32> TotalResults, const TOptional<TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>& ParticipantList, const TSharedRef<const FUniqueNetId> UserId, const EOnlineTournamentParticipantType ParticipantType);
	void OnTestQueryTournamentTeamDetailsComplete(const FOnlineError& ResultStatus, const TOptional<TArray<TSharedRef<const IOnlineTournamentTeamDetails>>>& TeamDetails, const TSharedRef<const FUniqueNetId> UserId);

protected:
	FOnlineSubsystemPS4& Subsystem;

PACKAGE_SCOPE:
	SceNpServiceLabel NpTournamentServiceLabel;

	struct FOnlineTournamentPS4Data
	{
		TArray<TSharedRef<const FOnlineTournamentId>> QueriedTournamentIds;
		TSharedRefMap<const FOnlineTournamentId, TSharedRef<const IOnlineTournamentDetails>> QueriedTournamentDetails;
		TSharedRefMap<const FOnlineTournamentId, TArray<TSharedRef<const FOnlineTournamentMatchId>>> QueriedTournamentMatchIds;
		TSharedRefMap<const FOnlineTournamentMatchId, TSharedRef<const IOnlineTournamentMatchDetails>> QueriedMatchDetails;
		TSharedRefMap<const FOnlineTournamentId, TMap<EOnlineTournamentParticipantType, TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>> QueriedParticipantList;
		TSharedRefMap<const FOnlineTournamentTeamId, TSharedRef<const IOnlineTournamentTeamDetails>> QueriedTeamDetails;
	};

	TMap<SceUserServiceUserId, FOnlineTournamentPS4Data> UserQueriedData;

	FOnOnlineTournamentTournamentJoined OnTournamentJoined;
	FOnOnlineTournamentMatchJoined OnMatchJoined;
};

typedef TSharedPtr<FOnlineTournamentPS4, ESPMode::ThreadSafe> FOnlineTournamentPS4Ptr;

