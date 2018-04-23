// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "OnlineSubsystemPS4Module.h"
#include "OnlineSubsystemModule.h"
#include "OnlineSubsystem.h"
#include "SocketSubsystem.h"
#include "ModuleManager.h"

/** pre-pended to all PSN logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("PSN: ")

#include <np.h>
#include <np/np_play_together.h>

#include <libsysmodule.h>
#include <libhttp.h>
#include <libssl.h>

#include <np_toolkit2.h>
namespace NpToolkit = sce::Toolkit::NP::V2;

#include "PS4Application.h"
#include "PS4Utilities.h"

