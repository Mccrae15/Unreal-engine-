// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.Linq;
using System.Reflection;

public class Audio3D : ModuleRules
{
	public Audio3D(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"Audio3D/Private"
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"	
			}
			);

		PrivateIncludePathModuleNames.Add("TargetPlatform");

		if (Target.Type == TargetType.Editor)
		{
			Type ThisType = typeof(Audio3D);
			var SubclassTypes = from Assembly in AppDomain.CurrentDomain.GetAssemblies()
										 from Type in Assembly.GetTypes()
										 where !Type.IsAbstract && Type.IsSubclassOf(ThisType)
										 select Type;

			foreach (var Type in SubclassTypes)
			{
				string[] Parts = Type.Name.Split('_');
				if (Parts.Length == 2 && Parts[0].Equals(ThisType.Name))
				{
					AppendStringToPublicDefinition("SUPPORTED_AUDIO3D_PLATFORMS", string.Format("TEXT(\"{0}\"), ", Parts[1]));
				}
			}
		}
	}
}
