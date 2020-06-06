// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
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

		/// <summary>
		/// Directory to save periodic screenshots to whilst the PGO run is in progress.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string ScreenshotDirectory;

		/// <summary>
		/// Maximum amount of time to allow the PGO run to complete.
		/// If the time elapses, the PGO run is terminated with an error.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string MaxRunTime;
	}

	/// <summary>
	/// Task which strips symbols from a set of files
	/// </summary>
	[TaskElement("PS4PGOProfile", typeof(PS4PGOTaskParameters))]
	public class PS4PGOProfileTask : CustomTask
	{
		private TimeSpan ScreenshotInterval = TimeSpan.FromSeconds(30);
		private const float ScreenshotScale = 1.0f / 3.0f;
		private const int ScreenshotQuality = 30;

		// Find SDK tools
		private readonly string OrbisRunPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS\\Tools\\Target Manager Server\\bin\\orbis-run.exe");
		private readonly string OrbisCtrlPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS\\Tools\\Target Manager Server\\bin\\orbis-ctrl.exe");
		private readonly string OrbisLlvmProfdataPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ORBIS_SDK_DIR%"), "host_tools\\bin\\orbis-llvm-profdata.exe");

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
			Uri ReservationServerUri;
			if (Uri.TryCreate(Parameters.DeviceReservationPath, UriKind.Absolute, out ReservationServerUri))
			{
				Trace.TraceInformation("Acquiring PS4 dev kit using reservation directory \"{0}\".", Parameters.DeviceReservationPath);
				DeviceReservation.Reservation.ReservationDetails = "PGO Automation";
				using (var DeviceReservation = new DeviceReservation.DeviceReservationAutoRenew(Parameters.DeviceReservationPath, 5, "", "PS4-DevKit"))
				{
					RunTest(DeviceReservation.Devices.First().IPOrHostName, TagNameToFileSet);
				}
			}
			else
			{
				// Assume we've been passed an Xbox hostname/IP address directly...
				RunTest(Parameters.DeviceReservationPath, TagNameToFileSet);
			}
		}

		private bool TakeScreenshot(string OrbisCtrlPath, string Hostname, string OutputPath)
		{
			try
			{
				int ExitCode;
				Utils.RunLocalProcessAndReturnStdOut(OrbisCtrlPath, string.Format("screenshot auto \"{0}\" {1}", OutputPath, Hostname), out ExitCode);
				return ExitCode == 0 && File.Exists(OutputPath);
			}
			catch
			{
				return false;
			}
		}

		private Task RunScreenshotTask(string OutputDirectory, string OrbisCtrlPath, string Hostname, CancellationToken Token)
		{
			Task ScreenshotTask = null;
			if (string.IsNullOrWhiteSpace(OutputDirectory))
			{
				Trace.TraceInformation("No screenshot directory specified. Periodic screenshot capture will be disabled.");
			}
			else
			{
				bool bSuccess = false;
				try
				{
					if (!Directory.Exists(OutputDirectory))
						Directory.CreateDirectory(OutputDirectory);

					bSuccess = true;
				}
				catch (Exception ex)
				{
					Trace.TraceInformation("Failed to create screenshot output directory \"{0}\" ({1}). Periodic screenshot capture will be disabled.", Parameters.ScreenshotDirectory, ex.Message);
				}

				if (bSuccess)
				{
					ScreenshotTask = Task.Run(async () =>
					{
						DateTime StartTime = DateTime.UtcNow;
						var TempPath = Path.Combine(Parameters.ScreenshotDirectory, "temp.png");
						
						while (!Token.IsCancellationRequested)
						{
							if (TakeScreenshot(OrbisCtrlPath, Hostname, TempPath))
							{
								try
								{
									TimeSpan ImageTimestamp = DateTime.UtcNow - StartTime;
									string ImageOutputPath = Path.Combine(Parameters.ScreenshotDirectory, ImageTimestamp.ToString().Replace(':', '-') + ".jpg");
									ImageUtils.ResaveImageAsJpgWithScaleAndQuality(TempPath, ImageOutputPath, ScreenshotScale, ScreenshotQuality);
								}
								catch
								{
									// Just ignore errors.
								}
								finally
								{
									// Delete the temporary .png file
									try { File.Delete(TempPath); }
									catch { }
								}
							}

							// Wait for some time, or until we are cancelled...
							try { await Task.Delay(ScreenshotInterval, Token); }
							catch (TaskCanceledException) { break; }
						}
					});
				}
			}

			return ScreenshotTask;
		} 

		private string GetRunningTitlePID(string Hostname)
		{
			var Result = Utils.RunLocalProcessAndReturnStdOut(OrbisCtrlPath, string.Format("plist \"{0}\"", Hostname));
			var Lines = Result.Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);

			foreach (var Line in Lines.Select(L => L.ToLower().Trim()))
			{
				var Index = Line.IndexOf("pid:");
				if (Index != -1)
				{
					var ID = Line.Substring(Index + 4);
					return ID.Trim();
				}
			}

			return null;
		}

		private void RunTest(string Hostname, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			Trace.TraceInformation("Using PS4 \"{0}\".", Hostname);

			// Find the PGO instrumented .self file from the input parameters.
			var SelfFile = ResolveFilespec(CommandUtils.RootDirectory, Parameters.Executable, TagNameToFileSet).Where(x => x.HasExtension(".self")).FirstOrDefault();
			if (SelfFile == null)
				throw new AutomationException("No PS4 self file specified for PGO profiling.");
			
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

			// Force disconnect anyone already using the dev kit
			Trace.TraceInformation("Forcing disconnect of any existing users connected to target PS4 \"{0}\"...", Hostname);
			ReturnCode = Utils.RunLocalProcessAndLogOutput(new ProcessStartInfo(OrbisCtrlPath, string.Format("force-disconnect \"{0}\"", Hostname)));
			if (ReturnCode != 0)
			{
				throw new AutomationException("Failed to force-disconnect PS4 \"{0}\". Error code {1}", Hostname, ReturnCode);
			}

			// Connect to the target PS4
			Trace.TraceInformation("Connecting to target PS4 \"{0}\"...", Hostname);
			ReturnCode = Utils.RunLocalProcessAndLogOutput(new ProcessStartInfo(OrbisCtrlPath, string.Format("connect \"{0}\"", Hostname)));
			if (ReturnCode != 0)
			{
				throw new AutomationException("Failed to connect to PS4 \"{0}\". Error code {1}", Hostname, ReturnCode);
			}

			CancellationTokenSource TokenSource = new CancellationTokenSource();
			CancellationToken Token = TokenSource.Token;

			try
			{
				// Generate the required command line to run a PGO profiling pass.
				string GameCommandLine = string.Format("{0} -pgoprofile=\"{1}\"", Parameters.CommandLine, PS4OutputDirectory);
				string Args = string.Format("/t:{0} /console:process /workingdirectory:\"{1}\" /elf \"{2}\" {3}", Hostname, Parameters.WorkingDirectory, SelfFile, GameCommandLine);

				// Start an async task to take screenshots every n seconds
				Task ScreenshotTask = RunScreenshotTask(Parameters.ScreenshotDirectory, OrbisCtrlPath, Hostname, Token);

				// Launch the .self and wait for exit.
				Trace.TraceInformation("Launching game via command line: {0} {1}", OrbisRunPath, Args);
				using(ManagedProcessGroup ProcessGroup = new ManagedProcessGroup())
				{
					using (var OrbisRunProc = new ManagedProcess(ProcessGroup, OrbisRunPath, Args, Environment.CurrentDirectory, null, null, ProcessPriorityClass.Normal))
					{
						var LoggingTask = Task.Run(() =>
						{
							string Line;
							while (OrbisRunProc.TryReadLine(out Line, Token))
							{
								string[] TriggersSrc = { "Warning:", "Error:", "Exception:" };
								string[] TriggersDst = { "Warn1ng:", "Err0r:", "Except10n:" };

								for (int Index = 0; Index < TriggersSrc.Length; ++Index)
								{
									if (Line.IndexOf(TriggersSrc[Index], StringComparison.OrdinalIgnoreCase) != -1)
									{
										Line = Regex.Replace(Line, TriggersSrc[Index], TriggersDst[Index], RegexOptions.IgnoreCase);
									}
								}

								Trace.TraceInformation("PS4: {0}", Line.Trim());
							}
						});

						TimeSpan MaxRunTime;					
						if (!string.IsNullOrWhiteSpace(Parameters.MaxRunTime) && TimeSpan.TryParse(Parameters.MaxRunTime, out MaxRunTime))
						{
							if (!Task.WaitAll(new Task[] { LoggingTask }, MaxRunTime))
							{
								// We timed out. Terminate the title on the dev kit...
								Trace.TraceInformation("PGO run timed out...");

								var ID = GetRunningTitlePID(Hostname);
								if (ID == null)
								{
									throw new AutomationException("PGO run took longer than the allowed maximum time limit ({0}), and unable to find process to terminate it.", MaxRunTime);
								}

								Trace.TraceInformation("Terminating process {0}...", ID);
								Utils.RunLocalProcessAndLogOutput(new ProcessStartInfo(OrbisCtrlPath, string.Format("pkill \"{0}\" \"{1}\"", ID, Hostname)));
								throw new AutomationException("PGO run took longer than the allowed maximum time limit ({0}). Aborting...", MaxRunTime);
							}
						}
						else
						{
							// No timeout specified, wait indefinitely...
							Task.WaitAll(LoggingTask);
						}

						if (OrbisRunProc.ExitCode != 0)
						{
							throw new AutomationException("Process exited with error code {0}", OrbisRunProc.ExitCode);
						}
					}
				}

				// Cancel outstanding tasks and sync
				TokenSource.Cancel();
				if (ScreenshotTask != null)
				{
					ScreenshotTask.Wait();
				}

				// Gather results and merge PGO data
				Trace.TraceInformation("PS4 process exited. Gathering profiling results...");
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
				// Be sure we've cancelled the async tasks.
				TokenSource.Cancel();

				// Disconnect from the target PS4
				Trace.TraceInformation("Disconnecting from target PS4 \"{0}\"...", Hostname);
				ReturnCode = Utils.RunLocalProcessAndLogOutput(new ProcessStartInfo(OrbisCtrlPath, string.Format("disconnect \"{0}\"", Hostname)));
				if (ReturnCode != 0)
				{
					Trace.TraceError("Failed to disconnect from PS4 \"{0}\". Error code {1}", Hostname, ReturnCode);
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
