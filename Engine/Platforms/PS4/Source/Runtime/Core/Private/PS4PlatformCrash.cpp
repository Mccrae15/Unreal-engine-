// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildSettings.h"
#include "Misc/CString.h"

#include <scebase_common.h>
#include <coredump.h>
#include <coredump_structureddata.h>

namespace FPS4CoreDumpHandler
{
	static void AddWideString(const char* Key, const TCHAR* Value, sce::CoredumpStructuredData::StructuredUserdata& Data)
	{
		// Manually narrow the string on the stack without making heap memory allocations
		int32 Chars = FCString::Strlen(Value);
		char* Result = (char*)alloca(Chars + 1);

		for (int32 Index = 0; Index < Chars; ++Index)
		{
			const TCHAR WideValue = Value[Index];
			Result[Index] = (WideValue > 255) ? '.' : (char)WideValue;
		}
		Result[Chars] = 0;

		Data.addValue(Key, Result);
	}

	static int32 HandleCoreDump(void* Common)
	{
		sce::CoredumpStructuredData::StructuredUserdata UserData;
		AddWideString("Branch Name", BuildSettings::GetBranchName(), UserData);
		AddWideString("Build Date", BuildSettings::GetBuildDate(), UserData);
		AddWideString("Build Version", BuildSettings::GetBuildVersion(), UserData);

#if UE_BUILD_DEBUG
		UserData.addValue("Build Configuration", "Debug");
#elif UE_BUILD_DEVELOPMENT
		UserData.addValue("Build Configuration", "Development");
#elif UE_BUILD_TEST
		UserData.addValue("Build Configuration", "Test");
#elif UE_BUILD_SHIPPING
		UserData.addValue("Build Configuration", "Shipping");
#else
#error Unknown build configuration
#endif

		UserData.addValue("Compatible Changelist", BuildSettings::GetCompatibleChangelist());
		UserData.addValue("Current Changelist", BuildSettings::GetCurrentChangelist());

		return SCE_OK;
	}

	void Initialize()
	{
		// Called very early in process lifetime, just after the memory allocators are initialized.
		// Hooks our custom crash dump handler so we get orbisdmps with valid crash context user data.
		int32 Ret = sceCoredumpRegisterCoredumpHandler(HandleCoreDump, 16384, nullptr);
		check(Ret == SCE_OK);
	}
}
