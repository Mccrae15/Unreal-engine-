// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PS4MoviePlayer : ModuleRules
	{
        public PS4MoviePlayer(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicSystemLibraries.Add("SceAvPlayer_stub_weak");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
				    "Engine",
                    "MoviePlayer",
                    "RenderCore",
                    "RHI",
                    "Slate",
					"PS4RHI"
				}
				);
		}
	}
}
