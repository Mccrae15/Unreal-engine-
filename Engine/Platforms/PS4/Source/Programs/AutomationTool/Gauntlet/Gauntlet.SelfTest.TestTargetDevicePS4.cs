// Copyright Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	[TestGroup("Devices")]
	class TestTargetDevicePS4 : TestTargetDevice
	{
		public override void TickTest()
		{
			TargetDevicePS4 Device = new TargetDevicePS4(Globals.Instance.PS4DevkitName);
			
			if (!CheckResult(Device.IsAvailable, "Device is not available"))
			{
				MarkComplete();
				return;
			}

			// tests power on, reboot, and connect. Device should be connected at this point
			CheckEssentialFunctions(Device);

			CheckResult(Directory.Exists(Device.MappedDataPath), "PS4 Datapath {0} does not exist", Device.MappedDataPath);

			// check disconnect works
			CheckResult(Device.Disconnect(), "Disconnect failed");

			CheckResult(Device.IsConnected == false, "Device failed to disconnect");

			CheckResult(Device.PowerOff() && Device.IsOn == false, "Device failed to power off");

			MarkComplete();
		}
	}
}
