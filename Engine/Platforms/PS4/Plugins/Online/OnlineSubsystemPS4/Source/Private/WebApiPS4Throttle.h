// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WebApiPS4Types.h"

THIRD_PARTY_INCLUDES_START
#include <np/np_webapi.h>
THIRD_PARTY_INCLUDES_END

enum class EOnlinePSNRateLimitStatus
{
	NotRateLimited,
	RateLimited
};

struct FNpWebApiThrottle
{
public:
	static EOnlinePSNRateLimitStatus GetRateLimitStatus(const ENpApiGroup ApiGroup)
	{
		const FScopeLock ScopeLock(GetCriticalSection());
		TMap<ENpApiGroup, FDateTime>& ApiGroupWaitTimeMap = GetApiGroupWaitTimeMap();

		if (const FDateTime* const NextApiTime = ApiGroupWaitTimeMap.Find(ApiGroup))
		{
			const FDateTime UtcNow = FDateTime::UtcNow();
			if (UtcNow >= *NextApiTime)
			{
				return EOnlinePSNRateLimitStatus::NotRateLimited;
			}

			return EOnlinePSNRateLimitStatus::RateLimited;
		}

		return EOnlinePSNRateLimitStatus::NotRateLimited;
	}

	static void SetNextWebRequestTime(const ENpApiGroup ApiGroup, const FDateTime NextTime)
	{
		bool bNeedsToAdd = true;

		const FScopeLock ScopeLock(GetCriticalSection());
		TMap<ENpApiGroup, FDateTime>& ApiGroupWaitTimeMap = GetApiGroupWaitTimeMap();

		if (const FDateTime* const CurrentNextApiTime = ApiGroupWaitTimeMap.Find(ApiGroup))
		{
			if (NextTime <= *CurrentNextApiTime)
			{
				bNeedsToAdd = false;
			}
		}

		if (bNeedsToAdd)
		{
			UE_LOG_ONLINE(Verbose, TEXT("PSN Web Api Group %s Rate-Limited until %s"), LexToString(ApiGroup), *NextTime.ToString());

			ApiGroupWaitTimeMap.Add(ApiGroup, NextTime);
		}
	}

	static void DetermineNextWebRequestTime(const ENpApiGroup ApiGroup, const uint64_t NpWebApiRequestId)
	{
		size_t HeaderValueSize = 0;
		int32 ReturnCode = sceNpWebApiGetHttpResponseHeaderValueLength(NpWebApiRequestId, FNpWebApiThrottle::SONY_RATE_LIMIT_HEADER_NAME, &HeaderValueSize);
		if (ReturnCode >= SCE_OK)
		{
			if (HeaderValueSize > 0)
			{
				TUniquePtr<ANSICHAR[]> HeaderValueBuffer = MakeUnique<ANSICHAR[]>(HeaderValueSize);
				ReturnCode = sceNpWebApiGetHttpResponseHeaderValue(NpWebApiRequestId, FNpWebApiThrottle::SONY_RATE_LIMIT_HEADER_NAME, HeaderValueBuffer.Get(), HeaderValueSize);
				if (ReturnCode >= SCE_OK)
				{
					const int64 UnixTimestamp = FCStringAnsi::Atoi64(HeaderValueBuffer.Get());
					if (UnixTimestamp > 0)
					{
						const FDateTime NextRequestTime = FDateTime::FromUnixTimestamp(UnixTimestamp);
						FNpWebApiThrottle::SetNextWebRequestTime(ApiGroup, NextRequestTime);
					}
					else
					{
						UE_LOG_ONLINE(Error, TEXT("Received odd throttle time from psn rate-limit header: %s"), ANSI_TO_TCHAR(HeaderValueBuffer.Get()));
					}
				}
				else
				{
					UE_LOG_ONLINE(Error, TEXT("sceNpWebApiGetHttpResponseHeaderValue() failed. error = 0x%08x"), ReturnCode);
				}
			}
		}
		else
		{
			UE_LOG_ONLINE(Error, TEXT("sceNpWebApiGetHttpResponseHeaderValueLength() failed. error = 0x%08x"), ReturnCode);
		}
	}

public:
	static constexpr const char* const SONY_RATE_LIMIT_HEADER_NAME = "X-RateLimit-Next-Available";

private:
	static FCriticalSection* GetCriticalSection()
	{
		static FCriticalSection CriticalSection;
		return &CriticalSection;
	}

	static TMap<ENpApiGroup, FDateTime>& GetApiGroupWaitTimeMap()
	{
		static TMap<ENpApiGroup, FDateTime> ApiGroupWaitTimeMap;
		return ApiGroupWaitTimeMap;
	}
};
