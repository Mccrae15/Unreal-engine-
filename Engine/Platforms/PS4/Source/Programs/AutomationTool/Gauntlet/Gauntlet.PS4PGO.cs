// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using System.Diagnostics;
using AutomationTool;
using UnrealBuildTool;

namespace Gauntlet
{
	internal class PS4PGOPlatform : IPGOPlatform
	{
		private readonly string OrbisLlvmProfdataPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ORBIS_SDK_DIR%"), "host_tools\\bin\\orbis-llvm-profdata.exe");

		string LocalOutputDirectory;
		string LocalProfDataFile;

		public UnrealTargetPlatform GetPlatform()
		{
			return UnrealTargetPlatform.PS4;
		}

		public void GatherResults(string ArtifactPath)
		{
			var ProfRawFiles = Directory.GetFiles(LocalOutputDirectory, "*.profraw");
			if (ProfRawFiles.Length == 0)
			{
				throw new AutomationException(string.Format("Process exited cleanly but no .profraw PGO files were found in the output directory \"{0}\".", LocalOutputDirectory));
			}

			StringBuilder MergeCommandBuilder = new StringBuilder();
			foreach (var ProfRawFile in ProfRawFiles)
			{
				MergeCommandBuilder.AppendFormat(" \"{0}\"", ProfRawFile);
			}

			// Merge multiple .profraw files into the compiler format .profdata file
			if (File.Exists(LocalProfDataFile))
			{
				new FileInfo(LocalProfDataFile).IsReadOnly = false;
			}

			int ReturnCode = UnrealBuildTool.Utils.RunLocalProcessAndLogOutput(new ProcessStartInfo(OrbisLlvmProfdataPath, string.Format("merge{0} -o \"{1}\"", MergeCommandBuilder, LocalProfDataFile)));

			if (ReturnCode != 0)
			{
				throw new AutomationException(string.Format("{0} failed to merge profraw data. Error code {1}.", Path.GetFileName(OrbisLlvmProfdataPath), ReturnCode));
			}

			// Check the profdata file exists
			if (!File.Exists(LocalProfDataFile))
			{
				throw new AutomationException(string.Format("Profraw data merging completed, but the profdata output file (\"{0}\") was not found.", LocalProfDataFile));
			}

		}

		public void ApplyConfiguration(PGOConfig Config)
		{
			LocalOutputDirectory = Config.ProfileOutputDirectory;
			LocalProfDataFile = Path.Combine(LocalOutputDirectory, "profile.profdata");

			UnrealTestRole ClientRole = Config.RequireRole(UnrealTargetRole.Client);
			ClientRole.CommandLine += string.Format(" -novsync -pgoprofile=\"{0}\"", "/host/" + Config.ProfileOutputDirectory);
		}

		public bool TakeScreenshot(ITargetDevice Device, string ScreenshotDirectory, out string ImageFilename)
		{
			ImageFilename = "";

			// Get device for screenshot
			TargetDevicePS4 PS4Device = Device as TargetDevicePS4;

			if (PS4Device == null)
			{
				return false;
			}

			ImageFilename = "temp.png";
			return PS4Device.TakeScreenshot(Path.Combine(ScreenshotDirectory, ImageFilename));
		}
	}
}



