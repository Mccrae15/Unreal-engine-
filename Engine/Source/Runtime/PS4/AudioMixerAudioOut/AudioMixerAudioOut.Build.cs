// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioMixerAudioOut : ModuleRules
{
	public AudioMixerAudioOut(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePathModuleNames.Add("TargetPlatform");
		PublicIncludePaths.Add("Runtime/AudioMixer/Public");
		PrivateIncludePaths.Add("Runtime/AudioMixer/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"Atrac9Decoder",
					"AudioMixer"
				}
		);

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
