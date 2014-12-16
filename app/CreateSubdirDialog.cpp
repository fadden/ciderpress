/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of CreateSubdirDialog.
 *
 * Gets the name from the user, validates it against the supplied
 * GenericArchive, and returns.
 */
#include "stdafx.h"
#include "CreateSubdirDialog.h"

BEGIN_MESSAGE_MAP(CreateSubdirDialog, CDialog)
    ON_WM_HELPINFO()
END_MESSAGE_MAP()

BOOL CreateSubdirDialog::OnInitDialog(void)
{
    /* do the DoDataExchange stuff */
    CDialog::OnInitDialog();

    /* select the default text and set the focus */
    CEdit* pEdit = (CEdit*) GetDlgItem(IDC_CREATESUBDIR_NEW);
    ASSERT(pEdit != NULL);
    pEdit->SetSel(0, -1);
    pEdit->SetFocus();

    return FALSE;   // we set the focus
}

void CreateSubdirDialog::DoDataExchange(CDataExchange* pDX)
{
    CString msg, failed;

    CheckedLoadString(&failed, IDS_MB_APP_NAME);

    /* put fNewName last so it gets the focus after failure */
    DDX_Text(pDX, IDC_CREATESUBDIR_BASE, fBasePath);
    DDX_Text(pDX, IDC_CREATESUBDIR_NEW, fNewName);

    /* validate the path field */
    if (pDX->m_bSaveAndValidate) {
        if (fNewName.IsEmpty()) {
            msg = L"You must specify a new name.";
            goto fail;
        }

        msg = fpArchive->TestPathName(fpParentEntry, fBasePath, fNewName,
                '\0');
        if (!msg.IsEmpty())
            goto fail;
    }

    return;

fail:
    ASSERT(!msg.IsEmpty());
    MessageBox(msg, failed, MB_OK);
    pDX->Fail();
    return;
}
