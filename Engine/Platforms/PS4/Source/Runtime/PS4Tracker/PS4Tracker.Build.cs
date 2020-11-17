// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

[SupportedPlatforms("PS4", "Win64")]
public class PS4Tracker : ModuleRules
{
	public PS4Tracker(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "PS4";

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"HeadMountedDisplay"
			}
		);

		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PrivateDependencyModuleNames.Add("PS4RHI");
			PublicSystemLibraries.AddRange(new string[] { "SceCamera_stub_weak", "SceVrTracker_stub_weak" });
			PublicSystemLibraries.Add("SceCommonDialog_stub_weak");
			PublicSystemLibraries.Add("SceMsgDialog_stub_weak");
			PublicSystemLibraries.Add("SceHmd_stub_weak");
			PublicDefinitions.Add("HAS_MORPHEUS_HMD_SDK=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string HMDSdkDir = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS", "Tools", "HMD Server");
			bool bHasHMDSdk = File.Exists(Path.Combine(HMDSdkDir, "hmd_client.h"));

			if (bHasHMDSdk)
			{
				PublicDefinitions.Add("HAS_MORPHEUS_HMD_SDK=1");
				PublicSystemIncludePaths.Add(HMDSdkDir);

				PublicDelayLoadDLLs.Add("hmd_client.dll");
				PublicAdditionalLibraries.Add(Path.Combine(HMDSdkDir, "x86_64", "hmd_client.lib"));
			}
			else
			{
				PublicDefinitions.Add("HAS_MORPHEUS_HMD_SDK=0");
			}
		}
		else
		{
			PublicDefinitions.Add("HAS_MORPHEUS_HMD_SDK=0");
		}
	}
}
