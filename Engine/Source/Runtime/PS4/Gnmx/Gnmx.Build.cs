// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class Gnmx : ModuleRules
{
    public Gnmx(ReadOnlyTargetRules Target)
        : base(Target)
	{
        BinariesSubFolder = "PS4";
        PrivateIncludePathModuleNames.Add("Core");

        PrivateIncludePaths.Add("Runtime/PS4/Gnmx/Private");

        // Disable unity builds (there isn't a "force no unity build" option).
        bFasterWithoutUnity = true;
    }
}
