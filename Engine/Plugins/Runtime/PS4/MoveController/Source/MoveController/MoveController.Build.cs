// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

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
			"InputDevice",
			"HeadMountedDisplay",
            "PS4Tracker"
		});
		
		if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			PublicAdditionalLibraries.Add("SceMove_stub_weak");				
		}
    }
}
