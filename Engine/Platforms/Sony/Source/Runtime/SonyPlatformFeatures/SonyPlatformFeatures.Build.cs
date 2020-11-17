// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SonyPlatformFeatures : ModuleRules
{
	public SonyPlatformFeatures(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "Sony";

		PrivateDependencyModuleNames.AddRange(new string[] 
			{ 
				"Core", 
				"Engine" 
			});
	}
}
