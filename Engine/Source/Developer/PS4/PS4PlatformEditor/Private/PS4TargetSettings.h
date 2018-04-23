// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PS4TargetSettings.h: Declares the UPS4TargetSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PS4TargetSettings.generated.h"

/**
 * Enumerates the storage type.
 */
UENUM()
enum EPS4ProjectPackagingStorageType
{
	/** Digital and BluRay, Max 25 GB. */
	PPST_BD25 UMETA(DisplayName="BD25"),

	/** Digital and BluRay, Max 50 GB. */
	PPST_BD50 UMETA(DisplayName="BD50"),

	/** Digital only, Max 25 GB. */
	PPST_Digital25 UMETA(DisplayName="Digital25"),

	/** Digital only, Max 50 GB. */
	PPST_Digital50 UMETA(DisplayName = "Digital50")
};

/**
 * Enumerates the app type.
 */
UENUM()
enum EPS4ProjectPackagingAppType
{
	/** Paid Standalone Full App */
	PPAT_Full UMETA(DisplayName="Full"),

	/** Upgradeable App */
	PPAT_Upgradeable UMETA(DisplayName="Upgradeable"),

	/** Demo App */
	PPAT_Demo UMETA(DisplayName="Demo"),

	/** Freemium App */
	PPAT_Freemium UMETA(DisplayName = "Freemium")
};

/**
 * Implements the settings for the PS4 target platform.
 */
UCLASS(config=Engine, defaultconfig)
class PS4PLATFORMEDITOR_API UPS4TargetSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()


	// Use PS4 .ini files in the editor
	virtual const TCHAR* GetConfigOverridePlatform() const override
	{
		return TEXT("PS4");
	}

	/**
	* To emit razor CPU events when using 'stat namedevents'
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime)
	bool bEnableRazorCPUEvents;

	/**
	* Register shaders with the standalone GPU debugger.
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime)
	bool bEnableGPUDebugger;

	/**
	* Enable Sulpha host side audio debugging.
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime)
	bool bEnableSulphaDebugger;

	/**
	* Enable UE4 LCUE validation.  Does not work with SDK LCUE. To use debug SDK LCUE you must compile GNMX in debug mode and link in debug library in UEBuildPS4.cs.
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime)
	bool bEnableGnmLCUEDebug;

	/**
	* Enable use of Unsafe Command Buffers. Checks for remaining buffer space before each draw call rather than before each Gnm command for improved CPU performance. Does not work with SDK LCUE.
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime)
	bool bEnableGnmUnsafeCommandBuffers;

	/**
	* Enable use of the new virtual memory pooling PS4 memory system. This replaces the fixed sized garlic/onion/cpu allocators and the GPU defrag allocator.
	*/
	UPROPERTY(EditAnywhere, config, Category = CompileTime)
	bool bEnableNewMemorySystem;

	/**
	* Enable use of the experimental GPU defragger.  The 'r.streaming.poolsize' becomes a single large allocation managed by the defragger.  Texture, vertex/index buffers, and temp blocks are allocated from this pool first.
	*/
	UPROPERTY(EditAnywhere, config, Category = CompileTime)
	bool bEnableGPUDefragger;

	/**
	* Unsafe Command Buffer reserve size. Reserve area at the end of the command buffer to prevent buffer overflow.
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime, meta=(ClampMin=8192, NoSpinbox))
	uint32 GnmUnsafeCommandBufferReserveSizeInBytes;

	/**
	* Enable Link Time Optimization for Test and Shipping configurations.  Link times are drastically increased.
	* See: https://ps4.scedev.net/resources/documents/SDK/2.500/Link_Time_Optimization-Overview/0001.html for details.
	*/
	UPROPERTY(EditAnywhere, config, Category = CompileTime)
	bool bEnableLTOPerfBuilds;

	/**
	* Enable Link Time Optimization for Development configuration.  Link times are drastically increased.
	* See: https://ps4.scedev.net/resources/documents/SDK/2.500/Link_Time_Optimization-Overview/0001.html for details.
	*/
	UPROPERTY(EditAnywhere, config, Category = CompileTime)
	bool bEnableLTODevBuilds;

	/**
	* Enable CompanionApp support.
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime)
	bool bEnableCompanionApp;


	/**
	* Move the audio rendering and stats to the 7th core when available
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime)
	bool bUse7thCore;

	/** TitleID */
	UPROPERTY(EditAnywhere, config, Category = Packaging, meta = (ConfigHierarchyEditable) )
	FString TitleID;

	/** TitlePasscode */
	UPROPERTY( EditAnywhere, config, Category = Packaging, meta = (ConfigHierarchyEditable) )
	FString TitlePasscode;
	
	/** StorageType */
	UPROPERTY( config, EditAnywhere, Category = Packaging )
	TEnumAsByte<EPS4ProjectPackagingStorageType> StorageType;

	/** AppType */
	UPROPERTY( config, EditAnywhere, Category = Packaging )
	TEnumAsByte<EPS4ProjectPackagingAppType> AppType;

	/** Build ISO Image file */
	UPROPERTY( EditAnywhere, config, Category = Packaging )
	bool BuildIsoImage;

	/** Move the package file to the outer edge of the BluRay improving read speeds but increasing file sizes */
	UPROPERTY( EditAnywhere, config, Category = Packaging )
	bool MoveFilesToOuterEdge;


	/** Generate a patch which only contains changed files, may want to disable if you are using the ps4 binary diffing patching (this creates a patch which contains all the objects */
	UPROPERTY(EditAnywhere, config, Category = Packaging)
	bool GenerateDiffPakPatch;

	/** Which of the currently enabled spatialization plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString SpatializationPlugin;

	/** Which of the currently enabled reverb plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString ReverbPlugin;

	/** Which of the currently enabled occlusion plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString OcclusionPlugin;
};
