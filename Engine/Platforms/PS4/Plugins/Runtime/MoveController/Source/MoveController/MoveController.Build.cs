// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class MoveController : ModuleRules
{
	public MoveController(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"ApplicationCore",
			"Engine",
            "InputCore",
            "InputDevice",
			"HeadMountedDisplay",
			"PS4Tracker"
		});

		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PublicSystemLibraries.Add("SceMove_stub_weak");
			PublicDefinitions.Add("HAS_MORPHEUS_HMD_SDK=1");
		}
		else
		{
			string HMDSdkDir = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS", "Tools", "HMD Server");
			bool bHasHMDSdk = File.Exists(Path.Combine(HMDSdkDir, "hmd_client.h"));

			if (bHasHMDSdk)
			{
				PublicDefinitions.Add("HAS_MORPHEUS_HMD_SDK=1");
			}
			else
			{
				PublicDefinitions.Add("HAS_MORPHEUS_HMD_SDK=0");
			}
		}
	}
}
