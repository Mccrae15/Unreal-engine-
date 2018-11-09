// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using orbiscorefileapi;
using System.IO;

namespace PS4DevKitUtil
{
	/// <summary>
	/// Wrapper around CoreFileAPI to pull callstack info from a dump. Borrows liberally from the
	/// orbis-coreview sample
	/// 
	/// Usage: call TryToPrintCallstackFromDumpFile with a TM and Target object. Kit needs to be set to store dumps
	/// locally.
	/// </summary>
	/// 

	class Utility
	{
		public static IThread GetThreadByID(IThreadCollection items, ulong id)
		{
			foreach (IThread item in items)
			{
				if (item.ID == id)
					return item;
			}
			return null;
		}

		public static int FindFirstFrameWithDebugInfo(IThread thread)
		{
			var frames = thread.Frames;

			foreach (IStackFrame item in frames)
			{
				if (item.HasDebugInfo)
					return item.Index;
			}
			return 0;
		}
	}

	class PrettyPrinter
	{
		public static string ZeroPaddedHex(ulong n)
		{
			return "0x" + n.ToString("X16");
		}
		public static string ZeroPaddedHex(uint n)
		{
			return "0x" + n.ToString("X8");
		}
		public static string Hex(ulong n)
		{
			return "0x" + n.ToString("X");
		}
		public static void Indent(TextWriter to, int v)
		{
			to.Write(new string(' ', v * 2));
		}
	}

	class Strings
	{
		public static string FullDump = "FullDump";
		public static string MiniDump = "MiniDump";
		public static string UserData = "UserData";
		public static string NoUserData = "NoUserData";
		public static string UserFile = "UserFile";
		public static string NoUserFile = "NoUserFile";
		public static string MissingManifestFile = "MissingManifestFile";
		public static string MissingMemoryFiles = "MissingMemoryFiles";
	}

	class StringTable
	{
		public StringTable(int columns)
		{
			m_columns = columns;
			m_rows = new List<List<string>>();
		}

		public List<string> AddRow()
		{
			var row = new List<string>(m_columns);

			for (uint i = 0; i != m_columns; ++i)
				row.Add("");

			m_rows.Add(row);
			return row;
		}


		public void PrintTo(TextWriter to)
		{
			var columnWidths = new List<int>();
			for (int i = 0; i != m_columns; ++i)
				columnWidths.Add(0);

			foreach (var row in m_rows)
			{
				for (int i = 0; i != m_columns; ++i)
					columnWidths[i] = Math.Max(columnWidths[i], row[i].Length + 1);
			}

			var ret = new StringBuilder();
			foreach (var row in m_rows)
			{
				for (int i = 0; i != m_columns; ++i)
				{
					if ((i + 1) == m_columns)
					{
						to.Write(row[i]);
					}
					else
					{
						string format = "{0,-" + columnWidths[i].ToString() + "}";
						to.Write(format, row[i]);
					}
				}
				to.WriteLine();
			}
		}

		int m_columns;
		List<List<string>> m_rows;
	}


	public class Postmortem
	{
		/// <summary>
		/// Attemps to postmortem the last crash on the specified devkit by searching for a dumpfile < AgeInSeconds old
		/// and analysing it
		/// </summary>
		/// <param name="Output"></param>
		/// <param name="TM"></param>
		/// <param name="Target"></param>
		/// <param name="AgeInSeconds">Age range to check, back, if 0 will find the latest</param>
		/// <returns></returns>
		public static bool TryPostmortemOnDevkit(TextWriter Output, ORTMAPILib.ORTMAPI TM, ORTMAPILib.ITarget Target, int AgeInSeconds=0)
		{
			uint Flags = 0;
			sbyte DriveLetter = TM.GetPFSDrive(out Flags);

			if (Flags <= 0)
			{
				Output.WriteLine("Could not find mapped drive for target {0}", Target.CachedName);
				return false;
			}

			string Drive = string.Format("{0}:\\", (char)DriveLetter);
			string DataPath1 = Path.Combine(Drive, Target.HostName, "data");
			string DataPath2 = Path.Combine(Drive, Target.CachedName, "data");
			string DumpPath = null;

			if (Directory.Exists(DataPath1))
			{
				DumpPath = Path.Combine(DataPath1, "sce_coredumps");
			}
			else if (Directory.Exists(DataPath2))
			{
				DumpPath = Path.Combine(DataPath2, "sce_coredumps");
			}

			if (string.IsNullOrEmpty(DumpPath))
			{
				Output.WriteLine("Could not find mapped path for target {0} at {1} or {2}", Target.CachedName, DataPath1, DataPath2);
				return false;
			}

			if (Directory.Exists(DumpPath) == false)
			{
				Output.WriteLine("Could not find sce_coredump location for target {0} at {1}", Target.CachedName, DumpPath);
				return false;
			}

			DirectoryInfo Di = new DirectoryInfo(DumpPath);
			
			Output.WriteLine("{0} contains {1} files named *.orbisdmp", DumpPath, Di.GetFiles("*.orbisdmp", SearchOption.AllDirectories).Length);

			// find all orbisdmp in this time range, sort by descending and take the first
			IEnumerable<FileInfo> DumpFiles = Di.GetFiles("*.orbisdmp", SearchOption.AllDirectories).OrderByDescending(F => F.CreationTime);

			if (AgeInSeconds > 0)
			{
				DateTime TimeNow = DateTime.Now;
				Output.WriteLine("Searching {0} for *.orbisdmp created {1} seconds before {2}", DumpPath, AgeInSeconds, TimeNow);
				DumpFiles = DumpFiles.Where(F => (TimeNow - F.CreationTime).TotalSeconds <= AgeInSeconds);
			}

			FileInfo DumpFile = DumpFiles.FirstOrDefault();

			if (DumpFile == null)
			{
				Output.WriteLine("No dump file found at {0} created < {1} seconds ago. (Are dumps enabled with Keep Corefiles?)", Di.FullName, AgeInSeconds);
				return false;
			}
			else
			{
				Output.WriteLine("Will attempt to postmortem crash from {0}", DumpFile.CreationTime);
			}

			return TryPostmortemOnFile(DumpFile.FullName, Output, Target);
		}

		/// <summary>
		/// Attempts to postmortem analyze the specified dumpfile
		/// </summary>
		/// <param name="DumpPath"></param>
		/// <param name="Output"></param>
		/// <param name="Target"></param>
		/// <returns></returns>
		public static bool TryPostmortemOnFile(string DumpPath, TextWriter Output, ORTMAPILib.ITarget Target)
		{
			FileInfo DumpFile = new FileInfo(DumpPath);

			bool ProcessedDump = false;

			if (DumpFile.Exists == false)
			{
				Output.WriteLine("Dumpfile {0} not found", DumpPath);
				return false;
			}

			DateTime StartTime = DateTime.Now;
		
			try
			{
				int LoadTimeoutInSeconds = 180;

				ICoreFileAPI CF = new CoreFileAPI();

				// try to load the core file
				Output.WriteLine("Attempting to load postmortem {0}", DumpPath);
				ITarget CFTarget = CF.LoadCoreFile(DumpFile.FullName, LoadTimeoutInSeconds * 1000);


				var Process = CFTarget.Process;

				var exceptionInfo = Process.ExceptionInfo;

				var SelectedThread = Utility.GetThreadByID(Process.Threads, exceptionInfo.ThreadID);
				var SelectedFrameIndex = Utility.FindFirstFrameWithDebugInfo(SelectedThread);

				Output.WriteLine();
				PrintCoreFileInformation(Output, Target);
				PrintCrashSummary(Output, Process);
				PrintCallStack(Output, Process, SelectedThread);
				Output.WriteLine();

				ProcessedDump = true;
			}
			catch (Exception Ex)
			{
				Output.WriteLine("Attempt to read dump file {0} failed in {1} secs: {2}", DumpFile.FullName, (DateTime.Now - StartTime).TotalSeconds, Ex.ToString());
			}

			return ProcessedDump;
		}

		static void PrintCoreFileInformation(TextWriter LogWriter, ORTMAPILib.ITarget Target)
		{
			if (Target != null)
			{
				var typeInfo = FormatCoreFileType(Target);
				if (string.IsNullOrEmpty(typeInfo))
					return;

				LogWriter.WriteLine(typeInfo);
			}
		}

		static void PrintCrashSummary(TextWriter LogWriter, IProcess Process)
		{
			string[] ExceptionInfo = Process.ExceptionInfo.Description.Split('\n');

			//Segmentation violation encountered at 0x00000000045C8AAC in OrionClient.self.
			//The instruction at 0x00000000045C8AAC referenced memory at 0x0000000000000003.
			//The memory could not be written.
			//
			//Additional information:
			//Access type : write
			//Origin : user space
			//Page presence : not present

			// The description format (as of 4.5) is above, but we want something that's similar to other platforms 
			// for automated parsing so we're going to play with this a little so it starts with the line
			// Unhandled Exception: <some clause>. Some scrapers rely on this....

			if (ExceptionInfo.Length > 4)
			{
				string FirstLine = string.Format("Unhandled Exception: {0} {1}", ExceptionInfo[1], ExceptionInfo[2]);

				ExceptionInfo = new[] { FirstLine }.Concat(ExceptionInfo.Where((s, i) => i != 1 && i != 2)).ToArray();
			}

			LogWriter.WriteLine("Postmortem Cause: {0}", ExceptionInfo);
		}

		static void PrintCallStack(TextWriter LogWriter, IProcess Process, IThread Thread)
		{
			//
			// Callstack lines should be written in the standard Unreal format
			//
			//	[Callstack] 0xaddress module!func [file.ext:line]
			// 
			// E.g. [Callstack] 0x045C8D01 OrionClient.self!UEngine::PerformError() [D:\Epic\Orion\Engine\Source\Runtime\Engine\Private\UnrealEngine.cpp:6481]
			//
			// Module may be omitted, everything else should be present, or substituted with a string that conforms to the expected type
			//
			// E.g [Callstack] 0x00000000 UnknownFunction []
			//
			// 
			var SelectedIndex = Utility.FindFirstFrameWithDebugInfo(Thread);

			var frames = Thread.Frames;

			var table = new StringTable(3);
			var headings = table.AddRow();

			for (int i = 0; i != frames.Count; ++i)
			{
				var frame = frames[i];

				var row = table.AddRow();

				row[0] = "[Callstack]";

				if ((frame.Flags & StackFrameFlags.FrameAnnotation) == 0)
				{
					row[1] = PrettyPrinter.ZeroPaddedHex((uint)frame.PC);
				}

				row[2] = FormatStackFrameFunctionName(Process, frame);
			}

			LogWriter.WriteLine("Postmortem Callstack:");
			table.PrintTo(LogWriter);
		}

		static bool CoreFileHasAllMemory(ORTMAPILib.ITarget15 target15, out bool hasAllMemory)
		{
			object names;
			try
			{
				hasAllMemory = !target15.HasMissingMemoryDumps(out names);
				return true;
			}
			catch (System.Exception)
			{
				hasAllMemory = false;
				return false;
			}
		}

		static bool CoreFileHasManifest(ORTMAPILib.ITarget15 target, out bool hasManifest)
		{
			try
			{
				hasManifest = target.HasManifest();
				return true;
			}
			catch
			{
				hasManifest = false;
				return false;
			}
		}

		static string FormatCoreFileType(ORTMAPILib.ITarget target)
		{
			try
			{
				var target15 = target as ORTMAPILib.ITarget15;
				if (target15 == null)
					return string.Empty;

				var typeBits = target15.GetCoreDumpType();

				bool hasManifest;
				bool couldCheckForManifest = CoreFileHasManifest(target15, out hasManifest);

				bool hasAllMemory;
				bool couldCheckForAllMemory = CoreFileHasAllMemory(target15, out hasAllMemory);

				var sb = new StringBuilder();

				sb.Append(
					((typeBits & 0x01) == (uint)ORTMAPILib.eCoreDumpContent.COREDUMP_FULLDUMP)
					? Strings.FullDump
					: Strings.MiniDump);
				sb.Append(", ");

				sb.Append(
					((typeBits & 0x02) == (uint)ORTMAPILib.eCoreDumpContent.COREDUMP_HAS_USER_DATA)
					? Strings.UserData
					: Strings.NoUserData);
				sb.Append(", ");

				sb.Append(
					((typeBits & 0x04) == (uint)ORTMAPILib.eCoreDumpContent.COREDUMP_HAS_USER_FILE)
					? Strings.UserFile
					: Strings.NoUserFile);

				if ((couldCheckForAllMemory || couldCheckForManifest)
					&& (!(hasManifest && hasAllMemory)))
				{
					sb.Append(", (");

					if (couldCheckForManifest && !hasManifest)
					{
						sb.Append(Strings.MissingManifestFile);
					}

					if (couldCheckForAllMemory && !hasAllMemory)
					{
						if (couldCheckForManifest && !hasManifest)
						{
							sb.Append(", ");
						}

						sb.Append(Strings.MissingMemoryFiles);
					}

					sb.Append(")");
				}

				return sb.ToString();
			}
			catch
			{
				return string.Empty;
			}
		}

		static string FormatStackFrameFunctionName(IProcess process, IStackFrame frame)
		{
			//
			// Callstack lines should be written in the standard Unreal format
			//
			//	[Callstack] 0xaddress module!func [file.ext:line]
			// 
			// E.g. [Callstack] 0x045C8D01 OrionClient.self!UEngine::PerformError() [D:\Epic\Orion\Engine\Source\Runtime\Engine\Private\UnrealEngine.cpp:6481]
			//
			// Module may be omitted, everything else should be present, or substituted with a string that conforms to the expected type
			//
			// E.g [Callstack] 0x00000000 UnknownFunction []
			//
			// 

			// Sometimes the Debugger will generate stack frames which don't correspond to a real
			// source location, these frames are flagged as annotations.
			// Check for that first, as the other fields (PC, FunctionName etc.) won't be valid.
			if ((frame.Flags & StackFrameFlags.FrameAnnotation) != 0)
			{
				return frame.Annotation;
			}

			var sb = new StringBuilder();

			var module = frame.Module;
			if (module != null)
			{
				sb.AppendFormat("{0}!", module.Name);
			}

			var funcName = frame.FunctionName;
			if (!string.IsNullOrEmpty(funcName))
			{
				sb.Append(funcName);
				sb.Append("()");
			}
			else
			{
				sb.AppendFormat("UnknownFunction");
			}

			// AddressToLine only returns a single line number, but in reality
			// it's common for an address to be associated with a range of source lines.
			// IStackFrame2 allows access to this data.
			var frame2 = frame as IStackFrame2;
			if (frame2 != null)
			{
				var docContext = frame2.DocumentContext;
				if (docContext != null)
				{
					var document = docContext.Document;

					if (document != null)
					{
						TextPosition begin;
						TextPosition end;
						docContext.GetSourceRange(out begin, out end);

						sb.AppendFormat(" [{0}:{1}]", document.Filename, end.Line);
					}
				}
				else
				{
					sb.AppendFormat(" [UnknownFile]");
				}
			}
			else
			{
				var lineInfo = process.AddressToLine(frame.PC);

				if (lineInfo != null)
				{
					sb.AppendFormat(" [{0}:{1}]", lineInfo.File, lineInfo.LineNumber);
				}
				else
				{
					sb.AppendFormat(" [UnknownFile]");
				}
			}

			return sb.ToString();
		}
	}
		
}
