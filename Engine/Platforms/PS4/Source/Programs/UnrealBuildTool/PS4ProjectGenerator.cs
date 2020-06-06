// Copyright Epic Games, Inc. All Rights Reserved.

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
	class PS4ProjectGenerator : PlatformProjectGenerator
	{
		/// <summary>
		/// No documentation available.
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-UsePS4ApplicationDebugger", Value = "true")]
		protected bool bUseApplicationDebugger = false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Arguments">Command line arguments passed to the project generator</param>
		public PS4ProjectGenerator(CommandLineArguments Arguments)
			: base(Arguments)
		{
			XmlConfig.ApplyTo(this);
			Arguments.ApplyTo(this);
		}

		/// <summary>
		/// Enumerate all the platforms that this generator supports
		/// </summary>
		public override IEnumerable<UnrealTargetPlatform> GetPlatforms()
		{
			yield return UnrealTargetPlatform.PS4;
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

		public override void GetVisualStudioPathsEntries(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, VCProjectFileFormat InProjectFileFormat, StringBuilder ProjectFileBuilder)
		{
			// Set the executable and include directories (PATH and INCLUDE environment variables) to the same values the SCE VSI uses for Makefile projects.
			ProjectFileBuilder.AppendLine(@"		<ExecutablePath>$(SCE_ORBIS_SDK_DIR)\host_tools\bin;$(VCTargetsPath)\Platforms\ORBIS;$(PATH);</ExecutablePath>");
			ProjectFileBuilder.AppendLine(@"		<IncludePath>$(SCE_ORBIS_SDK_DIR)\host_tools\lib\clang\include;$(SCE_ORBIS_SDK_DIR)\target\include;$(SCE_ORBIS_SDK_DIR)\target\include_common;</IncludePath>");
			
			// Set a working directory so the game can find trophy and npdata files under the sce_sys directory when running from visual studio.
			String TargetDirectory = Path.GetDirectoryName(NMakeOutputPath.FullName);
			String ProjectRootDirectory = Path.GetDirectoryName(Path.GetDirectoryName(TargetDirectory));
			String WorkingDirectory = Path.Combine(ProjectRootDirectory, "Saved\\StagedBuilds\\PS4");
			String TitleId = GetTitleId(ProjectRootDirectory, WorkingDirectory);

			ProjectFileBuilder.AppendLine("		<LocalDebuggerWorkingDirectory>" + WorkingDirectory + "</LocalDebuggerWorkingDirectory>");
			if (bUseApplicationDebugger && TitleId != null )
			{
				ProjectFileBuilder.AppendLine("     <DebuggerFlavor>ORBISDebuggerApp</DebuggerFlavor>");
			}
			else
			{
				ProjectFileBuilder.AppendLine("		<DebuggerFlavor>ORBISDebugger</DebuggerFlavor>");
			}

			// always set the useful defaults for the application debugger even if it's not being used yet
			if (TitleId != null)
			{
				ProjectFileBuilder.AppendLine("     <AppDebuggerTitleId>" + TitleId + "</AppDebuggerTitleId>");
				ProjectFileBuilder.AppendLine("     <AppDebuggerExecutableLoadLocation>ExecutableLoadHost</AppDebuggerExecutableLoadLocation>");
				ProjectFileBuilder.AppendLine("     <AppDebuggerCommand>$(TargetPath)</AppDebuggerCommand>");
				ProjectFileBuilder.AppendLine("     <AppDebuggerWorkingDirectory>SystemManaged</AppDebuggerWorkingDirectory>");
			}
		}

		public override void GetUnrealVSConfigurationEntries( StringBuilder UnrealVSContent )
		{
			UnrealVSContent.Append( "    <ORBIS>" + ProjectFileGenerator.NewLine );
			UnrealVSContent.Append( "        <DebuggerName>AppDebuggerCommandArguments</DebuggerName>" + ProjectFileGenerator.NewLine );
			UnrealVSContent.Append("    </ORBIS>" + ProjectFileGenerator.NewLine );
		}


		/// <summary>
		/// Return the VSI required property import lines
		/// </summary>
		public override void GetVisualStudioTargetOverrides(UnrealTargetPlatform InPlatform, VCProjectFileFormat InProjectFileFormat, StringBuilder ProjectFileBuilder)
		{
			if (InPlatform != UnrealTargetPlatform.PS4)
			{
				throw new BuildException("Unexpected format in PS4 Proj generator");
			}

			ProjectFileBuilder.AppendLine("  <Import Condition=\"'$(ConfigurationType)' == 'Makefile' and '$(Platform)' == 'Orbis' and Exists('$(VCTargetsPath)\\Platforms\\Orbis\\SCE.Makefile.Orbis.targets')\" Project=\"$(VCTargetsPath)\\Platforms\\Orbis\\SCE.Makefile.Orbis.targets\" />");
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
					case VCProjectFileFormat.VisualStudio2017:
						VSCompiler = WindowsCompiler.VisualStudio2017;
						break;
					case VCProjectFileFormat.VisualStudio2019:
						VSCompiler = WindowsCompiler.VisualStudio2019;
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

		private string GetTitleId( string ProjectRootDirectory, string WorkingDirectory )
		{
			string TitleId = null;

			//see if it's been found already
			if (ProjectTitleIdCache.TryGetValue(ProjectRootDirectory, out TitleId))
			{
				return TitleId;
			}

			//try to find the title Id
			if (!FindTitleIdFromJson(out TitleId, WorkingDirectory))
			{
				if (!FindTitleIdFromConfig(out TitleId, ProjectRootDirectory))
				{
					//failed to find it - leave it blank
					ProjectTitleIdCache.Add(ProjectRootDirectory, null);
					return null;
				}
			}

			//remove trailing _00 if any
			int Idx = TitleId.IndexOf('_');
			if (Idx > 0)
			{
				TitleId = TitleId.Substring(0, Idx);
			}

			//add it to the cache and return
			ProjectTitleIdCache.Add(ProjectRootDirectory, TitleId);
			return TitleId;
		}


		private static bool FindTitleIdFromJson( out string TitleId, string WorkingDirectory )
		{
			TitleId = null;
			List<string> JsonFiles = new List<string>();

			string TitleDataFolder = Path.Combine(WorkingDirectory, "titledata");
			if (Directory.Exists(TitleDataFolder))
			{
				const string TitleFile = "title.json";

				//build/ps4/titledata/title.json
				string BaseJsonFile = Path.Combine(TitleDataFolder, TitleFile);
				if (File.Exists(BaseJsonFile))
				{
					JsonFiles.Add(BaseJsonFile);
				}

				//build/ps4/titledata/<titleid>/title.json
				foreach (string DirectoryEntry in Directory.GetDirectories(TitleDataFolder))
				{
					string ThisJsonFile = Path.Combine(DirectoryEntry, TitleFile);
					if (File.Exists(ThisJsonFile))
					{
						JsonFiles.Add(ThisJsonFile);
					}
				}
			}

			//parse the json files until we find a title id
			foreach( string JsonFile in JsonFiles )
			{
				try
				{
					JsonObject TitleObj = null;
					if (JsonObject.TryRead(new FileReference(JsonFile), out TitleObj))
					{
						TitleId = TitleObj.GetStringField("title_id");
						return true;
					}
				}
				catch (Exception)
				{
				}
			}

			return false;
		}

		private static bool FindTitleIdFromConfig( out string TitleId, string ProjectRootDirectory )
		{
			TitleId = null;

			ConfigHierarchy Config = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, new DirectoryReference(ProjectRootDirectory), UnrealTargetPlatform.PS4);
			if (Config != null)
			{
				string FullTitleId = null;
				if (Config.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "TitleID", out FullTitleId))
				{
					//e.g IV0000-NPXX51358_00-SHOOTERGAMEXXXXX
					string[] Parts = FullTitleId.Split('-');
					if (Parts.Length == 3)
					{
						TitleId = Parts[1];
						return true;
					}
				}
			}

			return false;
		}


		private Dictionary<string, string> ProjectTitleIdCache = new Dictionary<string, string>();

	}
}
