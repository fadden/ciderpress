/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for ConvDiskOptionsDialog.
 */
#include "stdafx.h"
#include "CreateImageDialog.h"
#include "NewDiskSize.h"
#include "../diskimg/DiskImgDetail.h"       // need ProDOS filename validator

BEGIN_MESSAGE_MAP(CreateImageDialog, CDialog)
    ON_WM_HELPINFO()
    ON_COMMAND(IDHELP, OnHelp)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CREATEFS_DOS32, IDC_CREATEFS_BLANK,
        OnFormatChangeRange)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CONVDISK_140K, IDC_CONVDISK_SPECIFY,
        OnSizeChangeRange)
END_MESSAGE_MAP()


// TODO: obtain from DiskImgLib header?
const int kProDOSVolNameMax = 15;       // longest possible ProDOS volume name
const int kPascalVolNameMax = 7;        // longest possible Pascal volume name
const int kHFSVolNameMax = 27;          // longest possible HFS volume name
const long kMaxBlankBlocks = 16777216;  // 8GB in 512-byte blocks

BOOL CreateImageDialog::OnInitDialog(void)
{
    // high bit set in signed short means key is down
    if (::GetKeyState(VK_SHIFT) < 0) {
        LOGI("Shift key is down, enabling extended options");
        fExtendedOpts = true;
    }

    CEdit* pEdit = (CEdit*) GetDlgItem(IDC_CREATEFSPRODOS_VOLNAME);
    ASSERT(pEdit != NULL);
    pEdit->SetLimitText(kProDOSVolNameMax);

    pEdit = (CEdit*) GetDlgItem(IDC_CREATEFSPASCAL_VOLNAME);
    ASSERT(pEdit != NULL);
    pEdit->SetLimitText(kPascalVolNameMax);

    pEdit = (CEdit*) GetDlgItem(IDC_CREATEFSHFS_VOLNAME);
    ASSERT(pEdit != NULL);
    pEdit->SetLimitText(kHFSVolNameMax);

    pEdit = (CEdit*) GetDlgItem(IDC_CREATEFSDOS_VOLNUM);
    ASSERT(pEdit != NULL);
    pEdit->SetLimitText(3);     // 3 digit volume number

    pEdit = (CEdit*) GetDlgItem(IDC_CONVDISK_SPECIFY_EDIT);
    ASSERT(pEdit != NULL);
    pEdit->EnableWindow(FALSE);

    return CDialog::OnInitDialog();
}

void CreateImageDialog::DoDataExchange(CDataExchange* pDX)
{
    UINT specifyBlocks = 280;
    CString errMsg;

    DDX_Radio(pDX, IDC_CONVDISK_140K, fDiskSizeIdx);
    DDX_Radio(pDX, IDC_CREATEFS_DOS32, fDiskFormatIdx);
    DDX_Check(pDX, IDC_CREATEFSDOS_ALLOCDOS, fAllocTracks_DOS);
    DDX_Text(pDX, IDC_CREATEFSDOS_VOLNUM, fDOSVolumeNum);
    DDX_Text(pDX, IDC_CREATEFSPRODOS_VOLNAME, fVolName_ProDOS);
    DDX_Text(pDX, IDC_CREATEFSPASCAL_VOLNAME, fVolName_Pascal);
    DDX_Text(pDX, IDC_CREATEFSHFS_VOLNAME, fVolName_HFS);
    DDX_Text(pDX, IDC_CONVDISK_SPECIFY_EDIT, specifyBlocks);

    ASSERT(fDiskSizeIdx >= 0 && fDiskSizeIdx < (int)NewDiskSize::GetNumSizeEntries());

    if (pDX->m_bSaveAndValidate) {
        fNumBlocks = NewDiskSize::GetDiskSizeByIndex(fDiskSizeIdx);
        if (fNumBlocks == NewDiskSize::kSpecified)
            fNumBlocks = specifyBlocks;

        if (fDiskFormatIdx == kFmtDOS32) {
            CString tmpStr;
            tmpStr.Format(L"%d", fDOSVolumeNum);
            if (!IsValidVolumeName_DOS(tmpStr)) {
                CheckedLoadString(&errMsg, IDS_VALID_VOLNAME_DOS);
            }
        } else if (fDiskFormatIdx == kFmtDOS33) {
            CString tmpStr;
            tmpStr.Format(L"%d", fDOSVolumeNum);
            if (!IsValidVolumeName_DOS(tmpStr)) {
                CheckedLoadString(&errMsg, IDS_VALID_VOLNAME_DOS);
            }

            // only needed in "extended" mode -- this stuff is too painful to
            //  inflict on the average user
            if (fNumBlocks < 18*8 || fNumBlocks > 800 ||
                (fNumBlocks <= 400 && (fNumBlocks % 8) != 0) ||
                (fNumBlocks > 400 && (fNumBlocks % 16) != 0))
            {
                errMsg = L"Specify a size between 144 blocks (18 tracks) and"
                         L" 800 blocks (50 tracks/32 sectors).  The block count"
                         L" must be a multiple of 8 for 16-sector disks, or a"
                         L" multiple of 16 for 32-sector disks.  32 sector"
                         L" formatting starts at 400 blocks.  Disks larger than"
                         L" 400 blocks but less than 800 aren't recognized by"
                         L" CiderPress.";
            }
        } else if (fDiskFormatIdx == kFmtProDOS) {
            // Max is really 65535, but we allow 65536 for creation of volumes
            // that can be copied to CFFA cards.
            if (fNumBlocks < 16 || fNumBlocks > 65536) {
                errMsg = L"Specify a size of at least 16 blocks and no more"
                         L" than 65536 blocks.";
            } else if (fVolName_ProDOS.IsEmpty() ||
                fVolName_ProDOS.GetLength() > kProDOSVolNameMax)
            {
                errMsg = L"You must specify a volume name 1-15 characters long.";
            } else {
                if (!IsValidVolumeName_ProDOS(fVolName_ProDOS)) {
                    CheckedLoadString(&errMsg, IDS_VALID_VOLNAME_PRODOS);
                }
            }
        } else if (fDiskFormatIdx == kFmtPascal) {
            if (fVolName_Pascal.IsEmpty() ||
                fVolName_Pascal.GetLength() > kPascalVolNameMax)
            {
                errMsg = L"You must specify a volume name 1-7 characters long.";
            } else {
                if (!IsValidVolumeName_Pascal(fVolName_Pascal)) {
                    CheckedLoadString(&errMsg, IDS_VALID_VOLNAME_PASCAL);
                }
            }
        } else if (fDiskFormatIdx == kFmtHFS) {
            if (fNumBlocks < 1600 || fNumBlocks > 4194303) {
                errMsg = L"Specify a size of at least 1600 blocks and no more"
                         L" than 4194303 blocks.";
            } else if (fVolName_HFS.IsEmpty() ||
                fVolName_HFS.GetLength() > kHFSVolNameMax)
            {
                errMsg = L"You must specify a volume name 1-27 characters long.";
            } else {
                if (!IsValidVolumeName_HFS(fVolName_HFS)) {
                    CheckedLoadString(&errMsg, IDS_VALID_VOLNAME_HFS);
                }
            }
        } else if (fDiskFormatIdx == kFmtBlank) {
            if (fNumBlocks < 1 || fNumBlocks > kMaxBlankBlocks)
                errMsg = L"Specify a size of at least 1 block and no more"
                         L" than 16777216 blocks.";
        } else {
            ASSERT(false);
        }
    } else {
        OnFormatChangeRange(IDC_CREATEFS_DOS32 + fDiskFormatIdx);
    }

    if (!errMsg.IsEmpty()) {
        CString appName;
        CheckedLoadString(&appName, IDS_MB_APP_NAME);
        MessageBox(errMsg, appName, MB_OK);
        pDX->Fail();
    }

    CDialog::DoDataExchange(pDX);
}

void CreateImageDialog::OnFormatChangeRange(UINT nID)
{
    static const struct {
        UINT    buttonID;
        UINT    ctrlID;
    } kFormatTab[] = {
        { IDC_CREATEFS_DOS32, IDC_CREATEFSDOS_ALLOCDOS },
        { IDC_CREATEFS_DOS32, IDC_CREATEFSDOS_VOLNUM },
        { IDC_CREATEFS_DOS32, IDC_CONVDISK_140K },
        { IDC_CREATEFS_DOS33, IDC_CREATEFSDOS_ALLOCDOS },
        { IDC_CREATEFS_DOS33, IDC_CREATEFSDOS_VOLNUM },
        { IDC_CREATEFS_DOS33, IDC_CONVDISK_140K },
        { IDC_CREATEFS_PRODOS, IDC_CREATEFSPRODOS_VOLNAME },
        { IDC_CREATEFS_PRODOS, IDC_CONVDISK_140K },
        { IDC_CREATEFS_PRODOS, IDC_CONVDISK_800K },
        { IDC_CREATEFS_PRODOS, IDC_CONVDISK_1440K },
        { IDC_CREATEFS_PRODOS, IDC_CONVDISK_5MB },
        { IDC_CREATEFS_PRODOS, IDC_CONVDISK_16MB },
        { IDC_CREATEFS_PRODOS, IDC_CONVDISK_20MB },
        { IDC_CREATEFS_PRODOS, IDC_CONVDISK_32MB },
        { IDC_CREATEFS_PRODOS, IDC_CONVDISK_SPECIFY },
        { IDC_CREATEFS_PASCAL, IDC_CREATEFSPASCAL_VOLNAME },
        { IDC_CREATEFS_PASCAL, IDC_CONVDISK_140K },
        { IDC_CREATEFS_PASCAL, IDC_CONVDISK_800K },
        { IDC_CREATEFS_HFS, IDC_CREATEFSHFS_VOLNAME },
        { IDC_CREATEFS_HFS, IDC_CONVDISK_800K },
        { IDC_CREATEFS_HFS, IDC_CONVDISK_1440K },
        { IDC_CREATEFS_HFS, IDC_CONVDISK_5MB },
        { IDC_CREATEFS_HFS, IDC_CONVDISK_16MB },
        { IDC_CREATEFS_HFS, IDC_CONVDISK_20MB },
        { IDC_CREATEFS_HFS, IDC_CONVDISK_32MB },
        { IDC_CREATEFS_HFS, IDC_CONVDISK_SPECIFY },
        { IDC_CREATEFS_BLANK, IDC_CONVDISK_140K },
        { IDC_CREATEFS_BLANK, IDC_CONVDISK_800K },
        { IDC_CREATEFS_BLANK, IDC_CONVDISK_1440K },
        { IDC_CREATEFS_BLANK, IDC_CONVDISK_5MB },
        { IDC_CREATEFS_BLANK, IDC_CONVDISK_16MB },
        { IDC_CREATEFS_BLANK, IDC_CONVDISK_20MB },
        { IDC_CREATEFS_BLANK, IDC_CONVDISK_32MB },
        { IDC_CREATEFS_BLANK, IDC_CONVDISK_SPECIFY },
    };
    static const UINT kDetailControls[] = {
        IDC_CREATEFSDOS_ALLOCDOS,
        IDC_CREATEFSDOS_VOLNUM,
        IDC_CREATEFSPRODOS_VOLNAME,
        IDC_CREATEFSPASCAL_VOLNAME,
        IDC_CREATEFSHFS_VOLNAME
    };
    int i;
    
    LOGI("OnFormatChangeRange id=%d", nID);

    /* reset so 140K is highlighted */
    NewDiskSize::EnableButtons_ProDOS(this, 32, 16);

    /* disable all buttons */
    NewDiskSize::EnableButtons(this, FALSE);

    for (i = 0; i < NELEM(kDetailControls); i++) {
        CWnd* pWnd = GetDlgItem(kDetailControls[i]);
        if (pWnd != NULL)
            pWnd->EnableWindow(FALSE);
    }

    /* re-enable just the ones we like */
    for (i = 0; i < NELEM(kFormatTab); i++) {
        if (kFormatTab[i].buttonID == nID) {
            CWnd* pWnd = GetDlgItem(kFormatTab[i].ctrlID);
            ASSERT(pWnd != NULL);
            if (pWnd != NULL)
                pWnd->EnableWindow(TRUE);
        }
    }
    if (fExtendedOpts && nID != IDC_CREATEFS_DOS32) {
        CWnd* pWnd = GetDlgItem(IDC_CONVDISK_SPECIFY);
        pWnd->EnableWindow(TRUE);
    }

    /* make sure 140K is viable; doesn't work for HFS */
    CButton* pButton;
    pButton = (CButton*) GetDlgItem(IDC_CONVDISK_140K);
    if (!pButton->IsWindowEnabled()) {
        pButton->SetCheck(BST_UNCHECKED);
        pButton = (CButton*) GetDlgItem(IDC_CONVDISK_800K);
        pButton->SetCheck(BST_CHECKED);
    }
}

void CreateImageDialog::OnSizeChangeRange(UINT nID)
{
    LOGI("OnSizeChangeRange id=%d", nID);

    CButton* pButton = (CButton*) GetDlgItem(IDC_CONVDISK_SPECIFY);
    CEdit* pEdit = (CEdit*) GetDlgItem(IDC_CONVDISK_SPECIFY_EDIT);
    pEdit->EnableWindow(pButton->GetCheck() == BST_CHECKED);

    CButton* pBlank;
    CButton* pHFS;
    pBlank = (CButton*) GetDlgItem(IDC_CREATEFS_BLANK);
    pHFS = (CButton*) GetDlgItem(IDC_CREATEFS_HFS);
    if (pHFS->GetCheck() == BST_CHECKED)
        pEdit->SetLimitText(10);    // enough for "2147483647"
    else if (pBlank->GetCheck() == BST_CHECKED)
        pEdit->SetLimitText(8);     // enough for "16777216"
    else
        pEdit->SetLimitText(5);     // enough for "65535"

    NewDiskSize::UpdateSpecifyEdit(this);
}


bool CreateImageDialog::IsValidVolumeName_DOS(const WCHAR* name)
{
    CStringA nameStr(name);
    return DiskImgLib::DiskFSDOS33::IsValidVolumeName(nameStr);
}

bool CreateImageDialog::IsValidVolumeName_ProDOS(const WCHAR* name)
{
    CStringA nameStr(name);
    return DiskImgLib::DiskFSProDOS::IsValidVolumeName(nameStr);
}

bool CreateImageDialog::IsValidVolumeName_Pascal(const WCHAR* name)
{
    CStringA nameStr(name);
    return DiskImgLib::DiskFSPascal::IsValidVolumeName(nameStr);
}

bool CreateImageDialog::IsValidVolumeName_HFS(const WCHAR* name)
{
    CStringA nameStr(name);
    return DiskImgLib::DiskFSHFS::IsValidVolumeName(nameStr);
}
