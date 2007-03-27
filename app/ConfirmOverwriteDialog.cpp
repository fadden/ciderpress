/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for ConfirmOverwriteDialog and RenameOverwriteDialog classes.
 */
#include "stdafx.h"
#include "ConfirmOverwriteDialog.h"
#include "GenericArchive.h"
#include <time.h>


/*
 * ==========================================================================
 *		RenameOverwriteDialog
 * ==========================================================================
 */

BEGIN_MESSAGE_MAP(RenameOverwriteDialog, CDialog)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()

/*
 * Init static text fields.
 */
BOOL
RenameOverwriteDialog::OnInitDialog(void)
{
	CWnd* pWnd;

	pWnd = GetDlgItem(IDC_RENOVWR_SOURCE_NAME);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(fNewFileSource);

	return CDialog::OnInitDialog();
}

/*
 * Convert values.
 */
void
RenameOverwriteDialog::DoDataExchange(CDataExchange* pDX)
{
	DDX_Text(pDX, IDC_RENOVWR_ORIG_NAME, fExistingFile);
	DDX_Text(pDX, IDC_RENOVWR_NEW_NAME, fNewName);

	/* validate the path field */
	if (pDX->m_bSaveAndValidate) {
		if (fNewName.IsEmpty()) {
			MessageBox("You must specify a new name.",
				"CiderPress", MB_OK);
			pDX->Fail();
		}

		// we *could* try to validate the path here...
	}
}

BOOL
RenameOverwriteDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
	WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
	return TRUE;	// yes, we handled it
}


/*
 * ==========================================================================
 *		ConfirmOverwriteDialog
 * ==========================================================================
 */

BEGIN_MESSAGE_MAP(ConfirmOverwriteDialog, CDialog)
	ON_BN_CLICKED(IDC_OVWR_YES, OnYes)
	ON_BN_CLICKED(IDC_OVWR_YESALL, OnYesToAll)
	ON_BN_CLICKED(IDC_OVWR_NO, OnNo)
	ON_BN_CLICKED(IDC_OVWR_NOALL, OnNoToAll)
	ON_BN_CLICKED(IDC_OVWR_RENAME, OnRename)
	ON_WM_HELPINFO()
END_MESSAGE_MAP()


/*
 * Replace some static text fields.
 */
BOOL
ConfirmOverwriteDialog::OnInitDialog(void)
{
	CWnd* pWnd;
	CString tmpStr, dateStr;

	pWnd = GetDlgItem(IDC_OVWR_EXIST_NAME);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(fExistingFile);

	pWnd = GetDlgItem(IDC_OVWR_EXIST_INFO);
	ASSERT(pWnd != nil);
	FormatDate(fExistingFileModWhen, &dateStr);
	tmpStr.Format("Modified %s", dateStr);
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_OVWR_NEW_NAME);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(fNewFileSource);

	pWnd = GetDlgItem(IDC_OVWR_NEW_INFO);
	ASSERT(pWnd != nil);
	FormatDate(fNewFileModWhen, &dateStr);
	tmpStr.Format("Modified %s", dateStr);
	pWnd->SetWindowText(tmpStr);

	pWnd = GetDlgItem(IDC_OVWR_RENAME);
	ASSERT(pWnd != nil);
	pWnd->EnableWindow(fAllowRename);

	return CDialog::OnInitDialog();
}

/*
 * Handle a click on the question-mark button.
 */
BOOL
ConfirmOverwriteDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
	WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
	return TRUE;	// yes, we handled it
}

/*
 * One of the buttons was hit.
 */
void
ConfirmOverwriteDialog::OnYes(void)
{
	fResultOverwrite = true;
	CDialog::OnOK();
}
void
ConfirmOverwriteDialog::OnYesToAll(void)
{
	fResultOverwrite = true;
	fResultApplyToAll = true;
	CDialog::OnOK();
}
void
ConfirmOverwriteDialog::OnNo(void)
{
	//fResultOverwrite = false;
	CDialog::OnOK();
}
void
ConfirmOverwriteDialog::OnNoToAll(void)
{
	//fResultOverwrite = true;
	fResultApplyToAll = true;
	CDialog::OnOK();
}
void
ConfirmOverwriteDialog::OnRename(void)
{
	RenameOverwriteDialog dlg;

	dlg.fNewFileSource = fNewFileSource;
	dlg.fExistingFile = fExistingFile;
	dlg.fNewName = fExistingFile;
	if (dlg.DoModal() == IDOK) {
		fExistingFile = dlg.fNewName;
		fResultRename = true;
		CDialog::OnOK();
	}
}
