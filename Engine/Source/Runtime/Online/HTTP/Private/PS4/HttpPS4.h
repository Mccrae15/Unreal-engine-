// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../IHttpThreadedRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Containers/Map.h"
#include "HAL/ThreadSafeCounter.h"
#include "Http.h"

//The PS4 Net library
#include <net.h>

//The PS4 SSL libray
#include <libssl.h>

#include <libhttp.h>


/**
 * PS4 implementation of an Http request
 */
class FPS4HTTPRequest : public IHttpThreadedRequest
{
public:

	enum class EPS4RequestState
	{
		ESend,		
		EGetStatus,
		EGetLen,
		ERecv,
		EFail,
		ESuccess,
	};

	// IHttpBase

	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;	
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;

	// IHttpRequest

	virtual FString GetVerb() override;
	virtual void SetVerb(const FString& Verb) override;
	virtual void SetURL(const FString& URL) override;
	virtual void SetContent(const TArray<uint8>& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual FHttpRequestCompleteDelegate& OnProcessRequestComplete() override;
	virtual FHttpRequestProgressDelegate& OnRequestProgress() override;
	virtual void CancelRequest() override;
	virtual EHttpRequestStatus::Type GetStatus() override;
	virtual const FHttpResponsePtr GetResponse() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() override;

	// IHttpRequestThreaded
	virtual bool StartThreadedRequest() override;
	virtual bool IsThreadedRequestComplete() override;
	virtual void TickThreadedRequest(float DeltaSeconds) override;
	virtual void FinishRequest() override;

	// FPS4HTTPRequest

	/**
	 * Constructor
	 */
	FPS4HTTPRequest(int PlatformHTTPTemplateId);

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FPS4HTTPRequest();
		

private:	

	/**
	 * Create the session connection and initiate the web request
	 *
	 * @return true if the request was started
	 */
	bool StartRequest();	
	
	/**
	 * Process state for a finished request that no longer needs to be ticked
	 * Calls the completion delegate
	 */
	void FinishedRequest();

	/**
	 * Close session/request handles and unregister callbacks
	 */
	void CleanupRequest();	

	/**
	 * Trigger the request progress delegate if progress has changed
	 */
	void CheckProgressDelegate();

	/**
	 * Get a PS4 Parameter value from passed in ParameterName.
	 *
	 * @Param ParameterName passed in string to convert to PS4 parameter values
	 * @return the value of the verb converted to a representation understood by the PS4 libs
	 */
	int32 PS4UriParameterName(const FString& ParameterName);

	/**
	 * Get a PS4 lib understood Verb value.
	 *
	 * @return the value of the verb converted to a representation understood by the PS4 libs
	 */
	int32 PS4Verb();

	/**
	 * Get string of part of the RequestURL SceHttpUriElement object.
	 *
	 * @param the value of the URI Part to retrieve
	 * @return the string of the part asked to retrieve
	 */
	FString GetURIPart(uint32 URIPartToGet);

	/**
	 * Add the headers key/value pairs to the request
	 *
	 * @return true if all headers added without error, otherwise false.
	 */
	bool AddRequestHeaders();	

	/**
	 * Tick for the send state.  Sending may need to retry.
	 *
	 * @return Whether to tick again immediately or not.
	 */
	bool TickSendState();

	/**
	 * Tick for the query status state.
	 *
	 * @return Whether to tick again immediately or not.
	 */
	bool TickStatusState();

	/**
	 * Tick for the query response length state.
	 *
	 * @return Whether to tick again immediately or not.
	 */
	bool TickLenState();

	/**
	 * Tick for the response reading state.
	 *
	 * @return Whether to tick again immediately or not.
	 */
	bool TickRecvState();

	// implementation friends
	friend class FPS4HTTPResponse;
private:
	/** TemplateId for HTTP requests passed in from PSPlatformHttp */
	int32 TemplateId;

	/* Holds the request identifier */
	int32 RequestId;

	/* Holds the connection Id.  One per request so we don't have to worry about serializing requests on the same connection id */
	int32 ConnectionId;

	/** Whether the request has actually been sent to the server (at which point it is no longer safe to retry) */
	bool bRequestSent;

	/* Hold the current state for async requests */
	EPS4RequestState CurrentPS4State;

	/** URL address to connect request to */
	SceHttpUriElement RequestURL;

	/** Buffer that holds the string that members of the above struct point to */
	TArray<uint8> RequestURLBuffer;

	/** Verb for making request (GET,POST,etc) */
	FString RequestVerb;

	/** Mapping of header section to values. Used to generate final header string for request */
	TMap<FString, FString> RequestHeaders;

	/** BYTE array payload to use with the request. Typically for a POST */
	TArray<uint8> RequestPayload;

	/** Current status of request being processed */
	EHttpRequestStatus::Type CompletionStatus;

	/** Polling handle for non-blocking requests */
	SceHttpEpollHandle PollingHandle;

	/** Holds response data that comes back from a successful request.  NULL if request can't connect */
	TSharedPtr<class FPS4HTTPResponse,ESPMode::ThreadSafe> Response;

	/** Delegate that will get called once request completes or on any error */
	FHttpRequestCompleteDelegate RequestCompleteDelegate;

	/** Delegate that will get called once per tick with bytes downloaded so far */
	FHttpRequestProgressDelegate RequestProgressDelegate;

	/** Start of the request */
	double StartRequestTime;

	/** Time taken to complete/cancel the request. */
	float ElapsedTime;

	/** Last bytes read reported to progress delegate */
	int32 LastReportedBytesRead;
};

/**
 * PS4 implementation of an Http response
 */
class FPS4HTTPResponse : public IHttpResponse
{

public:	

	// IHttpBase

	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;	
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;

	// IHttpResponse

	virtual int32 GetResponseCode() override;
	virtual FString GetContentAsString() override;

	// FPS4HTTPResponse

	/**
	 * Constructor
	 *
	 * @param InRequest - original request that created this response
	 */
	FPS4HTTPResponse(const FPS4HTTPRequest& InRequest);

	/**
	 * Destructor
	 */
	virtual ~FPS4HTTPResponse();	

	/**
	 * Get a PS4 Parameter value from passed in ParameterName.
	 *
	 * @Param ParameterName passed in string to convert to PS4 parameter values
	 * @return the value of the verb converted to a representation understood by the PS4 libs
	 */
	int32 PS4UriParameterName(const FString& ParameterName);	

	/**
	 * Get string of part of the RequestURL SceHttpUriElement object.
	 *
	 * @param the value of the URI Part to retrieve
	 * @return the string of the part asked to retrieve
	 */
	FString GetURIPart(uint32 URIPartToGet);

	// implementation friends
	friend class FPS4HTTPRequest;
private:

	enum class EPayloadStatus
	{
		EReadAgain,
		ESuccess,
		EFail
	};
	
	/**
	 * Process response that has been received. Copy content to payload buffer via async reads
	 */
	void ProcessResponse();

	/**
	 * Query header info from the response and cache the results to ResponseHeaders
	 */
	void ProcessResponseHeaders();

	/**
	 * Attempts to read the payload of the response
	 *
	 * @param BytesRead total bytes read during this call
	 *
	 * @return Status of the read.
	 */
	EPayloadStatus ReadPayload(int32& BytesRead);

	/**
	 * Get the content length
	 *
	 * @return content length as an int32.  ContentLength will be 0 if response is chunked and reads are incomplete.
	 */
	int32 QueryContentLength() const;

	/** Request that owns this response */
	const FPS4HTTPRequest& Request;	

	/** Original URL used for the request */
	SceHttpUriElement RequestURL;

	/** Last bytes read from async call to sceHttpReadData */
	int32 AsyncBytesRead;

	/** Caches how many bytes of the response we've read so far */
	FThreadSafeCounter TotalBytesRead;

	/** Cached key/value header pairs. Parsed from HttpQueryInfo results once request completes */
	TMap<FString, FString> ResponseHeaders;

	/** cached value returned from sceHttpGetResponseContentLength */
	int32 ResponseContentLength;

	/** Cached code from completed response */
	int32 ResponseCode;

	/** Cached content length from completed response */
	int32 ContentLength;

	/** BYTE array to fill in as the response is read via sceHttpReadData */
	TArray<uint8> ResponsePayload;

	/** True when the response has finished async processing */
	bool bIsReady;
	/** True if the response was successfully received/processed */
	bool bResponseSucceeded;
};

