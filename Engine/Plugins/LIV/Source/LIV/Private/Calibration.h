#pragma once

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>
#include "Calibration.generated.h"

UCLASS()
class ACalibration : public AActor
{
	GENERATED_BODY()

public:
	ACalibration();

protected:
	void BeginPlay() override;

	FString GetConfigDir() const;

	static FString TrimCalibrationLine(FString CalibrationLine);

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LIV")
		bool IsConfigAvailable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LIV")
		FVector LocationOffset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LIV")
		FRotator RotationOffset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LIV")
		float FOV;

	UFUNCTION(BlueprintCallable, Category = "LIV")
		bool ReadConfig();
};
