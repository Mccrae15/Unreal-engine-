// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class NvCloth_PS4 : NvCloth
{
    protected override string LibraryFormatString_Default { get { return "lib{0}.a"; } }

	protected override string LibRootDirectory { get { return GetModuleDirectoryForSubClass(typeof(NvCloth_PS4)).FullName; } }
	protected override string NvClothLibDir { get { return Path.Combine(LibRootDirectory, "Lib"); } }

	public NvCloth_PS4(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("PX_PS4=1");
        PublicDefinitions.Add("PX_EXTERNAL_PLATFORM=1");
    }
}
