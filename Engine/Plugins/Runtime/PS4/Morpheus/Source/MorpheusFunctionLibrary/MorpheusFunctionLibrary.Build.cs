// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MorpheusFunctionLibrary : ModuleRules
{
	public MorpheusFunctionLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[]
			{
				"MorpheusFunctionLibrary/Private",
				"Morpheus/Private"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
				"Core",
				"CoreUObject",
				"HeadMountedDisplay",
				"Morpheus",
				"PS4Tracker",
			}
			);

		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
						"PS4RHI",
				}
			);
		}
	}
}
