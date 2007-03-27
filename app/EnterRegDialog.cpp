/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#if 0
/*
 * Support for entering registration data.
 */
#include "stdafx.h"
#include "EnterRegDialog.h"
#include "MyApp.h"
#include "HelpTopics.h"

BEGIN_MESSAGE_MAP(EnterRegDialog, CDialog)
	ON_EN_CHANGE(IDC_REGENTER_USER, OnUserChange)
	ON_EN_CHANGE(IDC_REGENTER_COMPANY, OnCompanyChange)
	ON_EN_CHANGE(IDC_REGENTER_REG, OnRegChange)
	ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()


/*
 * Disable the "OK" button initially.
 */
BOOL
EnterRegDialog::OnInitDialog(void)
{
	//CWnd* pWnd = GetDlgItem(IDOK);
	//ASSERT(pWnd != nil);
	//pWnd->EnableWindow(false);

	fMyEdit.ReplaceDlgCtrl(this, IDC_REGENTER_REG);
	fMyEdit.SetProperties(MyEdit::kCapsOnly | MyEdit::kNoWhiteSpace);

	/* place a reasonable cap on the field lengths, since these go
	   straight into the registry */
	CEdit* pEdit;
	pEdit = (CEdit*) GetDlgItem(IDC_REGENTER_USER);
	ASSERT(pEdit != nil);
	pEdit->SetLimitText(120);
	pEdit = (CEdit*) GetDlgItem(IDC_REGENTER_COMPANY);
	ASSERT(pEdit != nil);
	pEdit->SetLimitText(120);
	pEdit = (CEdit*) GetDlgItem(IDC_REGENTER_REG);
	ASSERT(pEdit != nil);
	pEdit->SetLimitText(40);

	return CDialog::OnInitDialog();
}

/*
 * Shuffle data in and out of the edit fields.  We do an extra validation
 * step on the registration key before accepting it.
 */
void
EnterRegDialog::DoDataExchange(CDataExchange* pDX)
{
	DDX_Text(pDX, IDC_REGENTER_USER, fUserName);
	DDX_Text(pDX, IDC_REGENTER_COMPANY, fCompanyName);
	DDX_Text(pDX, IDC_REGENTER_REG, fRegKey);

	/* validate the reg field */
	if (pDX->m_bSaveAndValidate) {
		ASSERT(!fUserName.IsEmpty());
		ASSERT(!fRegKey.IsEmpty());

		if (gMyApp.fRegistry.IsValidRegistrationKey(fUserName, fCompanyName,
				fRegKey))
		{
			WMSG3("Correct key entered: '%s' '%s' '%s'\n",
				(LPCTSTR)fUserName, (LPCTSTR)fCompanyName, (LPCTSTR)fRegKey);
		} else {
			WMSG0("Incorrect key entered, rejecting\n");
			CString appName, msg;
			appName.LoadString(IDS_MB_APP_NAME);
			msg.LoadString(IDS_REG_BAD_ENTRY);
			MessageBox(msg, appName, MB_ICONWARNING|MB_OK);
			pDX->Fail();
		}
	} else {
		OnUserChange();
		OnCompanyChange();
		OnRegChange();
	}
}

/*
 * Call this when the text in an edit field has changed.
 *
 * If there's nothing in the "user name" or "reg key" fields, dim the OK
 * button.
 */
void
EnterRegDialog::HandleEditChange(int editID, int crcID)
{
	CString userStr, regStr;
	CEdit* pEdit;
	CWnd* pWnd;

	/*
	 * Update the CRC for the modified control.
	 */
	pEdit = (CEdit*) GetDlgItem(editID);
	ASSERT(pEdit != nil);
	pEdit->GetWindowText(userStr);
	unsigned short crc;
	crc = gMyApp.fRegistry.ComputeStringCRC(userStr);
	userStr.Format("%04X", crc);
	pWnd = GetDlgItem(crcID);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(userStr);

	/*
	 * Update the OK button.
	 */
	pEdit = (CEdit*) GetDlgItem(IDC_REGENTER_USER);
	ASSERT(pEdit != nil);
	pEdit->GetWindowText(userStr);

	pEdit = (CEdit*) GetDlgItem(IDC_REGENTER_REG);
	ASSERT(pEdit != nil);
	pEdit->GetWindowText(regStr);

	pWnd = GetDlgItem(IDOK);
	ASSERT(pWnd != nil);
	pWnd->EnableWindow(!userStr.IsEmpty() && !regStr.IsEmpty());
}

/*
 * Handle changes in the three edit fields.
 */
void
EnterRegDialog::OnUserChange(void)
{
	HandleEditChange(IDC_REGENTER_USER, IDC_REGENTER_USERCRC);
}
void
EnterRegDialog::OnCompanyChange(void)
{
	HandleEditChange(IDC_REGENTER_COMPANY, IDC_REGENTER_COMPCRC);
}
void
EnterRegDialog::OnRegChange(void)
{
	HandleEditChange(IDC_REGENTER_REG, IDC_REGENTER_REGCRC);
}


/*
 * User pressed the "Help" button.
 */
void
EnterRegDialog::OnHelp(void)
{
	WinHelp(HELP_TOPIC_ENTER_REG_DATA, HELP_CONTEXT);
}


/*
 * Get registration info from the user.  This is a static utility function
 * that can be called from elsewhere in the app.
 *
 * Returns 0 on successful registration, nonzero on failure or if the user
 * cancels out of the dialog.
 */
/*static*/ int
EnterRegDialog::GetRegInfo(CWnd* pWnd)
{
	CString user, company, reg, versions, expire;

	/*
	 * Get current data (if any).  This call only fails if the registry itself
	 * appears to be generally inaccessible.
	 */
	if (gMyApp.fRegistry.GetRegistration(&user, &company, &reg, &versions,
			&expire) != 0)
	{
		CString msg;
		msg.LoadString(IDS_REG_FAILURE);
		ShowFailureMsg(pWnd, msg, IDS_FAILED);
		return -1;
	}

	/*
	 * Post the dialog.
	 */
	EnterRegDialog dlg(pWnd);
	int result = -1;

	if (dlg.DoModal() == IDOK) {
		user = dlg.fUserName;
		company = dlg.fCompanyName;
		reg = dlg.fRegKey;

		/* data was validated by EnterRegDialog, so just save it to registry */
		if (gMyApp.fRegistry.SetRegistration(user, company, reg, versions,
			expire) != 0)
		{
			CString msg;
			msg.LoadString(IDS_REG_FAILURE);
			ShowFailureMsg(pWnd, msg, IDS_FAILED);
		} else {
			result = 0;
		}
	}

	return result;
}

#endif /*0*/