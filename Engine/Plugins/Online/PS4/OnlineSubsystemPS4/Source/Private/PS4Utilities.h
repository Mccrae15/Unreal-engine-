// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"

/**
 * Returns the full real name, or verified account name, of a user's profile.
 */
FString PS4FullRealName(NpToolkit::UserProfile::NpProfile const& Profile);

/** Converts an SceNpOnlineId to an FString */
inline FString PS4OnlineIdToString(SceNpOnlineId OnlineId)
{
	return ANSI_TO_TCHAR(OnlineId.data);
}

/** Converts an FString to an SceNpOnlineId */
inline SceNpOnlineId PS4StringToOnlineId(FString const& String)
{
	SceNpOnlineId OnlineId = {};
	FCStringAnsi::Strcpy(OnlineId.data, TCHAR_TO_ANSI(*String));
	return OnlineId;
}

/** Converts an SceNpAccountId to an FString */
inline FString PS4AccountIdToString(SceNpAccountId AccountId)
{
	static_assert(sizeof(SceNpAccountId) == sizeof(uint64), "SceNpAccountId must be the same size as uint64.");
	return FString::Printf(TEXT("%llu"), (uint64)AccountId);
}

/** Converts an FString to an SceNpAccountId */
inline SceNpAccountId PS4StringToAccountId(const FString& String)
{
	// @todo Use something safer than Atoi64...
	static_assert(sizeof(SceNpAccountId) == sizeof(uint64), "SceNpAccountId must be the same size as uint64.");
	return (SceNpAccountId)FCString::Atoi64(*String);
}

/** Converts an FString to an SceNpSessionId */
inline SceNpSessionId PS4StringToSessionId(const FString& String)
{
	SceNpSessionId SessionId;
	FCStringAnsi::Strcpy(SessionId.data, TCHAR_TO_ANSI(*String));
	return SessionId;
}
