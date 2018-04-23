// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PS4TargetPlatform.cpp: Implements the FPS4TargetPlatform class.
=============================================================================*/

#include "PS4TargetPlatform.h"
#include "Misc/ScopeLock.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#if WITH_ENGINE
	#include "TextureResource.h"
#endif

#pragma warning (disable:4400)
#pragma warning (disable:4564)

DEFINE_LOG_CATEGORY_STATIC(LogPS4TargetPlatform, Log, All);


/* Static initialization
 *****************************************************************************/

FCriticalSection FPS4TargetPlatform::DevicesCriticalSection;

/* FPS4TargetPlatform structors
 *****************************************************************************/

FPS4TargetPlatform::FPS4TargetPlatform()
{
	// load the final ps4 engine settings for this game
	FConfigCacheIni::LoadLocalIniFile(PS4EngineSettings, TEXT("Engine"), true, *PlatformName());

#if WITH_ENGINE
	// load up texture settings from the config file
	PS4LODSettings = nullptr; // These are registered by the device profile system.
	StaticMeshLODSettings.Initialize(PS4EngineSettings);
#endif

	// we only need these when running the editor, plus there appears to be an issue where they may not cleanup
	// correctly in game/server mode, which is causing issues duting automated tests
	if (FString(FPlatformMisc::GetEngineMode()) == TEXT("Editor"))
	{
		FString CmdExe = TEXT("../DotNet/PS4/PS4DevKitUtil");
		FString CommandLine = FString::Printf(TEXT("Monitor %d %s"), FPlatformProcess::GetCurrentProcessId(), FPlatformProcess::ExecutableName());

		MonitoredProcess = MakeShareable(new FMonitoredProcess(CmdExe, CommandLine, true));
		MonitoredProcess->SetSleepInterval(1.0f);
		MonitoredProcess->OnOutput().BindRaw(this, &FPS4TargetPlatform::MonitoredProcessCommand);
		MonitoredProcess->Launch();
	}
}

FPS4TargetPlatform::~FPS4TargetPlatform( )
{
}

/* ITargetPlatform interface
 *****************************************************************************/

bool FPS4TargetPlatform::AddDevice( const FString& DeviceNameAndInfo, bool bDefault )
{
	FString DeviceName;
	if( !FParse::Value( *DeviceNameAndInfo, TEXT( "TargetAdded=" ), DeviceName ) )
	{
		// DeviceNameAndInfo only contains the device name (isn't coming from PS4DevkitTool "TargetAdded")
		DeviceName = DeviceNameAndInfo;
	}

	FScopeLock Lock( &DevicesCriticalSection );

	FPS4TargetDevicePtr& Device = Devices.FindOrAdd( DeviceName );

	if( !Device.IsValid() )
	{
		Device = MakeShareable( new FPS4TargetDevice( *this, DeviceName, DeviceNameAndInfo ) );
		DeviceDiscoveredEvent.Broadcast( Device.ToSharedRef() );
	}

	return true;
}


void FPS4TargetPlatform::GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const
{
	FScopeLock Lock( &DevicesCriticalSection );

	OutDevices.Reset();

	for( auto Iter = Devices.CreateConstIterator(); Iter; ++Iter )
	{
		OutDevices.Add( Iter.Value() );
	}
}


ECompressionFlags FPS4TargetPlatform::GetBaseCompressionMethod( ) const
{
	return COMPRESS_ZLIB;
}


bool FPS4TargetPlatform::GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const
{
	FString GameNameLower = FString(FApp::GetProjectName()).ToLower();
	FString TmpPackagingDir = FPaths::ProjectSavedDir() / TEXT("TmpPackaging/PS4");
	FString GP4PakFilesFilename = FString::Printf(TEXT("%s/%s_pakfiles_gp4.txt"), *TmpPackagingDir, *GameNameLower);
	FArchive* GP4PakFilesFile = IFileManager::Get().CreateFileWriter(*GP4PakFilesFilename);

	if (!GP4PakFilesFile)
	{
		UE_LOG(LogPS4TargetPlatform, Error, TEXT("Failed to open output gp4 pak files list file %s"), *GP4PakFilesFilename);
		return false;
	}

	for (auto ChunkIDIt = ChunkIDsInUse.CreateConstIterator(); ChunkIDIt; ++ChunkIDIt)
	{
		FString GP4PakFilesLine = FString::Printf(
				TEXT("		<file targ_path=\"%s/content/paks/%s%d.pak\"/>\r\n"), *GameNameLower, *GameNameLower, *ChunkIDIt);
		GP4PakFilesFile->Serialize(TCHAR_TO_ANSI(*GP4PakFilesLine), GP4PakFilesLine.Len());
	}

	GP4PakFilesFile->Close();
	delete GP4PakFilesFile;

	// write out the pakfile portion of the playgo-chunks.xml file
	FString ManifestFilename = TmpPackagingDir / TEXT("playgo-chunks_pakfiles_xml.txt");
	FArchive* ManifestFile = IFileManager::Get().CreateFileWriter(*ManifestFilename);

	if (!ManifestFile)
	{
		UE_LOG(LogPS4TargetPlatform, Error, TEXT("Failed to open output manifest file %s"), *ManifestFilename);
		return false;
	}

	// print out the header and chunk info
	FString ChunkHeader = FString::Printf(
				TEXT("<\?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"\?>\r\n")
				TEXT("<psproject fmt=\"playgo-chunks\" version=\"%d\">\r\n")
				TEXT("	<volume>\r\n")
				TEXT("		<chunk_info chunk_count=\"%d\" scenario_count=\"1\">\r\n")
				TEXT(" 			<chunks supported_languages=\"en\" default_language=\"en\">\r\n"), 1000, ChunkIDsInUse.Num());		//@todo ps4: hardcoded SDK version.  Use ORBIS_SDK_VERSION from sdk_version.h?
	ManifestFile->Serialize(TCHAR_TO_ANSI(*ChunkHeader), ChunkHeader.Len());

	// print out the chunk ids
	for (auto ChunkIDIt = ChunkIDsInUse.CreateConstIterator(); ChunkIDIt; ++ChunkIDIt)
	{
		FString ChunkInfo = FString::Printf(
				TEXT("				<chunk id=\"%d\" languages=\"en\" label=\"#%d\"/>\r\n"), *ChunkIDIt, *ChunkIDIt);
		ManifestFile->Serialize(TCHAR_TO_ANSI(*ChunkInfo), ChunkInfo.Len());
	}

	// print out the first part of the chunk footer
	FString ChunkFooter = FString::Printf(
				TEXT("			</chunks>\r\n")
				TEXT("			<scenarios default_id=\"0\">\r\n")
				TEXT("				<scenario id=\"0\" type=\"sp\" initial_chunk_count=\"%d\" initial_chunk_count_disc=\"%d\">\r\n"), 1, 1);
	ManifestFile->Serialize(TCHAR_TO_ANSI(*ChunkFooter), ChunkFooter.Len());

	// print out the chunk ids in install order
	for (auto ChunkIDIt = ChunkIDsInUse.CreateConstIterator(); ChunkIDIt; ++ChunkIDIt)
	{
		FString ChunkInfo = FString::Printf(TEXT("					%d\r\n"), *ChunkIDIt);
		ManifestFile->Serialize(TCHAR_TO_ANSI(*ChunkInfo), ChunkInfo.Len());
	}

	// print out the last part of the chunk footer
	ChunkFooter = FString(
				TEXT("				</scenario>\r\n")
				TEXT("			</scenarios>\r\n")
				TEXT("		</chunk_info>\r\n")
				TEXT("	</volume>\r\n")
				TEXT("	<files>\r\n"));
	ManifestFile->Serialize(TCHAR_TO_ANSI(*ChunkFooter), ChunkFooter.Len());

	for (auto ChunkIDIt = ChunkIDsInUse.CreateConstIterator(); ChunkIDIt; ++ChunkIDIt)
	{
		FString ManifestLine = FString::Printf(
				TEXT("		<file targ_path=\"%s/content/paks/%s%d.pak\" chunks=\"%d\" />\r\n"), *GameNameLower, *GameNameLower, *ChunkIDIt, *ChunkIDIt);
		ManifestFile->Serialize(TCHAR_TO_ANSI(*ManifestLine), ManifestLine.Len());
	}

	// don't currently output the entire manifest, we patch up the remainder in the packaging script
	//ChunkFooter = FString(
	//			TEXT("	</files>\r\n")
	//			TEXT("</psproject>\r\n"));
	//ManifestFile->Serialize(TCHAR_TO_ANSI(*ChunkFooter), ChunkFooter.Len());

	ManifestFile->Close();
	delete ManifestFile;

	return true;
}


ITargetDevicePtr FPS4TargetPlatform::GetDefaultDevice( ) const
{
	FScopeLock Lock( &DevicesCriticalSection );

	return Devices.FindRef( DefaultDeviceName );
}


ITargetDevicePtr FPS4TargetPlatform::GetDevice( const FTargetDeviceId& DeviceId )
{
	if( DeviceId.GetPlatformName() == this->PlatformName() )
	{
		FScopeLock Lock( &DevicesCriticalSection );
		for( auto MapIt = Devices.CreateIterator(); MapIt; ++MapIt )
		{
			FPS4TargetDevicePtr& Device = MapIt->Value;
			if( Device->GetName() == DeviceId.GetDeviceName() )
			{
				return Device;
			}
		}
	}

	return nullptr;
}


bool FPS4TargetPlatform::IsRunningPlatform( ) const
{
	return false; // but this will never be called because this platform doesn't run the target platform framework
}


bool FPS4TargetPlatform::SupportsFeature( ETargetPlatformFeatures Feature ) const
{
	switch (Feature)
	{
	case ETargetPlatformFeatures::SdkConnectDisconnect:
	case ETargetPlatformFeatures::Packaging:
		return true;
	default:
		return TTargetPlatformBase< FPS4PlatformProperties >::SupportsFeature(Feature);
	}
}


#if WITH_ENGINE

void FPS4TargetPlatform::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_SF_PS4(TEXT("SF_PS4"));
	OutFormats.AddUnique(NAME_SF_PS4);
}


void FPS4TargetPlatform::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}


const class FStaticMeshLODSettings& FPS4TargetPlatform::GetStaticMeshLODSettings( ) const
{
	return StaticMeshLODSettings;
}


void FPS4TargetPlatform::GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const
{
	// @todo ps4: Do we support BC6H and BC7? (see last param)
	FName DefaultFormat = GetDefaultTextureFormatName(this, InTexture, PS4EngineSettings, true);

	// PS4 needs to tile the resulting texture data; so we use the normal texture format as
	// a starting point, but run it through a special texture converter for PS4 only
	FName PS4Format(*(FString(TEXT("PS4_")) + DefaultFormat.ToString()));

	OutFormats.Add(PS4Format);
}

void FPS4TargetPlatform::GetAllTextureFormats(TArray<FName>& OutFormats) const
{
	// @todo ps4: Do we support BC6H and BC7? (see last param)
	TArray<FName> AllDefaultFormats;
	GetAllDefaultTextureFormats(this, AllDefaultFormats, true);

	for ( const auto& DefaultFormat : AllDefaultFormats)
	{
		// PS4 needs to tile the resulting texture data; so we use the normal texture format as
		// a starting point, but run it through a special texture converter for PS4 only
		FName PS4Format(*(FString(TEXT("PS4_")) + DefaultFormat.ToString()));
		OutFormats.Add(PS4Format);
	}
}


const UTextureLODSettings& FPS4TargetPlatform::GetTextureLODSettings() const
{
	return *PS4LODSettings;
}


FName FPS4TargetPlatform::GetWaveFormat( const USoundWave* Wave ) const
{
	static FName NAME_AT9(TEXT("AT9"));

	return NAME_AT9;
}

void FPS4TargetPlatform::GetAllWaveFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_AT9(TEXT("AT9"));
	OutFormats.Add(NAME_AT9);
}

#endif // WITH_ENGINE


/* PS4TargetPlatform implementation
 *****************************************************************************/

void FPS4TargetPlatform::RemoveDevice( FString DeviceName )
{
	FScopeLock Lock( &DevicesCriticalSection );

	FPS4TargetDevicePtr* Device = Devices.Find( DeviceName );
	DeviceLostEvent.Broadcast( Device->ToSharedRef() );
	Devices.Remove( DeviceName );
}

void FPS4TargetPlatform::MonitoredProcessCommand( FString Command )
{
	FString StringValue;
	if( FParse::Value( *Command, TEXT( "TargetAdded=" ), StringValue ) )
	{
		AddDevice( Command, false );
	}
	else if( FParse::Value( *Command, TEXT( "TargetDeleted=" ), StringValue ) )
	{
		RemoveDevice( StringValue );
	}
	else if( FParse::Value( *Command, TEXT( "DefaultTargetChanged=" ), StringValue ) )
	{
		DefaultDeviceName = StringValue;
	}
	else if( FParse::Value( *Command, TEXT( "UpdateTarget=" ), StringValue ) )
	{
		FPS4TargetDevicePtr* Device = Devices.Find( StringValue );
		if( Device && Device->IsValid() )
		{
			(*Device)->UpdateDeviceInfoCache( Command );
		}
	}
	else
	{
		// Unknown command
	}
}
