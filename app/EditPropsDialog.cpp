/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "StdAfx.h"
#include "EditPropsDialog.h"
#include "FileNameConv.h"

using namespace DiskImgLib;

BEGIN_MESSAGE_MAP(EditPropsDialog, CDialog)
    ON_BN_CLICKED(IDC_PROPS_ACCESS_W, UpdateSimpleAccess)
    ON_BN_CLICKED(IDC_PROPS_HFS_MODE, UpdateHFSMode)
    ON_CBN_SELCHANGE(IDC_PROPS_FILETYPE, OnTypeChange)
    ON_EN_CHANGE(IDC_PROPS_AUXTYPE, OnTypeChange)
    ON_EN_CHANGE(IDC_PROPS_HFS_FILETYPE, OnHFSTypeChange)
    ON_EN_CHANGE(IDC_PROPS_HFS_AUXTYPE, OnHFSTypeChange)
    ON_WM_HELPINFO()
    ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()


void EditPropsDialog::InitProps(GenericEntry* pEntry)
{
    fPathName = pEntry->GetPathNameUNI();
    fProps.fileType = pEntry->GetFileType();
    fProps.auxType = pEntry->GetAuxType();
    fProps.access = pEntry->GetAccess();
    fProps.createWhen = pEntry->GetCreateWhen();
    fProps.modWhen = pEntry->GetModWhen();

    if (!pEntry->GetFeatureFlag(GenericEntry::kFeatureCanChangeType))
        fAllowedTypes = kAllowedNone;
    else if (pEntry->GetFeatureFlag(GenericEntry::kFeaturePascalTypes))
        fAllowedTypes = kAllowedPascal;
    else if (pEntry->GetFeatureFlag(GenericEntry::kFeatureDOSTypes))
        fAllowedTypes = kAllowedDOS;
    else if (pEntry->GetFeatureFlag(GenericEntry::kFeatureHFSTypes))
        fAllowedTypes = kAllowedHFS;    // for HFS disks and ShrinkIt archives
    else
        fAllowedTypes = kAllowedProDOS;
    if (!pEntry->GetFeatureFlag(GenericEntry::kFeatureHasFullAccess)) {
        if (pEntry->GetFeatureFlag(GenericEntry::kFeatureHasSimpleAccess))
            fSimpleAccess = true;
        else
            fNoChangeAccess = true;
    }
    if (pEntry->GetFeatureFlag(GenericEntry::kFeatureHasInvisibleFlag))
        fAllowInvis = true;
}


/*
 * Set up the control.  We need to load the drop list with the file type
 * info, and configure any controls that aren't set by DoDataExchange.
 *
 * If this is a disk archive, we might want to make the aux type read-only,
 * though this would provide a way for users to fix badly-formed archives.
 */
BOOL EditPropsDialog::OnInitDialog(void)
{
    static const int kPascalTypes[] = {
        0x00 /*NON*/,   0x01 /*BAD*/,   0x02 /*PCD*/,   0x03 /*PTX*/,
        0xf3 /*$F3*/,   0x05 /*PDA*/,   0xf4 /*$F4*/,   0x08 /*FOT*/,
        0xf5 /*$f5*/
    };
    static const int kDOSTypes[] = {
        0x04 /*TXT*/,   0x06 /*BIN*/,   0xf2 /*$F2*/,   0xf3 /*$F3*/,
        0xf4 /*$F4*/,   0xfa /*INT*/,   0xfc /*BAS*/,   0xfe /*REL*/
    };
    CComboBox* pCombo;
    CWnd* pWnd;
    int comboIdx;

    pCombo = (CComboBox*) GetDlgItem(IDC_PROPS_FILETYPE);
    ASSERT(pCombo != NULL);

    pCombo->InitStorage(256, 256 * 8);

    for (int type = 0; type < 256; type++) {
        const WCHAR* str;
        WCHAR buf[10];

        if (fAllowedTypes == kAllowedPascal) {
            /* not the most efficient way, but it'll do */
            int j;
            for (j = 0; j < NELEM(kPascalTypes); j++) {
                if (kPascalTypes[j] == type)
                    break;
            }
            if (j == NELEM(kPascalTypes))
                continue;
        } else if (fAllowedTypes == kAllowedDOS) {
            int j;
            for (j = 0; j < NELEM(kDOSTypes); j++) {
                if (kDOSTypes[j] == type)
                    break;
            }
            if (j == NELEM(kDOSTypes))
                continue;
        }

        str = PathProposal::FileTypeString(type);
        if (str[0] == '$')
            wsprintf(buf, L"??? $%02X", type);
        else
            wsprintf(buf, L"%ls $%02X", str, type);
        comboIdx = pCombo->AddString(buf);
        pCombo->SetItemData(comboIdx, type);

        if ((int) fProps.fileType == type)
            pCombo->SetCurSel(comboIdx);
    }
    if (fProps.fileType >= 256) {
        if (fAllowedTypes == kAllowedHFS) {
            pCombo->SetCurSel(0);
        } else {
            // unexpected -- bogus data out of DiskFS?
            comboIdx = pCombo->AddString(L"???");
            pCombo->SetCurSel(comboIdx);
            pCombo->SetItemData(comboIdx, 256);
        }
    }

    CString dateStr;
    pWnd = GetDlgItem(IDC_PROPS_CREATEWHEN);
    ASSERT(pWnd != NULL);
    FormatDate(fProps.createWhen, &dateStr);
    pWnd->SetWindowText(dateStr);

    pWnd = GetDlgItem(IDC_PROPS_MODWHEN);
    ASSERT(pWnd != NULL);
    FormatDate(fProps.modWhen, &dateStr);
    pWnd->SetWindowText(dateStr);
    //LOGI("USING DATE '%ls' from 0x%08lx", dateStr, fProps.modWhen);

    CEdit* pEdit = (CEdit*) GetDlgItem(IDC_PROPS_AUXTYPE);
    ASSERT(pEdit != NULL);
    pEdit->SetLimitText(4);     // max len of aux type str
    pEdit = (CEdit*) GetDlgItem(IDC_PROPS_HFS_FILETYPE);
    pEdit->SetLimitText(4);
    pEdit = (CEdit*) GetDlgItem(IDC_PROPS_HFS_AUXTYPE);
    pEdit->SetLimitText(4);

    if (fReadOnly || fAllowedTypes == kAllowedNone) {
        pWnd = GetDlgItem(IDC_PROPS_FILETYPE);
        pWnd->EnableWindow(FALSE);
        pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
        pWnd->EnableWindow(FALSE);
    } else if (fAllowedTypes == kAllowedPascal) {
        pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
        pWnd->EnableWindow(FALSE);
    }
    if (fReadOnly || fSimpleAccess || fNoChangeAccess) {
        pWnd = GetDlgItem(IDC_PROPS_ACCESS_R);
        pWnd->EnableWindow(FALSE);
        pWnd = GetDlgItem(IDC_PROPS_ACCESS_B);
        pWnd->EnableWindow(FALSE);
        pWnd = GetDlgItem(IDC_PROPS_ACCESS_N);
        pWnd->EnableWindow(FALSE);
        pWnd = GetDlgItem(IDC_PROPS_ACCESS_D);
        pWnd->EnableWindow(FALSE);
    }
    if (fReadOnly || !fAllowInvis) {
        pWnd = GetDlgItem(IDC_PROPS_ACCESS_I);
        pWnd->EnableWindow(FALSE);
    }
    if (fReadOnly || fNoChangeAccess) {
        pWnd = GetDlgItem(IDC_PROPS_ACCESS_W);
        pWnd->EnableWindow(FALSE);
    }
    if (fReadOnly) {
        pWnd = GetDlgItem(IDOK);
        pWnd->EnableWindow(FALSE);

        CString title;
        GetWindowText(/*ref*/ title);
        title = title + " (read only)";
        SetWindowText(title);
    }

    if (fAllowedTypes != kAllowedHFS) {
        CButton* pButton = (CButton*) GetDlgItem(IDC_PROPS_HFS_MODE);
        pButton->EnableWindow(FALSE);
    }

    return CDialog::OnInitDialog();
}

void EditPropsDialog::DoDataExchange(CDataExchange* pDX)
{
    int fileTypeIdx;
    BOOL accessR, accessW, accessI, accessB, accessN, accessD;
    CComboBox* pCombo = (CComboBox*) GetDlgItem(IDC_PROPS_FILETYPE);

    if (pDX->m_bSaveAndValidate) {
        CString appName;
        CButton *pButton;
        bool typeChanged = false;

        CheckedLoadString(&appName, IDS_MB_APP_NAME);

        pButton = (CButton*) GetDlgItem(IDC_PROPS_HFS_MODE);
        if (pButton->GetCheck() == BST_CHECKED) {
            /* HFS mode */
            CString type, creator;
            DDX_Text(pDX, IDC_PROPS_HFS_FILETYPE, type);
            DDX_Text(pDX, IDC_PROPS_HFS_AUXTYPE, creator);
            if (type.GetLength() != 4 || creator.GetLength() != 4) {
                MessageBox(L"The file and creator types must be exactly"
                           L" 4 characters each.",
                    appName, MB_OK);
                pDX->Fail();
                return;
            }
            fProps.fileType = ((unsigned char) type[0]) << 24 |
                              ((unsigned char) type[1]) << 16 |
                              ((unsigned char) type[2]) << 8 |
                              ((unsigned char) type[3]);
            fProps.auxType  = ((unsigned char) creator[0]) << 24 |
                              ((unsigned char) creator[1]) << 16 |
                              ((unsigned char) creator[2]) << 8 |
                              ((unsigned char) creator[3]);
        } else {
            /* ProDOS mode */
            if (GetAuxType() < 0) {
                MessageBox(L"The AuxType field must be a valid 4-digit"
                           L" hexadecimal number.",
                    appName, MB_OK);
                pDX->Fail();
                return;
            }
            fProps.auxType = GetAuxType();

            /* pull the file type out, but don't disturb >= 256 */
            DDX_CBIndex(pDX, IDC_PROPS_FILETYPE, fileTypeIdx);
            if (fileTypeIdx != 256) {
                unsigned long oldType = fProps.fileType;
                fProps.fileType = pCombo->GetItemData(fileTypeIdx);
                if (fProps.fileType != oldType)
                    typeChanged = true;
            }
        }

        DDX_Check(pDX, IDC_PROPS_ACCESS_R, accessR);
        DDX_Check(pDX, IDC_PROPS_ACCESS_W, accessW);
        DDX_Check(pDX, IDC_PROPS_ACCESS_I, accessI);
        DDX_Check(pDX, IDC_PROPS_ACCESS_B, accessB);
        DDX_Check(pDX, IDC_PROPS_ACCESS_N, accessN);
        DDX_Check(pDX, IDC_PROPS_ACCESS_D, accessD);
        fProps.access = (accessR ? GenericEntry::kAccessRead : 0) |
                        (accessW ? GenericEntry::kAccessWrite : 0) |
                        (accessI ? GenericEntry::kAccessInvisible : 0) |
                        (accessB ? GenericEntry::kAccessBackup : 0) |
                        (accessN ? GenericEntry::kAccessRename : 0) |
                        (accessD ? GenericEntry::kAccessDelete : 0);

        if (fAllowedTypes == kAllowedDOS && typeChanged &&
            (fProps.fileType == kFileTypeBIN ||
             fProps.fileType == kFileTypeINT ||
             fProps.fileType == kFileTypeBAS))
        {
            CString msg;
            int result;

            CheckedLoadString(&msg, IDS_PROPS_DOS_TYPE_CHANGE);
            result = MessageBox(msg, appName, MB_ICONQUESTION|MB_OKCANCEL);
            if (result != IDOK) {
                pDX->Fail();
                return;
            }
        }
    } else {
        accessR = (fProps.access & GenericEntry::kAccessRead) != 0;
        accessW = (fProps.access & GenericEntry::kAccessWrite) != 0;
        accessI = (fProps.access & GenericEntry::kAccessInvisible) != 0;
        accessB = (fProps.access & GenericEntry::kAccessBackup) != 0;
        accessN = (fProps.access & GenericEntry::kAccessRename) != 0;
        accessD = (fProps.access & GenericEntry::kAccessDelete) != 0;
        DDX_Check(pDX, IDC_PROPS_ACCESS_R, accessR);
        DDX_Check(pDX, IDC_PROPS_ACCESS_W, accessW);
        DDX_Check(pDX, IDC_PROPS_ACCESS_I, accessI);
        DDX_Check(pDX, IDC_PROPS_ACCESS_B, accessB);
        DDX_Check(pDX, IDC_PROPS_ACCESS_N, accessN);
        DDX_Check(pDX, IDC_PROPS_ACCESS_D, accessD);

        if (fAllowedTypes == kAllowedHFS &&
            (fProps.fileType > 0xff || fProps.auxType > 0xffff))
        {
            char type[5], creator[5];

            type[0] = (unsigned char) (fProps.fileType >> 24);
            type[1] = (unsigned char) (fProps.fileType >> 16);
            type[2] = (unsigned char) (fProps.fileType >> 8);
            type[3] = (unsigned char)  fProps.fileType;
            type[4] = '\0';
            creator[0] = (unsigned char) (fProps.auxType >> 24);
            creator[1] = (unsigned char) (fProps.auxType >> 16);
            creator[2] = (unsigned char) (fProps.auxType >> 8);
            creator[3] = (unsigned char)  fProps.auxType;
            creator[4] = '\0';

            CString tmpStr;
            tmpStr = type;
            DDX_Text(pDX, IDC_PROPS_HFS_FILETYPE, tmpStr);
            tmpStr = creator;
            DDX_Text(pDX, IDC_PROPS_HFS_AUXTYPE, tmpStr);
            tmpStr = L"0000";
            DDX_Text(pDX, IDC_PROPS_AUXTYPE, tmpStr);

            CButton* pButton = (CButton*) GetDlgItem(IDC_PROPS_HFS_MODE);
            pButton->SetCheck(BST_CHECKED);
        } else {
            //fileTypeIdx = fProps.fileType;
            //if (fileTypeIdx > 256)
            //  fileTypeIdx = 256;
            //DDX_CBIndex(pDX, IDC_PROPS_FILETYPE, fileTypeIdx);

            /* write the aux type as a hex string */
            fAuxType.Format(L"%04X", fProps.auxType);
            DDX_Text(pDX, IDC_PROPS_AUXTYPE, fAuxType);
        }
        OnTypeChange();         // set the description field
        UpdateHFSMode();        // set up fields
        UpdateSimpleAccess();   // coordinate N/D with W
    }

    DDX_Text(pDX, IDC_PROPS_PATHNAME, fPathName);
}

void EditPropsDialog::OnTypeChange(void)
{
    static const WCHAR kUnknownFileType[] = L"Unknown file type";
    CComboBox* pCombo;
    CWnd* pWnd;
    int fileType, fileTypeIdx;
    long auxType;
    const WCHAR* descr = NULL;

    pCombo = (CComboBox*) GetDlgItem(IDC_PROPS_FILETYPE);
    ASSERT(pCombo != NULL);

    fileTypeIdx = pCombo->GetCurSel();
    fileType = pCombo->GetItemData(fileTypeIdx);
    if (fileType >= 256) {
        descr = kUnknownFileType;
    } else {
        auxType = GetAuxType();
        if (auxType < 0)
            auxType = 0;
        descr = PathProposal::FileTypeDescription(fileType, auxType);
        if (descr == NULL)
            descr = kUnknownFileType;
    }

    pWnd = GetDlgItem(IDC_PROPS_TYPEDESCR);
    ASSERT(pWnd != NULL);
    pWnd->SetWindowText(descr);

    /* DOS aux type only applies to BIN */
    if (!fReadOnly && fAllowedTypes == kAllowedDOS) {
        pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
        pWnd->EnableWindow(fileType == kFileTypeBIN);
    }
}

void EditPropsDialog::OnHFSTypeChange(void)
{
    assert(fAllowedTypes == kAllowedHFS);
}

void EditPropsDialog::UpdateHFSMode(void)
{
    CButton* pButton = (CButton*) GetDlgItem(IDC_PROPS_HFS_MODE);
    CComboBox* pCombo;
    CWnd* pWnd;

    if (pButton->GetCheck() == BST_CHECKED) {
        /* switch to HFS mode */
        LOGI("Switching to HFS mode");
        //fHFSMode = true;

        if (!fReadOnly) {
            pWnd = GetDlgItem(IDC_PROPS_HFS_FILETYPE);
            pWnd->EnableWindow(TRUE);
            pWnd = GetDlgItem(IDC_PROPS_HFS_AUXTYPE);
            pWnd->EnableWindow(TRUE);
            pWnd = GetDlgItem(IDC_PROPS_HFS_LABEL);
            pWnd->EnableWindow(TRUE);
        }

        /* point the file type at something safe */
        pCombo = (CComboBox*) GetDlgItem(IDC_PROPS_FILETYPE);
        pCombo->EnableWindow(FALSE);

        pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
        pWnd->EnableWindow(FALSE);

        pWnd = GetDlgItem(IDC_PROPS_TYPEDESCR);
        ASSERT(pWnd != NULL);
        pWnd->SetWindowText(L"(HFS type)");
        OnHFSTypeChange();
    } else {
        /* switch to ProDOS mode */
        LOGI("Switching to ProDOS mode");
        //fHFSMode = false;
        if (!fReadOnly) {
            pCombo = (CComboBox*) GetDlgItem(IDC_PROPS_FILETYPE);
            pCombo->EnableWindow(TRUE);
            pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
            pWnd->EnableWindow(TRUE);
        }

        pWnd = GetDlgItem(IDC_PROPS_HFS_FILETYPE);
        pWnd->EnableWindow(FALSE);
        pWnd = GetDlgItem(IDC_PROPS_HFS_AUXTYPE);
        pWnd->EnableWindow(FALSE);
        pWnd = GetDlgItem(IDC_PROPS_HFS_LABEL);
        pWnd->EnableWindow(FALSE);
        OnTypeChange();
    }
}

void EditPropsDialog::UpdateSimpleAccess(void)
{
    if (!fSimpleAccess)
        return;

    CButton* pButton;
    UINT checked;

    pButton = (CButton*) GetDlgItem(IDC_PROPS_ACCESS_W);
    checked = pButton->GetCheck();

    pButton = (CButton*) GetDlgItem(IDC_PROPS_ACCESS_N);
    pButton->SetCheck(checked);
    pButton = (CButton*) GetDlgItem(IDC_PROPS_ACCESS_D);
    pButton->SetCheck(checked);
}

long EditPropsDialog::GetAuxType(void)
{
    CWnd* pWnd = GetDlgItem(IDC_PROPS_AUXTYPE);
    ASSERT(pWnd != NULL);

    CString aux;
    pWnd->GetWindowText(aux);

    const WCHAR* str = aux;
    WCHAR* end;
    long val;

    if (str[0] == '\0') {
        LOGI(" HEY: blank aux type, returning -1");
        return -1;
    }
    val = wcstoul(aux, &end, 16);
    if (end != str + wcslen(str)) {
        LOGI(" HEY: found some garbage in aux type '%ls', returning -1",
            (LPCWSTR) aux);
        return -1;
    }
    return val;
}
