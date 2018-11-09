// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PS4Media : ModuleRules
	{
		public PS4Media(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"ApplicationCore",
					"Engine",
					"HTTP",
					"InputCore",
					"MediaUtils",
					"PS4MediaFactory",
					"PS4RHI",
					"MediaAssets",
					"UtilityShaders",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"PS4Media/Private",
					"PS4Media/Private/Player",
					"PS4Media/Private/Shared",
				});

			PublicAdditionalLibraries.Add("SceAvPlayer_stub_weak");
		}
	}
}
