// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class ICU_PS4 : ICU
{
	protected override string ICUVersion
	{
		get { return ICU64VersionString; }
	}

	protected override string ICULibRootPath
	{
		get { return GetModuleDirectoryForSubClass(typeof(ICU_PS4)).FullName; }
	}

	protected override string PlatformName
	{
		get { return null; }
	}

	public ICU_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicAdditionalLibraries.Add(Path.Combine(ICULibPath, UseDebugLibs ? "Debug" : "Release", "libicu.a"));

		// Use a bogus platform ID to prevent ICU assuming this is a BSD platform
		PublicDefinitions.Add("U_PLATFORM=9999");
	}
}
