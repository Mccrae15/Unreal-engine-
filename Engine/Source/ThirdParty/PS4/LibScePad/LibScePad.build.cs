// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.Diagnostics;
using System.IO;

public class LibScePad : ModuleRules
{
	private static string SceInstalledLocation = "%SCE_ROOT_DIR%\\Common\\External Tools\\libScePad for PC Games\\";

	private static string StaticLibSubDir = "lib\\Release";
	private static string StaticLib2015Name = "libScePad_static_vs2015_md.lib";

	private static string DebugStaticLibSubDir = "lib\\Debug";
	private static string DebugStaticLib2015Name = "libScePad_static_debug_vs2015_md.lib";

	private static bool IsPathUsable(WindowsCompiler Compiler, string PadLibLocation)
	{
		return Directory.Exists(PadLibLocation) && File.Exists(Path.Combine(PadLibLocation, StaticLibSubDir, StaticLib2015Name));
	}

	public static bool GetPadLibLocation(WindowsCompiler Compiler, out String PadLibLocation)
	{
		// we now only support the new static lib version of libScePad, but SDK 2.0 didn't have a valid lib we can link with
		PadLibLocation = Environment.ExpandEnvironmentVariables(SceInstalledLocation);

		// fallback 1
		if (!IsPathUsable(Compiler, PadLibLocation))
		{
			PadLibLocation = "ThirdParty\\PS4\\LibScePad\\NotForLicensees\\";
		}

		// fallback 2
		if (!IsPathUsable(Compiler, PadLibLocation))
		{
			PadLibLocation = "ThirdParty\\PS4\\LibScePad\\NoRedist\\";
		}

		// make sure the directory and library exist in final location
		bool bFoundLocation = IsPathUsable(Compiler, PadLibLocation);

		// dump out info
		if (!bHasPrinted && !bFoundLocation)
		{
			Console.WriteLine(@"");
			Console.WriteLine(@"warning: Cannot find PS4 pad lib, please read Engine/Source/ThirdParty/PS4/LibScePad/NoRedistReadme.txt");
			Console.WriteLine(@"");

			bHasPrinted = true;
		}

		return bFoundLocation;
	}

	// Only print out the PadLib location once
	private static bool bHasPrinted = false;

	public LibScePad(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External; 

		// These dependencies are not under the engine root, so don't report them when UBT is just gathering a list of build dependencies
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			String PadLibLocation;
			bool bFoundPadLib = GetPadLibLocation(Target.WindowsPlatform.Compiler, out PadLibLocation);

			if (bFoundPadLib)
			{
				PublicSystemIncludePaths.Add(PadLibLocation + "include");
				WhitelistRestrictedFolders.Add(Path.GetFullPath(PadLibLocation + "include"));
				
				PublicAdditionalLibraries.Add("winmm.lib");
				PublicAdditionalLibraries.Add("hid.lib");

				if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
				{
					// Debug runtime
					PublicLibraryPaths.Add(Path.Combine(PadLibLocation, DebugStaticLibSubDir));
					WhitelistRestrictedFolders.Add(Path.GetFullPath(Path.Combine(PadLibLocation, DebugStaticLibSubDir)));
					PublicAdditionalLibraries.Add(DebugStaticLib2015Name);
				}
				else
				{
					// Release runtime
					PublicLibraryPaths.Add(Path.Combine(PadLibLocation, StaticLibSubDir));
					WhitelistRestrictedFolders.Add(Path.GetFullPath(Path.Combine(PadLibLocation, StaticLibSubDir)));
					PublicAdditionalLibraries.Add(StaticLib2015Name);
				}
			}
		}
	}
}
