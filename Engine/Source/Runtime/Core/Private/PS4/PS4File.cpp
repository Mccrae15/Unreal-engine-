// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4File.h"
#include "Stats/StatsMisc.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "CoreGlobals.h"
#include "fios2.h"
#include "SecureHash.h"
#include "Map.h"
#include "App.h"
#include "Misc/ScopeLock.h"
#include <app_content.h>
#include <libsysmodule.h>

#include "PlatformFileCommon.h"

/**
 * Turn a UE4 filename into a PS4 filename. DO NOT EVER CACHE THE RETURN FROM THIS!
 */
#define ToPS4Filename(Filename) (TCHAR_TO_UTF8(*NormalizeFileName(Filename)))
#define ToPS4Directoryname(Filename) (TCHAR_TO_UTF8(*NormalizeDirectory(Filename)))

#define PS4FILE_SCOPE_SECONDS_COUNTER SCOPE_SECONDS_COUNTER

// make an FTimeSpan object that represents the "epoch" for time_t (from a stat struct)
const FDateTime PS4Epoch(1970, 1, 1);

namespace
{
	FORCEINLINE FDateTime PS4FiosDateToUEDateTime(const SceFiosDate& InFiosDate)
	{
		// SceFiosDate is in nanoseconds, FTimespan is in 100-nanosecond intervals
		return PS4Epoch + FTimespan(static_cast<int64>(InFiosDate * 0.01));
	}

	FFileStatData PS4FiosStatToUEFileData(const SceFiosStat& InFiosStat)
	{
		const bool bIsDirectory = (InFiosStat.statFlags & SCE_FIOS_STATUS_DIRECTORY);

		int64 FileSize = -1;
		if (!bIsDirectory)
		{
			FileSize = InFiosStat.fileSize;
		}

		return FFileStatData(
			PS4FiosDateToUEDateTime(InFiosStat.creationDate), 
			PS4FiosDateToUEDateTime(InFiosStat.accessDate), 
			PS4FiosDateToUEDateTime(InFiosStat.modificationDate), 
			FileSize, 
			bIsDirectory,
			((InFiosStat.statFlags & SCE_FIOS_STATUS_WRITABLE) == 0)
			);
	}
}

// Buffers for FIOS2 initialization.
// @todo PS4 - determine optimal buffer sizes
int64 OpStorage[64 * 1024];
int64 ChunkStorage[64 * 1024];
int64 FhStorage[64 * 1024];
int64 DhsStorage[1024];

static FString SandboxedTransformedFileRoot;
static const FString TransformedFileRoot(TEXT("deepfiles"));
static const TCHAR* TransformedMetaExt = TEXT(".ue4deepmeta");
static const TCHAR* TransformedMetaDataName = TEXT("origpath.ue4deepmeta");

/** Cached lowercase directory paths for logs and screenshots.  To enable reading/writing to /data/ for these always */
static FString PS4CachedLogDirectory;
static FString PS4CachedScreenShotDirectory;
static FString PS4CachedProfilingDirectory;

// data that gets cached to /data/ is now sandboxed by project so that deploys can be iterative and fast.
static FString GPS4SandboxedFileRootDirectory;
static FString GPS4DebugDataRootDirectory;
static FString GPS4PersistentDownloadDirectory(TEXT("/download0"));

// /temp0 directory
static FString GPS4TempDirectory;
static SceAppContentMountPoint	GPS4Temp0MountPoint;

// PS4 filesys limits you to 8 levels of folder hierarchy.  When our paths exceed that we generate a new path based on a hash of the full path.
// We also write out the original path in a metadata file in the new location so that IterateDirectories can return ALL the correct files, and
// we can detect collisions.  Keep a map of all the hashes we've done so far so we can quickly detect collisions.
static TMap<FString, FString> CompressedPathMap;

// Path shortening will break logical recursive directory iteration if we rely solely on the PS4 filesystem (because the directory structure doesn't actually exist on disk).
// To keep the iteration of the logical un-shortened structure, we keep a map of parent directory -> child directories.
static TMap<FString, TSet<FString>> CompressedSubDirMap;
static FCriticalSection CompressPathMapMutex;

bool bWriteMetaDataFiles = false;
bool bAllowPS4FileShortening = true;

// enable this define to write logs and screenshots to /data consistently across all methods of running the game.
// crucially this lets installed pkg's write logs and screenshots successfully.
#if !UE_BUILD_SHIPPING
#define PS4_FORCE_LOGS_SCREENSHOTS_TO_DEVICE 1
#else
#define PS4_FORCE_LOGS_SCREENSHOTS_TO_DEVICE 0
#endif

#define TRACK_READAHEAD_STATS STATS

#if TRACK_READAHEAD_STATS
#define READAHEAD_STAT(s) s
#else
#define READAHEAD_STAT(s)
#endif

static int32 GEnablePS4Filereadahead = 0;

//@todoio this actually crashes with GEnablePS4Filereadahead=1 if I don't have a minimum read size. I don't really want read ahead with the event driven loader anyway. 
static FAutoConsoleVariableRef CVarEnablePS4Filereadahead(
	TEXT("ps4.EnableFileReadahead"),
	GEnablePS4Filereadahead,
	TEXT("Enables file readahead on PS4\n") \
	TEXT("0 - No readahead. Each IO request is serviced as requested.\n") \
	TEXT("1 - Enable readahead. IO is buffered in fixed size chunks based on the read location, and async loaded ahead of use. Good for contiguous reading of large files, bad for small random access to big files\n"),
	ECVF_ReadOnly
	);

#if TRACK_READAHEAD_STATS
struct FPS4FileReadStats
{
	int64 NumReads = 0;
	int64 NumBytesRead = 0;
	int64 NumCacheMisses = 0;
	int64 NumFileSwitches = 0;
	int64 NumSuccessfulReadAheads = 0;
	int64 NumUnsuccessfulReadAheads = 0;
	double ReadTime = 0.0;
	double BlockTime = 0.0;
	double UncachedReadTime = 0.0;
	double MemCopyTime = 0.0;
	double SetReadPositionTime = 0.0;
	double CancelReadTime = 0.0;
	double Rate = 0.0;
};

TArray<FPS4FileReadStats*> GReadaheadCacheStats;
static uint64 GNumNonCachedReads = 0;
static uint64 GNonCachedReadSize = 0;
static double GNonCachedReadTime = 0;
#endif

struct FReadaheadCacheBuffer
{
	enum class EState
	{
		Empty,
		Reading,
		Ready,
	};

	static const int64 BufferSize = 128 * 1024;

	uint8 Data[BufferSize];
	SceFiosOp Op;
	int64 Offset;
	int64 Size;
	EState State;

	FReadaheadCacheBuffer()
		: State(EState::Empty)
	{
	}

	bool ContainsOffset(int64 InPosition) const
	{
		return State != EState::Empty && (InPosition >= Offset) && (InPosition < (Offset + Size));
	}

	bool IsFinished() const
	{
		return State == EState::Ready || (State == EState::Reading && sceFiosOpIsDone(Op));
	}

	bool FinishRead()
	{
		if (State == EState::Reading)
		{
			int Result = sceFiosOpSyncWait(Op);
			Op = SceFiosOp();

			if (Result != SCE_FIOS_OK)
			{
				State = EState::Empty;
				return false;
			}
			else
			{
				State = EState::Ready;
				return true;
			}
		}

		return true;
	}

	void CancelRead()
	{
		if (State == EState::Reading)
		{
			sceFiosOpCancel(Op);
			sceFiosOpSyncWait(Op);
			Op = SceFiosOp();
			State = EState::Empty;
		}
	}

	void Reset()
	{
		CancelRead();
		State = EState::Empty;
	}

	int64 GetNumValidBytes(int64 InPosition) const
	{
		check(ContainsOffset(InPosition));
		int64 StartIndex = InPosition - Offset;
		return Size - StartIndex;
	}
};

class FPS4FileHandle;

class FReadaheadCache
{
public:

	FReadaheadCache()
		: Owner(nullptr)
	{
	}

	~FReadaheadCache()
	{
		Release(Owner);
	}

	void Aquire(FPS4FileHandle* InOwner);
	int64 Read(FPS4FileHandle* InFileHandle, uint8* InDestination, int64 InBytesToRead, int64 InReadPosition);
	void Release(FPS4FileHandle* InOwner);

	static FReadaheadCache* Get()
	{
		static __thread FReadaheadCache* Instance = nullptr;

		if (GEnablePS4Filereadahead)
		{
			checkf(0, TEXT("GEnablePS4Filereadahead is ignored using the new async IO subsystem. It is never a win."));
			GEnablePS4Filereadahead = false;
		}

		if (Instance == nullptr)
		{
			if (GEnablePS4Filereadahead)
			{
				Instance = new FReadaheadCache();
				READAHEAD_STAT(GReadaheadCacheStats.Add(&Instance->Stats));
			}
		}
		else
		{
			if (!GEnablePS4Filereadahead)
			{
				READAHEAD_STAT(GReadaheadCacheStats.Remove(&Instance->Stats));
				delete Instance;
				Instance = nullptr;
			}
		}

		return Instance;
	}

#if TRACK_READAHEAD_STATS
	FPS4FileReadStats& GetStats()
	{
		return Stats;
	}
#endif

private:

	static const int32 MaxBuffers = 2;
	FReadaheadCacheBuffer Buffers[MaxBuffers];

	FPS4FileHandle* Owner;

#if TRACK_READAHEAD_STATS
	FPS4FileReadStats Stats;
#endif

	int32 FindExistingBuffer(uint64 InPosition);
	void SetReadPosition(int64 InPosition);
	bool DoUnsuccessfulReadaheadCatchup(FReadaheadCacheBuffer& Buffer);
};

/* IPlatformFile interface
 *****************************************************************************/

IPlatformFile& IPlatformFile::GetPlatformPhysical()
{
	static FPS4PlatformFile Singleton;
	return Singleton;
}

/**
* PS4 version of the file handle registry
*/
class FPS4FileRegistry : public FFileHandleRegistry
{
public:
	FPS4FileRegistry()
		: FFileHandleRegistry(200)
	{
	}

protected:
	virtual FRegisteredFileHandle* PlatformInitialOpenFile(const TCHAR* Filename) override;
	virtual bool PlatformReopenFile(FRegisteredFileHandle* Handle) override;
	virtual void PlatformCloseFile(FRegisteredFileHandle* Handle) override;
};
static FPS4FileRegistry GFileRegistry;


/* FPS4FileHandle class
 *****************************************************************************/


class CORE_API FPS4FileHandle
	: public FRegisteredFileHandle
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InFileHandle - The file handle.
	 * @param InFilename - The file name.
	 */
	FPS4FileHandle( SceFiosFH InFileHandle, const TCHAR* InFilename, bool InFileOpenAsWrite)
		: FileHandle(InFileHandle)
		, Filename(InFilename)
        , FileSize(0)
		, FileOpenAsWrite(InFileOpenAsWrite)
	{
		check(FileHandle != 0);

        // Only files opened for read will be managed
        if (!FileOpenAsWrite)
        {
			FileSize = sceFiosFHGetSize( FileHandle );
        }
		Seek(0);
	}

	/**
	 * Destructor.
	 */
	virtual ~FPS4FileHandle()
	{
		if (FReadaheadCache* Cache = FReadaheadCache::Get())
		{
			Cache->Release(this);
		}

		if (FileOpenAsWrite)
        {
		    sceFiosFHCloseSync(nullptr, FileHandle);
        }
		else
		{
			// only track registry for read files
			GFileRegistry.UnTrackAndCloseFile(this);
		}
		FileHandle = 0;
	}


public:

	// Begin IFileHandle interface

	virtual bool Read( uint8* Destination, int64 BytesToRead ) override
	{
		if (BytesToRead <= 0)
		{
			return false;
		}

		int64 BytesRead = 0;
		// handle virtual file handles
		GFileRegistry.TrackStartRead(this);

		if (FReadaheadCache* Cache = FReadaheadCache::Get())
		{
#if TRACK_READAHEAD_STATS
			FPS4FileReadStats& Stats = Cache->GetStats();
			double& ReadTime = Stats.ReadTime;
			{
				PS4FILE_SCOPE_SECONDS_COUNTER(ReadTime);
#endif
				BytesRead = Cache->Read(this, Destination, BytesToRead, FileOffset);
#if TRACK_READAHEAD_STATS
			}

			Stats.NumReads++;
			Stats.NumBytesRead += BytesRead;
			Stats.Rate = (double)(Stats.NumBytesRead) / 1024.0 / 1024.0 / Stats.ReadTime;
#endif
		}
		else
		{
#if TRACK_READAHEAD_STATS
			READAHEAD_STAT(GNumNonCachedReads++);
			READAHEAD_STAT(GNonCachedReadSize += BytesToRead);
			PS4FILE_SCOPE_SECONDS_COUNTER(GNonCachedReadTime);
#endif
			FScopedDiskUtilizationTracker Tracker(BytesToRead, FileOffset);
			BytesRead += sceFiosFHPreadSync(nullptr, FileHandle, Destination, BytesToRead, FileOffset);

		}

		// handle virtual file handles
		GFileRegistry.TrackEndRead(this);
		FileOffset += BytesRead;
		return BytesRead == BytesToRead;
	}

	virtual bool Seek( int64 NewPosition ) override
	{
		check(NewPosition >= 0);
		if (!FileOpenAsWrite)
		{
			FileOffset = NewPosition >= FileSize ? FileSize - 1 : NewPosition;
			return true;
		}
		else
		{
			return (sceFiosFHSeek(FileHandle, NewPosition, SCE_FIOS_SEEK_SET) >= 0);
		}
	}

	virtual bool SeekFromEnd( int64 NewPositionRelativeToEnd = 0 ) override
	{
		check(NewPositionRelativeToEnd <= 0);

		if (!FileOpenAsWrite)
		{
            FileOffset = (NewPositionRelativeToEnd >= FileSize) ? 0 : ( FileSize + NewPositionRelativeToEnd - 1 );
            return true;
        }
        else
        {
		    return (sceFiosFHSeek(FileHandle, NewPositionRelativeToEnd, SCE_FIOS_SEEK_END) >= 0);
        }
	}

	virtual int64 Size() override
	{
		if (!FileOpenAsWrite)
        {
            return FileSize;
        }
        else
        {
		    return sceFiosFHGetSize(FileHandle);
        }
	}

	virtual int64 Tell() override
	{
		if (!FileOpenAsWrite)
        {
            return FileOffset;
        }
        else
        {
		    return sceFiosFHTell(FileHandle);
        }
	}

	virtual bool Write( const uint8* Source, int64 BytesToWrite ) override
	{
		check(FileOpenAsWrite);
		return (sceFiosFHWriteSync(nullptr, FileHandle, Source, BytesToWrite) == BytesToWrite);
	}

	// End IFileHandle interface

private:

	// Holds the internal file handle.
	SceFiosFH FileHandle;

	// Holds the name of the file that this handle represents. Kept around for possible reopen of file.
	FString Filename;

    // Current file offset; valid if a managed handle.
    int64 FileOffset;

	// Cached file size
	int64 FileSize;
	// track if file is open for write
	bool FileOpenAsWrite;

	friend class FReadaheadCache;
	friend class FPS4FileRegistry;
};

FRegisteredFileHandle* FPS4FileRegistry::PlatformInitialOpenFile(const TCHAR* Filename)
{
	SceFiosFH FileHandle;
	SceFiosOpenParams Params;

	Params.openFlags = SCE_FIOS_O_RDONLY;

	int32 Ret = sceFiosFHOpenSync(nullptr, &FileHandle, TCHAR_TO_UTF8(Filename), &Params);
	if (Ret == SCE_FIOS_OK)
	{
		return new FPS4FileHandle(FileHandle, Filename, false);
	}

	return nullptr;

}

bool FPS4FileRegistry::PlatformReopenFile(FRegisteredFileHandle* Handle)
{
	FPS4FileHandle* PS4Handle = (FPS4FileHandle*)Handle;

	SceFiosOpenParams Params;
	Params.openFlags = SCE_FIOS_O_RDONLY;

	if (sceFiosFHOpenSync(nullptr, &PS4Handle->FileHandle, TCHAR_TO_UTF8(*PS4Handle->Filename), &Params) == SCE_FIOS_OK)
	{
		return true;
	}
	return false;
}

void FPS4FileRegistry::PlatformCloseFile(FRegisteredFileHandle* Handle)
{
	FPS4FileHandle* PS4Handle = (FPS4FileHandle*)Handle;
	sceFiosFHCloseSync(nullptr, PS4Handle->FileHandle);
}


/* FPS4PlatformFile structors
 *****************************************************************************/

FPS4PlatformFile::FPS4PlatformFile()
{
	// initialize the FIOS subsystem
	SceFiosParams Params = SCE_FIOS_PARAMS_INITIALIZER;

	Params.opStorage.pPtr = OpStorage;
	Params.opStorage.length = sizeof(OpStorage);
	Params.chunkStorage.pPtr = ChunkStorage;
	Params.chunkStorage.length = sizeof(ChunkStorage);
	Params.fhStorage.pPtr = FhStorage;
	Params.fhStorage.length = sizeof(FhStorage);
	Params.dhStorage.pPtr = DhsStorage;
	Params.dhStorage.length = sizeof(DhsStorage);
	Params.pVprintf = vprintf;
	Params.pMemcpy = memcpy;
	Params.pathMax = 256;

	sceFiosInitialize(&Params);	

	extern FString GFileRootDirectory;
	extern FString GSandboxName;

	// In blueprint-only projects, the game name is initialized after GSandbox,
	// when the project file name is parsed from the command line.
	// Check again if we don't have a sandbox name by now.
	if (GSandboxName.IsEmpty())
	{
		GSandboxName = FApp::GetProjectName();
		GSandboxName = GSandboxName.ToLower();
	}

	GPS4SandboxedFileRootDirectory = GFileRootDirectory;
	
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	//only write meta files when we're running under /data.  Under /app0 we are either an installed pkg, or running from a stageddirectory.  In either case
	// we should already have all the metadata we require from other sources.
	if (GFileRootDirectory != TEXT("/app0/"))
	{
		bWriteMetaDataFiles = true;
	}
#endif

	//don't break anyone with custom workflows based on /hostapp which we do not have an official use for yet.
	if (GFileRootDirectory.StartsWith(TEXT("/hostapp"), ESearchCase::CaseSensitive))
	{		
		bAllowPS4FileShortening = false;
	}

	// if the file root is /data/ they we're running CookOntheFly or Deployed.
	// either way we want file read/write to go through the sandboxed directories
	// to keep different projects from polluting each other on O:
	if (GFileRootDirectory == TEXT("/data/"))
	{		
		GPS4SandboxedFileRootDirectory.Append(GSandboxName);
		GPS4SandboxedFileRootDirectory.Append("/");
	}

	SandboxedTransformedFileRoot = GPS4SandboxedFileRootDirectory / TransformedFileRoot;	

#if PS4_FORCE_LOGS_SCREENSHOTS_TO_DEVICE
	GPS4DebugDataRootDirectory = "/data/";
	GPS4DebugDataRootDirectory.Append(GSandboxName);
	GPS4DebugDataRootDirectory.Append("/");

	// cache these as they are too expensive to use during path normalization.
	PS4CachedLogDirectory = StripRelativePath(FPaths::ProjectLogDir().ToLower());
	PS4CachedScreenShotDirectory = StripRelativePath(FPaths::ScreenShotDir().ToLower());
	PS4CachedProfilingDirectory = StripRelativePath(FPaths::ProfilingDir().ToLower());

	// make sure these subtrees are created ahead since CreateDirectoryTree may not detect subtrees as part of a path
	// that must go to /data/.  Don't use the standard function as it would strip any manual rooting to /data/
	sceFiosDirectoryCreateSync(nullptr, TCHAR_TO_UTF8(*GPS4DebugDataRootDirectory));
	CreateDevDataDirectoryTree(PS4CachedLogDirectory);
	CreateDevDataDirectoryTree(PS4CachedScreenShotDirectory);
	CreateDevDataDirectoryTree(PS4CachedProfilingDirectory);
#endif	

	verify( sceSysmoduleLoadModule( SCE_SYSMODULE_APP_CONTENT ) == SCE_OK );

	// initialize the app content module
	SceAppContentInitParam InitParam;
	SceAppContentBootParam BootParam;
	FMemory::Memzero( InitParam );
	FMemory::Memzero( BootParam );

	verify( sceAppContentInitialize( &InitParam, &BootParam ) == SCE_OK );

	// Mount temporary data directory
	if( sceAppContentTemporaryDataMount2( SCE_APP_CONTENT_TEMPORARY_DATA_OPTION_NONE, &GPS4Temp0MountPoint ) == SCE_OK )
	{
		GPS4TempDirectory = GPS4Temp0MountPoint.data;
	}

	InitializeDeepFileList();
}


FPS4PlatformFile::~FPS4PlatformFile()
{
	sceFiosTerminate();
}

const FString& FPS4PlatformFile::GetTempDirectory()
{
	return GPS4TempDirectory;
}

/* IPlatformFile overrides
 *****************************************************************************/

bool FPS4PlatformFile::CreateDirectory( const TCHAR* Directory )
{
	int Result = sceFiosDirectoryCreateSync(nullptr, ToPS4Directoryname(Directory));

	return (Result == SCE_FIOS_OK /*|| SCE_KERNEL_ERROR_EEXIST*/);
}

bool FPS4PlatformFile::CreateDirectoryTree( const TCHAR* Directory )
{
	FString DirectoryToCreate = Directory;	
	DirectoryToCreate = NormalizeDirectory( *DirectoryToCreate, false );
	return IPlatformFile::CreateDirectoryTree( *DirectoryToCreate );
}


bool FPS4PlatformFile::DeleteDirectory( const TCHAR* Directory )
{
	int Result = sceFiosDirectoryDeleteSync(nullptr, ToPS4Directoryname(Directory));

	return (Result == SCE_FIOS_OK);
}


bool FPS4PlatformFile::DeleteFile( const TCHAR* Filename )
{
	int Result = sceFiosFileDeleteSync(nullptr, ToPS4Filename(Filename));

	return (Result == SCE_FIOS_OK);
}


bool FPS4PlatformFile::DirectoryExists( const TCHAR* Directory )
{
	if ((Directory == nullptr) || (Directory[0] == 0))
	{
		return true;
	}
	
	bool bExists = sceFiosDirectoryExistsSync(nullptr, ToPS4Directoryname(Directory));	
	return bExists;
}


bool FPS4PlatformFile::FileExists( const TCHAR* Filename )
{
	bool bExists = sceFiosFileExistsSync(nullptr, ToPS4Filename(Filename));	
	return bExists;
}


int64 FPS4PlatformFile::FileSize( const TCHAR* Filename )
{
	int64 Size = sceFiosFileGetSizeSync(nullptr, ToPS4Filename(Filename));
	// return -1 for missing file or other errors
	if (Size < 0)
	{
		return -1;
	}
	return Size;
}


FDateTime FPS4PlatformFile::GetAccessTimeStamp( const TCHAR* Filename )
{
	SceFiosStat Stat;

	if (sceFiosStatSync(nullptr, ToPS4Filename(Filename), &Stat) == SCE_FIOS_OK)
	{
		return PS4FiosDateToUEDateTime(Stat.accessDate);
	}

	// min value on failure
	return FDateTime::MinValue();
}

FFileStatData FPS4PlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
	SceFiosStat Stat;

	if (sceFiosStatSync(nullptr, ToPS4Filename(FilenameOrDirectory), &Stat) == SCE_FIOS_OK)
	{
		return PS4FiosStatToUEFileData(Stat);
	}

	return FFileStatData();
}

FString FPS4PlatformFile::GetFilenameOnDisk(const TCHAR* Filename)
{
	return Filename;
}

FDateTime FPS4PlatformFile::GetTimeStamp( const TCHAR* Filename )
{
	SceFiosStat Stat;

	if (sceFiosStatSync(nullptr, ToPS4Filename(Filename), &Stat) == SCE_FIOS_OK)
	{
		return PS4FiosDateToUEDateTime(Stat.modificationDate);
	}

	// min value on failure
	return FDateTime::MinValue();
}


bool FPS4PlatformFile::Initialize( IPlatformFile* Inner, const TCHAR* CommandLineParam )
{
	// Physical platform file should never wrap anything.
	check(Inner == nullptr);	
	return true;
}


bool FPS4PlatformFile::IsReadOnly( const TCHAR* Filename )
{
	SceFiosStat Stat;

	if (sceFiosStatSync(nullptr, ToPS4Filename(Filename), &Stat) == SCE_FIOS_OK)
	{
		return ((Stat.statFlags & SCE_FIOS_STATUS_WRITABLE) == 0);
	}

	return false;
}


bool FPS4PlatformFile::MoveFile( const TCHAR* To, const TCHAR* From )
{
	return (sceFiosRenameSync(nullptr, ToPS4Filename(From), ToPS4Filename(To)) == SCE_FIOS_OK);
}


IFileHandle* FPS4PlatformFile::OpenRead( const TCHAR* Filename, bool bAllowWrite )
{
	// let the file registry manage read files
	return GFileRegistry.InitialOpenFile(*NormalizeFileName(Filename));
}


IFileHandle* FPS4PlatformFile::OpenWrite( const TCHAR* Filename, bool bAppend , bool bAllowRead )
{
	SceFiosFH FileHandle;
	SceFiosOpenParams Params;

	// always create if non-existent
	Params.openFlags = SCE_FIOS_O_CREAT;

	// read/write if we want to read
	if (bAllowRead)
	{
		Params.openFlags |= SCE_FIOS_O_RDWR;
	}
	else
	{
		Params.openFlags |= SCE_FIOS_O_WRONLY;
	}

	if (!bAppend)
	{
		Params.openFlags |= SCE_FIOS_O_TRUNC;
	}

	// open the file
	int Result = sceFiosFHOpenSync(nullptr, &FileHandle, ToPS4Filename(Filename), &Params);

	if (Result == SCE_FIOS_OK)
	{
		if (bAppend)
		{
			// Instead of using SCE_FIOS_O_APPEND we just seek to the end
			// This allows writing within an existing file instead of only at the end
			sceFiosFHSeek(FileHandle, 0, SCE_FIOS_SEEK_END);
		}

		return new FPS4FileHandle(FileHandle, *NormalizeFileName(Filename, false), true);
	}
	else
	{
		// temporarily remove to avoid infinite recursion as pkg builds fail to open the log file for write and then throw this warning.
		//UE_LOG(LogPS4, Warning, TEXT("Failed to open file for write: %s, error: 0x%x"), *NormalizeDirectory(Filename), Result);
	}

	return nullptr;
}


// @todo PS4: convert to FIOS2
bool FPS4PlatformFile::SetReadOnly( const TCHAR* Filename, bool bNewReadOnlyValue )
{
	SceKernelStat Stat;

	if (sceKernelStat(ToPS4Filename(Filename), &Stat) == SCE_OK)
	{
		if (bNewReadOnlyValue)
		{
			Stat.st_mode &= ~SCE_KERNEL_S_IWUSR;
		}
		else
		{
			Stat.st_mode |= SCE_KERNEL_S_IWUSR;
		}

		// update the mode, and return success
		return (sceKernelChmod(ToPS4Filename(Filename), Stat.st_mode) == SCE_OK);
	}

	return false;
}


// @todo PS4: convert to FIOS2
void FPS4PlatformFile::SetTimeStamp( const TCHAR* Filename, FDateTime DateTime )
{
	SceKernelStat Stat;

	if (sceKernelStat(ToPS4Filename(Filename), &Stat) == SCE_OK)
	{
		// change the modification time only
		SceKernelTimeval Times[2];
		Times[0].tv_sec = Stat.st_atime;
		Times[1].tv_sec = (DateTime - PS4Epoch).GetTotalSeconds();

		// ignore microseconds for files
		Times[0].tv_usec = Times[1].tv_usec = 0;

		// update the time
		sceKernelUtimes(ToPS4Filename(Filename), Times);
	}
}


FString FPS4PlatformFile::ConvertToAbsolutePathForExternalAppForRead( const TCHAR* Filename )
{
	return ToPS4Filename(Filename);
}


FString FPS4PlatformFile::ConvertToAbsolutePathForExternalAppForWrite( const TCHAR* Filename )
{
	return ToPS4Filename(Filename);
}


/* FPS4PlatformFile implementation
 *****************************************************************************/

FString PS4PathSeparator = TEXT("/");
FString PS4ValidWriteDir = TEXT("saved");
FString FPS4PlatformFile::NormalizeFileName(const TCHAR* FileName, bool bDirectory, bool bShortenForDeepFiles)
{
	FString Result(FileName);
	Result.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

	//fileserving and staging now send files as lowercase.  Must lowercase to match case-sensitive PS4 file system and to maintain hashing equivalance with staging.
	Result = Result.ToLower();	

	//if our path supports shortening we need to strip everything from the front, then shorten, then tack on the proper filesystem root.
	//otherwise we can just return it as-is from here.  For now this is just /download0 stuff as it's assumed to come in well-formed.
	bool bSupportsShortening = !Result.StartsWith(GPS4PersistentDownloadDirectory, ESearchCase::CaseSensitive);
	// temporary directory (/temp0) filenames are used as supplied
	bSupportsShortening = bSupportsShortening && !Result.StartsWith( GPS4TempDirectory, ESearchCase::CaseSensitive );

	if (bSupportsShortening)
	{
		// chop off all the ../'s (probably 3 of them) along with leading and trailing /'s and filesystem roots.
		Result = StripRelativePath(Result);

		bool bForceDataDir = false;
#if PS4_FORCE_LOGS_SCREENSHOTS_TO_DEVICE
		{
			// only test for log / screenshot dirs rather than all /saved paths because the PAK system expects /saved/paks to be a legitimate
			// pak file location.
			bForceDataDir = Result.StartsWith(PS4CachedLogDirectory, ESearchCase::CaseSensitive) || Result.StartsWith(PS4CachedScreenShotDirectory, ESearchCase::CaseSensitive) || Result.StartsWith(PS4CachedProfilingDirectory, ESearchCase::CaseSensitive);
		}
#endif


		// we even do this on /app0 file systems because we could be running from a shortened staging directory.  A final game should be using
		// -pak which won't go through the platform file system code.  If the final game didn't use -pak then we would need this anyway.
		// we only allow skipping shortening so when cook on the fly creates directory trees for files, it creates the WHOLE tree so recursive search will work.
		if (bAllowPS4FileShortening && bShortenForDeepFiles)
		{
			Result = ConditionallyShortenFilePath(*Result, bDirectory);
		}

		if (bForceDataDir)
		{
			//probably opening a log or statfile for write.  Make sure the full path exists so we can create the file.
			CreateDevDataDirectoryTree(bDirectory ? Result : FPaths::GetPath(Result));
			return GPS4DebugDataRootDirectory + Result;
		}
		else
		{
			return GPS4SandboxedFileRootDirectory + Result;
		}
	}
	else
	{
		return Result;
	}
}


FString FPS4PlatformFile::NormalizeDirectory(const TCHAR* Directory, bool bShortenForDeepFiles)
{
	FString Result = NormalizeFileName(Directory, true, bShortenForDeepFiles);
	return Result;
}


// @todo PS4: convert to FIOS2
bool FPS4PlatformFile::IterateDirectory( const TCHAR* Directory, FDirectoryVisitor& Visitor )
{
	auto InternalVisitor = [&](const FString& InPS4Path, const FString& InOrigPath, SceKernelDirent* InEntry) -> bool
	{
		return Visitor.Visit(*InOrigPath, InEntry->d_type == SCE_KERNEL_DT_DIR);
	};

	auto InternalVisitorDeepSubDir = [&](const FString& InOrigPath) -> bool
	{
		return Visitor.Visit(*InOrigPath, true);
	};

	return IterateDirectoryCommon(Directory, InternalVisitor, InternalVisitorDeepSubDir);
}

bool FPS4PlatformFile::IterateDirectoryStat( const TCHAR* Directory, FDirectoryStatVisitor& Visitor )
{
	auto InternalVisitor = [&](const FString& InPS4Path, const FString& InOrigPath, SceKernelDirent* InEntry) -> bool
	{
		SceFiosStat Stat;

		//always get the stat data using the actual PS4 filesystem path
		if (sceFiosStatSync(nullptr, TCHAR_TO_UTF8(*InPS4Path), &Stat) == SCE_FIOS_OK)
		{
			return Visitor.Visit(*InOrigPath, PS4FiosStatToUEFileData(Stat));
		}

		return true;
	};

	auto InternalVisitorDeepSubDir = [&](const FString& InOrigPath) -> bool
	{
		// We don't know much about the real directory on disk, so just report that we're a directory
		return Visitor.Visit(
			*InOrigPath, 
			FFileStatData(
				FDateTime::MinValue(),	// CreationTime
				FDateTime::MinValue(),	// AccessTime
				FDateTime::MinValue(),	// ModificationTime
				-1,						// FileSize
				true,					// bIsDirectory
				false					// bIsReadOnly
				)
			);
	};

	return IterateDirectoryCommon(Directory, InternalVisitor, InternalVisitorDeepSubDir);
}

bool FPS4PlatformFile::IterateDirectoryCommon( const TCHAR* Directory, const TFunctionRef<bool(const FString&, const FString&, struct dirent*)>& Visitor, const TFunctionRef<bool(const FString&)>& VisitorDeepSubDir )
{
	bool Result = false;	

	FScopeLock MapLock(&CompressPathMapMutex);

	FString RootDirString(Directory);

	//must lowercase so visitor result paths will match the paths that come from the fileserver.  Otherwise fileserver file matching fails
	//and we have to recache everything each run.
	RootDirString = RootDirString.ToLower();

	// open the directory	
	const char* TransformedDirectoryName = ToPS4Directoryname(Directory);
	int32 Handle = sceKernelOpen(TransformedDirectoryName, SCE_KERNEL_O_DIRECTORY | SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_IRU);

	if (Handle > 0)
	{
		Result = true;

		FString DeepDirHash;
		bool bDeepDir = RootDirString.Find(*SandboxedTransformedFileRoot) == 0;
		if (bDeepDir)
		{
			DeepDirHash = FPaths::GetCleanFilename(RootDirString);
			check(DeepDirHash.Len() > 0);
		}
		

		// get the block size of the directory
		SceKernelStat Stat;
		sceKernelFstat(Handle, &Stat);

		// make some room to gather directory entries (must be at least block size)
		int32 BlockSize = Stat.st_blksize;
		char* DirectoryEntryBlock = (char*)FMemory::Malloc(BlockSize);
		int32 DirentSize;

		// get a chunk of directory entries (returns 0 when all entries have been returned)
		while ((DirentSize = sceKernelGetdents(Handle, DirectoryEntryBlock, BlockSize)) > 0)
		{
			// walk over the dynamically sized directory entries until we have hit all the entries returned
			int32 BlockOffset = 0;

			while (BlockOffset < DirentSize)
			{
				// get the next entry
				SceKernelDirent* Entry = (SceKernelDirent*)(DirectoryEntryBlock + BlockOffset);

				if (Entry->d_reclen == 0)
				{
					break;
				}

				// checking with sony about this, but some of these 'deleted' files are showing up in certain directories.
				// we can't break out of the loop though or we will miss legitimate files.
				if (Entry->d_fileno != 0)
				{
					// process it
					if (FCStringAnsi::Strcmp(Entry->d_name, ".") != 0 && FCStringAnsi::Strcmp(Entry->d_name, "..") != 0)
					{
						// don't let platform independent code know about the deepfiles metafiles.
						const FString DirName = RootDirString / UTF8_TO_TCHAR(Entry->d_name);
						if (DirName.Find(TransformedMetaExt) == -1)
						{
							const bool bIsDir = Entry->d_type == SCE_KERNEL_DT_DIR;

							FString OrigFile = DirName;

							//if we're in a deep file directory, we actually want to return the 'original' path
							//so that external code can reason about the path properly.
							if (!bIsDir && DirName.Find(*SandboxedTransformedFileRoot) == 0)
							{
								const FString* OrigPath = CompressedPathMap.Find(DeepDirHash);
								if ( OrigPath )
								{
									//maintain proper incoming relative pathing (potentially).  The metadata file has a normalized path, not necessarily the incoming relative path.
									const FString OrigSubDir = FPaths::GetCleanFilename(*OrigPath);
									OrigFile = RootDirString / OrigSubDir / UTF8_TO_TCHAR(Entry->d_name);
								}
							}

							Result = Visitor(DirName, OrigFile, Entry);
						}
					}
				}

				// move to next record
				BlockOffset += Entry->d_reclen;
			}
		}

		// free temp storage
		FMemory::Free(DirectoryEntryBlock);

		// close the directory
		sceKernelClose(Handle);
	}

	// Iterate any matching deepfile folders
	TSet<FString>* DeepSubDirs = CompressedSubDirMap.Find(StripRelativePath(RootDirString).ToLower());
	if (DeepSubDirs)
	{
		for (auto Iter = DeepSubDirs->CreateIterator(); Iter; ++Iter)
		{
			//this is the stored path that was hashed. lowercased, with relative pathing stripped.
			//we want to maintain the form of the incoming path (possibly with relative casing) so just
			//tack on the new part of the path to the incoming one.
			const FString& OrigAdjustedPath = *Iter;
			Result = VisitorDeepSubDir(*(RootDirString / FPaths::GetCleanFilename(OrigAdjustedPath)));
		}
	}


	return Result;
}

FString FPS4PlatformFile::StripRelativePath(const FString& Path)
{
	FString RelativePath(Path);
	FPaths::NormalizeFilename(RelativePath);

	int32 DotDotSlashIdx = RelativePath.Find(TEXT("../"), ESearchCase::CaseSensitive);
	while (DotDotSlashIdx != INDEX_NONE)
	{
		RelativePath = RelativePath.Right(RelativePath.Len() - (DotDotSlashIdx + 3));
		DotDotSlashIdx = RelativePath.Find(TEXT("../"), ESearchCase::CaseSensitive);
	}

	if (RelativePath.StartsWith(GPS4SandboxedFileRootDirectory, ESearchCase::CaseSensitive))
	{
		RelativePath = RelativePath.Right(RelativePath.Len() - GPS4SandboxedFileRootDirectory.Len());
	}

	static FString App0Str(TEXT("/app0"));
	if (RelativePath.StartsWith(App0Str, ESearchCase::CaseSensitive))
	{
		RelativePath = RelativePath.Right(RelativePath.Len() - 5);		
	}

	static FString DataDirStr(TEXT("/data"));	
	if (RelativePath.StartsWith(DataDirStr, ESearchCase::CaseSensitive))
	{
		RelativePath = RelativePath.Right(RelativePath.Len() - 5);		
	}
	static FString Slash(TEXT("/"));
	if (RelativePath.StartsWith(Slash, ESearchCase::CaseSensitive))
	{
		RelativePath = RelativePath.Right(RelativePath.Len() - 1);
	}

	if (RelativePath.EndsWith(Slash, ESearchCase::CaseSensitive))
	{
		RelativePath = RelativePath.LeftChop(1);
	}

	return RelativePath;
}

// this MUST exactly match the remapper in PS4Platform.Automation.cs
FString FPS4PlatformFile::HashDirectoryPath(const FString& Path)
{
	FString PathToHash = Path.ToLower();	

	/** CRC 32 polynomial */
	//enum { Crc32Poly = 0x04c11db7 };
	static uint32 CrcTable[] =
	{
		0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005, 0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
		0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75, 0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
		0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5, 0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
		0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95, 0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
		0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072, 0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
		0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02, 0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
		0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692, 0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
		0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2, 0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
		0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB, 0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
		0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B, 0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
		0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B, 0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
		0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B, 0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
		0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C, 0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
		0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC, 0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
		0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C, 0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
		0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C, 0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
	};
	
	uint32 Hash = 0;
	for (int i = 0; i < PathToHash.Len(); ++i)
	{
		TCHAR Ch = PathToHash[i];
		uint16 B = Ch;
		Hash = ((Hash >> 8) & 0x00FFFFFF) ^ CrcTable[(Hash ^ B) & 0x000000FF];
		B = (uint16)(Ch >> 8);
		Hash = ((Hash >> 8) & 0x00FFFFFF) ^ CrcTable[(Hash ^ B) & 0x000000FF];
	}
	return FString::Printf(TEXT("%x"), Hash);
}


FString FPS4PlatformFile::ConditionallyShortenFilePath( const TCHAR* FileName, bool bIsDirectory )
{	
	FString ReturnPath(FileName);
	if (ReturnPath.Contains("deepfiles", ESearchCase::CaseSensitive))
	{
		return ReturnPath;
	}

	// assume StripRelativePath has already been called and filename is lower-case.
	FString OrigPath(FileName);

	// Find the folder level this file exists in.
	int32 FolderLevel = bIsDirectory ? 1 : 0;
	{	
		
		int32 LastFoundFolderIndex = OrigPath.Find(TEXT("/"), ESearchCase::CaseSensitive);
		while (LastFoundFolderIndex != INDEX_NONE && (LastFoundFolderIndex + 1) != OrigPath.Len())
		{
			FolderLevel++;
			LastFoundFolderIndex = OrigPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, LastFoundFolderIndex + 1);
		}
	}


	// Store files which exceed the PS4 directory depth limit into a "DeepFiles" dir on the disk
	if( FolderLevel >= 6 )
	{
		FString HashDirectory = bIsDirectory ? OrigPath : FPaths::GetPath(OrigPath);
		FString HashString = HashDirectoryPath(HashDirectory);

		FString ShortenedFilePath(TransformedFileRoot);
		ShortenedFilePath /= HashString;

		// End of Hash generation

		// If we are not processing a directory, then append the filename once again.
		if( bIsDirectory == false )
		{
			 ShortenedFilePath /= FPaths::GetCleanFilename(OrigPath);
		}

		// Return our adapted path /deepfiles/HASH/
		ReturnPath = ShortenedFilePath;

		// check if this hash has already been added to the map.
		FScopeLock MapLock(&CompressPathMapMutex);
		FString* ReverseHash = CompressedPathMap.Find(HashString);
		if (ReverseHash == nullptr)
		{
			FString MetaDataDirectory = FPaths::Combine(*TransformedFileRoot, *HashString);
			FString MetaDataPath = FPaths::Combine(*TransformedFileRoot, *HashString, TransformedMetaDataName);
			if (FileExists(*MetaDataPath))
			{
				FString StoredOrigPath = ReadTransformedMetaData(*MetaDataPath);
				checkf(StoredOrigPath.Compare(HashDirectory) == 0, TEXT("Paths: %s and %s hash to the same value!"), *HashDirectory, **ReverseHash);
			}
			else
			{
				// don't create the directory because we might be getting here from a 'directory exists' call and we don't want them to always succeed.
				// also only write meta files when we're running under /data.  Under /app0 we are either an installed pkg, or running from a stageddirectory.  In either case
				// we should already have all the metadata we require from other sources.
				if (bWriteMetaDataFiles && (!bIsDirectory || (DirectoryExists(*MetaDataDirectory))))
				{					
					WriteTransformedMetaData(*MetaDataPath, HashDirectory);

					// only add to the map if we actually write the file, otherwise we can get into situations where we never write it, which causes us to delete files
					// unintentionally on boot when we try to clean out the deepfiles. e.g. DirectoryExists call gets made on a dir that doesn't exist, then the dir is created, then a file is created.
					// if we add to the map on the exists call, but don't write the meta, then we never will.
					CompressedPathMap.Add(HashString, HashDirectory);
				}
			}			
		}
		else
		{
			checkf(ReverseHash->Compare(HashDirectory) == 0, TEXT("Paths: %s and %s hash to the same value!"), *HashDirectory, **ReverseHash);
		}
	}
	return ReturnPath;
}


FString FPS4PlatformFile::ReadTransformedMetaData( const TCHAR* FileName )
{
	IFileHandle* MetaFile = OpenRead(FileName);
	checkf(MetaFile, TEXT("Couldn't open metafile: %s"), FileName);

	TCHAR OrigPath[PATH_MAX +1];
	FMemory::Memzero(OrigPath);
	MetaFile->Read((uint8*)OrigPath, sizeof(OrigPath));
	delete MetaFile;

	// metafiles written by the staging process may have the unicode ByteOrderMark.  We don't want to include this in the runtime data.
	if (OrigPath[0] == 0xFEFF)
	{
		return FString(&OrigPath[1]);
	}
	return FString(OrigPath);
}


void FPS4PlatformFile::WriteTransformedMetaData( const TCHAR* FileName, const FString& OriginalPath)
{
	check(OriginalPath.Len() <= PATH_MAX);
	TCHAR OrigPath[PATH_MAX+1];
	FMemory::Memzero(OrigPath);
	FMemory::Memcpy(OrigPath, *OriginalPath, sizeof(TCHAR) * OriginalPath.Len());

	CreateDirectoryTree(*FPaths::GetPath(FileName));
	IFileHandle* MetaFile = OpenWrite(FileName);
	checkf(MetaFile, TEXT("Couldn't open metafile for write: %s"), FileName);

	MetaFile->Write((uint8*)OrigPath, sizeof(OrigPath));
	delete MetaFile;
}


class FDeepFileVisitor
	: public IPlatformFile::FDirectoryVisitor
{
public:
	TArray<FString>& Result;

	FDeepFileVisitor( TArray<FString>& InResult)
		: Result( InResult )
	{
	}

	virtual bool Visit( const TCHAR* FilenameOrDirectory, bool bIsDirectory )
	{
		// deep file walker not expected to have to recurse.
		check(!bIsDirectory);
		if (!bIsDirectory)
		{
			Result.Add(FString(FilenameOrDirectory));
		}		
		return true;
	}
};

void GetParentPath(const FString& ChildPath, FString& OutParentPath)
{
	check(&ChildPath != &OutParentPath);
	OutParentPath = ChildPath.Left(ChildPath.Len()-1);
	int32 Offset = 0;
	if (OutParentPath.FindLastChar('/', Offset))
	{
		OutParentPath = OutParentPath.Left(Offset);		
	}
	else
	{
		OutParentPath = "";
	}
}

void FPS4PlatformFile::InitializeDeepFileList()
{
	check(CompressedPathMap.Num() == 0);
	FScopeLock MapLock(&CompressPathMapMutex);

	// list of directories to skip
	TArray<FString> DirectoriesToSkip;
	TArray<FString> DirectoriesToNotRecurse;

	// open the deep files directory
	int32 Handle = sceKernelOpen(TCHAR_TO_UTF8(*SandboxedTransformedFileRoot), SCE_KERNEL_O_DIRECTORY | SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_IRU);
	if (Handle > 0)
	{
		// get the block size of the directory
		SceKernelStat Stat;
		sceKernelFstat(Handle, &Stat);

		// make some room to gather directory entries (must be at least block size)
		int32 BlockSize = Stat.st_blksize;
		char* DirectoryEntryBlock = (char*)FMemory::Malloc(BlockSize);
		int32 DirentSize;

		TArray<FString> FilesToDelete;
		TArray<FString> DirectoriesToDelete;

		// get a chunk of directory entries (returns 0 when all entries have been returned)
		while ((DirentSize = sceKernelGetdents(Handle, DirectoryEntryBlock, BlockSize)) > 0)
		{
			// walk over the dynamically sized directory entries until we have hit all the entries returned
			int32 BlockOffset = 0;

			while (BlockOffset < DirentSize)
			{
				// get the next entry
				SceKernelDirent* Entry = (SceKernelDirent*)(DirectoryEntryBlock + BlockOffset);

				if (Entry->d_reclen == 0)
				{
					break;
				}

				if (Entry->d_fileno != 0)
				{				
					// process it
					if (FCStringAnsi::Strcmp(Entry->d_name, ".") != 0 && FCStringAnsi::Strcmp(Entry->d_name, "..") != 0)
					{
						FString CurDir(UTF8_TO_TCHAR(Entry->d_name));
						FString DirPath(SandboxedTransformedFileRoot / CurDir);
						FString MetaFilePath = DirPath / TransformedMetaDataName;
						if (FileExists(*MetaFilePath))
						{						
							FString OriginalPath = ReadTransformedMetaData(*MetaFilePath);
							FString HashString = HashDirectoryPath(OriginalPath);						
							checkf(CurDir.Compare(HashString) == 0, TEXT("MetaData path: %s doesn't hash to current directory: %s"), *OriginalPath, *CurDir);
							CompressedPathMap.Add(HashString, OriginalPath);

							FString ParentPath;
							FString ChildPath = OriginalPath;
							GetParentPath(ChildPath, ParentPath);

							// add the parent directories up to the mount point if the child directory doesn't exist
							while (ParentPath.Len() != 0)
							{
								TSet<FString>& SubDirSet = CompressedSubDirMap.FindOrAdd(ParentPath);
								SubDirSet.Add(ChildPath);
								ChildPath = ParentPath;
								GetParentPath(ChildPath, ParentPath);

								FString NormalizedChildPath = NormalizeFileName(*ChildPath, true, false);

								int32 ChildHandle = sceKernelOpen(TCHAR_TO_UTF8(*NormalizedChildPath), SCE_KERNEL_O_DIRECTORY | SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_IRU);
								if( ChildHandle > 0 )
								{
									// Child exists, don't add any more paths
									sceKernelClose( ChildHandle );
									break;
								}
							}
						}
						else
						{
							// if we couldn't find the meta file for this folder for whatever reason, wipe it to be safe.						
							FDeepFileVisitor Visitor(FilesToDelete);						
							IterateDirectory(*FPaths::GetPath(MetaFilePath), Visitor);

							// manually delete metafile because IterateDirectory doesn't expose those to code that platform independent stuff has access to.
							FilesToDelete.Add(MetaFilePath);
							DirectoriesToDelete.Add(DirPath);											
						}
					}
				}

				// move to next record
				BlockOffset += Entry->d_reclen;
			}
		}

		// free temp storage
		FMemory::Free(DirectoryEntryBlock);		

		// close the directory
		sceKernelClose(Handle);

		// if we have deepfiles on app0 then we're running from a staged dir or something and it would be really bad to delete these files.
		extern FString GFileRootDirectory;
		if (GFileRootDirectory != TEXT("/app0/"))
		{
			for (int i = 0; i < FilesToDelete.Num(); ++i)
			{
				DeleteFile(*FilesToDelete[i]);
			}

			for (int i = 0; i < DirectoriesToDelete.Num(); ++i)
			{
				DeleteDirectory(*DirectoriesToDelete[i]);
			}
		}
	}
}

void FPS4PlatformFile::CreateDevDataDirectoryTree(const FString& DirectoryPath)
{
	TArray<FString> DirectoryParts;	
	DirectoryPath.ParseIntoArray(DirectoryParts, TEXT("/"), true);

	FString Path = GPS4DebugDataRootDirectory;
	for (int32 i = 0; i < DirectoryParts.Num(); ++i)
	{
		Path /= DirectoryParts[i];
		int32 Ret = sceFiosDirectoryCreateSync(nullptr, TCHAR_TO_UTF8(*Path));
		if (Ret != SCE_OK && Ret != SCE_FIOS_ERROR_ALREADY_EXISTS)
		{
			// Low level logging here, otherwise we can get a stack overflow calling regular UE_LOG
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to create directory %x. error: 0x%x"), *DirectoryPath, Ret);
		}
	}
}

void FReadaheadCache::Aquire(FPS4FileHandle* InOwner)
{
	if (Owner != InOwner)
	{
		if (Owner)
		{
			READAHEAD_STAT(Stats.NumFileSwitches++);
			Release(Owner);
		}

		Owner = InOwner;
	}
}

void FReadaheadCache::Release(FPS4FileHandle* InOwner)
{
	if (Owner == InOwner)
	{
		for (int32 i = 0; i < MaxBuffers; ++i)
		{
			Buffers[i].Reset();
		}

		Owner = nullptr;
	}
}

bool FReadaheadCache::DoUnsuccessfulReadaheadCatchup(FReadaheadCacheBuffer& Buffer)
{
	READAHEAD_STAT(Stats.NumUnsuccessfulReadAheads++);

#if TRACK_READAHEAD_STATS
	double& Time = Stats.BlockTime;
	PS4FILE_SCOPE_SECONDS_COUNTER(Time);
#endif

	if (!Buffer.FinishRead())
	{
		// The read failed, so abandon caching
		return false;
	}

	return true;
}

int64 FReadaheadCache::Read(FPS4FileHandle* InHandle, uint8* InDestination, int64 InBytesToRead, int64 InReadPosition)
{
	int64 BytesRead = 0;

	Aquire(InHandle);

	SetReadPosition(InReadPosition);

	int32 BufferIndex = -1;

	// Go over the data we have cached and copy out to our destination buffer
	while (InBytesToRead && ((BufferIndex = FindExistingBuffer(InReadPosition)) != -1))
	{
		FReadaheadCacheBuffer& Buffer = Buffers[BufferIndex];

		check(Buffer.ContainsOffset(InReadPosition));
		if (Buffer.State == FReadaheadCacheBuffer::EState::Reading)
		{
			if (Buffer.IsFinished())
			{
				READAHEAD_STAT(Stats.NumSuccessfulReadAheads++);
			}
			else
			{
				if (!DoUnsuccessfulReadaheadCatchup(Buffer))
				{
					break;
				}
			}			
		}

		int64 BufferOffset = InReadPosition - Buffer.Offset;
		int64 NumBytes = FMath::Min(Buffer.GetNumValidBytes(InReadPosition), InBytesToRead);
		check(BufferOffset >= 0 && BufferOffset < FReadaheadCacheBuffer::BufferSize);
		{
#if TRACK_READAHEAD_STATS
			double& Time = Stats.MemCopyTime;
			PS4FILE_SCOPE_SECONDS_COUNTER(Time);
#endif
			FMemory::Memcpy(InDestination, Buffer.Data + BufferOffset, NumBytes);
		}
		InBytesToRead -= NumBytes;
		InReadPosition += NumBytes;
		InDestination += NumBytes;
		BytesRead += NumBytes;

		// Update read-ahead for our new position
		SetReadPosition(InReadPosition);
	}

	// Any remaining required data which we didn't have cached can just be read read as normal
	if (InBytesToRead > 0)
	{
#if TRACK_READAHEAD_STATS
		double& Time = Stats.UncachedReadTime;
		PS4FILE_SCOPE_SECONDS_COUNTER(Time);
#endif
		FScopedDiskUtilizationTracker Tracker(InBytesToRead, InReadPosition);
		BytesRead += sceFiosFHPreadSync(nullptr, Owner->FileHandle, InDestination, InBytesToRead, InReadPosition);
	}

	return BytesRead;
}

void FReadaheadCache::SetReadPosition(int64 InPosition)
{
#if TRACK_READAHEAD_STATS
	double& Time = Stats.SetReadPositionTime;
	PS4FILE_SCOPE_SECONDS_COUNTER(Time);
#endif

	// Search for a buffer that already contains our position
	int32 FirstBuffer = FindExistingBuffer(InPosition);
	if (FirstBuffer == -1)
	{
		FirstBuffer = 0;
	}

	int64 RemainingToCache = Owner->FileSize - InPosition;
	bool bFirst = true;

	for (int32 LogicalBufferIndex = 0; RemainingToCache > 0 && LogicalBufferIndex < MaxBuffers; ++LogicalBufferIndex)
	{
		int32 ActualBufferIndex = (FirstBuffer + LogicalBufferIndex) % MaxBuffers;
		FReadaheadCacheBuffer& Buffer = Buffers[ActualBufferIndex];

		if (Buffer.State != FReadaheadCacheBuffer::EState::Empty && ((bFirst && Buffer.ContainsOffset(InPosition)) || (!bFirst && Buffer.Offset == InPosition)))
		{
			//InPosition += (Buffer.Size - Buffer.Offset);
			int64 Bytes = Buffer.GetNumValidBytes(InPosition);
			RemainingToCache -= Bytes;
			InPosition += Bytes;
		}
		else
		{
			if (bFirst)
			{
				READAHEAD_STAT(Stats.NumCacheMisses++);
			}

			// Cancel any existing reads
			if (Buffer.State == FReadaheadCacheBuffer::EState::Reading)
			{
				//Buffer.FinishRead();
#if TRACK_READAHEAD_STATS
				double& CancelTime = Stats.CancelReadTime;
				PS4FILE_SCOPE_SECONDS_COUNTER(CancelTime);
#endif

				Buffer.CancelRead();
			}

			Buffer.Offset = InPosition;
			Buffer.Size = FMath::Min(FReadaheadCacheBuffer::BufferSize, RemainingToCache);
			RemainingToCache -= Buffer.Size;
			InPosition += Buffer.Size;
			Buffer.State = FReadaheadCacheBuffer::EState::Reading;
			Buffer.Op = sceFiosFHPread(nullptr, Owner->FileHandle, Buffer.Data, Buffer.Size, Buffer.Offset);
		}

		bFirst = false;
	}
}

int32 FReadaheadCache::FindExistingBuffer(uint64 InPosition)
{
	for (int32 i = 0; i < MaxBuffers; ++i)
	{
		FReadaheadCacheBuffer& Buffer = Buffers[i];
		if (Buffer.ContainsOffset(InPosition))
		{
			return i;
		}
	}

	return -1;
}
