// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

namespace MemoryProfiler2
{
	public class FPS4SymbolParser : ISymbolParser
	{
		public static string StaticPlatformName()
		{
			return "PS4";
		}

		public override bool InitializeSymbolService(string ExecutableName, FUIBroker UIBroker)
		{
			// First try and find the self file from the meta-data rather than asking the user up-front
			{
				string ExpectedUtcTimestampStr;
				if (FStreamInfo.GlobalInstance.MetaData.TryGetValue("SelfFile", out SelfPath) && FStreamInfo.GlobalInstance.MetaData.TryGetValue("SelfUtcTimestamp", out ExpectedUtcTimestampStr))
				{
					var ExpectedUtcTimestamp = new DateTime(long.Parse(ExpectedUtcTimestampStr));
					if (File.Exists(SelfPath))
					{
						var ActualUtcTimestamp = File.GetLastWriteTimeUtc(SelfPath);
						if (ExpectedUtcTimestamp != ActualUtcTimestamp)
						{
							// Timestamp doesn't match - ask the user if they still want to use this file
							DialogResult Result = UIBroker.ShowMessageBox(
								"Executable Timestamp Mismatch",
								String.Format("The timestamp of the binary used to generate this profile no longer matches the timestamp of the binary on disk. Do you still want to use '{0}'?", SelfPath),
								MessageBoxButtons.YesNo,
								MessageBoxIcon.Warning
								);

							if (Result == DialogResult.No)
							{
								// Ask the user for a new file
								SelfPath = null;
							}
						}
					}
					else
					{
						// Ask the user for a new file
						SelfPath = null;
					}
				}
			}

			// No self file found from the meta-data - ask the user for one instead
			if (SelfPath == null)
			{
				var OpenSelfFileDialog = new OpenFileDialog();
				OpenSelfFileDialog.Title = "Open the executable file that this profile was generated from";
				OpenSelfFileDialog.Filter = "Signed ELF (*.self)|*.self";
				OpenSelfFileDialog.FileName = String.Format("{0}.self", ExecutableName);
				OpenSelfFileDialog.SupportMultiDottedExtensions = true;

				SelfPath = UIBroker.ShowOpenFileDialog(OpenSelfFileDialog);
			}

			if (SelfPath != null)
			{
				string BaseSDKPath = Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
				string BinToolPath = Path.Combine(BaseSDKPath, "host_tools/bin/orbis-bin.exe");

				if (File.Exists(BinToolPath) && File.Exists(SelfPath))
				{
					// If we were given a valid .self path then we can start orbis-bin now (we don't want the sub-process overhead per-request since that's *really* slow on Windows)
					// We'll use WriteLine to pipe in input, and ReadLine to read out the processed result
					OrbisBinProc = new Process();
					OrbisBinProc.StartInfo = new ProcessStartInfo
					{
						FileName = BinToolPath,
						Arguments = String.Format("-i \"{0}\" -gnu -a2l", SelfPath),
						UseShellExecute = false,
						RedirectStandardInput = true,
						RedirectStandardOutput = true,
						CreateNoWindow = true
					};

					OrbisBinProc.Start();
				}
			}

			return SelfPath != null;
		}

		public override void ShutdownSymbolService()
		{
			SelfPath = null;

			if (OrbisBinProc != null)
			{
				OrbisBinProc.Kill();
			}
		}

		public override bool ResolveAddressToSymboInfo(ESymbolResolutionMode SymbolResolutionMode, ulong Address, out string OutFileName, out string OutFunction, out int OutLineNumber)
		{
			OutFileName = null;
			OutFunction = null;
			OutLineNumber = 0;

			// Only attempt file/line resolution when resolving full symbol information
			// We never need to resolve the symbol name as that happens during runtime
			if (SymbolResolutionMode == ESymbolResolutionMode.Full && OrbisBinProc != null)
			{
				lock (OrbisBinProc)
				{
					// Write the address so a2l will process it
					OrbisBinProc.StandardInput.WriteLine(Address.ToString());

					// a2l produces 2 lines...

					// the first is the symbol name (which we ignore)
					OrbisBinProc.StandardOutput.ReadLine();

					// the second is the file and line (separated by a semicolon)
					string SymbolFileLine = OrbisBinProc.StandardOutput.ReadLine();
					int FileLineSeparator = SymbolFileLine.LastIndexOf(':');
					if (FileLineSeparator != -1)
					{
						OutLineNumber = int.Parse(SymbolFileLine.Substring(FileLineSeparator + 1));
						OutFileName = SymbolFileLine.Substring(0, FileLineSeparator).Replace('/', '\\');
					}
				}
				return true;
			}

			return false;
		}

		private string SelfPath;

		private Process OrbisBinProc;
	}
}
