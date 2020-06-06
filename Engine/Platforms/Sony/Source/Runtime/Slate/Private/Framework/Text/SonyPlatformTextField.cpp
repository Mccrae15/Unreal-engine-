// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/SonyPlatformTextField.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include <ime_dialog.h>
#include "SonyApplication.h"
#include "UObject/Class.h"
#include "SlateSettings.h"

FSonyImeDialogMonitor& FSonyImeDialogMonitor::Get()
{
	static FSonyImeDialogMonitor IMEMonitor;
	return IMEMonitor;
}

bool FSonyImeDialogMonitor::Init(TSharedPtr<IVirtualKeyboardEntry>& InTextEntryWidget, int32 InUserIndex, int32 InInputType, int32 InInputOptions, FSonyPlatformTextField* InOwner)
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
	
	SceUserServiceUserId DialogUser = FSonyApplication::GetSonyApplication()->GetUserID(UserIndex);
	if (DialogUser == SCE_USER_SERVICE_USER_ID_INVALID)
	{
		UE_LOG(LogSony, Warning, TEXT("Virtual keyboard couldn't find SceUserServiceId for user:%i"), UserIndex);
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
	DialogParam.maxTextLength = SONY_MAX_INPUT_LENGTH;
	DialogParam.posx = (1920.0f * 0.5f) - (DialogSizeWidth / 2);
	DialogParam.posy = (1080 * 0.5f) - (DialogSizeHeight / 2);
	DialogParam.horizontalAlignment = SCE_IME_HALIGN_LEFT;
	DialogParam.verticalAlignment = SCE_IME_VALIGN_TOP;

	FMemory::Memset(ResultTextBuf, 0);
	const FString& EntryText = InTextEntryWidget->GetText().ToString();
	uint32 StringLen = FMath::Min(SONY_MAX_INPUT_LENGTH, EntryText.Len());
	FMemory::Memcpy(&ResultTextBuf[0], *EntryText, StringLen*sizeof(TCHAR));
	Ret = sceImeDialogInit(&DialogParam, nullptr);
	if (Ret != SCE_OK)
	{
		UE_LOG(LogSony, Warning, TEXT("ERROR: sceImeDialogInit = %08x"), Ret);		
		return false;
	}

	TickerHandle = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FSonyImeDialogMonitor::Tick));
	return true;
}
	
bool FSonyImeDialogMonitor::Tick(float DeltaTime) 
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_FSonyImeDialogMonitor_Tick);

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
				UE_LOG(LogSony, Warning, TEXT("ERROR: sceImeDialogGetResult = %08x"), Ret);
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
				UE_LOG(LogSony, Warning, TEXT("dialogResult.endstatus = %d"), DialogResult.endstatus);
				UE_LOG(LogSony, Warning, TEXT("dialogStatus[SCE_IME_DIALOG_END_STATUS_ABORTED]=%d"), DialogStatus);
			}
		}
	}

	if (DialogStatus == SCE_IME_DIALOG_STATUS_RUNNING || DialogStatus == SCE_IME_DIALOG_STATUS_FINISHED)
	{
		Ret = sceImeDialogTerm();
		if (Ret != SCE_OK)
		{
			UE_LOG(LogSony, Warning, TEXT("ERROR: sceImeDialogTerm = %08x"), Ret);
		}
	}

	FTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	Reset();

	return false;
}

void FSonyImeDialogMonitor::KillExisitingDialog()
{
	bKillDialog = true;
	Tick(0.0f);
	bKillDialog = false;
}

void FSonyPlatformTextField::KillExisitingDialog()
{
	FSonyImeDialogMonitor::Get().KillExisitingDialog();
}

void FSonyPlatformTextField::ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget)
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
		FSonyImeDialogMonitor::Get().Init(TextEntryWidget, UserIndex, InputType, InputOptions, this);
	}
}
