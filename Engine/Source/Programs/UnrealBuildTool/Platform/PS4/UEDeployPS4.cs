// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

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

		private void CopyPrxDependecies(UEBuildDeployTarget InTarget)
		{
			// Copy any runtime .prx dependencies to the prx folder so the game can find it

			TargetReceipt Receipt;
			if (!TargetReceipt.TryRead(InTarget.BuildReceiptFileName, UnrealBuildTool.EngineDirectory, InTarget.ProjectDirectory, out Receipt))
			{
				return;
			}

			String App0Dir = Path.Combine(InTarget.ProjectDirectory.FullName, "Build", "PS4");

            foreach (RuntimeDependency RuntimeDependency in Receipt.RuntimeDependencies)
			{
				if (RuntimeDependency.Path.HasExtension(".prx"))
				{
					// User PRX
					string SrcFileName = RuntimeDependency.Path.FullName;
					string DestFileName = Path.Combine(App0Dir, "prx", RuntimeDependency.Path.GetFileName());
					CopyFile(SrcFileName, DestFileName);
				}
			}

			{
				try
				{
					// We need to copy the PRX's from the SDK to somewhere the game can find and load them.
					// default visual studio working directory (aka /app0) is ProjectDir/Build/PS4
					// UFE will also set this working dir.
					String BaseSDKPath = Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
					String SceModuleTargetDir = Path.Combine(App0Dir, "sce_module");
					String SceModuleSrcDir = Path.Combine(BaseSDKPath, "target", "sce_module");

					CopyFile(Path.Combine(SceModuleSrcDir, "libSceNpToolkit2.prx"), Path.Combine(SceModuleTargetDir, "libSceNpToolkit2.prx"));
				}
				catch (Exception ex)
				{
					Console.WriteLine("System PRX copy failed: " + ex.Message);
				}
			}

		}

		public override bool PrepTargetForDeployment(UEBuildDeployTarget InTarget)
		{
			// Note: Symbol generation is now done in PS4SymbolTool

			// Copy any runtime Prx dependencies to the app0\prx folder
			CopyPrxDependecies(InTarget);

			return true;
		}
	}
}
