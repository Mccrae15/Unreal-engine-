// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
