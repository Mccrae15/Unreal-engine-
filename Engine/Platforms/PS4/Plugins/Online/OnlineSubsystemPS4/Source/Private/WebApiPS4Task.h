// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystemPS4Private.h"
#include "OnlineSubsystemPS4Types.h"
#include "Async/AsyncWork.h"
#include "OnlineError.h"
#include "WebApiPS4Types.h"

// Wrapper class for making a 'simple' WebApi call with (optionally) a Json payload in a task
class FWebApiPS4Task
	: public FNonAbandonableTask
{
public:
	/** Constructor */
	explicit FWebApiPS4Task(const FNpWebApiUserContext InLocalUserContext)
		:	LocalUserContext(InLocalUserContext)
		,	ApiGroup(ENpApiGroup::Invalid)
		,	Method(SCE_NP_WEBAPI_HTTP_METHOD_POST)
		,	bHasStartedWork(false)
		,	Result(true)
		,	NumericErrorCode(0)
		,	TimeoutInSeconds(-1.0f)
	{
	}

	/** Move Constructor */
	FWebApiPS4Task(FWebApiPS4Task&& Other) = default;
	/** Move Assignment */
	FWebApiPS4Task& operator=(FWebApiPS4Task&& Other) = default;

	/** Disallow Copying */
	FWebApiPS4Task(const FWebApiPS4Task& Other) = delete;
	FWebApiPS4Task& operator=(const FWebApiPS4Task& Other) = delete;

	/** Performs work on thread */
	void DoWork();

	/** Returns the stat id for this task */
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT( FWebApiPS4Task, STATGROUP_ThreadPoolAsyncTasks );
	}

public:
	/** Set the API group, path, and method for the WebApi request. This method is necessary before the request is made in DoWork */
	void SetRequest(const ENpApiGroup InApiGroup, const FString& InPath, const SceNpWebApiHttpMethod InMethod);
	void SetRequest(const ENpApiGroup InApiGroup, FString&& InPath, const SceNpWebApiHttpMethod InMethod);

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

	/** Get the entire error result */
	const FOnlineError& GetErrorResult() const;

	/** Get the Http status code */
	int32 GetHttpStatusCode() const;

	/** Was the Http request successful? */
	bool WasSuccessful() const;

private:

	/** The context of the local user for the WebApi interface */
	FNpWebApiUserContext LocalUserContext;

	/** API group we are making requests on */
	ENpApiGroup ApiGroup;

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

	/** Result status information */
	FOnlineError Result;
	
	/** Extended information about error */
	int32 NumericErrorCode;

	/** Request timeout in seconds */
	float TimeoutInSeconds;
};
