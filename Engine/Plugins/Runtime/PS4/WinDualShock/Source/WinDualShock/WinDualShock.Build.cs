// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool.Rules
{
	public class WinDualShock : ModuleRules
	{
		public WinDualShock(ReadOnlyTargetRules Target) : base(Target)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[] { "LibScePad" });

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
					"ApplicationCore",
				    "Engine",
                    "Slate",
					"InputDevice"
				}
				);
			
			if (Target.Platform != UnrealTargetPlatform.PS4)
			{
				String PadLibLocation;
				bool bFoundPadLib = LibScePad.GetPadLibLocation(Target.WindowsPlatform.Compiler, out PadLibLocation);
				PublicDefinitions.Add("DUALSHOCK4_SUPPORT=" + (bFoundPadLib ? "1" : "0"));
			}
		}
	}
}
