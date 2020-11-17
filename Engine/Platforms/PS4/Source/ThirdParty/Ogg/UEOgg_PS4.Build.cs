// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class UEOgg_PS4 : UEOgg
{
	protected override string LibRootDirectory { get { return GetModuleDirectoryForSubClass(typeof(UEOgg_PS4)).FullName; } }
	protected override string OggLibPath { get { return Path.Combine(LibRootDirectory, OggVersion, "lib"); } }

	public UEOgg_PS4(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicSystemIncludePaths.Add(Path.Combine(LibRootDirectory, OggVersion, "include"));
		PublicAdditionalLibraries.Add(Path.Combine(OggLibPath, "libogg.a"));
    }
}
