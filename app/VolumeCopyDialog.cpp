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
#include "HelpTopics.h"
#include "Main.h"


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

    void SetCurrentFiles(const char* fromName, const char* toName) {
        CWnd* pWnd = GetDlgItem(IDC_VOLUMECOPYPROG_FROM);
        ASSERT(pWnd != nil);
        pWnd->SetWindowText(fromName);
        pWnd = GetDlgItem(IDC_VOLUMECOPYPROG_TO);
        ASSERT(pWnd != nil);
        pWnd->SetWindowText(toName);
    }

private:
    void OnOK(void) {
        WMSG0("Ignoring VolumeXferProgressDialog OnOK\n");
    }

    MainWindow* GetMainWindow(void) const {
        return (MainWindow*)::AfxGetMainWnd();
    }

    bool fAbortOperation;
};


/*
 * Scan the source image.
 */
BOOL
VolumeCopyDialog::OnInitDialog(void)
{
    CRect rect;

    //this->GetWindowRect(&rect);
    //WMSG4("RECT is %d, %d, %d, %d\n", rect.left, rect.top, rect.bottom, rect.right);

    ASSERT(fpDiskImg != nil);
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
    ASSERT(pListView != nil);
    ListView_SetExtendedListViewStyleEx(pListView->m_hWnd,
        LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

    int width1, width2, width3;
    //CRect rect;

    pListView->GetClientRect(&rect);
    width1 = pListView->GetStringWidth("XXVolume NameXXmmmmm");
    width2 = pListView->GetStringWidth("XXFormatXXmmmmmmmmmm");
    width3 = pListView->GetStringWidth("XXSizeXXmmm");
    //width4 = pListView->GetStringWidth("XXBlock CountXX");

    pListView->InsertColumn(0, "Volume Name", LVCFMT_LEFT, width1);
    pListView->InsertColumn(1, "Format", LVCFMT_LEFT, width2);
    pListView->InsertColumn(2, "Size", LVCFMT_LEFT, width3);
    pListView->InsertColumn(3, "Block Count", LVCFMT_LEFT,
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

/*
 * We need to make sure we throw out the DiskFS we created before the modal
 * dialog exits.  This is necessary because we rely on an external DiskImg,
 * and create DiskFS objects that point to it.
 */
void
VolumeCopyDialog::OnOK(void)
{
    Cleanup();
    CDialog::OnOK();
}
void
VolumeCopyDialog::OnCancel(void)
{
    Cleanup();
    CDialog::OnCancel();
}
void
VolumeCopyDialog::Cleanup(void)
{
    WMSG0("  VolumeCopyDialog is done, cleaning up DiskFS\n");
    delete fpDiskFS;
    fpDiskFS = nil;
}

/*
 * Something changed in the list.  Update the buttons.
 */
void
VolumeCopyDialog::OnListChange(NMHDR*, LRESULT* pResult)
{
    //CRect rect;
    //this->GetWindowRect(&rect);
    //WMSG4("RECT is %d, %d, %d, %d\n", rect.left, rect.top, rect.bottom, rect.right);

    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUMECOPYSEL_LIST);
    ASSERT(pListView != nil);
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

/*
 * (Re-)scan the disk image and any sub-volumes.
 *
 * The top-level disk image should already have been analyzed and the
 * format overridden (if necessary).  We don't want to do it in here the
 * first time around because the "override" dialog screws up placement
 * of our dialog box.  I guess opening windows from inside OnInitDialog
 * isn't expected.  Annoying.  [Um, maybe we could call CenterWindow??
 * Actually, now I'm a little concerned about modal dialogs coming and
 * going while we're in OnInitDialog, because MainWindow is disabled and
 * we're not yet enabled. ++ATM]
 */
void
VolumeCopyDialog::ScanDiskInfo(bool scanTop)
{
    const Preferences* pPreferences = GET_PREFERENCES();
    MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
    DIError dierr;
    CString errMsg, failed;

    assert(fpDiskImg != nil);
    assert(fpDiskFS == nil);

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
                DiskImg::kFormatUnknown, nil, true, &errMsg);
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
        WMSG0("  Deferring destroy on wait dialog\n");
        deferDestroy = true;
    } else {
        WMSG0("  Not deferring destroy on wait dialog\n");
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
    if (fpDiskFS == nil) {
        WMSG0("HEY: OpenAppropriateDiskFS failed!\n");
        /* this is fatal, but there's no easy way to die */
        /* (could we do a DestroyWindow from here?) */
        /* at any rate, with "allowUnknown" set, this shouldn't happen */
    } else {
        fpDiskFS->SetScanForSubVolumes(DiskFS::kScanSubContainerOnly);

        dierr = fpDiskFS->Initialize(fpDiskImg, DiskFS::kInitFull);
        if (dierr != kDIErrNone) {
            CString appName, msg;
            appName.LoadString(IDS_MB_APP_NAME);
            msg.Format("Warning: error during disk scan: %s.",
                DiskImgLib::DIStrError(dierr));
            fpWaitDlg->MessageBox(msg, appName, MB_OK | MB_ICONEXCLAMATION);
            /* keep going */
        }
    }

    if (!deferDestroy && fpWaitDlg != nil) {
        fpWaitDlg->DestroyWindow();
        fpWaitDlg = nil;
    }

    return;
}

/*
 * When the focus changes, e.g. after dialog construction completes, see if
 * we have a modeless dialog lurking about.
 */
LONG
VolumeCopyDialog::OnDialogReady(UINT, LONG)
{
    if (fpWaitDlg != nil) {
        WMSG0("OnDialogReady found active window, destroying\n");
        fpWaitDlg->DestroyWindow();
        fpWaitDlg = nil;
    }
    return 0;
}

/*
 * (Re-)load the volume and sub-volumes into the list.
 *
 * We currently only look at the first level of sub-volumes.  We're not
 * really set up to display a hierarchy in the list view.  Very few people
 * will ever need to access a sub-sub-volume in this way, so it's not
 * worth sorting it out.
 */
void
VolumeCopyDialog::LoadList(void)
{
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUMECOPYSEL_LIST);
    ASSERT(pListView != nil);
    int itemIndex = 0;

    CString unknown = "(unknown)";

    pListView->DeleteAllItems();
    if (fpDiskFS == nil) {
        /* can only happen if imported volume is unrecognizeable */
        return;
    }

    AddToList(pListView, fpDiskImg, fpDiskFS, &itemIndex);

    DiskImgLib::DiskFS::SubVolume* pSubVolume;
    pSubVolume = fpDiskFS->GetNextSubVolume(nil);
    while (pSubVolume != nil) {
        if (pSubVolume->GetDiskFS() == nil) {
            WMSG0("WARNING: sub-volume DiskFS is nil?!\n");
            assert(false);
        } else {
            AddToList(pListView, pSubVolume->GetDiskImg(),
                pSubVolume->GetDiskFS(), &itemIndex);
        }
        pSubVolume = fpDiskFS->GetNextSubVolume(pSubVolume);
    } 
}

/*
 * Create an entry for a diskimg/diskfs pair.
 */
void
VolumeCopyDialog::AddToList(CListCtrl* pListView, DiskImg* pDiskImg,
    DiskFS* pDiskFS, int* pIndex)
{
    CString volName, format, sizeStr, blocksStr;
    long numBlocks;

    assert(pListView != nil);
    assert(pDiskImg != nil);
    assert(pDiskFS != nil);
    assert(pIndex != nil);

    numBlocks = pDiskImg->GetNumBlocks();

    volName = pDiskFS->GetVolumeName();
    format = DiskImg::ToString(pDiskImg->GetFSFormat());
    blocksStr.Format("%ld", pDiskImg->GetNumBlocks());
    if (numBlocks > 1024*1024*2)
        sizeStr.Format("%.2fGB", (double) numBlocks / (1024.0*1024.0*2.0));
    else if (numBlocks > 1024*2)
        sizeStr.Format("%.2fMB", (double) numBlocks / (1024.0*2.0));
    else
        sizeStr.Format("%.2fKB", (double) numBlocks / 2.0);

    /* add entry; first entry is the whole volume */
    pListView->InsertItem(*pIndex, volName,
        *pIndex == 0 ? kListIconVolume : kListIconSubVolume);
    pListView->SetItemText(*pIndex, 1, format);
    pListView->SetItemText(*pIndex, 2, sizeStr);
    pListView->SetItemText(*pIndex, 3, blocksStr);
    pListView->SetItemData(*pIndex, (DWORD) pDiskFS);
    (*pIndex)++;
}


/*
 * Recover the DiskImg and DiskFS pointers for the volume or sub-volume
 * currently selected in the list.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
VolumeCopyDialog::GetSelectedDisk(DiskImg** ppDiskImg, DiskFS** ppDiskFS)
{
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_VOLUMECOPYSEL_LIST);
    ASSERT(pListView != nil);

    ASSERT(ppDiskImg != nil);
    ASSERT(ppDiskFS != nil);

    if (pListView->GetSelectedCount() != 1)
        return false;

    POSITION posn;
    posn = pListView->GetFirstSelectedItemPosition();
    if (posn == nil) {
        ASSERT(false);
        return false;
    }
    int num = pListView->GetNextSelectedItem(posn);
    DWORD data = pListView->GetItemData(num);

    *ppDiskFS = (DiskFS*) data;
    assert(*ppDiskFS != nil);
    *ppDiskImg = (*ppDiskFS)->GetDiskImg();
    return true;
}

/*
 * User pressed the "Help" button.
 */
void
VolumeCopyDialog::OnHelp(void)
{
    WinHelp(HELP_TOPIC_VOLUME_COPIER, HELP_CONTEXT);
}


/*
 * User pressed the "copy to file" button.  Copy the selected partition out to
 * a file on disk.
 */
void
VolumeCopyDialog::OnCopyToFile(void)
{
    VolumeXferProgressDialog* pProgressDialog = nil;
    Preferences* pPreferences = GET_PREFERENCES_WR();
    MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
    DiskImg::FSFormat originalFormat = DiskImg::kFormatUnknown;
    DiskImg* pSrcImg = nil;
    DiskFS* pSrcFS = nil;
    DiskImg dstImg;
    DIError dierr;
    CString errMsg, saveName, msg, srcName;
    int result;

    result = GetSelectedDisk(&pSrcImg, &pSrcFS);
    if (!result)
        return;
    assert(pSrcImg != nil);
    assert(pSrcFS != nil);

    srcName = pSrcFS->GetVolumeName();

    /* force the format to be generic ProDOS-ordered blocks */
    originalFormat = pSrcImg->GetFSFormat();
    dierr = pSrcImg->OverrideFormat(pSrcImg->GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, pSrcImg->GetSectorOrder());
    if (dierr != kDIErrNone) {
        errMsg.Format("Internal error: couldn't switch to generic ProDOS: %s.",
                DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    WMSG2("Logical volume '%s' has %d 512-byte blocks\n",
        srcName, pSrcImg->GetNumBlocks());

    /*
     * Select file to write blocks to.
     */
    {
        CFileDialog saveDlg(FALSE, "po", NULL,
            OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN|OFN_HIDEREADONLY,
            "All Files (*.*)|*.*||", this);

        CString saveFolder;
        static char* title = "New disk image (.po)";

        saveDlg.m_ofn.lpstrTitle = title;
        saveDlg.m_ofn.lpstrInitialDir =
            pPreferences->GetPrefString(kPrOpenArchiveFolder);
    
        if (saveDlg.DoModal() != IDOK) {
            WMSG0(" User bailed out of image save dialog\n");
            goto bail;
        }

        saveFolder = saveDlg.m_ofn.lpstrFile;
        saveFolder = saveFolder.Left(saveDlg.m_ofn.nFileOffset);
        pPreferences->SetPrefString(kPrOpenArchiveFolder, saveFolder);

        saveName = saveDlg.GetPathName();
    }
    WMSG1("File will be saved to '%s'\n", saveName);

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

        dierr = dstImg.CreateImage(saveName, nil,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatSectors,
                    nil,
                    DiskImg::kSectorOrderProDOS,
                    DiskImg::kFormatGenericProDOSOrd,
                    dstNumBlocks,
                    true /* don't need to erase contents */);

        pWaitDlg->DestroyWindow();
        //pMain->PeekAndPump(); // redraw
    }
    if (dierr != kDIErrNone) {
        errMsg.Format("Couldn't create disk image: %s.",
                DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /* initialize cancel dialog, and disable main window */
    pProgressDialog = new VolumeXferProgressDialog;
    EnableWindow(FALSE);
    if (pProgressDialog->Create(this) == FALSE) {
        WMSG0("Progress dialog init failed?!\n");
        ASSERT(false);
        goto bail;
    }
    pProgressDialog->SetCurrentFiles(srcName, saveName);

    time_t startWhen, endWhen;
    startWhen = time(nil);

    /*
     * Do the actual block copy.
     */
    dierr = pMain->CopyDiskImage(&dstImg, pSrcImg, false, false, pProgressDialog);
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrCancelled) {
            errMsg.LoadString(IDS_OPERATION_CANCELLED);
            ShowFailureMsg(pProgressDialog, errMsg, IDS_CANCELLED);
            // remove the partially-written file
            dstImg.CloseImage();
            unlink(saveName);
        } else {
            errMsg.Format("Copy failed: %s.", DiskImgLib::DIStrError(dierr));
            ShowFailureMsg(pProgressDialog, errMsg, IDS_FAILED);
        }
        goto bail;
    }

    dierr = dstImg.CloseImage();
    if (dierr != kDIErrNone) {
        errMsg.Format("ERROR: dstImg close failed (err=%d)\n", dierr);
        ShowFailureMsg(pProgressDialog, errMsg, IDS_FAILED);
        goto bail;
    }

    /* put elapsed time in the debug log */
    endWhen = time(nil);
    float elapsed;
    if (endWhen == startWhen)
        elapsed = 1.0;
    else
        elapsed = (float) (endWhen - startWhen);
    msg.Format("Copied %ld blocks in %ld seconds (%.2fKB/sec)",
        pSrcImg->GetNumBlocks(), endWhen - startWhen,
        (pSrcImg->GetNumBlocks() / 2.0) / elapsed);
    WMSG1("%s\n", (const char*) msg);
#ifdef _DEBUG
    pProgressDialog->MessageBox(msg, "DEBUG: elapsed time", MB_OK);
#endif

    pMain->SuccessBeep();


bail:
    // restore the dialog window to prominence
    EnableWindow(TRUE);
    //SetActiveWindow();
    if (pProgressDialog != nil)
        pProgressDialog->DestroyWindow();

    /* un-override the source disk */
    if (originalFormat != DiskImg::kFormatUnknown) {
        dierr = pSrcImg->OverrideFormat(pSrcImg->GetPhysicalFormat(),
                    originalFormat, pSrcImg->GetSectorOrder());
        if (dierr != kDIErrNone) {
            WMSG1("ERROR: couldn't un-override source image (dierr=%d)\n", dierr);
            // not much else to do; should be okay
        }
    }
    return;
}


/*
 * User pressed the "copy from file" button.  Copy a file over the selected
 * partition.  We may need to reload the main window after this completes.
 */
void
VolumeCopyDialog::OnCopyFromFile(void)
{
    VolumeXferProgressDialog* pProgressDialog = nil;
    Preferences* pPreferences = GET_PREFERENCES_WR();
    MainWindow* pMain = (MainWindow*)::AfxGetMainWnd();
    //DiskImg::FSFormat originalFormat = DiskImg::kFormatUnknown;
    CString openFilters;
    CString loadName, targetName, errMsg, warning;
    DiskImg* pDstImg = nil;
    DiskFS* pDstFS = nil;
    DiskImg srcImg;
    DIError dierr;
    int result;
    bool needReload = false;
    bool isPartial = false;

    warning.LoadString(IDS_WARNING);

    /*
     * Get the DiskImg and DiskFS pointers for the selected partition out of
     * the control.  The storage for these is part of fpDiskFS, which holds
     * the tree of subvolumes.
     */
    result = GetSelectedDisk(&pDstImg, &pDstFS);
    if (!result)
        return;

//  if (pDstFS == nil)
//      targetName = "the target volume";
//  else
        targetName = pDstFS->GetVolumeName();


    /*
     * Select the image to copy from.
     */
    openFilters = MainWindow::kOpenDiskImage;
    openFilters += MainWindow::kOpenAll;
    openFilters += MainWindow::kOpenEnd;
    CFileDialog dlg(TRUE, "dsk", NULL, OFN_FILEMUSTEXIST, openFilters, this);

    /* source file gets opened read-only */
    dlg.m_ofn.Flags |= OFN_HIDEREADONLY;
    dlg.m_ofn.lpstrTitle = "Select image to copy from";
    dlg.m_ofn.lpstrInitialDir = pPreferences->GetPrefString(kPrOpenArchiveFolder);

    if (dlg.DoModal() != IDOK)
        goto bail;
    loadName = dlg.GetPathName();

    {
        CWaitCursor waitc;
        CString saveFolder;

        /* open the image file and analyze it */
        dierr = srcImg.OpenImage(loadName, PathProposal::kLocalFssep, true);
        if (dierr != kDIErrNone) {
            errMsg.Format("Unable to open disk image: %s.",
                DiskImgLib::DIStrError(dierr));
            ShowFailureMsg(this, errMsg, IDS_FAILED);
            goto bail;
        }

        if (srcImg.AnalyzeImage() != kDIErrNone) {
            errMsg.Format("The file '%s' doesn't seem to hold a valid disk image.",
                loadName);
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
        errMsg = "The disk image must be block-oriented.  Nibble images"
                 " cannot be copied.";
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /* force source volume to generic ProDOS blocks */
    dierr = srcImg.OverrideFormat(srcImg.GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, srcImg.GetSectorOrder());
    if (dierr != kDIErrNone) {
        errMsg.Format("Internal error: couldn't switch source to generic ProDOS: %s.",
                DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    WMSG2("Source image '%s' has %d 512-byte blocks\n",
        loadName, srcImg.GetNumBlocks());

    WMSG1("Target volume has %d 512-byte blocks\n", pDstImg->GetNumBlocks());

    if (srcImg.GetNumBlocks() > pDstImg->GetNumBlocks()) {
        errMsg.Format("Error: the disk image file has %ld blocks, but the"
                      " target volume holds %ld blocks.  The target must"
                      " have more space than the input file.",
            srcImg.GetNumBlocks(), pDstImg->GetNumBlocks());
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    if (pDstImg->GetNumBlocks() >= DiskImgLib::kVolumeMaxBlocks) {
        errMsg.Format("Error: for safety reasons, copying disk images to"
                      " larger volumes is not supported when the target"
                      " is 8GB or larger.");
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    if (srcImg.GetNumBlocks() != pDstImg->GetNumBlocks()) {
        errMsg.LoadString(IDS_WARNING);
        errMsg.Format("The disk image file has %ld blocks, but the target"
                      " volume holds %ld blocks.  The leftover space may be"
                      " wasted, and non-ProDOS volumes may not be identified"
                      " correctly.  Do you wish to continue?",
            srcImg.GetNumBlocks(), pDstImg->GetNumBlocks());
        result = MessageBox(errMsg, warning, MB_OKCANCEL | MB_ICONQUESTION);
        if (result != IDOK) {
            WMSG0("User chickened out of oversized disk copy\n");
            goto bail;
        }
        isPartial = true;
    }

    errMsg.LoadString(IDS_WARNING);
    errMsg.Format("You are about to overwrite volume %s with the"
                   " contents of '%s'.  This will destroy all data on"
                   " %s.  Are you sure you wish to continue?",
        targetName, loadName, targetName);
    result = MessageBox(errMsg, warning, MB_OKCANCEL | MB_ICONEXCLAMATION);
    if (result != IDOK) {
        WMSG0("User chickened out of disk copy\n");
        goto bail;
    }

    /* force the target disk image to be generic ProDOS-ordered blocks */
    dierr = pDstImg->OverrideFormat(pDstImg->GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, pDstImg->GetSectorOrder());
    if (dierr != kDIErrNone) {
        errMsg.Format("Internal error: couldn't switch target to generic ProDOS: %s.",
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
        WMSG0("Progress dialog init failed?!\n");
        ASSERT(false);
        return;
    }
//  if (pDstFS == nil)
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
    startWhen = time(nil);

    /*
     * Do the actual block copy.
     */
    dierr = pMain->CopyDiskImage(pDstImg, &srcImg, false, isPartial,
                pProgressDialog);
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrCancelled) {
            errMsg.LoadString(IDS_OPERATION_CANCELLED);
            ShowFailureMsg(pProgressDialog, errMsg, IDS_CANCELLED);
        } else {
            errMsg.Format("Copy failed: %s.", DiskImgLib::DIStrError(dierr));
            ShowFailureMsg(pProgressDialog, errMsg, IDS_FAILED);
        }
        goto bail;
    }

    dierr = srcImg.CloseImage();
    if (dierr != kDIErrNone) {
        errMsg.Format("ERROR: srcImg close failed (err=%d)\n", dierr);
        ShowFailureMsg(pProgressDialog, errMsg, IDS_FAILED);
        goto bail;
    }

    endWhen = time(nil);
    float elapsed;
    if (endWhen == startWhen)
        elapsed = 1.0;
    else
        elapsed = (float) (endWhen - startWhen);
    errMsg.Format("Copied %ld blocks in %ld seconds (%.2fKB/sec)",
        srcImg.GetNumBlocks(), endWhen - startWhen,
        (srcImg.GetNumBlocks() / 2.0) / elapsed);
    WMSG1("%s\n", (const char*) errMsg);
#ifdef _DEBUG
    pProgressDialog->MessageBox(errMsg, "DEBUG: elapsed time", MB_OK);
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
    fpDiskFS = nil;
    assert(fpDiskImg->GetReadOnly());
    fpDiskImg->SetReadOnly(false);

bail:
    // restore the dialog window to prominence
    EnableWindow(TRUE);
    //SetActiveWindow();
    if (pProgressDialog != nil)
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
        WMSG0("RELOAD dialog\n");
        ScanDiskInfo(true);     // reopens fpDiskFS
        LoadList();

        /* will we need to reopen the currently-open file list archive? */
        if (pMain->IsOpenPathName(fPathName))
            pMain->SetReopenFlag();
    }
    return;
}
