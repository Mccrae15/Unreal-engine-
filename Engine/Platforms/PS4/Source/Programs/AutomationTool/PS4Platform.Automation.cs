// Copyright Epic Games, Inc. All Rights Reserved.
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
using System.Security.Cryptography;
using Tools.DotNETCommon;
using AutomationUtils.Automation;

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
	//Versioning file for iterative patch packaging
	private static readonly String AppVerFile = "AppVersion.txt";

	private static bool bEbootCreated = false;

	//Language references for packaging documentation
	private enum LanguageNumber
	{
		jp,
		enUS,
		fr,
		esES,
		de,
		it,
		nl,
		ptPT,
		ru,
		ko,
		zhHant,
		zhHans,
		fi,
		sv,
		da,
		no,
		pl,
		ptBR,
		enGB,
		tr,
		esLA,
		ar,
		frCA,
		cs,
		hu,
		el,
		ro,
		th,
		vi,
		id,
		Total
	}

	//The following attribute list can be found here - https://ps4.siedev.net/resources/documents/Misc/current/Param_File_Editor-Users_Guide/0004.html

	[Flags]
	enum Attribute1
	{
		None = 0,
		UserMgmtSupp = 1,
		CrossButtonEnter = 2,
		Stereo3D = 8,
		AssignmentEnterButton = 32,
		ShareMenuCustomised = 64,
		PSVRSupported = 16384,
		CPU6Mode = 32768,
		CPU7Mode = 65536,
		NEOMode = 8388608, //0x800000
		PSVRRequired = 67125248, //0x4004000
		HDRSupported = 536870912 //0x20000000
	}

	[Flags]
	enum Attribute2
	{
		None = 0,
		VideoLibraryOn = 2,
		ContentSearchOn = 4,
		EyeDistanceDefault = 16,
		DynamicEyeDistance = 32,
		SeparateBroadcast = 256,
		VRMoveDummyLoadOff = 512,
		MB2PageSetting = 32768,
		MB2ReserveWithMapping = 65536,
		MB2ReserveAll = 98304,
		UseThreadScheduler = 1048576,

		//When SDK version is 3.50 or earlier (next 3 enums)
		Tournament1on1 = 2048,
		//When SDK is 4.00 to 4.50 (next 2 enums)
		TournamentTeam = 4096,
		//When SDK is 5.00 to 5.50 (6.00+ does not require these)
		TournamentFree4All = 262144,
	}

	private static readonly List<string> SymbolFileNameSuffixes = new List<string>()
	{
		"-Symbols.bin",
		"-SymbolNames.bin",
		"-SymbolMetaData.txt"
	};

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
		if (Dest.Name.EndsWith(".prx"))
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
			else if (Params.GenerateRemaster)
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
	private static int GetChunkIndexForFile(String LocalDirectory, String Filename, Dictionary<FileReference, int> CustomFileChunkMapping)
	{
		FileReference localFilePath = new FileReference(CombinePaths(LocalDirectory, Filename));
		if (CustomFileChunkMapping != null && CustomFileChunkMapping.ContainsKey(localFilePath))
		{
			return CustomFileChunkMapping[localFilePath];
		}

		int ChunkIndex = 0;
		String ChunkSubString = "pakchunk";
		int PakChunkStringIndex = Filename.IndexOf(ChunkSubString);

		if (PakChunkStringIndex != -1)
		{
			// pakchunk filename format should look like 'pakchunk0-ps4.pak, pakchunk1-ps4.pak, etc'
			/*String[] FileNameParts = FileName.Split('-');
			String PakChunkPart = FileNameParts[0];
			String ChunkNumString = PakChunkPart.Substring(PakChunkStringIndex + ChunkSubString.Length, PakChunkPart.Length - PakChunkStringIndex - ChunkSubString.Length);*/


			int LastNumberIndex = PakChunkStringIndex + ChunkSubString.Length;
			for (; LastNumberIndex < Filename.Length; ++LastNumberIndex)
			{
				if (!Char.IsDigit(Filename[LastNumberIndex]))
					break;
			}
			String ChunkNumString = Filename.Substring(PakChunkStringIndex + ChunkSubString.Length, LastNumberIndex - PakChunkStringIndex - ChunkSubString.Length);

			if (!Int32.TryParse(ChunkNumString, out ChunkIndex))
			{
				if (!Filename.Contains("early"))
				{
					LogWarning("Couldn't parse filename: " + Filename + " section " + ChunkNumString + " for chunk index");
				}

				ChunkIndex = 0;
			}
		}

		return ChunkIndex;
	}

	// assumes that only the filename without a path is passed in.
	private static int GetChunkIndexForFileFromInstallBundles(String Filename, List<PS4BundleSettings> InstallBundles)
	{
		// Try to find a matching chunk regex
		foreach (var Bundle in InstallBundles)
		{
			foreach (string RegexString in Bundle.FileRegex)
			{
				if (Regex.Match(Filename, RegexString).Success)
				{
					LogInformation("Mapping {0} to Chunk {1} for Bundle {2}", Filename, Bundle.ChunkID, Bundle.Name);
					return Bundle.ChunkID;
				}
			}
		}

		LogInformation("Mapping {0} to Chunk 0 for unkown bundle", Filename);
		return 0;
	}

	private static bool IsUnsupportedLanguageChunk(List<ChunkLanguageEntry> ChunkLanguageEntries, string File, int FileChunk)
	{
		string Extension = Path.GetExtension(File);
		if (Extension != ".pak" && Extension != ".ucas" && Extension != ".utoc")
		{
			return false;
		}

		bool IsLanguageChunk = ChunkLanguageEntries.Any(languageEntry => languageEntry.ChunkId == FileChunk);
		if (!IsLanguageChunk)
		{
			return false;
		}

		bool IsSupportedLanguageChunk = ChunkLanguageEntries.Any(languageEntry => (languageEntry.ChunkId == FileChunk && languageEntry.Label != "EMPTY"));
		return !IsSupportedLanguageChunk;
	}

	private static void GetFilesAndDirectoriesContents(
		string LocalDirectory,
		List<ChunkLanguageEntry> ChunkLanguageEntries, List<PS4BundleSettings> InstallBundles, Dictionary<FileReference, int> CustomFileChunkMapping,
		int[] ChunkLayers,
		PerTitlePackageParameters TitleParams,
		UnrealTargetConfiguration TargetConfiguration,
		bool bPlayGoEmulation, bool bCompressPakFiles, string TargetExecutable, bool bGeneratingPatch, bool bGeneratingRemaster,
		out List<StagedFileEntry>[] FilesContentsByLayer, out List<string> DirNameList, bool bIsPackagingStep = false)
	{
		DirNameList = new List<string>();

		int NumLayers = ChunkLayers.Max() + 1;
		FilesContentsByLayer = new List<StagedFileEntry>[NumLayers];
		for (int LayerIndex = 0; LayerIndex < NumLayers; LayerIndex++)
		{
			FilesContentsByLayer[LayerIndex] = new List<StagedFileEntry>();
		}

		LogInformation("Assigning files to {0} Layers", NumLayers);

		GetFilesAndDirectoriesContents_RecurseStagingDirectory(
			LocalDirectory, "",
			ChunkLanguageEntries, InstallBundles, CustomFileChunkMapping,
			ChunkLayers,
			TitleParams,
			TargetConfiguration,
			bPlayGoEmulation, bCompressPakFiles, TargetExecutable, bGeneratingPatch, bGeneratingRemaster,
			FilesContentsByLayer, DirNameList, bIsPackagingStep);
	}

	private static void GetFilesAndDirectoriesContents_RecurseStagingDirectory(
		string LocalDirectory, string PS4Directory, 
		List<ChunkLanguageEntry> ChunkLanguageEntries, List<PS4BundleSettings> InstallBundles, Dictionary<FileReference, int> CustomFileChunkMapping,
		int[] ChunkLayers,
		PerTitlePackageParameters TitleParams, 
		UnrealTargetConfiguration TargetConfiguration, 
		bool bPlayGoEmulation, bool bCompressPakFiles, string TargetExecutable, bool bGeneratingPatch, bool bGeneratingRemaster,
		List<StagedFileEntry>[] FilesContentsByLayer, List<string> DirNameList, bool bIsPackagingStep = false)
	{
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
				if (String.Compare(DirNameInPackage + File, Remap.Key, StringComparison.InvariantCultureIgnoreCase) == 0)
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
				bEbootCreated = true;
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
			else if (bGeneratingRemaster)
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

			if(String.Compare(File, AppVerFile, StringComparison.InvariantCultureIgnoreCase) == 0)
			{
				//Exclude the app version tracker from the packaged product
				bExcluded = true;
			}

			if(String.Compare(File, "title_passcode_backup.txt", StringComparison.InvariantCultureIgnoreCase) == 0)
			{
				//Exclude the app version tracker from the packaged product
				bExcluded = true;
			}

			// Exclude symbol files not belonging to current configuration
			if(!File.StartsWith(TargetConfiguration.ToString(), StringComparison.OrdinalIgnoreCase)
				&& SymbolFileNameSuffixes.Any(suffix => File.EndsWith(suffix, StringComparison.OrdinalIgnoreCase)))
			{
				bExcluded = true;
			}

			//If we are chunking, then the following steps will break when this function is executed in PostStagingFileCopy
			//These steps are only needed in the final packaging steps. TitleParams is not initialised in PostStagingFileCopy
			if (bIsPackagingStep)
			{
				if(FileNameInPackage.Equals("param.sfo"))
				{
					//Include the title specific param.sfo and ensure the default is excluded, otherwise the default is included
					if(!FullFile.Contains(TitleParams.TitleID) && TitleParams.IgnoreDefaultSFO)
					{
						bExcluded = true;					
					}
					//Ensure only 1 param.sfo file is ever included if one hasn't been provided in the title id specific folder
					TitleParams.IgnoreDefaultSFO = true;
				}

			if (FileNameInPackage.Equals("nptitle.dat"))
				{
					//Include the title specific nptitle.dat and ensure the default is excluded, otherwise the default is included
				if (LocalDirectory.Contains(TitleParams.TitleID))
					{
						string TitleIDFolder = TitleParams.TitleID + "/";
					if (DirNameInPackage.Contains(TitleParams.TitleID))
						{
							int TitleIDIndex = DirNameInPackage.IndexOf(TitleIDFolder);
							DirNameInPackage = DirNameInPackage.Remove(TitleIDIndex, TitleIDFolder.Length);
						}
					}
				else if (TitleParams.IgnoreDefaultDAT)
					{
						bExcluded = true;
					}
					//Ensure only 1 nptitle.dat file is ever included if one hasn't been provided in the title id specific folder
					TitleParams.IgnoreDefaultDAT = true;
				}

			if (String.Compare(File, "PS4Engine.ini", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					//Remove the title passcode from the packaged PS4Engine.ini before copy
					ConfigFile PS4EngineConfig = new ConfigFile(new FileReference(LocalDirectory + "PS4Engine.ini"));
					ConfigFileSection PS4TargetSettings;
				if (PS4EngineConfig.TryGetSection("/Script/PS4PlatformEditor.PS4TargetSettings", out PS4TargetSettings))
					{
						ConfigLine TitlePasscodeLine;
					if (PS4TargetSettings.TryGetLine("TitlePasscode", out TitlePasscodeLine))
						{
							TitlePasscodeLine.Value = "REMOVED_DURING_PACKAGING";
							PS4EngineConfig.Write(new FileReference(LocalDirectory + "PS4Engine.ini"));
						}
					}				
				}

			if (String.Compare(File, "title.json", StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					//Rewrite the edited title.json file before copy to remove passcode etc.
					JsonObject TitleObj = null;
					string FilePath = LocalDirectory + "title.json";
					if (JsonObject.TryRead(new FileReference(FilePath), out TitleObj))
					{
						string Passcode = "";
						if(TitleObj.TryGetStringField("title_passcode", out Passcode))
						{
							System.IO.File.SetAttributes(FilePath, System.IO.File.GetAttributes(FilePath) & ~FileAttributes.ReadOnly);

							using (JsonWriter TitleWriter = new JsonWriter(new FileReference(FilePath)))
							{
								TitleWriter.WriteObjectStart();
								foreach (string TitleKey in TitleObj.KeyNames)
								{
								if (TitleKey.Equals("title_passcode"))
									{
										//Remove the title passcode from the packaged title.json file
										TitleWriter.WriteValue("title_passcode", "REMOVED_DURING_PACKAGING");
									}
									else if(TitleKey.Equals("title_id"))
									{
										//NP Title ID requires the 12 digit vertion of the title_id for online to work correctly, whilst param.sfo prefers the 9 digit.
										//This entry is only used in the packaged project for the online subsystem, it is not required otherwise.

										int TitleIDPosition = TitleParams.FullTitleId.IndexOf('-');
										int TitleIDLength = TitleParams.FullTitleId.LastIndexOf('-') - TitleIDPosition - 1;
										TitleIDPosition += 1;

										TitleWriter.WriteValue("title_id", TitleParams.FullTitleId.Substring(TitleIDPosition, TitleIDLength).ToUpperInvariant());
									}
									else
									{
										string FieldValue = null;
										if (TitleObj.TryGetStringField(TitleKey, out FieldValue))
										{
											TitleWriter.WriteValue(TitleKey, FieldValue);
										}
									}
								}

								TitleWriter.WriteObjectEnd();
							}
						}
					}
				}
			}

			if (bExcluded)
			{
				continue;
			}

			int FileChunk;
			if (InstallBundles != null)
			{
				FileChunk = GetChunkIndexForFileFromInstallBundles(File, InstallBundles);
			}
			else
			{
				FileChunk = GetChunkIndexForFile(LocalDirectory, File, CustomFileChunkMapping);
			}

			// Exclude unsupported language pakchunk files
			if (!IsUnsupportedLanguageChunk(ChunkLanguageEntries, File, FileChunk))
			{
				string Extension = Path.GetExtension(File);
				StagedFileEntry CurrentStagedFileEntry = new StagedFileEntry(
					DirNameInPackage + FileNameInPackage,
					bPlayGoEmulation ? String.Empty : (LocalDirectory + File),
					FileChunk,
					bCompressPakFiles && (Extension == ".pak" || Extension == ".ucas"));
				CurrentStagedFileEntry.bPlayGoEmulation = bPlayGoEmulation;

				int Layer = ChunkLayers[FileChunk];

				LogInformation("Mapping {0} to Layer {1} for Chunk {2}", File, Layer, FileChunk);

				FilesContentsByLayer[Layer].Add(CurrentStagedFileEntry);
			}

			DirNameList.Add(DirNameInPackage);
		}

		string[] Dirs = Directory.GetDirectories(LocalDirectory);
		foreach (string FullDir in Dirs)
		{
			string Dir = Path.GetFileName(FullDir);
			bool bExcluded = false;
			foreach (string ExcludeDir in TitleParams.ExcludedDirList)
			{
				if (ExcludeDir == PS4Directory + Dir)
				{
					bExcluded = true;
					break;
				}
			}

			if (bExcluded)
			{
				continue;
			}

			GetFilesAndDirectoriesContents_RecurseStagingDirectory(
				LocalDirectory + Dir + "\\", PS4Directory + Dir + "/",
				ChunkLanguageEntries, InstallBundles, CustomFileChunkMapping,
				ChunkLayers,
				TitleParams,
				TargetConfiguration,
				bPlayGoEmulation, bCompressPakFiles, TargetExecutable, bGeneratingPatch, bGeneratingRemaster,
				FilesContentsByLayer, DirNameList, bIsPackagingStep);
		}
	}

	private static void WriteChangeInfoXML(string[] PatchNotes, string ChangeInfoLocation, string XMLFileName)
	{
		if (DirectoryExists(ChangeInfoLocation) && XMLFileName.EndsWith(".xml"))
		{
			StringBuilder Change = new StringBuilder();
			Change.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
			Change.AppendLine("<changeinfo>");

			bool EmptyFile = true;
			foreach (string line in PatchNotes)
			{
				if (String.IsNullOrEmpty(line))
				{
					continue;
				}

				if (line.StartsWith("#"))
				{
					if (!EmptyFile)
					{
						Change.Append("]]>");
						Change.AppendLine("</changes>");
					}
					Change.AppendLine("<changes app_ver=\"" + line.Remove(0, 1) + "\">");
					Change.AppendLine("<![CDATA[");
					EmptyFile = false;
				}
				else
				{
					if (EmptyFile)
					{
						break;
					}
					else
					{
						Change.Append(line + "\r\n");
					}
				}
			}

			if (!EmptyFile)
			{
				Change.Append("]]>\r\n");
				Change.AppendLine("</changes>");
				Change.AppendLine("</changeinfo>\r\n");
				File.WriteAllText(Path.Combine(ChangeInfoLocation, XMLFileName), Change.ToString());
			}
		}
	}

	private static void GenerateChangeInfoXMLFiles(string PatchNotesLocation, List<ChunkLanguageEntry> ChunkLanguageEntries, string StagedLocation)
	{
		string SceSysDirectory = Path.Combine(StagedLocation, "sce_sys");

		if (File.Exists(PatchNotesLocation) && DirectoryExists(SceSysDirectory))
		{
			string ChangeInfoLocation = Path.Combine(SceSysDirectory, "changeinfo");
			if (!DirectoryExists(ChangeInfoLocation))
			{
				CreateDirectory(ChangeInfoLocation);
			}

			WriteChangeInfoXML(File.ReadAllLines(PatchNotesLocation), ChangeInfoLocation, "changeinfo.xml");

			foreach (ChunkLanguageEntry entry in ChunkLanguageEntries)
			{
				string LanguageEnum = entry.CultureId;
				if (!String.IsNullOrEmpty(LanguageEnum) && File.Exists(entry.LanguagePatchNotes))
				{
					LanguageNumber LangNum;
					if (LanguageEnum.Contains('-'))
					{
						LanguageEnum = LanguageEnum.Remove(LanguageEnum.IndexOf('-'), 1);
					}

					bool EnumParsed = Enum.TryParse(LanguageEnum, out LangNum);
					if (EnumParsed)
					{
						int FileNumber = (int)LangNum;
						WriteChangeInfoXML(File.ReadAllLines(entry.LanguagePatchNotes), ChangeInfoLocation, "changelist_" + FileNumber.ToString("00") + ".xml");
					}
				}
			}
		}
		else
		{
			LogWarning("Couldn't parse patch notes!");
		}
	}

	private class ChunkLanguageEntry
	{
		public string CultureId;
		public string Label;
		public int ChunkId;
		public bool IsDefault;
		public bool IsInitial;
		public string LanguageTitle = "";
		public string LanguagePatchNotes = "";
	}

	private static List<ChunkLanguageEntry> GetChunkLanguageEntries(ProjectParams Params, DeploymentContext SC)
	{
		List<ChunkLanguageEntry> ChunkLanguageEntries = new List<ChunkLanguageEntry>();
		ConfigHierarchy PlatformGameConfig = null;
		if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			List<string> ChunkLanguageList;
			PlatformGameConfig.GetArray("/Script/PS4PlatformEditor.PS4TargetSettings", "ChunkLanguageMapping", out ChunkLanguageList);

			if (ChunkLanguageList != null && ChunkLanguageList.Count() != 0)
			{
				// Remove parentheses
				ChunkLanguageList = ChunkLanguageList.Select(chunkLanguage => chunkLanguage.Trim("()".ToCharArray())).ToList();

				ChunkLanguageEntries = ChunkLanguageList.Select(language =>
				{
					ChunkLanguageEntry entry = new ChunkLanguageEntry();

					string[] languageProperties = language.Split(", ".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
					entry.CultureId = CommandUtils.ParseParamValue(languageProperties, "CultureId=").Trim("\"".ToCharArray());
					entry.Label = CommandUtils.ParseParamValue(languageProperties, "Label=").Trim("\"".ToCharArray());

					//Retrieve the per language packaging data if present
					entry.LanguageTitle = CommandUtils.ParseParamValue(languageProperties, "LanguageTitle=");
					entry.LanguagePatchNotes = CommandUtils.ParseParamValue(languageProperties, "LanguageNotesLocation=");

					if (!int.TryParse(CommandUtils.ParseParamValue(languageProperties, "ChunkId="), out entry.ChunkId))
					{
						LogWarning("Couldn't parse ChunkId from ChunkLanguageMapping in section /Script/PS4PlatformEditor.PS4TargetSettings");
						entry.ChunkId = -1;
					}

					if (!bool.TryParse(CommandUtils.ParseParamValue(languageProperties, "IsDefault="), out entry.IsDefault))
					{
						LogWarning("Couldn't parse IsDefault from ChunkLanguageMapping in section /Script/PS4PlatformEditor.PS4TargetSettings");
						entry.IsDefault = false;
					}

					if (!bool.TryParse(CommandUtils.ParseParamValue(languageProperties, "IsInitial="), out entry.IsInitial))
					{
						LogWarning("Couldn't parse IsInitial from ChunkLanguageMapping in section /Script/PS4PlatformEditor.PS4TargetSettings");
						entry.IsInitial = false;
					}

					return entry;
				}).ToList();
			}
		}

		return ChunkLanguageEntries;
	}

	/// <summary>
	/// Load custom chunk mapping from ini files.
	/// Format is like below.
	/// +CustomChunkMapping=(Pattern="shootergame/content/movies/*",ChunkId=1)
	/// +CustomChunkMapping=(Pattern="shootergame/content/movies/movie1.mp4",ChunkId=1)
	/// </summary>
	/// <param name="Params"></param>
	/// <param name="SC"></param>
	/// <returns></returns>
	private static Dictionary<FileReference, int> GetCustomChunkMapping(ProjectParams Params, DeploymentContext SC, string LocalDirectory)
	{
		Dictionary<string, int> CustomChunkMapping = new Dictionary<string, int>();
		ConfigHierarchy PlatformGameConfig = null;
		if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			List<string> ChunkMappingList;
			PlatformGameConfig.GetArray("/Script/PS4PlatformEditor.PS4TargetSettings", "CustomChunkMapping", out ChunkMappingList);

			if (ChunkMappingList != null && ChunkMappingList.Count() != 0)
			{
				// Remove parentheses
				ChunkMappingList = ChunkMappingList.Select(chunkMapping => chunkMapping.Trim("()".ToCharArray())).ToList();

				foreach (var chunkMapping in ChunkMappingList)
				{
					string chunkMappingPath = null;
					int chunkMappingChunkId = -1;

					string[] chunkMappingProperties = chunkMapping.Split(", ".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
					chunkMappingPath = CommandUtils.ParseParamValue(chunkMappingProperties, "Pattern").Trim('\"');
					if (!int.TryParse(CommandUtils.ParseParamValue(chunkMappingProperties, "ChunkId"), out chunkMappingChunkId))
					{
						LogWarning("Couldn't parse ChunkId from ChunkLanguageMapping in section /Script/PS4PlatformEditor.PS4TargetSettings");
						chunkMappingChunkId = -1;
					}

					if (!String.IsNullOrEmpty(chunkMappingPath) && chunkMappingChunkId != -1 && !CustomChunkMapping.ContainsKey(chunkMappingPath))
					{
						CustomChunkMapping.Add(chunkMappingPath, chunkMappingChunkId);
					}
					else
					{
						LogWarning("Found invalid or duplicate chunk mapping");
					}
				}
			}
		}

		Dictionary<FileReference, int> FileChunkMapping = new Dictionary<FileReference, int>();
		foreach (var chunkMapping in CustomChunkMapping)
		{
			string FullPathPattern = CombinePaths(LocalDirectory, chunkMapping.Key);

			if (FileFilter.FindWildcardIndex(FullPathPattern) != -1)
			{
				foreach (var file in FileFilter.ResolveWildcard(FullPathPattern))
				{
					if(!FileChunkMapping.ContainsKey(file))
					{
						FileChunkMapping.Add(file, chunkMapping.Value);
					}
				}
			}
			else if (FileExists(FullPathPattern))
			{
				FileReference PakFileReference = new FileReference(FullPathPattern);
				if (!FileChunkMapping.ContainsKey(PakFileReference))
				{
					FileChunkMapping.Add(PakFileReference, chunkMapping.Value);
				}
			}
		}

		return FileChunkMapping;
	}

	private static Dictionary<int, int> GetChunkLayerAssignments(ProjectParams Params, DeploymentContext SC)
	{
		Dictionary<int, int> ChunkLayerAssignments = new Dictionary<int, int>();

		ConfigHierarchy PlatformGameConfig = null;
		if (Params.GameConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			List<string> ChunkAssignmentList;
			PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "ChunkLayerAssignment", out ChunkAssignmentList);

			if (ChunkAssignmentList != null && ChunkAssignmentList.Count() != 0)
			{
				// Remove parentheses
				ChunkAssignmentList = ChunkAssignmentList.Select(ChunkAssignment => ChunkAssignment.Trim("()".ToCharArray())).ToList();
				foreach (string ChunkAssignment in ChunkAssignmentList)
				{
					string[] ChunkAssignmentProperties = ChunkAssignment.Split(", ".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
					int ChunkId = -1;
					int Layer = -1;
					if (int.TryParse(CommandUtils.ParseParamValue(ChunkAssignmentProperties, "ChunkId="), out ChunkId) &&
						int.TryParse(CommandUtils.ParseParamValue(ChunkAssignmentProperties, "Layer="), out Layer))
					{
						ChunkLayerAssignments.Add(ChunkId, Layer);
					}
				}
			}
		}

		return ChunkLayerAssignments;
	}

	private static int[] GetChunkLayers(List<ChunkLanguageEntry> ChunkLanguageEntries, out int NumLanguageChunks, out int NumChunks, bool bGeneratePatch, ProjectParams Params, DeploymentContext SC, bool bForceDualLayer)
	{
		NumLanguageChunks = 0;
		NumChunks = 1;

		// Update NumChunks with actual number of chunks defined in ini file and extend length of ChunkLayerList.
		if (ChunkLanguageEntries.Count != 0)
		{
			NumLanguageChunks = ChunkLanguageEntries.Max(chunkLanguage => chunkLanguage.ChunkId) + 1;
		}

		NumChunks = Math.Max(NumChunks, NumLanguageChunks);

		String ChunkLayerFilename = Path.Combine(SC.ProjectRoot.FullName, "Build", SC.CookPlatform, "ChunkLayerInfo", "pakchunklayers.txt");
		int[] ChunkLayerList;

		if (File.Exists(ChunkLayerFilename))
		{
			ChunkLayerList = ReadAllLines(ChunkLayerFilename).Select(
				layerString =>
				{
					int layerInt;
					if (int.TryParse(layerString, out layerInt))
					{
						return layerInt;
					}
					else
					{
						return 0;
					}
				}).ToArray();
		}
		// we couldn't find a pakchunk list.  error, or we didn't build with -manifests.
		else
		{
			// Generate chunk layer list based on ChunkLayerAssignment defined in [/Script/UnrealEd.ProjectPackagingSettings]
			LogInformation("pakchunklayers.txt couldn't be found.  Generating chunk layer list based on ChunkLayerAssignment in PS4Game.ini");

			Dictionary<int, int> ChunkLayerAssignments = GetChunkLayerAssignments(Params, SC);
			ChunkLayerList = new int[NumChunks];
			for (int ChunkId = 0; ChunkId < ChunkLayerList.Length; ChunkId++)
			{
				if (ChunkLayerAssignments.ContainsKey(ChunkId))
				{
					ChunkLayerList[ChunkId] = ChunkLayerAssignments[ChunkId];
				}
				else
				{
					ChunkLayerList[ChunkId] = 0;
				}
			}
		}

		NumChunks = Math.Max(NumChunks, ChunkLayerList.Length);

		if (ChunkLayerList.Length < NumChunks)
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("ChunkLayer info missing or mismatched in file: " + ChunkLayerFilename);
			Console.WriteLine("ChunksFound: " + ChunkLayerList.Length.ToString() + " Chunks Required: " + NumChunks);
			foreach (int Layer in ChunkLayerList)
			{
				Console.WriteLine(Layer.ToString());
			}
			Console.ResetColor();

			int[] NewChunkLayerList = new int[NumChunks];
			Array.Copy(ChunkLayerList, NewChunkLayerList, ChunkLayerList.Length);
			for (int i = ChunkLayerList.Length; i < NumChunks; ++i)
			{
				NewChunkLayerList[i] = 0;
			}
			ChunkLayerList = NewChunkLayerList;
		}

		bool bAllLayer0 = true;
		for (int i = 0; i < NumChunks; ++i)
		{
			if (ChunkLayerList[i] != 0)
			{
				bAllLayer0 = false;
				break;
			}
		}

		if (bAllLayer0 && bForceDualLayer)
		{
			Console.WriteLine("Forcing final chunk to layer 1 to allow bd50: ");
			ChunkLayerList[ChunkLayerList.Length - 1] = 1;
		}

		// For patches, ensure all chunks are assigned to layer 0, or sony publishing tools will fail
		if (bGeneratePatch)
		{
			for (int i = 0; i < NumChunks; ++i)
			{
				ChunkLayerList[i] = 0;
			}
		}

		return ChunkLayerList;
	}

	private static void BuildChunkContents(StringBuilder ChunkContents, List<ChunkLanguageEntry> ChunkLanguageEntries, int NumChunks, bool bPlaygoEmulation, ProjectParams Params, DeploymentContext SC, bool bForceDualLayer, ref int[] ChunkLayerList)
	{
		ChunkContents.AppendLine("    <chunk_info chunk_count=\"" + NumChunks.ToString() + "\" scenario_count=\"1\">");

		StringBuilder supportedLanguageList = new StringBuilder();
		string defaultLanguage = String.Empty;
		foreach (var languageEntry in ChunkLanguageEntries)
		{
			if (!String.IsNullOrEmpty(languageEntry.CultureId) && !String.Equals(languageEntry.Label, "EMPTY", StringComparison.OrdinalIgnoreCase))
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
			ChunkContents.AppendLine("      <chunks supported_languages=\"" + supportedLanguageList.ToString().Trim() + "\" default_language=\"" + defaultLanguage + "\">");
		}

		String InitialChunksList = String.Empty;
		String OtherChunksList = String.Empty;
		int InitialChunkCount = 0;

		for (int i = 0; i < NumChunks; ++i)
		{
			var MatchinglanguageEntries = ChunkLanguageEntries.Where(entry => entry.ChunkId == i);

			// Chunk 0 is always an initial chunk
			bool bIsInitial = (i == 0);

			// Only add used chunks
			if (MatchinglanguageEntries.Count() == 0 || bIsInitial || !String.Equals(MatchinglanguageEntries.First().Label, "EMPTY", StringComparison.OrdinalIgnoreCase))
			{
				String ChunkLine = "        <chunk id=\"" + i.ToString() + "\" ";
				// playgo-chunks.xml format is very picky.  We must remove anything that will cause it to fail reading the xml.
				if (!bPlaygoEmulation)
				{
					ChunkLine += "layer_no=\"" + ChunkLayerList[i] + "\"";
				}

				if (MatchinglanguageEntries.Count() > 0)
				{
					if (!String.IsNullOrEmpty(MatchinglanguageEntries.First().CultureId))
					{
						ChunkLine += " languages=\"" + MatchinglanguageEntries.First().CultureId + "\"";
					}

					ChunkLine += " label=\"" + MatchinglanguageEntries.First().Label + "\"/>";

					if (MatchinglanguageEntries.First().IsInitial)
					{
						bIsInitial = true;
					}
				}
				else
				{
					ChunkLine += " label=\"Chunk #" + i.ToString() + "\"/>";
				}

				if (bIsInitial)
				{
					InitialChunkCount++;
					InitialChunksList += i.ToString() + " ";
				}
				else
				{
					OtherChunksList += i.ToString() + " ";
				}

				ChunkContents.AppendLine(ChunkLine);
			}
			else
			{
				// Scenario list wants empty chunks as well
				OtherChunksList += i.ToString() + " ";
			}
		}
		ChunkContents.AppendLine("      </chunks>");
		ChunkContents.AppendLine("      <scenarios default_id=\"0\">");

		// generate one default scenario that loads all the chunks in order.
		String Scenario = "        <scenario id=\"0\" type=\"sp\" initial_chunk_count=\"";
		Scenario += InitialChunkCount.ToString();
		Scenario += "\" initial_chunk_count_disc=\"";
		Scenario += InitialChunkCount.ToString();
		Scenario += "\" label=\"Scenario #0\">";
		Scenario += InitialChunksList;
		Scenario += OtherChunksList.Trim();
		Scenario += "</scenario>";

		ChunkContents.AppendLine(Scenario);
		ChunkContents.AppendLine("      </scenarios>");
		ChunkContents.AppendLine("    </chunk_info>");
	}

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1 && Params.Package)
		{
			LogInformation("Archiving with more than one executable. Only {0} will be archived.", SC.StageExecutables[SC.StageExecutables.Count - 1]);
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

			// User input, so force to uppercase
			FullTitleId = FullTitleId.ToUpperInvariant();

			if (Params.TitleID.Count == 0)
			{
				Params.TitleID.Add(FullTitleId);
			}

			var TitleConfigurationPairs = Params.TitleID.SelectMany(TitleID => SC.StageTargetConfigurations, (t, c) => new { t, c });

			Parallel.ForEach(TitleConfigurationPairs, (TitleConfigurationPair) =>
			{
				// User input, so force to uppercase
				string TitleID = TitleConfigurationPair.t.ToUpperInvariant();
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

	private static void VerifyLaunchChunkSize(string ImgVerifyLog)
	{
		if (String.IsNullOrEmpty(ImgVerifyLog))
		{
			return;
		}

		const int LaunchChunkSizeAlertThreshold = 11766;  // 11.5 GiB
		const int LaunchChunkSizeLimit = 12288;  // 12 GiB
		string LaunchChunkSizePrefix = "Initial Payload = ";
		string LaunchChunkSizeSuffix = "MiB,";
		int LaunchChunkSizeStartIndex = -1;
		int LaunchChunkSizeEndIndex = -1;
		int LaunchChunkSize;
		bool bFoundMatch = false;

		LaunchChunkSizeStartIndex = ImgVerifyLog.IndexOf(LaunchChunkSizePrefix) + LaunchChunkSizePrefix.Length;
		if (LaunchChunkSizeStartIndex != -1)
		{
			LaunchChunkSizeEndIndex = ImgVerifyLog.IndexOf(LaunchChunkSizeSuffix, LaunchChunkSizeStartIndex);
			if (LaunchChunkSizeEndIndex != -1)
			{
				bFoundMatch = true;
			}
		}

		if (bFoundMatch && int.TryParse(ImgVerifyLog.Substring(LaunchChunkSizeStartIndex, LaunchChunkSizeEndIndex - LaunchChunkSizeStartIndex), out LaunchChunkSize))
		{
			float CurrentLaunchChunkSizeInGiB = (float)LaunchChunkSize / 1024;

			// Maximum launch chunk size allowed is 12 GiB
			if (LaunchChunkSize > LaunchChunkSizeLimit)
			{
				LogError("[EPIC] Launch chunk size has reached {0}GiB, which is larger than the limit(12GiB) allowed by Sony.", CurrentLaunchChunkSizeInGiB);
			}
			else if (LaunchChunkSize > LaunchChunkSizeAlertThreshold)
			{
				// Maximum launch chunk size allowed is 12 GiB
				LogWarning("[EPIC] Launch chunk size has reached {0}GiB, while limit is 12GiB.  Please keep an eye on launch chunk size growth from now on.", CurrentLaunchChunkSizeInGiB);
			}
		}
	}

	private class PerTitlePackageParameters
	{
		public Dictionary<string, string> RemapFilesList = new Dictionary<string, string>();
		public List<string> ExcludedDirList = new List<string>();
		public List<string> RemappedDirList = new List<string>();

		public string TitleID;
		public string ShortTitleID
		{
			get
			{
				return TitleID.Substring(0, 9);
			}
		}
		public string Title;
		public bool bGenerateSFO;
		public bool bCopyLatestPatch;
		public string FullTitleId;
		public string Passcode;
		public string StorageType;
		public int ParentalLevel;
		public double Version = -1.0;
		public double AppVersion = -1.0;
		public int[] DownloadDataSize = { 0, -1 };
		public int[] OverwriteAttribute = { -1, -1 };
		public Attribute1 Attribute = Attribute1.None;
		public Attribute2 Attribute2 = Attribute2.None;
		public int Self2MBPageAmount = -1;
		public int RemotePlayKey = 0;
		public int[] UserDefParam = { 0, 0, 0, 0 };
		public string InstallDirSavedata = "";
		public string SaveDataTransferTIDList = "";
		public int PTParam = -1;
		public string PatchInfoLoc = "";
		public string PubToolInfo = "";
		public bool IgnoreDefaultSFO = false;
		public bool IgnoreDefaultDAT = false;
	}

	private class StagedFileEntry
	{
		public string TargetPath;
		public string OriginalPath;
		public int ChunkId;
		public bool UsePfsCompression;
		public bool bPlayGoEmulation;

		public StagedFileEntry(string InTargetPath, string InOriginalPath, int InChunkId, bool InUsePfsCompression)
		{
			TargetPath = InTargetPath;
			OriginalPath = InOriginalPath;
			ChunkId = InChunkId;
			UsePfsCompression = InUsePfsCompression;
		}

		public override string ToString()
		{
			String FileLine = "    <file targ_path=\"" + TargetPath + "\"";

			if (!String.IsNullOrEmpty(OriginalPath))
			{
				FileLine += " orig_path=\"" + OriginalPath + "\"";
			}

			FileLine += " chunks=\"" + ChunkId.ToString() + "\"";

			if (!bPlayGoEmulation && UsePfsCompression)
			{
				FileLine += " pfs_compression=\"enable\"";
			}

			FileLine += "/>";

			return FileLine;
		}
	}

	private string GenerateDirectoriesContent(List<string> DirNameList)
	{
		var SortedDirNameList = DirNameList.Distinct().OrderBy(dir => dir).Where(dir => !String.IsNullOrEmpty(dir));

		List<string> FilePathStack = new List<string>();
		StringBuilder DirectoriesContent = new StringBuilder();
		int CurrentFilePathStackDepth = 0;

		// Generate directory tree
		foreach (string DirName in SortedDirNameList)
		{
			var DirPathFolders = DirName.Split(new char[] { '/' }, StringSplitOptions.RemoveEmptyEntries).ToList();

			CurrentFilePathStackDepth = FilePathStack.Count;
			while (CurrentFilePathStackDepth > 0
				&& (CurrentFilePathStackDepth > DirPathFolders.Count
				|| !FilePathStack.GetRange(0, CurrentFilePathStackDepth).SequenceEqual(DirPathFolders.GetRange(0, CurrentFilePathStackDepth))))
			{
				// Close directory
				for (int SpaceIndex = 0; SpaceIndex < CurrentFilePathStackDepth; SpaceIndex++)
				{
					DirectoriesContent.Append("  ");
				}
				DirectoriesContent.AppendLine("  </dir>");

				// Remove from stack
				FilePathStack.RemoveAt(CurrentFilePathStackDepth - 1);
				CurrentFilePathStackDepth--;
			}

			// Create folder from DirPathFolders[CurrentPathHierarchyLength]
			while (CurrentFilePathStackDepth < DirPathFolders.Count)
			{
				// Open directory
				for (int SpaceIndex = 0; SpaceIndex < CurrentFilePathStackDepth; SpaceIndex++)
				{
					DirectoriesContent.Append("  ");
				}
				DirectoriesContent.AppendLine("    <dir targ_name=\"" + DirPathFolders[CurrentFilePathStackDepth] + "\">");

				// Add to stack
				FilePathStack.Add(DirPathFolders[CurrentFilePathStackDepth]);
				CurrentFilePathStackDepth++;
			}
		}

		// Close root
		while (CurrentFilePathStackDepth > 0)
		{
			// Close directory
			for (int SpaceIndex = 0; SpaceIndex < CurrentFilePathStackDepth; SpaceIndex++)
			{
				DirectoriesContent.Append("  ");
			}
			DirectoriesContent.AppendLine("  </dir>");

			// Remove last
			FilePathStack.RemoveAt(CurrentFilePathStackDepth - 1);
			CurrentFilePathStackDepth--;
		}

		return DirectoriesContent.ToString();
	}

	string GetSFOTitle(bool bGeneratingPatch, bool bGeneratingRemaster)
	{
		//Pick the correct SFO title depending on type of package
		if (bGeneratingPatch)
		{
			return "param-patch";
		}
		else if (bGeneratingRemaster)
		{
			return  "param-remaster";
		}
		else
		{
			return "param";
		}
	}


	/// <summary>
	/// Create the relevant param.sfo file for packaging the PS4 game
	/// </summary>
	/// <param name="LocalDirectory">The directory containing the files and where the SFO is to be written</param>
	/// <param name="TitleParams">The per title packaging parameters</param>
	/// <param name="ChunkLanguageEntries">Language entries for the title</param>
	/// <param name="bGeneratingPatch">Is a patch being generated</param>
	/// <param name="bGeneratingRemaster">Is a remaster being generated</param>
	/// <returns></returns>
	private void CreateParamSFOFile(string LocalDirectory, PerTitlePackageParameters TitleParams, List<ChunkLanguageEntry> ChunkLanguageEntries, bool bGeneratingPatch, bool bGeneratingRemaster)
	{
		string SFTitle = GetSFOTitle(bGeneratingPatch, bGeneratingRemaster);

		//Write out the sfx file which orbis tools will convert to an sfo
		StringBuilder Contents = new StringBuilder();
		Contents.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>");
		Contents.AppendLine("<paramsfo>");

		Contents.AppendLine("  <param key=\"FORMAT\">" + "obs" + "</param>");
		Contents.AppendLine("  <param key=\"APP_TYPE\">" + "0" + "</param>");
		Contents.AppendLine("  <param key=\"SYSTEM_VER\">" + "0" + "</param>");
		Contents.AppendLine("  <param key=\"SERVICE_ID_ADDCONT_ADD_1\">" + "</param>");
		Contents.AppendLine("  <param key=\"SERVICE_ID_ADDCONT_ADD_2\">" + "</param>");
		Contents.AppendLine("  <param key=\"SERVICE_ID_ADDCONT_ADD_3\">" + "</param>");
		Contents.AppendLine("  <param key=\"SERVICE_ID_ADDCONT_ADD_4\">" + "</param>");
		Contents.AppendLine("  <param key=\"SERVICE_ID_ADDCONT_ADD_5\">" + "</param>");
		Contents.AppendLine("  <param key=\"SERVICE_ID_ADDCONT_ADD_6\">" + "</param>");
		Contents.AppendLine("  <param key=\"SERVICE_ID_ADDCONT_ADD_7\">" + "</param>");

		Contents.AppendLine("  <param key=\"TITLE_ID\">" + TitleParams.ShortTitleID + "</param>");
		Contents.AppendLine("  <param key=\"CONTENT_ID\">" + TitleParams.FullTitleId + "</param>");
		Contents.AppendLine("  <param key=\"TITLE\">" + TitleParams.Title + "</param>");
		if (!(TitleParams.Version < 0))
		{
			Contents.AppendLine("  <param key=\"VERSION\">" + TitleParams.Version.ToString("00.00", CultureInfo.InvariantCulture) + "</param>");
		}
		if (!(TitleParams.AppVersion < 0))
		{
			Contents.AppendLine("  <param key=\"APP_VER\">" + TitleParams.AppVersion.ToString("00.00", CultureInfo.InvariantCulture) + "</param>");
		}

		Contents.AppendLine("  <param key=\"PARENTAL_LEVEL\">" + TitleParams.ParentalLevel.ToString() + "</param>");
		Contents.AppendLine("  <param key=\"DOWNLOAD_DATA_SIZE\">" + TitleParams.DownloadDataSize[0].ToString() + "</param>");
		if (TitleParams.DownloadDataSize[1] >= 0)
		{
			Contents.AppendLine("  <param key=\"DOWNLOAD_DATA_SIZE_1\">" + TitleParams.DownloadDataSize[1].ToString() + "</param>");
		}

		if (TitleParams.OverwriteAttribute[0] >= 0)
		{
			Contents.AppendLine("  <param key=\"ATTRIBUTE\">" + TitleParams.OverwriteAttribute[0].ToString() + "</param>");
		}
		else
		{
			Contents.AppendLine("  <param key=\"ATTRIBUTE\">" + ((int)TitleParams.Attribute).ToString() + "</param>");
		}

		if (TitleParams.OverwriteAttribute[1] >= 0)
		{
			Contents.AppendLine("  <param key=\"ATTRIBUTE2\">" + TitleParams.OverwriteAttribute[1].ToString() + "</param>");
		}
		else
		{
			Contents.AppendLine("  <param key=\"ATTRIBUTE2\">" + ((int)TitleParams.Attribute2).ToString() + "</param>");
		}


		if (TitleParams.Self2MBPageAmount >= 0)
		{
			Contents.AppendLine("  <param key=\"SELF_2MIB_PAGE_AMOUNT\">" + TitleParams.Self2MBPageAmount.ToString() + "</param>");
		}
		Contents.AppendLine("  <param key=\"REMOTE_PLAY_KEY_ASSIGN\">" + TitleParams.RemotePlayKey.ToString() + "</param>");

		if (TitleParams.UserDefParam[0] > 0)
		{
			Contents.AppendLine("  <param key=\"USER_DEFINED_PARAM_1\">" + TitleParams.UserDefParam[0].ToString() + "</param>");
		}
		if (TitleParams.UserDefParam[1] > 0)
		{
			Contents.AppendLine("  <param key=\"USER_DEFINED_PARAM_2\">" + TitleParams.UserDefParam[1].ToString() + "</param>");
		}
		if (TitleParams.UserDefParam[2] > 0)
		{
			Contents.AppendLine("  <param key=\"USER_DEFINED_PARAM_3\">" + TitleParams.UserDefParam[2].ToString() + "</param>");
		}
		if (TitleParams.UserDefParam[3] > 0)
		{
			Contents.AppendLine("  <param key=\"USER_DEFINED_PARAM_4\">" + TitleParams.UserDefParam[3].ToString() + "</param>");
		}

		if (!String.IsNullOrEmpty(TitleParams.InstallDirSavedata))
		{
			Contents.AppendLine("  <param key=\"INSTALL_DIR_SAVEDATA\">" + TitleParams.InstallDirSavedata + "</param>");
		}
		if (!String.IsNullOrEmpty(TitleParams.SaveDataTransferTIDList))
		{
			Contents.AppendLine("  <param key=\"SAVE_DATA_TRANSFER_TITLE_ID_LIST\">" + TitleParams.SaveDataTransferTIDList + "</param>");
		}
		if (TitleParams.PTParam >= 0)
		{
			Contents.AppendLine("  <param key=\"PT_PARAM\">" + TitleParams.PTParam.ToString() + "</param>");
		}
		if (!String.IsNullOrEmpty(TitleParams.PubToolInfo))
		{
			Contents.AppendLine("  <param key=\"PUBTOOLINFO\">" + TitleParams.PubToolInfo + "</param>");
		}

		if (bGeneratingPatch)
		{
			Contents.AppendLine("  <param key=\"CATEGORY\">" + "gp" + "</param>");
		}
		else
		{
			Contents.AppendLine("  <param key=\"CATEGORY\">" + "gd" + "</param>");
		}

		if (bGeneratingRemaster)
		{
			Contents.AppendLine("  <param key=\"REMASTER_TYPE\">" + "1" + "</param>");
		}

		foreach (ChunkLanguageEntry ChunkLanguage in ChunkLanguageEntries)
		{
			if (!String.IsNullOrEmpty(ChunkLanguage.CultureId))
			{
				LanguageNumber LangNum;
				string LanguageEnum = ChunkLanguage.CultureId;
				if (LanguageEnum.Contains('-'))
				{
					LanguageEnum = LanguageEnum.Remove(LanguageEnum.IndexOf('-'), 1);
				}

				bool EnumParsed = Enum.TryParse(LanguageEnum, out LangNum);
				if (EnumParsed)
				{
					int FileNumber = (int)LangNum;
					string LanguageTitle = ChunkLanguage.LanguageTitle;
					if (String.IsNullOrEmpty(LanguageTitle))
					{
						LanguageTitle = TitleParams.Title;
					}
					Contents.AppendLine("  <param key=\"TITLE_" + FileNumber.ToString("00") + "\">" + ChunkLanguage.LanguageTitle + "</param>");
				}
			}
		}

		Contents.AppendLine("</paramsfo>");
		File.WriteAllText(Path.Combine(LocalDirectory, SFTitle + ".sfx"), Contents.ToString());

		IProcessResult Result = Run(Path.Combine(Environment.ExpandEnvironmentVariables("%SCE_ROOT_DIR%"), "ORBIS\\Tools\\Publishing Tools\\bin\\orbis-pub-cmd.exe"), "sfo_create " + '\"' + Path.Combine(LocalDirectory, SFTitle + ".sfx") + '\"' + " " + '\"' + Path.Combine(LocalDirectory, SFTitle + ".sfo") + '\"');
		Result.WaitForExit();
		//Remove the sfx file so it is not included in the final package build
		File.Delete(Path.Combine(LocalDirectory, SFTitle + ".sfx"));
	}

	private static void DirectoryCopy(string SourceDir, string DestDir)
	{
		DirectoryInfo DirInfo = new DirectoryInfo(SourceDir);

		// make sure destination exists
		if (!Directory.Exists(DestDir))
		{
			Directory.CreateDirectory(DestDir);
		}

		// get files in source directory
		foreach (FileInfo File in DirInfo.GetFiles())
		{
			string TargetFile = Path.Combine(DestDir, File.Name);
			FileInfo TargetFileInfo = new FileInfo(TargetFile);
			if (TargetFileInfo.Exists)
			{
				TargetFileInfo.Attributes &= ~FileAttributes.ReadOnly;
			}
			File.CopyTo(TargetFile, true);
		}

		// now recurse
		foreach (DirectoryInfo Subdir in DirInfo.GetDirectories())
		{
			DirectoryCopy(Subdir.FullName, Path.Combine(DestDir, Subdir.Name));
		}
	}
	private class PS4BundleSettings : BundleUtils.BundleSettings
	{
		public int ChunkID { get; set; }
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
		bool bGenerateSFO = false;
		bool bCopyLatestPatch = false;

		ConfigHierarchy PlatformGameConfig = null;
		if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "TitleID", out DefaultFullTitleId);
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "TitlePasscode", out DefaultPasscode);
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "StorageType", out DefaultStorageType);
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "AppType", out AppType);
			PlatformGameConfig.GetBool("/Script/PS4PlatformEditor.PS4TargetSettings", "BuildIsoImage", out bBuildIsoImage);
			PlatformGameConfig.GetBool("/Script/PS4PlatformEditor.PS4TargetSettings", "MoveFilesToOuterEdge", out bMoveFilesToOuterEdge);
			PlatformGameConfig.GetBool("/Script/PS4PlatformEditor.PS4TargetSettings", "CopyLatestPatch", out bCopyLatestPatch);
			PlatformGameConfig.GetBool("/Script/PS4PlatformEditor.PS4TargetSettings", "GenerateSFO", out bGenerateSFO);
		}

		if (string.IsNullOrEmpty(DefaultFullTitleId))
		{
			if (!bGenerateSFO)
			{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("Couldn't find TitleID.  Using default: IV0000-TEST00000_00-TESTTESTTESTTEST");
			Console.ResetColor();
			}
			DefaultFullTitleId = "IV0000-TEST00000_00-TESTTESTTESTTEST";
		}

		// User input, so force to uppercase
		DefaultFullTitleId = DefaultFullTitleId.ToUpperInvariant();

		if (Params.TitleID.Count == 0)
		{
			//No title ID is present then take the relevant subsection of the full title id

			int TitleIDPosition = DefaultFullTitleId.IndexOf('-');
			int TitleIDLength = DefaultFullTitleId.LastIndexOf('-') - TitleIDPosition - 1;
			TitleIDPosition += 1;

			Params.TitleID.Add(DefaultFullTitleId.Substring(TitleIDPosition , TitleIDLength).ToUpperInvariant());
		}

		if (string.IsNullOrEmpty(DefaultPasscode))
		{
			//No default passcode is provided so warn it will be auto generated.
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("No passcode found in PS4Engine.ini. Will use title.json passcode(s) or auto generate new one(s)");
			Console.ResetColor();
		}

		if (string.IsNullOrEmpty(DefaultStorageType))
		{
			if (!bGenerateSFO)
			{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("Couldn't find StorageType.  Using default: bd25");
			Console.ResetColor();
			}
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

			// User input, so force to uppercase
			TitleParams.TitleID = TitleID.ToUpperInvariant();
			TitleParams.FullTitleId = DefaultFullTitleId;
			TitleParams.Passcode = DefaultPasscode;
			TitleParams.StorageType = DefaultStorageType;
			TitleParams.bGenerateSFO = bGenerateSFO;
			TitleParams.bCopyLatestPatch = bCopyLatestPatch;

			TitleParameters.Add(TitleParams);
		}

		int OrbisPubCmdRunCount = 0;
		int OrbisPubFailCount = 0;
		string OutputDir = "";

		Parallel.ForEach(TitleParameters, (TitleParams) =>
		{
			// The PS4/titleid directory is staged as NonUFS so it will be lowercase.
			String StagedTitleOverrideDirectory = Path.Combine(Params.BaseStageDirectory, "PS4", TitleParams.TitleID);

			// read the title id and passcode from the title.json file in the specified titleid directory
			JsonObject TitleObj = null;
			bool Exists = false;
			bool bDefaultTitleJSON = false;

			if (File.Exists(Path.Combine(StagedTitleOverrideDirectory, "title.json")))
			{
				Exists = JsonObject.TryRead(new FileReference(Path.Combine(StagedTitleOverrideDirectory, "title.json")), out TitleObj);
			}
			else if (File.Exists(Path.Combine(SC.ProjectRoot.ToString(), "Build", "PS4", "titledata", TitleParams.TitleID, "title.json")))
			{
				Exists = JsonObject.TryRead(new FileReference(Path.Combine(SC.ProjectRoot.ToString(), "Build", "PS4", "titledata", TitleParams.TitleID, "title.json")), out TitleObj);
			}
			else
			{
				Exists = JsonObject.TryRead(new FileReference(Path.Combine(SC.ProjectRoot.ToString(), "Build", "PS4", "titledata", "title.json")), out TitleObj);
				bDefaultTitleJSON = true;
				StagedTitleOverrideDirectory = Path.Combine(Params.BaseStageDirectory, "PS4");
			}


		bool GenPasscodeAndReWriteTitleJSON = TitleParams.Passcode.Length == 32 ? false : true;
		//Grab the title.json file and read out the values
		if (Exists)
		{
			{
				string NewContentId;
				if (TitleObj.TryGetStringField("content_id", out NewContentId))
				{
					TitleParams.FullTitleId = NewContentId;
				}
					
				if (!TitleParams.FullTitleId.Contains(TitleParams.TitleID))
				{
					CommandUtils.LogError("Title ID must match the central value of \"content_id\" from relative title.json. CommandLineTitleID = " + TitleParams.TitleID + ", jsonContentID = " + TitleParams.FullTitleId);
					return;
				}
			}
			{
				string NewShortTitleId;
				if (TitleObj.TryGetStringField("title_id", out NewShortTitleId))
				{
					if (TitleParams.TitleID != NewShortTitleId)
					{
						CommandUtils.LogError("Title ID must match \"title_id\" from title.json. CommandLineTitleID = " + TitleParams.TitleID + ", jsonTitleID = " + NewShortTitleId);
						return;
					}
				}
			}
			{
				string NewPasscode = "";
				if (TitleObj.TryGetStringField("title_passcode", out NewPasscode))
				{
					if (NewPasscode.Length == 32)
					{
						TitleParams.Passcode = NewPasscode;
						GenPasscodeAndReWriteTitleJSON = false;
					}
					else if (GenPasscodeAndReWriteTitleJSON)
					{
						Console.ForegroundColor = ConsoleColor.Yellow;
						Console.WriteLine("Invalid passcode supplied. A new passcode will be autogenerated.");
						Console.ResetColor();
					}
				}
			}
			{
				string NewStorageType;
				if (TitleObj.TryGetStringField("storagetype", out NewStorageType))
				{
					TitleParams.StorageType = NewStorageType;
				}
			}
			{
				bool bNewGenerateSFO;
				if (TitleObj.TryGetBoolField("generate_sfo", out bNewGenerateSFO))
				{
					TitleParams.bGenerateSFO = bNewGenerateSFO;
				}
			}

			//If automatic generation of param files is enabled, fill out the other required values
			//Details of these parameters can be found at https://ps4.siedev.net/resources/documents/Misc/current/Param_File_Editor-Users_Guide/0004.html
			//Epic documentation for this feature can be found in Engine\Documentation\PDF\PS4\INT\Automated Patching on PS4 for UE4 Games.pdf
			if (TitleParams.bGenerateSFO)
			{
				if (!TitleObj.TryGetStringField("title", out TitleParams.Title))
				{
					TitleParams.Title = Params.ShortProjectName;
				}

				TitleObj.TryGetIntegerField("parental_level", out TitleParams.ParentalLevel);
				TitleObj.TryGetIntegerField("download_data_size", out TitleParams.DownloadDataSize[0]);
				if (!TitleObj.TryGetIntegerField("download_data_size_1", out TitleParams.DownloadDataSize[1]))
				{
					TitleParams.DownloadDataSize[1] = -1;
				}
				if (!TitleObj.TryGetIntegerField("self_2mib_page_amount", out TitleParams.Self2MBPageAmount))
				{
					TitleParams.Self2MBPageAmount = -1;
				}
				TitleObj.TryGetIntegerField("remote_play_key_assign", out TitleParams.RemotePlayKey);
				TitleObj.TryGetIntegerField("user_defined_param_1", out TitleParams.UserDefParam[0]);
				TitleObj.TryGetIntegerField("user_defined_param_2", out TitleParams.UserDefParam[1]);
				TitleObj.TryGetIntegerField("user_defined_param_3", out TitleParams.UserDefParam[2]);
				TitleObj.TryGetIntegerField("user_defined_param_4", out TitleParams.UserDefParam[3]);
				TitleObj.TryGetStringField("install_dir_savedata", out TitleParams.InstallDirSavedata);
				TitleObj.TryGetStringField("save_data_transfer_title_id_list", out TitleParams.SaveDataTransferTIDList);
				if (!TitleObj.TryGetIntegerField("pt_param", out TitleParams.PTParam))
				{
					TitleParams.PTParam = -1;
				}
				if (TitleParams.PTParam == 0 || TitleParams.PTParam == 1)
				{
					TitleParams.PTParam = 2;
				}
				if (TitleParams.PTParam > 8)
				{
					TitleParams.PTParam = 8;
				}
				TitleObj.TryGetStringField("patch_notes_location", out TitleParams.PatchInfoLoc);
				if (!TitleObj.TryGetDoubleField("overwrite_version", out TitleParams.Version))
				{
					TitleParams.Version = -1.0;
				}
				if (!TitleObj.TryGetDoubleField("overwrite_app_ver", out TitleParams.AppVersion))
				{
					TitleParams.AppVersion = -1.0;
				}
				TitleObj.TryGetStringField("pub_tool_info", out TitleParams.PubToolInfo);

				if (!TitleObj.TryGetIntegerField("overwrite_attribute", out TitleParams.OverwriteAttribute[0]))
				{
					TitleParams.OverwriteAttribute[0] = -1;
					TitleParams.Attribute = Attribute1.None;
					bool bResult = false;
					uint Option = 0;

					if (TitleObj.TryGetBoolField("user_management", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute |= Attribute1.UserMgmtSupp;
						}
					}

					if (TitleObj.TryGetUnsignedIntegerField("enter_button_assignment", out Option))
					{
						if (Option == 1)
						{
							TitleParams.Attribute |= Attribute1.CrossButtonEnter;
						}

						if (Option == 2)
						{
							TitleParams.Attribute |= Attribute1.AssignmentEnterButton;
						}
					}

					if (TitleObj.TryGetBoolField("share_menu", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute |= Attribute1.ShareMenuCustomised;
						}
					}

					if (TitleObj.TryGetUnsignedIntegerField("CPU_Mode", out Option))
					{
						if (Option == 1 || Option == 6)
						{
							TitleParams.Attribute |= Attribute1.CPU6Mode;
						}

						if (Option == 2 || Option == 7)
						{
							TitleParams.Attribute |= Attribute1.CPU7Mode;
						}
					}

					if (TitleObj.TryGetBoolField("stereoscopic3D_supported", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute |= Attribute1.Stereo3D;
						}
					}

					if (TitleObj.TryGetUnsignedIntegerField("PSVR", out Option))
					{
						if (Option == 1)
						{
							TitleParams.Attribute |= Attribute1.PSVRSupported;
						}

						if (Option == 2)
						{
							TitleParams.Attribute |= Attribute1.PSVRRequired;
						}
					}

					if (TitleObj.TryGetBoolField("NEO_mode", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute |= Attribute1.NEOMode;
						}
					}

					if (TitleObj.TryGetBoolField("hdr_mode", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute |= Attribute1.HDRSupported;
						}
					}
				}

				if (!TitleObj.TryGetIntegerField("overwrite_attribute2", out TitleParams.OverwriteAttribute[1]))
				{
					TitleParams.OverwriteAttribute[1] = -1;
					TitleParams.Attribute2 = Attribute2.None;
					bool bResult = false;
					uint Option = 0;

					if (TitleObj.TryGetBoolField("video_recording_library", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute2 |= Attribute2.VideoLibraryOn;
						}
					}

					if (TitleObj.TryGetBoolField("content_search_library", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute2 |= Attribute2.ContentSearchOn;
						}
					}

					if (TitleObj.TryGetUnsignedIntegerField("eye_distance", out Option))
					{
						if (Option == 1)
						{
							TitleParams.Attribute2 |= Attribute2.EyeDistanceDefault;
						}

						if (Option == 2)
						{
							TitleParams.Attribute2 |= Attribute2.DynamicEyeDistance;
						}
					}

					if (TitleObj.TryGetBoolField("broadcast", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute2 |= Attribute2.SeparateBroadcast;
						}
					}

					if (TitleObj.TryGetBoolField("vr_tracker_library", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute2 |= Attribute2.VRMoveDummyLoadOff;
						}
					}

					if (TitleObj.TryGetBoolField("tournament_1on1", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute2 |= Attribute2.Tournament1on1;
						}
					}

					if (TitleObj.TryGetBoolField("tournament_team", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute2 |= Attribute2.TournamentTeam;
						}
					}

					if (TitleObj.TryGetBoolField("tournament_free4all", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute2 |= Attribute2.TournamentFree4All;
						}
					}

					if (TitleObj.TryGetBoolField("thread_scheduler", out bResult))
					{
						if (bResult)
						{
							TitleParams.Attribute2 |= Attribute2.UseThreadScheduler;
						}
					}

					if (TitleObj.TryGetUnsignedIntegerField("2MB_page_setting", out Option))
					{
						if (Option == 1)
						{
							TitleParams.Attribute2 |= Attribute2.MB2PageSetting;
						}

						if (Option == 2)
						{
							TitleParams.Attribute2 |= Attribute2.MB2ReserveWithMapping;
						}

						if (Option == 3)
						{
							TitleParams.Attribute2 |= Attribute2.MB2ReserveAll;
						}
					}
				}
			}
		}

		if (!TitleParams.FullTitleId.Contains(TitleParams.TitleID))
		{
			CommandUtils.LogError("Title ID must match the central value of \"content_id\" from relative title.json. CommandLineTitleID = " + TitleParams.TitleID + ", jsonContentID = " + TitleParams.FullTitleId);
			return;
		}

			//Resave the title.json file including the new title_passcode if its been auto generated
			if (GenPasscodeAndReWriteTitleJSON)
			{
				{
					//If there is no title passcode provided by the user, auto generate a new one.
					Console.ForegroundColor = ConsoleColor.Yellow;
					Console.WriteLine("No passcode found in PS4Engine.ini or title.json - new passcode will be auto generated and saved to title.json");
					Console.ResetColor();

					//Generate and save new passcode
					RNGCryptoServiceProvider rngCrypto = new RNGCryptoServiceProvider();
					//24 bytes will encode as 32 chars
					byte[] PasscodeBytes = new byte[24];
					rngCrypto.GetBytes(PasscodeBytes);
					//Generate a new 32 digit passcode
					TitleParams.Passcode = Convert.ToBase64String(PasscodeBytes).Replace('+', '-').Replace('/', '_');

					Console.ForegroundColor = ConsoleColor.Yellow;
					Console.WriteLine("Auto-generated passcode = " + TitleParams.Passcode);
					Console.ResetColor();
				}

				{
					Console.ForegroundColor = ConsoleColor.Yellow;
					Console.WriteLine("Passcode was auto-generated, saving to title.json file in project and saved directories. This will be excluded in the packaged file.");
					Console.ResetColor();

					string[] JSONDirectories = new string[2];
					JSONDirectories[0] = StagedTitleOverrideDirectory;
					JSONDirectories[1] = Path.Combine(SC.ProjectRoot.ToString(), "Build", "PS4", "titledata");
					if(!bDefaultTitleJSON)
					{
						JSONDirectories[1] = Path.Combine(JSONDirectories[1], TitleParams.TitleID);
					}

					foreach (string Directory in JSONDirectories)
					{
						if (!DirectoryExists(Directory))
						{
							CreateDirectory(Directory);
						}

						string FilePath = Path.Combine(Directory, "title.json");
						if (!File.Exists(FilePath))
						{
							(File.Create(FilePath)).Close();
						}

						File.SetAttributes(FilePath, File.GetAttributes(FilePath) & ~FileAttributes.ReadOnly);

						using (JsonWriter TitleWriter = new JsonWriter(new FileReference(FilePath)))
						{
							TitleWriter.WriteObjectStart();
							{
								IEnumerable<string> KeyNames = TitleObj == null ? new List<string>() : TitleObj.KeyNames;
								if (!KeyNames.Contains("title_id"))
								{
									KeyNames = KeyNames.Concat(new[] { "title_id" });
								}
								if (!KeyNames.Contains("title_passcode"))
								{
									KeyNames = KeyNames.Concat(new[] { "title_passcode" });
								}
								if (!KeyNames.Contains("content_id"))
								{
									KeyNames = KeyNames.Concat(new[] { "content_id" });
								}

								foreach (string TitleKey in KeyNames)
								{
									if (TitleKey.Equals("title_passcode"))
									{
										TitleWriter.WriteValue("title_passcode", TitleParams.Passcode);
									}
									else if (TitleKey.Equals("title_id"))
									{
										TitleWriter.WriteValue("title_id", TitleParams.TitleID);
									}
									else if (TitleKey.Equals("content_id"))
									{
										TitleWriter.WriteValue("content_id", TitleParams.FullTitleId);
									}
									else if (TitleObj != null)
									{
										string FieldValue = null;
										if (TitleObj.TryGetStringField(TitleKey, out FieldValue))
										{
											TitleWriter.WriteValue(TitleKey, FieldValue);
										}
									}
								}
							}
							TitleWriter.WriteObjectEnd();
						}
					}
					{
						//Save an extra copy of the title passcode just in case.
						string FullFilePath = Path.Combine(JSONDirectories[1], "title_passcode_backup.txt");
						string Message = "Ensure you retain + store this passcode for future reference: \n" + TitleParams.Passcode;
						System.IO.File.WriteAllText(FullFilePath, Message);
					}
				}
			}

			//If SFO is being generated, ensure that the correct application version and master version are iterated as appropriate
			if (TitleParams.bGenerateSFO)
			{
				String AppVerDirectory = Path.Combine(SC.ProjectRoot.ToString(), "Build", "PS4", "titledata", TitleParams.TitleID);
				if (!DirectoryExists(AppVerDirectory))
				{
					CreateDirectory(AppVerDirectory);
				}
				StringBuilder AppVerText = new StringBuilder();
				if ((Params.GeneratePatch || Params.GenerateRemaster) && File.Exists(Path.Combine(AppVerDirectory, AppVerFile)))
				{
					string[] AppVersionText = File.ReadAllText(Path.Combine(AppVerDirectory, AppVerFile)).Split(new char[] { '\n', '\r' }, StringSplitOptions.RemoveEmptyEntries);

					if (TitleParams.Version < 0)
					{
						TitleParams.Version = double.Parse(AppVersionText[0], CultureInfo.InvariantCulture);
					}
					if (TitleParams.AppVersion < 0)
					{
						TitleParams.AppVersion = double.Parse(AppVersionText[1], CultureInfo.InvariantCulture);
						TitleParams.AppVersion += 0.01;
					}

					AppVerText.AppendLine(TitleParams.Version.ToString("00.00", CultureInfo.InvariantCulture));
					AppVerText.AppendLine(TitleParams.AppVersion.ToString("00.00", CultureInfo.InvariantCulture));

					if (AppVersionText.Length > 2 && int.Parse(AppVersionText[2], CultureInfo.InvariantCulture) == 1)
					{
						Params.GeneratePatch = false;
						Params.GenerateRemaster = true;
					}

					AppVerText.AppendLine(Params.GenerateRemaster ? "1" : "0");
				}
				else
				{
					if (TitleParams.Version >= 0)
					{
						AppVerText.AppendLine(TitleParams.Version.ToString("00.00", CultureInfo.InvariantCulture));
					}
					else
					{
						TitleParams.Version = 1.0;
						AppVerText.AppendLine("01.00");
					}

					if (TitleParams.AppVersion >= 0)
					{
						AppVerText.AppendLine(TitleParams.AppVersion.ToString("00.00", CultureInfo.InvariantCulture));
					}
					else
					{
						TitleParams.AppVersion = 1.0;
						AppVerText.AppendLine("01.00");
					}

				}
				File.WriteAllText(Path.Combine(AppVerDirectory, AppVerFile), AppVerText.ToString());

				//Ensure there is a name for the game
				if (String.IsNullOrEmpty(TitleParams.Title))
				{
					TitleParams.Title = Params.ShortProjectName;
				}

				if(!DirectoryExists(Path.Combine(Params.BaseStageDirectory, "PS4", "sce_sys", TitleParams.TitleID)))
				{
					CreateDirectory(Path.Combine(Params.BaseStageDirectory, "PS4", "sce_sys", TitleParams.TitleID));
				}
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
			if (!StagedTitleOverrideDirectory.Equals(Path.Combine(Params.BaseStageDirectory, "PS4")) && Directory.Exists(StagedTitleOverrideDirectory))
			{
				string[] OverrideFilesList = Directory.GetFiles(StagedTitleOverrideDirectory);
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
				if (String.Compare(Path.GetFileName(Dir), TitleParams.TitleID, StringComparison.InvariantCultureIgnoreCase) != 0 && ((!Params.GeneratePatch && !Params.GenerateRemaster) || !Path.GetFileName(Dir).Contains("changeinfo")) && !Path.GetFileName(Dir).Contains("trophy") && !Path.GetFileName(Dir).Contains("keymap_rp"))
				{
					TitleParams.ExcludedDirList.Add("sce_sys/" + Path.GetFileName(Dir));
				}
			}
			ExcludeDirs = Directory.GetDirectories(Path.Combine(Params.BaseStageDirectory, "PS4"));
			foreach (string Dir in ExcludeDirs)
			{
				if (String.Compare(Path.GetFileName(Dir), TitleParams.TitleID, StringComparison.InvariantCultureIgnoreCase) != 0)
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
				LogInformation("Waiting at {0} to start creating package {1}", DateTime.Now.ToString(), RunIndex);
				Thread.Sleep(RunIndex * 5 * 1000);
				LogInformation("Starting at {0} to create package {1}", DateTime.Now.ToString(), RunIndex);

				StringBuilder Contents = new StringBuilder();

				string ConfigurationStorageType = TitleParams.StorageType;
				if (Params.GeneratePatch)
				{
					//patches only support 25GB right now.
					ConfigurationStorageType = "digital25";
				}

				// may want to force dual layer to prepare for later disc masters with more data without needing another SKU.
				bool bForceDualLayer = String.Compare(ConfigurationStorageType, "bd50", true) == 0;
				List<ChunkLanguageEntry> ChunkLanguageEntries = GetChunkLanguageEntries(Params, SC);
				int[] ChunkLayers = GetChunkLayers(ChunkLanguageEntries, out int NumLanguageChunks, out int NumChunks, Params.GeneratePatch, Params, SC, bForceDualLayer);

				LogInformation("Chunk Layer Assignments:");
				for (int ChunkID = 0; ChunkID < ChunkLayers.Length; ++ChunkID)
				{
					LogInformation("Chunk {0} is assigned to Layer {1}", ChunkID, ChunkLayers[ChunkID]);
				}

				const bool bPlayGoEmulation = false;
				string LocalDirectory = Path.Combine(Params.BaseStageDirectory, "PS4") + "\\";
				string TargetExecutable = SC.StageExecutables[SC.StageTargetConfigurations.IndexOf(TargetConfiguration)] + Platform.GetExeExtension(SC.StageTargetPlatform.PlatformType);

				{
					string SceSysTitleDirectory = Path.Combine(Params.BaseStageDirectory, "PS4", "sce_sys", TitleParams.TitleID);
					//If the SFO generation has been requested, then create the param sfo file for packaging
					if (TitleParams.bGenerateSFO)
					{

						CreateParamSFOFile(SceSysTitleDirectory, TitleParams, ChunkLanguageEntries, Params.GeneratePatch, Params.GenerateRemaster);
						if (Params.GeneratePatch || Params.GenerateRemaster)
						{
							GenerateChangeInfoXMLFiles(TitleParams.PatchInfoLoc, ChunkLanguageEntries, SceSysTitleDirectory);
						}
					}

					//Ensure that only this title's sfo file is included when packaging, otherwise it will revert to the default found in sce_sys once GetFilesAndDirectoriesContents is called
					string SFOTitleFile = Path.Combine(SceSysTitleDirectory, GetSFOTitle(Params.GeneratePatch, Params.GenerateRemaster) + ".sfo");
					if (File.Exists(SFOTitleFile))
					{
						TitleParams.IgnoreDefaultSFO = true;
					}

					//Ensure that only the title-specific nptitle file is included when packaging, otherwise it will revert to the default found in sce_sys once GetFilesAndDirectoriesContents is called
					string NPTitleFile = Path.Combine(SceSysTitleDirectory, "nptitle.dat");
					if (File.Exists(NPTitleFile))
					{
						TitleParams.IgnoreDefaultDAT = true;
					}
				}

				ConfigHierarchy BundleConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.InstallBundle, SC.RawProjectPath.Directory, SC.StageTargetPlatform.PlatformType);

				Dictionary<FileReference, int> CustomFileChunkMapping = null;
				List<PS4BundleSettings> InstallBundles = null;
				if (BundleConfig.GetArray("InstallBundleManager.BundleSources", "DefaultBundleSources", out List<string> InstallBundleSources) &&
					InstallBundleSources.Contains("PlayGo"))
				{
					LogInformation("Using PlayGo Chunk mappings from install bundle config.");

					BundleUtils.LoadBundleConfig(SC.RawProjectPath.Directory, SC.StageTargetPlatform.PlatformType, out InstallBundles,
						delegate (PS4BundleSettings Settings, ConfigHierarchy InBundleConfig, string BundleSection)
						{
							if (InBundleConfig.GetInt32(BundleSection, "PlatformChunkID", out int ChunkID))
							{
								Settings.ChunkID = ChunkID;
							}
							else
							{
								Settings.ChunkID = 0;
							}
						});
				}
				else
				{
					LogInformation("Using PlayGo Chunk mappings from platform engine config");

					CustomFileChunkMapping = GetCustomChunkMapping(Params, SC, LocalDirectory);
				}

				GetFilesAndDirectoriesContents(
					LocalDirectory,
					ChunkLanguageEntries, InstallBundles, CustomFileChunkMapping,
					ChunkLayers,
					TitleParams,
					TargetConfiguration,
					bPlayGoEmulation, true, TargetExecutable, Params.GeneratePatch, Params.GenerateRemaster,
					out List<StagedFileEntry>[] FilesContentsByLayer, out List<string> DirNameList, true);

				// Possibly shrink number of chunks because unreal chunks may be remapped to different PS4 Chunks
				NumChunks = FilesContentsByLayer.Max(layer => (layer.Count != 0) ? layer.Max(entry => entry.ChunkId) : 0) + 1;
				NumChunks = Math.Max(NumChunks, NumLanguageChunks);

				//check that the .self file has been found and eboot.bin has been/will be created
				if (!bEbootCreated)
				{
					LogWarning("eboot.bin not created, check configuration + self file path");
					return;
				}

				StringBuilder ChunkContents = new StringBuilder();
				BuildChunkContents(ChunkContents, ChunkLanguageEntries, NumChunks, bPlayGoEmulation, Params, SC, bForceDualLayer, ref ChunkLayers);

				bool bDualLayer = false;
				foreach (int Layer in ChunkLayers)
				{
					if (Layer != 0)
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
				String AppPath = String.Empty;
				String ReleasePackagePath = String.Empty;
				String LatestPatchPackagePath = String.Empty;

				// Patches and Remasters should both state the original release they were based on, and the latest patch, to allow
				// fast patching
				if (Params.IsGeneratingPatch || Params.IsGeneratingRemaster)
				{
					string ReleaseVersionPath = CombinePaths(Params.GetBasedOnReleaseVersionPath(SC, Params.Client), TitleParams.TitleID);

					// Search all directories, as the .pkg may be in a "Submission-*" folder
					foreach (String PkgFile in Directory.GetFiles(ReleaseVersionPath, "*.pkg", SearchOption.AllDirectories))
					{
						ReleasePackagePath = PkgFile;
						break;
					}

					string PatchVersionPath = CombinePaths(Params.GetBasedOnReleaseVersionPath(SC, Params.Client), "LatestPatch", TitleParams.TitleID);

					try
					{
						// Search all directories, as the .pkg may be in a "Submission-*" folder
						foreach (String PkgFile in Directory.GetFiles(PatchVersionPath, "*.pkg", SearchOption.AllDirectories))
						{
							LatestPatchPackagePath = PkgFile;
							break;
						}
					}
					catch { }

					//todo, take a Day 1 patch parameter and skip the patch_type if it's set.

					if (LatestPatchPackagePath.Length == 0)
					{
						LogInformation("No LatestPatch found at {0}, patch will be generated against original master", PatchVersionPath);
						AppPath = String.Format("app_path=\"{0}\" patch_type=\"ref_a\"", ReleasePackagePath);
					}
					else
					{
						LogInformation("Generating patch against {0}", LatestPatchPackagePath);
						AppPath = String.Format("app_path=\"{0}\" latest_pkg_path=\"{1}\"", ReleasePackagePath, LatestPatchPackagePath);
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

				if (!string.IsNullOrEmpty(Params.DiscVersion))
				{
					Contents.AppendLine("    <disc_info>");
					Contents.AppendLine(String.Format("      <param key=\"disc_version\">{0}</param>", Params.DiscVersion));
					Contents.AppendLine("    </disc_info>");
				}
				else if (bGenerateSFO && !Params.IsGeneratingPatch)
				{
					Contents.AppendLine("    <disc_info>");
					Contents.AppendLine(String.Format("      <param key=\"disc_version\">{0}</param>", TitleParams.AppVersion.ToString("00.00", CultureInfo.InvariantCulture)));
					Contents.AppendLine("    </disc_info>");
				}

				Contents.AppendLine("  </volume>");
				Contents.AppendLine("  <files img_no=\"0\">");
				foreach(List<StagedFileEntry> FilesContents in FilesContentsByLayer)
				{
					foreach(StagedFileEntry StagedFile in FilesContents)
					{
						Contents.AppendLine(StagedFile.ToString());
					}
				}
				Contents.AppendLine("  </files>");
				Contents.AppendLine("  <rootdir>");
				Contents.Append(GenerateDirectoriesContent(DirNameList));
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
				OutputDir = GP4Dir;
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
					if (File.Exists(GP4Path) && File.Exists(PkgPath))
					{
					//	delete redundant files for patches
						UtilParams += "--delete_redundant_file ";
					}
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

				VerifyLaunchChunkSize(StdOut);
			});

			//If a new patch or remaster has been automatically generated, ensure it is copied to the "latest patch" folder in the original build so future patches can be build against it.
			if (TitleParams.bGenerateSFO && TitleParams.bCopyLatestPatch && (Params.GeneratePatch || Params.GenerateRemaster) && !String.IsNullOrEmpty(OutputDir))
			{
				string PatchVersionPath = CombinePaths(Params.GetBasedOnReleaseVersionPath(SC, Params.Client), "LatestPatch", TitleParams.TitleID);

				if (DirectoryExists(PatchVersionPath))
				{
					DirectoryInfo PatchDInfo = new DirectoryInfo(PatchVersionPath);
					PatchDInfo.Delete(true);
				}

				DirectoryCopy(OutputDir, PatchVersionPath);
			}
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

		//Check the pkgpath is valid
		if (!File.Exists(PkgPath))
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("Couldn't find PkgPath " + PkgPath);
			Console.ResetColor();
			return;
		}

		ConfigHierarchy PlatformGameConfig = null;
		if (Params.EngineConfigs.TryGetValue(PlatformType, out PlatformGameConfig))
		{
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "TitlePasscode", out Passcode);
		}

		string PkgTitleID = null;
		foreach(string TitleID in Params.TitleID)
		{
			if(PkgPath.ToUpperInvariant().Contains(TitleID.ToUpperInvariant()))
			{
				PkgTitleID = TitleID.ToUpperInvariant();
			}
		}

		if (string.IsNullOrEmpty(PkgTitleID))
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Console.WriteLine("Could not find appropriate titleID in pkg path for extracting package.");
			Console.ResetColor();
			return;
		}

		//Previously generated passcode must be used to extract package.
		bool Exists = false;
		JsonObject TitleObj = null;
		String StagedTitleOverrideDirectory = Path.Combine(Params.BaseStageDirectory, "PS4", PkgTitleID);
		if (File.Exists(Path.Combine(StagedTitleOverrideDirectory, "title.json")))
		{
			Exists = JsonObject.TryRead(new FileReference(Path.Combine(StagedTitleOverrideDirectory, "title.json")), out TitleObj);
		}
		else
		{
			Exists = JsonObject.TryRead(new FileReference(Path.Combine(Params.RawProjectPath.Directory.ToString(), "Build", "PS4", "titledata", PkgTitleID, "title.json")), out TitleObj);
		}

		if(Exists)
		{
			string NewPasscode;
			if(TitleObj.TryGetStringField("title_passcode", out NewPasscode))
			{
				Passcode = NewPasscode;
			}
		}

		if (string.IsNullOrEmpty(Passcode))
		{					
				Console.ForegroundColor = ConsoleColor.Yellow;
				Console.WriteLine("Couldn't find PS4TitlePasscode in PS4Engine.ini or appropriate title.json for extracting the package.");
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
		if (DirectoryReference.Exists(EngineMetadataPath))
		{
			SC.StageFiles(StagedFileType.SystemNonUFS, EngineMetadataPath, StageFilesSearch.AllDirectories, new StagedDirectoryReference("sce_sys"));
		}

		DirectoryReference ProjectMetadataPath = DirectoryReference.Combine(SC.ProjectRoot, "Build/PS4/sce_sys");
		if (DirectoryReference.Exists(ProjectMetadataPath))
		{
			SC.StageFiles(StagedFileType.SystemNonUFS, ProjectMetadataPath, StageFilesSearch.AllDirectories, new StagedDirectoryReference("sce_sys"));
		}

		DirectoryReference ProjectCloudPath = DirectoryReference.Combine(SC.ProjectRoot, "Build/PS4/Cloud");
		if (DirectoryReference.Exists(ProjectCloudPath))
		{
			SC.StageFiles(StagedFileType.SystemNonUFS, ProjectCloudPath, StageFilesSearch.AllDirectories, new StagedDirectoryReference("Cloud"));
		}

		// If not shipping, stage the symbol files that are generated for logging asserts, ensures, errors.
		if (SC.StageTargetConfigurations.Contains(UnrealTargetConfiguration.Shipping) == false)
		{
			DirectoryReference SymbolPath = DirectoryReference.Combine(SC.ProjectRoot, "Build/PS4/symbols");
			if (DirectoryReference.Exists(SymbolPath))
			{
				SC.StageFiles(StagedFileType.NonUFS, SymbolPath, StageFilesSearch.AllDirectories, new StagedDirectoryReference("symbols"));
			}
		}

			DirectoryReference ProjectNoRedistMetadataPath = DirectoryReference.Combine(SC.ProjectRoot, "Build/PS4/NoRedist/sce_sys");
		if (DirectoryReference.Exists(ProjectNoRedistMetadataPath))
		{
			SC.StageFiles(StagedFileType.SystemNonUFS, ProjectNoRedistMetadataPath, StageFilesSearch.AllDirectories, new StagedDirectoryReference("sce_sys"));
		}

		// Stage the invite icon
		FileReference InviteIcon = FileReference.Combine(SC.EngineRoot, "Build", "PS4", "InviteIcon.jpg");
		if (FileReference.Exists(InviteIcon))
		{
			SC.StageFile(StagedFileType.NonUFS, InviteIcon);
		}

		// Stage the title data
		// Stage these as NonUFS as they need to be lowercase because they are read at runtime by the engine.
		DirectoryReference EngineTitleDataDir = DirectoryReference.Combine(SC.EngineRoot, "Build", "PS4", "titledata");
		if (DirectoryReference.Exists(EngineTitleDataDir))
		{
			SC.StageFiles(StagedFileType.NonUFS, EngineTitleDataDir, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
		}

		DirectoryReference ProjectTitleDataDir = DirectoryReference.Combine(SC.ProjectRoot, "Build", "PS4", "titledata");
		if (DirectoryReference.Exists(ProjectTitleDataDir))
		{
			SC.StageFiles(StagedFileType.NonUFS, ProjectTitleDataDir, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
		}

		DirectoryReference ProjectNoRedistTitleDataDir = DirectoryReference.Combine(SC.ProjectRoot, "Build", "PS4", "NoRedist", "titledata");
		if (DirectoryReference.Exists(ProjectNoRedistTitleDataDir))
		{
			SC.StageFiles(StagedFileType.NonUFS, ProjectNoRedistTitleDataDir, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
		}

		// grab the modules that need to go into a package from the SDK directory
		DirectoryReference ModulesDir = new DirectoryReference(Environment.ExpandEnvironmentVariables("%SCE_ORBIS_SDK_DIR%\\target\\sce_module"));
		foreach (FileReference ModuleFile in DirectoryReference.EnumerateFiles(ModulesDir, "*.prx"))
		{
			if (!ModuleFile.FullName.EndsWith("_debug.prx", StringComparison.InvariantCultureIgnoreCase))
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
			if (TargetConfiguration != UnrealTargetConfiguration.Shipping && TargetConfiguration != UnrealTargetConfiguration.Test)
			{
				List<string> SymbolFileNames = new List<string>();
				SymbolFileNames.Add(TargetConfiguration.ToString() + "-Symbols.bin");
				SymbolFileNames.Add(TargetConfiguration.ToString() + "-SymbolNames.bin");
				SymbolFileNames.Add(TargetConfiguration.ToString() + "-SymbolMetaData.txt");

				foreach (string SymbolFileName in SymbolFileNames)
				{
					FileReference SymbolFile = FileReference.Combine(SC.ProjectRoot, "Build", "PS4", "Symbols", SymbolFileName);
					if (FileReference.Exists(SymbolFile))
					{
						SC.StageFile(StagedFileType.NonUFS, SymbolFile, StagedFileReference.Combine("Symbols", SymbolFile.GetFileName()));
					}
				}
			}

			// Stage the .self as a NonUFS file as it is in the project directory which needs to be lowercased to prevent case sensitivity issues.
			SC.StageFile(StagedFileType.NonUFS, FileReference.Combine(SC.ProjectBinariesFolder, Exe + Platform.GetExeExtension(SC.StageTargetPlatform.PlatformType)));
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
		// and to be able to reconstruct original paths during directory iteration at runtime.
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
				List<ChunkLanguageEntry> ChunkLanguageEntries = GetChunkLanguageEntries(Params, SC);
				int[] ChunkLayers = GetChunkLayers(ChunkLanguageEntries, out int NumLanguageChunks, out int NumChunks, Params.GeneratePatch, Params, SC, false);

				LogInformation("Chunk Layer Assignments:");
				for(int ChunkID = 0; ChunkID < ChunkLayers.Length; ++ChunkID)
				{
					LogInformation("Chunk {0} is assigned to Layer {1}", ChunkID, ChunkLayers[ChunkID]);
				}

				const bool bPlayGoEmulation = true;
				PerTitlePackageParameters PackageParameters = new PerTitlePackageParameters();
				string LocalDirectory = Path.Combine(Params.BaseStageDirectory, "PS4") + "\\";
				string TargetExecutable = SC.StageExecutables[0] + Platform.GetExeExtension(SC.StageTargetPlatform.PlatformType);

				ConfigHierarchy BundleConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.InstallBundle, SC.RawProjectPath.Directory, SC.StageTargetPlatform.PlatformType);

				Dictionary<FileReference, int> CustomFileChunkMapping = null;
				List<PS4BundleSettings> InstallBundles = null;
				if (BundleConfig.GetArray("InstallBundleManager.BundleSources", "DefaultBundleSources", out List<string> InstallBundleSources) &&
					InstallBundleSources.Contains("PlayGo"))
				{
					LogInformation("Using PlayGo Chunk mappings from install bundle config.");

					BundleUtils.LoadBundleConfig(SC.RawProjectPath.Directory, SC.StageTargetPlatform.PlatformType, out InstallBundles,
						delegate (PS4BundleSettings Settings, ConfigHierarchy InBundleConfig, string BundleSection)
						{
							if (InBundleConfig.GetInt32(BundleSection, "PlatformChunkID", out int ChunkID))
							{
								Settings.ChunkID = ChunkID;
							}
							else
							{
								Settings.ChunkID = 0;
							}
						});
				}
				else
				{
					LogInformation("Using PlayGo Chunk mappings from platform engine config");

					CustomFileChunkMapping = GetCustomChunkMapping(Params, SC, LocalDirectory);
				}

				GetFilesAndDirectoriesContents(
					LocalDirectory,
					ChunkLanguageEntries, InstallBundles, CustomFileChunkMapping,
					ChunkLayers,
					PackageParameters,
					SC.StageTargetConfigurations[0],
					bPlayGoEmulation, true, TargetExecutable, Params.GeneratePatch, Params.GenerateRemaster,
					out List<StagedFileEntry>[] FilesContentsByLayer, out List<string> DirNameList);

				// Possibly shrink number of chunks because unreal chunks may be remapped to different PS4 Chunks
				NumChunks = FilesContentsByLayer.Max(layer => (layer.Count != 0) ? layer.Max(entry => entry.ChunkId) : 0) + 1;
				NumChunks = Math.Max(NumChunks, NumLanguageChunks);

				StringBuilder ChunkContents = new StringBuilder();
				BuildChunkContents(ChunkContents, ChunkLanguageEntries, NumChunks, bPlayGoEmulation, Params, SC, false, ref ChunkLayers);

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
				foreach (List<StagedFileEntry> FilesContents in FilesContentsByLayer)
				{
					foreach (StagedFileEntry StagedFile in FilesContents)
					{
						Contents.AppendLine(StagedFile.ToString());
					}
				}
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

	public override string GetLaunchExtraCommandLine(ProjectParams Params)
	{
		ConfigHierarchy PlatformGameConfig = null;
		string DeployMode = "";
		if (Params.EngineConfigs.TryGetValue(UnrealTargetPlatform.PS4, out PlatformGameConfig))
		{
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "DeployMode", out DeployMode);
		}

		string ExtraCommandLine = "";

		if (Params.Deploy && !Params.CookOnTheFly && String.Compare(DeployMode, "DeployToDevice", StringComparison.InvariantCultureIgnoreCase) == 0)
		{
			if (Params.DeployFolder.Length > 0)
			{
				ExtraCommandLine = "-deployedbuild=" + Params.DeployFolder + " ";
			}
			else
			{
				ExtraCommandLine = "-deployedbuild ";
			}
		}

		return ExtraCommandLine;
	}

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

	public override string GetPlatformPakCommandLine(ProjectParams Params, DeploymentContext SC)
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
		ConfigHierarchy PlatformGameConfig = null;
		string DeployMode = "";
		if (Params.EngineConfigs.TryGetValue(UnrealTargetPlatform.PS4, out PlatformGameConfig))
		{
			PlatformGameConfig.GetString("/Script/PS4PlatformEditor.PS4TargetSettings", "DeployMode", out DeployMode);
		}

		// Unless DeployToDevice is enabled the build does not get deployed and is loaded over the host network
		if (String.Compare(DeployMode, "DeployToDevice", StringComparison.InvariantCultureIgnoreCase) != 0)
		{
			return;
		}

		if (SC.StageTargetConfigurations.Count != 1)
		{
			LogInformation("Deploying with more than one executable. Only {0} will be deployed.", SC.StageExecutables[SC.StageExecutables.Count - 1]);
		}

		// if we skipped staging for some reason, kill running process before starting deploy.
		if (!Params.Stage || Params.SkipStage)
		{
			KillCurrentRunningProcess(Params.DeviceNames[0]);
		}

		var TargetConfiguration = SC.StageTargetConfigurations[0];

		String BaseTargetPath = GetBaseDeployDirectory(Params, SC, Params.DeviceNames[0]);
		List<String> AllFilesToDeploy = new List<string>();

		LogInformation("Deployment path is " + BaseTargetPath);

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
				LogInformation("Deleting loose data because of PAK file deployment.");

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
				LogInformation("Deleting pak files because of non-PAK file deployment.");
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
			String TargetUFSDeployedPath = Path.Combine(BaseTargetPath, SC.GetUFSDeployedManifestFileName(null));
			String TargetNonUFSDeployedPath = Path.Combine(BaseTargetPath, SC.GetNonUFSDeployedManifestFileName(null));

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
				// User input so force to uppercase
				PkgPath = CombinePaths(PkgPath, Params.TitleID[0].ToUpperInvariant());
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
					LogInformation("Moving file from {0} to {1}", PakFile, DestPath);
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

	public override bool PublishSymbols(DirectoryReference SymbolStoreDirectory, List<FileReference> Files, string Product, string BuildVersion = null)
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

		DirectoryReference TempSymStoreDir = DirectoryReference.Combine(RootDirectory, "Saved", "SymStore");
		DirectoryReference.CreateDirectory(TempSymStoreDir);
		DeleteDirectoryContents(TempSymStoreDir);

		bool bSuccess = true;
		foreach (var SymbolFile in Files.Where(x => x.HasExtension(".self")))
		{
			IProcessResult RunSymStore = CommandUtils.Run(UtilPath, string.Format("add /f \"{0}\" /s \"{1}\" /o /compress", SymbolFile.FullName, TempSymStoreDir.FullName));
			if (RunSymStore.ExitCode != 0)
			{
				bSuccess = false;
				continue;
			}
		}

		foreach (DirectoryReference BinaryFileDir in DirectoryReference.EnumerateDirectories(DirectoryReference.Combine(TempSymStoreDir, "PS4")))
		{
			foreach (DirectoryReference HashDir in DirectoryReference.EnumerateDirectories(BinaryFileDir))
			{
				foreach (FileReference SymbolFile in DirectoryReference.EnumerateFiles(HashDir))
				{
					string RelativePath = SymbolFile.MakeRelativeTo(DirectoryReference.Combine(TempSymStoreDir));
					FileReference ActualDestinationFile = FileReference.Combine(SymbolStoreDirectory, RelativePath);

					// Try and add a version file.  Do this before checking to see if the symbol is there already in the case of exact matches (multiple builds could use the same pdb, for example)
					if (!string.IsNullOrWhiteSpace(BuildVersion))
					{
						FileReference BuildVersionFile = FileReference.Combine(ActualDestinationFile.Directory, string.Format("{0}.version", BuildVersion));
						// Attempt to create the file. Just continue if it fails.
						try
						{
							DirectoryReference.CreateDirectory(BuildVersionFile.Directory);
							FileReference.WriteAllText(BuildVersionFile, string.Empty);
						}
						catch (Exception Ex)
						{
							LogWarning("Failed to write the version file, reason {0}", Ex.ToString());
						}
					}

					// Don't bother copying the temp file if the destination file is there already.
					if (FileReference.Exists(ActualDestinationFile))
					{
						LogInformation("Destination file {0} already exists, skipping", ActualDestinationFile.FullName);
						continue;
					}

					FileReference TempDestinationFile = new FileReference(ActualDestinationFile.FullName + Guid.NewGuid().ToString());
					try
					{
						CommandUtils.CopyFile(SymbolFile.FullName, TempDestinationFile.FullName);
					}
					catch (Exception Ex)
					{
						throw new AutomationException("Couldn't copy the symbol file to the temp store! Reason: {0}", Ex.ToString());
					}
					// Move the file in the temp store over.
					try
					{
						FileReference.Move(TempDestinationFile, ActualDestinationFile);
					}
					catch (Exception Ex)
					{
						// If the file is there already, it was likely either copied elsewhere (and this is an ioexception) or it had a file handle open already.
						// Either way, it's fine to just continue on.
						if (FileReference.Exists(ActualDestinationFile))
						{
							LogInformation("Destination file {0} already exists or was in use, skipping.", ActualDestinationFile.FullName);
							continue;
						}
						// If it doesn't exist, we actually failed to copy it entirely.
						else
						{
							LogWarning("Couldn't move temp file {0} to the symbol store at location {1}! Reason: {2}", TempDestinationFile.FullName, ActualDestinationFile.FullName, Ex.ToString());
						}
					}
					// Delete the temp one no matter what, don't want them hanging around in the symstore
					finally
					{
						FileReference.Delete(TempDestinationFile);
					}
				}
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

	public override bool SymbolServerRequiresLock
	{
		// No lock file required on PS4 symbols.
		get { return false; }
	}
}