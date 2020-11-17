// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class RHI_PS4 : RHI
{
	public RHI_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDefinitions.Add("USE_STATIC_SHADER_PLATFORM_ENUMS=1");
	}
}
