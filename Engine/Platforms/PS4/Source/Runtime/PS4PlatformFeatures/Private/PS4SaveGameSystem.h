// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SaveGameSystem.h"
#include "SonySaveGameSystem.h"
#include <save_data.h>
#include <user_service.h>
#include <save_data_dialog.h>

//
// Design Notes:
//   This class works by maintaining a context at class scope that applies to multiple method calls.
// The most important are the MountDirName and the CurrentUser. These are needed by almost all of the 
// internal methods, i.e you must set the current user and mount dir name prior to attempting mounts, finding games, etc.
//


class FPS4SaveGameSystem : public FSonySaveGameSystem
{
public:
	FPS4SaveGameSystem();
	virtual ~FPS4SaveGameSystem();

protected:
	virtual const TCHAR* GetSaveFileName() override;
	virtual int32 Mount(const SceSaveDataMountMode Mode, SceUserServiceUserId UserId, SceSaveDataDirName& DirName, SceSaveDataBlocks Blocks, SceSaveDataMountResult& MountResult) override;
	virtual int32 Unmount(SceSaveDataMountPoint* MountPoint, bool bCommit) override;
	virtual SceSaveDataDialogSystemMessageType GetNoSpaceMessageType() override;
};
