// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineTournamentPS4NpTypes.h"
#include "OnlineTournamentPS4InterfaceTypes.h"

EOnlineTournamentParticipantState FPS4NpRegisteredUser::GetState() const
{
	if (Status == TEXT("checkedIn"))
	{
		return EOnlineTournamentParticipantState::CheckedIn;
	}

	return EOnlineTournamentParticipantState::Registered;
}

TSharedPtr<const FUniqueNetId> FPS4NpRegisteredUser::GetPlayerId() const
{
	if (!AccountId.IsEmpty() && !OnlineId.IsEmpty())
	{
		return FUniqueNetIdPS4::FindOrCreate(PS4StringToAccountId(AccountId), PS4StringToOnlineId(OnlineId));
	}
	else if (!AccountId.IsEmpty())
	{
		return FUniqueNetIdPS4::FindOrCreate(PS4StringToAccountId(AccountId));
	}

	return TSharedPtr<const FUniqueNetId>();
}

EOnlineTournamentParticipantState FPS4NpRegisteredRoster::GetState() const
{
	if (Status == TEXT("checkedIn"))
	{
		return EOnlineTournamentParticipantState::CheckedIn;
	}

	return EOnlineTournamentParticipantState::Registered;
}
