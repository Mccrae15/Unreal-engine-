// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class PS4TextureFormat : ModuleRules
{
	public PS4TextureFormat(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "PS4";

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
				"TextureCompressor",
				"Engine"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"TextureFormatDXT",
				"TextureFormatUncompressed",
				"ImageCore"
			}
			);

		// need this to allow debug builds to link to libSceGpuAddress
		PublicDefinitions.Add("_ITERATOR_DEBUG_LEVEL=0");

		string SDKDir = Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
		PublicSystemIncludePaths.Add(Path.Combine(SDKDir, "target", "include_common"));
		
		PublicAdditionalLibraries.Add(Path.Combine(SDKDir, "host_tools", "lib", "libSceGpuAddress.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(SDKDir, "host_tools", "lib", "libSceGnm.lib"));

		PublicDelayLoadDLLs.Add("libSceGpuAddress.dll");
		PublicDelayLoadDLLs.Add("libSceGnm.dll");
	}
}



