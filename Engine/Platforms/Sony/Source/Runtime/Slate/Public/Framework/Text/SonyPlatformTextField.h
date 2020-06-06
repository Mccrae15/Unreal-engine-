// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Framework/Application/IPlatformTextField.h"

class FSonyPlatformTextField;
class FSonyImeDialogMonitor
{
public:

	static FSonyImeDialogMonitor& Get();

	

	~FSonyImeDialogMonitor()
	{
		
	}

	bool Init(TSharedPtr<IVirtualKeyboardEntry>& InTextEntryWidget, int32 InUserIndex, int32 InInputType, int32 InInputOptions, FSonyPlatformTextField* InOwner);
	bool Tick(float DeltaTime); 
	void KillExisitingDialog();

private:

	FSonyImeDialogMonitor()
	{
		Reset();
	}

	void Reset()
	{		
		UserIndex = -1;
		InputType = -1;
		InputOptions = -1;
		TextEntryWidget = nullptr;
		Owner = nullptr;
		bKillDialog = false;
		TickerHandle.Reset();
	}

	static const int32 SONY_MAX_INPUT_LENGTH = 2047;
	TCHAR  ResultTextBuf[SONY_MAX_INPUT_LENGTH + 1];

	FDelegateHandle TickerHandle;
	int32 UserIndex;
	int32 InputType;
	int32 InputOptions;
	TWeakPtr<IVirtualKeyboardEntry> TextEntryWidget;
	FSonyPlatformTextField* Owner;
	bool bKillDialog;
};

class FRunnableThread;
class FSonyPlatformTextField : public IPlatformTextField
{
public:
	FSonyPlatformTextField()
	{		
	}

	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override;

private:

	void KillExisitingDialog();
		
	//	SlateTextField* TextField;
};