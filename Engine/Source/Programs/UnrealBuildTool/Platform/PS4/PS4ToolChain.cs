// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;
using System.Linq;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	class PS4ToolChain : UEToolChain
	{
		bool bEnableLTO;
		bool bCreatePGO;

		// cache the location of SDK tools
		string ClangPath;
		string SnarlPath;

		string PGOInput;
		string PGOType;

		public PS4ToolChain(bool InbEnableLTO, bool InbCreatePGO, string InPGOInput, string InPGOType)
			: base(CppPlatform.PS4)
		{
			bEnableLTO = InbEnableLTO;
			bCreatePGO = InbCreatePGO;
			PGOInput = InPGOInput;
			PGOType = InPGOType;

			// Make sure the SDK is installed
			string BaseSDKPath = Environment.GetEnvironmentVariable("SCE_ORBIS_SDK_DIR");
			if (String.IsNullOrEmpty(BaseSDKPath))
			{
				throw new BuildException("PS4 SDK is not installed");
			}

			BaseSDKPath = BaseSDKPath.Replace("\"", "");
			if (String.IsNullOrEmpty(BaseSDKPath))
			{
				throw new BuildException("PS4 SDK is not installed");
			}

			ClangPath = Path.Combine(BaseSDKPath, "host_tools/bin/orbis-clang.exe");
			SnarlPath = Path.Combine(BaseSDKPath, "host_tools/bin/orbis-snarl.exe");
		}

		static string GetCLArguments_PerFile(CppCompileEnvironment CompileEnvironment, string Filename)
		{
			string Result = "";

			// files to not optimize
			List<string> Files = new List<string>
			{
				"PS4NoOpt.cpp",
			};


			bool bForceNoOptimizations = Files.Contains(Path.GetFileName(Filename)) || !CompileEnvironment.bOptimizeCode;

			// optimization level
			// Do not optimize if module is marked as never, or if we're debug and the module is not marked as always
			if (bForceNoOptimizations)
			{
				Result += " -O0";
			}
			else
			{
				Result += " -O3";
				Result += " -ffast-math";
				Result += " -funroll-loops";
			}

			return Result;
		}

		string GetCLArguments_Global(CppCompileEnvironment CompileEnvironment)
		{
			string Result = "";

			// build up the commandline common to C and C++
			Result += " -c";
			Result += " -fdiagnostics-format=msvc";
			Result += " -Wall -Werror";
			Result += " -Wdelete-non-virtual-dtor";
			Result += " -mno-omit-leaf-frame-pointer";

			Result += " -Wno-unused-variable";
			// this will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Result += " -Wno-unused-function";
			// this hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Result += " -Wno-switch";
			// this hides the "use of logical '||' with constant operand" which we use with, ie GIsEditor || IsRunningCommandlet(), which we want to be valid.
			Result += " -Wno-constant-logical-operand";
			// this hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
			Result += " -Wno-tautological-compare";
			// this hides the "private field 'XXX' is not used", because it's not always right (see FSkeletalMeshVertexBuffer::bInflucencesByteSwapped)
			Result += " -Wno-unused-private-field";
			Result += " -Wno-invalid-offsetof"; // needed to suppress warnings about using offsetof on non-POD types.
			Result += " -Wno-inconsistent-missing-override";
			Result += " -Wno-unused-local-typedef";

			// we use this feature to allow static FNames.
			Result += " -Wno-gnu-string-literal-operator-template";

			// bEnableLTO is set in project settings, bAllowLTCG can be set by Foo.Target.cs if needed
			if (bCreatePGO)
			{
				Result += " -D \"WITH_PGO_OUTOUT\" -fprofile-instr-generate";
			}
			else if (bEnableLTO || CompileEnvironment.bAllowLTCG)
			{
				// if a PGO file was specified, use that...
				if (string.IsNullOrEmpty(PGOInput) == false)
				{
					// two types are available on PS4, sample and instrumented
					if (PGOType == "sampled")
					{
						// If the sampled data does not refer to a function a warning will be generated, which we disable
						Result += " -Wno-backend-plugin -fprofile-sample-use=" + PGOInput;
					}
					else if (PGOType == "instrumented")
					{
						Result += " -fprofile-instr-use=" + PGOInput;
					}
					else
					{
						// UEBuildPS4 should have validated this..
						throw new Exception("Unsupported & unvalidated PGO type " + PGOType);
					}
				}
				else
				{
					// else just plain LTO
					Result += " -flto";
				}
			}

			// Profile Guided Optimization (PGO)
			if (CompileEnvironment.bPGOOptimize)
			{
				//
				// Clang emits a warning for each compiled function that doesn't have a matching entry in the profile data.
				// This can happen when the profile data is older than the binaries we're compiling.
				//
				// We treat all warnings as errors, so this would kill the build. Demote this error back to a warning.
				//

				// @TODO: Completely disabling this warning for now. It's far too verbose. 
				Result += " -Wno-backend-plugin";

				// Add the path to the instrumented PGO profile data
				Result += string.Format(" -fprofile-instr-use=\"{0}\"", Path.Combine(CompileEnvironment.PGODirectory, CompileEnvironment.PGOFilenamePrefix));
			}
			else if (CompileEnvironment.bPGOProfile)
			{
				Result += " -fprofile-instr-generate";
			}

			if (CompileEnvironment.bEnableShadowVariableWarnings)
			{
				Result += " -Wshadow" + (CompileEnvironment.bShadowVariableWarningsAsErrors ? "" : " -Wno-error=shadow");
			}

			if (CompileEnvironment.bEnableUndefinedIdentifierWarnings)
			{
				Result += " -Wundef" + (CompileEnvironment.bUndefinedIdentifierWarningsAsErrors ? "" : " -Wno-error=undef");
			}

			// shipping builds will cause this warning with "ensure", so disable only in those case
			if (CompileEnvironment.Configuration == CppConfiguration.Shipping)
			{
				Result += " -Wno-unused-value";
			}

			// debug info
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Result += " -g";
				//                Result += " -ginlined-scopes";   // debug info for inlined functions
			}

			// Set optimization options
			if(!CompileEnvironment.bOptimizeCode)
			{
				Result += " -O0";
			}
			else
			{
				Result += " -O3";
				Result += " -ffast-math";
				Result += " -funroll-loops";
			}

			if (!CompileEnvironment.bUseInlining)
			{
				Result += " -fno-inline-functions";
			}

			return Result;
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -x c++";
			Result += " -std=c++14";
			return Result;

		}

		static string GetCompileArguments_C()
		{
			string Result = "";
			Result += " -x c";
			return Result;
		}

		static string GetCompileArguments_PCH()
		{
			string Result = "";
			Result += " -x c++-header";
			Result += " -std=c++14";
			return Result;
		}

		static string GetLinkArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			if (LinkEnvironment.bAllowASLR == false)
			{
				Result += " -Wl,--addressing=non-aslr";
				Result += " -Wl,-Map=\"" + LinkEnvironment.IntermediateDirectory + "/Symbols.map\"";
				Result += " -Wl,-sn-full-map";
			}
			return Result;
		}

		static string GetSnarlArguments(LinkEnvironment LinkEnvironment)
		{
			return "";
		}

		public static void CompileOutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null) && (Line.Data != ""))
			{

				bool bWasHandled = false;
				// does it look like an error? something like this:
				//     2>Core/Inc/UnStats.h:478:3: error : no matching constructor for initialization of 'FStatCommonData'

				try
				{
					if (!Line.Data.StartsWith(" ") && !Line.Data.StartsWith(","))
					{
						// if we split on colon, an error will have at least 4 tokens
						string[] Tokens = Line.Data.Split("(".ToCharArray());
						if (Tokens.Length > 1)
						{
							// make sure what follows the parens is what we expect
							string Filename = Path.GetFullPath(Tokens[0]);
							// build up the final string
							string Output = string.Format("{0}({1}", Filename, Tokens[1], Line.Data[0]);
							for (int T = 3; T < Tokens.Length; T++)
							{
								Output += Tokens[T];
								if (T < Tokens.Length - 1)
								{
									Output += "(";
								}
							}

							// output the result
							Log.TraceInformation(Output);
							bWasHandled = true;
						}
					}
				}
				catch (Exception)
				{
					bWasHandled = false;
				}

				// write if not properly handled
				if (!bWasHandled)
				{
					Log.TraceInformation("{0}", Line.Data);
				}
			}
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName, ActionGraph ActionGraph)
		{
			string Arguments = GetCLArguments_Global(CompileEnvironment);
			string PCHArguments = "";

			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				var PCHExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.PS4).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);
				// Add the precompiled header file's path to the include path so GCC can find it.
				// This needs to be before the other include paths to ensure GCC uses it instead of the source header file.
				PCHArguments += string.Format(" -include \"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath.Replace(PCHExtension, ""));
			}

			// Add include paths to the argument list.
			foreach (string IncludePath in CompileEnvironment.IncludePaths.UserIncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}
			foreach (string IncludePath in CompileEnvironment.IncludePaths.SystemIncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				string DefinitionArgument = Definition.Contains("\"") ? Definition.Replace("\"", "\\\"") : Definition;
				Arguments += string.Format(" -D \"{0}\"", DefinitionArgument);
			}

			Arguments += string.Format(" -D \"__PS4__\"");

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = ActionGraph.Add(ActionType.Compile);
				string FileArguments = "";
				bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";

				// Add C or C++ specific compiler arguments.
				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					FileArguments += GetCompileArguments_PCH();
				}
				else if (bIsPlainCFile)
				{
					FileArguments += GetCompileArguments_C();
				}
				else
				{
					FileArguments += GetCompileArguments_CPP();


					// only use PCH for .cpp files
					FileArguments += PCHArguments;
				}

				// per file flags
				FileArguments += GetCLArguments_PerFile(CompileEnvironment, SourceFile.AbsolutePath);

				// Add the C++ source file and its included files to the prerequisite item list.
				AddPrerequisiteSourceFile(CompileEnvironment, SourceFile, CompileAction.PrerequisiteItems);

				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					var PCHExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.PS4).GetBinaryExtension(UEBuildBinaryType.PrecompiledHeader);

					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + PCHExtension
							)
						);

					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" -o \"{0}\"", PrecompiledHeaderFile.AbsolutePath, false);
				}
				else
				{
					if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
					}

					var ObjectFileExtension = UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.PS4).GetBinaryExtension(UEBuildBinaryType.Object);

					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension
							)
						);
					CompileAction.ProducedItems.Add(ObjectFile);
					Result.ObjectFiles.Add(ObjectFile);

					FileArguments += string.Format(" -o \"{0}\"", ObjectFile.AbsolutePath, false);
				}

				// Add the source file path to the command-line.
				FileArguments += string.Format(" \"{0}\"", SourceFile.AbsolutePath);

				// Gets the target file so we can get the correct output path.
				FileItem TargetFile = CompileAction.ProducedItems[0];

				// Creates the path to the response file using the name of the output file and creates its contents.
				FileReference ResponseFileName = new FileReference(TargetFile.AbsolutePath + ".response");
				string ResponseFileContents = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
				ResponseFileContents = (ActionThread.ExpandEnvironmentVariables(ResponseFileContents));
				List<string> InputFileNames = new List<string>();
				InputFileNames.Add(string.Format("{0}", ResponseFileContents));
				// Adds the response file to the compiler input.
				CompileAction.CommandArguments += string.Format("@\"{0}\"", ResponseFile.Create(ResponseFileName, InputFileNames));

				CompileAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
				CompileAction.CommandPath = ClangPath;
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));

				CompileAction.OutputEventHandler = new DataReceivedEventHandler(CompileOutputReceivedDataEventHandler);

				// VC++ always outputs the source file name being compiled, so we don't need to emit this ourselves
				CompileAction.bShouldOutputStatusDescription = true;

				// Don't farm out creation of precompiled headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely =
					CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
					CompileEnvironment.bAllowRemotelyCompiledPCHs;
			}

			return Result;
		}

		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, ActionGraph ActionGraph)
		{
			// Create an action that invokes the linker.
			Action LinkAction = ActionGraph.Add(ActionType.Link);
			LinkAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			LinkAction.CommandPath = LinkEnvironment.bIsBuildingLibrary ? SnarlPath : ClangPath;

			// Get link arguments.
			LinkAction.CommandArguments = LinkEnvironment.bIsBuildingLibrary ? GetSnarlArguments(LinkEnvironment) : GetLinkArguments(LinkEnvironment);


			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);
			LinkAction.StatusDescription = string.Format("{0}", Path.GetFileName(OutputFile.AbsolutePath));

			if (LinkEnvironment.bIsBuildingLibrary)
			{
				// work around SNARL behavior change.  From 1.6 -> 1.7 behavior changed from 'replace' to 'append'.
				// so delete so we don't append new .o's on top of old ones.
				LinkAction.bShouldDeleteProducedItems = true;

				// Add the output file to the command-line.
				LinkAction.CommandArguments += string.Format(" \"{0}\"", OutputFile.AbsolutePath);
			}
			else
			{
				// Add the output file to the command-line.
				LinkAction.CommandArguments += string.Format(" -o \"{0}\"", OutputFile.AbsolutePath);

				if (LinkEnvironment.Configuration != CppConfiguration.Shipping)
				{
					// Map file is also a produced item of the link
					FileReference MapFilename = FileReference.Combine(LinkEnvironment.IntermediateDirectory, "Symbols.map");
					FileItem MapOutputFile = FileItem.GetItemByFileReference(MapFilename);
					LinkAction.ProducedItems.Add(MapOutputFile);
				}
			}

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				// work around a bug with orbis-snarl.  Quoting strings inside the response file will break.  Surprisingly,
				// unquoted strings with spaces still work inside the response file.
				if (LinkEnvironment.bIsBuildingLibrary)
				{
					InputFileNames.Add(string.Format("{0}", InputFile.AbsolutePath));
				}
				else
				{
					InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));//.Replace("\\", "/")));
				}
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			// Create a response file for the linker 
			FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);

			LinkAction.CommandArguments += string.Format(" @\"{0}\"", ResponseFile.Create(ResponseFileName, InputFileNames));

			// libs don't link in other libs
			if (!LinkEnvironment.bIsBuildingLibrary)
			{
				// Add the library paths to the argument list.
				foreach (string LibraryPath in LinkEnvironment.LibraryPaths)
				{
					LinkAction.CommandArguments += string.Format(" -L\"{0}\"", LibraryPath);
				}

				// add libraries in a library group
				LinkAction.CommandArguments += string.Format(" -Wl,--start-group");
				foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
				{
					if (String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)))
					{
						LinkAction.CommandArguments += string.Format(" \"-l{0}\"", AdditionalLibrary);
					}
					else
					{
						// full pathed libs are compiled by us, so we depend on linking them
						LinkAction.CommandArguments += string.Format(" \"{0}\"", AdditionalLibrary);
						LinkAction.PrerequisiteItems.Add(FileItem.GetItemByPath(AdditionalLibrary));
					}
				}
				LinkAction.CommandArguments += string.Format(" -Wl,--end-group");
			}

			// Add the additional arguments specified by the environment.
			LinkAction.CommandArguments += LinkEnvironment.AdditionalArguments;
			LinkAction.CommandArguments.Replace("\\", "/");

			// Always use the same output parser to prevent fences between XGE batches
			LinkAction.OutputEventHandler = new DataReceivedEventHandler(CompileOutputReceivedDataEventHandler);

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			return OutputFile;
		}

		private ICollection<FileItem> GenerateSymbols(FileItem Executable, LinkEnvironment LinkEnvironment, ActionGraph ActionGraph)
		{
			// Parse map file and output function names in SymbolNames.bin and 
			// Function Address, Function Size and Function Name offset into Symbols.bin

			string InputMapFilename = LinkEnvironment.IntermediateDirectory+ "\\Symbols.map";

			string ConfigName = LinkEnvironment.IntermediateDirectory.GetDirectoryName();

			// Tool takes a path and prefix - e.g. c:\foo\test- - and will write out c:\foo\test-symbols.bin, test-symbolnames.bin etc
			string OutputFilenameBase = System.IO.Path.Combine(LinkEnvironment.OutputFilePath.Directory.FullName, @"..\..\Build\PS4\Symbols", ConfigName + "-").ToLower();

			string SelfPath = LinkEnvironment.OutputFilePath.FullName;
			
			// Symbol tool generates three files used for lookups
			FileItem OutputSymbols = FileItem.GetItemByPath(OutputFilenameBase + "symbols.bin");
			FileItem OutputSymbolNames = FileItem.GetItemByPath(OutputFilenameBase + "symbolnames.bin");
			FileItem OutputSymbolMetaData = FileItem.GetItemByPath(OutputFilenameBase + "symbolmetadata.txt");

			Action PostBuildAction = ActionGraph.Add(ActionType.Link);
			PostBuildAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory.FullName;
			PostBuildAction.CommandPath = Path.Combine(Directory.GetCurrentDirectory(), "../Binaries/DotNET/PS4/PS4SymbolTool.exe");

			// Can pass in -verbose for more info
			PostBuildAction.CommandArguments = string.Format("generate -map=\"{0}\" -self=\"{1}\" -output=\"{2}\"", InputMapFilename, SelfPath, OutputFilenameBase);

			// We depend on the .exe file generated by the linker
			PostBuildAction.PrerequisiteItems.Add(Executable);

			PostBuildAction.ProducedItems.AddRange(new List<FileItem>() { OutputSymbols, OutputSymbolNames, OutputSymbolMetaData });
			PostBuildAction.bPrintDebugInfo = true;
			PostBuildAction.StatusDescription = "PS4SymbolTools";

			return new List<FileItem>() { OutputSymbols, OutputSymbolNames, OutputSymbolMetaData };
		}


		public override ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment BinaryLinkEnvironment, ActionGraph ActionGraph)
		{
			List<FileItem> OutputFiles = new List<FileItem>();

			// No symbols in shipping due to ASLR
			if (BinaryLinkEnvironment.bIsBuildingLibrary == false 
				&& BinaryLinkEnvironment.Configuration != CppConfiguration.Shipping)
			{
				OutputFiles.AddRange(GenerateSymbols(Executable, BinaryLinkEnvironment, ActionGraph));
			}

			return OutputFiles;
		}
    }
	
}
