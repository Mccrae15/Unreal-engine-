// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformFile.h"

class FString;
template <typename FuncType> class TFunctionRef;

/**
 * Implements the PS4 file system.
 */
class CORE_API FPS4PlatformFile
	: public IPlatformFile
{
public:

	/** Default constructor. */
	FPS4PlatformFile();

	/** Destructor. */
	~FPS4PlatformFile();

public:
	
	// IPlatformFile interface

	virtual bool CreateDirectory( const TCHAR* Directory ) override;
	virtual bool CreateDirectoryTree( const TCHAR* Directory ) override;
	virtual bool DeleteDirectory( const TCHAR* Directory ) override;
	virtual bool DeleteFile( const TCHAR* Filename ) override;
	virtual bool DirectoryExists( const TCHAR* Directory ) override;
	virtual bool FileExists( const TCHAR* Filename ) override;
	virtual int64 FileSize( const TCHAR* Filename ) override;
	virtual FDateTime GetAccessTimeStamp( const TCHAR* Filename ) override;

	virtual FFileStatData GetStatData( const TCHAR* FilenameOrDirectory ) override;

	virtual IPlatformFile* GetLowerLevel() override
	{
		return nullptr;
	}
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override
	{
		check(false); // can't override wrapped platform file for physical platform file
	}
	virtual const TCHAR* GetName() const override
	{
		return IPlatformFile::GetPhysicalTypeName();
	}

	virtual FDateTime GetTimeStamp( const TCHAR* Filename ) override;

	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override
	{
		return true;
	}	

	virtual bool Initialize( IPlatformFile* Inner, const TCHAR* CommandLineParam ) override;
	virtual bool IsReadOnly( const TCHAR* Filename ) override;
	virtual bool MoveFile( const TCHAR* To, const TCHAR* From ) override;
	virtual IFileHandle* OpenRead( const TCHAR* Filename, bool bAllowWrite = false ) override;
	virtual IFileHandle* OpenWrite( const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false ) override;
	virtual bool SetReadOnly( const TCHAR* Filename, bool bNewReadOnlyValue ) override;
	virtual void SetTimeStamp( const TCHAR* Filename, FDateTime DateTime ) override;
	virtual FString ConvertToAbsolutePathForExternalAppForRead( const TCHAR* Filename ) override;
	virtual FString ConvertToAbsolutePathForExternalAppForWrite( const TCHAR* Filename ) override;
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override;

	virtual bool IterateDirectory( const TCHAR* Directory, FDirectoryVisitor& Visitor ) override;
	virtual bool IterateDirectoryStat( const TCHAR* Directory, FDirectoryStatVisitor& Visitor ) override;

	/**
	 * Normalizes the specified file name.
	 *
	 * @param FileName The file name to normalize.
	 * @return The normalized file name.
	 * @see NormalizeDirectory
	 */
	FString NormalizeFileName(const TCHAR* FileName, bool bDirectory = false, bool bShortenForDeepFiles = true);

	/**
	 * Get the temporary directory path. Should be /temp0
	 *
	 * @return The temporary directory path.
	 */
	static const FString& GetTempDirectory();

protected:

	bool IterateDirectoryCommon( const TCHAR* Directory, const TFunctionRef<bool(const FString&, const FString&, struct dirent*)>& Visitor, const TFunctionRef<bool(const FString&)>& VisitorDeepSubDir );


	/**
	 * Normalizes the specified directory path.
	 *
	 * @param Directory The directory path to normalize.
	 * @return The normalized directory path.
	 * @see NormalizeFileName
	 */
	FString NormalizeDirectory(const TCHAR* Directory, bool bShortenForDeepFiles = true);

	/**
	 * Strips any '../' portions at the front of the given path.
	 * Also strips any filesystem identifies like /data or /app0, leading and trailing /'s
	 * Important for proper hashing for deepfile support.
	 *
	 * @param Path The path to strip.
	 * @return The stripped path.
	 */
	FString StripRelativePath(const FString& Path);

	/**
	 * Creates a hash for the given path.
	 *
	 * @param DirectoryPath The path to create the hash for.
	 * @return The created hash.
	 */
	FString HashDirectoryPath(const FString& DirectoryPath);

	/**
	 * Check if the file path exceeds the limit imposed by the ps4sdk, if so, shorten it.
	 *
	 * @param FileName The path we are considering shortening.  FileName MUST be lower case and have StripRelativePath called on it.
	 * @param bIsDirectory Flag to determine if the path we are checking is a directory
	 * @return The FileName we send in if it is acceptable, or a formatted filename if it exceeds the maximum level of deepness (8 levels)
	 */
	FString ConditionallyShortenFilePath( const TCHAR* FileName, bool bIsDirectory = false );

	/**
	 * Read the original path stored in a shortened directory's metadata file.
	 *
	 * @param FileName
	 * @return
	 */
	FString ReadTransformedMetaData( const TCHAR* FileName );

	/**
	 * Write the original path to the transformed location's metadata file.
	 *
	 * @param FileName
	 * @param OriginalPath
	 */
	void WriteTransformedMetaData( const TCHAR* FileName, const FString& OriginalPath);

	/** Reads all cached 'deep' files into an in-memory hash for faster search during directory iteration */
	void InitializeDeepFileList();

	void CreateDevDataDirectoryTree(const FString& DirectoryPath);
};
