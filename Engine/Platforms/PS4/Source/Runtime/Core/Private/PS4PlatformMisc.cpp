// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4PlatformMisc.h"
#include "PS4PlatformChunkInstall.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"

#include <app_content.h>
#include <libsysmodule.h>
#include <system_service.h>

void FPS4PlatformMisc::PlatformInit()
{
	FSonyPlatformMisc::PlatformInit();

	FCoreDelegates::OnGetOnScreenMessages.AddLambda([](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText >& OutMessages)
	{
			{
				static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PS4ContinuousSubmits"));
				int32 Value = CVar->GetInt();

				if (!Value)
				{
					// good for profiling (avoids bubbles) but bad for high fps
					OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning, FText::FromString(TEXT("r.PS4ContinuousSubmits")));
				}
			}
			{
				static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PS4StallsOnMarkers"));
				int32 Value = CVar->GetInt();

				if (Value)
				{
					// good to get Razor aligned GPU profiling but bad for high fps
					OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning, FText::FromString(TEXT("r.PS4StallsOnMarkers")));
				}
			}
	});
}

bool FPS4PlatformMisc::IsRunningOnDevKit()
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

IPlatformChunkInstall* FPS4PlatformMisc::GetPlatformChunkInstall()
{
	static FPS4ChunkInstall Singleton;
	return &Singleton;
}

TArray<FCustomChunk> FPS4PlatformMisc::GetAllOnDemandChunks()
{
	return GetCustomChunksByType(ECustomChunkType::OnDemandChunk);
}

TArray<FCustomChunk> FPS4PlatformMisc::GetAllLanguageChunks()
{
	return GetCustomChunksByType(ECustomChunkType::LanguageChunk);
}

TArray<FCustomChunk> FPS4PlatformMisc::GetCustomChunksByType(ECustomChunkType DesiredChunkType)
{
	static TArray<FCustomChunk> OnDemandChunks;
	static TArray<FCustomChunk> LanguageChunks;
	static bool bIsReady = false;
	if (!bIsReady)
	{
		TArray<FString> AllChunks;
		GConfig->GetArray(TEXT("/Script/PS4PlatformEditor.PS4TargetSettings"), TEXT("ChunkLanguageMapping"), AllChunks, GEngineIni);
		for (FString& Chunk : AllChunks)
		{
			// Remove parentheses
			Chunk.TrimStartAndEndInline();
			Chunk.ReplaceInline(TEXT("("), TEXT(""));
			Chunk.ReplaceInline(TEXT(")"), TEXT(""));

			// Find all custom chunks and parse
			const TCHAR* PropertyChunkId = TEXT("ChunkId=");
			const TCHAR* PropertyChunkTag = TEXT("CultureId=");
			const TCHAR* PropertyIsInitial = TEXT("IsInitial=");
			uint32 ChunkId;
			FString ChunkTag;
			bool IsInitialChunk;

			if (FParse::Value(*Chunk, PropertyChunkId, ChunkId) &&
				FParse::Value(*Chunk, PropertyChunkTag, ChunkTag) &&
				FParse::Bool(*Chunk, PropertyIsInitial, IsInitialChunk))
			{
				ChunkTag.ReplaceInline(TEXT("\""), TEXT(""));

				if (!IsInitialChunk && !ChunkTag.IsEmpty())
				{
					if (ChunkTag.StartsWith(TEXT("user")))
					{
						OnDemandChunks.Add(FCustomChunk(ChunkTag, ChunkId, ECustomChunkType::OnDemandChunk));
					}
					else
					{
						LanguageChunks.Add(FCustomChunk(ChunkTag, ChunkId, ECustomChunkType::LanguageChunk));
					}
				}
			}
		}

		bIsReady = true;
	}

	return (DesiredChunkType == ECustomChunkType::OnDemandChunk) ? OnDemandChunks : LanguageChunks;
}

FCustomChunkMapping::CustomChunkMappingType LexCustomChunkMappingFromString(const FString& Buffer)
{
	FCustomChunkMapping::CustomChunkMappingType RetValue = FCustomChunkMapping::CustomChunkMappingType::Main;

	if (FCString::Stricmp(*Buffer, TEXT("Optional")) == 0)
	{
		RetValue = FCustomChunkMapping::CustomChunkMappingType::Optional;
	}

	return RetValue;
}

bool FPS4PlatformMisc::GetCustomChunkMappings(TArray<FCustomChunkMapping>& Mapping)
{
	static TArray<FCustomChunkMapping> CustomChunkMappings;
	static bool bIsReady = false;
	if (!bIsReady && GConfig != nullptr && !GEngineIni.IsEmpty())
	{
		TArray<FString> CustomChunkMappingData;
		GConfig->GetArray(TEXT("/Script/PS4PlatformEditor.PS4TargetSettings"), TEXT("CustomChunkMapping"), CustomChunkMappingData, GEngineIni);

		for (FString& ChunkMapping : CustomChunkMappingData)
		{
			// Remove parentheses
			ChunkMapping.TrimStartAndEndInline();
			ChunkMapping.ReplaceInline(TEXT("("), TEXT(""));
			ChunkMapping.ReplaceInline(TEXT(")"), TEXT(""));

			// Find all custom chunks and parse
			const TCHAR* PropertyPattern = TEXT("Pattern=");
			const TCHAR* PropertyChunkId = TEXT("ChunkId=");
			const TCHAR* PropertyMappingType = TEXT("Type=");
			FString Pattern;
			uint32 ChunkId;

			if (FParse::Value(*ChunkMapping, PropertyChunkId, ChunkId) &&
				FParse::Value(*ChunkMapping, PropertyPattern, Pattern))
			{
				Pattern.ReplaceInline(TEXT("\""), TEXT(""));

				// Default is main if not specified
				FCustomChunkMapping::CustomChunkMappingType MappingType = FCustomChunkMapping::CustomChunkMappingType::Main;
				FString TypeString;
				if (FParse::Value(*ChunkMapping, PropertyMappingType, TypeString))
				{
					MappingType = LexCustomChunkMappingFromString(TypeString);
				}

				CustomChunkMappings.Add(FCustomChunkMapping(Pattern, ChunkId, MappingType));
			}
		}

		bIsReady = true;
	}

	Mapping = CustomChunkMappings;
	return bIsReady;
}

FString FPS4PlatformMisc::GetCPUBrand()
{
	return sceKernelIsNeoMode() ? FString(TEXT("Neo PS4 CPU")) : FString(TEXT("PS4 CPU"));
}

FString FPS4PlatformMisc::GetPrimaryGPUBrand()
{
	return sceKernelIsNeoMode() ? FString(TEXT("Sony Neo PS4 GPU")) : FString(TEXT("Sony PS4 GPU"));
}

void FPS4PlatformMisc::GetOSVersions(FString& out_OSVersionLabel, FString& out_OSSubVersionLabel)
{
	out_OSVersionLabel = FString(TEXT("PS4 OS"));
	out_OSSubVersionLabel = FString(TEXT(""));

#if !UE_BUILD_SHIPPING
	if (IsRunningOnDevKit())
	{
		char SystemSoftwareVersionString[SCE_SYSTEM_SOFTWARE_VERSION_LEN + 1];

		if (sceDbgGetSystemSwVersion(/*out*/ SystemSoftwareVersionString, UE_ARRAY_COUNT(SystemSoftwareVersionString)) == SCE_OK)
		{
			out_OSSubVersionLabel = StringCast<TCHAR>(SystemSoftwareVersionString).Get();
		}
	}
#endif
}

bool FPS4PlatformMisc::GetDownloadAreaSizeAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes)
{
	bool SuccessfullyGetDiskTotal = false;
	bool SuccessfullyGetFreeSpace = false;

	if (GConfig->IsReadyForUse())
	{
		int DownloadZeroSizeMB = 0;
		if (GConfig->GetInt(TEXT("/Script/PS4PlatformEditor.PS4TargetSettings"), TEXT("DownloadZeroSizeMB"), DownloadZeroSizeMB, GEngineIni))
		{
			TotalNumberOfBytes = DownloadZeroSizeMB * 1024 * 1024;
			SuccessfullyGetDiskTotal = true;
		}
	}

	SceAppContentMountPoint	DownloadDataMountPoint;
	strncpy(DownloadDataMountPoint.data, TCHAR_TO_ANSI(*InPath), SCE_APP_CONTENT_MOUNTPOINT_DATA_MAXSIZE);

	size_t AvailableSpaceKb;
	if (sceAppContentDownloadDataGetAvailableSpaceKb(&DownloadDataMountPoint, &AvailableSpaceKb) == SCE_OK)
	{
		NumberOfFreeBytes = AvailableSpaceKb * 1024;
		SuccessfullyGetFreeSpace = true;
	}

	return SuccessfullyGetDiskTotal && SuccessfullyGetFreeSpace;
}

struct PlayGoData
{
	FCriticalSection Lock;

	uint32 PlayGoInitCounter = 0;
	ScePlayGoHandle PlayGoHandle = 0;
	void* PlayGoBuffer = nullptr;
	bool bIsPlayGoInitialized = false;
	bool bIsPlayGoPackage = false;
};

PlayGoData& GetPlayGoData()
{
	static PlayGoData Data;
	return Data;
}

void PlayGoShutDownInternal(PlayGoData& Data)
{
	if (Data.bIsPlayGoPackage)
	{
		verify(SCE_OK == scePlayGoClose(Data.PlayGoHandle));
		Data.bIsPlayGoPackage = false;
	}

	Data.PlayGoHandle = 0;

	if (Data.bIsPlayGoInitialized)
	{
		verify(SCE_OK == scePlayGoTerminate());
		verify(SCE_OK == sceSysmoduleUnloadModule(SCE_SYSMODULE_PLAYGO));
		Data.bIsPlayGoInitialized = false;
	}

	if (Data.PlayGoBuffer)
	{
		FMemory::Free(Data.PlayGoBuffer);
		Data.PlayGoBuffer = nullptr;
	}
}

void FPS4PlatformMisc::PlayGoInit(ScePlayGoHandle& OutHandle)
{
	PlayGoData& Data = GetPlayGoData();
	FScopeLock Lock(&Data.Lock);

	OutHandle = 0;

	if (Data.PlayGoInitCounter > 0)
	{
		OutHandle = Data.PlayGoHandle;
		++Data.PlayGoInitCounter;
		return;
	}

	verify(SCE_OK == sceSysmoduleLoadModule(SCE_SYSMODULE_PLAYGO));

	ScePlayGoInitParams initParams;
	memset(&initParams, 0x0, sizeof(initParams));
	Data.PlayGoBuffer = FMemory::Malloc(SCE_PLAYGO_HEAP_SIZE);
	check(Data.PlayGoBuffer);
	initParams.bufAddr = Data.PlayGoBuffer;
	initParams.bufSize = SCE_PLAYGO_HEAP_SIZE;
	verify(SCE_OK == scePlayGoInitialize(&initParams));

	Data.bIsPlayGoInitialized = true;

	// get our package handle if this app is compatible with PlayGo
	int32_t SCEResult = scePlayGoOpen(&Data.PlayGoHandle, nullptr);
	Data.bIsPlayGoPackage = (SCEResult == SCE_OK);

	if (!Data.bIsPlayGoPackage)
	{
		PlayGoShutDownInternal(Data);
		OutHandle = 0;
		return;
	}

	OutHandle = Data.PlayGoHandle;
	++Data.PlayGoInitCounter;
	return;
}

void FPS4PlatformMisc::PlayGoShutdown(ScePlayGoHandle& InOutHandle)
{
	if (InOutHandle == 0)
	{
		return;
	}

	PlayGoData& Data = GetPlayGoData();
	FScopeLock Lock(&Data.Lock);

	InOutHandle = 0;

	ensureAlwaysMsgf(Data.PlayGoInitCounter > 0, TEXT("PlayGoInit and PlayGoShutdown must be called in matched pairs!"));
	if (Data.PlayGoInitCounter == 0)
	{
		return;
	}

	check(Data.PlayGoHandle == InOutHandle);

	--Data.PlayGoInitCounter;
	if (Data.PlayGoInitCounter == 0)
	{
		PlayGoShutDownInternal(Data);
	}
}

struct PlayGoLanguageData
{
	TMap<FString, ScePlayGoLanguageMask> PS4SystemLanguageMap;
	PlayGoLanguageData()
	{
		PS4SystemLanguageMap.Add(TEXT("ja"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_JAPANESE));
		PS4SystemLanguageMap.Add(TEXT("en"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_ENGLISH_US));
		PS4SystemLanguageMap.Add(TEXT("en-us"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_ENGLISH_US));
		PS4SystemLanguageMap.Add(TEXT("fr"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_FRENCH));
		PS4SystemLanguageMap.Add(TEXT("es"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_SPANISH));
		PS4SystemLanguageMap.Add(TEXT("es-es"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_SPANISH));
		PS4SystemLanguageMap.Add(TEXT("de"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_GERMAN));
		PS4SystemLanguageMap.Add(TEXT("it"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_ITALIAN));
		PS4SystemLanguageMap.Add(TEXT("nl"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_DUTCH));
		PS4SystemLanguageMap.Add(TEXT("pt"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT));
		PS4SystemLanguageMap.Add(TEXT("pt-pt"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT));
		PS4SystemLanguageMap.Add(TEXT("ru"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_RUSSIAN));
		PS4SystemLanguageMap.Add(TEXT("ko"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_KOREAN));
		PS4SystemLanguageMap.Add(TEXT("zh-hant"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_CHINESE_T));
		PS4SystemLanguageMap.Add(TEXT("zh-hans"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_CHINESE_S));
		PS4SystemLanguageMap.Add(TEXT("fi"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_FINNISH));
		PS4SystemLanguageMap.Add(TEXT("sv"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_SWEDISH));
		PS4SystemLanguageMap.Add(TEXT("da"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_DANISH));
		PS4SystemLanguageMap.Add(TEXT("no"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_NORWEGIAN));
		PS4SystemLanguageMap.Add(TEXT("pl"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_POLISH));
		PS4SystemLanguageMap.Add(TEXT("pt-br"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR));
		PS4SystemLanguageMap.Add(TEXT("en-gb"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_ENGLISH_GB));
		PS4SystemLanguageMap.Add(TEXT("tr"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_TURKISH));
		PS4SystemLanguageMap.Add(TEXT("es-la"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_SPANISH_LA));
		PS4SystemLanguageMap.Add(TEXT("ar"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_ARABIC));
		PS4SystemLanguageMap.Add(TEXT("fr-ca"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_FRENCH_CA));
		PS4SystemLanguageMap.Add(TEXT("cs"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_CZECH));
		PS4SystemLanguageMap.Add(TEXT("hu"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_HUNGARIAN));
		PS4SystemLanguageMap.Add(TEXT("el"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_GREEK));
		PS4SystemLanguageMap.Add(TEXT("ro"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_ROMANIAN));
		PS4SystemLanguageMap.Add(TEXT("th"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_THAI));
		PS4SystemLanguageMap.Add(TEXT("vi"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_VIETNAMESE));
		PS4SystemLanguageMap.Add(TEXT("id"), scePlayGoConvertLanguage(SCE_SYSTEM_PARAM_LANG_INDONESIAN));
		PS4SystemLanguageMap.Add(TEXT("user00"), SCE_PLAYGO_LANGUAGE_USER00);
		PS4SystemLanguageMap.Add(TEXT("user01"), SCE_PLAYGO_LANGUAGE_USER01);
		PS4SystemLanguageMap.Add(TEXT("user02"), SCE_PLAYGO_LANGUAGE_USER02);
		PS4SystemLanguageMap.Add(TEXT("user03"), SCE_PLAYGO_LANGUAGE_USER03);
		PS4SystemLanguageMap.Add(TEXT("user04"), SCE_PLAYGO_LANGUAGE_USER04);
		PS4SystemLanguageMap.Add(TEXT("user05"), SCE_PLAYGO_LANGUAGE_USER05);
		PS4SystemLanguageMap.Add(TEXT("user06"), SCE_PLAYGO_LANGUAGE_USER06);
		PS4SystemLanguageMap.Add(TEXT("user07"), SCE_PLAYGO_LANGUAGE_USER07);
		PS4SystemLanguageMap.Add(TEXT("user08"), SCE_PLAYGO_LANGUAGE_USER08);
		PS4SystemLanguageMap.Add(TEXT("user09"), SCE_PLAYGO_LANGUAGE_USER09);
		PS4SystemLanguageMap.Add(TEXT("user10"), SCE_PLAYGO_LANGUAGE_USER10);
		PS4SystemLanguageMap.Add(TEXT("user11"), SCE_PLAYGO_LANGUAGE_USER11);
		PS4SystemLanguageMap.Add(TEXT("user12"), SCE_PLAYGO_LANGUAGE_USER12);
		PS4SystemLanguageMap.Add(TEXT("user13"), SCE_PLAYGO_LANGUAGE_USER13);
		PS4SystemLanguageMap.Add(TEXT("user14"), SCE_PLAYGO_LANGUAGE_USER14);
	}
};

const PlayGoLanguageData& GetPlayGoLanguageData()
{
	// As of C++11 block scope static initialization is threadsafe
	static PlayGoLanguageData Data;
	return Data;
}

ScePlayGoLanguageMask FPS4PlatformMisc::GetPlayGoLanguageMask(const FString& LanguageName)
{
	const PlayGoLanguageData& Data = GetPlayGoLanguageData();

	const ScePlayGoLanguageMask* LanguageMask = Data.PS4SystemLanguageMap.Find(LanguageName.ToLower());
	return LanguageMask ? *LanguageMask : 0ULL;
}

bool FPS4PlatformMisc::GetDiskTotalAndFreeSpace(const FString& InPath, uint64& TotalNumberOfBytes, uint64& NumberOfFreeBytes)
{
	if (InPath == TEXT("/download0") || InPath.StartsWith(TEXT("/download0/")))
	{
		return GetDownloadAreaSizeAndFreeSpace(TEXT("/download0"), TotalNumberOfBytes, NumberOfFreeBytes);
	}
	else if (InPath == TEXT("/download1") || InPath.StartsWith(TEXT("/download1/")))
	{
		return GetDownloadAreaSizeAndFreeSpace(TEXT("/download1"), TotalNumberOfBytes, NumberOfFreeBytes);
	}

	return FGenericPlatformMisc::GetDiskTotalAndFreeSpace(InPath, TotalNumberOfBytes, NumberOfFreeBytes);
}

static FAutoConsoleCommand RestartPS4Command
(
	TEXT("PS4.Restart"),
	TEXT("Restart PS4"),
	FConsoleCommandDelegate::CreateLambda([]() { FPS4PlatformMisc::RestartApplication(); })
);

bool FPS4PlatformMisc::RestartApplication()
{
	FString OriginalCmdline = FCommandLine::GetOriginal();
	TArray<FString> CmdlineArgs;
	OriginalCmdline.ParseIntoArrayWS(CmdlineArgs);

	const int32 MaxArgLength = 256;
	char* ArgsBuffer = new char[MaxArgLength * CmdlineArgs.Num()];
	char** RestartArgV = new char*[CmdlineArgs.Num() + 2];

	RestartArgV[0] = (char*)"/app0/eboot.bin";

	for (int ArgIndex = 1; ArgIndex <= CmdlineArgs.Num(); ArgIndex++)
	{
		RestartArgV[ArgIndex] = &ArgsBuffer[MaxArgLength * (ArgIndex - 1)];
		int32 ArgLength = FMath::Min(CmdlineArgs[ArgIndex - 1].Len(), MaxArgLength - 1);
		strncpy(RestartArgV[ArgIndex], TCHAR_TO_ANSI(*CmdlineArgs[ArgIndex - 1]), ArgLength);
		RestartArgV[ArgIndex][ArgLength] = '\0';
	}

	RestartArgV[CmdlineArgs.Num() + 1] = nullptr;

	sceSystemServiceLoadExec(RestartArgV[0], RestartArgV);

	delete[]RestartArgV;
	delete[]ArgsBuffer;

	return true;
}

static TMap<int32, uint32> CachedPakchunkToChunkMapping;
static TMultiMap<uint32, FString> CachedChunkToPakchunkFileMapping;

void FPS4PlatformMisc::PopulateChunkPakchunkMapping()
{
	static bool bDefaultMappingApplied = false;
	static bool bCustomMappingApplied = false;
	static const FString PakDirectory = FPaths::ProjectContentDir() / TEXT("paks");

	if (!bDefaultMappingApplied)
	{
		// Add default mappings
		TArray<FString> AllPakFiles;
		IFileManager::Get().FindFilesRecursive(AllPakFiles, *PakDirectory, TEXT("*.pak"), true, false);
		for (FString& PakFile : AllPakFiles)
		{
			const FString BaseFileName = FPaths::GetCleanFilename(PakFile);
			int32 FilePakchunkIndex = GetPakchunkIndexFromPakFile(BaseFileName);

			if (!CachedPakchunkToChunkMapping.Contains(FilePakchunkIndex))
			{
				// Fall back chunk id to 0 if pak file doesn't have a pakchunk index.
				uint32 ChunkID = FilePakchunkIndex != INDEX_NONE ? FilePakchunkIndex : 0;

				CachedPakchunkToChunkMapping.Add(FilePakchunkIndex, ChunkID);
				CachedChunkToPakchunkFileMapping.AddUnique(ChunkID, BaseFileName);
			}
		}

		bDefaultMappingApplied = true;
	}
	
	if (bDefaultMappingApplied && !bCustomMappingApplied)
	{
		// Add mappings defined in custom chunk mapping
		TArray<FCustomChunkMapping> CustomChunkMappings;
		bool bCustomChunkMappingReady = GetCustomChunkMappings(CustomChunkMappings);
		if (bCustomChunkMappingReady)
		{
			for (FCustomChunkMapping CustomChunkMapping : CustomChunkMappings)
			{
				TArray<FString> FoundFiles;
				const FString SearchPath = FPaths::GetPath(CustomChunkMapping.Pattern);
				const FString SearchFilePattern = FPaths::GetCleanFilename(CustomChunkMapping.Pattern);
				IFileManager::Get().FindFilesRecursive(FoundFiles, *SearchPath, *SearchFilePattern, true, false);

				for (FString& PakFile : FoundFiles)
				{
					const FString BaseFileName = FPaths::GetCleanFilename(PakFile);
					int32 FilePakchunkIndex = GetPakchunkIndexFromPakFile(BaseFileName);

					check(CachedChunkToPakchunkFileMapping.Contains(CachedPakchunkToChunkMapping[FilePakchunkIndex]));
					CachedChunkToPakchunkFileMapping.Remove(CachedPakchunkToChunkMapping[FilePakchunkIndex], BaseFileName);
					CachedChunkToPakchunkFileMapping.AddUnique(CustomChunkMapping.ChunkID, BaseFileName);

					if (CustomChunkMapping.MappingType == FCustomChunkMapping::CustomChunkMappingType::Main)
					{
						CachedPakchunkToChunkMapping.FindOrAdd(FilePakchunkIndex) = CustomChunkMapping.ChunkID;
					}
				}
			}

			bCustomMappingApplied = true;
		}
	}
}

void FPS4PlatformMisc::UpdateChunkPakchunkMapping(uint32 ChunkID)
{
	// Add mappings defined in custom chunk mapping
	TArray<FCustomChunkMapping> CustomChunkMappings;
	bool bCustomChunkMappingReady = GetCustomChunkMappings(CustomChunkMappings);
	if (bCustomChunkMappingReady)
	{
		for (FCustomChunkMapping CustomChunkMapping : CustomChunkMappings)
		{
			if (CustomChunkMapping.ChunkID == ChunkID)
			{
				TArray<FString> FoundFiles;
				IFileManager::Get().FindFilesRecursive(FoundFiles, TEXT(""), *CustomChunkMapping.Pattern, true, false);

				for (FString& PakFile : FoundFiles)
				{
					const FString BaseFileName = FPaths::GetCleanFilename(PakFile);
					int32 FilePakchunkIndex = GetPakchunkIndexFromPakFile(BaseFileName);

					if (CustomChunkMapping.MappingType == FCustomChunkMapping::CustomChunkMappingType::Main)
					{
						CachedPakchunkToChunkMapping.FindOrAdd(FilePakchunkIndex) = CustomChunkMapping.ChunkID;
					}
					CachedChunkToPakchunkFileMapping.AddUnique(CustomChunkMapping.ChunkID, BaseFileName);
				}
			}
		}
	}
}

uint32 FPS4PlatformMisc::GetChunkIDFromPakchunkIndex(int32 PakchunkIndex)
{
	PopulateChunkPakchunkMapping();

	if (CachedPakchunkToChunkMapping.Contains(PakchunkIndex))
	{
		return CachedPakchunkToChunkMapping[PakchunkIndex];
	}
	else
	{
		return (uint32)PakchunkIndex;
	}
}

TArray<FString> FPS4PlatformMisc::GetPakchunkFilesFromChunkID(uint32 ChunkID)
{
	PopulateChunkPakchunkMapping();

	check(CachedChunkToPakchunkFileMapping.Contains(ChunkID));

	TArray<FString> PakchunkFiles;
	CachedChunkToPakchunkFileMapping.MultiFind(ChunkID, PakchunkFiles);

	return PakchunkFiles;
}
