// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

using System;
using System.IO;

public class PS4TargetPlatform : ModuleRules
{
	public PS4TargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "PS4";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"InputCore",
				"Sockets",
				"TargetPlatform",
				"DesktopPlatform",
				"RHI",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
				"Core_Sony",
				"Core_PS4"
			}
		);

		// compile with Engine
		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Engine",
					"Slate",
				}
			);

			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}
	}
}
