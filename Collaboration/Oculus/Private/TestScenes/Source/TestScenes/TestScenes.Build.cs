// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.IO;

public class TestScenes : ModuleRules
{
	public TestScenes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(
			new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
				"HeadMountedDisplay"
            });

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Json",
                "JsonUtilities",
                "OculusXRHMD",
                "OVRPluginXR",
                "EngineSettings",
                "RenderCore",
            });

        PrivateIncludePaths.Add(
            Path.GetFullPath(Target.RelativeEnginePath) + "Plugins/Runtime/OculusXR/OculusXR/Source/OculusXRHMD/Private"
            );

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
