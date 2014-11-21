/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Simple dialog class that returns when any of its buttons are hit.
 */
#include "StdAfx.h"
#include "DiskEditOpenDialog.h"

BEGIN_MESSAGE_MAP(DiskEditOpenDialog, CDialog)
    ON_BN_CLICKED(IDC_DEOW_FILE, OnButtonFile)
    ON_BN_CLICKED(IDC_DEOW_VOLUME, OnButtonVolume)
    ON_BN_CLICKED(IDC_DEOW_CURRENT, OnButtonCurrent)
END_MESSAGE_MAP()


BOOL DiskEditOpenDialog::OnInitDialog(void)
{
    if (!fArchiveOpen) {
        CButton* pButton = (CButton*) GetDlgItem(IDC_DEOW_CURRENT);
        ASSERT(pButton != NULL);
        pButton->EnableWindow(FALSE);
    }

    return CDialog::OnInitDialog();
}

void DiskEditOpenDialog::OnButtonFile(void)
{
    fOpenWhat = kOpenFile;
    OnOK();
}

void DiskEditOpenDialog::OnButtonVolume(void)
{
    fOpenWhat = kOpenVolume;
    OnOK();
}

void DiskEditOpenDialog::OnButtonCurrent(void)
{
    fOpenWhat = kOpenCurrent;
    OnOK();
}
