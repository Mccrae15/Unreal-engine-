// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineTournamentPS4InterfaceTypes.h"
#include "OnlineTournamentPS4NpTypes.h"
#include "AsyncTasks/OnlineAsyncTaskPS4QueryTournamentDetails.h"
#include "AsyncTasks/OnlineAsyncTaskPS4QueryTournamentMatchDetails.h"


FName FOnlineTournamentIdPS4::GetType() const
{
	return FOnlineTournamentIdPS4::GetType_Internal();
}

uint32 GetTypeHash(const FOnlineTournamentIdPS4& TournamentId)
{
	return ::GetTypeHash(TournamentId.UniqueNetIdStr);
}

bool operator==(const FOnlineTournamentIdPS4& A, const FOnlineTournamentIdPS4& B)
{
	return A.UniqueNetIdStr.Equals(B.UniqueNetIdStr, ESearchCase::CaseSensitive);
}

bool operator!=(const FOnlineTournamentIdPS4& A, const FOnlineTournamentIdPS4& B)
{
	return !A.UniqueNetIdStr.Equals(B.UniqueNetIdStr, ESearchCase::CaseSensitive);
}

FOnlineTournamentIdPS4::FOnlineTournamentIdPS4(FString&& InTournamentId)
	: FUniqueNetIdString(MoveTemp(InTournamentId), FOnlineTournamentIdPS4::GetType_Internal())
{
}

FOnlineTournamentIdPS4::FOnlineTournamentIdPS4(const FString& InTournamentId)
	: FUniqueNetIdString(InTournamentId, FOnlineTournamentIdPS4::GetType_Internal())
{
}

/*static*/
FName FOnlineTournamentIdPS4::GetType_Internal()
{
	static const FName MyType(TEXT("PS4TournamentId"));
	return MyType;
}

FName FOnlineTournamentMatchIdPS4::GetType() const
{
	return FOnlineTournamentMatchIdPS4::GetType_Internal();
}

uint32 GetTypeHash(const FOnlineTournamentMatchIdPS4& EventId)
{
	return ::GetTypeHash(EventId.UniqueNetIdStr);
}

bool operator==(const FOnlineTournamentMatchIdPS4& A, const FOnlineTournamentMatchIdPS4& B)
{
	return A.UniqueNetIdStr.Equals(B.UniqueNetIdStr, ESearchCase::CaseSensitive);
}

bool operator!=(const FOnlineTournamentMatchIdPS4& A, const FOnlineTournamentMatchIdPS4& B)
{
	return !A.UniqueNetIdStr.Equals(B.UniqueNetIdStr, ESearchCase::CaseSensitive);
}

FOnlineTournamentMatchIdPS4::FOnlineTournamentMatchIdPS4(FString&& InMatchId)
	: FUniqueNetIdString(MoveTemp(InMatchId), FOnlineTournamentMatchIdPS4::GetType_Internal())
{
}

FOnlineTournamentMatchIdPS4::FOnlineTournamentMatchIdPS4(const FString& InMatchId)
	: FUniqueNetIdString(InMatchId, FOnlineTournamentMatchIdPS4::GetType_Internal())
{
}

/*static*/
FName FOnlineTournamentMatchIdPS4::GetType_Internal()
{
	static const FName MyType(TEXT("PS4TournamentMatchId"));
	return MyType;
}

FName FOnlineTournamentTeamIdPS4::GetType() const
{
	static const FName MyType(TEXT("PS4TeamId"));
	return MyType;
}

const uint8* FOnlineTournamentTeamIdPS4::GetBytes() const
{
	return reinterpret_cast<const uint8*>(*FullTeamId);
}

int32 FOnlineTournamentTeamIdPS4::GetSize() const
{
	return sizeof(**FullTeamId) * (FullTeamId.Len() + 1);
}

bool FOnlineTournamentTeamIdPS4::IsValid() const
{
	return !Platform.IsEmpty() && !TeamId.IsEmpty();
}

FString FOnlineTournamentTeamIdPS4::ToString() const
{
	return FullTeamId;
}

FString FOnlineTournamentTeamIdPS4::ToDebugString() const
{
	return FString::Printf(TEXT("TournamentId=[%s] Platform=[%s] TeamId=[%s]"), *TournamentId->ToString(), *Platform, *TeamId);
}

uint32 GetTypeHash(const FOnlineTournamentTeamIdPS4& OtherTeamId)
{
	return ::GetTypeHash(OtherTeamId.FullTeamId);
}

bool operator==(const FOnlineTournamentTeamIdPS4& A, const FOnlineTournamentTeamIdPS4& B)
{
	return A.FullTeamId.Equals(B.FullTeamId, ESearchCase::CaseSensitive);
}

bool operator!=(const FOnlineTournamentTeamIdPS4& A, const FOnlineTournamentTeamIdPS4& B)
{
	return !A.FullTeamId.Equals(B.FullTeamId, ESearchCase::CaseSensitive);
}

FOnlineTournamentTeamIdPS4::FOnlineTournamentTeamIdPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, const FString& InFullTeamId)
	: FullTeamId(InFullTeamId)
	, TournamentId(InTournamentId)
{
	FullTeamId.Split(TEXT("|"), &Platform, &TeamId, ESearchCase::CaseSensitive);
}

FOnlineTournamentTeamIdPS4::FOnlineTournamentTeamIdPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, FString&& InFullTeamId)
	: FullTeamId(MoveTemp(InFullTeamId))
	, TournamentId(InTournamentId)
{
	FullTeamId.Split(TEXT("|"), &Platform, &TeamId, ESearchCase::CaseSensitive);
}

FOnlineTournamentTeamIdPS4::FOnlineTournamentTeamIdPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, const FString& InPlatform, const FString& InTeamId)
	: FullTeamId(FString::Printf(TEXT("%s|%s|%s"), *InTournamentId->ToString(), *InPlatform, *InTeamId))
	, TournamentId(InTournamentId)
	, Platform(InPlatform)
	, TeamId(InTeamId)
{
}

FOnlineTournamentTeamIdPS4::FOnlineTournamentTeamIdPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, FString&& InPlatform, FString&& InTeamId)
	: FullTeamId(FString::Printf(TEXT("%s|%s|%s"), *InTournamentId->ToString(), *InPlatform, *InTeamId))
	, TournamentId(InTournamentId)
	, Platform(MoveTemp(InPlatform))
	, TeamId(MoveTemp(InTeamId))
{
}

TSharedRef<const FOnlineTournamentId> FOnlineTournamentTeamIdPS4::GetTournamentId() const
{
	return TournamentId;
}

const FString& FOnlineTournamentTeamIdPS4::GetTeamId() const
{
	return TeamId;
}

const FString& FOnlineTournamentTeamIdPS4::GetPlatform() const
{
	return Platform;
}

namespace
{
	void LexFromNpString(TOptional<EOnlineTournamentParticipantType>& State, const TCHAR* const String)
	{
		if (FCString::Stricmp(String, TEXT("oneOnOne")) == 0)
		{
			State = EOnlineTournamentParticipantType::Individual;
		}
		else if (FCString::Stricmp(String, TEXT("teamOnTeam")) == 0)
		{
			State = EOnlineTournamentParticipantType::Team;
		}
		else
		{
			State = TOptional<EOnlineTournamentParticipantType>();
		}
	}
}

FOnlineTournamentDetailsPS4::FOnlineTournamentDetailsPS4(FString&& EventId, FPS4NpQueryTournamentDetailsTournamentDetail&& EventDetails)
	: TournamentId(MakeShared<FOnlineTournamentIdPS4>(MoveTemp(EventId)))
	, Title(MoveTemp(EventDetails.EventName))
	, Description(EventDetails.Description)
	, State(EventDetails.GetState())
	, Format(EOnlineTournamentFormat::Custom)
	, ParticipantType(EOnlineTournamentParticipantType::Individual)
	, RegistrationStartDate(EventDetails.Duration.RegisterStartDate)
	, RegistrationEndDate(EventDetails.Duration.RegisterEndDate)
	, StartDate(EventDetails.Duration.EventStartDate)
	, CheckinTimespan(FTimespan::FromMinutes(EventDetails.Duration.CheckInTimespanMinutes))
	, EndDate(EventDetails.Duration.EventEndDate)
	, LastUpdatedDate(EventDetails.LastUpdate)
	, bRequiresPSPlus(EventDetails.bPSPlusFlag)
{
	TOptional<EOnlineTournamentFormat> OptionalFormat;
	LexFromString(OptionalFormat, *EventDetails.Format);
	Format = OptionalFormat.Get(EOnlineTournamentFormat::Custom);

	TOptional<EOnlineTournamentParticipantType> OptionalParticipantType;
	LexFromNpString(OptionalParticipantType, *EventDetails.TournamentType);
	ParticipantType = OptionalParticipantType.Get(EOnlineTournamentParticipantType::Individual);

	// Sort our participants by their ranking
	Algo::SortBy(EventDetails.TournamentResults.TopParticipants, &FPS4NpEntity::Position);

	for (const FPS4NpEntity& RankedEntity : EventDetails.TournamentResults.TopParticipants)
	{
		Participants.Emplace(MakeShared<const FOnlineTournamentParticipantDetailsPS4>(TournamentId, ParticipantType, RankedEntity, EOnlineTournamentParticipantState::Present));
	}

	Attributes.Emplace(FName(TEXT("BackgroundImageUrl")), EventDetails.BackgroundImageUrl);
	Attributes.Emplace(FName(TEXT("BackgroundMusicUrl")), EventDetails.BackgroundMusicUrl);
	Attributes.Emplace(FName(TEXT("BannerImageUrl")), EventDetails.BannerImageUrl);
	Attributes.Emplace(FName(TEXT("BulletinBoardUrl")), EventDetails.BulletinBoardUrl);
	Attributes.Emplace(FName(TEXT("CheckedInCount")), EventDetails.CheckedInCount);
	Attributes.Emplace(FName(TEXT("Closed")), EventDetails.bClosed);
	Attributes.Emplace(FName(TEXT("ExternalLinkUrl")), EventDetails.ExternalLinkUrl);
	Attributes.Emplace(FName(TEXT("FocusColor")), EventDetails.FocusColor.ToString());
	Attributes.Emplace(FName(TEXT("FontColor")), EventDetails.FontColor.ToString());
	Attributes.Emplace(FName(TEXT("NPTitleIds")), FString::Join(EventDetails.NPTitleIds, TEXT(",")));
	Attributes.Emplace(FName(TEXT("PoweredBy")), EventDetails.PoweredBy);
	Attributes.Emplace(FName(TEXT("PromoterName")), EventDetails.PromoterName);
	Attributes.Emplace(FName(TEXT("RegisteredTeamCount")), EventDetails.RegisteredTeamCount);
	Attributes.Emplace(FName(TEXT("RegisteredUserCount")), EventDetails.RegisteredUserCount);
	Attributes.Emplace(FName(TEXT("RelatedEvents")), FString::Join(EventDetails.RelatedEvents, TEXT(",")));
	Attributes.Emplace(FName(TEXT("Requirements")), EventDetails.Requirements);
	Attributes.Emplace(FName(TEXT("Rewards")), EventDetails.Rewards);
	Attributes.Emplace(FName(TEXT("RewardsImageUrl")), EventDetails.RewardsImageUrl);
	Attributes.Emplace(FName(TEXT("SponsoredBy")), EventDetails.SponsoredBy);
	Attributes.Emplace(FName(TEXT("bSponsoredFlag")), EventDetails.bSponsoredFlag);
	Attributes.Emplace(FName(TEXT("SupportDeskUrl")), EventDetails.SupportDeskUrl);
	Attributes.Emplace(FName(TEXT("TeamMinimumRequiredMembers")), EventDetails.TeamRequirements.MinimumRequiredMembers);
	Attributes.Emplace(FName(TEXT("TermsAndConditionsUrl")), EventDetails.TermsAndConditionsUrl);
	Attributes.Emplace(FName(TEXT("TileColor")), EventDetails.TileColor.ToString());
	Attributes.Emplace(FName(TEXT("TileOpacity")), EventDetails.TileOpacity);
	Attributes.Emplace(FName(TEXT("TitleIconImageUrl")), EventDetails.TitleIconImageUrl);
	Attributes.Emplace(FName(TEXT("TitleName")), EventDetails.TitleName);
	Attributes.Emplace(FName(TEXT("VendorEventId")), EventDetails.VendorEventId);
}

TSharedRef<const FOnlineTournamentId> FOnlineTournamentDetailsPS4::GetTournamentId() const
{
	return TournamentId;
}

const FString& FOnlineTournamentDetailsPS4::GetTitle() const
{
	return Title;
}

const FString& FOnlineTournamentDetailsPS4::GetDescription() const
{
	return Description;
}

EOnlineTournamentState FOnlineTournamentDetailsPS4::GetState() const
{
	return State;
}

EOnlineTournamentFormat FOnlineTournamentDetailsPS4::GetFormat() const
{
	return Format;
}

EOnlineTournamentParticipantType FOnlineTournamentDetailsPS4::GetParticipantType() const
{
	return ParticipantType;
}

const TArray<TSharedRef<const IOnlineTournamentParticipantDetails>>& FOnlineTournamentDetailsPS4::GetParticipants() const
{
	return Participants;
}

TOptional<FDateTime> FOnlineTournamentDetailsPS4::GetRegistrationStartDateUTC() const
{
	return RegistrationStartDate;
}

TOptional<FDateTime> FOnlineTournamentDetailsPS4::GetRegistrationEndDateUTC() const
{
	return RegistrationEndDate;
}

TOptional<FDateTime> FOnlineTournamentDetailsPS4::GetStartDateUTC() const
{
	return StartDate;
}

TOptional<FTimespan> FOnlineTournamentDetailsPS4::GetCheckInTimespan() const
{
	return CheckinTimespan;
}

TOptional<FDateTime> FOnlineTournamentDetailsPS4::GetEndDateUTC() const
{
	return EndDate;
}

TOptional<FDateTime> FOnlineTournamentDetailsPS4::GetLastUpdatedDateUTC() const
{
	return LastUpdatedDate;
}

TOptional<bool> FOnlineTournamentDetailsPS4::RequiresPremiumSubscription() const
{
	return bRequiresPSPlus;
}

TOptional<FVariantData> FOnlineTournamentDetailsPS4::GetAttribute(const FName AttributeName) const
{
	const FVariantData* const FoundAttribute = Attributes.Find(AttributeName);
	if (FoundAttribute)
	{
		return *FoundAttribute;
	}
	else
	{
		return TOptional<FVariantData>();
	}
}

TSharedRef<const FOnlineTournamentMatchId> FOnlineTournamentMatchDetailsPS4::GetMatchId() const
{
	return MatchId;
}

EOnlineTournamentParticipantType FOnlineTournamentMatchDetailsPS4::GetParticipantType() const
{
	return ParticipantType;
}

EOnlineTournamentMatchState FOnlineTournamentMatchDetailsPS4::GetMatchState() const
{
	return MatchState;
}

TOptional<FString> FOnlineTournamentMatchDetailsPS4::GetBracket() const
{
	return Bracket;
}

TOptional<int32> FOnlineTournamentMatchDetailsPS4::GetRound() const
{
	return Round;
}

TOptional<FDateTime> FOnlineTournamentMatchDetailsPS4::GetStartDateUTC() const
{
	return StartDateUTC;
}

TArray<TSharedRef<const IOnlineTournamentParticipantDetails>> FOnlineTournamentMatchDetailsPS4::GetParticipants() const
{
	TArray<TSharedRef<const IOnlineTournamentParticipantDetails>> ReturnValues;
	Participants.GenerateValueArray(ReturnValues);
	return ReturnValues;
}

TOptional<FVariantData> FOnlineTournamentMatchDetailsPS4::GetAttribute(const FName AttributeName) const
{
	if (const FVariantData* const Attribute = Attributes.Find(AttributeName))
	{
		return *Attribute;
	}

	return TOptional<FVariantData>();
}

FOnlineTournamentMatchDetailsPS4::FOnlineTournamentMatchDetailsPS4(FString&& MatchId, FPS4NpQueryMatchDetailsMatchDetail&& MatchDetails)
	: MatchId(MakeShared<const FOnlineTournamentMatchIdPS4>(MoveTemp(MatchId)))
	, Bracket(MatchDetails.Bracket)
	, Round(MatchDetails.RoundIndex)
	, StartDateUTC(MatchDetails.Duration.EventStartDate)
{
	TOptional<EOnlineTournamentParticipantType> OptionalParticipantType;
	LexFromNpString(OptionalParticipantType, *MatchDetails.TournamentType);
	ParticipantType = OptionalParticipantType.Get(EOnlineTournamentParticipantType::Individual);

	TSharedRef<const FOnlineTournamentId> TournamentId = MakeShared<const FOnlineTournamentIdPS4>(MatchDetails.ParentEventId);

	// Only populated if event already took place
	for (const FPS4NpEntity& AbsentEntity : MatchDetails.MatchResults.Results.AbsentPlayers)
	{
		TSharedRef<const FOnlineTournamentParticipantDetailsPS4> NewPlayer = MakeShared<const FOnlineTournamentParticipantDetailsPS4>(TournamentId, ParticipantType, AbsentEntity, EOnlineTournamentParticipantState::Absent);
		if (NewPlayer->PlayerId.IsValid() && ParticipantType == EOnlineTournamentParticipantType::Individual)
		{
			Participants.Emplace(NewPlayer->PlayerId.ToSharedRef(), NewPlayer);
		}
		else if (NewPlayer->TeamId.IsValid() && ParticipantType == EOnlineTournamentParticipantType::Team)
		{
			Participants.Emplace(NewPlayer->TeamId.ToSharedRef(), NewPlayer);
		}
	}
	// Only populated if event already took place
	for (const FPS4NpEntity& RankedEntity : MatchDetails.MatchResults.Results.Players)
	{
		TSharedRef<const FOnlineTournamentParticipantDetailsPS4> NewPlayer = MakeShared<const FOnlineTournamentParticipantDetailsPS4>(TournamentId, ParticipantType, RankedEntity, EOnlineTournamentParticipantState::Present);
		if (NewPlayer->PlayerId.IsValid() && ParticipantType == EOnlineTournamentParticipantType::Individual)
		{
			Participants.Emplace(NewPlayer->PlayerId.ToSharedRef(), NewPlayer);
		}
		else if (NewPlayer->TeamId.IsValid() && ParticipantType == EOnlineTournamentParticipantType::Team)
		{
			Participants.Emplace(NewPlayer->TeamId.ToSharedRef(), NewPlayer);
		}
	}

	if (ParticipantType == EOnlineTournamentParticipantType::Individual)
	{
		for (const FPS4NpEntity& UserEntity : MatchDetails.RegisteredUsers)
		{
			// Populated always if individual tournament
			TSharedRef<const FOnlineTournamentParticipantDetailsPS4> NewPlayer = MakeShared<const FOnlineTournamentParticipantDetailsPS4>(TournamentId, ParticipantType, UserEntity, EOnlineTournamentParticipantState::Registered);
			if (NewPlayer->PlayerId.IsValid())
			{
				// Only add player if they weren't already added
				TSharedRef<const FUniqueNetId> NewPlayerIdRef = NewPlayer->PlayerId.ToSharedRef();
				if (Participants.Find(NewPlayerIdRef) == nullptr)
				{
					Participants.Emplace(NewPlayerIdRef, NewPlayer);
				}
			}
		}

	}
	else if (ParticipantType == EOnlineTournamentParticipantType::Team)
	{
		for (const FPS4NpEntity& TeamEntity : MatchDetails.RegisteredTeams)
		{
			// Populated always if team tournament
			TSharedRef<const FOnlineTournamentParticipantDetailsPS4> NewTeam = MakeShared<const FOnlineTournamentParticipantDetailsPS4>(TournamentId, ParticipantType, TeamEntity, EOnlineTournamentParticipantState::Registered);
			if (NewTeam->TeamId.IsValid())
			{
				// Only add team if they weren't already added
				TSharedRef<const FUniqueNetId> NewTeamIdRef = NewTeam->TeamId.ToSharedRef();
				if (Participants.Find(NewTeamIdRef) == nullptr)
				{
					Participants.Emplace(NewTeamIdRef, NewTeam);
				}
			}
		}
	}

	Attributes.Emplace(FName(TEXT("BackgroundImageUrl")), MatchDetails.BackgroundImageUrl);
	Attributes.Emplace(FName(TEXT("BackgroundMusicUrl")), MatchDetails.BackgroundMusicUrl);
	Attributes.Emplace(FName(TEXT("BannerImageUrl")), MatchDetails.BannerImageUrl);
	Attributes.Emplace(FName(TEXT("BootArgument")), MatchDetails.BootArgument);
	Attributes.Emplace(FName(TEXT("Bracket")), MatchDetails.Bracket);
	Attributes.Emplace(FName(TEXT("Closed")), MatchDetails.bClosed);
	Attributes.Emplace(FName(TEXT("Description")), MatchDetails.Description);
	Attributes.Emplace(FName(TEXT("EventName")), MatchDetails.EventName);
	Attributes.Emplace(FName(TEXT("ExternalLinkUrl")), MatchDetails.ExternalLinkUrl);
	Attributes.Emplace(FName(TEXT("FocusColor")), MatchDetails.FocusColor.ToString());
	Attributes.Emplace(FName(TEXT("FontColor")), MatchDetails.FontColor.ToString());
	Attributes.Emplace(FName(TEXT("Format")), MatchDetails.Format);
	Attributes.Emplace(FName(TEXT("FreeTextType")), MatchDetails.FreeTextType);
	Attributes.Emplace(FName(TEXT("LastUpdate")), MatchDetails.LastUpdate.ToString());
	Attributes.Emplace(FName(TEXT("NPTitleIds")), FString::Join(MatchDetails.NPTitleIds, TEXT(",")));
	Attributes.Emplace(FName(TEXT("ParentEventId")), MatchDetails.ParentEventId);
	Attributes.Emplace(FName(TEXT("ParentEventName")), MatchDetails.ParentEventName);
	Attributes.Emplace(FName(TEXT("PlayerCount")), MatchDetails.PlayerCount);
	Attributes.Emplace(FName(TEXT("Position")), MatchDetails.Position);
	Attributes.Emplace(FName(TEXT("RegisteredTeamCount")), MatchDetails.RegisteredTeamCount);
	Attributes.Emplace(FName(TEXT("RegisteredUserCount")), MatchDetails.RegisteredUserCount);
	Attributes.Emplace(FName(TEXT("ScreenshotType")), MatchDetails.ScreenshotType);
	Attributes.Emplace(FName(TEXT("SupportDeskUrl")), MatchDetails.SupportDeskUrl);
	Attributes.Emplace(FName(TEXT("TeamMinimumRequiredMembers")), MatchDetails.TeamRequirements.MinimumRequiredMembers);
	Attributes.Emplace(FName(TEXT("TileColor")), MatchDetails.TileColor.ToString());
	Attributes.Emplace(FName(TEXT("TileOpacity")), MatchDetails.TileOpacity);
	Attributes.Emplace(FName(TEXT("TitleIconImageUrl")), MatchDetails.TitleIconImageUrl);
	Attributes.Emplace(FName(TEXT("TitleName")), MatchDetails.TitleName);
	Attributes.Emplace(FName(TEXT("TournamentType")), MatchDetails.TournamentType);
	Attributes.Emplace(FName(TEXT("VendorEventId")), MatchDetails.VendorEventId);
}

TSharedRef<const FOnlineTournamentId> FOnlineTournamentParticipantDetailsPS4::GetTournamentId() const
{
	return TournamentId;
}

TSharedPtr<const FUniqueNetId> FOnlineTournamentParticipantDetailsPS4::GetPlayerId() const
{
	return PlayerId;
}

TSharedPtr<const FOnlineTournamentTeamId> FOnlineTournamentParticipantDetailsPS4::GetTeamId() const
{
	return TeamId;
}

const FString& FOnlineTournamentParticipantDetailsPS4::GetDisplayName() const
{
	return DisplayName;
}

EOnlineTournamentParticipantState FOnlineTournamentParticipantDetailsPS4::GetState() const
{
	return State;
}

TOptional<int32> FOnlineTournamentParticipantDetailsPS4::GetPosition() const
{
	return Position;
}

TOptional<FVariantData> FOnlineTournamentParticipantDetailsPS4::GetScore() const
{
	return Score;
}

TOptional<FVariantData> FOnlineTournamentParticipantDetailsPS4::GetAttribute(const FName AttributeName) const
{
	if (const FVariantData* const FoundAttribute = Attributes.Find(AttributeName))
	{
		return *FoundAttribute;
	}

	return TOptional<FVariantData>();
}

FOnlineTournamentParticipantDetailsPS4::FOnlineTournamentParticipantDetailsPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, const EOnlineTournamentParticipantType InParticipantType, const FPS4NpEntity& InRankedEntity, const EOnlineTournamentParticipantState InState)
	: TournamentId(InTournamentId)
	, State(InState)
{
	if (InParticipantType == EOnlineTournamentParticipantType::Individual)
	{
		PlayerId = FUniqueNetIdPS4::FindOrCreate(PS4StringToAccountId(InRankedEntity.AccountId), PS4StringToOnlineId(InRankedEntity.OnlineId));
		DisplayName = InRankedEntity.OnlineId;
	}
	else
	{
		TeamId = MakeShared<const FOnlineTournamentTeamIdPS4>(InTournamentId, InRankedEntity.Platform, InRankedEntity.TeamId);
		DisplayName = InRankedEntity.Name;
	}

	if (!InRankedEntity.Label.IsEmpty())
	{
		Attributes.Emplace(TEXT("Label"), InRankedEntity.Label);
	}
	if (InRankedEntity.Position != INDEX_NONE)
	{
		Position = InRankedEntity.Position;
	}
	if (!InRankedEntity.Score.IsEmpty())
	{
		Score = FCString::Atoi(*InRankedEntity.Score);
	}
}

FOnlineTournamentParticipantDetailsPS4::FOnlineTournamentParticipantDetailsPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, FPS4NpRegisteredUser&& InUser)
	: TournamentId(InTournamentId)
	, PlayerId(InUser.GetPlayerId())
	, State(InUser.GetState())
{
}

FOnlineTournamentParticipantDetailsPS4::FOnlineTournamentParticipantDetailsPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, FPS4NpRegisteredRoster&& InRoster)
	: TournamentId(InTournamentId)
	, TeamId(MakeShared<const FOnlineTournamentTeamIdPS4>(InTournamentId, InRoster.TeamDetails.Platform, InRoster.TeamDetails.TeamId))
	, State(InRoster.GetState())
{
}

TSharedRef<const FOnlineTournamentTeamId> FOnlineTournamentTeamDetailsPS4:: GetTeamId() const
{
	return TeamId;
}

TOptional<TArray<TSharedRef<const FUniqueNetId>>> FOnlineTournamentTeamDetailsPS4::GetPlayerIds() const
{
	return Roster;
}

FString FOnlineTournamentTeamDetailsPS4::GetDisplayName() const
{
	return DisplayName;
}

TOptional<FVariantData> FOnlineTournamentTeamDetailsPS4::GetAttribute(const FName AttributeName) const
{
	if (const FVariantData* const FoundAttribute = Attributes.Find(AttributeName))
	{
		return *FoundAttribute;
	}

	return TOptional<FVariantData>();
}

FOnlineTournamentTeamDetailsPS4::FOnlineTournamentTeamDetailsPS4(TSharedRef<const FOnlineTournamentId> InTournamentId, FPS4NpEntity&& TeamDetails, TArray<FPS4NpEntity>&& InRoster)
	: TeamId(MakeShared<const FOnlineTournamentTeamIdPS4>(InTournamentId, TeamDetails.Platform, TeamDetails.TeamId))
	, DisplayName(TeamDetails.Name)
{
	Roster.Empty(InRoster.Num());
	for (FPS4NpEntity& MemberEntity : InRoster)
	{
		Roster.Emplace(FUniqueNetIdPS4::FindOrCreate(PS4StringToAccountId(MemberEntity.AccountId), PS4StringToOnlineId(MemberEntity.OnlineId)));
	}

	Attributes.Emplace(TEXT("IconUrl"), TeamDetails.IconUrl);
}

FOnlineAsyncWebTaskPS4::FOnlineAsyncWebTaskPS4(FOnlineSubsystemPS4& InSubsystem)
	: FOnlineAsyncTaskBasicPS4(&InSubsystem)
{
}

FOnlineAsyncWebTaskPS4::~FOnlineAsyncWebTaskPS4()
{
	// Clean up our task if we're deleted before it is completed
	if (WebAsyncTask.IsValid())
	{
		WebAsyncTask->Cancel();

		WebAsyncTask.Reset();
	}
}

void FOnlineAsyncWebTaskPS4::Initialize()
{
	FOnlineAsyncTaskBasicPS4::Initialize();

	// Create our task and set it to be processsed in a background thread
	TOptional<FWebApiPS4Task> Task = CreateWebTask();
	if (!Task.IsSet())
	{
		bWasSuccessful = false;
		bIsComplete = true;
		return;
	}

	WebAsyncTask = MakeUnique<FAsyncTask<FWebApiPS4Task>>(MoveTemp(Task.GetValue()));
	WebAsyncTask->StartBackgroundTask();
}

void FOnlineAsyncWebTaskPS4::Tick()
{
	FOnlineAsyncTaskBasicPS4::Tick();

	// We need to have created a WebAsyncTask in our call to Initialize
	if (WebAsyncTask.IsValid() && WebAsyncTask->IsDone())
	{
		{
			FWebApiPS4Task& InternalTask = WebAsyncTask->GetTask();

			if (InternalTask.GetErrorResult().WasSuccessful())
			{
				UE_LOG_ONLINE(VeryVerbose, TEXT("AsyncWebTaskPS4 Complete bWasSuccess=[1] Response=[%s]"), *InternalTask.GetResponseBody());
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("AsyncWebTaskPS4 Complete bWasSuccess=[0] Response=[%s]"), *InternalTask.GetResponseBody());
			}

			// Process our task now that it's complete
			ProcessResult(InternalTask);
		}

		// Release our object now that we're done with it
		WebAsyncTask.Reset();

		// Mark this async task as completed
		bIsComplete = true;
	}
}

FOnlineAsyncWebTaskListPS4::FOnlineAsyncWebTaskListPS4(FOnlineSubsystemPS4& InSubsystem)
	: FOnlineAsyncTaskBasicPS4(&InSubsystem)
{
}

FOnlineAsyncWebTaskListPS4::~FOnlineAsyncWebTaskListPS4()
{
	// Clean up our task if we're deleted before it is completed
	for (TUniquePtr<FAsyncTask<FWebApiPS4Task>>& Task : WebAsyncTasks)
	{
		if (Task.IsValid())
		{
			Task->Cancel();
		}
	}
}

void FOnlineAsyncWebTaskListPS4::Initialize()
{
	FOnlineAsyncTaskBasicPS4::Initialize();

	// Create our task list
	TArray<FWebApiPS4Task> NewTaskList = CreateWebTasks();

	// Check if we actually have tasks
	if (NewTaskList.Num() == 0)
	{
		bWasSuccessful = false;
		bIsComplete = true;
		return;
	}

	// Start our tasks and set then to be processsed in a background thread
	WebAsyncTasks.Empty(NewTaskList.Num());
	for (FWebApiPS4Task& NewTask : NewTaskList)
	{
		TUniquePtr<FAsyncTask<FWebApiPS4Task>> AsyncTask = MakeUnique<FAsyncTask<FWebApiPS4Task>>(MoveTemp(NewTask));
		AsyncTask->StartBackgroundTask();
		WebAsyncTasks.Emplace(MoveTemp(AsyncTask));
	}
}

void FOnlineAsyncWebTaskListPS4::Tick()
{
	FOnlineAsyncTaskBasicPS4::Tick();

	// Keep track if we have any active async tasks
	bool bHasActiveTasks = false;

	// We need to have created a WebAsyncTask in our call to Initialize
	const int32 NumTasks = WebAsyncTasks.Num();
	for (int32 i = 0; i < NumTasks; ++i)
	{
		TUniquePtr<FAsyncTask<FWebApiPS4Task>>& Task = WebAsyncTasks[i];
		if (Task.IsValid())
		{
			if (Task->IsDone())
			{
				{
					FWebApiPS4Task& InternalTask = Task->GetTask();

					if (InternalTask.GetErrorResult().WasSuccessful())
					{
						UE_LOG_ONLINE(VeryVerbose, TEXT("AsyncWebTaskPS4 Complete bWasSuccess=[1] Response=[%s]"), *InternalTask.GetResponseBody());
					}
					else
					{
						UE_LOG_ONLINE(Warning, TEXT("AsyncWebTaskPS4 Complete bWasSuccess=[0] Response=[%s]"), *InternalTask.GetResponseBody());
					}

					// Process our task now that it's complete
					ProcessResult(InternalTask, i);
				}

				// Release our object now that we're done with it
				Task.Reset();
			}
			else
			{
				// We have at least one active task still
				bHasActiveTasks = true;
			}
		}
	}

	if (!bHasActiveTasks)
	{
		// Cleanup our task list
		WebAsyncTasks.Reset();

		// Give our async task a chance to do work on the online thread now that all of our tasks have completed
		OnAllTasksComplete();

		// Mark this async task as completed
		bIsComplete = true;
	}
}
