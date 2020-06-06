// Copyright Epic Games, Inc. All Rights Reserved.

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
				"RenderCore"
			}
		);

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "Gnmx",
                "RHI",
                "RenderCore",
            }
        );
	}
}
