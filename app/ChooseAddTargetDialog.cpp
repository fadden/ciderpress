/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Functions for the ChooseAddTarget dialog box.
 */
#include "StdAfx.h"
#include "ChooseAddTargetDialog.h"
#include "DiskFSTree.h"

using namespace DiskImgLib;

BEGIN_MESSAGE_MAP(ChooseAddTargetDialog, CDialog)
    ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()

BOOL ChooseAddTargetDialog::OnInitDialog(void)
{
    CDialog::OnInitDialog();

    CTreeCtrl* pTree = (CTreeCtrl*) GetDlgItem(IDC_ADD_TARGET_TREE);

    ASSERT(fpDiskFS != NULL);
    ASSERT(pTree != NULL);

    fDiskFSTree.fIncludeSubdirs = true;
    fDiskFSTree.fExpandDepth = -1;
    if (!fDiskFSTree.BuildTree(fpDiskFS, pTree)) {
        LOGI("Tree load failed!");
        OnCancel();
    }

    int count = pTree->GetCount();
    LOGI("ChooseAddTargetDialog tree has %d items", count);
    if (count <= 1) {
        LOGI(" Skipping out of target selection");
        // adding to root volume of the sole DiskFS
        fpChosenDiskFS = fpDiskFS;
        ASSERT(fpChosenSubdir == NULL);
        OnOK();
    }

    return TRUE;
}

void ChooseAddTargetDialog::DoDataExchange(CDataExchange* pDX)
{
    /*
     * Not much to do on the way in.  On the way out, make sure that they've
     * selected something acceptable, and copy the values to an easily
     * accessible location.
     */
    if (pDX->m_bSaveAndValidate) {
        CTreeCtrl* pTree = (CTreeCtrl*) GetDlgItem(IDC_ADD_TARGET_TREE);
        CString errMsg, appName;
        CheckedLoadString(&appName, IDS_MB_APP_NAME);

        /* shortcut for simple disk images */
        if (pTree->GetCount() == 1 && fpChosenDiskFS != NULL)
            return;

        HTREEITEM selected;
        selected = pTree->GetSelectedItem();
        if (selected == NULL) {
            errMsg = L"Please select a disk or subdirectory to add files to.";
            MessageBox(errMsg, appName, MB_OK);
            pDX->Fail();
            return;
        }

        DiskFSTree::TargetData* pTargetData;
        pTargetData = (DiskFSTree::TargetData*) pTree->GetItemData(selected);
        if (!pTargetData->selectable) {
            errMsg = L"You can't add files there.";
            MessageBox(errMsg, appName, MB_OK);
            pDX->Fail();
            return;
        }

        fpChosenDiskFS = pTargetData->pDiskFS;
        fpChosenSubdir = pTargetData->pFile;
    }
}
