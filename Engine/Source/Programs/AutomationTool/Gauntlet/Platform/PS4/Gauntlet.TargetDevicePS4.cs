// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Linq;

namespace Gauntlet
{
	class PS4AppInstance : LocalAppProcess
	{
		protected PS4AppInstall Install;

		public PS4AppInstance(PS4AppInstall InInstall, IProcessResult InProcess)
			: base(InProcess, InInstall.CommandArguments)
		{
			Install = InInstall;
		}

		public override string ArtifactPath
		{
			get
			{
				return Install.ArtifactPath;
			}
		}

		public override ITargetDevice Device
		{
			get
			{
				return Install.Device;
			}
		}
	}

	class PS4AppInstall : IAppInstall
	{
		public string Name { get; private set; }

		public string LocalPath;

		public string ExecutablePath;

		public string WorkingDir;

		public string CommandArguments;

		public string ArtifactPath;

		public TargetDevicePS4 PS4Device { get; private set; }
	
		public ITargetDevice Device {  get { return PS4Device;  } }

		public CommandUtils.ERunOptions RunOptions { get; set; }

		public PS4AppInstall(string InName, TargetDevicePS4 InDevice)
		{
			Name = InName;
			PS4Device = InDevice;
			this.RunOptions = CommandUtils.ERunOptions.NoWaitForExit;
		}

		public IAppInstance Run()
		{
			return PS4Device.Run(this);
		}
	}

	/// <summary>
	/// Represents a class capable of creating devices of a specific type
	/// </summary>
	public class PS4DeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform Platform)
		{
			return Platform == UnrealTargetPlatform.PS4;
		}

		public ITargetDevice CreateDevice(string InRef, string InParam)
		{
			return new TargetDevicePS4(InRef);
		}
	}

	/// <summary>
	/// PS4 implementation of a device that can run applications
	/// </summary>
	public class TargetDevicePS4 : ITargetDevice
	{ 
		public class DeviceInfo
		{
			public string Name;
			public string Hostname;
			public bool Default;
			public Version SDKVersion;
			public string Drive;
			public DeviceInfo(string DefaultName)
			{
				Name = DefaultName;
			}
		}
		
		public class StateInfo
		{
			// These names map to values in the PS4 TMAPI - don't change them!
			public enum ePowerStatus
			{
				POWER_STATUS_UNKNOWN = 0,
				POWER_STATUS_ON = 2,
				POWER_STATUS_STANDBY = 4,
				POWER_STATUS_SUSPEND = 8,
				POWER_STATUS_MAIN_ON_STANDBY = 16,
				POWER_STATUS_OFF = 32
			}

			public enum eConnectionState
			{
				CONNECTION_AVAILABLE = 0,
				CONNECTION_CONNECTED = 1,
				CONNECTION_IN_USE = 2,
				CONNECTION_UNAVAILABLE,		// not part of Sony enums but used
			};
						
			public bool				Connected;
			public ePowerStatus		PowerStatus;
			public eConnectionState	ConnectionState;
			
			public StateInfo()
			{
				ConnectionState = eConnectionState.CONNECTION_UNAVAILABLE;
				PowerStatus = ePowerStatus.POWER_STATUS_UNKNOWN;
			}
		}

		protected DeviceInfo		StaticDeviceInfo;
		protected StateInfo			CachedStateInfo;
        /// <summary>
        /// Our mappings of Intended directories to where they actually represent on this platform.
        /// </summary>

        protected Dictionary<EIntendedBaseCopyDirectory, string> LocalDirectoryMappings { get; set; }

        public string Name
		{
			get
			{
				// should always be valid, and either the refname till retrieved or actual name
				return StaticDeviceInfo.Name;
			}
		}

		public string DeviceName { get { return StaticDeviceInfo.Name; } }


        public StateInfo CurrentStateInfo
		{
			get
			{
				if (CachedStateInfo == null || (DateTime.Now - LastInfoTime).TotalSeconds > 60.0f)
				{
					QueryInfo();
					LastInfoTime = DateTime.Now;
				}

				return CachedStateInfo;
			}
		}

		public bool IsValid
		{
			get
			{
				return CurrentStateInfo.ConnectionState != StateInfo.eConnectionState.CONNECTION_UNAVAILABLE;
			}
		}

		public bool IsAvailable
		{
			get
			{
				// mark as available if we're already connected!
				return IsConnected || CurrentStateInfo.ConnectionState == StateInfo.eConnectionState.CONNECTION_AVAILABLE;
			}
		}

		public bool IsConnected
		{
			get
			{
				return CurrentStateInfo.ConnectionState == StateInfo.eConnectionState.CONNECTION_CONNECTED;
			}
		}

		public string DataPath
		{
			get
			{
				if (CurrentStateInfo.PowerStatus != StateInfo.ePowerStatus.POWER_STATUS_ON)
				{
					throw new DeviceException("Cannot retrieve path for powered-off kit {0}", StaticDeviceInfo.Name);
				}

				// The user may have mapped their kit by Name or hostname, figure out which...
				if (Directory.Exists(Path.Combine(StaticDeviceInfo.Drive, StaticDeviceInfo.Name)))
				{
					return Path.Combine(StaticDeviceInfo.Drive, StaticDeviceInfo.Name, "data").ToLower();
				}
				else if (Directory.Exists(Path.Combine(StaticDeviceInfo.Drive, StaticDeviceInfo.Hostname)))
				{
					return Path.Combine(StaticDeviceInfo.Drive, StaticDeviceInfo.Hostname, "data").ToLower();
				}
				else
				{
					throw new DeviceException("Unable to find data path for {0}", StaticDeviceInfo.Name);
				}
			}
		}

		protected string ReferenceName;

		protected DateTime LastInfoTime = DateTime.MinValue;

		protected bool RemoveOnDestruction;
		protected bool DeviceIsValid;

		public TargetDevicePS4(string InReferenceName)
		{
			ReferenceName = InReferenceName;
			RunOptions = CommandUtils.ERunOptions.NoWaitForExit | CommandUtils.ERunOptions.NoLoggingOfRunCommand;

			StaticDeviceInfo = new DeviceInfo(ReferenceName);
			CachedStateInfo = new StateInfo();

            SetUpDirectoryMappings();

			DeviceInfo[] DefaultDevices = GetAvailableDevices();

			// If this is not in the default device list for theis machine then we'll remove it when done
			if (InReferenceName.Equals("default", StringComparison.OrdinalIgnoreCase) == false)
			{
				// Any devices with a name or IP that match this? If so don't remove
				RemoveOnDestruction = DefaultDevices
					.Count(D => D.Name.Equals(InReferenceName, StringComparison.OrdinalIgnoreCase)
					|| D.Hostname.Equals(InReferenceName, StringComparison.OrdinalIgnoreCase)) == 0;
			}

			SetupDevice();
		}

		~TargetDevicePS4()
		{
			Dispose(false);
		}

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				if (disposing)
				{
					// TODO: dispose managed state (managed objects).
				}

				if (RemoveOnDestruction)
				{
					RemoveDevice();
				}

				disposedValue = true;
			}
		}

		public void Dispose()
		{
			// Do not change this code. Put cleanup code in Dispose(bool disposing) above.
			Dispose(true);
		}
		#endregion

		public CommandUtils.ERunOptions RunOptions { get; set; }

		static List<DeviceInfo> DefaultDevices = null;

		static protected DeviceInfo[] GetAvailableDevices()
		{
			if (DefaultDevices == null)
			{
				IProcessResult QueryProcess = ExecutePS4DevKitUtilCommand("list");

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

				DefaultDevices = new List<DeviceInfo>();

				foreach (Match M in Matches)
				{
					DeviceInfo Info = new DeviceInfo(M.Groups[2].ToString());
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

		bool ThrowIfRecursive = false;

		bool QueryInfo(bool LogOutput=false)
		{
			if (ThrowIfRecursive)
			{
				throw new DeviceException("QueryInfo went recursive!");
			}

			ThrowIfRecursive = true;

			DeviceInfo NewDeviceInfo = new DeviceInfo(ReferenceName);
			StateInfo NewStateInfo = new StateInfo();
					
			try
			{
				IProcessResult QueryProcess = ExecutePS4DevKitUtilCommand(string.Format("detail -target=\"{0}\"", ReferenceName));
				
				if (QueryProcess.ProcessObject.ExitCode != 0)
				{
					throw new DeviceException("Unable to resolve PS4 details for '{0}'. PS4DevKitUtil returned:\r\n{1}", ReferenceName, QueryProcess.Output);
				}

				/*
					Name="AndrewG"
					Default=True
					PowerStatus=POWER_STATUS_ON
					ConnectionState=CONNECTION_AVAILABLE
					SDKVersion=4.008.071
					HostName=10.1.104.106
					MappedDrive=U
				*/

				string CleanOutput = QueryProcess.Output.Replace("\r", "");

				if (LogOutput)
				{
					Log.Verbose("{0}", CleanOutput);
				}

				NewDeviceInfo.Name = Regex.Match(CleanOutput, @"Name=""(.+)""").Groups[1].Value;
				NewDeviceInfo.Hostname = Regex.Match(CleanOutput, @"HostName=(.+)").Groups[1].Value;

				if (String.IsNullOrEmpty(NewDeviceInfo.Name))
				{
					NewDeviceInfo.Name = StaticDeviceInfo.Hostname;
				}

				NewDeviceInfo.Drive = Regex.Match(CleanOutput, @"MappedDrive=(.+)").Groups[1].Value;
				NewDeviceInfo.Drive += @":\";

				if (string.IsNullOrEmpty(NewDeviceInfo.Drive))
				{
					throw new DeviceException("Unable to find mapped neighborhood path for device {0}", ReferenceName);
				}

				string PowerStatus = Regex.Match(CleanOutput, @"PowerStatus=(.+)").Groups[1].Value;
				string ConnectionState = Regex.Match(CleanOutput, @"ConnectionState=(.+)").Groups[1].Value;

				NewStateInfo.PowerStatus = (StateInfo.ePowerStatus)Enum.Parse(typeof(StateInfo.ePowerStatus), PowerStatus);
				NewStateInfo.ConnectionState = (StateInfo.eConnectionState)Enum.Parse(typeof(StateInfo.eConnectionState), ConnectionState);

				if (NewStateInfo.PowerStatus == StateInfo.ePowerStatus.POWER_STATUS_ON)
				{
					Match VersionMatch = Regex.Match(CleanOutput, @"SDKVersion=(\d+)\.(\d+)\.(\d+)");

					NewDeviceInfo.SDKVersion = new Version(Convert.ToInt32(VersionMatch.Groups[1].Value), Convert.ToInt32(VersionMatch.Groups[2].Value), Convert.ToInt32(VersionMatch.Groups[3].Value));
				}
			}
			catch (Exception Ex)
			{
				Log.Info("Error querying status of {0}, marking as unavailable: {1}", ReferenceName, Ex.ToString());

				NewDeviceInfo.Name = ReferenceName;
				NewStateInfo.PowerStatus = StateInfo.ePowerStatus.POWER_STATUS_UNKNOWN;
				NewStateInfo.ConnectionState = StateInfo.eConnectionState.CONNECTION_UNAVAILABLE;
			}
			finally
			{
				StaticDeviceInfo = NewDeviceInfo;
				CachedStateInfo = NewStateInfo;

				ThrowIfRecursive = false;
			}

			return CachedStateInfo.PowerStatus != StateInfo.ePowerStatus.POWER_STATUS_UNKNOWN;
		}

		void RemoveDevice()
		{
			// use t
			ExecutePS4DevKitUtilCommand(string.Format("remove -target=\"{0}\"", this.Name));
		}

		void SetupDevice()
		{
			IProcessResult QueryProcess;

			DeviceIsValid = false;

			if (ReferenceName.Contains("."))
			{
				// passed by IP, add this and resolve it.
				Log.Verbose("Attempting to add device {0} to PS4 Neighborhood", ReferenceName);

				QueryProcess = ExecutePS4DevKitUtilCommand(string.Format("add -target=\"{0}\"", ReferenceName));

				if (QueryProcess.ProcessObject.ExitCode != 0)
				{
					Log.Info("Unable to add PS4 {0}, kit will be unavailable. PS4DevKitUtil returned:\r\n{1}", ReferenceName, QueryProcess.Output);
				}
				else
				{
					Log.Verbose("Added Device {0}", ReferenceName);

					// Wait 5 secs as it seems the drive can take a little time to mount...
					Thread.Sleep(5000);

					DeviceIsValid = true;
				}
			}
			else
			{
				// passed by name, assume it's already in TM
				Log.Verbose("Using existing device {0} from PS4 Neighborhood", ReferenceName);
				DeviceIsValid = true;
			}

			if (DeviceIsValid == false)
			{
				throw new DeviceException("Could not add device with reference {0}", ReferenceName);
			}
			
			bool QueriedInfo = false;
			int AttemptsRemaining = 2;

			do
			{
				QueryInfo(true);

				if (CurrentStateInfo.PowerStatus == StateInfo.ePowerStatus.POWER_STATUS_UNKNOWN)
				{
					// possibly still mounting, or it's in use by another task and mid-reboot...
					AttemptsRemaining = AttemptsRemaining - 1;
					if (AttemptsRemaining > 0)
					{
						Log.Info("Device returned unknown power status. Assuming it's still mounting or rebooting. Attempts left={0}", AttemptsRemaining);
						Thread.Sleep(5000);
					}
					else
					{
						throw new DeviceException("Device {0} could not be queried for DynamicInfo. May need a reboot", ReferenceName);
					}
				}
				else
				{
					Log.Verbose("Queried details of device {0}", ReferenceName);
					QueriedInfo = true;
				}
			} while (QueriedInfo == false && AttemptsRemaining > 0);

			// If not currently connected, boot any existing app connections, inflight distributed copies, and zombie connections from other sources 
			if (QueriedInfo == true 
				&& IsAvailable == true 
				&& IsConnected == false)
			{
				Log.Verbose("Device should be available, booting other connections to be sure...");
				ForceDisconnect();
			}
		}

        void SetUpDirectoryMappings()
        {
            LocalDirectoryMappings = new Dictionary<EIntendedBaseCopyDirectory, string>();
        }

		/// <summary>
		/// Run a PS4DevKitUtil command 
		/// </summary>
		static protected IProcessResult ExecutePS4DevKitUtilCommand(String CommandLine, int WaitTime = 60)
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
		static protected IProcessResult ExecuteOrbisCtrlCommand(String CommandLine, int WaitTime = 60, bool WarnOnTimeout = true)
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


		public UnrealTargetPlatform Platform {  get { return UnrealTargetPlatform.PS4; } }

		public bool IsOn { get { return CurrentStateInfo.PowerStatus == StateInfo.ePowerStatus.POWER_STATUS_ON; } }

		public bool PowerOn()
		{
			IProcessResult Result = ExecutePS4DevKitUtilCommand(string.Format("poweron -target=\"{0}\"", StaticDeviceInfo.Name), 120);
			CachedStateInfo = null;
			return Result.ProcessObject.ExitCode == 0;
		}
		public bool PowerOff()
		{
			IProcessResult Result = ExecutePS4DevKitUtilCommand(string.Format("poweroff -target=\"{0}\"", StaticDeviceInfo.Name));
			if (CachedStateInfo != null)
			{
				CachedStateInfo.PowerStatus = StateInfo.ePowerStatus.POWER_STATUS_OFF;
			}
			return Result.ProcessObject.ExitCode == 0;
		}

		public bool Reboot()
		{
			IProcessResult Result = ExecutePS4DevKitUtilCommand(string.Format("reboot -target=\"{0}\"", StaticDeviceInfo.Name), 120);
			CachedStateInfo = null;
			return Result.ProcessObject.ExitCode == 0;
		}

		public bool Connect()
		{
			IProcessResult Result = ExecutePS4DevKitUtilCommand(string.Format("connect -target=\"{0}\"", StaticDeviceInfo.Name));
			if (Result.ProcessObject.ExitCode == 0)
			{
				CurrentStateInfo.ConnectionState = StateInfo.eConnectionState.CONNECTION_CONNECTED;
			}
			else
			{
				CachedStateInfo = null;
			}
			return Result.ProcessObject.ExitCode == 0;
		}

		public bool Disconnect()
		{
			IProcessResult Result = ExecutePS4DevKitUtilCommand(string.Format("disconnect -target=\"{0}\"", StaticDeviceInfo.Name));
			CachedStateInfo = null;
			return Result.ProcessObject.ExitCode == 0;
		}

		/// <summary>
		/// Disconnect all applications from the devit
		/// </summary>
		public bool ForceDisconnect()
		{
			IProcessResult Result = ExecuteOrbisCtrlCommand(string.Format("force-disconnect \"{0}\"", StaticDeviceInfo.Name));
			CachedStateInfo = null;
			return Result.ProcessObject.ExitCode == 0;
		}

		public bool SetSetting(string SettingName, string NewValue)
		{
			IProcessResult Result = ExecutePS4DevKitUtilCommand(string.Format("setsetting -target=\"{0}\" -setting=\"{1}\" -value=\"{2}\"", StaticDeviceInfo.Name, SettingName, NewValue));
			CachedStateInfo = null;
			return Result.ProcessObject.ExitCode == 0;
		}

		public override string ToString()
		{
			return StaticDeviceInfo.Name;
		}

        
        public void PopulateDirectoryMappings(string BasePath, string ProjectDir)
        {
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Binaries, Path.Combine(ProjectDir, "binaries"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Config, Path.Combine(ProjectDir, "saved", "config"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Content, Path.Combine(ProjectDir, "content"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Demos, Path.Combine(ProjectDir, "saved", "demos"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Profiling, Path.Combine(ProjectDir, "saved", "profiling"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Saved, Path.Combine(ProjectDir, "saved"));
        }
		/// <summary>
		/// Enables or disables dev kit distributed copying
		/// </summary>
		protected bool EnableDistributedCopy(bool Enabled = true)
		{
			IProcessResult Result = ExecuteOrbisCtrlCommand(String.Format("dcopy {0}", Enabled ? "enable" : "disable"), 180);
			return Result.ProcessObject.ExitCode == 0;
		}

		/// <summary>
		/// Copies a file or directory recursively using the orbis-ctrl dcopy command
		/// Important Note: dcopy doesn't cancel if orbis-ctrl is killed, only command line option to interrupt an existing dcopy is to cycle connection
		/// @todo: there may be a way to cancel a dcopy, interfacing directly with Interop.ORTMAPILib.dll? PS4 Neighborhood has a Target -> Copy -> Cancel operation in UI
		/// </summary>
		protected bool DistributedCopy(string SourcePath, string DestPath, Utils.SystemHelpers.CopyOptions Options = Utils.SystemHelpers.CopyOptions.Copy, int RetryCount = 5)
		{
			bool IsDirectory = false;

			if (Directory.Exists(SourcePath))
			{
				IsDirectory = true;
			}

			if (!File.Exists(SourcePath) && !IsDirectory)
			{
				throw new AutomationException(String.Format("Path doesn't exist for distributed copy {0}",SourcePath));
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
			CopyArgs.Add(StaticDeviceInfo.Hostname);

			// @todo: we could generate a copy time estimate based on file sizes and gigabit copy or add option
			// This is currently set to 45 minutes due to current network issue, should be more like 15 minutes
			const int WaitTimeMinutes = 45 * 60;

			// don't warn on dcopy timeouts, these are generally due to slow network conditions and are not actionable
			// for other failures, an exception is raised
			IProcessResult Result = ExecuteOrbisCtrlCommand(String.Join(" ", CopyArgs), WaitTimeMinutes, false);
			if (Result.ProcessObject.ExitCode != 0)
			{
				throw new AutomationException("orbis-ctrl dcopy failed");
			}

			return true;
		}


		/// <summary>
		/// Transforms a path into the unreal PS4 convention of being all lowercase, other
		/// than two types of Sony files..
		/// </summary>
		/// <param name="Path"></param>
		/// <returns></returns>
		static string TransformPathForPS4(string Path)
		{
			if (Path.StartsWith("CUSA", StringComparison.OrdinalIgnoreCase)
				|| Path.IndexOf("sce_", StringComparison.OrdinalIgnoreCase) != -1
				)
			{
				return Path;
			}

			return Path.ToLower();
		}

		/// <summary>
		/// Install application to PS4 kit using orbis-ctrl dcopy command to avoid known issues with mapped drive copies
		/// </summary>
		public IAppInstall InstallApplication(UnrealAppConfig AppConfig)
		{
			StagedBuild Build = AppConfig.Build as StagedBuild;

			if (Build == null)
			{
				throw new AutomationException("Unsupported build type for PS4!");
			}

			// todo - add sandbox support.
			string SubDir = string.IsNullOrEmpty(AppConfig.Sandbox) ? AppConfig.ProjectName : AppConfig.Sandbox;
			// Mapped destination path
			string DestPath = Path.Combine(this.DataPath, SubDir).ToLower();
			// Root destination path
			string RootDestPath = Path.Combine("/data", SubDir).ToLower().Replace('\\','/');


			PS4AppInstall AppInstall = new PS4AppInstall(AppConfig.Name, this);

			// todo - figure this out..
			/*string PS4Files = Path.Combine(Environment.CurrentDirectory, AppConfig.ProjectName, "Build", "PS4");

			if (Directory.Exists(PS4Files))
			{
				AppInstall.WorkingDir = PS4Files;
			}
			else
			{
				AppInstall.WorkingDir = AppConfig.SourcePath;
			}*/

			AppInstall.WorkingDir = DestPath;

			// e.g O:\KitName\Data\Sandbox\Project\Saved
			AppInstall.ArtifactPath = Path.Combine(DestPath, AppConfig.ProjectName, @"Saved").ToLower();

			// local path refers to the data as seen by the PS4. 
			AppInstall.LocalPath = "/data/" + SubDir.ToLower();
			// don't add the deployed build arg yet, do it on run incase these are later modified...
			AppInstall.CommandArguments = AppConfig.CommandLine;

			Log.Info("Installing {0} to {1}", AppConfig.Name, ToString());
			Log.Verbose("\tCopying {0} to {1}", Build.BuildPath, DestPath);

			// Get a list of build source files
			DirectoryInfo SourceDir = new DirectoryInfo(Build.BuildPath);
			System.IO.FileInfo[] SourceFiles = SourceDir.GetFiles("*", SearchOption.AllDirectories);

			// local app install with additional files, transformed paths from build, development symbols, etc
			// this directory will be mirrored to dev kit in a single operation
			string AppInstallPath;

			// Check that no files in build need to be transformed
			using (var PauseEC = new ScopedSuspendECErrorParsing())
			{
				foreach (FileInfo SourceInfo in SourceFiles)
				{
					string SourceFilePath = SourceInfo.FullName.Replace(SourceDir.FullName, "");

					// remove leading separator
					if (SourceFilePath.First() == Path.DirectorySeparatorChar)
					{
						SourceFilePath = SourceFilePath.Substring(1);
					}

					string Transformed = TransformPathForPS4(SourceFilePath);

					if (Transformed != SourceFilePath)
					{
						Log.Warning("PS4 build file transformed {0} -> {1}", SourceFilePath, Transformed);
					}
				}
			}

			EnableDistributedCopy();

			// fast, mirroring copy, only transfers what has changed
			DistributedCopy(Build.BuildPath, RootDestPath.Replace('\\', '/'), Utils.SystemHelpers.CopyOptions.Mirror);

			// parallel PS4 tests use same app install folder, so lock it as setup is quick
			lock (Globals.MainLock)
			{				
				AppInstallPath = Path.Combine(Globals.TempDir, "PS4AppInstall");

				if (Directory.Exists(AppInstallPath))
				{
					Directory.Delete(AppInstallPath, true);
				}

				Directory.CreateDirectory(AppInstallPath);

				// write a token, used to detect and old gauntlet-installed builds periodically
				string TokenPath = Path.Combine(AppInstallPath, "gauntlet.token");
				File.WriteAllText(TokenPath, "Created by Gauntlet");

				if (LocalDirectoryMappings.Count == 0)
				{
					PopulateDirectoryMappings(AppInstallPath, Path.Combine(AppInstallPath, AppConfig.ProjectName).ToLower());
				}

				if (AppConfig.FilesToCopy != null)
				{
					foreach (UnrealFileToCopy FileToCopy in AppConfig.FilesToCopy)
					{
						string PathToCopyTo = Path.Combine(LocalDirectoryMappings[FileToCopy.TargetBaseDirectory], FileToCopy.TargetRelativeLocation);
						PathToCopyTo = TransformPathForPS4(PathToCopyTo);
						if (File.Exists(FileToCopy.SourceFileLocation))
						{
							FileInfo SrcInfo = new FileInfo(FileToCopy.SourceFileLocation);
							SrcInfo.IsReadOnly = false;
							string DirectoryToCopyTo = Path.GetDirectoryName(PathToCopyTo);
							if (!Directory.Exists(DirectoryToCopyTo))
							{
								Directory.CreateDirectory(DirectoryToCopyTo);
							}
							if (File.Exists(PathToCopyTo))
							{
								FileInfo ExistingFile = new FileInfo(PathToCopyTo);
								ExistingFile.IsReadOnly = false;
							}

							SrcInfo.CopyTo(PathToCopyTo, true);
							Log.Info("Copying {0} to {1}", FileToCopy, DirectoryToCopyTo);
						}
						else
						{
							Log.Warning("File to copy {0} not found", FileToCopy);
						}
					}
				}

				// executables

				// We boot from our filesystem, not the PS4
				if (Path.IsPathRooted(Build.ExecutablePath))
				{
					AppInstall.ExecutablePath = Build.ExecutablePath;
				}
				else
				{
					// TODO - this should be at a higher level....
					string BinaryPath = Path.Combine(Build.BuildPath, Build.ExecutablePath);

					// TODO - need to copy this elf locally to temp?

					// check for a local newer executable
					if (Globals.Params.ParseParam("dev"))
					{
						string LocalBinary = Path.Combine(Environment.CurrentDirectory, Build.ExecutablePath);

						bool LocalFileExists = File.Exists(LocalBinary);
						bool LocalFileNewer = LocalFileExists && File.GetLastWriteTime(LocalBinary) > File.GetLastWriteTime(BinaryPath);

						Log.Verbose("Checking for newer binary at {0}", LocalBinary);
						Log.Verbose("LocalFile exists: {0}. Newer: {1}", LocalFileExists, LocalFileNewer);

						if (LocalFileExists && LocalFileNewer)
						{
							Log.Info("Using local binary {0}", LocalBinary);
							BinaryPath = LocalBinary;

							// need to copy symbols locally too...
							string SymbolsDestPath = Path.Combine(AppInstallPath, "symbols");
							string SymbolSrcPath = Path.Combine(Environment.CurrentDirectory, AppConfig.ProjectName, "Build", "PS4", "Symbols");

							if (Directory.Exists(SymbolSrcPath))
							{
								Log.Verbose("Copying symbols from {0} to {1}", SymbolSrcPath, SymbolsDestPath);

								Utils.SystemHelpers.CopyDirectory(SymbolSrcPath, SymbolsDestPath, Utils.SystemHelpers.CopyOptions.Mirror,TransformPathForPS4);
							}
							else
							{
								Log.Warning("Executable is newer but no symbol files found at {0}!", SymbolSrcPath);
							}							
						}
					}

					AppInstall.ExecutablePath = BinaryPath;
				}

				// sleep a bit before next dcopy, otherwise if nothing changes can cause error as too fast
				Thread.Sleep(2500);

				// Copy the local app installation to device
				DistributedCopy(AppInstallPath, RootDestPath);

			}

			if (File.Exists(AppInstall.ExecutablePath) == false)
			{
				throw new DeviceException("Executable {0} does not exist", AppInstall.ExecutablePath);
			}

			if (Path.GetExtension(AppInstall.ExecutablePath).ToLower().Contains("elf") == false)
			{
				throw new DeviceException("Incorrect executable type - {0}", AppInstall.ExecutablePath);
			}

	
			return AppInstall;
		}

		public IAppInstance Run(IAppInstall App)
		{
			PS4AppInstall PS4App = App as PS4AppInstall;

			if (PS4App == null)
			{
				throw new Exception("AppInstance is of incorrect type!");
			}

			if (File.Exists(PS4App.ExecutablePath) == false)
			{
				throw new DeviceException("Specified path {0} not found!", PS4App.ExecutablePath);
			}

			string DefaultArgs = PS4App.CommandArguments;
			string Sandbox = PS4App.LocalPath.Replace("/data/", "");

			string DeviceRef = string.IsNullOrEmpty(DeviceName) ? "" : string.Format("-target=\"{0}\"",DeviceName);
			string WorkingDirArg = string.IsNullOrEmpty(PS4App.WorkingDir) ? "" : "-workingDirectory=" + PS4App.WorkingDir;
			string TargetArgs = string.Format("-args={1} -deployedbuild={0}", Sandbox, DefaultArgs);

			string CommandLine = string.Format("launch {0} -elf={1} {2} {3}", DeviceRef, PS4App.ExecutablePath, WorkingDirArg, TargetArgs);

			Log.Info("Launching {0} on {1}", App.Name, ToString());
			Log.Verbose("PS4DevkitUtil {0}", CommandLine);

			IProcessResult Result = ExecutePS4DevKitUtilCommand(CommandLine, 0);

			Thread.Sleep(5000);

			// Give PS4DevKitUtil a chance to throw out any errors...
			if (Result.HasExited)
			{
				Log.Warning("PS4DevkitUtil exited early: " + Result.Output);
				throw new DeviceException("Failed to launch on {0}. {1}", StaticDeviceInfo.Name, Result.Output);
			}

			return new PS4AppInstance(PS4App, Result);
		}

		public Dictionary<EIntendedBaseCopyDirectory, string> GetPlatformDirectoryMappings()
		{
			if(LocalDirectoryMappings.Count == 0)
			{
				Log.Warning("Platform directory mappings have not been populated yet! This should be done within TargetDevice.InstallApplication()");
			}
			return LocalDirectoryMappings;
		}
	}
}