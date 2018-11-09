// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Ngs2 : ModuleRules
{
	public Ngs2(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Engine", "Atrac9Decoder" });

        // Compile and link code to A3D library. Turn this on if you are using Morpheus or you may hear unintentional reverb on 2D sounds.
        PublicDefinitions.Add("A3D=0");

        // Whether or not we're actually using the A3D library. Note that if USING_A3D=1, you also need to set A3D=1.
		// We still need to compile with it so that we can disable certain components of the library that automatically run.
        PublicDefinitions.Add("USING_A3D=0");
    }
}
