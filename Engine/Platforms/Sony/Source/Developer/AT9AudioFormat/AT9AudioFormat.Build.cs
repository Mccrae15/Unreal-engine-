// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64")]
public class AT9AudioFormat : ModuleRules
{
	public AT9AudioFormat(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "Sony";

		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(new string[] 
		{
			"Core"
		});

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}
	}
}
