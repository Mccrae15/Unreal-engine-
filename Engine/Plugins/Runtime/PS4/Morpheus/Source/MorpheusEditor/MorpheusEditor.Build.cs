// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MorpheusEditor : ModuleRules
{
	public MorpheusEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("MorpheusEditor/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
				"Core",
				"CoreUObject",	
                "Morpheus",
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",				
			}
		);
	}
}
