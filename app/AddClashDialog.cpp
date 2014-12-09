/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "stdafx.h"
#include "ConfirmOverwriteDialog.h"
#include "AddClashDialog.h"

BEGIN_MESSAGE_MAP(AddClashDialog, CDialog)
    ON_BN_CLICKED(IDC_CLASH_RENAME, OnRename)
    ON_BN_CLICKED(IDC_CLASH_SKIP, OnSkip)
END_MESSAGE_MAP()

/*
 * Replaces some static text fields.
 */
BOOL AddClashDialog::OnInitDialog(void)
{
    CWnd* pWnd;

    pWnd = GetDlgItem(IDC_CLASH_WINNAME);
    ASSERT(pWnd != NULL);
    pWnd->SetWindowText(fWindowsName);

    pWnd = GetDlgItem(IDC_CLASH_STORAGENAME);
    ASSERT(pWnd != NULL);
    pWnd->SetWindowText(fStorageName);

    return CDialog::OnInitDialog();
}

void AddClashDialog::OnSkip(void)
{
    fDoRename = false;
    CDialog::OnOK();
}

void AddClashDialog::OnRename(void)
{
    RenameOverwriteDialog dlg;

    dlg.fNewFileSource = fWindowsName;
    dlg.fExistingFile = fStorageName;
    dlg.fNewName = fStorageName;
    if (dlg.DoModal() == IDOK) {
        fNewName = dlg.fNewName;
        fDoRename = true;
        CDialog::OnOK();
    }
}
