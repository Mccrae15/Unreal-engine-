#include "PermissionsUserWidget.h"
#include "AndroidPermissionFunctionLibrary.h"
#include "AndroidPermissionCallbackProxy.h"
#include "CoreMinimal.h"

void UPermissionsUserWidget::NativeConstruct()
{
	// Check if we have the related permissions enabled.
	HasMicPermission = HasPermission(TEXT("android.permission.RECORD_AUDIO"));
	HasInternetPermission = HasPermission(TEXT("android.permission.INTERNET"));
	HasWriteExternalStoragePermission = HasPermission(TEXT("android.permission.WRITE_EXTERNAL_STORAGE"));
}

bool UPermissionsUserWidget::HasPermission(const FString& PermissionStr)
{
#if PLATFORM_ANDROID
	return UAndroidPermissionFunctionLibrary::CheckPermission(PermissionStr);
#else
	return true;
#endif
}

void UPermissionsUserWidget::RequestPermission(const FString& PermissionStr)
{
#if PLATFORM_ANDROID
	if (!UAndroidPermissionFunctionLibrary::CheckPermission(PermissionStr))
	{
		// As a demonstration, we only request for a unique permission at a time.
		TArray<FString> PermsToEnable;
		PermsToEnable.Add(PermissionStr);

		if (UAndroidPermissionCallbackProxy* Callback = UAndroidPermissionFunctionLibrary::AcquirePermissions(PermsToEnable))
		{
			// We bind a lambda to the delegate to update the class' permission states.
			Callback->OnPermissionsGrantedDelegate.AddLambda([this, PermissionStr](const TArray<FString>& Permissions, const TArray<bool>& GrantResults)
			{
				bool HasBeenGranted = false;
				if (GrantResults.Num() > 0)
				{
					// Technically, we don't need to loop through the returned permissions as we only request a unique permission at a time.
					// However, this demonstrates how one would handle multiple permission requests.
					for (int32 i = 0; i < Permissions.Num(); ++i)
					{
						if (Permissions[i] == PermissionStr)
						{
							HasBeenGranted = GrantResults[i];
							break;
						}
					}
				}
				
				// Update the correct permission state depending on the function parameter that was passed in.
				if (TEXT("android.permission.RECORD_AUDIO") == PermissionStr)
				{
					HasMicPermission = HasBeenGranted;
				}
				else if (TEXT("android.permission.INTERNET") == PermissionStr)
				{
					HasInternetPermission = HasBeenGranted;
				}
				else if (TEXT("android.permission.WRITE_EXTERNAL_STORAGE") == PermissionStr)
				{
					HasWriteExternalStoragePermission = HasBeenGranted;
				}
			});
		}
	}
#endif
}

void UPermissionsUserWidget::RequestMicPermission()
{
	RequestPermission(TEXT("android.permission.RECORD_AUDIO"));
}

void UPermissionsUserWidget::RequestInternetPermission()
{
	RequestPermission(TEXT("android.permission.INTERNET"));
}

void UPermissionsUserWidget::RequestWriteExternalStoragePermission()
{
	RequestPermission(TEXT("android.permission.WRITE_EXTERNAL_STORAGE"));
}
