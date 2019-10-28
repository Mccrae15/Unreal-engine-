#pragma once
#if PLATFORM_PS4
#include <message_dialog.h>

class C3dRudderDialog
{
	bool m_DialogOpen;
	bool m_DialogInitialized;	// module loaded and initialized, dialog does not have to be open
	bool m_DialogNeedsClosing;
	int m_DialogMode;
	SceMsgDialogButtonType  m_UserMessageType;	
	int m_DialogResult;

public:
	C3dRudderDialog();
	~C3dRudderDialog();

	bool IsDialogOpen() const { return m_DialogOpen; }
	bool StartDialogUserMessage(const TCHAR* str);
	bool StartDialogProgressBar(const TCHAR* str);
	bool SetProgressBarPercent(int percent);
	bool SetProgressBarMessage(const TCHAR* str);
	bool CloseDialog();	

	int Update(void);
private:
	void TerminateDialog();
};
#endif