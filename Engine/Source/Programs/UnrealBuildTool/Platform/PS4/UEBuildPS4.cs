// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	partial class PS4TargetRules
	{
		/// <summary>
		/// IMPORTANT: Set this to true when you are making a build that is meant to be "final", aka released to publisher.
		/// </summary>
		public bool bIsMakingFinalBuild = true;

		/// <summary>
		/// Enable Razor CPU profiling
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bEnableRazorCPUEvents = true;

		/// <summary>
		/// Set this to true to register shaders with the standalone GPU debugger.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bEnableGPUDebugger = false;

		/// <summary>
		/// Set this to true to enable Sulpha host side audio debugging.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bEnableSulphaDebugger = false;

		/// <summary>
		/// Set this to true to enable gnm asserts and validations in the LCUE
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bEnableGnmLCUEDebug = false;

		/// <summary>
		/// Enable unsafe command buffers
		/// Use unsafe command buffers to remove the reserve space call for each gnm command for a perf gain on the render thread
		/// Remaining command buffer space is checked before each draw/dispatch and the ReserveFailure callback is called if required
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bEnableGnmUnsafeCommandBuffers = false;

		/// <summary>
		/// Enables the new virtual memory pooling PS4 memory system.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bEnableNewMemorySystem = false;

		/// <summary>
        /// Enable experimental GPU defragger
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
        public bool bEnableGPUDefragger = false;

		/// <summary>
        /// Unsafe command buffers reserve size
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
        public int GnmUnsafeCommandBufferReserveSizeInBytes = 8 * 1024;

		/// <summary>
		/// Enable companion app support.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bEnableCompanionApp = false;

		/// <summary>
		/// Enable link time optimization for test/shipping builds.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bEnableLTOPerfBuilds = false;

		/// <summary>
		/// Enable link time optimization for development/test/shipping builds.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bEnableLTODevBuilds = false;

		/// <summary>
		/// Set this to true to create a build to generate PGO data that can be set as 
		/// ProfileGuidedOptimizationInput
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bGenerateProfileGuidedOptimizationInput = false;

		/// <summary>
		/// Input file for profile guided optimization
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public string ProfileGuidedOptimizationInput = "";

		/// <summary>
		/// Type of profile guided optimization. One of "sampled" or "instrumented"
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public string ProfileGuidedOptimizationType = "";

		/// <summary>
		/// Move the audio rendering and stats thread to the 7th core when available
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
		public bool bUse7thCore = false;

		/// <summary>
		/// Store marker sting Stack when executing the commandlist also works with minidumps 
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/PS4PlatformEditor.PS4TargetSettings")]
        public bool bEnableCommandListDebugTraces = true;
	}

	partial class ReadOnlyPS4TargetRules
	{
		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>
		#region Read-only accessor properties 
		#if !__MonoCS__
		#pragma warning disable CS1591
		#endif

		public bool bIsMakingFinalBuild
		{
			get { return Inner.bIsMakingFinalBuild; }
		}

		public bool bEnableRazorCPUEvents
		{
			get { return Inner.bEnableRazorCPUEvents; }
		}

		public bool bEnableGPUDebugger
		{
			get { return Inner.bEnableGPUDebugger; }
		}

		public bool bEnableSulphaDebugger
		{
			get { return Inner.bEnableSulphaDebugger; }
		}

		public bool bEnableGnmLCUEDebug
		{
			get { return Inner.bEnableGnmLCUEDebug; }
		}

		public bool bEnableGnmUnsafeCommandBuffers
		{
			get { return Inner.bEnableGnmUnsafeCommandBuffers; }
		}

		public bool bEnableNewMemorySystem
		{
			get { return Inner.bEnableNewMemorySystem; }
		}

        public bool bEnableGPUDefragger
		{
			get { return Inner.bEnableGPUDefragger; }
		}

        public int GnmUnsafeCommandBufferReserveSizeInBytes
		{
			get { return Inner.GnmUnsafeCommandBufferReserveSizeInBytes; }
		}

		public bool bEnableCompanionApp
		{
			get { return Inner.bEnableCompanionApp; }
		}

		public bool bEnableLTOPerfBuilds
		{
			get { return Inner.bEnableLTOPerfBuilds; }
		}
		public bool bEnableLTODevBuilds
		{
			get { return Inner.bEnableLTODevBuilds; }
		}
		public bool bGenerateProfileGuidedOptimizationInput
		{
			get { return Inner.bGenerateProfileGuidedOptimizationInput; }
		}

		public string ProfileGuidedOptimizationInput
		{
			get { return Inner.ProfileGuidedOptimizationInput; }
		}

		public string ProfileGuidedOptimizationType
		{
			get { return Inner.ProfileGuidedOptimizationType; }
		}

		public bool bUse7thCore
		{
			get { return Inner.bUse7thCore; }
		}

		public bool bEnableCommandListDebugTraces
		{
			get { return Inner.bEnableCommandListDebugTraces; }
		}

		#if !__MonoCS__
		#pragma warning restore CS1591
		#endif
		#endregion
	}

	class PS4Platform : UEBuildPlatform
	{
		// the PS4 SDK instance
		PS4PlatformSDK SDK;

		public PS4Platform(PS4PlatformSDK InSDK)
			: base(UnrealTargetPlatform.PS4, CppPlatform.PS4)
		{
			SDK = InSDK;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		public override void ResetTarget(TargetRules Target)
		{
			Target.bCompileSimplygon = false;
            Target.bCompileSimplygonSSF = false;
			Target.PS4Platform.bEnableGnmLCUEDebug |= (Target.Configuration == UnrealTargetConfiguration.Debug);
		}

		public override void ValidateTarget(TargetRules Target)
		{
			Target.bCompileLeanAndMeanUE = true;
			Target.bUsePCHFiles = false;
			Target.bDeployAfterCompile = true;
			Target.bCompileNvCloth = true;

			if (Target.PS4Platform.bGenerateProfileGuidedOptimizationInput)
			{
				Console.WriteLine("Building for PGO generation. Output will be to /data/<gamename>/build/ps4/default.profraw!");
			}
			else if (GetLTOEnabled(new ReadOnlyTargetRules(Target)))
			{
				Console.WriteLine("Using LTO, linking will be extremely (45-90m) slow!");

				if (string.IsNullOrEmpty(Target.PS4Platform.ProfileGuidedOptimizationInput) == false)
				{
					string PGOInput = Path.Combine(UnrealBuildTool.RootDirectory.FullName, Target.PS4Platform.ProfileGuidedOptimizationInput);

					if (File.Exists(PGOInput) == false)
					{
						throw new Exception(string.Format("PGOInput file {0} not found!", PGOInput));
					}

					string PGOType = Target.PS4Platform.ProfileGuidedOptimizationType.ToLower();

					// two types are available on PS4, sample and instrumented
					if (PGOType != "sampled" && PGOType != "instrumented")
					{
						throw new Exception(string.Format("PGO type '{0}' not supported. Must be sampled or instrumented", PGOType));
					}

					Console.WriteLine("Using {0} PGO with input {1}", PGOType, PGOInput);
				}
			}
		}

		/// <summary>
		/// If this platform can be compiled with XGE
		/// </summary>
		public override bool CanUseXGE()
		{
			return true;
		}

		/// <summary>
		/// If this platform can be compiled with SN-DBS
		/// </summary>
		public override bool CanUseSNDBS()
		{
			return true;
		}

		/// <summary>
		/// Determines if the given name is a build product for a target.
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".prx")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".self")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".a");
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binrary type being built</param>
		/// <returns>string    The binary extenstion (ie 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".prx";
				case UEBuildBinaryType.Executable:
					return ".self";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
				case UEBuildBinaryType.Object:
					return ".o";
				case UEBuildBinaryType.PrecompiledHeader:
					return ".gch";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		/// <summary>
		/// Get the extension to use for debug info for the given binary type
		/// </summary>
		/// <param name="Target">Rules for the target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The debug info extension (i.e. 'pdb')</returns>
		public override string GetDebugInfoExtension(ReadOnlyTargetRules Target, UEBuildBinaryType InBinaryType)
		{
			return "";
		}

		/// <summary>
		/// Whether this platform should build a monolithic binary
		/// </summary>
		public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			return true;
		}

		/// <summary>
		/// Whether the editor should be built for this platform or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		/// <summary>
		/// Whether the build platform requires deployment prep
		/// </summary>
		/// <returns></returns>
		public override bool RequiresDeployPrepAfterCompile()
		{
			return true;
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				// PS4 does not have 32-bit libraries...
				bool bIsMonolithic = true;
				var BuildPlatform = GetBuildPlatform(Target.Platform);

				if (BuildPlatform != null)
				{
					bIsMonolithic = BuildPlatform.ShouldCompileMonolithicBinary(Target.Platform);
				}

				if ((Target.bBuildEditor == false) || !bIsMonolithic)
				{
					bool bBuildShaderFormats = Target.bForceBuildShaderFormats;

					if (!Target.bBuildRequiresCookedData)
					{
						if (ModuleName == "Engine")
						{
							if (Target.bBuildDeveloperTools)
							{
								Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("PS4TargetPlatform");
								Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("PS4PlatformEditor");
							}
						}
						else if (ModuleName == "TargetPlatform")
						{
							bBuildShaderFormats = true;
							if (Target.bBuildDeveloperTools)
							{
								Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("AT9AudioFormat");
								Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("PS4TextureFormat");
							}
						}
					}

					// allow standalone tools to use target platform modules, without needing Engine
					if (ModuleName == "TargetPlatform")
					{
						if (Target.bForceBuildTargetPlatforms)
						{
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("PS4TargetPlatform");
						}

						if (bBuildShaderFormats)
						{
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("PS4ShaderFormat");
						}
					}
				}
			}
		}

		public bool GetLTOEnabled(ReadOnlyTargetRules Target)
		{
			bool bDevBuild = (Target.Configuration == UnrealTargetConfiguration.Development);
			bool bPerfBuild = (Target.Configuration == UnrealTargetConfiguration.Test) || (Target.Configuration == UnrealTargetConfiguration.Shipping);
			bool bForced = Target.bAllowLTCG;
			bool bPGOEnabled = Target.bPGOOptimize || Target.bPGOProfile;

			return bForced || (Target.PS4Platform.bEnableLTOPerfBuilds && bPerfBuild) || (Target.PS4Platform.bEnableLTODevBuilds && bDevBuild) || bPGOEnabled;
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			if (Target.PS4Platform.bEnableRazorCPUEvents && Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				Rules.PublicDefinitions.Add("PS4_PROFILING_ENABLED=1");
			}
			else
			{
				Rules.PublicDefinitions.Add("PS4_PROFILING_ENABLED=0");
			}

			//removed until we provide ICU data with a non-source code file
			//Rules.Definitions.Add("ICU_OVERRIDE_LINKED_DATA");

			if (ModuleName == "Core")
			{
				Rules.PublicIncludePaths.Add("Runtime/Core/Public/PS4");
				Rules.PublicDependencyModuleNames.Add("zlib");
			}
			else if (ModuleName == "Engine")
			{
				Rules.PrivateDependencyModuleNames.Add("zlib");
				Rules.PrivateDependencyModuleNames.Add("UElibPNG");
			}
			else if (ModuleName == "FreeType2")
			{
				string FreeType2Path = "ThirdParty/FreeType2/FreeType2-2.4.12/";
				Rules.PublicLibraryPaths.Add(FreeType2Path + "Lib/PS4");
				Rules.PublicAdditionalLibraries.Add("freetype2412");
			}
			else if (ModuleName == "UElibPNG")
			{
				string libPNGPath = "ThirdParty/libPNG/libPNG-1.5.2";
				Rules.PublicLibraryPaths.Add(libPNGPath + "/lib/PS4");
				Rules.PublicAdditionalLibraries.Add("png152");
			}
			else if (ModuleName == "zlib")
			{
				Rules.PublicLibraryPaths.Add("ThirdParty/zlib/zlib-1.2.5/Lib/PS4");
				Rules.PublicAdditionalLibraries.Add("z");
			}
			else if (ModuleName == "Slate")
			{
			}
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			string AutoSDKPath = Environment.GetEnvironmentVariable("UE_SDKS_ROOT");
			string BaseSDKPath = Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");

			string SDKVersionString = PS4PlatformSDK.SDKVersionToString(PS4PlatformSDK.GetInstalledSDKVersion());

			if (AutoSDKPath != null && BaseSDKPath != null && BaseSDKPath.StartsWith(AutoSDKPath))
			{
				Console.WriteLine("Compiling against AutoSDK: " + SDKVersionString);
			}
			else
			{
				Console.WriteLine("Compiling against SDK: " + SDKVersionString);
			}

			CompileEnvironment.Definitions.Add("MINIMUM_SYSTEM_SOFTWARE_VERSION=" + PS4PlatformSDK.MinimumSystemSoftwareVersion);
			CompileEnvironment.Definitions.Add("PLATFORM_DESKTOP=0");
			CompileEnvironment.Definitions.Add("PLATFORM_64BITS=1");
			CompileEnvironment.Definitions.Add("EXCEPTIONS_DISABLED=0");
			CompileEnvironment.Definitions.Add("DUALSHOCK4_SUPPORT=1");

			CompileEnvironment.Definitions.Add("WITH_OGGVORBIS=0");

			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(SCE_ORBIS_SDK_DIR)");
			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(SCE_ORBIS_SDK_DIR)/host_tools/lib/clang/3.1/include");
			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(SCE_ORBIS_SDK_DIR)/target/include");
			CompileEnvironment.IncludePaths.SystemIncludePaths.Add("$(SCE_ORBIS_SDK_DIR)/target/include_common");

			LinkEnvironment.LibraryPaths.Add("$(SCE_ORBIS_SDK_DIR)/target/lib");

			CompileEnvironment.Definitions.Add("UNICODE");
			CompileEnvironment.Definitions.Add("_UNICODE");
			CompileEnvironment.Definitions.Add("PLATFORM_PS4=1");
			CompileEnvironment.Definitions.Add("EXCEPTIONS_DISABLED=0");

			if (GetLTOEnabled(Target))
			{
				LinkEnvironment.AdditionalLibraries.Add("SceGnm_lto");
			}
			else
			{
				if (Target.PS4Platform.bEnableGnmLCUEDebug && Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					LinkEnvironment.AdditionalLibraries.Add("SceGnm_debug");
				}
				else
				{
					LinkEnvironment.AdditionalLibraries.Add("SceGnm");
				}
			}
			// Compile and link with PS4 API libraries.
			
			LinkEnvironment.AdditionalLibraries.Add("SceGnmDriver_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceGpuAddress");
			LinkEnvironment.AdditionalLibraries.Add("SceShaderBinary");
			LinkEnvironment.AdditionalLibraries.Add("SceSha256");
			LinkEnvironment.AdditionalLibraries.Add("ScePad_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceNet_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceNetCtl_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceNgs2_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceAudioOut_stub_weak");
            LinkEnvironment.AdditionalLibraries.Add("SceAjm_stub_weak");
            LinkEnvironment.AdditionalLibraries.Add("SceSysModule_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceVideoOut_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceUserService_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceFios2_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("ScePosix_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceCompanionHttpd_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceNet_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceSsl_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceHttp_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("ScePlayGo_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceNpCommon_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceNpManager_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceNpMatching2_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceNpTrophy_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceAppContent_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceSaveData_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceSaveDataDialog_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceCommonDialog_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceGameLiveStreaming_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceSystemService_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceIme_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceImeDialog_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceCoredump_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceMsgDialog_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceNpCommerce_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceScreenShot_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceVideoRecording_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceSharePlay_stub_weak");
            LinkEnvironment.AdditionalLibraries.Add("SceMouse_stub_weak");
            LinkEnvironment.AdditionalLibraries.Add("SceRandom_stub_weak");
			LinkEnvironment.AdditionalLibraries.Add("SceSocialScreen_stub_weak");


			ProjectDescriptor Project = (Target.ProjectFile != null) ? ProjectDescriptor.FromFile(Target.ProjectFile) : null;
			PluginInfo MorpheusPlugin = new PluginInfo(new FileReference("../Plugins/Runtime/PS4/Morpheus/Morpheus.uplugin"), PluginType.Engine);
			bool bMorpheusPluginEnabled = Plugins.IsPluginEnabledForProject(MorpheusPlugin, Project, UnrealTargetPlatform.PS4, Target.Type);

			// Morpheus support on device requires an SDK patch that we cannot distribute. So check if we have it installed.
			string MorpheusPath = Path.Combine(BaseSDKPath, "target", "include", "hmd.h");
			bool bHasMorpheusSDKPatch = File.Exists(MorpheusPath);
			if (bHasMorpheusSDKPatch && bMorpheusPluginEnabled)
			{
				Console.WriteLine("Enabling Morpheus.");
				// add defines to enable the code paths in the engine
				CompileEnvironment.Definitions.Add("HAS_MORPHEUS=1");
				CompileEnvironment.Definitions.Add("MORPHEUS_ENGINE_DISTORTION=0");
			}
			else
			{
				CompileEnvironment.Definitions.Add("HAS_MORPHEUS=0");
			}

			if(Target.PS4Platform.bUse7thCore)
			{
				CompileEnvironment.Definitions.Add("USE_7TH_CORE=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("USE_7TH_CORE=0");
			}

            if (Target.PS4Platform.bEnableCommandListDebugTraces && Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test)
            {
                CompileEnvironment.Definitions.Add("RHI_COMMAND_LIST_DEBUG_TRACES=1");
            }
            else
            {
                CompileEnvironment.Definitions.Add("RHI_COMMAND_LIST_DEBUG_TRACES=0");
            }

			
			// some libs are unshippable, but we still want in our "Shipping" config
			if (Target.Configuration != UnrealTargetConfiguration.Shipping || !Target.PS4Platform.bIsMakingFinalBuild)
			{
				LinkEnvironment.AdditionalLibraries.Add("SceDbgPlayGo_stub_weak");

				LinkEnvironment.AdditionalLibraries.Add("ScePm4Dump");
				LinkEnvironment.AdditionalLibraries.Add("ScePerf_stub_weak");

 				LinkEnvironment.AdditionalLibraries.Add("SceDbg_stub_weak");

 				LinkEnvironment.AdditionalLibraries.Add("SceRazorGpuThreadTrace_stub_weak");

                LinkEnvironment.AdditionalLibraries.Add("SceRazorCPU_stub_weak");
                LinkEnvironment.AdditionalLibraries.Add("SceRazorCPU_debug_stub_weak");
            }
			else
			{
				if (Target.Configuration != UnrealTargetConfiguration.Shipping)
				{
					throw new BuildException("Compiling a Final build in non-Shipping configuration!");
				}
			}
			
			if (Target.PS4Platform.bEnableGnmLCUEDebug && Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				CompileEnvironment.Definitions.Add("SCE_GNM_DEBUG=1");
				CompileEnvironment.Definitions.Add("SCE_GNM_LCUE_VALIDATE_COMPLETE_RESOURCE_BINDING_ENABLED=1");
			}

			CompileEnvironment.Definitions.Add("SCE_GNMX_TRACK_EMBEDDED_CB=0");

			if (Target.PS4Platform.bEnableGnmUnsafeCommandBuffers)
			{
				// Enable Unsafe Command Buffers

				// Disable checking for space in the command buffer when writing each gnm command
				CompileEnvironment.Definitions.Add("SCE_GNMX_ENABLE_UNSAFE_COMMAND_BUFFERS=1");

				// Reserve at end of each dcb, during predraw checks if we have entered the reserve and if so does the buffer full callback
				CompileEnvironment.Definitions.Add("PS4_ENABLE_COMMANDBUFFER_RESERVE=1");
				CompileEnvironment.Definitions.Add("COMMANDBUFFER_RESERVE_SIZE_BYTES=" + Target.PS4Platform.GnmUnsafeCommandBufferReserveSizeInBytes);
			}
			else
			{
				CompileEnvironment.Definitions.Add("PS4_ENABLE_COMMANDBUFFER_RESERVE=0");
			}

			if (Target.PS4Platform.bEnableNewMemorySystem)
			{
				Console.WriteLine("Building PS4 with new memory system.");
				CompileEnvironment.Definitions.Add("USE_DEFRAG_ALLOCATOR=0");
				CompileEnvironment.Definitions.Add("USE_NEW_PS4_MEMORY_SYSTEM=1");
			}
			else if (Target.PS4Platform.bEnableGPUDefragger)
			{
				Console.WriteLine("Building PS4 with GPU Defrag support");
				CompileEnvironment.Definitions.Add("USE_DEFRAG_ALLOCATOR=1");
				CompileEnvironment.Definitions.Add("USE_NEW_PS4_MEMORY_SYSTEM=0");
			}
			else
			{
				CompileEnvironment.Definitions.Add("USE_DEFRAG_ALLOCATOR=0");
				CompileEnvironment.Definitions.Add("USE_NEW_PS4_MEMORY_SYSTEM=0");
			}

			// Disable support for Dispatch Draw. We don't use it so we can save the CPU overhead
			CompileEnvironment.Definitions.Add("SCE_GNM_LCUE_ENABLE_DISPATCH_DRAW=0");

			// Unused define in cue.cpp. Defined to 0 to avoid compiler warning.
			CompileEnvironment.Definitions.Add("SCE_GNM_CUE2_VALIDATE_DISPATCH_DRAW_INPUT_USAGE_TABLE=0");

			if (Target.PS4Platform.bEnableSulphaDebugger && Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				CompileEnvironment.Definitions.Add("ENABLE_SULPHA_DEBUGGER=1");
				LinkEnvironment.AdditionalLibraries.Add("SceSulpha_stub_weak");
			}
			else
			{
				CompileEnvironment.Definitions.Add("ENABLE_SULPHA_DEBUGGER=0");
			}

			if (Target.PS4Platform.bEnableGPUDebugger && Target.Configuration != UnrealTargetConfiguration.Shipping)
			{
				CompileEnvironment.Definitions.Add("ENABLE_GPU_DEBUGGER=1");
				LinkEnvironment.AdditionalLibraries.Add("SceGpuDebugger_stub_weak");
			}
			else
			{
				CompileEnvironment.Definitions.Add("ENABLE_GPU_DEBUGGER=0");
			}

			if (Target.PS4Platform.bEnableCompanionApp)
			{
				CompileEnvironment.Definitions.Add("PS4_FEATURE_COMPANION_APP=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("PS4_FEATURE_COMPANION_APP=0");
			}

			if (Target.bPGOOptimize || Target.bPGOProfile)
			{
				CompileEnvironment.PGODirectory = Path.Combine(DirectoryReference.FromFile(Target.ProjectFile).FullName, "Build", "PS4", "PGO");
				CompileEnvironment.PGOFilenamePrefix = "profile.profdata";

				LinkEnvironment.PGODirectory = CompileEnvironment.PGODirectory;
				LinkEnvironment.PGOFilenamePrefix = CompileEnvironment.PGOFilenamePrefix;
			}
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			switch (Target.Configuration)
			{
				case UnrealTargetConfiguration.Development:
				case UnrealTargetConfiguration.Shipping:
				case UnrealTargetConfiguration.Test:
				case UnrealTargetConfiguration.Debug:
				default:
					return true;
			};
		}

		/// <summary>
		/// Setup the binaries for this specific platform.
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="ExtraModuleNames"></param>
		public override void AddExtraModules(ReadOnlyTargetRules Target, List<string> ExtraModuleNames)
		{
			ExtraModuleNames.Add("PS4RHI");
			ExtraModuleNames.Add("Ngs2");
            ExtraModuleNames.Add("Atrac9Decoder");
            ExtraModuleNames.Add("AudioMixerAudioOut");
            ExtraModuleNames.Add("PS4PlatformFeatures");
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="CppPlatform">The platform to create a toolchain for</param>
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CppPlatform CppPlatform, ReadOnlyTargetRules Target)
		{
			string PGOInput = Target.PS4Platform.ProfileGuidedOptimizationInput;

			if (string.IsNullOrEmpty(PGOInput) == false)
			{
				PGOInput = Path.Combine(UnrealBuildTool.RootDirectory.FullName, PGOInput);
			}

			string PGOType = Target.PS4Platform.ProfileGuidedOptimizationType.ToLower();
			return new PS4ToolChain(GetLTOEnabled(Target), Target.PS4Platform.bGenerateProfileGuidedOptimizationInput, PGOInput, PGOType);
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Target">Information about the target being deployed</param>
		public override void Deploy(UEBuildDeployTarget Target)
		{
			new UEDeployPS4().PrepTargetForDeployment(Target);
		}
	}

	class PS4PlatformSDK : UEBuildPlatformSDK
	{
		// This is the SDK version we support in //CarefullyRedist.
		// May include minor revisions or descriptions that a default install from SDK_Manager won't have.
		// e.g. 1.600_Patch001, or 1.610.001.  The SDK_Manager always installs minor revision patches straight into
		// the default major revision folder.
		public const string ExpectedSDKVersion = "5.008.071";

		// SDK version expected from a default install from SDK_Manager
		public const string ExpectedManuallyInstalledSDKVersion = "5.000";

		// Platform name for finding SDK under //CarefullyRedist
		public const string TargetPlatformName = "PS4";

		// Minimum System Software version 
		public const uint MinimumSystemSoftwareVersion = 0x05030061u;

		/// <summary>
		/// Whether platform supports switching SDKs during runtime
		/// </summary>
		/// <returns>true if supports</returns>
		protected override bool PlatformSupportsAutoSDKs()
		{
			return true;
		}

		/// <summary>
		/// Returns platform-specific name used in SDK repository
		/// </summary>
		/// <returns>path to SDK Repository</returns>
		public override string GetSDKTargetPlatformName()
		{
			return TargetPlatformName;
		}

		/// <summary>
		/// Returns SDK string as required by the platform
		/// </summary>
		/// <returns>Valid SDK string</returns>
		protected override string GetRequiredSDKString()
		{
			return ExpectedSDKVersion;
		}

		protected override String GetRequiredScriptVersionString()
		{
			// major.minor.bumps (bumps used to force reapplication of an SDK update)
			return "4.5.4";
		}

		private const uint INVALID_SDK = 0xFFFFFFFF;
		/// <summary>
		/// Gets the version of the SDK installed by looking at SCE_ORBIS_SDK_VERSION in sdk_version.h
		/// </summary>
		/// <returns>SDK version</returns>
		static public uint GetInstalledSDKVersion()
		{
			uint InstalledSDKVersion = INVALID_SDK;

			string BaseSDKPath = Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
			if ((BaseSDKPath != null) && (BaseSDKPath.Length > 0))
			{
				// Check for the *actual* sdk version
				string SDKVersionHeaderFile = Path.Combine(BaseSDKPath, "target", "include", "sdk_version.h");
				bool bHasSDKVersionHeader = File.Exists(SDKVersionHeaderFile);

				if (bHasSDKVersionHeader)
				{
					string VersionFileString = File.ReadAllText(SDKVersionHeaderFile);

					// get hex version number between parenthesis
					const string OrbisSDKVersionString = "SCE_ORBIS_SDK_VERSION (0x";
					int StartIndex = VersionFileString.IndexOf(OrbisSDKVersionString) + OrbisSDKVersionString.Length;
					int EndIndex = VersionFileString.IndexOf("u)");
					string VersionSubString = VersionFileString.Substring(StartIndex, EndIndex - StartIndex);
					InstalledSDKVersion = Convert.ToUInt32(VersionSubString, 16);
				}
			}

			return InstalledSDKVersion;
		}

		/// <summary>
		/// Converts a hex SDK version to a printable string
		/// </summary>
		/// <returns>SDK version as a string</returns>
		static public string SDKVersionToString(uint SDKVersion)
		{
			string VersionString = SDKVersion.ToString("X8");
			VersionString = VersionString.Insert(2, ".");
			VersionString = VersionString.Insert(6, ".");
			if ( VersionString[0] == '0')
			{
				VersionString = VersionString.Substring(1);
            }

			return VersionString;
		}

		/// <summary>
		/// Converts an SDK version string to a uint
		/// </summary>
		/// <returns>SDK version as a uint</returns>
		static public uint SDKVersionStringToUint(string VersionString)
		{
			VersionString = VersionString.Replace(".", string.Empty);
			return Convert.ToUInt32(VersionString, 16);
		}


		/// <summary>
		/// Whether the required external SDKs are installed for this platform
		/// </summary>
		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			// if any autosdk setup has been done then the local process environment is suspect
			if (HasSetupAutoSDK())
			{
				return SDKStatus.Invalid;
			}

			uint InstalledVersion = GetInstalledSDKVersion();

			if (InstalledVersion != INVALID_SDK)
			{
				if (InstalledVersion == SDKVersionStringToUint(ExpectedSDKVersion))
				{
					return SDKStatus.Valid;
				}
				else
				{
					// Check if autosdk folder is valid so we can suppress warning messages about the installed SDK version
					bool bAutoSDKIsValid = false;
					string PathToAutoSDK = GetPathToPlatformAutoSDKs();
					if( PathToAutoSDK != null )
					{
						PathToAutoSDK = Path.Combine(PathToAutoSDK, ExpectedSDKVersion);
						bAutoSDKIsValid = (Directory.Exists(PathToAutoSDK));
					}

					if (InstalledVersion > SDKVersionStringToUint(ExpectedSDKVersion))
					{
						if( !bAutoSDKIsValid )
						{
							Console.WriteLine("*** Found installed PS4 SDK version {0} but require {1}, this may cause unknown issues ***", SDKVersionToString(InstalledVersion), ExpectedSDKVersion);
						}
						return SDKStatus.Valid;
					}
					else
					{
						if (!bAutoSDKIsValid)
						{
							Console.WriteLine("*** Found installed PS4 SDK version {0} but require {1} ***", SDKVersionToString(InstalledVersion), ExpectedSDKVersion);
						}
					}
				}
			}

			return SDKStatus.Invalid;
		}
	}

	class PS4PlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.PS4; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		protected override void RegisterBuildPlatforms(SDKOutputLevel OutputLevel)
		{
			PS4PlatformSDK SDK = new PS4PlatformSDK();
			SDK.ManageAndValidateSDK(OutputLevel);

			// Make sure the SDK is installed
			if (SDK.HasRequiredSDKsInstalled() == SDKStatus.Valid)
			{
				// Register this build platform for PS4
				Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.PS4.ToString());
				UEBuildPlatform.RegisterBuildPlatform(new PS4Platform(SDK));
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.PS4, UnrealPlatformGroup.Sony);
			}
		}
	}
}
