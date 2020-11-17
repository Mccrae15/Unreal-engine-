// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class PhysXVehicleLib_PS4 : PhysXVehicleLib
{
	protected override string LibRootDirectory
	{
		get
		{
			string ThisModuleDir = GetModuleDirectoryForSubClass(typeof(PhysXVehicleLib_PS4)).FullName;
			return Path.GetFullPath(Path.Combine(ThisModuleDir, @"..\..\..\..\..\..\Source\ThirdParty"));
		}
	}

	public PhysXVehicleLib_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicAdditionalLibraries.Add(Path.Combine(PhysXLibDir, string.Format("libPhysX3Vehicle{0}.a", LibrarySuffix)));
    }
}
