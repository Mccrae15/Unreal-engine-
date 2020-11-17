// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class Icmp_Sony : Icmp
{
	public Icmp_Sony(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("PLATFORM_SUPPORTS_ICMP=1");
		PublicDefinitions.Add("PLATFORM_USES_POSIX_ICMP=0");
		PublicDefinitions.Add("PING_ALLOWS_CUSTOM_THREAD_SIZE=0");
	}
}
