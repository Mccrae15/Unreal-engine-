// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PS4PlatformEditor : ModuleRules
{
	public PS4PlatformEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "PS4";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"InputCore",
				"DesktopPlatform",
				"Engine",
				"MainFrame",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"PropertyEditor",
				"SharedSettingsWidgets",
				"SourceControl",
				"TargetPlatform",
                "AudioSettingsEditor",
                "AudioPlatformConfiguration",
            }
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);
	}
}
