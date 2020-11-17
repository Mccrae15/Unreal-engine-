// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IHttpThreadedRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Containers/Map.h"
#include "HAL/ThreadSafeCounter.h"
#include "Http.h"
#include "GenericPlatform/HttpRequestPayload.h"

//The Sony Net library
#include <net.h>

//The Sony SSL libray
#include <libssl.h>

#include <libhttp.h>


/**
 * Sony implementation of an Http request
 */
class FSonyHTTPRequest : public IHttpThreadedRequest
{
public:

	enum class ESonyRequestState
	{
		ESend,		
		EGetStatus,
		EGetLen,
		ERecv,
		EFail,
		ESuccess,
	};

	// IHttpBase

	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual int32 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;

	// IHttpRequest

	virtual FString GetVerb() const override;
	virtual void SetVerb(const FString& Verb) override;
	virtual void SetURL(const FString& URL) override;
	virtual void SetContent(const TArray<uint8>& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
	virtual bool SetContentAsStreamedFile(const FString& Filename) override;
	virtual bool SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual void CancelRequest() override;
	virtual EHttpRequestStatus::Type GetStatus() const override;
	virtual const FHttpResponsePtr GetResponse() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() const override;

	// IHttpRequestThreaded
	virtual bool StartThreadedRequest() override;
	virtual bool IsThreadedRequestComplete() override;
	virtual void TickThreadedRequest(float DeltaSeconds) override;
	virtual void FinishRequest() override;

	// FSonyHTTPRequest

	/**
	 * Constructor
	 */
	FSonyHTTPRequest(int PlatformHTTPTemplateId);

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FSonyHTTPRequest();
		

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
	 * Get a Sony Parameter value from passed in ParameterName.
	 *
	 * @Param ParameterName passed in string to convert to Sony parameter values
	 * @return the value of the verb converted to a representation understood by the Sony libs
	 */
	static int32 SonyUriParameterName(const FString& ParameterName);

	/**
	 * Get a Sony lib understood Verb value.
	 *
	 * @return the value of the verb converted to a representation understood by the Sony libs
	 */
	int32 SonyVerb() const;

	/**
	 * Get string of part of the RequestURL SceHttpUriElement object.
	 *
	 * @param the value of the URI Part to retrieve
	 * @return the string of the part asked to retrieve
	 */
	FString GetURIPart(uint32 URIPartToGet) const;

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

	/**
	* Abort any in flight request using RequestId, callable from game thread
	*/
	void AbortRequest();

	// implementation friends
	friend class FSonyHTTPResponse;
private:
	/** TemplateId for HTTP requests passed in from PSPlatformHttp */
	int32 TemplateId;

	/* Holds the request identifier */
	TAtomic<int32> RequestId;

	/* Holds the connection Id.  One per request so we don't have to worry about serializing requests on the same connection id */
	int32 ConnectionId;

	/** Whether the request has actually been sent to the server (at which point it is no longer safe to retry) */
	bool bRequestSent;

	/* Hold the current state for async requests */
	ESonyRequestState CurrentSonyState;

	/** URL address to connect request to */
	SceHttpUriElement RequestURL;

	/** Buffer that holds the string that members of the above struct point to */
	TArray<uint8> RequestURLBuffer;

	/** Verb for making request (GET,POST,etc) */
	FString RequestVerb;

	/** Mapping of header section to values. Used to generate final header string for request */
	TMap<FString, FString> RequestHeaders;

	/** RequestPayload to use with the request. Typically for a POST */
	TUniquePtr<FRequestPayload> RequestPayload;

	/** Maximum Buffer size for sending data in a single request*/
	static const int32 MaxSendBufferLength = 16 * 1024;

	/** Buffer used for sending data in the request. Typically for a POST */
	uint8 SendBuffer[MaxSendBufferLength];

	/** Number of bytes successfully sent **/
	int64 BytesSent;

	/** Current status of request being processed */
	EHttpRequestStatus::Type CompletionStatus;

	/** Polling handle for non-blocking requests */
	SceHttpEpollHandle PollingHandle;

	/** Holds response data that comes back from a successful request.  NULL if request can't connect */
	TSharedPtr<class FSonyHTTPResponse,ESPMode::ThreadSafe> Response;

	/** Start of the request */
	double StartRequestTime;

	/** Last time we received data */
	double LastResponseTime;

	/** Time taken to complete/cancel the request. */
	float ElapsedTime;

	/** Last bytes read reported to progress delegate */
	int32 LastReportedBytesRead;

	/** Set when CancelRequest() is called to signal thread to stop */
	bool bCanceled;
};

/**
 * Sony implementation of an Http response
 */
class FSonyHTTPResponse : public IHttpResponse
{

public:	

	// IHttpBase

	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual int32 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;

	// IHttpResponse

	virtual int32 GetResponseCode() const override;
	virtual FString GetContentAsString() const override;

	// FSonyHTTPResponse

	/**
	 * Constructor
	 *
	 * @param InRequest - original request that created this response
	 */
	FSonyHTTPResponse(const FSonyHTTPRequest& InRequest);

	/**
	 * Destructor
	 */
	virtual ~FSonyHTTPResponse();	

	/**
	 * Get a Sony Parameter value from passed in ParameterName.
	 *
	 * @Param ParameterName passed in string to convert to Sony parameter values
	 * @return the value of the verb converted to a representation understood by the Sony libs
	 */
	static int32 SonyUriParameterName(const FString& ParameterName);	

	/**
	 * Get string of part of the RequestURL SceHttpUriElement object.
	 *
	 * @param the value of the URI Part to retrieve
	 * @return the string of the part asked to retrieve
	 */
	FString GetURIPart(uint32 URIPartToGet) const;

	// implementation friends
	friend class FSonyHTTPRequest;
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
	const FSonyHTTPRequest& Request;	

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

