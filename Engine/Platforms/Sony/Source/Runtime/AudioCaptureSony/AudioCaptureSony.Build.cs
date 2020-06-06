// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioCaptureSony : ModuleRules
{
    public AudioCaptureSony(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.Add("Core");
        PrivateDependencyModuleNames.Add("AudioCaptureCore");
        PrivateDependencyModuleNames.Add("AudioMixerCore");
        PublicDefinitions.Add("WITH_AUDIOCAPTURE=1");
    }
}
