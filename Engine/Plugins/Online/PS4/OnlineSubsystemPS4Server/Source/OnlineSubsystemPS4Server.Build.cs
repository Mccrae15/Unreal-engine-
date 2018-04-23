// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemPS4Server : ModuleRules
{
	public OnlineSubsystemPS4Server(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("ONLINESUBSYSTEMPS4SERVER_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
				"Engine", 
				"OnlineSubsystem", 
				"OnlineSubsystemUtils",
			}
			);
	}
}
