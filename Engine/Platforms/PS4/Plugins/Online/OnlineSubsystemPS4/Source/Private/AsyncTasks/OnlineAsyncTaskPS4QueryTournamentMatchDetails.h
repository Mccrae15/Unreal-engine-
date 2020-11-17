// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "../OnlineTournamentPS4InterfaceTypes.h"
#include "../OnlineTournamentPS4NpTypes.h"
#include "Serialization/JsonSerializerMacros.h"

struct FPS4NpQueryMatchDetailsMatchDetail
	: public FJsonSerializable
{
public:
	FPS4NpQueryMatchDetailsMatchDetail()
		: bClosed(false)
		, LastUpdate(FDateTime::MinValue())
		, PlayerCount(0)
		, Position(0)
		, RegisteredTeamCount(0)
		, RegisteredUserCount(0)
		, RoundIndex(-1)
		, TileOpacity(0)
	{
	}

	virtual ~FPS4NpQueryMatchDetailsMatchDetail() = default;

	EOnlineTournamentMatchState GetState() const;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("backgroundImageUrl", BackgroundImageUrl);
		JSON_SERIALIZE("backgroundMusicUrl", BackgroundMusicUrl);
		JSON_SERIALIZE("bannerImageUrl", BannerImageUrl);
		JSON_SERIALIZE("bootArgument", BootArgument);
		JSON_SERIALIZE("bracket", Bracket);
		JSON_SERIALIZE("closed", bClosed);
		JSON_SERIALIZE("description", Description);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("duration", Duration);
		JSON_SERIALIZE("eventName", EventName);
		JSON_SERIALIZE("externalLinkUrl", ExternalLinkUrl);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("focusColor", FocusColor);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("fontColor", FontColor);
		JSON_SERIALIZE("format", Format);
		JSON_SERIALIZE("freeTextType", FreeTextType);
		JSON_SERIALIZE("lastUpdate", LastUpdate);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("matchResults", MatchResults);
		JSON_SERIALIZE_ARRAY("npTitleIds", NPTitleIds);
		JSON_SERIALIZE("parentEventId", ParentEventId);
		JSON_SERIALIZE("parentEventName", ParentEventName);
		JSON_SERIALIZE("playerCount", PlayerCount);
		JSON_SERIALIZE("position", Position);
		JSON_SERIALIZE("registeredTeamCount", RegisteredTeamCount);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("registeredTeams", RegisteredTeams, FPS4NpEntity);
		JSON_SERIALIZE("registeredUserCount", RegisteredUserCount);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("registeredUsers", RegisteredUsers, FPS4NpEntity);
		JSON_SERIALIZE("roundIndex", RoundIndex);
		JSON_SERIALIZE("screenshotType", ScreenshotType);
		JSON_SERIALIZE("supportDeskUrl", SupportDeskUrl);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("teamRequirements", TeamRequirements);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("tileColor", TileColor);
		JSON_SERIALIZE("tileOpacity", TileOpacity);
		JSON_SERIALIZE("titleIconImageUrl", TitleIconImageUrl);
		JSON_SERIALIZE("titleName", TitleName);
		JSON_SERIALIZE("tournamentType", TournamentType);
		JSON_SERIALIZE("vendorEventId", VendorEventId);
	END_JSON_SERIALIZER

public:
	FString BackgroundImageUrl;
	FString BackgroundMusicUrl;
	FString BannerImageUrl;
	FString BootArgument;
	FString Bracket;
	bool bClosed;
	FString Description;
	FPS4NpDuration Duration;
	FString EventName;
	FString ExternalLinkUrl;
	FPS4NpColor FocusColor;
	FPS4NpColor FontColor;
	FString Format;
	FString FreeTextType;
	FDateTime LastUpdate;
	FPS4NpMatchResults MatchResults;
	TArray<FString> NPTitleIds;
	FString ParentEventId;
	FString ParentEventName;
	int64 PlayerCount;
	int32 Position;
	int32 RegisteredTeamCount;
	TArray<FPS4NpEntity> RegisteredTeams;
	int32 RegisteredUserCount;
	TArray<FPS4NpEntity> RegisteredUsers;
	int32 RoundIndex;
	FString ScreenshotType;
	FString SupportDeskUrl;
	FPS4NpTeamRequirements TeamRequirements;
	FPS4NpColor TileColor;
	int32 TileOpacity;
	FString TitleIconImageUrl;
	FString TitleName;
	FString TournamentType;
	FString VendorEventId;
};

class FOnlineAsyncTaskPS4QueryTournamentMatchDetails
	: public FOnlineAsyncWebTaskListPS4
{
public:
	FOnlineAsyncTaskPS4QueryTournamentMatchDetails(FOnlineSubsystemPS4& InSubsystem, const SceUserServiceUserId InSonyUserId, const SceNpServiceLabel InNpTournamentServiceLabel, const FNpWebApiUserContext InWebApiUserContext, const TArray<TSharedRef<const FOnlineTournamentMatchId>>& InTournamentMatchIds, const FOnlineTournamentQueryMatchDetailsComplete& InDelegate);

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
	TArray<TSharedRef<const FOnlineTournamentMatchId>> TournamentMatchIds;
	FOnlineTournamentQueryMatchDetailsComplete Delegate;

	FOnlineError Result;
	TOptional<TArray<TSharedRef<const IOnlineTournamentMatchDetails>>> OptionalMatchDetails;
};
