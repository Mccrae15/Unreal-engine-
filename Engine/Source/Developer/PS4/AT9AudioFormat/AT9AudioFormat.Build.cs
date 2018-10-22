// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AT9AudioFormat : ModuleRules
{
	public AT9AudioFormat(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "PS4";

		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"PS4SDK"
			}
			);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
		}
	}
}
