// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Core_Sony : Core
{
	public Core_Sony(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.Add("zlib");
	}
}
