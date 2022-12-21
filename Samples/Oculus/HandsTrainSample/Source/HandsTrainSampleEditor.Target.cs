// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class HandsTrainSampleEditorTarget : TargetRules
{
	public HandsTrainSampleEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion  = EngineIncludeOrderVersion.Unreal5_1;	
		ExtraModuleNames.AddRange( new string[] { "HandsTrainSample" } );
	}
}
