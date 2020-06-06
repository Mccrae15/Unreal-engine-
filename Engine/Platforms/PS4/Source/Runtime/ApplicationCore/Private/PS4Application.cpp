// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4Application.h"

#include <invitation_dialog.h>
#include <video_out.h>

static int32 GPS4Allow4kOutput = 0;
static FAutoConsoleVariableRef CVarPS4Allow4kOutput(
	TEXT("r.PS4Allow4kOutput"),
	GPS4Allow4kOutput,
	TEXT("Allows the engine to allocate 4k backbuffers if a 4k tv is detected."),
	ECVF_ReadOnly
);

static int32 GPS4MaxBackbufferWidth = 3840;
static FAutoConsoleVariableRef CVarGPS4MaxBackbufferWidth(
	TEXT("r.PS4MaxBackbufferWidth"),
	GPS4MaxBackbufferWidth,
	TEXT("The maximum width of the backbuffer."),
	ECVF_ReadOnly
);

static int32 GPS4MaxBackbufferHeight = 2160;
static FAutoConsoleVariableRef CVarGPS4MaxBackbufferHeight(
	TEXT("r.PS4MaxBackbufferHeight"),
	GPS4MaxBackbufferHeight,
	TEXT("The maximum height of the backbuffer."),
	ECVF_ReadOnly
);

FPS4Application* FPS4Application::CreatePS4Application()
{
	check(Singleton == nullptr);
	FPS4Application* PS4App = new FPS4Application;
	Singleton = PS4App;
	return PS4App;
}

FPS4Application::FPS4Application()
{
	CachedPlayTogetherHostEventParam.Reset();
	CachedTournamentJoinEventParam.Reset();
	CachedTournamentJoinMatchEventParam.Reset();
	CachedTournamentJoinTeamOnTeamMatchEventParam.Reset();

	// in the error case, use the default value
	FConfigSection* AgeRestrictionConfigs = GConfig->GetSectionPrivate(TEXT("PS4Application"), false, true, GEngineIni);
	if (AgeRestrictionConfigs != nullptr)
	{
		for (FConfigSection::TIterator It(*AgeRestrictionConfigs); It; ++It)
		{
			if (It.Key() == TEXT("CountryAgeRestrictions"))
			{
				FCountryAgeRestriction CountryAgeRestriction;
				if (CountryAgeRestriction.ParseConfig(*It.Value().GetValue()))
				{
					CountryAgeRestrictions.Add(CountryAgeRestriction);
				}
			}
		}
	}

	int32 DefaultAgeRestriction = 0;
	if (GConfig->GetInt(TEXT("PS4Application"), TEXT("DefaultAgeRestriction"), DefaultAgeRestriction, GEngineIni) || CountryAgeRestrictions.Num() > 0)
	{
		SceNpContentRestriction ContentRestriction;
		FMemory::Memset(ContentRestriction, 0x00);
		ContentRestriction.size = sizeof(SceNpContentRestriction);
		ContentRestriction.defaultAgeRestriction = DefaultAgeRestriction;
		ContentRestriction.ageRestrictionCount = CountryAgeRestrictions.Num();
		SceNpAgeRestriction* ageRestriction = CountryAgeRestrictions.Num() > 0 ? new SceNpAgeRestriction[CountryAgeRestrictions.Num()] : nullptr;
		for (int Idx = 0; Idx < CountryAgeRestrictions.Num(); Idx++)
		{
			ageRestriction[Idx].age = CountryAgeRestrictions[Idx].Age;
			FMemory::Memset(ageRestriction[Idx].countryCode, 0x00);
			FMemory::Memcpy(&ageRestriction[Idx].countryCode.data, TCHAR_TO_ANSI(*CountryAgeRestrictions[Idx].Country), SCE_NP_COUNTRY_CODE_LENGTH);
		}
		ContentRestriction.ageRestriction = ageRestriction;
		int32 Ret = sceNpSetContentRestriction(&ContentRestriction);
		if (ageRestriction != nullptr)
		{
			delete[] ageRestriction;
		}
		check(Ret == SCE_OK);
	}
}

void FPS4Application::Tick(const float TimeDelta)
{
	FSonyApplication::Tick(TimeDelta);

	if (CachedPlayTogetherHostEventParam.IsSet())
	{
		const SceNpPlayTogetherHostEventParamA& CachedValue = CachedPlayTogetherHostEventParam.GetValue();
		if (CachedValue.inviteeListLen > 0 && OnPlayTogetherSystemServiceEventDelegate.IsBound())
		{
			OnPlayTogetherSystemServiceEventDelegate.Broadcast(CachedValue);
			CachedPlayTogetherHostEventParam.Reset();
			// CachedValue is now dangling; do not use
		}
	}

	if (CachedTournamentJoinEventParam.IsSet() && OnTournamentJoinEventDelegate.IsBound())
	{
		OnTournamentJoinEventDelegate.Broadcast(CachedTournamentJoinEventParam.GetValue());
		CachedTournamentJoinEventParam.Reset();
	}
	if (CachedTournamentJoinMatchEventParam.IsSet() && OnTournamentJoinMatchEventDelegate.IsBound())
	{
		OnTournamentJoinMatchEventDelegate.Broadcast(CachedTournamentJoinMatchEventParam.GetValue());
		CachedTournamentJoinMatchEventParam.Reset();
	}
	if (CachedTournamentJoinTeamOnTeamMatchEventParam.IsSet() && OnTournamentJoinTeamMatchEventDelegate.IsBound())
	{
		OnTournamentJoinTeamMatchEventDelegate.Broadcast(CachedTournamentJoinTeamOnTeamMatchEventParam.GetValue());
		CachedTournamentJoinTeamOnTeamMatchEventParam.Reset();
	}
	if (!GetProtocolActivationUri().IsEmpty() && OnActivatedByProtocol().IsBound())
	{
		OnActivatedByProtocol().Broadcast(GetProtocolActivationUri());
		SetProtocolActivationUri(FString());
	}
}

FOnPlayGoLocusUpdateDelegate FPS4Application::OnPlayGoLocusUpdateDelegate;

bool FPS4Application::HandleSystemServiceEvent(SceSystemServiceEvent& Event)
{
	switch (Event.eventType)
	{
	default:
		return false;

		case SCE_SYSTEM_SERVICE_EVENT_DISPLAY_SAFE_AREA_UPDATE:
		{
			FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
			break;
		}

		case SCE_SYSTEM_SERVICE_EVENT_SESSION_INVITATION:
		{
			SceNpSessionInvitationEventParam& param = reinterpret_cast<SceNpSessionInvitationEventParam&>(Event.data.param);
			const FString SessionId(ANSI_TO_TCHAR(param.sessionId.data));
			FCoreDelegates::OnInviteAccepted.Broadcast(FString::Printf(TEXT("%d"), param.userId), SessionId);
			UE_LOG(LogSony, Log, TEXT("Invite accepted with session id %s"), *SessionId);
			break;
		}
		case SCE_SYSTEM_SERVICE_EVENT_RESET_VR_POSITION:
			if (FCoreDelegates::VRHeadsetRecenter.IsBound())
			{
				FCoreDelegates::VRHeadsetRecenter.Broadcast();
			}
			else
			{
				// If nothing specific has been bound we will just recenter as specified in TRC4181
				OnVRHeadsetRecenterDefaultDelegate.Broadcast();
			}
			break;
		case SCE_SYSTEM_SERVICE_EVENT_PLAY_TOGETHER_HOST_A:
		{
			const SceNpPlayTogetherHostEventParamA& Param = reinterpret_cast<SceNpPlayTogetherHostEventParamA&>(Event.data.param);
			UE_LOG(LogSony, Log, TEXT("Play together event received for %d party members."), Param.inviteeListLen);

			if (OnPlayTogetherSystemServiceEventDelegate.IsBound())
			{
				OnPlayTogetherSystemServiceEventDelegate.Broadcast(Param);
			}
			else
			{
				CachedPlayTogetherHostEventParam = Param;
			}
			break;
		}
		case SCE_SYSTEM_SERVICE_EVENT_JOIN_EVENT:
		{
			const SceNpEventJoinEventParam& Param = reinterpret_cast<const SceNpEventJoinEventParam&>(Event.data.param);
			UE_LOG(LogSony, Log, TEXT("JoinEvent event received."));

			if (OnTournamentJoinEventDelegate.IsBound())
			{
				OnTournamentJoinEventDelegate.Broadcast(Param);
			}
			else
			{
				CachedTournamentJoinEventParam = Param;
			}
			break;
		}
		case SCE_SYSTEM_SERVICE_EVENT_JOIN_MATCH_EVENT:
		{
			const SceNpEventJoinMatchEventParam& Param = reinterpret_cast<const SceNpEventJoinMatchEventParam&>(Event.data.param);
			UE_LOG(LogSony, Log, TEXT("JoinMatchEvent event received."));

			if (OnTournamentJoinMatchEventDelegate.IsBound())
			{
				OnTournamentJoinMatchEventDelegate.Broadcast(Param);
			}
			else
			{
				CachedTournamentJoinMatchEventParam = Param;
			}
			break;
		}
		case SCE_SYSTEM_SERVICE_EVENT_JOIN_TEAM_ON_TEAM_MATCH_EVENT:
		{
			const SceNpEventJoinTeamOnTeamMatchEventParam& Param = reinterpret_cast<const SceNpEventJoinTeamOnTeamMatchEventParam&>(Event.data.param);
			UE_LOG(LogSony, Log, TEXT("JoinMatchEvent event received."));

			if (OnTournamentJoinTeamMatchEventDelegate.IsBound())
			{
				OnTournamentJoinTeamMatchEventDelegate.Broadcast(Param);
			}
			else
			{
				CachedTournamentJoinTeamOnTeamMatchEventParam = Param;
			}
			break;
		}
		case SCE_SYSTEM_SERVICE_EVENT_LAUNCH_APP:
		{
			UE_LOG(LogSony, Log, TEXT("LaunchApp event received."));
			const FString LaunchAppParam = FString(reinterpret_cast<const ANSICHAR*>(Event.data.launchApp.arg));

			if (FPS4Application::GetPS4Application()->OnActivatedByProtocol().IsBound())
			{
				FPS4Application::GetPS4Application()->OnActivatedByProtocol().Broadcast(LaunchAppParam);
			}
			else
			{
				FPS4Application::GetPS4Application()->SetProtocolActivationUri(LaunchAppParam);
			}
			break;
		}
		case SCE_SYSTEM_SERVICE_EVENT_PLAYGO_LOCUS_UPDATE:
		{
			UE_LOG(LogSony, Log, TEXT("PlayGoLocusUpdate event received."));
			OnPlayGoLocusUpdateDelegate.Broadcast();
			break;
		}
	}

	return true;
}

FPlatformRect GetWorkAreaInternal(const FPlatformRect& CurrentWindow)
{
	//@todo ps4: Use the actual device settings here.
	static bool bDetectedResolution = false;
	static FPlatformRect WorkArea;

	if (bDetectedResolution == false)
	{
		WorkArea.Left = 0;
		WorkArea.Top = 0;
		WorkArea.Right = 1920;
		WorkArea.Bottom = 1080;

		if (GPS4Allow4kOutput)
		{
			SceVideoOutResolutionStatus ResolutionStatus;
			int Handle = sceVideoOutOpen(SCE_USER_SERVICE_USER_ID_SYSTEM, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
			if (Handle > 0)
			{
				if (SCE_OK == sceVideoOutGetResolutionStatus(Handle, &ResolutionStatus) && ResolutionStatus.fullHeight > 1080)
				{
					// 4k Mode
					WorkArea.Right = 3840;
					WorkArea.Bottom = 2160;
				}
				sceVideoOutClose(Handle);
			}
		}

		WorkArea.Right = FMath::Min(WorkArea.Right, GPS4MaxBackbufferWidth);
		WorkArea.Bottom = FMath::Min(WorkArea.Bottom, GPS4MaxBackbufferHeight);

		bDetectedResolution = true;
	}

	return WorkArea;
}

FPlatformRect FPS4Application::GetWorkArea(const FPlatformRect& CurrentWindow) const
{
	return GetWorkAreaInternal(CurrentWindow);
}

void FDisplayMetrics::RebuildDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect =
		GetWorkAreaInternal(OutDisplayMetrics.PrimaryDisplayWorkAreaRect);

	OutDisplayMetrics.PrimaryDisplayWidth = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right;
	OutDisplayMetrics.PrimaryDisplayHeight = OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom;

	// calculate the size the safe area - on PS4 there's only one safe zone, so we use it
	// for TitleSafe and ActionSafe

	// @todo: SCE_SYSTEM_SERVICE_EVENT_DISPLAY_SAFE_AREA_UPDATE isn't currently being sent by OS (1.510), so we manually check it
	// use a local variable since we can't update SafeAreaPercentage in this const function
	float LocalSafePercent = FPS4Application::DefaultSafeAreaPercentage;
	SceSystemServiceDisplaySafeAreaInfo Info;
	if (sceSystemServiceGetDisplaySafeAreaInfo(&Info) == SCE_OK)
	{
		LocalSafePercent = Info.ratio;
	}

	float SafePaddingX = OutDisplayMetrics.PrimaryDisplayWidth * ((1.0f - LocalSafePercent) * 0.5f);
	float SafePaddingY = OutDisplayMetrics.PrimaryDisplayHeight * ((1.0f - LocalSafePercent) * 0.5f);

	OutDisplayMetrics.TitleSafePaddingSize = OutDisplayMetrics.ActionSafePaddingSize =
		FVector4(SafePaddingX, SafePaddingY, SafePaddingX, SafePaddingY);
}

void FPS4Application::SetProtocolActivationUri(const FString& NewUriString)
{
	ProtocolActivationUri = NewUriString;
}

const FString& FPS4Application::GetProtocolActivationUri() const
{
	return ProtocolActivationUri;
}
