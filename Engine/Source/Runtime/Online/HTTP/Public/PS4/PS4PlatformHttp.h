// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformHttp.h"

class IHttpRequest;

/**
 * Platform specific Http implementations
 */
class FPS4PlatformHttp
	: public FGenericPlatformHttp
{
private:

	/** Net library initialization id. */
	static int LibNetId;

	/** SSL library initialization id. */
	static int LibSSLCtxId;

	/** Http library initialization id. */
	static int LibHttpCtxId;

	/** Template Id for the connection. */
	static int TemplateId;

public:

	/**
	 * Platform initialization step.
	 *
	 * @see Shutdown
	 */
	static void Init();

	/**
	 * Platform shutdown step.
	 *
	 * @see Init
	 */
	static void Shutdown();

	/**
	 * Creates a new Http request instance for the current platform
	 *
	 * @return request object
	 */
	static IHttpRequest* ConstructRequest();

	/**
	 * Gets the HTTP context identifier.
	 *
	 * @return The context ID.
	 * @see GetLibSslCtxId
	 */
	static int32 GetLibHttpCtxId()
	{
		return LibHttpCtxId;
	}

	/**
	 * Gets the SSL context identifier.
	 *
	 * @return The context ID.
	 * @see GetLibHttpCtxId
	 */
	static int32 GetLibSslCtxId()
	{
		return LibHttpCtxId;
	}
};


typedef FPS4PlatformHttp FPlatformHttp;
