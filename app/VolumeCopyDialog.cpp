/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Dialog that allows copying volumes or sub-volumes to and from files on
 * disk.
 *
 * NOTE: we probably shouldn't allow copying volumes that start at block 0
 * and are equal to the DiskImgLib limit on sizes (e.g. 8GB).  We'd be
 * copying a partial volume, which doesn't make sense.
 */
#include "stdafx.h"
#include "VolumeCopyDialog.h"
#include "Main.h"
#include "../reformat/Charset.h"


BEGIN_MESSAGE_MAP(VolumeCopyDialog, CDialog)
    ON_COMMAND(IDHELP, OnHelp)
    ON_COMMAND(IDC_VOLUEMCOPYSEL_TOFILE, OnCopyToFile)
    ON_COMMAND(IDC_VOLUEMCOPYSEL_FROMFILE, OnCopyFromFile)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_VOLUMECOPYSEL_LIST, OnListChange)
    ON_MESSAGE(WMU_DIALOG_READY, OnDialogReady)
END_MESSAGE_MAP()


/*
 * Sub-class the generic libutil CancelDialog class.
 */
class VolumeXferProgressDialog : public ProgressCancelDialog {
public:
    BOOL Create(CWnd* pParentWnd = NULL) {
        fAbortOperation = false;
        return ProgressCancelDialog::Create(&fAbortOperation,
                IDD_VOLUMECOPYPROG, IDC_VOLUMECOPYPROG_PROGRESS, pParentWnd);
    }

    void SetCurrentFiles(const WCHAR* fromName, const WCHAR* toName) {
        CWnd* pWnd = GetDlgItem(IDC_VOLUMECOPYPROG_FROM);
        ASSERT(pWnd != NULL);
        pWnd->SetWindowText(fromName);
        pWnd = GetDlgItem(IDC_VOLUMECOPYPROG_TO);
        ASSERT(pWnd != NULL);
        pWnd->SetWindowText(toName);
    }

private:
    void OnOK(void) override {
        LOGI("Ignoring VolumeXferProgressDialog OnOK");
    }

    MainWindow* GetMainWindow(void) const {
        return (MainWindow*)::AfxGetMainWnd();
    }

    bool fAbortOperation;
};


BOOL VolumeCopyDialog::OnInitDialog(void)
{
    /*
     * Scan the source image.
     */

    CRect rect;

    //this->GetWindowRect(&rect);
    //LOGI("RECT is %d, %d, %d, %d", rect.left, rect.top, rect.bottom, rect.right);

    ASSERT(fpDiskImg != NULL);
    ScanDiskInfo(false);

    CDialog::OnInitDialog();        // does DDX init

    CButton* pButton;
    pButton = (CButton*) GetDlgItem(IDC_VOLUEMCOPYSEL_FROMFILE);
    pButton->EnableWindow(FALSE);
    pButton = (CButton*) GetDlgItem(IDC_VOLUEMCOPYSEL_TOFILE);
    pButton->EnableWindow(FALSE);

    CString newTitle;
    GetWindowText(newTitle);
    newTitle += " - ";
    newTitle += fPathName;
    SetWindowText(newTitle);

    /*
     * Prep the listview control.
     *
     * Columns:
     *  [icon] Volume name | Format | Size (MB/GB) | Block count
     */
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUMECOPYSEL_LIST);
    ASSERT(pListView != NULL);
    ListView_SetExtendedListViewStyleEx(pListView->m_hWnd,
        LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    int width1, width2, width3;
    //CRect rect;

    pListView->GetClientRect(&rect);
    width1 = pListView->GetStringWidth(L"XXVolume NameXXmmmmm");
    width2 = pListView->GetStringWidth(L"XXFormatXXmmmmmmmmmm");
    width3 = pListView->GetStringWidth(L"XXSizeXXmmm");
    //width4 = pListView->GetStringWidth("XXBlock CountXX");

    pListView->InsertColumn(0, L"Volume Name", LVCFMT_LEFT, width1);
    pListView->InsertColumn(1, L"Format", LVCFMT_LEFT, width2);
    pListView->InsertColumn(2, L"Size", LVCFMT_LEFT, width3);
    pListView->InsertColumn(3, L"Block Count", LVCFMT_LEFT,
        rect.Width() - (width1+width2+width3)
        - ::GetSystemMetrics(SM_CXVSCROLL) );

    /* add images for list; this MUST be loaded before header images */
    LoadListImages();
    pListView->SetImageList(&fListImageList, LVSIL_SMALL);

    LoadList();

    CenterWindow();

    int cc = PostMessage(WMU_DIALOG_READY, 0, 0);
    ASSERT(cc != 0);

    return TRUE;
}

void VolumeCopyDialog::OnOK(void)
{
    /*
     * We need to make sure we throw out the DiskFS we created before the modal
     * dialog exits.  This is necessary because we rely on an external DiskImg,
     * and create DiskFS objects that point to it.
     */
    Cleanup();
    CDialog::OnOK();
}

void VolumeCopyDialog::OnCancel(void)
{
    Cleanup();
    CDialog::OnCancel();
}

void VolumeCopyDialog::Cleanup(void)
{
    LOGD("  VolumeCopyDialog is done, cleaning up DiskFS");
    delete fpDiskFS;
    fpDiskFS = NULL;
}

void VolumeCopyDialog::OnListChange(NMHDR*, LRESULT* pResult)
{
    //CRect rect;
    //this->GetWindowRect(&rect);
    //LOGI("RECT is %d, %d, %d, %d", rect.left, rect.top, rect.bottom, rect.right);

    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUMECOPYSEL_LIST);
    ASSERT(pListView != NULL);
    CButton* pButton;
    UINT selectedCount;

    selectedCount = pListView->GetSelectedCount();
    
    pButton = (CButton*) GetDlgItem(IDC_VOLUEMCOPYSEL_TOFILE);
    pButton->EnableWindow(selectedCount != 0);

    if (!fpDiskImg->GetReadOnly()) {
        pButton = (CButton*) GetDlgItem(IDC_VOLUEMCOPYSEL_FROMFILE);
        pButton->EnableWindow(selectedCount != 0);
    }

    *pResult = 0;
}

void VolumeCopyDialog::ScanDiskInfo(bool scanTop)
{
    /*
     * The top-level disk image should already have been analyzed and the
     * format overridden (if necessary).  We don't want to do it in here the
     * first time around because the "override" dialog screws up placement
     * of our dialog box.  I guess opening windows from inside OnInitDialog
     * isn't expected.  Annoying.  [Um, maybe we could call CenterWindow??
     * Actually, now I'm a little concerned about modal dialogs coming and
     * going while we're in OnInitDialog, because MainWindow is disabled and
     * we're not yet enabled. ++ATM]
     */

    const Preferences* pPreferences = GET_PREFERENCES();
    MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
    DIError dierr;
    CString errMsg, failed;

    assert(fpDiskImg != NULL);
    assert(fpDiskFS == NULL);

    if (scanTop) {
        DiskImg::FSFormat oldFormat;
        oldFormat = fpDiskImg->GetFSFormat();

        /* check to see if the top-level FS has changed */
        fpDiskImg->AnalyzeImageFS();

        /*
         * If requested (or necessary), verify the format.  We only do this
         * if we think the format has changed.  This is possible, e.g. if
         * somebody drops an MS-DOS volume into the first partition of a
         * CFFA disk.
         */
        if (oldFormat != fpDiskImg->GetFSFormat() &&
                (fpDiskImg->GetFSFormat() == DiskImg::kFormatUnknown ||
                fpDiskImg->GetSectorOrder() == DiskImg::kSectorOrderUnknown ||
                pPreferences->GetPrefBool(kPrQueryImageFormat)))
        {
            // ignore them if they hit "cancel"
            (void) pMain->TryDiskImgOverride(fpDiskImg, fPathName,
                DiskImg::kFormatUnknown, NULL, true, &errMsg);
            if (!errMsg.IsEmpty()) {
                ShowFailureMsg(this, errMsg, IDS_FAILED);
                return;
            }
        }
    }

    /*
     * Creating the "busy" window here is problematic, because we get called
     * from OnInitDialog, at which point our window isn't yet established.
     * Since we're modal, we disabled MainWindow, which means that when the
     * "busy" box goes away there's no CiderPress window to take control.  As
     * a result we get a nasty flash.
     *
     * The only way around this is to defer the destruction of the modeless
     * dialog until after we become visible.
     */
    bool deferDestroy = false;
    if (!IsWindowVisible() || !IsWindowEnabled()) {
        LOGI("  Deferring destroy on wait dialog");
        deferDestroy = true;
    } else {
        LOGI("  Not deferring destroy on wait dialog");
    }

    fpWaitDlg = new ExclusiveModelessDialog;
    fpWaitDlg->Create(IDD_LOADING, this);
    fpWaitDlg->CenterWindow(pMain);
    pMain->PeekAndPump();

    CWaitCursor waitc;

    /*
     * Create an appropriate DiskFS object.  We only need to do this to get
     * the sub-volume info, which is unfortunate since it can be slow.
     */
    fpDiskFS = fpDiskImg->OpenAppropriateDiskFS(true);
    if (fpDiskFS == NULL) {
        LOGI("HEY: OpenAppropriateDiskFS failed!");
        /* this is fatal, but there's no easy way to die */
        /* (could we do a DestroyWindow from here?) */
        /* at any rate, with "allowUnknown" set, this shouldn't happen */
    } else {
        fpDiskFS->SetScanForSubVolumes(DiskFS::kScanSubContainerOnly);

        dierr = fpDiskFS->Initialize(fpDiskImg, DiskFS::kInitFull);
        if (dierr != kDIErrNone) {
            CString appName, msg;
            CheckedLoadString(&appName, IDS_MB_APP_NAME);
            msg.Format(L"Warning: error during disk scan: %hs.",
                DiskImgLib::DIStrError(dierr));
            fpWaitDlg->MessageBox(msg, appName, MB_OK | MB_ICONEXCLAMATION);
            /* keep going */
        }
    }

    if (!deferDestroy && fpWaitDlg != NULL) {
        fpWaitDlg->DestroyWindow();
        fpWaitDlg = NULL;
    }

    return;
}

LONG VolumeCopyDialog::OnDialogReady(UINT, LONG)
{
    if (fpWaitDlg != NULL) {
        LOGI("OnDialogReady found active window, destroying");
        fpWaitDlg->DestroyWindow();
        fpWaitDlg = NULL;
    }
    return 0;
}

void VolumeCopyDialog::LoadList(void)
{
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUMECOPYSEL_LIST);
    ASSERT(pListView != NULL);
    int itemIndex = 0;

    CString unknown = "(unknown)";

    pListView->DeleteAllItems();
    if (fpDiskFS == NULL) {
        /* can only happen if imported volume is unrecognizeable */
        return;
    }

    AddToList(pListView, fpDiskImg, fpDiskFS, &itemIndex);

    DiskImgLib::DiskFS::SubVolume* pSubVolume;
    pSubVolume = fpDiskFS->GetNextSubVolume(NULL);
    while (pSubVolume != NULL) {
        if (pSubVolume->GetDiskFS() == NULL) {
            LOGI("WARNING: sub-volume DiskFS is NULL?!");
            assert(false);
        } else {
            AddToList(pListView, pSubVolume->GetDiskImg(),
                pSubVolume->GetDiskFS(), &itemIndex);
        }
        pSubVolume = fpDiskFS->GetNextSubVolume(pSubVolume);
    } 
}

void VolumeCopyDialog::AddToList(CListCtrl* pListView, DiskImg* pDiskImg,
    DiskFS* pDiskFS, int* pIndex)
{
    CString sizeStr, blocksStr;

    assert(pListView != NULL);
    assert(pDiskImg != NULL);
    assert(pDiskFS != NULL);
    assert(pIndex != NULL);

    long numBlocks = pDiskImg->GetNumBlocks();

    CString volName(Charset::ConvertMORToUNI(pDiskFS->GetVolumeName()));
    CString format = DiskImg::ToString(pDiskImg->GetFSFormat());
    blocksStr.Format(L"%ld", pDiskImg->GetNumBlocks());
    if (numBlocks > 1024*1024*2)
        sizeStr.Format(L"%.2fGB", (double) numBlocks / (1024.0*1024.0*2.0));
    else if (numBlocks > 1024*2)
        sizeStr.Format(L"%.2fMB", (double) numBlocks / (1024.0*2.0));
    else
        sizeStr.Format(L"%.2fKB", (double) numBlocks / 2.0);

    /* add entry; first entry is the whole volume */
    pListView->InsertItem(*pIndex, volName,
        *pIndex == 0 ? kListIconVolume : kListIconSubVolume);
    pListView->SetItemText(*pIndex, 1, format);
    pListView->SetItemText(*pIndex, 2, sizeStr);
    pListView->SetItemText(*pIndex, 3, blocksStr);
    pListView->SetItemData(*pIndex, (DWORD) pDiskFS);
    (*pIndex)++;
}

bool VolumeCopyDialog::GetSelectedDisk(DiskImg** ppDiskImg, DiskFS** ppDiskFS)
{
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUMECOPYSEL_LIST);
    ASSERT(pListView != NULL);

    ASSERT(ppDiskImg != NULL);
    ASSERT(ppDiskFS != NULL);

    if (pListView->GetSelectedCount() != 1)
        return false;

    POSITION posn;
    posn = pListView->GetFirstSelectedItemPosition();
    if (posn == NULL) {
        ASSERT(false);
        return false;
    }
    int num = pListView->GetNextSelectedItem(posn);
    DWORD data = pListView->GetItemData(num);

    *ppDiskFS = (DiskFS*) data;
    assert(*ppDiskFS != NULL);
    *ppDiskImg = (*ppDiskFS)->GetDiskImg();
    return true;
}

void VolumeCopyDialog::OnCopyToFile(void)
{
    VolumeXferProgressDialog* pProgressDialog = NULL;
    Preferences* pPreferences = GET_PREFERENCES_WR();
    MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
    DiskImg::FSFormat originalFormat = DiskImg::kFormatUnknown;
    DiskImg* pSrcImg = NULL;
    DiskFS* pSrcFS = NULL;
    DiskImg dstImg;
    DIError dierr;
    CString errMsg, saveName, msg;
    int result;

    result = GetSelectedDisk(&pSrcImg, &pSrcFS);
    if (!result)
        return;
    assert(pSrcImg != NULL);
    assert(pSrcFS != NULL);

    CString srcName(Charset::ConvertMORToUNI(pSrcFS->GetVolumeName()));

    /* force the format to be generic ProDOS-ordered blocks */
    originalFormat = pSrcImg->GetFSFormat();
    dierr = pSrcImg->OverrideFormat(pSrcImg->GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, pSrcImg->GetSectorOrder());
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Internal error: couldn't switch to generic ProDOS: %hs.",
                DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    LOGI("Logical volume '%ls' has %d 512-byte blocks",
        (LPCWSTR) srcName, pSrcImg->GetNumBlocks());

    /*
     * Select file to write blocks to.
     */
    {
        CFileDialog saveDlg(FALSE, L"po", NULL,
            OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN|OFN_HIDEREADONLY,
            L"All Files (*.*)|*.*||", this);

        CString saveFolder;

        saveDlg.m_ofn.lpstrTitle = L"New disk image (.po)";
        saveDlg.m_ofn.lpstrInitialDir =
            pPreferences->GetPrefString(kPrOpenArchiveFolder);
    
        if (saveDlg.DoModal() != IDOK) {
            LOGI(" User bailed out of image save dialog");
            goto bail;
        }

        saveFolder = saveDlg.m_ofn.lpstrFile;
        saveFolder = saveFolder.Left(saveDlg.m_ofn.nFileOffset);
        pPreferences->SetPrefString(kPrOpenArchiveFolder, saveFolder);

        saveName = saveDlg.GetPathName();
    }
    LOGI("File will be saved to '%ls'", (LPCWSTR) saveName);

    /* DiskImgLib does not like it if file already exists */
    errMsg = pMain->RemoveFile(saveName);
    if (!errMsg.IsEmpty()) {
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /*
     * Create a block image with the expected number of blocks.
     */
    int dstNumBlocks;
    dstNumBlocks = pSrcImg->GetNumBlocks();

    {
        ExclusiveModelessDialog* pWaitDlg = new ExclusiveModelessDialog;
        pWaitDlg->Create(IDD_FORMATTING, this);
        pWaitDlg->CenterWindow(pMain);
        pMain->PeekAndPump();   // redraw
        CWaitCursor waitc;

        CStringA saveNameA(saveName);
        dierr = dstImg.CreateImage(saveNameA, NULL,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatSectors,
                    NULL,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    dstNumBlocks,
                    true /* don't need to erase contents */);

        pWaitDlg->DestroyWindow();
        //pMain->PeekAndPump(); // redraw
    }
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Couldn't create disk image: %hs.",
                DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /* initialize cancel dialog, and disable main window */
    pProgressDialog = new VolumeXferProgressDialog;
    EnableWindow(FALSE);
    if (pProgressDialog->Create(this) == FALSE) {
        LOGI("Progress dialog init failed?!");
        ASSERT(false);
        goto bail;
    }
    pProgressDialog->SetCurrentFiles(srcName, saveName);

    time_t startWhen, endWhen;
    startWhen = time(NULL);

    /*
     * Do the actual block copy.
     */
    dierr = pMain->CopyDiskImage(&dstImg, pSrcImg, false, false, pProgressDialog);
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrCancelled) {
            CheckedLoadString(&errMsg, IDS_OPERATION_CANCELLED);
            ShowFailureMsg(pProgressDialog, errMsg, IDS_CANCELLED);
            // remove the partially-written file
            dstImg.CloseImage();
            (void) _wunlink(saveName);
        } else {
            errMsg.Format(L"Copy failed: %hs.", DiskImgLib::DIStrError(dierr));
            ShowFailureMsg(pProgressDialog, errMsg, IDS_FAILED);
        }
        goto bail;
    }

    dierr = dstImg.CloseImage();
    if (dierr != kDIErrNone) {
        errMsg.Format(L"ERROR: dstImg close failed (err=%d)\n", dierr);
        ShowFailureMsg(pProgressDialog, errMsg, IDS_FAILED);
        goto bail;
    }

    /* put elapsed time in the debug log */
    endWhen = time(NULL);
    float elapsed;
    if (endWhen == startWhen)
        elapsed = 1.0;
    else
        elapsed = (float) (endWhen - startWhen);
    msg.Format(L"Copied %ld blocks in %ld seconds (%.2fKB/sec)",
        pSrcImg->GetNumBlocks(), (long) (endWhen - startWhen),
        (pSrcImg->GetNumBlocks() / 2.0) / elapsed);
    LOGI("%ls", (LPCWSTR) msg);
#ifdef _DEBUG
    pProgressDialog->MessageBox(msg, L"DEBUG: elapsed time", MB_OK);
#endif

    pMain->SuccessBeep();


bail:
    // restore the dialog window to prominence
    EnableWindow(TRUE);
    //SetActiveWindow();
    if (pProgressDialog != NULL)
        pProgressDialog->DestroyWindow();

    /* un-override the source disk */
    if (originalFormat != DiskImg::kFormatUnknown) {
        dierr = pSrcImg->OverrideFormat(pSrcImg->GetPhysicalFormat(),
                    originalFormat, pSrcImg->GetSectorOrder());
        if (dierr != kDIErrNone) {
            LOGI("ERROR: couldn't un-override source image (dierr=%d)", dierr);
            // not much else to do; should be okay
        }
    }
    return;
}

void VolumeCopyDialog::OnCopyFromFile(void)
{
    VolumeXferProgressDialog* pProgressDialog = NULL;
    Preferences* pPreferences = GET_PREFERENCES_WR();
    MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
    //DiskImg::FSFormat originalFormat = DiskImg::kFormatUnknown;
    CString openFilters;
    CString loadName, errMsg, warning;
    DiskImg* pDstImg = NULL;
    DiskFS* pDstFS = NULL;
    DiskImg srcImg;
    DIError dierr;
    int result;
    bool needReload = false;
    bool isPartial = false;

    CheckedLoadString(&warning, IDS_WARNING);

    /*
     * Get the DiskImg and DiskFS pointers for the selected partition out of
     * the control.  The storage for these is part of fpDiskFS, which holds
     * the tree of subvolumes.
     */
    result = GetSelectedDisk(&pDstImg, &pDstFS);
    if (!result)
        return;

    CString targetName(Charset::ConvertMORToUNI(pDstFS->GetVolumeName()));


    /*
     * Select the image to copy from.
     */
    openFilters = MainWindow::kOpenDiskImage;
    openFilters += MainWindow::kOpenAll;
    openFilters += MainWindow::kOpenEnd;
    CFileDialog dlg(TRUE, L"dsk", NULL, OFN_FILEMUSTEXIST, openFilters, this);

    /* source file gets opened read-only */
    dlg.m_ofn.Flags |= OFN_HIDEREADONLY;
    dlg.m_ofn.lpstrTitle = L"Select image to copy from";
    dlg.m_ofn.lpstrInitialDir = pPreferences->GetPrefString(kPrOpenArchiveFolder);

    if (dlg.DoModal() != IDOK)
        goto bail;
    loadName = dlg.GetPathName();

    {
        CWaitCursor waitc;
        CString saveFolder;

        /* open the image file and analyze it */
        CStringA loadNameA(loadName);
        dierr = srcImg.OpenImage(loadNameA, PathProposal::kLocalFssep, true);
        if (dierr != kDIErrNone) {
            errMsg.Format(L"Unable to open disk image: %hs.",
                DiskImgLib::DIStrError(dierr));
            ShowFailureMsg(this, errMsg, IDS_FAILED);
            goto bail;
        }

        if (srcImg.AnalyzeImage() != kDIErrNone) {
            errMsg.Format(L"The file '%ls' doesn't seem to hold a valid disk image.",
                (LPCWSTR) loadName);
            ShowFailureMsg(this, errMsg, IDS_FAILED);
            goto bail;
        }

        // save our folder choice in the preferences file
        saveFolder = dlg.m_ofn.lpstrFile;
        saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
        pPreferences->SetPrefString(kPrOpenArchiveFolder, saveFolder);
    }

    /*
     * Require that the input be block-addressable.  This isn't really the
     * right test, because it's conceivable that somebody would want to put
     * a nibble image onto a disk volume.  I can't think of a good reason
     * to do this -- you can't just splat a fixed-track-length .NIB file
     * onto a 5.25" disk, assuming you could get the drive to work on a PC
     * in the first place -- so I'm going to take the simple way out.  The
     * right test is to verify that the EOF on the input is the same as the
     * EOF on the output.
     */
    if (!srcImg.GetHasBlocks()) {
        errMsg = L"The disk image must be block-oriented.  Nibble images"
                 L" cannot be copied.";
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /* force source volume to generic ProDOS blocks */
    dierr = srcImg.OverrideFormat(srcImg.GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, srcImg.GetSectorOrder());
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Internal error: couldn't switch source to generic ProDOS: %hs.",
                DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    LOGI("Source image '%ls' has %d 512-byte blocks",
        (LPCWSTR) loadName, srcImg.GetNumBlocks());

    LOGI("Target volume has %d 512-byte blocks", pDstImg->GetNumBlocks());

    if (srcImg.GetNumBlocks() > pDstImg->GetNumBlocks()) {
        errMsg.Format(L"Error: the disk image file has %ld blocks, but the"
                      L" target volume holds %ld blocks.  The target must"
                      L" have more space than the input file.",
            srcImg.GetNumBlocks(), pDstImg->GetNumBlocks());
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    if (pDstImg->GetNumBlocks() >= DiskImgLib::kVolumeMaxBlocks) {
        // TODO: re-evaluate this limitation
        errMsg.Format(L"Error: for safety reasons, copying disk images to"
                      L" larger volumes is not supported when the target"
                      L" is 8GB or larger.");
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    if (srcImg.GetNumBlocks() != pDstImg->GetNumBlocks()) {
        //errMsg.LoadString(IDS_WARNING);
        errMsg.Format(L"The disk image file has %ld blocks, but the target"
                      L" volume holds %ld blocks.  The leftover space may be"
                      L" wasted, and non-ProDOS volumes may not be identified"
                      L" correctly.  Do you wish to continue?",
            srcImg.GetNumBlocks(), pDstImg->GetNumBlocks());
        result = MessageBox(errMsg, warning, MB_OKCANCEL | MB_ICONQUESTION);
        if (result != IDOK) {
            LOGI("User chickened out of oversized disk copy");
            goto bail;
        }
        isPartial = true;
    }

    //errMsg.LoadString(IDS_WARNING);
    errMsg.Format(L"You are about to overwrite volume %ls with the"
                  L" contents of '%ls'.  This will destroy all data on"
                  L" %ls.  Are you sure you wish to continue?",
        (LPCWSTR) targetName, (LPCWSTR) loadName, (LPCWSTR) targetName);
    result = MessageBox(errMsg, warning, MB_OKCANCEL | MB_ICONEXCLAMATION);
    if (result != IDOK) {
        LOGI("User chickened out of disk copy");
        goto bail;
    }

    /* force the target disk image to be generic ProDOS-ordered blocks */
    dierr = pDstImg->OverrideFormat(pDstImg->GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, pDstImg->GetSectorOrder());
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Internal error: couldn't switch target to generic ProDOS: %hs.",
                DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /* from here on out, before we exit we must re-analyze this volume */
    needReload = true;

    // redraw main to erase previous dialog
    pMain->PeekAndPump();

    /* initialize cancel dialog, and disable dialog */
    pProgressDialog = new VolumeXferProgressDialog;
    EnableWindow(FALSE);
    if (pProgressDialog->Create(this) == FALSE) {
        LOGI("Progress dialog init failed?!");
        ASSERT(false);
        return;
    }
//  if (pDstFS == NULL)
//      pProgressDialog->SetCurrentFiles(loadName, "target");
//  else
        pProgressDialog->SetCurrentFiles(loadName, targetName);

    /*
     * We want to delete fpDiskFS now, but we can't because it's holding the
     * storage for the DiskImg/DiskFS pointers in the subvolume list.  We
     * flush it to ensure that it won't try to write to the disk after the
     * copy completes.
     */
    fpDiskFS->Flush(DiskImg::kFlushAll);

    time_t startWhen, endWhen;
    startWhen = time(NULL);

    /*
     * Do the actual block copy.
     */
    dierr = pMain->CopyDiskImage(pDstImg, &srcImg, false, isPartial,
                pProgressDialog);
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrCancelled) {
            CheckedLoadString(&errMsg, IDS_OPERATION_CANCELLED);
            ShowFailureMsg(pProgressDialog, errMsg, IDS_CANCELLED);
        } else {
            errMsg.Format(L"Copy failed: %hs.", DiskImgLib::DIStrError(dierr));
            ShowFailureMsg(pProgressDialog, errMsg, IDS_FAILED);
        }
        goto bail;
    }

    dierr = srcImg.CloseImage();
    if (dierr != kDIErrNone) {
        errMsg.Format(L"ERROR: srcImg close failed (err=%d)\n", dierr);
        ShowFailureMsg(pProgressDialog, errMsg, IDS_FAILED);
        goto bail;
    }

    endWhen = time(NULL);
    float elapsed;
    if (endWhen == startWhen)
        elapsed = 1.0;
    else
        elapsed = (float) (endWhen - startWhen);
    errMsg.Format(L"Copied %ld blocks in %ld seconds (%.2fKB/sec)",
        srcImg.GetNumBlocks(), (long) (endWhen - startWhen),
        (srcImg.GetNumBlocks() / 2.0) / elapsed);
    LOGI("%ls", (LPCWSTR) errMsg);
#ifdef _DEBUG
    pProgressDialog->MessageBox(errMsg, L"DEBUG: elapsed time", MB_OK);
#endif

    pMain->SuccessBeep();

    /*
     * If a DiskFS insists on privately caching stuff (e.g. libhfs), we could
     * end up corrupting the image we just wrote.  We use SetAllReadOnly() to
     * ensure that nothing will be written before we delete the DiskFS.
     */
    assert(!fpDiskImg->GetReadOnly());
    fpDiskFS->SetAllReadOnly(true);
    delete fpDiskFS;
    fpDiskFS = NULL;
    assert(fpDiskImg->GetReadOnly());
    fpDiskImg->SetReadOnly(false);

bail:
    // restore the dialog window to prominence
    EnableWindow(TRUE);
    //SetActiveWindow();
    if (pProgressDialog != NULL)
        pProgressDialog->DestroyWindow();

    /*
     * Force a reload.  We need to reload the disk information, then reload
     * the list contents.
     *
     * By design, anything that would require un-overriding the format of
     * the target DiskImg requires reloading it completely.  Sort of heavy-
     * handed, but it's reliable.
     */
    if (needReload) {
        LOGI("RELOAD dialog");
        ScanDiskInfo(true);     // reopens fpDiskFS
        LoadList();

        /* will we need to reopen the currently-open file list archive? */
        if (pMain->IsOpenPathName(fPathName))
            pMain->SetReopenFlag();
    }
    return;
}
