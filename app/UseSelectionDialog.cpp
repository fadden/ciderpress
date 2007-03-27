/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for UseSelectionDialog.
 */
#include "stdafx.h"
#include "UseSelectionDialog.h"
#include "HelpTopics.h"

BEGIN_MESSAGE_MAP(UseSelectionDialog, CDialog)
	ON_WM_HELPINFO()
	//ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()


/*
 * Set up the dialog that lets the user choose file deletion options.
 *
 * All we really need to do is update the string that indicates how many
 * files have been selected.
 */
BOOL
UseSelectionDialog::OnInitDialog(void)
{
	CString str;
	CString selStr;
	CWnd* pWnd;

	CDialog::OnInitDialog();

	/* grab the radio button with the selection count */
	pWnd = GetDlgItem(IDC_USE_SELECTED);
	ASSERT(pWnd != nil);

	/* set the string using a string table entry */
	if (fSelectedCount == 1) {
		str.LoadString(fSelCountID);
		pWnd->SetWindowText(str);
	} else {
		str.LoadString(fSelCountsID);
		selStr.Format((LPCTSTR) str, fSelectedCount);
		pWnd->SetWindowText(selStr);

		if (fSelectedCount == 0)
			pWnd->EnableWindow(FALSE);
	}

	/* set the other strings */
	str.LoadString(fTitleID);
	SetWindowText(str);

	pWnd = GetDlgItem(IDC_USE_ALL);
	ASSERT(pWnd != nil);
	str.LoadString(fAllID);
	pWnd->SetWindowText(str);

	pWnd = GetDlgItem(IDOK);
	ASSERT(pWnd != nil);
	str.LoadString(fOkLabelID);
	pWnd->SetWindowText(str);

	return TRUE;
}

/*
 * Convert values.
 */
void
UseSelectionDialog::DoDataExchange(CDataExchange* pDX)
{
	DDX_Radio(pDX, IDC_USE_SELECTED, fFilesToAction);
}

/*
 * Context help request (question mark button).
 */
BOOL
UseSelectionDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
	WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
	return TRUE;	// yes, we handled it
}
