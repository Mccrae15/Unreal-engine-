// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4PlatformFeatures.h"
#include "PS4ShareScreenshot.h"
#include "PS4ShareVideo.h"
#include "PS4SharePlay.h"
#include "PS4CompanionServer.h"
#include "PS4SaveGameSystem.h"
#include "PS4DVRStreaming.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"

#include <libsysmodule.h>
#include <app_content.h>

IMPLEMENT_MODULE(FPS4PlatformFeaturesModule, PS4PlatformFeatures);

FPS4PlatformFeaturesModule::FPS4PlatformFeaturesModule()
{
	// load generic modules
	StartupModules();

	// start a companion app server
#if PS4_FEATURE_COMPANION_APP
	new FPS4CompanionServer;
#endif

	bool bAllowShareScreenshots = PS4_FEATURE_SHARESCREENSHOT != 0;
	bool bAllowShareVideos = PS4_FEATURE_SHAREVIDEO != 0;
	bool bAllowSharePlay = PS4_FEATURE_SHAREPLAY != 0;
	bool bAllowLiveStreaming = PS4_FEATURE_GAMELIVESTREAMING != 0;

	TSharedPtr<const FJsonObject> Settings = GetTitleSettings();
	if (Settings.IsValid())
	{
		Settings->TryGetBoolField(TEXT("allow_sharescreenshots"), bAllowShareScreenshots);
		Settings->TryGetBoolField(TEXT("allow_sharevideo"), bAllowShareVideos);
		Settings->TryGetBoolField(TEXT("allow_shareplay"), bAllowSharePlay);
		Settings->TryGetBoolField(TEXT("allow_livestreaming"), bAllowLiveStreaming);
	}

	static FPS4ShareScreenshot ShareScreenshot;
	ShareScreenshot.EnableScreenshots(bAllowShareScreenshots);

	GetVideoRecordingSystem()->EnableRecording(bAllowShareVideos);

	static FPS4SharePlay SharePlay;
	SharePlay.EnableSharing(bAllowSharePlay);

	IDVRStreamingSystem* Streaming = GetStreamingSystem();
	Streaming->EnableStreaming(bAllowLiveStreaming);
}

ISaveGameSystem* FPS4PlatformFeaturesModule::GetSaveGameSystem()
{
	static FPS4SaveGameSystem System;
	return &System;
}

IDVRStreamingSystem* FPS4PlatformFeaturesModule::GetStreamingSystem()
{
	static FPS4DVRStreamingSystem System;
	return &System;
}

TSharedPtr<const class FJsonObject> FPS4PlatformFeaturesModule::GetTitleSettings()
{
	static bool bInitialized = false;
	if (!bInitialized)
	{
		// read the file
		FString TitleFile = TEXT("title.json");
#if !UE_BUILD_SHIPPING
		FString TitleID;
		if (FParse::Value(FCommandLine::Get(), TEXT("-TITLEID="), TitleID))
		{
			TitleFile = TitleID / TitleFile;
		}
#endif
		FArchive* Stream = IFileManager::Get().CreateFileReader(*TitleFile);
#if !UE_BUILD_SHIPPING
		if (Stream == nullptr)
		{
			const FString GameTitleFile = FPaths::ProjectDir() / TEXT("Build") / TEXT("PS4") / TEXT("titledata") / TitleFile;
			Stream = IFileManager::Get().CreateFileReader(*GameTitleFile);
		}
#endif
		if (Stream != nullptr)
		{
			// get the size of the file
			int32 Size = Stream->TotalSize();
			// read the file
			TArray<uint8> Buffer;
			Buffer.Empty(Size);
			Buffer.AddUninitialized(Size);
			Stream->Serialize(Buffer.GetData(), Size);
			// zero terminate it
			Buffer.Add(0);
			// Release the file
			delete Stream;

			// Now read it
			// init traveling pointer
			ANSICHAR* Ptr = (ANSICHAR*)Buffer.GetData();
			TSharedRef< TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(ANSI_TO_TCHAR(Ptr));
			FJsonSerializer::Deserialize(JsonReader, TitleSettings);
			bInitialized = true;
		}
	}

	return TitleSettings;
}

FString FPS4PlatformFeaturesModule::GetUniqueAppId()
{
	TSharedPtr<const FJsonObject> Settings = GetTitleSettings();
	if (Settings.IsValid())
	{
		return Settings->GetStringField(TEXT("title_id"));
	}
	return FString();
}

IVideoRecordingSystem* FPS4PlatformFeaturesModule::GetVideoRecordingSystem()
{
	static FPS4ShareVideo ShareVideo;
	return &ShareVideo;
}

bool FPS4PlatformFeaturesModule::StartupModules()
{
	return true;
}
