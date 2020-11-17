// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WebSockets_PS4 : WebSockets
{
	protected override bool PlatformSupportsLibWebsockets { get { return true; } }
	protected override bool UsePlatformSSL { get { return false; } }

	public WebSockets_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
	}
}
