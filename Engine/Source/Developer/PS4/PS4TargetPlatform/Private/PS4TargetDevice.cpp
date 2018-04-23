// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4TargetDevice.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#pragma warning (disable:4564)
#pragma warning (disable:4400)

DEFINE_LOG_CATEGORY_STATIC(LogPS4TargetDevice, Log, All);

/* FPS4TargetDevice structors
 *****************************************************************************/
FPS4TargetDevice::FPS4TargetDevice( const ITargetPlatform& InTargetPlatform, const FString& InName, const FString& InDeviceInfo )
: CachedHostName( InName )
, TargetPlatform( InTargetPlatform )
{
	FString DeviceName;
	if( !FParse::Value( *InDeviceInfo, TEXT( "TargetAdded=" ), DeviceName ) )
	{
		// InDeviceInfo only contains the device name (isn't coming from PS4DevkitTool "TargetAdded")
		// Request device details from external app
		GetDeviceInfoAndUpdate();
	}
	else
	{
		// InDeviceInfo contains all device info so extract it
		UpdateDeviceInfoCache( InDeviceInfo );
	}
}

/* ITargetDevice interface
 *****************************************************************************/

bool FPS4TargetDevice::Connect()
{
	if( CachedPowerStatus != EPowerStatus::PowerStatusOn )
	{
		return false;
	}

	if( CachedConnectionState == EConnectionState::ConnectionInUse )
	{
		// Force disconnect anyone else that is currently connected to the device
		ExecOrbisCommand( "force-disconnect" );
	}

	ExecOrbisCommand("Connect");
	GetDeviceInfoAndUpdate();
	
	return true;
}


bool FPS4TargetDevice::Deploy( const FString& SourceFolder, FString& OutAppId )
{
	OutAppId = TEXT("");

	char DriveLetter[2];
	if( CachedDriveLetter.IsEmpty() )
	{
		return false;
	}
	DriveLetter[0] = CachedDriveLetter[0];
	DriveLetter[1] = '\0';

	FString DeploymentDir = (FString(ANSI_TO_TCHAR(DriveLetter)) + TEXT(":")) / CachedHostName / TEXT("data");

	// delete previous build
	IFileManager::Get().DeleteDirectory(*DeploymentDir, false, true);

	// copy files into device directory
	TArray<FString> FileNames;

	IFileManager::Get().FindFilesRecursive(FileNames, *SourceFolder, TEXT("*.*"), true, false);

	for (int32 FileIndex = 0; FileIndex < FileNames.Num(); ++FileIndex)
	{
		const FString& SourceFilePath = FileNames[FileIndex];
		FString DestFilePath = DeploymentDir + SourceFilePath.RightChop(SourceFolder.Len());

		IFileManager::Get().Copy(*DestFilePath, *SourceFilePath);
	}

	return true;
}


void FPS4TargetDevice::Disconnect( )
{
	ExecOrbisCommand("disconnect");
	CachedConnectionState = EConnectionState::ConnectionAvailable;
}


int32 FPS4TargetDevice::GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) 
{
	if (CachedConnectionState != EConnectionState::ConnectionConnected)
	{
		return 0;
	}

	void* WritePipe;
	void* ReadPipe;
	FPlatformProcess::CreatePipe( ReadPipe, WritePipe );
	FString Params = FString( TEXT("snapshot \"") ) + CachedHostName + TEXT("\"");
	FProcHandle RunningProc = FPlatformProcess::CreateProc( TEXT( "../DotNET/PS4/PS4DevKitUtil.exe" ), *Params, true, false, false, NULL, 0, NULL, WritePipe );
	FPlatformProcess::WaitForProc( RunningProc );
	int32 ReturnCode;
	FPlatformProcess::GetProcReturnCode( RunningProc, &ReturnCode );

	if( ReturnCode == 0 )
	{
		FString ProcessOutput = FPlatformProcess::ReadPipe( ReadPipe );
		if( ProcessOutput.Len() > 0 )
		{
			TArray<FString> OutputLines;
			ProcessOutput.ParseIntoArray( OutputLines, TEXT( "\n" ), false );

			int32 LineIndex = 0;
			while( LineIndex < OutputLines.Num() )
			{
				FString TempString;
				if( FParse::Value( *OutputLines[LineIndex], TEXT( "ProcessInfo" ), TempString ) )
				{
					LineIndex++;

					FTargetDeviceProcessInfo ProcessInfo;

					FString Pid;
					FString PPID;

					FParse::Value( *OutputLines[LineIndex++], TEXT( "PID=" ), Pid );
					FParse::Value( *OutputLines[LineIndex++], TEXT( "PPID=" ), PPID );
					FParse::Value( *OutputLines[LineIndex++], TEXT( "Name=" ), ProcessInfo.Name );
					FParse::Value( *OutputLines[LineIndex++], TEXT( "Username=" ), ProcessInfo.UserName );

					ProcessInfo.Id = FParse::HexNumber( *Pid );
					ProcessInfo.ParentId = FParse::HexNumber( *PPID );

					while( true )
					{
						// Read thread info
						if( FParse::Value( *OutputLines[LineIndex], TEXT( "ThreadInfo" ), TempString ) )
						{
							LineIndex++;

							FTargetDeviceThreadInfo ThreadInfo;
							FString State;
							FString WaitState;
							FParse::Value( *OutputLines[LineIndex++], TEXT( "Id=" ), ThreadInfo.Id );
							FParse::Value( *OutputLines[LineIndex++], TEXT( "Name=" ), ThreadInfo.Name );
							FParse::Value( *OutputLines[LineIndex++], TEXT( "StackSize=" ), ThreadInfo.StackSize );
							FParse::Value( *OutputLines[LineIndex++], TEXT( "ExitCode=" ), ThreadInfo.ExitCode );
							FParse::Value( *OutputLines[LineIndex++], TEXT( "State=" ), State );
							FParse::Value( *OutputLines[LineIndex++], TEXT( "WaitState=" ), WaitState );

							if( State == TEXT( "TDS_INACTIVE" ) )
							{
								ThreadInfo.State = ETargetDeviceThreadStates::Inactive;
							}
							else if( State == TEXT( "TDS_INHIBITED" ) )
							{
								ThreadInfo.State = ETargetDeviceThreadStates::Inhibited;
							}
							else if( State == TEXT( "TDS_CAN_RUN" ) )
							{
								ThreadInfo.State = ETargetDeviceThreadStates::CanRun;
							}
							else if( State == TEXT( "TDS_RUNQ" ) )
							{
								ThreadInfo.State = ETargetDeviceThreadStates::RunQueue;
							}
							else if( State == TEXT( "TDS_RUNNING" ) )
							{
								ThreadInfo.State = ETargetDeviceThreadStates::Running;
							}
							else
							{
								ThreadInfo.State = ETargetDeviceThreadStates::Unknown;
							}

							if( WaitState == TEXT( "TDI_SUSPENDED" ) )
							{
								ThreadInfo.WaitState = ETargetDeviceThreadWaitStates::Suspended;
							}
							else if( WaitState == TEXT( "TDI_SLEEPING" ) )
							{
								ThreadInfo.WaitState = ETargetDeviceThreadWaitStates::Sleeping;
							}
							else if( WaitState == TEXT( "TDI_SWAPPED" ) )
							{
								ThreadInfo.WaitState = ETargetDeviceThreadWaitStates::Swapped;
							}
							else if( WaitState == TEXT( "TDI_LOCK" ) )
							{
								ThreadInfo.WaitState = ETargetDeviceThreadWaitStates::Locked;
							}
							else if( WaitState == TEXT( "TDI_IWAIT" ) )
							{
								ThreadInfo.WaitState = ETargetDeviceThreadWaitStates::Waiting;
							}
							else
							{
								ThreadInfo.WaitState = ETargetDeviceThreadWaitStates::Unknown;
							}

							ProcessInfo.Threads.Add( ThreadInfo );
						}
						else
						{
							break;
						}
					}

					OutProcessInfos.Add( ProcessInfo );

				}
				else
				{
					LineIndex++;
				}
			}
		}
	}

	FPlatformProcess::CloseProc( RunningProc );
	FPlatformProcess::ClosePipe( ReadPipe, WritePipe );

	return OutProcessInfos.Num();
}


bool FPS4TargetDevice::Launch( const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId )
{
	return Run(AppId, Params, OutProcessId);
}


void FPS4TargetDevice::ExecOrbisCommand( FString Params )
{
	Params = Params + FString::Printf(TEXT(" \"%s\""), *CachedHostName);
	FProcHandle RunningProc = FPlatformProcess::CreateProc( TEXT( "orbis-ctrl" ), *Params, true, false, false, NULL, 0, NULL, NULL );
	FPlatformProcess::WaitForProc( RunningProc );
}


bool FPS4TargetDevice::PowerOff( bool Force )
{
	ExecOrbisCommand("off");
	GetDeviceInfoAndUpdate();

	return true;
}


bool FPS4TargetDevice::PowerOn( )
{
	ExecOrbisCommand("on");
	GetDeviceInfoAndUpdate();

	return true;
}


bool FPS4TargetDevice::Reboot( bool bReconnect )
{
	ExecOrbisCommand("reboot");
	GetDeviceInfoAndUpdate();

	return true;
}


FString FPS4TargetDevice::GetWorkingDir( FString FullPathStr, FString ParamsStr )
{
	FString WorkingDir = "";
	if( FullPathStr.Len() > 0 )
	{
		// find the sce_sys directory
		// if we're in a staged build, the elf's location is three directories down from where sce_sys is.
		// UE4/Project/Saved/StagedBuilds/PS4/Project/Binaries/PS4.  sce_sys should be at UE4/Project/Saved/StagedBuilds/PS4
		// or if we are in staged directory specified by Drive/PS4/Project/Binaries/PS4
		// also, a staged dir may have been packaged, which renames the self to eboot.bin and moves it to the root of the staged dir.
		if( FullPathStr.Find( "StagedBuilds" ) != INDEX_NONE || FullPathStr.Find( "PS4" ) < FullPathStr.Find( "PS4", ESearchCase::IgnoreCase, ESearchDir::FromEnd ) )
		{
			//strip the file name
			WorkingDir = FPaths::GetPath( FullPathStr );

			// if our exe is eboot.bin, then the packaging process already moved it up to the proper root working dir.
			FString ExecutableName = FPaths::GetCleanFilename( FullPathStr );
			if( ExecutableName != TEXT( "eboot.bin" ) )
			{
				//go up 3 dirs
				WorkingDir = FPaths::GetPath( FPaths::GetPath( FPaths::GetPath( WorkingDir ) ) );
			}

		}
		// if we're not staged, stupidly assume we're in a Project/Binaries/PS4 dir,
		// come up to the Project dir and go down to Build/PS4 where the sce_sys dir is.
		else
		{
			FString UE4Dir = FPaths::RootDir();

			//strip the file name
			WorkingDir = FPaths::GetPath( FullPathStr );

			WorkingDir = FPaths::GetPath( FPaths::GetPath( WorkingDir ) );
			WorkingDir = WorkingDir / TEXT("Build") / TEXT("PS4");
		}
	}

	return WorkingDir;
}


bool FPS4TargetDevice::Run( const FString& ExecutablePath, const FString& Params, uint32* OutProcessId )
{
	// Make sure the devkit is powered on
	if( CachedPowerStatus != EPowerStatus::PowerStatusOn )
	{
		PowerOn();
		if( CachedPowerStatus != EPowerStatus::PowerStatusOn )
		{
			// Not powered on, probably physically unreachable
			return false;
		}
	}
	
	Connect();
	if( CachedConnectionState != EConnectionState::ConnectionConnected )
	{
		return false;
	}

	FProcHandle RunningProc;
	FString CommandLine;
	FString Executable;
	if( FindExecutable( ExecutablePath, Executable ) )
	{
		FString FullPath = FPaths::ConvertRelativePathToFull( Executable );
		FString WorkingDir = GetWorkingDir( FullPath, Params );
		CommandLine = (TEXT("launch target=")) + CachedHostName + TEXT(" workingdirectory=\"") + WorkingDir + TEXT("\" elf=\"") + FullPath + TEXT("\" Args=\"") + Params + TEXT("\"");
	}
	else
	{
		FString FullPath = ExecutablePath;
		CommandLine = ( TEXT( "launch target=" ) ) + CachedHostName + TEXT( " device=raw " ) + TEXT( " elf=\"" ) + FullPath + TEXT( "\" Args=\"" ) + Params + TEXT( "\"" );
	}

	LogMessage( TEXT( "Launch params:" ) );
	LogMessage( *CommandLine );


	void* WritePipe;
	void* ReadPipe;
	FPlatformProcess::CreatePipe( ReadPipe, WritePipe );

	RunningProc = FPlatformProcess::CreateProc( TEXT( "../DotNET/PS4/PS4DevKitUtil.exe" ), *CommandLine, true, false, false, NULL, 0, NULL, WritePipe );

	// Handle any output coming back from the running process
	{
		FString Line;

		while( FPlatformProcess::IsProcRunning( RunningProc ) )
		{
			FString NewLine = FPlatformProcess::ReadPipe( ReadPipe );
			if( NewLine.Len() > 0 )
			{
				// process the string to break it up in to lines
				Line += NewLine;
				TArray<FString> StringArray;
				int32 NumLines = Line.ParseIntoArray( StringArray, TEXT( "\n" ), true );
				if( NumLines > 1 )
				{
					for( int32 Index = 0; Index < NumLines - 1; ++Index )
					{
						StringArray[Index].TrimEndInline();
						if( StringArray[Index].StartsWith( TEXT( "STDERR:" ) ) )
						{
							ErrorMessage( *( StringArray[Index].Mid( 7 ) ) );
						}
						else
						{
							LogMessage( *StringArray[Index] );
						}
					}
					Line = StringArray[NumLines - 1];
					if( NewLine.EndsWith( TEXT( "\n" ) ) )
					{
						Line += TEXT( "\n" );
					}
				}
			}
			FPlatformProcess::Sleep( 0.25 );
		}

		FString NewLine = FPlatformProcess::ReadPipe( ReadPipe );
		while( NewLine.Len() > 0 )
		{
			// process the string to break it up in to lines
			Line += NewLine;
			TArray<FString> StringArray;
			int32 NumLines = Line.ParseIntoArray( StringArray, TEXT( "\n" ), true );
			if( NumLines > 1 )
			{
				for( int32 Index = 0; Index < NumLines - 1; ++Index )
				{
					StringArray[Index].TrimEndInline();
					if( StringArray[Index].StartsWith( TEXT( "STDERR:" ) ) )
					{
						ErrorMessage( *( StringArray[Index].Mid( 7 ) ) );
					}
					else
					{
						LogMessage( *StringArray[Index] );
					}
				}
				Line = StringArray[NumLines - 1];
				if( NewLine.EndsWith( TEXT( "\n" ) ) )
				{
					Line += TEXT( "\n" );
				}
			}

			NewLine = FPlatformProcess::ReadPipe( ReadPipe );
		}
		if( Line.StartsWith( TEXT( "STDERR:" ) ) )
		{
			ErrorMessage( *( Line.Mid( 7 ) ) );
		}
		else
		{
			LogMessage( *Line );
		}
	}

	FPlatformProcess::CloseProc( RunningProc );
	FPlatformProcess::ClosePipe( ReadPipe, WritePipe );

	return true;
}


bool FPS4TargetDevice::FindExecutable( const FString& InExecutablePath, FString& InOutExecutable )
{
	//@todo UAT should just pass in the correct executable location.
	FString StagedFolder = FPaths::GetPath(InExecutablePath);
	StagedFolder = FPaths::Combine(*StagedFolder, TEXT(".."), TEXT(".."), TEXT(".."));
	FString StagedExecutable = StagedFolder / FPaths::GetCleanFilename(InExecutablePath);
	FString PackagedExecutable = StagedFolder / TEXT("eboot.bin");
	FString UE4CommandLineFilename;
	bool RetVal = false;

	if (FPaths::FileExists(InExecutablePath))
	{
		InOutExecutable = InExecutablePath;
		UE4CommandLineFilename = FPaths::GetPath(InExecutablePath) / TEXT("ue4commandline.txt");
		RetVal = true;
	}
	else if (FPaths::FileExists(StagedExecutable))
	{
		InOutExecutable = StagedExecutable;
		UE4CommandLineFilename = StagedFolder / TEXT("ue4commandline.txt");
		RetVal = true;
	}
	else if (FPaths::FileExists(PackagedExecutable))
	{
		InOutExecutable = PackagedExecutable;
		UE4CommandLineFilename = StagedFolder / TEXT("ue4commandline.txt");
		RetVal = true;
	}

	if (FPaths::FileExists(UE4CommandLineFilename))
	{
		IFileManager::Get().Delete(*UE4CommandLineFilename);
	}
	return RetVal;
}


bool FPS4TargetDevice::SupportsFeature( ETargetDeviceFeatures Feature ) const
{
	switch (Feature)
	{
	case ETargetDeviceFeatures::PowerOff:
		return true;

	case ETargetDeviceFeatures::PowerOn:
		return true;

	case ETargetDeviceFeatures::ProcessSnapshot:
		return true;

	case ETargetDeviceFeatures::Reboot:
		return true;
	}

	return false;
}


bool FPS4TargetDevice::SupportsSdkVersion( const FString& VersionString ) const
{
	// @todo filter SDK versions

	return true;
}


bool FPS4TargetDevice::TerminateProcess( const int64 ProcessId )
{
	FString Params = FString::Printf( TEXT( "pkill %lld" ), ProcessId );
	ExecOrbisCommand( Params );

	return true;
}


/* FPS4TargetDevice implementation
 *****************************************************************************/
void FPS4TargetDevice::GetDeviceInfoAndUpdate()
{
	// Defaults
	CachedDefault = false;
	CachedPowerStatus = PowerStatusUnknown;
	CachedConnectionState = ConnectionAvailable;
	CachedOSName = FString( "Unknown" );
	CachedDriveLetter.Empty();
	CachedName = CachedHostName;

	// Shell out to external tool to get detailed info and update the caches
	void* WritePipe;
	void* ReadPipe;
	FPlatformProcess::CreatePipe( ReadPipe, WritePipe );
	FString Params = FString( TEXT( "Detail \"" ) ) + CachedName + TEXT( "\"" );
	FProcHandle RunningProc = FPlatformProcess::CreateProc( TEXT( "../DotNET/PS4/PS4DevKitUtil.exe" ), *Params, true, false, false, NULL, 0, NULL, WritePipe );
	FPlatformProcess::WaitForProc( RunningProc );

	int32 ReturnCode;
	FPlatformProcess::GetProcReturnCode( RunningProc, &ReturnCode );

	if( ReturnCode == 0 )
	{
		FString ProcessOutput = FPlatformProcess::ReadPipe( ReadPipe );
		UpdateDeviceInfoCache( ProcessOutput );
	}

	FPlatformProcess::CloseProc( RunningProc );
	FPlatformProcess::ClosePipe( ReadPipe, WritePipe );
}


void FPS4TargetDevice::UpdateDeviceInfoCache( const FString& DeviceInfo )
{
	// Defaults
	CachedDefault = false;
	CachedPowerStatus = PowerStatusUnknown;
	CachedConnectionState = ConnectionAvailable;
	CachedOSName = FString( "Unknown" );
	CachedDriveLetter.Empty();
	CachedName = CachedHostName;
	
	if( DeviceInfo.Len() > 0 )
	{
		FParse::Value( *DeviceInfo, TEXT( "Name=" ), CachedName );
		if( CachedName.IsEmpty() )
		{
			CachedName = CachedHostName;

		}
		FParse::Bool( *DeviceInfo, TEXT( "Default=" ), CachedDefault );

		FString PowerStatus;
		if( FParse::Value( *DeviceInfo, TEXT( "PowerStatus=" ), PowerStatus ) )
		{
			if( PowerStatus == FString("POWER_STATUS_ON"))
			{
				CachedPowerStatus = PowerStatusOn;

				FString ConnectionState;
				FParse::Value( *DeviceInfo, TEXT( "ConnectionState=" ), ConnectionState );
				if( ConnectionState == FString( "CONNECTION_AVAILABLE" ) )
				{
					CachedConnectionState = ConnectionAvailable;
				}
				else if( ConnectionState == FString( "CONNECTION_CONNECTED" ) )
				{
					CachedConnectionState = ConnectionConnected;
				}
				else if( ConnectionState == FString( "CONNECTION_IN_USE" ) )
				{
					CachedConnectionState = ConnectionInUse;
				}

				FString SDKVersion;
				FParse::Value( *DeviceInfo, TEXT( "SDKVersion=" ), SDKVersion );
				CachedOSName = FString::Printf( TEXT( "ORBIS %s" ), *SDKVersion );

				FParse::Value( *DeviceInfo, TEXT( "MappedDrive=" ), CachedDriveLetter );
			}
		}
	}

	CachedId = FTargetDeviceId( TargetPlatform.PlatformName(), CachedName );
}


void FPS4TargetDevice::LogMessage( const TCHAR* Message )
{
	UE_LOG(LogPS4TargetDevice, Display, TEXT("%s"), Message)
}


void FPS4TargetDevice::ErrorMessage( const TCHAR* Message )
{
	UE_LOG(LogPS4TargetDevice, Error, TEXT("%s"), Message)
}
