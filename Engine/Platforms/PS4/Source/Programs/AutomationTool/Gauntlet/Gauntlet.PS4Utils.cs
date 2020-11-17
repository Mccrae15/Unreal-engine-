// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace Gauntlet
{
	/// <summary>
	/// Info that we pull from the device
	/// </summary>
	public class PS4DeviceInfo
	{
		/// <summary>
		/// Name as it appears in target manager
		/// </summary>
		public string Name;

		/// <summary>
		/// Hostname / IP
		/// </summary>
		public string Hostname;

		/// <summary>
		/// is this the default kit?
		/// </summary>
		public bool Default;

		/// <summary>
		/// SDK version that the device is running
		/// </summary>
		public Version SDKVersion;

		/// <summary>
		/// Mapped drive
		/// </summary>
		public string Drive;
		public PS4DeviceInfo(string DefaultName)
		{
			Name = DefaultName;
		}
	}

	/// <summary>
	/// Utility functions used by PS4 implementations
	/// </summary>
	class PS4Utils
	{
		/// <summary>
		/// Run a PS4DevKitUtil command 
		/// </summary>
		static public IProcessResult ExecutePS4DevKitUtilCommand(String CommandLine, int WaitTime = 60)
		{
			using (var PauseEC = new ScopedSuspendECErrorParsing())
			{
				String DevKitUtilPath = Path.Combine(Globals.UE4RootDir, "Engine/Binaries/DotNET/PS4/PS4DevKitUtil.exe");

				CommandUtils.ERunOptions RunOptions = CommandUtils.ERunOptions.AppMustExist | CommandUtils.ERunOptions.NoWaitForExit;

				if (Log.IsVeryVerbose)
				{
					RunOptions |= CommandUtils.ERunOptions.AllowSpew;
				}
				else
				{
					RunOptions |= CommandUtils.ERunOptions.NoLoggingOfRunCommand;
				}

				Log.Verbose("PS4DevkitUtil executing '{0}'", CommandLine);

				IProcessResult Result = CommandUtils.Run(DevKitUtilPath, CommandLine, Options: RunOptions);

				if (WaitTime > 0)
				{
					DateTime StartTime = DateTime.Now;

					Result.ProcessObject.WaitForExit(WaitTime * 1000);

					// todo - use WaitForExit(time) directly?
					if (Result.HasExited == false)
					{
						if ((DateTime.Now - StartTime).TotalSeconds >= WaitTime)
						{
							Log.Warning("PS4DevKitUtil timeout after {0} secs: {1}", WaitTime, CommandLine);
							Result.ProcessObject.Kill();
						}
					}

					if (Result.ProcessObject != null)
					{
						Log.Verbose("Cmd completed with result {1}", CommandLine, Result.ProcessObject.ExitCode);
					}
				}

				return Result;
			}
		}

		/// <summary>
		/// Run an orbis-ctrl command
		/// </summary>
		static public IProcessResult ExecuteOrbisCtrlCommand(String CommandLine, int WaitTime = 60, bool WarnOnTimeout = true)
		{
			String OrbisCtrlPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), @"ORBIS\Tools\Target Manager Server\bin\orbis-ctrl.exe");

			if (!File.Exists(OrbisCtrlPath))
			{
				throw new AutomationException("Unable to run orbis-ctrl.exe at {0} : is PS4 SDK installed?");
			}

			CommandUtils.ERunOptions RunOptions = CommandUtils.ERunOptions.AppMustExist | CommandUtils.ERunOptions.NoWaitForExit;

			if (Log.IsVeryVerbose)
			{
				RunOptions |= CommandUtils.ERunOptions.AllowSpew;
			}
			else
			{
				RunOptions |= CommandUtils.ERunOptions.NoLoggingOfRunCommand;
			}

			Log.Verbose("OrbisCtrl executing '{0}'", CommandLine);

			IProcessResult Result = CommandUtils.Run(OrbisCtrlPath, CommandLine, Options: RunOptions);

			if (WaitTime > 0)
			{
				DateTime StartTime = DateTime.Now;

				Result.ProcessObject.WaitForExit(WaitTime * 1000);

				if (Result.HasExited == false)
				{
					if ((DateTime.Now - StartTime).TotalSeconds >= WaitTime)
					{
						string Message = String.Format("OrbisCtrl timeout after {0} secs: {1}, killing process", WaitTime, CommandLine);

						if (WarnOnTimeout)
						{
							Log.Warning(Message);
						}
						else
						{
							Log.Info(Message);
						}

						Result.ProcessObject.Kill();
						// wait up to 15 seconds for process exit
						Result.ProcessObject.WaitForExit(15000);
					}
				}
			}

			return Result;
		}

		/// <summary>
		/// Enables or disables dev kit distributed copying
		/// </summary>
		static public bool EnableDistributedCopy(bool Enabled = true)
		{
			IProcessResult Result = PS4Utils.ExecuteOrbisCtrlCommand(String.Format("dcopy {0}", Enabled ? "enable" : "disable"), 180);
			return Result.ProcessObject.ExitCode == 0;
		}

		/// <summary>
		/// Copies a file or directory recursively using the orbis-ctrl dcopy command
		/// Important Note: dcopy doesn't cancel if orbis-ctrl is killed, only command line option to interrupt an existing dcopy is to cycle connection
		/// @todo: there may be a way to cancel a dcopy, interfacing directly with Interop.ORTMAPILib.dll? PS4 Neighborhood has a Target -> Copy -> Cancel operation in UI
		/// </summary>
		static public bool DistributedCopy(string Hostname, string SourcePath, string DestPath, Utils.SystemHelpers.CopyOptions Options = Utils.SystemHelpers.CopyOptions.Copy, int RetryCount = 5)
		{
			bool IsDirectory = false;

			if (Directory.Exists(SourcePath))
			{
				IsDirectory = true;
			}

			if (!File.Exists(SourcePath) && !IsDirectory)
			{
				throw new AutomationException(String.Format("Path doesn't exist for distributed copy {0}", SourcePath));
			}

			// Setup dcopy arguments
			List<string> CopyArgs = new List<string>() { "dcopy" };

			// Source path
			CopyArgs.Add(string.Format("\"{0}\"", SourcePath.Trim(new char[] { ' ', '"' })));

			// Destination path
			CopyArgs.Add(string.Format("\"{0}\"", DestPath));

			// Directory options
			if (IsDirectory)
			{
				CopyArgs.Add("/recursive");

				if ((Options & Utils.SystemHelpers.CopyOptions.Mirror) == Utils.SystemHelpers.CopyOptions.Mirror)
				{
					CopyArgs.Add("/mirror");
				}
			}

			if (RetryCount > 0)
			{
				CopyArgs.Add(String.Format("/retry-count:{0}", Math.Min(RetryCount, 5)));
			}

			// This forces copy whether source and dest are reported matching or not...
			// (enable if issues with src/dst matches are being incorrectly identified
			// CopyArgs.Add("/force");

			// @todo: support device copy for same build to multiple devices
			CopyArgs.Add(Hostname);

			// @todo: we could generate a copy time estimate based on file sizes and gigabit copy or add option
			// This is currently set to 45 minutes due to current network issue, should be more like 15 minutes
			const int WaitTimeMinutes = 45 * 60;

			// don't warn on dcopy timeouts, these are generally due to slow network conditions and are not actionable
			// for other failures, an exception is raised
			IProcessResult Result = PS4Utils.ExecuteOrbisCtrlCommand(String.Join(" ", CopyArgs), WaitTimeMinutes, false);
			if (Result.ProcessObject.ExitCode != 0)
			{
				throw new AutomationException("orbis-ctrl dcopy failed");
			}

			return true;
		}

		static List<PS4DeviceInfo> DefaultDevices = null;

		/// <summary>
		/// Returns devices available to this system
		/// </summary>
		/// <returns></returns>
		static public PS4DeviceInfo[] GetAvailableDevices()
		{
			if (DefaultDevices == null)
			{
				IProcessResult QueryProcess = PS4Utils.ExecutePS4DevKitUtilCommand("list");

				if (QueryProcess.ProcessObject.ExitCode != 0)
				{
					throw new DeviceException("Unable to call PS4DevKitUtil list: {0}", QueryProcess.Output);
				}

				/*
					Name="AndrewGNeo"
					HostName="10.1.204.17"
					Default=False
					Name="AndrewG"
					HostName="10.1.204.22"
					Default=False
				*/

				string CleanOutput = QueryProcess.Output.Replace("\r", "");

				MatchCollection Matches = Regex.Matches(CleanOutput, @"Name=""(.+)""\nHostName=""(.+)""\nDefault=(.+)");

				DefaultDevices = new List<PS4DeviceInfo>();

				foreach (Match M in Matches)
				{
					PS4DeviceInfo Info = new PS4DeviceInfo(M.Groups[2].ToString());
					Info.Hostname = M.Groups[1].ToString();
					Info.Default = M.Groups[3].ToString() == "True";

					DefaultDevices.Add(Info);
				}
			}

			return DefaultDevices.ToArray();
		}


		static public ITargetDevice[] GetDefaultDevices()
		{
			var DeviceInfo = GetAvailableDevices();

			List<ITargetDevice> Devices = new List<ITargetDevice>();

			foreach (var Info in DeviceInfo)
			{
				TargetDevicePS4 PS4Device = new TargetDevicePS4(Info.Hostname);

				Devices.Add(PS4Device);
			}

			return Devices.ToArray();
		}

		/// <summary>
		/// Transforms a path into the unreal PS4 convention of being all lowercase, other
		/// than two types of Sony files..
		/// </summary>
		/// <param name="Path"></param>
		/// <returns></returns>
		static public string TransformPathForPS4(string Path)
		{
			if (Path.StartsWith("CUSA", StringComparison.OrdinalIgnoreCase)
				|| Path.IndexOf("sce_", StringComparison.OrdinalIgnoreCase) != -1
				)
			{
				return Path;
			}

			return Path.ToLower();
		}
	}
}
