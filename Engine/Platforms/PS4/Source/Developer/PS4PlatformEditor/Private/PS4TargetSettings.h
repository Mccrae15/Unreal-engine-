// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PS4TargetSettings.h: Declares the UPS4TargetSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AudioCompressionSettings.h"
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
 * Enumerates the deploy modes.
 */
UENUM()
enum class EPS4DeployMode : uint8
{
	/** Copy the staged build to the device */
	DeployToDevice UMETA(DisplayName = "Deploy to device"),

	/** Launch from the host's working directory in staged builds */
	RunFromHost UMETA(DisplayName = "Run from host")
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
	* Enables support for the PS4 Memory Analyzer tool.
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime)
	bool bEnableMemoryAnalyzer;

	/**
	* Enables the Address Sanitizer (ASan).
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime)
	bool bEnableAddressSanitizer;

	/**
	* Enables the Undefined Behavior Sanitizer (UBSan).
	*/
	UPROPERTY(EditAnywhere, config, Category=CompileTime)
	bool bEnableUndefinedBehaviorSanitizer;

	/**
	* Enables shader performance analysis
	*/
	UPROPERTY(EditAnywhere, config, Category = CompileTime)
	bool bEnableShaderPerfAnalysis;

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

	/** Does the project use the download0 directory.  Needs setup in param.sfo */
	UPROPERTY(EditAnywhere, config, Category = Storage)
	bool bUsesDownloadZero;

	/** The mode to use when deploying and launching */
	UPROPERTY(EditAnywhere, config, Category = Deploy)
	EPS4DeployMode DeployMode;

	/** Which of the currently enabled spatialization plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString SpatializationPlugin;

	/** Which of the currently enabled reverb plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString ReverbPlugin;

	/** Which of the currently enabled occlusion plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString OcclusionPlugin;

	/** Audio Cook Settings */

	/** Various overrides for how this platform should handle compression and decompression */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio")
	FPlatformRuntimeAudioCompressionOverrides CompressionOverrides;

	/** When this is enabled, Actual compressed data will be separated from the USoundWave, and loaded into a cache. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides", meta = (DisplayName = "Use Stream Caching (Experimental)"))
	bool bUseAudioStreamCaching;

	/** This determines the max amount of memory that should be used for the cache at any given time. If set low (<= 8 MB), it lowers the size of individual chunks of audio during cook. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides|Stream Caching", meta = (DisplayName = "Max Cache Size (KB)"))
	int32 CacheSizeKB;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides")
	bool bResampleForDevice;

	/** Quality Level to COOK SoundCues at (if set, all other levels will be stripped by the cooker). */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides", meta = (DisplayName = "Sound Cue Cook Quality"))
	int32 SoundCueCookQualityIndex = INDEX_NONE;

	/** Mapping of which sample rates are used for each sample rate quality for a specific platform. */

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides|ResamplingQuality", meta = (DisplayName = "Max"))
	float MaxSampleRate;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides|ResamplingQuality", meta = (DisplayName = "High"))
	float HighSampleRate;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides|ResamplingQuality", meta = (DisplayName = "Medium"))
	float MedSampleRate;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides|ResamplingQuality", meta = (DisplayName = "Low"))
	float LowSampleRate;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides|ResamplingQuality", meta = (DisplayName = "Min"))
	float MinSampleRate;

	/** Scales all compression qualities when cooking to this platform. For example, 0.5 will halve all compression qualities, and 1.0 will leave them unchanged. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides")
	float CompressionQualityModifier;

	/** When set to anything beyond 0, this will ensure any SoundWaves longer than this value, in seconds, to stream directly off of the disk. */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "Audio|CookOverrides", meta = (DisplayName = "Stream All Soundwaves Longer Than: "))
	float AutoStreamingThreshold;
};
