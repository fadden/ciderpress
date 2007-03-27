/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for the "add files" dialog.
 */
#include "stdafx.h"
#include "AddFilesDialog.h"
#include "FileNameConv.h"
#include "HelpTopics.h"
#include "resource.h"


/*
 * A lot like DoDataExchange, only different.
 *
 * We do some OnInitDialog-type stuff in here, because we're a subclass of
 * SelectFilesDialog and don't really get to have one of those.
 *
 * Returns "true" if all is well, "false" if something failed.  Usually a
 * "false" indication occurs during saveAndValidate==true, and means that we
 * shouldn't allow the dialog to close yet.
 */
bool
AddFilesDialog::MyDataExchange(bool saveAndValidate)
{
	CWnd* pWnd;

	if (saveAndValidate) {
		if (GetDlgButtonCheck(this, IDC_ADDFILES_NOPRESERVE) == BST_CHECKED)
			fTypePreservation = kPreserveNone;
		else if (GetDlgButtonCheck(this, IDC_ADDFILES_PRESERVE) == BST_CHECKED)
			fTypePreservation = kPreserveTypes;
		else if (GetDlgButtonCheck(this, IDC_ADDFILES_PRESERVEPLUS) == BST_CHECKED)
			fTypePreservation = kPreserveAndExtend;
		else {
			ASSERT(false);
			fTypePreservation = kPreserveNone;
		}

		if (GetDlgButtonCheck(this, IDC_ADDFILES_CONVEOLNONE) == BST_CHECKED)
			fConvEOL = kConvEOLNone;
		else if (GetDlgButtonCheck(this, IDC_ADDFILES_CONVEOLTYPE) == BST_CHECKED)
			fConvEOL = kConvEOLType;
		else if (GetDlgButtonCheck(this, IDC_ADDFILES_CONVEOLTEXT) == BST_CHECKED)
			fConvEOL = kConvEOLAuto;
		else if (GetDlgButtonCheck(this, IDC_ADDFILES_CONVEOLALL) == BST_CHECKED)
			fConvEOL = kConvEOLAll;
		else {
			ASSERT(false);
			fConvEOL = kConvEOLNone;
		}

		fIncludeSubfolders =
			(GetDlgButtonCheck(this, IDC_ADDFILES_INCLUDE_SUBFOLDERS) == BST_CHECKED);
		fStripFolderNames =
			(GetDlgButtonCheck(this, IDC_ADDFILES_STRIP_FOLDER) == BST_CHECKED);
		fOverwriteExisting =
			(GetDlgButtonCheck(this, IDC_ADDFILES_OVERWRITE) == BST_CHECKED);

		pWnd = GetDlgItem(IDC_ADDFILES_PREFIX);
		ASSERT(pWnd != nil);
		pWnd->GetWindowText(fStoragePrefix);

		if (!ValidateStoragePrefix())
			return false;

		return true;
	} else {
		SetDlgButtonCheck(this, IDC_ADDFILES_NOPRESERVE,
			fTypePreservation == kPreserveNone);
		SetDlgButtonCheck(this, IDC_ADDFILES_PRESERVE,
			fTypePreservation == kPreserveTypes);
		SetDlgButtonCheck(this, IDC_ADDFILES_PRESERVEPLUS,
			fTypePreservation == kPreserveAndExtend);

		SetDlgButtonCheck(this, IDC_ADDFILES_CONVEOLNONE,
			fConvEOL == kConvEOLNone);
		SetDlgButtonCheck(this, IDC_ADDFILES_CONVEOLTYPE,
			fConvEOL == kConvEOLType);
		SetDlgButtonCheck(this, IDC_ADDFILES_CONVEOLTEXT,
			fConvEOL == kConvEOLAuto);
		SetDlgButtonCheck(this, IDC_ADDFILES_CONVEOLALL,
			fConvEOL == kConvEOLAll);

		SetDlgButtonCheck(this, IDC_ADDFILES_INCLUDE_SUBFOLDERS,
			fIncludeSubfolders != FALSE);
		SetDlgButtonCheck(this, IDC_ADDFILES_STRIP_FOLDER,
			fStripFolderNames != FALSE);
		SetDlgButtonCheck(this, IDC_ADDFILES_OVERWRITE,
			fOverwriteExisting != FALSE);

		pWnd = GetDlgItem(IDC_ADDFILES_PREFIX);
		ASSERT(pWnd != nil);
		pWnd->SetWindowText(fStoragePrefix);
		if (!fStoragePrefixEnable)
			pWnd->EnableWindow(FALSE);

		if (!fStripFolderNamesEnable) {
			::EnableControl(this, IDC_ADDFILES_STRIP_FOLDER, false);
		}

		if (!fConvEOLEnable) {
			::EnableControl(this, IDC_ADDFILES_CONVEOLNONE, false);
			::EnableControl(this, IDC_ADDFILES_CONVEOLTYPE, false);
			::EnableControl(this, IDC_ADDFILES_CONVEOLTEXT, false);
			::EnableControl(this, IDC_ADDFILES_CONVEOLALL, false);
		}

		return true;
	}
}

/*
 * Make sure the storage prefix they entered is valid.
 */
bool
AddFilesDialog::ValidateStoragePrefix(void)
{
	if (fStoragePrefix.IsEmpty())
		return true;

	const char kFssep = PathProposal::kDefaultStoredFssep;
	if (fStoragePrefix[0] == kFssep || fStoragePrefix.Right(1) == kFssep) {
		CString errMsg;
		errMsg.Format("The storage prefix may not start or end with '%c'.",
			kFssep);
		MessageBox(errMsg, m_ofn.lpstrTitle, MB_OK | MB_ICONWARNING);
		return false;
	}

	return true;
}


/*
 * Override base class version.
 */
UINT
AddFilesDialog::MyOnCommand(WPARAM wParam, LPARAM lParam)
{
	switch (wParam) {
	case IDHELP:
		OnIDHelp();
		return 1;
	default:
		return SelectFilesDialog::MyOnCommand(wParam, lParam);
	}
}

/*
 * Override base class version so we can move our stuff around.
 *
 * It's important that the base class be called last, because it calls
 * Invalidate to redraw the dialog.
 */
void
AddFilesDialog::ShiftControls(int deltaX, int deltaY)
{
	/*
	 * These only need to be here so that the initial move puts them
	 * where they belong.  Once the dialog has been created, the
	 * CFileDialog will move things where they need to go.
	 */
	MoveControl(this, IDC_ADDFILES_STATIC1, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_NOPRESERVE, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_PRESERVE, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_PRESERVEPLUS, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_STATIC2, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_STRIP_FOLDER, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_INCLUDE_SUBFOLDERS, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_OVERWRITE, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_STATIC3, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_PREFIX, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_STATIC4, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_CONVEOLNONE, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_CONVEOLTYPE, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_CONVEOLTEXT, 0, deltaY, false);
	MoveControl(this, IDC_ADDFILES_CONVEOLALL, 0, deltaY, false);

	/*
	 * These actively move.
	 */
	MoveControl(this, IDHELP, deltaX, deltaY, false);
	StretchControl(this, IDC_ADDFILES_PREFIX, deltaX, 0, false);
	SelectFilesDialog::ShiftControls(deltaX, deltaY);
}

/*
 * User pressed the "Help" button.
 */
void
AddFilesDialog::OnIDHelp(void)
{
	CWnd* pWndMain = ::AfxGetMainWnd();
	CWinApp* pAppMain = ::AfxGetApp();

	::WinHelp(pWndMain->m_hWnd, pAppMain->m_pszHelpFilePath,
				HELP_CONTEXT, HELP_TOPIC_ADD_FILES_DLG);
}
