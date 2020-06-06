// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Gnmx : ModuleRules
{
    public Gnmx(ReadOnlyTargetRules Target)
        : base(Target)
	{
        BinariesSubFolder = "PS4";
        PrivateIncludePathModuleNames.Add("Core");

        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

		// Since Gnmx is written by Sony, there's no guarantee the code will compile correctly under unity builds.
		// Disable unity so that we don't have to modify Gnmx code to get it to compile under UBT.
		bUseUnity = false;

		// Don't require source files to include matching headers first.
		bEnforceIWYU = false;
		PrivatePCHHeaderFile = "Private/GnmxPrivatePCH.h";
	}
}
