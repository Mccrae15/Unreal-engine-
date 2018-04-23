// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/PS4/PS4PlatformTextField.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include <ime_dialog.h>
#include "PS4Application.h"
#include "Class.h"
#include "SlateSettings.h"

FPS4ImeDialogMonitor& FPS4ImeDialogMonitor::Get()
{
	static FPS4ImeDialogMonitor IMEMonitor;
	return IMEMonitor;
}

bool FPS4ImeDialogMonitor::Init(TSharedPtr<IVirtualKeyboardEntry>& InTextEntryWidget, int32 InUserIndex, int32 InInputType, int32 InInputOptions, FPS4PlatformTextField* InOwner)
{
	TextEntryWidget = InTextEntryWidget;
	UserIndex = InUserIndex;
	InputType = InInputType;
	InputOptions = InInputOptions;
	Owner = InOwner;

	if (!TextEntryWidget.IsValid())
	{
		return false;
	}
	
	SceUserServiceUserId DialogUser = FPS4Application::GetPS4Application()->GetUserID(UserIndex);
	if (DialogUser == SCE_USER_SERVICE_USER_ID_INVALID)
	{
		UE_LOG(LogPS4, Warning, TEXT("Virtual keyboard couldn't find SceUserServiceId for user:%i"), UserIndex);
		return false;
	}

	SceImeDialogParam				DialogParam;
	uint32 DialogSizeWidth;
	uint32 DialogSizeHeight;
	sceImeDialogParamInit(&DialogParam);
	int32 Ret = sceImeDialogGetPanelSizeExtended(&DialogParam, nullptr, &DialogSizeWidth, &DialogSizeHeight);

	DialogParam.userId = DialogUser;
	DialogParam.option = InputOptions;
	DialogParam.supportedLanguages = 0;
	DialogParam.type = (SceImeType)InputType;
	DialogParam.inputTextBuffer = ResultTextBuf;
	DialogParam.maxTextLength = PS4_MAX_INPUT_LENGTH;
	DialogParam.posx = (1920.0f * 0.5f) - (DialogSizeWidth / 2);
	DialogParam.posy = (1080 * 0.5f) - (DialogSizeHeight / 2);
	DialogParam.horizontalAlignment = SCE_IME_HALIGN_LEFT;
	DialogParam.verticalAlignment = SCE_IME_VALIGN_TOP;

	FMemory::Memset(ResultTextBuf, 0);
	const FString& EntryText = InTextEntryWidget->GetText().ToString();
	uint32 StringLen = FMath::Min(PS4_MAX_INPUT_LENGTH, EntryText.Len());
	FMemory::Memcpy(&ResultTextBuf[0], *EntryText, StringLen*sizeof(TCHAR));
	Ret = sceImeDialogInit(&DialogParam, nullptr);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogPS4, Warning, TEXT("ERROR: sceImeDialogInit = %08x"), Ret);		
		return false;
	}

	TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FPS4ImeDialogMonitor::Tick));
	return true;
}
	
bool FPS4ImeDialogMonitor::Tick(float DeltaTime) 
{
	check(IsInGameThread());	

	SceImeDialogStatus	DialogStatus;
	SceImeDialogResult	DialogResult;

	int32 Ret;

	DialogStatus = sceImeDialogGetStatus();
	if(TextEntryWidget.IsValid())
	{
		TSharedPtr<IVirtualKeyboardEntry> TextEntryWidgetPin = TextEntryWidget.Pin();
		if(DialogStatus == SCE_IME_DIALOG_STATUS_NONE)
		{
			FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			Reset();
			return false;
		}

		if(DialogStatus == SCE_IME_DIALOG_STATUS_FINISHED && bKillDialog)
		{
			return true;
		}

		if(DialogStatus == SCE_IME_DIALOG_STATUS_RUNNING && !bKillDialog)
		{
			return true;
		}

		if(DialogStatus == SCE_IME_DIALOG_STATUS_RUNNING && bKillDialog)
		{
			// HACK: Make sure we are not in the middle of finishing
			FPlatformProcess::Sleep(0.2f);
			DialogStatus = sceImeDialogGetStatus();
			if(DialogStatus == SCE_IME_DIALOG_STATUS_FINISHED)
			{
				return true;
			}

			// Forcing a shutdown so abort the dialog and wait unit it has finished closing
			sceImeDialogAbort();
			DialogStatus = sceImeDialogGetStatus();
			while(DialogStatus == SCE_IME_DIALOG_STATUS_RUNNING)
			{
				FPlatformProcess::Sleep(0.1f);
				DialogStatus = sceImeDialogGetStatus();
			}
		}

		if(!bKillDialog)
		{
			// get ime dialog result
			FMemory::Memset(&DialogResult, 0, sizeof(SceImeDialogResult));
			Ret = sceImeDialogGetResult(&DialogResult);
			if(Ret != SCE_OK)
			{
				UE_LOG(LogPS4, Warning, TEXT("ERROR: sceImeDialogGetResult = %08x"), Ret);
			}

			if(DialogResult.endstatus == SCE_IME_DIALOG_END_STATUS_OK)
			{
				TextEntryWidgetPin->SetTextFromVirtualKeyboard(FText::FromString(ResultTextBuf), ETextEntryType::TextEntryAccepted);
			}

			if(DialogResult.endstatus == SCE_IME_DIALOG_END_STATUS_USER_CANCELED)
			{
				TextEntryWidgetPin->SetTextFromVirtualKeyboard(FText::FromString(ResultTextBuf), ETextEntryType::TextEntryCanceled);
			}

			if(DialogResult.endstatus == SCE_IME_DIALOG_END_STATUS_ABORTED)
			{
				UE_LOG(LogPS4, Warning, TEXT("dialogResult.endstatus = %d"), DialogResult.endstatus);
				UE_LOG(LogPS4, Warning, TEXT("dialogStatus[SCE_IME_DIALOG_END_STATUS_ABORTED]=%d"), DialogStatus);
			}
		}
	}

	if (DialogStatus == SCE_IME_DIALOG_STATUS_RUNNING || DialogStatus == SCE_IME_DIALOG_STATUS_FINISHED)
	{
		Ret = sceImeDialogTerm();
		if (Ret != SCE_OK)
		{
			UE_LOG(LogPS4, Warning, TEXT("ERROR: sceImeDialogTerm = %08x"), Ret);
		}
	}

	FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	Reset();

	return false;
}

void FPS4ImeDialogMonitor::KillExisitingDialog()
{
	bKillDialog = true;
	Tick(0.0f);
	bKillDialog = false;
}

void FPS4PlatformTextField::KillExisitingDialog()
{
	FPS4ImeDialogMonitor::Get().KillExisitingDialog();
}

void FPS4PlatformTextField::ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget)
{
	KillExisitingDialog();
	
	if (bShow)
	{		
		// Set the EditBox inputType based on keyboard type
		int32 InputType = SCE_IME_TYPE_DEFAULT;
		int32 InputOptions = SCE_IME_OPTION_DEFAULT;;
		switch (TextEntryWidget->GetVirtualKeyboardType())
		{
		case EKeyboardType::Keyboard_Number:
			InputType = SCE_IME_TYPE_NUMBER;
			break;
		case EKeyboardType::Keyboard_Web:
			InputType = SCE_IME_TYPE_URL;
			break;
		case EKeyboardType::Keyboard_Email:
			InputType = SCE_IME_TYPE_MAIL;
			InputOptions = SCE_IME_OPTION_NO_AUTO_CAPITALIZATION;
			break;
		case EKeyboardType::Keyboard_Password:
			InputType = SCE_IME_TYPE_BASIC_LATIN;
			InputOptions = SCE_IME_OPTION_PASSWORD | SCE_IME_OPTION_NO_AUTO_CAPITALIZATION | SCE_IME_OPTION_NO_LEARNING;
			break;
		case EKeyboardType::Keyboard_AlphaNumeric:
			InputType = SCE_IME_TYPE_BASIC_LATIN;
			break;
		case EKeyboardType::Keyboard_Default:
		default:
			InputType = SCE_IME_TYPE_DEFAULT;
			break;
		}
		FPS4ImeDialogMonitor::Get().Init(TextEntryWidget, UserIndex, InputType, InputOptions, this);
	}
}
