// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4Misc.h"
#include "Misc/CoreStats.h"
#include "Misc/CoreMisc.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include <libsha256.h>
#include "HAL/PlatformProcess.h"
#include "Misc/OutputDeviceRedirector.h"

#include <libsysmodule.h>

#if !UE_BUILD_SHIPPING
	#include <libdbg.h>
#endif

#include <libnetctl.h>
#include <system_service.h>
#include <net.h>
#include "PS4ChunkInstall.h"
#include <libime/ime_keycode.h>

#include <message_dialog.h>
#include "IConsoleManager.h"
#include "PS4/PS4File.h"

#ifndef WITH_PGO_OUTOUT
	#define WITH_PGO_OUTOUT 0
#endif


// Set via UBT when PS4Platform.bGenerateProfileGuidedOptimizationInput is true. That part turns
// on instrumented output, now we just need to specify where to write it... abd then actually
// write it out in RequestExit
#if WITH_PGO_OUTOUT

extern "C" void __llvm_profile_reset_counters(void);
extern "C" void __llvm_profile_set_filename(const char *Name);
extern "C" int __llvm_profile_write_file();

static FAutoConsoleCommandWithOutputDevice PGOStartCommand(
	TEXT("pgo.start"),
	TEXT("Clears all profile counters used to create PGO output files"),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Ar) {
	__llvm_profile_reset_counters();
	Ar.Logf(TEXT("Reset PGO counters"));
})
);

static FAutoConsoleCommandWithOutputDevice PGOStopCommand(
	TEXT("pgo.stop"),
	TEXT("Writes out profile information for PGO"),
	FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Ar) {

	FPS4PlatformFile File;
	FString OutPath = File.NormalizeFileName(TEXT("build/ps4/output.profraw"), false, false);

	__llvm_profile_set_filename(TCHAR_TO_UTF8(*OutPath));

	if (__llvm_profile_write_file() == 0)
	{
		Ar.Logf(TEXT("Wrote PGO file to %s"), *OutPath);
	}
	else
	{
		Ar.Logf(TEXT("Failed to write PGO file to %s"), *OutPath);
	}
})
);

#endif // WITH_PGO_OUTOUT


void FPS4Misc::PlatformInit()
{
	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName());
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName());

	// Get CPU info.

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle());
}


void FPS4Misc::PlatformHandleSplashScreen(bool ShowSplashScreen)
{
	// at this point in startup, the loading movie has just started, so now we can hide the splash screen.
	// @todo ps4: If the loading movie starts earlier, which it should, this will need to go much earlier!!
	sceSystemServiceHideSplashScreen();	
#if UE_GAME != 1
	#error "The PS4 must either be a game or a client"
#endif
	check(IsRunningGame() || IsRunningClientOnly());
}


IPlatformChunkInstall* FPS4Misc::GetPlatformChunkInstall()
{
	static FPS4ChunkInstall Singleton;
	return &Singleton;
}


bool FPS4Misc::SupportsMessaging()
{
	SceNetCtlInfo info;
	int32 err = sceNetCtlGetInfo(4, &info);
//	err = sceNetCtlGetInfo(1, &info);
//	err = sceNetCtlGetInfo(2, &info);

	return info.link == 1;
}


FString FPS4Misc::GetDefaultLocale()
{
	int32 SystemLanguage = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
	int32 Ret = sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_LANG, &SystemLanguage);
	checkf(Ret == SCE_OK, TEXT("sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_LANG) failed: 0x%x"), Ret);

	switch (SystemLanguage)
	{
		case SCE_SYSTEM_PARAM_LANG_JAPANESE:
			return FString(TEXT("ja"));
		case SCE_SYSTEM_PARAM_LANG_FRENCH:
			return FString(TEXT("fr"));
		case SCE_SYSTEM_PARAM_LANG_FRENCH_CA:
			return FString(TEXT("fr-CA"));
		case SCE_SYSTEM_PARAM_LANG_SPANISH:
			return FString(TEXT("es"));
		case SCE_SYSTEM_PARAM_LANG_GERMAN:
			return FString(TEXT("de"));
		case SCE_SYSTEM_PARAM_LANG_ITALIAN:
			return FString(TEXT("it"));
		case SCE_SYSTEM_PARAM_LANG_DUTCH:
			return FString(TEXT("nl"));
		case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
			return FString(TEXT("pt"));
		case SCE_SYSTEM_PARAM_LANG_RUSSIAN:
			return FString(TEXT("ru"));
		case SCE_SYSTEM_PARAM_LANG_KOREAN:
			return FString(TEXT("ko"));
		case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
			return FString(TEXT("zh-Hant"));
		case SCE_SYSTEM_PARAM_LANG_CHINESE_S:
			return FString(TEXT("zh-Hans"));
		case SCE_SYSTEM_PARAM_LANG_FINNISH:
			return FString(TEXT("fi"));
		case SCE_SYSTEM_PARAM_LANG_SWEDISH:
			return FString(TEXT("sv"));
		case SCE_SYSTEM_PARAM_LANG_DANISH:
			return FString(TEXT("da"));
		case SCE_SYSTEM_PARAM_LANG_NORWEGIAN:
			return FString(TEXT("no"));
		case SCE_SYSTEM_PARAM_LANG_POLISH:
			return FString(TEXT("pl"));
		case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR:
			return FString(TEXT("pt-BR"));
		case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB:
			return FString(TEXT("en-GB"));
		case SCE_SYSTEM_PARAM_LANG_TURKISH:
			return FString(TEXT("tr"));
		case SCE_SYSTEM_PARAM_LANG_SPANISH_LA:
			return FString(TEXT("es-419"));
		case SCE_SYSTEM_PARAM_LANG_ARABIC:
			return FString(TEXT("ar"));
		default:
		case SCE_SYSTEM_PARAM_LANG_ENGLISH_US:
			return FString(TEXT("en-US"));
			break;
	}
}

FString FPS4Misc::GetTimeZoneId()
{
	int32 SystemTimeZoneOffsetMinutes = 0;
	int32 Ret = sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_TIME_ZONE, &SystemTimeZoneOffsetMinutes);
	checkf(Ret == SCE_OK, TEXT("sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_TIME_ZONE) failed: 0x%x"), Ret);

	int32 SystemTimeZoneIsSummerTime = 0;
	Ret = sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_SUMMERTIME, &SystemTimeZoneIsSummerTime);
	checkf(Ret == SCE_OK, TEXT("sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_SUMMERTIME) failed: 0x%x"), Ret);

	const int32 RawTimeZoneOffsetHours = (SystemTimeZoneOffsetMinutes / 60) + SystemTimeZoneIsSummerTime; // Assumes DST always adds 1h
	const int32 RawTimeZoneOffsetMinutes = SystemTimeZoneOffsetMinutes % 60;
	return FString::Printf(TEXT("GMT%+d:%02d"), RawTimeZoneOffsetHours, RawTimeZoneOffsetMinutes);
}

bool FPS4Misc::IsRunningOnDevKit()
{
	// Hack! We can only use the debug keyboard on devkits in development mode so if we can't open it then we must be
	// running on a test kit or a devkit in non-development mode
	// @TODO: Find a better way of detecting the mode that the devkit is on

	static bool bIsInitialized = false;
	static bool bIsDevKit = false;

#if !UE_BUILD_SHIPPING
	if( bIsInitialized == false )
	{
		int32 Result = sceSysmoduleLoadModule( SCE_SYSMODULE_DEBUG_KEYBOARD );
		if( Result == SCE_OK )
		{
			bIsDevKit = true;
			sceSysmoduleUnloadModule( SCE_SYSMODULE_DEBUG_KEYBOARD );
		}

		bIsInitialized = true;
	}
#endif
	return bIsDevKit;
}

//make these lowercase on PS4 to be consistent with staged and fileserved paths.
const TCHAR* FPS4Misc::RootDir()
{
	static FString RootDirectory = TEXT("");
	if (RootDirectory.Len() == 0)
	{
		RootDirectory = FGenericPlatformMisc::RootDir();
		RootDirectory = RootDirectory.ToLower();
	}
	return *RootDirectory;
}

const TCHAR* FPS4Misc::EngineDir()
{
	static FString EngineDirectory = TEXT("");
	if (EngineDirectory.Len() == 0)
	{
		EngineDirectory = FGenericPlatformMisc::EngineDir();
		EngineDirectory = EngineDirectory.ToLower();
	}
	return *EngineDirectory;
}

const TCHAR* FPS4Misc::ProjectDir()
{
	static FString ProjectDirectory = TEXT("");
	if (ProjectDirectory.Len() == 0)
	{
		ProjectDirectory = FGenericPlatformMisc::ProjectDir();
		ProjectDirectory = ProjectDirectory.ToLower();
	}
	return *ProjectDirectory;
}

FString FPS4Misc::CloudDir()
{
	static const FString DownloadDir("/download0/");

	FString UserSavedDirPath = FPaths::ProjectSavedDir();
	int32 DotDotSlashIdx = UserSavedDirPath.Find(TEXT("../"), ESearchCase::CaseSensitive);
	while (DotDotSlashIdx != INDEX_NONE)
	{
		UserSavedDirPath = UserSavedDirPath.Right(UserSavedDirPath.Len() - (DotDotSlashIdx + 3));
		DotDotSlashIdx = UserSavedDirPath.Find(TEXT("../"), ESearchCase::CaseSensitive);
	}

	return DownloadDir + UserSavedDirPath + TEXT("Cloud/");
}

const TCHAR* FPS4Misc::GamePersistentDownloadDir()
{
	static const TCHAR* GamePersistentDownloadDir = TEXT("/download0/");
	return GamePersistentDownloadDir;
}

bool FPS4Misc::GetSHA256Signature(const void* Data, uint32 ByteSize, FSHA256Signature& OutSignature)
{
	const int32 Ret = sceSha256Digest(Data, ByteSize, OutSignature.Signature);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogPS4, Warning, TEXT("sceSha256Digest failed: 0x%x"), Ret);
		FMemory::Memzero(OutSignature.Signature);		
	}	
	return Ret == SCE_OK;
}

#if PS4_PROFILING_ENABLED

void FPS4Misc::BeginNamedEvent(const struct FColor& Color, const TCHAR* Text)
{
	if (IsRunningOnDevKit() && sceRazorCpuIsCapturing())
	{
		sceRazorCpuPushMarker(TCHAR_TO_ANSI(Text), Color.ToPackedABGR(), 0);
	}
}

void FPS4Misc::BeginNamedEvent(const struct FColor& Color, const ANSICHAR* Text)
{
	if (IsRunningOnDevKit() && sceRazorCpuIsCapturing())
	{
		sceRazorCpuPushMarker(Text, Color.ToPackedABGR(), 0);
	}
}

void FPS4Misc::EndNamedEvent()
{
	if (IsRunningOnDevKit() && sceRazorCpuIsCapturing())
	{		
		sceRazorCpuPopMarker();
	}
}

void* PS4TaggedMemoryBuffer = nullptr;

void FPS4Misc::InitTaggedStorage(uint32 NumTags)
{
	if (IsRunningOnDevKit())
	{
		const size_t NumBytes = sceRazorCpuGetDataTagStorageSize(NumTags);
		PS4TaggedMemoryBuffer = FMemory::Malloc(NumBytes);
		sceRazorCpuInitDataTags(PS4TaggedMemoryBuffer, NumBytes);
	}
}

void FPS4Misc::ShutdownTaggedStorage()
{
	if (PS4TaggedMemoryBuffer)
	{
		sceRazorCpuShutdownDataTags();
		FMemory::Free(PS4TaggedMemoryBuffer);
		PS4TaggedMemoryBuffer = nullptr;
	}
}

void FPS4Misc::TagBuffer(const char* Label, uint32 Category, const void* Buffer, size_t BufferSize)
{
	if (PS4TaggedMemoryBuffer)
	{
		sceRazorCpuTagBuffer(Label, Category, Buffer, BufferSize);
	}
}

#endif

FString FPS4Misc::GetCPUVendor()
{
	return FString(TEXT("Sony"));
}

FString FPS4Misc::GetCPUBrand()
{
	return sceKernelIsNeoMode() ? FString(TEXT("Neo PS4 CPU")) : FString(TEXT("PS4 CPU"));
}

FString FPS4Misc::GetPrimaryGPUBrand()
{
	return sceKernelIsNeoMode() ? FString(TEXT("Sony Neo PS4 GPU")) : FString(TEXT("Sony PS4 GPU"));	
}

void FPS4Misc::GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel)
{
	out_OSVersionLabel = FString(TEXT("PS4 OS"));
	out_OSSubVersionLabel = FString(TEXT(""));

#if !UE_BUILD_SHIPPING
	if (IsRunningOnDevKit())
	{
		char SystemSoftwareVersionString[SCE_SYSTEM_SOFTWARE_VERSION_LEN + 1];

		if (sceDbgGetSystemSwVersion(/*out*/ SystemSoftwareVersionString, ARRAY_COUNT(SystemSoftwareVersionString)) == SCE_OK)
		{
			out_OSSubVersionLabel = StringCast<TCHAR>(SystemSoftwareVersionString).Get();
		}
	}
#endif
}

FString FPS4Misc::GetOSVersion()
{
	return FString();
}

void FPS4Misc::RequestExit(bool Force)
{
	UE_LOG(LogPS4, Log, TEXT("FPlatformMisc::RequestExit(%i)"), Force);

	// Clean application exit not allowed, force an immediate exit.
	// Dangerous because config code isn't flushed, global destructors aren't called, etc.

	// Make sure the log is flushed.
	if (GLog)
	{
		// This may be called from other thread, so set this thread as the master.
		GLog->SetCurrentThreadAsMasterThread();
		GLog->TearDown();
	}

	if (GIsCriticalError)
	{
		// Calling abort will give us a crash dump, if enabled.
		abort();
	}
	else
	{
#if ENABLE_PGO_PROFILE
		// Write the PGO profiling file on a clean shutdown.
		extern void PGO_WriteFile();
		PGO_WriteFile();
#endif

		// Otherwise exit cleanly and immediately.
		quick_exit(EXIT_SUCCESS);
	}
}

TArray<uint8> FPS4Misc::GetMacAddress()
{
	TArray<uint8> Result;

	SceNetEtherAddr MacAddr;
	FMemory::Memzero(MacAddr);
	const int32 Ret = sceNetGetMacAddress(&MacAddr, 0);
	if (Ret == SCE_OK)
	{
		Result.AddZeroed(SCE_NET_ETHER_ADDR_LEN);
		FMemory::Memcpy(Result.GetData(), MacAddr.data, SCE_NET_ETHER_ADDR_LEN);
	}

	return Result;
}

FString FPS4Misc::GetDeviceId()
{
	// @todo: When this function is finally removed, the functionality used will need to be moved in here.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GetUniqueDeviceId();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

static FCriticalSection GMessageBoxCS;

EAppReturnType::Type FPS4Misc::MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	FScopeLock Lock(&GMessageBoxCS);

	sceSystemServiceHideSplashScreen();

	// Scoped object to ensure sceMsgDialogTerminate is always called when we return.
	struct FScopedMsgDialogLibrary
	{
		bool bInitialized;

		FScopedMsgDialogLibrary()
			: bInitialized(false)
		{
			int Result = sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
			if (Result != SCE_OK)
			{
				UE_LOG(LogPS4, Warning, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG) failed (result 0x%08x)."), Result);
				return;
			}

			Result = sceCommonDialogInitialize();
			if (Result != SCE_OK && Result != SCE_COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED)
			{
				UE_LOG(LogPS4, Warning, TEXT("sceCommonDialogInitialize() failed (result 0x%08x)."), Result);
				return;
			}

			Result = sceMsgDialogInitialize();
			if (Result != SCE_OK)
			{
				UE_LOG(LogPS4, Warning, TEXT("sceMsgDialogInitialize() failed (result 0x%08x)."), Result);
				return;
			}

			bInitialized = true;
		}

		~FScopedMsgDialogLibrary()
		{
			if (bInitialized)
			{
				sceMsgDialogTerminate();
			}
		}
	} MsgLibraryScope;

	if (!MsgLibraryScope.bInitialized)
	{
		// Fallback to generic platform behavior.
		UE_LOG(LogPS4, Warning, TEXT("Failed to initialize MsgDialog library."));
		return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
	}

	SceMsgDialogUserMessageParam UserMsgParam = {};
	SceMsgDialogButtonsParam ButtonsParam = {};

	switch (MsgType)
	{
		// Other message box types are not supported on PS4
	default:
		UE_LOG(LogPS4, Warning, TEXT("Message box type is not supported on PS4."));
		return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);

	case EAppMsgType::Ok:		UserMsgParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK; break;
	case EAppMsgType::YesNo:	UserMsgParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_YESNO; break;
	case EAppMsgType::OkCancel:	UserMsgParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL; break;
	}

	auto AnsiString = StringCast<ANSICHAR>(Text);
	UserMsgParam.msg = AnsiString.Get();

	SceMsgDialogParam Param;
	sceMsgDialogParamInitialize(&Param);
	Param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	Param.userMsgParam = &UserMsgParam;

	int Result = sceMsgDialogOpen(&Param);
	if (Result < 0)
	{
		UE_LOG(LogPS4, Warning, TEXT("sceMsgDialogOpen failed (result %d)."), Result);
		return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
	}

	// Block this thread until the message box returns.
	while (sceMsgDialogUpdateStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
		FPlatformProcess::Sleep(0.01f);

	SceMsgDialogResult DialogResult = {};
	Result = sceMsgDialogGetResult(&DialogResult);
	if (Result < 0)
	{
		UE_LOG(LogPS4, Warning, TEXT("sceMsgDialogGetResult failed (result %d)."), Result);
		return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
	}

	// Convert the result back to the engine type.
	switch (MsgType)
	{
	default:
		checkNoEntry(); // We should never get here
		return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);

	case EAppMsgType::Ok:
		return EAppReturnType::Ok;

	case EAppMsgType::YesNo:
		if (DialogResult.result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED ||
			DialogResult.buttonId == SCE_MSG_DIALOG_BUTTON_ID_NO)
			return EAppReturnType::No;
		else
			return EAppReturnType::Yes;

	case EAppMsgType::OkCancel:
		if (DialogResult.result == SCE_COMMON_DIALOG_RESULT_USER_CANCELED)
			return EAppReturnType::Cancel;
		else
			return EAppReturnType::Ok;
	}
}
