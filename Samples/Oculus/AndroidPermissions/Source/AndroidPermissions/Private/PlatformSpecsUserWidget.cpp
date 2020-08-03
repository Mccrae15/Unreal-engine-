#include "PlatformSpecsUserWidget.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#if PLATFORM_ANDROID
#include "Android/AndroidPlatformMisc.h"
#endif
#include "CoreMinimal.h"

void UPlatformSpecsUserWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{	
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update dynamic states
#if PLATFORM_ANDROID
	FAndroidMisc::FBatteryState BatteryState = FAndroidMisc::GetBatteryState();
	BatteryIsCharging = BatteryState.State == FAndroidMisc::BATTERY_STATE_CHARGING;
	BatteryLevel = BatteryState.Level;
	BatteryTemp = BatteryState.Temperature;
	CurVolume = FAndroidMisc::GetVolumeState();
#else
	BatteryIsCharging = true;
	BatteryLevel = 0;
	BatteryTemp = 0;
	CurVolume = 0;
#endif
}

FString UPlatformSpecsUserWidget::GetAndroidVersion()
{
#if PLATFORM_ANDROID
	return FAndroidMisc::GetAndroidVersion();
#else
	return TEXT("");
#endif
}

FString UPlatformSpecsUserWidget::GetDeviceMake()
{
#if PLATFORM_ANDROID
	return FAndroidMisc::GetDeviceMake();
#else
	return TEXT("");
#endif
}

FString UPlatformSpecsUserWidget::GetDeviceModel()
{
#if PLATFORM_ANDROID
	return FAndroidMisc::GetDeviceModel();
#else
	return TEXT("");
#endif
}
