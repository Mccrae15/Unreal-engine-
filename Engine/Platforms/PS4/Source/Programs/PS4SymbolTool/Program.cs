// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace PS4SymbolTools
{
	class Program
	{
		/// <summary>
		/// Helper for parsing values. 
		/// </summary>
		/// <param name="Param"></param>
		/// <param name="Default"></param>
		/// <returns></returns>
		static string ParseParamValue(string Param, string Default = null)
		{
			if (!Param.EndsWith("="))
			{
				Param += "=";
			}
			foreach (string Arg in Environment.GetCommandLineArgs())
			{
				string StringArg = Arg;

				if (StringArg.StartsWith("-"))
				{
					StringArg = StringArg.Substring(1);
				}

				if (StringArg.StartsWith(Param, StringComparison.InvariantCultureIgnoreCase))
				{
					return StringArg.Substring(Param.Length);
				}
			}
			return Default;
		}

		/// <summary>
		/// Read and parse map file into portable symbols file
		/// </summary>
		/// <param name="MapReader"></param>
		/// <param name="SymbolsWriter"></param>
		/// <param name="SymbolNameWriter"></param>
		/// <returns></returns>
		static void WriteSymbolInfo(BinaryWriter SymbolsWriter, BinaryWriter SymbolNameWriter, uint Address, uint FunctionSize, string FunctionString, ref int CurrentFunctionNameOffset)
		{
			int FunctionNameLength = FunctionString.Length;
			SymbolsWriter.Write(Address);
			SymbolsWriter.Write(FunctionSize);
			SymbolsWriter.Write((uint)CurrentFunctionNameOffset);
			SymbolNameWriter.Write(FunctionString);

			CurrentFunctionNameOffset += FunctionNameLength;

			// Number of bytes used to store size of string
			int NumBytesForStringSize = 0;
			do
			{
				FunctionNameLength >>= 7;
				NumBytesForStringSize++;
			} while (FunctionNameLength != 0);
			CurrentFunctionNameOffset += NumBytesForStringSize;
		}

		/// <summary>
		/// Read and parse map file into portable symbols file
		/// </summary>
		/// <param name="MapReader"></param>
		/// <param name="SymbolsWriter"></param>
		/// <param name="SymbolNameWriter"></param>
		/// <returns></returns>
		static void ReadMapFile(StreamReader MapReader, BinaryWriter SymbolsWriter, BinaryWriter SymbolNameWriter)
		{
			int CurrentFunctionNameOffset = 0;
			string InputString = String.Empty;
			while ((InputString = MapReader.ReadLine()) != null)
			{
				if (InputString.Length > 49)
				{
					// Check for a function
					if (InputString[21] == ' ' && InputString[22] == '0')
					{
						// Get address
						string AddressString = InputString.Substring(0, 8);

						uint Address = uint.Parse(AddressString, System.Globalization.NumberStyles.HexNumber);
						if (Address != 0)
						{
							// Get size
							string SizeString = InputString.Substring(10, 8);

							uint FunctionSize = uint.Parse(SizeString, System.Globalization.NumberStyles.HexNumber);
							if (FunctionSize != 0)
							{
								// Get function name
								string FunctionString = InputString.Substring(48);
								WriteSymbolInfo(SymbolsWriter, SymbolNameWriter, Address, FunctionSize, FunctionString, ref CurrentFunctionNameOffset);
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Read and parse nm symbol table into portable symbols file
		/// </summary>
		/// <param name="SymbolReader"></param>
		/// <param name="SymbolsWriter"></param>
		/// <param name="SymbolNameWriter"></param>
		/// <returns></returns>
		static void ReadSymbolFile(StreamReader SymbolReader, BinaryWriter SymbolsWriter, BinaryWriter SymbolNameWriter)
		{
			int CurrentFunctionNameOffset = 0;
			string InputString = String.Empty;
			while ((InputString = SymbolReader.ReadLine()) != null)
			{
				if (InputString.Length > 36)
				{
					// Check for a function
					if (InputString[33] == ' ' && (InputString[34] == 'T' || InputString[34] == 't') && InputString[35] == ' ')
					{
						// Get address
						string AddressString = InputString.Substring(0, 16);

						uint Address = uint.Parse(AddressString, System.Globalization.NumberStyles.HexNumber);
						if (Address != 0)
						{
							// Get size
							string SizeString = InputString.Substring(17, 16);

							uint FunctionSize = uint.Parse(SizeString, System.Globalization.NumberStyles.HexNumber);
							if (FunctionSize != 0)
							{
								// Get function name
								string FunctionString = InputString.Substring(36);
								WriteSymbolInfo(SymbolsWriter, SymbolNameWriter, Address, FunctionSize, FunctionString, ref CurrentFunctionNameOffset);
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Generate actual symbols
		/// </summary>
		/// <param name="MapFile"></param>
		/// <param name="bUsingMapFile"></param>
		/// <param name="SelfFile"></param>
		/// <param name="OutputFilenameBase"></param>
		/// <returns></returns>
		static void GenerateSymbols(string InputSymbolFile, bool bUsingMapFile, string SelfFile, string OutputFilenamePrefix, bool Verbose)
		{
			OutputFilenamePrefix = OutputFilenamePrefix.ToLower();

			string OutputSymbols = OutputFilenamePrefix + "symbols.bin";
			string OutputSymbolNames = OutputFilenamePrefix + "symbolnames.bin";
			string OutputSymbolMetaData = OutputFilenamePrefix + "symbolmetadata.txt";
		
			try
			{
				// make the subdirectory if needed
				string DestSubdir = Path.GetDirectoryName(OutputFilenamePrefix);
				if (!Directory.Exists(DestSubdir))
				{
					Directory.CreateDirectory(DestSubdir);
				}

				using (StreamReader Reader = File.OpenText(InputSymbolFile))
				using (BinaryWriter SymbolsWriter = new BinaryWriter(File.Open(OutputSymbols, FileMode.Create)))
				using (BinaryWriter SymbolNameWriter = new BinaryWriter(File.Open(OutputSymbolNames, FileMode.Create)))
				{
					if (bUsingMapFile)
					{
						ReadMapFile(Reader, SymbolsWriter, SymbolNameWriter);
					}
					else
					{
						ReadSymbolFile(Reader, SymbolsWriter, SymbolNameWriter);
					}
				}

				using (StreamWriter MetaDataWriter = new StreamWriter(File.Open(OutputSymbolMetaData, FileMode.Create)))
				{
					if (File.Exists(SelfFile))
					{
						// Write out the binary name. MProf2 uses this to try and automatically find the source .self file when loading PS4 profiles.
						MetaDataWriter.WriteLine("SelfFile:{0}", SelfFile);

						// Write out the binary timestamp. MProf2 uses this to warn if loading a binary that isn't the same one the PS4 profile was made against.
						MetaDataWriter.WriteLine("SelfUtcTimestamp:{0}", File.GetLastWriteTimeUtc(SelfFile).Ticks);
					}
				}

				if (Verbose)
				{
					Console.WriteLine("Wrote symbol files to {0}-*", OutputFilenamePrefix);
				}
			}
			catch (Exception Ex)
			{
				Console.Error.WriteLine("Warning - failed to generate symbol files: {0}", Ex.Message);
			}
		}

		/// <summary>
		/// Print help to the log
		/// </summary>
		/// <returns></returns>
		static void DisplayHelp()
		{
			Console.WriteLine("Usage: PS4SymbolTools generate -map=<file> -self=<file> -output=<basefilename>");
			Console.WriteLine(@"E.g  PS4SymbolTools generate -map=c:\path\target.map -self=c:\path\target.self -output=c:\path\symbols\target-");
		}

		/// <summary>
		/// Main entry point 
		/// </summary>
		/// <param name="args"></param>
		/// <returns></returns>
		static int Main(string[] args)
		{
			if (args.Length < 2 || args.Any(arg => arg.ToLower() == "-help") || args[1].ToLower() == "generate")
			{
				DisplayHelp();
				return 0;
			}

			bool Verbose = args.Any(arg => arg.ToLower() == "-verbose");

			string MapFile = ParseParamValue("map");
			string SelfFile = ParseParamValue("self");
			string Out = ParseParamValue("output");
			string NMFile = ParseParamValue("nm");

			if (Verbose)
			{
				Console.WriteLine("Command line: {0}", args.Skip(1).ToArray());
			}

			if (string.IsNullOrEmpty(SelfFile) || string.IsNullOrEmpty(Out) || (string.IsNullOrEmpty(MapFile) && string.IsNullOrEmpty(NMFile)))
			{
				Console.WriteLine("Arguments missing");
				DisplayHelp();
				return -1;
			}

			try
			{
				bool bUsingMapFile = !string.IsNullOrEmpty(MapFile);
				GenerateSymbols(bUsingMapFile ? MapFile : NMFile, bUsingMapFile, SelfFile, Out, Verbose);
			}
			catch (Exception Ex)
			{
				Console.WriteLine("Symbol generation failed: {0}", Ex);
				return -1;
			}

			return 0;
		}
	}
}
