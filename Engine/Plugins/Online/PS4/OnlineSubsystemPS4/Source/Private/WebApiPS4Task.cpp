// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "WebApiPS4Task.h"


/**
 * FWebApiPS4Task::DoWork()
 * Note: This is a blocking function.
 * The idea is for WebApiPS4Task to be called via FAsyncTask<FWebApiPS4Task>::StartBackgroundTask()
 */
void FWebApiPS4Task::DoWork()
{
	bHasStartedWork = true;

	SceNpWebApiContentParameter ContentParam;
	FMemory::Memset(&ContentParam, 0, sizeof(ContentParam));
	int32 UTF8BodyLength = FPlatformString::Strlen(TCHAR_TO_UTF8(*RequestBody));

	ContentParam.contentLength = UTF8BodyLength;

	if(ContentType.IsEmpty())
	{
		ContentParam.pContentType = SCE_NP_WEBAPI_CONTENT_TYPE_APPLICATION_JSON_UTF8;
	}
	else
	{
		ContentParam.pContentType = TCHAR_TO_ANSI(*ContentType);
	}

	int64_t RequestId = 0;
	int32 ReturnCode = sceNpWebApiCreateRequest(LocalUserContext, TCHAR_TO_UTF8(*ApiGroup), TCHAR_TO_UTF8(*Path), Method, &ContentParam, &RequestId);

	float WebApiTimeoutValue = -1.0f;
	GConfig->GetFloat(TEXT("OnlineSubsystemPS4"), TEXT("WebApiTimeoutInSeconds"), WebApiTimeoutValue, GEngineIni);

	// Override with the timeout for this specific task if it's set.
	if (TimeoutInSeconds > 0.0f)
	{
		WebApiTimeoutValue = TimeoutInSeconds;
	}

	if (WebApiTimeoutValue > 0.0f)
	{
		sceNpWebApiSetRequestTimeout(RequestId, WebApiTimeoutValue * 1000 * 1000);
	}

	SceNpWebApiResponseInformationOption ResponseInformationOption;
	FMemory::Memzero(ResponseInformationOption);

	if (ReturnCode < 0)
	{
		ErrorStr = FString::Printf(TEXT("sceNpWebApiCreateRequest() failed. error = 0x%08x"), ReturnCode);
	}
	else
	{
		for (TMap<FString, FString>::TConstIterator It(Headers); It; ++It)
		{
			sceNpWebApiAddHttpRequestHeader(RequestId, TCHAR_TO_ANSI(*It->Key), TCHAR_TO_ANSI(*It->Value));
		}

		// Send the request
		ReturnCode = sceNpWebApiSendRequest2(RequestId, TCHAR_TO_UTF8(*RequestBody), UTF8BodyLength, &ResponseInformationOption);
		if (ReturnCode < 0)
		{
			ErrorStr = FString::Printf(TEXT("sceNpWebApiSendRequest2() failed. error = 0x%08x"), ReturnCode);
		}
		else
		{
			// Get the status of our request
			HttpStatusCode = ResponseInformationOption.httpStatus;
			
			// Read the data in. We may need to do so in pieces.
			// We want to do this even in cases of HttpStatusCode not being 200 or 201
			static const int32 BufferSize = 1024;
			char Buffer[BufferSize + 1];

			int32 BytesRead = 0;
			do
			{
				BytesRead = sceNpWebApiReadData(RequestId, Buffer, BufferSize);
				if (BytesRead < 0)
				{
					ErrorStr = FString::Printf(TEXT("sceNpWebApiReadData() failed. error = 0x%08x"), BytesRead);
				}
				else
				{
					Buffer[BytesRead] = '\0';
					ResponseBody += Buffer;
 				}
			} while (BytesRead > 0);			
		}

		// Clean up the request. (Make sure error cases get to this)
		ReturnCode = sceNpWebApiDeleteRequest(RequestId);
		if (ReturnCode < 0)
		{
			UE_LOG_ONLINE(Error, TEXT("sceNpWebApiDeleteRequest() failed. error = 0x%08x"), ReturnCode);
		}
	}
}

void FWebApiPS4Task::SetRequest(const FString& InApiGroup, const FString& InPath, const SceNpWebApiHttpMethod InMethod)
{
	check(!bHasStartedWork);
	ApiGroup = InApiGroup;
	Path = InPath;
	Method = InMethod;
}

void FWebApiPS4Task::SetRequestBody(const FString& InBody)
{
	check(!bHasStartedWork);
	RequestBody = InBody;
}

void FWebApiPS4Task::AddRequestHeader(const FString& InHeader, const FString& InValue)
{
	check(!bHasStartedWork);
	Headers.Add(InHeader, InValue);
}

void FWebApiPS4Task::SetContentType(const FString& InContentType)
{
	check(!bHasStartedWork);
	ContentType = InContentType;
}

void FWebApiPS4Task::SetTimeout(const float InTimeoutInSeconds)
{
	TimeoutInSeconds = InTimeoutInSeconds;
}

const FString& FWebApiPS4Task::GetResponseBody() const
{
	return ResponseBody;
}

const FString& FWebApiPS4Task::GetErrorString() const
{
	return ErrorStr;
}

int FWebApiPS4Task::GetHttpStatusCode() const
{
	return HttpStatusCode;
}
