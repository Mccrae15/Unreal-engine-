// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HmdClient : ModuleRules
{
	public HmdClient(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External; 
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(Target.UEThirdPartySourceDirectory + "PS4\\HmdClient\\include");
		}
	}
}
