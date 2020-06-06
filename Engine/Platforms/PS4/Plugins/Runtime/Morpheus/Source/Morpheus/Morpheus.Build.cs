// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

[SupportedPlatforms("PS4", "Win64")]
public class Morpheus : ModuleRules
{
	public Morpheus(ReadOnlyTargetRules Target) : base(Target)
	{
		AddEngineThirdPartyPrivateStaticDependencies(Target, new string[] { "LibScePad" });

		PrivateIncludePaths.AddRange(new string[]
		{
			// @todo: post process plugins need a clean interface to the renderer
			"../../../../../../Source/Runtime/Renderer/Private"
		});

		PrivateIncludePathModuleNames.AddRange(new string[]
		{
				"ApplicationCore_Sony",
				"ApplicationCore_PS4"
		});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
					"Core",
					"CoreUObject",
					"Engine",
					"EngineSettings",
					"Renderer",
					"RenderCore",
					"HeadMountedDisplay",
					"RHI",
					"Slate",
					"SlateCore",
					"InputDevice",
					"Projects",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
						"PS4RHI",
						"PS4Tracker"
				}
			);

			// add required morpheus libs
			PublicSystemLibraries.Add("SceHmd_stub_weak");
			PublicSystemLibraries.Add("SceAudio3d_stub_weak");
			PublicSystemLibraries.Add("SceHmdSetupDialog_stub_weak");
			PublicSystemLibraries.Add("SceSocialScreenDialog_stub_weak");

			PublicDefinitions.Add("MORPHEUS_ENGINE_DISTORTION=0");
			PublicDefinitions.Add("HAS_MORPHEUS_HMD_SDK=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
						"PS4Tracker",
						"ApplicationCore"
				}
			);

			String PadLibLocation;
			bool bFoundPadLib = LibScePad.GetPadLibLocation(Target.WindowsPlatform.Compiler, out PadLibLocation);
			PublicDefinitions.Add("DUALSHOCK4_SUPPORT=" + (bFoundPadLib ? "1" : "0"));

			// On PS4 the SDK now handles distortion correction. On PC we will still have to handle it manually.
			PublicDefinitions.Add("MORPHEUS_ENGINE_DISTORTION=1");

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
