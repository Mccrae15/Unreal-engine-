// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class Morpheus : ModuleRules
	{
		public Morpheus(ReadOnlyTargetRules Target) : base(Target)
		{

			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[] { "LibScePad" });

			PrivateIncludePaths.AddRange(
				new string[] 
				{
					"OculusRift/Private",
					// ... add other private include paths required here ...
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"EngineSettings",
					"Renderer",
					"ShaderCore",
					"RenderCore",
					"HeadMountedDisplay",
					"RHI",
					"Slate",
					"SlateCore",
					"UtilityShaders",
					"InputDevice",
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
				PublicAdditionalLibraries.Add("SceHmd_stub_weak");
				PublicAdditionalLibraries.Add("SceAudio3d_stub_weak"); 
				PublicAdditionalLibraries.Add("SceHmdSetupDialog_stub_weak");
				PublicAdditionalLibraries.Add("SceSocialScreenDialog_stub_weak");
			}
			else if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
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
			}			
		}
	}
}
