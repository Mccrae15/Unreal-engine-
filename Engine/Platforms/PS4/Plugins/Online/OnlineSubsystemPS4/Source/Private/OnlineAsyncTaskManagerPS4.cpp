// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineAsyncTaskManagerPS4.h"
#include "OnlineSubsystemPS4.h"


void FOnlineAsyncTaskManagerPS4::OnlineTick()
{
	check(PS4Subsystem);
	check(IsInOnlineThread());
}

bool FOnlineAsyncTaskManagerPS4::IsInOnlineThread() const
{
	return FPlatformTLS::GetCurrentThreadId() == OnlineThreadId;
}

void FOnlineAsyncTaskPS4::PrintNPToolkitServerError(const FString& Prefix, const NpToolkit::Core::ServerError& ServerError)
{
	UE_LOG_ONLINE(Warning, TEXT("%s: ServerError: httpStatusCode: %d"), *Prefix, ServerError.httpStatusCode);
	UE_LOG_ONLINE(Warning, TEXT("%s: ServerError: webApiNextAvailableTime: %lld"), *Prefix, ServerError.webApiNextAvailableTime);

	// Ensure that the jsonData string is null terminated (documentation does not indicate it will be)
	char ServerErrorJsonData[NpToolkit::Core::ServerError::JSON_DATA_MAX_LEN + 1];
	FCStringAnsi::Strncpy(ServerErrorJsonData, ServerError.jsonData, NpToolkit::Core::ServerError::JSON_DATA_MAX_LEN + 1);
	const int32 ServerErrorJsonDataLen = FCStringAnsi::Strlen(ServerErrorJsonData);
	if (ServerErrorJsonDataLen > 0)
	{
		auto ConvertedString = StringCast<TCHAR>(ServerErrorJsonData, ServerErrorJsonDataLen);
		UE_LOG_ONLINE(Warning, TEXT("%s: ServerError: jsonData=[%s]"), *Prefix, ConvertedString.Get());
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("%s: ServerError: jsonData=<missing>"), *Prefix);
	}
}
