#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OVR_Platform.h"
#include "CloudSaveWidget.generated.h"

/**
 * A derived user widget to handle cloud saves.
 * Demonstrates usage of the Oculus cloud storage (v2) API to handle cloud saves.
 */
UCLASS()
class CLOUDSAVEDLC_API UCloudSaveWidget : public UUserWidget
{
	GENERATED_BODY()

private:	
	virtual void NativeConstruct() override;
	
public:
	void GetUserDirPath();

	UFUNCTION(BlueprintCallable, Category = "BPCloudSave")
	void IncrementCount();

	UFUNCTION(BlueprintCallable, Category = "BPCloudSave")
	void DecrementCount();

	UFUNCTION(BlueprintCallable, Category = "BPCloudSave")
	void LoadFromCloud();

	UFUNCTION(BlueprintCallable, Category = "BPCloudSave")
	void SaveToCloud();

	UFUNCTION(BlueprintCallable, Category = "BPCloudSave")
	void RequestWriteStoragePermission();

	// Counter state
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BPCloudSave")
	int32 Counter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BPCloudSave")
	bool HasAcquiredCloudStorageUserDirPath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BPCloudSave")
	FString CloudStorageUserDirPath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BPCloudSave")
	bool HasWriteExternalStoragePermission;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BPCloudSave")
	FString LoadSaveStatus;
};
