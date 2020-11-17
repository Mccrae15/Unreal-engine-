// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Atrac9Decoder : ModuleRules
{
    public Atrac9Decoder(ReadOnlyTargetRules Target) : base(Target)
    {
		BinariesSubFolder = "Sony";

		PrivateIncludePathModuleNames.Add("TargetPlatform");

        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Engine" });

        // Set to 1 to measure and log Ajm batch statistics
        PublicDefinitions.Add("AT9_BENCHMARK=0");
    }
}