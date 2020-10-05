// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class CloudSaveDLC : ModuleRules
{
	public CloudSaveDLC(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AndroidPermission", "UMG", "LibOVRPlatform" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "OnlineSubsystem", "OnlineSubsystemOculus" });

        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
        {
            if (Target.Platform == UnrealTargetPlatform.Win32)
            {
                PublicDelayLoadDLLs.Add("LibOVRPlatform32_1.dll");
            }
            else
            {
                PublicDelayLoadDLLs.Add("LibOVRPlatform64_1.dll");
            }
        }
        else if (Target.Platform != UnrealTargetPlatform.Android)
        {
            PrecompileForTargets = PrecompileTargetsType.None;
        }
    }
}
