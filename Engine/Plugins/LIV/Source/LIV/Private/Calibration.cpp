#include "Calibration.h"
#include <Engine/Engine.h>
#include <FileManager.h>
#include <FileHelper.h>
#include <Paths.h>

#define CALIBRATION_NAME "liv-camera.cfg"
#define CALIBRATION_LEGACY_NAME "externalcamera.cfg"

ACalibration::ACalibration()
	: IsConfigAvailable(false)
	, LocationOffset(FVector::ZeroVector)
	, RotationOffset(FRotator::ZeroRotator)
	, FOV(60.0f)
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACalibration::BeginPlay()
{
	Super::BeginPlay();

	ReadConfig();
}

bool ACalibration::ReadConfig()
{
	TArray<FString> ConfigLines;

	// In Editor mode, we use the project root.
	// In Standalone mode, we use the package root.
	const auto ConfigDir = GetConfigDir();

	auto ConfigRead = FFileHelper::LoadANSITextFileToStrings(
		*FPaths::Combine(ConfigDir, TEXT(CALIBRATION_NAME)),
		nullptr,
		ConfigLines
	);

	if (!ConfigRead)
	{
		// Fallback to a legacy config name.

		ConfigRead = FFileHelper::LoadANSITextFileToStrings(
			*FPaths::Combine(ConfigDir, TEXT(CALIBRATION_LEGACY_NAME)),
			nullptr,
			ConfigLines
		);
	}

	if (!ConfigRead) {
		IsConfigAvailable = false;
		return IsConfigAvailable;
	}

	for (auto& ConfigLine : ConfigLines)
	{
		ConfigLine = TrimCalibrationLine(ConfigLine);

		const FString Delimiter{ "=" };
		TArray<FString> Tokens;

		if (ConfigLine.ParseIntoArray(Tokens, *Delimiter) != 2)
			continue;

		const auto Parameter = Tokens[0];
		const auto ParameterValue = FCString::Atof(*Tokens[1]);

		if (Parameter.StartsWith("x"))
			LocationOffset.X = ParameterValue;
		else if (Parameter.StartsWith("y"))
			LocationOffset.Y = ParameterValue;
		else if (Parameter.StartsWith("z"))
			LocationOffset.Z = ParameterValue;
		else if (Parameter.StartsWith("rx"))
			RotationOffset.Pitch = ParameterValue;
		else if (Parameter.StartsWith("ry"))
			RotationOffset.Yaw = ParameterValue;
		else if (Parameter.StartsWith("rz"))
			RotationOffset.Roll = ParameterValue;
		else if (Parameter.StartsWith("fov"))
			FOV = ParameterValue;
	}

	IsConfigAvailable = true;
	return IsConfigAvailable;
}

FString ACalibration::TrimCalibrationLine(FString CalibrationLine)
{
#ifdef UE_4_18_OR_LATER
	return CalibrationLine.TrimStartAndEnd();
#else
	return CalibrationLine.Trim().TrimTrailing();
#endif
}

FString ACalibration::GetConfigDir() const
{
#ifdef UE_4_18_OR_LATER
	return GetWorld()->WorldType == EWorldType::Game ? FPaths::LaunchDir() : FPaths::ProjectDir();
#else
	return GetWorld()->WorldType == EWorldType::Game ? FPaths::LaunchDir() : FPaths::GameDir();
#endif
}
