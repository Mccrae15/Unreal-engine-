// Fill out your copyright notice in the Description page of Project Settings.

#include "StateDumperTestCase.h"
#include "OculusXRHMD.h"
#include "JsonObjectConverter.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "OculusXRFunctionLibrary.h"

AStateDumperTestCase::AStateDumperTestCase()
{ }

FPluginState AStateDumperTestCase::DumpState()
{
	// If this isn't a supported platform, just return immediately.
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	// Get a pointer to the HMD so I can use available functions to get some of the plugin state data
	OculusXRHMD::FOculusXRHMD* HMD;
	
	// Have to duplicate code from the OculusFunctionLibrary because the damned "GetOculusHMD" function is protected and I can't access it.
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		if (GEngine->XRSystem->GetSystemName() == TEXT("OculusXRHMD"))
		{
			HMD = static_cast<OculusXRHMD::FOculusXRHMD*>(GEngine->XRSystem.Get());
			ovrpBool OutBool;

			State.VersionString = HMD->GetVersionString();
			State.NativeSDKVersionString = GetFStringFromFunc(TFunctionRef<ovrpResult(char const**)>(FOculusXRHMDModule::GetPluginWrapper().GetNativeSDKVersion2));
			State.bIsInitialized = FOculusXRHMDModule::GetPluginWrapper().GetInitialized() != ovrpBool_False;
			State.bChromaticCorrectionEnabled = HMD->IsChromaAbCorrectionEnabled();
			State.OrientationTrackingEnabled = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetTrackingOrientationEnabled2));
			State.bPositionalTrackingSupported = HMD->DoesSupportPositionalTracking();
			State.bPositionalTrackingEnabled = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetTrackingPositionEnabled2));
			State.bTrackingIPDEnabled = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetTrackingIPDEnabled2));
			State.bPowerSavingEnabled = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetSystemPowerSavingMode2));

			OutBool = false;
			FOculusXRHMDModule::GetPluginWrapper().GetNodePositionTracked2(ovrpNode_EyeCenter, &OutBool);
			State.bHMDPresent = OutBool == ovrpBool_True;

			State.bUserPresent = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetUserPresent2));
#if PLATFORM_ANDROID
			State.bHeadphonesPresent = FAndroidMisc::AreHeadPhonesPluggedIn();
#else
			//State.bHeadphonesPresent = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetSystemHeadphonesPresent2));
#endif
			State.RecommendedMSAALevel = GetIntFromFunc(TFunctionRef<ovrpResult(int*)>(FOculusXRHMDModule::GetPluginWrapper().GetSystemRecommendedMSAALevel2));
			ovrpSystemRegion SystemRegion = ovrpSystemRegion_Unspecified;
			if (OVRP_SUCCESS(FOculusXRHMDModule::GetPluginWrapper().GetSystemRegion2(&SystemRegion)))
			{
				State.SystemRegion = GetSystemRegionString(SystemRegion);
			}
			const TCHAR* AudioOutDeviceID;
			const TCHAR* AudioInDeviceID;
			if (OVRP_SUCCESS(FOculusXRHMDModule::GetPluginWrapper().GetAudioOutDeviceId2((const void**)&AudioOutDeviceID)) && AudioOutDeviceID)
			{
				State.AudioOutDeviceID = FString(AudioOutDeviceID);
			}
			if (OVRP_SUCCESS(FOculusXRHMDModule::GetPluginWrapper().GetAudioInDeviceId2((const void**)&AudioInDeviceID)) && AudioInDeviceID)
			{
				State.AudioInDeviceID = FString(AudioInDeviceID);
			}
			State.bHasVRFocus = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetAppHasVrFocus2));
			State.bHasInputFocus = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetAppHasInputFocus));
			State.bShouldQuit = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetAppShouldQuit2));
			State.bShouldRecenter = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetAppShouldRecenter2));
			ovrpSystemHeadset headset;
			if (OVRP_FAILURE(FOculusXRHMDModule::GetPluginWrapper().GetSystemHeadsetType2(&headset)))
			{
				headset = ovrpSystemHeadset_None;
			}
			State.SystemProductName = GetSystemHeadsetString(headset);

			ovrpAppLatencyTimings AppLatencyTimings;
			if (OVRP_SUCCESS(FOculusXRHMDModule::GetPluginWrapper().GetAppLatencyTimings2(&AppLatencyTimings)))
			{
				State.AppLatencyTimings = FAppLatencyTimings(&AppLatencyTimings);
			}
			State.UserEyeHeight = GetFloatFromFunc(TFunctionRef<ovrpResult(float*)>(FOculusXRHMDModule::GetPluginWrapper().GetUserEyeHeight2));
			State.CPULevel = GetProcessorPerformanceLevelFromFunc(TFunctionRef<ovrpResult(ovrpProcessorPerformanceLevel*)>(FOculusXRHMDModule::GetPluginWrapper().GetSuggestedCpuPerformanceLevel));
			State.GPULevel = GetProcessorPerformanceLevelFromFunc(TFunctionRef<ovrpResult(ovrpProcessorPerformanceLevel*)>(FOculusXRHMDModule::GetPluginWrapper().GetSuggestedGpuPerformanceLevel));
			State.SystemVSyncCount = GetIntFromFunc(TFunctionRef<ovrpResult(int*)>(FOculusXRHMDModule::GetPluginWrapper().GetSystemVSyncCount2));
			State.UserIPD = GetFloatFromFunc(TFunctionRef<ovrpResult(float*)>(FOculusXRHMDModule::GetPluginWrapper().GetUserIPD2));

#if PLATFORM_ANDROID
			FAndroidMisc::FBatteryState BatteryState = FAndroidMisc::GetBatteryState();
			State.SystemBatteryLevel = (float)BatteryState.Level;
			State.SystemBatteryTemperature = BatteryState.Temperature;
			State.BatteryStatus = GetBatteryStatusString(BatteryState.State);
			State.SystemVolume = (float)FAndroidMisc::GetDeviceVolume();
#else
			State.SystemBatteryLevel = 0.0f;
			State.SystemBatteryTemperature = 0.0f;
			State.BatteryStatus = GetBatteryStatusString(ovrpBatteryStatus_Unknown);
			//State.SystemVolume = GetFloatFromFunc(TFunctionRef<ovrpResult(float*)>(FOculusXRHMDModule::GetPluginWrapper().GetSystemVolume2));;
#endif
			ovrpControllerState4 ControllerState;
			FOculusXRHMDModule::GetPluginWrapper().GetControllerState4((ovrpController)(ovrpController_LTrackedRemote | ovrpController_RTrackedRemote | ovrpController_Touch), &ControllerState);
			State.LConBatteryLevel = ControllerState.BatteryPercentRemaining[0];
			State.RConBatteryLevel = ControllerState.BatteryPercentRemaining[1];

			State.bTiledMultiResSupported = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetTiledMultiResSupported));
			State.bGPUUtilSupported = GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)>(FOculusXRHMDModule::GetPluginWrapper().GetGPUUtilSupported));
			State.GPUUtilLevel = GetFloatFromFunc(TFunctionRef<ovrpResult(float*)>(FOculusXRHMDModule::GetPluginWrapper().GetGPUUtilLevel));
			// Get all available display frequencies
			int NumFrequencies = 0;
			
			if (OVRP_SUCCESS(FOculusXRHMDModule::GetPluginWrapper().GetSystemDisplayAvailableFrequencies(nullptr, &NumFrequencies)))
			{
				State.SystemDisplayFrequenciesAvailable.SetNum(NumFrequencies);
				FOculusXRHMDModule::GetPluginWrapper().GetSystemDisplayAvailableFrequencies(State.SystemDisplayFrequenciesAvailable.GetData(), &NumFrequencies);
			}
			// End display frequency section
			State.SystemDisplayFrequency = GetFloatFromFunc(TFunctionRef<ovrpResult(float*)>(FOculusXRHMDModule::GetPluginWrapper().GetSystemDisplayFrequency2));

			FString OutJsonString;
			if (!FJsonObjectConverter::UStructToJsonObjectString<FPluginState>(State, OutJsonString))
			{
				OutJsonString = TEXT("Unable to serialize OVR Plugin state.");
				// UE_LOG(LogOculusTestCase, Error, TEXT("%s"), *OutJsonString);
			}
		}	
	}
#endif
	return State;
}

bool AStateDumperTestCase::SerializeResults()
{
	FString OutString;
	if (FJsonObjectConverter::UStructToJsonObjectString<FPluginState>(State, OutString))
	{
		FString ThisLogPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("StateDumperResults.txt"));
		return FFileHelper::SaveStringToFile(OutString, *ThisLogPath);
	}
	return false;
}

bool AStateDumperTestCase::GetBoolFromFunc(TFunctionRef<ovrpResult(ovrpBool*)> FromFunc)
{
	ovrpBool OutBool;
	if (OVRP_SUCCESS(FromFunc(&OutBool)))
	{
		return OutBool == ovrpBool_True;
	}
	return false;
}

int AStateDumperTestCase::GetIntFromFunc(TFunctionRef<ovrpResult(int*)> FromFunc)
{
	int OutInt;
	if (OVRP_SUCCESS(FromFunc(&OutInt)))
	{
		return OutInt;
	}
	return -1;
}

float AStateDumperTestCase::GetFloatFromFunc(TFunctionRef<ovrpResult(float*)> FromFunc)
{
	float OutFloat;
	if (OVRP_SUCCESS(FromFunc(&OutFloat)))
	{
		return OutFloat;
	}
	return -1.0f;
}

FString AStateDumperTestCase::GetFStringFromFunc(TFunctionRef<ovrpResult(char const**)> FromFunc)
{
	char const* OutString;
	if (OVRP_SUCCESS(FromFunc(&OutString)))
	{
		return FString(UTF8_TO_TCHAR(OutString));
	}
	return FString(TEXT("Failed to retrieve value from plugin."));
}

int AStateDumperTestCase::GetProcessorPerformanceLevelFromFunc(TFunctionRef<ovrpResult(ovrpProcessorPerformanceLevel*)> FromFunc)
{
	ovrpProcessorPerformanceLevel OutLevel;
	if (OVRP_SUCCESS(FromFunc(&OutLevel)))
	{
		return (int)OutLevel;
	}
	return -1;
}

#if PLATFORM_ANDROID
FString AStateDumperTestCase::GetBatteryStatusString(FAndroidMisc::EBatteryState StatusEnum)
{
	if (StatusEnum == FAndroidMisc::EBatteryState::BATTERY_STATE_CHARGING)
	{
		return FString(TEXT("Charging"));
	}
	else if (StatusEnum == FAndroidMisc::EBatteryState::BATTERY_STATE_DISCHARGING)
	{
		return FString(TEXT("Discharging"));
	}
	else if (StatusEnum == FAndroidMisc::EBatteryState::BATTERY_STATE_FULL)
	{
		return FString(TEXT("Full"));
	}
	else if (StatusEnum == FAndroidMisc::EBatteryState::BATTERY_STATE_NOT_CHARGING)
	{
		return FString(TEXT("Not Charging"));
	}
	return FString(TEXT("Unknown"));
}
#else
FString AStateDumperTestCase::GetBatteryStatusString(ovrpBatteryStatus StatusEnum)
{
	if (StatusEnum == ovrpBatteryStatus_Charging)
	{
		return FString(TEXT("Charging"));
	}
	else if (StatusEnum == ovrpBatteryStatus_Discharging)
	{
		return FString(TEXT("Discharging"));
	}
	else if (StatusEnum == ovrpBatteryStatus_Full)
	{
		return FString(TEXT("Full"));
	}
	else if (StatusEnum == ovrpBatteryStatus_NotCharging)
	{
		return FString(TEXT("Not Charging"));
	}
	return FString(TEXT("Unknown"));
}
#endif

FString AStateDumperTestCase::GetSystemRegionString(ovrpSystemRegion SystemRegion)
{
	if (SystemRegion == ovrpSystemRegion_Japan)
	{
		return FString(TEXT("Japan"));
	}
	else if (SystemRegion == ovrpSystemRegion_China)
	{
		return FString(TEXT("China"));
	}
	return FString(TEXT("Unspecified"));
}

FString AStateDumperTestCase::GetSystemHeadsetString(ovrpSystemHeadset systemHeadset)
{
	if (systemHeadset == ovrpSystemHeadset_None)
	{
		return FString(TEXT("None"));
	}
	else if (systemHeadset >= ovrpSystemHeadset_GearVR_R320 && systemHeadset <= ovrpSystemHeadset_GearVR_R325)
	{
		return FString(TEXT("GearVR"));
	}
	else if (systemHeadset == ovrpSystemHeadset_Oculus_Go)
	{
		return FString(TEXT("Go"));
	}
	else if (systemHeadset == ovrpSystemHeadset_Oculus_Quest)
	{
		return FString(TEXT("Quest"));
	}
	else if (systemHeadset == ovrpSystemHeadset_Oculus_Quest_2)
	{
		return FString(TEXT("Quest 2"));
	}
	else if (systemHeadset >= ovrpSystemHeadset_Rift_DK1 && systemHeadset <= ovrpSystemHeadset_Rift_CB)
	{
		return FString(TEXT("Rift"));
	}
	else if (systemHeadset == ovrpSystemHeadset_Rift_S)
	{
		return FString(TEXT("Rift S"));
	}
	else if (systemHeadset == ovrpSystemHeadset_Oculus_Link_Quest)
	{
		return FString(TEXT("Quest (Link)"));
	}
	else
	{
		return FString(TEXT("Unknown"));
	}
}