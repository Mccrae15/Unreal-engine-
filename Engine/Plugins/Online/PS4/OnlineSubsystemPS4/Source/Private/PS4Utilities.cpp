// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4Utilities.h"

FString PS4FullRealName(NpToolkit::UserProfile::NpProfile const& Profile)
{
	if (Profile.isVerifiedAccount)
	{
		// Verified accounts never return a "real name". Return the display name without further processing.
		return ANSI_TO_TCHAR(Profile.personalDetails.verifiedAccountDisplayName);
	}
	else
	{
		auto const& RealName = Profile.personalDetails.realName;

		// *** TODO: Are there cultural differences in how First Middle Last names are packed into a string?
		FString FullRealName;
		bool bHasFirstName = FCStringAnsi::Strlen(RealName.firstName) > 0;

		// If no first name is given then either this profile doesn't have their real name set on their account or they haven't given us access to it.
		if (bHasFirstName)
		{
			// Note: Middle name appears optional with PSN accounts where first and last names are required
			bool bHasMiddle = FCStringAnsi::Strlen(RealName.middleName) > 0;

			if (bHasMiddle)
			{
				FullRealName = FString::Printf(TEXT("%s %s %s"),
					ANSI_TO_TCHAR(RealName.firstName),
					ANSI_TO_TCHAR(RealName.middleName),
					ANSI_TO_TCHAR(RealName.lastName));
			}
			else
			{
				FullRealName = FString::Printf(TEXT("%s %s"),
					ANSI_TO_TCHAR(RealName.firstName),
					ANSI_TO_TCHAR(RealName.lastName));
			}
		}

		return FullRealName;
	}
}
