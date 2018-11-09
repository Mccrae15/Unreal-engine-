// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PS4PlatformFeatures : ModuleRules
{
	public PS4PlatformFeatures(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] 
			{ 
				"Core", 
				"Json", 
				"Engine" 
			});

        PublicAdditionalLibraries.Add("SceContentExport_stub_weak");
	}
}
