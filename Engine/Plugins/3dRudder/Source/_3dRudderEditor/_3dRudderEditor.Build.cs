// Copyright 3dRudder 2017, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{ 
    public class _3dRudderEditor : ModuleRules
    {
		private string ModulePath
		{
            get { return  ModuleDirectory; }
        }

        private string SDKPath
		{
			get { return Path.GetFullPath(Path.Combine(ModulePath, "../ThirdParty/3dRudderSDK/")); }
		}		
		
        public _3dRudderEditor(ReadOnlyTargetRules Target): base(Target)
        {
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			// ... add public include paths required here ...
			PublicIncludePaths.AddRange( new string[] {
                
            });
 
            // ... add other private include paths required here ...
            PrivateIncludePaths.AddRange( new string[] {
                "_3dRudderEditor/Private",
                "_3dRudder/Private",
            });
 
            // ... add other public dependencies that you statically link with here ...
            PublicDependencyModuleNames.AddRange( new string[] {
                "Engine",
                "Core", 
                "CoreUObject",      // Provides Actors and Structs
                "Slate",
                "SlateCore",
                "EditorStyle",
                "UnrealEd",
                "PropertyEditor",
                "CurveEditor",
                "UMG",
                "InputCore",
                "AdvancedPreviewScene",
                "_3dRudder",
            });            

            LoadThirdPartyLibraries(Target);
        }
 
        public bool LoadThirdPartyLibraries(ReadOnlyTargetRules Target)
        {
            bool isLibrarySupported = false;
 
            string ArchitecturePath = "";
 
            // When you are building for Win64
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                ArchitecturePath = "x64\\Static";
 
                isLibrarySupported = true;
            }
            // When you are building for Win32
            else if (Target.Platform == UnrealTargetPlatform.Win32)
            {
                isLibrarySupported = false;
            }
 
            // If the current build architecture was supported by the above if statements
            if (isLibrarySupported)
            {
                // Add the architecture specific path to the library files
                PublicAdditionalLibraries.Add(Path.Combine(SDKPath, "Lib", ArchitecturePath, "3dRudderSDK.lib"));
                // Add a more generic path to the include header files
                PublicIncludePaths.Add(Path.Combine(SDKPath, "Include"));
            }
 
            // Defination lets us know whether we successfully found our library!
            PublicDefinitions.Add(string.Format("WITH_MY_LIBRARY_PATH_USE={0}", isLibrarySupported ? 1 : 0));
 
            return isLibrarySupported;
        }
    }
}