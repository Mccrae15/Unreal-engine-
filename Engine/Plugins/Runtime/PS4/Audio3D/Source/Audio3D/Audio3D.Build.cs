// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class Audio3D : ModuleRules
{
	public Audio3D(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				"Audio3D/Public"
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"Audio3D/Private"
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"	
			}
			);

        PrivateIncludePathModuleNames.Add("TargetPlatform");

        if (Target.Platform == UnrealTargetPlatform.PS4)
        {
            string BaseSDKPath = Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
            if (BaseSDKPath.Length > 0)
            {
                PrivateIncludePaths.Add(BaseSDKPath + "/target/include");
                PublicAdditionalLibraries.Add("SceAudio3d_stub_weak");
                PublicDefinitions.Add("USING_A3D=1");
            }
        }
        else
        {
            PublicDefinitions.Add("USING_A3D=0");
        }
    }
}
