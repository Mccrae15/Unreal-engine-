// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class PhysX_PS4 : PhysX
{
	protected override string LibRootDirectory { get { return GetModuleDirectoryForSubClass(typeof(PhysX_PS4)).FullName; } }
	protected override string PhysXLibDir { get { return Path.Combine(LibRootDirectory, "Lib"); } }
	protected override string PxSharedLibDir { get { return Path.Combine(LibRootDirectory, "Lib"); } }

	public PhysX_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("PX_PS4=1");
		PublicDefinitions.Add("PX_EXTERNAL_PLATFORM=1");

		string[] StaticLibraries = new string[] {
				"libPhysX3{0}.a",
				"libPhysX3Extensions{0}.a",
				"libPhysX3Cooking{0}.a",
				"libPhysX3Common{0}.a",
				"libLowLevel{0}.a",
				"libLowLevelAABB{0}.a",
				"libLowLevelCloth{0}.a",
				"libLowLevelDynamics{0}.a",
				"libLowLevelParticles{0}.a",
				"libSceneQuery{0}.a",
				"libSimulationController{0}.a",
				"libPxFoundation{0}.a",
				"libPxTask{0}.a",
				"libPxPvdSDK{0}.a",
				"libPsFastXml{0}.a"
			};

		foreach (string Lib in StaticLibraries)
		{
			PublicAdditionalLibraries.Add(Path.Combine(PhysXLibDir, String.Format(Lib, LibrarySuffix)));
		}
	}
}