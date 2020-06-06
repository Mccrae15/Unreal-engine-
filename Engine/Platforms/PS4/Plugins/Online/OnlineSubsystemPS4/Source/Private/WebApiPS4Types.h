// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "Async/AsyncWork.h"

enum class ENpApiGroup : uint8
{
	Invalid,

	ActivityFeed,
	Commerce,
	Entitlements,
	Events,
	GameCustomData,
	IdentityMapper,
	ScoreRanking,
	SessionAndInvitation,
	SharedMedia,
	TitlePlayerInfo,
	TitleSmallStrage,
	TitleUserStorage,
	Tournament,
	Trophy,
	UserProfile,
	WordFilter
};

inline
const TCHAR* LexToString(const ENpApiGroup ApiGroup)
{
	switch (ApiGroup)
	{
	case ENpApiGroup::ActivityFeed:
		return TEXT("sdk:activityFeed");
	case ENpApiGroup::Commerce:
		return TEXT("sdk:commerce");
	case ENpApiGroup::Entitlements:
		return TEXT("sdk:entitlement");
	case ENpApiGroup::Events:
		return TEXT("sdk:eventsClient");
	case ENpApiGroup::GameCustomData:
		return TEXT("sdk:gameCustomData");
	case ENpApiGroup::IdentityMapper:
		return TEXT("sdk:identityMapper");
	case ENpApiGroup::ScoreRanking:
		return TEXT("sdk:scoreRanking");
	case ENpApiGroup::SessionAndInvitation:
		return TEXT("sdk:sessionInvitation");
	case ENpApiGroup::SharedMedia:
		return TEXT("sdk:sharedMedia");
	case ENpApiGroup::TitlePlayerInfo:
		return TEXT("sdk:titlePlayerInfo");
	case ENpApiGroup::TitleSmallStrage:
		return TEXT("sdk:tss");
	case ENpApiGroup::TitleUserStorage:
		return TEXT("sdk:tus");
	case ENpApiGroup::Tournament:
		return TEXT("sdk:tournaments");
	case ENpApiGroup::Trophy:
		return TEXT("sdk:trophy");
	case ENpApiGroup::UserProfile:
		return TEXT("sdk:userProfile");
	case ENpApiGroup::WordFilter:
		return TEXT("sdk:wordFilter");
	}

	checkNoEntry();
	return TEXT("");
}

