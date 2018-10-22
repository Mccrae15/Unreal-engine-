// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"


namespace PS4Media
{
	/**
	 * Convert an AvPlayer event to string.
	 *
	 * @param Event The event to convert.
	 * @return The corresponding string.
	 */
	FString EventToString(int32 Event);

	/**
	 * Convert a language code to string.
	 *
	 * @param Language The language code to convert.
	 * @return The corresponding string.
	 */
	FString LanguageToString(const uint8* Language);

	/**
	 * Convert an AvPlayer result code to string.
	 *
	 * @param Result The result code to convert.
	 * @return The corresponding string.
	 */
	FString ResultToString(int32 Result);
}
