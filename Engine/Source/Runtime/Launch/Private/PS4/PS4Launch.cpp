// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceError.h"
#include "LaunchEngineLoop.h"
#include <sdk_version.h>
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "HAL/PlatformStackWalk.h"

#if WITH_PS4_LIVE_CODE_EDIT 
#include <dbg_enc.h>
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLaunchPS4, Log, All);

/** The global EngineLoop instance */
FEngineLoop	GEngineLoop;

const char sceUserMainThreadName[]	= "UE4 Main Thread";
int sceUserMainThreadPriority		= SCE_KERNEL_PRIO_FIFO_DEFAULT;
size_t sceUserMainThreadStackSize	= 5 * 1024 * 1024;
size_t sceLibcHeapSize				= 0 * 1024 * 1024;

// set where to read/write files, depending on how we boot
FString GFileRootDirectory;

// Sandbox name.
FString GSandboxName;

//Edit and Continue enables a developer to make live changes to a running process 
//without having to restart the process on the DevKit. It is useful for fixing minor bugs, 
//tweaking code behavior, and inserting diagnostic or temporary code on the fly.
static void ApplyCodeChangeSafePoint()
{
#if WITH_PS4_LIVE_CODE_EDIT
	if (sceDbgEnCCheckForPendingChanges())
	{
		//Apply Code Changes results are displayed in Output window. If successful, the number of replaced modules is displayed. 
		//If unsuccessful, the reason for the failure, or failures, is displayed in the output window.
		//Check the Features and limitations of Edit and Continue documentation on the ps4dev site.

		// We sync Game/Render/RHI to allow the callstack to be as shallow as possible. This gives the debugger the greatest chance of applying any changes.
		static FFrameEndSync FrameEndSync;
		FrameEndSync.Sync(false);

		//Two ways of applying code changes
		//1- First way:
		//      1-Break the debugger, by using Break All from the Debug menu.
		//      2-Modify your code.
		//      3-Use the Apply Code Changes option from the Debug Menu
		//      
		//2- Second way:
		//      1-Modify your code
		//      2-Use the Apply Code Changes option from the Debug Menu

		// Depending on which thread your code modification is apply, you may need to add other synchronization to make 
		// sure edit and continue is not trying to change code that is currently on the callstack. You may manually move the instruction pointer to a safe location. 
		// The easiest way to do this is to step out of the functions in the debugger a few times until clear of the changed function.
		sceDbgEnCApplyChangesNow();
	}
#endif
}

static void ConvertSDKVersionToString( uint32 SDKVersion, char* SDKVersionString )
	{
		int32 SDKVersionIndex = 7;
		while( SDKVersion >> ( SDKVersionIndex * 4 ) == 0 && SDKVersionIndex >= 0 )
		{
			SDKVersionIndex--;
		}

		int32 SDKStringIndex = 0;
		for( ; SDKVersionIndex >= 0; SDKVersionIndex-- )
		{
			if( ( SDKVersionIndex + 1 ) % 3 == 0 )
			{
				SDKVersionString[SDKStringIndex++] = '.';
			}
			SDKVersionString[SDKStringIndex++] = char( ( ( SDKVersion >> ( SDKVersionIndex * 4 ) ) & 0xf ) + 0x30 );
		}
		SDKVersionString[SDKStringIndex++] = 0x0;
}

// Check that we are running a version of the PS4 System Software that is required by our game
void CheckForSDKSystemSoftwareMismatch()
{
#if !UE_BUILD_SHIPPING
	if( FPS4Misc::IsRunningOnDevKit() )
	{
		char SystemSoftwareVersionString[SCE_SYSTEM_SOFTWARE_VERSION_LEN + 1];
		char SDKVersionString[SCE_SYSTEM_SOFTWARE_VERSION_LEN + 1];
		char MinSystemSoftwareVersionString[SCE_SYSTEM_SOFTWARE_VERSION_LEN + 1];

		ConvertSDKVersionToString( SCE_ORBIS_SDK_VERSION, SDKVersionString );
		ConvertSDKVersionToString( MINIMUM_SYSTEM_SOFTWARE_VERSION, MinSystemSoftwareVersionString );

		// Get system software version string
		sceDbgGetSystemSwVersion( SystemSoftwareVersionString, SCE_SYSTEM_SOFTWARE_VERSION_LEN + 1 );

		// Check if system software is compatible with the minimum version the game requires
		uint32 Result = sceDbgRequireSystemSwVersion( MINIMUM_SYSTEM_SOFTWARE_VERSION );
		if( Result == SCE_OK )
		{
			printf( "PS4 SDK Version     = %s\n", SDKVersionString );
			printf( "PS4 System Software =%s\n", SystemSoftwareVersionString );
		}
		else
		{
			printf(	"************************************************************\n"
					"*** Detected SDK/System Software Version Incompatibility ***\n"
					"PS4 SDK Version     = %s\n"
					"PS4 System Software =%s\n"
					"PS4 Minimum Required System Software = %s\n"
					"Please update your PS4 DevKit System Software to a newer\n"
					"version that is compatible with the SDK being used\n"
					"************************************************************\n", SDKVersionString, SystemSoftwareVersionString, MinSystemSoftwareVersionString );

			UE_DEBUG_BREAK();
		}
	}
#endif
}



INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	CheckForSDKSystemSoftwareMismatch();

	double EngineInitializationStartTime = FPlatformTime::Seconds();	

	ANSICHAR AddArgsContents[256];
	AddArgsContents[0] ='\0';

	bool bUsingFileServer = false;
	bool bDeployedBuild = false;

	// By default we sandbox all data, logfiles, screnshots etc into a "GameName" folder when running from /data or writing
	// screenshots to hdd. This is great for different games but can be cumbersome for versions of the same game so the 
	// sandbox folder can be overriden by using -deployedbuild=name instead of just -deployedbuild. 
	// E.g -deployedbuild=GameName_Branch would read/write to /data/gamename_branch
	GSandboxName = FApp::GetProjectName();
	GSandboxName = GSandboxName.ToLower();

	// Args are not available to packaged builds, but still parse them in shipping for 
	// debugger purposes.
	for (int i = 1; i < ArgC; ++i)
	{
		FString Command(ArgV[i]);
		if (Command.Contains(TEXT("-filehostip")))
		{
			bUsingFileServer = true;
			break;
		}
		if (Command.Contains(TEXT("-deployedbuild")))
		{
			bDeployedBuild = true;
			FParse::Value(*Command, TEXT("deployedbuild="), GSandboxName);
			GSandboxName = GSandboxName.ToLower();
			break;
		}
	}	


	bool bApp0 = false;
	// if we're running with a fileserver, we must not allow the filesystem to be rooted to /app0, even if there is a ue4commandline.txt.
	// app0 with an active fileserver will not play well with the shortend path (deepfiles) support.	
	if (!bUsingFileServer)
	{
		//a deployed build will still set the working directory to the staging directory so that the OS can find the proper system files (trophy files, prx's, etc)
		//however all necessary data has been copied to the device, so we want to force a root of /data for speed.
		if (!bDeployedBuild)
		{
			FILE* Handle = nullptr;

			// in non-shipping builds check the root of the /temp0 drive for a commandline file. This allows
			// QA to use packaged builds but override the command line args
#if !UE_BUILD_SHIPPING
			Handle = fopen("/temp0/ue4commandline.txt", "r");
			if (Handle != nullptr)
			{
				printf("Using commandline args from /data/ue4commandline.txt!");
			}
#endif
			// look in /app0 for AddArgs.txt - if it's found, then we load from /app0 (VSH) otherwise load from /data (good for debugger)
			// @todo: PS3 used to have a way to detect how it was launched - we could use that now!
			if (Handle == nullptr)
			{
				Handle = fopen("/app0/ue4commandline.txt", "r");
			}

			if (Handle != NULL)
			{
				fgets(AddArgsContents, sizeof(AddArgsContents), Handle);
				fclose(Handle);
				Handle = NULL;

				printf("AdditionalArgs before fixup = %s\n", AddArgsContents);

				// chop off trailing spaces
				while (*AddArgsContents && isspace(AddArgsContents[strlen(AddArgsContents) - 1]))
				{
					AddArgsContents[strlen(AddArgsContents) - 1] = 0;
				}

				// always try for .pak files when installed
				strcat(AddArgsContents, " -pak");
				printf("AdditionalArgs after fixup = %s\n", AddArgsContents);

				FString AddArgsString( AddArgsContents );
				if( AddArgsString.Contains( TEXT( "-filehostip" ) ) || AddArgsString.Contains( TEXT( "-deployedbuild" ) ) )
				{
					bApp0 = false;
				}
				else
				{
					printf( "File root is /app0/!\n" );
					GFileRootDirectory = TEXT( "/app0/" );
					bApp0 = true;
				}
			}
		}
	}

	if (!bApp0)
	{
		printf("File root is hard drive!\n");
		GFileRootDirectory = TEXT("/data/");
	}

	FPlatformStackWalk::LoadSymbolInfo();

	// initialize the engine, and load out of the PC's cooked sandbox
	if (GEngineLoop.PreInit(ArgC, ArgV, ANSI_TO_TCHAR(AddArgsContents)))
	{
		printf("GEngineLoop.PreInit Failed!\n");
		_Exit(0);
	}
	if (GEngineLoop.Init())
	{
		printf("GEngineLoop.GEngineLoop.Init() Failed!\n");
		_Exit(0);
	}

	const double EngineInitializationTime = FPlatformTime::Seconds() - EngineInitializationStartTime;	
	UE_LOG(LogLaunchPS4, Display, TEXT("Engine Initialization Time: %fs"), EngineInitializationTime);
	ACCUM_LOADTIME(TEXT("EngineInitialization"), EngineInitializationTime);

	// tick until done
	while (!GIsRequestingExit)
	{
		GEngineLoop.Tick();
		ApplyCodeChangeSafePoint();
	}

	// exit out!
	_Exit(0);
//	GEngineLoop.Exit();

	return 0;
}




