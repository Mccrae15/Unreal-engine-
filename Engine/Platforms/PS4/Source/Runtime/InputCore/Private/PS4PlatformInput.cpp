// Copyright Epic Games, Inc. All Rights Reserved.

#include "PS4PlatformInput.h"
#include <system_service.h>

FKey FPS4PlatformInput::GetGamepadAcceptKey()
{
	int32 GetButtonAssignment;
	int32 Ret = sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN, &GetButtonAssignment);
	checkf(Ret == SCE_OK, TEXT("sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN) failed: 0x%x"), Ret);

	return GetButtonAssignment == SCE_SYSTEM_PARAM_ENTER_BUTTON_ASSIGN_CIRCLE ? EKeys::Gamepad_FaceButton_Right : EKeys::Gamepad_FaceButton_Bottom;
}

FKey FPS4PlatformInput::GetGamepadBackKey()
{
	int32 GetButtonAssignment;
	int32 Ret = sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN, &GetButtonAssignment);
	checkf(Ret == SCE_OK, TEXT("sceSystemServiceParamGetInt(SCE_SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN) failed: 0x%x"), Ret);

	return GetButtonAssignment == SCE_SYSTEM_PARAM_ENTER_BUTTON_ASSIGN_CIRCLE ? EKeys::Gamepad_FaceButton_Bottom : EKeys::Gamepad_FaceButton_Right;
}
