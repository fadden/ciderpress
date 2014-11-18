/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for ConvDiskOptionsDialog.
 */
#include "stdafx.h"
#include "ConvDiskOptionsDialog.h"
#include "NufxArchive.h"
#include "Main.h"
#include "ActionProgressDialog.h"
#include "DiskArchive.h"
#include "NewDiskSize.h"
#include "../diskimg/DiskImgDetail.h"       // need ProDOS filename validator

BEGIN_MESSAGE_MAP(ConvDiskOptionsDialog, CDialog)
    ON_WM_HELPINFO()
    //ON_COMMAND(IDHELP, OnHelp)
    ON_BN_CLICKED(IDC_CONVDISK_COMPUTE, OnCompute)
    ON_BN_CLICKED(IDC_USE_SELECTED, ResetSizeControls)
    ON_BN_CLICKED(IDC_USE_ALL, ResetSizeControls)
    //ON_BN_CLICKED(IDC_CONVDISK_SPARSE, ResetSizeControls)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_CONVDISK_140K, IDC_CONVDISK_SPECIFY,
        OnRadioChangeRange)
END_MESSAGE_MAP()



const int kProDOSVolNameMax = 15;   // longest possible ProDOS volume name

/*
 * Set up our modified version of the "use selection" dialog.
 */
BOOL
ConvDiskOptionsDialog::OnInitDialog(void)
{
    CEdit* pEdit = (CEdit*) GetDlgItem(IDC_CONVDISK_VOLNAME);
    ASSERT(pEdit != NULL);
    pEdit->SetLimitText(kProDOSVolNameMax);

    ResetSizeControls();

    pEdit = (CEdit*) GetDlgItem(IDC_CONVDISK_SPECIFY_EDIT);
    ASSERT(pEdit != NULL);
    pEdit->SetLimitText(5);     // enough for "65535"
    pEdit->EnableWindow(FALSE);

    return UseSelectionDialog::OnInitDialog();
}

/*
 * Convert values.
 */
void
ConvDiskOptionsDialog::DoDataExchange(CDataExchange* pDX)
{
    UINT specifyBlocks = 280;
    CString errMsg;

    DDX_Radio(pDX, IDC_CONVDISK_140K, fDiskSizeIdx);
    //DDX_Check(pDX, IDC_CONVDISK_ALLOWLOWER, fAllowLower);
    //DDX_Check(pDX, IDC_CONVDISK_SPARSE, fSparseAlloc);
    DDX_Text(pDX, IDC_CONVDISK_VOLNAME, fVolName);
    DDX_Text(pDX, IDC_CONVDISK_SPECIFY_EDIT, specifyBlocks);

    ASSERT(fDiskSizeIdx >= 0 && fDiskSizeIdx < (int)NewDiskSize::GetNumSizeEntries());

    if (pDX->m_bSaveAndValidate) {

        fNumBlocks = NewDiskSize::GetDiskSizeByIndex(fDiskSizeIdx);
        if (fNumBlocks == NewDiskSize::kSpecified) {
            fNumBlocks = specifyBlocks;

            // Max is really 65535, but we allow 65536 for creation of volumes
            // that can be copied to CFFA cards.
            if (specifyBlocks < 16 || specifyBlocks > 65536)
                errMsg = "Specify a size of at least 16 blocks and no more"
                          " than 65536 blocks.";
        }


        if (fVolName.IsEmpty() || fVolName.GetLength() > kProDOSVolNameMax) {
            errMsg = "You must specify a volume name 1-15 characters long.";
        } else {
            if (!IsValidVolumeName_ProDOS(fVolName))
                errMsg.LoadString(IDS_VALID_VOLNAME_PRODOS);
        }
    }

    if (!errMsg.IsEmpty()) {
        CString appName;
        appName.LoadString(IDS_MB_APP_NAME);
        MessageBox(errMsg, appName, MB_OK);
        pDX->Fail();
    }

    UseSelectionDialog::DoDataExchange(pDX);
}

/*
 * When one of the radio buttons is clicked on, update the active status
 * and contents of the "specify size" edit box.
 */
void
ConvDiskOptionsDialog::OnRadioChangeRange(UINT nID)
{
    WMSG1("OnChangeRange id=%d\n", nID);

    CButton* pButton = (CButton*) GetDlgItem(IDC_CONVDISK_SPECIFY);
    CEdit* pEdit = (CEdit*) GetDlgItem(IDC_CONVDISK_SPECIFY_EDIT);
    pEdit->EnableWindow(pButton->GetCheck() == BST_CHECKED);

    NewDiskSize::UpdateSpecifyEdit(this);
}

/*
 * Test a ProDOS filename for validity.
 */
bool
ConvDiskOptionsDialog::IsValidVolumeName_ProDOS(const WCHAR* name)
{
    CStringA nameA(name);
    return DiskImgLib::DiskFSProDOS::IsValidVolumeName(nameA);
}


/*
 * Enable all size radio buttons and reset the "size required" display.
 *
 * This should be invoked whenever the convert selection changes, and may be
 * called at any time.
 */
void
ConvDiskOptionsDialog::ResetSizeControls(void)
{
    CWnd* pWnd;
    CString spaceReq;

    WMSG0("Resetting size controls\n");
    spaceReq.Format(IDS_CONVDISK_SPACEREQ, "(unknown)");
    pWnd = GetDlgItem(IDC_CONVDISK_SPACEREQ);
    ASSERT(pWnd != NULL);
    pWnd->SetWindowText(spaceReq);

#if 0
    int i;
    for (i = 0; i < NELEM(gDiskSizes); i++) {
        pWnd = GetDlgItem(gDiskSizes[i].ctrlID);
        ASSERT(pWnd != NULL);
        pWnd->EnableWindow(TRUE);
    }
#endif
    NewDiskSize::EnableButtons(this);
}

/*
 * Display the space requirements and disable radio button controls that are
 * for values that are too small.
 *
 * Pass in the number of blocks required on a 32MB ProDOS volume.
 */
void
ConvDiskOptionsDialog::LimitSizeControls(long totalBlocks, long blocksUsed)
{
    WMSG2("LimitSizeControls %ld %ld\n", totalBlocks, blocksUsed);
    WMSG1("Full volume requires %ld bitmap blocks\n",
        NewDiskSize::GetNumBitmapBlocks_ProDOS(totalBlocks));

    CWnd* pWnd;
    long usedWithoutBitmap =
        blocksUsed - NewDiskSize::GetNumBitmapBlocks_ProDOS(totalBlocks);
    long sizeInK = usedWithoutBitmap / 2;
    CString sizeStr, spaceReq;
    sizeStr.Format(L"%dK", sizeInK);
    spaceReq.Format(IDS_CONVDISK_SPACEREQ, sizeStr);

    pWnd = GetDlgItem(IDC_CONVDISK_SPACEREQ);
    ASSERT(pWnd != NULL);
    pWnd->SetWindowText(spaceReq);

    NewDiskSize::EnableButtons_ProDOS(this, totalBlocks, blocksUsed);

#if 0
    bool first = true;
    for (int i = 0; i < NELEM(gDiskSizes); i++) {
        if (gDiskSizes[i].blocks == -1)
            continue;

        CButton* pButton;
        pButton = (CButton*) GetDlgItem(gDiskSizes[i].ctrlID);
        ASSERT(pButton != NULL);
        if (usedWithoutBitmap + GetNumBitmapBlocks(gDiskSizes[i].blocks) <=
            gDiskSizes[i].blocks)
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
#endif
}


/*
 * Compute the amount of space required for the files.  We use the result to
 * disable the controls that can't be used.
 *
 * We don't need to enable controls here, because the only way to change the
 * set of files is by flipping between "all" and "selected", and we can handle
 * that separately.
 */
void
ConvDiskOptionsDialog::OnCompute(void)
{
    MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
    const Preferences* pPreferences = GET_PREFERENCES();

    if (UpdateData() == FALSE)
        return;

    /*
     * Create a "selection set" of data forks, resource forks, and
     * disk images.  We don't want comment threads.  We can filter all that
     * out later, though, so we just specify "any".
     */
    SelectionSet selSet;
    int threadMask = GenericEntry::kAnyThread;

    if (fFilesToAction == UseSelectionDialog::kActionSelection) {
        selSet.CreateFromSelection(pMain->GetContentList(), threadMask);
    } else {
        selSet.CreateFromAll(pMain->GetContentList(), threadMask);
    }

    if (selSet.GetNumEntries() == 0) {
        /* should be impossible */
        MessageBox(L"No files matched the selection criteria.",
            L"No match", MB_OK|MB_ICONEXCLAMATION);
        return;
    }

    XferFileOptions xferOpts;
    //xferOpts.fAllowLowerCase =
    //  pPreferences->GetPrefBool(kPrProDOSAllowLower) != 0;
    //xferOpts.fUseSparseBlocks =
    //  pPreferences->GetPrefBool(kPrProDOSUseSparse) != 0;

    WMSG1("New volume name will be '%ls'\n", fVolName);

    /*
     * Create a new disk image file.
     */
    CString errStr;
    WCHAR nameBuf[MAX_PATH];
    UINT unique;
    unique = GetTempFileName(pMain->GetPreferences()->GetPrefString(kPrTempPath),
                L"CPdisk", 0, nameBuf);
    if (unique == 0) {
        DWORD dwerr = ::GetLastError();
        errStr.Format(L"GetTempFileName failed on '%ls' (err=0x%08lx)\n",
            pMain->GetPreferences()->GetPrefString(kPrTempPath), dwerr);
        ShowFailureMsg(this, errStr, IDS_FAILED);
        return;
    }
    WMSG1(" Will xfer to file '%ls'\n", nameBuf);
    // annoying -- DiskArchive insists on creating it
    (void) _wunlink(nameBuf);

    DiskArchive::NewOptions options;
    memset(&options, 0, sizeof(options));
    options.base.format = DiskImg::kFormatProDOS;
    options.base.sectorOrder = DiskImg::kSectorOrderProDOS;
    options.prodos.volName = fVolName;
    options.prodos.numBlocks = 65535;

    xferOpts.fTarget = new DiskArchive;

    {
        CWaitCursor waitc;
        errStr = xferOpts.fTarget->New(nameBuf, &options);
    }
    if (!errStr.IsEmpty()) {
        ShowFailureMsg(this, errStr, IDS_FAILED);
    } else {
        /*
         * Set up the progress window as a modal dialog.
         */
        GenericArchive::XferStatus result;

        ActionProgressDialog* pActionProgress = new ActionProgressDialog;
        pMain->SetActionProgressDialog(pActionProgress);
        pActionProgress->Create(ActionProgressDialog::kActionConvFile, this);
        pMain->PeekAndPump();
        result = pMain->GetOpenArchive()->XferSelection(pActionProgress, &selSet,
                    pActionProgress, &xferOpts);
        pActionProgress->Cleanup(this);
        pMain->SetActionProgressDialog(NULL);

        if (result == GenericArchive::kXferOK) {
            DiskFS* pDiskFS;
            long totalBlocks, freeBlocks;
            int unitSize;
            DIError dierr;

            WMSG0("SUCCESS\n");

            pDiskFS = ((DiskArchive*) xferOpts.fTarget)->GetDiskFS();
            ASSERT(pDiskFS != NULL);

            dierr = pDiskFS->GetFreeSpaceCount(&totalBlocks, &freeBlocks,
                        &unitSize);
            if (dierr != kDIErrNone) {
                errStr.Format(L"Unable to get free space count: %hs.\n",
                    DiskImgLib::DIStrError(dierr));
                ShowFailureMsg(this, errStr, IDS_FAILED);
            } else {
                ASSERT(totalBlocks >= freeBlocks);
                ASSERT(unitSize == DiskImgLib::kBlockSize);
                LimitSizeControls(totalBlocks, totalBlocks - freeBlocks);
            }
        } else if (result == GenericArchive::kXferCancelled) {
            WMSG0("CANCEL - cancel button hit\n");
            ResetSizeControls();
        } else {
            WMSG1("FAILURE (result=%d)\n", result);
            ResetSizeControls();
        }
    }

    // debug
    ((DiskArchive*) (xferOpts.fTarget))->GetDiskFS()->DumpFileList();

    /* clean up */
    delete xferOpts.fTarget;
    (void) _wunlink(nameBuf);
}
