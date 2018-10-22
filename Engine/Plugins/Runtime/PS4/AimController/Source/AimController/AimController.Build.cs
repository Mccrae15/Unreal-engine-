// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AimController : ModuleRules
{
    public AimController(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePathModuleNames.Add("TargetPlatform");

        PrivateDependencyModuleNames.AddRange(new string[]
        {
			"Core",
			"CoreUObject",
			"Engine",
			"InputDevice",
			"HeadMountedDisplay",
            "PS4Tracker"
		});
    }
}
