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

BOOL DEFileDialog::OnInitDialog(void)
{
    CWnd* pWnd = GetDlgItem(IDOK);
    ASSERT(pWnd != NULL);
    pWnd->EnableWindow(FALSE);

    return CDialog::OnInitDialog();
}

void DEFileDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Text(pDX, IDC_DEFILE_FILENAME, fName);
    DDX_Check(pDX, IDC_DEFILE_RSRC, fOpenRsrcFork);
}

void DEFileDialog::OnChange(void)
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
