/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Choose file name and characteristics for a file imported from an audio
 * cassette tape.
 */
#include "StdAfx.h"
#include "CassImpTargetDialog.h"
#include "GenericArchive.h"     // just want kFileTypeXXX

BEGIN_MESSAGE_MAP(CassImpTargetDialog, CDialog)
    ON_BN_CLICKED(IDC_CASSIMPTARG_BAS, OnTypeChange)
    ON_BN_CLICKED(IDC_CASSIMPTARG_INT, OnTypeChange)
    ON_BN_CLICKED(IDC_CASSIMPTARG_BIN, OnTypeChange)
    ON_EN_CHANGE(IDC_CASSIMPTARG_BINADDR, OnAddrChange)
END_MESSAGE_MAP()

BOOL CassImpTargetDialog::OnInitDialog(void)
{
    /* substitute our replacement edit control */
    fAddrEdit.ReplaceDlgCtrl(this, IDC_CASSIMPTARG_BINADDR);
    fAddrEdit.SetProperties(MyEdit::kCapsOnly | MyEdit::kHexOnly);

    //CWnd* pWnd;
    CEdit* pEdit;

    pEdit = (CEdit*) GetDlgItem(IDC_CASSIMPTARG_BINADDR);
    pEdit->SetLimitText(4);     // 4-digit hex value

    /* do the DDX thing, then update computed fields */
    CDialog::OnInitDialog();
    OnTypeChange();
    OnAddrChange();

    pEdit = (CEdit*) GetDlgItem(IDC_CASSIMPTARG_FILENAME);
    pEdit->SetSel(0, -1);
    pEdit->SetFocus();
    return FALSE;       // don't change the focus
}

void CassImpTargetDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_Radio(pDX, IDC_CASSIMPTARG_BAS, fFileTypeIndex);
    DDX_Text(pDX, IDC_CASSIMPTARG_FILENAME, fFileName);

    if (pDX->m_bSaveAndValidate) {
        CString appName;
        CheckedLoadString(&appName, IDS_MB_APP_NAME);

        if (fFileTypeIndex == kTypeBIN) {
            if (GetStartAddr() < 0) {
                MessageBox(L"The address field must be a valid 4-digit "
                           L" hexadecimal number.",
                    appName, MB_OK);
                pDX->Fail();
                return;
            }
            fStartAddr = (unsigned short) GetStartAddr();
        }
        if (fFileName.IsEmpty()) {
            MessageBox(L"You must enter a filename.", appName, MB_OK);
            pDX->Fail();
            return;
        }
    } else {
        CWnd* pWnd;
        CString tmpStr;

        pWnd = GetDlgItem(IDC_CASSIMPTARG_BINADDR);
        tmpStr.Format(L"%04X", fStartAddr);
        pWnd->SetWindowText(tmpStr);
    }
}

void CassImpTargetDialog::OnTypeChange(void)
{
    CButton* pButton;
    CWnd* pWnd;

    pButton = (CButton*) GetDlgItem(IDC_CASSIMPTARG_BIN);
    pWnd = GetDlgItem(IDC_CASSIMPTARG_BINADDR);

    pWnd->EnableWindow(pButton->GetCheck() == BST_CHECKED);
}

void CassImpTargetDialog::OnAddrChange(void)
{
    CWnd* pWnd;
    CString tmpStr;
    long val;

    val = GetStartAddr();
    if (val < 0)
        val = 0;

    tmpStr.Format(L".%04X", val + fFileLength-1);

    pWnd = GetDlgItem(IDC_CASSIMPTARG_RANGE);
    pWnd->SetWindowText(tmpStr);
}

long CassImpTargetDialog::GetStartAddr(void) const
{
    CWnd* pWnd = GetDlgItem(IDC_CASSIMPTARG_BINADDR);
    ASSERT(pWnd != NULL);

    CString aux;
    pWnd->GetWindowText(aux);

    const WCHAR* str = aux;
    WCHAR* end;
    long val;

    if (str[0] == '\0') {
        LOGI(" HEY: blank addr, returning -1");
        return -1;
    }
    val = wcstoul(aux, &end, 16);
    if (end != str + wcslen(str)) {
        LOGI(" HEY: found some garbage in addr '%ls', returning -1",
            (LPCWSTR) aux);
        return -1;
    }
    return val;
}

long CassImpTargetDialog::GetFileType(void) const
{
    switch (fFileTypeIndex) {
    case kTypeBIN:  return kFileTypeBIN;
    case kTypeINT:  return kFileTypeINT;
    case kTypeBAS:  return kFileTypeBAS;
    default:
        assert(false);
        return -1;
    }
}

void CassImpTargetDialog::SetFileType(long type)
{
    switch (type) {
    case kFileTypeBIN:  fFileTypeIndex = kTypeBIN;  break;
    case kFileTypeINT:  fFileTypeIndex = kTypeINT;  break;
    case kFileTypeBAS:  fFileTypeIndex = kTypeBAS;  break;
    default:
        assert(false);
        break;
    }
}
