// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/JsonSerializerMacros.h"

class FUniqueNetId;

struct FPS4NpDuration
	: public FJsonSerializable
{
public:
	FPS4NpDuration()
		: CheckInTimespanMinutes(0)
		, RegisterStartDate(FDateTime::MinValue())
		, RegisterEndDate(FDateTime::MinValue())
		, EventStartDate(FDateTime::MinValue())
		, EventEndDate(FDateTime::MinValue())
	{
	}

	virtual ~FPS4NpDuration() = default;

	//~ Begin FJsonSerializable Interface
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("checkInTimeSpan", CheckInTimespanMinutes);
		JSON_SERIALIZE("registerStartDate", RegisterStartDate);
		JSON_SERIALIZE("registerEndDate", RegisterEndDate);
		JSON_SERIALIZE("eventStartDate", EventStartDate);
		JSON_SERIALIZE("eventEndDate", EventEndDate);
	END_JSON_SERIALIZER
	//~ End FJsonSerializable Interface

public:
	int32 CheckInTimespanMinutes;
	FDateTime RegisterStartDate;
	FDateTime RegisterEndDate;
	FDateTime EventStartDate;
	FDateTime EventEndDate;
};

struct FPS4NpColor
	: public FJsonSerializable
{
public:
	FPS4NpColor()
		: Red(0)
		, Green(0)
		, Blue(0)
	{
	}

	virtual ~FPS4NpColor() = default;

	//~ Begin FJsonSerializable Interface
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("r", Red);
		JSON_SERIALIZE("g", Green);
		JSON_SERIALIZE("b", Blue);
	END_JSON_SERIALIZER
	//~ End FJsonSerializable Interface

	FString ToString() const
	{
		return FString::Printf(TEXT("%d,%d,%d"), Red, Green, Blue);
	}

public:
	int32 Red;
	int32 Green;
	int32 Blue;
};

struct FPS4NpTeamRequirements
	: public FJsonSerializable
{
public:
	FPS4NpTeamRequirements()
		: MinimumRequiredMembers(0)
	{
	}

	virtual ~FPS4NpTeamRequirements() = default;

	//~ Begin FJsonSerializable Interface
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("minimumRequiredMember", MinimumRequiredMembers);
	END_JSON_SERIALIZER
	//~ End FJsonSerializable Interface

public:
	int32 MinimumRequiredMembers;
};

struct FPS4NpEntity
	: public FJsonSerializable
{
public:
	FPS4NpEntity()
		: Position(INDEX_NONE)
	{
	}

	virtual ~FPS4NpEntity() = default;

	//~ Begin FJsonSerializable Interface
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("label", Label);
		JSON_SERIALIZE("position", Position);
		JSON_SERIALIZE("score", Score);
		JSON_SERIALIZE("iconUrl", IconUrl);
		JSON_SERIALIZE("name", Name);
		JSON_SERIALIZE("platform", Platform);
		JSON_SERIALIZE("teamId", TeamId);
		JSON_SERIALIZE("accountId", AccountId);
		JSON_SERIALIZE("onlineId", OnlineId);
	END_JSON_SERIALIZER
	//~ End FJsonSerializable Interface

public:
	// Set if Entity is RankedEntity
	FString Label;
	int32 Position;

	// Set if Entity is ScoredEntity
	FString Score;

	// Set if Entity is TeamEntity
	FString IconUrl;
	FString Name;
	FString Platform;
	FString TeamId;

	// Set if Entity is UserEntity
	FString AccountId;
	FString OnlineId;
};

struct FPS4NpTournamentResults
	: public FJsonSerializable
{
public:
	FPS4NpTournamentResults()
		: Version(0)
	{
	}

	virtual ~FPS4NpTournamentResults() = default;

	//~ Begin FJsonSerializable Interface
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("version", Version);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("ranking", TopParticipants, FPS4NpEntity);
	END_JSON_SERIALIZER
	//~ End FJsonSerializable Interface

public:
	int32 Version;
	TArray<FPS4NpEntity> TopParticipants;
};

struct FPS4NpResults
	: public FJsonSerializable
{
public:
	virtual ~FPS4NpResults() = default;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("absentPlayers", AbsentPlayers, FPS4NpEntity);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("players", Players, FPS4NpEntity);
	END_JSON_SERIALIZER

public:
	TArray<FPS4NpEntity> AbsentPlayers;
	TArray<FPS4NpEntity> Players;
};

struct FPS4NpMatchResults
	: public FJsonSerializable
{
public:
	virtual ~FPS4NpMatchResults() = default;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("version", Version);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("results", Results);
	END_JSON_SERIALIZER

public:
	int32 Version;
	FPS4NpResults Results;
};

enum class EOnlineTournamentParticipantState : uint8;

struct FPS4NpRegisteredUser
	: public FJsonSerializable
{
public:
	~FPS4NpRegisteredUser() = default;

	EOnlineTournamentParticipantState GetState() const;
	TSharedPtr<const FUniqueNetId> GetPlayerId() const;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("accountId", AccountId);
		JSON_SERIALIZE("onlineId", OnlineId);
		JSON_SERIALIZE("status", Status);
	END_JSON_SERIALIZER

public:
	FString AccountId;
	FString OnlineId;
	FString Status;
};

struct FPS4NpRegisteredRoster
	: public FJsonSerializable
{
public:
	~FPS4NpRegisteredRoster() = default;

	EOnlineTournamentParticipantState GetState() const;

	// FJsonSerializable
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("status", Status);
		JSON_SERIALIZE_OBJECT_SERIALIZABLE("teamDetail", TeamDetails);
	END_JSON_SERIALIZER

public:
	FString Status;
	FPS4NpEntity TeamDetails;
};
