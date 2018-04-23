// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4PlatformHttp.h"
#include "HttpPS4.h"

//The PS4 Net library
#include <net.h>

//The PS4 SSL libray
#include <libssl.h>

#include <libhttp.h>

//size of the memory pool to be used for the Http library

#define LIBSSL_POOLSIZE		(1024 * 1024)
#define LIBNET_POOLSIZE		(16 * 1024)
#define LIBHTTP_POOLSIZE	(256 * 1024)
#define USER_AGENT			"-UE4/0.1"

//Link to the ps4 doc for easy reference https://ps4.scedev.net/docs/ps4-en,Http-Overview-orbis,Using_the_Library/1/

/*Net library initialization id*/
int FPS4PlatformHttp::LibNetId = -1;

/*SSL library initialization id*/
int FPS4PlatformHttp::LibSSLCtxId = -1;

/*Http library initialization id*/
int FPS4PlatformHttp::LibHttpCtxId = -1;

/* Template Id for the connection */
int FPS4PlatformHttp::TemplateId = -1;

void FPS4PlatformHttp::Init()
{
	// Attempt to open an Internet connection

	UE_LOG(LogHttp, Log, TEXT("Initializing PS4 Http settings"));

	/*Net library initialization*/
	int RetValue = sceNetInit();
	// From PS4 Dev Net Returns a negative value for errors. Although there is no specific situation in which this function is presumed to return an error, the application must not malfunction even if an error is returned
	verify(RetValue>-1);

	LibNetId = sceNetPoolCreate("simple", LIBNET_POOLSIZE, 0);
	switch(LibNetId)
	{
		case SCE_NET_ERROR_EINVAL:
			UE_LOG(LogHttp, Warning, TEXT("NetPoolCreate failed, Function called with an invalid argument or content"));
			break;
		case SCE_NET_ERROR_ENOALLOCMEM:
			UE_LOG(LogHttp, Warning, TEXT("NetPoolCreate failed, Memory could not be allocated"));
			break;
		case SCE_NET_ERROR_ENAMETOOLONG:
			UE_LOG(LogHttp, Warning, TEXT("NetPoolCreate failed, The debug name is too long"));
			break;
		default:
			break;
	}

	/*SSL library initialization id*/
	LibSSLCtxId = sceSslInit(LIBSSL_POOLSIZE);
	switch (LibSSLCtxId)
	{
	case SCE_SSL_ERROR_ALREADY_INITED:
		UE_LOG(LogHttp, Warning, TEXT("SslInit failed, Library has already been initialized"));
		break;
	case SCE_SSL_ERROR_OUT_OF_MEMORY:
		UE_LOG(LogHttp, Warning, TEXT("SslInit failed, Insufficient free memory space"));
		break;
	}


	/*Http library initialization*/
	LibHttpCtxId = sceHttpInit(LibNetId, LibSSLCtxId, LIBHTTP_POOLSIZE);
	switch(LibHttpCtxId)
	{
		case SCE_HTTP_ERROR_ALREADY_INITED:
			UE_LOG(LogHttp, Warning, TEXT("HttpInit failed, sceHttpInit() was called a second time without calling sceHttpTerm()"));
			break;
		case SCE_HTTP_ERROR_OUT_OF_MEMORY:
			UE_LOG(LogHttp, Warning, TEXT("HttpInit failed, Insufficient free memory space"));
			break;
		default:
			break;
	}


	//@todo Temp USER_AGENT, what are we supposed to use here?
	/* Create template settings */
	TemplateId = sceHttpCreateTemplate(LibHttpCtxId, USER_AGENT, SCE_HTTP_VERSION_1_1, SCE_TRUE);
	switch(TemplateId)
	{
		case SCE_HTTP_ERROR_BEFORE_INIT:
			UE_LOG(LogHttp, Warning, TEXT("HttpCreateTemplate failed, The library is not initialized"));
			break;
		case SCE_HTTP_ERROR_OUT_OF_MEMORY:
			UE_LOG(LogHttp, Warning, TEXT("HttpCreateTemplate failed, Insufficient free memory space"));
			break;
		case SCE_HTTP_ERROR_INVALID_VERSION:
			UE_LOG(LogHttp, Warning, TEXT("HttpCreateTemplate failed, The HTTP version is invalid"));
			break;
		case SCE_HTTP_ERROR_INVALID_ID:
			UE_LOG(LogHttp, Warning, TEXT("HttpCreateTemplate failed, Invalid Http library context ID"));
			break;
		case SCE_HTTP_ERROR_INVALID_VALUE:
			UE_LOG(LogHttp, Warning, TEXT("HttpCreateTemplate failed, An invalid value was specified for an argument"));
			break;
		default:
			break;
	}
}

void FPS4PlatformHttp::Shutdown()
{
	UE_LOG(LogHttp, Log, TEXT("Closing PS4 Http settings"));

	if (TemplateId > -1)
	{
		int ReturnCode = sceHttpDeleteTemplate(TemplateId);
		switch (ReturnCode)
		{
			case SCE_HTTP_ERROR_BEFORE_INIT:
				UE_LOG(LogHttp, Warning, TEXT("HttpDeleteTemplate failed, The library is not initialized"));
				break;
			case SCE_HTTP_ERROR_INVALID_ID:
				UE_LOG(LogHttp, Warning, TEXT("HttpDeleteTemplate failed, The TemplateId specified for the argument is invalid: %i"),TemplateId);
				break;
			default:
				break;
		}
		TemplateId = -1;
	}

	/* libnet */
	if (LibNetId > -1)
	{
		int ReturnCode = sceNetPoolDestroy(LibNetId);
		switch (ReturnCode)
		{
			case SCE_NET_ERROR_EBADF:
				UE_LOG(LogHttp, Warning, TEXT("NetPoolDestroy failed, Invalid Net library memory ID was specified: %i"),LibNetId);
				break;
			case SCE_NET_ERROR_ENOTEMPTY:
				UE_LOG(LogHttp, Warning, TEXT("NetPoolDestroy failed, Memory is being used"));
				break;
			default:
				break;
		}
		LibNetId = -1;
	}

	/* Http library termination processing */
	if (LibHttpCtxId > -1)
	{
		int ReturnCode = sceHttpTerm(LibHttpCtxId);
		switch (ReturnCode)
		{
			case SCE_HTTP_ERROR_BEFORE_INIT:
				UE_LOG(LogHttp, Warning, TEXT("HttpTerm failed, Before library initialization"));
				break;
			case SCE_NET_ERROR_ENOTEMPTY:
				UE_LOG(LogHttp, Warning, TEXT("HttpTerm failed, Invalid Http library context ID: %i"),LibHttpCtxId);
				break;
			default:
				break;
		}
		LibHttpCtxId = -1;
	}

	if (LibSSLCtxId > -1)
	{
		int ReturnCode = sceSslTerm(LibSSLCtxId);
		switch (ReturnCode)
		{
			case SCE_HTTP_ERROR_BEFORE_INIT:
				UE_LOG(LogHttp, Warning, TEXT("SslTerm failed, Library not initialized"));
				break;
			case SCE_NET_ERROR_ENOTEMPTY:
				UE_LOG(LogHttp, Warning, TEXT("SslTerm failed, Ssl library context ID is invalid: %i"),LibHttpCtxId);
				break;
			default:
				break;
		}
		LibSSLCtxId = -1;
	}

	int ReturnCode = sceNetTerm();
	//Not checking return code, there is only one which is that is was not initialised, which will occur due to calling shutdown at the start of the initialise.
}


IHttpRequest* FPS4PlatformHttp::ConstructRequest()
{
	return new FPS4HTTPRequest(TemplateId);
}

