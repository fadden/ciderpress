/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "stdafx.h"
#include "ExtractOptionsDialog.h"
#include "ChooseDirDialog.h"

BEGIN_MESSAGE_MAP(ExtractOptionsDialog, CDialog)
    ON_BN_CLICKED(IDC_EXT_CHOOSE_FOLDER, OnChooseFolder)
    ON_BN_CLICKED(IDC_EXT_CONVEOLNONE, OnChangeTextConv)
    ON_BN_CLICKED(IDC_EXT_CONVEOLTYPE, OnChangeTextConv)
    ON_BN_CLICKED(IDC_EXT_CONVEOLTEXT, OnChangeTextConv)
    ON_BN_CLICKED(IDC_EXT_CONVEOLALL, OnChangeTextConv)
    ON_BN_CLICKED(IDC_EXT_CONFIG_PRESERVE, OnConfigPreserve)
    ON_BN_CLICKED(IDC_EXT_CONFIG_CONVERT, OnConfigConvert)
    ON_WM_HELPINFO()
    ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()


/*
 * Set up the dialog that lets the user choose file extraction options.
 *
 * All we really need to do is update the string that indicates how many
 * files have been selected.
 */
BOOL ExtractOptionsDialog::OnInitDialog(void)
{
    CString countFmt;
    CString selStr;
    CWnd* pWnd;

    /* grab the radio button with the selection count */
    pWnd = GetDlgItem(IDC_EXT_SELECTED);
    ASSERT(pWnd != NULL);

    /* set the count string using a string table entry */
    if (fSelectedCount == 1) {
        CheckedLoadString(&countFmt, IDS_EXT_SELECTED_COUNT);
        pWnd->SetWindowText(countFmt);
    } else {
        CheckedLoadString(&countFmt, IDS_EXT_SELECTED_COUNTS_FMT);
        selStr.Format((LPCWSTR) countFmt, fSelectedCount);
        pWnd->SetWindowText(selStr);

        // disable "extract selection" when nothing is selected
        if (fSelectedCount == 0)
            pWnd->EnableWindow(FALSE);
    }

    /* if "no convert" is selected, disable high ASCII button */
    if (fConvEOL == kConvEOLNone) {
        pWnd = GetDlgItem(IDC_EXT_CONVHIGHASCII);
        pWnd->EnableWindow(false);
    }

    /* replace the existing button with one of our bitmap buttons */
    fChooseFolderButton.ReplaceDlgCtrl(this, IDC_EXT_CHOOSE_FOLDER);
    fChooseFolderButton.SetBitmapID(IDB_CHOOSE_FOLDER);

    return CDialog::OnInitDialog();
    //return TRUE;  // let Windows set the focus
}

void ExtractOptionsDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Text(pDX, IDC_EXT_PATH, fExtractPath);

    DDX_Radio(pDX, IDC_EXT_SELECTED, fFilesToExtract);

    DDX_Check(pDX, IDC_EXT_DATAFORK, fIncludeDataForks);
    DDX_Check(pDX, IDC_EXT_RSRCFORK, fIncludeRsrcForks);
    DDX_Check(pDX, IDC_EXT_DISKIMAGE, fIncludeDiskImages);

    DDX_Check(pDX, IDC_EXT_REFORMAT, fEnableReformat);
    DDX_Check(pDX, IDC_EXT_DISK_2MG, fDiskTo2MG);

    DDX_Check(pDX, IDC_EXT_ADD_PRESERVE, fAddTypePreservation);
    DDX_Check(pDX, IDC_EXT_ADD_EXTEN, fAddExtension);
    DDX_Check(pDX, IDC_EXT_STRIP_FOLDER, fStripFolderNames);

    DDX_Radio(pDX, IDC_EXT_CONVEOLNONE, fConvEOL);
    DDX_Check(pDX, IDC_EXT_CONVHIGHASCII, fConvHighASCII);

    DDX_Check(pDX, IDC_EXT_OVERWRITE_EXIST, fOverwriteExisting);

    if (pDX->m_bSaveAndValidate) {
        if (!fIncludeDataForks && !fIncludeRsrcForks && !fIncludeDiskImages) {
            ShowFailureMsg(this, IDS_NO_FORKS_SPECIFIED, IDS_MB_APP_NAME);
            pDX->Fail();
            return;
        }
    }
}

void ExtractOptionsDialog::OnConfigPreserve(void)
{
    // IDC_EXT_PATH, IDC_EXT_SELECTED
    SetDlgButtonCheck(this, IDC_EXT_DATAFORK, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_RSRCFORK, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_DISKIMAGE, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_REFORMAT, BST_UNCHECKED);
    SetDlgButtonCheck(this, IDC_EXT_DISK_2MG, BST_UNCHECKED);
    SetDlgButtonCheck(this, IDC_EXT_ADD_PRESERVE, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_ADD_EXTEN, BST_UNCHECKED);
    //SetDlgButtonCheck(this, IDC_EXT_STRIP_FOLDER, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_CONVEOLNONE, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_CONVEOLTYPE, BST_UNCHECKED);
    SetDlgButtonCheck(this, IDC_EXT_CONVEOLTEXT, BST_UNCHECKED);
    SetDlgButtonCheck(this, IDC_EXT_CONVEOLALL, BST_UNCHECKED);
    SetDlgButtonCheck(this, IDC_EXT_CONVHIGHASCII, BST_UNCHECKED);
    //SetDlgButtonCheck(this, IDC_EXT_OVERWRITE_EXIST, BST_CHECKED);

    OnChangeTextConv();
}

void ExtractOptionsDialog::OnConfigConvert(void)
{
    // IDC_EXT_PATH, IDC_EXT_SELECTED
    SetDlgButtonCheck(this, IDC_EXT_DATAFORK, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_RSRCFORK, BST_UNCHECKED);
    SetDlgButtonCheck(this, IDC_EXT_DISKIMAGE, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_REFORMAT, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_DISK_2MG, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_ADD_PRESERVE, BST_UNCHECKED);
    SetDlgButtonCheck(this, IDC_EXT_ADD_EXTEN, BST_CHECKED);
    //SetDlgButtonCheck(this, IDC_EXT_STRIP_FOLDER, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_CONVEOLNONE, BST_UNCHECKED);
    SetDlgButtonCheck(this, IDC_EXT_CONVEOLTYPE, BST_UNCHECKED);
    SetDlgButtonCheck(this, IDC_EXT_CONVEOLTEXT, BST_CHECKED);
    SetDlgButtonCheck(this, IDC_EXT_CONVEOLALL, BST_UNCHECKED);
    SetDlgButtonCheck(this, IDC_EXT_CONVHIGHASCII, BST_CHECKED);
    //SetDlgButtonCheck(this, IDC_EXT_OVERWRITE_EXIST, BST_CHECKED);

    OnChangeTextConv();
}

void ExtractOptionsDialog::OnChangeTextConv(void)
{
    CButton* pButton = (CButton*) GetDlgItem(IDC_EXT_CONVEOLNONE);
    ASSERT(pButton != NULL);
    bool convDisabled = (pButton->GetCheck() == BST_CHECKED);

    CWnd* pWnd = GetDlgItem(IDC_EXT_CONVHIGHASCII);
    ASSERT(pWnd != NULL);
    pWnd->EnableWindow(!convDisabled);
}

void ExtractOptionsDialog::OnChooseFolder(void)
{
    ChooseDirDialog chooseDir(this);
    CWnd* pEditWnd;
    CString editPath;

    /* get the currently-showing text from the edit field */
    pEditWnd = GetDlgItem(IDC_EXT_PATH);
    ASSERT(pEditWnd != NULL);
    pEditWnd->GetWindowText(editPath);

    chooseDir.SetPathName(editPath);
    if (chooseDir.DoModal() == IDOK) {
        const WCHAR* ccp = chooseDir.GetPathName();
        LOGI("New extract path chosen = '%ls'", ccp);

        pEditWnd->SetWindowText(ccp);
    }
}
