#include "DLCWidget.h"
#include "AndroidPermissionFunctionLibrary.h"
#include "AndroidPermissionCallbackProxy.h"
#include "CoreMinimal.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "OVRProduct.h"

// An asset file we uploaded to the Oculus dashboard using ovr-platform-util.
const char* UDLCWidget::DLC_ASSET_FILE_NAME = "DLCAsset.txt";

// Purchasable IAP products we've configured on the Oculus Dashboard
const char* UDLCWidget::CONSUMABLE_PURCHASE_SKU = "EXAMPLECON";
const char* UDLCWidget::DURABLE_PURCHASE_SKU = "EXAMPLEDUR";

void UDLCWidget::NativeConstruct()
{
	// Store a reference to the OnlineSubsystem since we'll be using it in many places.
	OSS = static_cast<FOnlineSubsystemOculus*>(IOnlineSubsystem::Get());

	// Initialize all of our properties to the default state: download unavailable and permissions denied.
	DLCAssetText = TEXT("Unable to download DLC Asset: both READ_EXTERNAL_STORAGE and INTERNET permissions must be enabled.");
	DLCStatus = EDLCStatus::DS_UNAVAILABLE;
	DLCLoadProgress = 1.0;
	DownloadedAssetFilePath = nullptr;
	HasPermissions = false;

	// Register for any asset file download updates so we can update our widgets
	// and load the downloaded file if the download has completed.
	DownloadUpdateNotificationHandle =
		OSS->GetNotifDelegate(ovrMessage_Notification_AssetFile_DownloadUpdate)
		.AddUObject(this, &UDLCWidget::DLCAssetDownloadProgressUpdate);
	
	// Check to see whether permissions are available, and if not ask to acquire them.
	AcquirePermissions();
}

void UDLCWidget::NativeDestruct()
{
	// Clean up the download update notification handle.
	if (DownloadUpdateNotificationHandle.IsValid())
	{
		OSS->RemoveNotifDelegate(ovrMessage_Notification_Matchmaking_MatchFound, DownloadUpdateNotificationHandle);
		DownloadUpdateNotificationHandle.Reset();
	}
}

void UDLCWidget::AcquirePermissions()
{
#if PLATFORM_ANDROID
	// Check to see if we have the required permissions
	bool HasReadExternalStoragePermission = UAndroidPermissionFunctionLibrary::CheckPermission(TEXT("android.permission.READ_EXTERNAL_STORAGE"));
	bool HasInternetPermission = UAndroidPermissionFunctionLibrary::CheckPermission(TEXT("android.permission.INTERNET"));
	if (!HasReadExternalStoragePermission || !HasInternetPermission)
	{
		TArray<FString> PermsToEnable;
		if (!HasReadExternalStoragePermission)
		{
			PermsToEnable.Add(TEXT("android.permission.READ_EXTERNAL_STORAGE"));
		}
		if (!HasInternetPermission)
		{
			PermsToEnable.Add(TEXT("android.permission.INTERNET"));
		}

		// Request any missing permissions
		if (UAndroidPermissionCallbackProxy* Callback = UAndroidPermissionFunctionLibrary::AcquirePermissions(PermsToEnable))
		{
			Callback->OnPermissionsGrantedDelegate.BindLambda([this](const TArray<FString>& Permissions, const TArray<bool>& GrantResults)
				{
					if (GrantResults.Num() >= 2 && GrantResults[0] && GrantResults[1])
					{
						// We were granted the permissions, so we can continue.
						OnPermissionsGranted();
					}
				});
		}
	}
	else
	{
		// We already had the permissions, so we can continue.
		OnPermissionsGranted();
	}
#else
	OnPermissionsGranted();
#endif
}

void UDLCWidget::OnPermissionsGranted()
{
	HasPermissions = true;

	// We launch two tasks here.
	// First, we fetch information about the DLC asset we will try to download.
	FetchAssetFileInfo();

	// Second, we retrieve a list of the in-app-purchase products that the user
	// has already purchased. We will retrieve the full available product list
	// once we know which ones the user already owns.
	FetchPurchasedProducts();
}

void UDLCWidget::FetchAssetFileInfo()
{
	// We need to retrieve information about all available download assets
	// and then iterate through the list to find one that matches DLC_ASSET_FILE_NAME.

	ovrRequest AssetListRequest = ovr_AssetFile_GetList();
	DLCAssetText = TEXT("Retrieving DLC Asset List.");

	OSS->AddRequestDelegate(AssetListRequest, FOculusMessageOnCompleteDelegate::CreateLambda(
		[this](ovrMessageHandle Message, bool bIsError)
		{
			if (bIsError)
			{
				ovrErrorHandle Error = ovr_Message_GetError(Message);
				DLCAssetText = TEXT("Error retrieving asset list: ") + FString(ovr_Error_GetMessage(Error));
				UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ovr_Error_GetMessage(Error)));
				return;
			}

			ovrAssetDetailsArrayHandle AssetDetailsArray = ovr_Message_GetAssetDetailsArray(Message);

			size_t AssetCount = ovr_AssetDetailsArray_GetSize(AssetDetailsArray);
			UE_LOG(LogTemp, Display, TEXT("%llu assets found in the asset detail array."), (uint64_t)AssetCount);
			for (size_t i = 0; i < AssetCount; ++i)
			{
				ovrAssetDetailsHandle Asset = ovr_AssetDetailsArray_GetElement(AssetDetailsArray, i);
				const char* FilePath = ovr_AssetDetails_GetFilepath(Asset);
				UE_LOG(LogTemp, Display, TEXT("Found asset with path %s"), *FString(FilePath));
				DLCAssetText = TEXT("Asset available for download.");

				// If the file path for this asset is a partial match for DLC_ASSET_FILE_NAME,
				// mark the download as available and store the asset handle for later use.
				if (strstr(FilePath, DLC_ASSET_FILE_NAME) != NULL)
				{
					DLCAssetHandle = Asset;
					DLCStatus = EDLCStatus::DS_AVAILABLE;
					DLCLoadProgress = 0.0;

					const char* DownloadStatus = ovr_AssetDetails_GetDownloadStatus(DLCAssetHandle);
					// If we've already downloaded the DLC, load it immediately.
					if (strcmp(DownloadStatus, "installed") == 0)
					{
						DLCStatus = EDLCStatus::DS_DOWNLOADED;
						DLCLoadProgress = 1.0;
						DownloadedAssetFilePath = ovr_AssetDetails_GetFilepath(DLCAssetHandle);
						LoadDownloadedTextAsset();
					}

					break;
				}
			}
			
			// If we couldn't find the asset we were looking for, output some debug text on the screen
			// containing the list of assets we did find.
			if (!DLCAssetHandle)
			{
				DLCStatus = EDLCStatus::DS_UNAVAILABLE;

				DLCAssetText = FString(TEXT("DLC Asset not found. Found ")) + FString::FromInt(AssetCount) + FString(TEXT(" assets: \n"));
				for (size_t i = 0; i < AssetCount; ++i)
				{
					ovrAssetDetailsHandle Asset = ovr_AssetDetailsArray_GetElement(AssetDetailsArray, i);
					const char* FilePath = ovr_AssetDetails_GetFilepath(Asset);
					DLCAssetText += FString(FilePath);
				}
			}
		}));
}

/// Launches the download flow for DLC_ASSET_FILE_NAME.
void UDLCWidget::DownloadDLCAsset()
{
	// If the asset isn't available, there's nothing we can do here.
	if (DLCStatus != EDLCStatus::DS_AVAILABLE)
	{
		return;
	}

	// If we're not entitled to the asset (for example, if it's tied to an in-app purchase),
	// we mark the DLC as unavailable. This shouldn't be the case for this example demo
	// unless you associated the asset file with a purchase SKU in the Oculus Dashboard.
	const char* IAPStatus = ovr_AssetDetails_GetIapStatus(DLCAssetHandle);
	if (strcmp(IAPStatus, "not-entitled") == 0)
	{
		DLCAssetText = TEXT("Unable to download DLC Asset: the user is not entitled to download this asset.");
		DLCStatus = EDLCStatus::DS_UNAVAILABLE;
		return;
	}

	// Start the download.
	ovrID AssetID = ovr_AssetDetails_GetAssetId(DLCAssetHandle);
	ovrRequest DownloadRequest = ovr_AssetFile_DownloadById(AssetID);
	DLCStatus = EDLCStatus::DS_DOWNLOADING;
	DLCAssetText = TEXT("Downloading DLC Asset.");
	DLCLoadProgress = 0.0;

	// The ovrAssetFileDownloadResult is available immediately after starting the download;
	// progress updates will be called on the DLCAssetDownloadProgressUpdate callback that
	// we registered in NativeConstruct.
	OSS->AddRequestDelegate(DownloadRequest, FOculusMessageOnCompleteDelegate::CreateLambda(
		[this](ovrMessageHandle Message, bool bIsError)
		{
			if (bIsError)
			{
				DLCStatus = EDLCStatus::DS_UNAVAILABLE;
				ovrErrorHandle Error = ovr_Message_GetError(Message);
				UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ovr_Error_GetMessage(Error)));
				DLCAssetText = TEXT("Error downloading DLC asset: ") + FString(ovr_Error_GetMessage(Error));
				return;
			}

			// We need to store the file-path that the asset file will be downloaded to
			// so we can load from it later.
			ovrAssetFileDownloadResultHandle Result = ovr_Message_GetAssetFileDownloadResult(Message);
			const char* FilePath = ovr_AssetFileDownloadResult_GetFilepath(Result);
			DownloadedAssetFilePath = FilePath;
		}));

	return;
}

void UDLCWidget::DLCAssetDownloadProgressUpdate(ovrMessageHandle Message, bool bIsError)
{
	if (bIsError)
	{
		DLCStatus = EDLCStatus::DS_UNAVAILABLE;
		ovrErrorHandle Error = ovr_Message_GetError(Message);
		UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ovr_Error_GetMessage(Error)));
		DLCAssetText = TEXT("Error downloading DLC asset: ") + FString(ovr_Error_GetMessage(Error));
		return;
	}

	ovrAssetFileDownloadUpdateHandle Update = ovr_Message_GetAssetFileDownloadUpdate(Message);

	// The download completion is notified through this callback for 
	// ovrMessage_Notification_AssetFile_DownloadUpdate, which we registered in NativeConstruct.
	if (ovr_AssetFileDownloadUpdate_GetCompleted(Update))
	{
		DLCLoadProgress = 1.0;
		DLCStatus = EDLCStatus::DS_DOWNLOADED;

		// If we've completed the download, we can load its data.
		LoadDownloadedTextAsset();
	}
	else
	{
		// The download update callback also allows us to query how much progress the download has made.
		size_t BytesDownloaded = ovr_AssetFileDownloadUpdate_GetBytesTransferred(Update);
		size_t BytesTotal = ovr_AssetFileDownloadUpdate_GetBytesTotal(Update);
		DLCLoadProgress = (float)((double)BytesDownloaded / (double)BytesTotal);
	}
}

void UDLCWidget::DeleteDownloadedAsset()
{
	if (DLCStatus != EDLCStatus::DS_DOWNLOADED || !DownloadedAssetFilePath)
	{
		return;
	}

	// We can delete the downloaded asset to allow the user to run the download flow again.
	FString AbsoluteFilePath(DownloadedAssetFilePath);
	if (FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*AbsoluteFilePath))
	{
		DLCStatus = EDLCStatus::DS_UNAVAILABLE;
		DLCLoadProgress = 1.0;
		DownloadedAssetFilePath = nullptr;
		DownloadUpdateNotificationHandle.Reset();

		FetchAssetFileInfo();
	}
}

/// Loads text data from the file at DownloadedAssetFilePath and sets DLCAssetText
/// so the downloaded data is displayed in the UI.
void UDLCWidget::LoadDownloadedTextAsset()
{
	if (!DownloadedAssetFilePath)
	{
		DLCStatus = EDLCStatus::DS_INVALID;
		DLCAssetText = TEXT("No file path provided for DLC asset");
		return;
	}

	FString AbsoluteFilePath(DownloadedAssetFilePath);

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*AbsoluteFilePath))
	{
		DLCAssetText = TEXT("No DLC asset file found at path ") + AbsoluteFilePath;
		DLCStatus = EDLCStatus::DS_INVALID;
	}
	else
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* FileHandle = PlatformFile.OpenRead(*AbsoluteFilePath);
		if (FileHandle)
		{
			size_t FileSize = FileHandle->Size();
			uint8_t* Buffer = new uint8_t[FileSize];
			FileHandle->Read(Buffer, FileSize);
			delete FileHandle;

			const char* String = reinterpret_cast<const char*>(Buffer);
			DLCAssetText = TEXT("Text successfully downloaded:\n") + FString(String);
			delete[] Buffer;

			DLCStatus = EDLCStatus::DS_DOWNLOADED;
		}
		else
		{
			DLCAssetText = TEXT("Loading DLC asset failed (can't read file)");
			DLCStatus = EDLCStatus::DS_INVALID;
		}
	}
}

void UDLCWidget::FetchPurchasedProducts()
{
	// Retrieve the list of purchased products so we know which products
	// the user already owns when displaying the IAP list.
	ovrRequest RequestId = ovr_IAP_GetViewerPurchases();

	OSS->AddRequestDelegate(RequestId, FOculusMessageOnCompleteDelegate::CreateLambda(
		[this](ovrMessageHandle Message, bool bIsError)
		{
			if (bIsError)
			{
				ovrErrorHandle Error = ovr_Message_GetError(Message);
				UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ovr_Error_GetMessage(Error)));
				return;
			}

			ovrPurchaseArrayHandle PurchaseArray = ovr_Message_GetPurchaseArray(Message);
			size_t Size = ovr_PurchaseArray_GetSize(PurchaseArray);

			TSet<FString> PurchasedSKUs;
			for (size_t i = 0; i < Size; ++i)
			{
				ovrPurchaseHandle PurchaseHandle = ovr_PurchaseArray_GetElement(PurchaseArray, i);
				const char* SKU = ovr_Purchase_GetSKU(PurchaseHandle);
				unsigned long long GrantTime = ovr_Purchase_GetGrantTime(PurchaseHandle);
				UE_LOG(LogTemp, Display, TEXT("Purchased: sku = %s, grant time = %llu"), *FString(SKU), GrantTime);
				PurchasedSKUs.Add(FString(SKU));
			}

			// Pass the purchases list through to FetchProductDetails so that its request
			// can cross-reference against the purchases list.
			FetchProductDetails(PurchasedSKUs);
		}));
}


// get the current price for the configured IAP item
void UDLCWidget::FetchProductDetails(const TSet<FString>& PurchasedSKUs)
{
	// Launch a request to get information about our two specified SKUs.
	const char* skus[] = { CONSUMABLE_PURCHASE_SKU, DURABLE_PURCHASE_SKU };
	ovrRequest RequestId = ovr_IAP_GetProductsBySKU(skus, sizeof(skus) / sizeof(skus[0]));

	OSS->AddRequestDelegate(RequestId, FOculusMessageOnCompleteDelegate::CreateLambda(
		[this, PurchasedSKUs](ovrMessageHandle Message, bool bIsError)
		{
			if (bIsError)
			{
				ovrErrorHandle Error = ovr_Message_GetError(Message);
				UE_LOG(LogTemp, Error, TEXT("%s"), *FString(ovr_Error_GetMessage(Error)));
				return;
			}

			ovrProductArrayHandle ProductArray = ovr_Message_GetProductArray(Message);
			size_t Size = ovr_ProductArray_GetSize(ProductArray);

			for (size_t i = 0; i < Size; ++i)
			{
				// Get information about each product and output it to the log.
				ovrProductHandle ProductHandle = ovr_ProductArray_GetElement(ProductArray, i);
				const char* SKU = ovr_Product_GetSKU(ProductHandle);
				const char* Name = ovr_Product_GetName(ProductHandle);
				const char* FormattedPrice = ovr_Product_GetFormattedPrice(ProductHandle);
				UE_LOG(LogTemp, Display, TEXT("Product: SKU = %s, name = %s, price = %s"), *FString(SKU), *FString(Name), *FString(FormattedPrice));

				// Check to see whether the product is consumable. 
				// Since we only have one consumable purchase we can simply compare against its SKU.
				bool Consumable = strcmp(CONSUMABLE_PURCHASE_SKU, SKU) == 0;
				bool Purchased = PurchasedSKUs.Contains(FString(SKU));

				// Create a UOVRProduct model object and pass it to the 
				// DLCListView for display.
				UOVRProduct* Product = NewObject<UOVRProduct>();
				Product->SetProduct(ProductHandle, Consumable, Purchased, &this->ConsumableCounter);
				DLCListView->AddItem(Product);
			}
		}));
}
