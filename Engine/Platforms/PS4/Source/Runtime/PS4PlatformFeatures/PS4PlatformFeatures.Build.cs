// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PS4PlatformFeatures : ModuleRules
{
	public PS4PlatformFeatures(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "PS4";

		PrivateDependencyModuleNames.AddRange(new string[] 
			{ 
				"Core", 
				"Json", 
				"Engine",
				"SonyPlatformFeatures"
            });

        PublicSystemLibraries.Add("SceContentExport_stub_weak");
	}
}
