// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineExternalUIInterfacePS4.h"
#include "OnlineSessionInterfacePS4.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/ConfigCacheIni.h"

#include <np_profile_dialog.h>
#include <web_browser_dialog.h>
#include <invitation_dialog.h>
#include <np_commerce_dialog.h>
#include <error_dialog.h>
#include <message_dialog.h>
#include <game_custom_data_dialog.h>

// Comment out this code to disable automatic creation of a test session for sending invites
//#define EXTERNAL_UI_CREATE_TEST_SESSION

#ifdef EXTERNAL_UI_CREATE_TEST_SESSION
FString CreateWebApiSessionForTesting(FOnlineSubsystemPS4* PS4Subsystem)
{
	// Note that his function, intended for testing only, only creates a session
	// In a real system, we'd have similar methods for joining, leaving, and destroying a session

	// This is a blocking function only for testing. Ideally, it would wrapped into a task on another thread.
	// Creates a game session for testing the invite dialog

	FString SessionName;


	// Before we get started, want to make sure have an image to attach to the session creation
	// The image must be a jpeg, cannot exceed 160kb, and is recommended to be 457x257
	TArray<uint8> ImageData;
	{
		IFileHandle* ImageFileHandle = nullptr;
		FString ImageFullPath = FPaths::ProjectContentDir() + TEXT("OSS/PS4/Invites/Testing.jpg");
		ImageFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*ImageFullPath);
		if (ImageFileHandle)
		{
			ImageData.AddZeroed(ImageFileHandle->Size());
			if (!ImageFileHandle->Read(ImageData.GetData(), ImageData.Num()))
			{
				ImageData.Empty();
			}
			delete ImageFileHandle;
		}
	}
	if (ImageData.Num() == 0)
	{
		return SessionName;
	}

	// Create a multi-part request for creating a session
	// There are 3 parts to send: The json description of the session request, the image data for the session, and the app-specific session data
	IOnlineIdentityPtr Identity = PS4Subsystem->GetIdentityInterface();	
	int32 WebApiContext = PS4Subsystem->GetUserWebApiContext(FUniqueNetIdPS4::Cast(Identity->GetUniquePlayerId(0)));

	if (WebApiContext == -1)
	{
		return SessionName;
	}

	int32 ResultCode = 0;
	int64_t WebRequestId = 0;
	
	// Create the request
	ResultCode = sceNpWebApiCreateMultipartRequest(WebApiContext, "sessionInvitation", "/v1/sessions", SCE_NP_WEBAPI_HTTP_METHOD_POST, &WebRequestId);
	if (ResultCode < 0)
	{
		return SessionName;
	}

	// Part 1: Session Request
	int32 SessionRequestIndex = 0;

	// Hardcoded session description for testing
	const char* SessionRequestJson =
		"{\r\n"
		" \"sessionType\":\"owner-migration\",\r\n"
		" \"sessionMaxUser\":16,\r\n"
		" \"sessionName\":\"Testing Death Match!\",\r\n"
		" \"sessionStatus\":\"stage 1. beginner only\",\r\n"
		" \"availablePlatforms\":[\"PS4\"]\r\n"
		"}\r\n";
	{
		SceNpWebApiHttpHeader Headers[2];
		SceNpWebApiMultipartPartParameter PartParam;
 
		FMemory::Memzero(&Headers, sizeof(Headers));
		Headers[0].pName = const_cast<char *>("Content-Type");
		Headers[0].pValue = const_cast<char *>("application/json; charset=utf-8");
		Headers[1].pName = const_cast<char *>("Content-Description");
		Headers[1].pValue = const_cast<char *>("session-request");
 
		FMemory::Memzero(&PartParam, sizeof(PartParam));
		PartParam.pHeaders = Headers;
		PartParam.headerNum = 2;
		PartParam.contentLength = FCStringAnsi::Strlen(SessionRequestJson);
 
		ResultCode = sceNpWebApiAddMultipartPart(WebRequestId, &PartParam, &SessionRequestIndex);
		if (ResultCode < 0)
		{
			sceNpWebApiDeleteRequest(WebRequestId);
			return SessionName;
		}
	}

	// Part 2: Session Image
	int32 SessionImageIndex = 0;
	{
		SceNpWebApiHttpHeader Headers[3];
		SceNpWebApiMultipartPartParameter PartParam;
 
		FMemory::Memzero(&Headers, sizeof(Headers));
		Headers[0].pName = const_cast<char *>("Content-Type");
		Headers[0].pValue = const_cast<char *>("image/jpeg");
		Headers[1].pName = const_cast<char *>("Content-Description");
		Headers[1].pValue = const_cast<char *>("session-image");
		Headers[2].pName = const_cast<char *>("Content-Disposition");
		Headers[2].pValue = const_cast<char *>("attachment");
 
		FMemory::Memzero(&PartParam, sizeof(PartParam));
		PartParam.pHeaders = Headers;
		PartParam.headerNum = 3;
		PartParam.contentLength = ImageData.Num();
 
		ResultCode = sceNpWebApiAddMultipartPart(WebRequestId, &PartParam, &SessionImageIndex);
		if (ResultCode < 0)
		{
			sceNpWebApiDeleteRequest(WebRequestId);
			return SessionName;
		}
	}

	// Part 3: Session Data (faked for testing)
	int32 SessionDataIndex = 0;

	// Hardcoded 'data' for session
	const char* SessionData = "Fake Session Test Data";
	{
		SceNpWebApiHttpHeader Headers[3];
		SceNpWebApiMultipartPartParameter PartParam;
 
		FMemory::Memzero(&Headers, sizeof(Headers));
		Headers[0].pName = const_cast<char *>("Content-Type");
		Headers[0].pValue = const_cast<char *>("application/octet-stream");
		Headers[1].pName = const_cast<char *>("Content-Description");
		Headers[1].pValue = const_cast<char *>("session-data");
		Headers[2].pName = const_cast<char *>("Content-Disposition");
		Headers[2].pValue = const_cast<char *>("attachment");
 
		FMemory::Memzero(&PartParam, sizeof(PartParam));     
		PartParam.pHeaders = Headers;
		PartParam.headerNum = 3;
		PartParam.contentLength = FCStringAnsi::Strlen(SessionData);
 
		ResultCode = sceNpWebApiAddMultipartPart(WebRequestId, &PartParam, &SessionDataIndex);
		if (ResultCode < 0)
		{
			sceNpWebApiDeleteRequest(WebRequestId);
			return SessionName;
		}
	}

	SceNpWebApiResponseInformationOption ResponseInformationOption;
	FMemory::Memzero(ResponseInformationOption);

	// All the headers are ready so we can send the data
	// Part 1	
	ResultCode = sceNpWebApiSendMultipartRequest2(WebRequestId, SessionRequestIndex, SessionRequestJson, FCStringAnsi::Strlen(SessionRequestJson, &ResponseInformationOption));
	if (ResultCode < 0)
	{
		sceNpWebApiDeleteRequest(WebRequestId);
		return SessionName;
	}

	// Part 2
	ResultCode = sceNpWebApiSendMultipartRequest2(WebRequestId, SessionImageIndex, ImageData.GetData(), ImageData.Num(), &ResponseInformationOption);
	if (ResultCode < 0)
	{
		sceNpWebApiDeleteRequest(WebRequestId);
		return SessionName;
	}

	// Part3
	ResultCode = sceNpWebApiSendMultipartRequest2(WebRequestId, SessionDataIndex, SessionData, FCStringAnsi::Strlen(SessionData), &ResponseInformationOption);
	if (ResultCode < 0)
	{
		sceNpWebApiDeleteRequest(WebRequestId);
		return SessionName;
	}

	// All 3 parts have been sent so we should have a status code now
	int32_t HttpStatusCode = 0;
	ResultCode = sceNpWebApiGetHttpStatusCode(WebRequestId, &HttpStatusCode);
	if (ResultCode < 0)
	{
		sceNpWebApiDeleteRequest(WebRequestId);
		return SessionName;
	}

	if (HttpStatusCode == 200)
	{
		// Successfully created a session
		// We can read the data to get the session Id
		TArray<uint8> ResponseData;
		uint8 Buffer[1024];
		do
		{
			FMemory::Memset(Buffer, 0, sizeof(Buffer));
			ResultCode = sceNpWebApiReadData(WebRequestId, Buffer, sizeof(Buffer));
			if (ResultCode < 0)
			{
				// Error
				break;
			}

			// Add to our response data
			ResponseData.Append(Buffer, ResultCode);

		} while (ResultCode > 0);

		// The ResponseData contains Json of the form {"sessionId":"xxx-some-hex-numbers-xxx"}
		UE_LOG_ONLINE(Log, TEXT("Create session response body:\n%s"), UTF8_TO_TCHAR(ResponseData.GetData()));

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(UTF8_TO_TCHAR(ResponseData.GetData()));
		if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			SessionName = JsonObject->GetStringField("sessionId");
		}
	}

	sceNpWebApiDeleteRequest(WebRequestId);
	return SessionName;
}
#endif // EXTERNAL_UI_CREATE_TEST_SESSION


// FOnlineAsyncTaskPS4UpdateProfileUI

class FOnlineAsyncTaskPS4UpdateProfileUI : public FOnlineAsyncTaskPS4
{
public:

	/** Constructor */
	FOnlineAsyncTaskPS4UpdateProfileUI(FOnlineSubsystemPS4* InSubsystem, const FOnProfileUIClosedDelegate& InProfileUIClosedDelegate)
		:	FOnlineAsyncTaskPS4(InSubsystem)
		,	ProfileUIClosedDelegate(InProfileUIClosedDelegate)
	{
	}

	FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4UpdateProfileUI bWasSuccessful: %d"), bWasSuccessful);
	}

	virtual void Tick() override
	{
		SceCommonDialogStatus Status = sceNpProfileDialogUpdateStatus();
		if (Status == SCE_COMMON_DIALOG_STATUS_FINISHED)
		{
			bIsComplete = true;
			bWasSuccessful = true;
		}
		else if (Status == SCE_COMMON_DIALOG_STATUS_NONE)
		{
			bIsComplete = true;
			bWasSuccessful = false;
		}
	}

	virtual void Finalize() override
	{
		sceNpProfileDialogTerminate();
	}

	virtual void TriggerDelegates() override
	{
		FOnlineAsyncTaskPS4::TriggerDelegates();

		// Tell delegates we are closing the profile UI and external UI
		ProfileUIClosedDelegate.ExecuteIfBound();
		Subsystem->GetExternalUIInterface()->TriggerOnExternalUIChangeDelegates(false);
	}

private:

	FOnProfileUIClosedDelegate ProfileUIClosedDelegate;
};

// FOnlineAsyncTaskPS4UpdateWebBrowser

class FOnlineAsyncTaskPS4UpdateWebBrowserDialog : public FOnlineAsyncTaskPS4
{
public:

	/** Constructor */
	explicit FOnlineAsyncTaskPS4UpdateWebBrowserDialog(FOnlineSubsystemPS4* InSubsystem, const FOnShowWebUrlClosedDelegate& InClosedDelegate)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, ClosedDelegate(InClosedDelegate)
	{
	}

	FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4UpdateWebBrowserDialog bWasSuccessful: %d"), bWasSuccessful);
	}

	virtual void Tick() override
	{
		SceCommonDialogStatus Status = sceWebBrowserDialogUpdateStatus();
		if (Status == SCE_COMMON_DIALOG_STATUS_FINISHED)
		{
			SceWebBrowserDialogResult Result;
			memset(&Result, 0x00, sizeof(Result));

			SceWebBrowserDialogCallbackResultParam	CallbackResult;
			memset(&CallbackResult, 0, sizeof(CallbackResult));
			CallbackResult.size = sizeof(SceWebBrowserDialogCallbackResultParam);
			Result.callbackResultParam = &CallbackResult;

			int32 ReturnCode = sceWebBrowserDialogGetResult(&Result);
			if (Result.callbackResultParam->size > 0 &&
				ReturnCode == SCE_OK)
			{
				FinalUrl = UTF8_TO_TCHAR(Result.callbackResultParam->data);
			}
			else if (Result.callbackResultParam->bufferSize > 0)
			{
				// buffer had enough space for 2048 chars
				if (ReturnCode == SCE_OK)
				{
					FinalUrl = UTF8_TO_TCHAR(Result.callbackResultParam->buffer);
				}
				// buffer was too small but callback gives us the appropriate size
				else if (ReturnCode == SCE_COMMON_DIALOG_ERROR_PARAM_INVALID)
				{
					// resize buffer
					Result.callbackResultParam->buffer = new char[Result.callbackResultParam->bufferSize];
					// try again with sized buffer
					ReturnCode = sceWebBrowserDialogGetResult(&Result);
					if (ReturnCode == SCE_OK)
					{
						FinalUrl = UTF8_TO_TCHAR(Result.callbackResultParam->buffer);
					}
					delete[] Result.callbackResultParam->buffer;
					Result.callbackResultParam->buffer = nullptr;
				}
			}

			bIsComplete = true;
			bWasSuccessful = true;
		}
		else if (Status == SCE_COMMON_DIALOG_STATUS_NONE)
		{
			bIsComplete = true;
			bWasSuccessful = false;
		}
	}

	virtual void Finalize() override
	{
		sceWebBrowserDialogTerminate();
	}

	virtual void TriggerDelegates() override
	{
		FOnlineAsyncTaskPS4::TriggerDelegates();

		// Tell delegates we are closing the external UI
		ClosedDelegate.ExecuteIfBound(FinalUrl);
		Subsystem->GetExternalUIInterface()->TriggerOnExternalUIChangeDelegates(false);
	}

	/** The resulting Url after invoking the web browser */
	FString FinalUrl;
	/** Closed UI completion callback */
	FOnShowWebUrlClosedDelegate ClosedDelegate;
};


// FOnlineAsyncTaskPS4UpdateInviteUI

class FOnlineAsyncTaskPS4UpdateInviteUI : public FOnlineAsyncTaskPS4
{
public:

	/** Constructor */
	explicit FOnlineAsyncTaskPS4UpdateInviteUI(FOnlineSubsystemPS4* InSubsystem)
		:	FOnlineAsyncTaskPS4(InSubsystem)
	{
	}

	FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4UpdateInviteUI bWasSuccessful: %d"), bWasSuccessful);
	}

	virtual void Tick() override
	{
		SceCommonDialogStatus Status = sceInvitationDialogUpdateStatus();
		if (Status == SCE_COMMON_DIALOG_STATUS_FINISHED)
		{
			bIsComplete = true;
			bWasSuccessful = true;
		}
		else if (Status == SCE_COMMON_DIALOG_STATUS_NONE)
		{
			bIsComplete = true;
			bWasSuccessful = false;
		}
	}

	virtual void Finalize() override
	{
		sceInvitationDialogTerminate();
	}

	virtual void TriggerDelegates() override
	{
		FOnlineAsyncTaskPS4::TriggerDelegates();

		// Tell delegates we are closing the external UI
		Subsystem->GetExternalUIInterface()->TriggerOnExternalUIChangeDelegates(false);
	}
};


// FOnlineExternalUIPS4


/** Load the PlayStation store's default position if one is set.  Default to Left if not set (previous default before this option) */
static int32 GetSonyStoreIconPosition()
{
	FString StoreIconString;

	const bool bStorePositionSet = GConfig->GetString(TEXT("OnlineSubsystemPS4"), TEXT("StoreIconPosition"), StoreIconString, GEngineIni);
	if (StoreIconString.Equals(TEXT("Left"), ESearchCase::IgnoreCase))
	{
		return static_cast<int32>(SCE_NP_COMMERCE_PS_STORE_ICON_LEFT);
	}
	else if (StoreIconString.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
	{
		return static_cast<int32>(SCE_NP_COMMERCE_PS_STORE_ICON_CENTER);
	}
	else if (StoreIconString.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
	{
		return static_cast<int32>(SCE_NP_COMMERCE_PS_STORE_ICON_RIGHT);
	}

	if (bStorePositionSet)
	{
		// Only warn if this is set, and was an unknown position
		UE_LOG_ONLINE(Warning, TEXT("Unexpected PlayStation Store Icon position of %s, defaulting to Left"), *StoreIconString);
	}

	return static_cast<int32>(SCE_NP_COMMERCE_PS_STORE_ICON_LEFT);
}

FOnlineExternalUIPS4::FOnlineExternalUIPS4(FOnlineSubsystemPS4* InPS4Subsystem)
	: PS4Subsystem(InPS4Subsystem)
	, bLaunchedPSPlusDialog(false)
	, LoginUIControllerIndex(-1)
	, StoreIconPosition(GetSonyStoreIconPosition())
{
	sceSysmoduleLoadModule(SCE_SYSMODULE_NP_PROFILE_DIALOG);
	sceSysmoduleLoadModule(SCE_SYSMODULE_WEB_BROWSER_DIALOG);
	sceSysmoduleLoadModule(SCE_SYSMODULE_INVITATION_DIALOG);
	sceSysmoduleLoadModule(SCE_SYSMODULE_ERROR_DIALOG);
	sceSysmoduleLoadModule(SCE_SYSMODULE_GAME_CUSTOM_DATA_DIALOG);

	int32 ReturnCode = sceCommonDialogInitialize();
	if (ReturnCode < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("sceCommonDialogInitialize error = 0x%08x"), ReturnCode);
	}
}

FOnlineExternalUIPS4::~FOnlineExternalUIPS4()
{
	sceSysmoduleUnloadModule(SCE_SYSMODULE_GAME_CUSTOM_DATA_DIALOG);
	sceSysmoduleUnloadModule(SCE_SYSMODULE_ERROR_DIALOG);
	sceSysmoduleUnloadModule(SCE_SYSMODULE_INVITATION_DIALOG);
	sceSysmoduleUnloadModule(SCE_SYSMODULE_WEB_BROWSER_DIALOG);
	sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_PROFILE_DIALOG);
}

bool FOnlineExternalUIPS4::ShowLoginUI(const int ControllerIndex, bool bShowOnlineOnly, bool bShowSkipButton, const FOnLoginUIClosedDelegate& Delegate)
{
	int32 ReturnCode = sceErrorDialogInitialize();
	if (ReturnCode < SCE_OK)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceErrorDialogInitialize() failed with error code 0x%x"), ReturnCode);

		TSharedPtr<const FUniqueNetId> UserId = PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(ControllerIndex);
		Delegate.ExecuteIfBound(UserId, ControllerIndex);
		return false;
	}

	SceErrorDialogParam Param;
	sceErrorDialogParamInitialize(&Param);
	Param.errorCode = SCE_NP_ERROR_SIGNED_OUT;
	ReturnCode = sceUserServiceGetInitialUser(&Param.userId);
	if (ReturnCode < SCE_OK)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceUserServiceGetInitialUser() failed with error code 0x%x"), ReturnCode);

		TSharedPtr<const FUniqueNetId> UserId = PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(ControllerIndex);
		Delegate.ExecuteIfBound(UserId, ControllerIndex);
		return false;
	}

	ReturnCode = sceErrorDialogOpen(&Param);
	if (ReturnCode < SCE_OK)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceUserServiceGetInitialUser() failed with error code 0x%x"), ReturnCode);

		TSharedPtr<const FUniqueNetId> UserId = PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(ControllerIndex);
		Delegate.ExecuteIfBound(UserId, ControllerIndex);
		return false;
	}

	LoginUIControllerIndex = ControllerIndex;
	OnLoginUIClosedDelegate = Delegate;
	return true;
}

bool FOnlineExternalUIPS4::ShowFriendsUI(int32 LocalUserNum)
{
	// Note: The NpFriendDialog is for selecting a single friend (that the application should consume in some way) and is not a good fit here.
	UE_LOG_ONLINE(Warning, TEXT("ShowFriendsUI not supported on PS4"));
	return false;
}

bool FOnlineExternalUIPS4::ShowInviteUI(int32 LocalUserNum, FName SessionName)
{
	// For testing purposes, we allow the invite dialog to come up even if there isn't a session in place
#ifndef EXTERNAL_UI_CREATE_TEST_SESSION
	IOnlineSessionPtr SessionInt = PS4Subsystem->GetSessionInterface();
	if (SessionInt.IsValid() == false || SessionInt->HasPresenceSession() == false)
	{
		UE_LOG_ONLINE(Warning, TEXT("ShowInviteUI failed because there is no session to invite to."));
		return false;
	}
#endif

	bool bShowedInviteUI = false;

	int32 ReturnCode = sceInvitationDialogInitialize();
	if (ReturnCode < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("sceInvitationDialogInitialize error = 0x%08x"), ReturnCode);
		bShowedInviteUI = false;
	}
	else
	{
		TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum));
		if (!LocalUserId.IsValid())
		{
			UE_LOG_ONLINE(Error, TEXT("ShowInviteUI failed. No local user id for local user %d"), LocalUserNum);
			bShowedInviteUI = false;
		}
		else
		{
			// @todo: SessionNameId has to come from the WebApi, which is not hooked into the PS4 session interface yet
			// We have testing code in place that creates such a WebApi session to send invites from for now
#ifdef EXTERNAL_UI_CREATE_TEST_SESSION
			FString SessionNameId = CreateWebApiSessionForTesting(PS4Subsystem);
#else
			FOnlineSessionPS4Ptr PS4Session = StaticCastSharedPtr<FOnlineSessionPS4>(PS4Subsystem->GetSessionInterface());
			FNamedOnlineSession * Session = PS4Session->GetNamedSession(SessionName);
			if(!Session)
			{
				return false;
			}
			TSharedPtr<FOnlineSessionInfoPS4> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoPS4>(Session->SessionInfo);
			if(!SessionInfo.IsValid())
			{
				return false;
			}
			FString SessionNameId = SessionInfo->SessionId.ToString();
#endif
			// Set up our invitation dialog parameters
			SceInvitationDialogParamA DialogParam;
			sceInvitationDialogParamInitializeA(&DialogParam);
			DialogParam.mode = SCE_INVITATION_DIALOG_MODE_SEND;
			DialogParam.userId = LocalUserId->GetUserId();

			SceInvitationDialogDataParamA DataParam;
			memset(&DataParam, 0x00, sizeof(DataParam));
			DataParam.SendInfo.addressParam.addressType = SCE_INVITATION_DIALOG_ADDRESS_TYPE_USERENABLE;

			// Always invite up to the max session count, minus 1 since we are already in the session.
			DataParam.SendInfo.addressParam.addressInfo.UserSelectEnableAddress.userMaxCount = Session->SessionSettings.NumPrivateConnections + Session->SessionSettings.NumPublicConnections - 1;

			// @todo: We can put a custom invite message here. That should come from session data somehow.
			DataParam.SendInfo.userMessage = "";

			SceNpSessionId SessionId;
			FCStringAnsi::Strncpy(SessionId.data, TCHAR_TO_ANSI(*SessionNameId), SCE_NP_SESSION_ID_MAX_SIZE);
			DataParam.SendInfo.sessionId = &SessionId;

			DialogParam.dataParam = &DataParam;

			// Open the invitation dialog
			ReturnCode = sceInvitationDialogOpenA(&DialogParam);
			if (ReturnCode < 0)
			{
				UE_LOG_ONLINE(Error, TEXT("sceInvitationDialogOpenA error = 0x%08x"), ReturnCode);
				bShowedInviteUI = false;
			}
			else
			{
				bShowedInviteUI = true;
			}
		}
	}

	if (bShowedInviteUI)
	{
		// Trigger to let us know if the external UI has been made active
		PS4Subsystem->GetExternalUIInterface()->TriggerOnExternalUIChangeDelegates(true);

		// Async task to update the invitation dialog and trigger its closure
		FOnlineAsyncTaskPS4UpdateInviteUI* NewUpdateInviteUI= new FOnlineAsyncTaskPS4UpdateInviteUI(PS4Subsystem);
		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(NewUpdateInviteUI);
	}
	else
	{
		// Clean up
		sceInvitationDialogTerminate();
	}

	return bShowedInviteUI;
}

bool FOnlineExternalUIPS4::ShowAchievementsUI(int32 LocalUserNum)
{
	UE_LOG_ONLINE(Warning, TEXT("ShowAchievementsUI not supported on PS4"));
	return false;
}

bool FOnlineExternalUIPS4::ShowLeaderboardUI( const FString& LeaderboardName )
{
	UE_LOG_ONLINE(Warning, TEXT("ShowLeaderboardUI not supported on PS4"));
	return false;
}

bool FOnlineExternalUIPS4::ShowWebURL(const FString& Url, const FShowWebUrlParams& ShowParams, const FOnShowWebUrlClosedDelegate& Delegate)
{
	bool bShowedWebURL = false;

	UE_LOG_ONLINE(Log, TEXT("ShowWebURL: %s"), *Url);

	if (Url.Len() > SCE_WEB_BROWSER_DIALOG_URL_SIZE_EXTENDED)
	{
		UE_LOG_ONLINE(Warning, TEXT("ShowWebURL: Url length is too long length=%d url=%s"), Url.Len(), *Url);
	}

	int32 ReturnCode = sceWebBrowserDialogInitialize();
	if (ReturnCode == SCE_COMMON_DIALOG_ERROR_ALREADY_INITIALIZED)
	{
		sceWebBrowserDialogTerminate();
		ReturnCode = sceWebBrowserDialogInitialize();
	}

	if (ReturnCode < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("sceWebBrowserDialogInitialize error = 0x%08x"), ReturnCode);
	}
	else
	{
		// Reset cookies before invoking browser UI
		if (ShowParams.bResetCookies)
		{
			SceWebBrowserDialogResetCookieParam ResetCookiesParam;
			memset(&ResetCookiesParam, 0x00, sizeof(ResetCookiesParam));
			ResetCookiesParam.size = sizeof(ResetCookiesParam);
			ReturnCode = sceWebBrowserDialogResetCookie(&ResetCookiesParam);
		}

		// Set up our browser parameters
		SceWebBrowserDialogParam DialogParam;
		sceWebBrowserDialogParamInitialize(&DialogParam);		
		DialogParam.mode = SCE_WEB_BROWSER_DIALOG_MODE_DEFAULT;

		SceWebBrowserDialogWebViewParam	WebViewParam; // Might not be used
		FMemory::Memzero(WebViewParam);

		DialogParam.url = new ANSICHAR[Url.Len() + 1];
		TCString<ANSICHAR>::Strcpy(const_cast<ANSICHAR*>(DialogParam.url), Url.Len() + 1, TCHAR_TO_ANSI(*Url));

		// register for triggering callback based on matching portion of url
		SceWebBrowserDialogCallbackInitParam CallbackParam;
		memset(&CallbackParam, 0x00, sizeof(CallbackParam));
		if (!ShowParams.CallbackPath.IsEmpty())
		{
			CallbackParam.size = sizeof(SceWebBrowserDialogCallbackInitParam);
			CallbackParam.type = SCE_WEB_BROWSER_DIALOG_CALLBACK_PARAM_TYPE_URL;

			SIZE_T DataSize = ShowParams.CallbackPath.Len() + 1;
			CallbackParam.data = new ANSICHAR[DataSize];
			TCString<ANSICHAR>::Strcpy(const_cast<ANSICHAR*>(CallbackParam.data), DataSize, TCHAR_TO_ANSI(*ShowParams.CallbackPath));
			DialogParam.callbackInitParam = &CallbackParam;
		}
		
		// Set up as embedded web browser
		if (ShowParams.bEmbedded)
		{
			DialogParam.mode = SCE_WEB_BROWSER_DIALOG_MODE_CUSTOM;
			DialogParam.parts = SCE_WEB_BROWSER_DIALOG_CUSTOM_PARTS_NONE | SCE_WEB_BROWSER_DIALOG_CUSTOM_PARTS_WAIT_DIALOG;
			DialogParam.headerWidth = ShowParams.SizeX;
			DialogParam.headerPositionX = ShowParams.OffsetX;
			DialogParam.headerPositionY = ShowParams.OffsetY;
			DialogParam.control = SCE_WEB_BROWSER_DIALOG_CUSTOM_CONTROL_NONE;
			DialogParam.animation = SCE_WEB_BROWSER_DIALOG_ANIMATION_DISABLE;
			DialogParam.positionX = ShowParams.OffsetX;
			DialogParam.positionY = ShowParams.OffsetY;
			DialogParam.width = ShowParams.SizeX;
			DialogParam.height = ShowParams.SizeY;

			if (ShowParams.bShowCloseButton)
			{
				DialogParam.parts |= SCE_WEB_BROWSER_DIALOG_CUSTOM_PARTS_FOOTER;
				DialogParam.control |= SCE_WEB_BROWSER_DIALOG_CUSTOM_CONTROL_EXIT;
			}
			if (ShowParams.bShowBackground)
			{
				DialogParam.parts |= SCE_WEB_BROWSER_DIALOG_CUSTOM_PARTS_BACKGROUND;
			}
			if( ShowParams.bHideCursor )
			{
				WebViewParam.size = sizeof( SceWebBrowserDialogWebViewParam );
				WebViewParam.option = SCE_WEB_BROWSER_DIALOG_WEBVIEW_OPTION_CURSOR_NONE;
				DialogParam.webviewParam = &WebViewParam;
			}
		}
		
		// Get the initial user
		ReturnCode = sceUserServiceGetInitialUser(&DialogParam.userId);
		if (ReturnCode < 0)
		{
			UE_LOG_ONLINE(Error, TEXT("sceUserServiceGetInitialUser error = 0x%08x"), ReturnCode);
			bShowedWebURL = false;
		}
		else
		{
			if (ShowParams.AllowedDomains.Num() > 0)
			{
				if (ShowParams.AllowedDomains.Num() > SCE_WEB_BROWSER_DIALOG_DOMAIN_COUNT)
				{
					UE_LOG_ONLINE(Warning, TEXT("ShowWebURL Passing in a greater number of AllowedDomains(%d) than supported(%d)"), ShowParams.AllowedDomains.Num(), SCE_WEB_BROWSER_DIALOG_DOMAIN_COUNT);
				}

				// Check if the requested domain is in our managed-domain whitelist
				bool bIsRequestWhitelistedDomain = false;
				for (const FString& Domain : ShowParams.AllowedDomains)
				{
					if (Url.Contains(Domain, ESearchCase::IgnoreCase, ESearchDir::FromStart))
					{
						bIsRequestWhitelistedDomain = true;
						break;
					}
				}

				// If this is a white-listed domain, use the special white-list command (only allows browsing of white-listed domains)
				if (bIsRequestWhitelistedDomain)
				{
					// Create buffer to store utf8 domain name strings (TCHAR_TO_UTF8 macro does not have a long enough lifetime to use here)
					TArray<FTCHARToUTF8, TFixedAllocator<SCE_WEB_BROWSER_DIALOG_DOMAIN_COUNT>> Utf8DomainArray;
					Utf8DomainArray.Empty(SCE_WEB_BROWSER_DIALOG_DOMAIN_COUNT);

					SceWebBrowserDialogPredeterminedContentParam PredeterminedDomains;
					memset(&PredeterminedDomains, 0x00, sizeof(PredeterminedDomains));
					for (int32 Idx = 0; Idx < ShowParams.AllowedDomains.Num() && Idx < SCE_WEB_BROWSER_DIALOG_DOMAIN_COUNT; Idx++)
					{
						const int32 BufferArrayIndex = Utf8DomainArray.Emplace(*ShowParams.AllowedDomains[Idx]);
						PredeterminedDomains.domain[Idx] = Utf8DomainArray[BufferArrayIndex].Get();
					}
					PredeterminedDomains.size = sizeof(SceWebBrowserDialogPredeterminedContentParam);
					// Open the web browser dialog with predetermined domains
					ReturnCode = sceWebBrowserDialogOpenForPredeterminedContent(&DialogParam, &PredeterminedDomains);
				}
				else
				{
					// Open the normal web browser dialog
					ReturnCode = sceWebBrowserDialogOpen(&DialogParam);
				}
			}
			else
			{
				// Open the normal web browser dialog
				ReturnCode = sceWebBrowserDialogOpen(&DialogParam);
			}

			if (ReturnCode < 0)
			{
				UE_LOG_ONLINE(Error, TEXT("sceWebBrowserDialogOpen error = 0x%08x"), ReturnCode);
				bShowedWebURL = false;
			}
			else
			{
				bShowedWebURL = true;
			}
		}
		delete[] DialogParam.url;
		if (DialogParam.callbackInitParam != nullptr)
		{
			delete[] DialogParam.callbackInitParam->data;
		}
	}

	if (bShowedWebURL)
	{
		// Trigger to let us know if the external UI has been made active
		PS4Subsystem->GetExternalUIInterface()->TriggerOnExternalUIChangeDelegates(true);

		// Async task to update the web browser dialog and let us know when it closes
		FOnlineAsyncTaskPS4UpdateWebBrowserDialog* NewUpdateBrowserDialogTask = new FOnlineAsyncTaskPS4UpdateWebBrowserDialog(PS4Subsystem, Delegate);
		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(NewUpdateBrowserDialogTask);
	}
	else
	{
		// Clean up
		sceWebBrowserDialogTerminate();
	}

	return bShowedWebURL;
}

bool FOnlineExternalUIPS4::CloseWebURL()
{
	int32 Ret = sceWebBrowserDialogClose();
	if (Ret != SCE_OK)
	{
		UE_LOG_ONLINE(Error, TEXT("sceWebBrowserDialogClose error = 0x%08x"), Ret);
		return false;
	}
	return true;
}

bool FOnlineExternalUIPS4::ShowProfileUI(const FUniqueNetId& Requestor, const FUniqueNetId& Requestee, const FOnProfileUIClosedDelegate& Delegate)
{
	bool bShowedProfileUI = false;

	int32 ReturnCode = sceNpProfileDialogInitialize();
	if (ReturnCode < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("sceNpProfileDialogInitialize error = 0x%08x"), ReturnCode);
		bShowedProfileUI = false;
	}
	else
	{
		FUniqueNetIdPS4 const& PS4Requestor = FUniqueNetIdPS4::Cast(Requestor);
		FUniqueNetIdPS4 const& PS4Requestee = FUniqueNetIdPS4::Cast(Requestee);

		// Set up the paramaters for the profile dialog
		SceNpProfileDialogParamA DialogParam;
		sceNpProfileDialogParamInitializeA(&DialogParam);
		DialogParam.mode = SCE_NP_PROFILE_DIALOG_MODE_NORMAL;

		// The user requesting the profile
		SceUserServiceUserId UserId = PS4Requestor.GetUserId();
		DialogParam.userId = UserId;

		// The user we're requesting to see
		DialogParam.targetAccountId = PS4Requestee.GetAccountId();

		ReturnCode = sceNpProfileDialogOpenA(&DialogParam);
		if (ReturnCode < 0)
		{
			UE_LOG_ONLINE(Error, TEXT("sceNpProfileDialogOpenA error = 0x%08x"), ReturnCode);
			bShowedProfileUI = false;
		}
		else
		{
			bShowedProfileUI = true;
		}
	}

	if (bShowedProfileUI)
	{
		// Trigger to let us know if the external UI has been made active
		PS4Subsystem->GetExternalUIInterface()->TriggerOnExternalUIChangeDelegates(true);

		// Async task to monitor when the profile external UI has been closed
		FOnlineAsyncTaskPS4UpdateProfileUI* NewUpdateProfileUITask = new FOnlineAsyncTaskPS4UpdateProfileUI(PS4Subsystem, Delegate);
		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(NewUpdateProfileUITask);
	}
	else
	{
		// Clean up
		sceNpProfileDialogTerminate();
	}

	return bShowedProfileUI;
}

bool FOnlineExternalUIPS4::ShowAccountUpgradeUI(const FUniqueNetId& UniqueId)
{
	FUniqueNetIdPS4 const& LocalUserIdPS4 = FUniqueNetIdPS4::Cast(UniqueId);

	SceUserServiceUserId LocalPlatformId = LocalUserIdPS4.GetUserId();
	if (LocalPlatformId != SCE_USER_SERVICE_USER_ID_INVALID)
	{
		SceNpCommerceDialogParam Params;
		sceNpCommerceDialogParamInitialize(&Params);

		Params.userId = LocalPlatformId;
		Params.mode = SCE_NP_COMMERCE_DIALOG_MODE_PLUS;
		Params.features = SCE_NP_PLUS_FEATURE_REALTIME_MULTIPLAY;

		int32 Ret = sceNpCommerceDialogInitialize();
		if (Ret != SCE_OK)
		{
			UE_LOG_ONLINE(Log, TEXT("sceNpCommerceDialogInitialize failed with error: 0x%x"), Ret);
			return false;
		}

		Ret = sceNpCommerceDialogOpen(&Params);
		if (Ret != SCE_OK)
		{
			UE_LOG_ONLINE(Log, TEXT("sceNpCommerceDialogOpen failed with error: 0x%x"), Ret);
			return false;
		}

		bLaunchedPSPlusDialog = Ret == SCE_OK;
		return bLaunchedPSPlusDialog;
	}

	return false;
}

class FOnlineAsyncTaskPS4ShowSystemDialog : public FNonAbandonableTask
{
public:

	FOnlineAsyncTaskPS4ShowSystemDialog(FOnlineSubsystemPS4* InPS4Subsystem, SceUserServiceUserId InServiceId, EPlatformMessageType InMessageType)
		: PS4Subsystem(InPS4Subsystem)
		, ServiceId(InServiceId)
		, MessageType(InMessageType)
	{}

	void DoWork()
	{
		int32 Ret = sceMsgDialogInitialize();
		if (Ret != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceMsgDialogInitialize failed with error: 0x%x"), Ret);
			return;
		}

		SceMsgDialogSystemMessageParam SystemParam = {};
		switch (MessageType)
		{
		case EPlatformMessageType::EmptyStore:
			SystemParam.sysMsgType = SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_EMPTY_STORE;
			break;
		case EPlatformMessageType::ChatRestricted:
			SystemParam.sysMsgType = SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_PSN_CHAT_RESTRICTION;
			break;
		case EPlatformMessageType::UGCRestricted:
			SystemParam.sysMsgType = SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_PSN_UGC_RESTRICTION;
			break;
		default:
			UE_LOG_ONLINE(Warning, TEXT("Unknown SceMsgDialogParam message type"));
			return;
		}		

		SceMsgDialogParam Params;
		sceMsgDialogParamInitialize(&Params);
		Params.mode = SCE_MSG_DIALOG_MODE_SYSTEM_MSG;
		Params.sysMsgParam = &SystemParam;
		Params.userId = ServiceId;

		Ret = sceMsgDialogOpen(&Params);
		if (Ret != SCE_OK)
		{
			UE_LOG_ONLINE(Log, TEXT("sceMsgDialogOpen failed with error: 0x%x"), Ret);
			return;
		}

		while (sceMsgDialogUpdateStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
			;

		sceMsgDialogTerminate();
	}

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FOnlineAsyncTaskPS4ShowSystemDialog, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	FOnlineSubsystemPS4* PS4Subsystem;
	SceUserServiceUserId ServiceId;
	EPlatformMessageType MessageType;

};
typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4ShowSystemDialog> AutoDeleteShowSystemDialogTask;

bool FOnlineExternalUIPS4::ShowPlatformMessageBox(const FUniqueNetId& UserId, EPlatformMessageType MessageType)
{
	FSlowHeartBeatScope SuspendHeartBeat;
	// make sure module is loaded
	if (sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG) != SCE_OK)
	{
		return false;
	}
	
	(new AutoDeleteShowSystemDialogTask(PS4Subsystem, FUniqueNetIdPS4::Cast(UserId).GetUserId(), MessageType))->StartBackgroundTask();
	return true;
}

void FOnlineExternalUIPS4::ReportEnterInGameStoreUI()
{
	sceNpCommerceShowPsStoreIcon(static_cast<SceNpCommercePsStoreIconPos>(StoreIconPosition));
}

void FOnlineExternalUIPS4::ReportExitInGameStoreUI()
{
	sceNpCommerceHidePsStoreIcon();
}

void FOnlineExternalUIPS4::Tick(float DeltaTime)
{
	if (bLaunchedPSPlusDialog)
	{
		SceCommonDialogStatus Status = sceNpCommerceDialogUpdateStatus();
		if (Status == SCE_COMMON_DIALOG_STATUS_FINISHED)
		{
			sceNpCommerceDialogTerminate();
			bLaunchedPSPlusDialog = false;
		}
	}

	SceErrorDialogStatus SignInDialogStatus = sceErrorDialogUpdateStatus();
	if (SignInDialogStatus == SCE_ERROR_DIALOG_STATUS_FINISHED)
	{
		sceErrorDialogTerminate();
		TSharedPtr<const FUniqueNetId> UserId = PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LoginUIControllerIndex);
		OnLoginUIClosedDelegate.ExecuteIfBound(UserId, LoginUIControllerIndex);
		OnLoginUIClosedDelegate.Unbind();
		LoginUIControllerIndex = -1;
	}
}

class FOnlineAsyncTaskPS4StoreDialog : public FOnlineAsyncTaskPS4
{
public:

	/** Constructor */
	explicit FOnlineAsyncTaskPS4StoreDialog(FOnlineSubsystemPS4* InSubsystem, const FOnShowStoreUIClosedDelegate& InClosedDelegate)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, bPurchased(false)
		, ClosedDelegate(InClosedDelegate)
	{
	}

	FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4StoreDialog bWasSuccessful: %d"), bWasSuccessful);
	}

	virtual void Tick() override
	{
		SceCommonDialogStatus Status = sceNpCommerceDialogUpdateStatus();
		if (Status == SCE_COMMON_DIALOG_STATUS_FINISHED || Status == SCE_COMMON_DIALOG_STATUS_NONE)
		{
			bIsComplete = true;
			SceNpCommerceDialogResult Result;
			memset(&Result, 0, sizeof(Result));
			int32 Ret = sceNpCommerceDialogGetResult(&Result);
			if (Ret < 0)
			{
				UE_LOG_ONLINE(Warning, TEXT("sceNpCommerceDialogGetResult failed with error: 0x%x"), Ret);
			}
			else
			{
				bPurchased = Result.result == SCE_NP_COMMERCE_DIALOG_RESULT_PURCHASED;
				bWasSuccessful = true;
			}
		}
	}

	virtual void Finalize() override
	{
		sceNpCommerceDialogTerminate();
	}

	virtual void TriggerDelegates() override
	{
		// Tell delegates we are closing the external UI
		ClosedDelegate.ExecuteIfBound(bPurchased);
		Subsystem->GetExternalUIInterface()->TriggerOnExternalUIChangeDelegates(false);
	}

	/** true if purchase was made in the store */
	bool bPurchased;
	/** Closed UI completion callback */
	FOnShowStoreUIClosedDelegate ClosedDelegate;
};

bool FOnlineExternalUIPS4::ShowStoreUI(int32 LocalUserNum, const FShowStoreParams& ShowParams, const FOnShowStoreUIClosedDelegate& Delegate)
{
	bool bStarted = false;
	
	int32 Ret = sceNpCommerceDialogInitialize();
	if (Ret == SCE_COMMON_DIALOG_ERROR_ALREADY_INITIALIZED)
	{
		sceNpCommerceDialogTerminate();
		Ret = sceNpCommerceDialogInitialize();
	}
	if (Ret != SCE_OK)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceNpCommerceDialogInitialize failed with error: 0x%x"), Ret);
	}
	else
	{
		// init params with appropriate category
		SceNpCommerceDialogParam Params;
		sceNpCommerceDialogParamInitialize(&Params);
		Params.mode = SCE_NP_COMMERCE_DIALOG_MODE_CATEGORY;
		Params.userId = FOnlineIdentityPS4::GetSceUserId(LocalUserNum);
		const char* Targets[] = { new ANSICHAR[ShowParams.Category.Len() + 1] };
		if (ShowParams.Category.IsEmpty())
		{
			Params.targets = NULL;
			Params.numTargets = 0;
		}
		else
		{
			TCString<ANSICHAR>::Strcpy(const_cast<ANSICHAR*>(Targets[0]), ShowParams.Category.Len() + 1, TCHAR_TO_ANSI(*ShowParams.Category));
			Params.targets = Targets;
			Params.numTargets = 1;
		}	
		// open the dialog
		Ret = sceNpCommerceDialogOpen(&Params);
		if (Ret != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceNpCommerceDialogOpen failed with error: 0x%x"), Ret);
		}
		else
		{
			bStarted = true;
		}
		delete[] Targets[0];
	}

	if (bStarted)
	{
		// Trigger to let us know if the external UI has been made active
		PS4Subsystem->GetExternalUIInterface()->TriggerOnExternalUIChangeDelegates(true);

		// Async task to update the store dialog and let us know when it closes
		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(new FOnlineAsyncTaskPS4StoreDialog(PS4Subsystem, Delegate));
	}
	else
	{
		// Clean up
		sceNpCommerceDialogTerminate();
	}

	return bStarted;
}

class FOnlineAsyncTaskPS4SendMessageDialog : public FOnlineAsyncTaskPS4
{
public:

	/** Constructor */
	explicit FOnlineAsyncTaskPS4SendMessageDialog(FOnlineSubsystemPS4* InSubsystem, const FOnShowSendMessageUIClosedDelegate& InClosedDelegate)
		: FOnlineAsyncTaskPS4(InSubsystem)
		, bMessagSent(false)
		, ClosedDelegate(InClosedDelegate)
	{
	}

	FString ToString() const override
	{
		return FString::Printf(TEXT("FOnlineAsyncTaskPS4SendMessageDialog bWasSuccessful: %d"), bWasSuccessful);
	}

	virtual void Tick() override
	{
		SceCommonDialogStatus Status = sceGameCustomDataDialogUpdateStatus();
		if (Status == SCE_COMMON_DIALOG_STATUS_FINISHED || Status == SCE_COMMON_DIALOG_STATUS_NONE)
		{
			bIsComplete = true;
			SceGameCustomDataDialogResultA Result;
			memset(&Result, 0, sizeof(Result));
			int32 Ret = sceGameCustomDataDialogGetResultA(&Result);
			if (Ret < 0)
			{
				UE_LOG_ONLINE(Warning, TEXT("sceGameCustomDataDialogGetResultA failed with error: 0x%x"), Ret);
			}
			else
			{
				bMessagSent = Result.result == SCE_COMMON_DIALOG_RESULT_OK;
				bWasSuccessful = true;
			}
		}
	}

	virtual void Finalize() override
	{
		sceGameCustomDataDialogTerminate();
	}

	virtual void TriggerDelegates() override
	{
		// Tell delegates we are closing the external UI
		ClosedDelegate.ExecuteIfBound(bMessagSent);
		Subsystem->GetExternalUIInterface()->TriggerOnExternalUIChangeDelegates(false);
	}

	/** true if message was sent and not canceled by user */
	bool bMessagSent;
	/** Closed UI completion callback */
	FOnShowSendMessageUIClosedDelegate ClosedDelegate;
};

bool FOnlineExternalUIPS4::ShowSendMessageUI(int32 LocalUserNum, const FShowSendMessageParams& ShowParams, const FOnShowSendMessageUIClosedDelegate& Delegate)
{
	bool bStarted = false;

	int32 Ret = sceGameCustomDataDialogInitialize();
	if (Ret == SCE_COMMON_DIALOG_ERROR_ALREADY_INITIALIZED)
	{
		sceGameCustomDataDialogTerminate();
		Ret = sceGameCustomDataDialogInitialize();
	}
	if (Ret != SCE_OK)
	{
		UE_LOG_ONLINE(Warning, TEXT("sceGameCustomDataDialogInitialize failed with error: 0x%x"), Ret);
	}
	else
	{
		//SCE_GAME_CUSTOM_DATA_DIALOG_MAX_USER_MSG_SIZE

		// init dialog params
		SceGameCustomDataDialogParamA Params;
		sceGameCustomDataDialogParamInitializeA(&Params);
		Params.mode = SCE_GAME_CUSTOM_DATA_DIALOG_MODE_SEND;
		Params.userId = FOnlineIdentityPS4::GetSceUserId(LocalUserNum);

		// init data params
		SceGameCustomDataDialogDataParamA DataParam;
		memset(&DataParam, 0, sizeof(DataParam));
		Params.dataParam = &DataParam;
		TArray<uint8> DataPayload;
		FString DataPayloadJson = ShowParams.DataPayload.ToJsonStr();		
		DataPayload.AddZeroed(DataPayloadJson.Len() + 1);
		FMemory::Memcpy(DataPayload.GetData(), TCHAR_TO_UTF8(*DataPayloadJson), DataPayload.Num());
		if (!DataPayloadJson.IsEmpty())
		{
			DataParam.SendInfo.data = (void*)DataPayload.GetData();
			DataParam.SendInfo.dataSize = DataPayload.Num();
		}

		FString DisplayTitleStr = ShowParams.DisplayTitle.ToString();
		FString DisplayMessageStr = ShowParams.DisplayMessage.ToString();		
		FString DisplayDetailsStr = ShowParams.DisplayDetails.ToString();
		TArray<SceGameCustomDataMultiLanguageString> DataTitleMultiLang;
		TArray<SceGameCustomDataMultiLanguageString> DataNameDetailLang;

		FTCHARToUTF8 UTF8DisplayTitle(*DisplayTitleStr);
		DataParam.SendInfo.dataName = new ANSICHAR[UTF8DisplayTitle.Length() + 1];
		FMemory::Memcpy((void*)DataParam.SendInfo.dataName, UTF8DisplayTitle.Get(), UTF8DisplayTitle.Length() + 1);

		FTCHARToUTF8 UTF8UserMessage(*DisplayMessageStr);
		DataParam.SendInfo.userMessage = new ANSICHAR[UTF8UserMessage.Length() + 1];
		FMemory::Memcpy((void*)DataParam.SendInfo.userMessage, UTF8UserMessage.Get(), UTF8UserMessage.Length() + 1);

		FTCHARToUTF8 UTF8DataDetail(*DisplayDetailsStr);
		DataParam.SendInfo.dataDetail = new ANSICHAR[UTF8DataDetail.Length() + 1];
		FMemory::Memcpy((void*)DataParam.SendInfo.dataDetail, UTF8DataDetail.Get(), UTF8DataDetail.Length() + 1);

		DataTitleMultiLang.Empty(ShowParams.DisplayTitle_Loc.Num());
		for (const auto& It : ShowParams.DisplayTitle_Loc)
		{
			SceGameCustomDataMultiLanguageString Entry;

			FTCHARToUTF8 UTF8LanguageString(*It.Key);
			Entry.language = new ANSICHAR[UTF8LanguageString.Length() + 1];
			FMemory::Memcpy((void*)Entry.language, UTF8LanguageString.Get(), UTF8LanguageString.Length() + 1);

			FTCHARToUTF8 UTF8TextString(*It.Value);
			Entry.str = new ANSICHAR[UTF8TextString.Length() + 1];
			FMemory::Memcpy((void*)Entry.str, UTF8TextString.Get(), UTF8TextString.Length() + 1);

			DataTitleMultiLang.Add(Entry);
		}

		DataNameDetailLang.Empty(ShowParams.DisplayTitle_Loc.Num());
		for (const auto& It : ShowParams.DisplayDetails_Loc)
		{
			SceGameCustomDataMultiLanguageString Entry;

			FTCHARToUTF8 UTF8LanguageString(*It.Key);
			Entry.language = new ANSICHAR[UTF8LanguageString.Length() + 1];
			FMemory::Memcpy((void*)Entry.language, UTF8LanguageString.Get(), UTF8LanguageString.Length() + 1);

			FTCHARToUTF8 UTF8TextString(*It.Value);
			Entry.str = new ANSICHAR[UTF8TextString.Length() + 1];
			FMemory::Memcpy((void*)Entry.str, UTF8TextString.Get(), UTF8TextString.Length() + 1);

			DataNameDetailLang.Add(Entry);
		}

		if (DataTitleMultiLang.Num() > 0)
		{
			DataParam.SendInfo.dataNameMultiLang = DataTitleMultiLang.GetData();
			DataParam.SendInfo.dataNameMultiLangNum = DataTitleMultiLang.Num();
		}

		if (DataNameDetailLang.Num() > 0)
		{
			DataParam.SendInfo.dataDetailMultiLang = DataNameDetailLang.GetData();
			DataParam.SendInfo.dataDetailMultiLangNum = DataNameDetailLang.Num();
		}

		TArray<uint8> ImageData;
		if (ShowParams.DisplayThumbnail.Num() > 0)
		{
			DataParam.SendInfo.thumbnail = (void*)ShowParams.DisplayThumbnail.GetData();
			DataParam.SendInfo.thumbnailSize = ShowParams.DisplayThumbnail.Num();
		}
		else
		{
			// must have a thumbnail image, so load a default one if not specified
			IFileHandle* ImageFileHandle = nullptr;
			FString ImageFullPath = FPaths::EngineDir() + TEXT("Build/PS4/InviteIcon.jpg");
			ImageFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*ImageFullPath);
			if (ImageFileHandle)
			{
				ImageData.AddZeroed(ImageFileHandle->Size());
				if (!ImageFileHandle->Read(ImageData.GetData(), ImageData.Num()))
				{
					ImageData.Empty();
				}
				delete ImageFileHandle;
			}
			DataParam.SendInfo.thumbnail = (void*)ImageData.GetData();
			DataParam.SendInfo.thumbnailSize = ImageData.Num();
		}
		DataParam.SendInfo.addressParam.addressType = SCE_GAME_CUSTOM_DATA_DIALOG_ADDRESS_TYPE_USERENABLE;
		DataParam.SendInfo.addressParam.addressInfo.UserSelectEnableAddress.userMaxCount = ShowParams.MaxRecipients;
		
		static const ANSICHAR* platforms[] = { "PS4" };
		DataParam.SendInfo.availablePlatform.platformName = platforms;
		DataParam.SendInfo.availablePlatform.count = 1;
		
		// open the dialog
		Ret = sceGameCustomDataDialogOpenA(&Params);
		if (Ret != SCE_OK)
		{
			UE_LOG_ONLINE(Warning, TEXT("sceGameCustomDataDialogOpenA failed with error: 0x%x"), Ret);
		}
		else
		{
			bStarted = true;
		}
		delete[] DataParam.SendInfo.dataName;
		delete[] DataParam.SendInfo.userMessage;
		delete[] DataParam.SendInfo.dataDetail;

		for (auto& It : DataTitleMultiLang)
		{
			delete[] It.language;
			delete[] It.str;
		}
		for (auto& It : DataNameDetailLang)
		{
			delete[] It.language;
			delete[] It.str;
		}
	}

	if (bStarted)
	{
		// Trigger to let us know if the external UI has been made active
		PS4Subsystem->GetExternalUIInterface()->TriggerOnExternalUIChangeDelegates(true);

		// Async task to update the store dialog and let us know when it closes
		PS4Subsystem->GetAsyncTaskManager()->AddToInQueue(new FOnlineAsyncTaskPS4SendMessageDialog(PS4Subsystem, Delegate));
	}
	else
	{
		// Clean up
		sceGameCustomDataDialogTerminate();
	}

	return bStarted;
}
