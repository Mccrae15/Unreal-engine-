// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class zlib_PS4 : zlib
{
    public zlib_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
		string OldzlibPathInc = Path.Combine(ModuleDirectory, OldZlibVersion);
        string OldzlibPathLib = Path.Combine(GetModuleDirectoryForSubClass(typeof(zlib_PS4)).FullName, OldZlibVersion);

        PublicIncludePaths.Add(Path.Combine(OldzlibPathInc, "Inc"));
		PublicAdditionalLibraries.Add(Path.Combine(OldzlibPathLib, "Lib", "libz.a"));
    }
}
