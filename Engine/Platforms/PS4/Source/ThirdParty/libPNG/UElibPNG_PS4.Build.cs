// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class UElibPNG_PS4 : UElibPNG
{
	protected override string LibPNGVersion { get { return "libPNG-1.5.2"; } }
	protected override string LibRootDirectory { get { return GetModuleDirectoryForSubClass(typeof(UElibPNG_PS4)).FullName; } }
	protected override string LibPNGPath { get { return Path.Combine(LibRootDirectory, LibPNGVersion, "lib"); } }

	public UElibPNG_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicAdditionalLibraries.Add(Path.Combine(LibPNGPath, "libpng152.a"));
	}
}
