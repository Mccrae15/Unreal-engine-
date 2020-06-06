// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyPlatformMisc.h"
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
#include <libime/ime_keycode.h>

#include <message_dialog.h>
#include "HAL/IConsoleManager.h"

#if SONY_PROFILING_ENABLED
thread_local TArray<FColor> GTlsProfilerColorStack;
#endif

void FSonyPlatformMisc::PlatformInit()
{
	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName());
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName());

	// Get CPU info.

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle());
}


void FSonyPlatformMisc::PlatformHandleSplashScreen(bool ShowSplashScreen)
{
	// at this point in startup, the loading movie has just started, so now we can hide the splash screen.
	// @todo ps4: If the loading movie starts earlier, which it should, this will need to go much earlier!!
	sceSystemServiceHideSplashScreen();
}


bool FSonyPlatformMisc::SupportsMessaging()
{
	SceNetCtlInfo info;
	int32 err = sceNetCtlGetInfo(4, &info);
	//	err = sceNetCtlGetInfo(1, &info);
	//	err = sceNetCtlGetInfo(2, &info);

	return info.link == 1;
}

ENetworkConnectionType FSonyPlatformMisc::GetNetworkConnectionType()
{
	ENetworkConnectionType Result = ENetworkConnectionType::Unknown;

	SceNetCtlInfo Info;
	if (sceNetCtlGetInfo(SCE_NET_CTL_INFO_LINK, &Info) == SCE_OK)
	{
		if (Info.link == SCE_NET_CTL_LINK_CONNECTED)
		{
			if (sceNetCtlGetInfo(SCE_NET_CTL_INFO_DEVICE, &Info) == SCE_OK)
			{
				if (Info.device == SCE_NET_CTL_DEVICE_WIRED)
				{
					Result = ENetworkConnectionType::Ethernet;
				}
				else if (Info.device == SCE_NET_CTL_DEVICE_WIRELESS)
				{
					Result = ENetworkConnectionType::WiFi;
				}
			}
		}
		else
		{
			Result = ENetworkConnectionType::None;
		}
	}

	return Result;
}

bool FSonyPlatformMisc::HasActiveWiFiConnection()
{
	return GetNetworkConnectionType() == ENetworkConnectionType::WiFi;
}

FString FSonyPlatformMisc::GetDefaultLocale()
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
		case SCE_SYSTEM_PARAM_LANG_CZECH:
			return FString(TEXT("cs"));
		case SCE_SYSTEM_PARAM_LANG_HUNGARIAN:
			return FString(TEXT("hu"));
		case SCE_SYSTEM_PARAM_LANG_GREEK:
			return FString(TEXT("el"));
		case SCE_SYSTEM_PARAM_LANG_ROMANIAN:
			return FString(TEXT("ro"));
		case SCE_SYSTEM_PARAM_LANG_THAI:
			return FString(TEXT("th"));
		case SCE_SYSTEM_PARAM_LANG_VIETNAMESE:
			return FString(TEXT("vi"));
		case SCE_SYSTEM_PARAM_LANG_INDONESIAN:
			return FString(TEXT("id"));
		default:
		case SCE_SYSTEM_PARAM_LANG_ENGLISH_US:
			return FString(TEXT("en-US"));
			break;
	}
}

FString FSonyPlatformMisc::GetTimeZoneId()
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

//make these lowercase on PS4 to be consistent with staged and fileserved paths.
const TCHAR* FSonyPlatformMisc::RootDir()
{
	static FString RootDirectory = TEXT("");
	if (RootDirectory.Len() == 0)
	{
		RootDirectory = FGenericPlatformMisc::RootDir();
		RootDirectory = RootDirectory.ToLower();
	}
	return *RootDirectory;
}

const TCHAR* FSonyPlatformMisc::EngineDir()
{
	static FString EngineDirectory = TEXT("");
	if (EngineDirectory.Len() == 0)
	{
		EngineDirectory = FGenericPlatformMisc::EngineDir();
		EngineDirectory = EngineDirectory.ToLower();
	}
	return *EngineDirectory;
}

const TCHAR* FSonyPlatformMisc::ProjectDir()
{
	static FString ProjectDirectory = TEXT("");
	if (ProjectDirectory.Len() == 0)
	{
		ProjectDirectory = FGenericPlatformMisc::ProjectDir();
		ProjectDirectory = ProjectDirectory.ToLower();
	}
	return *ProjectDirectory;
}

FString FSonyPlatformMisc::CloudDir()
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

const TCHAR* FSonyPlatformMisc::GamePersistentDownloadDir()
{
	static const TCHAR* GamePersistentDownloadDir = TEXT("/download0/");
	return GamePersistentDownloadDir;
}

bool FSonyPlatformMisc::GetSHA256Signature(const void* Data, uint32 ByteSize, FSHA256Signature& OutSignature)
{
	const int32 Ret = sceSha256Digest(Data, ByteSize, OutSignature.Signature);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogSony, Warning, TEXT("sceSha256Digest failed: 0x%x"), Ret);
		FMemory::Memzero(OutSignature.Signature);
	}
	return Ret == SCE_OK;
}

#if SONY_PROFILING_ENABLED

void FSonyPlatformMisc::BeginNamedEvent(const struct FColor& Color, const TCHAR* Text)
{
	BeginNamedEvent(Color, TCHAR_TO_ANSI(Text));
}

void FSonyPlatformMisc::BeginNamedEvent(const struct FColor& Color, const ANSICHAR* Text)
{
	// this is a temporary workaround for an issue with Razor Live CPU v7.0.0.6
	// where user markers longer than 63 bytes (including ending terminator) cause Razor Live CPU to crash

	const size_t TextLengthMax = 62; // 63 with ending terminator
	const size_t TextLength = strlen(Text);

	const char* TextStart = TextLength > TextLengthMax ? Text + (TextLength - TextLengthMax) : Text;

	if (FPlatformMisc::IsRunningOnDevKit() && sceRazorCpuIsCapturing())
	{
		if (Color.DWColor() == 0 && GTlsProfilerColorStack.Num())
		{
			sceRazorCpuPushMarker(TextStart, GTlsProfilerColorStack.Last().ToPackedABGR(), 0);
		}
		else
		{
			sceRazorCpuPushMarker(TextStart, Color.ToPackedABGR(), 0);
		}
	}
#if CPUPROFILERTRACE_ENABLED
	if (CpuChannel)
	{
		FCpuProfilerTrace::OutputBeginDynamicEvent(Text);
	}
#endif
}

void FSonyPlatformMisc::BeginNamedEventStatic(const struct FColor& Color, const TCHAR* Text)
{
	// We can't use the sceRazorCpuPushMarkerStatic function here, as Sony requires ANSICHAR.
	// Forward to the regular named event function...
	BeginNamedEvent(Color, Text);
}

void FSonyPlatformMisc::BeginNamedEventStatic(const struct FColor& Color, const ANSICHAR* Text)
{
	// this is a temporary workaround for an issue with Razor Live CPU v7.0.0.6
	// where user markers longer than 63 bytes (including ending terminator) cause Razor Live CPU to crash

	const size_t TextLengthMax = 62; // 63 with ending terminator
	const size_t TextLength = strlen(Text);

	const char* TextStart = TextLength > TextLengthMax ? Text + (TextLength - TextLengthMax) : Text;

	if (FPlatformMisc::IsRunningOnDevKit() && sceRazorCpuIsCapturing())
	{
		if (Color.DWColor() == 0 && GTlsProfilerColorStack.Num())
		{
			sceRazorCpuPushMarkerStatic(TextStart, GTlsProfilerColorStack.Last().ToPackedABGR(), 0);
		}
		else
		{
			sceRazorCpuPushMarkerStatic(TextStart, Color.ToPackedABGR(), 0);
		}
	}
#if CPUPROFILERTRACE_ENABLED
	if (CpuChannel)
	{
		FCpuProfilerTrace::OutputBeginDynamicEvent(Text);
	}
#endif
}

void FSonyPlatformMisc::EndNamedEvent()
{
	if (FPlatformMisc::IsRunningOnDevKit() && sceRazorCpuIsCapturing())
	{
		sceRazorCpuPopMarker();
	}
#if CPUPROFILERTRACE_ENABLED
	if (CpuChannel)
	{
		FCpuProfilerTrace::OutputEndEvent();
	}
#endif
}

void* PS4TaggedMemoryBuffer = nullptr;

void FSonyPlatformMisc::InitTaggedStorage(uint32 NumTags)
{
	if (FPlatformMisc::IsRunningOnDevKit())
	{
		const size_t NumBytes = sceRazorCpuGetDataTagStorageSize(NumTags);
		PS4TaggedMemoryBuffer = FMemory::Malloc(NumBytes);
		sceRazorCpuInitDataTags(PS4TaggedMemoryBuffer, NumBytes);
	}
}

void FSonyPlatformMisc::ShutdownTaggedStorage()
{
	if (PS4TaggedMemoryBuffer)
	{
		sceRazorCpuShutdownDataTags();
		FMemory::Free(PS4TaggedMemoryBuffer);
		PS4TaggedMemoryBuffer = nullptr;
	}
}

void FSonyPlatformMisc::TagBuffer(const char* Label, uint32 Category, const void* Buffer, size_t BufferSize)
{
	if (PS4TaggedMemoryBuffer)
	{
		sceRazorCpuTagBuffer(Label, Category, Buffer, BufferSize);
	}
}

void FSonyPlatformMisc::BeginProfilerColor(const struct FColor& Color)
{
	GTlsProfilerColorStack.Push(Color);
}

void FSonyPlatformMisc::EndProfilerColor()
{
	GTlsProfilerColorStack.Pop();
}

#endif // SONY_PROFILING_ENABLED

FString FSonyPlatformMisc::GetCPUVendor()
{
	return FString(TEXT("Sony"));
}

FString FSonyPlatformMisc::GetOSVersion()
{
	return FString();
}

void FSonyPlatformMisc::RequestExit(bool Force)
{
	UE_LOG(LogSony, Log, TEXT("FPlatformMisc::RequestExit(%i)"), Force);

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

TArray<uint8> FSonyPlatformMisc::GetMacAddress()
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

FString FSonyPlatformMisc::GetDeviceId()
{
	// @todo: When this function is finally removed, the functionality used will need to be moved in here.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return GetUniqueDeviceId();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

#if !UE_BUILD_SHIPPING
bool FSonyPlatformMisc::IsDebuggerPresent()
{
	extern CORE_API bool GIgnoreDebugger;
	if (GIgnoreDebugger == false && FPlatformMisc::IsRunningOnDevKit())
	{
		return !!::sceDbgIsDebuggerAttached();
	}
	else
	{
		return false;
	}
}
#endif

static FCriticalSection GMessageBoxCS;

EAppReturnType::Type FSonyPlatformMisc::MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
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
				UE_LOG(LogSony, Warning, TEXT("sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG) failed (result 0x%08x)."), Result);
				return;
			}

			Result = sceCommonDialogInitialize();
			if (Result != SCE_OK && Result != SCE_COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED)
			{
				UE_LOG(LogSony, Warning, TEXT("sceCommonDialogInitialize() failed (result 0x%08x)."), Result);
				return;
			}

			Result = sceMsgDialogInitialize();
			if (Result != SCE_OK)
			{
				UE_LOG(LogSony, Warning, TEXT("sceMsgDialogInitialize() failed (result 0x%08x)."), Result);
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
		UE_LOG(LogSony, Warning, TEXT("Failed to initialize MsgDialog library."));
		return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
	}

	SceMsgDialogUserMessageParam UserMsgParam = {};
	SceMsgDialogButtonsParam ButtonsParam = {};

	switch (MsgType)
	{
		// Other message box types are not supported on PS4
	default:
		UE_LOG(LogSony, Warning, TEXT("Message box type is not supported on PS4."));
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
		UE_LOG(LogSony, Warning, TEXT("sceMsgDialogOpen failed (result %d)."), Result);
		return FGenericPlatformMisc::MessageBoxExt(MsgType, Text, Caption);
	}

	// Block this thread until the message box returns.
	while (sceMsgDialogUpdateStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
		FPlatformProcess::Sleep(0.01f);

	SceMsgDialogResult DialogResult = {};
	Result = sceMsgDialogGetResult(&DialogResult);
	if (Result < 0)
	{
		UE_LOG(LogSony, Warning, TEXT("sceMsgDialogGetResult failed (result %d)."), Result);
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
