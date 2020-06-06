// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

class BuildPhysX_PS4 : BuildPhysX.MakefileTargetPlatform
{
	public override UnrealTargetPlatform Platform => UnrealTargetPlatform.PS4;
	public override bool HasBinaries => false;
	public override string DebugDatabaseExtension => null;
	public override string DynamicLibraryExtension => null;
	public override string StaticLibraryExtension => "a";
	public override bool IsPlatformExtension => true;
	public override bool UseResponseFiles => true;
	public override string TargetBuildPlatform => "ps4";
	public override string GetToolchainName(BuildPhysX.PhysXTargetLib TargetLib, string TargetConfiguration) => "PS4Toolchain.txt";

	public override bool SupportsTargetLib(BuildPhysX.PhysXTargetLib Library)
	{
		switch (Library)
		{
			case BuildPhysX.PhysXTargetLib.APEX: return true;
			case BuildPhysX.PhysXTargetLib.NvCloth: return true;
			case BuildPhysX.PhysXTargetLib.PhysX: return true;
			default: return false;
		}
	}
}
