// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineTournamentInterfacePS4.h"
#include "OnlineAsyncTaskManager.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4.h"
#include "WebApiPS4Task.h"
#include "OnlineError.h"
#include "Serialization/JsonSerializerMacros.h"
#include "OnlineTournamentPS4InterfaceTypes.h"
#include "AsyncTasks/OnlineAsyncTaskPS4QueryTournamentDetails.h"
#include "AsyncTasks/OnlineAsyncTaskPS4QueryTournamentList.h"
#include "AsyncTasks/OnlineAsyncTaskPS4QueryTournamentMatchDetails.h"
#include "AsyncTasks/OnlineAsyncTaskPS4QueryTournamentMatchList.h"
#include "AsyncTasks/OnlineAsyncTaskPS4QueryTournamentParticipantList.h"
#include "AsyncTasks/OnlineAsyncTaskPS4QueryTournamentTeamDetails.h"
#include "AsyncTasks/OnlineAsyncTaskPS4SubmitTournamentMatchResults.h"

THIRD_PARTY_INCLUDES_START
#include <np/np_event.h>
THIRD_PARTY_INCLUDES_END

FOnlineTournamentPS4::FOnlineTournamentPS4(FOnlineSubsystemPS4& InSubsystem)
	: Subsystem(InSubsystem)
	, NpTournamentServiceLabel(SCE_NP_INVALID_SERVICE_LABEL)
{
}

void FOnlineTournamentPS4::Init()
{
	int32 ConfigServiceLabel;
	if (GConfig->GetInt(TEXT("TournamentsPS4"), TEXT("NpServiceLabel"), ConfigServiceLabel, GEngineIni))
	{
		NpTournamentServiceLabel = static_cast<SceNpServiceLabel>(ConfigServiceLabel);
	}
	else
	{
		NpTournamentServiceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
	}
}

namespace
{
	static bool IsTournamentQueryFilterValid(const FOnlineTournamentQueryFilter& QueryFilter)
	{
		if (QueryFilter.Format.IsSet())
		{
			// Filter not supported by PSN
			return false;
		}

		// Cannot specify both a TeamId and a Player (they are exclusive)
		if (QueryFilter.TeamId.IsSet() && QueryFilter.PlayerId.IsSet())
		{
			return false;
		}

		return true;
	}
}

void FOnlineTournamentPS4::QueryTournamentList(const TSharedRef<const FUniqueNetId> UserId, const FOnlineTournamentQueryFilter& QueryFilter, const FOnlineTournamentQueryTournamentListComplete& Delegate)
{
	if (!UserId->IsValid())
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTournamentList] UserId is Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const FOnlineTournamentId>>>());
		});
		return;
	}

	if (!IsTournamentQueryFilterValid(QueryFilter))
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTournamentList] Tournament Filter Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const FOnlineTournamentId>>>());
		});
		return;
	}

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = FUniqueNetIdPS4::Cast(UserId);
	const FNpWebApiUserContext LocalUserContext = Subsystem.GetUserWebApiContext(*PS4UserId);
	if (LocalUserContext == INDEX_NONE)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTournamentList] LocalUser WebApiContext not found"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const FOnlineTournamentId>>>());
		});
		return;
	}

	Subsystem.CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskPS4QueryTournamentList>(Subsystem, PS4UserId->GetUserId(), NpTournamentServiceLabel, LocalUserContext, QueryFilter, Delegate);
}

TArray<TSharedRef<const FOnlineTournamentId>> FOnlineTournamentPS4::GetTournamentList(const TSharedRef<const FUniqueNetId> UserId) const
{
	const TSharedRef<const FUniqueNetIdPS4> UserIdPS4 = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);

	if (const FOnlineTournamentPS4Data* const TournamentDataCache = UserQueriedData.Find(UserIdPS4->GetUserId()))
	{
		return TournamentDataCache->QueriedTournamentIds;
	}

	return TArray<TSharedRef<const FOnlineTournamentId>>();
}

void FOnlineTournamentPS4::QueryTournamentDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentId>>& TournamentIds, const FOnlineTournamentQueryTournamentDetailsComplete& Delegate)
{
	if (!UserId->IsValid())
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTournamentDetails] UserId is Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentDetails>>>());
		});
		return;
	}

	if (TournamentIds.Num() == 0)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTournamentDetails] No Tournaments Requested"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentDetails>>>());
		});
		return;
	}

	for (TSharedRef<const FOnlineTournamentId> TournamentId : TournamentIds)
	{
		if (!TournamentId->IsValid())
		{
			Subsystem.ExecuteNextTick([Delegate]()
			{
				UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTournamentDetails] A provided tournament was invalid"));
				Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentDetails>>>());
			});
			return;
		}
	}

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = FUniqueNetIdPS4::Cast(UserId);
	const FNpWebApiUserContext LocalUserContext = Subsystem.GetUserWebApiContext(*PS4UserId);
	if (LocalUserContext == INDEX_NONE)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTournamentDetails] LocalUser WebApiContext not found"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentDetails>>>());
		});
		return;
	}

	Subsystem.CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskPS4QueryTournamentDetails>(Subsystem, PS4UserId->GetUserId(), NpTournamentServiceLabel, LocalUserContext, TournamentIds, Delegate);
}

TSharedPtr<const IOnlineTournamentDetails> FOnlineTournamentPS4::GetTournamentDetails(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentId> TournamentId) const
{
	const TSharedRef<const FUniqueNetIdPS4> UserIdPS4 = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	if (const FOnlineTournamentPS4Data* const TournamentDataCache = UserQueriedData.Find(UserIdPS4->GetUserId()))
	{
		if (const TSharedRef<const IOnlineTournamentDetails>* TournamentDetailsPtr = TournamentDataCache->QueriedTournamentDetails.Find(TournamentId))
		{
			return *TournamentDetailsPtr;
		}
	}

	return TSharedPtr<const IOnlineTournamentDetails>();
}

TArray<TSharedPtr<const IOnlineTournamentDetails>> FOnlineTournamentPS4::GetTournamentDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentId>>& TournamentIds) const
{
	TArray<TSharedPtr<const IOnlineTournamentDetails>> Results;

	const TSharedRef<const FUniqueNetIdPS4> UserIdPS4 = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	if (const FOnlineTournamentPS4Data* const TournamentDataCache = UserQueriedData.Find(UserIdPS4->GetUserId()))
	{
		for (TSharedRef<const FOnlineTournamentId> TournamentId : TournamentIds)
		{
			if (const TSharedRef<const IOnlineTournamentDetails>* TournamentDetailsPtr = TournamentDataCache->QueriedTournamentDetails.Find(TournamentId))
			{
				Results.Add(*TournamentDetailsPtr);
			}
			else
			{
				Results.Emplace();
			}
		}
	}
	return Results;
}

void FOnlineTournamentPS4::QueryMatchList(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentId> TournamentId, const FOnlineTournamentQueryMatchListComplete& Delegate)
{
	if (!UserId->IsValid())
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryMatchList] UserId is Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const FOnlineTournamentMatchId>>>());
		});
		return;
	}

	if (!TournamentId->IsValid())
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryMatchList] TournamentId is Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const FOnlineTournamentMatchId>>>());
		});
		return;
	}

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = FUniqueNetIdPS4::Cast(UserId);
	const FNpWebApiUserContext LocalUserContext = Subsystem.GetUserWebApiContext(*PS4UserId);
	if (LocalUserContext == INDEX_NONE)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryMatchList] LocalUser WebApiContext not found"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const FOnlineTournamentMatchId>>>());
		});
		return;
	}

	Subsystem.CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskPS4QueryTournamentMatchList>(Subsystem, PS4UserId->GetUserId(), NpTournamentServiceLabel, LocalUserContext, TournamentId, Delegate);

}

TArray<TSharedRef<const FOnlineTournamentMatchId>> FOnlineTournamentPS4::GetMatchList(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentId> TournamentId) const
{
	const TSharedRef<const FUniqueNetIdPS4> UserIdPS4 = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	if (const FOnlineTournamentPS4Data* const TournamentDataCache = UserQueriedData.Find(UserIdPS4->GetUserId()))
	{
		if (const TArray<TSharedRef<const FOnlineTournamentMatchId>>* MatchIds = TournamentDataCache->QueriedTournamentMatchIds.Find(TournamentId))
		{
			return *MatchIds;
		}
	}

	return TArray<TSharedRef<const FOnlineTournamentMatchId>>();
}

void FOnlineTournamentPS4::QueryMatchDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentMatchId>>& MatchIds, const FOnlineTournamentQueryMatchDetailsComplete& Delegate)
{
	if (!UserId->IsValid())
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryMatchDetails] UserId is Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentMatchDetails>>>());
		});
		return;
	}

	if (MatchIds.Num() == 0)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryMatchDetails] No Matches Requested"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentMatchDetails>>>());
		});
		return;
	}

	for (TSharedRef<const FOnlineTournamentMatchId> MatchId : MatchIds)
	{
		if (!MatchId->IsValid())
		{
			Subsystem.ExecuteNextTick([Delegate]()
			{
				UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryMatchDetails] A provided MatchId is Invalid"));
				Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentMatchDetails>>>());
			});
			return;
		}
	}

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = FUniqueNetIdPS4::Cast(UserId);
	const FNpWebApiUserContext LocalUserContext = Subsystem.GetUserWebApiContext(*PS4UserId);
	if (LocalUserContext == INDEX_NONE)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryMatchDetails] LocalUser WebApiContext not found"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentMatchDetails>>>());
		});
		return;
	}

	Subsystem.CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskPS4QueryTournamentMatchDetails>(Subsystem, PS4UserId->GetUserId(), NpTournamentServiceLabel, LocalUserContext, MatchIds, Delegate);
}

TSharedPtr<const IOnlineTournamentMatchDetails> FOnlineTournamentPS4::GetMatchDetails(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentMatchId> MatchId) const
{
	const TSharedRef<const FUniqueNetIdPS4> UserIdPS4 = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	if (const FOnlineTournamentPS4Data* const TournamentDataCache = UserQueriedData.Find(UserIdPS4->GetUserId()))
	{
		if (const TSharedRef<const IOnlineTournamentMatchDetails>* MatchDetailsPtr = TournamentDataCache->QueriedMatchDetails.Find(MatchId))
		{
			return *MatchDetailsPtr;
		}
	}

	return TSharedPtr<const IOnlineTournamentMatchDetails>();
}

TArray<TSharedPtr<const IOnlineTournamentMatchDetails>> FOnlineTournamentPS4::GetMatchDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentMatchId>>& MatchIds) const
{
	TArray<TSharedPtr<const IOnlineTournamentMatchDetails>> Results;

	const TSharedRef<const FUniqueNetIdPS4> UserIdPS4 = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	if (const FOnlineTournamentPS4Data* const TournamentDataCache = UserQueriedData.Find(UserIdPS4->GetUserId()))
	{
		for (TSharedRef<const FOnlineTournamentMatchId> MatchId : MatchIds)
		{
			if (const TSharedRef<const IOnlineTournamentMatchDetails>* MatchDetailsPtr = TournamentDataCache->QueriedMatchDetails.Find(MatchId))
			{
				Results.Add(*MatchDetailsPtr);
			}
			else
			{
				Results.Emplace();
			}
		}
	}
	return Results;
}

void FOnlineTournamentPS4::QueryParticipantList(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentId> TournamentId, const FOnlineTournamentParticipantQueryFilter& QueryFilter, const FOnlineTournamentQueryParticipantListComplete& Delegate)
{
	if (!UserId->IsValid())
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryParticipantList] UserId is Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<uint32>(), TOptional<TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>());
		});
		return;
	}

	if (!TournamentId->IsValid())
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryParticipantList] TournamentId is Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<uint32>(), TOptional<TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>());
		});
		return;
	}

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = FUniqueNetIdPS4::Cast(UserId);

	// Check if we're trying to query team information for an individual player tournament
	if (QueryFilter.ParticipantType == EOnlineTournamentParticipantType::Team)
	{
		if (const FOnlineTournamentPS4Data* const TournamentDataCache = UserQueriedData.Find(PS4UserId->GetUserId()))
		{
			if (const TSharedRef<const IOnlineTournamentDetails>* const Details = TournamentDataCache->QueriedTournamentDetails.Find(TournamentId))
			{
				if ((*Details)->GetParticipantType() != EOnlineTournamentParticipantType::Team)
				{
					Subsystem.ExecuteNextTick([Delegate]()
					{
						UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryParticipantList] Queried Team Participants on an Individual Tournament"));
						Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<uint32>(), TOptional<TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>());
					});
					return;
				}
			}
		}
	}

	const FNpWebApiUserContext LocalUserContext = Subsystem.GetUserWebApiContext(*PS4UserId);
	if (LocalUserContext == INDEX_NONE)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryParticipantList] LocalUser WebApiContext not found"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<uint32>(), TOptional<TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>());
		});
		return;
	}

	Subsystem.CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskPS4QueryTournamentParticipantList>(Subsystem, PS4UserId->GetUserId(), NpTournamentServiceLabel, LocalUserContext, TournamentId, QueryFilter, Delegate);
}

TArray<TSharedRef<const IOnlineTournamentParticipantDetails>> FOnlineTournamentPS4::GetParticipantList(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentId> TournamentId, const EOnlineTournamentParticipantType ParticipantType) const
{
	const TSharedRef<const FUniqueNetIdPS4> UserIdPS4 = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	if (const FOnlineTournamentPS4Data* const TournamentDataCache = UserQueriedData.Find(UserIdPS4->GetUserId()))
	{
		if (const TMap<EOnlineTournamentParticipantType, TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>* const Participants = TournamentDataCache->QueriedParticipantList.Find(TournamentId))
		{
			if (const TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>* ParticipantDetails = Participants->Find(ParticipantType))
			{
				return *ParticipantDetails;
			}
		}
	}

	return TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>();
}

void FOnlineTournamentPS4::QueryTeamDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentTeamId>>& TeamIds, const FOnlineTournamentQueryTeamDetailsComplete& Delegate)
{
	if (!UserId->IsValid())
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTeamDetails] UserId is Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentTeamDetails>>>());
		});
		return;
	}

	if (TeamIds.Num() == 0)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTeamDetails] No Teams Requested"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentTeamDetails>>>());
		});
		return;
	}

	for (TSharedRef<const FOnlineTournamentMatchId> TeamId : TeamIds)
	{
		if (!TeamId->IsValid())
		{
			Subsystem.ExecuteNextTick([Delegate]()
			{
				UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTeamDetails] Invalid TeamId provided"));
				Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentTeamDetails>>>());
			});
			return;
		}
	}

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = FUniqueNetIdPS4::Cast(UserId);
	const FNpWebApiUserContext LocalUserContext = Subsystem.GetUserWebApiContext(*PS4UserId);
	if (LocalUserContext == INDEX_NONE)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[QueryTeamDetails] LocalUser WebApiContext not found"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown), TOptional<TArray<TSharedRef<const IOnlineTournamentTeamDetails>>>());
		});
		return;
	}

	Subsystem.CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskPS4QueryTournamentTeamDetails>(Subsystem, PS4UserId->GetUserId(), NpTournamentServiceLabel, LocalUserContext, TeamIds, Delegate);
}

TSharedPtr<const IOnlineTournamentTeamDetails> FOnlineTournamentPS4::GetTeamDetails(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentTeamId> TeamId) const
{
	const TSharedRef<const FUniqueNetIdPS4> UserIdPS4 = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	if (const FOnlineTournamentPS4Data* const TournamentDataCache = UserQueriedData.Find(UserIdPS4->GetUserId()))
	{
		if (const TSharedRef<const IOnlineTournamentTeamDetails>* TeamDetailsPtr = TournamentDataCache->QueriedTeamDetails.Find(TeamId))
		{
			return *TeamDetailsPtr;
		}
	}

	return TSharedPtr<const IOnlineTournamentTeamDetails>();
}

TArray<TSharedPtr<const IOnlineTournamentTeamDetails>> FOnlineTournamentPS4::GetTeamDetails(const TSharedRef<const FUniqueNetId> UserId, const TArray<TSharedRef<const FOnlineTournamentTeamId>>& TeamIds) const
{
	TArray<TSharedPtr<const IOnlineTournamentTeamDetails>> Results;

	const TSharedRef<const FUniqueNetIdPS4> UserIdPS4 = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	if (const FOnlineTournamentPS4Data* const TournamentDataCache = UserQueriedData.Find(UserIdPS4->GetUserId()))
	{
		for (TSharedRef<const FOnlineTournamentTeamId> TeamId : TeamIds)
		{
			if (const TSharedRef<const IOnlineTournamentTeamDetails>* TeamDetailsPtr = TournamentDataCache->QueriedTeamDetails.Find(TeamId))
			{
				Results.Add(*TeamDetailsPtr);
			}
			else
			{
				Results.Emplace();
			}
		}
	}
	return Results;
}

void FOnlineTournamentPS4::SubmitMatchResults(const TSharedRef<const FUniqueNetId> UserId, const TSharedRef<const FOnlineTournamentMatchId> MatchId, const FOnlineTournamentMatchResults& MatchResults, const FOnlineTournamentSubmitMatchResultsComplete& Delegate)
{
	if (!UserId->IsValid())
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[SubmitMatchResults] UserId is Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown));
		});
		return;
	}

	if (!MatchId->IsValid())
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[SubmitMatchResults] MatchId is Invalid"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown));
		});
		return;
	}

	if (MatchResults.ScoresToSubmit.Num() == 0)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[SubmitMatchResults] No Scores to Submit"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown));
		});
		return;
	}

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = FUniqueNetIdPS4::Cast(UserId);
	const FNpWebApiUserContext LocalUserContext = Subsystem.GetUserWebApiContext(*PS4UserId);
	if (LocalUserContext == INDEX_NONE)
	{
		Subsystem.ExecuteNextTick([Delegate]()
		{
			UE_LOG_ONLINE_TOURNAMENT(Warning, TEXT("[SubmitMatchResults] LocalUser WebApiContext not found"));
			Delegate.ExecuteIfBound(FOnlineError(EOnlineErrorResult::Unknown));
		});
		return;
	}

	Subsystem.CreateAndDispatchAsyncTaskParallel<FOnlineAsyncTaskPS4SubmitTournamentMatchResults>(Subsystem, PS4UserId->GetUserId(), NpTournamentServiceLabel, LocalUserContext, MatchId, MatchResults, Delegate);

}

FDelegateHandle FOnlineTournamentPS4::AddOnOnlineTournamentTournamentJoined(const FOnOnlineTournamentTournamentJoinedDelegate& Delegate)
{
	return OnTournamentJoined.Add(Delegate);
}

void FOnlineTournamentPS4::RemoveOnOnlineTournamentTournamentJoined(const FDelegateHandle& DelegateHandle)
{
	OnTournamentJoined.Remove(DelegateHandle);
}

FDelegateHandle FOnlineTournamentPS4::AddOnOnlineTournamentMatchJoinedDelegate(const FOnOnlineTournamentMatchJoinedDelegate& Delegate)
{
	return OnMatchJoined.Add(Delegate);
}

void FOnlineTournamentPS4::RemoveOnOnlineTournamentMatchJoinedDelegate(const FDelegateHandle& DelegateHandle)
{
	OnMatchJoined.Remove(DelegateHandle);
}

namespace
{
	void PrintParticipant(const TCHAR* const Padding, const TSharedRef<const IOnlineTournamentParticipantDetails> Participant)
	{
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("%sTournamentId: %s"),	Padding, *Participant->GetTournamentId()->ToString());
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("%sPlayerId: %s"),		Padding, Participant->GetPlayerId().IsValid() ? *Participant->GetPlayerId()->ToDebugString() : TEXT("Invalid"));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("%sTeamId: %s"),			Padding, Participant->GetTeamId().IsValid() ? *Participant->GetTeamId()->ToDebugString() : TEXT("Invalid"));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("%sDisplayName: %s"),	Padding, *Participant->GetDisplayName());
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("%sState: %s"),			Padding, *LexToString(Participant->GetState()));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("%sPosition: %d"),		Padding, Participant->GetPosition().IsSet() ? Participant->GetPosition().GetValue() : -1);
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("%sScore: %s"),			Padding, Participant->GetScore().IsSet() ? *Participant->GetScore()->ToString() : TEXT("Unset"));

		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("%sAttributes:"), Padding);
		const TSharedRef<const FOnlineTournamentParticipantDetailsPS4> PS4Participant = StaticCastSharedRef<const FOnlineTournamentParticipantDetailsPS4>(Participant);
		for (const TPair<FName, FVariantData>& AttributePair : PS4Participant->Attributes)
		{
			UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("%s  %s: %s"), Padding, *AttributePair.Key.ToString(), *AttributePair.Value.ToString());
		}
	}
}

#if !UE_BUILD_SHIPPING
void FOnlineTournamentPS4::DumpCachedTournamentInfo(const TSharedRef<const FUniqueNetId> UserId) const
{
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("==========================="));
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("===== Tournament Info ====="));
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("==========================="));

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	const FOnlineTournamentPS4Data* UserData = UserQueriedData.Find(PS4UserId->GetUserId());
	if (!UserData)
	{
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("No Data for %s"), *UserId->ToString());
		return;
	}

	// Print cached tournament ids
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Queried TournamentIds for %s:"), *UserId->ToString());
	for (const TSharedRef<const FOnlineTournamentId> TournamentId : UserData->QueriedTournamentIds)
	{
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("  %s"), *TournamentId->ToString());
	}
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT(""));

	// Print cached tournament details
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Queried Tournament Details for %s:"), *UserId->ToString());
	for (const TPair<TSharedRef<const FOnlineTournamentId>, TSharedRef<const IOnlineTournamentDetails>>& TournmentDetailsPair : UserData->QueriedTournamentDetails)
	{
		const FOnlineTournamentDetailsPS4& Details = static_cast<const FOnlineTournamentDetailsPS4&>(*TournmentDetailsPair.Value);
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("  Tournament %s"), *TournmentDetailsPair.Key->ToString());
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    TournamentId: %s"), *Details.GetTournamentId()->ToDebugString());
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Title: %s"), *Details.GetTitle());
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Description: %s"), *Details.GetDescription().ReplaceCharWithEscapedChar());
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    State: %s"), *LexToString(Details.GetState()));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Format: %s"), *LexToString(Details.GetFormat()));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    ParticipantType: %s"), *LexToString(Details.GetParticipantType()));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    RegistrationStart Date: %s"), Details.GetRegistrationStartDateUTC().IsSet() ? *Details.GetRegistrationStartDateUTC().GetValue().ToString() : TEXT("Unset"));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    RegistrationEnd Date: %s"), Details.GetRegistrationEndDateUTC().IsSet() ? *Details.GetRegistrationEndDateUTC().GetValue().ToString() : TEXT("Unset"));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Start Date: %s"), Details.GetStartDateUTC().IsSet() ? *Details.GetStartDateUTC().GetValue().ToString() : TEXT("Unset"));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Check-In Minutes: %s"), Details.GetCheckInTimespan().IsSet() ? *Details.GetCheckInTimespan().GetValue().ToString() : TEXT("Unset"));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    End Date: %s"), Details.GetEndDateUTC().IsSet() ? *Details.GetEndDateUTC().GetValue().ToString() : TEXT("Unset"));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Last Updated: %s"), Details.GetLastUpdatedDateUTC().IsSet() ? *Details.GetLastUpdatedDateUTC().GetValue().ToString() : TEXT("Unset"));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    bRequiresPremium: %d"), Details.RequiresPremiumSubscription().Get(false));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Participants: "));
		for (const TSharedRef<const IOnlineTournamentParticipantDetails>& Participant : Details.GetParticipants())
		{
			PrintParticipant(TEXT("    "), Participant);
		}
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Attributes: "));
		for (const TPair<FName, FVariantData>& AttributePair : Details.Attributes)
		{
			UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("      %s: %s"), *AttributePair.Key.ToString(), *AttributePair.Value.ToString());
		}
	}
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT(""));
}

void FOnlineTournamentPS4::DumpCachedMatchInfo(const TSharedRef<const FUniqueNetId> UserId) const
{
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("==========================="));
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("======== Match Info ======="));
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("==========================="));

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	const FOnlineTournamentPS4Data* UserData = UserQueriedData.Find(PS4UserId->GetUserId());
	if (!UserData)
	{
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("No Data for %s"), *UserId->ToString());
		return;
	}

	// Print cached tournament ids
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Queried Match Ids for %s:"), *UserId->ToString());
	for (const TPair<TSharedRef<const FOnlineTournamentId>, TArray<TSharedRef<const FOnlineTournamentId>>>& TournamentMatchPair : UserData->QueriedTournamentMatchIds)
	{
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("  Tournament %s"), *TournamentMatchPair.Key->ToDebugString());
		for (const TSharedRef<const FOnlineTournamentId> MatchId : TournamentMatchPair.Value)
		{
			UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    %s"), *MatchId->ToDebugString());
		}
	}
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT(""));

	// Print cached tournament details
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Queried Match Details for %s:"), *UserId->ToString());
	for (const TPair<TSharedRef<const FOnlineTournamentMatchId>, TSharedRef<const IOnlineTournamentMatchDetails>>& MatchDetailsPair : UserData->QueriedMatchDetails)
	{
		const FOnlineTournamentMatchDetailsPS4& Details = static_cast<const FOnlineTournamentMatchDetailsPS4&>(*MatchDetailsPair.Value);
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("  Match %s"), *MatchDetailsPair.Key->ToString());
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    MatchId: %s"), *MatchDetailsPair.Key->ToDebugString());
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    ParticipantType: %s"), *LexToString(Details.GetParticipantType()));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Match State: %s"), *LexToString(Details.GetMatchState()));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Bracket: %s"), Details.GetBracket().IsSet() ? *Details.GetBracket().GetValue() : TEXT("Unset"));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Round: %d"), Details.GetRound().IsSet() ? Details.GetRound().GetValue() : -1);
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Start Date: %s"), Details.GetStartDateUTC().IsSet() ? *Details.GetStartDateUTC().GetValue().ToString() : TEXT("Unset"));
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Participants: "));
		for (const TSharedRef<const IOnlineTournamentParticipantDetails>& Participant : Details.GetParticipants())
		{
			PrintParticipant(TEXT("    "), Participant);
		}
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Attributes: "));
		for (const TPair<FName, FVariantData>& AttributePair : Details.Attributes)
		{
			UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("      %s: %s"), *AttributePair.Key.ToString(), *AttributePair.Value.ToString());
		}
	}
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT(""));
}

void FOnlineTournamentPS4::DumpCachedParticipantInfo(const TSharedRef<const FUniqueNetId> UserId) const
{
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("==========================="));
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("===== Participant Info ===="));
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("==========================="));

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	const FOnlineTournamentPS4Data* UserData = UserQueriedData.Find(PS4UserId->GetUserId());
	if (!UserData)
	{
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("No Data for %s"), *UserId->ToString());
		return;
	}

	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Queried Participant Details for %s:"), *UserId->ToString());
	for (const TPair<TSharedRef<const FOnlineTournamentId>, TMap<EOnlineTournamentParticipantType, TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>>& TournamentParticipantTypePair: UserData->QueriedParticipantList)
	{
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("  Tournament %s"), *TournamentParticipantTypePair.Key->ToString());

		for (const TPair<EOnlineTournamentParticipantType, TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>& ParticipantTypeParticipantsPair : TournamentParticipantTypePair.Value)
		{
			UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Participant Type %s"), *LexToString(ParticipantTypeParticipantsPair.Key));
			for (const TSharedRef<const IOnlineTournamentParticipantDetails> Participant : ParticipantTypeParticipantsPair.Value)
			{
				PrintParticipant(TEXT("      "), Participant);
			}
		}
	}
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT(""));
}

void FOnlineTournamentPS4::DumpCachedTeamInfo(const TSharedRef<const FUniqueNetId> UserId) const
{

	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("==========================="));
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("======== Team Info ========"));
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("==========================="));

	const TSharedRef<const FUniqueNetIdPS4> PS4UserId = StaticCastSharedRef<const FUniqueNetIdPS4>(UserId);
	const FOnlineTournamentPS4Data* UserData = UserQueriedData.Find(PS4UserId->GetUserId());
	if (!UserData)
	{
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("No Data for %s"), *UserId->ToString());
		return;
	}

	// Print cached team details
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Queried Team Details for %s:"), *UserId->ToString());
	for (const TPair<TSharedRef<const FOnlineTournamentTeamId>, TSharedRef<const IOnlineTournamentTeamDetails>>& TeamDetailsPair : UserData->QueriedTeamDetails)
	{
		const FOnlineTournamentTeamDetailsPS4& Details = static_cast<const FOnlineTournamentTeamDetailsPS4&>(*TeamDetailsPair.Value);
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("  Team %s"), *TeamDetailsPair.Key->ToString())
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    TeamId: %s"), *Details.GetTeamId()->ToDebugString());
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    DisplayName: %s"), *Details.GetDisplayName());
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Members: "));
		TOptional<TArray<TSharedRef<const FUniqueNetId>>> OptionalPlayerIds = Details.GetPlayerIds();
		if (OptionalPlayerIds.IsSet())
		{
			for (const TSharedRef<const FUniqueNetId>& Member : OptionalPlayerIds.GetValue())
			{
				UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("      %s"), *Member->ToDebugString());
			}
		}
		else
		{
			UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("      Members Unset"));
		}
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("    Attributes: "));
		for (const TPair<FName, FVariantData>& AttributePair : Details.Attributes)
		{
			UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("      %s: %s"), *AttributePair.Key.ToString(), *AttributePair.Value.ToString());
		}
	}
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT(""));
}
#endif // !UE_BUILD_SHIPPING

void FOnlineTournamentPS4::OnTournamentJoinEvent(const SceNpEventJoinEventParam& Param)
{
	TSharedRef<const FUniqueNetId> UserId = FUniqueNetIdPS4::FindOrCreate(Param.userId);

	FString TournamentIdString = ANSI_TO_TCHAR(Param.eventId);
	TSharedRef<const FOnlineTournamentId> TournamentId = MakeShared<const FOnlineTournamentIdPS4>(MoveTemp(TournamentIdString));

	FAdditionalMetaDataMap MetaData;
	MetaData.Emplace(TEXT("BootArguments"), UTF8_TO_TCHAR(Param.bootArgument));

	OnTournamentJoined.Broadcast(UserId, TournamentId, MetaData);
}

void FOnlineTournamentPS4::OnTournamentJoinMatchEvent(const SceNpEventJoinMatchEventParam& Param)
{
	TSharedRef<const FUniqueNetId> UserId = FUniqueNetIdPS4::FindOrCreate(Param.userId);

	FString MatchIdString = ANSI_TO_TCHAR(Param.eventId);
	TSharedRef<const FOnlineTournamentMatchId> MatchId = MakeShared<const FOnlineTournamentMatchIdPS4>(MoveTemp(MatchIdString));

	FAdditionalMetaDataMap MetaData;
	FString BootArguments = UTF8_TO_TCHAR(Param.bootArgument);
	MetaData.Emplace(TEXT("BootArguments"), MoveTemp(BootArguments));

	OnMatchJoined.Broadcast(UserId, MatchId, MetaData);
}

void FOnlineTournamentPS4::OnTournamentJoinTeamMatchEvent(const SceNpEventJoinTeamOnTeamMatchEventParam& Param)
{
	TSharedRef<const FUniqueNetId> UserId = FUniqueNetIdPS4::FindOrCreate(Param.userId);

	FString MatchIdString = ANSI_TO_TCHAR(Param.eventId);
	TSharedRef<const FOnlineTournamentMatchId> MatchId = MakeShared<const FOnlineTournamentMatchIdPS4>(MoveTemp(MatchIdString));

	FAdditionalMetaDataMap MetaData;
	FString BootArguments = UTF8_TO_TCHAR(Param.bootArgument);
	MetaData.Emplace(TEXT("BootArguments"), MoveTemp(BootArguments));

	OnMatchJoined.Broadcast(UserId, MatchId, MetaData);
}

void FOnlineTournamentPS4::TestQueryTournamentInfo(const TSharedRef<const FUniqueNetId> UserId)
{
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Testing Tournaments for %s"), *UserId->ToDebugString());

	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Querying Tournament List"), *UserId->ToDebugString());
	QueryTournamentList(UserId, FOnlineTournamentQueryFilter(), FOnlineTournamentQueryTournamentListComplete::CreateThreadSafeSP(this, &FOnlineTournamentPS4::OnTestQueryTournamentListComplete, UserId));
}

void FOnlineTournamentPS4::OnTestQueryTournamentListComplete(const FOnlineError& ResultStatus, const TOptional<TArray<TSharedRef<const FOnlineTournamentId>>>& TournamentIds, const TSharedRef<const FUniqueNetId> UserId)
{
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Tournament List Query Complete.  Result=[%s] TournamentsFound=[%d]"), *ResultStatus.ToLogString(), TournamentIds.Get(TArray<TSharedRef<const FOnlineTournamentId>>()).Num());

	if (TournamentIds.IsSet() && TournamentIds->Num() > 0)
	{
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Querying Tournament %d Details"), TournamentIds->Num());
		QueryTournamentDetails(UserId, TournamentIds.GetValue(), FOnlineTournamentQueryTournamentDetailsComplete::CreateThreadSafeSP(this, &FOnlineTournamentPS4::OnTestQueryTournamentTournamentDetailsComplete, UserId));
	}
}

void FOnlineTournamentPS4::OnTestQueryTournamentTournamentDetailsComplete(const FOnlineError& ResultStatus, const TOptional<TArray<TSharedRef<const IOnlineTournamentDetails>>>& TournamentDetails, const TSharedRef<const FUniqueNetId> UserId)
{
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Tournament Details Query Complete.  Result=[%s] TournamentDetailsFound=[%d]"), *ResultStatus.ToLogString(), TournamentDetails.Get(TArray<TSharedRef<const IOnlineTournamentDetails>>()).Num());

	if (TournamentDetails.IsSet() && TournamentDetails->Num() > 0)
	{
		for (const TSharedRef<const IOnlineTournamentDetails> Details : TournamentDetails.GetValue())
		{
			UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Querying Tournament %s Match List"), *Details->GetTournamentId()->ToString());
			QueryMatchList(UserId, Details->GetTournamentId(), FOnlineTournamentQueryMatchListComplete::CreateThreadSafeSP(this, &FOnlineTournamentPS4::OnTestQueryTournamentMatchListComplete, UserId));

			UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Querying Tournament %s Participants List"), *Details->GetTournamentId()->ToString());
			QueryParticipantList(UserId, Details->GetTournamentId(), FOnlineTournamentParticipantQueryFilter(Details->GetParticipantType()), FOnlineTournamentQueryParticipantListComplete::CreateThreadSafeSP(this, &FOnlineTournamentPS4::OnTestQueryTournamentParticipantListComplete, UserId, Details->GetParticipantType()));
		}
	}
}

void FOnlineTournamentPS4::OnTestQueryTournamentMatchListComplete(const FOnlineError& ResultStatus, const TOptional<TArray<TSharedRef<const FOnlineTournamentMatchId>>>& MatchIds, const TSharedRef<const FUniqueNetId> UserId)
{
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Tournament Match List Query Complete.  Result=[%s] MatchIdsFound=[%d]"), *ResultStatus.ToLogString(), MatchIds.Get(TArray<TSharedRef<const FOnlineTournamentMatchId>>()).Num());

	if (MatchIds.IsSet() && MatchIds->Num() > 0)
	{
		UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Querying Tournament %d Match Details"), MatchIds->Num());
		QueryMatchDetails(UserId, MatchIds.GetValue(), FOnlineTournamentQueryMatchDetailsComplete::CreateThreadSafeSP(this, &FOnlineTournamentPS4::OnTestQueryTournamentMatchDetailsComplete, UserId));
	}
}

void FOnlineTournamentPS4::OnTestQueryTournamentMatchDetailsComplete(const FOnlineError& ResultStatus, const TOptional<TArray<TSharedRef<const IOnlineTournamentMatchDetails>>>& MatchDetails, const TSharedRef<const FUniqueNetId> UserId)
{
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Tournament Match Details Query Complete.  Result=[%s] MatchDetailsFound=[%d]"), *ResultStatus.ToLogString(), MatchDetails.Get(TArray<TSharedRef<const IOnlineTournamentMatchDetails>>()).Num());
}

void FOnlineTournamentPS4::OnTestQueryTournamentParticipantListComplete(const FOnlineError& ResultStatus, const TOptional<uint32> TotalResults, const TOptional<TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>>& ParticipantList, const TSharedRef<const FUniqueNetId> UserId, const EOnlineTournamentParticipantType ParticipantType)
{
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Tournament Participant List Query Complete.  Result=[%s] ParticipantsFound=[%d]"), *ResultStatus.ToLogString(), ParticipantList.Get(TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>()).Num());


	if (ParticipantList.IsSet() && ParticipantList->Num() > 0)
	{
		if (ParticipantType == EOnlineTournamentParticipantType::Team)
		{
			TArray<TSharedRef<const FOnlineTournamentTeamId>> TeamIds;
			for (const TSharedRef<const IOnlineTournamentParticipantDetails> Participant : ParticipantList.GetValue())
			{
				TSharedPtr<const FOnlineTournamentTeamId> TeamId = Participant->GetTeamId();
				if (ensure(TeamId.IsValid()))
				{
					TeamIds.Emplace(TeamId.ToSharedRef());
				}
			}

			if (TeamIds.Num() > 0)
			{
				UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Querying Tournament %d Team Details"), TeamIds.Num());
				QueryTeamDetails(UserId, TeamIds, FOnlineTournamentQueryTeamDetailsComplete::CreateThreadSafeSP(this, &FOnlineTournamentPS4::OnTestQueryTournamentTeamDetailsComplete, UserId));
			}
		}
	}
}

void FOnlineTournamentPS4::OnTestQueryTournamentTeamDetailsComplete(const FOnlineError& ResultStatus, const TOptional<TArray<TSharedRef<const IOnlineTournamentTeamDetails>>>& TeamDetails, const TSharedRef<const FUniqueNetId> UserId)
{
	UE_LOG_ONLINE_TOURNAMENT(Log, TEXT("Tournament Team Details Query Complete.  Result=[%s] TeamDetailsFound=[%d]"), *ResultStatus.ToLogString(), TeamDetails.Get(TArray<TSharedRef<const IOnlineTournamentTeamDetails>>()).Num());
}
