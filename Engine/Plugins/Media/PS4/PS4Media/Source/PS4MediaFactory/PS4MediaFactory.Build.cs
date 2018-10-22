// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PS4MediaFactory : ModuleRules
	{
		public PS4MediaFactory(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MediaAssets",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
					"PS4Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"PS4MediaFactory/Private",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
				});

			if (Target.Type == TargetType.Editor)
			{
				DynamicallyLoadedModuleNames.Add("Settings");
				PrivateIncludePathModuleNames.Add("Settings");
			}

			if (Target.Platform == UnrealTargetPlatform.PS4)
			{
				DynamicallyLoadedModuleNames.Add("PS4Media");
			}
		}
	}
}
