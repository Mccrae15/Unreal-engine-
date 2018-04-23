// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class PS4Tracker : ModuleRules
{
	public PS4Tracker(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{ 
				"Core",
				"Engine",
				"HeadMountedDisplay"
			}
		);

        if (Target.Platform == UnrealTargetPlatform.PS4)
        {
			BinariesSubFolder = "PS4";
					
            PrivateDependencyModuleNames.Add("PS4RHI");
            PublicAdditionalLibraries.AddRange(new string[] { "SceCamera_stub_weak", "SceVrTracker_stub_weak" });
            PublicAdditionalLibraries.Add("SceCommonDialog_stub_weak");
            PublicAdditionalLibraries.Add("SceMsgDialog_stub_weak");
            PublicAdditionalLibraries.Add("SceHmd_stub_weak");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
        {
			BinariesSubFolder = "PS4";
					
            string LibraryPath = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(RulesCompiler.GetFileNameFromType(GetType())), "../../../ThirdParty/PS4/HmdClient/lib", Target.Platform == UnrealTargetPlatform.Win64 ? "Win64" : "Win32"));
            PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "hmd_client.lib"));
            PublicDelayLoadDLLs.Add("hmd_client.dll");

            RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/PS4/" + Target.Platform.ToString() + "/" + "hmd_client.dll");
        }
	}
}
