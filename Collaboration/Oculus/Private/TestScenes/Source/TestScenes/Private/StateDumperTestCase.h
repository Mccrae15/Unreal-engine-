// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OculusXRHMDModule.h"
#include "StateDumperTestCase.generated.h"


USTRUCT(BlueprintType)
struct FAppLatencyTimings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="App Latency Struct")
	float LatencyRender;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="App Latency Struct")
	float LatencyTimewarp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="App Latency Struct")
	float LatencyPostPresent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="App Latency Struct")
	float ErrorRender;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="App Latency Struct")
	float ErrorTimewarp;

	FAppLatencyTimings() :
		LatencyRender(-1.0f),
		LatencyTimewarp(-1.0f),
		LatencyPostPresent(-1.0f),
		ErrorRender(-1.0f),
		ErrorTimewarp(-1.0f)
	{}

	FAppLatencyTimings(const ovrpAppLatencyTimings* Timings) :
		LatencyRender(Timings->LatencyRender),
		LatencyTimewarp(Timings->LatencyTimewarp),
		LatencyPostPresent(Timings->LatencyPostPresent),
		ErrorRender(Timings->ErrorRender),
		ErrorTimewarp(Timings->ErrorTimewarp)
	{}
};

USTRUCT(BlueprintType)
struct FPluginState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	FString VersionString;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	FString NativeSDKVersionString;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bIsInitialized;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bChromaticCorrectionEnabled;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool OrientationTrackingEnabled;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bPositionalTrackingSupported;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bPositionalTrackingEnabled;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bTrackingIPDEnabled;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bPowerSavingEnabled;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bHMDPresent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bUserPresent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bHeadphonesPresent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	int RecommendedMSAALevel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	FString SystemRegion;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	FString AudioOutDeviceID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	FString AudioInDeviceID;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bHasVRFocus;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bHasInputFocus;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bShouldQuit;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bShouldRecenter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	FString SystemProductName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	FAppLatencyTimings AppLatencyTimings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	float UserEyeHeight;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	float SystemBatteryLevel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	float SystemBatteryTemperature;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	int LConBatteryLevel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	int RConBatteryLevel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	int CPULevel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	int GPULevel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	int SystemVSyncCount;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	float SystemVolume;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	float UserIPD;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	FString BatteryStatus;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bTiledMultiResSupported;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	bool bGPUUtilSupported;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	float GPUUtilLevel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	TArray<float> SystemDisplayFrequenciesAvailable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plugin State Struct")
	float SystemDisplayFrequency;

	FPluginState() :
		VersionString(TEXT("Unknown")),
		NativeSDKVersionString(TEXT("Unknown")),
		bIsInitialized(false),
		bChromaticCorrectionEnabled(false),
		OrientationTrackingEnabled(false),
		bPositionalTrackingSupported(false),
		bPositionalTrackingEnabled(false),
		bTrackingIPDEnabled(false),
		bPowerSavingEnabled(false),
		bHMDPresent(false),
		bUserPresent(false),
		bHeadphonesPresent(false),
		RecommendedMSAALevel(-1),
		SystemRegion(TEXT("Unspecified")),
		AudioOutDeviceID(TEXT("Unknown")),
		AudioInDeviceID(TEXT("Unknown")),
		bHasVRFocus(false),
		bHasInputFocus(false),
		bShouldQuit(false),
		bShouldRecenter(false),
		SystemProductName(TEXT("Unknown")),
		AppLatencyTimings(FAppLatencyTimings()),
		UserEyeHeight(-1.0f),
		SystemBatteryLevel(-1.0f),
		SystemBatteryTemperature(-1.0f),
		LConBatteryLevel(0),
		RConBatteryLevel(0),
		CPULevel(-1),
		GPULevel(-1),
		SystemVSyncCount(-1),
		SystemVolume(-1.0f),
		UserIPD(-1.0f),
		BatteryStatus(TEXT("Unknown")),
		bTiledMultiResSupported(false),
		bGPUUtilSupported(false),
		GPUUtilLevel(-1.0f),
		SystemDisplayFrequenciesAvailable(TArray<float>()),
		SystemDisplayFrequency(-1.0f)
	{}
};


UCLASS(BlueprintType, Blueprintable, DefaultToInstanced)
class AStateDumperTestCase : public AActor
{
	GENERATED_BODY()

public:
	AStateDumperTestCase();

	UFUNCTION(BlueprintCallable)
		FPluginState DumpState();

	UFUNCTION(BlueprintCallable)
		bool SerializeResults();

private:
	UPROPERTY()
		FPluginState State;

	bool GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)> FromFunc);
	int GetIntFromFunc(TFunctionRef<ovrpResult(int*)> FromFunc);
	float GetFloatFromFunc(TFunctionRef<ovrpResult(float*)> FromFunc);
	FString GetFStringFromFunc(TFunctionRef<ovrpResult(char const**)> FromFunc);
	int GetProcessorPerformanceLevelFromFunc(TFunctionRef<ovrpResult(ovrpProcessorPerformanceLevel*)> FromFunc);

#if PLATFORM_ANDROID
	FString GetBatteryStatusString(FAndroidMisc::EBatteryState StatusEnum);
#else
	FString GetBatteryStatusString(ovrpBatteryStatus StatusEnum);
#endif
	FString GetSystemRegionString(ovrpSystemRegion SystemRegion);
	FString GetSystemHeadsetString(ovrpSystemHeadset systemHeadset);
};
