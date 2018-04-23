// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using AutomationTool;
using UnrealBuildTool;
using System.IO;
using System.Globalization;
using System.Threading.Tasks;
using System.Threading;
using System.Diagnostics;
using System.Text.RegularExpressions;
using Tools.DotNETCommon;

public class PS4Platform : Platform
{
	public PS4Platform()
		: base(UnrealTargetPlatform.PS4)
	{
	}

	//users should rename the file to playgo-chunks.xml to enable it.
	private static readonly String PlayGoEmulationFileName = "playgo-chunks-disabled.xml";
    private static readonly String NonUFSFilesManifest = "Manifest_NonUFSFiles_PS4.txt";
	private static readonly String[] ExcludeFilesList = new String[] { PlayGoEmulationFileName, NonUFSFilesManifest };

	private static readonly String DeepFileMetaDataName = "origpath.ue4deepmeta";

	// if we remapped any files to /deepfiles then we must create metadata for each directory and subdirectory of the original path so that
	// runtime Directory iteration can work properly.	
	private static Dictionary<String, String> ReverseShortenedDictionarypaths = new Dictionary<String, String>(); //hash from hashdir -> origdir.

	private String StripRelativeAndNormalize(String InPath)
	{
		String RelativePath = InPath;

		RelativePath = RelativePath.Replace('\\', '/');
		RelativePath = RelativePath.ToLower();

		Int32 LastRelativeIndex = InPath.LastIndexOf("../");
		if (LastRelativeIndex != -1)
		{
			RelativePath = InPath.Substring(LastRelativeIndex + 3);
		}


		if (RelativePath.EndsWith("/"))
		{
			RelativePath = RelativePath.Substring(0, RelativePath.Length - 1);
		}

		return RelativePath;
	}


	/** CRC 32 polynomial */
	//enum { Crc32Poly = 0x04c11db7 };
	UInt32[] CrcTable =
	{
		0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005, 0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
		0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75, 0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
		0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5, 0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
		0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95, 0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
		0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072, 0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
		0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02, 0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
		0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692, 0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
		0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2, 0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
		0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB, 0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
		0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B, 0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
		0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B, 0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
		0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B, 0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
		0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C, 0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
		0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC, 0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
		0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C, 0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
		0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C, 0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
	};

	// Hash function here and in PS4File must match exactly.
	private UInt32 PS4FilePathHash(String InPath)
	{
		InPath = InPath.ToLower();
		UInt32 Hash = 0;
		for (int i = 0; i < InPath.Length; ++i)
		{
			Char Ch = InPath[i];
			UInt16 B = Ch;
			Hash = ((Hash >> 8) & 0x00FFFFFF) ^ CrcTable[(Hash ^ B) & 0x000000FF];
			B = (UInt16)(Ch >> 8);
			Hash = ((Hash >> 8) & 0x00FFFFFF) ^ CrcTable[(Hash ^ B) & 0x000000FF];
		}
		return Hash;
	}

	private String ShortenPath(String InPath, out bool bShortened)
	{
		//normal C# file operations replace /'s with \'s, so normalize again just in case.  We want all
		//hashing to happen on normalized paths to match the runtime.
		InPath = StripRelativeAndNormalize(InPath);

		bShortened = false;

		// Find the folder level this file exists in.
		Int32 FolderLevel = 0;
		{
			String LevelPath = InPath;
			while (LevelPath.Length > 0)
			{
				FolderLevel++;
				LevelPath = Path.GetDirectoryName(LevelPath);
			}
		}

		// maximum depth is 8 INCLUDING the filename.
		// see https://ps4.scedev.net/resources/documents/Misc/current/Package_Generator-Users_Guide/0003.html
		// we check for 6 levels because deployment sandboxing will add an extra level in some cases.
		// shipping builds in paks will be unaffected.
		if (FolderLevel >= 6)
		{
			UInt32 Hash = PS4FilePathHash(InPath);
			CultureInfo CI = new CultureInfo("en-us");
			String NewDest = "deepfiles/" + Hash.ToString("x", CI);
			bShortened = true;
			return NewDest;
		}
		else
		{
			return InPath;
		}

	}

	public override StagedFileReference Remap(StagedFileReference Dest)
	{
		// don't adjust sony system files at all
		if (Dest.Name.Contains("sce_sys") || Dest.Name.Contains("sce_module"))
		{
			return Dest;
		}

		// Move .prx files to the prx folder
		if( Dest.Name.EndsWith(".prx"))
		{
			String Filename = Path.Combine("prx", Path.GetFileName(Dest.Name));
			return new StagedFileReference(Filename.ToLower());
		}

		String FullDirectory = Path.GetDirectoryName(Dest.Name);
		String NoRelativeDirectory = StripRelativeAndNormalize(FullDirectory);

		bool bShortened;
		String ShortenedPath = ShortenPath(NoRelativeDirectory, out bShortened);

		if (bShortened)
		{
			String ExistingVal;
			if (ReverseShortenedDictionarypaths.TryGetValue(ShortenedPath, out ExistingVal))
			{
				if (ExistingVal.CompareTo(NoRelativeDirectory) != 0)
				{
					throw new AutomationException("Hashed orig dir: " + ShortenedPath + " matches 2 original dirs: " + NoRelativeDirectory + " and " + ExistingVal);
				}
			}
			else
			{
				ReverseShortenedDictionarypaths.Add(ShortenedPath, NoRelativeDirectory);
			}

			return new StagedFileReference(ShortenedPath + "/" + Path.GetFileName(Dest.Name).ToLower());
		}
		else
		{
			//every file on PS4 written as a full lower case path, always.
			return new StagedFileReference(Dest.Name.ToLower());
		}
	}

	static protected string MakeEBootFileName(UnrealTargetConfiguration TargetConfiguration)
	{
		String EbootName = "eboot";
		if (TargetConfiguration != UnrealTargetConfiguration.Development)
		{
			EbootName += TargetConfiguration.ToString();
		}
		EbootName += ".bin";

		return EbootName.ToLower();
	}

	protected bool DoPackageForSubmission(ProjectParams Params, UnrealTargetConfiguration TargetConfiguration)
	{
		return Params.Distribution && TargetConfiguration == UnrealTargetConfiguration.Shipping;
	}

	protected void MakePKGFileName(DeploymentContext SC, UnrealTargetConfiguration TargetConfiguration, ProjectParams Params, bool bBuildIsoImage, string TitleID, out String PKGPath, out String GP4Path)
	{
		string ProjectDir = Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries/PS4");

		if (Params.HasCreateReleaseVersion)
		{
			ProjectDir = Path.Combine(Params.GetCreateReleaseVersionPath(SC, Params.Client), TitleID);
		}

		string ProjectPKG = ProjectDir;

		// handle non-code projects
		if (!Params.IsCodeBasedProject)
		{
			ProjectPKG = Path.Combine(ProjectPKG, "UE4Game");
		}
		else
		{
			// strip off any extension of the executable
			ProjectPKG = Path.Combine(ProjectPKG, Path.GetFileNameWithoutExtension(Params.GetProjectExeForPlatform(UnrealTargetPlatform.PS4).ToString()));
		}

		// seems to be a bug/feature in orbis-pub-cmd where it only wants a directory if you try to generate
		// submission materials.  It comes up with its own long name based on the titleID.
		if (DoPackageForSubmission(Params, TargetConfiguration))
		{
			ProjectPKG = Path.Combine(Path.GetDirectoryName(ProjectPKG), "Submission-" + TitleID);
			Directory.CreateDirectory(ProjectPKG);
		}
		else if (bBuildIsoImage == true)
		{
			// orbis-pub-cmd needs a directory not a filename when creating isos
			ProjectPKG = Path.Combine(Path.GetDirectoryName(ProjectPKG), TargetConfiguration.ToString() + "-" + TitleID);
			Directory.CreateDirectory(ProjectPKG);
		}
		else
		{
			// append config if not Development
			if (TargetConfiguration != UnrealTargetConfiguration.Development)
			{
				ProjectPKG += "-" + PlatformType.ToString() + "-" + TargetConfiguration.ToString();
			}
			if (Params.GeneratePatch)
			{
				ProjectPKG += "-patch";
			}
			else if(Params.GenerateRemaster)
			{
				ProjectPKG += "-remaster";
			}
			if (!string.IsNullOrEmpty(TitleID))
			{
				ProjectPKG += "-" + TitleID;
			}
			ProjectPKG += ".pkg";
		}

		PKGPath = ProjectPKG;
		GP4Path = Path.ChangeExtension(PKGPath, ".gp4");
	}


	// assumes that only the filename without a path is passed in.
	private static int GetChunkIndexForFile(String FileName)
	{
		int ChunkIndex = 0;
		String ChunkSubString = "pakchunk";
		int PakChunkStringIndex = FileName.IndexOf(ChunkSubString);

		if (PakChunkStringIndex != -1)
		{
			// pakchunk filename format should look like 'pakchunk0-ps4.pak, pakchunk1-ps4.pak, etc'
			/*String[] FileNameParts = FileName.Split('-');
			String PakChunkPart = FileNameParts[0];
			String ChunkNumString = PakChunkPart.Substring(PakChunkStringIndex + ChunkSubString.Length, PakChunkPart.Length - PakChunkStringIndex - ChunkSubString.Length);*/


			int LastNumberIndex = PakChunkStringIndex + ChunkSubString.Length;
			for (; LastNumberIndex < FileName.Length; ++LastNumberIndex)
			{
				if (!Char.IsDigit(FileName[LastNumberIndex]))
					break;
			}
			String ChunkNumString = FileName.Substring(PakChunkStringIndex + ChunkSubString.Length, LastNumberIndex - PakChunkStringIndex - ChunkSubString.Length);

			if (!Int32.TryParse(ChunkNumString, out ChunkIndex))
			{
				Console.ForegroundColor = ConsoleColor.Yellow;
				Console.WriteLine("Couldn't parse filename: " + FileName + " section " + ChunkNumString + " for chunk index");
				Console.ResetColor();

				ChunkIndex = 0;
			}
		}

		return ChunkIndex;
	}

	private static void RecurseStagingDirectory(string LocalDirectory, string PS4Directory, int Depth, PerTitlePackageParameters TitleParams, StringBuilder FilesContents, StringBuilder DirectoriesContents, UnrealTargetConfiguration TargetConfiguration, bool bPlayGoEmulation, bool bCompressPakFiles, string TargetExecutable, bool bGeneratingPatch, bool bGeneratingRemaster, out int NumChunks)
	{
		NumChunks = 1;

		string[] Files = Directory.GetFiles(LocalDirectory);
		foreach (string FullFile in Files)
		{
			bool bExcluded = false;
			// don't add any explicitly excluded files to the gp4.
			foreach (String ExcludedFile in ExcludeFilesList)
			{
				if (FullFile.IndexOf(ExcludedFile) != -1)
				{
					bExcluded = true;
					break;
				}
			}

			string File = Path.GetFileName(FullFile);
			string DirNameInPackage = PS4Directory;
			foreach (var Remap in TitleParams.RemapFilesList)
			{
				// remap key is local-fs, so compare insensitively to the remap location
				if ((DirNameInPackage + File).ToLowerInvariant() == Remap.Key.ToLowerInvariant())
				{
					DirNameInPackage = Path.GetDirectoryName(Remap.Value).Replace("\\", "/") + "/";
					if (DirNameInPackage == "/")
					{
						DirNameInPackage = "";
					}
					break;
				}
				else if (DirNameInPackage + File == Remap.Value)
				{
					bExcluded = true;
					break;
				}
			}

			//we may have built multiple configurations in this run.  However, we only want the single correct eboot.bin for this configuration
			string FileNameInPackage = File;
			if (String.Compare(File, TargetExecutable, StringComparison.InvariantCultureIgnoreCase) == 0)
			{
				FileNameInPackage = "eboot.bin";
				DirNameInPackage = "";
			}
			else if (File.EndsWith(".self", StringComparison.InvariantCultureIgnoreCase))
			{
				bExcluded = true;
			}

			// include the appropriate PARAM.SFO files for patches
			if (bGeneratingPatch)
			{
				if (String.Compare(File, "param.sfo", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					bExcluded = true;
				}
				else if (String.Compare(File, "param-remaster.sfo", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					bExcluded = true;
				}
				else if (String.Compare(File, "param-patch.sfo", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					FileNameInPackage = "param.sfo";
				}
			}
			else if(bGeneratingRemaster)
			{
				if (String.Compare(File, "param.sfo", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					bExcluded = true;
				}
				else if (String.Compare(File, "param-patch.sfo", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					bExcluded = true;
				}
				else if (String.Compare(File, "param-remaster.sfo", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					FileNameInPackage = "param.sfo";
				}
			}
			else
			{
				if (String.Compare(File, "param-patch.sfo", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					bExcluded = true;
				}
				else if (String.Compare(File, "param-remaster.sfo", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					bExcluded = true;
				}
			}

			if (bExcluded)
			{
				continue;
			}

			int FileChunk = GetChunkIndexForFile(File);
			String FileLine = "    <file targ_path=\"" + DirNameInPackage + FileNameInPackage + "\"";
			if (!bPlayGoEmulation)
			{
				FileLine += " orig_path=\"" + LocalDirectory + File + "\"";
			}

				FileLine += " chunks=\"" + FileChunk.ToString() + "\"";

			if (bCompressPakFiles && Path.GetExtension(File) == ".pak")
			{
				FileLine += " pfs_compression=\"enable\"";
			}

			FileLine += "/>";
			FilesContents.AppendLine(FileLine);

			NumChunks = Math.Max(NumChunks, FileChunk + 1);
		}

		string[] Dirs = Directory.GetDirectories(LocalDirectory);
		foreach (string FullDir in Dirs)
		{
			string Dir = Path.GetFileName(FullDir);
			bool bExcluded = false;
			bool bMapped = true;
			foreach (string ExcludeDir in TitleParams.ExcludedDirList)
			{
				if (ExcludeDir == PS4Directory + Dir)
				{
					bExcluded = true;
					break;
				}
			}

			foreach (string MappedDir in TitleParams.RemappedDirList)
			{
				if (MappedDir == PS4Directory + Dir)
				{
					bMapped = false;
				}
			}
			if (bExcluded)
			{
				continue;
			}

			if (bMapped)
			{
				for (int SpaceIndex = 0; SpaceIndex < Depth; SpaceIndex++)
				{
					DirectoriesContents.Append("  ");
				}
				DirectoriesContents.AppendLine("    <dir targ_name=\"" + Dir + "\">");
			}

			int RecurseChunks;
			RecurseStagingDirectory(LocalDirectory + Dir + "\\", PS4Directory + Dir + "/", Depth + 1, TitleParams, FilesContents, DirectoriesContents, TargetConfiguration, bPlayGoEmulation, bCompressPakFiles, TargetExecutable, bGeneratingPatch, bGeneratingRemaster, out RecurseChunks);
			NumChunks = Math.Max(NumChunks, RecurseChunks);

			if (bMapped)
			{
				for (int SpaceIndex = 0; SpaceIndex < Depth; SpaceIndex++)
				{
					DirectoriesContents.Append("  ");
				}
				DirectoriesContents.AppendLine("    </dir>");
			}
		}
	}

	private class ChunkLanguageEntry
	{
		public string CultureId;
		public string Label;
		public int ChunkId;
		public bool IsDefault;
	}

	private static List<ChunkLanguageEntry> GetChunkLanguageEntries(ProjectParams Params, DeploymentContext SC)
	{
		List<ChunkLanguageEntry> ChunkLanguageEntries = new List<ChunkLanguageEntry>();
		ConfigHierarchy PlatformGameConfig = null;
		if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			List<string> ChunkLanguageList;
			PlatformGameConfig.GetArray("/Script/PS4PlatformEditor.PS4TargetSettings", "ChunkLanguageMapping", out ChunkLanguageList);

			if(ChunkLanguageList != null && ChunkLanguageList.Count() != 0)
			{
				// Remove parentheses
				ChunkLanguageList = ChunkLanguageList.Select(chunkLanguage => chunkLanguage.Trim("()".ToCharArray())).ToList();

				ChunkLanguageEntries = ChunkLanguageList.Select(language =>
				{
					ChunkLanguageEntry entry = new ChunkLanguageEntry();

					string[] languageProperties = language.Split(",".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
					foreach (string languageProperty in languageProperties)
					{
						string[] languagePropertyPair = languageProperty.Split("=".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);

						if (languagePropertyPair.Length == 2)
						{
							switch (languagePropertyPair[0].ToLower())
							{
								case "cultureid":
									entry.CultureId = languagePropertyPair[1].Trim("\"".ToCharArray());
									break;
								case "label":
									entry.Label = languagePropertyPair[1].Trim("\"".ToCharArray());
									break;
								case "chunkid":
									if (!Int32.TryParse(languagePropertyPair[1], out entry.ChunkId))
									{
										Console.ForegroundColor = ConsoleColor.Yellow;
										Console.WriteLine("Couldn't parse ChunkId " + languagePropertyPair[1] + " from ChunkLanguageMapping in section /Script/PS4PlatformEditor.PS4TargetSettings");
										Console.ResetColor();

										entry.ChunkId = -1;
									}
									break;
								case "isdefault":
									if (!bool.TryParse(languagePropertyPair[1], out entry.IsDefault))
									{
										Console.ForegroundColor = ConsoleColor.Yellow;
										Console.WriteLine("Couldn't parse IsDefault: " + languagePropertyPair[1] + " from ChunkLanguageMapping in section /Script/PS4PlatformEditor.PS4TargetSettings");
										Console.ResetColor();

										entry.IsDefault = false;
									}
									break;
							}
						}
					}

					return entry;
				}).ToList();
			}
		}

		return ChunkLanguageEntries;
	}

	private static void BuildChunkContents(StringBuilder ChunkContents, int NumChunks, bool bPlaygoEmulation, ProjectParams Params, DeploymentContext SC, bool bForceDualLayer, out String[] OutChunkLayers)
	{
		String ChunkLayerFilename = Path.Combine(SC.ProjectRoot.FullName, "Build", SC.CookPlatform, "ChunkLayerInfo", "pakchunklayers.txt");
		String[] ChunkLayerList;

		if (File.Exists(ChunkLayerFilename))
		{
			ChunkLayerList = ReadAllLines(ChunkLayerFilename);
		}
		// we couldn't find a pakchunk list.  error, or we didn't build with -manifests.
		else
		{
			ChunkLayerList = new String[1];
			ChunkLayerList[0] = "0";
		}
		
		// Update NumChunks with actual number of chunks defined in ini file and extend length of ChunkLayerList.
		// The old NumChunks was figured out by scanning all pakchunk?-ps4.pak files, but these files might not be staged.
		List<ChunkLanguageEntry> ChunkLanguageEntries = GetChunkLanguageEntries(Params, SC);
		if(ChunkLanguageEntries.Count != 0)
		{
			NumChunks = Math.Max(NumChunks, ChunkLanguageEntries.Max(chunkLanguage => chunkLanguage.ChunkId) + 1);
		}

		if (ChunkLayerList.Length > NumChunks)
		{
			NumChunks = ChunkLayerList.Length;
		}

		if (ChunkLayerList.Length != NumChunks)
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("ChunkLayer info missing or mismatched in file: " + ChunkLayerFilename);
			Console.WriteLine("ChunksFound: " + ChunkLayerList.Length.ToString() + " Chunks Required: " + NumChunks);
			foreach (String Layer in ChunkLayerList)
			{
				Console.WriteLine(Layer.ToString());
			}
			Console.ResetColor();

			ChunkLayerList = new String[NumChunks];
			for (int i = 0; i < NumChunks; ++i)
			{
				ChunkLayerList[i] = "0";
			}
		}
		OutChunkLayers = ChunkLayerList;

		ChunkContents.AppendLine("    <chunk_info chunk_count=\"" + NumChunks.ToString() + "\" scenario_count=\"1\">");

		// playgo-chunks.xml format is very picky.  We must remove anything that will cause it to fail reading the xml.
		if (bPlaygoEmulation)
		{
			ChunkContents.AppendLine("      <chunks>");
		}
		else
		{
			StringBuilder supportedLanguageList = new StringBuilder();
			string defaultLanguage = String.Empty;
			foreach (var languageEntry in ChunkLanguageEntries)
			{
				if (!String.IsNullOrEmpty(languageEntry.CultureId))
				{
					supportedLanguageList.Append(languageEntry.CultureId);
					supportedLanguageList.Append(" ");

					if (languageEntry.IsDefault)
					{
						defaultLanguage = languageEntry.CultureId;
					}
				}
			}

			if (supportedLanguageList.Length == 0)
			{
				ChunkContents.AppendLine("      <chunks supported_languages=\"\">");
			}
			else
			{ 
				ChunkContents.AppendLine("      <chunks supported_languages=\"" + supportedLanguageList.ToString() + "\" default_language=\"" + defaultLanguage + "\">");
			}
		}

		bool bAllLayer0 = true;
		for (int i = 0; i < NumChunks; ++i)
		{
			if (ChunkLayerList[i].CompareTo("0") != 0)
			{
				bAllLayer0 = false;
				break;
			}
		}

		if (bAllLayer0 && bForceDualLayer)
		{
			Console.WriteLine("Forcing final chunk to layer 1 to allow bd50: ");
			ChunkLayerList[ChunkLayerList.Length - 1] = "1";
		}

		// For patches, ensure all chunks are assigned to layer 0, or sony publishing tools will fail
		if (Params.IsGeneratingPatch)
		{
			for (int i = 0; i < NumChunks; ++i)
			{
				ChunkLayerList[i] = "0";
			}
		}

		for (int i = 0; i < NumChunks; ++i)
		{
			String ChunkLine = "        <chunk id=\"" + i.ToString() + "\" ";
			if (!bPlaygoEmulation)
			{
				ChunkLine += "layer_no=\"" + ChunkLayerList[i] + "\"";
			}

			var MatchinglanguageEntries = ChunkLanguageEntries.Where(entry => entry.ChunkId == i);
			if (MatchinglanguageEntries.Count() > 0)
			{
				ChunkLine += " languages=\"" + MatchinglanguageEntries.First().CultureId + "\"";
				ChunkLine += " label=\"" + MatchinglanguageEntries.First().Label + "\"/>";
			}
			else
			{
				ChunkLine += " label=\"Chunk #" + i.ToString() + "\"/>";
			}

			ChunkContents.AppendLine(ChunkLine);
		}
		ChunkContents.AppendLine("      </chunks>");
		ChunkContents.AppendLine("      <scenarios default_id=\"0\">");

		// generate one default scenario that loads all the chunks in order.
		String ScenearioLine = "        <scenario id=\"0\" type=\"sp\" initial_chunk_count=\"1\" initial_chunk_count_disc=\"1\" label=\"Scenario #0\">0-";
		ScenearioLine += (NumChunks - 1).ToString();
		ScenearioLine += "</scenario>";

		ChunkContents.AppendLine(ScenearioLine);
		ChunkContents.AppendLine("      </scenarios>");
		ChunkContents.AppendLine("    </chunk_info>");

	}

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1 && Params.Package)
		{
			Log("Archiving with more than one executable. Only {0} will be archived.", SC.StageExecutables[SC.StageExecutables.Count - 1]);
		}

		// if we packaged a build, archive that, instead of the raw staging directory
		if (Params.Package)
		{
			string FullTitleId = null;

			ConfigHierarchy PlatformGameConfig = null;
			if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
			{
				PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "TitleID", out FullTitleId);
			}
			bool bBuildIsoImage = false;
			if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
			{
				PlatformGameConfig.GetBool("/Script/PS4PlatformEditor.PS4TargetSettings", "BuildIsoImage", out bBuildIsoImage);
			}

			if (string.IsNullOrEmpty(FullTitleId))
			{
				Console.ForegroundColor = ConsoleColor.Yellow;
				Console.WriteLine("Couldn't find TitleID.  Using default: IV0000-TEST00000_00-TESTTESTTESTTEST");
				Console.ResetColor();
				FullTitleId = "IV0000-TEST00000_00-TESTTESTTESTTEST";
			}

			if (Params.TitleID.Count == 0)
			{
				Params.TitleID.Add(FullTitleId);
			}

			var TitleConfigurationPairs = Params.TitleID.SelectMany(TitleID => SC.StageTargetConfigurations, (t, c) => new { t, c });

			Parallel.ForEach(TitleConfigurationPairs, (TitleConfigurationPair) =>
			{
				string TitleID = TitleConfigurationPair.t;
				var TargetConfiguration = TitleConfigurationPair.c;

				string ProjectPKG;
				string ProjectGP4;
				MakePKGFileName(SC, TargetConfiguration, Params, bBuildIsoImage, TitleID, out ProjectPKG, out ProjectGP4);

				// if ProjectPKG is a directory, then archive everything as it will have submission materials
				if (Directory.Exists(ProjectPKG))
				{
					SC.ArchiveFiles(ProjectPKG, "*.*", true, null, TitleID);
					SC.ArchiveFiles(Path.GetDirectoryName(ProjectGP4), Path.GetFileName(ProjectGP4), false, null, TitleID);
				}
				else
				{
					SC.ArchiveFiles(Path.GetDirectoryName(ProjectPKG), Path.GetFileName(ProjectPKG), true, null, TitleID);
					SC.ArchiveFiles(Path.GetDirectoryName(ProjectGP4), Path.GetFileName(ProjectGP4), true, null, TitleID);
				}
			});
		}
		else
		{
			// otherwise, just archive the 
			base.GetFilesToArchive(Params, SC);
		}
	}

	private class PerTitlePackageParameters
	{
		public Dictionary<string, string> RemapFilesList = new Dictionary<string, string>();
		public List<string> ExcludedDirList = new List<string>();
		public List<string> RemappedDirList = new List<string>();

		public string TitleID;

		public string FullTitleId;
		public string Passcode;
		public string StorageType;
	}

    public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		// ensure there is a ue4commandline.txt file
		if (!File.Exists(Path.Combine(Params.BaseStageDirectory, "PS4", "ue4commandline.txt")))
		{
			File.WriteAllText(Path.Combine(Params.BaseStageDirectory, "PS4", "ue4commandline.txt"), "");
		}

		if (Params.IsGeneratingPatch && Params.IsGeneratingRemaster)
		{
			throw new AutomationException("IsGeneratingPatch and IsGeneratingRemaster cannot both be true!");
		}

		// Get the parameters for creating the pkg from the defaultEngine.ini
		string DefaultFullTitleId = null;
		string DefaultPasscode = null;
		string DefaultStorageType = null;
		string AppType = null;
		bool bBuildIsoImage = false;
		bool bMoveFilesToOuterEdge = false;

		ConfigHierarchy PlatformGameConfig = null;
		if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "TitleID", out DefaultFullTitleId);
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "TitlePasscode", out DefaultPasscode);
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "StorageType", out DefaultStorageType);
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "AppType", out AppType);
			PlatformGameConfig.GetBool("/Script/PS4PlatformEditor.PS4TargetSettings", "BuildIsoImage", out bBuildIsoImage);
			PlatformGameConfig.GetBool("/Script/PS4PlatformEditor.PS4TargetSettings", "MoveFilesToOuterEdge", out bMoveFilesToOuterEdge);
		}

		if (string.IsNullOrEmpty(DefaultFullTitleId))
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("Couldn't find TitleID.  Using default: IV0000-TEST00000_00-TESTTESTTESTTEST");
			Console.ResetColor();
			DefaultFullTitleId = "IV0000-TEST00000_00-TESTTESTTESTTEST";
		}

		if (Params.TitleID.Count == 0)
		{
			Params.TitleID.Add(DefaultFullTitleId);
		}

		if (string.IsNullOrEmpty(DefaultPasscode))
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("Couldn't find PS4TitlePasscode.  Using default: xiB6LnPxcxIDl2CrCJC7eBnZ1wQXvjNm ");
			Console.ResetColor();
			DefaultPasscode = "xiB6LnPxcxIDl2CrCJC7eBnZ1wQXvjNm";
		}

		if (string.IsNullOrEmpty(DefaultStorageType))
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("Couldn't find StorageType.  Using default: bd25");
			Console.ResetColor();
			DefaultStorageType = "bd25";
		}
		else
		{
			switch (DefaultStorageType)
			{
				case "PPST_BD25":
					DefaultStorageType = "bd25";
					break;
				case "PPST_BD50":
					DefaultStorageType = "bd50";
					break;
				case "PPST_Digital25":
					DefaultStorageType = "digital25";
					break;
				case "PPST_Digital50":
					DefaultStorageType = "digital50";
					break;
				default:
					Console.ForegroundColor = ConsoleColor.Yellow;
					Console.WriteLine("Invalid StorageType.  Using default: bd25");
					Console.ResetColor();
					DefaultStorageType = "bd25";
					break;
			}
		}

		if (string.IsNullOrEmpty(AppType))
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("Couldn't find AppType.  Using default: full");
			Console.ResetColor();
			AppType = "full";
		}
		else
		{
			switch (AppType)
			{
				case "PPAT_Full":
					AppType = "full";
					break;
				case "PPAT_Upgradeable":
					// Spelling upgradeable without middle 'e' is intentional. The PS4 expects 'upgradable' as the spelling for app_type
					// https://ps4.scedev.net/resources/documents/Misc/current/Package_Generator-Users_Guide/0011.html
					AppType = "upgradable";
					break;
				case "PPAT_Demo":
					AppType = "demo";
					break;
				case "PPAT_Freemium":
					AppType = "freemium";
					break;
				default:
					Console.ForegroundColor = ConsoleColor.Yellow;
					Console.WriteLine("Invalid AppType.  Using default: full");
					Console.ResetColor();
					AppType = "full";
					break;
			}
		}

		List<PerTitlePackageParameters> TitleParameters = new List<PerTitlePackageParameters>();
		foreach (string TitleID in Params.TitleID)
		{
			PerTitlePackageParameters TitleParams = new PerTitlePackageParameters();

			TitleParams.TitleID = TitleID;
			TitleParams.FullTitleId = DefaultFullTitleId;
			TitleParams.Passcode = DefaultPasscode;
			TitleParams.StorageType = DefaultStorageType;

			TitleParameters.Add(TitleParams);
		}

		int OrbisPubCmdRunCount = 0;
		int OrbisPubFailCount = 0;

		Parallel.ForEach(TitleParameters, (TitleParams) =>
		{
			// read the title id and passcode from the title.json file in the specified sce_sys/titleid directory
			JsonObject TitleObj = null;
			if (JsonObject.TryRead(new FileReference(Path.Combine(Params.BaseStageDirectory, "PS4", TitleParams.TitleID, "title.json")), out TitleObj))
			{
				TitleParams.FullTitleId = TitleObj.GetStringField("content_id");
				TitleParams.Passcode = TitleObj.GetStringField("title_passcode");
				TitleParams.StorageType = TitleObj.GetStringField("storagetype");
			}

			if (Directory.Exists(Path.Combine(Params.BaseStageDirectory, "PS4", "sce_sys", TitleParams.TitleID)))
			{
				string BaseDir = Path.Combine(Params.BaseStageDirectory, "PS4").Replace("\\", "/");
				string SysDir = "sce_sys";
				string TitleIdDir = TitleParams.TitleID;
				string FullTitleIdDir = BaseDir + "/" + SysDir + "/" + TitleIdDir + "/";
				string[] OverrideFilesList = Directory.GetFiles(FullTitleIdDir, "*", SearchOption.AllDirectories);
				foreach (string RemapFile in OverrideFilesList)
				{
					string FileRelativeToTitleIdDir = RemapFile.Replace("\\", "/").Replace(FullTitleIdDir, "");
					TitleParams.RemapFilesList.Add(SysDir + "/" + TitleIdDir + "/" + FileRelativeToTitleIdDir, SysDir + "/" + FileRelativeToTitleIdDir);
				}
				TitleParams.RemappedDirList.Add("sce_sys/" + TitleParams.TitleID);
			}
			if (Directory.Exists(Path.Combine(Params.BaseStageDirectory, "PS4", TitleParams.TitleID.ToLowerInvariant())))
			{
				string[] OverrideFilesList = Directory.GetFiles(Path.Combine(Params.BaseStageDirectory, "PS4", TitleParams.TitleID));
				foreach (string RemapFile in OverrideFilesList)
				{
					// pair is local-path to remote path, so key should be treated as case-insensitive
					TitleParams.RemapFilesList.Add(Path.Combine(TitleParams.TitleID, Path.GetFileName(RemapFile)).Replace("\\", "/"), Path.Combine(Path.GetFileName(RemapFile)).Replace("\\", "/"));
				}
				TitleParams.RemappedDirList.Add(TitleParams.TitleID);
			}
			string[] ExcludeDirs = Directory.GetDirectories(Path.Combine(Params.BaseStageDirectory, "PS4", "sce_sys"));
			foreach (string Dir in ExcludeDirs)
			{
				if (Path.GetFileName(Dir).ToLowerInvariant() != TitleParams.TitleID.ToLowerInvariant() && ((!Params.GeneratePatch && !Params.GenerateRemaster) || !Path.GetFileName(Dir).Contains("changeinfo")) && !Path.GetFileName(Dir).Contains("trophy") && !Path.GetFileName(Dir).Contains("keymap_rp"))
				{
					TitleParams.ExcludedDirList.Add("sce_sys/" + Path.GetFileName(Dir));
				}
			}
			ExcludeDirs = Directory.GetDirectories(Path.Combine(Params.BaseStageDirectory, "PS4"));
			foreach (string Dir in ExcludeDirs)
			{
				if (Path.GetFileName(Dir).ToLowerInvariant() != TitleParams.TitleID.ToLowerInvariant())
				{
					string[] Files = Directory.GetFiles(Dir);
					foreach (var f in Files)
					{
						if (f.Contains("title.json"))
						{
							TitleParams.ExcludedDirList.Add(Path.GetFileName(Dir));
							break;
						}
					}
				}
			}

			Parallel.For(0, SC.StageTargetConfigurations.Count, TargetConfigurationIdx =>
			{
				UnrealTargetConfiguration TargetConfiguration = SC.StageTargetConfigurations[TargetConfigurationIdx];

				// Introduce a staggered delay for each thread here; orbis-pub-cmd.exe seems to fail with an error about sc.exe being the wrong version 
				// if two instances happen to start at exactly the same time.
				int RunIndex = Interlocked.Increment(ref OrbisPubCmdRunCount) - 1;
				Log("Waiting at {0} to start creating package {1}", DateTime.Now.ToString(), RunIndex);
				Thread.Sleep(RunIndex * 5 * 1000);
				Log("Starting at {0} to create package {1}", DateTime.Now.ToString(), RunIndex);

				StringBuilder Contents = new StringBuilder();

				bool bPlayGoEmulation = false;
				StringBuilder FilesContents = new StringBuilder();
				StringBuilder DirectoriesContents = new StringBuilder();
				int NumChunks = 1;
				string TargetExecutable = SC.StageExecutables[SC.StageTargetConfigurations.IndexOf(TargetConfiguration)] + Platform.GetExeExtension(SC.StageTargetPlatform.PlatformType);
				RecurseStagingDirectory(Path.Combine(Params.BaseStageDirectory, "PS4") + "\\", "", 0, TitleParams, FilesContents, DirectoriesContents, TargetConfiguration, bPlayGoEmulation, true, TargetExecutable, Params.GeneratePatch, Params.GenerateRemaster, out NumChunks);

				string ConfigurationStorageType = TitleParams.StorageType;

				if (Params.GeneratePatch)
				{
					//patches only support 25GB right now.
					ConfigurationStorageType = "digital25";
				}

				//may want to force dual layer to prepare for later disc masters with more data without needing another SKU.
				bool bForceDualLayer = String.Compare(ConfigurationStorageType, "bd50", true) == 0;
				StringBuilder ChunkContents = new StringBuilder();
				String[] ChunkLayers;
				BuildChunkContents(ChunkContents, NumChunks, bPlayGoEmulation, Params, SC, bForceDualLayer, out ChunkLayers);

				bool bDualLayer = false;
				foreach (String Layer in ChunkLayers)
				{
					if (Layer != "0")
					{
						bDualLayer = true;
					}
				}
				if (String.Compare(ConfigurationStorageType, "bd25", true) == 0)
				{
					if (bDualLayer)
					{
						ConfigurationStorageType = "bd50";
						Console.ForegroundColor = ConsoleColor.Red;
						Console.WriteLine("Storage type is BD25, can only have chunks in layer0 or pub tools will fail. Modify Project Settings > Platforms > PS4 > Packaging > StorageType to be BD50 if you really want multiple layers.  Changing to BD50 for this build.");
						Console.ResetColor();
					}
				}
				else if (String.Compare(ConfigurationStorageType, "bd50", true) == 0)
				{
					if (!bDualLayer)
					{
						ConfigurationStorageType = "bd25";
						Console.ForegroundColor = ConsoleColor.Red;
						Console.WriteLine("Storage type is BD50, and must have chunks in layer1 or pub tools will fail. Add chunks to layer1 or modify Project Settings > Platforms > PS4 > Packaging > StorageType to be BD25.  Changing to BD25 for this build.");
						Console.ResetColor();
					}
				}

				// patch must have a type of "digital" (apps and remasters should be bd, even though they may be distributed as digital-only!)
				if (Params.IsGeneratingPatch)
				{
					ConfigurationStorageType = ConfigurationStorageType.Replace("bd", "digital");
				}

				string PkgPath;
				string GP4Path;
				MakePKGFileName(SC, TargetConfiguration, Params, bBuildIsoImage, TitleParams.TitleID, out PkgPath, out GP4Path);

				String VolumeType = "pkg_ps4_app";
				String AppPath = "";

				// Patches and Remasters should both state the original release they were based on, and the latest patch, to allow
				// fast patching
				if (Params.IsGeneratingPatch || Params.IsGeneratingRemaster)
				{
					string ReleaseVersionPath = CombinePaths(Params.GetBasedOnReleaseVersionPath(SC, Params.Client), TitleParams.TitleID);

					// Search all directories, as the .pkg may be in a "Submission-*" folder
					foreach (String PkgFile in Directory.GetFiles(ReleaseVersionPath, "*.pkg", SearchOption.AllDirectories))
					{
						ReleaseVersionPath = PkgFile;
						break;
					}

					string PatchVersionPath = CombinePaths(Params.GetBasedOnReleaseVersionPath(SC, Params.Client), "LatestPatch", TitleParams.TitleID);
					string LatestPatch = "";

					try
					{
						// Search all directories, as the .pkg may be in a "Submission-*" folder
						foreach (String PkgFile in Directory.GetFiles(PatchVersionPath, "*.pkg", SearchOption.AllDirectories))
						{
							LatestPatch = PkgFile;
							break;
						}
					}
					catch { }

					//todo, take a Day 1 patch parameter and skip the patch_type if it's set.

					if (LatestPatch.Length == 0)
					{
						Log("No LatestPatch found at {0}, patch will be generated against original master", PatchVersionPath);
						AppPath = String.Format("app_path=\"{0}\" patch_type=\"ref_a\"", ReleaseVersionPath);
					}
					else
					{
						Log("Generating patch against {0}", LatestPatch);
						AppPath = String.Format("app_path=\"{0}\" latest_pkg_path=\"{1}\"", ReleaseVersionPath, LatestPatch);
					}

					VolumeType = Params.IsGeneratingPatch ? "pkg_ps4_patch" : "pkg_ps4_remaster";
				}

				Console.WriteLine("Generating project of type {0}", VolumeType);

				Contents.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>");
				Contents.AppendLine("<psproject fmt=\"gp4\" version=\"1000\">");
				Contents.AppendLine("  <volume>");
				Contents.AppendLine(String.Format("    <volume_type>{0}</volume_type>", VolumeType));
				Contents.AppendLine("    <volume_id>PS4VOLUME</volume_id>");
				Contents.AppendLine("    <volume_ts>2017-06-24 20:55:30</volume_ts>");
				Contents.AppendLine(String.Format("    <package content_id=\"{0}\" passcode=\"{1}\" storage_type=\"{2}\" app_type=\"{3}\" {4}/>", TitleParams.FullTitleId, TitleParams.Passcode, ConfigurationStorageType, AppType, AppPath));
				Contents.Append(ChunkContents.ToString());

				if(!string.IsNullOrEmpty(Params.DiscVersion))
				{
					Contents.AppendLine("    <disc_info>");
					Contents.AppendLine(String.Format("      <param key=\"disc_version\">{0}</param>", Params.DiscVersion));
					Contents.AppendLine("    </disc_info>");
				}

				Contents.AppendLine("  </volume>");
				Contents.AppendLine("  <files img_no=\"0\">");

				Contents.Append(FilesContents.ToString());

				Contents.AppendLine("  </files>");
				Contents.AppendLine("  <rootdir>");
				Contents.Append(DirectoriesContents.ToString());
				Contents.AppendLine("  </rootdir>");
				Contents.AppendLine("</psproject>");

				// first delete any existing file
				if (File.Exists(PkgPath))
				{
					File.Delete(PkgPath);
				}

				// write out the .gp4 file			
				string GP4Dir = Path.GetDirectoryName(GP4Path);
				try
				{
					Directory.CreateDirectory(GP4Dir);
				}
				catch (Exception CreateDirException)
				{
					Console.ForegroundColor = ConsoleColor.Yellow;
					Console.WriteLine("Couldn't create directory for gp4: " + GP4Dir);
					Console.WriteLine(CreateDirException.Message);
					Console.ResetColor();
				}

				File.WriteAllText(GP4Path, Contents.ToString());

				// get the path to the util
				string UtilPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS\\Tools\\Publishing Tools\\bin\\orbis-pub-cmd.exe");

				bool bIsDistribution = DoPackageForSubmission(Params, TargetConfiguration);

				string UtilParams = "img_create ";
				if (bBuildIsoImage || bIsDistribution)
				{
					UtilParams += "--oformat pkg" + (bBuildIsoImage ? "+iso" : "") + (bIsDistribution ? "+subitem " : " ");
				}

				if (bBuildIsoImage && bMoveFilesToOuterEdge)
				{
					UtilParams += "--move_outer ";
				}

				if (Params.GeneratePatch)
				{
					// delete redundant files for patches
					UtilParams += "--delete_redundant_file ";
				}

				// quote the paths in case there's a space in them. orbis-pub-cmd requires them.
				UtilParams += "\"" + GP4Path + "\" " + "\"" + PkgPath + "\"";

				string LogName = "orbis-pub-cmd_" + Path.GetFileNameWithoutExtension(PkgPath);

				int ExitCode;

				// now create the package
				string StdOut = RunAndLog(CmdEnv, UtilPath, UtilParams, out ExitCode, LogName);

				// if failed, show all warnings/errors
				if (ExitCode != 0)
				{
					var Matches = Regex.Matches(StdOut, @".*(?:\[Error\]|\[Warn\])(.+)");

					foreach (Match M in Matches)
					{
						var Group = M.Groups[1].ToString();
						if (M.Value.Contains("[Error]"))
						{
							Console.WriteLine("Error: {0}", Group);
						}
						else
						{
							Console.WriteLine("Warning: {0}", Group);
						}
					}

					OrbisPubFailCount++;
				}
			});
		});

		if (OrbisPubFailCount > 0)
		{
			throw new AutomationException("One or more publishing steps failed");
		}
	}

	public override void ExtractPackage(ProjectParams Params, string PkgPath, string OutputPath)
	{
		// Get the parameters for creating the pkg from the defaultEngine.ini
		string Passcode = null;

		ConfigHierarchy PlatformGameConfig = null;
		if (Params.EngineConfigs.TryGetValue(PlatformType, out PlatformGameConfig))
		{
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "TitlePasscode", out Passcode);
		}

		if (string.IsNullOrEmpty(Passcode))
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("Couldn't find PS4TitlePasscode.  Using default: xiB6LnPxcxIDl2CrCJC7eBnZ1wQXvjNm ");
			Console.ResetColor();
			Passcode = "xiB6LnPxcxIDl2CrCJC7eBnZ1wQXvjNm";
		}

		if (!File.Exists(PkgPath))
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("Couldn't find PkgPath " + PkgPath);
			Console.ResetColor();
			return;
		}


		// get the path to the util
		string UtilPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS\\Tools\\Publishing Tools\\bin\\orbis-pub-cmd.exe");

		string UtilParams = " img_extract  --passcode " + Passcode + " \"" + PkgPath + "\" \"" + OutputPath + "\"";

		RunAndLog(CmdEnv, UtilPath, UtilParams);
	}

	public void KillCurrentRunningProcess(String Device)
	{
		if (!String.IsNullOrWhiteSpace(Device))
		{
			IProcessResult SnapShotResult = ExecutePS4DevKitUtilCommand("snapshot", Device);
			String[] DevKitUtilOutput = SnapShotResult.Output.Split(new char[] { '\n', '\r' }, StringSplitOptions.RemoveEmptyEntries);
			String RunningProcess = CommandUtils.ParseParamValue(DevKitUtilOutput, "Name=", null);

			if (!String.IsNullOrWhiteSpace(RunningProcess))
			{
				Console.WriteLine("Killing Process {0} on device {1} in prep for staging.", RunningProcess, Device);
				String KillCommandLine = "pkill " + RunningProcess;
				if (String.IsNullOrEmpty(Device) == false)
				{
					KillCommandLine += " \"" + Device + "\"";
				}
				ExecuteOrbisCtrlCommand(KillCommandLine);
			}
		}
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// rather than stage each individual file, just stage the entire build directory.
		// skip the param.sfos because we need special patch handling for those.
		DirectoryReference EngineMetadataPath = DirectoryReference.Combine(SC.EngineRoot, "Build/PS4/sce_sys");
		if(DirectoryReference.Exists(EngineMetadataPath))
		{
			SC.StageFiles(StagedFileType.SystemNonUFS, EngineMetadataPath, StageFilesSearch.AllDirectories, new StagedDirectoryReference("sce_sys"));
		}

		DirectoryReference ProjectMetadataPath = DirectoryReference.Combine(SC.ProjectRoot, "Build/PS4/sce_sys");
		if (DirectoryReference.Exists(ProjectMetadataPath))
		{
			SC.StageFiles(StagedFileType.SystemNonUFS, ProjectMetadataPath, StageFilesSearch.AllDirectories, new StagedDirectoryReference("sce_sys"));
		}

		DirectoryReference ProjectNoRedistMetadataPath = DirectoryReference.Combine(SC.ProjectRoot, "Build/PS4/NoRedist/sce_sys");
		if(DirectoryReference.Exists(ProjectNoRedistMetadataPath))
		{
			SC.StageFiles(StagedFileType.SystemNonUFS, ProjectNoRedistMetadataPath, StageFilesSearch.AllDirectories, new StagedDirectoryReference("sce_sys"));
		}

		// Stage the invite icon
		FileReference InviteIcon = FileReference.Combine(SC.EngineRoot, "Build", "PS4", "InviteIcon.jpg");
		if(FileReference.Exists(InviteIcon))
		{
			SC.StageFile(StagedFileType.NonUFS, InviteIcon);
		}

		// Stage the title data
		DirectoryReference EngineTitleDataDir = DirectoryReference.Combine(SC.EngineRoot, "Build", "PS4", "titledata");
		if(DirectoryReference.Exists(EngineTitleDataDir))
		{
			SC.StageFiles(StagedFileType.SystemNonUFS, EngineTitleDataDir, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
		}

		DirectoryReference ProjectTitleDataDir = DirectoryReference.Combine(SC.ProjectRoot, "Build", "PS4", "titledata");
		if(DirectoryReference.Exists(ProjectTitleDataDir))
		{
			SC.StageFiles(StagedFileType.SystemNonUFS, ProjectTitleDataDir, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
		}

		DirectoryReference ProjectNoRedistTitleDataDir = DirectoryReference.Combine(SC.ProjectRoot, "Build", "PS4", "NoRedist", "titledata");
		if(DirectoryReference.Exists(ProjectNoRedistTitleDataDir))
		{
			SC.StageFiles(StagedFileType.SystemNonUFS, ProjectNoRedistTitleDataDir, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
		}

		// grab the modules that need to go into a package from the SDK directory
		DirectoryReference ModulesDir = new DirectoryReference(Environment.ExpandEnvironmentVariables("%SCE_ORBIS_SDK_DIR%\\target\\sce_module"));
		foreach(FileReference ModuleFile in DirectoryReference.EnumerateFiles(ModulesDir, "*.prx"))
		{
			if(!ModuleFile.FullName.EndsWith("_debug.prx", StringComparison.InvariantCultureIgnoreCase))
			{
				SC.StageFile(StagedFileType.SystemNonUFS, ModuleFile, StagedFileReference.Combine("sce_module", ModuleFile.GetFileName()));
			}
		}
		
		if (SC.StageExecutables.Count != SC.StageTargetConfigurations.Count)
		{
			throw new AutomationException("Exe count doesn't match config count. {0}, {1}", SC.StageExecutables.Count, SC.StageTargetConfigurations.Count);
		}

		// stage all built executables
		for (Int32 i = 0; i < SC.StageExecutables.Count; ++i)
		{
			UnrealTargetConfiguration TargetConfiguration = SC.StageTargetConfigurations[i];
			String Exe = SC.StageExecutables[i];

			// stage the symbol info data if not a shipping build
			if (TargetConfiguration != UnrealTargetConfiguration.Shipping)
			{
				List<string> SymbolFileNames = new List<string>(); 
				SymbolFileNames.Add(TargetConfiguration.ToString() + "-Symbols.bin");
				SymbolFileNames.Add(TargetConfiguration.ToString() + "-SymbolNames.bin");
				SymbolFileNames.Add(TargetConfiguration.ToString() + "-SymbolMetaData.txt");

				foreach(string SymbolFileName in SymbolFileNames)
				{
					FileReference SymbolFile = FileReference.Combine(SC.ProjectRoot, "Build", "PS4", "Symbols", SymbolFileName);
					if(FileReference.Exists(SymbolFile))
					{
						SC.StageFile(StagedFileType.SystemNonUFS, SymbolFile, StagedFileReference.Combine("Symbols", SymbolFile.GetFileName()));
					}
				}
			}

			SC.StageFile(StagedFileType.SystemNonUFS, FileReference.Combine(SC.ProjectBinariesFolder, Exe + Platform.GetExeExtension(SC.StageTargetPlatform.PlatformType)));
		}

		// we need to kill running processes before staging so staging doesn't fail from the running process having some files open (like the .self).
		foreach (var DeviceName in Params.DeviceNames)
		{
			KillCurrentRunningProcess(DeviceName);
		}
	}

	public override void PostStagingFileCopy(ProjectParams Params, DeploymentContext SC)
	{
		// deep file path shortening handling.
		// we write out a metadata file containing the unhashed directory in each deep location for sanity checking, 
		// and to be able to reconstruct original paths during diretory iteration at runtime.
		if (!Params.CookOnTheFly)
		{
			foreach (KeyValuePair<String, String> Pair in ReverseShortenedDictionarypaths)
			{
				String NormalizedOriginalDirectory = Pair.Value;
				String DeepPath = Pair.Key;

				String MetaDataPath = Path.Combine(SC.StageDirectory.FullName, DeepPath, DeepFileMetaDataName);
				File.WriteAllText(MetaDataPath, NormalizedOriginalDirectory, Encoding.Unicode);

				// write out metadata for each parent directory to ensure that 'DirectoryExists' calls at runtime will succeed even if
				// the parent directories have no files of their own.  Otherwise the path shortening would just remove these directories completely.
				String OriginalSubDir = Path.GetDirectoryName(NormalizedOriginalDirectory);
				while (!String.IsNullOrEmpty(OriginalSubDir))
				{
					OriginalSubDir = StripRelativeAndNormalize(OriginalSubDir);

					bool bShortened;
					String SubDirShortened = ShortenPath(OriginalSubDir, out bShortened);

					//write out subdir metadata no matter what to guarantee that the directories exist in the staged filesystem.  this is necessary so that directory iteration works properly.
					String SubDirMetaDataDir = Path.Combine(SC.StageDirectory.FullName, SubDirShortened);
					String SubDirMetaDataPath = Path.Combine(SubDirMetaDataDir, DeepFileMetaDataName);
					Directory.CreateDirectory(SubDirMetaDataDir);
					File.WriteAllText(SubDirMetaDataPath, OriginalSubDir, Encoding.Unicode);

					OriginalSubDir = Path.GetDirectoryName(OriginalSubDir);
				}
			}
		}

		//PlayGo handling
		{
			String FilePath = Path.Combine(SC.StageDirectory.FullName, PlayGoEmulationFileName);
			if (SC.PlatformUsesChunkManifests && !Params.Distribution)
			{
				StringBuilder FilesContents = new StringBuilder();
				StringBuilder DirectoriesContents = new StringBuilder();

				PerTitlePackageParameters PackageParameters = new PerTitlePackageParameters();

				bool bPlayGoEmulation = true;
				int NumChunks = 1;
				string TargetExecutable = SC.StageExecutables[0] + Platform.GetExeExtension(SC.StageTargetPlatform.PlatformType);
				RecurseStagingDirectory(Path.Combine(Params.BaseStageDirectory, "PS4") + "\\", "", 0, PackageParameters, FilesContents, DirectoriesContents, SC.StageTargetConfigurations[0], bPlayGoEmulation, false, TargetExecutable, Params.GeneratePatch, Params.GenerateRemaster, out NumChunks);

				StringBuilder ChunkContents = new StringBuilder();
				String[] ChunkLayers;
				BuildChunkContents(ChunkContents, NumChunks, bPlayGoEmulation, Params, SC, false, out ChunkLayers);

				// Main thing we're doing here is generating the playgo-chunks.xml file which enables PlayGo emulation over HostFS.
				// We're putting it in the root of the staging directory so that if the staging dir is used as the root directory, the emulation will automatically
				// work.  See the PlayGo-Overview_e.pdf in the SDK for more info.
				StringBuilder Contents = new StringBuilder();
				Contents.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>\r\n");
				Contents.AppendLine("<psproject fmt=\"playgo-chunks\" version=\"1000\">\r\n");

				Contents.AppendLine("  <volume>");
				Contents.Append(ChunkContents.ToString());
				Contents.AppendLine("  </volume>");

				Contents.AppendLine("  <files>");
				Contents.Append(FilesContents.ToString());
				Contents.AppendLine("  </files>");

				Contents.AppendLine("</psproject>");

				//This file currently causes application start to throw an undocumented error.  Will bring it back when Sony responds to the support ticket (20872)
				File.WriteAllText(FilePath, Contents.ToString());
			}
			else
			{
				// an out of date chunk file is worse than none.
				if (File.Exists(FilePath))
				{
					File.Delete(FilePath);
				}
			}
		}
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		if (bDedicatedServer)
		{
			throw new AutomationException("The PS4 cannot host dedicated servers.");
		}
		return "PS4";
	}

	public override bool DeployLowerCaseFilenames()
	{
		// we can't change the case of the sce_module files, which are NonUFS
		return true;
	}

	public override bool IsSupported { get { return true; } }

	public override bool DeployViaUFE { get { return false; } }
	public override bool LaunchViaUFE { get { return false; } }
	public override bool UseAbsLog { get { return false; } }

	public override List<string> GetDebugFileExtensions()
	{
		return new List<string> { ".dsym" };
	}

	public override PakType RequiresPak(ProjectParams Params)
	{
		if (Params.Distribution)
		{
			// Always create a pak file when building for distribution for improved load times
			// and patch tool compatibility
			return PakType.Always;
		}
		else
		{
			return PakType.DontCare;
		}
	}

	public override string GetPlatformPakCommandLine()
	{
		return " -blocksize=256MB -patchpaddingalign=65536";
	}

	public override bool GetPlatformPatchesWithDiffPak(ProjectParams Params, DeploymentContext SC)
	{
		ConfigHierarchy PlatformGameConfig = null;
		if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			bool GenerateDiffPakPatch = false;
			PlatformGameConfig.GetBool("/Script/PS4PlatformEditor.PS4TargetSettings", "GenerateDiffPakPatch", out GenerateDiffPakPatch);
			return GenerateDiffPakPatch;
		}
		return false;
	}

	public IProcessResult ExecutePS4DevKitUtilCommand(String CommandLine, String Device)
	{
		String DevKitUtilPath = Path.Combine(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/PS4/PS4DevKitUtil.exe");
		if (!String.IsNullOrEmpty(Device))
		{
			CommandLine = CommandLine + " \"" + Device + "\"";
		}
		return Run(DevKitUtilPath, CommandLine);
	}

	public IProcessResult ExecuteOrbisCtrlCommand(String CommandLine)
	{
		String OrbisCtrlPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS\\Tools\\Target Manager Server\\bin\\orbis-ctrl.exe");
		return Run(OrbisCtrlPath, CommandLine);
	}

	// This is the base directory on the device to copy files for this project.  We want it to be sandboxed so that different
	// games don't stomp each other's files and we can maintain proper file deployment data per game.
	// e.g. O:\10.1.107.14\data\shootergame
	public String GetBaseDeployDirectory(ProjectParams Params, DeploymentContext SC, string DeviceName)
	{
		IProcessResult Result = ExecutePS4DevKitUtilCommand("Detail", DeviceName);
		if (Result.ExitCode != 0)
		{
			throw new AutomationException("PS4DevKitUtil to get IP for deployment exited with error: " + Result.ExitCode.ToString());
		}

		String DevKitUtilOutput = Result.Output;
		String[] GetIPResults = DevKitUtilOutput.Split(new char[] { '\n', '\r' }, StringSplitOptions.RemoveEmptyEntries);

		String HostNameIP = CommandUtils.ParseParamValue(GetIPResults, "HostName=", null);
		String MappedDrive = CommandUtils.ParseParamValue(GetIPResults, "MappedDrive=", null);
		String ParsedDeviceName = CommandUtils.ParseParamValue(GetIPResults, "Name=", DeviceName).Trim('"');
		Console.WriteLine("PS4 Mapped Path: {0}:{1}", MappedDrive, HostNameIP);

		if (String.IsNullOrEmpty(HostNameIP))
		{
			throw new AutomationException("PS4DevKitUtil could not get HostName for deploying to: " + DeviceName);
		}
		if (String.IsNullOrEmpty(MappedDrive))
		{
			throw new AutomationException("PS4DevKitUtil could not get mapped drive for deploying to: " + DeviceName + " Please check that the IP address of the devkit is correct");
		}

		String BaseTargetPath = Path.Combine(MappedDrive + ":\\", HostNameIP);

		// check this path 
		if (Directory.Exists(BaseTargetPath) == false)
		{
			if (String.IsNullOrEmpty(ParsedDeviceName))
			{
				throw new AutomationException("PS4DevKitUtil: Path not found at {0} and DeviceName was not specified (-device=Name)", BaseTargetPath);
			}

			// Some setups now seem to mount kits as a name and not an IP, so check that too...
			String AltBaseTargetPath = Path.Combine(MappedDrive + ":\\", ParsedDeviceName);

			if (Directory.Exists(AltBaseTargetPath))
			{
				BaseTargetPath = AltBaseTargetPath;
			}
			else
			{
				throw new AutomationException("Could not find deployment path (Neither {0} or {1} exist)", BaseTargetPath, AltBaseTargetPath);
			}
		}

		Console.WriteLine("PS4 Mapped Path is " + BaseTargetPath);

		BaseTargetPath = Path.Combine(BaseTargetPath, "data", Params.DeployFolder.ToLower());

		return BaseTargetPath;
	}

	private String GetDeployedFilePath(String StagedFilePath, String BaseDeploymentPath, String StageDirectory)
	{
		String RelativePath = StagedFilePath.Substring(StageDirectory.Length);
		if (RelativePath[0] == '\\')
		{
			RelativePath = RelativePath.Substring(1);
		}

		return Path.Combine(BaseDeploymentPath, RelativePath);
	}

	private bool ReadManifestFile(String ManifestFile, out String[] FileList)
	{
		FileList = null;
		if (File.Exists(ManifestFile))
		{
			string FilesRaw = File.ReadAllText(ManifestFile);
			FileList = FilesRaw.Split(new char[] { '\n', '\r' }, StringSplitOptions.RemoveEmptyEntries);
			return true;
		}
		return false;
	}

	public override void Deploy(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
			Log("Deploying with more than one executable. Only {0} will be deployed.", SC.StageExecutables[SC.StageExecutables.Count - 1]);
		}

		// if we skipped staging for some reason, kill running process before starting deploy.
		if (!Params.Stage || Params.SkipStage)
		{
			KillCurrentRunningProcess(Params.DeviceNames[0]);
		}

		var TargetConfiguration = SC.StageTargetConfigurations[0];

		String BaseTargetPath = GetBaseDeployDirectory(Params, SC, Params.DeviceNames[0]);
		List<String> AllFilesToDeploy = new List<string>();

		Log("Deployment path is " + BaseTargetPath);

		bool bCleanedAll = false;

		// delete any old files which are no longer referenced
		{
			Console.WriteLine("Deleting Obsolete Files:");
			String NonUFSObsoleteFilePath = SC.GetNonUFSDeploymentObsoletePath(Params.DeviceNames[0]);
			string[] NonUFSFiles = null;
			if (ReadManifestFile(NonUFSObsoleteFilePath, out NonUFSFiles))
			{
				foreach (String SrcFile in NonUFSFiles)
				{
					String DestFilePath = Path.Combine(BaseTargetPath, SrcFile);
					Console.WriteLine(DestFilePath);
					try
					{
						File.Delete(DestFilePath);
					}
					catch (Exception Ex)
					{
						Console.WriteLine("Obsolete NonUFS Delete " + DestFilePath + " failed with exception:");
						Console.WriteLine(Ex.Message);
					}
				}
			}

			String UFSObsoleteFilePath = SC.GetUFSDeploymentObsoletePath(Params.DeviceNames[0]);
			string[] UFSFiles = null;
			if (ReadManifestFile(UFSObsoleteFilePath, out UFSFiles))
			{
				foreach (String SrcFile in UFSFiles)
				{
					String DestFilePath = Path.Combine(BaseTargetPath, SrcFile);
					Console.WriteLine(DestFilePath);

					try
					{
						File.Delete(DestFilePath);
					}
					catch (Exception Ex)
					{
						Console.WriteLine("Obsolete UFS Delete " + DestFilePath + " failed with exception:");
						Console.WriteLine(Ex.Message);
					}
				}
			}

			//if we're deploying paks, wipe out any old loose files that might be around.  ObsoleteFiles list not reliable in this case.
			if (Params.Pak)
			{
				Log("Deleting loose data because of PAK file deployment.");

				try
				{
					String DeepFilesPath = Path.Combine(BaseTargetPath, "deepfiles");
					InternalUtils.SafeDeleteDirectory(DeepFilesPath);

					String EnginePath = Path.Combine(BaseTargetPath, "engine");
					InternalUtils.SafeDeleteDirectory(EnginePath);

					String GameContentPath = Path.Combine(BaseTargetPath, Params.ShortProjectName.ToLower(), "content");
					InternalUtils.SafeDeleteDirectory(GameContentPath);
				}
				catch (Exception Ex)
				{
					Console.WriteLine("Deleting loose files at " + BaseTargetPath + " failed with exception:");
					Console.WriteLine(Ex.Message);
				}
			}
			else
			{
				Log("Deleting pak files because of non-PAK file deployment.");
				//if we aren't deploying paks, make sure we wipe them.  paks aren't part of the normal file manifests.
				String GamePaksPath = Path.Combine(BaseTargetPath, Params.ShortProjectName.ToLower(), "content", "paks");
				InternalUtils.SafeDeleteDirectory(GamePaksPath);
			}

			//iterative pak deploy not supported.  wipe everything.
			if (Params.Clean.HasValue && Params.Clean.Value || (Params.Pak && Params.IterativeDeploy))
			{
				Console.WriteLine("Cleaning entire deployment sandbox:");
				Console.WriteLine(BaseTargetPath);
				InternalUtils.SafeDeleteDirectory(BaseTargetPath);
				bCleanedAll = true;
			}
		}

		// if iterative deploy, determine the file delta
		if (Params.IterativeDeploy && !bCleanedAll)
		{
			String NonUFSDeltaFilePath = SC.GetNonUFSDeploymentDeltaPath(Params.DeviceNames[0]);
			string[] NonUFSFiles = null;

			// check to determine if we need to update the deployed files on O:
			if (ReadManifestFile(NonUFSDeltaFilePath, out NonUFSFiles))
			{
				foreach (String SrcFile in NonUFSFiles)
				{
					AllFilesToDeploy.Add(Path.Combine(SC.StageDirectory.FullName, SrcFile.ToLowerInvariant()));
				}
			}

			String UFSDeltaFilePath = SC.GetUFSDeploymentDeltaPath(Params.DeviceNames[0]);
			string[] UFSFiles = null;
			if (ReadManifestFile(UFSDeltaFilePath, out UFSFiles))
			{
				foreach (String SrcFile in UFSFiles)
				{
					AllFilesToDeploy.Add(Path.Combine(SC.StageDirectory.FullName, SrcFile.ToLowerInvariant()));
				}
			}

			//path shortening metadata files aren't part of the normal staging manifests, so we need to copy them over specifically.
			String DeepFilesDir = Path.Combine(SC.StageDirectory.FullName, "deepfiles");
			if (Directory.Exists(DeepFilesDir))
			{
				string[] DeepFilesMetaData = Directory.GetFiles(DeepFilesDir, DeepFileMetaDataName, SearchOption.AllDirectories);
				foreach (String MetaFile in DeepFilesMetaData)
				{
					AllFilesToDeploy.Add(MetaFile.ToLowerInvariant());
				}
			}
		}
		else
		{
			AllFilesToDeploy.AddRange(Directory.GetFiles(SC.StageDirectory.FullName, "*.*", SearchOption.AllDirectories));
		}


		Console.WriteLine("Files To deploy:");
		foreach (String File in AllFilesToDeploy)
		{
			Console.WriteLine(File);
		}

		try
		{
			Directory.CreateDirectory(BaseTargetPath);
		}
		catch (Exception Ex)
		{
			Console.WriteLine("Failed to create destination directory " + BaseTargetPath);
			Console.WriteLine(Ex.Message);
			throw new Exception("PS4 Deployment Failed");
		}

		using (var Copy = new ScopedTimer("Time to Deploy " + AllFilesToDeploy.Count.ToString() + " Files:", LogEventType.Console))
		{
			foreach (String SourceFile in AllFilesToDeploy)
			{
				String DestFilePath = GetDeployedFilePath(SourceFile, BaseTargetPath, SC.StageDirectory.FullName);
				Directory.CreateDirectory(Path.GetDirectoryName(DestFilePath));

				try
				{
					//copy with overwrite seems to fail often on mapped PS4 drives, leaving bogus 0 byte files around and failing the copy operation
					File.Delete(DestFilePath);
				}
				catch (Exception Ex)
				{
					Console.WriteLine("Delete " + DestFilePath + " failed with exception:");
					Console.WriteLine(Ex.Message);
					throw (Ex);
				}

				try
				{
					Console.WriteLine(DestFilePath);
					File.Copy(SourceFile, DestFilePath);
				}
				catch (Exception Ex)
				{
					Console.WriteLine("Copy " + SourceFile + " to " + DestFilePath + " failed with exception:");
					Console.WriteLine(Ex.Message);
					throw (Ex);
				}
			}
		}
	}

	public override bool RetrieveDeployedManifests(ProjectParams Params, DeploymentContext SC, string DeviceName, out List<string> UFSManifests, out List<string> NonUFSManifests)
	{
		bool Result = true;
		UFSManifests = new List<string>();
		NonUFSManifests = new List<string>();

		try
		{
			// we don't actually need to copy anything for PS4, we should be able to read straight off the mapped drive.
			String BaseTargetPath = GetBaseDeployDirectory(Params, SC, DeviceName);
			String TargetUFSDeployedPath = Path.Combine(BaseTargetPath, SC.UFSDeployedManifestFileName);
			String TargetNonUFSDeployedPath = Path.Combine(BaseTargetPath, SC.NonUFSDeployedManifestFileName);

			if (File.Exists(TargetUFSDeployedPath))
			{
				UFSManifests.Add(TargetUFSDeployedPath);
			}

			if (File.Exists(TargetNonUFSDeployedPath))
			{
				NonUFSManifests.Add(TargetNonUFSDeployedPath);
			}
		}
		catch (System.Exception e)
		{
			Console.WriteLine("Failed retrieiving deployed manifests!");
			Console.WriteLine(e.Message);
			Result = false;
		}

		return Result;
	}

	public void ExecOrbisCommand(String Command, String Device)
	{
		String OrbisCtrlPath = Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS\\Tools\\Target Manager Server\\bin\\orbis-ctrl.exe");
		Command = Command + " \"" + Device + "\"";

		IProcessResult Result = Run(OrbisCtrlPath, Command);
		if (Result.ExitCode != 0)
		{
			Console.WriteLine(OrbisCtrlPath + " " + Command + " failed with code " + Result.ExitCode.ToString());
		}
	}

	void ConnectPS4(String Device)
	{
		ExecOrbisCommand("Connect", Device);
	}

	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		ConnectPS4(Params.DeviceNames[0]);
		String WorkingDir = Path.Combine(Params.BaseStageDirectory, "PS4");
		String DevKitUtilCommandLine = "launch target=\"" + Params.DeviceNames[0] + "\" workingdirectory=\"" + WorkingDir + "\" elf=\"" + ClientApp + "\" args=" + ClientCmdLine;
		IProcessResult Result = ExecutePS4DevKitUtilCommand(DevKitUtilCommandLine, "");
		return Result;
	}



	static String ExtractedPkgFile = null;
	/// <summary>
	/// Get a release pak file path, if we are currently building a patch then get the previous release pak file path, if we are creating a new release this will be the output path
	/// </summary>
	/// <param name="SC"></param>
	/// <param name="Params"></param>
	/// <param name="PakName"></param>
	/// <returns></returns>
	public override string GetReleasePakFilePath(DeploymentContext SC, ProjectParams Params, string PakName)
	{
		if (Params.IsGeneratingPatch)
		{
			// if we are generating a patch we need to extract the pak file from the pkg
			string PkgPath = Params.GetBasedOnReleaseVersionPath(SC, Params.Client);
			if (Params.TitleID.Count == 1)
			{
				PkgPath = CombinePaths(PkgPath, Params.TitleID[0]);
			}

			foreach (String PkgFile in Directory.EnumerateFiles(PkgPath, "*.pkg"))
			{
				PkgPath = CombinePaths(PkgPath, Path.GetFileName(PkgFile));
				break;
			}

			if (!File.Exists(PkgPath))
			{
				return base.GetReleasePakFilePath(SC, Params, PakName);
			}

			string ExtractedPkgPath = CombinePaths(Path.GetDirectoryName(PkgPath), "ExtractedPkg");
			string PakPath = CombinePaths(Path.GetDirectoryName(PkgPath), "ExtractedPak");

			if (ExtractedPkgFile != PkgPath) // we already extracted this guy
			{

				ExtractedPkgFile = PkgPath;

				Directory.CreateDirectory(PakPath);
				Directory.CreateDirectory(ExtractedPkgPath);

				ExtractPackage(Params, PkgPath, ExtractedPkgPath);

				foreach (String PakFile in Directory.EnumerateFiles(ExtractedPkgPath, "*.pak", SearchOption.AllDirectories))
				{
					string DestPath = CombinePaths(PakPath, Path.GetFileName(PakFile));
					Log("Moving file from {0} to {1}", PakFile, DestPath);
					File.Move(PakFile, DestPath);
				}
			}

			return Path.GetFullPath(CombinePaths(PakPath, PakName));
		}
		else
		{
			return base.GetReleasePakFilePath(SC, Params, PakName);
		}
	}

	public override bool SupportsMultiDeviceDeploy
	{
		get { return true; }
	}

	public override bool PublishSymbols(DirectoryReference SymbolStoreDirectory, List<FileReference> Files, string Product)
	{
		string OrbisSDKRoot = Environment.ExpandEnvironmentVariables("%SCE_ORBIS_SDK_DIR%");
		if (string.IsNullOrWhiteSpace(OrbisSDKRoot))
		{
			CommandUtils.LogError("SCE_ORBIS_SDK_DIR environment variable is not set. Cannot upload PS4 symbols.");
			return false;
		}

		string UtilPath = Path.Combine(OrbisSDKRoot, "host_tools\\bin\\orbis-symupload.exe");
		if (!File.Exists(UtilPath))
		{
			CommandUtils.LogError("Couldn't find orbis-symupload at this location: \"{0}\". Cannot upload PS4 symbols.", UtilPath);
			return false;
		}

		bool bSuccess = true;
		foreach (var SymbolFile in Files.Where(x => x.HasExtension(".self")))
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = UtilPath;
			StartInfo.Arguments = string.Format("add /f \"{0}\" /s \"{1}\" /compress", SymbolFile.FullName, SymbolStoreDirectory.FullName);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			if (Utils.RunLocalProcessAndLogOutput(StartInfo) != 0)
			{
				bSuccess = false;
			}
		}

		return bSuccess;
	}

	public override string[] SymbolServerDirectoryStructure
	{
		get
		{
			return new string[]
			{
								"PS4",            // Root Directory
								"{0}*.self",      // Binary File Directory (e.g. QAGameClient-PS4-Test.self)
								"*",              // Hash Directory        (e.g. A92F5744D99F416EB0CCFD58CCE719CD1)
			};
		}
	}
}
