// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class FreeType2_PS4 : FreeType2
{
    protected override string FreeType2Version { get { return "FreeType2-2.10.0"; } }
	protected override string LibRootDirectory { get { return GetModuleDirectoryForSubClass(typeof(FreeType2_PS4)).FullName; } }

	protected override string FreeType2LibPath
	{
		get
		{
			return Path.Combine(LibRootDirectory, FreeType2Version, "Lib");
		}
	}

	public FreeType2_PS4(ReadOnlyTargetRules Target) : base(Target)
    {
		bool bUseDebugLibs = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT;
		PublicAdditionalLibraries.Add(Path.Combine(FreeType2LibPath, "PS4", bUseDebugLibs ? "Debug" : "Release", "libfreetype.a"));
    }
}
