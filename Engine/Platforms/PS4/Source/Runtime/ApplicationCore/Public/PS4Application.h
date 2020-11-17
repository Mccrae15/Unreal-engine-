// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SonyApplication.h"
#include <np/np_play_together.h>
#include <np/np_event.h>

/** Delegate for when we receive a play together system event */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayTogetherSystemServiceEventDelegate, const SceNpPlayTogetherHostEventParamA&);

/** Delegates for when we receive Join Match tournament system events */
DECLARE_EVENT_OneParam(FPS4Application, FOnPS4JoinEventDelegate, const SceNpEventJoinEventParam& /*Param*/);
DECLARE_EVENT_OneParam(FPS4Application, FOnPS4JoinMatchEventDelegate, const SceNpEventJoinMatchEventParam& /*Param*/);
DECLARE_EVENT_OneParam(FPS4Application, FOnPS4JoinTeamMatchEventDelegate, const SceNpEventJoinTeamOnTeamMatchEventParam& /*Param*/);

DECLARE_MULTICAST_DELEGATE(FOnVRHeadsetRecenterDefaultDelegate);

DECLARE_MULTICAST_DELEGATE(FOnPlayGoLocusUpdateDelegate);

/**
 * Delegate broadcast when the application has been activated by protocol
 * @param ProtocolUri the protocol uri that the application has been activated with, also obtained from FPS4Application::GetProtocolActivationUri
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActivatedByProtocol, FString);

class FPS4Application : public FSonyApplication
{
public:
	/** Explicit allocation of the single possible FPS4Application. */
	static FPS4Application* CreatePS4Application();

	/** Gets the FPS4Application. Must be explicitly initialized already. */
	static inline FPS4Application* GetPS4Application() { return static_cast<FPS4Application*>(FSonyApplication::GetSonyApplication()); }

	FOnPlayTogetherSystemServiceEventDelegate OnPlayTogetherSystemServiceEventDelegate;
	TOptional<SceNpPlayTogetherHostEventParamA> CachedPlayTogetherHostEventParam;

	FOnVRHeadsetRecenterDefaultDelegate OnVRHeadsetRecenterDefaultDelegate;

	static FOnPlayGoLocusUpdateDelegate OnPlayGoLocusUpdateDelegate;

	FOnPS4JoinEventDelegate OnTournamentJoinEventDelegate;
	FOnPS4JoinMatchEventDelegate OnTournamentJoinMatchEventDelegate;
	FOnPS4JoinTeamMatchEventDelegate OnTournamentJoinTeamMatchEventDelegate;

	virtual bool HandleSystemServiceEvent(SceSystemServiceEvent& Event) override final;

	virtual void Tick(const float TimeDelta) override final;

	virtual FPlatformRect GetWorkArea(const FPlatformRect& CurrentWindow) const override;

	FOnActivatedByProtocol& OnActivatedByProtocol() { return ActivatedByProtocolDelegates; }

	/**
	 * Function to save the URI string from a protocol activation for use elsewhere in the game.
	 */
	void SetProtocolActivationUri(const FString& NewUriString);

	/**
	 * Function to get the URI string from the most recent protocol activation.
	 */
	const FString& GetProtocolActivationUri() const;

private:
	FPS4Application();

	TOptional<SceNpEventJoinEventParam> CachedTournamentJoinEventParam;
	TOptional<SceNpEventJoinMatchEventParam> CachedTournamentJoinMatchEventParam;
	TOptional<SceNpEventJoinTeamOnTeamMatchEventParam> CachedTournamentJoinTeamOnTeamMatchEventParam;

	/**
	 * Country code with age restriction
	 */
	struct FCountryAgeRestriction
	{
		FCountryAgeRestriction(const FString& InCountry = FString(), int32 InAge = 0)
			: Country(InCountry)
			, Age(InAge)
		{}

		/** parse from ini string */
		bool ParseConfig(const FString& ConfigStr)
		{
			return (FParse::Value(*ConfigStr, TEXT("Country="), Country) &&
				FParse::Value(*ConfigStr, TEXT("Age="), Age));
		}

		/** Country code */
		FString Country;
		/** Min age required */
		int32 Age;
	};
	/** List of countries with age restrictions */
	TArray<FCountryAgeRestriction> CountryAgeRestrictions;

	FOnActivatedByProtocol ActivatedByProtocolDelegates;

	/** character buffer containing the last protocol activation URI */
	FString ProtocolActivationUri;

};
