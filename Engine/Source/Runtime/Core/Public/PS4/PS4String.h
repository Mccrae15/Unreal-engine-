// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreTypes.h"
#include "GenericPlatform/StandardPlatformString.h"


/**
 * PS4 string implementation.
 */
struct FPS4PlatformString
	: public FStandardPlatformString
{
	static const ANSICHAR* GetEncodingName()
	{
		return "UTF-16LE";
	}

	static const bool IsUnicodeEncoded = true;
};


// default implementation
typedef FPS4PlatformString FPlatformString;
