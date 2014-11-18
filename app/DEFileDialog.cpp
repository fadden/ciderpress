/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of Disk Editor "open file" dialog.
 */
#include "stdafx.h"
#include "DEFileDialog.h"


BEGIN_MESSAGE_MAP(DEFileDialog, CDialog)
    ON_EN_CHANGE(IDC_DEFILE_FILENAME, OnChange)
    ON_WM_HELPINFO()
END_MESSAGE_MAP()

/*
 * Turn off the "OK" button, which is only active when some text
 * has been typed in the window.
 */
BOOL
DEFileDialog::OnInitDialog(void)
{
    CWnd* pWnd = GetDlgItem(IDOK);
    ASSERT(pWnd != NULL);
    pWnd->EnableWindow(FALSE);

    return CDialog::OnInitDialog();
}

/*
 * Get the filename and the "open resource fork" check box.
 */
void
DEFileDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Text(pDX, IDC_DEFILE_FILENAME, fName);
    DDX_Check(pDX, IDC_DEFILE_RSRC, fOpenRsrcFork);
}

/*
 * The text has changed.  If there's nothing in the box, dim the
 * "OK" button.
 */
void
DEFileDialog::OnChange(void)
{
    CEdit* pEdit = (CEdit*) GetDlgItem(IDC_DEFILE_FILENAME);
    ASSERT(pEdit != NULL);

    CString str;
    pEdit->GetWindowText(str);
    //LOGI("STR is '%ls' (%d)", str, str.GetLength());

    CWnd* pWnd = GetDlgItem(IDOK);
    ASSERT(pWnd != NULL);
    pWnd->EnableWindow(!str.IsEmpty());
}

/*
 * Context help request (question mark button).
 */
BOOL
DEFileDialog::OnHelpInfo(HELPINFO* lpHelpInfo)
{
    WinHelp((DWORD) lpHelpInfo->iCtrlId, HELP_CONTEXTPOPUP);
    return TRUE;    // yes, we handled it
}
