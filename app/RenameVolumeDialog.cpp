/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "stdafx.h"
#include "RenameVolumeDialog.h"
#include "DiskFSTree.h"
#include "DiskArchive.h"

BEGIN_MESSAGE_MAP(RenameVolumeDialog, CDialog)
    ON_NOTIFY(TVN_SELCHANGED, IDC_RENAMEVOL_TREE, OnSelChanged)
    ON_BN_CLICKED(IDHELP, OnHelp)
    ON_WM_HELPINFO()
END_MESSAGE_MAP()

BOOL RenameVolumeDialog::OnInitDialog(void)
{
    /* do the DoDataExchange stuff */
    CDialog::OnInitDialog();

    CTreeCtrl* pTree = (CTreeCtrl*) GetDlgItem(IDC_RENAMEVOL_TREE);
    DiskImgLib::DiskFS* pDiskFS = fpArchive->GetDiskFS();

    ASSERT(pTree != NULL);

    fDiskFSTree.fIncludeSubdirs = false;
    fDiskFSTree.fExpandDepth = -1;
    if (!fDiskFSTree.BuildTree(pDiskFS, pTree)) {
        LOGI("Tree load failed!");
        OnCancel();
    }

    int count = pTree->GetCount();
    LOGI("ChooseAddTargetDialog tree has %d items", count);

    /* select the default text and set the focus */
    CEdit* pEdit = (CEdit*) GetDlgItem(IDC_RENAMEVOL_NEW);
    ASSERT(pEdit != NULL);
    pEdit->SetSel(0, -1);
    pEdit->SetFocus();

    return FALSE;   // we set the focus
}

void RenameVolumeDialog::DoDataExchange(CDataExchange* pDX)
{
    CString msg, failed;
    //DiskImgLib::DiskFS*   pDiskFS = fpArchive->GetDiskFS();

    CheckedLoadString(&failed, IDS_MB_APP_NAME);

    /* put fNewName last so it gets the focus after failure */
    DDX_Text(pDX, IDC_RENAMEVOL_NEW, fNewName);

    /* validate the path field */
    if (pDX->m_bSaveAndValidate) {
        /*
         * Make sure they chose a volume that can be modified.
         */
        CTreeCtrl* pTree = (CTreeCtrl*) GetDlgItem(IDC_RENAMEVOL_TREE);
        CString errMsg, appName;
        CheckedLoadString(&appName, IDS_MB_APP_NAME);

        HTREEITEM selected;
        selected = pTree->GetSelectedItem();
        if (selected == NULL) {
            errMsg = "Please select a disk to rename.";
            MessageBox(errMsg, appName, MB_OK);
            pDX->Fail();
            return;
        }

        DiskFSTree::TargetData* pTargetData;
        pTargetData = (DiskFSTree::TargetData*) pTree->GetItemData(selected);
        if (!pTargetData->selectable) {
            errMsg = "You can't rename that volume.";
            MessageBox(errMsg, appName, MB_OK);
            pDX->Fail();
            return;
        }
        ASSERT(pTargetData->kind == DiskFSTree::kTargetDiskFS);

        /*
         * Verify that the new name is okay.  (Do this *after* checking the
         * volume above to avoid spurious complaints about unsupported
         * filesystems.)
         */
        if (fNewName.IsEmpty()) {
            msg = "You must specify a new name.";
            goto fail;
        }
        msg = fpArchive->TestVolumeName(pTargetData->pDiskFS, fNewName);
        if (!msg.IsEmpty())
            goto fail;


        /*
         * Looks good.  Fill in the answer.
         */
        fpChosenDiskFS = pTargetData->pDiskFS;
    }

    return;

fail:
    ASSERT(!msg.IsEmpty());
    MessageBox(msg, failed, MB_OK);
    pDX->Fail();
    return;
}

void RenameVolumeDialog::OnSelChanged(NMHDR* pnmh, LRESULT* pResult)
{
    CTreeCtrl* pTree = (CTreeCtrl*) GetDlgItem(IDC_RENAMEVOL_TREE);
    HTREEITEM selected;
    CString newText;

    selected = pTree->GetSelectedItem();
    if (selected != NULL) {
        DiskFSTree::TargetData* pTargetData;
        pTargetData = (DiskFSTree::TargetData*) pTree->GetItemData(selected);
        if (pTargetData->selectable) {
            newText = pTargetData->pDiskFS->GetBareVolumeName();
        } else {
            newText = "";
        }
    }
    
    CEdit* pEdit = (CEdit*) GetDlgItem(IDC_RENAMEVOL_NEW);
    ASSERT(pEdit != NULL);
    pEdit->SetWindowText(newText);
    pEdit->SetSel(0, -1);

    *pResult = 0;
}
