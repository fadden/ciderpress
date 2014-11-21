/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "stdafx.h"
#include "NewDiskSize.h"
#include "resource.h"

/*
 * Number of blocks in the disks we create.
 *
 * These must be in ascending order.
 */
/*static*/ const NewDiskSize::RadioCtrlMap NewDiskSize::kCtrlMap[] = {
    { IDC_CONVDISK_140K,        280 },
    { IDC_CONVDISK_800K,        1600 },
    { IDC_CONVDISK_1440K,       2880 },
    { IDC_CONVDISK_5MB,         10240 },
    { IDC_CONVDISK_16MB,        32768 },
    { IDC_CONVDISK_20MB,        40960 },
    { IDC_CONVDISK_32MB,        65535 },
    { IDC_CONVDISK_SPECIFY,     kSpecified },
};
static const int kEditBoxID = IDC_CONVDISK_SPECIFY_EDIT;

/*static*/ unsigned int NewDiskSize::GetNumSizeEntries(void)
{
    return NELEM(kCtrlMap);
}

/*static*/ long NewDiskSize::GetDiskSizeByIndex(int idx)
{
    ASSERT(idx >= 0 && idx < NELEM(kCtrlMap));
    return kCtrlMap[idx].blocks;
}

/*static*/ void NewDiskSize::EnableButtons(CDialog* pDialog, BOOL state)
{
    CWnd* pWnd;

    for (int i = 0; i < NELEM(kCtrlMap); i++) {
        pWnd = pDialog->GetDlgItem(kCtrlMap[i].ctrlID);
        if (pWnd != NULL)
            pWnd->EnableWindow(state);
    }
}

/*static*/ void NewDiskSize::EnableButtons_ProDOS(CDialog* pDialog,
    long totalBlocks, long blocksUsed)
{
    CButton* pButton;
    long usedWithoutBitmap = blocksUsed - GetNumBitmapBlocks_ProDOS(totalBlocks);
    bool first = true;

    LOGI("EnableButtons_ProDOS total=%ld used=%ld usedw/o=%ld",
        totalBlocks, blocksUsed, usedWithoutBitmap);

    for (int i = 0; i < NELEM(kCtrlMap); i++) {
        pButton = (CButton*) pDialog->GetDlgItem(kCtrlMap[i].ctrlID);
        if (pButton == NULL) {
            LOGI("WARNING: couldn't find ctrlID %d", kCtrlMap[i].ctrlID);
            continue;
        }

        if (kCtrlMap[i].blocks == kSpecified) {
            pButton->SetCheck(BST_UNCHECKED);
            pButton->EnableWindow(TRUE);
            CWnd* pWnd = pDialog->GetDlgItem(kEditBoxID);
            pWnd->EnableWindow(FALSE);
            continue;
        }

        if (usedWithoutBitmap + GetNumBitmapBlocks_ProDOS(kCtrlMap[i].blocks) <=
            kCtrlMap[i].blocks)
        {
            pButton->EnableWindow(TRUE);
            if (first) {
                pButton->SetCheck(BST_CHECKED);
                first = false;
            } else {
                pButton->SetCheck(BST_UNCHECKED);
            }
        } else {
            pButton->EnableWindow(FALSE);
            pButton->SetCheck(BST_UNCHECKED);
        }
    }

    UpdateSpecifyEdit(pDialog);
}

/*static*/ long NewDiskSize::GetNumBitmapBlocks_ProDOS(long totalBlocks) {
    ASSERT(totalBlocks > 0);
    const int kBitsPerBlock = 512 * 8;
    int numBlocks = (totalBlocks + kBitsPerBlock-1) / kBitsPerBlock;
    return numBlocks;
}

/*static*/ void NewDiskSize::UpdateSpecifyEdit(CDialog* pDialog)
{
    CEdit* pEdit = (CEdit*) pDialog->GetDlgItem(kEditBoxID);
    int i;

    if (pEdit == NULL) {
        ASSERT(false);
        return;
    }

    for (i = 0; i < NELEM(kCtrlMap); i++) {
        CButton* pButton = (CButton*) pDialog->GetDlgItem(kCtrlMap[i].ctrlID);
        if (pButton == NULL) {
            LOGI("WARNING: couldn't find ctrlID %d", kCtrlMap[i].ctrlID);
            continue;
        }

        if (pButton->GetCheck() == BST_CHECKED) {
            if (kCtrlMap[i].blocks == kSpecified)
                return;
            break;
        }
    }
    if (i == NELEM(kCtrlMap)) {
        LOGI("WARNING: couldn't find a checked radio button");
        return;
    }

    CString fmt;
    fmt.Format(L"%ld", kCtrlMap[i].blocks);
    pEdit->SetWindowText(fmt);
}
