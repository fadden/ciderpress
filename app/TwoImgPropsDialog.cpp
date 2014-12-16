/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "StdAfx.h"
#include "TwoImgPropsDialog.h"

BEGIN_MESSAGE_MAP(TwoImgPropsDialog, CDialog)
    ON_BN_CLICKED(IDC_TWOIMG_LOCKED, OnChange)
    ON_BN_CLICKED(IDC_TWOIMG_DOSVOLSET, OnChange)
    ON_EN_CHANGE(IDC_TWOIMG_DOSVOLNUM, OnChange)
    ON_EN_CHANGE(IDC_TWOIMG_COMMENT, OnChange)
    ON_WM_HELPINFO()
END_MESSAGE_MAP()


BOOL TwoImgPropsDialog::OnInitDialog(void)
{
    CWnd* pWnd;
    CEdit* pEdit;
    CString tmpStr;

    ASSERT(fpHeader != NULL);

    /*
     * Set up the static fields.
     */
    pWnd = GetDlgItem(IDC_TWOIMG_CREATOR);
    tmpStr.Format(L"'%hs'", fpHeader->GetCreatorStr());
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_TWOIMG_VERSION);
    tmpStr.Format(L"%d", fpHeader->fVersion);
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_TWOIMG_FORMAT);
    switch (fpHeader->fImageFormat) {
    case TwoImgHeader::kImageFormatDOS:     tmpStr = L"DOS order sectors";      break;
    case TwoImgHeader::kImageFormatProDOS:  tmpStr = L"ProDOS order sectors";   break;
    case TwoImgHeader::kImageFormatNibble:  tmpStr = L"Raw nibbles";            break;
    default:                                tmpStr = L"Unknown";                break;
    }
    pWnd->SetWindowText(tmpStr);

    pWnd = GetDlgItem(IDC_TWOIMG_BLOCKS);
    tmpStr.Format(L"%d", fpHeader->fNumBlocks);
    pWnd->SetWindowText(tmpStr);

    /*
     * Restrict the edit field.
     */
    pEdit = (CEdit*) GetDlgItem(IDC_TWOIMG_DOSVOLNUM);
    pEdit->LimitText(3);        // 1-254

    /*
     * Disable the "Save" button.
     */
    pWnd = GetDlgItem(IDOK);
    pWnd->EnableWindow(FALSE);

    /* for read-only mode, all buttons are disabled */
    if (fReadOnly) {
        GetDlgItem(IDC_TWOIMG_LOCKED)->EnableWindow(FALSE);
        GetDlgItem(IDC_TWOIMG_DOSVOLSET)->EnableWindow(FALSE);
        GetDlgItem(IDC_TWOIMG_COMMENT)->EnableWindow(FALSE);
        GetDlgItem(IDC_TWOIMG_DOSVOLNUM)->EnableWindow(FALSE);

        GetWindowText(tmpStr);
        tmpStr += " (read-only)";
        SetWindowText(tmpStr);
    }

    return CDialog::OnInitDialog();
}

void TwoImgPropsDialog::DoDataExchange(CDataExchange* pDX)
{
    BOOL locked, dosVolSet;
    CString comment;
    int dosVolNum;

    if (pDX->m_bSaveAndValidate) {
        DDX_Check(pDX, IDC_TWOIMG_LOCKED, locked);
        DDX_Check(pDX, IDC_TWOIMG_DOSVOLSET, dosVolSet);
        DDX_Text(pDX, IDC_TWOIMG_COMMENT, comment);
        DDX_Text(pDX, IDC_TWOIMG_DOSVOLNUM, dosVolNum);

        LOGI("GOT dosVolNum = %d", dosVolNum);

        fpHeader->fFlags &= ~(TwoImgHeader::kFlagLocked);
        if (locked)
            fpHeader->fFlags |= TwoImgHeader::kFlagLocked;

        fpHeader->fFlags &= ~(TwoImgHeader::kDOSVolumeMask);
        fpHeader->fFlags &= ~(TwoImgHeader::kDOSVolumeSet);
        if (dosVolSet) {
            fpHeader->fFlags |= TwoImgHeader::kDOSVolumeSet;
            fpHeader->fFlags |= (dosVolNum & TwoImgHeader::kDOSVolumeMask);

            CString appStr, errMsg;
            if (dosVolNum < 1 || dosVolNum > 254) {
                CheckedLoadString(&appStr, IDS_MB_APP_NAME);
                CheckedLoadString(&errMsg, IDS_VALID_VOLNAME_DOS);
                MessageBox(errMsg, appStr, MB_OK);
                pDX->Fail();
            } else {
                fpHeader->SetDOSVolumeNum(dosVolNum);
            }
        }


        if (!comment.IsEmpty()) {
            CStringA commentA(comment);
            fpHeader->SetComment(commentA);
        } else {
            fpHeader->SetComment(NULL);
        }
    } else {
        CWnd* pWnd;

        locked = (fpHeader->fFlags & TwoImgHeader::kFlagLocked) != 0;
        dosVolSet = (fpHeader->fFlags & TwoImgHeader::kDOSVolumeSet) != 0;
        comment = fpHeader->GetComment();
        if (dosVolSet)
            dosVolNum = fpHeader->GetDOSVolumeNum();
        else
            dosVolNum = TwoImgHeader::kDefaultVolumeNum;

        DDX_Check(pDX, IDC_TWOIMG_LOCKED, locked);
        DDX_Check(pDX, IDC_TWOIMG_DOSVOLSET, dosVolSet);
        DDX_Text(pDX, IDC_TWOIMG_COMMENT, comment);
        DDX_Text(pDX, IDC_TWOIMG_DOSVOLNUM, dosVolNum);

        /* set initial state of dos volume number edit field */
        if (!fReadOnly) {
            pWnd = GetDlgItem(IDC_TWOIMG_DOSVOLNUM);
            pWnd->EnableWindow(dosVolSet);
        }
    }
}

void TwoImgPropsDialog::OnChange(void)
{
    CButton* pButton;
    UINT checked;

    ASSERT(!fReadOnly);

    GetDlgItem(IDOK)->EnableWindow(TRUE);

    pButton = (CButton*) GetDlgItem(IDC_TWOIMG_DOSVOLSET);
    checked = pButton->GetCheck();
    GetDlgItem(IDC_TWOIMG_DOSVOLNUM)->EnableWindow(checked == BST_CHECKED);
}
