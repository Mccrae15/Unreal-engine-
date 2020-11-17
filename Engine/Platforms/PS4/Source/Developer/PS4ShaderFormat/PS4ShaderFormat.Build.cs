// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class PS4ShaderFormat : ModuleRules
{
	public PS4ShaderFormat(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "PS4";

		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PrivateIncludePathModuleNames.Add("PS4RHI");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"RenderCore",
				"ShaderPreprocessor",
				"ShaderCompilerCommon",
				"FileUtilities"
			}
			);

		string SDKDir = Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
		PublicSystemIncludePaths.Add(Path.Combine(SDKDir, "target", "include_common"));
		PublicSystemIncludePaths.Add(Path.Combine(SDKDir, "host_tools", "include"));

		string LibDir = Path.Combine(SDKDir, "host_tools", "lib");
		PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libSceShaderBinary.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libSceShaderPerf.lib"));
		PublicAdditionalLibraries.Add(Path.Combine(LibDir, "libSceShaderWavePsslc.lib"));

		PublicDelayLoadDLLs.Add("libSceShaderBinary.dll");
		PublicDelayLoadDLLs.Add("libSceShaderPerf.dll");
		PublicDelayLoadDLLs.Add("libSceShaderWavePsslc.dll");
	}
}
