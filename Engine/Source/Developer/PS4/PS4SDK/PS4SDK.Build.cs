// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PS4SDK : ModuleRules
{
	public PS4SDK(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string SDKDir = System.Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
		if ((SDKDir != null) && (SDKDir.Length > 0))
		{
			PublicIncludePaths.Add(SDKDir + "/target/include_common");
			PublicLibraryPaths.Add(SDKDir + "/host_tools/lib");
			
			// used by PS4ShaderCompiler (must load the DLL explicitly)
			PublicAdditionalLibraries.Add("libSceShaderBinary.lib");
 			PublicDelayLoadDLLs.Add("libSceShaderBinary.dll");
			PublicAdditionalLibraries.Add("libSceShaderPerf.lib");
			PublicDelayLoadDLLs.Add("libSceShaderPerf.dll");

			PublicAdditionalLibraries.Add("libSceShaderWavePsslc.lib");
			PublicDelayLoadDLLs.Add("libSceShaderWavePsslc.dll");
			
			// used by PS4TextureFormat (must load the DLL explicitly)
			PublicAdditionalLibraries.Add("libSceGpuAddress.lib");
			PublicDelayLoadDLLs.Add("libSceGpuAddress.dll");
 			PublicAdditionalLibraries.Add("libSceGnm.lib");
 			PublicDelayLoadDLLs.Add("libSceGnm.dll");
		}
	}
}
