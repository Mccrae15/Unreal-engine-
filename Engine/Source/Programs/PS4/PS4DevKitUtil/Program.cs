// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Threading;
using System.Diagnostics;
using System.IO;

using ORTMAPILib;

namespace PS4DevKitTool
{
	class FTargetDevice
	{
		public static string GetDeviceDetails(ORTMAPI TM, ITarget Target)
		{
			StringWriter Output = new StringWriter();

			try
			{
				if (!String.IsNullOrEmpty(Target.CachedName))
				{
					Output.Write("Name=\"{0}\" ", Target.CachedName);
				}
				else
				{
					Output.Write("Name=\"{0}\" ", Target.Name);
				}
				Output.Write("Default={0} ", Target.Default);
				if (Target.PowerStatus == ePowerStatus.POWER_STATUS_ON)
				{
					Output.Write("PowerStatus={0} ", Target.PowerStatus);
					Output.Write("ConnectionState={0} ", Target.ConnectionState);

					uint Major = 0, Minor = 0, Build = 0;
					Target.SDKVersion(out Major, out Minor, out Build);

					Output.Write("SDKVersion={0:x}.{1:x3}.{2:x3} ", Major, Minor, Build);
					Output.Write("HostName={0} ", Target.HostName);

					uint Flags = 0;
					sbyte DriveLetter = TM.GetPFSDrive(out Flags);
					switch (DriveLetter)
					{
						case 0:
						case -1:
							Output.Write("MappedDrive= ");
							break;
						default:
							Output.Write("MappedDrive={0} ", (char)DriveLetter);
							break;
					}
				}
			}
			catch
			{ }

			return Output.ToString();
		}
	}

	class FTargetConnection : IDisposable
	{
		private ITarget Target = null;

		public FTargetConnection(ITarget InTarget)
		{
			Target = InTarget;
			Target.RequestConnection();
		}

		public void Dispose()
		{
		}
	}

	class FAttachedProcess : IDisposable
	{
		readonly IProcessInfo ProcessInfo = null;
		readonly bool AlreadyAttached = false;

		public FAttachedProcess(IProcessInfo InProcessInfo)
		{
			ProcessInfo = InProcessInfo;
			try
			{
				ProcessInfo.ProcessInterface.Attach(0);
				AlreadyAttached = false;
			}
			catch (COMException e)
			{
				if (e.ErrorCode == (int)eErrorCode.TMAPI_COMMS_ERR_ALREADY_ATTACHED)
				{
					AlreadyAttached = true;
				}
				else
				{
					throw;
				}
			}
		}

		public void Dispose()
		{
			if (!AlreadyAttached)
			{
				ProcessInfo.ProcessInterface.Detach(0u);
			}
		}
	}

	class FSuspendedProcess : IDisposable
	{
		readonly IProcessInfo ProcessInfo = null;
		readonly bool AlreadySuspended = false;

		public FSuspendedProcess(IProcessInfo InProcessInfo)
		{
			ProcessInfo = InProcessInfo;
			try
			{
				object TempThreads = null;
				IProcessInfo TempProcessInfo;
				object ThreadInfo = null;

				ProcessInfo.ProcessInterface.Suspend(out TempProcessInfo, out ThreadInfo, out TempThreads);
				AlreadySuspended = false;
			}
			catch (COMException e)
			{
				if (e.ErrorCode == (int)eErrorCode.TMAPI_COMMS_ERR_PROCESS_IS_SUSPENDED_ALREADY)
				{
					AlreadySuspended = true;
				}
				else
				{
					throw;
				}
			}

		}

		public void Dispose()
		{
			if (!AlreadySuspended)
			{
				ProcessInfo.ProcessInterface.Resume(null);
			}
		}
	}

	class FAttachedAndSuspendedProcess : IDisposable
	{
		FAttachedProcess AttachedProcess = null;
		FSuspendedProcess SuspendedProcess = null;

		public FAttachedAndSuspendedProcess(IProcessInfo ProcessInfo)
		{
			AttachedProcess = new FAttachedProcess(ProcessInfo);
			SuspendedProcess = new FSuspendedProcess(ProcessInfo);
		}

		public void Dispose()
		{
			if (SuspendedProcess != null)
			{
				SuspendedProcess.Dispose();
			}

			if (AttachedProcess != null)
			{
				AttachedProcess.Dispose();
			}
		}
	}

	// A handler used when launching an executable on the target which monitors for debug events (process exiting etc)
	// and also pipes debug output to the console
	class FLaunchTargetHandler : IEventConsoleOutput, IEventTarget, IEventDebug
	{
		public enum eProcessResult
		{
			Success = 0,
			ErrorGeneric = -100,
			ErrorProcessKilled,
			ErrorShutdown,
			ErrorDisconnected,
			ErrorForceDisconnected,
			ErrorLoadExecFail,
			ErrorLoadExecTimeout,
			
		}
		
        ManualResetEvent ProcessExitEvent = new ManualResetEvent(false);

		readonly ITarget Target = null;

		uint ProcessId = 0;

		public eProcessResult	Result { get; protected set; }

		public string			ResultString { get; protected set; }	

		public int ExitCode { get; protected set; }

		public FLaunchTargetHandler(ITarget InTarget, uint InProcessId)
		{
			Target = InTarget;
			ProcessId = InProcessId;
			Target.AdviseDebugEvents(this);
			Target.AdviseTargetEvents(this);
			Target.AdviseConsoleOutputEvents(this);
			ExitCode = -1;
		}

		public void Unregister()
        {
            try
            {
                Target.UnadviseDebugEvents(this);
            }
            catch
            { }

            try
            {
                Target.UnadviseTargetEvents(this);
            }
            catch
            { }
	
			try
			{
				Target.UnadviseConsoleOutputEvents(this);
			}
			catch
			{ }
		}

		public void WaitForExit()
		{
			while( !ProcessExitEvent.WaitOne(0, false) )
			{
				Thread.CurrentThread.Join(50);
			}
		}

		public void OnProcessExit(IProcessExitEvent pEvent)
		{
			if (ProcessId == pEvent.ProcessId)
			{
				ExitCode = (int)pEvent.ExitCode;
				SetDone(eProcessResult.Success, "Process Exited");
			}
		}

		public void OnProcessKill(IProcessKillEvent pEvent)
		{
			if (ProcessId == pEvent.ProcessId)
			{
                // TODO: Differentiate between the user closing the app,
                // and the app crashing and being killed by the system.
                //SetDone(eProcessResult.ErrorProcessKilled, "Process was Killed");
                SetDone(eProcessResult.Success, "Process was Killed");
                ExitCode = 0;
			}
		}

		public void OnPowerState(IPowerStateEvent pEvent)
		{
			if (pEvent.Operation == ePowerOperation.POWEROP_SHUTDOWN)
			{
				if (pEvent.Progress == ePowerProgress.POWER_OP_STATUS_COMPLETED)
				{
					SetDone(eProcessResult.ErrorShutdown, "Powerdown Occurred");
				}
			}
		}

		public void OnDisconnect(IDisconnectEvent pEvent)
		{
			SetDone(eProcessResult.ErrorDisconnected, "Disconnect Occurred");
		}

		public void OnForceDisconnected(IForceDisconnectedEvent pEvent)
		{
			SetDone(eProcessResult.ErrorForceDisconnected, "Force Disconnect Occurred");
		}	

		public void OnLoadExecFail(ILoadExecFailEvent pEvent)
		{
			if (this.ProcessId == pEvent.Process.Id)
			{
				SetDone(eProcessResult.ErrorLoadExecFail, "LoadExec Fail");
			}
		}

		public void OnLoadExecTimeout(ILoadExecTimeoutEvent pEvent)
		{
			if (this.ProcessId == pEvent.Process.Id)
			{
				SetDone(eProcessResult.ErrorLoadExecTimeout, "LoadExec Timeout");
			}
		}

		protected void SetDone(eProcessResult InResult, string InReason)
		{
			Result = InResult;
			ResultString = InReason;
			ProcessExitEvent.Set();
		}

		public void OnCoredumpCompleted(ICoredumpCompletedEvent pEvent)
		{
			//DidCoreDump = true;
		}

		public void OnCoredumpInProgress(ICoredumpInProgressEvent pEvent)
		{
		}

		public void OnDynamicLibraryLoad(IDynamicLibraryLoadEvent pEvent)
		{ }
		public void OnDynamicLibraryUnload(IDynamicLibraryUnloadEvent pEvent)
		{ }
		public void OnProcessCreate(IProcessCreateEvent pEvent)
		{ }
		public void OnProcessLoading(IProcessLoadingEvent pEvent)
		{ }
		public void OnStopNotification(IStopNotificationEvent pEvent)
		{ }
		public void OnThreadCreate(IThreadCreateEvent pEvent)
		{ }
		public void OnThreadExit(IThreadExitEvent pEvent)
		{ }

		public void OnBusy(IBusyEvent pEvent)
		{ }
		public void OnConnect(IConnectEvent pEvent)
		{ }
		public void OnConnected(IConnectedEvent pEvent)
		{ }
		public void OnExpiryTime(IExpiryTimeEvent pEvent)
		{ }
		public void OnFileServingCaseSensitivityChanged(IFileServingCaseSensitivityChangedEvent pEvent)
		{ }
		public void OnFileServingRootChanged(IFileServingRootChangedEvent pEvent)
		{ }
		public void OnForcedPowerOff(IForcedPowerOffEvent pEvent)
		{ }
		public void OnIdle(IIdleEvent pEvent)
		{ }
		public void OnMultiPhaseProgress(IMultiPhaseProgressEvent pEvent)
		{ }
		public void OnMultiPhaseProgressError(IMultiPhaseProgressErrorEvent pEvent)
		{ }
		public void OnNameUpdate(INameUpdateEvent pEvent)
		{ }
		public void OnProgress(IProgressEvent pEvent)
		{ }
		public void OnSettingsChanged(ISettingsChangedEvent pEvent)
		{ }
		public void OnUpdateError(IUpdateErrorEvent pEvent)
		{ }
		public void OnUpdateProgress(IUpdateProgressEvent pEvent)
		{ }


        public void OnConsoleOutput(IConsoleOutputEvent pEvent)
		{
			if (pEvent.Category == eConsoleOutputCategory.PROCESS_OUTPUT)
			{
				if (pEvent.ProcessId == ProcessId)
				{
					if (pEvent.Port == eConsoleOutputPort.STREAM_STDIO)
					{
						Console.Out.Write(pEvent.Text);
					}
					else if (pEvent.Port == eConsoleOutputPort.STREAM_STDERR)
					{
						Console.Out.Write("STDERR:" + pEvent.Text);
					}
				}
			}
		}

		public void OnBufferReady(IBufferReadyEvent pEvent)
		{
		}
	}

	// A handler for monitoring changes to a target, power state, connection state etc
	class FTargetMonitorHandler : IEventTarget
	{
		ORTMAPI TM = null;
		readonly ITarget Target = null;

		public FTargetMonitorHandler(ORTMAPI InTM, ITarget InTarget)
		{
			TM = InTM;
			Target = InTarget;
			Target.AdviseTargetEvents(this);
		}

		public void Unregister()
		{
			try
			{
				Target.UnadviseTargetEvents(this);
			}
			catch
			{ }
		}

		public void RequestUpdate()
		{
			Console.WriteLine("UpdateTarget=" + Target.HostName + " " + FTargetDevice.GetDeviceDetails(TM, Target));
		}

		public void OnPowerState(IPowerStateEvent pEvent)
		{
			if (pEvent.Progress == ePowerProgress.POWER_OP_STATUS_COMPLETED)
			{
				RequestUpdate();
			}
		}

		public void OnDisconnect(IDisconnectEvent pEvent)
		{
			RequestUpdate();
		}

		public void OnForceDisconnected(IForceDisconnectedEvent pEvent)
		{
			RequestUpdate();
		}

		public void OnBusy(IBusyEvent pEvent)
		{ }
		public void OnConnect(IConnectEvent pEvent)
		{
			RequestUpdate();
		}
		public void OnConnected(IConnectedEvent pEvent)
		{ }
		public void OnExpiryTime(IExpiryTimeEvent pEvent)
		{ }
		public void OnFileServingCaseSensitivityChanged(IFileServingCaseSensitivityChangedEvent pEvent)
		{ }
		public void OnFileServingRootChanged(IFileServingRootChangedEvent pEvent)
		{ }
		public void OnForcedPowerOff(IForcedPowerOffEvent pEvent)
		{ }
		public void OnIdle(IIdleEvent pEvent)
		{ }
		public void OnMultiPhaseProgress(IMultiPhaseProgressEvent pEvent)
		{ }
		public void OnMultiPhaseProgressError(IMultiPhaseProgressErrorEvent pEvent)
		{ }
		public void OnNameUpdate(INameUpdateEvent pEvent)
		{
			RequestUpdate();
		}
		public void OnProgress(IProgressEvent pEvent)
		{ }
		public void OnSettingsChanged(ISettingsChangedEvent pEvent)
		{ }
		public void OnUpdateError(IUpdateErrorEvent pEvent)
		{ }
		public void OnUpdateProgress(IUpdateProgressEvent pEvent)
		{ }
	}

	// A handler to monitors devices being added/removed. Also monitors each discovered device for changes such as power state,
	// connection state, name changes etc
	class FMonitorHandler : IEventServer
	{
		ORTMAPI TM = null;
		Dictionary<String, FTargetMonitorHandler> TargetHandlers = new Dictionary<String, FTargetMonitorHandler>();	

		public FMonitorHandler(ORTMAPI InTM)
		{
			TM = InTM;

			foreach (ITarget Target in TM.Targets)
			{
				try
				{
					Console.WriteLine("TargetAdded=" + Target.HostName + " " + FTargetDevice.GetDeviceDetails(TM, Target));
					TargetHandlers.Add(Target.HostName, new FTargetMonitorHandler(TM, Target));
				}
				catch
				{ }
			}

			try
			{
				if (TM.DefaultTarget != null && TM.DefaultTarget.HostName.Length != 0)
				{
					Console.WriteLine("DefaultTargetChanged=" + TM.DefaultTarget.HostName);
				}
			}
			catch
			{ }

			TM.AdviseServerEvents(this);
		}

		public void Unregister()
		{
			foreach( var Handler in TargetHandlers)
			{
				Handler.Value.Unregister();
			}

			try
			{
				TM.UnadviseServerEvents(this);
			}
			catch
			{ }
		}

		public void OnTargetAdded(ITargetAddedEvent pEvent)
		{

			Array Targets = TM.GetTargetsByHost(pEvent.HostName);
			foreach (ITarget Target in Targets)
			{
				Console.WriteLine("TargetAdded=" + pEvent.HostName + " " + FTargetDevice.GetDeviceDetails(TM, Target));
				TargetHandlers.Add(pEvent.HostName, new FTargetMonitorHandler(TM, Target));
			}
		}

		public void OnTargetDeleted(ITargetDeletedEvent pEvent)
		{
			Console.WriteLine("TargetDeleted=" + pEvent.HostName);

			TargetHandlers.Remove(pEvent.HostName);
		}

		public void OnDefaultTargetChanged(IDefaultTargetChangedEvent pEvent)
		{
			Console.WriteLine("DefaultTargetChanged=" + pEvent.NewHostName);
		}
	}


	class Program
	{
		// List minimal info about available targets to avoid stalls
		static int ListDevices(ORTMAPI TM)
		{
			foreach (ITarget Target in TM.Targets)
			{
				
				string Name = Target.CachedName;
				if (Name.Length == 0)
				{
					Name = Target.HostName;
				}

				Console.WriteLine("Target:");
				Console.WriteLine("Name=\"{0}\"", Name);
				Console.WriteLine("HostName=\"{0}\"", Target.HostName);
				Console.WriteLine("Default={0}", Target.Default);
			}

			return 0;
		}

		// Monitors devices being added/removed along with changes to a specific device
		static int Monitor(ORTMAPI TM, List<string> ArgList)
		{
			FMonitorHandler Handler = new FMonitorHandler(TM);

			int ParentProcessId = Int32.Parse(ArgList[1]);
			string ParentProcessName = ArgList[2];

			for (; ; )
			{
				System.Threading.Thread.Sleep(5000);
				try
				{
					Process ParentProcess = Process.GetProcessById(ParentProcessId);
					if (ParentProcess.ProcessName != ParentProcessName)
					{
						break;
					}
				}
				catch (Exception)
				{
					break;
				}
			}

			Handler.Unregister();

			return 0;
		}

		// Get Target from either its name or its host ip or use default target if name isn't specified
		static ITarget GetTarget(ORTMAPI TM, string TargetString)
		{
			ITarget Target = null;

			if (String.IsNullOrEmpty(TargetString) || TargetString.Equals("default", StringComparison.OrdinalIgnoreCase))
			{
				// Use default
				Target = TM.DefaultTarget;
			}
			else
			{ 
				// Find target
				Array Targets = (Array)TM.GetTargetsByName(TargetString);
				if (Targets.Length == 0)
				{
					Targets = (Array)TM.GetTargetsByHost(TargetString);
				}

				if (Targets.Length != 0)
				{
					Target = (ITarget)Targets.GetValue(0);
				}

				if (Target == null)
				{
					Console.WriteLine("No target " + TargetString);
				}
			}
	
			return Target;
		}

		// Get detailed info about a specific target 
		static int DisplayDeviceDetails(ORTMAPI TM, ITarget Target)
		{
			Console.WriteLine("Name=\"{0}\"", Target.CachedName);
			Console.WriteLine("HostName={0}", Target.HostName);
			Console.WriteLine("Default={0}", Target.Default);

			Console.WriteLine("PowerStatus={0}", Target.PowerStatus);
			Console.WriteLine("ConnectionState={0}", Target.ConnectionState);

			uint Flags = 0;
			sbyte DriveLetter = TM.GetPFSDrive(out Flags);
			switch (DriveLetter)
			{
				case 0:
				case -1:
					Console.WriteLine("MappedDrive=");
					break;
				default:
					Console.WriteLine("MappedDrive={0}", (char)DriveLetter);
					break;
			}

			// This is the only property that requires power to query
			if (Target.PowerStatus == ePowerStatus.POWER_STATUS_ON)
			{
				uint Major = 0, Minor = 0, Build = 0;
				Target.SDKVersion(out Major, out Minor, out Build);

				Console.WriteLine("SDKVersion={0:x}.{1:x3}.{2:x3}", Major, Minor, Build);
			}

			return 0;
		}

		static int DeviceDetails(ORTMAPI TM, List<string> ArgList)
		{
			string TargetString = String.Empty;
			if (ArgList.Count < 2)
			{
				Console.WriteLine("No device specified.  Use default");
			}
			else
			{
				TargetString = ArgList[1];
			}

			// Support both old implicit, old -device and new -target
			TargetString = ParseParamValue(ArgList.ToArray(), "device", TargetString);
			TargetString = ParseParamValue(ArgList.ToArray(), "target", TargetString);

			ITarget Target = GetTarget(TM, TargetString);
			DisplayDeviceDetails(TM, Target);

			return 0;
		}

		static int SetPowerState(ORTMAPI TM, ePowerStatus State, List<string> ArgList)
		{
			if (ArgList.Count < 2)
			{
				Console.WriteLine("No device specified");
				return -1;
			}

			// Support both old implicit, old -device and new -target
			string TargetString = ArgList[1];
			TargetString = ParseParamValue(ArgList.ToArray(), "device", TargetString);
			TargetString = ParseParamValue(ArgList.ToArray(), "target", TargetString);

			ITarget Target = GetTarget(TM, TargetString);

			if (Target.PowerStatus == State)
			{
				Console.WriteLine("Target is already set to {0}", State);
				return 0;
			}

			if (State == ePowerStatus.POWER_STATUS_ON)
			{
				Target.PowerOn();
				Console.Write("{0} power set to {1}", Target.Name, Target.PowerStatus);
			}
			else if (State == ePowerStatus.POWER_STATUS_OFF)
			{
				string Name = Target.Name;
				Target.PowerOff();
				// note - can't access these after turning off!
				Console.Write("{0} power set to {1}", Name, "Off");
			}
			else if (State == ePowerStatus.POWER_STATUS_SUSPEND)
			{
				string Name = Target.Name;
				Target.Suspend();
				Console.Write("{0} power set to {1}", Target.Name, Target.PowerStatus);
			}
			else
			{
				Console.WriteLine("Unsupported power state {0}", State);
				return -1;
			}
	
			return 0;
		}

		static int SetConnectionState(ORTMAPI TM, bool State, List<string> ArgList)
		{
			if (ArgList.Count < 2)
			{
				Console.WriteLine("No device specified");
				return -1;
			}

			// Support both old implicit, old -device and new -target
			string TargetString = ArgList[1];
			TargetString = ParseParamValue(ArgList.ToArray(), "device", TargetString);
			TargetString = ParseParamValue(ArgList.ToArray(), "target", TargetString);

			bool Force = ArgList.Any(s => s.Equals("-force", StringComparison.OrdinalIgnoreCase));

			ITarget Target = GetTarget(TM, TargetString);

			if (Target == null)
			{
				throw new Exception(string.Format("Could not find target {0}", TargetString));
			}
			else
			{
				if (State)
				{
					Target.Connect();
				}
				else
				{
					if (Force)
					{
						Target.ForceDisconnect();
					}
					else
					{
						Target.Disconnect();
					}
				}
			}

			Console.Write("{0} connection set to {1}", Target.Name, Target.ConnectionState);

			return 0;
		}

		// Get detailed info about a specific target 
		static int AddDevice(ORTMAPI TM, List<string> ArgList)
		{ 
			if (ArgList.Count < 2)
			{
				Console.WriteLine("No hostname specified");
			}

			// Support both old implicit, old -device and new -target
			string Hostname = ArgList[1];
			Hostname = ParseParamValue(ArgList.ToArray(), "device", Hostname);
			Hostname = ParseParamValue(ArgList.ToArray(), "target", Hostname);

			ITarget Target = TM.AddTarget(Hostname);

			try
			{
				Console.WriteLine("Added {0} @ {1}", Target.CachedName, Hostname);
				DisplayDeviceDetails(TM, Target);
			}
			catch (Exception Ex)
			{
				throw new Exception(string.Format("Failed to add target {0}. Error: {1}", Hostname, Ex));
			}

			return 0;
		}

		static int RemoveDevice(ORTMAPI TM, List<string> ArgList, bool removeAll)
		{
			if (removeAll)
			{
				// Find target
				Array Targets = (Array)TM.Targets;

				for (int i = 0; i < Targets.Length; i++)
				{
					var Target = (ITarget)Targets.GetValue(i);

					Target.Delete();
				}
			}
			else
			{
				if (ArgList.Count < 2)
				{
					throw new Exception("No hostname specified");
				}

				// Support both old implicit, old -device and new -target
				string Hostname = ArgList[1];
				Hostname = ParseParamValue(ArgList.ToArray(), "device", Hostname);
				Hostname = ParseParamValue(ArgList.ToArray(), "target", Hostname);
				ITarget Target = GetTarget(TM, Hostname);

				if (Target != null)
				{
					try
					{
						Target.Delete();
					}
					catch (Exception Ex)
					{
						throw new Exception(string.Format("Failed to delete target {0}. Error: {1}", Hostname, Ex));
					}
				}
				else
				{
					throw new Exception(string.Format("Could not find target {0}", Hostname));
				}
				
			}

			return 0;

		}

		static int Reboot(ORTMAPI TM, List<string> ArgList)
		{
			if (ArgList.Count < 2)
			{
				Console.WriteLine("No device specified");
				return -1;
			}

			// Support both old implicit, old -device and new -target
			string TargetString = ArgList[1];
			TargetString = ParseParamValue(ArgList.ToArray(), "device", TargetString);
			TargetString = ParseParamValue(ArgList.ToArray(), "target", TargetString);

			ITarget Target = GetTarget(TM, TargetString);
			Target.Reboot();

			return 0;
		}


		// Get running processes and thread info
		static int DeviceSnapshot(ORTMAPI TM, List<string> ArgList)
		{
			string TargetString = null;
			if( ArgList.Count == 2 )
			{
				TargetString = ArgList[1];
			}

			ITarget Target = null;

			try
			{
				Target = GetTarget(TM, TargetString);
			}
			catch (Exception Ex)
			{
				Console.WriteLine("Warning: PS4DevKitUtil " + ArgList.ToString() + "failed");
				Console.WriteLine(Ex.Message);
			}

			if (Target == null)
			{
				throw new ApplicationException("Invalid DevKit: " + TargetString);
			}

			if (Target.PowerStatus == ePowerStatus.POWER_STATUS_ON)
			{
				try
				{
					using (FTargetConnection Connection = new FTargetConnection(Target))
					{
						Array ProcessInfos = (Array)Target.ProcessInfoSnapshot;

						foreach (IProcessInfo ProcessInfo in ProcessInfos)
						{
							Console.WriteLine("ProcessInfo");
							Console.WriteLine("PID=0x{0:x8}", ProcessInfo.Id);
							Console.WriteLine("PPID=0x{0:x8}", ProcessInfo.ParentProcessId);
							Console.WriteLine("Name={0}", ProcessInfo.Name);
							if ((ProcessInfo.Attributes & eProcessAttr.PROCESS_ATTR_SYSTEM) != 0)
							{
								Console.WriteLine("Username=System");
							}
							else
							{
								Console.WriteLine("Username=User");
							}

							// Thread info    
							if ((ProcessInfo.Attributes & eProcessAttr.PROCESS_ATTR_ATTACHED) != 0)
							{
								using (FAttachedAndSuspendedProcess SuspendedProcess = new FAttachedAndSuspendedProcess(ProcessInfo))
								{
									foreach (IThreadInfo ThreadInfo in ProcessInfo.ProcessInterface.ThreadInfoSnapshot)
									{
										Console.WriteLine("ThreadInfo");
										Console.WriteLine("Id={0}", ThreadInfo.Id);
										Console.WriteLine("Name=\"{0}\"", ThreadInfo.Name);
										Console.WriteLine("StackSize={0}", ThreadInfo.StackSize);
										Console.WriteLine("ExitCode={0}", ThreadInfo.ExitCode);
										Console.WriteLine("State={0}", ThreadInfo.State);
										Console.WriteLine("WaitState={0}", ThreadInfo.WaitState);
									}
								}
							}
						}
					}
				}
				catch (COMException e)
				{
					if (TM != null)
					{
						Console.Error.WriteLine("[ERROR]: {0} (0x{1:x}) - Target DevKit:{2}", e.Message, e.ErrorCode, TargetString);
					}
				}
			}

			return 0;
		}

		static string ParseParamValue(object[] ArgList, string Param, string Default = null)
		{
			if (!Param.EndsWith("="))
			{
				Param += "=";
			}

			bool ignoringArgs = false;

			foreach (object Arg in ArgList)
			{
				string ArgStr = Arg.ToString();

				// skip any - or / used to specify args
				if (ArgStr.StartsWith("-") || ArgStr.StartsWith("/"))
				{
					ArgStr = ArgStr.Substring(1);
				}

				if (!ignoringArgs)
				{
					if (ArgStr.StartsWith(Param, StringComparison.InvariantCultureIgnoreCase))
					{
						return ArgStr.Substring(Param.Length);
					}
				}

				// do not parse anything between -args and argsEnd.
				if (String.Equals(ArgStr, "Args", StringComparison.OrdinalIgnoreCase))
				{
					ignoringArgs = true;
				}

				if (String.Equals(ArgStr, "ArgsEnd", StringComparison.OrdinalIgnoreCase))
				{
					ignoringArgs = false;
				}
			}
			return Default;
		}

		static int Postmortem(ORTMAPI TM, string[] args)
		{
			string DumpFile = ParseParamValue(args, "dump");

			if (string.IsNullOrEmpty(DumpFile) == false)
			{
				return PS4DevKitUtil.Postmortem.TryPostmortemOnFile(DumpFile, Console.Out, null) == true ? 0 : -1;
			}

			string TargetString = ParseParamValue(args, "Target");

			if (string.IsNullOrEmpty(TargetString))
			{
				Console.WriteLine("Postmortem command requires either -target=<kit> or -dump=<dumppath>");
				return -1;
			}

			try
			{
				ITarget Target = GetTarget(TM, TargetString);

				Console.WriteLine("Attempting to postmortem most recent crash on {0}", Target.Name);

				return PS4DevKitUtil.Postmortem.TryPostmortemOnDevkit(Console.Out, TM, Target) ? 0 : -1;
			}
			catch (Exception Ex)
			{
				Console.WriteLine("Warning: PS4DevKitUtil " + args.ToString() + "failed");
				Console.WriteLine(Ex.Message);
			}

			return -1;
		}

		// Launch an executable
		static int Launch(ORTMAPI TM, string[] args)
		{
			string TargetString = ParseParamValue(args, "Target");
			ITarget Target = null;

			try
			{
				Target = GetTarget(TM, TargetString);
			}
			catch (Exception Ex)
			{
				Console.WriteLine("Warning: PS4DevKitUtil " + args.ToString() + "failed");
				Console.WriteLine(Ex.Message);
			}

			if (Target == null)
			{
				throw new ApplicationException("Invalid DevKit: " + TargetString);
			}
			
			eDevice Device = eDevice.DEVICE_HOST;
			string DeviceString = ParseParamValue(args, "Device");
			if( DeviceString == "raw")
			{
				Device = eDevice.DEVICE_RAW;
			}

			string WorkingDirectory = ParseParamValue(args, "WorkingDirectory", "");
			string Executable = Path.GetFullPath(ParseParamValue(args, "Elf"));
			string CommandLine = Environment.CommandLine;
			string ExecutableArgs = "";

			if (Executable == null || File.Exists(Executable) == false)
			{
				throw new ApplicationException("Executable invalid or does not exist: " + Executable);
			}

			int ArgsStartIndex = CommandLine.IndexOf("Args=", StringComparison.OrdinalIgnoreCase);
			int ArgsEndIndex = CommandLine.IndexOf("ArgsEnd");

			if (ArgsStartIndex != -1 )
			{
				ArgsStartIndex += 5; // skip args=

				// If ArgsEnd was specified as an end sentinel....
				if (ArgsEndIndex != -1)
				{
					ExecutableArgs = CommandLine.Substring(ArgsStartIndex, ArgsEndIndex - ArgsStartIndex - 1);
				}
				else
				{
					// If not everything after -args= goes to the elf
					ExecutableArgs = CommandLine.Substring(ArgsStartIndex);
				}				
			}

			int ReturnCode = 0;
			
			using (FTargetConnection Connection = new FTargetConnection(Target))
			{
				try
				{
					IProcess Process = Target.LoadProcess(	Device,
															Executable,
															(uint)eLoadOptions.LOAD_OPTIONS_DEFAULT,
															0,	// default stack size
															ExecutableArgs,
															WorkingDirectory );

					FLaunchTargetHandler TargetHandler = new FLaunchTargetHandler(Target, Process.Id);

					// block until the process completes
					TargetHandler.WaitForExit();					

					if (TargetHandler.Result != FLaunchTargetHandler.eProcessResult.Success)
					{
						ReturnCode = (int)TargetHandler.Result;
						Console.WriteLine("Target Process Failed ({0}): {1}.", ReturnCode, TargetHandler.ResultString);
						Console.WriteLine("Waiting 30 secs for devkit to finish writing dump...", ReturnCode, TargetHandler.ResultString);

						Thread.Sleep(30 * 1000);

						Console.WriteLine("Attempting to postmortem most recent crash on {0}", Target.Name);

						if (PS4DevKitUtil.Postmortem.TryPostmortemOnDevkit(Console.Out, TM, Target, 60) == false)
						{
							Console.WriteLine("Could not find callstack via dumpfile");
						}
					}
					else
					{
						ReturnCode = TargetHandler.ExitCode;
					}
				}
				catch (COMException Ex)
				{
					string Desc = TM.GetErrorDescription(Ex.ErrorCode);
					throw new ApplicationException("Launch Failure: " + Desc);
				}
			}

			return ReturnCode;
		}


		static int ShowHelp()
		{
			Dictionary<string, List<string>> Options = new Dictionary<string, List<string>>();
			string[] Functions = new string[] {"list", "detail", "add", "remove", "removeall", "snapshot","launch",
				"connect", "disconnect", "poweron", "poweroff", "reboot","suspend","monitor", "postmortem" };

			Console.WriteLine("\r\nOptions: " + string.Join(", ", Functions));

			foreach (string Func in Functions)
			{
				Options[Func] = new List<string>();
			}	

			Options["list"].Add("Shows all current devices");

			Options["detail"].Add("detail -target<name>: Shows details of specified target");

			Options["add"].Add("add -target=<ip>: Adds the specified target");


			Options["remove"].Add("remove -target=name: Removes the specified target (must be the TM name)");
			Options["removeall"].Add("removeall: clears all devices from Target Manager (useful for build machines)");

			Options["snapshot"].Add("snapshot <name>: Shows running process and thread info");


			Options["launch"].Add("Launches an executable");
			Options["launch"].Add("-target=<name> : Device to use. If empty uses Target Manager default.");
			Options["launch"].Add("-workingdirectory=<path> : Working directory of executable");
			Options["launch"].Add("-elf=<path> : Path to executable");
			Options["launch"].Add("-Args=<path> : Args to executable. Must be last argument, or terminate with ArgsEnd!");

			Options["connect"].Add("connnect -target=<name>: Connects to the specified target");
			Options["disconnect"].Add("disconnect -target=<name> [-force]: Disconnects from the specified target");
			Options["poweron"].Add("poweron -target=<name>: Set power state of the specified target to on");
			Options["poweroff"].Add("poweroff -target=<name>:Set power state of the specified target to off");
			Options["reboot"].Add("reboot -target=<name>: Reboots the specified target");
			Options["suspend"].Add("suspend -target=<name>: Suspends the specified target");
			Options["postmortem"].Add("postmortem -dump=<path>: Dumps callstack info from the specified orbismp file");

			foreach (String Func in Functions)
			{
				Console.WriteLine("\r\n{0}", Func);

				List<String> Args = Options[Func];

				foreach(string Arg in Args)
				{
					Console.WriteLine("\t{0}", Arg);
				}
			}

			return 0;
		}


		static int Main(string[] args)
		{
			ORTMAPI TM = null;
			try
			{
				TM = new ORTMAPI();
				TM.CheckCompatibility((uint)eCompatibleVersion.BuildVersion);
			}
			catch (COMException e)
			{
				Console.WriteLine("Failed to create PS4 ORTMAPI interface.");
				Console.WriteLine(e.ToString());
				return -1;
			}
			
			List<string> ArgList = new List<string>(args);

			int ReturnCode = 0;

			try
			{
				if (ArgList.Count > 0)
				{
					if (String.Compare(ArgList[0], "help", StringComparison.InvariantCultureIgnoreCase) == 0
						|| (String.Compare(ArgList[0], "-help", StringComparison.InvariantCultureIgnoreCase) == 0)
						|| (String.Compare(ArgList[0], "/?", StringComparison.InvariantCultureIgnoreCase) == 0))
					{
						ReturnCode = ShowHelp();
					}
					else if (String.Compare(ArgList[0], "list", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = ListDevices(TM);
					}
					else if (String.Compare(ArgList[0], "detail", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = DeviceDetails(TM, ArgList);
					}
					else if (String.Compare(ArgList[0], "add", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = AddDevice(TM, ArgList);
					}
					else if (String.Compare(ArgList[0], "remove", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = RemoveDevice(TM, ArgList, false);
					}
					else if (String.Compare(ArgList[0], "removeall", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = RemoveDevice(TM, ArgList, true);
					}
					else if (String.Compare(ArgList[0], "snapshot", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = DeviceSnapshot(TM, ArgList);
					}
					else if (String.Compare(ArgList[0], "poweron", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = SetPowerState(TM, ePowerStatus.POWER_STATUS_ON, ArgList);
					}
					else if (String.Compare(ArgList[0], "poweroff", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = SetPowerState(TM, ePowerStatus.POWER_STATUS_OFF, ArgList);
					}
					else if (String.Compare(ArgList[0], "suspend", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = SetPowerState(TM, ePowerStatus.POWER_STATUS_SUSPEND, ArgList);
					}
					else if (String.Compare(ArgList[0], "connect", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = SetConnectionState(TM, true, ArgList);
					}
					else if (String.Compare(ArgList[0], "disconnect", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = SetConnectionState(TM, false, ArgList);
					}
					else if (String.Compare(ArgList[0], "reboot", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = Reboot(TM, ArgList);
					}
					else if (String.Compare(ArgList[0], "launch", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = Launch(TM, args);
					}
					else if (ArgList.Count == 3 && String.Compare(ArgList[0], "monitor", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = Monitor(TM, ArgList);
					}
					else if (String.Compare(ArgList[0], "postmortem", StringComparison.InvariantCultureIgnoreCase) == 0)
					{
						ReturnCode = Postmortem(TM, args);
					}
					else
					{
						throw new ApplicationException("No command specified. Valid commands are list, detail, snapshot, launch, monitor. Use help for help");
					}
				}
				else
				{
					ShowHelp();
					ReturnCode = -1;
				}
			}
			catch (Exception Ex)
			{
				Console.WriteLine("PS4DevKitUtil error: " + Ex.ToString());
				ReturnCode = -1;
			}

			return ReturnCode;
		}
	}
}
