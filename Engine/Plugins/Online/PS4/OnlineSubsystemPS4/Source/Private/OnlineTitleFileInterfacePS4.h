// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "OnlineSubsystemPS4Private.h"
#include "OnlineTitleFileInterface.h"
#include "OnlineSubsystemPS4Types.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineSubsystemPS4Package.h"

// Information about a title file, as well as its contents.
// 
struct FPS4TitleFile
{
	TArray<uint8> Contents;
	int           SlotId;
	SceRtcTick    TimeLastUpdated;
	size_t        ContentSize; // enumerated content size
	bool          bDoesExist;  // true if a file exists in the given slot, based on enumeration
	bool          bIsLoaded;   // true if the file contents have been retrieved

	FPS4TitleFile() : SlotId(-1), ContentSize(0), bDoesExist(false), bIsLoaded(false)
	{
		TimeLastUpdated.tick = 0L;
	}

	FPS4TitleFile(int SlotNum) : SlotId(SlotNum), ContentSize(0), bDoesExist(false), bIsLoaded(false)
	{
		TimeLastUpdated.tick = 0L;
	}

	void Unload()
	{
		Contents.Empty();
		bIsLoaded = false;
		TimeLastUpdated.tick = 0L;
	}

	void Forget()
	{
		Unload();
		bDoesExist = false;
	}
};

typedef TArray<FPS4TitleFile> FTitleFileArray;
typedef TMap<FString, FPS4TitleFile> FTitleFileCollection;

class FOnlineAsyncTaskPS4EnumerateTitleFiles : public FOnlineAsyncTaskPS4
{
	struct EnumEntry
	{
		int        SlotId;
		size_t     ContentLength;
		SceRtcTick TimeLastUpdated;

		EnumEntry(int InSlotId, size_t InContentLength, const SceRtcTick &InLastUpdated) :
			SlotId(InSlotId),
			ContentLength(InContentLength),
			TimeLastUpdated(InLastUpdated)
		{
		}
	};

	/** The list of slots to query during enumeration */
	TArray<int> RequestedSlots;

	/** The list of slots found during the enumeration */
	TArray<EnumEntry> FoundSlots;

	/** The destination for storing the resulting data in the finalize method. Also the directory of known files */
	FTitleFileCollection *FileCollection;

	/** The title context used for all the operations */
	int TitleContextId;

	/** The current querying phase: 0 means 'need to query', and 1 means 'polling for last query results' */
	enum EnumeratePhase
	{
		PHASE_NEED_QUERY,
		PHASE_POLLING
	};
	EnumeratePhase CurrentPhase;

	/** The request id used for fetching the data */
	int RequestId;

	/** The user cloud implementation that issued this enumeration task */
	class FOnlineTitleFilePS4 *CloudPtr;

	/** Storage for current status query results*/
	SceNpTssDataStatus StatusData;

	/** Current index in the enumeration */
	int CurrentIndex;

	/** Hidden on purpose */
	FOnlineAsyncTaskPS4EnumerateTitleFiles() :
		FOnlineAsyncTaskPS4(NULL),
		FileCollection(NULL),
		CurrentPhase(PHASE_NEED_QUERY),
		RequestId(-1),
		CloudPtr(NULL),
		CurrentIndex(-1)
	{
	}

	/** Calls the PS4 API to fetch the status */
	void StartFetchingStatusData();

public:
	FOnlineAsyncTaskPS4EnumerateTitleFiles(class FOnlineSubsystemPS4* InSubsystem, class FOnlineTitleFilePS4 *InCloud, FTitleFileCollection *OutFileSet ) :
		FOnlineAsyncTaskPS4(InSubsystem),
		FileCollection(OutFileSet),
		CurrentPhase(PHASE_NEED_QUERY),
		RequestId(-1),
		CloudPtr(InCloud),
		CurrentIndex(-1)
	{
		StartFetchingStatusData();
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;
	
	/**
	 * Marshal the accumlated data into its final home
	 */
	virtual void Finalize() override;

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

class FOnlineAsyncTaskPS4ReadFile : public FOnlineAsyncTaskPS4
{
	/** The name of the file being requested */
	FString FileName;

	/** The slot containing the requestd file */
	SceNpTssSlotId SlotId;

	/** The title context used for all the operations */
	int TitleContextId;

	/** The request id used for fetching the data */
	int RequestId;

	/** The user cloud implementation that issued this enumeration task */
	class FOnlineTitleFilePS4 *CloudPtr;

	/** The contents of the file, stored locally until Finalize */
	TArray<uint8> FileContents;

	/** The destination for storing the resulting data in the finalize method. Also the directory of known files */
	FTitleFileCollection *FileCollection;

	/** Current value of the last modified time, 0 if unknown */
	SceRtcTick OldTimeLastUpdated;

	/** Storage for the status from the initial query */
	SceNpTssDataStatus StatusData;

	/** Two phases: initial query, and data fetch */
	enum ReadPhase
	{
		PHASE_QUERY,
		PHASE_FETCH
	};
	ReadPhase CurrentPhase;

	/** Hidden on purpose */
	FOnlineAsyncTaskPS4ReadFile() :
		FOnlineAsyncTaskPS4(NULL),
		RequestId(-1),
		CloudPtr(NULL),
		FileCollection(NULL),
		CurrentPhase(PHASE_QUERY)
	{
	}

	/** Calls the PS4 API to fetch the file contents */
	void RequestFileData(const FString& InFileName);

public:
	FOnlineAsyncTaskPS4ReadFile(class FOnlineSubsystemPS4* InSubsystem, class FOnlineTitleFilePS4 *InCloud, FTitleFileCollection *OutFileSet, const FString& InFileName ) :
		FOnlineAsyncTaskPS4(InSubsystem),
		RequestId(-1),
		CloudPtr(InCloud),
		FileCollection(OutFileSet),
		CurrentPhase(PHASE_QUERY)
	{
		RequestFileData(InFileName);
	}

	/**
	 *	Get a human readable description of task
	 */
	virtual FString ToString() const override;

	/**
	 * Give the async task time to do its work
	 * Can only be called on the async task manager thread
	 */
	virtual void Tick() override;
	
	/**
	 * Marshal the accumlated data into its final home
	 */
	virtual void Finalize() override;

	/**
	 *	Async task is given a chance to trigger it's delegates
	 */
	virtual void TriggerDelegates() override;
};

class FOnlineTitleFilePS4 : public IOnlineTitleFile
{
private:
	/** Reference to the main PS4 subsystem */
	class FOnlineSubsystemPS4* PS4Subsystem;

	/** The list of available files, indexed by filename */
	FTitleFileCollection FileSet;

PACKAGE_SCOPE:
	FOnlinePS4ContextCache ContextCache;

	/** The configured service label to use on all calls */
	SceNpServiceLabel TssServiceLabel;

private:
	FOnlineTitleFilePS4() :
		PS4Subsystem(NULL),
		// NOTE: sceNpTssDeleteNpTitleCtx() is currently a macro that calls sceNpTusDeleteNpTitleCtx(), despite the sceNpTssCreateNpTitleCtxA being
		// a real function. As such, it can't be passed as a function pointer below.
		// @todo: switch to sceNpTssDeleteNpTitleCtx() if the definition changes in the header
		ContextCache(sceNpTssCreateNpTitleCtxA, sceNpTusDeleteNpTitleCtx)
	{
	}

PACKAGE_SCOPE:

	FOnlineTitleFilePS4(class FOnlineSubsystemPS4* InSubsystem) :
		PS4Subsystem(InSubsystem),
		// NOTE: sceNpTssDeleteNpTitleCtx() is currently a macro that calls sceNpTusDeleteNpTitleCtx(), despite the sceNpTssCreateNpTitleCtxA being
		// a real function. As such, it can't be passed as a function pointer below.
		// @todo: switch to sceNpTssDeleteNpTitleCtx() if the definition changes in the header
		ContextCache(sceNpTssCreateNpTitleCtxA, sceNpTusDeleteNpTitleCtx)
	{
	}

	/** Load the configuration data */
	void LoadConfig();

	bool Init();
	void Shutdown();

	int ConfiguredFileCount() { return FileSet.Num(); }

	/** IOnlineTitleFile interface */
	virtual bool GetFileContents(const FString& FileName, TArray<uint8>& FileContents);
	virtual bool ClearFiles();
	virtual bool ClearFile(const FString& FileName);
	virtual void DeleteCachedFiles(bool bSkipEnumerated);
	virtual bool EnumerateFiles(const FPagedQuery& Page = FPagedQuery());
	virtual void GetFileList(TArray<FCloudFileHeader>& Files);
	virtual bool ReadFile(const FString& FileName);
};



