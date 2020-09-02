#include "CloudSaveWidget.h"
#include "AndroidPermissionFunctionLibrary.h"
#include "AndroidPermissionCallbackProxy.h"
#include "OnlineSubsystemOculus.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "CoreMinimal.h"

// The cloud save file name we use for the app
#define CLOUD_SAVE_FILE_NAME "/count_save.dat"

void UCloudSaveWidget::NativeConstruct()
{
	Counter = 0;
	HasWriteExternalStoragePermission = true;
	HasAcquiredCloudStorageUserDirPath = false;

	// As described at https://developer.oculus.com/documentation/platform/latest/concepts/dg-cc-cloud-storage/,
	// we need the WRITE_EXTERNAL_STORAGE permission.
#if PLATFORM_ANDROID	
	if (!UAndroidPermissionFunctionLibrary::CheckPermission(TEXT("android.permission.WRITE_EXTERNAL_STORAGE")))
		HasWriteExternalStoragePermission = false;
	else
		GetUserDirPath();
#endif
}

void UCloudSaveWidget::LoadFromCloud()
{
	// Try to load the save file from our cloud storage directory.
	FString AbsoluteFilePath = CloudStorageUserDirPath + TEXT(CLOUD_SAVE_FILE_NAME);
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*AbsoluteFilePath))
	{
		LoadSaveStatus = TEXT("Load fail (no cloud save found)");
	}
	else
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* FileHandle = PlatformFile.OpenRead(*AbsoluteFilePath);

		// For simplicity, our save file directly stores the int32's bytes.
		if (FileHandle)
		{			
			int32* IntPointer = &Counter;			
			uint8* ByteBuffer = reinterpret_cast<uint8*>(IntPointer);			
			FileHandle->Read(ByteBuffer, sizeof(int32));
			delete FileHandle;

			LoadSaveStatus = TEXT("Loaded successfully");
		}
		else
			LoadSaveStatus = TEXT("Load failed (can't read file)");
	}
}

void UCloudSaveWidget::SaveToCloud()
{
	// Try to save the save file to the cloud storage directory.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString AbsoluteFilePath = CloudStorageUserDirPath + TEXT(CLOUD_SAVE_FILE_NAME);
	IFileHandle* FileHandle = PlatformFile.OpenWrite(*AbsoluteFilePath);
	if (FileHandle)
	{
		// Write the int32's bytes directly to the save file.
		int32* IntPointer = &Counter;
		uint8* ByteBuffer = reinterpret_cast<uint8*>(IntPointer);
		FileHandle->Write(ByteBuffer, sizeof(int32));
		delete FileHandle;
		LoadSaveStatus = TEXT("Saved successfully");
	}
	else
	{
		LoadSaveStatus = TEXT("Save fail (can't write to file)");
	}
}

void UCloudSaveWidget::RequestWriteStoragePermission()
{
	// As described at https://developer.oculus.com/documentation/platform/latest/concepts/dg-cc-cloud-storage/,
	// we need the WRITE_EXTERNAL_STORAGE permission.
#if PLATFORM_ANDROID
	FString PermissionStr = TEXT("android.permission.WRITE_EXTERNAL_STORAGE");
	if (!UAndroidPermissionFunctionLibrary::CheckPermission(PermissionStr))
	{
		TArray<FString> PermsToEnable;
		PermsToEnable.Add(PermissionStr);

		if (UAndroidPermissionCallbackProxy* Callback = UAndroidPermissionFunctionLibrary::AcquirePermissions(PermsToEnable))
		{
			Callback->OnPermissionsGrantedDelegate.BindLambda([this](const TArray<FString>& Permissions, const TArray<bool>& GrantResults)
			{
				if (GrantResults.Num() > 0 && GrantResults[0])
				{
					HasWriteExternalStoragePermission = true;
					GetUserDirPath();
				}
			});
		}
	}
	else
	{
		HasWriteExternalStoragePermission = true;
		GetUserDirPath();
	}
#else
	HasWriteExternalStoragePermission = true;
	GetUserDirPath();
#endif
}

void UCloudSaveWidget::GetUserDirPath()
{
	// Generates a request to retrieve the directory for cloud storage of user data.
	if (!HasAcquiredCloudStorageUserDirPath)
	{
		ovrRequest RequestId = ovr_CloudStorage2_GetUserDirectoryPath();
		FOnlineSubsystemOculus* OSS = static_cast<FOnlineSubsystemOculus*>(IOnlineSubsystem::Get());
		OSS->AddRequestDelegate(RequestId, FOculusMessageOnCompleteDelegate::CreateLambda(
			[this](ovrMessageHandle Message, bool bIsError)
		{
			if (bIsError)
			{
				ovrErrorHandle Error = ovr_Message_GetError(Message);
				CloudStorageUserDirPath = ovr_Error_GetMessage(Error);
			}
			else
			{
				// When the request has sucessfully completed, we store the directory path
				// and mark HasAcquiredCloudStorageUserDirPath as true for the Blueprint UIs
				// to query it.
				CloudStorageUserDirPath = *FString(ovr_Message_GetString(Message));
				HasAcquiredCloudStorageUserDirPath = true;
			}
		}));
	}
}

void UCloudSaveWidget::IncrementCount()
{
	++Counter;
}

void UCloudSaveWidget::DecrementCount()
{
	--Counter;
}
