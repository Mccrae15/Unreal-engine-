// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

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
				"ShaderCore",
				"ShaderPreprocessor",
				"PS4SDK",
				"ShaderCompilerCommon",
			}
			);

		string SDKDir = System.Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
		if (string.IsNullOrEmpty(SDKDir) == false)
		{
			PublicLibraryPaths.Add(Path.Combine(SDKDir, "/host_tools/lib"));
			PublicAdditionalLibraries.Add("libSceShaderPerf.lib");
			PublicAdditionalLibraries.Add("libSceShaderWavePsslc.lib");
			PrivateIncludePaths.Add(Path.Combine(SDKDir, "target/include_common/shader"));
			PrivateIncludePaths.Add(Path.Combine(SDKDir, "host_tools/include"));
		}
	}
}
