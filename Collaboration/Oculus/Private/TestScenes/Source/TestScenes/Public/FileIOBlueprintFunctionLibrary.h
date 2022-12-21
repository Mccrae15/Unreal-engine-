// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FileIOBlueprintFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class TESTSCENES_API UFileIOBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", Category = "FileIOLibrary"))
		static bool FileExists(FString FilePath);

	UFUNCTION(BlueprintCallable, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", Category = "FileIOLibrary"))
		static bool WriteStringToFile(FString FilePath, FString Contents, bool AllowOverwrite);

	UFUNCTION(BlueprintCallable, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", Category = "FileIOLibrary"))
		static bool DeleteFile(FString FilePath);
};
