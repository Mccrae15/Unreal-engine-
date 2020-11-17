// Copyright Epic Games, Inc. All Rights Reserved.

#include "SonyPlatformHttp.h"
#include "SonyHttp.h"

//The Sony Net library
#include <net.h>

//The Sony SSL libray
#include <libssl.h>

#include <libhttp.h>

//size of the memory pool to be used for the Http library

#define LIBSSL_POOLSIZE		((1024 + 512) * 1024)
#define LIBNET_POOLSIZE		(16 * 1024)
#define LIBHTTP_POOLSIZE	(1024 * 1024)
#define USER_AGENT			"-UE4/0.1"

//Link to the ps4 doc for easy reference https://ps4.scedev.net/docs/ps4-en,Http-Overview-orbis,Using_the_Library/1/

/*Net library initialization id*/
int FSonyPlatformHttp::LibNetId = -1;

/*SSL library initialization id*/
int FSonyPlatformHttp::LibSSLCtxId = -1;

/*Http library initialization id*/
int FSonyPlatformHttp::LibHttpCtxId = -1;

/* Template Id for the connection */
int FSonyPlatformHttp::TemplateId = -1;

void FSonyPlatformHttp::Init()
{
	// Attempt to open an Internet connection

	UE_LOG(LogHttp, Log, TEXT("Initializing Sony Http settings"));

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

void FSonyPlatformHttp::Shutdown()
{
	UE_LOG(LogHttp, Log, TEXT("Closing Sony Http settings"));

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
}


IHttpRequest* FSonyPlatformHttp::ConstructRequest()
{
	return new FSonyHTTPRequest(TemplateId);
}

bool FSonyPlatformHttp::UsesThreadedHttp()
{
	return true;
}
