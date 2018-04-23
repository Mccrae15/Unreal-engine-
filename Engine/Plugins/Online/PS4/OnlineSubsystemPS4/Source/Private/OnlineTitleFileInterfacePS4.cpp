// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineTitleFileInterfacePS4.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineSubsystemPS4.h"
#include "Misc/ConfigCacheIni.h"

//
// The enumeration task
//

FString FOnlineAsyncTaskPS4EnumerateTitleFiles::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4EnumerateTitleFiles bWasSuccessful:%d"), bWasSuccessful);
}

void FOnlineAsyncTaskPS4EnumerateTitleFiles::StartFetchingStatusData()
{
	// Default to having completed the request unsuccessfully.
	bIsComplete = true;
	bWasSuccessful = false;

	// Bail if we don't have any configured slots
	if (CloudPtr->ConfiguredFileCount() == 0)
	{
		return;
	}

	// Since we don't have a user, use the initial user, per the Sony documentation
	SceUserServiceUserId InitialUser;

	int Result;
	Result = sceUserServiceGetInitialUser(&InitialUser);
	if (Result < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("sceUserServiceGetInitialUser() failed when enumerating title files. Error code: 0x%08x"), Result);
		return;
	}

	// Get the title context for this user, storing it for later use
	TSharedRef<FUniqueNetIdPS4> UserId = FUniqueNetIdPS4::FindOrCreate(InitialUser);
	TitleContextId = CloudPtr->ContextCache.GetTitleContextId(CloudPtr->TssServiceLabel, UserId.Get());
 
	// Create the list of slots to query by walking the file collection and copying over the slots listed there
	RequestedSlots.Empty();
	RequestedSlots.Reserve(FileCollection->Num());

	for (FTitleFileCollection::TConstIterator It(*FileCollection); It; ++It)
	{
		RequestedSlots.Push(It->Value.SlotId);
	}

	// Clean up any previous results and set the index to 0 to prepare the enumeration to start in the first Tick() call.
	CurrentIndex = 0;
	CurrentPhase = PHASE_NEED_QUERY;

	// Ready to go, but not complete
	bIsComplete = false;
}

void FOnlineAsyncTaskPS4EnumerateTitleFiles::Tick()
{
	// NOTE: Assuming here that if bIsComplete was true, Tick() is not called.
	if (CurrentPhase == PHASE_POLLING)
	{
		// Need to poll for completion of the previous request

		// Poll for completion
		int32 OperationResult = 0;
		int Result = sceNpTusPollAsync(RequestId, &OperationResult);
		if (Result == 0)
		{
			// Processing is complete.
			// NOTE: OperationResult is what would have been returned if the call had been synchronous.
			// In this case the result is either 0 or a negative error code

			if (OperationResult == SCE_OK)
			{
				// We have status for the slot that was queried.
				// NOTE: The status function will return success even if the file is not there; it will just return 0 for the size.
				// This makes it impossible distinguish between a file being gone and a file being there with a 0 size.
				// The only use for a 0 sized file is to use its existence as a flag. Since that can still be accomplished with a 1 byte file
				// we'll consider 0 sized files as not existing

				// If the file has a size, add a new entry into the found list
				if (StatusData.contentLength > 0)
				{
					// This file is definitely there. Push the characteristic data into the FoundSlots list
					FoundSlots.Push(EnumEntry(RequestedSlots[CurrentIndex], StatusData.contentLength, StatusData.lastModified));
				}
			}
			else
			{
				// There was an error. Stop enumerating and return a failure for everything
				UE_LOG_ONLINE(Error, TEXT("sceNpTssGetDataAsync returned an error. Error code: 0x%08x"), OperationResult);
				bWasSuccessful = false;
				bIsComplete = true;
				return;
			}

			// Advance to the next requested slot
			++CurrentIndex;

			// If we've reached the end, mark as complete and successful, otherwise go back to the initial phase
			if (CurrentIndex >= RequestedSlots.Num())
			{
				bWasSuccessful = true;
				bIsComplete = true;
				return;
			}
			else
			{
				// NOTE: This will cause the next request to be issued before we exit this function
				CurrentPhase = PHASE_NEED_QUERY;
			}
		}
		else if (Result == 1)
		{
			// Processing is still in progress
			bIsComplete = false;
		}
		else
		{
			// There was an error.
			UE_LOG_ONLINE(Error, TEXT("sceNpTusPollAsync failed. Error code: 0x%08x"), Result);

			// In this case, we call it complete and unsuccessful
			bIsComplete = true;
			bWasSuccessful = false;
			return;
		}
	}

	if (CurrentPhase == PHASE_NEED_QUERY)
	{
		// Need to issue a request at the current index
		int Result;

		// Delete any current request
		if (RequestId >=0)
		{
			Result = sceNpTusDeleteRequest(RequestId);
			if (Result != SCE_OK)
			{
				UE_LOG_ONLINE(Warning, TEXT("sceNpTusDeleteRequest() failed. Error code: 0x%08x"), Result);
			}

			// Regardless of the error or not, clear out the request id
			RequestId = -1;
		}

		// Create a request ID for this query
		Result = sceNpTusCreateRequest(TitleContextId);
		if (Result < 0)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusCreateRequest() failed. Error code: 0x%08x"), Result);
			bWasSuccessful = false;
			bIsComplete = true;
			return;  // isComplete(true), wasSuccessful(false)
		}
		RequestId = Result;

		// Issue the request for the current index
		SceNpTssSlotId SlotId = RequestedSlots[CurrentIndex];
		FMemory::Memset(&StatusData, 0, sizeof(StatusData));
		Result = sceNpTssGetDataAsync
		(
			 RequestId,
			 SlotId,
			 &StatusData,
			 sizeof(StatusData),
			 NULL,
			 0,
			 NULL
		);

		if (Result == SCE_OK)
		{
			// The request has been made, we are not complete
			bIsComplete = false;
			
			// Move to the polling phase
			CurrentPhase = PHASE_POLLING;
		}
		else
		{
			// The call failed, so bail as a failure
			bIsComplete = true;
			bWasSuccessful = false;
		}
	}
}

void FOnlineAsyncTaskPS4EnumerateTitleFiles::Finalize() 
{
	// assert that we're on the game thread
	FOnlineAsyncTaskPS4::Finalize();

	// If the operation was successful, copy all the metadata from this task into the final destination
	if (WasSuccessful()) 
	{
		// Update the FileCollection 
		if (FileCollection)
		{
			// Handle updates to existing files
			for (int i = 0; i < FoundSlots.Num(); ++i)
			{
				// Find the file in the collection that matches this slot and update its enumeration values
				for (FTitleFileCollection::TIterator It(*FileCollection); It; ++It)
				{
					if (It->Value.SlotId == FoundSlots[i].SlotId)
					{
						It->Value.bDoesExist = true;
						It->Value.ContentSize = FoundSlots[i].ContentLength;
						// NOTE: The timestamp is not updated here on purpose because doing so would prevent
						// the file from being updated on the next read operation.
						// The rule is: only the read file operation updated the TimeLastUpdated field
						// It->Value.TimeLastUpdated = FoundSlots[i].TimeLastUpdated;
						break;
					}
				}
			}

			// Find files that we think exist but didn't enumerate, and clear their data
			for (FTitleFileCollection::TIterator It(*FileCollection); It; ++It)
			{
				bool bWasFound = false;
				for (int i = 0; i < FoundSlots.Num(); ++i)
				{
					if (FoundSlots[i].SlotId == It->Value.SlotId)
					{
						bWasFound = true;
						break;
					}
				}

				// If we didn't find it, then it has been removed from the server so clear it out.
				if (!bWasFound)
				{
					It->Value.Forget();
				}
			}
		}
	}

	// Clean up any allocated resources
	if (RequestId >= 0)
	{
		int Result = sceNpTusDeleteRequest(RequestId);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusDeleteRequest() failed. Error code: 0x%08x"), Result);
		}

		// Regardless of the error or not, clear out the request id
		RequestId = -1;
	}
}

void FOnlineAsyncTaskPS4EnumerateTitleFiles::TriggerDelegates()
{ 
	FOnlineAsyncTaskPS4::TriggerDelegates();

	IOnlineTitleFilePtr TitleFileInterface = Subsystem->GetTitleFileInterface();
	TitleFileInterface->TriggerOnEnumerateFilesCompleteDelegates(bWasSuccessful, TEXT(""));
}

//
// The file reading task
//

FString FOnlineAsyncTaskPS4ReadFile::ToString() const
{
	return FString::Printf(TEXT("FOnlineAsyncTaskPS4ReadFile bWasSuccessful:%d"), bWasSuccessful);
}

void FOnlineAsyncTaskPS4ReadFile::RequestFileData(const FString& InFileName)
{
	// Default to having completed the request unsucessfully.
	bIsComplete = true;
	bWasSuccessful = false;

	// Bail if we don't have any configured slots
	if (CloudPtr->ConfiguredFileCount() == 0)
	{
		return;
	}

	// Find the slot for the given filename. If it isn't configured, bail
	FileName = InFileName;
	if (!FileCollection->Contains(FileName))
	{
		return;
	}
	FPS4TitleFile &FileData = (*FileCollection)[FileName];
	SlotId = FileData.SlotId;

	// If the file is already loaded, store off the current value for the last updated time, used to optimize file fetching
	// Otherwise set the update time to 0, which will force the file to load
	if (FileData.bIsLoaded)
	{
		OldTimeLastUpdated = FileData.TimeLastUpdated;
	}
	else
	{
		OldTimeLastUpdated.tick = 0L;
	}

	// Since we don't have a user, use the initial user, per the Sony documentation
	SceUserServiceUserId InitialUser;

	// *** TODO: Pull this user stuff out into a common function since this takes a fair amount of code space each time
	// *** it is needed
	int Result;
	Result = sceUserServiceGetInitialUser(&InitialUser);
	if (Result < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceUserServiceGetInitialUser() failed. Error code: 0x%08x"), Result);
		return;
	}

	// Get the title context for this user, storing it for later use
	TSharedRef<FUniqueNetIdPS4> UserId = FUniqueNetIdPS4::FindOrCreate(InitialUser);
	TitleContextId = CloudPtr->ContextCache.GetTitleContextId(CloudPtr->TssServiceLabel, UserId.Get());

	// Create a request id
	Result = sceNpTusCreateRequest(TitleContextId);
	if (Result < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTusCreateRequest() failed. Error code: 0x%08x"), Result);
		bWasSuccessful = false;
		bIsComplete = true;
		return;  // isComplete(true), wasSuccessful(false)
	}
	RequestId = Result;

	// Make the initial request, which will be for the status only, so we can get the size of the file
	Result = sceNpTssGetDataAsync
	(
		 RequestId,
		 SlotId,
		 &StatusData,
		 sizeof(StatusData),
		 NULL,
		 0,
		 NULL
	);

	if (Result != SCE_OK)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpTssGetDataAsync() failed. Error code: 0x%08x"), Result);
		bWasSuccessful = false;
		bIsComplete = true;
		return;  // isComplete(true), wasSuccessful(false)
	}

	// Waiting for the initial query to finish
	CurrentPhase = PHASE_QUERY;

	// Ready to go, but not complete
	bIsComplete = false;
}

void FOnlineAsyncTaskPS4ReadFile::Tick()
{
	// NOTE: Assuming here that if bIsComplete was true, Tick() is not called.
	int32 OperationResult = 0;
	int Result = sceNpTusPollAsync(RequestId, &OperationResult);
	if (Result == 0)
	{
		// Processing is complete.
		// NOTE: OperationResult is what would have been returned if the call had been synchronous.
		// In this case the result is either 0 or a negative error code

		if (OperationResult == SCE_OK)
		{
			// What this means is dependent on the phase
			if (CurrentPhase == PHASE_QUERY)
			{
				// The initial query has succeeded, so clean up the current request
				if (RequestId >= 0)
				{
					Result = sceNpTusDeleteRequest(RequestId);
					if (Result != SCE_OK)
					{
						UE_LOG_ONLINE(Warning, TEXT("sceNpTusDeleteRequest() failed in phase %d. Error code: 0x%08x"), CurrentPhase, Result);
					}

					// Regardless of the error or not, clear out the request id
					RequestId = -1;
				}

				// If there is no content, end the task as unsuccessful
				if (StatusData.contentLength == 0)
				{
					bWasSuccessful = false;
					bIsComplete = true;
					return;
				}

				// If here, then the file has data, so its last modified value should be != 0
				// If the timestamp matches the old one, then the content hasn't changed since we last fetched it.
				// In that case, complete successfully here without re-fetching the data
				if (StatusData.lastModified.tick == OldTimeLastUpdated.tick)
				{
					bWasSuccessful = true;
					bIsComplete = true;
					return;
				}

				// Save off the new last updated time, for use in Finalize()
				OldTimeLastUpdated = StatusData.lastModified;

				// If here, we have to fetch the data, so allocate space for it and request the data
				FileContents.SetNumUninitialized(StatusData.contentLength);

				// Create a request id
				Result = sceNpTusCreateRequest(TitleContextId);
				if (Result < 0)
				{
					UE_LOG_ONLINE(Warning, TEXT("sceNpTusCreateRequest() failed in phase %d. Error code: 0x%08x"), CurrentPhase, Result);
					bWasSuccessful = false;
					bIsComplete = true;
					return;  // isComplete(true), wasSuccessful(false)
				}
				RequestId = Result;

				// Make the initial request, which will be for the status only, so we can get the size of the file
				Result = sceNpTssGetDataAsync
				(
					 RequestId,
					 SlotId,
					 &StatusData,
					 sizeof(StatusData),
					 FileContents.GetData(),
					 FileContents.Num(),
					 NULL
				);

				if (Result != SCE_OK)
				{
					UE_LOG_ONLINE(Warning, TEXT("sceNpTssGetDataAsync() failed in phase %d. Error code: 0x%08x"), CurrentPhase, Result);
					bWasSuccessful = false;
					bIsComplete = true;
					return;  // isComplete(true), wasSuccessful(false)
				}

				// Advance to the next phase
				CurrentPhase = PHASE_FETCH;
			}
			else
			{
				// The second phase: This was the read request, so stash the contents.
				// NOTE: The docs imply that you can only get a partial read if you requested it. However, since the read size is returned
				// it is checked here against the size requested. If there is a difference, fail the call rather than
				// assume that it was read correctly.
				if (StatusData.contentLength != FileContents.Num())
				{
					// File size changed between the two calls. Fail the call.
					// *** TODO: Log this error condition, since it should never happen anyway.
					bWasSuccessful = false;
				}
				else
				{
					bWasSuccessful = true;
				}

				// Task is complete, regardless
				bIsComplete = true;
			}
		}
		else
		{
			// There was an error. Stop enumerating and return a failure for everything
			UE_LOG_ONLINE(Error, TEXT("sceNpTusGetMultiSlotDataStatusAAsync returned an error in phase %d. Error code: 0x%08x"), CurrentPhase, OperationResult);
			bWasSuccessful = false;
			bIsComplete = true;
			return;
		}
	}
	else if (Result == 1)
	{
		// Processing is still in progress
		bIsComplete = false;
	}
	else
	{
		// There was an error.
		UE_LOG_ONLINE(Error, TEXT("sceNpTusPollAsync failed in phase %d. Error code: 0x%08x"), CurrentPhase, Result);

		// In this case, we call it complete and unsuccessful
		bIsComplete = true;
		bWasSuccessful = false;
		return;
	}
}

void FOnlineAsyncTaskPS4ReadFile::Finalize() 
{
	// assert that we're on the game thread
	FOnlineAsyncTaskPS4::Finalize();

	// If the operation was successful, copy the file contents into its final home
	if (WasSuccessful()) 
	{
		// Find the file entry and update its contents with what was fetched
		if (FileCollection && FileContents.Num() > 0)
		{
			FPS4TitleFile &File = (*FileCollection)[FileName];
			File.Contents = FileContents;
			File.bIsLoaded = true;
			File.bDoesExist = true;
			File.TimeLastUpdated = OldTimeLastUpdated;
		}
	}

	// Clean up any allocated resources
	FileContents.Empty();
	if (RequestId >= 0)
	{
		int Result = sceNpTusDeleteRequest(RequestId);
		if (Result != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpTusDeleteRequest() failed. Error code: 0x%08x"), Result);
		}

		// Regardless of the error or not, clear out the request id
		RequestId = -1;
	}
}

void FOnlineAsyncTaskPS4ReadFile::TriggerDelegates()
{ 
	FOnlineAsyncTaskPS4::TriggerDelegates();

	IOnlineTitleFilePtr TitleFileInterface = Subsystem->GetTitleFileInterface();
	TitleFileInterface->TriggerOnReadFileCompleteDelegates(bWasSuccessful, FileName);
}

//
// FOnlineTitleFilePS4 
//
static const TCHAR *OSS_PS4_TITLE_FILE = TEXT("OnlineSubsystemPS4.TitleFile");

void FOnlineTitleFilePS4::LoadConfig()
{
	// Fetch the service label to use
	int32 LabelValue;
	if (!GConfig->GetInt( OSS_PS4_TITLE_FILE, TEXT("ServiceLabel"), LabelValue, GEngineIni))
	{
		LabelValue = 0;
	}
	TssServiceLabel = LabelValue;

	// Clear the files and empty the fileset before proceeding
	ClearFiles();
	FileSet.Empty();

	// Fetch the configured list of files
	TArray<FString> TitleFileSpecs;
	int Count = GConfig->GetArray( OSS_PS4_TITLE_FILE, TEXT("Filespec"), TitleFileSpecs, GEngineIni );
	if (Count)
	{
		// We have some file specifications. They should be of the syntax: Filename SLOT=slotId
		// Parse each out to create a 
		for (int i = 0; i < Count; ++i)
		{
			TitleFileSpecs[i].TrimStartInline();
			const TCHAR *Spec = *TitleFileSpecs[i];
			FString Filename = FParse::Token(Spec, false);
			int SlotId;
			if (FParse::Value(Spec, TEXT("SLOT="), SlotId))
			{
				FileSet.Add(Filename, FPS4TitleFile(SlotId));
			}
		}
	}
}

bool FOnlineTitleFilePS4::Init()
{
	// NOTE: This module is shared with FOnlineUserCloudPS4
	int Result = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TUS);
	if ( Result != SCE_OK )
	{
		UE_LOG_ONLINE(Warning, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TUS) failed. Error code: 0x%08x"), Result);
		return false;
	}

	// The context functions aren't valid until the module is loaded, so set the functions into the context cache now
	// @todo: replace sceNpTusDeleteNpTitleCtx() with sceNpTssDeleteNpTitleCtx when SDK 1.6 is supported
	ContextCache.SetFunctions(sceNpTssCreateNpTitleCtxA, sceNpTusDeleteNpTitleCtx);

	LoadConfig();

	return true;
}

void FOnlineTitleFilePS4::Shutdown()
{
	ContextCache.DestroyAll();

	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_TUS))
	{
		int Result = sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TUS);
		if (Result != SCE_OK )
		{
			UE_LOG_ONLINE(Warning, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TUS) failed. Error code: 0x%08x"), Result);
		}
	}
}

bool FOnlineTitleFilePS4::GetFileContents(const FString& FileName, TArray<uint8>& FileContents)
{
	if (FileSet.Contains(FileName))
	{
		FPS4TitleFile &File = FileSet[FileName];
		if (File.bDoesExist)
		{
			if (File.bIsLoaded)
			{
				FileContents.Empty();
				FileContents = File.Contents;
				return true;
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("TitleFile: GetFileContents() failed. File %s is configured and enumeratoed but has not been loaded"), *FileName);
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("TitleFile: GetFileContents() failed. File %s is configured but has not been enumerated"), *FileName);
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("TitleFile:: GetFileContents() failed. File %s is not configured. Edit PS4Engine.ini to configure"), *FileName);
	}

	return false;
}

bool FOnlineTitleFilePS4::ClearFiles()
{
	// This is actually the clearing of the file contents, leaving the file structure in the FileSet.
	// In other words, this won't destroy the enumeration, it just dumps loaded file contents
	TArray<FString> Keys;
	int Count = FileSet.GetKeys(Keys);
	for (int i = 0; i < Keys.Num(); ++i)
	{
		FPS4TitleFile &File = FileSet[Keys[i]];
		File.Unload();
	}
	return true;
}

bool FOnlineTitleFilePS4::ClearFile(const FString& FileName)
{
	if (FileSet.Contains(FileName))
	{
		FileSet[FileName].Unload();
		return true;
	}
	return false;
}

void FOnlineTitleFilePS4::DeleteCachedFiles(bool bSkipEnumerated)
{
	// not implemented
}

bool FOnlineTitleFilePS4::EnumerateFiles(const FPagedQuery& Page)
{
	FOnlineAsyncTaskPS4EnumerateTitleFiles *Task = new FOnlineAsyncTaskPS4EnumerateTitleFiles(PS4Subsystem, this, &FileSet);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( Task );
	return true;
}

void FOnlineTitleFilePS4::GetFileList(TArray<FCloudFileHeader>& Files)
{
	Files.Empty();

	// Walk the list of enumerated files, returning their names into the given file array
	TArray<FString> Keys;
	int Count = FileSet.GetKeys(Keys);
	for (int i = 0; i < Keys.Num(); ++i)
	{
		const FString &Key = Keys[i];
		FPS4TitleFile &File = FileSet[Key];
		if (File.bDoesExist)
		{
			Files.Add(FCloudFileHeader(Key, Key, File.ContentSize));
		}
	}
}

bool FOnlineTitleFilePS4::ReadFile(const FString& FileName)
{
	FOnlineAsyncTaskPS4ReadFile *Task = new FOnlineAsyncTaskPS4ReadFile(PS4Subsystem, this, &FileSet, FileName);
	PS4Subsystem->GetAsyncTaskManager()->AddToInQueue( Task );
	return true;
}
