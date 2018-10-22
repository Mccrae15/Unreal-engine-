// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ITargetDevice.h"
#include "Interfaces/ITargetPlatform.h"


/**
 * Type definition for shared pointers to instances of FPS4TargetDevice.
 */
typedef TSharedPtr<class FPS4TargetDevice, ESPMode::ThreadSafe> FPS4TargetDevicePtr;

/**
 * Type definition for shared references to instances of FPS4TargetDevice.
 */
typedef TSharedRef<class FPS4TargetDevice, ESPMode::ThreadSafe> FPS4TargetDeviceRef;


/**
 * Implements a PS4 target device.
 */
class FPS4TargetDevice
	: public ITargetDevice
{
public:

	/**
	 * Creates and initializes a new PS4 target device.
	 *
	 * @param InTargetPlatform - The target platform.
	 * @param InName - The device name.
	 * @param InDeviceInfo - The device info (power status, connection status etc).
	 */
	FPS4TargetDevice( const ITargetPlatform& InTargetPlatform, const FString& InName, const FString& InDeviceInfo  );

	/**
	 * Destructor.
	 */
	~FPS4TargetDevice( )
	{
	}

public:

	//~ Begin ITargetDevice Interface

	virtual bool Connect( ) override;

	virtual bool Deploy( const FString& SourceFolder, FString& OutAppId ) override;

	virtual void Disconnect( ) override;

	virtual ETargetDeviceTypes GetDeviceType( ) const override
	{
		return ETargetDeviceTypes::Console;
	}

	virtual FTargetDeviceId GetId( ) const override
	{
		return CachedId;
	}

	virtual FString GetName( ) const override
	{
		return CachedName;
	}

	virtual FString GetOperatingSystemName( ) override
	{
		return CachedOSName;
	}

	virtual int32 GetProcessSnapshot( TArray<FTargetDeviceProcessInfo>& OutProcessInfos ) override;

	virtual const class ITargetPlatform& GetTargetPlatform( ) const override
	{
		return TargetPlatform;
	}

	virtual bool GetUserCredentials( FString& OutUserName, FString& OutUserPassword ) override
	{
		return false;
	}

	virtual bool IsConnected( )
	{
		return CachedConnectionState == EConnectionState::ConnectionConnected;
	}

	virtual bool IsDefault( ) const override
	{
		return CachedDefault;
	}

	virtual bool Launch( const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId ) override;

	virtual bool PowerOff( bool Force ) override;

	virtual bool PowerOn( ) override;

	virtual bool Reboot( bool bReconnect = false ) override;

	virtual bool Run( const FString& ExecutablePath, const FString& Params, uint32* OutProcessId ) override;

	virtual void SetUserCredentials( const FString& UserName, const FString& UserPassword ) override { }

	virtual bool SupportsFeature( ETargetDeviceFeatures Feature ) const override;

	virtual bool SupportsSdkVersion( const FString& VersionString ) const override;

	virtual bool TerminateProcess( const int64 ProcessId ) override;

	//~ End ITargetDevice Interface

	/** 
	 * Update the cached info for this device
	 *
	 */
	void UpdateDeviceInfoCache( const FString& DeviceInfo );

protected:

	/** 
	 * Get the info for this device and update the cache
	 *
	 */
	void GetDeviceInfoAndUpdate();

	/**
	 * Finds the provided executable
	 *
	 * @param InExecutablePath - The likely location for the executable
	 * @param InOutExecutable - The resolved executable
	 */
	bool FindExecutable(const FString& InExecutablePath, FString& InOutExecutable);

	void LogMessage(const TCHAR* Message);
	void ErrorMessage(const TCHAR* Message);

	/** 
	 * Gets the proper working dir so the ps4 can find trophy and npdata files properly.  Will be the directory with sce_sys for the project
	 *
	 */
	FString GetWorkingDir( FString FullPathStr, FString ParamsStr );

	/** 
	 * Execute an orbis command such as connect, reboot, power on etc, by shelling out to orbis-ctrl.exe
	 *
	 */
	void ExecOrbisCommand( FString Params );

private:
	enum EPowerStatus
	{
		PowerStatusUnknown,
		PowerStatusOn
	};

	enum EConnectionState
	{
		ConnectionAvailable,
		ConnectionConnected,
		ConnectionInUse
	};

	// Cached connection status.
	EConnectionState CachedConnectionState;

	// Cached default flag.
	bool CachedDefault;

	// cache the Host Name.
	FString CachedHostName;

	// Cached device identifier.
	FTargetDeviceId CachedId;

	// Cached host name.
	FString CachedName;

	// Cached operating system name.
	FString CachedOSName;

	// Cached mapped drive letter
	FString CachedDriveLetter;

	// Cached power status.
	EPowerStatus CachedPowerStatus;

	// Holds a reference to the device's target platform.
	const ITargetPlatform& TargetPlatform;
};
