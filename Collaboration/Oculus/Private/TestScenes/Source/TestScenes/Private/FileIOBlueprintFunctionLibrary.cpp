// Fill out your copyright notice in the Description page of Project Settings.

#include "FileIOBlueprintFunctionLibrary.h"
#include "TestScenes.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformFile.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"


bool UFileIOBlueprintFunctionLibrary::FileExists(FString FilePath)
{
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath);
}

bool UFileIOBlueprintFunctionLibrary::WriteStringToFile(FString FilePath, FString Contents, bool AllowOverwrite)
{
	FString Directory = FPaths::GetPath(FilePath);

	if (!FPaths::DirectoryExists(Directory))
	{
		if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory))
		{
			return false;
		}
	}

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath) && !AllowOverwrite)
	{
		return false;
	}

	if (FFileHelper::SaveStringToFile(Contents, *FilePath))
	{
		return true;
	}
	   	
	return false;
}

bool UFileIOBlueprintFunctionLibrary::DeleteFile(FString FilePath)
{
	if (FPaths::FileExists(FilePath))
	{
		if (!FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*FilePath))
		{
			return false;
		}
		return true;
	}
	return false;
}
