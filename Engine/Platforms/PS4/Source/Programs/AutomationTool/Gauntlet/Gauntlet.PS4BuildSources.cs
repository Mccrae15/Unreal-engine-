// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using UnrealBuildTool;

namespace Gauntlet
{
	public class PS4BuildSource : StagedBuildSource<StagedBuild>
	{
		public override string BuildName { get { return "PS4StagedBuild"; } }

		public override UnrealTargetPlatform Platform { get { return UnrealTargetPlatform.PS4; } }

		public override string PlatformFolderPrefix { get { return "PS4"; } }
	}
}
