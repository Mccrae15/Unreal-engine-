// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyHttp.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "SonyPlatformHttp.h"
#include "GenericPlatform/HttpRequestPayload.h"
#include "HAL/FileManager.h"
#include "ProfilingDebugging/CsvProfiler.h"

CSV_DEFINE_CATEGORY(SonyHttp, false);

/**
 * Helper for setting up a valid Internet connection for use by Http request
 */
class FSonyHTTPConnection
{
public:

	/**
	 * Constructor
	 */
	FSonyHTTPConnection()	
	{
	}

	/**
	 * Singleton accessor
	 */
	static FSonyHTTPConnection& Get()
	{
		static FSonyHTTPConnection Singleton;
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

// FSonyHTTPRequest
FSonyHTTPRequest::FSonyHTTPRequest(int PlatformHttpTemplateId)
:	TemplateId(PlatformHttpTemplateId),
	RequestId(-1),
	ConnectionId(-1),
	bRequestSent(false),
	CurrentSonyState(ESonyRequestState::ESend),
	RequestPayload(MakeUnique<FRequestPayloadInMemory>(TArray<uint8>())),
	BytesSent(0),
	CompletionStatus(EHttpRequestStatus::NotStarted),	
	PollingHandle(nullptr),
	StartRequestTime(0.0),
	LastResponseTime(0.0),
	ElapsedTime(0.0f),
	LastReportedBytesRead(0),
	bCanceled(false)
{
	// Add default headers
	const TMap<FString, FString>& DefaultHeaders = FHttpModule::Get().GetDefaultHeaders();
	for (TMap<FString, FString>::TConstIterator It(DefaultHeaders); It; ++It)
	{
		SetHeader(It.Key(), It.Value());
	}

	FMemory::Memzero(RequestURL);
}

FSonyHTTPRequest::~FSonyHTTPRequest()
{
}

FString FSonyHTTPRequest::GetURIPart(uint32 URIPartToGet) const
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



FString FSonyHTTPRequest::GetURL() const
{
	return GetURIPart(SCE_HTTP_URI_BUILD_WITH_ALL);
}

FString FSonyHTTPRequest::GetURLParameter(const FString& ParameterName) const
{
	return GetURIPart(SonyUriParameterName(ParameterName));
}

FString FSonyHTTPRequest::GetHeader(const FString& HeaderName) const
{
	const FString* Header = RequestHeaders.Find(HeaderName);
	return Header != NULL ? *Header : TEXT("");
}

TArray<FString> FSonyHTTPRequest::GetAllHeaders() const
{
	TArray<FString> Result;
	for (TMap<FString, FString>::TConstIterator It(RequestHeaders); It; ++It)
	{
		Result.Add(It.Key() + TEXT(": ") + It.Value());
	}
	return Result;
}

FString FSonyHTTPRequest::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FSonyHTTPRequest::GetContentLength() const
{
	return RequestPayload.IsValid() ? RequestPayload->GetContentLength() : 0;
}

const TArray<uint8>& FSonyHTTPRequest::GetContent() const
{
	static const TArray<uint8> EmptyContent;
	return RequestPayload.IsValid() ? RequestPayload->GetContent() : EmptyContent;
}

FString FSonyHTTPRequest::GetVerb() const
{
	return RequestVerb;
}

void FSonyHTTPRequest::SetVerb(const FString& Verb)
{
	RequestVerb = Verb;
}

void FSonyHTTPRequest::SetURL(const FString& URL)
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

void FSonyHTTPRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	RequestPayload = MakeUnique<FRequestPayloadInMemory>(ContentPayload);
	//SendBuffer.SetNum(RequestPayload->GetContentLength());
}

void FSonyHTTPRequest::SetContentAsString(const FString& ContentString)
{
	FTCHARToUTF8 Converter(*ContentString);
	TArray<uint8> Buffer;
	Buffer.SetNum(Converter.Length());
	FMemory::Memcpy(Buffer.GetData(), (uint8*)(ANSICHAR*)Converter.Get(), Buffer.Num());
	RequestPayload = MakeUnique<FRequestPayloadInMemory>(Buffer);
	//SendBuffer.SetNum(RequestPayload->GetContentLength());
}

bool FSonyHTTPRequest::SetContentAsStreamedFile(const FString& Filename)
{
	UE_LOG(LogHttp, Verbose, TEXT("FPS4HTTPRequest::SetContentAsStreamedFile() - %s"), *Filename);

	FArchive* File = IFileManager::Get().CreateFileReader(*Filename);
	if (File)
	{
		RequestPayload = MakeUnique<FRequestPayloadInFileStream>(MakeShareable(File));
		//SendBuffer.SetNum(MaxSendBufferLength);
		return true;
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("FPS4HTTPRequest::SetContentAsStreamedFile Failed to open %s for reading"), *Filename);
		RequestPayload.Reset();
		return false;
	}
	return false;
}

bool FSonyHTTPRequest::SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	UE_LOG(LogHttp, Verbose, TEXT("FPS4HTTPRequest::SetContentFromStream() - %s"), *Stream->GetArchiveName());

	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FPS4HTTPRequest::SetContentFromStream() - attempted to set content on a request that is inflight"));
		return false;
	}

	RequestPayload = MakeUnique<FRequestPayloadInFileStream>(Stream);
	//SendBuffer.SetNum(MaxSendBufferLength);
	return true;
}

void FSonyHTTPRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	RequestHeaders.Add(HeaderName, HeaderValue);
}

void FSonyHTTPRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
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

bool FSonyHTTPRequest::ProcessRequest()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSonyHTTPRequest_ProcessRequest);
	bool bStarted = false;
	bRequestSent = false;

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
	else if (!FHttpModule::Get().GetHttpManager().IsDomainAllowed(GetURL()))
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. URL '%s' is not using a whitelisted domain. %p"), *GetURL(), this);
	}
	else
	{
		// Mark as in-flight to prevent overlapped requests using the same object
		CompletionStatus = EHttpRequestStatus::Processing;
		//Reset the BytesSent so that we send from the start of the data
		BytesSent = 0;
		// Response object to handle data that comes back after starting this request
		Response = MakeShareable(new FSonyHTTPResponse(*this));		

		bStarted = true;

		// Add to global list so the request gets ticked.
		FHttpModule::Get().GetHttpManager().AddThreadedRequest(SharedThis(this));
	}
	StartRequestTime = FPlatformTime::Seconds();
	LastResponseTime = StartRequestTime;
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

void FSonyHTTPRequest::CancelRequest()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSonyHTTPRequest_CancelRequest);
	bCanceled = true;
	AbortRequest();
	UE_LOG(LogHttp, Verbose, TEXT("%p: HTTP request canceled.  URL=%s"), this, *GetURL());
}

EHttpRequestStatus::Type FSonyHTTPRequest::GetStatus() const
{
	return CompletionStatus;
}

const FHttpResponsePtr FSonyHTTPRequest::GetResponse() const
{
	return Response;
}

bool FSonyHTTPRequest::TickSendState()
{
	CSV_SCOPED_TIMING_STAT(SonyHttp, TickSendState);
	check(CurrentSonyState == ESonyRequestState::ESend);

	int32 ReturnCode;
	size_t NumBytesToSend = 0;
	double SendStartTime = FPlatformTime::Seconds();
	float HTTPSendTime = 0.0f;
	int64 PreviousBytesSent = BytesSent;

	NumBytesToSend = RequestPayload->FillOutputBuffer(SendBuffer, MaxSendBufferLength, BytesSent);

	ReturnCode = sceHttpSendRequest(RequestId, NumBytesToSend > 0 ? SendBuffer : NULL, NumBytesToSend);
	if (ReturnCode == SCE_OK)
	{
		BytesSent += NumBytesToSend;
	}

	HTTPSendTime = (float)(FPlatformTime::Seconds() - SendStartTime);
	UE_LOG(LogHttp, Verbose, TEXT("HttpSendRequest Id %i, returned %d. Took %f ms, uploaded %d Bytes (%d of %d)"), RequestId.Load(), ReturnCode, HTTPSendTime, BytesSent - PreviousBytesSent, BytesSent, RequestPayload->GetContentLength());

	// SCE_HTTP_ERROR_EAGAIN and SCE_OK are the only return codes that indicate the upload can still succeed, return and wait for next tick to upload the next block
	if ((ReturnCode == SCE_HTTP_ERROR_EAGAIN || ReturnCode == SCE_OK)
		&& BytesSent < RequestPayload->GetContentLength())
	{
		return false;
	}

	if (ReturnCode == SCE_HTTP_ERROR_BEFORE_INIT)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. The library is not initialized."));
		bRequestSent = false;
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_BUSY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. One of these three has occurred:"));
		UE_LOG(LogHttp, Warning, TEXT("	- Multiple threads attempted to send requests simultaneously using the same connection settings"));
		UE_LOG(LogHttp, Warning, TEXT("	- Attempted to send the next request using the same connection settings before sceHttpReadData() finished receiving data"));
		UE_LOG(LogHttp, Warning, TEXT("	- Attempted to send another request using the same connection settings before the sending of POST data completed"));
		bRequestSent = true;
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode >= SCE_HTTP_ERROR_RESOLVER_EPACKET && ReturnCode <= SCE_HTTP_ERROR_RESOLVER_ENORECORD)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. DNS resolver error %i."), ReturnCode);
		bRequestSent = false;
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_NET_ERROR_ENOLIBMEM)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Insufficient free library memory space. (error: %d)"), ReturnCode);
		bRequestSent = true;
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Insufficient free http memory space. (error: %d)"), ReturnCode);
		bRequestSent = true;
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_SSL_ERROR_OUT_OF_MEMORY)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Insufficient free SSL memory space. (error: %d)"), ReturnCode);
		bRequestSent = true;
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_SSL)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. HTTPS certificate error."));
		bRequestSent = false;
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_NETWORK)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. An error was returned by the TCP stack."));
		bRequestSent = true;
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_TIMEOUT)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Either the timeout period set using the timeout setting function has passed or the TCP timeout period has passed."));
		// NOTE: confirmed with sony that the send-timeout-timer stops when the last byte of the request is written to the socket, so if we get a send timeout
		// it means we have sent (at most) a partial request which the server should harmlessly reject.
		// HOWEVER: sceHttpSendRequest may also report SCE_HTTP_ERROR_TIMEOUT in the case of a receive timeout because it actually waits for the first response headers to come down.
		//bRequestSent = false; we must assume the request was sent here :-(
		bRequestSent = true;
		CurrentSonyState = ESonyRequestState::EFail;
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
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_ID)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. The ID specified for the argument is invalid: %i"), RequestId.Load());
		bRequestSent = false;
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode == SCE_HTTP_ERROR_EAGAIN)
	{
		// possible valid response.  Tick function will call send again next frame
	}
	else if (ReturnCode == SCE_HTTP_ERROR_ABORTED)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Request was aborted: %i"), RequestId.Load());
		bRequestSent = true;
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else if (ReturnCode < 0)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpSendRequest failed. Unknown error: %i RequestId: %i"), ReturnCode, RequestId.Load());
		bRequestSent = true; // Assume other errors happen after connection is established
		CurrentSonyState = ESonyRequestState::EFail;
		return true;
	}
	else
	{
		check(ReturnCode == SCE_OK);

		// send completed.  Move to next state.
		bRequestSent = true;
		LastResponseTime = FPlatformTime::Seconds();
		CurrentSonyState = ESonyRequestState::EGetStatus;
		return true;
	}

	return false;
}

bool FSonyHTTPRequest::TickStatusState()
{
	// get the response code
	Response->ResponseCode = EHttpResponseCodes::Unknown;

	int ReturnCode = sceHttpGetStatusCode(RequestId, &Response->ResponseCode);
	switch(ReturnCode)
	{
		case SCE_HTTP_ERROR_BEFORE_INIT:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetStatusCode failed. The library is not initialized.  %p"), this);
			CurrentSonyState = ESonyRequestState::EFail;
			return true;		
		case SCE_HTTP_ERROR_BEFORE_SEND:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetStatusCode failed. The specified request has not been sent yet.  %p"), this);
			CurrentSonyState = ESonyRequestState::EFail;
			return true;
		case SCE_HTTP_ERROR_INVALID_ID:			
			UE_LOG(LogHttp, Warning, TEXT("HttpGetStatusCode failed. The ID specified for the argument is invalid: %i.  %p"), RequestId.Load(), this);
			CurrentSonyState = ESonyRequestState::EFail;
			return true;
		case SCE_HTTP_ERROR_EAGAIN:
			return false;
		case SCE_OK:
			break;
		default:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetStatusCode failed with error: 0x%i"), ReturnCode);
			CurrentSonyState = ESonyRequestState::EFail;
			return true;
	}

	// next step is content length for all responses
	if (Response->ResponseCode > 0)
	{
		LastResponseTime = FPlatformTime::Seconds();
		CurrentSonyState = ESonyRequestState::EGetLen;
		return true;
	}
	
	return false;
}

bool FSonyHTTPRequest::TickLenState()
{
	int32 Result = 0;
	uint64_t ReturnContentLength = 0;

	int ReturnCode = sceHttpGetResponseContentLength(RequestId, &Result, &ReturnContentLength);
	switch (ReturnCode)
	{
		case SCE_HTTP_ERROR_BEFORE_INIT:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetResponseContentLength failed. The library is not initialized. %p"), this);
			CurrentSonyState = ESonyRequestState::EFail;
			return true;
		case SCE_HTTP_ERROR_BEFORE_SEND:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetResponseContentLength failed. The specified request has not been sent yet. %p"), this);
			CurrentSonyState = ESonyRequestState::EFail;
			return true;
		case SCE_HTTP_ERROR_INVALID_ID:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetResponseContentLength failed. The ID specified for the argument is invalid: %i. %p"), RequestId.Load(), this);
			CurrentSonyState = ESonyRequestState::EFail;
			return true;
		case SCE_HTTP_ERROR_EAGAIN:
			// result not ready, try again next frame.
			return false;
		case SCE_OK:
			// success!  carry on to actual handling.
			break;
		default: 
			UE_LOG(LogHttp, Warning, TEXT("HttpGetResponseContentLength failed with error: 0x%x"), ReturnCode);
			CurrentSonyState = ESonyRequestState::EFail;
			return true;
	}

	LastResponseTime = FPlatformTime::Seconds();

	switch (Result)
	{
		case SCE_HTTP_CONTENTLEN_CHUNK_ENC:
			// not a failure case, response body can still be received by calling sceHttpReadData multiple times.
			UE_LOG(LogHttp, Log, TEXT("HttpGetResponseContentLength failed. The Content-Length could not be obtained since it was chunk encoded. %p"), this);
			break;
		case SCE_HTTP_CONTENTLEN_NOT_FOUND:
			CurrentSonyState = ESonyRequestState::ESuccess;
			Response->ResponseContentLength = 0;
			Response->bResponseSucceeded = true;
			Response->bIsReady = true;
			return true;

		case SCE_HTTP_CONTENTLEN_EXIST:			
			// success! carry on processing
			break;
		default:
			UE_LOG(LogHttp, Warning, TEXT("HttpGetResponseContentLength failed with Result error: 0x%x"), Result);
			CurrentSonyState = ESonyRequestState::EFail;
			return true;
	}	

	verify(ReturnContentLength<INT_MAX);	
	Response->ResponseContentLength = ReturnContentLength & 0xffffffff;
	CurrentSonyState = ESonyRequestState::ERecv;
	return true;
}

bool FSonyHTTPRequest::TickRecvState()
{
	bool bTickAgain = false;

	int32 BytesRead = 0;
	FSonyHTTPResponse::EPayloadStatus PayloadStatus = FSonyHTTPResponse::EPayloadStatus::ESuccess;
	// There is no payload for a HEAD request
	if (RequestVerb != TEXT("HEAD"))
	{
		PayloadStatus = Response->ReadPayload(BytesRead);
	}

	LastResponseTime = FPlatformTime::Seconds();

	switch (PayloadStatus)
	{
		case FSonyHTTPResponse::EPayloadStatus::EReadAgain:	
			break;	
		case FSonyHTTPResponse::EPayloadStatus::ESuccess:
			Response->ProcessResponse();
			check(Response->bIsReady);
			CurrentSonyState = ESonyRequestState::ESuccess;
			bTickAgain = true;
			break;
		case FSonyHTTPResponse::EPayloadStatus::EFail:
			CurrentSonyState = ESonyRequestState::EFail;
			bTickAgain = true;
		default:
			break;
	}
	
	return bTickAgain;
}


void FSonyHTTPRequest::Tick(float DeltaSeconds)
{
	CheckProgressDelegate();
}

void FSonyHTTPRequest::CheckProgressDelegate()
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

void FSonyHTTPRequest::TickThreadedRequest(float DeltaSeconds)
{
	CSV_SCOPED_TIMING_STAT(SonyHttp, TickThreadedRequest);

	int32 ReturnCode = 0;

	SceHttpNBEvent NetworkEvent;
	ReturnCode = sceHttpWaitRequest(PollingHandle, &NetworkEvent, 1, 1);
	if (ReturnCode > 0 && NetworkEvent.id == RequestId)
	{
		if (NetworkEvent.events & (SCE_HTTP_NB_EVENT_SOCK_ERR | SCE_HTTP_NB_EVENT_HUP | SCE_HTTP_NB_EVENT_RESOLVER_ERR))
		{
			UE_LOG(LogHttp, Warning, TEXT("HTTPRequest: 0x%x got network event error: 0x%x."), RequestId.Load(), NetworkEvent.events);
			CurrentSonyState = ESonyRequestState::EFail;			
		}
	}
	if (ReturnCode < 0 ) 
	{
		UE_LOG(LogHttp, Warning, TEXT("sceHttpWaitRequest error: 0x%x."), ReturnCode);
		CurrentSonyState = ESonyRequestState::EFail;		
	}		

	UE_LOG(LogHttp, Verbose, TEXT("HTTPRequest: 0x%x got network event msg: 0x%x."), RequestId.Load(), NetworkEvent.events);

	const float HttpTimeout = FHttpModule::Get().GetHttpTimeout();
	const double CurrentTime = FPlatformTime::Seconds();

	ElapsedTime = (float)(CurrentTime - StartRequestTime);

	float TimeSinceLastResponse = (float)(CurrentTime - LastResponseTime);

	if (HttpTimeout > 0.0f && TimeSinceLastResponse >= HttpTimeout)
	{
		UE_LOG(LogHttp, Warning, TEXT("Timeout processing Http request. %p"), this);
		CurrentSonyState = ESonyRequestState::EFail;
	}

	if (bCanceled)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpRequest canceled on request. %p"), this);
		CurrentSonyState = ESonyRequestState::EFail;
	}

	bool bTickAgain = false;
	switch (CurrentSonyState)
	{
		case ESonyRequestState::ESend:			
			bTickAgain = TickSendState();
			break;
		case ESonyRequestState::EGetStatus:
			bTickAgain = TickStatusState();
			break;
		case ESonyRequestState::EGetLen:
			bTickAgain = TickLenState();
			break;		
		case ESonyRequestState::ERecv:
			bTickAgain = TickRecvState();
			break;
		case ESonyRequestState::ESuccess:
			UE_LOG(LogHttp, Log, TEXT("HttpRequest Succeeded on Request: %p, RequestID: 0x%x, URL: %s."), this, RequestId.Load(), *GetURL());
			CleanupRequest();
			break;
		case ESonyRequestState::EFail:
		default:
			UE_LOG(LogHttp, Warning, TEXT("HttpRequest failed on Request: %p, RequestID: 0x%x, URL: %s."), this, RequestId.Load(), *GetURL());
			CleanupRequest();
			break;
	}

	// Recursive tick calls slightly less ugly than fallthrough case statements. Maximum 5 deep.
	if (bTickAgain)
	{
		TickThreadedRequest(DeltaSeconds);
	}
}

int32 FSonyHTTPRequest::SonyUriParameterName(const FString& ParameterName)
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

int32 FSonyHTTPRequest::SonyVerb() const
{

	//Convert Verb to Sony recognized value.
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

bool FSonyHTTPRequest::StartRequest()
{
	if (bCanceled)
	{
		UE_LOG(LogHttp, Warning, TEXT("StartRequest ignored because request has been canceled. %p %s url=%s"), this, *GetVerb(), *GetURL());
		return false;
	}

	// Make sure old handles are not being reused
	CleanupRequest();
	CurrentSonyState = ESonyRequestState::ESend;

	ConnectionId = FSonyHTTPConnection::Get().CreateConnectionId(GetURL(),TemplateId);

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

	const int32 SonyHttpVerb = SonyVerb();
	if (SonyHttpVerb != -1)
	{
		RequestId = sceHttpCreateRequestWithURL(ConnectionId, SonyVerb(), TCHAR_TO_ANSI(*GetURL()), RequestPayload->GetContentLength());
	}
	else
	{
		RequestId = sceHttpCreateRequestWithURL2(ConnectionId, TCHAR_TO_ANSI(*RequestVerb), TCHAR_TO_ANSI(*GetURL()), RequestPayload->GetContentLength());
	}
	
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
		UE_LOG(LogHttp, Warning, TEXT("HttpCreateRequestWithURL failed. The value specified in method is invalid: %i. Verb=[%s]"), SonyHttpVerb, *RequestVerb);
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
		UE_LOG(LogHttp, Warning, TEXT("sceHttpSetNonblock failed with request ID: %i, Error: 0x%x"), RequestId.Load(), ReturnCode);
		return false;
	}

	ReturnCode = sceHttpCreateEpoll(FSonyPlatformHttp::GetLibHttpCtxId(), &PollingHandle);
	if (ReturnCode != SCE_OK)
	{
		UE_LOG(LogHttp, Warning, TEXT("sceHttpCreateEpoll failed, Error: 0x%x"), ReturnCode);		
		return false;
	}

	ReturnCode = sceHttpSetEpoll(RequestId, PollingHandle, this);
	if (ReturnCode != SCE_OK)
	{
		UE_LOG(LogHttp, Warning, TEXT("sceHttpSetEpoll failed on RequestID: %i, Error: 0x%x"), RequestId.Load(), ReturnCode);		
		return false;
	}

	CurrentSonyState = ESonyRequestState::ESend;

	// Successfully started the request, sceHttpSendRequest() blocks until processing is completed. Specifically, the function returns after the HTTP request is sent and the response header is received from the server.
	return true;
}

void FSonyHTTPRequest::FinishedRequest()
{
	ElapsedTime = (float)(FPlatformTime::Seconds() - StartRequestTime);
	if (Response.IsValid() &&
		Response->bResponseSucceeded)
	{
		// Mark last request attempt as completed successfully
		CompletionStatus = EHttpRequestStatus::Succeeded;
		// TODO: Try to broadcast OnHeaderReceived when we receive headers instead of here at the end
		BroadcastResponseHeadersReceived();
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

void FSonyHTTPRequest::CleanupRequest()
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
			UE_LOG(LogHttp, Warning, TEXT("HttpDeleteRequest failed. The ID specified for the argument is invalid: %i"), RequestId.Load());
		}

		RequestId = -1;
	}
	
	if (PollingHandle != nullptr)
	{		
		sceHttpDestroyEpoll(FSonyPlatformHttp::GetLibHttpCtxId(), PollingHandle);
		PollingHandle = nullptr;
	}	

	if (ConnectionId != -1)
	{
		FSonyHTTPConnection::Get().DestroyConnectionId(ConnectionId);
		ConnectionId = -1;
	}
}

bool FSonyHTTPRequest::AddRequestHeaders()
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
			UE_LOG(LogHttp, Warning, TEXT("HttpAddRequestHeader failed. The ID specified for the argument is invalid: %i."), RequestId.Load());
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

float FSonyHTTPRequest::GetElapsedTime() const
{
	return ElapsedTime;
}

bool FSonyHTTPRequest::StartThreadedRequest()
{
	return StartRequest();
}

void FSonyHTTPRequest::FinishRequest()
{
	FinishedRequest();
}

bool FSonyHTTPRequest::IsThreadedRequestComplete()
{
	return CurrentSonyState == ESonyRequestState::ESuccess || CurrentSonyState == ESonyRequestState::EFail;
}

void FSonyHTTPRequest::AbortRequest()
{
	int32 LocalRequestId = RequestId;
	if (LocalRequestId > -1)
	{
		int32 ReturnCode = sceHttpAbortRequest(LocalRequestId);
		if (ReturnCode == SCE_HTTP_ERROR_BUSY)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpAbortRequest failed."));
		}
		else if (ReturnCode == SCE_HTTP_ERROR_INVALID_ID)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpAbortRequest failed. The ID specified for the argument is invalid: %i"), LocalRequestId);
		}
		else if (ReturnCode == SCE_HTTP_ERROR_INSUFFICIENT_STACKSIZE)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpAbortRequest failed. Insufficient stack size."));
		}
		else if (ReturnCode < 0)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpAbortRequest failed. Unknown error: %i RequestId: %i"), ReturnCode, LocalRequestId);
		}
		else
		{
			check(ReturnCode == SCE_OK);
		}
	}
}

// FSonyHTTPResponse

FSonyHTTPResponse::FSonyHTTPResponse(const FSonyHTTPRequest& InRequest)
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

FSonyHTTPResponse::~FSonyHTTPResponse()
{

}


FString FSonyHTTPResponse::GetURIPart(uint32 URIPartToGet) const
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

int32 FSonyHTTPResponse::SonyUriParameterName(const FString& ParameterName)
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

FString FSonyHTTPResponse::GetURL() const
{
	return GetURIPart(SCE_HTTP_URI_BUILD_WITH_ALL);
}


FString FSonyHTTPResponse::GetURLParameter(const FString& ParameterName) const
{
	return GetURIPart(SonyUriParameterName(ParameterName));
}

FString FSonyHTTPResponse::GetHeader(const FString& HeaderName) const
{
	FString Result(TEXT(""));
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached header [%s]. Response still processing. %p"),
			*HeaderName, &Request);
	}
	else
	{
		const FString* Header = ResponseHeaders.Find(HeaderName);
		if (Header != NULL)
		{
			return *Header;
		}
	}
	return Result;
}

TArray<FString> FSonyHTTPResponse::GetAllHeaders() const
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

FString FSonyHTTPResponse::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FSonyHTTPResponse::GetContentLength() const
{
	return ContentLength;
}

const TArray<uint8>& FSonyHTTPResponse::GetContent() const
{
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"),&Request);
	}
	return ResponsePayload;
}

int32 FSonyHTTPResponse::GetResponseCode() const
{
	return ResponseCode;
}

FString FSonyHTTPResponse::GetContentAsString() const
{
	TArray<uint8> ZeroTerminatedPayload(GetContent());
	ZeroTerminatedPayload.Add(0);
	return UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData());
}

void FSonyHTTPResponse::ProcessResponse()
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

void FSonyHTTPResponse::ProcessResponseHeaders()
{
	size_t HeaderSize = 0;
	char* ReturnPtr;


	int32 ReturnCode = sceHttpGetAllResponseHeaders(Request.RequestId, &ReturnPtr, &HeaderSize);
	if (ReturnCode == SCE_OK)
	{
		FString HeaderLine;
		FString RestOfHeader = ReturnPtr ? FString(ANSI_TO_TCHAR(ReturnPtr)) : FString();

		bool RowSplit = false;

		// parse all the key/value pairs
		// don't count the terminating NULL character as one to search.
		do
		{
			RowSplit = RestOfHeader.Split("\r\n", &HeaderLine, &RestOfHeader);

			FString HeaderKey, HeaderValue;

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
		} while (RowSplit);
	}
	else if (ReturnCode == SCE_HTTP_ERROR_BEFORE_INIT)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpGetAllResponseHeaders failed. The library is not initialized.  %p"), &Request);
	}
	else if (ReturnCode == SCE_HTTP_ERROR_BEFORE_SEND)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpGetAllResponseHeaders failed. The specified request has not been sent yet.  %p"), &Request);
	}
	else if (ReturnCode == SCE_HTTP_ERROR_INVALID_ID)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpGetAllResponseHeaders failed. The ID specified for the argument is invalid: %i.  %p"), Request.RequestId.Load(), &Request);
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpGetAllResponseHeaders failed. Unhandled ReturnCode: %i.  %p"), ReturnCode, &Request);
	}
}

FSonyHTTPResponse::EPayloadStatus FSonyHTTPResponse::ReadPayload(int32& BytesRead)
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
		UE_LOG(LogHttp, VeryVerbose, TEXT("TotalBytesRead: %i, StartAddr: %p, SizeRemaining: %i, Read %i bytes of Payload, RequestID: 0x%x"), LastTotalBytesRead, &ResponsePayload[LastTotalBytesRead], ResponsePayload.Num() - LastTotalBytesRead, ReturnValue, Request.RequestId.Load());

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
			UE_LOG(LogHttp, Warning, TEXT("HttpReadData failed. The ID specified for the argument is invalid: %i. %p"), Request.RequestId.Load(), &Request);
			return EPayloadStatus::EFail;
		}
		else if (ReturnValue == SCE_HTTP_ERROR_EAGAIN)
		{
			return EPayloadStatus::EReadAgain;
		}
		else if (ReturnValue == SCE_HTTP_ERROR_ABORTED)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpReadData failed. Request was aborted: %i"), Request.RequestId.Load());
			return EPayloadStatus::EFail;
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

int32 FSonyHTTPResponse::QueryContentLength() const
{
	return ResponseContentLength;
}
