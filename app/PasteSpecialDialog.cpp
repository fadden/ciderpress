/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for PasteSpecialDialog.
 */
#include "StdAfx.h"
#include "PasteSpecialDialog.h"

#if 0
BEGIN_MESSAGE_MAP(PasteSpecialDialog, CDialog)
END_MESSAGE_MAP()

BOOL
PasteSpecialDialog::OnInitDialog(void)
{
    CString countStr;
    CWnd* pWnd;

    countStr.Format(IDS_PASTE_SPECIAL_COUNT, 3);
    pWnd = GetDlgItem(IDC_PASTE_SPECIAL_COUNT);
    pWnd->SetWindowText(countStr);

    return CDialog::OnInitDialog();
}
#endif

void PasteSpecialDialog::DoDataExchange(CDataExchange* pDX)
{
    /*
     * Initialize radio control with value from fPasteHow.
     */

    if (!pDX->m_bSaveAndValidate) {
        UINT ctrlId;

        if (fPasteHow == kPastePaths)
            ctrlId = IDC_PASTE_SPECIAL_PATHS;
        else
            ctrlId = IDC_PASTE_SPECIAL_NOPATHS;

        CButton* pButton = (CButton*) GetDlgItem(ctrlId);
        pButton->SetCheck(BST_CHECKED);
    } else {
        CButton* pButton = (CButton*) GetDlgItem(IDC_PASTE_SPECIAL_PATHS);

        if (pButton->GetCheck() == BST_CHECKED)
            fPasteHow = kPastePaths;
        else
            fPasteHow = kPasteNoPaths;
    }
}
