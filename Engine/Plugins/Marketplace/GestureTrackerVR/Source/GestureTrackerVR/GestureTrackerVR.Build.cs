// Copyright 2016-2017 Hunter Delattre. All Rights Reserved.

using UnrealBuildTool;

public class GestureTrackerVR : ModuleRules
{
    public GestureTrackerVR(ReadOnlyTargetRules Target)
        : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[] {
                "GestureTrackerVR/Public"
            }
            );


        PrivateIncludePaths.AddRange(
            new string[] {
                "GestureTrackerVR/Private"
            }
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {

            }
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                // ... add private dependencies that you statically link with here ...    
            }
            );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // ... add any modules that your module loads dynamically here ...
            }
            );
    }
}
