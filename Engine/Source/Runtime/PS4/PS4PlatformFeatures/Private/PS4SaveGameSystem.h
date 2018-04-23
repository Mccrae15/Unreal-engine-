// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SaveGameSystem.h"
#include <save_data.h>
#include <user_service.h>
#include <save_data_dialog.h>

//
// Design Notes:
//   This class works by maintaining a context at class scope that applies to multiple method calls.
// The most important are the MountDirName and the CurrentUser. These are needed by almost all of the 
// internal methods, i.e you must set the current user and mount dir name prior to attempting mounts, finding games, etc.
//


enum class EGameDelegates_SaveGame : short;

class FPS4SaveGameSystem : public ISaveGameSystem
{
public:
	FPS4SaveGameSystem();
	virtual ~FPS4SaveGameSystem();

	// ISaveGameSystem interface
	virtual bool PlatformHasNativeUI() override;

	virtual bool DoesSaveGameExist(const TCHAR* Name, const int32 UserIndex) override
	{
		return ESaveExistsResult::OK == DoesSaveGameExistWithResult(Name, UserIndex);
	}
	
	virtual ESaveExistsResult DoesSaveGameExistWithResult(const TCHAR* Name, const int32 UserIndex) override;

	virtual bool SaveGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, const TArray<uint8>& Data) override;

	virtual bool LoadGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex, TArray<uint8>& Data) override;

	virtual bool DeleteGame(bool bAttemptToUseUI, const TCHAR* Name, const int32 UserIndex) override { return false; }

private:
	/**
	 * Initializes the SaveData library then loads and initializes the SaveDialog library
	 */
	bool Initialize();

	/**
	 * Terminates and unloads the SaveDialog library then terminates the SaveData library
	 */
	void Shutdown();

	/**
	 * Get the ID of the user under which saved games are to be managed
	 */
	SceUserServiceUserId GetSaveGameUserId(const int32 UserIndex);
	
	/**
	 * Sets the ID of the user for which subsequent save/load operations are to be done
	 */
	void SetCurrentUser(const SceUserServiceUserId User);

	/**
	 * Fills out the saved game list with the known saved games
	 */
	void FindSavedGames();

	/**
	 * Fills out the mount configuration data with defaults based on the given mode
	 */
	void FillOutSceSaveDataMount(const SceSaveDataMountMode Mode, SceSaveDataMount2 *Mount);

	/**
	 * Sets the name of the directory to mount, using a TCHAR string
	 */
	void SetMountDirName(const TCHAR *DirName);

	/**
	 * Sets the name of the directory to mount, using a UTF-8 string
	 */
	void SetMountDirName(const char *DirName);

	/**
	 * Delete the last set mount directory
	 */
	bool DeleteSavedGame(); 

	//
	// General UI functions
	//

	/**
	 * Fill out the save game dialog data with general, common information
	 * The configuration will be specialized from there
	 */
	void FillBasicSaveGameDialogData();

	/**
	 * Displays the currently configured dialog and fetches the results
	 */
	bool DisplayDialog();

	//
	// Specialized UI functions
	//

	/**
	 * Method for using the GUI to select from an existing saved game or to create a new saved game.
	 * If it returns true, then SetMountDirName() will have been called with the name of the selected or created
	 * save data directory name. Otherwise the mount directory will be in a undefined state.
	 */
	bool SelectOrCreateSavedGame(SceSaveDataParam &MetaData);
	
	/** 
	 * General error display function
	 */
	void DisplayError(int32 errorCode, SceSaveDataDialogType DialogType);

	/**
	 * Specialized error display for the save game code path
	 */
	void DisplaySaveError(int32 errorCode);

	/**
	 * Specialized error display for the load game code path
	 */
	void DisplayLoadError(int32 errorCode);

	/**
	 * Manages display of the delete dialog and associated input processing
	 */
	bool DisplayDeleteDialog();

	/**
	 * Handles all system message dialogs
	 */
	bool DisplaySystemMessageDialog (SceSaveDataDialogType DialogType, SceSaveDataDialogSystemMessageType MessageType);

	/**
	 * Displays a confirm dialog specialized for delete
	 */
	bool DisplayDeleteConfirm();

	/**
	 * General confirm dialog, with no result handling
	 */
	bool DisplayConfirm(SceSaveDataDialogType DialogType);

	/**
	 * Display the corrupted save game dialog
	 */
	void DisplayCorruptDialog(SceSaveDataDialogType DialogType);

	// Utilities for save game delegate. Converts data to/from Sony-friendly UTF8 format

	/**
	 * Fetches a value from the save game delegate and converts to UTF-8 so the result can be readily processed by the Sony functions
	 */
	bool GetSaveGameParameter (const TCHAR* SaveName, EGameDelegates_SaveGame Key, char * KeyValue, int KeyValueSize);

	/**
	 * Fetches a value from the save game delegate and converts to UTF-8 so the result can be readily processed by the Sony functions
	 */
	bool GetSaveGameParameter (const char * SaveName, EGameDelegates_SaveGame Key, char * KeyValue, int KeyValueSize);

	/**
	 * Utility to find an available slot among the saved UI generated saved games
	 */
	int FindSaveSlot();

	// Async saving and loading

	/**
	 * Saves a uint8 array to the given filename via a polled async call
	 */
	int32 SaveArrayToSceFile( const TArray<uint8>& Array, const TCHAR *Filename );

	/**
	 * Reads in a uint8 array from the given filename via a polled async call
	 */
	int32 LoadSceFileToArray( TArray<uint8>& Result, const TCHAR *Filename );

	ESaveExistsResult PlatformResultToGenericResult(const int32 ResultCode)
	{
		switch (ResultCode)
		{
			case SCE_OK:
				return ESaveExistsResult::OK;

			case SCE_SAVE_DATA_ERROR_NOT_FOUND:
				return ESaveExistsResult::DoesNotExist;

			case SCE_SAVE_DATA_ERROR_BROKEN:
				return ESaveExistsResult::Corrupt;

			default:
				return ESaveExistsResult::UnspecifiedError;
		};
	};

private:
	bool bIsInitialized;
	bool bHasNativeUI;

	// Context data for the various sceXXX calls
	SceUserServiceUserId CurrentUser;  // currently selected user
	SceUserServiceLoginUserIdList UserList;
	SceSaveDataDirName MountDirName; // name of currently mounted (or requested to mount) save game
	SceSaveDataDirName SavedGameDirs[SCE_SAVE_DATA_DIRNAME_MAX_COUNT];  // the names of all the currently existing saved game directories
	uint32 SavedGameCount; // count of valid saved game directories in SavedGameDirs

	// SaveData dialog parameters
	SceSaveDataDialogParam SaveDialogParam;
	SceSaveDataDialogItems SaveDialogItems;
	SceSaveDataDialogNewItem SaveDialogNewItem;
	SceSaveDataDialogSystemMessageParam SaveDialogMsgParam;
	SceSaveDataDialogErrorCodeParam SaveDialogErrorParam;
	SceSaveDataDialogAnimationParam  SaveDialogAnimParam;

	// Results for SaveData dialogs
	SceSaveDataDialogResult SaveDialogResults;
	SceSaveDataDirName SelectedDirName;
	SceSaveDataParam DialogSaveDataParam;
};
