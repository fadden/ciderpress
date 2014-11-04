/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for RenameEntryDialog.
 */
#include "stdafx.h"
#include "RenameEntryDialog.h"
#include "HelpTopics.h"

BEGIN_MESSAGE_MAP(RenameEntryDialog, CDialog)
    ON_WM_HELPINFO()
    ON_COMMAND(IDHELP, OnHelp)
    ON_BN_CLICKED(IDC_RENAME_SKIP, OnSkip)
END_MESSAGE_MAP()


/*
 * Set up the control.
 */
BOOL
RenameEntryDialog::OnInitDialog(void)
{
    ASSERT(fBasePath.IsEmpty());
    fOldFile = fOldName;
    fFssepStr = fFssep;

    CEdit* pEdit = (CEdit*) GetDlgItem(IDC_RENAME_PATHSEP);
    pEdit->SetReadOnly(!fCanChangeFssep);
    pEdit->LimitText(1);

    /* if they can't rename the full path, only give them the file name */
    if (fCanRenameFullPath || fFssep == '\0') {
        fNewName = fOldName;
        // fBasePath is empty
    } else {
        int offset;

        offset = fOldName.ReverseFind(fFssep);
        if (offset < fOldName.GetLength()) {
            fBasePath = fOldName.Left(offset);
            fNewName = fOldName.Right(fOldName.GetLength() - (offset+1));
        } else {
            /* weird -- filename ended with an fssep? */
            ASSERT(false);      // debugbreak
            // fBasePath is empty
            fNewName = fOldName;
        }
    }

    /* do the DoDataExchange stuff */
    CDialog::OnInitDialog();

    /* select the editable text and set the focus */
    pEdit = (CEdit*) GetDlgItem(IDC_RENAME_NEW);
    ASSERT(pEdit != nil);
    pEdit->SetSel(0, -1);
    pEdit->SetFocus();

    return FALSE;   // we set the focus
}

/*
 * Convert values.
 */
void
RenameEntryDialog::DoDataExchange(CDataExchange* pDX)
{
    CString msg, failed;

    msg = "";
    failed.LoadString(IDS_MB_APP_NAME);

    /* fNewName must come last, or the focus will be set on the wrong field
       when we return after failure */
    DDX_Text(pDX, IDC_RENAME_OLD, fOldFile);
    DDX_Text(pDX, IDC_RENAME_PATHSEP, fFssepStr);
    DDX_Text(pDX, IDC_RENAME_NEW, fNewName);

    /* validate the path field */
    if (pDX->m_bSaveAndValidate) {
        if (fNewName.IsEmpty()) {
            msg = "You must specify a new name.";
            goto fail;
        }

        msg = fpArchive->TestPathName(fpEntry, fBasePath, fNewName, fFssep);
        if (!msg.IsEmpty())
            goto fail;

        if (fFssepStr.IsEmpty())
            fFssep = '\0';
        else
            fFssep = fFssepStr.GetAt(0);    // could be '\0', that's okay
    }

    return;

fail:
    ASSERT(!msg.IsEmpty());
    MessageBox(msg, failed, MB_OK);
    pDX->Fail();
    return;
}

/*
 * User pressed the "skip" button, which causes us to bail with a result that
 * skips the rename but continues with the series.
 */
void
RenameEntryDialog::OnSkip(void)
{
    EndDialog(IDIGNORE);
}

/*
 * Context help request (question mark button).
 */
BOOL
RenameEntryDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
    WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
    return TRUE;    // yes, we handled it
}

/*
 * User pressed the "Help" button.
 */
void
RenameEntryDialog::OnHelp(void)
{
    WinHelp(HELP_TOPIC_RENAME_ENTRY, HELP_CONTEXT);
}
