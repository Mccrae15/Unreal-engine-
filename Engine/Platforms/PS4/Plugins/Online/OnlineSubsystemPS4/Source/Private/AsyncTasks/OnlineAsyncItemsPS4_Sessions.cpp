// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncItemsPS4_Sessions.h"
#include "OnlineSubsystemSessionSettings.h"
#include "WebApiPS4Throttle.h"
#include "HAL/PlatformFilemanager.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "PS4Application.h"

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_CreateSession
//////////////////////////////////////////////////////////////////////////

void FOnlineAsyncTaskPS4_CreateSession::DoWork()
{
	// NEW
	//NpToolkit::Session::Request::Create CreateSessionRequest;
	//CreateSessionRequest.privacy = Data.NewSessionSettings.bShouldAdvertise ? NpToolkit::Session::Privacy::publicSession : NpToolkit::Session::Privacy::privateSession;
	//CreateSessionRequest.maxUsers = Data.NewSessionSettings.NumPrivateConnections + Data.NewSessionSettings.NumPublicConnections;
	//CreateSessionRequest.name = *Data.SessionName.ToString();
	//CreateSessionRequest.availablePlatforms = NpToolkit::Session::AvailablePlatforms::ps4;

	//FOnlineSessionPS4::CopyCustomSettingDataToBuffer(Data.NewSessionSettings, CreateSessionRequest.fixedData);

	//// Maybe do what we have below still, since it seems we still need to provide the size?
	//CreateSessionRequest.SessionImage.sessionImgPath = *(FPaths::EngineDir() + TEXT("Build/PS4/InviteIcon.jpg"));

	//bool bUseHostMigration = false;
	//Data.NewSessionSettings.Get(SETTING_HOST_MIGRATION, bUseHostMigration);
	//CreateSessionRequest.type = bUseHostMigration ? NpToolkit::Session::SessionType::ownerMigration : NpToolkit::Session::SessionType::ownerBind;


	// OLD
	Data.bWasSuccessful = false;

	if (FNpWebApiThrottle::GetRateLimitStatus(ENpApiGroup::SessionAndInvitation) == EOnlinePSNRateLimitStatus::RateLimited)
	{
		UE_LOG_ONLINE_SESSION(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - request was rate-limited."));
		SendResults();
		return;
	}

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
		UE_LOG_ONLINE_SESSION(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - session image is empty or not found."));
		SendResults();
		return;
	}

	// Create a multi-part request for creating a session
	// There are 3 parts to send: The json description of the session request, the image data for the session, and the app-specific session data
	if (UserWebApiContext == -1)
	{
		UE_LOG_ONLINE_SESSION(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - UserWebApiContext not set."));
		SendResults();
		return;
	}

	int32 ResultCode = 0;
	int64_t WebRequestId = 0;

	// Create the request
	ResultCode = sceNpWebApiCreateMultipartRequest(UserWebApiContext, "sessionInvitation", "/v1/sessions", SCE_NP_WEBAPI_HTTP_METHOD_POST, &WebRequestId);
	if (ResultCode < 0)
	{
		UE_LOG_ONLINE_SESSION(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiCreateMultipartRequest returned with error code 0x%x"), ResultCode);
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

	UE_LOG_ONLINE_SESSION(VeryVerbose, TEXT("FOnlineAsyncTaskPS4SessionCreate session info payload:\n%s"), *SessionRequestJsonStr);

	const uint32 MaxRequestBodySize = 1024;

	char SessionRequestJson[MaxRequestBodySize] = {};
	FTCHARToUTF8 UTF8StringRequestJson(*SessionRequestJsonStr);
	int32 UTF8RequestJsonStringByteLen = UTF8StringRequestJson.Length();
	if (UTF8RequestJsonStringByteLen >= MaxRequestBodySize)
	{
		UE_LOG_ONLINE(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - Json Payload is too large."));
		sceNpWebApiDeleteRequest(WebRequestId);
		SendResults();
		return;
	}

	FMemory::Memcpy(SessionRequestJson, UTF8StringRequestJson.Get(), UTF8RequestJsonStringByteLen);

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
			UE_LOG_ONLINE_SESSION(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiAddMultipartPart for session request returned with error code 0x%x"), ResultCode);
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
			UE_LOG_ONLINE_SESSION(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiAddMultipartPart for session image returned with error code 0x%x"), ResultCode);
			sceNpWebApiDeleteRequest(WebRequestId);
			SendResults();
			return;
		}
	}

	// Part 3: Session Data
	int32 SessionDataIndex = 0;
	char SessionData[FOnlineSessionPS4::MAX_SESSION_DATA_LENGTH] = {};
	int32 SessionDataLength = FOnlineSessionPS4::CopyCustomSettingDataToBuffer(Data.NewSessionSettings, SessionData);

	UE_LOG_ONLINE_SESSION(VeryVerbose, TEXT("FOnlineAsyncTaskPS4SessionCreate session data payload:\n%s"), ANSI_TO_TCHAR(SessionData));

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
			UE_LOG_ONLINE_SESSION(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiAddMultipartPart for session data returned with error code 0x%x"), ResultCode);
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
		UE_LOG_ONLINE_SESSION(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiSendMultipartRequest2 for session request returned with error code 0x%x"), ResultCode);

		if (ResponseInformationOption.httpStatus == EHttpResponseCodes::TooManyRequests)
		{
			FNpWebApiThrottle::DetermineNextWebRequestTime(ENpApiGroup::SessionAndInvitation, WebRequestId);
		}

		sceNpWebApiDeleteRequest(WebRequestId);
		SendResults();
		return;
	}

	// Part 2
	ResultCode = sceNpWebApiSendMultipartRequest2(WebRequestId, SessionImageIndex, ImageData.GetData(), ImageData.Num(), &ResponseInformationOption);
	if (ResultCode < 0)
	{
		UE_LOG_ONLINE_SESSION(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiSendMultipartRequest2 for session image returned with error code 0x%x"), ResultCode);

		if (ResponseInformationOption.httpStatus == EHttpResponseCodes::TooManyRequests)
		{
			FNpWebApiThrottle::DetermineNextWebRequestTime(ENpApiGroup::SessionAndInvitation, WebRequestId);
		}

		sceNpWebApiDeleteRequest(WebRequestId);
		SendResults();
		return;
	}

	// Part 3
	ResultCode = sceNpWebApiSendMultipartRequest2(WebRequestId, SessionDataIndex, SessionData, SessionDataLength, &ResponseInformationOption);
	if (ResultCode < 0)
	{
		UE_LOG_ONLINE_SESSION(Display, TEXT("FOnlineAsyncTaskPS4SessionCreate failed - sceNpWebApiSendMultipartRequest2 for session data returned with error code 0x%x"), ResultCode);

		if (ResponseInformationOption.httpStatus == EHttpResponseCodes::TooManyRequests)
		{
			FNpWebApiThrottle::DetermineNextWebRequestTime(ENpApiGroup::SessionAndInvitation, WebRequestId);
		}

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

	EPS4Session::Type SessionType = PS4Subsystem.AreRoomsEnabled() ? EPS4Session::RoomSession : EPS4Session::StandaloneSession;

	TSharedPtr<FOnlineSessionInfoPS4> NewSessionInfo = MakeShareable(new FOnlineSessionInfoPS4(SessionType, SCE_NP_MATCHING2_INVALID_WORLD_ID, SCE_NP_MATCHING2_INVALID_LOBBY_ID, SCE_NP_MATCHING2_INVALID_ROOM_ID));
	Data.Session->SessionInfo = NewSessionInfo;

	if (HttpStatusCode == 200)
	{
		// The ResponseData contains Json of the form {"sessionId":"xxx-some-hex-numbers-xxx"}
		UE_LOG_ONLINE_SESSION(Log, TEXT("Create session response body:\n%s"), UTF8_TO_TCHAR(ResponseData.GetData()));
		UE_LOG_ONLINE_SESSION(Log, TEXT("Create session payload: %s"), *SessionRequestJsonStr);

		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(UTF8_TO_TCHAR(ResponseData.GetData()));
		if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			NewSessionInfo->SessionId = FUniqueNetIdString(JsonObject->GetStringField("sessionId"), PS4_SUBSYSTEM);
		}

		if (PS4Subsystem.AreRoomsEnabled())
		{
			FOnlineSessionPS4Ptr PS4SessionSubsystem = StaticCastSharedPtr<FOnlineSessionPS4>(PS4Subsystem.GetSessionInterface());
			FOnlineAsyncTaskPS4_PutChangeableSessionData* PutDataTask = new FOnlineAsyncTaskPS4_PutChangeableSessionData(PS4Subsystem, UserWebApiContext, NewSessionInfo->SessionId, TEXT("INVALIDSESSION"));
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
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Create session response code:%d"), HttpStatusCode);
	}

	sceNpWebApiDeleteRequest(WebRequestId);

	if (!Data.bWasSuccessful && Data.Session)
	{
		UE_LOG_ONLINE_SESSION(Error, TEXT("FOnlineSessionPS4::CreateSession: DoCreateJoinRoom FAILED."));
		// if the room creation fails, retroactively destroy the session
		FOnlineAsyncTaskPS4_DestroySession DestroyTask(PS4Subsystem, UserWebApiContext, Data.Session->SessionName, Data.Session->GetSessionIdStr(), FOnDestroySessionCompleteDelegate());
		DestroyTask.DoWork();
		SendResults();
		return;
	}
}


void FOnlineAsyncTaskPS4_CreateSession::SendResults()
{
	FOnlineAsyncTaskManagerPS4 * AsyncTaskManager = PS4Subsystem.GetAsyncTaskManager();
	if (AsyncTaskManager)
	{
		auto NewEvent = new FAsyncEventPS4_CreateSessionComplete(PS4Subsystem, Data);
		AsyncTaskManager->AddToOutQueue(NewEvent);
	}
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_DestroySession
//////////////////////////////////////////////////////////////////////////

void FOnlineAsyncTaskPS4_DestroySession::DoWork()
{
	// Build our URI
	const FString URL = FString::Printf(TEXT("/v1/sessions/%s"), *SessionId);

	// Start task
	FAsyncTask<FWebApiPS4Task> AsyncTask(UserWebApiContext);
	AsyncTask.GetTask().SetRequest(ENpApiGroup::SessionAndInvitation, URL, SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_DELETE);
	AsyncTask.StartSynchronousTask();

	// Get result
	const FWebApiPS4Task& TaskResult = AsyncTask.GetTask();

	// Determine if we succeeded
	const int32 StatusCode = TaskResult.GetHttpStatusCode();
	const bool bWasSuccessful = StatusCode == 204;

	// Log our result
	if (bWasSuccessful)
	{
		UE_LOG_ONLINE_SESSION(Verbose, TEXT("Destroy Session successful Name=[%s] SessionId=[%s]"), *SessionName.ToString(), *SessionId);
	}
	else
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to Destroy Session Name=[%s] SessionId=[%s] HttpStatus=[%d] Error=[%s]"), *SessionName.ToString(), *SessionId, StatusCode, *TaskResult.GetErrorString());
	}

	// Report results
	FOnlineAsyncTaskManagerPS4* const AsyncTaskManager = PS4Subsystem.GetAsyncTaskManager();
	if (AsyncTaskManager)
	{
		auto NewEvent = new FAsyncEventPS4_DestroySessionComplete(PS4Subsystem, SessionName, bWasSuccessful, CompletionDelegate);
		AsyncTaskManager->AddToOutQueue(NewEvent);
	}
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_JoinSession
//////////////////////////////////////////////////////////////////////////

void FOnlineAsyncTaskPS4_JoinSession::DoWork()
{
	const FString URL = FString::Printf(TEXT("/v1/sessions/%s/members"), *SessionId);

	FAsyncTask<FWebApiPS4Task> Task(UserWebApiContext);
	Task.GetTask().SetRequest(ENpApiGroup::SessionAndInvitation, URL, SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_POST);
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
				UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to join ps4 session: %s"), *ErrorJsonObject->GetStringField("message"));

				switch (PSNJsonErrorCode)
				{
					// there don't appear to be identifiers for these error codes, so go go magic numbers
				case 2114561:
					Result = EOnJoinSessionCompleteResult::SessionIsFull;
					UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to join ps4 session: Session is full"));
					break;
				case 2113549:
					Result = EOnJoinSessionCompleteResult::SessionDoesNotExist;
					UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to join ps4 session: Session does not exist"));
					break;
				default:
					Result = EOnJoinSessionCompleteResult::UnknownError;
					UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to join ps4 session: Unknown Error Code. HTTPStatusCode=[%d] PSNErrorCode=[%d] Error=[%s] Response=[%s]"), StatusCode, PSNJsonErrorCode, *ErrorString, *Response);
					break;
				}
			}
			else
			{
				UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to join ps4 session and reason is missing. HTTPStatusCode=[%d] Error=[%s] Response=[%s]"), StatusCode, *ErrorString, *Response);
			}
		}
		else
		{
			UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to join ps4 session and cannot deserialize response. HTTPStatusCode=[%d] Error=[%s] Response=[%s]"), StatusCode, *ErrorString, *Response);
		}
	}

	if (FOnlineAsyncTaskManagerPS4* AsyncTaskManager = PS4Subsystem.GetAsyncTaskManager())
	{
		FAsyncEventPS4_JoinSessionComplete* NewEvent = new FAsyncEventPS4_JoinSessionComplete(PS4Subsystem, SessionName, DesiredSession, Result);
		AsyncTaskManager->AddToOutQueue(NewEvent);
	}
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_LeaveSession
//////////////////////////////////////////////////////////////////////////

void FOnlineAsyncTaskPS4_LeaveSession::DoWork()
{
	FOnlineAsyncTaskManagerPS4* AsyncTaskManager = PS4Subsystem.GetAsyncTaskManager();
	if (AsyncTaskManager == nullptr)
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("FOnlineAsyncTaskPS4SessionLeave: AsyncTaskManager is null - missing task manager."));
		return;
	}

	/*if (Session == nullptr)
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("FOnlineAsyncTaskPS4SessionLeave: Session pointer is null - we are unable to leave it."));
		FAsyncEventPS4_LeaveSessionComplete* const NewEvent = new FAsyncEventPS4_LeaveSessionComplete(PS4Subsystem, NAME_None, false, CompletionDelegate);
		AsyncTaskManager->AddToOutQueue(NewEvent);
		return;
	}
*/
	FOnlineSessionPS4Ptr PS4SessionSubsystem = StaticCastSharedPtr<FOnlineSessionPS4>(PS4Subsystem.GetSessionInterface());
	FNamedOnlineSession* Session = PS4SessionSubsystem->GetNamedSession(SessionName);

	if (Session == nullptr || !Session->SessionInfo.IsValid())
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("FOnlineAsyncTaskPS4SessionLeave: Session->SessionInfo pointer is null - we are unable to leave %s. This might mean the session has already been left."), *SessionName.ToString());
		FAsyncEventPS4_LeaveSessionComplete* const NewEvent = new FAsyncEventPS4_LeaveSessionComplete(PS4Subsystem, SessionName, false, CompletionDelegate);
		AsyncTaskManager->AddToOutQueue(NewEvent);
		return;
	}

	const FString URL = FString::Printf(TEXT("/v1/sessions/%s/members/me"), *SessionId);

	FAsyncTask<FWebApiPS4Task> AsyncTask(UserWebApiContext);
	AsyncTask.GetTask().SetRequest(ENpApiGroup::SessionAndInvitation, URL, SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_DELETE);
	AsyncTask.StartSynchronousTask();

	const FWebApiPS4Task& TaskResult = AsyncTask.GetTask();

	const int32 StatusCode = TaskResult.GetHttpStatusCode();
	const bool bWasSuccessful = StatusCode == 204;

	if (bWasSuccessful)
	{
		UE_LOG_ONLINE_SESSION(Verbose, TEXT("Leave Session successful Name=[%s] SessionId=[%s]"), *SessionName.ToString(), *SessionId);
	}
	else
	{
		UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to leave session Name=[%s] SessionId=[%s] HttpStatus=[%d] Error=[%s]"), *SessionName.ToString(), *SessionId, StatusCode, *TaskResult.GetErrorString());
	}

	FAsyncEventPS4_LeaveSessionComplete* const NewEvent = new FAsyncEventPS4_LeaveSessionComplete(PS4Subsystem, SessionName, bWasSuccessful, CompletionDelegate);
	AsyncTaskManager->AddToOutQueue(NewEvent);
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_GetSessionData
//////////////////////////////////////////////////////////////////////////

FOnlineAsyncTaskPS4_GetSessionData::FOnlineAsyncTaskPS4_GetSessionData(FOnlineSubsystemPS4& PS4Subsystem, TSharedRef<const FUniqueNetIdPS4> InUserId, TSharedPtr<FOnlineSessionSearch> InSessionSearch, const FOnGetSessionComplete& InOnGetSessionDataComplete)
	: FOnlineAsyncTaskPS4(&PS4Subsystem)
	, UserId(InUserId)
	, OnlineSessionSearch(InSessionSearch)
	, OnGetSessionDataComplete(InOnGetSessionDataComplete)
{
}

void FOnlineAsyncTaskPS4_GetSessionData::Tick()
{
	if (!bIsInitialized)
	{
		bIsInitialized = true;

		FAsyncTask<FWebApiPS4Task> task(Subsystem->GetUserWebApiContext(UserId.Get()));
		task.GetTask().SetRequest(ENpApiGroup::SessionAndInvitation, FString(TEXT("/v1/sessions/")) + OnlineSessionSearch->SearchResults[0].Session.SessionInfo->GetSessionId().ToString() + FString(TEXT("/sessionData")), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);
		task.StartSynchronousTask();
		bWasSuccessful = task.GetTask().GetHttpStatusCode() == 200;
		bIsComplete = true;

		TaskResponseBody = task.GetTask().GetResponseBody();
	}
}

void FOnlineAsyncTaskPS4_GetSessionData::TriggerDelegates()
{
	OnGetSessionDataComplete.ExecuteIfBound(UserId.Get(), OnlineSessionSearch, TaskResponseBody, bWasSuccessful);
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_GetSession
//////////////////////////////////////////////////////////////////////////

FOnlineAsyncTaskPS4_GetSession::FOnlineAsyncTaskPS4_GetSession(FOnlineSubsystemPS4& PS4Subsystem, TSharedRef<const FUniqueNetIdPS4> InUserId, TSharedPtr<FOnlineSessionSearch> InSessionSearch, const FOnGetSessionComplete& InOnGetSessionComplete)
	: FOnlineAsyncTaskPS4(&PS4Subsystem)
	, UserId(InUserId)
	, OnlineSessionSearch(InSessionSearch)
	, OnGetSessionComplete(InOnGetSessionComplete)
{
}

void FOnlineAsyncTaskPS4_GetSession::Tick()
{
	if (!bIsInitialized)
	{
		bIsInitialized = true;

		FAsyncTask<FWebApiPS4Task> Task(Subsystem->GetUserWebApiContext(UserId.Get()));
		const FString& SessionId = OnlineSessionSearch->SearchResults[0].Session.SessionInfo->GetSessionId().ToString();
		Task.GetTask().SetRequest(ENpApiGroup::SessionAndInvitation, FString(TEXT("/v1/sessions/")) + SessionId + FString(TEXT("?fields=@default,members")), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);
		Task.StartSynchronousTask();

		const int32 HttpStatusCode = Task.GetTask().GetHttpStatusCode();

		TaskResponseBody = Task.GetTask().GetResponseBody();
		bWasSuccessful = HttpStatusCode == 200;

		const FOnlineError& ErrorResult = Task.GetTask().GetErrorResult();
		if (bWasSuccessful)
		{
			UE_LOG_ONLINE_SESSION(VeryVerbose, TEXT("GetSession: SessionId=[%s] HTTPStatusCode=%d Result=[%s] ResponseBody=[%s]"), *SessionId, HttpStatusCode, *ErrorResult.ToLogString(), *TaskResponseBody);
		}
		else
		{
			UE_LOG_ONLINE_SESSION(Warning, TEXT("GetSession: SessionId=[%s] HTTPStatusCode=%d Result=[%s] ResponseBody=[%s]"), *SessionId, HttpStatusCode, *ErrorResult.ToLogString(), *TaskResponseBody);
		}

		bIsComplete = true;
	}
}

void FOnlineAsyncTaskPS4_GetSession::TriggerDelegates()
{
	OnGetSessionComplete.ExecuteIfBound(UserId.Get(), OnlineSessionSearch, TaskResponseBody, bWasSuccessful);
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_LeaveRoom
//////////////////////////////////////////////////////////////////////////

FOnlineAsyncTaskPS4_LeaveRoom::FOnlineAsyncTaskPS4_LeaveRoom(FOnlineSubsystemPS4& InPS4Subsystem, const FName& InSessionName, const FOnDestroySessionCompleteDelegate& InDestroySessionDelegate, SceNpMatching2RequestId InRequestId)
	: FOnlineAsyncTaskPS4(&InPS4Subsystem)
	, PS4Subsystem(InPS4Subsystem)
	, SessionName(InSessionName)
	, DestroySessionDelegate(InDestroySessionDelegate)
	, RequestId(InRequestId)
{
}

void FOnlineAsyncTaskPS4_LeaveRoom::Tick()
{
	if (!bIsInitialized)
	{
		bIsInitialized = true;

		FOnlineSessionPS4Ptr PS4SessionSubsystem = StaticCastSharedPtr<FOnlineSessionPS4>(PS4Subsystem.GetSessionInterface());
		FNamedOnlineSession* Session = PS4SessionSubsystem->GetNamedSession(SessionName);

		//this process fires off the destroysessioncomplete delegate, so we set the destroying state.
		if (Session != nullptr)
		{
			Session->SessionState = EOnlineSessionState::Destroying;

			// Guaranteed to be called after the flush is complete
			TSharedPtr<FOnlineSessionInfoPS4> PS4SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoPS4>(Session->SessionInfo);
			if (PS4SessionInfo.IsValid())
			{
				if (PS4SessionInfo->SessionType == EPS4Session::RoomSession && PS4SessionInfo->RoomId != SCE_NP_MATCHING2_INVALID_ROOM_ID)
				{
					SceNpMatching2LeaveRoomRequest LeaveRoomRequestParameters;
					FMemory::Memset(&LeaveRoomRequestParameters, 0, sizeof(LeaveRoomRequestParameters));
					SceNpMatching2RequestOptParam RequestOptionParameters;
					FMemory::Memset(&RequestOptionParameters, 0, sizeof(RequestOptionParameters));

					RequestOptionParameters.cbFunc = FStaticPS4SessionCallbacks::HandleRequestEvent;
					RequestOptionParameters.cbFuncArg = this;

					LeaveRoomRequestParameters.roomId = PS4SessionInfo->RoomId;

					FOnlineSessionPS4Ptr OnlineSessionPS4Ptr = StaticCastSharedPtr<FOnlineSessionPS4>(Subsystem->GetSessionInterface());
					if (OnlineSessionPS4Ptr.IsValid())
					{
						// don't use the HostingPlayerNum because a sign-out event might trigger the leave, and we won't be able to look up the netid from the usernum in that case.
						TSharedPtr<const FUniqueNetIdPS4> UserNetId = FUniqueNetIdPS4::Cast(Session->bHosting ? Session->OwningUserId : Session->LocalOwnerId);
						int LeaveRoomReturnCode = sceNpMatching2LeaveRoom(OnlineSessionPS4Ptr->GetUserMatching2Context(*UserNetId, false), &LeaveRoomRequestParameters, &RequestOptionParameters, &RequestId);
						if (LeaveRoomReturnCode < SCE_OK)
						{
							UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to leave a room - 0x%x"), LeaveRoomReturnCode);
							bIsComplete = true;
						}
						else
						{
							UE_LOG_ONLINE_SESSION(Verbose, TEXT("Attempting to leave room - 0x%x"), PS4SessionInfo->RoomId);
						}
					}
					else
					{
						UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to leave a room - Invalid session info"));
						bIsComplete = true;
					}
				}
				else
				{
					UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to leave a room - Session Type (%d) or Room ID (%lu) invalid"), static_cast<int32>(PS4SessionInfo->SessionType), static_cast<uint64>(PS4SessionInfo->RoomId));
					bIsComplete = true;
				}
			}
			else
			{
				UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to leave a room - Invalid session info"));
				bIsComplete = true;
			}
		}
		else
		{
			UE_LOG_ONLINE_SESSION(Warning, TEXT("Failed to leave a room - Invalid session"));
			bIsComplete = true;
		}
	}
}

void FOnlineAsyncTaskPS4_LeaveRoom::ProcessCallbackResult(int InErrorCode)
{
	UE_LOG_ONLINE_SESSION(Verbose, TEXT("Left room result was successful: - %d"), InErrorCode == SCE_OK);
	ErrorCode = InErrorCode;
	bWasSuccessful = ErrorCode == SCE_OK;
	bIsComplete = true;
}

void FOnlineAsyncTaskPS4_LeaveRoom::TriggerDelegates()
{
	FOnlineSessionPS4* OnlineSessionPS4 = (FOnlineSessionPS4*)Subsystem->GetSessionInterface().Get();
	if (OnlineSessionPS4)
	{
		OnlineSessionPS4->TriggerOnEndSessionCompleteDelegates(SessionName, true);
		OnlineSessionPS4->OnRoomLeaveComplete(SessionName, bWasSuccessful, DestroySessionDelegate);
	}
}


//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_SendSessionInvite
//////////////////////////////////////////////////////////////////////////

FOnlineAsyncTaskPS4_SendSessionInvite::FOnlineAsyncTaskPS4_SendSessionInvite(FOnlineSubsystemPS4& InPS4Subsystem, const FUniqueNetIdPS4Ref& InUserId, const FName& InSessionName, const FString& InSessionId, const TArray<TSharedRef<const FUniqueNetId>>& InFriends)
	: FOnlineAsyncTaskPS4(&InPS4Subsystem)
	, PS4Subsystem(InPS4Subsystem)
{
	check(InFriends.Num() <= NpToolkit::Session::Request::SendInvitation::MAX_NUM_RECIPIENTS);

	if (FNpWebApiThrottle::GetRateLimitStatus(ENpApiGroup::SessionAndInvitation) == EOnlinePSNRateLimitStatus::RateLimited)
	{
		UE_LOG_ONLINE_SESSION(Error, TEXT("Session::sendInvitation Rate limited"));
		bWasSuccessful = false;
		bIsComplete = true;
		return;
	}
	
	NpToolkit::Session::Request::SendInvitation Request;
	Request.userId = InUserId->GetUserId();
	Request.serviceLabel = SCE_NP_DEFAULT_SERVICE_LABEL;
	Request.async = true;

	Request.sessionId = PS4StringToSessionId(InSessionId);

	Request.numRecipients = InFriends.Num();
	for (int32 Index = 0; Index < InFriends.Num(); ++Index)
	{
		Request.recipients[Index] = FUniqueNetIdPS4::Cast(InFriends[Index])->GetAccountId();
	}

	FOnlineSessionPS4Ptr PS4SessionSubsystem = StaticCastSharedPtr<FOnlineSessionPS4>(PS4Subsystem.GetSessionInterface());
	FNamedOnlineSession* Session = PS4SessionSubsystem->GetNamedSession(InSessionName);
	if (Session == nullptr)
	{
		UE_LOG_ONLINE_SESSION(Error, TEXT("Session::sendInvitation Invalid session [%s]"), *InSessionName.ToString());
		bWasSuccessful = false;
		bIsComplete = true;
		return;
	}

	Request.maxNumberRecipientsToAdd = Session->NumOpenPrivateConnections + Session->NumOpenPublicConnections;
	Request.recipientsEditableByUser = false;
	Request.enableDialog = false;

	int32 ResultCode = NpToolkit::Session::sendInvitation(Request, &SendInviteResponse);
	if (ResultCode < SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		UE_LOG_ONLINE_SESSION(Error, TEXT("Session::sendInvitation (sync) failed with error code 0x%08x"), ResultCode);
		bWasSuccessful = false;
		bIsComplete = true;
	}
}

void FOnlineAsyncTaskPS4_SendSessionInvite::Tick()
{
	if (!SendInviteResponse.isLocked())
	{
		// No results to gather when this request completes.
		// Just log any errors.
		
		int32 ResultCode = SendInviteResponse.getReturnCode();
		bWasSuccessful = ResultCode == SCE_TOOLKIT_NP_V2_SUCCESS;

		if (!bWasSuccessful)
		{
			UE_LOG_ONLINE_SESSION(Error, TEXT("Session::sendInvitation (async) failed with error code 0x%08x"), ResultCode);

			// Print out the ServerError if any exists
			const NpToolkit::Core::ServerError* const ServerError = SendInviteResponse.getServerError();
			if (ServerError)
			{
				PrintNPToolkitServerError(TEXT("FOnlineAsyncTaskPS4_SendSessionInvite"), *ServerError);

				// If the request failed due to the rate limit, set next time for future requests to the same service
				if (ServerError->httpStatusCode == EHttpResponseCodes::TooManyRequests)
				{
					// webApiNextAvailableTime returns a delay in seconds
					FDateTime NextTime = FDateTime::UtcNow() + FTimespan::FromSeconds(ServerError->webApiNextAvailableTime);
					FNpWebApiThrottle::SetNextWebRequestTime(ENpApiGroup::SessionAndInvitation, NextTime);
				}
			}
		}
		bIsComplete = true;
	}
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_GetChangeableSessionData
//////////////////////////////////////////////////////////////////////////

void FOnlineAsyncTaskPS4_GetChangeableSessionData::DoWork()
{
	FAsyncTask<FWebApiPS4Task> Task(UserWebApiContext);
	FString UrlPath = FString::Printf(TEXT("/v1/sessions/%s/changeableSessionData"), *SessionId);
	Task.GetTask().SetRequest(ENpApiGroup::SessionAndInvitation, MoveTemp(UrlPath), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_GET);
	Task.StartSynchronousTask();

	FOnlineAsyncTaskManagerPS4* const AsyncTaskManager = PS4Subsystem.GetAsyncTaskManager();
	if (LIKELY(AsyncTaskManager != nullptr))
	{
		const int32 HttpStatusCode = Task.GetTask().GetHttpStatusCode();
		const bool bSucceeded = (HttpStatusCode == 200);
		FString ResponseBody = Task.GetTask().GetResponseBody();
		const FOnlineError& ErrorResult = Task.GetTask().GetErrorResult();
		UE_LOG_ONLINE_SESSION(VeryVerbose, TEXT("GetChangeableSessionData: SessionId=[%s] HTTPStatusCode=%d Result=[%s] ResponseBody=[%s]"), *SessionId, HttpStatusCode, *ErrorResult.ToLogString(), *ResponseBody);
		FAsyncEventPS4_GetChangeableSessionDataComplete* const NewEvent = new FAsyncEventPS4_GetChangeableSessionDataComplete(PS4Subsystem, UserId, MoveTemp(SessionId), MoveTemp(ResponseBody), Callback, bSucceeded);
		AsyncTaskManager->AddToOutQueue(NewEvent);
	}
}

void FOnlineAsyncTaskPS4_PutChangeableSessionData::DoWork()
{
	FAsyncTask<FWebApiPS4Task> Task(UserWebApiContext);
	FString UrlPath = FString::Printf(TEXT("/v1/sessions/%s/changeableSessionData"), *SessionId);
	Task.GetTask().SetRequest(ENpApiGroup::SessionAndInvitation, MoveTemp(UrlPath), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_PUT);
	Task.GetTask().SetContentType("application/octet-stream");
	Task.GetTask().SetRequestBody(Data);
	Task.StartSynchronousTask();

	//Callback?
	const int32 HttpStatusCode = Task.GetTask().GetHttpStatusCode();
	const FOnlineError& ErrorResult = Task.GetTask().GetErrorResult();
	UE_LOG_ONLINE_SESSION(VeryVerbose, TEXT("PutChangeableSessionData: SessionId=[%s] HTTPStatusCode=%d Result=[%s] Data=[%s]"), *SessionId, HttpStatusCode, *ErrorResult.ToLogString(), *Data);
}

//////////////////////////////////////////////////////////////////////////
// FOnlineAsyncTaskPS4_PutSession
//////////////////////////////////////////////////////////////////////////

void FOnlineAsyncTaskPS4_PutSession::DoWork()
{
	FAsyncTask<FWebApiPS4Task> task(UserWebApiContext);
	task.GetTask().SetRequest(ENpApiGroup::SessionAndInvitation, FString(TEXT("/v1/sessions/")) + SessionId.ToString(), SceNpWebApiHttpMethod::SCE_NP_WEBAPI_HTTP_METHOD_PUT);
	task.GetTask().SetContentType("application/json; charset=utf-8");

	FString SessionRequestJsonStr;
	SessionRequestJsonStr += TEXT("{");
	SessionRequestJsonStr += FString::Printf(TEXT("\"sessionPrivacy\" : \"%s\", "), UpdatedSessionSettings.bShouldAdvertise ? TEXT("public") : TEXT("private"));
	SessionRequestJsonStr += FString::Printf(TEXT("\"sessionMaxUser\" : %d"), UpdatedSessionSettings.NumPrivateConnections + UpdatedSessionSettings.NumPublicConnections);
	SessionRequestJsonStr += TEXT("}");

	task.GetTask().SetRequestBody(SessionRequestJsonStr);
	task.StartSynchronousTask();

	UE_LOG_ONLINE_SESSION(Log, TEXT("PutSession(%s) completed with result=%d payload=%s"), *SessionId.ToString(), task.GetTask().GetHttpStatusCode(), *SessionRequestJsonStr);
}

//////////////////////////////////////////////////////////////////////////
// Async Events
//////////////////////////////////////////////////////////////////////////

void FAsyncEventPS4_JoinSession::TriggerDelegates()
{
	StaticCastSharedPtr<FOnlineSessionPS4>(Subsystem->GetSessionInterface())->JoinSession(JoiningPlayerId.Get(), SessionName, DesiredSession);
}

void FAsyncEventPS4_CreateSessionComplete::TriggerDelegates()
{
	FOnlineSessionPS4Ptr PS4Session = StaticCastSharedPtr<FOnlineSessionPS4>(Subsystem->GetSessionInterface());
	if (PS4Session.IsValid())
	{
		PS4Session->OnSessionCreateComplete(Data);
	}
}

void FAsyncEventPS4_DestroySessionComplete::TriggerDelegates()
{
	FOnlineSessionPS4Ptr PS4Session = StaticCastSharedPtr<FOnlineSessionPS4>(Subsystem->GetSessionInterface());
	if (PS4Session.IsValid())
	{
		PS4Session->OnSessionDestroyComplete(SessionName, bWasSuccessful, CompletionDelegate);
	}
}

void FAsyncEventPS4_JoinSessionComplete::TriggerDelegates()
{
	FOnlineSessionPS4Ptr PS4Session = StaticCastSharedPtr<FOnlineSessionPS4>(Subsystem->GetSessionInterface());
	if (PS4Session.IsValid())
	{
		PS4Session->OnSessionJoinComplete(SessionName, DesiredSession, Result);
	}
}

void FAsyncEventPS4_LeaveSessionComplete::TriggerDelegates()
{
	FOnlineSessionPS4Ptr PS4Session = StaticCastSharedPtr<FOnlineSessionPS4>(Subsystem->GetSessionInterface());
	if (PS4Session.IsValid())
	{
		// we use destroy here since the logic is the same, but we may need an OnSessionLeaveComplete at some point
		PS4Session->OnSessionDestroyComplete(SessionName, bWasSuccessful, CompletionDelegate);
	}
}

void FAsyncEventPS4_GetChangeableSessionDataComplete::TriggerDelegates()
{
	int32 LocalUserNum = FPS4Application::GetPS4Application()->GetUserIndex(UserId->GetUserId());
	Callback.ExecuteIfBound(LocalUserNum, SessionId, ChangeableSessionData, bWasSuccessful);
}