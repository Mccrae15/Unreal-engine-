// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebApiPS4Task.h"
#include "WebApiPS4Throttle.h"
#include "Interfaces/IHttpResponse.h"

#define TEST_NP_RATE_LIMIT 0
#define TEST_NP_FORCE_RATE_LIMIT 0
#define TEST_NP_FORCE_RATE_LIMIT_VALUE 50

#define OSS_RATE_LIMIT_ERROR TEXT("errors.com.epicgames.oss.request_rate_limited")

namespace
{
	static const TCHAR* LexToString(SceNpWebApiHttpMethod HttpMethod)
	{
		switch (HttpMethod)
		{
		case SCE_NP_WEBAPI_HTTP_METHOD_GET:
			return TEXT("GET");
		case SCE_NP_WEBAPI_HTTP_METHOD_POST:
			return TEXT("POST");
		case SCE_NP_WEBAPI_HTTP_METHOD_PUT:
			return TEXT("PUT");
		case SCE_NP_WEBAPI_HTTP_METHOD_DELETE:
			return TEXT("DELETE");
		}

		return TEXT("UNKNOWN");
	}
}

/**
 * FWebApiPS4Task::DoWork()
 * Note: This is a blocking function.
 * The idea is for WebApiPS4Task to be called via FAsyncTask<FWebApiPS4Task>::StartBackgroundTask()
 */
void FWebApiPS4Task::DoWork()
{
	bHasStartedWork = true;

	if (LocalUserContext < SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		Result.ErrorCode = FString::Printf(TEXT("0x%08x"), LocalUserContext);
		Result.ErrorMessage = FText::FromString(FString::Printf(TEXT("DoWork() failed; invalid local user context %s"), *Result.ErrorCode));
		Result.bSucceeded = false;
		return;
	}

	check(ApiGroup != ENpApiGroup::Invalid);

	if (FNpWebApiThrottle::GetRateLimitStatus(ApiGroup) == EOnlinePSNRateLimitStatus::RateLimited)
	{
		UE_LOG_ONLINE(Warning, TEXT("Attempted request for ApiGroup %s, but was rate-limited"), LexToString(ApiGroup));

		NumericErrorCode = EHttpResponseCodes::TooManyRequests;
		Result.ErrorCode = FString(OSS_RATE_LIMIT_ERROR);
		Result.ErrorMessage = NSLOCTEXT("OnlineSubsystem", "RequestRateLimited", "Your request would exceed the rate-limit, and has been cancelled. Please try again later.");
		Result.bSucceeded = false;
		return;
	}

	SceNpWebApiContentParameter ContentParam;
	FMemory::Memset(&ContentParam, 0, sizeof(ContentParam));

	const FTCHARToUTF8 UTF8Body(*RequestBody);

	ContentParam.contentLength = UTF8Body.Length();

	TUniquePtr<ANSICHAR[]> AnsiContentType;
	if (ContentType.IsEmpty())
	{
		ContentParam.pContentType = SCE_NP_WEBAPI_CONTENT_TYPE_APPLICATION_JSON_UTF8;
	}
	else
	{
		const TStringConversion<TStringConvert<TCHAR, ANSICHAR>> ConvertedContentType(*ContentType);
		const int32 ContentSize = ConvertedContentType.Length() + 1;
		AnsiContentType = MakeUnique<ANSICHAR[]>(ContentSize);
		FCStringAnsi::Strncpy(AnsiContentType.Get(), ConvertedContentType.Get(), ContentSize);

		ContentParam.pContentType = AnsiContentType.Get();
	}

	UE_LOG_ONLINE(VeryVerbose, TEXT("AsyncWebTaskPS4 ApiGroup=[%s] Method=[%s] Path=[%s]"), LexToString(ApiGroup), LexToString(Method), *Path);

	int64_t RequestId = 0;
	int32 ReturnCode = sceNpWebApiCreateRequest(LocalUserContext, TCHAR_TO_UTF8(LexToString(ApiGroup)), TCHAR_TO_UTF8(*Path), Method, &ContentParam, &RequestId);

	if (ReturnCode < SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		Result.ErrorCode = FString::Printf(TEXT("0x%08x"), ReturnCode);
		Result.ErrorMessage = FText::FromString(FString::Printf(TEXT("sceNpWebApiCreateRequest() failed. error = %s"), *Result.ErrorCode));
		Result.bSucceeded = false;
		return;
	}

	float WebApiTimeoutValue = -1.0f;

	// Override with the timeout for this specific task if it's set.
	if (TimeoutInSeconds > 0.0f)
	{
		WebApiTimeoutValue = TimeoutInSeconds;
	}
	else
	{
		// Use Ini default if this request doesn't have an override set in TimeoutInSeconds
		GConfig->GetFloat(TEXT("OnlineSubsystemPS4"), TEXT("WebApiTimeoutInSeconds"), WebApiTimeoutValue, GEngineIni);
	}

	if (WebApiTimeoutValue > 0.0f)
	{
		sceNpWebApiSetRequestTimeout(RequestId, WebApiTimeoutValue * 1000 * 1000);
	}

	SceNpWebApiResponseInformationOption ResponseInformationOption;
	FMemory::Memzero(ResponseInformationOption);

	for (const TPair<FString, FString>& Header : Headers)
	{
		sceNpWebApiAddHttpRequestHeader(RequestId, TCHAR_TO_ANSI(*Header.Key), TCHAR_TO_ANSI(*Header.Value));
	}

#if TEST_NP_RATE_LIMIT && !UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR
		sceNpWebApiAddHttpRequestHeader(RequestId, "X-NP-RateLimit", "GetDetail");
#endif // TEST_NP_RATE_LIMIT && !UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR

	// Send the request
	ReturnCode = sceNpWebApiSendRequest2(RequestId, UTF8Body.Get(), UTF8Body.Length(), &ResponseInformationOption);
	if (ReturnCode < SCE_TOOLKIT_NP_V2_SUCCESS)
	{
		Result.ErrorCode = FString::Printf(TEXT("0x%08x"), ReturnCode);
		Result.ErrorMessage = FText::FromString(FString::Printf(TEXT("sceNpWebApiSendRequest2() failed. error = %s"), *Result.ErrorCode));
		Result.bSucceeded = false;

		if (ResponseInformationOption.httpStatus == EHttpResponseCodes::TooManyRequests)
		{
			FNpWebApiThrottle::DetermineNextWebRequestTime(ApiGroup, RequestId);
		}
	}
	else
	{
		// Get the status of our request
		NumericErrorCode = ResponseInformationOption.httpStatus;

		// Read the response in 1024 byte blocks
		static const int32 BlockSize = 1024;

		// Amount of bytes used that are real data
		int32 TotalBytesRead = 0;

		// Buffer to store raw utf8 data
		TArray<char> UTF8Buffer;

		// Preallocate buffer with 4 blocks
		UTF8Buffer.Empty(BlockSize * 4);

		int32 BytesRead = 0;
		do
		{
			// Ensure we have at least a full block allocated
			if (UTF8Buffer.Num() - TotalBytesRead < BlockSize)
			{
				UTF8Buffer.AddUninitialized(BlockSize);
			}

			// Read Data
			BytesRead = sceNpWebApiReadData(RequestId, UTF8Buffer.GetData() + TotalBytesRead, BlockSize);
			if (BytesRead < 0)
			{
				// An error occured if BytesRead is less than 0
				Result.ErrorCode = FString::Printf(TEXT("0x%08x"), ReturnCode);
				Result.ErrorMessage = FText::FromString(FString::Printf(TEXT("sceNpWebApiReadData() failed. error = %s"), *Result.ErrorCode));
				Result.bSucceeded = false;
			}
			else if (BytesRead > 0)
			{
				// Record how many bytes we just read (can be less than BlockSize)
				TotalBytesRead += BytesRead;
			}
		}
		while (BytesRead > 0);

		// If we successfully read data, copy it into our ResponseBody
		if (TotalBytesRead > 0)
		{
			// Trim any excess bytes
			UTF8Buffer.SetNum(TotalBytesRead, false);

			// Get the TCHAR length of our UTF8 buffer (we need this many TCHARs to respresent the UTF8CHAR data)
			const int32 ConvertedLength = FUTF8ToTCHAR_Convert::ConvertedLength(UTF8Buffer.GetData(), TotalBytesRead);
			check(ConvertedLength > 0);

			// Get our response string and allocate it to our ConvertedLength + 1 for the null-terminator
			TArray<TCHAR>& CharArray = ResponseBody.GetCharArray();
			CharArray.SetNumUninitialized(ConvertedLength + 1, false);

			// Convert our UTF8 buffer into our FString response
			FUTF8ToTCHAR_Convert::Convert(CharArray.GetData(), CharArray.Num(), UTF8Buffer.GetData(), UTF8Buffer.Num());

			// Append our null-terminator
			CharArray[ConvertedLength] = '\0';
		}

		// If we have sent too many request, we have to check for an error code that tells us to go away until X time in a UTC Unix Timestamp
		if (NumericErrorCode == EHttpResponseCodes::TooManyRequests)
		{
			FNpWebApiThrottle::DetermineNextWebRequestTime(ApiGroup, RequestId);
		}
	}

#if TEST_NP_RATE_LIMIT && !UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR
	TArray<FString> DebugHeaderNames = {FString(TEXT("X-RateLimit-Next-Available")), FString(TEXT("X-RateLimit-TimePeriod")), FString(TEXT("X-RateLimit-Limit")), FString(TEXT("X-RateLimit-Remaining"))};
	for (const FString& DebugHeaderName : DebugHeaderNames)
	{
		size_t HeaderValueSize = 0;
		int32 TestReturnCode = sceNpWebApiGetHttpResponseHeaderValueLength(RequestId, TCHAR_TO_ANSI(*DebugHeaderName), &HeaderValueSize);
		if (TestReturnCode >= SCE_OK)
		{
			if (HeaderValueSize > 0)
			{
				TUniquePtr<ANSICHAR[]> HeaderValueBuffer = MakeUnique<ANSICHAR[]>(HeaderValueSize);
				TestReturnCode = sceNpWebApiGetHttpResponseHeaderValue(RequestId, TCHAR_TO_ANSI(*DebugHeaderName), HeaderValueBuffer.Get(), HeaderValueSize);
				if (TestReturnCode >= SCE_OK)
				{
					UE_LOG_ONLINE(Log, TEXT("Received Debug Np Rate Limit Header=[%s] Value=[%s]"), *DebugHeaderName, ANSI_TO_TCHAR(HeaderValueBuffer.Get()));
				}
				else
				{
					UE_LOG_ONLINE(Error, TEXT("sceNpWebApiGetHttpResponseHeaderValue() failed. error = 0x%08x"), TestReturnCode);
				}
			}
			else
			{
				UE_LOG_ONLINE(Error, TEXT("Received Debug Np Rate Limit Header=[%s] No Value"), *DebugHeaderName);
			}
		}
		else
		{
			UE_LOG_ONLINE(Error, TEXT("sceNpWebApiGetHttpResponseHeaderValueLength() failed. error = 0x%08x"), TestReturnCode);
		}
	}
#endif // TEST_NP_RATE_LIMIT && !UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR

	// Clean up the request. (Make sure error cases get to this)
	ReturnCode = sceNpWebApiDeleteRequest(RequestId);
	if (ReturnCode < 0)
	{
		UE_LOG_ONLINE(Error, TEXT("sceNpWebApiDeleteRequest() failed. error = 0x%08x"), ReturnCode);
	}
}

void FWebApiPS4Task::SetRequest(const ENpApiGroup InApiGroup, const FString& InPath, const SceNpWebApiHttpMethod InMethod)
{
	check(!bHasStartedWork);

	ApiGroup = InApiGroup;
	Path = InPath;
	Method = InMethod;
}

void FWebApiPS4Task::SetRequest(const ENpApiGroup InApiGroup, FString&& InPath, const SceNpWebApiHttpMethod InMethod)
{
	check(!bHasStartedWork);

	ApiGroup = InApiGroup;
	Path = MoveTemp(InPath);
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
	check(!bHasStartedWork);
	TimeoutInSeconds = InTimeoutInSeconds;
}

const FString& FWebApiPS4Task::GetResponseBody() const
{
	return ResponseBody;
}

const FString& FWebApiPS4Task::GetErrorString() const
{
	return Result.GetErrorMessage().ToString();
}

const FOnlineError& FWebApiPS4Task::GetErrorResult() const
{
	return Result;
}

int32 FWebApiPS4Task::GetHttpStatusCode() const
{
	return NumericErrorCode;
}

bool FWebApiPS4Task::WasSuccessful() const
{
	return Result.WasSuccessful();
}
