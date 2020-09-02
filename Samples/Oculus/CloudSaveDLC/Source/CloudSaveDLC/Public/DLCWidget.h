#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ListView.h"
#include "OVR_Platform.h"
#include "OnlineSubsystemOculus.h"
#include "DLCWidget.generated.h"

UENUM()
enum class EDLCStatus : uint8
{
	DS_FETCHING UMETA(DisplayName="Fetching Status"),
	DS_UNAVAILABLE UMETA(DisplayName="Asset Unavailable"),
	DS_AVAILABLE UMETA(DisplayName="Asset Available"),
	DS_DOWNLOADING UMETA(DisplayName="Downloading Asset"),
	DS_DOWNLOADED UMETA(DisplayName="Asset Downloaded"),
	DS_INVALID UMETA(DisplayName = "Asset Invalid")
};

/**
 * A derived user widget to handle DLC.
 * Demonstrates how to use the Oculus API to handle downloading assets 
 * and purchasing in-app purchases
 */
UCLASS()
class CLOUDSAVEDLC_API UDLCWidget : public UUserWidget
{
	GENERATED_BODY()

private:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	void OnPermissionsGranted();
	void LoadDownloadedTextAsset();
	void FetchAssetFileInfo();
	void FetchProductDetails(const TSet<FString>& PurchasedSKUs);
	void FetchPurchasedProducts();

	void DLCAssetDownloadProgressUpdate(ovrMessageHandle Message, bool bIsError);

	FOnlineSubsystemOculus* OSS;
	ovrAssetDetailsHandle DLCAssetHandle;
	FDelegateHandle DownloadUpdateNotificationHandle;
	const char* DownloadedAssetFilePath;

	static const char* DURABLE_PURCHASE_SKU;
	static const char* CONSUMABLE_PURCHASE_SKU;
	static const char* DLC_ASSET_FILE_NAME;

public:
	UFUNCTION(BlueprintCallable, Category = "BPDLC")
	void DownloadDLCAsset();

	UFUNCTION(BlueprintCallable, Category = "BPDLC")
	void DeleteDownloadedAsset();

	UFUNCTION(BlueprintCallable, Category = "BPDLC")
	void AcquirePermissions();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BPDLC")
	bool HasPermissions;

	/// The current status of the download, which can be queried by
	// Blueprint code to update the UI state.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BPDLC")
	EDLCStatus DLCStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BPDLC")
	FString DLCAssetText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BPDLC")
	float DLCLoadProgress;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "BPDLC")
	UListView* DLCListView;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BPDLC")
	int32 ConsumableCounter;
};
