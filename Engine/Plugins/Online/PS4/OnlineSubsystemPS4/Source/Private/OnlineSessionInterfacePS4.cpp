// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSessionInterfacePS4.h"
#include "OnlineSubsystemSessionSettings.h"
#include "OnlineSubsystemPS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "OnlineVoiceInterfacePS4.h"
#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineIdentityInterfacePS4.h"
#include "OnlineFriendsInterfacePS4.h"
#include "IPAddress.h"
#include "JsonObject.h"
#include "JsonSerializer.h"
#include "JsonReader.h"
#include "HAL/PlatformFilemanager.h"

#include <message_dialog.h>
#include <netinet/in.h>

static IOnlineSubsystem* OnlineSub = nullptr;

namespace
{
	int32 CopyCustomSettingDataToBuffer(const FOnlineSessionSettings& SessionSettings, char* Buffer)
	{
		int32 SessionDataLength = 0;
		const FOnlineSessionSetting* CustomSetting = SessionSettings.Settings.Find(SETTING_CUSTOM);
		if (CustomSetting)
		{
			// Convert to UTF-8 to utilize as little space as possible in the buffer.
			const FString& data = CustomSetting->Data.ToString();
			TStringConversion<FTCHARToUTF8_Convert> Convert(*data);
			SessionDataLength = Convert.Length();
			FMemory::Memcpy(Buffer, Convert.Get(), SessionDataLength);
		}
		else
		{
			SessionDataLength = 5;
			FMemory::Memcpy(Buffer, "Empty", SessionDataLength);
		}
		return SessionDataLength;
	}
}

void FOnlineAsyncTaskPS4SessionCreate::DoWork()
{
	Data.bWasSuccessful = false;


	// Before we get started, want to make sure have an image to attach to the session creation
	// The image must be a jpeg, cannot exceed 160kb, and is recommended to be 457x257
	TArray<uint8> ImageData;
	{
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
	}
	if (ImageData.Num() == 0)
	{
		UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - session image is empty or not found."));
		SendResults();
		return;
	}

	// Create a multi-part request for creating a session
	// There are 3 parts to send: The json description of the session request, the image data for the session, and the app-specific session data
	if (UserWebApiContext == -1)
	{
		UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - UserWebApiContext not set."));
		SendResults();
		return;
	}

	int32 ResultCode = 0;
	int64_t WebRequestId = 0;

	// Create the request
	ResultCode = sceNpWebApiCreateMultipartRequest(UserWebApiContext, "sessionInvitation", "/v1/sessions", SCE_NP_WEBAPI_HTTP_METHOD_POST, &WebRequestId);
	if (ResultCode < 0)
	{
		UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiCreateMultipartRequest returned with error code 0x%x"), ResultCode);
		SendResults();
		return;
	}

	// Part 1: Session Request
	int32 SessionRequestIndex = 0;

	// Hardcoded session description for testing
	FString SessionRequestJsonStr;
	SessionRequestJsonStr += TEXT("{");
	SessionRequestJsonStr += FString::Printf(TEXT("\"sessionPrivacy\" : \"%s\", "), Data.NewSessionSettings.bShouldAdvertise ? TEXT("public") : TEXT("private"));
	SessionRequestJsonStr += FString::Printf(TEXT("\"sessionMaxUser\" : %d, "), Data.NewSessionSettings.NumPrivateConnections + Data.NewSessionSettings.NumPublicConnections);
	SessionRequestJsonStr += FString::Printf(TEXT("\"sessionName\" : \"%s\", "), *Data.SessionName.ToString());

	bool bUseHostMigration = false;
	Data.NewSessionSettings.Get(SETTING_HOST_MIGRATION, bUseHostMigration);

	SessionRequestJsonStr += TEXT("\"sessionType\" : ");
	SessionRequestJsonStr += bUseHostMigration ? TEXT("\"owner-migration\", ") : TEXT("\"owner-bind\", ");
	SessionRequestJsonStr += TEXT("\"availablePlatforms\" : [ \"PS4\" ]");
	SessionRequestJsonStr += TEXT("}");

	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineAsyncTaskPS4SessionCreate session info payload:\n%s"), *SessionRequestJsonStr);

	char SessionRequestJson[1024] = {};
	FMemory::Memcpy(SessionRequestJson, TCHAR_TO_ANSI(*SessionRequestJsonStr), SessionRequestJsonStr.Len());

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
			UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiAddMultipartPart for session request returned with error code 0x%x"), ResultCode);
			sceNpWebApiDeleteRequest(WebRequestId);
			SendResults();
			return;
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
			UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiAddMultipartPart for session image returned with error code 0x%x"), ResultCode);
			sceNpWebApiDeleteRequest(WebRequestId);
			SendResults();
			return;
		}
	}

	// Part 3: Session Data
	int32 SessionDataIndex = 0;
	char SessionData[FOnlineSessionPS4::MAX_SESSION_DATA_LENGTH] = {};
	int32 SessionDataLength = CopyCustomSettingDataToBuffer(Data.NewSessionSettings, SessionData);

	UE_LOG_ONLINE(VeryVerbose, TEXT("FOnlineAsyncTaskPS4SessionCreate session data payload:\n%s"), ANSI_TO_TCHAR(SessionData));

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
			UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiAddMultipartPart for session data returned with error code 0x%x"), ResultCode);
			sceNpWebApiDeleteRequest(WebRequestId);
			SendResults();
			return;
		}
	}

	SceNpWebApiResponseInformationOption ResponseInformationOption;
	FMemory::Memzero(ResponseInformationOption);
	
	// All the headers are ready so we can send the data
	// Part 1
	ResultCode = sceNpWebApiSendMultipartRequest2(WebRequestId, SessionRequestIndex, SessionRequestJson, FCStringAnsi::Strlen(SessionRequestJson), &ResponseInformationOption);
	if (ResultCode < 0)
	{
		UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiSendMultipartRequest2 for session request returned with error code 0x%x"), ResultCode);
		sceNpWebApiDeleteRequest(WebRequestId);
		SendResults();
		return;
	}

	// Part 2
	ResultCode = sceNpWebApiSendMultipartRequest2(WebRequestId, SessionImageIndex, ImageData.GetData(), ImageData.Num(), &ResponseInformationOption);
	if (ResultCode < 0)
	{
		UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiSendMultipartRequest2 for session image returned with error code 0x%x"), ResultCode);
		sceNpWebApiDeleteRequest(WebRequestId);
		SendResults();
		return;
	}

	// Part 3
	ResultCode = sceNpWebApiSendMultipartRequest2(WebRequestId, SessionDataIndex, SessionData, SessionDataLength, &ResponseInformationOption);
	if (ResultCode < 0)
	{
		UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiSendMultipartRequest2 for session data returned with error code 0x%x"), ResultCode);
		sceNpWebApiDeleteRequest(WebRequestId);
		SendResults();
		return;
	}

	// All 3 parts have been sent so we should have a status code now
	int32_t HttpStatusCode = ResponseInformationOption.httpStatus;

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

	// null terminator
	ResponseData.Add(0);

	EPS4Session::Type SessionType = PS4Subsystem->AreRoomsEnabled() ? EPS4Session::RoomSession : EPS4Session::StandaloneSession;

	TSharedPtr<FOnlineSessionInfoPS4> NewSessionInfo = MakeShareable(new FOnlineSessionInfoPS4(SessionType, SCE_NP_MATCHING2_INVALID_WORLD_ID, SCE_NP_MATCHING2_INVALID_LOBBY_ID, SCE_NP_MATCHING2_INVALID_ROOM_ID));
	Data.Session->SessionInfo = NewSessionInfo;

	if (HttpStatusCode == 200)
	{
		// The ResponseData contains Json of the form {"sessionId":"xxx-some-hex-numbers-xxx"}
		UE_LOG_ONLINE(Log, TEXT("Create session response body:\n%s"), UTF8_TO_TCHAR(ResponseData.GetData()));
		UE_LOG_ONLINE(Log, TEXT("Create session payload:  %s"), *SessionRequestJsonStr);

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(UTF8_TO_TCHAR(ResponseData.GetData()));
		if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			NewSessionInfo->SessionId = FUniqueNetIdString(JsonObject->GetStringField("sessionId"));
		}		

		if (PS4Subsystem->AreRoomsEnabled())
		{
			FOnlineSessionPS4Ptr PS4SessionSubsystem = StaticCastSharedPtr<FOnlineSessionPS4>(PS4Subsystem->GetSessionInterface());
			FOnlineAsyncTaskPS4SessionPutChangeableSessionData * PutDataTask = new FOnlineAsyncTaskPS4SessionPutChangeableSessionData(PS4Subsystem, UserWebApiContext, &NewSessionInfo->SessionId, TEXT("INVALIDSESSION"));
			// it's okay to call this on a worker thread since it only does simple accesses (could be static)
			Data.bWasSuccessful = PS4SessionSubsystem->DoCreateJoinRoom(Data.Session, UserMatching2Context, PutDataTask);
		}
		else
		{
			Data.Session->SessionState = EOnlineSessionState::Pending;
			Data.bWasSuccessful = true;
			sceNpWebApiDeleteRequest(WebRequestId);
			SendResults();
			return;
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Create session response code:%d"), HttpStatusCode);
	}
	sceNpWebApiDeleteRequest(WebRequestId);	

	if (!Data.bWasSuccessful)
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::CreateSession: DoCreateJoinRoom FAILED."));
		// if the room creation fails, retroactively destroy the session
		FOnlineAsyncTaskPS4SessionDestroy DestroyTask(PS4Subsystem, UserWebApiContext, Data.Session, FOnDestroySessionCompleteDelegate());
		DestroyTask.DoWork();
		SendResults();
		return;
	}
}


void FOnlineAsyncTaskPS4SessionCreate::SendResults()
{
	FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
	if (AsyncTaskManager)
	{
		auto NewEvent = new FAsyncEventSessionCreateTaskCompleted(PS4Subsystem, Data);
		AsyncTaskManager->AddToOutQueue(NewEvent);
	}
}

void FOnlineAsyncTaskPS4SessionDestroy::DoWork()
{
	FAsyncTask<FWebApiPS4Task> task(UserWebApiContext);
	task.GetTask().SetRequest(TEXT("sessionInvitation"), FString(TEXT("/v1/sessions/")) + Session->SessionInfo->GetSessionId().ToString(), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_DELETE);
	task.StartSynchronousTask();

	FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
	if (AsyncTaskManager)
	{
		auto NewEvent = new FAsyncEventSessionDestroyTaskCompleted(PS4Subsystem, Session->SessionName, task.GetTask().GetHttpStatusCode() == 204, CompletionDelegate);
		AsyncTaskManager->AddToOutQueue(NewEvent);
	}
}

void FOnlineAsyncTaskPS4SessionJoin::DoWork()
{
	FAsyncTask<FWebApiPS4Task> Task(UserWebApiContext);
	Task.GetTask().SetRequest(TEXT("sessionInvitation"), FString(TEXT("/v1/sessions/")) + Session->SessionInfo->GetSessionId().ToString() + FString(TEXT("/members")), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_POST);
	Task.StartSynchronousTask();

	EOnJoinSessionCompleteResult::Type Result = EOnJoinSessionCompleteResult::UnknownError;

	FWebApiPS4Task& TaskResult = Task.GetTask();

	const int32 StatusCode = TaskResult.GetHttpStatusCode();
	if (StatusCode == 204)
	{
		Result = EOnJoinSessionCompleteResult::Success;
	}
	else
	{
		const FString& Response = TaskResult.GetResponseBody();
		const FString& ErrorString = TaskResult.GetErrorString();

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Response);
		if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			auto ErrorJsonObject = JsonObject->GetObjectField("error");
			if (ErrorJsonObject.IsValid())
			{
				const int32 PSNJsonErrorCode = static_cast<int32>(ErrorJsonObject->GetNumberField("code"));
				UE_LOG_ONLINE(Warning, TEXT("Failed to join ps4 session: %s"), *ErrorJsonObject->GetStringField("message"));

				switch (PSNJsonErrorCode)
				{
					// there don't appear to be identifiers for these error codes, so go go magic numbers
				case 2114561:
					Result = EOnJoinSessionCompleteResult::SessionIsFull;
					UE_LOG_ONLINE(Warning, TEXT("Failed to join ps4 session: Session is full"));
					break;
				case 2113549:
					Result = EOnJoinSessionCompleteResult::SessionDoesNotExist;
					UE_LOG_ONLINE(Warning, TEXT("Failed to join ps4 session: Session does not exist"));
					break;
				default:
					Result = EOnJoinSessionCompleteResult::UnknownError;
					UE_LOG_ONLINE(Warning, TEXT("Failed to join ps4 session: Unknown Error Code. HTTPStatusCode=[%d] PSNErrorCode=[%d] Error=[%s] Response=[%s]"), StatusCode, PSNJsonErrorCode, *ErrorString, *Response);
					break;
				}
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("Failed to join ps4 session and reason is missing. HTTPStatusCode=[%d] Error=[%s] Response=[%s]"), StatusCode, *ErrorString, *Response);
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Failed to join ps4 session and cannot deserialize response. HTTPStatusCode=[%d] Error=[%s] Response=[%s]"), StatusCode, *ErrorString, *Response);
		}
	}

	if (FOnlineAsyncTaskManagerPS4* AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager())
	{
		FAsyncEventSessionJoinTaskCompleted* NewEvent = new FAsyncEventSessionJoinTaskCompleted(PS4Subsystem, Session->SessionName, DesiredSession, Result);
		AsyncTaskManager->AddToOutQueue(NewEvent);
	}
}

void FOnlineAsyncTaskPS4SessionLeave::DoWork()
{
	FOnlineAsyncTaskManagerPS4* AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4SessionLeave: Session pointer is null - we are unable to leave it."));
		if (AsyncTaskManager)
		{
			auto NewEvent = new FAsyncEventSessionLeaveTaskCompleted(PS4Subsystem, FName(), false, CompletionDelegate);
			AsyncTaskManager->AddToOutQueue(NewEvent);
		}
		return;
	}
	else if (!Session->SessionInfo.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineAsyncTaskPS4SessionLeave: Session->SessionInfo pointer is null - we are unable to leave %s. This might mean the session has already been left."), *Session->SessionName.ToString());
		if (AsyncTaskManager)
		{
			auto NewEvent = new FAsyncEventSessionLeaveTaskCompleted(PS4Subsystem, FName(), false, CompletionDelegate);
			AsyncTaskManager->AddToOutQueue(NewEvent);
		}
		return;
	}

	TSharedPtr<const FUniqueNetIdPS4> UserNetId = FUniqueNetIdPS4::Cast(Session->bHosting ? Session->OwningUserId : Session->LocalOwnerId);

	FAsyncTask<FWebApiPS4Task> task(UserWebApiContext);
	task.GetTask().SetRequest(TEXT("sessionInvitation"), FString(TEXT("/v1/sessions/")) + Session->SessionInfo->GetSessionId().ToString() + FString(TEXT("/members/")) + UserNetId->ToString(), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_DELETE);
	task.StartSynchronousTask();

	if (AsyncTaskManager)
	{
		auto NewEvent = new FAsyncEventSessionLeaveTaskCompleted(PS4Subsystem, Session->SessionName, task.GetTask().GetHttpStatusCode() == 204, CompletionDelegate);
		AsyncTaskManager->AddToOutQueue(NewEvent);
	}
}

FOnlineAsyncTaskPS4GetSessionData::FOnlineAsyncTaskPS4GetSessionData(FOnlineSubsystemPS4* InSubsystem, TSharedRef<const FUniqueNetIdPS4> InUserId, TSharedPtr<FOnlineSessionSearch> InSessionSearch, const FOnGetSessionDataCompleteDelegate& InOnGetSessionDataCompleteDelegate)
	: PS4Subsystem(InSubsystem)
	, UserId(InUserId)
	, OnlineSessionSearch(InSessionSearch)
	, OnGetSessionDataCompleteDelegate(InOnGetSessionDataCompleteDelegate)
	, bInitialized(false)
{
}

void FOnlineAsyncTaskPS4GetSessionData::Tick()
{
	if (!bInitialized)
	{
		bInitialized = true;

		FAsyncTask<FWebApiPS4Task> task(PS4Subsystem->GetUserWebApiContext(UserId.Get()));
		task.GetTask().SetRequest(TEXT("sessionInvitation"), FString(TEXT("/v1/sessions/")) + OnlineSessionSearch->SearchResults[0].Session.SessionInfo->GetSessionId().ToString() + FString(TEXT("/sessionData")), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);
		task.StartSynchronousTask();
		bWasSuccessful = task.GetTask().GetHttpStatusCode() == 200;
		bIsComplete = true;

		TaskResponseBody = task.GetTask().GetResponseBody();
	}
}

void FOnlineAsyncTaskPS4GetSessionData::TriggerDelegates()
{
	OnGetSessionDataCompleteDelegate.ExecuteIfBound(UserId.Get(), OnlineSessionSearch, TaskResponseBody, bWasSuccessful);
}

FOnlineAsyncTaskPS4GetSession::FOnlineAsyncTaskPS4GetSession(FOnlineSubsystemPS4* InSubsystem, TSharedRef<const FUniqueNetIdPS4> InUserId, TSharedPtr<FOnlineSessionSearch> InSessionSearch, const FOnGetSessionDataCompleteDelegate& InOnGetSessionCompleteDelegate)
	: PS4Subsystem(InSubsystem)
	, UserId(InUserId)
	, OnlineSessionSearch(InSessionSearch)
	, OnGetSessionCompleteDelegate(InOnGetSessionCompleteDelegate)
	, bInitialized(false)
{
}

void FOnlineAsyncTaskPS4GetSession::Tick()
{
	if (!bInitialized)
	{
		bInitialized = true;

		FAsyncTask<FWebApiPS4Task> task(PS4Subsystem->GetUserWebApiContext(UserId.Get()));
		task.GetTask().SetRequest(TEXT("sessionInvitation"), FString(TEXT("/v1/sessions/")) + OnlineSessionSearch->SearchResults[0].Session.SessionInfo->GetSessionId().ToString() + FString(TEXT("?fields=@default,members")), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);
		task.StartSynchronousTask();
		bWasSuccessful = task.GetTask().GetHttpStatusCode() == 200;
		bIsComplete = true;

		TaskResponseBody = task.GetTask().GetResponseBody();
	}
}

void FOnlineAsyncTaskPS4GetSession::TriggerDelegates()
{
	OnGetSessionCompleteDelegate.ExecuteIfBound(UserId.Get(), OnlineSessionSearch, TaskResponseBody, bWasSuccessful);
}


void FOnlineAsyncTaskPS4SessionGetChangeableSessionData::DoWork()
{
	FAsyncTask<FWebApiPS4Task> task(UserWebApiContext);
	task.GetTask().SetRequest(TEXT("sessionInvitation"), FString(TEXT("/v1/sessions/")) + SessionId + FString(TEXT("/changeableSessionData")), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);
	task.StartSynchronousTask();

	FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
	if (AsyncTaskManager)
	{
		auto NewEvent = new FAsyncEventSessionGetChangeableSessionDataTaskCompleted(PS4Subsystem, UserId, SessionId, task.GetTask().GetResponseBody(), Callback, task.GetTask().GetHttpStatusCode() == 200);
		AsyncTaskManager->AddToOutQueue(NewEvent);
	}
}

void FOnlineAsyncTaskPS4SessionPutChangeableSessionData::DoWork()
{
	FAsyncTask<FWebApiPS4Task> task(UserWebApiContext);
	task.GetTask().SetRequest(TEXT("sessionInvitation"), FString(TEXT("/v1/sessions/")) + SessionId.ToString() + FString(TEXT("/changeableSessionData")), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_PUT);
	task.GetTask().SetContentType("application/octet-stream");
	task.GetTask().SetRequestBody(Data);
	task.StartSynchronousTask();

	//Callback?
}

void FOnlineAsyncTaskPS4SessionPutSession::DoWork()
{
	FAsyncTask<FWebApiPS4Task> task(UserWebApiContext);
	task.GetTask().SetRequest(TEXT("sessionInvitation"), FString(TEXT("/v1/sessions/")) + SessionId.ToString(), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_PUT);
	task.GetTask().SetContentType("application/json; charset=utf-8");

	FString SessionRequestJsonStr;
	SessionRequestJsonStr += TEXT("{");
	SessionRequestJsonStr += FString::Printf(TEXT("\"sessionPrivacy\" : \"%s\", "), UpdatedSessionSettings.bShouldAdvertise ? TEXT("public") : TEXT("private"));
	SessionRequestJsonStr += FString::Printf(TEXT("\"sessionMaxUser\" : %d"), UpdatedSessionSettings.NumPrivateConnections + UpdatedSessionSettings.NumPublicConnections);
	SessionRequestJsonStr += TEXT("}");

	task.GetTask().SetRequestBody(SessionRequestJsonStr);
	task.StartSynchronousTask();

	UE_LOG_ONLINE(Log, TEXT("PutSession(%s) completed with result=%d payload=%s"), *SessionId.ToString(), task.GetTask().GetHttpStatusCode(), *SessionRequestJsonStr);
}

void FAsyncEventSessionJoinTask::TriggerDelegates()
{
	SessionPS4->JoinSession(JoiningPlayerId.Get(), SessionName, DesiredSession);
}

void FAsyncEventSessionCreateTaskCompleted::TriggerDelegates()
{
	FOnlineSessionPS4Ptr PS4Session = StaticCastSharedPtr<FOnlineSessionPS4>(PS4Subsystem->GetSessionInterface());
	if(PS4Session.IsValid())
	{
		PS4Session->OnSessionCreateComplete(Data);
	}
}

void FAsyncEventSessionDestroyTaskCompleted::TriggerDelegates()
{
	FOnlineSessionPS4Ptr PS4Session = StaticCastSharedPtr<FOnlineSessionPS4>(PS4Subsystem->GetSessionInterface());
	if (PS4Session.IsValid())
	{
		PS4Session->OnSessionDestroyComplete(SessionName, bWasSuccessful, CompletionDelegate);
	}
}

void FAsyncEventSessionJoinTaskCompleted::TriggerDelegates()
{
	FOnlineSessionPS4Ptr PS4Session = StaticCastSharedPtr<FOnlineSessionPS4>(PS4Subsystem->GetSessionInterface());
	if (PS4Session.IsValid())
	{
		PS4Session->OnSessionJoinComplete(SessionName, DesiredSession, Result);
	}
}

void FAsyncEventSessionLeaveTaskCompleted::TriggerDelegates()
{
	FOnlineSessionPS4Ptr PS4Session = StaticCastSharedPtr<FOnlineSessionPS4>(PS4Subsystem->GetSessionInterface());
	if (PS4Session.IsValid())
	{
		// we use destroy here since the logic is the same, but we may need an OnSessionLeaveComplete at some point
		PS4Session->OnSessionDestroyComplete(SessionName, bWasSuccessful, CompletionDelegate);
	}
}

void FAsyncEventSessionGetChangeableSessionDataTaskCompleted::TriggerDelegates()
{	
	int32 LocalUserNum = FPS4Application::GetPS4Application()->GetUserIndex(UserId->GetUserId());
	Callback.ExecuteIfBound(LocalUserNum, SessionId, ChangeableSessionData, bWasSuccessful);
}

/** Constructor for sessions that represent a PS4 room */
FOnlineSessionInfoPS4::FOnlineSessionInfoPS4(EPS4Session::Type InSessionType, const SceNpMatching2WorldId InWorldId, const SceNpMatching2WorldId InLobbyId, const SceNpMatching2RoomId InRoomId, const FUniqueNetIdString& InSessionId) :
	SessionId(InSessionId),
	SessionType(InSessionType),
	WorldId(InWorldId),
	LobbyId(InLobbyId),
	RoomId(InRoomId)
{
}

/** 
 * Initialize a PS4 session info with a unique session ID
 */
void FOnlineSessionInfoPS4::Init()
{
}

static void PS4RequestCallback(
	SceNpMatching2ContextId        CtxId,
	SceNpMatching2RequestId        ReqId,
	SceNpMatching2Event            Event,
	int     ErrorCode,
	const void *Data,
	void   *Arg
	)
{
	if (OnlineSub)
	{
		FOnlineSessionPS4Ptr SessionInterface = StaticCastSharedPtr<FOnlineSessionPS4>(OnlineSub->GetSessionInterface());
		SessionInterface->PS4RequestProcessing(CtxId, ReqId, Event, ErrorCode, Data, Arg);
	}
}

static void PS4ContextCallback(
	SceNpMatching2ContextId  CtxId,
	SceNpMatching2Event      Event,
	SceNpMatching2EventCause EventCause,
	int   ErrorCode,
	void   *Arg
	)
{
	if (OnlineSub)
	{
		FOnlineSessionPS4Ptr SessionInterface = StaticCastSharedPtr<FOnlineSessionPS4>(OnlineSub->GetSessionInterface());
		SessionInterface->PS4ContextProcessing(CtxId, Event, EventCause, ErrorCode, Arg);
	}
}

static void PS4RoomEventCallback(
	SceNpMatching2ContextId         CtxId,
	SceNpMatching2RoomId            RoomId,
	SceNpMatching2Event             Event,
	const void *Data,
	void   *Arg
	)
{
	if (OnlineSub)
	{
		FOnlineSessionPS4Ptr SessionInterface = StaticCastSharedPtr<FOnlineSessionPS4>(OnlineSub->GetSessionInterface());
		SessionInterface->PS4RoomEventProcessing(CtxId, RoomId, Event, Data, Arg);
	}
}

static void PS4LobbyEventCallback(
	SceNpMatching2ContextId         CtxId,
	SceNpMatching2LobbyId           LobbyId,
	SceNpMatching2Event             Event,
	const void *Data,
	void   *Arg
	)
{
	if (OnlineSub)
	{
		FOnlineSessionPS4Ptr SessionInterface = StaticCastSharedPtr<FOnlineSessionPS4>(OnlineSub->GetSessionInterface());
		SessionInterface->PS4LobbyEventProcessing(CtxId, LobbyId, Event, Data, Arg);
	}
}


static void PS4SignalingCallback(
	SceNpMatching2ContextId        CtxId,
	SceNpMatching2RoomId           RoomId,
	SceNpMatching2RoomMemberId     PeerMemberId,
	SceNpMatching2Event            Event,
	int     ErrorCode,
	void   *Arg
	)
{
	if (OnlineSub)
	{
		FOnlineSessionPS4Ptr SessionInterface = StaticCastSharedPtr<FOnlineSessionPS4>(OnlineSub->GetSessionInterface());
		SessionInterface->PS4SignalingProcessing(CtxId, RoomId, PeerMemberId, Event, ErrorCode, Arg);
	}
}


bool FOnlineSessionPS4::Init()
{
	bool InitializedSuccessfully = false;
	bUsingQuickmatch = false;

	OnlineSub = PS4Subsystem;

	SessionSettingMapping.Add(SETTING_CUSTOMSEARCHINT1, SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_1_ID);
	SessionSettingMapping.Add(SETTING_CUSTOMSEARCHINT2, SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_2_ID);
	SessionSettingMapping.Add(SETTING_CUSTOMSEARCHINT3, SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_3_ID);
	SessionSettingMapping.Add(SETTING_CUSTOMSEARCHINT4, SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_4_ID);
	SessionSettingMapping.Add(SETTING_CUSTOMSEARCHINT5, SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_5_ID);
	SessionSettingMapping.Add(SETTING_CUSTOMSEARCHINT6, SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_6_ID);
	SessionSettingMapping.Add(SETTING_CUSTOMSEARCHINT7, SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_7_ID);
	SessionSettingMapping.Add(SETTING_CUSTOMSEARCHINT8, SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_8_ID);
	
	if (PS4Subsystem->AreRoomsEnabled())
	{
		int LoadNpMatching2DllReturnCode = sceSysmoduleLoadModule(SCE_SYSMODULE_NP_MATCHING2);

		if (LoadNpMatching2DllReturnCode < SCE_OK)
		{
			UE_LOG_ONLINE(Log, TEXT("Failed to load NP Matching 2 dll : 0x%x"), LoadNpMatching2DllReturnCode);
		}
		else
		{
			SceNpMatching2InitializeParameter Matching2InitalizeParameters;
			FMemory::Memset(&Matching2InitalizeParameters, 0, sizeof(Matching2InitalizeParameters));
			Matching2InitalizeParameters.poolSize = 1024 * 1024;
			Matching2InitalizeParameters.size = sizeof(Matching2InitalizeParameters);

			int InitializeNpMatching2ReturnCode = sceNpMatching2Initialize(&Matching2InitalizeParameters);
			if (InitializeNpMatching2ReturnCode < SCE_OK)
			{
				UE_LOG_ONLINE(Log, TEXT("Failed to initialize NP Matching 2 : 0x%x"), InitializeNpMatching2ReturnCode);
				Finalize();
			}
			else
			{
				//todo: create contexts more proactively during gameplay based on login/logout/connection events
				InitializedSuccessfully = CreateInitialContext();
			}
		}
	}
	else
	{
		InitializedSuccessfully = true;
	}

	FCoreDelegates::OnInviteAccepted.AddRaw(this, &FOnlineSessionPS4::OnSessionInviteAccepted);

	return InitializedSuccessfully;
}

void FOnlineSessionPS4::Finalize()
{
	DoStopAllContexts();
	DoDestroyAllContexts();	

	sceNpMatching2Terminate();

	int UnloadNpMatching2ReturnCode = sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_MATCHING2);
	if (UnloadNpMatching2ReturnCode < SCE_OK) 
	{
		UE_LOG_ONLINE(Log, TEXT("sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_MATCHING2) failed. : 0x%x"), UnloadNpMatching2ReturnCode);
	}
}

class FMatching2ContextStartedCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
	FMatching2ContextStartedCompleted() : FOnlineAsyncEvent( nullptr )
	{
	}

public:
	FMatching2ContextStartedCompleted(FOnlineSubsystemPS4 * InPS4Subsystem, FOnlineSessionPS4 * InSessionPS4, SceNpMatching2ContextId InMatching2Context) :
		FOnlineAsyncEvent( InPS4Subsystem ),
		SessionPS4( InSessionPS4 ),
		Matching2Context( InMatching2Context) { }

private:
	FOnlineSessionPS4 *	SessionPS4;
	SceNpMatching2ContextId Matching2Context;

	virtual void		Finalize() override {}
	virtual FString		ToString() const override { return TEXT( "FMatching2ContextStartedCompleted" ); }
	virtual void		TriggerDelegates() override 
	{ 
		SessionPS4->DoGetWorldInfoList(Matching2Context);
	}
};

/**
 *	Async task that gets the address of the room owner
 */
class FAsyncGetRoomOwnerAddressTask : public FOnlineAsyncTask
{
public:
	enum EOwnerAddressState
	{
		Idle,
		Pending,
		Complete,
		Failed
	};

	FOnlineSubsystemPS4 *		SubsystemPS4;			// PS4 main subsystem
	FOnlineSessionPS4 *			SessionPS4;
	SceNpMatching2ContextId		ContextId;
	SceNpMatching2RoomId		RoomId;
	SceNpMatching2RoomMemberId	OwnerMemberId;
	EOwnerAddressState			State;

	FString						HostIpString;
	int32						HostPlatformPortHostOrder;

	/**
	 * Constructor.
	 */
	FAsyncGetRoomOwnerAddressTask( 
		FOnlineSubsystemPS4 *				InSubsystemPS4, 
		FOnlineSessionPS4 *					InSessionPS4, 
		SceNpMatching2ContextId				InContextId,
		const SceNpMatching2RoomId			InRoomId,
		const SceNpMatching2RoomMemberId	InOwnerMemberId ) :
		SubsystemPS4( InSubsystemPS4 ),
		SessionPS4( InSessionPS4 ),
		ContextId( InContextId ),
		RoomId( InRoomId ),
		OwnerMemberId( InOwnerMemberId ),
		State( Idle ),
		HostPlatformPortHostOrder(0)
	{
	}

	virtual bool	IsDone() override;
	virtual void	Tick() override;
	virtual bool	WasSuccessful() override;
	virtual FString	ToString() const override;
	virtual void	TriggerDelegates() override;
};

bool FAsyncGetRoomOwnerAddressTask::IsDone()
{
	return ( State == EOwnerAddressState::Complete || State == EOwnerAddressState::Failed );
}

void FAsyncGetRoomOwnerAddressTask::Tick()
{
	if ( State != EOwnerAddressState::Idle )
	{
		return;
	}

	State = EOwnerAddressState::Pending;

	// Wait until we establish a P2P connection with room owner
	while ( true )
	{
		int ConnStatus = SCE_NP_MATCHING2_SIGNALING_CONN_STATUS_INACTIVE;
		SceNetInAddr PeerAddr = { 0 };
		uint16_t PeerPort = 0;

		const int Ret = sceNpMatching2SignalingGetConnectionStatus( ContextId, RoomId, OwnerMemberId, &ConnStatus, &PeerAddr, &PeerPort );

		if ( Ret < SCE_OK )
		{
			UE_LOG_ONLINE( Error, TEXT( "FAsyncGetRoomOwnerAddressTask::Tick: sceNpMatching2SignalingGetConnectionStatus FAILED, ErrorCode: 0x%#x" ), Ret );
			State = EOwnerAddressState::Failed;
			break;
		}

		if ( ConnStatus == SCE_NP_MATCHING2_SIGNALING_CONN_STATUS_INACTIVE )
		{
			UE_LOG_ONLINE( Error, TEXT( "FAsyncGetRoomOwnerAddressTask::Tick: P2P Connection with owner is INACTIVE!" ) );
			State = EOwnerAddressState::Failed;
			break;
		}

		if ( ConnStatus == SCE_NP_MATCHING2_SIGNALING_CONN_STATUS_ACTIVE )
		{
			char IpBuffer[64] = { 0 };

			sceNetInetNtop( SCE_NET_AF_INET, &PeerAddr, IpBuffer, sizeof( IpBuffer ) );

			HostIpString = FString( IpBuffer );
			HostPlatformPortHostOrder = ntohs(PeerPort);

			UE_LOG_ONLINE( Log, TEXT( "FAsyncGetRoomOwnerAddressTask::Tick: P2P Connection with owner established. Address: %s: %i" ), *HostIpString, PeerPort );

			State = EOwnerAddressState::Complete;
			break;
		}

		FPlatformProcess::Sleep( 0.5f );	// It doesn't make sense to check this more than a couple times a second
	}
}

bool FAsyncGetRoomOwnerAddressTask::WasSuccessful()
{
	return State == EOwnerAddressState::Complete;
}

FString FAsyncGetRoomOwnerAddressTask::ToString() const
{
	return TEXT( "FAsyncGetRoomOwnerAddressTask" );
}

void FAsyncGetRoomOwnerAddressTask::TriggerDelegates()
{
	FNamedOnlineSession * Session = SessionPS4->GetRoomIdSession( RoomId );

	if ( Session == nullptr )
	{
		// This shouldn't happen, but we need to handle this just in case
		UE_LOG_ONLINE( Error, TEXT( "FAsyncGetRoomOwnerAddressTask::TriggerDelegates: Session == nullptr!" ) );
		SessionPS4->TriggerOnJoinSessionCompleteDelegates( NAME_None, EOnJoinSessionCompleteResult::UnknownError );
		return;
	}

	if ( !Session->SessionInfo.IsValid() )
	{
		UE_LOG_ONLINE( Error, TEXT( "FAsyncGetRoomOwnerAddressTask::TriggerDelegates: !Session->SessionInfo.IsValid()" ) );
		State = EOwnerAddressState::Failed;
	}

	if ( State == EOwnerAddressState::Failed )
	{
		// We failed, make sure to remove the session
		UE_LOG_ONLINE( Error, TEXT( "FAsyncGetRoomOwnerAddressTask::TriggerDelegates: State == EOwnerAddressState::Failed" ) );		
		SessionPS4->TriggerOnJoinSessionCompleteDelegates( Session->SessionName, EOnJoinSessionCompleteResult::CouldNotRetrieveAddress );
		return;
	}

	//
	// Success
	//

	// Generate the host address of this session
	FOnlineSessionInfoPS4 * PS4SessionInfo = (FOnlineSessionInfoPS4*)( Session->SessionInfo.Get() );

	
	PS4SessionInfo->HostAddr = ISocketSubsystem::Get()->CreateInternetAddr();
	bool bIsValid = false;
	PS4SessionInfo->HostAddr->SetIp( *HostIpString, bIsValid );
	PS4SessionInfo->HostAddr->SetPort( 7777 );	// FIXME: Obtain from room
	PS4SessionInfo->HostAddr->SetPlatformPort( HostPlatformPortHostOrder );

	// Notify the game thread of success
	SessionPS4->TriggerOnJoinSessionCompleteDelegates( Session->SessionName, EOnJoinSessionCompleteResult::Success );
	SessionPS4->TriggerOnMatchmakingCompleteDelegates(Session->SessionName, true);
}

FOnlineAsyncTaskPS4LeaveRoom::FOnlineAsyncTaskPS4LeaveRoom(FOnlineSubsystemPS4* InSubsystem, FNamedOnlineSession* InSession, const FOnDestroySessionCompleteDelegate& InDestroySessionDelegate, SceNpMatching2RequestId InRequestId)
	: PS4Subsystem(InSubsystem)
	, Session(InSession)
	, DestroySessionDelegate(InDestroySessionDelegate)
	, bInitialized(false)
	, ErrorCode(UINT_MAX)
	, RequestId(InRequestId)
{
}

void FOnlineAsyncTaskPS4LeaveRoom::Tick()
{
	if (!bInitialized)
	{
		bInitialized = true;

		bool LeaveRoomSuccess = false;

		check(Session);

		//this process fires off the destroysessioncomplete delegate, so we set the destroying state.
		Session->SessionState = EOnlineSessionState::Destroying;

		// Guaranteed to be called after the flush is complete
		if (Session->SessionInfo.IsValid())
		{
			FOnlineSessionInfoPS4* PS4SessionInfo = (FOnlineSessionInfoPS4*)(Session->SessionInfo.Get());
			if (PS4SessionInfo->SessionType == EPS4Session::RoomSession && PS4SessionInfo->RoomId != SCE_NP_MATCHING2_INVALID_ROOM_ID)
			{
				SceNpMatching2LeaveRoomRequest LeaveRoomRequestParameters;
				FMemory::Memset(&LeaveRoomRequestParameters, 0, sizeof(LeaveRoomRequestParameters));
				SceNpMatching2RequestOptParam RequestOptionParameters;
				FMemory::Memset(&RequestOptionParameters, 0, sizeof(RequestOptionParameters));

				RequestOptionParameters.cbFunc = &PS4RequestCallback;
				RequestOptionParameters.cbFuncArg = this;

				LeaveRoomRequestParameters.roomId = PS4SessionInfo->RoomId;

				FOnlineSessionPS4* OnlineSessionPS4 = (FOnlineSessionPS4*)PS4Subsystem->GetSessionInterface().Get();
				if (OnlineSessionPS4)
				{
					// don't use the HostingPlayerNum because a sign-out event might trigger the leave, and we won't be able to look up the netid from the usernum in that case.
					TSharedPtr<const FUniqueNetIdPS4> UserNetId = FUniqueNetIdPS4::Cast(Session->bHosting ? Session->OwningUserId : Session->LocalOwnerId);
					int LeaveRoomReturnCode = sceNpMatching2LeaveRoom(OnlineSessionPS4->GetUserMatching2Context(*UserNetId, false), &LeaveRoomRequestParameters, &RequestOptionParameters, &RequestId);
					if (LeaveRoomReturnCode < SCE_OK)
					{
						UE_LOG_ONLINE(Error, TEXT("Failed to leave a room - 0x%x"), LeaveRoomReturnCode);
						bIsComplete = true;
					}
					else
					{
						UE_LOG_ONLINE(Verbose, TEXT("Attempting to leave room - 0x%x"), PS4SessionInfo->RoomId);
					}
				}
			}
		}
	}
}

void FOnlineAsyncTaskPS4LeaveRoom::ProcessCallbackResult(int InErrorCode)
{
	UE_LOG_ONLINE(Verbose, TEXT("Left room result was successful: - %d"), InErrorCode == SCE_OK);
	ErrorCode = InErrorCode;
	bWasSuccessful = ErrorCode == SCE_OK;
	bIsComplete = true;
}

void FOnlineAsyncTaskPS4LeaveRoom::TriggerDelegates()
{
	FOnlineSessionPS4* OnlineSessionPS4 = (FOnlineSessionPS4*)PS4Subsystem->GetSessionInterface().Get();
	if (OnlineSessionPS4)
	{
		OnlineSessionPS4->TriggerOnEndSessionCompleteDelegates(Session->SessionName, true);
		OnlineSessionPS4->OnRoomLeaveComplete(Session->SessionName, bWasSuccessful, DestroySessionDelegate);
	}
}

/**
 *	Async task that gets the QoS of all the search rooms
 */
class FMatching2WaitOnQoSTask : public FOnlineAsyncTask
{
public:
	enum EWaitOnQoSState
	{
		Idle,
		Pending,
		Complete,
		Failed
	};

	FOnlineSubsystemPS4 *		SubsystemPS4;			// PS4 main subsystem
	FOnlineSessionPS4 *			SessionPS4;
	EWaitOnQoSState				State;

	/**
	 * Constructor.
	 */
	FMatching2WaitOnQoSTask( 
		FOnlineSubsystemPS4 *				InSubsystemPS4, 
		FOnlineSessionPS4 *					InSessionPS4 ) :
		SubsystemPS4( InSubsystemPS4 ),
		SessionPS4( InSessionPS4 ),
		State( Idle )
	{
	}

	virtual bool	IsDone() override;
	virtual void	Tick() override;
	virtual bool	WasSuccessful() override;
	virtual FString	ToString() const override;
	virtual void	TriggerDelegates() override;
};

bool FMatching2WaitOnQoSTask::IsDone()
{
	return ( State == EWaitOnQoSState::Complete || State == EWaitOnQoSState::Failed );
}

void FMatching2WaitOnQoSTask::Tick()
{
	if ( State != EWaitOnQoSState::Idle )
	{
		return;
	}

	State = EWaitOnQoSState::Pending;

	// Wait until we get QoS results back for all the search results
	while ( State == EWaitOnQoSState::Pending )
	{
		// Assume we're done, but verify below
		State = EWaitOnQoSState::Complete;

		for ( int32 i = 0; i < SessionPS4->CurrentSessionSearch->SearchResults.Num(); ++i )
		{
			if ( SessionPS4->CurrentSessionSearch->SearchResults[i].PingInMs < 0.0f )
			{
				// Still waiting on a result, move back to pending state
				State = EWaitOnQoSState::Pending;
				break;
			}
		}

		FPlatformProcess::Sleep( 0.25f );
	}
}

bool FMatching2WaitOnQoSTask::WasSuccessful()
{
	return State == EWaitOnQoSState::Complete;
}

FString FMatching2WaitOnQoSTask::ToString() const
{
	return TEXT( "FMatching2WaitOnQoSTask" );
}

void FMatching2WaitOnQoSTask::TriggerDelegates()
{
	if ( State == EWaitOnQoSState::Failed )
	{
		UE_LOG_ONLINE( Error, TEXT( "FMatching2WaitOnQoSTask::TriggerDelegates: State == EWaitOnQoSState::Failed" ) );
		SessionPS4->CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
		SessionPS4->CurrentSessionSearch = nullptr;
		SessionPS4->TriggerOnFindSessionsCompleteDelegates( false );
		return;
	}

	//
	// Success
	//

	SessionPS4->CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Done;
	SessionPS4->CurrentSessionSearch = nullptr;
	SessionPS4->TriggerOnFindSessionsCompleteDelegates( true );
}


class FMatching2JoinRoomCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
	FMatching2JoinRoomCompleted() : FOnlineAsyncEvent( nullptr )
	{
	}

public:
	FMatching2JoinRoomCompleted(	FOnlineSubsystemPS4 *				InPS4Subsystem, 
		FOnlineSessionPS4 *					InSessionPS4,
		const int							InErrorCode,
		const SceNpMatching2ContextId		InContextId,
		const SceNpMatching2RoomId			InRoomId,
		const SceNpMatching2RoomMemberId	InOwnerMemberId,
		const FName &						InSessionName) :
			FOnlineAsyncEvent( InPS4Subsystem ),
			SessionPS4( InSessionPS4 ),
			ErrorCode( InErrorCode ),
			ContextId( InContextId ),
			RoomId( InRoomId ),
			OwnerMemberId( InOwnerMemberId ),
			SessionName(InSessionName) { }

private:
	FOnlineSessionPS4 *			SessionPS4;
	int							ErrorCode;
	SceNpMatching2ContextId		ContextId;
	SceNpMatching2RoomId		RoomId;
	SceNpMatching2RoomMemberId	OwnerMemberId;
	FName						SessionName;

	virtual void		Finalize() override {}
	virtual FString		ToString() const override { return TEXT( "FMatching2JoinRoomCmpleted" ); }
	virtual void		TriggerDelegates() override 
	{ 
		FNamedOnlineSession * Session = SessionPS4->GetNamedSession( SessionName );

		if ( Session == nullptr )
		{
			// This shouldn't happen, but we need to handle this just in case
			UE_LOG_ONLINE( Error, TEXT( "FMatching2JoinRoomCmpleted::TriggerDelegates: Session == nullptr!" ) );
			SessionPS4->TriggerOnJoinSessionCompleteDelegates( NAME_None, EOnJoinSessionCompleteResult::UnknownError );
			return;
		}

		if ( ErrorCode < SCE_OK )
		{
			EOnJoinSessionCompleteResult::Type Error = EOnJoinSessionCompleteResult::UnknownError;
			UE_LOG_ONLINE(Error, TEXT("FMatching2JoinRoomCmpleted::TriggerDelegates ErrorCode < SCE_OK, ErrorCode: 0x%#x"), ErrorCode);
			switch(ErrorCode)
			{
			case SCE_NP_MATCHING2_SERVER_ERROR_ROOM_FULL:
				Error = EOnJoinSessionCompleteResult::SessionIsFull;
				break;
			case SCE_NP_MATCHING2_SERVER_ERROR_ALREADY_JOINED:				
				Error = EOnJoinSessionCompleteResult::AlreadyInSession;
				break;
			}			
			SessionPS4->TriggerOnJoinSessionCompleteDelegates(Session->SessionName, Error);
			return;
		}

		auto NewTask = new FAsyncGetRoomOwnerAddressTask( Subsystem, SessionPS4, ContextId, RoomId, OwnerMemberId );
		Subsystem->GetAsyncTaskManager()->AddToInQueue( NewTask );
	}
};

void FOnlineSessionPS4::PS4ContextProcessing(
	SceNpMatching2ContextId  CtxId,
	SceNpMatching2Event      Event,
	SceNpMatching2EventCause EventCause,
	int   ErrorCode,
	void* Arg
	)
{
	switch(Event)
	{
		case SCE_NP_MATCHING2_CONTEXT_EVENT_STARTED:
			{				
				UE_LOG_ONLINE(Log, TEXT("NP_MATCHING2_CONTEXT_EVENT_STARTED: Context: 0x%x, Cause: 0x%x, Error: 0x%x"), CtxId, EventCause, ErrorCode);

				//Save the server Id
				int GetServerIdReturnCode = sceNpMatching2GetServerId(CtxId, &ServerId);
				if (GetServerIdReturnCode < SCE_OK)
				{
					DoDestroyContext(CtxId);
					UE_LOG_ONLINE(Error, TEXT("sceNpMatching2GetServerId() failed 0x%x"), GetServerIdReturnCode);
					return;
				}

				EMatching2ContextState ContextState = ErrorCode == SCE_OK ? EMatching2ContextState::Started : EMatching2ContextState::Invalid;
				SetUserMatching2ContextState(CtxId, ContextState);

				if (WorldId == SCE_NP_MATCHING2_INVALID_WORLD_ID)
				{
					// Use an async event to make sure we call DoGetWorldInfoList from the game thread
					auto NewEvent = new FMatching2ContextStartedCompleted(PS4Subsystem, this, CtxId);
					PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
				}
			}
			break;
		case SCE_NP_MATCHING2_CONTEXT_EVENT_START_OVER:
			{
				// todo: actually set this up to retry a few times.
				EMatching2ContextState ContextState = EMatching2ContextState::Invalid;
				SetUserMatching2ContextState(CtxId, ContextState);
				DoDestroyContext(CtxId);
				UE_LOG_ONLINE(Warning, TEXT("SCE_NP_MATCHING2_CONTEXT_EVENT_START_OVER - eventCause=%u, errorCode=0x%x"), EventCause, ErrorCode);
				ServerId = SCE_NP_MATCHING2_INVALID_SERVER_ID;
			}
			break;
		case SCE_NP_MATCHING2_CONTEXT_EVENT_STOPPED:
			{
				EMatching2ContextState ContextState = ErrorCode == SCE_OK ? EMatching2ContextState::Ended : EMatching2ContextState::Invalid;
				SetUserMatching2ContextState(CtxId, ContextState);
				UE_LOG_ONLINE(Warning, TEXT("SCE_NP_MATCHING2_CONTEXT_EVENT_STOPPED"));
				ServerId = SCE_NP_MATCHING2_INVALID_SERVER_ID;
			}
			break;
		default:
			UE_LOG_ONLINE(Warning, TEXT("Unknown context event: 0x%x - eventCause=%u, errorCode=0x%x"), Event, EventCause, ErrorCode);
			break;
	}
}

class FMatching2SearchRoomCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
	FMatching2SearchRoomCompleted() = delete;

public:
	FMatching2SearchRoomCompleted(FOnlineSubsystemPS4 * InPS4Subsystem, FOnlineSessionPS4 * InSessionPS4, const FUniqueNetIdPS4& InSearchingPlayerId, const int InErrorCode) :
		FOnlineAsyncEvent( InPS4Subsystem ),
		SessionPS4( InSessionPS4 ),
		SearchingPlayerId( InSearchingPlayerId.AsShared() ),
		ErrorCode( InErrorCode ){ }

private:
	FOnlineSessionPS4 *					SessionPS4;
	TSharedRef<const FUniqueNetIdPS4>	SearchingPlayerId;
	int									ErrorCode;

	virtual void		Finalize() override {}
	virtual FString		ToString() const override { return TEXT( "FMatching2SearchRoomCompleted" ); }
	virtual void		TriggerDelegates() override 
	{ 
		if ( ErrorCode < 0 )
		{
			SessionPS4->CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
			SessionPS4->CurrentSessionSearch = nullptr;
			SessionPS4->TriggerOnMatchmakingCompleteDelegates(SessionPS4->GetQuickmatchSearchingSessionName(), false);
			SessionPS4->CancelMatchmaking(SessionPS4->GetQuickmatchSearchingPlayerId(), SessionPS4->GetQuickmatchSearchingSessionName());
			SessionPS4->TriggerOnFindSessionsCompleteDelegates( false );
			return;
		}

		if (!SessionPS4->DoPingSearchResults(SearchingPlayerId.Get(), *SessionPS4->CurrentSessionSearch))
		{
			UE_LOG_ONLINE( Error, TEXT( "FMatching2SearchRoomCompleted::TriggerDelegates: DoPingSearchResults FAILED" ) );
			SessionPS4->CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::Failed;
			SessionPS4->CurrentSessionSearch = nullptr;
			SessionPS4->TriggerOnMatchmakingCompleteDelegates(SessionPS4->GetQuickmatchSearchingSessionName(), false); //set this to true since we want to tell the game to host a match
			SessionPS4->CancelMatchmaking(SessionPS4->GetQuickmatchSearchingPlayerId(), SessionPS4->GetQuickmatchSearchingSessionName());
			SessionPS4->TriggerOnFindSessionsCompleteDelegates( false );
			return;
		}

		// Now add the task to wait QoS checks that were fired off from DoPingSearchResults above
		auto NewTask = new FMatching2WaitOnQoSTask( Subsystem, SessionPS4 );
		Subsystem->GetAsyncTaskManager()->AddToInQueue( NewTask );
	}
};

class FMatching2CreateJoinRoomCompleted : public FOnlineAsyncEvent<FOnlineSubsystemPS4>
{
	FMatching2CreateJoinRoomCompleted() : FOnlineAsyncEvent( nullptr )
	{
	}

public:
	FMatching2CreateJoinRoomCompleted(	FOnlineSubsystemPS4 *		InPS4Subsystem, 
										FOnlineSessionPS4 *			InSessionPS4,
										const int32					InErrorCode,
										const SceNpMatching2WorldId	InWorldId, 
										const SceNpMatching2WorldId InLobbyId, 
										const SceNpMatching2RoomId	InRoomId ) :
		FOnlineAsyncEvent( InPS4Subsystem ),
		SessionPS4( InSessionPS4 ),
		ErrorCode( InErrorCode ),
		WorldId( InWorldId ),
		LobbyId( InLobbyId ),
		RoomId( InRoomId )
		{ }

private:
	FOnlineSessionPS4 *		SessionPS4;
	int32					ErrorCode;
	SceNpMatching2WorldId	WorldId;
	SceNpMatching2WorldId	LobbyId; 
	SceNpMatching2RoomId	RoomId;

	virtual void		Finalize() override {}
	virtual FString		ToString() const override { return TEXT( "FMatching2SearchRoomCompleted" ); }
	virtual void		TriggerDelegates() override 
	{ 
		//Find the session that is being created
		FNamedOnlineSession * Session = SessionPS4->GetCreatingSession();
		if ( Session == nullptr )
		{
			// This can't technically happen, but handle it just in case			
			UE_LOG_ONLINE( Error, TEXT( "FMatching2CreateJoinRoomCompleted::TriggerDelegates: There is no session being created currently" ) );
			SessionPS4->TriggerOnCreateSessionCompleteDelegates( NAME_None, false );
			return;
		}

		if ( ErrorCode < SCE_OK ) 
		{
			SessionPS4->RemoveNamedSession(Session->SessionName);
			UE_LOG_ONLINE( Error, TEXT( "FMatching2CreateJoinRoomCompleted::TriggerDelegates: RequestEvent = SCE_NP_MATCHING2_REQUEST_EVENT_CREATE_JOIN_ROOM_A, error(0x%#x)" ), ErrorCode );			
			SessionPS4->TriggerOnCreateSessionCompleteDelegates( Session->SessionName, false );			
			return;
		}

		//
		// Success, create the PS Session Info and store the RoomId
		//

		// Setup the host session info

		if(!(Session->SessionInfo.IsValid() && Session->SessionInfo->GetSessionId().IsValid()))
		{
			UE_LOG_ONLINE(Error, TEXT("FMatching2CreateJoinRoomCompleted::TriggerDelegates: Trying to create a room without a session id"));			
			SessionPS4->RemoveNamedSession(Session->SessionName);
			SessionPS4->TriggerOnCreateSessionCompleteDelegates(Session->SessionName, false);
			return;
		}

		TSharedPtr<FOnlineSessionInfoPS4> SessionInfoPS4 = StaticCastSharedPtr<FOnlineSessionInfoPS4>(Session->SessionInfo);
		SessionInfoPS4->WorldId = WorldId;
		SessionInfoPS4->RoomId = RoomId;
		SessionInfoPS4->LobbyId = LobbyId;
		Session->SessionState = EOnlineSessionState::Pending;
		SessionPS4->RegisterLocalPlayers( Session );

		UE_LOG_ONLINE( Log, TEXT( "FMatching2CreateJoinRoomCompleted::TriggerDelegates: Created & Joined Room. RoomId: %016lx, WorldId: %i" ), RoomId, WorldId );

		SessionPS4->TriggerOnCreateSessionCompleteDelegates( Session->SessionName, true );
	}
};

class FMatching2LeaveRoomCompleted : public FOnlineAsyncEvent < FOnlineSubsystemPS4 >
{
	FMatching2LeaveRoomCompleted() : FOnlineAsyncEvent(nullptr)
	{
	}

public:
	FMatching2LeaveRoomCompleted(FOnlineSubsystemPS4 *		InPS4Subsystem,
		FOnlineSessionPS4 *			InSessionPS4,
		const int32					InErrorCode,
		FName						InSessionName,
		const FOnDestroySessionCompleteDelegate& InCompletionDelegate) :
		FOnlineAsyncEvent(InPS4Subsystem),
		SessionPS4(InSessionPS4),
		SessionName(InSessionName),
		CompletionDelegate(InCompletionDelegate)
	{ }

private:
	FOnlineSessionPS4 *		SessionPS4;
	int32					ErrorCode;
	FName					SessionName;
	FOnDestroySessionCompleteDelegate CompletionDelegate;

	virtual void		Finalize() override {}
	virtual FString		ToString() const override { return TEXT("FMatching2LeaveRoomCompleted"); }
	virtual void		TriggerDelegates() override
	{
		SessionPS4->TriggerOnEndSessionCompleteDelegates(SessionName, true);
		SessionPS4->OnRoomLeaveComplete(SessionName, ErrorCode == SCE_OK, CompletionDelegate);
	}
};

class FMatching2GetRoomDataCompleted : public FOnlineAsyncEvent < FOnlineSubsystemPS4 >
{
	FMatching2GetRoomDataCompleted() : FOnlineAsyncEvent(nullptr)
	{
	}

public:
	FMatching2GetRoomDataCompleted(FOnlineSubsystemPS4 * InPS4Subsystem, int32 InLocalUserNum, TSharedPtr<FOnlineSessionSearchResult> InSearchResult, bool bInWasSuccessful)
		: FOnlineAsyncEvent(InPS4Subsystem), LocalUserNum(InLocalUserNum), SearchResult(InSearchResult), bWasSuccessful(bInWasSuccessful)
	{}

private:
	int32 LocalUserNum;
	TSharedPtr<FOnlineSessionSearchResult> SearchResult;
	bool bWasSuccessful;

	virtual void		Finalize() override {}
	virtual FString		ToString() const override { return TEXT("FMatching2GetRoomDataCompleted"); }
	virtual void		TriggerDelegates() override
	{
		if(bWasSuccessful)
		{
			/*const auto AcceptingUserIndex =
				PS4Subsystem->GetIdentityLive()->GetControllerIndexForUser(AcceptingUser);
*/
			IOnlineIdentityPtr IdentityInt = Subsystem->GetIdentityInterface();
			if (!IdentityInt.IsValid())
			{
				UE_LOG_ONLINE(Error, TEXT("FMatching2GetRoomDataCompleted::TriggerDelegates: Identity interface DNE"));
				return;
			}
			TSharedPtr<const FUniqueNetId> LocalUserId = IdentityInt->GetUniquePlayerId(LocalUserNum);
			if (!LocalUserId.IsValid())
			{
				UE_LOG_ONLINE(Error, TEXT("FMatching2GetRoomDataCompleted::TriggerDelegates: LocalUserId is not valid"));
				return;
			}
			IOnlineSessionPtr SessionInt = Subsystem->GetSessionInterface();
			if (!SessionInt.IsValid())
			{
				UE_LOG_ONLINE(Error, TEXT("FMatching2GetRoomDataCompleted::TriggerDelegates: Session interface DNE"));
				return;
			}
			SessionInt->TriggerOnSessionUserInviteAcceptedDelegates(true, LocalUserNum, LocalUserId, *SearchResult);
		}
	}
};

namespace
{
	void GenerateSessionFromRoomData(FOnlineSession & Session, const SceNpMatching2RoomDataExternalA& ExternalData)
	{
		// Fill session members
		Session.OwningUserId = FUniqueNetIdPS4::FindOrCreate(ExternalData.owner.accountId, ExternalData.ownerOnlineId);

		Session.OwningUserName = PS4OnlineIdToString(ExternalData.ownerOnlineId);

		Session.NumOpenPublicConnections = ExternalData.openPublicSlotNum;
		Session.NumOpenPrivateConnections = ExternalData.openPrivateSlotNum;

		// Fill session settings members
		Session.SessionSettings.NumPublicConnections = ExternalData.publicSlotNum;
		Session.SessionSettings.NumPrivateConnections = ExternalData.privateSlotNum;
		Session.SessionSettings.bAntiCheatProtected = false;
		Session.SessionSettings.bUsesPresence = true;
		//Session.SessionSettings.Set(SETTING_MAPNAME, FString(UTF8_TO_TCHAR(ServerDetails->m_szMap)), EOnlineDataAdvertisementType::ViaOnlineService);

		FUniqueNetIdString SessionId(FString(TEXT("INVALID")));

		if (ExternalData.roomSearchableBinAttrExternalNum == 1 && ExternalData.roomSearchableBinAttrExternal[0].id == SCE_NP_MATCHING2_ROOM_SEARCHABLE_BIN_ATTR_EXTERNAL_1_ID)
		{
			//determine parse the session data string so we can fill out some necessary data
			FString SessionMapName = "";
			FString SessionGameType = "";
			FString SessionIdStr = "";
			FString SessionData = FString(reinterpret_cast<const ANSICHAR*>(ExternalData.roomSearchableBinAttrExternal[0].ptr));

			SessionIdStr = SessionData.LeftChop(SessionData.Len() - SessionData.Find("?"));
			SessionId = FUniqueNetIdString(SessionIdStr);

			SessionData = SessionData.RightChop(SessionIdStr.Len() + 1);
			SessionGameType = SessionData.LeftChop(SessionData.Len() - SessionData.Find("?"));
			Session.SessionSettings.Set(SETTING_GAMEMODE, SessionGameType, EOnlineDataAdvertisementType::ViaOnlineService);

			SessionData = SessionData.RightChop(SessionGameType.Len() + 1);
			SessionMapName = SessionData.LeftChop(SessionData.Len() - SessionData.Find("?"));
			Session.SessionSettings.Set(SETTING_MAPNAME, SessionMapName, EOnlineDataAdvertisementType::ViaOnlineService);
		}

		FOnlineSessionInfoPS4* NewSessionInfo = new FOnlineSessionInfoPS4(EPS4Session::RoomSession, ExternalData.worldId, ExternalData.lobbyId, ExternalData.roomId, SessionId);
		NewSessionInfo->Init();

		Session.SessionInfo = MakeShareable(NewSessionInfo);
	}
}

//Following can be event types (not yet implemented in the request processing
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_ROOM_MEMBER_DATA_EXTERNAL_LIST_A = sceNpMatching2GetRoomMemberDataExternalList() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_ROOM_MEMBER_DATA_EXTERNAL_LIST = sceNpMatching2GetRoomMemberDataExternalList() completed (deprecated)
// SCE_NP_MATCHING2_REQUEST_EVENT_SET_ROOM_DATA_EXTERNAL = sceNpMatching2SetRoomDataExternal() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_ROOM_DATA_EXTERNAL_LIST = sceNpMatching2GetRoomDataExternalList() completed (deprecated)
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_LOBBY_INFO_LIST = sceNpMatching2GetLobbyInfoList() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_SET_USER_INFO = sceNpMatching2SetUserInfo() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_USER_INFO_LIST_A = sceNpMatching2GetUserInfoListA() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_USER_INFO_LIST = sceNpMatching2GetUserInfoList() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_CREATE_JOIN_ROOM = sceNpMatching2CreateJoinRoom() completed (deprecated)
// SCE_NP_MATCHING2_REQUEST_EVENT_JOIN_ROOM = sceNpMatching2JoinRoom() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GRANT_ROOM_OWNER = sceNpMatching2GrantRoomOwner() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_KICKOUT_ROOM_MEMBER = sceNpMatching2KickoutRoomMember() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_SEARCH_ROOM = sceNpMatching2SearchRoom() completed (deprecated)
// SCE_NP_MATCHING2_REQUEST_EVENT_SEND_ROOM_CHAT_MESSAGE = sceNpMatching2SendRoomChatMessage() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_SEND_ROOM_MESSAGE = sceNpMatching2SendRoomMessage() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_SET_ROOM_DATA_INTERNAL = sceNpMatching2SetRoomDataInternal() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_ROOM_DATA_INTERNAL = sceNpMatching2GetRoomDataInternal() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_SET_ROOM_MEMBER_DATA_INTERNAL = sceNpMatching2SetRoomMemberDataInternal() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_ROOM_MEMBER_DATA_INTERNAL_A = sceNpMatching2GetRoomMemberDataInternal() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_ROOM_MEMBER_DATA_INTERNAL = sceNpMatching2GetRoomMemberDataInternal() completed (deprecated)
// SCE_NP_MATCHING2_REQUEST_EVENT_SET_SIGNALING_OPT_PARAM = sceNpMatching2SetSignalingOptParam() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_SEND_LOBBY_CHAT_MESSAGE = sceNpMatching2SendLobbyChatMessage() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_SET_LOBBY_MEMBER_DATA_INTERNAL = sceNpMatching2SetLobbyMemberDataInternal() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_LOBBY_MEMBER_DATA_INTERNAL_A = sceNpMatching2GetLobbyMemberDataInternal() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_LOBBY_MEMBER_DATA_INTERNAL = sceNpMatching2GetLobbyMemberDataInternal() completed (deprecated)
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_LOBBY_MEMBER_DATA_INTERNAL_LIST_A = sceNpMatching2GetLobbyMemberDataInternalList() completed
// SCE_NP_MATCHING2_REQUEST_EVENT_GET_LOBBY_MEMBER_DATA_INTERNAL_LIST = sceNpMatching2GetLobbyMemberDataInternalList() completed (deprecated)
void FOnlineSessionPS4::PS4RequestProcessing(
	SceNpMatching2ContextId		CtxId,
	SceNpMatching2RequestId		ReqId,
	SceNpMatching2Event			Event,
	int							ErrorCode,
	const void *				Data,
	void *						Arg
)
{
	switch(Event)
	{
		case SCE_NP_MATCHING2_REQUEST_EVENT_GET_WORLD_INFO_LIST:
		{
			if ( ErrorCode < SCE_OK ) 
			{
				UE_LOG_ONLINE(Error, TEXT("RequestEvent=SCE_NP_MATCHING2_REQUEST_EVENT_GET_WORLD_INFO_LIST, error(0x%x)"), ErrorCode);
				//WorldId = SCE_NP_MATCHING2_INVALID_WORLD_ID;
				break;
			}
			SceNpMatching2GetWorldInfoListResponse *ResponseData = (SceNpMatching2GetWorldInfoListResponse *)Data;
			if ( ResponseData->worldNum > 0 )
			{
				SceNpMatching2World *BestWorld = ResponseData->world;
				SceNpMatching2World *CurrentWorld = ResponseData->world;
				uint32_t MostNumRooms = 0;
				const uint32_t MaxNumRoomsPerWorld = 2000; //temporary value until the real value for the max number of rooms that a world can hold is found
				//Find the most populated world O(n)
				for (int32 i = 0; i < ResponseData->worldNum; ++i)
				{
					if (CurrentWorld)
					{
						//As long as this world is not completely full
						if (CurrentWorld->curNumOfRoom < MaxNumRoomsPerWorld)
						{
							//store this world as the best choice if it has the most num of rooms out of the rooms we've looked at so far
							if (CurrentWorld->curNumOfRoom > MostNumRooms)
							{
								MostNumRooms = CurrentWorld->curNumOfRoom;
								BestWorld = ResponseData->world;
							}
						}
						else //current world is full so set the next world as the best choice
						{
							if (CurrentWorld->next)
							{
								BestWorld = CurrentWorld->next;
								MostNumRooms = BestWorld->curNumOfRoom;
							}
						}
						CurrentWorld = CurrentWorld->next;
					}
				}
				WorldId = BestWorld->worldId;

				UE_LOG_ONLINE(Log, TEXT("Get WorldInfoList success.  WorldId = %#x\n, curNumOfRoom: %u, curNumOfTotalRoomMember: %u"), WorldId, ResponseData->world->curNumOfRoom, ResponseData->world->curNumOfTotalRoomMember);
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("Get WorldInfoList found no world"));
			}
		}
		break;
		case SCE_NP_MATCHING2_REQUEST_EVENT_CREATE_JOIN_ROOM_A:
		{
			// Push this as an async task so that it happens on the game thread
			if ( ErrorCode < SCE_OK ) 
			{
				UE_LOG_ONLINE(Error, TEXT("RequestEvent=SCE_NP_MATCHING2_REQUEST_EVENT_CREATE_JOIN_ROOM_A, error(0x%x)"), ErrorCode);
				auto NewEvent = new FMatching2CreateJoinRoomCompleted( PS4Subsystem, this, ErrorCode, SCE_NP_MATCHING2_INVALID_WORLD_ID, SCE_NP_MATCHING2_INVALID_LOBBY_ID, SCE_NP_MATCHING2_INVALID_ROOM_ID );
				PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue( NewEvent );
				break;
			}
			SceNpMatching2CreateJoinRoomResponseA *ResponseData = (SceNpMatching2CreateJoinRoomResponseA *)Data;
			FOnlineAsyncTaskPS4SessionPutChangeableSessionData *PutDataTask = (FOnlineAsyncTaskPS4SessionPutChangeableSessionData*)Arg;
			PutDataTask->SetData(FString::Printf(TEXT("%llu"), ResponseData->roomDataInternal->roomId));
			PutDataTask->DoWork(); // We're not on the main thread, but we might be blocking Matching2 here?  Works for now.

			auto NewEvent = new FMatching2CreateJoinRoomCompleted( PS4Subsystem, this, ErrorCode, ResponseData->roomDataInternal->worldId, ResponseData->roomDataInternal->lobbyId, ResponseData->roomDataInternal->roomId );
			PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue( NewEvent );
		}
		break;
		case SCE_NP_MATCHING2_REQUEST_EVENT_SEARCH_ROOM_A:
		{
			if ( ErrorCode < SCE_OK ) 
			{
				UE_LOG_ONLINE(Error, TEXT("RequestEvent=SCE_NP_MATCHING2_REQUEST_EVENT_SEARCH_ROOM_A, error(0x%x)"), ErrorCode);
				// Use an async event to marshal the data to the game thread
				auto NewEvent = new FMatching2SearchRoomCompleted(PS4Subsystem, this, *QuickmatchSearchingPlayerId, ErrorCode);
				PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue( NewEvent );
				CancelMatchmaking(*QuickmatchSearchingPlayerId, QuickmatchSearchingSessionName);
				break;
			}

			FOnlineSessionSearch * SearchSettings = static_cast<FOnlineSessionSearch*>( Arg );

			SceNpMatching2SearchRoomResponseA * ResponseData = (SceNpMatching2SearchRoomResponseA *)Data;

			UE_LOG_ONLINE(Log, TEXT("Searching Rooms range.resultCount = %u, range.startIndex = %u, range.total = %u"), ResponseData->range.resultCount, ResponseData->range.startIndex, ResponseData->range.total);

			// It's debatable whether or not we should muck with SearchSettings since we're on a thread here, but leaving this here since
			// it would be a pain to copy this data any other way, and I don't see immediate issues since the game thread shouldn't 
			// be touching this data at this point
			SearchSettings->SearchResults.Empty();

			// roomDataExternal is a linked list, therefore don't access it like an array.
			// However, we do have the number expected, so we'll use that to ensure no circular
			// references.
			size_t CurrentRoomIdx=0;
			const SceNpMatching2RoomDataExternalA* CurrentRoomDataExt = ResponseData->roomDataExternal;

			while (CurrentRoomDataExt != nullptr && CurrentRoomIdx < ResponseData->range.resultCount)
			{
				// Fill search result members
				FOnlineSessionSearchResult* NewResult = new( SearchSettings->SearchResults )FOnlineSessionSearchResult();
				GenerateSessionFromRoomData(NewResult->Session, *CurrentRoomDataExt);
				CurrentRoomDataExt = CurrentRoomDataExt->next;
				CurrentRoomIdx++;
			}

			// After iterating, we expect to have reached the end of the list.
			// If not, the list potentially has a circular reference (or otherwise corrupt).
			if (CurrentRoomDataExt != nullptr)
			{
				UE_LOG_ONLINE(Warning, TEXT("Expected number of external rooms handled without reaching end. Potential circular reference."));
			}

			if (bUsingQuickmatch)
			{
				// Search the results for best session to join
				int32 BestMatchIndex = -1; // Stores the index of the most populated session
				int32 MostOpenConnections = INT_MAX; // Used to find the session with the least open connections
				//find the session with the least open connections (i.e. most players in the session)
				for (int32 i = 0; i < SearchSettings->SearchResults.Num(); i++)
				{
					const FOnlineSessionSearchResult& SearchResult = SearchSettings->SearchResults[i];
					if(SearchResult.Session.NumOpenPublicConnections > 0)
					{
						if (SearchResult.Session.NumOpenPublicConnections < MostOpenConnections)
						{
							MostOpenConnections = SearchResult.Session.NumOpenPublicConnections;
							BestMatchIndex = i;
						}
					}
				}
				if (BestMatchIndex != -1)
				{
					auto NewEvent = new FAsyncEventSessionJoinTask(PS4Subsystem, this, *QuickmatchSearchingPlayerId, QuickmatchSearchingSessionName, SearchSettings->SearchResults[BestMatchIndex]);
					PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
				}
				bUsingQuickmatch = false;
			}
			
			// Use an async event to marshal the data to the game thread
			auto NewEvent = new FMatching2SearchRoomCompleted(PS4Subsystem, this, *QuickmatchSearchingPlayerId, ErrorCode);
			PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue( NewEvent );
		}
		break;
		case SCE_NP_MATCHING2_REQUEST_EVENT_JOIN_ROOM_A:
		{
			FName* SessionName = reinterpret_cast<FName*>(Arg);
			SceNpMatching2JoinRoomResponseA * ResponseData = (SceNpMatching2JoinRoomResponseA *)Data;
			if (!ResponseData || ErrorCode < SCE_OK)
			{
				UE_LOG_ONLINE(Error, TEXT("RequestEvent=SCE_NP_MATCHING2_REQUEST_EVENT_JOIN_ROOM_A, error(0x%#x)"), ErrorCode);
				auto NewEvent = new FMatching2JoinRoomCompleted(PS4Subsystem, this, ErrorCode, CtxId, SCE_NP_MATCHING2_INVALID_ROOM_ID, SCE_NP_MATCHING2_INVALID_ROOM_MEMBER_ID, *SessionName);
				delete SessionName;
				PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
				break;
			}
			auto NewEvent = new FMatching2JoinRoomCompleted(PS4Subsystem, this, ErrorCode, CtxId, ResponseData->roomDataInternal->roomId, ResponseData->memberList.owner->memberId, *SessionName);
			delete SessionName;
			PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
		}
		break;
		case SCE_NP_MATCHING2_REQUEST_EVENT_JOIN_LOBBY:
		{
			SceNpMatching2JoinLobbyResponse *JoinLobbyResponseData = (SceNpMatching2JoinLobbyResponse *)Data;
			if (!JoinLobbyResponseData)
			{
				UE_LOG_ONLINE(Error, TEXT("RequestEvent=SCE_NP_MATCHING2_REQUEST_EVENT_JOIN_LOBBY, error(0x%#x)"), ErrorCode);
				break;
			}
			//Find the session by lobbyId
			FNamedOnlineSession* Session = GetLobbyIdSession(JoinLobbyResponseData->lobbyDataInternal->lobbyId);
			if (Session == nullptr)
			{
				UE_LOG_ONLINE(Error, TEXT("Can not find the session being joined"));
			}
			else
			{
				if ( ErrorCode < SCE_OK ) 
				{
					UE_LOG_ONLINE(Error, TEXT("RequestEvent=SCE_NP_MATCHING2_REQUEST_EVENT_JOIN_LOBBY, error(0x%x)"), ErrorCode);
					// Clean up the session info so we don't get into a confused state
					RemoveLobbyIdSession(JoinLobbyResponseData->lobbyDataInternal->lobbyId);
					TriggerOnJoinSessionCompleteDelegates(Session->SessionName, EOnJoinSessionCompleteResult::UnknownError);
				}
				else
				{					
					UE_LOG_ONLINE(Log, TEXT("Joined lobby lobbyId=%016lx"), JoinLobbyResponseData->lobbyDataInternal->lobbyId);
					TriggerOnJoinSessionCompleteDelegates(Session->SessionName, EOnJoinSessionCompleteResult::Success);
				}
			}
		}
		break;
		case SCE_NP_MATCHING2_REQUEST_EVENT_LEAVE_ROOM:
		{
			// This is theoretically dangerous. We are assuming the task will still be in the async queue and not deleted before we
			// get the callback. However, if cancelling tasks is implemented or we shut down and this result is triggered somehow after we've destroyed
			// the async queue, we could crash.
			FOnlineAsyncTaskPS4LeaveRoom *LeaveRoomTask = (FOnlineAsyncTaskPS4LeaveRoom *)Arg;
			check(LeaveRoomTask);

			LeaveRoomTask->ProcessCallbackResult(ErrorCode);
		}
		break;
		case SCE_NP_MATCHING2_REQUEST_EVENT_LEAVE_LOBBY:
		{
			if ( ErrorCode < SCE_OK ) 
			{
				UE_LOG_ONLINE(Error, TEXT("RequestEvent=SCE_NP_MATCHING2_REQUEST_EVENT_LEAVE_LOBBY, error(0x%x)"), ErrorCode);
				return;
			}
			else
			{
				SceNpMatching2LeaveLobbyResponse *ResponseData = (SceNpMatching2LeaveLobbyResponse *)Data;
				FNamedOnlineSession* Session = GetLobbyIdSession(ResponseData->lobbyId);
				if (Session)
				{
					Session->SessionState = EOnlineSessionState::Ended;
				}
				else
				{
					UE_LOG_ONLINE(Error, TEXT("LEAVE_LOBBY: Can not find the session we're leaving"));
					return; // Get outta here before we dereference it
				}
				TriggerOnEndSessionCompleteDelegates(Session->SessionName, true);
				UE_LOG_ONLINE(Log, TEXT("Left lobby lobbyId=%016lx"), ResponseData->lobbyId);
			}
		}
		break;
		case SCE_NP_MATCHING2_REQUEST_EVENT_SIGNALING_GET_PING_INFO:
		{
			FOnlineSessionSearchResult* SearchResult = static_cast<FOnlineSessionSearchResult*>(Arg);
			if ( ErrorCode < SCE_OK ) 
			{
				//UE_LOG_ONLINE( Error, TEXT( "RequestEvent=SCE_NP_MATCHING2_REQUEST_EVENT_SIGNALING_GET_PING_INFO, error(0x%x)" ), ErrorCode );
				SearchResult->PingInMs = 9999.0f;	// Set to a large value so that we won't continue to wait on this result
				return;
			}
			else
			{
				SceNpMatching2SignalingGetPingInfoResponse *ResponseData = (SceNpMatching2SignalingGetPingInfoResponse *)Data;
				SearchResult->PingInMs = ResponseData->rtt / 1000;
			}
		}
		break;
		case SCE_NP_MATCHING2_REQUEST_EVENT_GET_ROOM_DATA_EXTERNAL_LIST_A:
		{
			int32* LocalUserNum = reinterpret_cast<int32*>(Arg);

			if (ErrorCode < SCE_OK)
			{
				UE_LOG_ONLINE(Error, TEXT("RequestEvent=SCE_NP_MATCHING2_REQUEST_EVENT_GET_ROOM_DATA_EXTERNAL_LIST_A, error(0x%#x)"), ErrorCode);

				// Use an async event to marshal the data to the game thread
				auto NewEvent = new FMatching2GetRoomDataCompleted(PS4Subsystem, *LocalUserNum, nullptr, false);
				delete LocalUserNum;
				PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
				break;
			}

			TSharedPtr<FOnlineSessionSearchResult> SearchResult = MakeShareable(new FOnlineSessionSearchResult());

			SceNpMatching2GetRoomDataExternalListResponseA * ResponseData = (SceNpMatching2GetRoomDataExternalListResponseA *)Data;

			if (ResponseData->roomDataExternalNum > 0)
			{
				// only grab the first one for now -- may need a more comprehensive solution later
				GenerateSessionFromRoomData(SearchResult->Session, ResponseData->roomDataExternal[0]);

				auto NewEvent = new FMatching2GetRoomDataCompleted(PS4Subsystem, *LocalUserNum, SearchResult, true);
				delete LocalUserNum;
				PS4Subsystem->GetAsyncTaskManager()->AddToOutQueue(NewEvent);
			}
		}
		break;
		default:
		{
			if ( ErrorCode < SCE_OK ) 
			{
				UE_LOG_ONLINE(Log, TEXT("Unknown RequestEvent=%04x, error(0x%#x)\n"), Event, ErrorCode)
				return;
			}
		}
		break;
	}
}

void FOnlineSessionPS4::PS4RoomEventProcessing(
	SceNpMatching2ContextId         CtxId,
	SceNpMatching2RoomId            RoomId,
	SceNpMatching2Event             Event,
	const void *Data,
	void *Arg
	)
{
	switch(Event)
	{
		case SCE_NP_MATCHING2_ROOM_EVENT_KICKEDOUT:
			{
				RemoveRoomIdSession(RoomId);
				SceNpMatching2RoomUpdateInfo *UpdateInfo = (SceNpMatching2RoomUpdateInfo *)Data;
				UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_ROOM_EVENT_KICKEDOUT, eventCause=%u"), UpdateInfo->eventCause);
			}
			break;
		case SCE_NP_MATCHING2_ROOM_EVENT_MEMBER_JOINED_A:
			{
				SceNpMatching2RoomMemberUpdateInfoA *UpdateInfo = (SceNpMatching2RoomMemberUpdateInfoA *)Data;
				UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_ROOM_EVENT_MEMBER_JOINED_A, %s"), ANSI_TO_TCHAR( UpdateInfo->roomMemberDataInternal->onlineId.data ));
			}
			break;
		case SCE_NP_MATCHING2_ROOM_EVENT_MEMBER_LEFT_A:
			{
				SceNpMatching2RoomMemberUpdateInfoA *UpdateInfo = (SceNpMatching2RoomMemberUpdateInfoA *)Data;
				UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_ROOM_EVENT_MEMBER_LEFT_A, %s"), ANSI_TO_TCHAR( UpdateInfo->roomMemberDataInternal->onlineId.data ));
			}
			break;
		case SCE_NP_MATCHING2_ROOM_EVENT_ROOM_DESTROYED:
			{
				SceNpMatching2RoomUpdateInfo *UpdateInfo = (SceNpMatching2RoomUpdateInfo *)Data;
				UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_ROOM_EVENT_ROOM_DESTROYED, eventCause=%u, errorCode=%#x"), UpdateInfo->eventCause, UpdateInfo->errorCode);
			}
			break;
		case SCE_NP_MATCHING2_ROOM_EVENT_ROOM_OWNER_CHANGED:
			{
				SceNpMatching2RoomOwnerUpdateInfo *UpdateInfo = (SceNpMatching2RoomOwnerUpdateInfo *)Data;
				UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_ROOM_EVENT_ROOM_OWNER_CHANGED, eventCause=%u, newOwner=0x%04x"), UpdateInfo->eventCause, UpdateInfo->newOwner);
			}
			break;
		case SCE_NP_MATCHING2_ROOM_EVENT_UPDATED_ROOM_DATA_INTERNAL:
			UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_ROOM_EVENT_UPDATED_ROOM_DATA_INTERNAL"));
			break;
		case SCE_NP_MATCHING2_ROOM_EVENT_UPDATED_ROOM_MEMBER_DATA_INTERNAL_A:
			UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_ROOM_EVENT_UPDATED_ROOM_MEMBER_DATA_INTERNAL_A"));
			break;
		case SCE_NP_MATCHING2_ROOM_EVENT_UPDATED_SIGNALING_OPT_PARAM:
			UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_ROOM_EVENT_UPDATED_SIGNALING_OPT_PARAM"));
			break;
		default:
			break;
	}
}

void FOnlineSessionPS4::PS4LobbyEventProcessing(
	SceNpMatching2ContextId CtxId, 
	SceNpMatching2LobbyId LobbyId, 
	SceNpMatching2Event Event, 
	const void *Data,
	void *Arg
	)
{
	switch(Event)
	{
		case SCE_NP_MATCHING2_LOBBY_EVENT_MEMBER_JOINED_A:
			{
				SceNpMatching2LobbyMemberUpdateInfoA *UpdateInfo = (SceNpMatching2LobbyMemberUpdateInfoA *)Data;
				UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_LOBBY_EVENT_MEMBER_JOINED_A, %s"), ANSI_TO_TCHAR( UpdateInfo->lobbyMemberDataInternal->onlineId.data ));
			}
			break;
		case SCE_NP_MATCHING2_LOBBY_EVENT_MEMBER_LEFT_A:
			{
				SceNpMatching2LobbyMemberUpdateInfoA *UpdateInfo = (SceNpMatching2LobbyMemberUpdateInfoA *)Data;
				UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_LOBBY_EVENT_MEMBER_LEFT_A, %s"), ANSI_TO_TCHAR( UpdateInfo->lobbyMemberDataInternal->onlineId.data ));
			}
			break;
		case SCE_NP_MATCHING2_LOBBY_EVENT_LOBBY_DESTROYED:
			{
				RemoveLobbyIdSession(LobbyId);
				SceNpMatching2LobbyUpdateInfo *UpdateInfo = (SceNpMatching2LobbyUpdateInfo *)Data;
				UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_LOBBY_EVENT_LOBBY_DESTROYED, eventCause=%u, errorCode=%#x"), UpdateInfo->eventCause, UpdateInfo->errorCode);
			}
			break;
		case SCE_NP_MATCHING2_LOBBY_EVENT_UPDATED_LOBBY_MEMBER_DATA_INTERNAL_A:
			UE_LOG_ONLINE(Log, TEXT("SCE_NP_MATCHING2_LOBBY_EVENT_UPDATED_LOBBY_MEMBER_DATA_INTERNAL_A"));
			break;
		default:
			break;
	}
}


void FOnlineSessionPS4::PS4SignalingProcessing(
	SceNpMatching2ContextId			CtxId,
	SceNpMatching2RoomId			RoomId,
	SceNpMatching2RoomMemberId		PeerMemberId,
	SceNpMatching2Event				Event,
	int								ErrorCode,
	void *							Arg
	)
{
	switch(Event)
	{
		case SCE_NP_MATCHING2_SIGNALING_EVENT_ESTABLISHED:
		{
			UE_LOG_ONLINE( Log, TEXT( "FOnlineSessionPS4::PS4SignalingProcessing: ESTABLISHED: roomId=0x%016lx, memberId=0x%04x\n" ), RoomId, PeerMemberId );
			break;
		}
		case SCE_NP_MATCHING2_SIGNALING_EVENT_DEAD:
		{
			UE_LOG_ONLINE( Log, TEXT( "FOnlineSessionPS4::PS4SignalingProcessing: DEAD: error=%#x, roomId=0x%016lx, memberId=0x%04x\n" ), ErrorCode, RoomId, PeerMemberId );
			break;
		}
		default:
		{
			UE_LOG_ONLINE( Log, TEXT( "FOnlineSessionPS4::PS4SignalingProcessing: Unhandled Event: %#x, Error=%#x\n" ), Event, ErrorCode );
			break;
		}
	}
}

bool FOnlineSessionPS4::CreateInitialContext()
{
	bool CreatedContextSuccess = false;
	
	SceNpServiceLabel ServiceLabel = 0;
	
	bool bFoundOnlineUser = false;
	SceUserServiceLoginUserIdList UserIdList;
	int Ret = sceUserServiceGetLoginUserIdList(&UserIdList);
	if(Ret<SCE_OK)
	{
		return false;
	}
	bool bStartedContext = false;
	for (SceUserServiceUserId UserId : UserIdList.userId)
	{
		if(UserId == SCE_USER_SERVICE_USER_ID_INVALID)
		{
			continue;
		}
		else
		{
			TSharedRef<FUniqueNetIdPS4> NetId = FUniqueNetIdPS4::FindOrCreate(UserId);
			SceNpMatching2ContextId Id = GetUserMatching2Context(NetId.Get(), true);
			bStartedContext = true;
			break;
		}
	}
	return bStartedContext;
}

SceNpMatching2ContextId FOnlineSessionPS4::GetUserMatching2Context(const FUniqueNetIdPS4 & UserNetId, bool bCreateIfUnavailable)
{
	if (PS4Subsystem->AreRoomsEnabled())
	{
		FScopeLock Lock(&Matching2ContextMutex);
		SceUserServiceUserId LocalUserId = UserNetId.GetUserId();
		check(LocalUserId != SCE_USER_SERVICE_USER_ID_INVALID);

		if (UserToWebApiContexts.Contains(LocalUserId))
		{
			Matching2ContextState& ContexData = UserToWebApiContexts[LocalUserId];
			return ContexData.ContextId;
		}
		else if (bCreateIfUnavailable)
		{
			SceNpMatching2ContextId NewContextId;

			SceNpServiceLabel ServiceLabel = 0;

			SceNpMatching2CreateContextParamA CreateContextParam;
			FMemory::Memset(&CreateContextParam, 0, sizeof(CreateContextParam));

			CreateContextParam.user = UserNetId.GetUserId();
			CreateContextParam.serviceLabel = ServiceLabel;
			CreateContextParam.size = sizeof(CreateContextParam);

			UE_LOG_ONLINE(Log, TEXT("FOnlineSessionPS4::DoCreateContext: Creating Context with user: %d"), CreateContextParam.user);

			const int CreateContextReturnCode = sceNpMatching2CreateContextA(&CreateContextParam, &NewContextId);
			if (CreateContextReturnCode < SCE_OK)
			{
				UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoCreateContext: Failed to create context 0x%x"), CreateContextReturnCode);
				return SCE_NP_MATCHING2_INVALID_CONTEXT_ID;
			}
			int RegisterContextCallback = sceNpMatching2RegisterContextCallback(PS4ContextCallback, nullptr);
			if (RegisterContextCallback < SCE_OK)
			{
				UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoCreateContext: Failed to register the context callback 0x%x"), RegisterContextCallback);
			}
			int RegisterRoomEventCallbackReturnCode = sceNpMatching2RegisterRoomEventCallback(NewContextId, PS4RoomEventCallback, nullptr);
			if (RegisterRoomEventCallbackReturnCode < SCE_OK)
			{
				UE_LOG_ONLINE(Error, TEXT("sceNpMatching2RegisterRoomEventCallback failed: 0x%x"), RegisterRoomEventCallbackReturnCode);
			}

			int RegisterLobbyEventCallbackReturnCode = sceNpMatching2RegisterLobbyEventCallback(NewContextId, PS4LobbyEventCallback, nullptr);
			if (RegisterLobbyEventCallbackReturnCode < SCE_OK)
			{
				UE_LOG_ONLINE(Error, TEXT("sceNpMatching2RegisterLobbyEventCallback failed: 0x%x"), RegisterLobbyEventCallbackReturnCode);
			}

			int RegisterSignalingCallbackReturnCode = sceNpMatching2RegisterSignalingCallback(NewContextId, PS4SignalingCallback, nullptr);
			if (RegisterSignalingCallbackReturnCode < SCE_OK)
			{
				UE_LOG_ONLINE(Error, TEXT("sceNpMatching2RegisterSignalingCallback failed: 0x%x"), RegisterSignalingCallbackReturnCode);
			}

			UE_LOG_ONLINE(Log, TEXT("FOnlineSessionPS4::DoCreateContext: Created context id: %d"), NewContextId);
			UserToWebApiContexts.Add(LocalUserId, Matching2ContextState(NewContextId, EMatching2ContextState::Starting));

			int32 ContextStartReturnCode = sceNpMatching2ContextStart(NewContextId, 0);
			if (ContextStartReturnCode < SCE_OK)
			{
				UserToWebApiContexts.Remove(LocalUserId);
				UE_LOG_ONLINE(Error, TEXT("Context start failed: 0x%x"), ContextStartReturnCode);
				return SCE_NP_MATCHING2_INVALID_CONTEXT_ID;
			}
			return NewContextId;
		}
	}

	return SCE_NP_MATCHING2_INVALID_CONTEXT_ID;
}

EMatching2ContextState FOnlineSessionPS4::GetUserMatching2ContextState(const FUniqueNetIdPS4& UserNetId)
{
	FScopeLock Lock(&Matching2ContextMutex);
	SceUserServiceUserId LocalUserId = UserNetId.GetUserId();
	check(LocalUserId != SCE_USER_SERVICE_USER_ID_INVALID);

	if (UserToWebApiContexts.Contains(LocalUserId))
	{
		Matching2ContextState& ContexData = UserToWebApiContexts[LocalUserId];
		return ContexData.ContextState;
	}
	return EMatching2ContextState::Invalid;
}

EMatching2ContextState FOnlineSessionPS4::GetUserMatching2ContextState(SceNpMatching2ContextId ContextId)
{
	FScopeLock Lock(&Matching2ContextMutex);
	for (auto Iter = UserToWebApiContexts.CreateIterator(); Iter; ++Iter)
	{
		if (Iter.Value().ContextId == ContextId)
		{
			return Iter.Value().ContextState;
		}
	}
	return EMatching2ContextState::Invalid;
}
void FOnlineSessionPS4::SetUserMatching2ContextState(const FUniqueNetIdPS4& UserNetId, EMatching2ContextState State)
{
	FScopeLock Lock(&Matching2ContextMutex);
	SceUserServiceUserId LocalUserId = UserNetId.GetUserId();
	check(LocalUserId != SCE_USER_SERVICE_USER_ID_INVALID);

	if (UserToWebApiContexts.Contains(LocalUserId))
	{
		Matching2ContextState& ContexData = UserToWebApiContexts[LocalUserId];
		ContexData.ContextState = State;
	}
}

void FOnlineSessionPS4::SetUserMatching2ContextState(SceNpMatching2ContextId ContextId, EMatching2ContextState State)
{
	FScopeLock Lock(&Matching2ContextMutex);
	for (auto Iter = UserToWebApiContexts.CreateIterator(); Iter; ++Iter)
	{
		if (Iter.Value().ContextId == ContextId)
		{
			Iter.Value().ContextState = State;
		}
	}
}

bool FOnlineSessionPS4::DoDestroyContext(const FUniqueNetIdPS4& PlayerId)
{	
	FScopeLock Lock(&Matching2ContextMutex);
	bool DestroyContextSuccess = true;
	SceNpMatching2ContextId ContextId = GetUserMatching2Context(PlayerId, false);
	if (ContextId != SCE_NP_MATCHING2_INVALID_CONTEXT_ID)
	{
		int DestroyContextReturnCode = sceNpMatching2DestroyContext(ContextId);
		if (DestroyContextReturnCode<SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("sceNpMatching2DestroyContext failed: 0x%x"),DestroyContextReturnCode);
			DestroyContextSuccess = false;
		}

		SceUserServiceUserId LocalUserId = PlayerId.GetUserId();
		check(LocalUserId != SCE_USER_SERVICE_USER_ID_INVALID);

		UserToWebApiContexts.Remove(LocalUserId);
	}
	return DestroyContextSuccess;
}

bool FOnlineSessionPS4::DoDestroyContext(SceNpMatching2ContextId ContextId)
{
	FScopeLock Lock(&Matching2ContextMutex);
	bool DestroyContextSuccess = false;
	for (auto Iter = UserToWebApiContexts.CreateIterator(); Iter; ++Iter)
	{
		if (Iter.Value().ContextId == ContextId)
		{
			int32 DestroyContextReturnCode = sceNpMatching2DestroyContext(ContextId);
			DestroyContextSuccess = DestroyContextReturnCode == SCE_OK;
			if (DestroyContextReturnCode < SCE_OK)
			{
				UE_LOG_ONLINE(Error, TEXT("sceNpMatching2DestroyContext failed: 0x%x"), DestroyContextReturnCode);				
			}		
			UserToWebApiContexts.Remove(Iter.Key());
			break;
		}
	}
	return DestroyContextSuccess;
}

bool FOnlineSessionPS4::DoStopContext(const FUniqueNetIdPS4& PlayerId)
{	
	FScopeLock Lock(&Matching2ContextMutex);
	bool StopContextSuccess = true;
	SceNpMatching2ContextId ContextId = GetUserMatching2Context(PlayerId, false);
	if (ContextId != SCE_NP_MATCHING2_INVALID_CONTEXT_ID)
	{
		int ContextStopReturnCode = sceNpMatching2ContextStop(ContextId);
		if (ContextStopReturnCode < SCE_OK)
		{
			StopContextSuccess = false;
			UE_LOG_ONLINE(Error, TEXT("Context stop failed: 0x%x"),ContextStopReturnCode);
		}
	}
	return StopContextSuccess;
}

void FOnlineSessionPS4::DoStopAllContexts()
{
	FScopeLock Lock(&Matching2ContextMutex);
	for (auto Iter = UserToWebApiContexts.CreateIterator(); Iter; ++Iter)
	{
		SceNpMatching2ContextId ContextId = Iter.Value().ContextId;
		int ContextStopReturnCode = sceNpMatching2ContextStop(ContextId);
		if (ContextStopReturnCode < SCE_OK)
		{			
			UE_LOG_ONLINE(Warning, TEXT("DoStopAllContexts: Context stop failed: id: 0x%x, Error: 0x%x"), ContextId, ContextStopReturnCode);
		}
	}
}

void FOnlineSessionPS4::DoDestroyAllContexts()
{
	FScopeLock Lock(&Matching2ContextMutex);
	for (auto Iter = UserToWebApiContexts.CreateIterator(); Iter; ++Iter)
	{
		SceNpMatching2ContextId ContextId = Iter.Value().ContextId;
		int32 DestroyContextReturnCode = sceNpMatching2DestroyContext(ContextId);
		if (DestroyContextReturnCode < SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("DoDestroyAllContexts:: sceNpMatching2DestroyContext failed: id: 0x%x, Error: 0x%x"), ContextId, DestroyContextReturnCode);			
		}		
	}
	UserToWebApiContexts.Empty();
}

bool FOnlineSessionPS4::DoGetWorldInfoList(SceNpMatching2ContextId Matching2Context)
{
	if (GetUserMatching2ContextState(Matching2Context) != EMatching2ContextState::Started)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't get world info with context: 0x%x in state: %i"), Matching2Context, (int)GetUserMatching2ContextState(Matching2Context));
		return false;
	}

	bool GetWorldInfoListSuccess = false;

	SceNpMatching2GetWorldInfoListRequest WorldInfoListRequestParameters;
	FMemory::Memset(&WorldInfoListRequestParameters, 0, sizeof(WorldInfoListRequestParameters));
	SceNpMatching2RequestOptParam RequestOptParameters;
	FMemory::Memset(&RequestOptParameters, 0, sizeof(RequestOptParameters));

	RequestOptParameters.cbFunc = PS4RequestCallback;

	WorldInfoListRequestParameters.serverId = ServerId;

	int GetWorldInfoListReturnCode = sceNpMatching2GetWorldInfoList(Matching2Context, &WorldInfoListRequestParameters, &RequestOptParameters, &RequestId);
	if (GetWorldInfoListReturnCode<SCE_OK)
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to get world info list - 0x%x"),GetWorldInfoListReturnCode);
	}
	else
	{
		GetWorldInfoListSuccess = true;
	}
	return GetWorldInfoListSuccess;
}

int32 FOnlineSessionPS4::SessionSettingToExternalIntAttrID(const FName& SessionSetting)
{		
	int32* SettingId = SessionSettingMapping.Find(SessionSetting);
	if (SettingId)
	{
		return *SettingId;
	}
	return -1;
}

bool FOnlineSessionPS4::DoCreateJoinRoom(FNamedOnlineSession* Session, SceNpMatching2ContextId UserMatching2Context, FOnlineAsyncTaskPS4SessionPutChangeableSessionData* PutDataTask)
{
	if (!PS4Subsystem->AreRoomsEnabled())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoCreateJoinRoom: Rooms are disabled."));
		return false;
	}

	if (WorldId == SCE_NP_MATCHING2_INVALID_WORLD_ID)
	{
		DoGetWorldInfoList(UserMatching2Context);

		// We don't have a valid world
		UE_LOG_ONLINE( Error, TEXT( "FOnlineSessionPS4::DoCreateJoinRoom: No valid world is currently set." ) );
		return false;
	}
	
	while (GetUserMatching2ContextState(UserMatching2Context) == EMatching2ContextState::Starting)
	{
		FPlatformProcess::Sleep(0.2f);
	}

	if (GetUserMatching2ContextState(UserMatching2Context) != EMatching2ContextState::Started)
	{
		// We don't have a valid world
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoCreateJoinRoom: Matching2Context: 0x%x in bad state: %i."), UserMatching2Context, (int)GetUserMatching2ContextState(UserMatching2Context));
		return false;
	}

	SceNpMatching2CreateJoinRoomRequestA CreateJoinRoomRequestParameters;
	FMemory::Memset(&CreateJoinRoomRequestParameters, 0, sizeof(CreateJoinRoomRequestParameters));
	SceNpMatching2RequestOptParam RequestOptionParameters;
	FMemory::Memset(&RequestOptionParameters, 0, sizeof(RequestOptionParameters));

	RequestOptionParameters.cbFunc = PS4RequestCallback;
	RequestOptionParameters.cbFuncArg = PutDataTask;

	//@todo there can be multiple worlds how does session represent that, it looks like the platform specific data in other subsystems is only created when the session is created rather than allowing for platform specific data to be used in the creation
	//@todo this would also relate to other session settings for PS4, e.g. room password, teams etc
	CreateJoinRoomRequestParameters.worldId = WorldId;
	//@todo are we going to use lobbys?
	//CreateJoinRoomRequestParameters.lobbyId =
	CreateJoinRoomRequestParameters.maxSlot = Session->NumOpenPrivateConnections + Session->NumOpenPublicConnections;


	SceNpMatching2SignalingOptParam SignallingOptionParameters;
	FMemory::Memset(&SignallingOptionParameters, 0, sizeof(SignallingOptionParameters));
	SignallingOptionParameters.type = SCE_NP_MATCHING2_SIGNALING_TYPE_MESH;
	CreateJoinRoomRequestParameters.sigOptParam = &SignallingOptionParameters;

	const int32 SessionDataSize = 128;
	char SessionData[SessionDataSize] = {};
	FString SessionGameType = "";
	FString SessionMapName = "";
	FString ParserStr = "?"; //string to use to parse through the data that we're storing in a string
	
	Session->SessionSettings.Get(SETTING_GAMEMODE, SessionGameType);
	Session->SessionSettings.Get(SETTING_MAPNAME, SessionMapName);

	FPlatformString::Strcpy(SessionData, SessionDataSize, TCHAR_TO_ANSI(*Session->SessionInfo->GetSessionId().ToString()));
	FPlatformString::Strcat(SessionData, SessionDataSize, TCHAR_TO_ANSI(*ParserStr));
	FPlatformString::Strcat(SessionData, SessionDataSize, TCHAR_TO_ANSI(*SessionGameType));
	FPlatformString::Strcat(SessionData, SessionDataSize, TCHAR_TO_ANSI(*ParserStr));
	FPlatformString::Strcat(SessionData, SessionDataSize, TCHAR_TO_ANSI(*SessionMapName));
	FPlatformString::Strcat(SessionData, SessionDataSize, TCHAR_TO_ANSI(*ParserStr));

	SceNpMatching2BinAttr & SessionDataAttr = *new SceNpMatching2BinAttr{};
	SessionDataAttr.id = SCE_NP_MATCHING2_ROOM_SEARCHABLE_BIN_ATTR_EXTERNAL_1_ID;
	SessionDataAttr.ptr = SessionData;
	SessionDataAttr.size = FPlatformString::Strlen(SessionData) + 1;
	CreateJoinRoomRequestParameters.roomSearchableBinAttrExternal = &SessionDataAttr;
	CreateJoinRoomRequestParameters.roomSearchableBinAttrExternalNum = 1;

	//extract user-defined integer params used for search filters.
	SceNpMatching2IntAttr ExternalIntSearchAttrs[SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM];
	FMemory::Memset(ExternalIntSearchAttrs, 0);
	int32 NumExternalIntAttrs = 0;
	for (FSessionSettings::TConstIterator It(Session->SessionSettings.Settings); It; ++It)
	{
		FName Key = It.Key();
		const FOnlineSessionSetting& Setting = It.Value();

		//only set keys that are in our map so the mapping remains stable.
		int32 PS4SearchId = SessionSettingToExternalIntAttrID(Key);
		if (PS4SearchId != -1)
		{
			if (Setting.AdvertisementType >= EOnlineDataAdvertisementType::ViaOnlineService)
			{
				if (Setting.Data.GetType() == EOnlineKeyValuePairDataType::Int32)
				{
					int32 SettingValue;
					Setting.Data.GetValue(SettingValue);

					ExternalIntSearchAttrs[NumExternalIntAttrs].id = PS4SearchId;
					ExternalIntSearchAttrs[NumExternalIntAttrs].num = SettingValue;
					++NumExternalIntAttrs;
				}
			}
		}
	}

	CreateJoinRoomRequestParameters.roomSearchableIntAttrExternal = ExternalIntSearchAttrs;
	CreateJoinRoomRequestParameters.roomSearchableIntAttrExternalNum = NumExternalIntAttrs;

	int CreateJoinRoomReturnCode = sceNpMatching2CreateJoinRoomA(UserMatching2Context, &CreateJoinRoomRequestParameters, &RequestOptionParameters, &RequestId);
	if (CreateJoinRoomReturnCode<SCE_OK)
	{
		UE_LOG_ONLINE(Error, TEXT("Failed to create and join a room - 0x%x"),CreateJoinRoomReturnCode);
		return false;
	}

	return true;
}

int32 SessionCompareToNP2Compare(EOnlineComparisonOp::Type InCompareOp)
{
	switch (InCompareOp)
	{
	case EOnlineComparisonOp::Equals:
		return SCE_NP_MATCHING2_OPERATOR_EQ;
	case EOnlineComparisonOp::NotEquals:
		return SCE_NP_MATCHING2_OPERATOR_NE;
	case EOnlineComparisonOp::GreaterThan:
		return SCE_NP_MATCHING2_OPERATOR_GT;
	case EOnlineComparisonOp::GreaterThanEquals:
		return SCE_NP_MATCHING2_OPERATOR_GE;
	case EOnlineComparisonOp::LessThan:
		return SCE_NP_MATCHING2_OPERATOR_LT;
	case EOnlineComparisonOp::LessThanEquals:
		return SCE_NP_MATCHING2_OPERATOR_LE;
	default:
		checkf(false, TEXT("unsupported searchtype: %i"), (int32)InCompareOp);
		return 0;
	}
}

bool FOnlineSessionPS4::DoSearchRooms(const TSharedRef<FOnlineSessionSearch>& SearchSettings, const FUniqueNetIdPS4& SearchingPlayerId)
{
	if (!PS4Subsystem->AreRoomsEnabled())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoSearchRooms: Rooms are disabled."));
		return false;
	}

	SceNpMatching2ContextId SearchingPlayerContextId = GetUserMatching2Context(SearchingPlayerId, true);

	//todo mw: Make room searching check the context in an async way.
	while (GetUserMatching2ContextState(SearchingPlayerContextId) == EMatching2ContextState::Starting)
	{
		FPlatformProcess::Sleep(0.2f);
	}

	if (GetUserMatching2ContextState(SearchingPlayerContextId) != EMatching2ContextState::Started)
	{
		// We don't have a valid world
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoCreateJoinRoom: Matching2Context: 0x%x in bad state: %i."), SearchingPlayerContextId, (int)GetUserMatching2ContextState(SearchingPlayerContextId));
		return false;
	}

	if ( WorldId == SCE_NP_MATCHING2_INVALID_WORLD_ID )
	{
		DoGetWorldInfoList(SearchingPlayerContextId);

		// We don't have a valid world
		UE_LOG_ONLINE( Error, TEXT( "FOnlineSessionPS4::DoSearchRooms: No valid world is currently set." ) );
		return false;
	}

	SceNpMatching2IntSearchFilter CustomSearchFilters[SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_NUM];
	FMemory::Memset(CustomSearchFilters, 0);
	int32 NumFilters = 0;
	for (FSearchParams::TConstIterator It(SearchSettings->QuerySettings.SearchParams); It; ++It)
	{
		const FName Key = It.Key();
		const FOnlineSessionSearchParam& SearchParam = It.Value();
		
		// Game server keys are skipped
		if (Key == SEARCH_DEDICATED_ONLY || Key == SETTING_MAPNAME || Key == SEARCH_EMPTY_SERVERS_ONLY || Key == SEARCH_SECURE_SERVERS_ONLY || Key == SEARCH_PRESENCE)
		{
			continue;
		}

		SceNpMatching2IntSearchFilter& CurrentFilter = CustomSearchFilters[NumFilters];
		int32 NP2CompareOperator = SessionCompareToNP2Compare(SearchParam.ComparisonOp);
		switch (SearchParam.Data.GetType())
		{
			case EOnlineKeyValuePairDataType::Int32:
			{														   
				int32 SearchParamId = SessionSettingToExternalIntAttrID(Key);
				int32 Value;
				SearchParam.Data.GetValue(Value);
				
				if ((SearchParamId >= SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_1_ID) && (SearchParamId <= SCE_NP_MATCHING2_ROOM_SEARCHABLE_INT_ATTR_EXTERNAL_8_ID))
				{
					CurrentFilter.searchOperator = NP2CompareOperator;
					CurrentFilter.attr.num = Value;
					CurrentFilter.attr.id = SearchParamId;

					++NumFilters;
				}
				else
				{
					UE_LOG_ONLINE(Log, TEXT("Unable to set unknown search parameter id %s: %s"), *Key.ToString(), *SearchParam.ToString());
				}
				break;
			}			
			default:
				UE_LOG_ONLINE(Warning, TEXT("Unable to set search parameter %s: %s"), *Key.ToString(), *SearchParam.ToString());
				break;
		}
	}	

	SceNpMatching2SearchRoomRequest SearchRoomRequestParameters;
	FMemory::Memset(&SearchRoomRequestParameters, 0, sizeof(SearchRoomRequestParameters));
	SceNpMatching2RequestOptParam RequestOptionParameters;
	FMemory::Memset(&RequestOptionParameters, 0, sizeof(RequestOptionParameters));

	RequestOptionParameters.cbFunc = PS4RequestCallback;
	RequestOptionParameters.cbFuncArg = &SearchSettings.Get();

	SearchRoomRequestParameters.worldId = WorldId;
	SearchRoomRequestParameters.option = SCE_NP_MATCHING2_SEARCH_ROOM_OPTION_RANDOM;
	verify(SearchSettings->MaxSearchResults <= SCE_NP_MATCHING2_RANGE_FILTER_MAX);
	SearchRoomRequestParameters.rangeFilter.max = SearchSettings->MaxSearchResults;
	const SceNpMatching2AttributeId AttrId = SCE_NP_MATCHING2_ROOM_SEARCHABLE_BIN_ATTR_EXTERNAL_1_ID;
	SearchRoomRequestParameters.attrId = &AttrId;
	SearchRoomRequestParameters.attrIdNum = 1;

	SearchRoomRequestParameters.intFilter = CustomSearchFilters;
	SearchRoomRequestParameters.intFilterNum = NumFilters;

	QuickmatchSearchingPlayerId = SearchingPlayerId.AsShared();

	const int SearchRoomReturnCode = sceNpMatching2SearchRoom(SearchingPlayerContextId, &SearchRoomRequestParameters, &RequestOptionParameters, &RequestId);

	if ( SearchRoomReturnCode < SCE_OK )
	{
		UE_LOG_ONLINE( Error, TEXT( "Failed to search rooms - 0x%x" ), SearchRoomReturnCode );
		return false;
	}

	return true;
}

bool FOnlineSessionPS4::DoJoinRoom(const FUniqueNetIdPS4& JoiningPlayerId, SceNpMatching2ContextId UserMatching2Context, FNamedOnlineSession* Session, const FOnlineSession* SearchSession)
{
	if (!PS4Subsystem->AreRoomsEnabled())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoJoinRoom: Rooms are disabled."));
		return false;
	}

	while (GetUserMatching2ContextState(UserMatching2Context) == EMatching2ContextState::Starting)
	{
		FPlatformProcess::Sleep(0.2f);
	}

	if (GetUserMatching2ContextState(UserMatching2Context) != EMatching2ContextState::Started)
	{
		// We don't have a valid world
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoCreateJoinRoom: Matching2Context: 0x%x in bad state: %i."), UserMatching2Context, (int)GetUserMatching2ContextState(UserMatching2Context));
		return false;
	}

	bool JoinRoomSuccess = false;

	Session->SessionState = EOnlineSessionState::Pending;

	if (Session->SessionInfo.IsValid())
	{
		FOnlineSessionInfoPS4* PS4SessionInfo = (FOnlineSessionInfoPS4*)(Session->SessionInfo.Get());
		if (PS4SessionInfo->SessionType == EPS4Session::RoomSession && PS4SessionInfo->RoomId != SCE_NP_MATCHING2_INVALID_ROOM_ID)
		{
			// Copy the session info over
			const FOnlineSessionInfoPS4* SearchSessionInfo = (const FOnlineSessionInfoPS4*)SearchSession->SessionInfo.Get();
			PS4SessionInfo->SessionId = SearchSessionInfo->SessionId;
			PS4SessionInfo->WorldId = SearchSessionInfo->WorldId;
			PS4SessionInfo->LobbyId = SearchSessionInfo->LobbyId;
			PS4SessionInfo->RoomId = SearchSessionInfo->RoomId;

			SceNpMatching2JoinRoomRequestA JoinRoomRequestParameters;
			FMemory::Memset(&JoinRoomRequestParameters, 0, sizeof(JoinRoomRequestParameters));
			SceNpMatching2RequestOptParam RequestOptionParameters;
			FMemory::Memset(&RequestOptionParameters, 0, sizeof(RequestOptionParameters));

			RequestOptionParameters.cbFunc = PS4RequestCallback;
			RequestOptionParameters.cbFuncArg = new FName(Session->SessionName);

			JoinRoomRequestParameters.roomId = PS4SessionInfo->RoomId;

			int JoinRoomReturnCode = sceNpMatching2JoinRoomA(UserMatching2Context, &JoinRoomRequestParameters, &RequestOptionParameters, &RequestId);
			if (JoinRoomReturnCode<SCE_OK)
			{
				UE_LOG_ONLINE(Error, TEXT("Failed to join room - 0x%x"),JoinRoomReturnCode);
			}
			else
			{
				JoinRoomSuccess = true;
			}
		}
	}

	return JoinRoomSuccess;
}

bool FOnlineSessionPS4::DoJoinLobby(int32 PlayerNum, FNamedOnlineSession* Session, const FOnlineSession* SearchSession)
{
	if (!PS4Subsystem->AreRoomsEnabled())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoJoinLobby: Rooms are disabled."));
		return false;
	}

	bool JoinLobbySuccess = false;

	Session->SessionState = EOnlineSessionState::Pending;	
	if (Session->SessionInfo.IsValid())
	{
		FOnlineSessionInfoPS4* PS4SessionInfo = (FOnlineSessionInfoPS4*)(Session->SessionInfo.Get());
		if (PS4SessionInfo->SessionType == EPS4Session::LobbySession && PS4SessionInfo->LobbyId != SCE_NP_MATCHING2_INVALID_LOBBY_ID)
		{
			// Copy the session info over
			const FOnlineSessionInfoPS4* SearchSessionInfo = (const FOnlineSessionInfoPS4*)SearchSession->SessionInfo.Get();
			PS4SessionInfo->SessionId = SearchSessionInfo->SessionId;
			PS4SessionInfo->WorldId = SearchSessionInfo->WorldId;
			PS4SessionInfo->LobbyId = SearchSessionInfo->LobbyId;
			PS4SessionInfo->RoomId = SCE_NP_MATCHING2_INVALID_ROOM_ID;

			SceNpMatching2JoinLobbyRequest JoinLobbyRequestParameters;
			FMemory::Memset(&JoinLobbyRequestParameters, 0, sizeof(JoinLobbyRequestParameters));
			SceNpMatching2RequestOptParam RequestOptionParameters;
			FMemory::Memset(&RequestOptionParameters, 0, sizeof(RequestOptionParameters));

			RequestOptionParameters.cbFunc = PS4RequestCallback;

			JoinLobbyRequestParameters.lobbyId = PS4SessionInfo->LobbyId;

			TSharedPtr<const FUniqueNetIdPS4> UserNetId = FUniqueNetIdPS4::Cast(Session->bHosting ? Session->OwningUserId : Session->LocalOwnerId);
			SceNpMatching2ContextId Matching2Context = GetUserMatching2Context(*UserNetId, false);
			int JoinLobbyReturnCode = sceNpMatching2JoinLobby(Matching2Context, &JoinLobbyRequestParameters, &RequestOptionParameters, &RequestId);
			if (JoinLobbyReturnCode<SCE_OK)
			{
				UE_LOG_ONLINE(Error, TEXT("Failed to join room - 0x%x"),JoinLobbyReturnCode);
			}
			else
			{
				JoinLobbySuccess = true;
			}
		}
	}

	return JoinLobbySuccess;
}

bool FOnlineSessionPS4::DoLeaveRoom(FNamedOnlineSession* Session, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	if (!PS4Subsystem->AreRoomsEnabled())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoLeaveRoom: Rooms are disabled."));
		return false;
	}

	FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
	if (AsyncTaskManager)
	{
		auto NewTask = new FOnlineAsyncTaskPS4LeaveRoom(PS4Subsystem, Session, CompletionDelegate, RequestId);
		AsyncTaskManager->AddToInQueue(NewTask);
		return true;
	}

	UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoLeaveRoom: No Async Task Manager"));
	return false;
}

bool FOnlineSessionPS4::DoLeaveSession(FNamedOnlineSession* Session, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	TSharedPtr<const FUniqueNetIdPS4> PlayerId = FUniqueNetIdPS4::Cast(Session->bHosting ? Session->OwningUserId : Session->LocalOwnerId);
	(new AutoDeleteSessionLeaveTask(PS4Subsystem, PS4Subsystem->GetUserWebApiContext(*PlayerId), Session, CompletionDelegate))->StartBackgroundTask();

	return true;
}

bool FOnlineSessionPS4::DoPingSearchResults(const FUniqueNetIdPS4& SearchingPlayerId, FOnlineSessionSearch & SearchSettings )
{
	if (!PS4Subsystem->AreRoomsEnabled())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoPingSearchResults: Rooms are disabled."));
		return false;
	}

	bool PingSearchResultsSuccess = false;

	for (int i = 0; i<SearchSettings.SearchResults.Num(); ++i)
	{
		SceNpMatching2SignalingGetPingInfoRequest GetPingRequestParameters;
		FMemory::Memset(&GetPingRequestParameters, 0, sizeof(GetPingRequestParameters));
		SceNpMatching2RequestOptParam RequestOptionParameters;
		FMemory::Memset(&RequestOptionParameters, 0, sizeof(RequestOptionParameters));

		RequestOptionParameters.cbFunc = PS4RequestCallback;
		RequestOptionParameters.cbFuncArg = &SearchSettings.SearchResults[i];

		SearchSettings.SearchResults[i].PingInMs = -1.0f;
		
		FOnlineSession& Session = SearchSettings.SearchResults[i].Session;
		FOnlineSessionInfoPS4 * PS4SessionInfo = (FOnlineSessionInfoPS4 *)Session.SessionInfo.Get();

		GetPingRequestParameters.roomId = PS4SessionInfo->RoomId;
		
		SceNpMatching2ContextId Matching2ContextId = GetUserMatching2Context(SearchingPlayerId, false);
		const int GetPingReturnCode = sceNpMatching2SignalingGetPingInfo(Matching2ContextId, &GetPingRequestParameters, &RequestOptionParameters, &RequestId);
		if (GetPingReturnCode<SCE_OK)
		{
			UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::DoPingSearchResults: Failed to get ping info - 0x%x"),GetPingReturnCode);
		}
		else
		{
			PingSearchResultsSuccess |= true;
		}
	}
	
	return PingSearchResultsSuccess;

}

void FOnlineSessionPS4::OnSessionCreateComplete(const SessionCreateTaskData & InData)
{
	if (!InData.bWasSuccessful)
	{
		RemoveNamedSession(InData.SessionName);
	}

	TriggerOnCreateSessionCompleteDelegates(InData.SessionName, InData.bWasSuccessful);
}

void FOnlineSessionPS4::OnSessionDestroyComplete(FName SessionName, bool bWasSuccessful, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	IOnlineVoicePtr VoiceInt = PS4Subsystem->GetVoiceInterface();
	if (VoiceInt.IsValid())
	{
		VoiceInt->UnregisterLocalTalkers();
	}

	// The session info is no longer needed
	RemoveNamedSession(SessionName);

	CompletionDelegate.ExecuteIfBound(SessionName, bWasSuccessful);
	TriggerOnDestroySessionCompleteDelegates(SessionName, bWasSuccessful);
}

void FOnlineSessionPS4::OnSessionJoinComplete(FName SessionName, const FOnlineSessionSearchResult& DesiredSession, EOnJoinSessionCompleteResult::Type Result)
{
	if (Result != EOnJoinSessionCompleteResult::Success)
	{		
		TriggerOnJoinSessionCompleteDelegates(SessionName, Result);
		return;
	}

	bool Return = false;

	FNamedOnlineSession * Session = GetNamedSession(SessionName);

	//This happened to be null when canceling quickmatch at one point
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("OnSessionJoinComplete: No session."));
		return;
	}

	// Create Internet or LAN match
	if (!Session->SessionSettings.bIsLANMatch)
	{
		if (DesiredSession.Session.SessionInfo.IsValid())
		{
			const FOnlineSessionInfoPS4* SearchSessionInfo = (const FOnlineSessionInfoPS4*)DesiredSession.Session.SessionInfo.Get();

			EPS4Session::Type SessionType = PS4Subsystem->AreRoomsEnabled() ? EPS4Session::RoomSession : EPS4Session::StandaloneSession;
			FOnlineSessionInfoPS4* NewSessionInfo = new FOnlineSessionInfoPS4(SessionType, SearchSessionInfo->WorldId, SearchSessionInfo->LobbyId, SearchSessionInfo->RoomId, SearchSessionInfo->SessionId);
			Session->SessionInfo = MakeShareable(NewSessionInfo);

			if (SessionType == EPS4Session::RoomSession)
			{
				TSharedPtr<const FUniqueNetIdPS4> UserNetId = FUniqueNetIdPS4::Cast(Session->LocalOwnerId);
				Return = DoJoinRoom(*UserNetId, GetUserMatching2Context(*UserNetId, false), Session, &DesiredSession.Session);
			}
			else
			{
				Session->SessionState = EOnlineSessionState::Pending;
				Return = true;
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Invalid session info on search result"), *SessionName.ToString());
		}
	}
	else
	{
		//@todo need to check this
		UE_LOG_ONLINE(Log, TEXT("Joining LAN Sessions unsuported on PS4"));
	}

	if (Return)
	{
		// Clean up the session info so we don't get into a confused state
		RegisterLocalPlayers(Session);
		if (!PS4Subsystem->AreRoomsEnabled())
		{
			// Just trigger the delegate as having failed
			TriggerOnJoinSessionCompleteDelegates(SessionName, Result);
		}
	}
	else
	{
		// Just trigger the delegate as having failed
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
	}
}

void FOnlineSessionPS4::OnSessionInviteAccepted(const FString& LocalUserIdStr, const FString& SessionId)
{
	SceUserServiceUserId LocalUserId = FCString::Atoi(*LocalUserIdStr);
	TSharedPtr<FUniqueNetIdPS4 const> PlayerId = FUniqueNetIdPS4::FindOrCreate(LocalUserId);

	if (PlayerId->IsValid())
	{
		if (PS4Subsystem->AreRoomsEnabled())
		{
			(new AutoDeleteSessionGetChangeableSessionDataTask(PS4Subsystem, PS4Subsystem->GetUserWebApiContext(*PlayerId), PlayerId, SessionId, FOnlineAsyncTaskPS4SessionGetChangeableSessionData::FOnSessionGetChangeableSessionDataComplete::CreateRaw(this, &FOnlineSessionPS4::HandleGetChangeableDataComplete)))->StartBackgroundTask();
		}
		else
		{
			FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
			if (AsyncTaskManager != nullptr)
			{
				CurrentSessionSearch = MakeShareable(new FOnlineSessionSearch);
				CurrentSessionSearch->bIsLanQuery = false;
				CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::InProgress;

				FOnlineSessionSearchResult * NewResult = new(CurrentSessionSearch->SearchResults)FOnlineSessionSearchResult();

				EPS4Session::Type SessionType = EPS4Session::StandaloneSession;
				FOnlineSessionInfoPS4* NewSessionInfo = new FOnlineSessionInfoPS4(SessionType, 0, 0, 0, FUniqueNetIdString(SessionId));
				NewSessionInfo->Init();
				NewResult->Session.SessionInfo = MakeShareable(NewSessionInfo);

				// This delegate will get called after the changeable session data retrieval call at the end of the FOnlineAsyncTaskPS4GetSession task.
				FOnlineAsyncTaskPS4SessionGetChangeableSessionData::FOnSessionGetChangeableSessionDataComplete Delegate = FOnlineAsyncTaskPS4SessionGetChangeableSessionData::FOnSessionGetChangeableSessionDataComplete::CreateRaw(this, &FOnlineSessionPS4::HandleGetChangeableDataCompleteRoomsDisabled);

				auto NewGetSessionTask = new FOnlineAsyncTaskPS4GetSession(PS4Subsystem, PlayerId.ToSharedRef(), CurrentSessionSearch, FOnGetSessionCompleteDelegate::CreateRaw(this, &FOnlineSessionPS4::OnGetSessionCompleted, Delegate));
				AsyncTaskManager->AddToInQueue(NewGetSessionTask);
			}
			else
			{
				UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionPS4::OnSessionInviteAccepted - No AsyncTaskManager, couldn't get session data."));
			}
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionPS4::OnSessionInviteAccepted - Couldn't find local user to accept invite"));
	}
}

void FOnlineSessionPS4::OnRoomLeaveComplete(FName SessionName, bool bWasSuccessful, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	if (!PS4Subsystem->AreRoomsEnabled())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::OnRoomLeaveComplete: Rooms are disabled."));
		return;
	}

	// Find the session in question
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't destroy a null online session (%s)"), *SessionName.ToString());
		return;
	}

	DoLeaveOrDestroySession(Session, CompletionDelegate);
}

void FOnlineSessionPS4::HandleGetChangeableDataComplete(int32 LocalUserNum, const FString& SessionId, const FString& ChangeableSessionData, bool bWasSuccessful)
{
	if(bWasSuccessful && PS4Subsystem->AreRoomsEnabled())
	{
		//this needs to persist throughout the lifetime of the request -- should probably cache somewhere and delete when it's safe
		SceNpMatching2GetRoomDataExternalListRequest * Request = new SceNpMatching2GetRoomDataExternalListRequest;
		SceNpMatching2RoomId RoomId = FPlatformString::Atoi64(*ChangeableSessionData);
		Request->roomId = &RoomId;
		Request->roomIdNum = 1;
		SceNpMatching2AttributeId AttrId = SCE_NP_MATCHING2_ROOM_SEARCHABLE_BIN_ATTR_EXTERNAL_1_ID;
		Request->attrId = &AttrId;
		Request->attrIdNum = 1;		
		SceNpMatching2RequestOptParam OptParam;
		OptParam.cbFunc = PS4RequestCallback;
		OptParam.cbFuncArg = new int32(LocalUserNum); //needs to persist
		TSharedPtr<const FUniqueNetIdPS4> UserNetId;
		IOnlineIdentityPtr Identity = PS4Subsystem->GetIdentityInterface();
		if (!Identity.IsValid())
		{
			UE_LOG_ONLINE(Warning, TEXT("Failed to get IdentityInterface in HandleGetChangeableDataComplete()"));
			return;
		}
		UserNetId = FUniqueNetIdPS4::Cast(Identity->GetUniquePlayerId(LocalUserNum));
		if (!UserNetId.IsValid())
		{
			UE_LOG_ONLINE(Warning, TEXT("Failed to get UserNetId in HandleGetChangeableDataComplete()"));
			return;
		}
		int ret = sceNpMatching2GetRoomDataExternalList(GetUserMatching2Context(*UserNetId, false), Request, &OptParam, &RequestId);
		if (ret < 0)
		{
			UE_LOG_ONLINE(Warning, TEXT("Failed to GetRoomDataExternalList in HandleGetChangeableDataComplete() with error code [%x]"), ret);
			return;
		}
	}
}

void FOnlineSessionPS4::HandleGetChangeableDataCompleteRoomsDisabled(int32 LocalUserNum, const FString& SessionId, const FString& ChangeableSessionData, bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		CurrentSessionSearch->SearchResults[0].Session.SessionSettings.Set(SETTING_CUSTOM, ChangeableSessionData, EOnlineDataAdvertisementType::DontAdvertise);
	}

	IOnlineIdentityPtr Identity = PS4Subsystem->GetIdentityInterface();
	TSharedPtr<const FUniqueNetIdPS4> UserNetId;
	if (Identity.IsValid())
	{
		UserNetId = FUniqueNetIdPS4::Cast(Identity->GetUniquePlayerId(LocalUserNum));
		if (!UserNetId.IsValid())
		{
			UE_LOG_ONLINE(Warning, TEXT("Failed to get UserNetId in OnGetSessionDataCompleted()"));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("PS4 Identity Subsystem invalid"));
	}

	if (!UserNetId.IsValid())
	{
		bWasSuccessful = false;
	}
	TriggerOnSessionUserInviteAcceptedDelegates(bWasSuccessful, LocalUserNum, UserNetId, CurrentSessionSearch->SearchResults[0]);
}

void FOnlineSessionPS4::HandleGetChangeableDataCompleteFindSessionById(int32 LocalUserNum, const FString& SessionId, const FString& ChangeableSessionData, bool bWasSuccessful, FOnSingleSessionResultCompleteDelegate CompletionDelegates)
{
	if (bWasSuccessful)
	{
		CurrentSessionSearch->SearchResults[0].Session.SessionSettings.Set(SETTING_CUSTOM, ChangeableSessionData, EOnlineDataAdvertisementType::DontAdvertise);
	}
	CompletionDelegates.ExecuteIfBound(LocalUserNum, bWasSuccessful, CurrentSessionSearch->SearchResults[0]);
}

void FOnlineSessionPS4::OnGetSessionDataCompletedFindSession(const FUniqueNetId& UserId, TSharedPtr<FOnlineSessionSearch> OnlineSessionSearch, const FString& SessionData, bool bWasSuccessful, FOnSingleSessionResultCompleteDelegate CompletionDelegates)
{
	if (bWasSuccessful)
	{
		OnlineSessionSearch->SearchResults[0].Session.SessionSettings.Set(SETTING_CUSTOM, SessionData, EOnlineDataAdvertisementType::DontAdvertise);
	}
	CompletionDelegates.ExecuteIfBound(0, bWasSuccessful, OnlineSessionSearch->SearchResults[0]);
}

void FOnlineSessionPS4::OnGetSessionCompleted(const FUniqueNetId& UserId, TSharedPtr<FOnlineSessionSearch> OnlineSessionSearch, const FString& SessionData, bool bWasSuccessful, FOnlineAsyncTaskPS4SessionGetChangeableSessionData::FOnSessionGetChangeableSessionDataComplete GetChangeableSessionDataCompleteDelegate)
{
	bool bSuccess = false;
	auto JsonReader = TJsonReaderFactory<>::Create(SessionData);
	TSharedPtr<FJsonObject> JsonSessionInfo;
	if (FJsonSerializer::Deserialize(JsonReader, JsonSessionInfo) && JsonSessionInfo.IsValid())
	{
		FString SessionCreator = JsonSessionInfo->GetStringField("sessionCreator");
		FOnlineSessionSearchResult& SearchResult = CurrentSessionSearch->SearchResults[0];
		// Fill session members
		SearchResult.Session.OwningUserId = FUniqueNetIdPS4::FromString(SessionCreator);
		SearchResult.Session.OwningUserName = SessionCreator;
		SearchResult.Session.SessionSettings.bAntiCheatProtected = false;
		SearchResult.Session.SessionSettings.bUsesPresence = true;

		FString SessionPrivacy = JsonSessionInfo->GetStringField("sessionPrivacy");
		const TArray< TSharedPtr<FJsonValue> >& MemberList = JsonSessionInfo->GetArrayField("members");
		int32 MaxUsers = JsonSessionInfo->GetIntegerField("sessionMaxUser");
		if (SessionPrivacy == "public")
		{
			SearchResult.Session.NumOpenPublicConnections = MaxUsers - MemberList.Num();
			SearchResult.Session.SessionSettings.NumPublicConnections = MaxUsers;
		}
		else
		{
			SearchResult.Session.NumOpenPrivateConnections = MaxUsers - MemberList.Num();
			SearchResult.Session.SessionSettings.NumPrivateConnections = MaxUsers;
		}
	}

	FUniqueNetIdPS4 const& UserNetId = FUniqueNetIdPS4::Cast(UserId);
	FString SessionIdString(OnlineSessionSearch->SearchResults[0].Session.SessionInfo->GetSessionId().ToString());

	(new AutoDeleteSessionGetChangeableSessionDataTask(PS4Subsystem, PS4Subsystem->GetUserWebApiContext(UserNetId), UserNetId.AsShared(), SessionIdString, GetChangeableSessionDataCompleteDelegate))->StartBackgroundTask();

}

bool FOnlineSessionPS4::IsChatDisabled(const FUniqueNetIdPS4& User)
{
	int ret = sceNpCreateRequest();
	if (ret < 0)
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed to create a request in IsChatEnabled() with error code [%x]"), ret);
	}
	else
	{
		int NewRequestId = ret;
		int8_t UserAge = 0;
		SceNpParentalControlInfo ParentalControlInfo = { 0 };
		ret = sceNpGetParentalControlInfoA(NewRequestId, User.GetUserId(), &UserAge, &ParentalControlInfo);
		if (ret < 0)
		{
			UE_LOG_ONLINE(Warning, TEXT("Checking Parental Controls failed with error code [%x]"), ret);
		}
		else
		{
			return ParentalControlInfo.chatRestriction;
		}

		sceNpDeleteRequest(NewRequestId);
	}
	return false;
}

class FOnlineAsyncTaskPS4ShowChatRestrictionDialog : public FNonAbandonableTask
{
public:

	FOnlineAsyncTaskPS4ShowChatRestrictionDialog(FOnlineSubsystemPS4 * InPS4Subsystem, SceUserServiceUserId InServiceId)
		: PS4Subsystem(InPS4Subsystem), ServiceId(InServiceId)
	{}

	void DoWork()
	{
		// we should not be doing this here
		if (sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG) != SCE_OK)
		{
			return;
		}

		sceMsgDialogInitialize();

		SceMsgDialogSystemMessageParam SystemParam = {};
		SystemParam.sysMsgType = SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_PSN_CHAT_RESTRICTION;

		SceMsgDialogParam Param;
		sceMsgDialogParamInitialize(&Param);
		Param.mode = SCE_MSG_DIALOG_MODE_SYSTEM_MSG;
		Param.sysMsgParam = &SystemParam;
		Param.userId = ServiceId;

		int Ret = sceMsgDialogOpen(&Param);
		if(Ret < 0)
		{
			//we failed to open the dialog, abort
			return;
		}

		while (sceMsgDialogUpdateStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
			;
		
		sceMsgDialogTerminate();
	}

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FOnlineAsyncTaskPS4ShowChatRestrictionDialog, STATGROUP_ThreadPoolAsyncTasks );
	}

private:

	FOnlineSubsystemPS4 * PS4Subsystem;
	SceUserServiceUserId ServiceId;

};

typedef FAutoDeleteAsyncTask<FOnlineAsyncTaskPS4ShowChatRestrictionDialog> AutoDeleteShowChatRestrictionDialogTask;

void FOnlineSessionPS4::RegisterLocalPlayers(FNamedOnlineSession* Session)
{
	//@todo add all local players on PS4 to the room
	IOnlineVoicePtr VoiceInt = PS4Subsystem->GetVoiceInterface();
	if (VoiceInt.IsValid())
	{
		IOnlineIdentityPtr IdentitySys = PS4Subsystem->GetIdentityInterface();
		TArray<int32> ValidIndices;
		for (int32 Index = 0; Index < MAX_LOCAL_PLAYERS; Index++)
		{
			//Check whether the user we'll be looking at doesn't have parental controls that disable chatting before registering him
			TSharedPtr<const FUniqueNetIdPS4> LocalUserId = FUniqueNetIdPS4::Cast(IdentitySys->GetUniquePlayerId(Index));
			if (LocalUserId.IsValid())
			{
				if (!LocalUserId->IsOnlineId())
				{
					UE_LOG_ONLINE(Warning, TEXT("RegisterLocalPlayers failed. No local user id for local user %d"), Index);
				}
				else
				{
					bool bIsChatDisabled = IsChatDisabled(*LocalUserId.Get());
					if (!bIsChatDisabled)
					{
						ValidIndices.Add(Index);
					}
					else
					{
						(new AutoDeleteShowChatRestrictionDialogTask(PS4Subsystem, LocalUserId->GetUserId()))->StartBackgroundTask();
						return;
					}
				}
			}
		}

		//if we got here, no one has chat disabled

		for(int32 Index : ValidIndices)
		{
			// Register the local player as a local talker
			VoiceInt->RegisterLocalTalker(Index);
		}
	}
}

FNamedOnlineSession* FOnlineSessionPS4::GetNamedSession(FName SessionName)
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			return &Sessions[SearchIndex];
		}
	}

	return nullptr;
}

FNamedOnlineSession* FOnlineSessionPS4::GetRoomIdSession(SceNpMatching2RoomId RoomId)
{
	if (!PS4Subsystem->AreRoomsEnabled())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::GetRoomIdSession: Rooms are disabled."));
		return nullptr;
	}

	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		FOnlineSessionInfoPS4 *PS4SessionInfo = (FOnlineSessionInfoPS4 *)Sessions[SearchIndex].SessionInfo.Get();
		if (PS4SessionInfo->RoomId == RoomId)
		{
			return &Sessions[SearchIndex];
		}
	}

	return nullptr;
}

FNamedOnlineSession* FOnlineSessionPS4::GetLobbyIdSession(SceNpMatching2LobbyId LobbyId)
{
	if (!PS4Subsystem->AreRoomsEnabled())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::GetLobbyIdSession: Rooms are disabled."));
		return nullptr;
	}

	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		FOnlineSessionInfoPS4 *PS4SessionInfo = (FOnlineSessionInfoPS4 *)Sessions[SearchIndex].SessionInfo.Get();
		if (PS4SessionInfo->LobbyId == LobbyId)
		{
			return &Sessions[SearchIndex];
		}
	}

	return nullptr;
}

FNamedOnlineSession* FOnlineSessionPS4::GetCreatingSession()
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionState == EOnlineSessionState::Creating)
		{
			return &Sessions[SearchIndex];
		}
	}


	return nullptr;
}

void FOnlineSessionPS4::RemoveNamedSession(FName SessionName)
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			Sessions.RemoveAtSwap(SearchIndex);
			return;
		}
	}
}

void FOnlineSessionPS4::RemoveRoomIdSession(SceNpMatching2RoomId RoomId)
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		FOnlineSessionInfoPS4 *PS4SessionInfo = (FOnlineSessionInfoPS4 *)Sessions[SearchIndex].SessionInfo.Get();
		if (PS4SessionInfo->RoomId == RoomId)
		{
			Sessions.RemoveAtSwap(SearchIndex);
			return;
		}
	}
}



void FOnlineSessionPS4::RemoveLobbyIdSession(SceNpMatching2LobbyId LobbyId)
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		FOnlineSessionInfoPS4 *PS4SessionInfo = (FOnlineSessionInfoPS4 *)Sessions[SearchIndex].SessionInfo.Get();
		if (PS4SessionInfo->LobbyId == LobbyId)
		{
			Sessions.RemoveAtSwap(SearchIndex);
			return;
		}
	}
}

EOnlineSessionState::Type FOnlineSessionPS4::GetSessionState(FName SessionName) const
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionName == SessionName)
		{
			return Sessions[SearchIndex].SessionState;
		}
	}

	return EOnlineSessionState::NoSession;
}

bool FOnlineSessionPS4::HasPresenceSession()
{
	FScopeLock ScopeLock(&SessionLock);
	for (int32 SearchIndex = 0; SearchIndex < Sessions.Num(); SearchIndex++)
	{
		if (Sessions[SearchIndex].SessionSettings.bUsesPresence)
		{
			return true;
		}
	}

	return false;
}

bool FOnlineSessionPS4::CreateSession(int32 HostingPlayerNum, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	check(PS4Subsystem);

	IOnlineIdentityPtr Identity = PS4Subsystem->GetIdentityInterface();
	if (!Identity.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionPS4::CreateSession: Identify interface is nullptr"));
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	TSharedPtr<const FUniqueNetId> UniqueNetId = Identity->GetUniquePlayerId(HostingPlayerNum);
	if (UniqueNetId.IsValid() && UniqueNetId->IsValid())
	{
		return CreateSession(*UniqueNetId, SessionName, NewSessionSettings);
	}	

	UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionPS4::CreateSession: Invalid HostingPlayerNum: %i"), HostingPlayerNum);
	TriggerOnCreateSessionCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionPS4::CreateSession(const FUniqueNetId& HostingPlayerId, FName SessionName, const FOnlineSessionSettings& NewSessionSettings)
{
	if (!HostingPlayerId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionPS4::CreateSession: Cannot create session with invalid Player: %s."), *HostingPlayerId.ToString());
		return false;
	}

	// Check for an existing session
	FNamedOnlineSession * Session = GetNamedSession(SessionName);

	if (Session != nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionPS4::CreateSession: Cannot create session '%s': session already exists."), *SessionName.ToString());
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}	
	
	TSharedPtr<const FUniqueNetId> SharedHostingPlayerId = HostingPlayerId.AsShared();
	FUniqueNetIdPS4 const& HostingPlayerIdPS4 = FUniqueNetIdPS4::Cast(HostingPlayerId);

	if (PS4Subsystem->AreRoomsEnabled() && WorldId == SCE_NP_MATCHING2_INVALID_WORLD_ID)
	{
		DoGetWorldInfoList(GetUserMatching2Context(HostingPlayerIdPS4, true));

		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionPS4::CreateSession: Cannot create session '%s': no valid world is currently set."), *SessionName.ToString());
		TriggerOnCreateSessionCompleteDelegates(SessionName, false);
		return false;
	}

	// Create a new session and deep copy the game settings
	Session = AddNamedSession(SessionName, NewSessionSettings);
	Session->SessionState = EOnlineSessionState::Creating;
	Session->bHosting = true;

	check(Session);
	IOnlineIdentityPtr Identity = PS4Subsystem->GetIdentityInterface();
	check(Identity.IsValid());

	Session->NumOpenPrivateConnections = NewSessionSettings.NumPrivateConnections;
	Session->NumOpenPublicConnections = NewSessionSettings.NumPublicConnections;	// always start with full public connections, local player will register later

	Session->HostingPlayerNum = INDEX_NONE;
	Session->OwningUserId = SharedHostingPlayerId;
	Session->LocalOwnerId = SharedHostingPlayerId;
	Session->OwningUserName = Identity->GetPlayerNickname(HostingPlayerId);

	// Unique identifier of this build for compatibility
	Session->SessionSettings.BuildUniqueId = GetBuildUniqueId();

	SessionCreateTaskData Data;
	Data.NewSessionSettings = NewSessionSettings;
	Data.Session = Session;
	Data.SessionName = SessionName;
	Data.bWasSuccessful = false;
	
	(new AutoDeleteSessionCreateTask(PS4Subsystem, PS4Subsystem->GetUserWebApiContext(HostingPlayerIdPS4), GetUserMatching2Context(HostingPlayerIdPS4, true), Data))->StartBackgroundTask();

	return true;
}

bool FOnlineSessionPS4::StartSession(FName SessionName)
{
	bool Result = false;
	// Grab the session information by name
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		// Can't start a match multiple times
		if (Session->SessionState == EOnlineSessionState::Pending ||
			Session->SessionState == EOnlineSessionState::Ended)
		{
			Result = true;
			Session->SessionState = EOnlineSessionState::InProgress;
		}
		else
		{
			UE_LOG_ONLINE(Warning,	TEXT("Can't start an online session (%s) in state %s"),
				*SessionName.ToString(),
				EOnlineSessionState::ToString(Session->SessionState));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't start an online game for session (%s) that hasn't been created"), *SessionName.ToString());
	}

	// Just trigger the delegate
	TriggerOnStartSessionCompleteDelegates(SessionName, Result);

	return Result;
}

bool FOnlineSessionPS4::UpdateSession(FName SessionName, FOnlineSessionSettings& UpdatedSessionSettings, bool bShouldRefreshOnlineData)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session != nullptr)
	{
		const FUniqueNetIdPS4& PlayerIdPS4 = *FUniqueNetIdPS4::Cast(Session->bHosting ? Session->OwningUserId : Session->LocalOwnerId);
		TSharedPtr< FOnlineSessionInfoPS4 > SessionInfo = StaticCastSharedPtr< FOnlineSessionInfoPS4 >(Session->SessionInfo);

		if (SessionInfo.IsValid())
		{
			(new AutoDeleteSessionPutSessionTask(PS4Subsystem, PS4Subsystem->GetUserWebApiContext(PlayerIdPS4), &SessionInfo->SessionId, UpdatedSessionSettings))->StartBackgroundTask();

			char SessionData[MAX_SESSION_DATA_LENGTH] = {};
			CopyCustomSettingDataToBuffer(UpdatedSessionSettings, SessionData);
			
			(new AutoDeleteSessionPutChangeableSessionDataTask(PS4Subsystem, PS4Subsystem->GetUserWebApiContext(PlayerIdPS4), &SessionInfo->SessionId, SessionData))->StartBackgroundTask();
			return true;
		}
	}
	return false;
}

bool FOnlineSessionPS4::EndSession(FName SessionName)
{
	bool bWasSuccessful = false;

	// Grab the session information by name
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		// Can't end a match that isn't in progress
		if (Session->SessionState == EOnlineSessionState::InProgress)
		{
			Session->SessionState = EOnlineSessionState::Ended;
			bWasSuccessful = true;
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Can't end session (%s) in state %s"),
				*SessionName.ToString(),
				EOnlineSessionState::ToString(Session->SessionState));
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't end an online game for session (%s) that hasn't been created"),
			*SessionName.ToString());
	}


	if (Session)
	{
		Session->SessionState = EOnlineSessionState::Ended;
	}

	TriggerOnEndSessionCompleteDelegates(SessionName, bWasSuccessful);

	return bWasSuccessful;
}

bool FOnlineSessionPS4::DestroySession(FName SessionName, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	bool bPending = false;

	// Find the session in question
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (!Session)
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't destroy a null online session (%s)"), *SessionName.ToString());
		OnSessionDestroyComplete(SessionName, false, CompletionDelegate);
	}
	else if (Session->SessionState == EOnlineSessionState::Destroying)
	{
		// don't double destroy, async destroy task is outstanding.  As long as removenamedsession is only called on the main thread it's safe to cache this session pointer locally.
		return false;
	}
	else if (PS4Subsystem->AreRoomsEnabled() && !DoLeaveRoom(Session, CompletionDelegate))
	{
		// DoLeaveRoom will kick off tasks to do session removal no matter what, so no need to delete it here.
		return false;
	}
	else if (!PS4Subsystem->AreRoomsEnabled())
	{
		Session->SessionState = EOnlineSessionState::Destroying;
		if (!DoLeaveOrDestroySession(Session, CompletionDelegate))
		{
			OnSessionDestroyComplete(SessionName, false, CompletionDelegate);
			return false;
		}
	}

	return true;
}

bool FOnlineSessionPS4::DoLeaveOrDestroySession(FNamedOnlineSession* Session, const FOnDestroySessionCompleteDelegate& CompletionDelegate)
{
	if (Session != nullptr)
	{
		const FUniqueNetIdPS4& PlayerIdPS4 = *FUniqueNetIdPS4::Cast(Session->bHosting ? Session->OwningUserId : Session->LocalOwnerId);
		bool bUsingHostMigration = false;
		Session->SessionSettings.Get(SETTING_HOST_MIGRATION, bUsingHostMigration);

		// If the sessions have host migration enabled, we shouldn't destroy the session, since it is supposed to be destroyed automatically when the number of players reaches zero.
		if (Session->bHosting && !bUsingHostMigration)
		{
			(new AutoDeleteSessionDestroyTask(PS4Subsystem, PS4Subsystem->GetUserWebApiContext(PlayerIdPS4), Session, CompletionDelegate))->StartBackgroundTask();
		}
		else
		{
			(new AutoDeleteSessionLeaveTask(PS4Subsystem, PS4Subsystem->GetUserWebApiContext(PlayerIdPS4), Session, CompletionDelegate))->StartBackgroundTask();
		}
	}

	return true;
}

bool FOnlineSessionPS4::IsPlayerInSession(FName SessionName, const FUniqueNetId& UniqueId)
{
	return IsPlayerInSessionImpl(this, SessionName, UniqueId);
}

bool FOnlineSessionPS4::StartMatchmaking(const TArray< TSharedRef<const FUniqueNetId> >& LocalPlayers, FName SessionName, const FOnlineSessionSettings& NewSessionSettings, TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	if (!PS4Subsystem->AreRoomsEnabled())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::StartMatchmaking: Rooms are disabled."));
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
		return false;
	}

	// Use the first local player in the array for now, online splitscreen on PS4 still needs lots of work. See UE-7231
	if (LocalPlayers.Num() != 1)
	{
		UE_LOG_ONLINE(Warning, TEXT("Matchmaking on PS4 only supports a single player right now. LocalPlayers.Num() == %d"), LocalPlayers.Num());
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
		return false;
	}

	TSharedRef<const FUniqueNetIdPS4> SearchingPlayerId = FUniqueNetIdPS4::Cast(LocalPlayers[0]);

	if (!SearchingPlayerId->IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Matchmaking cannot start with invalid PlayerId: %s."), *SearchingPlayerId->ToString());
		TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
		return false;
	}

	
	QuickmatchSearchingSessionName = SessionName;
	QuickmatchSearchingPlayerId = SearchingPlayerId;
	bUsingQuickmatch = true;

	if (WorldId == SCE_NP_MATCHING2_INVALID_WORLD_ID)
	{
		IOnlineIdentityPtr Identity = PS4Subsystem->GetIdentityInterface();
		if (Identity.IsValid())
		{			
			DoGetWorldInfoList(GetUserMatching2Context((FUniqueNetIdPS4&)SearchingPlayerId, true));
		}

		// We don't have a valid world
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::FindSessions: No valid world is currently set."));
		CancelMatchmaking(*QuickmatchSearchingPlayerId, QuickmatchSearchingSessionName);
		return false;
	}

	if (CurrentSessionSearch.IsValid() || SearchSettings->SearchState == EOnlineAsyncTaskState::InProgress)
	{
		// Don't start another search while one is in progress
		UE_LOG_ONLINE(Warning, TEXT("Ignoring game search request while one is pending"));
		CancelMatchmaking(*QuickmatchSearchingPlayerId, QuickmatchSearchingSessionName);
		return false;
	}

	// Free up previous results
	SearchSettings->SearchResults.Empty();

	// Copy the search pointer so we can keep it around
	CurrentSessionSearch = SearchSettings;

	// Check if its a LAN query
	if (SearchSettings->bIsLanQuery)
	{
		//@todo is lan supported
		UE_LOG_ONLINE(Warning, TEXT("LAN search not supported"));
		CancelMatchmaking(*QuickmatchSearchingPlayerId, QuickmatchSearchingSessionName);
		return false;
	}

	if (!DoSearchRooms(SearchSettings, SearchingPlayerId.Get()))
	{
		UE_LOG_ONLINE(Warning, TEXT("Did not successfully find any rooms"));
		CancelMatchmaking(*QuickmatchSearchingPlayerId, QuickmatchSearchingSessionName);
		return false;
	}

	SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;
	return true;

}

bool FOnlineSessionPS4::CancelMatchmaking(int32 SearchingPlayerNum, FName SessionName)
{
	UE_LOG_ONLINE(Warning, TEXT("Matchmaking is not supported on this platform."));
	TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionPS4::CancelMatchmaking(const FUniqueNetId& SearchingPlayerId, FName SessionName)
{
	UE_LOG_ONLINE(Warning, TEXT("Matchmaking is not supported on this platform."));
	TriggerOnCancelMatchmakingCompleteDelegates(SessionName, false);
	return false;
}

bool FOnlineSessionPS4::FindSessions(int32 SearchingPlayerNum, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{
	IOnlineIdentityPtr Identity = PS4Subsystem->GetIdentityInterface();
	TSharedPtr<const FUniqueNetIdPS4> SearchingPlayerId;
	if (Identity.IsValid())
	{
		SearchingPlayerId = FUniqueNetIdPS4::Cast(Identity->GetUniquePlayerId(SearchingPlayerNum));
	}

	if (!SearchingPlayerId->IsValid())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::FindSessions: No Valid NetId for Player: %i."), SearchingPlayerNum);
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	return FindSessions(*SearchingPlayerId, SearchSettings);
}

bool FOnlineSessionPS4::FindSessions(const FUniqueNetId& SearchingPlayerId, const TSharedRef<FOnlineSessionSearch>& SearchSettings)
{	
	const FUniqueNetIdPS4& SearchingPlayerIdPS4 = (const FUniqueNetIdPS4&)SearchingPlayerId;
	if (!SearchingPlayerIdPS4.IsValid())
	{
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::FindSessions: No Valid NetId for Player: %s."), *SearchingPlayerIdPS4.ToString());
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	if (WorldId == SCE_NP_MATCHING2_INVALID_WORLD_ID)
	{
		DoGetWorldInfoList(GetUserMatching2Context(SearchingPlayerIdPS4, true));

		// We don't have a valid world
		UE_LOG_ONLINE(Error, TEXT("FOnlineSessionPS4::FindSessions: No valid world is currently set."));
		return false;
	}

	if (CurrentSessionSearch.IsValid() || SearchSettings->SearchState == EOnlineAsyncTaskState::InProgress)
	{
		// Don't start another search while one is in progress
		UE_LOG_ONLINE(Warning, TEXT("Ignoring game search request while one is pending"));
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	// Free up previous results
	SearchSettings->SearchResults.Empty();

	// Copy the search pointer so we can keep it around
	CurrentSessionSearch = SearchSettings;

	// Check if its a LAN query
	if (SearchSettings->bIsLanQuery)
	{
		//@todo is lan supported
		UE_LOG_ONLINE(Warning, TEXT("LAN search not supported"));
		CurrentSessionSearch = nullptr;
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	if (!DoSearchRooms(SearchSettings, SearchingPlayerIdPS4))
	{
		CurrentSessionSearch = nullptr;
		TriggerOnFindSessionsCompleteDelegates(false);
		return false;
	}

	SearchSettings->SearchState = EOnlineAsyncTaskState::InProgress;

	return true;
}

bool FOnlineSessionPS4::FindSessionById(const FUniqueNetId& SearchingUserId, const FUniqueNetId& SessionId, const FUniqueNetId& FriendId, const FOnSingleSessionResultCompleteDelegate& CompletionDelegates)
{
	// Create a search settings object 
	CurrentSessionSearch = MakeShareable(new FOnlineSessionSearch());
	CurrentSessionSearch->bIsLanQuery = false;
	CurrentSessionSearch->SearchState = EOnlineAsyncTaskState::InProgress;

	FOnlineSessionSearchResult * NewResult = new(CurrentSessionSearch->SearchResults)FOnlineSessionSearchResult();

	EPS4Session::Type SessionType = PS4Subsystem->AreRoomsEnabled() ? EPS4Session::RoomSession : EPS4Session::StandaloneSession;
	FOnlineSessionInfoPS4* NewSessionInfo = new FOnlineSessionInfoPS4(SessionType, 0, 0, 0, FUniqueNetIdString(SessionId.ToString()));
	NewSessionInfo->Init();
	NewResult->Session.SessionInfo = MakeShareable(NewSessionInfo);

	FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager();
	if (AsyncTaskManager)
	{
		FOnlineAsyncTaskPS4SessionGetChangeableSessionData::FOnSessionGetChangeableSessionDataComplete Delegate = FOnlineAsyncTaskPS4SessionGetChangeableSessionData::FOnSessionGetChangeableSessionDataComplete::CreateRaw(this, &FOnlineSessionPS4::HandleGetChangeableDataCompleteFindSessionById, CompletionDelegates);
		auto NewGetSessionTask = new FOnlineAsyncTaskPS4GetSession(PS4Subsystem, FUniqueNetIdPS4::Cast(SearchingUserId.AsShared()), CurrentSessionSearch, FOnGetSessionCompleteDelegate::CreateRaw(this, &FOnlineSessionPS4::OnGetSessionCompleted, Delegate));
		AsyncTaskManager->AddToInQueue(NewGetSessionTask);
	}
	else
	{
		CompletionDelegates.ExecuteIfBound(0, false, CurrentSessionSearch->SearchResults[0]);
	}

	return true;
}

bool FOnlineSessionPS4::CancelFindSessions()
{
	UE_LOG_ONLINE(Warning, TEXT("Can't cancel a search on PS4"));
	return false;
}

bool FOnlineSessionPS4::PingSearchResults(const FOnlineSessionSearchResult& SearchResult)
{
	return false;
}

bool FOnlineSessionPS4::JoinSession(int32 PlayerNum, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	TSharedPtr<const FUniqueNetIdPS4> JoiningPlayerId;
	IOnlineIdentityPtr Identity = PS4Subsystem->GetIdentityInterface();
	if (Identity.IsValid())
	{
		JoiningPlayerId = FUniqueNetIdPS4::Cast(Identity->GetUniquePlayerId(PlayerNum));
	}

	if (!JoiningPlayerId.IsValid() || !JoiningPlayerId->IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Can't join session with invalid Player: %i"), PlayerNum);
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	return JoinSession(*JoiningPlayerId, SessionName, DesiredSession);	
}

bool FOnlineSessionPS4::JoinSession(const FUniqueNetId& PlayerId, FName SessionName, const FOnlineSessionSearchResult& DesiredSession)
{
	if (!PlayerId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionPS4::JoinSession: Invalid unique net id"));
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}

	const FUniqueNetIdPS4& PlayerIdPS4 = FUniqueNetIdPS4::Cast(PlayerId);

	int32 JoiningPlayerNum = FOnlineIdentityPS4::GetLocalUserIndex(PlayerIdPS4.GetUserId());
	if (JoiningPlayerNum == INDEX_NONE)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineSessionPS4::JoinSession: Could not find userindex for PlayerId: %s"), *PlayerIdPS4.ToString());
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::UnknownError);
		return false;
	}


	bool bSuccess = false;
	FNamedOnlineSession* Session = GetNamedSession(SessionName);

	// Don't join a session if already in one or hosting one
	if (Session == nullptr)
	{		
		// Create a named session from the search result data
		Session = AddNamedSession(SessionName, DesiredSession.Session);

		//all the OSS implementations use 'hosting player num' to hold the joining player, and 'owninguserid' to hold the remote hosting player.  This is confusing and needs a refactor.
		//PS4 is going to spearhead the move to using JoiningPlayerId and OwningUserId where appropriate.
		Session->HostingPlayerNum = INDEX_NONE;
		Session->LocalOwnerId = PlayerIdPS4.AsShared();
		Session->bHosting = false;

		(new AutoDeleteSessionJoinTask(PS4Subsystem, PS4Subsystem->GetUserWebApiContext(PlayerIdPS4), Session, DesiredSession))->StartBackgroundTask();
		bSuccess = true;
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Session (%s) already exists, can't join twice"), *SessionName.ToString());
		TriggerOnJoinSessionCompleteDelegates(SessionName, EOnJoinSessionCompleteResult::AlreadyInSession);
	}	
	return bSuccess;
}

bool FOnlineSessionPS4::FindFriendSession(int32 LocalUserNum, const FUniqueNetId& Friend)
{
	if (LocalUserNum == PLATFORMUSERID_NONE)
	{
		TArray<FOnlineSessionSearchResult> EmptyResult;
		TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, false, EmptyResult);
		return false;
	}

	return FindFriendSession(*PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum).ToSharedRef(), Friend);
}


bool FOnlineSessionPS4::FindFriendSession(const FUniqueNetId& LocalUserId, const FUniqueNetId& Friend)
{
	TArray<TSharedRef<const FUniqueNetId>> FriendList;
	FriendList.Add(Friend.AsShared());
	return FindFriendSession(LocalUserId, FriendList);
}

bool FOnlineSessionPS4::FindFriendSession(const FUniqueNetId& LocalUserId, const TArray<TSharedRef<const FUniqueNetId>>& FriendList)
{
	bool bSuccessfullyJoinedFriendSession = false;

	UE_LOG(LogOnline, Display, TEXT("FOnlineSessionPS4::FindFriendSession - not implemented"));

	int32 LocalUserNum = PS4Subsystem->GetIdentityInterface()->GetPlatformUserIdFromUniqueNetId(LocalUserId);

	TArray<FOnlineSessionSearchResult> EmptyResult;
	TriggerOnFindFriendSessionCompleteDelegates(LocalUserNum, bSuccessfullyJoinedFriendSession, EmptyResult);

	return bSuccessfullyJoinedFriendSession;
}

FOnlineAsyncTaskPS4SendInvitation::FOnlineAsyncTaskPS4SendInvitation(FOnlineSubsystemPS4* InSubsystem, const FUniqueNetIdPS4& InUserId, const FNamedOnlineSession& InSession, const TArray< TSharedRef<const FUniqueNetId> >& InFriends)
	: PS4Subsystem(InSubsystem)
{
	check(InFriends.Num() <= NpToolkit::Session::Request::SendInvitation::MAX_NUM_RECIPIENTS);

	NpToolkit::Session::Request::SendInvitation Request;
	Request.userId = InUserId.GetUserId();
	Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
	Request.async = true;

	Request.sessionId = PS4StringToSessionId(InSession.GetSessionIdStr());

	Request.numRecipients = InFriends.Num();
	for (int32 Index = 0; Index < InFriends.Num(); ++Index)
	{
		Request.recipients[Index] = FUniqueNetIdPS4::Cast(InFriends[Index])->GetAccountId();
	}

	Request.maxNumberRecipientsToAdd = InSession.NumOpenPrivateConnections + InSession.NumOpenPublicConnections;
	Request.recipientsEditableByUser = false;
	Request.enableDialog = false;

	int32 ResultCode = NpToolkit::Session::sendInvitation(Request, &Response);
	if (ResultCode < SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		UE_LOG_ONLINE(Error, TEXT("Session::sendInvitation (sync) failed with error code 0x%08x"), ResultCode);
		bIsComplete = true;
		bWasSuccessful = false;
	}
}

void FOnlineAsyncTaskPS4SendInvitation::Tick()
{
	if (!Response.isLocked())
	{
		// No results to gather when this request completes.
		// Just log any errors.
		bIsComplete = true;
		
		int32 ResultCode = Response.getReturnCode();
		bWasSuccessful = ResultCode == SCE_TOOLKIT_NP_V2_SUCCESS;

		if (!bWasSuccessful)
		{
			UE_LOG_ONLINE(Error, TEXT("Session::sendInvitation (async) failed with error code 0x%08x"), ResultCode);
		}
	}
}

bool FOnlineSessionPS4::SendSessionInviteToFriend(int32 LocalUserNum, FName SessionName, const FUniqueNetId& Friend)
{
	TArray< TSharedRef<const FUniqueNetId> > Friends;
	Friends.Add(Friend.AsShared());

	return SendSessionInviteToFriends(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum).ToSharedRef().Get(), SessionName, Friends);
}

bool FOnlineSessionPS4::SendSessionInviteToFriend(const FUniqueNetId& LocalUserId, FName SessionName, const FUniqueNetId& Friend)
{
	TArray< TSharedRef<const FUniqueNetId> > Friends;
	Friends.Add(Friend.AsShared());

	return SendSessionInviteToFriends(LocalUserId, SessionName, Friends);
}

bool FOnlineSessionPS4::SendSessionInviteToFriends(int32 LocalUserNum, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	return SendSessionInviteToFriends(PS4Subsystem->GetIdentityInterface()->GetUniquePlayerId(LocalUserNum).ToSharedRef().Get(), SessionName, Friends);
}

bool FOnlineSessionPS4::SendSessionInviteToFriends(const FUniqueNetId& LocalUserId, FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Friends)
{
	FNamedOnlineSession* const Session = GetNamedSession(SessionName);
	if (Session == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot send session invite to friends, no session found by name %s"), *SessionName.ToString());
		return false;
	}

	if (!Session->SessionInfo.IsValid() || !Session->SessionInfo->IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("Cannot send session invite to friends, session %s information not valid"), *SessionName.ToString());
		return false;
	}

	if (FOnlineAsyncTaskManagerPS4* AsyncTaskManager = PS4Subsystem->GetAsyncTaskManager())
	{
		FOnlineAsyncTaskPS4SendInvitation* NewTask = new FOnlineAsyncTaskPS4SendInvitation(PS4Subsystem, FUniqueNetIdPS4::Cast(LocalUserId), *Session, Friends);
		AsyncTaskManager->AddToInQueue(NewTask);
		return true;
	}

	return false;
}

static bool GetConnectStringFromSessionInfo(TSharedPtr<FOnlineSessionInfoPS4>& SessionInfo, FString& ConnectInfo, int32 PortOverride=0)
{
	if ( !SessionInfo.IsValid() )
	{
		return false;
	}

	if ( SessionInfo->HostAddr.IsValid() && SessionInfo->HostAddr->IsValid() )
	{
		if ( PortOverride != 0 )
		{
			ConnectInfo = FString::Printf( TEXT( "%s:%d" ), *SessionInfo->HostAddr->ToString( false ), PortOverride );
		}
		else
		{
			ConnectInfo = FString::Printf( TEXT( "%s" ), *SessionInfo->HostAddr->ToString( true ) );
		}
	}
	else
	{
		//HostAddr could be null, so it was taken out of this log since it's dangerous to dereference that
		UE_LOG_ONLINE(Warning, TEXT("GetConnectStringFromSessionInfo: Invalid HostAddr used"));
		return false;
	}

	return true;
}

bool FOnlineSessionPS4::GetResolvedConnectString(FName SessionName, FString& ConnectInfo, FName PortType)
{
	// Find the session
	FNamedOnlineSession * Session = GetNamedSession( SessionName );

	if ( Session != nullptr )
	{
		TSharedPtr< FOnlineSessionInfoPS4 > SessionInfo = StaticCastSharedPtr< FOnlineSessionInfoPS4 >( Session->SessionInfo );

		bool bSuccess = false;

		if (PortType == NAME_BeaconPort)
		{
			int32 BeaconListenPort = GetBeaconPortFromSessionSettings(Session->SessionSettings);
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo, BeaconListenPort);
		}
		else if (PortType == NAME_GamePort)
		{
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo);
		}

		if ( !bSuccess )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineSessionPS4::GetResolvedConnectString: Invalid session info for session %s in GetResolvedConnectString()" ), *SessionName.ToString() );
			return false;
		}
	}
	else
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineSessionPS4::GetResolvedConnectString: Unknown session name (%s) specified to GetResolvedConnectString()" ), *SessionName.ToString() );
		return false;
	}

	return true;
}

bool FOnlineSessionPS4::GetResolvedConnectString(const FOnlineSessionSearchResult& SearchResult, FName PortType, FString& ConnectInfo)
{
	bool bSuccess = false;
	if (SearchResult.Session.SessionInfo.IsValid())
	{
		TSharedPtr<FOnlineSessionInfoPS4> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoPS4>(SearchResult.Session.SessionInfo);

		if (PortType == NAME_BeaconPort)
		{
			int32 BeaconListenPort = GetBeaconPortFromSessionSettings(SearchResult.Session.SessionSettings);
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo, BeaconListenPort);
		}
		else if (PortType == NAME_GamePort)
		{
			bSuccess = GetConnectStringFromSessionInfo(SessionInfo, ConnectInfo);
		}
	}

	if (!bSuccess || ConnectInfo.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("Invalid session info in search result to GetResolvedConnectString()"));
	}

	return bSuccess;
}

FOnlineSessionSettings* FOnlineSessionPS4::GetSessionSettings(FName SessionName)
{
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		return &Session->SessionSettings;
	}
	return nullptr;
}

void FOnlineSessionPS4::RegisterVoice(const FUniqueNetId& PlayerId)
{
	IOnlineVoicePtr VoiceInt = PS4Subsystem->GetVoiceInterface();
	if (VoiceInt.IsValid())
	{
		int32 LocalUserNum = FOnlineIdentityPS4::GetLocalUserIndex(FUniqueNetIdPS4::Cast(PlayerId).GetUserId());
		if (LocalUserNum == -1)
		{
			VoiceInt->RegisterRemoteTalker(PlayerId);
		}
		else
		{
			// This is a local player. In case their PlayerState came last during replication,  muting
			VoiceInt->ProcessMuteChangeNotification();
		}
	}
}

void FOnlineSessionPS4::UnregisterVoice(const FUniqueNetId& PlayerId)
{
	IOnlineVoicePtr VoiceInt = PS4Subsystem->GetVoiceInterface();
	if (VoiceInt.IsValid())
	{
		int32 LocalUserNum = FOnlineIdentityPS4::GetLocalUserIndex(FUniqueNetIdPS4::Cast(PlayerId).GetUserId());
		if (LocalUserNum >= 0)
		{
			if (VoiceInt.IsValid())
			{
				VoiceInt->UnregisterRemoteTalker(PlayerId);
			}
		}
	}
}

bool FOnlineSessionPS4::RegisterPlayer(FName SessionName, const FUniqueNetId& PlayerId, bool bWasInvited)
{
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(PlayerId.AsShared());
	return RegisterPlayers(SessionName, Players, bWasInvited);
}

bool FOnlineSessionPS4::RegisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players, bool bWasInvited)
{
	bool bSuccess = false;
	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		if (Session->SessionInfo.IsValid())
		{
			for (int32 PlayerIdx=0; PlayerIdx < Players.Num(); PlayerIdx++)
			{
				const TSharedRef<const FUniqueNetId>& PlayerId = Players[PlayerIdx];
				RegisterVoice(PlayerId.Get());
				if (Session->NumOpenPublicConnections > 0)
				{
					Session->NumOpenPublicConnections--;
				}
				else if (Session->NumOpenPrivateConnections > 0)
				{
					Session->NumOpenPrivateConnections--;
				}
			}

			bSuccess = true;
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("No session info to join for session (%s)"), *SessionName.ToString());
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("No game present to join for session (%s)"), *SessionName.ToString());
	}

	TriggerOnRegisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
	return bSuccess;
}

bool FOnlineSessionPS4::UnregisterPlayer(FName SessionName, const FUniqueNetId& PlayerId)
{
	TArray< TSharedRef<const FUniqueNetId> > Players;
	Players.Add(PlayerId.AsShared());
	return UnregisterPlayers(SessionName, Players);
}

bool FOnlineSessionPS4::UnregisterPlayers(FName SessionName, const TArray< TSharedRef<const FUniqueNetId> >& Players)
{
	bool bSuccess = false;

	FNamedOnlineSession* Session = GetNamedSession(SessionName);
	if (Session)
	{
		if (Session->SessionInfo.IsValid())
		{
			for (int32 PlayerIdx=0; PlayerIdx < Players.Num(); PlayerIdx++)
			{
				const TSharedRef<const FUniqueNetId>& PlayerId = Players[PlayerIdx];

				FUniqueNetIdMatcher PlayerMatch(*PlayerId);
				int32 RegistrantIndex = Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch);
				if (RegistrantIndex != INDEX_NONE)
				{
					Session->RegisteredPlayers.RemoveAtSwap(RegistrantIndex);
					UnregisterVoice(*PlayerId);
				}
				else
				{
					UE_LOG_ONLINE(Warning, TEXT("Player %s is not part of session (%s)"), *PlayerId->ToDebugString(), *SessionName.ToString());
				}

				if (Session->NumOpenPublicConnections > 0)
				{
					Session->NumOpenPublicConnections++;
				}
				else if (Session->NumOpenPrivateConnections > 0)
				{
					Session->NumOpenPrivateConnections++;
				}
			}

			bSuccess = true;
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("No session info to leave for session (%s)"), *SessionName.ToString());
		}
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("No game present to leave for session (%s)"), *SessionName.ToString());
	}

	TriggerOnUnregisterPlayersCompleteDelegates(SessionName, Players, bSuccess);
	return bSuccess;
}

int32 FOnlineSessionPS4::GetNumSessions()
{
	return Sessions.Num();
}

void FOnlineSessionPS4::DumpSessionState()
{
	FScopeLock ScopeLock(&SessionLock);

	for (int32 SessionIdx=0; SessionIdx < Sessions.Num(); SessionIdx++)
	{
		DumpNamedSession(&Sessions[SessionIdx]);
	}
}

void FOnlineSessionPS4::RegisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnRegisterLocalPlayerCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(PlayerId, EOnJoinSessionCompleteResult::Success);
}

void FOnlineSessionPS4::UnregisterLocalPlayer(const FUniqueNetId& PlayerId, FName SessionName, const FOnUnregisterLocalPlayerCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(PlayerId, true);
}