#pragma once

#include "CoreMinimal.h"
#include "JsonObjectConverter.h"
#include "OculusXRHMD.h"
#include "TestStructs.generated.h"

UENUM(BlueprintType)
enum class EAdaptiveResPassType : uint8
{
	High,
	Low
};


USTRUCT(BlueprintType)
struct FAdaptiveResPassResults
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LowestFrameRate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HighestFrameRate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float InitialScreenPercentage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FinalFrameRate;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FinalScreenPercentage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAdaptiveResPassType PassType;
};

USTRUCT(BlueprintType)
struct FAdaptiveResResults
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
		FString VersionString;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		TArray<FAdaptiveResPassResults> Results;

	FAdaptiveResResults() :
		Results(TArray<FAdaptiveResPassResults>())
	{
		if (GEngine && GEngine->XRSystem.IsValid())
		{
			if (GEngine->XRSystem->GetSystemName() == TEXT("OculusXRHMD"))
			{
				OculusXRHMD::FOculusXRHMD* HMD = static_cast<OculusXRHMD::FOculusXRHMD*>(GEngine->XRSystem.Get());
				VersionString = HMD->GetVersionString();
			}
		}
	}
};

UCLASS()
class TESTSCENES_API UTestStructToJSONFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject", Category = "TestStructs"))
	static bool AdaptiveResResultsToString(const FAdaptiveResResults Results, FString& ResultsString)
	{
		return UTestStructToJSONFunctionLibrary::ConvertStructToString<FAdaptiveResResults>(Results, ResultsString);
	}

	template <typename StructType>
	static bool ConvertStructToString(const StructType Struct, FString& JSONString)
	{
		if (FJsonObjectConverter::UStructToJsonObjectString<StructType>(Struct, JSONString))
		{
			return true;
		}
		return false;
	}
};
