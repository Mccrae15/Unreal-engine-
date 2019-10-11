// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreTypes.h"
#include "GenericPlatform/StandardPlatformString.h"


/**
 * PS4 string implementation.
 */
struct FPS4PlatformString
	: public FStandardPlatformString
{

};


// default implementation
typedef FPS4PlatformString FPlatformString;
