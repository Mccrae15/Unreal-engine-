// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// Module includes
#include "OnlineLeaderboardsInterfacePS4.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineFriendsInterfacePS4.h"
#include "OnlineIdentityInterfacePS4.h"

// Used for transforming floats to 32:32 fixed point, and vice versa
static const int64 FIXED_POINT_32_32_UNIT = 0x0000000100000000;

/**
 *	Given a leaderboard name and a stat name, find a PSN scoreboard Id that matches from the INI
 */
SceNpScoreBoardId FindScoreBoardId(const FName& LeaderboardName, const FName& StatName)
{
	int32 ScoreBoardId = -1;

	const FString CombinedKey = LeaderboardName.ToString() + "." + StatName.ToString();
	const bool Ret = GConfig->GetInt(TEXT("LeaderboardsPS4"), *CombinedKey, ScoreBoardId, GEngineIni);
	if (Ret == false)
	{
		UE_LOG_ONLINE(Error,
			TEXT("FindScoreBoardId error. LeaderboardsPS4 section not found or doesn't have %s.%s key in PS4Engine.ini"), *LeaderboardName.ToString(), *StatName.ToString());
	}

	return ScoreBoardId;
}

/**
 *	Transform a variant into a score value that PSN accepts
 *	float values are handled through fixed point notation
 */
SceNpScoreValue GetNpScoreFromVariant(const FVariantData& Stat)
{
	// SceNpScoreValue is a 64 bit signed integer
	SceNpScoreValue ScoreValue = 0;
	const EOnlineKeyValuePairDataType::Type StatType = Stat.GetType();

	// Only int32 and floats supported.
	if (StatType == EOnlineKeyValuePairDataType::Int32)
	{
		int32 Value = 0;
		Stat.GetValue(Value);
		ScoreValue = Value;
	}
	else if (StatType == EOnlineKeyValuePairDataType::Float)
	{
		float Value = 0;
		Stat.GetValue(Value);

		// Use 32:32 fixed point for float values.
		double DoubleValue = Value * FIXED_POINT_32_32_UNIT;
		ScoreValue = int64(DoubleValue);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Unsuppported score type for PS4 scoreboards: %s"), *Stat.ToString());
	}

	return ScoreValue;
}


/**
 *	Given a score value from a PSN scoreboard, transform it into a varient for a leaderboard
 */
FVariantData GetVarientFromNpScore(const EOnlineKeyValuePairDataType::Type StatType, const SceNpScoreValue Score)
{
	if (StatType == EOnlineKeyValuePairDataType::Int32)
	{
		int32 IntValue = int32(Score);
		return FVariantData(IntValue);
	}
	else if (StatType == EOnlineKeyValuePairDataType::Float)
	{
		// Transform 32:32 fixed point to float
		double CoversionFactor = 1.0 / FIXED_POINT_32_32_UNIT;
		float FloatValue = float(Score * CoversionFactor);
		return FVariantData(FloatValue);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Unsuppported score type for PS4 scoreboards: %s"), *EOnlineKeyValuePairDataType::ToString(StatType));
	}

	// Return empty data for unsupported types
	return FVariantData();
}


/**
 *	Task object for PS4 async scoreboard functions
 */
class FOnlineAsyncTaskPS4Leaderboard : public FOnlineAsyncTaskPS4
{
public:

	/** Constructor */
	FOnlineAsyncTaskPS4Leaderboard(FOnlineSubsystemPS4* InSubsystem, int InNpTitleCtxId)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, NpTitleCtxId(InNpTitleCtxId)
		, bInit(false)
		, RequestId(0)
	{
	}

	virtual void Tick() override
	{
		if (!bInit)
		{
			RequestId = sceNpScoreCreateRequest(NpTitleCtxId);

			if (RequestId == SCE_NP_COMMUNITY_ERROR_TOO_MANY_OBJECTS)
			{
				// Too many requests outstanding. Try again next tick.
				return;
			}
			else if (RequestId == SCE_NP_COMMUNITY_ERROR_INVALID_ONLINE_ID)
			{
				UE_LOG_ONLINE(Error, TEXT("sceNpScoreCreateRequest error = 0x%08x, user is not signed in to PSN"), RequestId);
				bInit = true;
				bWasSuccessful = false;
				bIsComplete = true;
			}
			else if (RequestId < 0)
			{
				UE_LOG_ONLINE(Error, TEXT("sceNpScoreCreateRequest error = 0x%08x"), RequestId);
				bInit = true;
				bWasSuccessful = false;
				bIsComplete = true;
			}
			else
			{
				if (AsyncBegin(RequestId) == false)
				{
					bWasSuccessful = false;
					bIsComplete = true;
				}
				bInit = true;
			}
		}
		else
		{
			int32 Result = 0;
			int32 Waiting = sceNpScorePollAsync(RequestId, &Result);

			if (Waiting == 0)
			{
				// The async operation has completed.
				bIsComplete = true;
				AsyncCheckResult(Result);
			}
			else if (Waiting < 0)
			{
				// We've hit an error
				UE_LOG_ONLINE(Error, TEXT("sceNpScorePollAsync error = 0x%08x"), Waiting);
				bIsComplete = true;
				bWasSuccessful = false;

			}
		}
	}

	virtual void Finalize() override
	{
		if (RequestId > 0)
		{
			int Ret = sceNpScoreDeleteRequest(RequestId);
			check(Ret == 0);
		}
		else
		{
			UE_LOG_ONLINE(Error, TEXT("FOnlineAsyncTaskPS4Leaderboard RequestId error = 0x%08x"), RequestId);
		}
		AsyncFinalize();
	}

	// Override in child classes for wrapping different async Sony APIs
	virtual bool AsyncBegin(int32 InRequestId) = 0;
	virtual void AsyncCheckResult(int32 Result) = 0;
	virtual void AsyncFinalize() = 0;

private:

	/** Context needed for Sony API calls */
	const int NpTitleCtxId;

	/** Has this task been initialized yet */
	bool bInit;

	/** Request handle for polling async task state */
	int32 RequestId;
};


/**
 *	Wraps sceNpScoreRecordScoreAsync in a task object
 */
class FOnlineAsyncTaskPS4WriteLeaderboards : public FOnlineAsyncTaskPS4Leaderboard
{
public:

	/** Constructor */
	FOnlineAsyncTaskPS4WriteLeaderboards(FOnlineSubsystemPS4* InSubsystem, int InNpTitleCtxId, SceNpScoreBoardId InBoardId, SceNpScoreValue InScoreValue)
		: FOnlineAsyncTaskPS4Leaderboard(InSubsystem, InNpTitleCtxId)
		, BoardId(InBoardId)
		, ScoreValue(InScoreValue)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4WriteLeaderboards bWasSuccessful: %d"), bWasSuccessful);
	}

	/**
	 *	Start recording a score value for the current user
	 */
	virtual bool AsyncBegin(int32 InRequestId) override
	{
		int32 Result = sceNpScoreRecordScoreAsync(InRequestId, BoardId, ScoreValue, NULL, NULL, NULL, NULL, NULL);
		if (Result < 0)
		{
			UE_LOG_ONLINE(Error, TEXT("sceNpScoreRecordScoreAsync error = 0x%08x"), Result);
			return false;
		}
		return true;
	}

	/**
	 *	The async method to record a score has completed. Check the result.
	 */
	virtual void AsyncCheckResult(int32 Result) override
	{
		bWasSuccessful = true;
		if (Result == SCE_NP_COMMUNITY_SERVER_ERROR_NOT_BEST_SCORE)
		{
			// Not recording the best score is okay even thought Sony returns an error code for it.
			UE_LOG_ONLINE(Display, TEXT("sceNpScoreRecordScore -- not best score"));
		}
		else if (Result == SCE_NP_COMMUNITY_SERVER_ERROR_INVALID_ANTICHEAT_DATA)
		{
			UE_LOG_ONLINE(Error, TEXT("sceNpScoreRecordScore error = 0x%08x, Invalid passphrase (check that proper Title Id and Secret is used in sceNpSetNpTitleId)"), Result);
			bWasSuccessful = false;
		}
		else if (Result < 0)
		{
			// Recording the score failed for some reason.
			UE_LOG_ONLINE(Error, TEXT("sceNpScoreRecordScore error = 0x%08x"), Result);
			bWasSuccessful = false;
		}
	}

	/**
	 *	Cleanup or marshalling of data after async API finished
	 */
	virtual void AsyncFinalize() override
	{
		// Do nothing
	}

private:

	/** The scoreboard we are writing a score to */
	SceNpScoreBoardId BoardId;

	/** The score value to record into the scoreboard */
	SceNpScoreValue ScoreValue;
};


/**
 *	Wraps sceNpScoreGetRankingByAccountIdAsync in a task object
 */
class FOnlineAsyncTaskPS4ReadLeaderboards : public FOnlineAsyncTaskPS4Leaderboard
{
public:

	FOnlineAsyncTaskPS4ReadLeaderboards(FOnlineSubsystemPS4* InSubsystem, FOnlineIdMap const& InResolvedIdsMap, int InNpTitleCtxId, FOnlineLeaderboardReadRef const& InReadObject, int32 InColumnIdx, int32 InMaxNumberOfRows)
		: FOnlineAsyncTaskPS4Leaderboard(InSubsystem, InNpTitleCtxId)
		, UserIdCount(0)
		, ReadObject(InReadObject)
		, ColumnIdx(InColumnIdx)
		, MaxNumberOfRows(InMaxNumberOfRows)
		, bShouldTriggerDelegates(false)
		, ResolvedIdsMap(InResolvedIdsMap)
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4ReadLeaderboards bWasSuccessful: %d"), bWasSuccessful);
	}

	/** Can only handle so many requests */
	bool IsFull()
	{
		return UserIdCount == MAX_SCORES_PER_REQUEST;
	}

	void AddUserId(TSharedRef<FUniqueNetIdPS4 const> const& UserId)
	{
		check(UserIdCount < MAX_SCORES_PER_REQUEST);
		uint32 UserIndex = UserIdCount++;
		NpAccountIdArray[UserIndex] = UserId->GetAccountId();
		UserIdArray[UserIndex] = UserId;
	}

	virtual bool AsyncBegin(int32 InRequestId) override
	{
		check(UserIdCount > 0 && UserIdCount <= MAX_SCORES_PER_REQUEST);

		const SceNpScoreBoardId BoardId = FindScoreBoardId(ReadObject->LeaderboardName, ReadObject->ColumnMetadata[ColumnIdx].ColumnName);
		if (BoardId == -1)
		{
			// Leaderboard Id not found
			return false;
		}

		// Get the score and ranking information
		int32 Ret = sceNpScoreGetRankingByAccountIdAsync(InRequestId,
			BoardId,
			NpAccountIdArray,
			UserIdCount * sizeof(SceNpAccountId),
			RankArray,
			UserIdCount * sizeof(SceNpScorePlayerRankDataA),
			NULL,			// commments (ignored)
			0,
			NULL,			// game info (ignored)
			0,
			UserIdCount,	// Number of ranking infos requested
			NULL,			// last sort date (ignored)
			NULL,			// total record (ignored)
			NULL);			// leave NULL

		return Ret == 0;
	}

	/**
	 *	The async method to request ranking data has completed. Check the result.
	 */
	virtual void AsyncCheckResult(int32 Result) override
	{
		if (Result == SCE_NP_COMMUNITY_ERROR_INVALID_SIGNATURE)
		{
			UE_LOG_ONLINE(Error,
				TEXT("sceNpScoreGetRankingByAccountIdAsync error = 0x%08x, Server response signature is invalid, or SceNpCommunicationPassphrase is incorrect. (Check TitleId and Secret in sceNpSetNpTitleId)"), Result);
			bWasSuccessful = false;
		}
		else if (Result < 0)
		{
			// Recording the score failed for some reason.
			UE_LOG_ONLINE(Error, TEXT("sceNpScoreGetRankingByAccountIdAsync error = 0x%08x"), Result);
			bWasSuccessful = false;
		}
		else
		{
			bWasSuccessful = true;
		}
	}

	/**
	 *	Copy results into the leaderboard object reader
	 */
	virtual void AsyncFinalize() override
	{
		// We add score data whether the task was successful or not.
		// When all ranking data is received we need all ReadObject row and column data to be filled out.

		const FColumnMetaData& ColumnMetaData = ReadObject->ColumnMetadata[ColumnIdx];

		for (int32 Idx = 0; Idx < UserIdCount; ++Idx)
		{
			TSharedPtr<FUniqueNetIdPS4 const> const& CurrentUserId = UserIdArray[Idx];

			FOnlineStatsRow* UserRow = ReadObject->FindPlayerRecord(*CurrentUserId);
			if (UserRow == nullptr)
			{
				// Find the nickname from the resolved names map
				FString NickName;
				SceNpOnlineId OnlineId;
				if (ResolvedIdsMap.GetOnlineId(CurrentUserId->GetAccountId(), OnlineId))
				{
					NickName = PS4OnlineIdToString(OnlineId);
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("Leaderboards: Cannot determine nickname for account '%s'."), *PS4AccountIdToString(CurrentUserId->GetAccountId()));
				}

				// The OnlineId is the user's nickname
				UserRow = new (ReadObject->Rows) FOnlineStatsRow(NickName, CurrentUserId.ToSharedRef());
				UserRow->Rank = -1;
			}

			SceNpScorePlayerRankDataA const& RankData = RankArray[Idx];
			if (bWasSuccessful && RankData.hasData)
			{
				// Check we're accessing the correct user's data
				check(RankData.rankData.accountId == CurrentUserId->GetAccountId());

				// Rank is tied to the sorted column. If there is no SortedColumn than just use the last column for rank.
				if (ReadObject->SortedColumn.GetComparisonIndex() == 0 || ReadObject->SortedColumn == ColumnMetaData.ColumnName)
				{
					UserRow->Rank = RankData.rankData.rank;
				}

				UserRow->Columns.Add(ColumnMetaData.ColumnName, GetVarientFromNpScore(ColumnMetaData.DataType, RankData.rankData.scoreValue));
			}
			else
			{
				// Add empty data so there is something in every row and column
				UserRow->Columns.Add(ColumnMetaData.ColumnName, FVariantData());
			}
		}

		// Have we filled in every row?
		if (ReadObject->Rows.Num() == MaxNumberOfRows)
		{
			// Have we filled in every column?
			if (ReadObject->Rows[0].Columns.Num() == ReadObject->ColumnMetadata.Num())
			{
				// All the cells have been filled. Reading is done and we should trigger delegates.
				// Note: If we're requesting leaderboard rankings for a lot of players then this task will be the last
				// in a number of tasks that are requesting such data.
				// In that case, only this task should be firing delegates.
				ReadObject->ReadState = EOnlineAsyncTaskState::Done;
				bShouldTriggerDelegates = true;
			}
		}
	}


	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override 
	{
		FOnlineAsyncTaskPS4Leaderboard::TriggerDelegates();

		if (bShouldTriggerDelegates)
		{
			IOnlineLeaderboardsPtr LeaderboardsPtr = Subsystem->GetLeaderboardsInterface();
			LeaderboardsPtr->TriggerOnLeaderboardReadCompleteDelegates(ReadObject->ReadState == EOnlineAsyncTaskState::Done ? true : false);
		}
	}

private:

	/** Can only request 101 scores at a time */
	static const int MAX_SCORES_PER_REQUEST = SCE_NP_SCORE_MAX_NPID_NUM_PER_REQUEST;

	/** Array of player Ids to request ranking/score information for */
	SceNpAccountId NpAccountIdArray[MAX_SCORES_PER_REQUEST];
	TSharedPtr<FUniqueNetIdPS4 const> UserIdArray[MAX_SCORES_PER_REQUEST];

	/** Array of ranking data that is filled from the request */
	SceNpScorePlayerRankDataA RankArray[MAX_SCORES_PER_REQUEST];

	/** How many ranking data requests are being made by this task? */
	int UserIdCount;

	/** Read object to copy ranking results to */
	FOnlineLeaderboardReadRef ReadObject;

	/** Which column from the ReadObject we are getting from Sony's scoreboards */
	const int ColumnIdx;

	/**
	 *	How many rows of data will all tasks (combined) by copied into the leaderboard read object?
	 *	When the last column of the last row is read in then we know we are done requesting rank data.
	 */
	const int MaxNumberOfRows;

	/** If we are the last async task to gather a group of ranking information then we may trigger delegates */
	bool bShouldTriggerDelegates;

	/** The map of resolved player names. Used to retrieve the nickname of each player. */
	FOnlineIdMap ResolvedIdsMap;
};


/**
 *	Task object to get friends list and from that get scoreboarding scores.
 */
class FOnlineAsyncTaskPS4ReadLeaderboardsForFriends : public FOnlineAsyncTaskPS4
{
public:

	FOnlineAsyncTaskPS4ReadLeaderboardsForFriends(FOnlineSubsystemPS4* InSubsystem, int32 InLocalUserNum, FOnlineLeaderboardReadRef& InReadObject)
		:	FOnlineAsyncTaskPS4(InSubsystem)
		,	LocalUserNum(InLocalUserNum)
		,	ReadObject(InReadObject)
	{
		IOnlineFriendsPtr OnlineFriendsPtr = Subsystem->GetFriendsInterface();
		check(OnlineFriendsPtr.IsValid());

		FOnReadFriendsListComplete OnReadFriendsCompleteDelegate = FOnReadFriendsListComplete::CreateRaw(this, &FOnlineAsyncTaskPS4ReadLeaderboardsForFriends::OnReadFriendsComplete);
		OnlineFriendsPtr->ReadFriendsList(LocalUserNum, EFriendsLists::ToString(EFriendsLists::Default), OnReadFriendsCompleteDelegate);
	}

	virtual ~FOnlineAsyncTaskPS4ReadLeaderboardsForFriends()
	{
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4ReadLeaderboardsForFriends bWasSuccessful: %d"), bWasSuccessful);
	}

private:

	void OnReadFriendsComplete(int32 LocalPlayer, bool bReadFriendsSuccessful, const FString& ListName, const FString& ErrorStr)
	{
		check(LocalPlayer == LocalUserNum);

		IOnlineFriendsPtr OnlineFriendsPtr = Subsystem->GetFriendsInterface();
		check(OnlineFriendsPtr.IsValid());

		TArray<TSharedRef<const FUniqueNetId>> FriendsList;

		// Add the current user
		TSharedPtr<const FUniqueNetId> LocalUserId = Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum);
		FriendsList.Add(LocalUserId.ToSharedRef());

		// And all friends
		if (bReadFriendsSuccessful)
		{
			TArray<TSharedRef<FOnlineFriend>> Friends;
			OnlineFriendsPtr->GetFriendsList(LocalUserNum, EFriendsLists::ToString(EFriendsLists::Default), Friends);
			for (int32 Idx=0; Idx<Friends.Num(); ++Idx)
			{
				FriendsList.Add(Friends[Idx]->GetUserId());
			}
		}

		IOnlineLeaderboardsPtr OnlineLeaderboardsPtr = Subsystem->GetLeaderboardsInterface();
		check(OnlineLeaderboardsPtr.IsValid());
		OnlineLeaderboardsPtr->ReadLeaderboards(FriendsList, ReadObject);

		// We're done with this async task
		bWasSuccessful = bReadFriendsSuccessful;
		bIsComplete = true;
	}

private:

	int32 LocalUserNum;

	FOnlineLeaderboardReadRef ReadObject;
};


// FOnlineLeaderboardsPS4

FOnlineLeaderboardsPS4::FOnlineLeaderboardsPS4(FOnlineSubsystemPS4* InSubsystem)
	: PS4Subsystem(InSubsystem)
	, TitleCtxId(0)
{
	int32 Result;
	
	if ((Result = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_SCORE_RANKING)) != SCE_OK)
	{
		UE_LOG_ONLINE(Fatal, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_NP_SCORE_RANKING) failed: 0x%x"), Result);
	}

	int32 ServiceLabel = 0;
	GConfig->GetInt(TEXT("LeaderboardsPS4"), TEXT("NpServiceLabel"), ServiceLabel, GEngineIni);

	SceUserServiceUserId InitialUserId;
	if ((Result = sceUserServiceGetInitialUser(&InitialUserId)) != SCE_OK)
	{
		UE_LOG_ONLINE(Fatal, TEXT("sceUserServiceGetInitialUser failed: 0x%x"), Result);
	}

	if ((Result = sceNpScoreCreateNpTitleCtxA(ServiceLabel, InitialUserId)) <= 0)
	{
		UE_LOG_ONLINE(Log, TEXT("sceNpScoreCreateNpTitleCtx failed: 0x%x"), Result);
	}

	TitleCtxId = Result;
}

FOnlineLeaderboardsPS4::~FOnlineLeaderboardsPS4()
{
	sceNpScoreDeleteNpTitleCtx(TitleCtxId);
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_SCORE_RANKING);
}


bool FOnlineLeaderboardsPS4::ReadLeaderboards(const TArray<TSharedRef<const FUniqueNetId>>& Players, FOnlineLeaderboardReadRef& ReadObject)
{
	int32 Result;
	SceUserServiceUserId InitialUser;
	if ((Result = sceUserServiceGetInitialUser(&InitialUser)) != SCE_OK)
	{
		UE_LOG_ONLINE(Error, TEXT("sceUserServiceGetInitialUser failed with error code 0x%08x"), Result);
		return false;
	}

	int32 LocalUserIndex = FPS4Application::GetPS4Application()->GetUserIndex(InitialUser);
	TSharedRef<FUniqueNetIdPS4 const> LocalPlayerId = FUniqueNetIdPS4::Cast(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserIndex).ToSharedRef());

	TArray<SceNpAccountId> AccountIdsToResolve;
	for (auto const& Player : Players)
	{
		AccountIdsToResolve.Add(FUniqueNetIdPS4::Cast(Player)->GetAccountId());
	}

	FOnIdResolveComplete Delegate;
	Delegate.AddLambda([=](FOnlineIdMap ResolvedIds, bool bNameResolutionSuccessful, FString ErrorString)
	{
		if (!bNameResolutionSuccessful)
		{
			UE_LOG_ONLINE(Error, TEXT("Leaderboard name resolution failed: %s"), *ErrorString);
		}

		// Clear out any existing data
		ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
		ReadObject->Rows.Empty();

		// We get a column of ranking data for a group of players at once
		for (int32 ColumnIdx = 0; ColumnIdx < ReadObject->ColumnMetadata.Num(); ++ColumnIdx)
		{
			FOnlineAsyncTaskPS4ReadLeaderboards* NewReadLeaderboardsTask = nullptr;

			for (int32 PlayerIdx = 0; PlayerIdx < Players.Num(); ++PlayerIdx)
			{
				if (NewReadLeaderboardsTask == nullptr || NewReadLeaderboardsTask->IsFull())
				{
					if (NewReadLeaderboardsTask != nullptr)
					{
						// We are done adding users so add to queue
						PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(NewReadLeaderboardsTask);
					}

					// Create a new task that groups requests for ranking data for players
					NewReadLeaderboardsTask = new FOnlineAsyncTaskPS4ReadLeaderboards(PS4Subsystem, ResolvedIds, TitleCtxId, ReadObject, ColumnIdx, Players.Num());
				}

				NewReadLeaderboardsTask->AddUserId(FUniqueNetIdPS4::Cast(Players[PlayerIdx]));
			}

			// Queue the remaining async task
			if (NewReadLeaderboardsTask != nullptr)
			{
				PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(NewReadLeaderboardsTask);
			}
		}
	});

	PS4Subsystem->ResolveOnlineIdsAsync(*LocalPlayerId, AccountIdsToResolve, Delegate);
	return true;
}


bool FOnlineLeaderboardsPS4::ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject)
{
	FOnlineAsyncTaskPS4ReadLeaderboardsForFriends* NewReadLeaderboardsForFriendsTask = new FOnlineAsyncTaskPS4ReadLeaderboardsForFriends(PS4Subsystem, LocalUserNum, ReadObject);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(NewReadLeaderboardsForFriendsTask);
	return true;
}

bool FOnlineLeaderboardsPS4::ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
	UE_LOG_ONLINE(Warning, TEXT("FOnlineLeaderboardsPS4::ReadLeaderboardsAroundRank is currently not supported."));
	return false;
}
bool FOnlineLeaderboardsPS4::ReadLeaderboardsAroundUser(TSharedRef<const FUniqueNetId> Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject)
{
	UE_LOG_ONLINE(Warning, TEXT("FOnlineLeaderboardsPS4::ReadLeaderboardsAroundUser is currently not supported."));
	return false;
}

void FOnlineLeaderboardsPS4::FreeStats(FOnlineLeaderboardRead& ReadObject)
{
	UE_LOG_ONLINE(Display, TEXT("FOnlineLeaderboardsPS4::FreeStats()"));
}


bool FOnlineLeaderboardsPS4::WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject)
{
	bool bWasSuccessful = true;

	int32 NumLeaderboards = WriteObject.LeaderboardNames.Num();
	for (int32 LeaderboardIdx = 0; LeaderboardIdx < NumLeaderboards; ++LeaderboardIdx)
	{
		for (FStatPropertyArray::TConstIterator It(WriteObject.Properties); It; ++It)
		{
			const FName& StatName = It.Key();
			const FVariantData& Stat = It.Value();

			const SceNpScoreBoardId BoardId = FindScoreBoardId(WriteObject.LeaderboardNames[LeaderboardIdx], StatName);
			if (BoardId != -1)
			{
				const SceNpScoreValue ScoreValue = GetNpScoreFromVariant(Stat);

				FOnlineAsyncTaskPS4WriteLeaderboards* NewWriteLeaderboards = new FOnlineAsyncTaskPS4WriteLeaderboards(PS4Subsystem, TitleCtxId, BoardId, ScoreValue);
				PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(NewWriteLeaderboards);
			}
		}
	}

	return bWasSuccessful;
}

bool FOnlineLeaderboardsPS4::FlushLeaderboards(const FName& SessionName)
{
	UE_LOG_ONLINE(Display, TEXT("FOnlineLeaderboardsPS4::FlushLeaderboards() not supported"));
	TriggerOnLeaderboardFlushCompleteDelegates(SessionName, true);
	return true;
}

bool FOnlineLeaderboardsPS4::WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores)
{
	UE_LOG_ONLINE(Display, TEXT("FOnlineLeaderboardsPS4::WriteOnlinePlayerRatings() not supported"));
	return false;
}
