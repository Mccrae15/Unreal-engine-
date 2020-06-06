// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../OnlineTournamentPS4InterfaceTypes.h"
#include "../OnlineTournamentPS4NpTypes.h"
#include "Serialization/JsonSerializerMacros.h"

struct FPS4NpQueryTournamentDetailsTournamentDetail
	: public FJsonSerializable
{
	FPS4NpQueryTournamentDetailsTournamentDetail()
		: CheckedInCount(0)
		, bClosed(false)
		, LastUpdate(FDateTime::MinValue())
		, bPSPlusFlag(0)
		, RegisteredTeamCount(0)
		, RegisteredUserCount(0)
		, bSponsoredFlag(0)
		, TileOpacity(0)
	{
	}

	virtual ~FPS4NpQueryTournamentDetailsTournamentDetail() = default;

	EOnlineTournamentState GetState() const;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("backgroundImageUrl", BackgroundImageUrl);
		JSON_SERIALIZE("backgroundMusicUrl", BackgroundMusicUrl);
		JSON_SERIALIZE("bannerImageUrl", BannerImageUrl);
		JSON_SERIALIZE("bulletinBoardUrl", BulletinBoardUrl);
		JSON_SERIALIZE("checkedInCount", CheckedInCount);
		JSON_SERIALIZE("closed", bClosed);
		JSON_SERIALIZE("description", Description);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("duration", Duration);
		JSON_SERIALIZE("eventName", EventName);
		JSON_SERIALIZE("externalLinkUrl", ExternalLinkUrl);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("focusColor", FocusColor);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("fontColor", FontColor);
		JSON_SERIALIZE("format", Format);
		JSON_SERIALIZE("lastUpdate", LastUpdate);
		JSON_SERIALIZE_ARRAY("npTitleIds", NPTitleIds);
		JSON_SERIALIZE("poweredBy", PoweredBy);
		JSON_SERIALIZE("promoterName", PromoterName);
		JSON_SERIALIZE("pSPlusFlag", bPSPlusFlag);
		JSON_SERIALIZE("registeredTeamCount", RegisteredTeamCount);
		JSON_SERIALIZE("registeredUserCount", RegisteredUserCount);
		JSON_SERIALIZE_ARRAY("relatedEvents", RelatedEvents);
		JSON_SERIALIZE("requirements", Requirements);
		JSON_SERIALIZE("rewards", Rewards);
		JSON_SERIALIZE("rewardsImageUrl", RewardsImageUrl);
		JSON_SERIALIZE("sponsoredBy", SponsoredBy);
		JSON_SERIALIZE("sponsoredFlag", bSponsoredFlag);
		JSON_SERIALIZE("supportDeskUrl", SupportDeskUrl);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("teamRequirements", TeamRequirements);
		JSON_SERIALIZE("termsAndConditionsUrl", TermsAndConditionsUrl);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("tileColor", TileColor);
		JSON_SERIALIZE("tileOpacity", TileOpacity);
		JSON_SERIALIZE("titleIconImageUrl", TitleIconImageUrl);
		JSON_SERIALIZE("titleName", TitleName);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("tournamentResults", TournamentResults);
		JSON_SERIALIZE("tournamentType", TournamentType);
		JSON_SERIALIZE("vendorEventId", VendorEventId);
	END_JSON_SERIALIZER

public:
	FString BackgroundImageUrl;
	FString BackgroundMusicUrl;
	FString BannerImageUrl;
	FString BulletinBoardUrl;
	int32 CheckedInCount;
	bool bClosed;
	FString Description;
	FPS4NpDuration Duration;
	FString EventName;
	FString ExternalLinkUrl;
	FPS4NpColor FocusColor;
	FPS4NpColor FontColor;
	FString Format;
	FDateTime LastUpdate;
	TArray<FString> NPTitleIds;
	FString PoweredBy;
	FString PromoterName;
	bool bPSPlusFlag;
	int32 RegisteredTeamCount;
	int32 RegisteredUserCount;
	TArray<FString> RelatedEvents;
	FString Requirements;
	FString Rewards;
	FString RewardsImageUrl;
	FString SponsoredBy;
	bool bSponsoredFlag;
	FString SupportDeskUrl;
	FPS4NpTeamRequirements TeamRequirements;
	FString TermsAndConditionsUrl;
	FPS4NpColor TileColor;
	int32 TileOpacity;
	FString TitleIconImageUrl;
	FString TitleName;
	FPS4NpTournamentResults TournamentResults;
	FString TournamentType;
	FString VendorEventId;
};

class FOnlineAsyncTaskPS4QueryTournamentDetails
	: public FOnlineAsyncWebTaskListPS4
{
public:
	FOnlineAsyncTaskPS4QueryTournamentDetails(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TArray<TSharedRef<const FOnlineTournamentId>>& InTournamentIds, const FOnlineTournamentQueryTournamentDetailsComplete& InDelegate);

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
	TArray<TSharedRef<const FOnlineTournamentId>> TournamentIds;
	FOnlineTournamentQueryTournamentDetailsComplete Delegate;

	FOnlineError Result;
	TOptional<TArray<TSharedRef<const IOnlineTournamentDetails>>> OptionalTournamentDetails;
};
