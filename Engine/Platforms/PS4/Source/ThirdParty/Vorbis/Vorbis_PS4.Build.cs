// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Vorbis_PS4 : Vorbis
{
    protected override string LibRootDirectory { get { return GetModuleDirectoryForSubClass(typeof(Vorbis_PS4)).FullName; } }
	protected override string VorbisLibPath { get { return Path.Combine(LibRootDirectory, VorbisVersion, "lib"); } }

	public Vorbis_PS4(ReadOnlyTargetRules Target) : base(Target)
    {
		PublicAdditionalLibraries.Add(Path.Combine(VorbisLibPath, "libvorbis.a"));
    }
}
