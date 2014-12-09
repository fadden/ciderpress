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
 *      RenameOverwriteDialog
 * ==========================================================================
 */

BEGIN_MESSAGE_MAP(RenameOverwriteDialog, CDialog)
    ON_WM_HELPINFO()
END_MESSAGE_MAP()

BOOL RenameOverwriteDialog::OnInitDialog(void)
{
    CWnd* pWnd;

    pWnd = GetDlgItem(IDC_RENOVWR_SOURCE_NAME);
    ASSERT(pWnd != NULL);
    pWnd->SetWindowText(fNewFileSource);

    return CDialog::OnInitDialog();
}

void RenameOverwriteDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Text(pDX, IDC_RENOVWR_ORIG_NAME, fExistingFile);
    DDX_Text(pDX, IDC_RENOVWR_NEW_NAME, fNewName);

    /* validate the path field */
    if (pDX->m_bSaveAndValidate) {
        if (fNewName.IsEmpty()) {
            MessageBox(L"You must specify a new name.",
                L"CiderPress", MB_OK);
            pDX->Fail();
        }

        // we *could* try to validate the path here...
    }
}


/*
 * ==========================================================================
 *      ConfirmOverwriteDialog
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


BOOL ConfirmOverwriteDialog::OnInitDialog(void)
{
    CWnd* pWnd;
    CString tmpStr, dateStr;

    pWnd = GetDlgItem(IDC_OVWR_EXIST_NAME);
    ASSERT(pWnd != NULL);
    pWnd->SetWindowText(fExistingFile);

    pWnd = GetDlgItem(IDC_OVWR_EXIST_INFO);
    ASSERT(pWnd != NULL);
    FormatDate(fExistingFileModWhen, &dateStr);
    tmpStr.Format(L"Modified %ls", (LPCWSTR) dateStr);
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_OVWR_NEW_NAME);
    ASSERT(pWnd != NULL);
    pWnd->SetWindowText(fNewFileSource);

    pWnd = GetDlgItem(IDC_OVWR_NEW_INFO);
    ASSERT(pWnd != NULL);
    FormatDate(fNewFileModWhen, &dateStr);
    tmpStr.Format(L"Modified %ls", (LPCWSTR) dateStr);
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_OVWR_RENAME);
    ASSERT(pWnd != NULL);
    pWnd->EnableWindow(fAllowRename);

    return CDialog::OnInitDialog();
}

void ConfirmOverwriteDialog::OnYes(void)
{
    fResultOverwrite = true;
    CDialog::OnOK();
}

void ConfirmOverwriteDialog::OnYesToAll(void)
{
    fResultOverwrite = true;
    fResultApplyToAll = true;
    CDialog::OnOK();
}

void ConfirmOverwriteDialog::OnNo(void)
{
    //fResultOverwrite = false;
    CDialog::OnOK();
}

void ConfirmOverwriteDialog::OnNoToAll(void)
{
    //fResultOverwrite = true;
    fResultApplyToAll = true;
    CDialog::OnOK();
}

void ConfirmOverwriteDialog::OnRename(void)
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
