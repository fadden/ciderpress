/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
 /*
 * Support for ActionProgressDialog class.
 */
#include "stdafx.h"
#include "ActionProgressDialog.h"
#include "AddFilesDialog.h"
#include "Main.h"

BEGIN_MESSAGE_MAP(ActionProgressDialog, ProgressCancelDialog)
	//ON_MESSAGE(WMU_START, OnStart)
END_MESSAGE_MAP()

/*
 * Initialize the static text controls to say something reasonable.
 */
BOOL
ActionProgressDialog::OnInitDialog(void)
{
	CDialog::OnInitDialog();

	WMSG1("Action is %d\n", fAction);

	CenterWindow(AfxGetMainWnd());

	CWnd* pWnd;

	// clear the filename fields
	pWnd = GetDlgItem(IDC_PROG_ARC_NAME);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(_T("-"));
	pWnd = GetDlgItem(IDC_PROG_FILE_NAME);
	ASSERT(pWnd != nil);
	pWnd->SetWindowText(_T("-"));

	pWnd->SetFocus();	// get the focus off the Cancel button

	if (fAction == kActionExtract) {
		/* defaults are correct */
	} else if (fAction == kActionRecompress) {
		CString tmpStr;
		pWnd = GetDlgItem(IDC_PROG_VERB);
		ASSERT(pWnd != nil);
		tmpStr.LoadString(IDS_NOW_EXPANDING);
		pWnd->SetWindowText(tmpStr);

		pWnd = GetDlgItem(IDC_PROG_TOFROM);
		ASSERT(pWnd != nil);
		tmpStr.LoadString(IDS_NOW_COMPRESSING);
		pWnd->SetWindowText(tmpStr);
	} else if (fAction == kActionAdd || fAction == kActionAddDisk ||
			fAction == kActionConvFile || fAction == kActionConvDisk)
	{
		CString tmpStr;
		pWnd = GetDlgItem(IDC_PROG_VERB);
		ASSERT(pWnd != nil);
		tmpStr.LoadString(IDS_NOW_ADDING);
		pWnd->SetWindowText(tmpStr);

		pWnd = GetDlgItem(IDC_PROG_TOFROM);
		ASSERT(pWnd != nil);
		tmpStr.LoadString(IDS_ADDING_AS);
		pWnd->SetWindowText(tmpStr);
	} else if (fAction == kActionDelete) {
		CString tmpStr;
		pWnd = GetDlgItem(IDC_PROG_VERB);
		ASSERT(pWnd != nil);
		tmpStr.LoadString(IDS_NOW_DELETING);
		pWnd->SetWindowText(tmpStr);

		pWnd = GetDlgItem(IDC_PROG_TOFROM);
		pWnd->DestroyWindow();
		pWnd = GetDlgItem(IDC_PROG_FILE_NAME);
		ASSERT(pWnd != nil);
		pWnd->SetWindowText(_T(""));
	} else if (fAction == kActionTest) {
		CString tmpStr;
		pWnd = GetDlgItem(IDC_PROG_VERB);
		ASSERT(pWnd != nil);
		tmpStr.LoadString(IDS_NOW_TESTING);
		pWnd->SetWindowText(tmpStr);

		pWnd = GetDlgItem(IDC_PROG_TOFROM);
		pWnd->DestroyWindow();
		pWnd = GetDlgItem(IDC_PROG_FILE_NAME);
		ASSERT(pWnd != nil);
		pWnd->SetWindowText(_T(""));
	} else {
		ASSERT(false);
	}

	return FALSE;
}

/*
 * Set the name of the file as it appears in the archive.
 */
void
ActionProgressDialog::SetArcName(const char* str)
{
	CString oldStr;

	CWnd* pWnd = GetDlgItem(IDC_PROG_ARC_NAME);
	ASSERT(pWnd != nil);
	pWnd->GetWindowText(oldStr);
	if (oldStr != str)
		pWnd->SetWindowText(str);
}

const CString
ActionProgressDialog::GetFileName(void)
{
	CString str;

	CWnd* pWnd = GetDlgItem(IDC_PROG_FILE_NAME);
	ASSERT(pWnd != nil);
	pWnd->GetWindowText(str);

	return str;
}

/*
 * Set the name of the file as it appears under Windows.
 */
void
ActionProgressDialog::SetFileName(const char* str)
{
	CString oldStr;

	CWnd* pWnd = GetDlgItem(IDC_PROG_FILE_NAME);
	ASSERT(pWnd != nil);
	pWnd->GetWindowText(oldStr);
	if (oldStr != str)
		pWnd->SetWindowText(str);
}

/*
 * Update the progress meter.
 *
 * We take a percentage, but the underlying control uses 1000ths.
 */
int
ActionProgressDialog::SetProgress(int perc)
{
	ASSERT(perc >= 0 && perc <= 100);
	MainWindow* pMainWin = (MainWindow*)::AfxGetMainWnd();

	/* solicit input */
	pMainWin->PeekAndPump();

	return ProgressCancelDialog::SetProgress(perc *
									(kProgressResolution/100));
}
