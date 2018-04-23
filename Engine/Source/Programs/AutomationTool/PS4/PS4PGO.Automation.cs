// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the PGO profiling task, which generates PGO data from an instrumented PS4 build.
	/// </summary>
	public class PS4PGOTaskParameters
	{
		/// <summary>
		/// Path to the device reservation working directory.
		/// </summary>
		[TaskParameter]
		public string DeviceReservationPath;

		/// <summary>
		/// Path to the instrumented PS4 .self executable file.
		/// </summary>
		[TaskParameter]
		public string Executable;

		/// <summary>
		/// Working directory to use when running the PS4 .self (i.e. the staging directory).
		/// </summary>
		[TaskParameter]
		public string WorkingDirectory;

		/// <summary>
		/// Output directory to write the resulting profile data to.
		/// </summary>
		[TaskParameter]
		public string OutputDirectory;

		/// <summary>
		/// Command line arguments to pass to the game executable.
		/// </summary>
		[TaskParameter]
		public string CommandLine;
	}

	/// <summary>
	/// Task which strips symbols from a set of files
	/// </summary>
	[TaskElement("PS4PGOProfile", typeof(PS4PGOTaskParameters))]
	public class PS4PGOProfileTask : CustomTask
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		PS4PGOTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="Parameters">Parameters for the task</param>
		public PS4PGOProfileTask(PS4PGOTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override void Execute(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			Trace.TraceInformation("Acquiring PS4 dev kit using reservation directory \"{0}\".", Parameters.DeviceReservationPath);
			using (var DeviceReservation = new DeviceReservation.DeviceReservationAutoRenew(Parameters.DeviceReservationPath, "PS4-DevKit"))
			{
				string Hostname = DeviceReservation.Devices.First().IPOrHostName;
				Trace.TraceInformation("Using PS4 \"{0}\".", Hostname);

				// Find the PGO instrumented .self file from the input parameters.
				var SelfFile = ResolveFilespec(CommandUtils.RootDirectory, Parameters.Executable, TagNameToFileSet).Where(x => x.HasExtension(".self")).FirstOrDefault();
				if (SelfFile == null)
					throw new AutomationException("No PS4 self file specified for PGO profiling.");

				// Find SDK tools
				string OrbisRunPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS\\Tools\\Target Manager Server\\bin\\orbis-run.exe");
				string OrbisCtrlPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS\\Tools\\Target Manager Server\\bin\\orbis-ctrl.exe");
				string OrbisLlvmProfdataPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ORBIS_SDK_DIR%"), "host_tools\\bin\\orbis-llvm-profdata.exe");

				// Get output filenames and delete existing data.
				string LocalOutputDirectory = Path.GetFullPath(Parameters.OutputDirectory);
				string LocalProfDataFile = Path.Combine(LocalOutputDirectory, "profile.profdata");
				string PS4OutputDirectory = "/host/" + LocalOutputDirectory;

				// Clean the output directory
				if (Directory.Exists(LocalOutputDirectory))
					Directory.Delete(LocalOutputDirectory, true);

				Directory.CreateDirectory(LocalOutputDirectory);

				// Add PS4 to dev kit list
				Trace.TraceInformation("Adding target PS4 \"{0}\"...", Hostname);
				int ReturnCode = Utils.RunLocalProcessAndLogOutput(new ProcessStartInfo(OrbisCtrlPath, string.Format("add \"{0}\"", Hostname)));
				if (ReturnCode != 0)
				{
					throw new AutomationException("Failed to add PS4 \"{0}\" to this machine's dev kit list. Error code {1}", Hostname, ReturnCode);
				}

				// Connect to the target PS4
				Trace.TraceInformation("Connecting to target PS4 \"{0}\"...", Hostname);
				ReturnCode = Utils.RunLocalProcessAndLogOutput(new ProcessStartInfo(OrbisCtrlPath, string.Format("connect \"{0}\"", Hostname)));
				if (ReturnCode != 0)
				{
					throw new AutomationException("Failed to connect to PS4 \"{0}\". Error code {1}", Hostname, ReturnCode);
				}

				try
				{
					// Generate the required command line to run a PGO profiling pass.
					string GameCommandLine = string.Format("{0} -pgoprofile=\"{1}\"", Parameters.CommandLine, PS4OutputDirectory);
					string Args = string.Format("/t:{0} /console:process /workingdirectory:\"{1}\" /elf \"{2}\" {3}", Hostname, Parameters.WorkingDirectory, SelfFile, GameCommandLine);

					// Launch the .self and wait for exit.
					Trace.TraceInformation("Launching game via command line: {0} {1}", OrbisRunPath, Args);
					ReturnCode = Utils.RunLocalProcess(new Process() { StartInfo = new ProcessStartInfo(OrbisRunPath, Args) });
					if (ReturnCode != 0)
					{
						throw new AutomationException("Process exited with error code {0}", ReturnCode);
					}

					// Gather results and merge PGO data
					Trace.TraceInformation("PS4 process exited. Gathering profiling results...");
					var ProfRawFiles = Directory.GetFiles(LocalOutputDirectory, "*.profraw");
					if (ProfRawFiles.Length == 0)
					{
						throw new AutomationException(string.Format("Process exited cleanly but no .profraw PGO files were not found in the output directory \"{0}\".", LocalOutputDirectory));
					}

					StringBuilder MergeCommandBuilder = new StringBuilder();
					foreach (var ProfRawFile in ProfRawFiles)
					{
						MergeCommandBuilder.AppendFormat(" \"{0}\"", ProfRawFile);
					}

					// Merge multiple .profraw files into the compiler format .profdata file
					ReturnCode = Utils.RunLocalProcessAndLogOutput(new ProcessStartInfo(OrbisLlvmProfdataPath, string.Format("merge{0} -o \"{1}\"", MergeCommandBuilder, LocalProfDataFile)));
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
				finally
				{
					// Disconnect from the target PS4
					Trace.TraceInformation("Disconnecting from target PS4 \"{0}\"...", Hostname);
					ReturnCode = Utils.RunLocalProcessAndLogOutput(new ProcessStartInfo(OrbisCtrlPath, string.Format("disconnect \"{0}\"", Hostname)));
					if (ReturnCode != 0)
					{
						Trace.TraceError("Failed to disconnect from PS4 \"{0}\". Error code {1}", Hostname, ReturnCode);
					}
				}
			}
		}


		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			foreach (string TagName in FindTagNamesFromFilespec(Parameters.Executable))
			{
				yield return TagName;
			}
			foreach (string TagName in FindTagNamesFromFilespec(Parameters.WorkingDirectory))
			{
				yield return TagName;
			}
			foreach (string TagName in FindTagNamesFromFilespec(Parameters.OutputDirectory))
			{
				yield return TagName;
			}
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}
	}
}
