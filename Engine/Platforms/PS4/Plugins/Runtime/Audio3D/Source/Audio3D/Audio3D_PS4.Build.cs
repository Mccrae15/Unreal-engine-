// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;

public class Audio3D_PS4 : Audio3D
{
	public Audio3D_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicSystemLibraries.Add("SceAudio3d_stub_weak");
		PublicDefinitions.Add("USING_A3D=1");
		AppendStringToPublicDefinition("SUPPORTED_AUDIO3D_PLATFORMS", "TEXT(\"PS4\"), ");
	}
}