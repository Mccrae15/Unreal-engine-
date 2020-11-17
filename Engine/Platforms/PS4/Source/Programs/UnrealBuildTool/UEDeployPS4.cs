// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using Tools.DotNETCommon;
using System.Xml;

namespace UnrealBuildTool
{
	class UEDeployPS4 : UEBuildDeploy
	{
		public static bool SafeCreateDirectory(string Path)
		{
			bool Result = true;
			try
			{
				Result = Directory.CreateDirectory(Path).Exists;
			}
			catch (Exception)
			{
				if (Directory.Exists(Path) == false)
				{
					Result = false;
				}
			}
			return Result;
		}


		private void CopyFile(string InSource, string InDest)
		{
			String DestDirName = Path.GetDirectoryName(InDest);

			if (File.Exists(InSource) == true)
			{
				if (!Directory.Exists(DestDirName))
				{
					if (!SafeCreateDirectory(DestDirName))
					{
						Log.TraceInformation("PS4CopyFile: Failed to create directory for copy - {0}", DestDirName);
						return;
					}
				}

				System.DateTime SourceTime = File.GetLastWriteTime(InSource);
				System.DateTime DestTime = File.GetLastWriteTime(InDest);
				if (SourceTime > DestTime)
				{
					try
					{
						if (File.Exists(InDest) == true)
						{
							// Delete the old dest file
							FileAttributes attributes = File.GetAttributes(InDest);
							if ((attributes & FileAttributes.ReadOnly) == FileAttributes.ReadOnly)
							{
								attributes &= ~FileAttributes.ReadOnly;
								File.SetAttributes(InDest, attributes);
							}
							File.Delete(InDest);
						}

						// Copy the file
						File.Copy(InSource, InDest.ToLower(), true);
					}
					catch (Exception exceptionMessage)
					{
						Log.TraceInformation("Failed to copy {0}: {1}", InSource, exceptionMessage);
					}
				}
			}
			else
			{
				Log.TraceInformation("PS4CopyFile: File doesn't exist - {0}", InSource);
			}
		}

		private void CopyDirectory(string InSource, string InDest)
		{
			if (Directory.Exists(InSource))
			{
				foreach (string Directory in Directory.EnumerateDirectories(InSource))
				{
					CopyDirectory(Directory, Directory.Replace(InSource, InDest));
				}

				foreach (string File in Directory.EnumerateFiles(InSource))
				{
					CopyFile(File, File.Replace(InSource, InDest));
				}
			}
		}

		private void CopyPrxDependecies(TargetReceipt Receipt, string BuildDir)
		{
			// Copy any runtime .prx dependencies to the prx folder
			foreach (RuntimeDependency RuntimeDependency in Receipt.RuntimeDependencies)
			{
				if (RuntimeDependency.Path.HasExtension(".prx"))
				{
					// User PRX
					string SrcFileName = RuntimeDependency.Path.FullName;
					string DestFileName = Path.Combine(BuildDir, "prx", RuntimeDependency.Path.GetFileName());
					CopyFile(SrcFileName, DestFileName);
				}
			}

			{
				try
				{
					// We need to copy the PRX's from the SDK
					String BaseSDKPath = Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
					String SceModuleTargetDir = Path.Combine(BuildDir, "sce_module");
					String SceModuleSrcDir = Path.Combine(BaseSDKPath, "target", "sce_module");

					// Matches PS4Platform.Automation.cs GetFilesToDeployOrStage 
					foreach (string File in Directory.EnumerateFiles(SceModuleSrcDir))
					{
						if (!File.EndsWith("_debug.prx", StringComparison.InvariantCultureIgnoreCase))
						{
							CopyFile(File, File.Replace(SceModuleSrcDir, SceModuleTargetDir));
						}
					}
				}
				catch (Exception ex)
				{
					Log.TraceInformation("System PRX copy failed: " + ex.Message);
				}
			}

		}

		private string ReadWorkingDirectoryFromVSProject(string ProjectFile, string Configuration, string TargetType)
		{
			if (File.Exists(ProjectFile))
			{
				XmlDocument Document = new XmlDocument();
				Document.Load(ProjectFile);

				XmlNamespaceManager NamespaceList = new XmlNamespaceManager(Document.NameTable);
				NamespaceList.AddNamespace("ns", "http://schemas.microsoft.com/developer/msbuild/2003");

				// Fixup the game target type to be empty.
				TargetType = TargetType.Replace("Game", "");

				string LocalDebuggerWorkingDirectoryXPathQuery = string.Format("//ns:PropertyGroup[@Condition=\"'$(Configuration)|$(Platform)'=='{0}{1}{2}|{3}'\"]/ns:LocalDebuggerWorkingDirectory", Configuration, string.IsNullOrEmpty(TargetType) ? "" : "_", TargetType, "ORBIS");

				XmlNode LocalDebuggerWorkingDirectoryNode = Document.SelectSingleNode(LocalDebuggerWorkingDirectoryXPathQuery, NamespaceList);
				if (LocalDebuggerWorkingDirectoryNode != null)
				{
					string WorkingDirectory = LocalDebuggerWorkingDirectoryNode.InnerText;
					if (!Path.IsPathRooted(WorkingDirectory))
					{
						WorkingDirectory = Path.Combine(Path.GetDirectoryName(ProjectFile), WorkingDirectory);
					}

					return Path.GetFullPath(WorkingDirectory);
				}
			}

			return "";
		}

		public override bool PrepTargetForDeployment(TargetReceipt Receipt)
		{
			// Note: Symbol generation is now done in PS4SymbolTool

			DirectoryReference ProjectDirectory = DirectoryReference.FromFile(Receipt.ProjectFile) ?? UnrealBuildTool.EngineDirectory;
			string BuildDir = Path.Combine(ProjectDirectory.FullName, "Build", "PS4");

			// Copy any runtime Prx dependencies to the Build/PS4 directory
			CopyPrxDependecies(Receipt, BuildDir);

			// Limit to only when built via visual studio/MSBuild (-FromMSBuild is defined).
			if (Environment.CommandLine.ToLower().Contains("-frommsbuild"))
			{
				// Check if there is a WorkingDirectory set in Visual Studio
				string RelativeProjectFilePath = Path.Combine("Intermediate", "ProjectFiles", Receipt.TargetName + ".vcxproj");
				string VSProject = Path.Combine(ProjectDirectory.FullName, RelativeProjectFilePath);
				if (!File.Exists(VSProject))
				{
					VSProject = Path.Combine(UnrealBuildTool.EngineDirectory.FullName, RelativeProjectFilePath);
				}

				// Try to find the WorkingDirectory from the VS user project data
				string App0Dir = ReadWorkingDirectoryFromVSProject(VSProject + ".user", Receipt.Configuration.ToString(), Receipt.TargetType.ToString());
				if (string.IsNullOrEmpty(App0Dir))
				{
					// If the WorkingDirectory was not found try the actual project
					App0Dir = ReadWorkingDirectoryFromVSProject(VSProject, Receipt.Configuration.ToString(), Receipt.TargetType.ToString());
				}

				string RelativeStagedBuildPath = Path.Combine("Saved", "StagedBuilds", "PS4");

				// Copy the build directory contents to the App0Dir if it is in the StagedBuild path or the directory already contains sce_module
				if (!App0Dir.Equals(BuildDir) && (App0Dir.EndsWith(RelativeStagedBuildPath) || Directory.Exists(Path.Combine(App0Dir, "sce_module"))))
				{
					CopyDirectory(Path.Combine(BuildDir, "sce_module"), Path.Combine(App0Dir, "sce_module"));
					CopyDirectory(Path.Combine(BuildDir, "sce_sys"), Path.Combine(App0Dir, "sce_sys"));
					CopyDirectory(Path.Combine(BuildDir, "symbols"), Path.Combine(App0Dir, "symbols"));
					CopyDirectory(Path.Combine(BuildDir, "titledata"), Path.Combine(App0Dir));
				}
			}

			return true;
		}
	}
}
