// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <wctype.h>
#include <sceerror.h>
#include <kernel.h>
#include <pthread.h>

// Set up compiler pragmas, etc
#include "SonyPlatformCompilerSetup.h"

// map the Windows functions (that UE4 unfortunately uses be default) to normal functions
#define _alloca alloca

struct tagRECT
{
	int32 left;
	int32 top;
	int32 right;
	int32 bottom;
};
typedef struct tagRECT RECT;

#define OUT
#define IN