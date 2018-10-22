// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PS4CompanionServer.h"
#include "GameDelegates.h"

#include <libsysmodule.h>
#include <app_content.h>

// settings (pulled from sample)
const int32 ServerPort = 13000;
const int32 ServerOrientation = SCE_COMPANION_HTTPD_ORIENTATION_PORTRAIT;
const int32 ServerThreadCount = 4;

FPS4CompanionServer::FPS4CompanionServer()
#if PS4_FEATURE_COMPANION_APP
	: NumConnections(0)
#endif
{
#if PS4_FEATURE_COMPANION_APP
	StartServer();
#endif
}


#if PS4_FEATURE_COMPANION_APP
static void DefaultServerDelegate(int32 UserIndex, const FString& Action, const FString& URL, const TMap<FString, FString>& Params, TMap<FString, FString>& Response)
{
/*
	UE_LOG(LogPS4, Log, TEXT("WebRequest: User %d"), UserIndex);
	UE_LOG(LogPS4, Log, TEXT("          : Action '%s'"), *Action);
	UE_LOG(LogPS4, Log, TEXT("          : URL '%s'"), *URL);
	UE_LOG(LogPS4, Log, TEXT("          : Params:"));
	for (auto It = Params.CreateConstIterator(); It; ++It)
	{
		UE_LOG(LogPS4, Log, TEXT("  %s = '%s'"), *It.Key(), *It.Value());
	}
*/

	// you shouldn't normally use this method to get a UWorld as it won't always be correct in a PIE context.
	// However, the PS4 companion app server will never run in the Editor.
	UGameEngine* GameEngine = CastChecked<UGameEngine>(GEngine);
	if (GameEngine)
	{
		UWorld* World = GameEngine->GetGameWorld();
		if (World)
		{		
			// handle returning the main page query
 			if (URL == TEXT("/index.html?stats"))
			{
				Response.Add(TEXT("Content-Type"), TEXT("text/html; charset=utf-8"));
				extern float GAverageFPS;
				FString JSONResponse = FString::Printf(TEXT("{\"gamename\":\"%s\",\"map\":\"%s\",\"stats\":\"FPS: %.2f\"}"), FApp::GetProjectName(),
					*FPackageName::GetShortName(World->PersistentLevel->GetOutermost()->GetName()), GAverageFPS);
				Response.Add(TEXT("Body"), JSONResponse);
			}

			// send a console command
			if (URL == TEXT("/console.cgi"))
			{
				// @todo: DeferredCommands NEEDS to be thread safe
				GEngine->DeferredCommands.Add(Params.FindRef(TEXT("Body")));
			}
		}
	}
}

bool FPS4CompanionServer::StartServer()
{
	const FString& TempDirectory = FPS4PlatformFile::GetTempDirectory();
	if( TempDirectory.IsEmpty() )
	{
		return false;
	}

	// load the HTTPD module
	sceSysmoduleLoadModule(SCE_SYSMODULE_COMPANION_HTTPD);

	// start with defaults
	SceCompanionHttpdOptParam Param;
	sceCompanionHttpdOptParamInitialize(&Param);

	// allocate memory for the server (this leaks, since we never tear down the server)
	void* ServerMemory = FMemory::Malloc(SCE_COMPANION_HTTPD_DATALENGTH_MIN_WORK_HEAPMEMORY_SIZE);
	check(ServerMemory != NULL);
	Param.workMemory = ServerMemory;
	Param.workMemorySize = SCE_COMPANION_HTTPD_DATALENGTH_MIN_WORK_HEAPMEMORY_SIZE;
	// use defaults or values from sample to start with
	Param.workThreadStackSize = SCE_COMPANION_HTTPD_DATALENGTH_MIN_WORK_STACKMEMORY_SIZE;
	Param.port = ServerPort;
	Param.screenOrientation = ServerOrientation;
	Param.transceiverThreadCount= ServerThreadCount;
	Param.transceiverStackSize		= 128 * 1024;
	Param.workDirectory = *TempDirectory;

	// initialize the server
	Ret = sceCompanionHttpdInitialize(&Param);
	if (Ret != SCE_OK)
	{
		return false;
	}

	// look up the location for disk files
	WebServerRootDir = GConfig->GetStr(TEXT("PS4CompanionServer"), TEXT("WebServerRootDir"), GEngineIni);

	// register a callback function for HTTP requests
	Ret = sceCompanionHttpdRegisterRequestCallback(RequestCallback, this);
	if (Ret != SCE_OK)
	{
		return false;
	}

	// if there's no delegate already bound, bind the default one now (game can overwrite this after the fact as well)
	if (!FGameDelegates::Get().GetWebServerActionDelegate().IsBound())
	{
		FGameDelegates::Get().GetWebServerActionDelegate() = FWebServerActionDelegate::CreateStatic(DefaultServerDelegate);
	}

	return true;
}

int32 FPS4CompanionServer::RequestCallback(SceUserServiceUserId UserId, const SceCompanionHttpdRequest* HttpRequest,
										  SceCompanionHttpdResponse* HttpResponse, void* UserData)
{
	FPS4CompanionServer* Server = (FPS4CompanionServer*)UserData;

	FWebServerActionDelegate& Delegate = FGameDelegates::Get().GetWebServerActionDelegate();

	// figure out the action form the request
	FString Verb = 
		HttpRequest->method == SCE_COMPANION_HTTPD_METHOD_POST ? TEXT("POST") :
		HttpRequest->method == SCE_COMPANION_HTTPD_METHOD_GET ? TEXT("GET") :
		TEXT("UNKNOWN");

	// get all the header bits
	TMap<FString, FString> Params;

	// first get and zero terminate the body
	ANSICHAR* ZeroTermBody = (ANSICHAR*)FMemory::Malloc(HttpRequest->bodySize + 1);
	FMemory::Memcpy(ZeroTermBody, HttpRequest->body, HttpRequest->bodySize);
	ZeroTermBody[HttpRequest->bodySize] = 0;

	// add it as a special field
	Params.Add(TEXT("Body"), ZeroTermBody);
	FMemory::Free(ZeroTermBody);

	for (SceCompanionHttpdHeader* Header = HttpRequest->header; Header != NULL; Header = Header->header)
	{
		Params.Add(Header->key, Header->value);
	}

	// let the game handle the request
	TMap<FString, FString> Response;
	// @todo: The UserId is possibly differnet than the UserIndex (see PS4InputInterface.cpp)
	Delegate.ExecuteIfBound(UserId, Verb, HttpRequest->url, Params, Response);

	// if the game returned a response, then return it
	if (Response.Num() > 0)
	{
		for (TMap<FString, FString>::TIterator It(Response); It; ++It)
		{
			if (It.Key() == TEXT("Body"))
			{
				auto UTF8 = StringCast<ANSICHAR>(*It.Value());
				sceCompanionHttpdSetBody(UTF8.Get(), (size_t)UTF8.Length(), HttpResponse);
			}
			else
			{
				sceCompanionHttpdAddHeader(TCHAR_TO_UTF8(*It.Key()), TCHAR_TO_UTF8(*It.Value()), HttpResponse);
			}
		}
		return SCE_OK;
	}

	// if the delegate didn't respond, then attempt to look on disk for the filename
	if (Server->WebServerRootDir.Len() > 0)
	{
		FString Contents;
		if (FFileHelper::LoadFileToString(Contents, *(Server->WebServerRootDir + (FCStringAnsi::Strcmp(HttpRequest->url, "/") == 0 ? TEXT("/index.html") : UTF8_TO_TCHAR(HttpRequest->url)))))
		{
			// set the contents as the body of the response
			auto UTF8 = StringCast<ANSICHAR>(*Contents);
			sceCompanionHttpdSetBody(UTF8.Get(), (size_t)UTF8.Length(), HttpResponse);

			return SCE_OK;
		}
	}
	
	// if there was no response, and no file could be loaded, then return a value that will make look on disk for file
	return SCE_COMPANION_HTTPD_ERROR_NOT_GENERATE_RESPONSE;
}
#endif

void FPS4CompanionServer::Tick(float DeltaTime)
{
#if PS4_FEATURE_COMPANION_APP
	// look for any events
	SceCompanionHttpdEvent Event;
	if (sceCompanionHttpdGetEvent(&Event) == SCE_OK)
	{
		// when someone connects, start the server
		if (Event.event == SCE_COMPANION_HTTPD_EVENT_CONNECT)
		{
			if (NumConnections == 0)
			{
				// start the server
				int32 Result = sceCompanionHttpdStart();
				if (Result != SCE_OK) 
				{
					UE_LOG(LogPS4, Log, TEXT("Failed to sceCompanionHttpdStart, ret = %x"), Result);
				}
				Result = sceCompanionHttpdRegisterRequestCallback(RequestCallback, this);
				if (Result != SCE_OK) 
				{
					UE_LOG(LogPS4, Log, TEXT("Failed to sceCompanionHttpdRegisterRequestCallback, ret = %x"), Result);
				}
			}
			NumConnections++;
		}
		else if (Event.event == SCE_COMPANION_HTTPD_EVENT_DISCONNECT)
		{
			NumConnections--;
			if (NumConnections == 0)
			{
				// stop the server
				int32 Result = sceCompanionHttpdStop();
				if (Result != SCE_OK)
				{
					UE_LOG(LogPS4, Log, TEXT("Failed to sceCompanionHttpdStop, ret = %x"), Result);
				}
			}
		}
	}
#endif
}

TStatId FPS4CompanionServer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPS4CompanionServer, STATGROUP_Tickables);
}
