// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PS4TextureFormat : ModuleRules
{
	public PS4TextureFormat(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "PS4";

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
				"TextureCompressor",
				"Engine"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"TextureFormatDXT",
				"TextureFormatUncompressed",
				"PS4SDK",
				"ImageCore"
			}
			);

		// need this to allow debug builds to link to libSceGpuAddress
		PublicDefinitions.Add("_ITERATOR_DEBUG_LEVEL=0");
	}
}



