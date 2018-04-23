// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "AsyncWork.h"

// Wrapper class for making a 'simple' WebApi call with (optionally) a Json payload in a task
class FWebApiPS4Task : public FNonAbandonableTask
{
public:

	/** Constructor */
	FWebApiPS4Task(int32 InLocalUserContext)
		:	LocalUserContext(InLocalUserContext)
		,	ApiGroup()
		,	Path()
		,	Method(SCE_NP_WEBAPI_HTTP_METHOD_POST)
		,	Headers()
		,	RequestBody()
		,	ResponseBody()
		,	bHasStartedWork(false)
		,	ErrorStr()
		,	HttpStatusCode(0)
		,   TimeoutInSeconds(-1.0f)
	{
	}

	/** Performs work on thread */
	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FWebApiPS4Task, STATGROUP_ThreadPoolAsyncTasks );
	}

public:

	/** Set the API group, path, and method for the WebApi request. This method is necessary before the request is made in DoWork */
	void SetRequest(const FString& InApiGroup, const FString& InPath, SceNpWebApiHttpMethod InMethod);

	/** Set the body (if needed) to send with the web requests */
	void SetRequestBody(const FString& InBody);

	/** Set or override any headers in the web request */
	void AddRequestHeader(const FString& InHeader, const FString& InValue);

	/** Set the content type of the request. If set to an empty string, a default will be used */
	void SetContentType(const FString& InContentType);

	/** Set the timeout length of the request in seconds */
	void SetTimeout(const float InTimeoutInSeconds);

	/** Get the response */
	const FString& GetResponseBody() const;

	/** Get the error string */
	const FString& GetErrorString() const;

	/** Get the Http status code */
	int GetHttpStatusCode() const;

private:

	/** The context of the local user for the WebApi interface */
	int32 LocalUserContext;

	/** API group we are making requests on */
	FString ApiGroup;

	/** The path of the WebApi to execute */
	FString Path;

	/** Which Http method to use in our request */
	SceNpWebApiHttpMethod Method;

	/** The header mapping */
	TMap<FString, FString> Headers;

	/** The content type of the request */
	FString ContentType;

	/** Body to send with our request */
	FString RequestBody;

	/** Reponse body from the request */
	FString ResponseBody;

	/** Have we started the work? */
	bool bHasStartedWork;

	/** Error tracking */
	FString ErrorStr;

	/** Http status code */
	int HttpStatusCode;

	/** Request timeout in seconds */
	float TimeoutInSeconds;
};






