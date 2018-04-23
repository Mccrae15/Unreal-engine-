// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PS4TargetPlatform.h: Declares the FPS4TargetPlatform class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "TargetPlatformBase.h"
#include "PS4/PS4Properties.h"
#include "PS4TargetDevice.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MonitoredProcess.h"
#include "Containers/Ticker.h"

#if WITH_ENGINE
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

/**
 * Implements the PS4 target platform.
 */
class FPS4TargetPlatform
	: public TTargetPlatformBase<FPS4PlatformProperties>
{	

public:

	/**
	 * Default constructor.
	 */
	FPS4TargetPlatform( );

	/**
	 * Destructor.
	 */
	~FPS4TargetPlatform( );

public:

	//~ Begin ITargetPlatform Interface

	virtual bool AddDevice( const FString& DeviceName, bool bDefault ) override;

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override;

	virtual ECompressionFlags GetBaseCompressionMethod( ) const override;

	virtual bool GenerateStreamingInstallManifest( const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse ) const override;

	virtual ITargetDevicePtr GetDefaultDevice( ) const override;

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override;

	virtual bool IsRunningPlatform( ) const override;

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override;

#if WITH_ENGINE
	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const override;

	virtual void GetTextureFormats( const UTexture* InTexture, TArray<FName>& OutFormats ) const override;

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;

	virtual const UTextureLODSettings& GetTextureLODSettings() const override;

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		PS4LODSettings = InTextureLODSettings;
	}

	// get the wave format name for this specific soundwave
	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override;

	// this should be all the possible return values from the function GetWaveFormat
	virtual void GetAllWaveFormats( TArray<FName>& OutFormats ) const override;
#endif // WITH_ENGINE

	DECLARE_DERIVED_EVENT(FPS4TargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(FPS4TargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	//~ End ITargetPlatform Interface

	void QueryConnectedDevices();

private:
	
	// Holds the PS4 engine settings.
	FConfigFile PS4EngineSettings;	

#if WITH_ENGINE
	// Holds the cached target LOD settings.
	const UTextureLODSettings* PS4LODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;
#endif

private:

	// Holds a map of valid devices.
	TMap<FString, FPS4TargetDevicePtr> Devices;

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;

	// Holds a delegate to be invoked when the widget ticks.
	FTickerDelegate TickDelegate;

	// The name of the default device
	FString DefaultDeviceName;

	void RemoveDevice( FString DeviceName );

	
	// Holds a critical section for locking access to the collection of devices.
	static FCriticalSection DevicesCriticalSection;

	// The spawned DevKitTool process that is monitored for output
	TSharedPtr<FMonitoredProcess> MonitoredProcess;

	// Handle commands coming back from the spawned DevKitTool process
	void MonitoredProcessCommand( FString InString );

};
