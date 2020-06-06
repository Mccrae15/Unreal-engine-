// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Linq;
using System.Xml;
using System.Xml.Linq;
using System.Text;

namespace Gauntlet
{
	/// <summary>
	/// Represents a running instance of a PS4 app. We use PS4DevkitUtil to launch and monitor the app
	/// on the PS4 so this is effectively a local process
	/// </summary>
	class PS4AppInstance : LocalAppProcess
	{
		protected PS4AppInstall Install;

		public PS4AppInstance(PS4AppInstall InInstall, IProcessResult InProcess)
			: base(InProcess, InInstall.CommandArguments)
		{
			Install = InInstall;
		}

		/// <summary>
		/// Path that artifacts from this app instance are written to
		/// </summary>
		public override string ArtifactPath
		{
			get
			{
				return Install.ArtifactPath;
			}
		}

		/// <summary>
		/// Device that this app instance is running on
		/// </summary>
		public override ITargetDevice Device
		{
			get
			{
				return Install.Device;
			}
		}
	}

	/// <summary>
	/// Represents an install of an app on a PS4
	/// </summary>
	class PS4AppInstall : IAppInstall
	{
		/// <summary>
		/// Name that refers to this install
		/// </summary>
		public string Name { get; private set; }

		/// <summary>
		/// Was it installed via deploying?
		/// </summary>
		public bool UseDeploy { get; private set; }

		/// <summary>
		/// PS4 directory name where this app was copied if using deployment. E.g foo is /data/foo)
		/// </summary>
		public string DeploymentDirectory;

		/// <summary>
		/// Path to the executable that should be launched
		/// </summary>
		public string ExecutablePath;

		/// <summary>
		/// Path that should be used as a working dir for the PS4. Even when deployed we set this to the build
		/// folder because prx files can't be loaded from local paths.
		/// </summary>
		public string WorkingDir;

		/// <summary>
		/// Arguments to pass to the process
		/// </summary>
		public string CommandArguments;

		/// <summary>
		/// Path that artifacts will be saved to
		/// </summary>
		public string ArtifactPath;

		/// <summary>
		/// Device we were installed on
		/// </summary>
		public ITargetDevice Device { get; private set; }	

		/// <summary>
		/// Options to use when running this app
		/// </summary>
		public CommandUtils.ERunOptions RunOptions { get; set; }

		/// <summary>
		/// Constructor for the install. Most things should be passed in
		/// </summary>
		/// <param name="InName"></param>
		/// <param name="InDevice"></param>
		/// <param name="InUseDeploy"></param>
		public PS4AppInstall(string InName, TargetDevicePS4 InDevice, bool InUseDeploy)
		{
			Name = InName;
			Device = InDevice;
			UseDeploy = InUseDeploy;
			this.RunOptions = CommandUtils.ERunOptions.NoWaitForExit;
		}

		/// <summary>
		/// Runs this instance
		/// </summary>
		/// <returns></returns>
		public IAppInstance Run()
		{
			return Device.Run(this);
		}
	}

	/// <summary>
	/// Represents a factory class capable of creating PS4 devices
	/// </summary>
	public class PS4DeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform? Platform)
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
		/// <summary>
		/// Info that represents the current state of the device
		/// </summary>
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
						
			/// <summary>
			/// Power state
			/// </summary>
			public ePowerStatus		PowerStatus;

			/// <summary>
			/// Connection state (CONNECTED = us)
			/// </summary>
			public eConnectionState	ConnectionState;

			/// <summary>
			/// Are we connected?
			/// </summary>
			public bool Connected;

			public StateInfo()
			{
				ConnectionState = eConnectionState.CONNECTION_UNAVAILABLE;
				PowerStatus = ePowerStatus.POWER_STATUS_UNKNOWN;
			}
		}

		/// <summary>
		/// Name passed in as a creation reference.
		/// </summary>
		protected string ReferenceName;

		/// <summary>
		/// Cached device info. We update this periodically via QueryInfo
		/// </summary>
		protected PS4DeviceInfo StaticDeviceInfo;

		/// <summary>
		/// Cached state info. We update this periodically via QueryInfo 
		/// </summary>
		protected StateInfo	CachedStateInfo;

		/// <summary>
		/// For parallel tests, builds need to be staged separately
		/// </summary>
		private static int ParallelStagingIdx = 0;

		/// <summary>
		/// Our mappings of Intended directories to where they actually represent on this platform.
		/// </summary>
		protected Dictionary<EIntendedBaseCopyDirectory, string> LocalDirectoryMappings { get; set; }

		/// <summary>
		/// Name accessor required by ITargetDevice
		/// </summary>
		public string Name
		{
			get
			{
				// should always be valid, and either the refname till retrieved or actual name
				return StaticDeviceInfo.Name;
			}
		}

		/// <summary>
		/// Device name required by ITargetDevice
		/// </summary>
		public string DeviceName { get { return StaticDeviceInfo.Name; } }

		/// <summary>
		/// We are and always will be a PS4!
		/// </summary>
		public UnrealTargetPlatform? Platform { get { return UnrealTargetPlatform.PS4; } }

		// Returns the current state info, refreshing it if it's older than 60s
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

		/// <summary>
		/// Return true if the device is valid, e.g. we could try connecting
		/// </summary>
		public bool IsValid
		{
			get
			{
				return CurrentStateInfo.ConnectionState != StateInfo.eConnectionState.CONNECTION_UNAVAILABLE;
			}
		}

		/// <summary>
		/// Returns true if the device is available. E.g. a connection attempt should succeed (but is not guaranteed)
		/// </summary>
		public bool IsAvailable
		{
			get
			{
				// mark as available if we're already connected!
				return IsConnected || CurrentStateInfo.ConnectionState == StateInfo.eConnectionState.CONNECTION_AVAILABLE;
			}
		}

		/// <summary>
		/// Returns true if we are connected to the device
		/// </summary>
		public bool IsConnected
		{
			get
			{
				return CurrentStateInfo.ConnectionState == StateInfo.eConnectionState.CONNECTION_CONNECTED;
			}
		}


		/// <summary>
		/// Is the device on...
		/// </summary>
		public bool IsOn { get { return CurrentStateInfo.PowerStatus == StateInfo.ePowerStatus.POWER_STATUS_ON; } }


		/// <summary>
		/// Returns the mapped path to this PS4s data folder
		/// </summary>
		public string MappedDataPath
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

	
		/// <summary>
		/// Time we last queried info
		/// </summary>
		protected DateTime LastInfoTime = DateTime.MinValue;

		/// <summary>
		/// Should the device be removed from PS4 neighborhood on destruction? (Yes if it wasn't
		/// there in the first place)
		/// </summary>
		protected bool RemoveOnDestruction;

		/// <summary>
		/// Options used when running on this device
		/// </summary>
		public CommandUtils.ERunOptions RunOptions { get; set; }

		/// <summary>
		/// Constructor that takes a reference name. The name will be used to either find or add the
		/// device to PS4N
		/// </summary>
		/// <param name="InReferenceName"></param>
		public TargetDevicePS4(string InReferenceName)
		{
			ReferenceName = InReferenceName;
			RunOptions = CommandUtils.ERunOptions.NoWaitForExit | CommandUtils.ERunOptions.NoLoggingOfRunCommand;

			StaticDeviceInfo = new PS4DeviceInfo(ReferenceName);
			CachedStateInfo = new StateInfo();

			SetUpDirectoryMappings();

			PS4DeviceInfo[] DefaultDevices = PS4Utils.GetAvailableDevices();

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

		/// <summary>
		/// Returns available devices
		/// </summary>
		/// <returns></returns>
		static public ITargetDevice[] GetDefaultDevices()
		{
			var DeviceInfo = PS4Utils.GetAvailableDevices();

			List<ITargetDevice> Devices = new List<ITargetDevice>();

			foreach (var Info in DeviceInfo)
			{
				TargetDevicePS4 PS4Device = new TargetDevicePS4(Info.Hostname);

				Devices.Add(PS4Device);
			}

			return Devices.ToArray();
		}

		bool ThrowIfRecursive = false;

		/// <summary>
		/// Query the devkits info
		/// </summary>
		/// <param name="LogOutput"></param>
		/// <returns></returns>
		bool QueryInfo(bool LogOutput=false)
		{
			if (ThrowIfRecursive)
			{
				throw new DeviceException("QueryInfo went recursive!");
			}

			ThrowIfRecursive = true;

			PS4DeviceInfo NewDeviceInfo = new PS4DeviceInfo(ReferenceName);
			StateInfo NewStateInfo = new StateInfo();
					
			try
			{
				IProcessResult QueryProcess = PS4Utils.ExecutePS4DevKitUtilCommand(string.Format("detail -target=\"{0}\"", ReferenceName));
				
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

		/// <summary>
		/// Remove the device from the system
		/// </summary>
		void RemoveDevice()
		{
			// use t
			PS4Utils.ExecutePS4DevKitUtilCommand(string.Format("remove -target=\"{0}\"", this.Name));
		}

		/// <summary>
		/// Performs device setup, in this case adding it to PS4N so it's usable
		/// </summary>
		void SetupDevice()
		{
			IProcessResult QueryProcess;

			bool DeviceIsValid = false;

			if (ReferenceName.Contains("."))
			{
				// passed by IP, add this and resolve it.
				Log.Verbose("Attempting to add device {0} to PS4 Neighborhood", ReferenceName);

				QueryProcess = PS4Utils.ExecutePS4DevKitUtilCommand(string.Format("add -target=\"{0}\"", ReferenceName));

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

			// Configure kit memory mode, any problems will throw in ready check so another device can be selected
			ConfigureMemoryMode();

		}

		/// <summary>
		/// Powers on the PS4. Will wait for the command to complete
		/// </summary>
		/// <returns></returns>
		public bool PowerOn()
		{
			IProcessResult Result = PS4Utils.ExecutePS4DevKitUtilCommand(string.Format("poweron -target=\"{0}\"", StaticDeviceInfo.Name), 120);
			CachedStateInfo = null;
			return Result.ProcessObject.ExitCode == 0;
		}

		/// <summary>
		/// Powers off the PS4, returns immediately.
		/// </summary>
		/// <returns></returns>
		public bool PowerOff()
		{
			IProcessResult Result = PS4Utils.ExecutePS4DevKitUtilCommand(string.Format("poweroff -target=\"{0}\"", StaticDeviceInfo.Name));
			if (CachedStateInfo != null)
			{
				CachedStateInfo.PowerStatus = StateInfo.ePowerStatus.POWER_STATUS_OFF;
			}
			return Result.ProcessObject.ExitCode == 0;
		}

		/// <summary>
		/// Reboots the PS4, waits for the operation to complete
		/// </summary>
		/// <returns></returns>
		public bool Reboot()
		{
			IProcessResult Result = PS4Utils.ExecutePS4DevKitUtilCommand(string.Format("reboot -target=\"{0}\"", StaticDeviceInfo.Name), 120);
			CachedStateInfo = null;
			return Result.ProcessObject.ExitCode == 0;
		}

		/// <summary>
		/// Connects to the PS4
		/// </summary>
		/// <returns></returns>
		public bool Connect()
		{
			IProcessResult Result = PS4Utils.ExecutePS4DevKitUtilCommand(string.Format("connect -target=\"{0}\"", StaticDeviceInfo.Name));
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

		/// <summary>
		/// Disconnects from the PS4, returns immediately
		/// </summary>
		/// <returns></returns>
		public bool Disconnect()
		{
			IProcessResult Result = PS4Utils.ExecutePS4DevKitUtilCommand(string.Format("disconnect -target=\"{0}\"", StaticDeviceInfo.Name));
			CachedStateInfo = null;
			return Result.ProcessObject.ExitCode == 0;
		}

		/// <summary>
		/// Disconnect all applications from the devit
		/// </summary>
		public bool ForceDisconnect()
		{
			IProcessResult Result = PS4Utils.ExecuteOrbisCtrlCommand(string.Format("force-disconnect \"{0}\"", StaticDeviceInfo.Name));
			CachedStateInfo = null;
			return Result.ProcessObject.ExitCode == 0;
		}

		/// <summary>
		/// Sets a PS4 setting 
		/// </summary>
		/// <param name="SettingName"></param>
		/// <param name="NewValue"></param>
		/// <returns></returns>
		public bool SetSetting(string SettingName, string NewValue)
		{
			IProcessResult Result = PS4Utils.ExecutePS4DevKitUtilCommand(string.Format("setsetting -target=\"{0}\" -setting=\"{1}\" -value=\"{2}\"", StaticDeviceInfo.Name, SettingName, NewValue));
			CachedStateInfo = null;
			return Result.ProcessObject.ExitCode == 0;
		}

		/// <summary>
		/// Get a setting value from PS4 kit 
		/// </summary>
		/// <param name="Key">hex key of the setting to get, ex. 0x141E6003 </param>
		/// <param name="Document">Document settings loaded from XML</param>
		/// <param name="Setting">XElement of specified setting</param>
		/// <param name="Size">size of the setting in bytes</param>
		/// <param name="Value">string value of setting</param>
		public bool GetSetting(string Key, out XElement Document, out XElement Setting, out int Size, out string Value)
		{
			Size = 0;
			Document = null;
			Setting = null;
			Value = string.Empty;

			// Export settings to date stamped file (parallel support)

			// The temp directory may not exist yet, so create it
			if (!Directory.Exists(Globals.TempDir))
			{
				try
				{
					CommandUtils.CreateDirectory(Globals.TempDir);
				}
				catch (Exception Ex)
				{
					Log.Warning("Unable to create folder {0} : {1}", Globals.TempDir, Ex.Message);
					return false;
				}
			}

			string OutputPath = Path.Combine(Globals.TempDir, string.Format("PS4_Settings_Export_{0}.xml", string.Format("{0:yyyy-MM-dd_hh-mm-ss-fff}", DateTime.UtcNow)));
			IProcessResult Result = PS4Utils.ExecuteOrbisCtrlCommand(String.Format("settings-export \"{0}\" \"{1}\"", OutputPath, Name), WarnOnTimeout: false);
			if (Result.ExitCode != 0)
			{
				Log.Warning("Failed to export PS4 settings");
				return false;
			}

			try
			{
				using (XmlReader Reader = XmlReader.Create(OutputPath, new XmlReaderSettings()))
				{
					Document = XElement.Load(Reader);
					Setting = Document.Elements("setting").FirstOrDefault(E => E.Attribute("key").Value == Key);
					if (Setting == null)
					{
						return false;
					}

					Size = int.Parse(Setting.Attribute("size").Value);
					Value = Setting.Attribute("value").Value;
				}

				//CommandUtils.DeleteFile_NoExceptions(OutputPath);

				return true;
			}
			catch (Exception Error)
			{
				Log.Warning("Failed to parse exported PS4 settings, {0}", Error);
				return false;
			}
		}

		public override string ToString()
		{
			return StaticDeviceInfo.Name;
		}
		void SetUpDirectoryMappings()
		{
			LocalDirectoryMappings = new Dictionary<EIntendedBaseCopyDirectory, string>();
		}
		public void PopulateDirectoryMappings(string ProjectDir)
		{
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Build, Path.Combine(ProjectDir, "build"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Binaries, Path.Combine(ProjectDir, "binaries"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Config, Path.Combine(ProjectDir, "saved", "config"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Content, Path.Combine(ProjectDir, "content"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Demos, Path.Combine(ProjectDir, "saved", "demos"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Profiling, Path.Combine(ProjectDir, "saved", "profiling"));
			LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Saved, Path.Combine(ProjectDir, "saved"));
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

			Log.Info("Installing {0} to {1}", AppConfig.Name, ToString());

			string MappedPS4Path = null;
			string SandboxName = null;
			string PS4LocalPath = null;

			// todo, move this elsewhere?
			bool UseDeploy = Globals.Params.ParseParam("deploy");			

			PS4AppInstall AppInstall = new PS4AppInstall(AppConfig.Name, this, UseDeploy);			

			bool UseLocalExecutable = false;

			// path to build we'll install or run from, may be adjusted
			string BuildPath = Build.BuildPath;

			// default path to exe we'll launch, may be adjusted
			string ExecutablePathToLaunch = Path.Combine(BuildPath, Build.ExecutablePath);

			// check for a local newer executable - TODO - this should be at a higher level....
			if (Globals.Params.ParseParam("dev"))
			{
				string LocalBinary = Path.Combine(Environment.CurrentDirectory, Build.ExecutablePath);

				bool LocalFileExists = File.Exists(LocalBinary);
				bool LocalFileNewer = LocalFileExists && File.GetLastWriteTime(LocalBinary) > File.GetLastWriteTime(ExecutablePathToLaunch);

				Log.Verbose("Checking for newer binary at {0}", LocalBinary);
				Log.Verbose("LocalFile exists: {0}. Newer: {1}", LocalFileExists, LocalFileNewer);

				if (LocalFileExists && LocalFileNewer)
				{
					Log.Info("Using local binary {0}", LocalBinary);
					ExecutablePathToLaunch = LocalBinary;
					UseLocalExecutable = true;
				}
			}

			// Do we need to stage things locally? Either to deploy, or to run from
			bool NeedLocalStage = (AppConfig.FilesToCopy.Count > 0) || !UseDeploy;

			string LocalStagingPath = null;

			// if we're deploying and have local files we need to stage them, similarly if 
			// we're not going to deploy we need somewhere to copy the build to so we can
			// add the local files
			if (NeedLocalStage)
			{
				lock (Globals.MainLock)
				{
					ParallelStagingIdx++;
				}

				LocalStagingPath = Path.Combine(Globals.TempDir, string.Format("ps4_{0}_staged", ParallelStagingIdx));

				if (UseDeploy && Directory.Exists(LocalStagingPath))
				{
					// we need a place for local files but will copy them to the kit,
					// so make sure this directory is empty
					Directory.Delete(LocalStagingPath, true);
				}

				if (!Directory.Exists(LocalStagingPath))
				{
					Directory.CreateDirectory(LocalStagingPath);
				}

				Utils.SystemHelpers.MarkDirectoryForCleanup(LocalStagingPath);
			}

			// if we don't deploy and need local files then we need a copy of the build to modify and we'll
			// use that copy of the executable
			if (!UseDeploy)
			{
				// copy the build to our temp path, that's now our build path
				Log.Info("Mirroring build to {0}", LocalStagingPath);

				Utils.SystemHelpers.CopyDirectory(BuildPath, LocalStagingPath, Utils.SystemHelpers.CopyOptions.Mirror, PS4Utils.TransformPathForPS4);

				// our build path is now local
				BuildPath = LocalStagingPath;

				if (!UseLocalExecutable)
				{
					ExecutablePathToLaunch = Path.Combine(BuildPath, Build.ExecutablePath);
				}
			}

			PopulateDirectoryMappings(Path.Combine(LocalStagingPath, AppConfig.ProjectName).ToLower());

			// now copy in any files from appconfig to the local staging directory
			if (AppConfig.FilesToCopy.Count > 0)
			{
				// copy the build to our temp path
				Log.Info("Copying AppConfig files to {0}", LocalStagingPath);

				foreach (UnrealFileToCopy FileToCopy in AppConfig.FilesToCopy)
				{
					if (File.Exists(FileToCopy.SourceFileLocation))
					{
						string PathToCopyTo = Path.Combine(LocalDirectoryMappings[FileToCopy.TargetBaseDirectory], FileToCopy.TargetRelativeLocation);

						PathToCopyTo = PS4Utils.TransformPathForPS4(PathToCopyTo);

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

						File.Copy(FileToCopy.SourceFileLocation, PathToCopyTo, true);

						Log.Verbose("Copying {0} to {1}", FileToCopy, DirectoryToCopyTo);

						// clear any read-only flags
						FileInfo DestInfo = new FileInfo(PathToCopyTo);
						DestInfo.IsReadOnly = false;
					}
					else
					{
						Log.Warning("File to copy {0} not found", FileToCopy);
					}
				}
			}

			// copy in symbols if necessary
			if (UseLocalExecutable)
			{
				// need to copy symbols locally too...
				string SymbolsDestPath = Path.Combine(LocalStagingPath, "symbols");
				string SymbolSrcPath = Path.Combine(Environment.CurrentDirectory, AppConfig.ProjectName, "Build", "PS4", "Symbols");

				if (Directory.Exists(SymbolSrcPath))
				{
					Log.Verbose("Copying symbols from {0} to {1}", SymbolSrcPath, SymbolsDestPath);

					Utils.SystemHelpers.CopyDirectory(SymbolSrcPath, SymbolsDestPath, Utils.SystemHelpers.CopyOptions.Mirror, PS4Utils.TransformPathForPS4);
				}
				else
				{
					Log.Warning("Executable is newer but no symbol files found at {0}!", SymbolSrcPath);
				}
			}

			// We now have either -
			// a) a build source we can just run against
			// b) a local copy of the source with additional files & location we can run against
			// c) a build source we can deploy, and a local copy of files & symbols we can copy on top of

			if (UseDeploy)
			{
				// todo - add sandbox support.
				SandboxName = string.IsNullOrEmpty(AppConfig.Sandbox) ? AppConfig.ProjectName : AppConfig.Sandbox;
				SandboxName = SandboxName.ToLower();
				MappedPS4Path = Path.Combine(this.MappedDataPath, SandboxName).ToLower();
				// the reason we do this is that PS4 can't load prx files from its data path, so to avoid having to copy
				// them locally we set our mapped devkit path as app0. Todo - should we just copy them locally?
				AppInstall.WorkingDir = MappedPS4Path;
			}
			else
			{
				SandboxName = AppConfig.ProjectName.ToLower();
				AppInstall.WorkingDir = BuildPath;
			}

			MappedPS4Path = Path.Combine(this.MappedDataPath, SandboxName).ToLower();
			PS4LocalPath = Path.Combine("/data", SandboxName).Replace('\\', '/');

			// e.g O:\KitName\Data\Sandbox\Project\Saved
			AppInstall.ArtifactPath = Path.Combine(MappedPS4Path, AppConfig.ProjectName, @"Saved").ToLower();

			// remove any existing saved directory
			if (Directory.Exists(AppInstall.ArtifactPath))
			{
				Directory.Delete(AppInstall.ArtifactPath, true);
			}

			// local path refers to the data as seen by the PS4. 
			AppInstall.DeploymentDirectory = UseDeploy ? SandboxName : null;
			// don't add the deployed build arg yet, do it on run incase these are later modified...
			AppInstall.CommandArguments = AppConfig.CommandLine;


			// For deploy copy the files to the kit
			if (UseDeploy)
			{
				// to deploy, start by copying the build over
				Log.Verbose("\tCopying {0} to {1}", Build.BuildPath, MappedPS4Path);

				// Get a list of build source files
				DirectoryInfo SourceDir = new DirectoryInfo(Build.BuildPath);
				System.IO.FileInfo[] SourceFiles = SourceDir.GetFiles("*", SearchOption.AllDirectories);

				// local app install with additional files, transformed paths from build, development symbols, etc
				// this directory will be mirrored to dev kit in a single operation
				//string AppInstallPath;

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

						string Transformed = PS4Utils.TransformPathForPS4(SourceFilePath);

						if (Transformed != SourceFilePath)
						{
							Log.Warning("PS4 build file transformed {0} -> {1}", SourceFilePath, Transformed);
						}
					}
				}

				PS4Utils.EnableDistributedCopy();

				// fast, mirroring copy, only transfers what has changed
				PS4Utils.DistributedCopy(StaticDeviceInfo.Hostname, Build.BuildPath, PS4LocalPath.Replace('\\', '/'), Utils.SystemHelpers.CopyOptions.Mirror);

				// now copy the staged files
				// sleep a bit before next dcopy, otherwise if nothing changes can cause error as too fast
				Thread.Sleep(2500);

				// Copy the local app installation to device if necessary
				if (string.IsNullOrEmpty(LocalStagingPath) == false)
				{
					PS4Utils.DistributedCopy(StaticDeviceInfo.Hostname, LocalStagingPath, PS4LocalPath);
				}

				// write a token, used to detect and old gauntlet-installed builds periodically
				string TokenPath = Path.Combine(MappedPS4Path, "gauntlet.token");
				File.WriteAllText(TokenPath, "Created by Gauntlet");
			}

			AppInstall.ExecutablePath = ExecutablePathToLaunch;

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

		/// <summary>
		/// Runs the previously installed app
		/// </summary>
		/// <param name="App"></param>
		/// <returns></returns>
		public IAppInstance Run(IAppInstall App)
		{
			PS4AppInstall PS4App = App as PS4AppInstall;

			if (PS4App == null)
			{
				throw new Exception("AppInstall is of incorrect type!");
			}

			if (File.Exists(PS4App.ExecutablePath) == false)
			{
				throw new DeviceException("Specified path {0} not found!", PS4App.ExecutablePath);
			}

			if (PS4App.Device != this)
			{
				throw new Exception("AppInstall was created on a different device!");
			}

			if (File.Exists(PS4App.ExecutablePath) == false)
			{
				throw new DeviceException("Specified path {0} not found!", PS4App.ExecutablePath);
			}

			string DefaultArgs = PS4App.CommandArguments;

			// todo - support sandboxes for non-deployed builds
			string SandboxArg = PS4App.UseDeploy ? string.Format("-deployedbuild=\"{0}\"", PS4App.DeploymentDirectory) : "";

			string DeviceRef = string.IsNullOrEmpty(DeviceName) ? "" : string.Format("-target=\"{0}\"",DeviceName);
			string WorkingDirArg = string.IsNullOrEmpty(PS4App.WorkingDir) ? "" : String.Format("-workingDirectory=\"{0}\"", PS4App.WorkingDir);

			string TargetArgs = string.Format("-args={0} {1}", DefaultArgs, SandboxArg);

			string CommandLine = string.Format("launch {0} -elf=\"{1}\" {2} {3}", DeviceRef, PS4App.ExecutablePath, WorkingDirArg, TargetArgs);

			Log.Info("Launching {0} on {1}", App.Name, ToString());
			Log.Verbose("PS4DevkitUtil {0}", CommandLine);

			IProcessResult Result = PS4Utils.ExecutePS4DevKitUtilCommand(CommandLine, 0);

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

		/// <summary>
		/// Take a screenshot of kits screen, works in shipping builds and also when device isn't running game application
		/// </summary>
		public bool TakeScreenshot(string OutputPath)
		{
			try
			{
				IProcessResult Result = PS4Utils.ExecuteOrbisCtrlCommand(String.Format("screenshot auto \"{0}\" \"{1}\"", OutputPath, StaticDeviceInfo.Name), 30, false);
				return Result.ExitCode == 0 && File.Exists(OutputPath);
			}
			catch
			{
				return false;
			}
		}

		private void ConfigureMemoryMode()
		{			
			if (Globals.Params.ParseParam("largememory"))
			{
				int Size;
				string Value;
				XElement Document;
				XElement Setting;
				const string LargeMemoryKey = "0x78020300";

				// First see if we're already in large memory mode
				if (!GetSetting(LargeMemoryKey, out Document, out Setting, out Size, out Value) || Size != 32)
				{
					throw new DeviceException("Device {0} requires large memory and unable to get device settings to verify", ReferenceName);
				}

				if (Value.Substring(5, 1) != "1")
				{
					// Not in large memory, so adjust value and set
					StringBuilder Builder = new StringBuilder(Value);
					Builder[5] = '1';
					Setting.Attribute("value").Value = Builder.ToString();

					// Prune down to only the large memory setting
					Document.Elements("setting").Where(E => E != Setting).Remove();

					string ImportPath = Path.Combine(Globals.TempDir, string.Format("PS4_Settings_Import_{0}.xml", string.Format("{0:yyyy-MM-dd_hh-mm-ss-fff}", DateTime.UtcNow)));
					Document.Save(ImportPath);
					
					IProcessResult Result = PS4Utils.ExecuteOrbisCtrlCommand(String.Format("settings-import \"{0}\" \"{1}\"", ImportPath, Name), WarnOnTimeout:false);
					if (Result.ExitCode != 0)
					{
						throw new DeviceException("Device {0} requires large memory and unable to import large memory settings", ReferenceName);
					}

					CommandUtils.DeleteFile_NoExceptions(ImportPath);

					Log.Info("Rebooting PS4 {0} to enable large memory mode", ReferenceName);
					Reboot();
				}
			}
		}
	}
}