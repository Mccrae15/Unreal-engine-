// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemPS4 : ModuleRules
{
	public OnlineSubsystemPS4(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateIncludePaths.Add("Private");

		PublicAdditionalLibraries.AddRange(new string[]
		{
			"SceNpToolkit2_stub_weak",
			"SceNpWebApi_stub_weak",
			"SceNpCommerce_stub_weak",
			"SceNpSnsFacebookDialog_stub_weak",
			"SceNpUtility_stub_weak",
			"SceNpAuth_stub_weak",
			"SceInvitationDialog_stub_weak",
			"SceGameCustomDataDialog_stub_weak",
			"SceRtc_stub_weak",
			"SceSystemService_stub_weak",
			"SceNet_stub_weak",
			"SceNetCtl_stub_weak",
			"SceHttp_stub_weak",
			"SceSsl_stub_weak",
			"SceNpCommon_stub_weak",
			"SceNpManager_stub_weak",
			"SceNpMatching2_stub_weak",
			"SceNpScore_stub_weak",
			"SceCommonDialog_stub_weak",
			"SceNpProfileDialog_stub_weak",
			"SceWebBrowserDialog_stub_weak",
			"SceNpSns_stub_weak",
			"SceVoiceQoS_stub_weak",
			"SceErrorDialog_stub_weak",
			"SceJson2_stub_weak",
			"SceSigninDialog_stub_weak",

			// NOTE: This library is used for both TUS and TSS support
			"SceNpTus_stub_weak",
		});

		PublicDefinitions.Add("ONLINESUBSYSTEMPS4_PACKAGE=1");
		PublicDefinitions.Add("WITH_PS4=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Sockets",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
		});
	}
}
