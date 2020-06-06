// Copyright Epic Games, Inc. All Rights Reserved.

#include <limits.h>
#include <unistd.h>

#define U_HAVE_DIRENT_H 0

// Use a bogus platform value
#define U_PLATFORM 9999

// Disable some unsupported things
#define U_PLATFORM_HAS_TZSET    0
#define U_PLATFORM_HAS_TIMEZONE 0
#define U_PLATFORM_HAS_TZNAME   0
#define U_PLATFORM_HAS_GETENV   0
