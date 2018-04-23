// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "HttpPS4.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "PS4PlatformHttp.h"

/**
 * Helper for setting up a valid Internet connection for use by Http request
 */
class FPS4HTTPConnection
{
public:

	/**
	 * Constructor
	 */
	FPS4HTTPConnection()	
	{
	}

	/**
	 * Singleton accessor
	 */
	static FPS4HTTPConnection& Get()
	{
		static FPS4HTTPConnection Singleton;
		return Singleton;
	}

	/**
	 * Close the internet connection handle
	 *
	 * @return true if succeeded
	 */
	bool ShutdownConnection()
	{
		UE_LOG(LogHttp, Log, TEXT("Closing internet connection"));

		bool bSuccess = true;

		/* Delete all previously created connection settings and template settings */
		for (int i = 0; i < ConnectionIds.Num(); ++i)
		{
			int ConnectionId = ConnectionIds[i];
			bSuccess &= DestroyConnectionId(ConnectionId);
		}			
		return bSuccess;
	}

	/**
	 * Determine if internet connection handle is valid
	 *
	 * @return true if valid
	 */
	bool IsConnectionValid(int32 ConnectionId)
	{
		return ConnectionId > -1;
	}

	/**
	 * Get the connection Id or create for the given URL
	 *
	 * @Param
	 */
	int CreateConnectionId(const FString& ConnectionURL, int TemplateId)
	{
		int ConnectionId = sceHttpCreateConnectionWithURL(TemplateId, TCHAR_TO_ANSI(*ConnectionURL), SCE_TRUE);
		if (ConnectionId == SCE_HTTP_ERROR_BEFORE_INIT)
		{
			UE_LOG(LogHttp, Warning, TEXT("CreateConnectionWithURL failed, The library is not initialized"));
			return -1;
		}
		else if (ConnectionId == SCE_HTTP_ERROR_OUT_OF_MEMORY)
		{
			UE_LOG(LogHttp, Warning, TEXT("CreateConnectionWithURL failed, Insufficient free memory space"));
			return -1;
		}
		else if (ConnectionId == SCE_HTTP_ERROR_UNKNOWN_SCHEME)
		{
			UE_LOG(LogHttp, Warning, TEXT("CreateConnectionWithURL failed, A scheme other than HTTP or HTTPS was specified in the URI: %s"), *ConnectionURL);
			return -1;
		}
		else if (ConnectionId == SCE_HTTP_ERROR_INVALID_ID)
		{
			UE_LOG(LogHttp, Warning, TEXT("CreateConnectionWithURL failed, The specified template ID is invalid: %d"), TemplateId);
			return -1;
		}
		else if (ConnectionId <= 0)
		{
			UE_LOG(LogHttp, Warning, TEXT("CreateConnectionWithURL failed, unknown error: %d"), ConnectionId);
			return -1;
		}
		return ConnectionId;
	}

	bool DestroyConnectionId(int ConnectionId)
	{
		bool bSuccess = true;
		if (ConnectionId != -1)
		{
			int ReturnCode = sceHttpDeleteConnection(ConnectionId);

			if (ReturnCode==SCE_HTTP_ERROR_BEFORE_INIT)
			{
				UE_LOG(LogHttp, Warning, TEXT("HttpDeleteConnection failed, The library is not initialized"));				
				bSuccess = false;
			}
			else if (ReturnCode==SCE_HTTP_ERROR_INVALID_ID)
			{
				UE_LOG(LogHttp, Warning, TEXT("HttpDeleteConnection failed, The ConnectionId specified for the argument is invalid: %i"),ConnectionId);				
				bSuccess = false;
			}
		}
		return bSuccess;
	}

private:
	/* Connection Ids used for processing requests */
	TArray<int32> ConnectionIds;	
};

// FPS4HTTPRequest
FPS4HTTPRequest::FPS4HTTPRequest(int PlatformHttpTemplateId)
:	TemplateId(PlatformHttpTemplateId),
	RequestId(-1),
	ConnectionId(-1),
	bRequestSent(false),
	CurrentPS4State(EPS4RequestState::ESend),
	CompletionStatus(EHttpRequestStatus::NotStarted),	
	PollingHandle(nullptr),
	LastReportedBytesRead(0)
{
}

FPS4HTTPRequest::~FPS4HTTPRequest()
{
}

FString FPS4HTTPRequest::GetURIPart(uint32 URIPartToGet)
{
	char *RebuildUri;

	size_t MallocSize, UseSize;

	//First call to sceHttpUriBuild is to get size for storing the returned value
	int ReturnCode = sceHttpUriBuild(NULL, &MallocSize, 0, &RequestURL, URIPartToGet);
	if (ReturnCode == SCE_HTTP_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. The number of bytes necessary for output exceeded the value specified by prepare."));
		return TEXT("");
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_VALUE)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. Both out/pool and require were NULL"));
		return TEXT("");
	}

	TArray<char> Buffer;
	Buffer.AddUninitialized(MallocSize);
	RebuildUri = Buffer.GetData();

	if (RebuildUri == NULL){
		UE_LOG(LogHttp, Warning, TEXT("Unable to allocate memory for buffer storage."));
		return TEXT("");
	}

	ReturnCode = sceHttpUriBuild(RebuildUri, &UseSize, MallocSize, &RequestURL, URIPartToGet);
	if (ReturnCode == SCE_HTTP_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. The number of bytes necessary for output exceeded the value specified by prepare."));
		return TEXT("");
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_VALUE)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. Both out/pool and require were NULL"));
		return TEXT("");
	}

	return FString(RebuildUri);
}



FString FPS4HTTPRequest::GetURL()
{
	return GetURIPart(SCE_HTTP_URI_BUILD_WITH_ALL);
}

FString FPS4HTTPRequest::GetURLParameter(const FString& ParameterName)
{
	return GetURIPart(PS4UriParameterName(ParameterName));
}

FString FPS4HTTPRequest::GetHeader(const FString& HeaderName)
{
	FString* Header = RequestHeaders.Find(HeaderName);
	return Header != NULL ? *Header : TEXT("");
}

TArray<FString> FPS4HTTPRequest::GetAllHeaders()
{
	TArray<FString> Result;
	for (TMap<FString, FString>::TConstIterator It(RequestHeaders); It; ++It)
	{
		Result.Add(It.Key() + TEXT(": ") + It.Value());
	}
	return Result;
}

FString FPS4HTTPRequest::GetContentType()
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FPS4HTTPRequest::GetContentLength()
{
	return RequestPayload.Num();
}

const TArray<uint8>& FPS4HTTPRequest::GetContent()
{
	return RequestPayload;
}

FString FPS4HTTPRequest::GetVerb()
{
	return RequestVerb;
}

void FPS4HTTPRequest::SetVerb(const FString& Verb)
{
	RequestVerb = Verb;
}

void FPS4HTTPRequest::SetURL(const FString& URL)
{
	size_t MallocSize, UseSize;

	//First call the sceHttpUriParse is to get the size for storing the object
	int32 ReturnCode = sceHttpUriParse(NULL, TCHAR_TO_ANSI(*URL) , NULL, &MallocSize, 0);
	if (ReturnCode == SCE_HTTP_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. The number of bytes necessary for output exceeded the value specified by prepare."));
		return;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_VALUE)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. Both out/pool and require were NULL"));
		return;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_URL)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. The format of the URI specified for srcUri is invalid."));
		return;
	}

	RequestURLBuffer.Empty(MallocSize);
	RequestURLBuffer.AddUninitialized(MallocSize);
	void* BufferForParsedResult = RequestURLBuffer.GetData();

	if (BufferForParsedResult == NULL){
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. Can't allocate memory to store parsed URL."));
		return;
	}

	ReturnCode = sceHttpUriParse(&RequestURL, TCHAR_TO_ANSI(*URL), BufferForParsedResult, &UseSize, MallocSize);
	if (ReturnCode == SCE_HTTP_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. The number of bytes necessary for output exceeded the value specified by prepare."));
		return;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_VALUE)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. Both out/pool and require were NULL"));
		return;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_URL)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. The format of the URI specified for srcUri is invalid."));
		return;
	}

}

void FPS4HTTPRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	RequestPayload = ContentPayload;
}

void FPS4HTTPRequest::SetContentAsString(const FString& ContentString)
{
	FTCHARToUTF8 Converter(*ContentString);
	RequestPayload.SetNum(Converter.Length());
	FMemory::Memcpy(RequestPayload.GetData(), (uint8*)(ANSICHAR*)Converter.Get(), RequestPayload.Num());
}

void FPS4HTTPRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	RequestHeaders.Add(HeaderName, HeaderValue);
}

void FPS4HTTPRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
	if (!HeaderName.IsEmpty() && !AdditionalHeaderValue.IsEmpty())
	{
		FString* PreviousValue = RequestHeaders.Find(HeaderName);
		FString NewValue;
		if (PreviousValue != nullptr && !PreviousValue->IsEmpty())
		{
			NewValue = (*PreviousValue) + TEXT(", ");
		}
		NewValue += AdditionalHeaderValue;

		SetHeader(HeaderName, NewValue);
	}
}

bool FPS4HTTPRequest::ProcessRequest()
{
	bool bStarted = false;

	// Prevent overlapped requests using the same instance
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
	}
	// Nothing to do without a valid URL
	else if (GetURL().IsEmpty())
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. No URL was specified."));
	}
	// Make sure the URL is parsed correctly with a valid HTTP scheme
	else if (GetURLParameter(TEXT("Scheme")) != TEXT("http://") &&
		GetURLParameter(TEXT("Scheme")) != TEXT("https://"))
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. URL '%s' is not a valid HTTP request. %p"),
			*GetURL(), this);
	}	
	else
	{
		// Mark as in-flight to prevent overlapped requests using the same object
		CompletionStatus = EHttpRequestStatus::Processing;
		// Response object to handle data that comes back after starting this request
		Response = MakeShareable(new FPS4HTTPResponse(*this));		

		bStarted = true;

		// Add to global list so the request gets ticked.
		FHttpModule::Get().GetHttpManager().AddThreadedRequest(SharedThis(this));
	}
	StartRequestTime = FPlatformTime::Seconds();
	// reset the elapsed time.
	ElapsedTime = 0.0f;

	// always call completion delegates
	if (!bStarted)
	{
		FinishedRequest();
	}

	// Successfully started the request
	return bStarted;
}

FHttpRequestCompleteDelegate& FPS4HTTPRequest::OnProcessRequestComplete()
{
	return RequestCompleteDelegate;
}

FHttpRequestProgressDelegate& FPS4HTTPRequest::OnRequestProgress() 
{
	return RequestProgressDelegate;
}

void FPS4HTTPRequest::CancelRequest()
{
	//@todo implement
}

EHttpRequestStatus::Type FPS4HTTPRequest::GetStatus()
{
	return CompletionStatus;
}

const FHttpResponsePtr FPS4HTTPRequest::GetResponse() const
{
	return Response;
}

bool FPS4HTTPRequest::TickSendState()
{
	check(CurrentPS4State == EPS4RequestState::ESend);

	bRequestSent = true;
	int32 ReturnCode = sceHttpSendRequest(RequestId, RequestPayload.Num() > 0 ? RequestPayload.GetData() : NULL, RequestPayload.Num());
	if (ReturnCode == SCE_HTTP_ERROR_BEFORE_INIT)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. The library is not initialized."));
		bRequestSent = false;
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_BUSY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. One of these three has occurred:"));
		UE_LOG(LogHttp, Warning, TEXT("	- Multiple threads attempted to send requests simultaneously using the same connection settings"));
		UE_LOG(LogHttp, Warning, TEXT("	- Attempted to send the next request using the same connection settings before sceHttpReadData() finished receiving data"));
		UE_LOG(LogHttp, Warning, TEXT("	- Attempted to send another request using the same connection settings before the sending of POST data completed"));
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode >= SCE_HTTP_ERROR_RESOLVER_EPACKET && ReturnCode <= SCE_HTTP_ERROR_RESOLVER_ENORECORD)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. DNS resolver error %i."), ReturnCode);
		bRequestSent = false;
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_NET_ERROR_ENOLIBMEM)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Insufficient free library memory space. (error: %d)"), ReturnCode);
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Insufficient free http memory space. (error: %d)"), ReturnCode);
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_SSL_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Insufficient free SSL memory space. (error: %d)"), ReturnCode);
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_SSL)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. HTTPS certificate error."));
		bRequestSent = false;
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_NETWORK)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. An error was returned by the TCP stack."));
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_TIMEOUT)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Either the timeout period set using the timeout setting function has passed or the TCP timeout period has passed."));
		// NOTE: confirmed with sony that the send-timeout-timer stops when the last byte of the request is written to the socket, so if we get a send timeout
		// it means we have sent (at most) a partial request which the server should harmlessly reject.
		// HOWEVER: sceHttpSendRequest may also report SCE_HTTP_ERROR_TIMEOUT in the case of a receive timeout because it actually waits for the first response headers to come down.
		//bRequestSent = false; we must assume the request was sent here :-(
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_PROXY)
	{
		int LastErrorNumber;
		//Get the last error from http to find more details
		ReturnCode = sceHttpGetLastErrno(RequestId, &LastErrorNumber);
		if (ReturnCode<0)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Failed to establish the connection to the HTTP Proxy"));
		}
		else
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Failed to establish the connection to the HTTP Proxy: %i"),LastErrorNumber);
		}

		bRequestSent = false;
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_ID)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. The ID specified for the argument is invalid: %i"),RequestId);
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_EAGAIN)
	{
		// possible valid response.  Tick function will call send again next frame
	}
	else if (ReturnCode < 0)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Unknown error: %i RequestId: %i"), ReturnCode, RequestId);
		CurrentPS4State = EPS4RequestState::EFail;
		return true;
	}
	else
	{
		check(ReturnCode == SCE_OK);

		// send completed.  Move to next state.
		CurrentPS4State = EPS4RequestState::EGetStatus;
		return true;
	}

	return false;
}

bool FPS4HTTPRequest::TickStatusState()
{
	// get the response code
	Response->ResponseCode = EHttpResponseCodes::Unknown;

	int ReturnCode = sceHttpGetStatusCode(RequestId,&Response->ResponseCode);
	switch(ReturnCode)
	{
		case SCE_HTTP_ERROR_BEFORE_INIT:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetStatusCode failed. The library is not initialized.  %p"), this);
			CurrentPS4State = EPS4RequestState::EFail;
			return true;		
		case SCE_HTTP_ERROR_BEFORE_SEND:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetStatusCode failed. The specified request has not been sent yet.  %p"), this);
			CurrentPS4State = EPS4RequestState::EFail;
			return true;
		case SCE_HTTP_ERROR_INVALID_ID:			
			UE_LOG(LogHttp, Warning, TEXT("HttpGetStatusCode failed. The ID specified for the argument is invalid: %i.  %p"),RequestId, this);
			CurrentPS4State = EPS4RequestState::EFail;
			return true;
		case SCE_HTTP_ERROR_EAGAIN:
			return false;
		case SCE_OK:
			break;
		default:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetStatusCode failed with error: 0x%i"), ReturnCode);
			CurrentPS4State = EPS4RequestState::EFail;
			return true;
	}

	// next step is content length for all responses
	if (Response->ResponseCode > 0)
	{
		CurrentPS4State = EPS4RequestState::EGetLen;
		return true;
	}
	
	return false;
}

bool FPS4HTTPRequest::TickLenState()
{
	int32 Result = 0;
	uint64_t ReturnContentLength = 0;

	int ReturnCode = sceHttpGetResponseContentLength(RequestId, &Result, &ReturnContentLength);
	switch (ReturnCode)
	{
		case SCE_HTTP_ERROR_BEFORE_INIT:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetResponseContentLength failed. The library is not initialized. %p"), this);
			CurrentPS4State = EPS4RequestState::EFail;
			return true;
		case SCE_HTTP_ERROR_BEFORE_SEND:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetResponseContentLength failed. The specified request has not been sent yet. %p"), this);
			CurrentPS4State = EPS4RequestState::EFail;
			return true;
		case SCE_HTTP_ERROR_INVALID_ID:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetResponseContentLength failed. The ID specified for the argument is invalid: %i. %p"),RequestId, this);
			CurrentPS4State = EPS4RequestState::EFail;
			return true;
		case SCE_HTTP_ERROR_EAGAIN:
			// result not ready, try again next frame.
			return false;
		case SCE_OK:
			// success!  carry on to actual handling.
			break;
		default: 
			UE_LOG(LogHttp, Warning, TEXT("HttpGetResponseContentLength failed with error: 0x%x"), ReturnCode);
			CurrentPS4State = EPS4RequestState::EFail;
			return true;
	}

	switch (Result)
	{
		case SCE_HTTP_CONTENTLEN_CHUNK_ENC:
			// not a failure case, response body can still be received by calling sceHttpReadData multiple times.
			UE_LOG(LogHttp, Log, TEXT("HttpGetResponseContentLength failed. The Content-Length could not be obtained since it was chunk encoded. %p"), this);
			break;
		case SCE_HTTP_CONTENTLEN_NOT_FOUND:
			CurrentPS4State = EPS4RequestState::ESuccess;
			Response->ResponseContentLength = 0;
			Response->bResponseSucceeded = true;
			Response->bIsReady = true;
			return true;

		case SCE_HTTP_CONTENTLEN_EXIST:			
			// success! carry on processing
			break;
		default:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetResponseContentLength failed with Result error: 0x%x"), Result);
			CurrentPS4State = EPS4RequestState::EFail;
			return true;
	}	

	verify(ReturnContentLength<INT_MAX);	
	Response->ResponseContentLength = ReturnContentLength & 0xffffffff;
	CurrentPS4State = EPS4RequestState::ERecv;
	return true;
}

bool FPS4HTTPRequest::TickRecvState()
{
	bool bTickAgain = false;

	int32 BytesRead = 0;
	FPS4HTTPResponse::EPayloadStatus PayloadStatus = Response->ReadPayload(BytesRead);

	switch (PayloadStatus)
	{
		case FPS4HTTPResponse::EPayloadStatus::EReadAgain:			
			break;	
		case FPS4HTTPResponse::EPayloadStatus::ESuccess:			
			Response->ProcessResponse();
			check(Response->bIsReady);
			CurrentPS4State = EPS4RequestState::ESuccess;
			bTickAgain = true;
			break;
		case FPS4HTTPResponse::EPayloadStatus::EFail:
			CurrentPS4State = EPS4RequestState::EFail;
			bTickAgain = true;
		default:
			break;
	}
	
	return bTickAgain;
}


void FPS4HTTPRequest::Tick(float DeltaSeconds)
{
	CheckProgressDelegate();
}

void FPS4HTTPRequest::CheckProgressDelegate()
{
	if (Response.IsValid())
	{
		int32 CurrentBytesRead = Response->TotalBytesRead.GetValue();
		if (CurrentBytesRead != LastReportedBytesRead)
		{
			LastReportedBytesRead = CurrentBytesRead;
			// Update response progress
			OnRequestProgress().ExecuteIfBound(SharedThis(this), 0, LastReportedBytesRead);
		}
	}
}

void FPS4HTTPRequest::TickThreadedRequest(float DeltaSeconds)
{
	int32 ReturnCode = 0;

	SceHttpNBEvent NetworkEvent;
	ReturnCode = sceHttpWaitRequest(PollingHandle, &NetworkEvent, 1, 1);
	if (ReturnCode > 0 && NetworkEvent.id == RequestId)
	{
		if (NetworkEvent.events & (SCE_HTTP_NB_EVENT_SOCK_ERR | SCE_HTTP_NB_EVENT_HUP | SCE_HTTP_NB_EVENT_RESOLVER_ERR))
		{
			UE_LOG(LogHttp, Warning, TEXT("HTTPRequest: 0x%x got network event error: 0x%x."), RequestId, NetworkEvent.events);
			CurrentPS4State = EPS4RequestState::EFail;			
		}
	}
	if (ReturnCode < 0 ) 
	{
		UE_LOG(LogHttp, Warning, TEXT("sceHttpWaitRequest error: 0x%x."), ReturnCode);
		CurrentPS4State = EPS4RequestState::EFail;		
	}		

	const float HttpTimeout = FHttpModule::Get().GetHttpTimeout();
	ElapsedTime = (float)(FPlatformTime::Seconds() - StartRequestTime);

	if (HttpTimeout > 0.0f && ElapsedTime >= HttpTimeout)
	{
		UE_LOG(LogHttp, Warning, TEXT("Timeout processing Http request. %p"), this);
		CurrentPS4State = EPS4RequestState::EFail;
	}

	bool bTickAgain = false;
	switch (CurrentPS4State)
	{
		case EPS4RequestState::ESend:			
			bTickAgain = TickSendState();
			break;
		case EPS4RequestState::EGetStatus:
			bTickAgain = TickStatusState();
			break;
		case EPS4RequestState::EGetLen:
			bTickAgain = TickLenState();
			break;		
		case EPS4RequestState::ERecv:
			bTickAgain = TickRecvState();
			break;
		case EPS4RequestState::ESuccess:
			UE_LOG(LogHttp, Log, TEXT("HttpRequest Succeeded on Request: %p, RequestID: 0x%x, URL: %s."), this, RequestId, *GetURL());
			CleanupRequest();
			break;
		case EPS4RequestState::EFail:
		default:
			UE_LOG(LogHttp, Warning, TEXT("HttpRequest failed on Request: %p, RequestID: 0x%x, URL: %s."), this, RequestId, *GetURL());
			CleanupRequest();
			break;
	}

	// Recursive tick calls slightly less ugly than fallthrough case statements. Maximum 5 deep.
	if (bTickAgain)
	{
		TickThreadedRequest(DeltaSeconds);
	}
}

int32 FPS4HTTPRequest::PS4UriParameterName(const FString& ParameterName)
{
	// @todo what parameters are going to be requested?
	if (ParameterName == TEXT("Scheme"))
	{
		return SCE_HTTP_URI_BUILD_WITH_SCHEME;
	}
	else if (ParameterName == TEXT("Hostname"))
	{
		return SCE_HTTP_URI_BUILD_WITH_HOSTNAME;
	}
	else if (ParameterName == TEXT("Port"))
	{
		return SCE_HTTP_URI_BUILD_WITH_PORT;
	}
	else if (ParameterName == TEXT("Path"))
	{
		return SCE_HTTP_URI_BUILD_WITH_PATH;
	}
	else if (ParameterName == TEXT("Username"))
	{
		return SCE_HTTP_URI_BUILD_WITH_USERNAME;
	}
	else if (ParameterName == TEXT("Password"))
	{
		return SCE_HTTP_URI_BUILD_WITH_PASSWORD;
	}
	else if (ParameterName == TEXT("Query"))
	{
		return SCE_HTTP_URI_BUILD_WITH_QUERY;
	}
	else if (ParameterName == TEXT("Fragment"))
	{
		return SCE_HTTP_URI_BUILD_WITH_FRAGMENT;
	}
	else
	{
		return -1;
	}
}

int32 FPS4HTTPRequest::PS4Verb()
{

	//Convert Verb to PS4 recognized value.
	if (RequestVerb.IsEmpty()) //Use Get for Empty as with WinInet (http://msdn.microsoft.com/en-us/library/windows/desktop/aa384233%28v=vs.85%29.aspx)
	{
		return SCE_HTTP_METHOD_GET;
	}
	else if (RequestVerb == TEXT("GET"))
	{
		return SCE_HTTP_METHOD_GET;
	}
	else if (RequestVerb == TEXT("HEAD"))
	{
		return SCE_HTTP_METHOD_HEAD;
	}
	else if (RequestVerb == TEXT("POST"))
	{
		return SCE_HTTP_METHOD_POST;
	}
	else if (RequestVerb == TEXT("PUT"))
	{
		return SCE_HTTP_METHOD_PUT;
	}
	else if (RequestVerb == TEXT("DELETE"))
	{
		return SCE_HTTP_METHOD_DELETE;
	}
	else if (RequestVerb == TEXT("TRACE"))
	{
		return SCE_HTTP_METHOD_TRACE;
	}
	else
	{
		//Default to negative number so that processes needing this value will error
		return -1;
	}
}

bool FPS4HTTPRequest::StartRequest()
{
	// Make sure old handles are not being reused
	CleanupRequest();
	CurrentPS4State = EPS4RequestState::ESend;

	ConnectionId = FPS4HTTPConnection::Get().CreateConnectionId(GetURL(),TemplateId);

	UE_LOG(LogHttp, Log, TEXT("Start request. %p %s url=%s"), this, *GetVerb(), *GetURL());
	if (UE_LOG_ACTIVE(LogHttp, Verbose))
	{
		for (TMap<FString, FString>::TConstIterator It(RequestHeaders); It; ++It)
		{
			if (!It.Key().Contains(TEXT("Authorization")))
			{
				UE_LOG(LogHttp, Verbose, TEXT("%p Header %s : %s"), this, *It.Key(), *It.Value());
			}
		}
	}

	if (ConnectionId == -1)
	{
		UE_LOG(LogHttp, Warning, TEXT("StartRequest couldn't create valid ConnectionId."));
		return false;
	}

	RequestId = sceHttpCreateRequestWithURL(ConnectionId, PS4Verb(), TCHAR_TO_ANSI(*GetURL()), RequestPayload.Num());
	
	if (RequestId == SCE_HTTP_ERROR_BEFORE_INIT)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpCreateRequestWithURL failed. The library is not initialized."));
		return false;
	}
	else if (RequestId == SCE_HTTP_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpCreateRequestWithURL failed. Insufficient free memory space."));
		return false;
	}
	else if (RequestId == SCE_HTTP_ERROR_INVALID_VERSION)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpCreateRequestWithURL failed. PUT or DELETE was set for method using connection settings when 1.0 was set as the HTTP version."));
		return false;
	}
	else if (RequestId == SCE_HTTP_ERROR_UNKNOWN_METHOD)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpCreateRequestWithURL failed. The value specified in method is invalid: %i"), PS4Verb());
		return false;
	}
	else if (RequestId == SCE_HTTP_ERROR_INVALID_ID)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpCreateRequestWithURL failed. The specified ID of the connection settings is invalid: %i"), ConnectionId);
		return false;
	}

	if (!AddRequestHeaders())
	{
		return false;
	}

	int32 ReturnCode = sceHttpSetNonblock(RequestId, SCE_TRUE);
	if (ReturnCode != SCE_OK)
	{
		UE_LOG(LogHttp, Warning, TEXT("sceHttpSetNonblock failed with request ID: %i, Error: 0x%x"), RequestId, ReturnCode);
		return false;
	}

	ReturnCode = sceHttpCreateEpoll(FPS4PlatformHttp::GetLibHttpCtxId(), &PollingHandle);
	if (ReturnCode != SCE_OK)
	{
		UE_LOG(LogHttp, Warning, TEXT("sceHttpCreateEpoll failed, Error: 0x%x"), ReturnCode);		
		return false;
	}

	ReturnCode = sceHttpSetEpoll(RequestId, PollingHandle, this);
	if (ReturnCode != SCE_OK)
	{
		UE_LOG(LogHttp, Warning, TEXT("sceHttpSetEpoll failed on RequestID: %i, Error: 0x%x"), RequestId, ReturnCode);		
		return false;
	}

	CurrentPS4State = EPS4RequestState::ESend;

	// Successfully started the request, sceHttpSendRequest() blocks until processing is completed. Specifically, the function returns after the HTTP request is sent and the response header is received from the server.
	return true;
}

void FPS4HTTPRequest::FinishedRequest()
{
	ElapsedTime = (float)(FPlatformTime::Seconds() - StartRequestTime);
	if (Response.IsValid() &&
		Response->bResponseSucceeded)
	{
		// Mark last request attempt as completed successfully
		CompletionStatus = EHttpRequestStatus::Succeeded;
		// Call delegate with valid request/response objects
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this),Response,true);
	}
	else
	{
		// Mark last request attempt as completed but failed
		CompletionStatus = bRequestSent ? EHttpRequestStatus::Failed : EHttpRequestStatus::Failed_ConnectionError;
		// No response since connection failed
		Response = NULL;
		// Call delegate with failure
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this),NULL,false);
	}
}

void FPS4HTTPRequest::CleanupRequest()
{
	if (RequestId > -1)
	{
		int ReturnCode = sceHttpDeleteRequest(RequestId);
		if (ReturnCode == SCE_HTTP_ERROR_BEFORE_INIT)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpDeleteRequest failed. The library is not initialized."));
		}
		else if (ReturnCode == SCE_HTTP_ERROR_INVALID_ID)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpDeleteRequest failed. The ID specified for the argument is invalid: %i"),RequestId);
		}
		RequestId = -1;
	}
	
	if (PollingHandle != nullptr)
	{		
		sceHttpDestroyEpoll(FPS4PlatformHttp::GetLibHttpCtxId(), PollingHandle);
		PollingHandle = nullptr;
	}	

	if (ConnectionId != -1)
	{
		FPS4HTTPConnection::Get().DestroyConnectionId(ConnectionId);
		ConnectionId = -1;
	}
}

bool FPS4HTTPRequest::AddRequestHeaders()
{
	for (TMap<FString, FString>::TConstIterator It(RequestHeaders); It; ++It)
	{
		int ReturnCode = sceHttpAddRequestHeader(RequestId, TCHAR_TO_ANSI(*It.Key()), TCHAR_TO_ANSI(*It.Value()), SCE_HTTP_HEADER_ADD);
		
		if (ReturnCode == SCE_HTTP_ERROR_BEFORE_INIT)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpAddRequestHeader failed. The library is not initialized."));
			return false;
		}
		else if (ReturnCode == SCE_HTTP_ERROR_INVALID_ID)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpAddRequestHeader failed. The ID specified for the argument is invalid: %i."),RequestId);
			return false;
		}
		else if (ReturnCode == SCE_HTTP_ERROR_INVALID_VALUE)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpAddRequestHeader failed. Name is NULL or the value specified for mode is invalid."));
			return false;
		}
	}
	return true;
}

float FPS4HTTPRequest::GetElapsedTime()
{
	return ElapsedTime;
}

bool FPS4HTTPRequest::StartThreadedRequest()
{
	return StartRequest();
}

void FPS4HTTPRequest::FinishRequest()
{
	FinishedRequest();
}

bool FPS4HTTPRequest::IsThreadedRequestComplete()
{
	return CurrentPS4State == EPS4RequestState::ESuccess || CurrentPS4State == EPS4RequestState::EFail;
}



// FPS4HTTPResponse

FPS4HTTPResponse::FPS4HTTPResponse(const FPS4HTTPRequest& InRequest)
:	Request(InRequest)
,	RequestURL(InRequest.RequestURL)
,	AsyncBytesRead(0)
,	TotalBytesRead(0)
,	ResponseCode(EHttpResponseCodes::Unknown)
,	ContentLength(0)
,	bIsReady(false)
,	bResponseSucceeded(false)
{

}

FPS4HTTPResponse::~FPS4HTTPResponse()
{

}


FString FPS4HTTPResponse::GetURIPart(uint32 URIPartToGet)
{
	size_t MallocSize, UseSize;

	//First call to sceHttpUriBuild is to get size for storing the returned value
	int ReturnCode = sceHttpUriBuild(NULL, &MallocSize, 0, &RequestURL, URIPartToGet);
	if (ReturnCode == SCE_HTTP_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. The number of bytes necessary for output exceeded the value specified by prepare."));
		return TEXT("");
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_VALUE)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. Both out/pool and require were NULL"));
		return TEXT("");
	}

	TArray<char> Buffer;
	Buffer.AddUninitialized(MallocSize);
	char *RebuildUri = Buffer.GetData();

	if (RebuildUri == NULL){
		UE_LOG(LogHttp, Warning, TEXT("Unable to allocate memory for buffer storage."));
		return TEXT("");
	}

	ReturnCode = sceHttpUriBuild(RebuildUri, &UseSize, MallocSize, &RequestURL, URIPartToGet);
	if (ReturnCode == SCE_HTTP_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. The number of bytes necessary for output exceeded the value specified by prepare."));
		return TEXT("");
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_VALUE)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpUriParse failed. Both out/pool and require were NULL"));
		return TEXT("");
	}

	return FString(RebuildUri);
}

int32 FPS4HTTPResponse::PS4UriParameterName(const FString& ParameterName)
{
	// @todo what parameters are going to be requested?
	if (ParameterName == TEXT("Scheme"))
	{
		return SCE_HTTP_URI_BUILD_WITH_SCHEME;
	}
	else if (ParameterName == TEXT("Hostname"))
	{
		return SCE_HTTP_URI_BUILD_WITH_HOSTNAME;
	}
	else if (ParameterName == TEXT("Port"))
	{
		return SCE_HTTP_URI_BUILD_WITH_PORT;
	}
	else if (ParameterName == TEXT("Path"))
	{
		return SCE_HTTP_URI_BUILD_WITH_PATH;
	}
	else if (ParameterName == TEXT("Username"))
	{
		return SCE_HTTP_URI_BUILD_WITH_USERNAME;
	}
	else if (ParameterName == TEXT("Password"))
	{
		return SCE_HTTP_URI_BUILD_WITH_PASSWORD;
	}
	else if (ParameterName == TEXT("Query"))
	{
		return SCE_HTTP_URI_BUILD_WITH_QUERY;
	}
	else if (ParameterName == TEXT("Fragment"))
	{
		return SCE_HTTP_URI_BUILD_WITH_FRAGMENT;
	}
	else
	{
		return -1;
	}
}

FString FPS4HTTPResponse::GetURL()
{
	return GetURIPart(SCE_HTTP_URI_BUILD_WITH_ALL);
}


FString FPS4HTTPResponse::GetURLParameter(const FString& ParameterName)
{
	return GetURIPart(PS4UriParameterName(ParameterName));
}

FString FPS4HTTPResponse::GetHeader(const FString& HeaderName)
{
	FString Result(TEXT(""));
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached header [%s]. Response still processing. %p"),
			*HeaderName, &Request);
	}
	else
	{
		FString* Header = ResponseHeaders.Find(HeaderName);
		if (Header != NULL)
		{
			return *Header;
		}
	}
	return Result;
}

TArray<FString> FPS4HTTPResponse::GetAllHeaders()
{
	TArray<FString> Result;
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached headers. Response still processing. %p"),&Request);
	}
	else
	{
		for (TMap<FString, FString>::TConstIterator It(ResponseHeaders); It; ++It)
		{
			Result.Add(It.Key() + TEXT(": ") + It.Value());
		}
	}
	return Result;
}

FString FPS4HTTPResponse::GetContentType()
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FPS4HTTPResponse::GetContentLength()
{
	return ContentLength;
}

const TArray<uint8>& FPS4HTTPResponse::GetContent()
{
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"),&Request);
	}
	return ResponsePayload;
}

int32 FPS4HTTPResponse::GetResponseCode()
{
	return ResponseCode;
}

FString FPS4HTTPResponse::GetContentAsString()
{
	TArray<uint8> ZeroTerminatedPayload(GetContent());
	ZeroTerminatedPayload.Add(0);
	return UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData());
}

void FPS4HTTPResponse::ProcessResponse()
{	
	if (ContentLength != 0 &&
		TotalBytesRead.GetValue() != ContentLength)
	{
		UE_LOG(LogHttp, Warning, TEXT("Response payload was %d bytes, content-length indicated (%d) bytes. %p"),
			TotalBytesRead.GetValue(), ContentLength, &Request);
	}
	UE_LOG(LogHttp, Verbose, TEXT("TotalBytesRead = %d. %p"), TotalBytesRead.GetValue(), &Request);

	// Shrink array to only the valid data
	ResponsePayload.SetNum(TotalBytesRead.GetValue());

	// Query for header data and cache it
	ProcessResponseHeaders();	

	// Cache content length now that response is done
	ContentLength = QueryContentLength();

	// Mark as valid processed response
	bResponseSucceeded = true;

	// Done processing
	bIsReady = true;
}

void FPS4HTTPResponse::ProcessResponseHeaders()
{
	size_t HeaderSize = 0;
	char* ReturnPtr;


	int32 ReturnCode = sceHttpGetAllResponseHeaders(Request.RequestId, &ReturnPtr, &HeaderSize);
	if (ReturnCode == SCE_HTTP_ERROR_BEFORE_INIT)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpGetAllResponseHeaders failed. The library is not initialized.  %p"), &Request);
	}
	else if (ReturnCode == SCE_HTTP_ERROR_BEFORE_SEND)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpGetAllResponseHeaders failed. The specified request has not been sent yet.  %p"), &Request);
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_ID)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpGetAllResponseHeaders failed. The ID specified for the argument is invalid: %i.  %p"),Request.RequestId, &Request);
	}
	else
	{
		check (ReturnCode == SCE_OK);

		FString HeaderLine;
		FString RestOfHeader = FString(ANSI_TO_TCHAR(ReturnPtr));

		bool RowSplit = false;

		// parse all the key/value pairs
		// don't count the terminating NULL character as one to search.
		do
		{
			RowSplit = RestOfHeader.Split("\r\n", &HeaderLine, &RestOfHeader);

			FString HeaderKey,HeaderValue;

			FString* TextToSplit;

			if (!RowSplit)
			{
				TextToSplit = &RestOfHeader;
			}
			else
			{
				TextToSplit = &HeaderLine;
			}

			if (TextToSplit != nullptr && TextToSplit->Split(TEXT(":"), &HeaderKey, &HeaderValue))
			{
				if (!HeaderKey.IsEmpty() && !HeaderValue.IsEmpty())
				{
					FString* PreviousValue = ResponseHeaders.Find(HeaderKey);
					FString NewValue;
					if (PreviousValue != nullptr && !PreviousValue->IsEmpty())
					{
						NewValue = (*PreviousValue) + TEXT(", ");
					}
					HeaderValue.TrimStartInline();
					NewValue += HeaderValue;
					ResponseHeaders.Add(HeaderKey, NewValue);
				}
			}
		}	while (RowSplit);
	}
}

FPS4HTTPResponse::EPayloadStatus FPS4HTTPResponse::ReadPayload(int32& BytesRead)
{
	BytesRead = 0;

	// We might be calling back into this from another asynchronous read, so continue where we left off.
	// if there is no content length, we're probably receiving chunked data.
	int32 CurContentLength = QueryContentLength();

	// Size of the buffer to read when calling sceHttpReadData. Payload grows by this amount as necessary
	const int32 MAX_READ_BUFFER_SIZE = 16*1024;

	int32 LastTotalBytesRead = TotalBytesRead.GetValue();
	
	// For chunked responses, add data using a fixed size buffer at a time.
	int32 BufferSize = LastTotalBytesRead + MAX_READ_BUFFER_SIZE;

	// For non-chunked responses, allocate one extra uint8 to check if we are sent extra content
	if (ContentLength > 0 && LastTotalBytesRead == 0)
	{
		BufferSize = ContentLength + 1;
	}

	// Size read buffer
	ResponsePayload.SetNum(BufferSize);	

	bool bFailed = false;
	do
	{
		// Read directly into the response payload
		int32 ReturnValue = sceHttpReadData(Request.RequestId, &ResponsePayload[LastTotalBytesRead], ResponsePayload.Num() - LastTotalBytesRead);
		UE_LOG(LogHttp, VeryVerbose, TEXT("TotalBytesRead: %i, StartAddr: %p, SizeRemaining: %i, Read %i bytes of Payload, RequestID: 0x%x"), LastTotalBytesRead, &ResponsePayload[LastTotalBytesRead], ResponsePayload.Num() - LastTotalBytesRead, ReturnValue, Request.RequestId);

		if (ReturnValue == SCE_HTTP_ERROR_BEFORE_INIT)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpReadData failed. The library is not initialized. %p"), &Request);			
			return EPayloadStatus::EFail;
		}
		else if (ReturnValue == SCE_HTTP_ERROR_BEFORE_SEND)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpReadData failed. The specified request has not been sent yet. %p"), &Request);
			return EPayloadStatus::EFail;
		}
		else if (ReturnValue == SCE_HTTP_ERROR_BEFORE_SEND)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpReadData failed. The specified request is for the HEAD method. %p"), &Request);
			return EPayloadStatus::EFail;
		}
		else if (ReturnValue == SCE_HTTP_ERROR_INVALID_ID)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpReadData failed. The ID specified for the argument is invalid: %i. %p"),Request.RequestId, &Request);
			return EPayloadStatus::EFail;
		}
		else if (ReturnValue == SCE_HTTP_ERROR_EAGAIN)
		{
			return EPayloadStatus::EReadAgain;
		}
		else if (ReturnValue < 0)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpReadData failed. Unknown Error: 0x%x. %p"), ReturnValue, &Request);
			return EPayloadStatus::EFail;
		}
		else
		{
			check (ReturnValue >= 0);

			UE_LOG(LogHttp, VeryVerbose, TEXT("HttpReadData result= (%u bytes read) (%u bytes total read). %p"),
				ReturnValue, LastTotalBytesRead, &Request);

			AsyncBytesRead = ReturnValue;
			BytesRead += AsyncBytesRead;

			// Keep track of total read so far
			TotalBytesRead.Add(AsyncBytesRead);
			LastTotalBytesRead = TotalBytesRead.GetValue();

			// resize the buffer if we don't know our content length, otherwise don't let the buffer grow larger than content length.
			if (LastTotalBytesRead >= ResponsePayload.Num())
			{
				if (ContentLength > 0)
				{
					UE_LOG(LogHttp, VeryVerbose, TEXT("Response payload (%d bytes read so far) is larger than the content-length (%d). Resizing buffer to accommodate. %p"),
						LastTotalBytesRead, ContentLength, &Request);
				}
				ResponsePayload.AddZeroed(MAX_READ_BUFFER_SIZE);
			}
		}
	} 
	while (AsyncBytesRead > 0);

	// ResponseContentLength will be 0 if the response was chunked.  So fill it in if we're done reading the response.
	if (ResponseContentLength == 0)
	{
		ResponseContentLength = LastTotalBytesRead;
	}	
	return EPayloadStatus::ESuccess;
}

int32 FPS4HTTPResponse::QueryContentLength() const
{
	return ResponseContentLength;
}
