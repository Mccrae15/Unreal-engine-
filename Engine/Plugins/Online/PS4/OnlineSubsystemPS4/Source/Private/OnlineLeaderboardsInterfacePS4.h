// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineLeaderboardInterface.h"

/**
 *	FOnlineLeaderboardsPS4 - Interface class for PS4 leaderboards (scoreboards)
 */
class FOnlineLeaderboardsPS4 : public IOnlineLeaderboards
{
public:

	//~ Begin IOnlineLeaderboards Interface
	virtual bool ReadLeaderboards(const TArray< TSharedRef<const FUniqueNetId> >& Players, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundRank(int32 Rank, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual bool ReadLeaderboardsAroundUser(TSharedRef<const FUniqueNetId> Player, uint32 Range, FOnlineLeaderboardReadRef& ReadObject) override;
	virtual void FreeStats(FOnlineLeaderboardRead& ReadObject) override;
	virtual bool WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject) override;
	virtual bool FlushLeaderboards(const FName& SessionName) override;
	virtual bool WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores) override;
	//~ End IOnlineLeaderboards Interface

	/**
	 * Constructor
	 */
	explicit FOnlineLeaderboardsPS4(class FOnlineSubsystemPS4* InSubsystem);

	/**
	 * Default destructor
	 */
	virtual ~FOnlineLeaderboardsPS4();

private:

	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

	int TitleCtxId;
};

typedef TSharedPtr<FOnlineLeaderboardsPS4, ESPMode::ThreadSafe> FOnlineLeaderboardsPS4Ptr;
