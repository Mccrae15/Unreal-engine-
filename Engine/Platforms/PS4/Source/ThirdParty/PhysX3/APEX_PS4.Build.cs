// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;
using System.Collections.Generic;

public class APEX_PS4 : APEX
{
	protected override bool bIsApexStaticallyLinked_Default
	{
		get { return true; }
	}

	protected override bool bHasApexLegacy_Default
	{
		get { return false; }
	}

	protected override string LibraryFormatString_Default { get { return "lib{0}.a"; } }

	protected override string LibRootDirectory { get { return GetModuleDirectoryForSubClass(typeof(APEX_PS4)).FullName; } }

	protected override List<string> ApexLibraries_Default
	{
		get
		{
			return new List<string>()
			{
				"NvParameterized{0}",
				"RenderDebug{0}"
			};
		}
	}

	public APEX_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("PX_PS4=1");
		PublicDefinitions.Add("PX_EXTERNAL_PLATFORM=1");
	}
}
