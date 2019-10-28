#include "3dRudderDialog.h"
#if PLATFORM_PS4
#include <libsysmodule.h>

//C3dRudderDialog gDialog;

C3dRudderDialog::C3dRudderDialog()
	: m_DialogOpen(false)
	, m_DialogInitialized(false)
	, m_DialogNeedsClosing(false)
	, m_DialogMode(SCE_MSG_DIALOG_MODE_INVALID)
	, m_UserMessageType(SCE_MSG_DIALOG_BUTTON_TYPE_NONE)		
{
}


C3dRudderDialog::~C3dRudderDialog()
{
}


bool C3dRudderDialog::CloseDialog()
{
	if (!m_DialogOpen)
	{
		//UE_LOG(LogPS4, Warning, "CommonDialog is not open");
		return false;
	}

	m_DialogNeedsClosing = true;

	return true;
}

void C3dRudderDialog::TerminateDialog()
{
	int32_t ret = sceMsgDialogTerminate();
	if (ret < 0)
	{
		//Messages::LogError("CommonDialog::%s::sceMsgDialogTerminate()::@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(ret));
	}
	m_DialogInitialized = false;
}

bool C3dRudderDialog::StartDialogUserMessage(const TCHAR* str)
{
	if (m_DialogOpen)
	{
		//Messages::Log("CommonDialog is already open\n");
		return false;
	}

	int32_t ret = sceSysmoduleIsLoaded(SCE_SYSMODULE_MESSAGE_DIALOG);
	if (ret != SCE_OK)
	{
		if (ret == SCE_SYSMODULE_ERROR_UNLOADED)
		{
			ret = sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
		}

		if (ret != SCE_OK)
		{
			//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(ret));
			return false;
		}
	}

	ret = sceMsgDialogInitialize();
	if (ret != SCE_OK)
	{
		//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(ret));
		return false;
	}
	m_DialogInitialized = true;
	SceMsgDialogParam msgParam;
	SceMsgDialogUserMessageParam userMsgParam;

	// initialize parameter of message dialog
	sceMsgDialogParamInitialize(&msgParam);
	msgParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	m_DialogMode = msgParam.mode;	

	// initialize message dialog
	memset(&userMsgParam, 0, sizeof(SceMsgDialogUserMessageParam));
	msgParam.userMsgParam = &userMsgParam;
	auto AnsiString = StringCast<ANSICHAR>(str);
	msgParam.userMsgParam->msg = AnsiString.Get();
	msgParam.userMsgParam->buttonType = m_UserMessageType;
	//m_UserMessageType = m_UserMessageType;


	ret = sceMsgDialogOpen(&msgParam);
	if (ret < 0)
	{
		//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(ret));
		TerminateDialog();

		return false;
	}

	m_DialogOpen = true;

	return true;
}

bool C3dRudderDialog::StartDialogProgressBar(const TCHAR *str)
{
	if (m_DialogOpen)
	{
		//Messages::Log("CommonDialog is already open\n");
		return false;
	}	

	int32_t ret = sceSysmoduleIsLoaded(SCE_SYSMODULE_MESSAGE_DIALOG);
	if (ret != SCE_OK)
	{
		if (ret == SCE_SYSMODULE_ERROR_UNLOADED)
		{
			ret = sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
		}

		if (ret != SCE_OK)
		{
			//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(ret));
			return false;
		}
	}

	ret = sceMsgDialogInitialize();
	if (ret != SCE_OK)
	{
		//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(ret));
		return false;
	}
	m_DialogInitialized = true;

	SceMsgDialogParam msgParam;
	SceMsgDialogProgressBarParam progBarParam;

	// initialize parameter of message dialog
	sceMsgDialogParamInitialize(&msgParam);
	msgParam.mode = SCE_MSG_DIALOG_MODE_PROGRESS_BAR;
	m_DialogMode = msgParam.mode;

	// initialize message dialog
	memset(&progBarParam, 0, sizeof(SceMsgDialogProgressBarParam));
	msgParam.progBarParam = &progBarParam;
	msgParam.progBarParam->barType = SCE_MSG_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE;
	auto AnsiString = StringCast<ANSICHAR>(str);
	msgParam.progBarParam->msg = AnsiString.Get();


	ret = sceMsgDialogOpen(&msgParam);
	if (ret < 0)
	{
		//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(ret));
		sceMsgDialogTerminate();
		return false;
	}

	m_DialogOpen = true;

	return true;
}

bool C3dRudderDialog::SetProgressBarPercent(int percent)
{
	if (!m_DialogOpen || m_DialogMode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR)
	{
		//Messages::Log("ProgressBar dialog is not open\n");
		return false;
	}

	int ret = sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, percent);
	if (ret < 0)
	{
		//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(ret));
		return false;
	}

	return true;
}

bool C3dRudderDialog::SetProgressBarMessage(const TCHAR* str)
{
	if (!m_DialogOpen || m_DialogMode != SCE_MSG_DIALOG_MODE_PROGRESS_BAR)
	{
		//Messages::Log("ProgressBar dialog is not open\n");
		return false;
	}
	auto AnsiString = StringCast<ANSICHAR>(str);
	int ret = sceMsgDialogProgressBarSetMsg(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, AnsiString.Get());
	if (ret < 0)
	{
		//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(ret));
		return false;
	}

	return true;
}

int C3dRudderDialog::Update(void)
{
	SceCommonDialogStatus	cdStatus;
	SceMsgDialogResult		msgResult;

	int		res = SCE_OK;
	int		term_res = SCE_OK;

	if (m_DialogInitialized == false) return 0;

	// Get message dialog status
	cdStatus = sceMsgDialogUpdateStatus();

	if (m_DialogNeedsClosing && cdStatus == SCE_COMMON_DIALOG_STATUS_RUNNING)
	{
		// terminate message dialog
		int ret = sceMsgDialogClose();
		if (ret != SCE_OK)
		{
			//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(ret));
		}
		else
		{
			m_DialogNeedsClosing = false;
		}
	}

	switch (cdStatus)
	{
	default:
	case SCE_COMMON_DIALOG_STATUS_NONE:
	case SCE_COMMON_DIALOG_STATUS_RUNNING:
		return cdStatus;

	case SCE_COMMON_DIALOG_STATUS_FINISHED:
		break;
	}

	// Get message dialog result
	memset(&msgResult, 0, sizeof(SceMsgDialogResult));
	res = sceMsgDialogGetResult(&msgResult);	// returns msgResult.result
	if (res < 0)	// values >=0 are fine
	{
		//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(res));
	}


	// Terminate message dialog
	term_res = sceMsgDialogTerminate();
	if (term_res != SCE_OK)
	{
		//Messages::LogError("CommonDialog::%s@L%d - %s", __FUNCTION__, __LINE__, LookupErrorCode(term_res));
		res = term_res;
	}

	m_DialogInitialized = false;

	m_DialogOpen = false;
	return res;
}
#endif