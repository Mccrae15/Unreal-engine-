// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "IPlatformTextField.h"

class FPS4PlatformTextField;
class FPS4ImeDialogMonitor
{
public:

	static FPS4ImeDialogMonitor& Get();

	

	~FPS4ImeDialogMonitor()
	{
		
	}

	bool Init(TSharedPtr<IVirtualKeyboardEntry>& InTextEntryWidget, int32 InUserIndex, int32 InInputType, int32 InInputOptions, FPS4PlatformTextField* InOwner);
	bool Tick(float DeltaTime); 
	void KillExisitingDialog();

private:

	FPS4ImeDialogMonitor()
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

	static const int32 PS4_MAX_INPUT_LENGTH = 2047;
	TCHAR  ResultTextBuf[PS4_MAX_INPUT_LENGTH + 1];

	FDelegateHandle TickerHandle;
	int32 UserIndex;
	int32 InputType;
	int32 InputOptions;
	TWeakPtr<IVirtualKeyboardEntry> TextEntryWidget;
	FPS4PlatformTextField* Owner;
	bool bKillDialog;
};

class FRunnableThread;
class FPS4PlatformTextField : public IPlatformTextField
{
public:
	FPS4PlatformTextField()
	{		
	}

	virtual void ShowVirtualKeyboard(bool bShow, int32 UserIndex, TSharedPtr<IVirtualKeyboardEntry> TextEntryWidget) override;

private:

	void KillExisitingDialog();
		
	//	SlateTextField* TextField;
};

typedef FPS4PlatformTextField FPlatformTextField;

