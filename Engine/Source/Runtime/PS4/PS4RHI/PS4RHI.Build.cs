// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PS4RHI : ModuleRules
{	
	public PS4RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "PS4";

		PrivateIncludePathModuleNames.Add("RHI");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
				"RenderCore",
				"UtilityShaders"
			}
		);

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "Gnmx",
                "RHI",
                "ShaderCore",
            }
        );

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("PS4SDK");
		}
        
		PrivateIncludePaths.Add("Runtime/PS4/PS4RHI/Private");
	}
}
