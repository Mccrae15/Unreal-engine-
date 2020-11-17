// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class TraceLog_PS4 : TraceLog
{
	public TraceLog_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
		EnableTraceByDefault(Target);
    }
}
