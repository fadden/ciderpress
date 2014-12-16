/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "stdafx.h"
#include "ChooseDirDialog.h"
#include "NewFolderDialog.h"
#include "DiskFSTree.h"

BEGIN_MESSAGE_MAP(ChooseDirDialog, CDialog)
    ON_NOTIFY(TVN_SELCHANGED, IDC_CHOOSEDIR_TREE, OnSelChanged)
    ON_BN_CLICKED(IDC_CHOOSEDIR_EXPAND_TREE, OnExpandTree)
    ON_BN_CLICKED(IDC_CHOOSEDIR_NEW_FOLDER, OnNewFolder)
    ON_WM_HELPINFO()
    //ON_COMMAND(ID_HELP, OnIDHelp)
    ON_BN_CLICKED(IDHELP, OnHelp)
END_MESSAGE_MAP()


BOOL ChooseDirDialog::OnInitDialog(void)
{
    CDialog::OnInitDialog();

    /* set up the "new folder" button */
    fNewFolderButton.ReplaceDlgCtrl(this, IDC_CHOOSEDIR_NEW_FOLDER);
    fNewFolderButton.SetBitmapID(IDB_NEW_FOLDER);

    /* replace the tree control with a ShellTree */
    if (fShellTree.ReplaceDlgCtrl(this, IDC_CHOOSEDIR_TREE) != TRUE) {
        LOGI("WARNING: ShellTree replacement failed");
        ASSERT(false);
    }

    //enable images
    fShellTree.EnableImages();
    //populate for the with Shell Folders for the first time
    fShellTree.PopulateTree(/*CSIDL_DRIVES*/);

    if (fPathName.IsEmpty()) {
        // start somewhere reasonable
        fShellTree.ExpandMyComputer();
    } else {
        CString msg("");
        fShellTree.TunnelTree(fPathName, &msg);
        if (!msg.IsEmpty()) {
            /* failed */
            LOGI("TunnelTree failed on '%ls' (%ls), using MyComputer instead",
                (LPCWSTR) fPathName, (LPCWSTR) msg);
            fShellTree.ExpandMyComputer();
        }
    }

    fShellTree.SetFocus();
    return FALSE;   // leave focus on shell tree
}

BOOL ChooseDirDialog::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN &&
         pMsg->wParam == VK_RETURN)
    {
        //LOGI("RETURN!");
        if (GetFocus() == GetDlgItem(IDC_CHOOSEDIR_PATHEDIT)) {
            OnExpandTree();
            return TRUE;
        }
    }

    return CDialog::PreTranslateMessage(pMsg);
}

void ChooseDirDialog::OnSelChanged(NMHDR* pnmh, LRESULT* pResult)
{
    CString path;
    CWnd* pWnd = GetDlgItem(IDC_CHOOSEDIR_PATH);
    ASSERT(pWnd != NULL);

    if (fShellTree.GetFolderPath(&path))
        fPathName = path;
    else
        fPathName = L"";
    pWnd->SetWindowText(fPathName);

    // disable the "Select" button when there's no path ready
    pWnd = GetDlgItem(IDOK);
    ASSERT(pWnd != NULL);
    pWnd->EnableWindow(!fPathName.IsEmpty());

    // It's confusing to have two different paths showing, so wipe out the
    // free entry field when the selection changes.
    pWnd = GetDlgItem(IDC_CHOOSEDIR_PATHEDIT);
    pWnd->SetWindowText(L"");

    *pResult = 0;
}

void ChooseDirDialog::OnExpandTree(void)
{
    CWnd* pWnd;
    CString str;
    CString msg;

    pWnd = GetDlgItem(IDC_CHOOSEDIR_PATHEDIT);
    ASSERT(pWnd != NULL);
    pWnd->GetWindowText(str);

    if (!str.IsEmpty()) {
        fShellTree.TunnelTree(str, &msg);
        if (!msg.IsEmpty()) {
            CString failed;
            CheckedLoadString(&failed, IDS_FAILED);
            MessageBox(msg, failed, MB_OK | MB_ICONERROR);
        }
    }
}

void ChooseDirDialog::OnNewFolder(void)
{
    if (fPathName.IsEmpty()) {
        MessageBox(L"You can't create a folder in this part of the tree.",
            L"Bad Location", MB_OK | MB_ICONERROR);
        return;
    }

    NewFolderDialog newFolderDlg;

    newFolderDlg.fCurrentFolder = fPathName;
    if (newFolderDlg.DoModal() == IDOK) {
        if (newFolderDlg.GetFolderCreated()) {
            /*
             * They created a new folder.  We want to add it to the tree
             * and then select it.  This is not too hard because we know
             * that the folder was created under the currently-selected
             * tree node.
             */
            if (fShellTree.AddFolderAtSelection(newFolderDlg.fNewFolder)) {
                CString msg;
                LOGI("Success, tunneling to '%ls'",
                    (LPCWSTR) newFolderDlg.fNewFullPath);
                fShellTree.TunnelTree(newFolderDlg.fNewFullPath, &msg);
                if (!msg.IsEmpty()) {
                    LOGI("TunnelTree failed: %ls", (LPCWSTR) msg);
                }
            } else {
                LOGI("AddFolderAtSelection FAILED");
                ASSERT(false);
            }
        } else {
            LOGI("NewFolderDialog returned IDOK but no create");
        }
    }
}
