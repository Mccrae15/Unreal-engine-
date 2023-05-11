// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class OculusXRPassthroughAssets : ModuleRules
    {
        public OculusXRPassthroughAssets(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "OculusXRHMD",
                    "OculusXRPassthrough",
                    "OVRPluginXR",
                    "HeadMountedDisplay",
                    "AssetTools",
                    "AssetRegistry",
                });

            PublicIncludePaths.AddRange(new string[] {
                "Runtime/Engine/Classes/Components",
                "Runtime/Engine/Classes/Kismet",
            });

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.AddRange( new string[] {
                    "UnrealEd",
                });
            }
        }
    }
}
