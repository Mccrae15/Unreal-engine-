// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
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
	/// <summary>
	/// Validates that we can install and run various build configurations on PS4
	/// </summary>
	[TestGroup("Unreal", 7)]
	class TestUnrealInstallThenRunPS4 : TestUnrealInstallAndRunBase
	{
		public override void TickTest()
		{
			// create a new build
			UnrealBuildSource Build = new UnrealBuildSource(this.ProjectName, this.ProjectFile, this.UnrealPath, this.UsesSharedBuildType, this.BuildPath);

			// check it's valid
			if (!CheckResult(Build.BuildCount > 0, "staged build was invalid"))
			{
				MarkComplete();
				return;
			}

			// Create devices to run the client and server
			ITargetDevice ClientDevice = new TargetDevicePS4(this.DevkitName);

			// PS4 only supports clients. Test all three configs
			TestInstallThenRun(Build, UnrealTargetRole.Client, ClientDevice, UnrealTargetConfiguration.Development);
			TestInstallThenRun(Build, UnrealTargetRole.Client, ClientDevice, UnrealTargetConfiguration.Test);
			TestInstallThenRun(Build, UnrealTargetRole.Client, ClientDevice, UnrealTargetConfiguration.Shipping);

			MarkComplete();
		}
	}
}
