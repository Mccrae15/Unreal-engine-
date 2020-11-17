// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libOpus_PS4 : libOpus
{
	public libOpus_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
		string ExtensionDir = Path.Combine(GetModuleDirectoryForSubClass(typeof(libOpus_PS4)).FullName, OpusVersion);

		PublicIncludePaths.Add(Path.Combine(ExtensionDir, "include"));
		PublicAdditionalLibraries.Add(Path.Combine(ExtensionDir, "lib", "Release", "libOpus.a"));
	}
}
