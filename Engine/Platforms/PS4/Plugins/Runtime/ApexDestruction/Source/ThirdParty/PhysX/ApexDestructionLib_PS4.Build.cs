// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class ApexDestructionLib_PS4 : ApexDestructionLib
{
	protected override string LibRootDirectory
	{
		get
		{
			string ThisModuleDir = GetModuleDirectoryForSubClass(typeof(ApexDestructionLib_PS4)).FullName;
			return Path.GetFullPath(Path.Combine(ThisModuleDir, @"..\..\..\..\..\..\Source\ThirdParty\PhysX3"));
		}
	}

	protected override string LibraryFormatString_Default
    {
		get { return Path.Combine(ApexLibDir, "lib{0}.a"); }
	}
	
    public ApexDestructionLib_PS4(ReadOnlyTargetRules Target) : base(Target)
    {
    }
}
