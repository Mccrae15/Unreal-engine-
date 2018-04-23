// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class PS4ProjectGenerator : UEPlatformProjectGenerator
	{
		/// <summary>
		/// Register the platform with the UEPlatformProjectGenerator class
		/// </summary>
		public override void RegisterPlatformProjectGenerator()
		{
			// Register this project generator for PS4
			Log.TraceVerbose("        Registering for {0}", UnrealTargetPlatform.PS4.ToString());
			UEPlatformProjectGenerator.RegisterPlatformProjectGenerator(UnrealTargetPlatform.PS4, this);
		}

		///
		///	VisualStudio project generation functions
		///	
		/// <summary>
		/// Return the VisualStudio platform name for this build platform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>string    The name of the platform that VisualStudio recognizes</returns>
		public override string GetVisualStudioPlatformName(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			if (InPlatform == UnrealTargetPlatform.PS4)
			{
				return "ORBIS";
			}
			return InPlatform.ToString();
		}

		public override string GetVisualStudioPathsEntries(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, VCProjectFileFormat InProjectFileFormat)
		{
			string PathsLines = "";

			// Set the executable and include directories (PATH and INCLUDE environment variables) to the same values the SCE VSI uses for Makefile projects.
			PathsLines += @"		<ExecutablePath>$(SCE_ORBIS_SDK_DIR)\host_tools\bin;$(VCTargetsPath)\Platforms\ORBIS;$(PATH);</ExecutablePath>" + ProjectFileGenerator.NewLine;
			PathsLines += @"		<IncludePath>$(SCE_ORBIS_SDK_DIR)\host_tools\lib\clang\include;$(SCE_ORBIS_SDK_DIR)\target\include;$(SCE_ORBIS_SDK_DIR)\target\include_common;</IncludePath>" + ProjectFileGenerator.NewLine;

			// Set a working directory so the game can find trophy and npdata files under the sce_sys directory when running from visual studio.
			String TargetDirectory = Path.GetDirectoryName(NMakeOutputPath.FullName);
			String ProjectRootDirectory = Path.GetDirectoryName(Path.GetDirectoryName(TargetDirectory));
			String WorkingDirectory = Path.Combine(ProjectRootDirectory, "Build\\PS4");

			PathsLines += "		<LocalDebuggerWorkingDirectory>" + WorkingDirectory + "</LocalDebuggerWorkingDirectory>" + ProjectFileGenerator.NewLine;
			PathsLines += "		<DebuggerFlavor>ORBISDebugger</DebuggerFlavor>" + ProjectFileGenerator.NewLine;

			return PathsLines;
		}

		/// <summary>
		/// Return the ORBIS VSI required property import lines
		/// </summary>
		public override string GetVisualStudioTargetOverrides(UnrealTargetPlatform InPlatform, VCProjectFileFormat InProjectFileFormat)
		{
			if (InPlatform != UnrealTargetPlatform.PS4)
			{
				throw new BuildException("Unexpected format in PS4 Proj generator");
			}

			return
				"	<Import Condition=\"'$(ConfigurationType)' == 'Makefile' and Exists('$(VCTargetsPath)\\Platforms\\$(Platform)\\SCE.Makefile.$(Platform).targets')\" " +
				"Project=\"$(VCTargetsPath)\\Platforms\\$(Platform)\\SCE.Makefile.$(Platform).targets\" />\n";
		}

		public override bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFileFormat ProjectFileFormat)
		{
			bool bFoundVSI = false;
			String SCERootDir = Environment.GetEnvironmentVariable("SCE_ROOT_DIR");
			if (!String.IsNullOrEmpty(SCERootDir))
			{
				WindowsCompiler VSCompiler;

				String VSIExtPath = null;

				switch (ProjectFileFormat)
				{
					case VCProjectFileFormat.VisualStudio2015:
						VSCompiler = WindowsCompiler.VisualStudio2015;
						break;
					case VCProjectFileFormat.VisualStudio2017:
						VSCompiler = WindowsCompiler.VisualStudio2017;
						break;
					default:
						// Unknown VS Version
						return false;
				}

				DirectoryReference VSInstallDir;
				if(WindowsPlatform.TryGetVSInstallDir(VSCompiler, out VSInstallDir))
				{
					VSIExtPath = Path.Combine(VSInstallDir.FullName, "Common7\\IDE\\Extensions\\SCE\\SceVSI\\SceVSI.dll");
					bFoundVSI = File.Exists(VSIExtPath);
				}
			}
			return bFoundVSI;
		}
	}
}
