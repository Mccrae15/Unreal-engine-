// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioMixerAudioOut : ModuleRules
{
	public AudioMixerAudioOut(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "Sony";

		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
					"Core",
					"AudioMixerCore"
				}
		);

		if(Target.bCompileAgainstEngine)
        {
			PrivateDependencyModuleNames.AddRange(
            new string[] {
                    "Engine",
                    "Atrac9Decoder"
                }
			);
        }
	}
}
