/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File actions.  These are actually part of MainWindow, but for readability
 * these are split into their own file.
 */
#include "stdafx.h"
#include "Main.h"
#include "ViewFilesDialog.h"
#include "ChooseDirDialog.h"
#include "AddFilesDialog.h"
#include "CreateSubdirDialog.h"
#include "ExtractOptionsDialog.h"
#include "UseSelectionDialog.h"
#include "RecompressOptionsDialog.h"
#include "ConvDiskOptionsDialog.h"
#include "ConvFileOptionsDialog.h"
#include "EditCommentDialog.h"
#include "EditPropsDialog.h"
#include "RenameVolumeDialog.h"
#include "ConfirmOverwriteDialog.h"
#include "ImageFormatDialog.h"
#include "FileNameConv.h"
#include "GenericArchive.h"
#include "NufxArchive.h"
#include "DiskArchive.h"
#include "ChooseAddTargetDialog.h"
#include "CassetteDialog.h"
#include "BasicImport.h"
#include "../diskimg/TwoImg.h"
#include <errno.h>


/*
 * ==========================================================================
 *      View
 * ==========================================================================
 */

void MainWindow::OnActionsView(void)
{
    HandleView();
}
void MainWindow::OnUpdateActionsView(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL &&
        fpContentList->GetSelectedCount() > 0);
}

void MainWindow::HandleView(void)
{
    ASSERT(fpContentList != NULL);

    SelectionSet selSet;
    int threadMask = GenericEntry::kAnyThread | GenericEntry::kAllowDamaged |
        GenericEntry::kAllowDirectory | GenericEntry::kAllowVolumeDir;
    selSet.CreateFromSelection(fpContentList, threadMask);
    selSet.Dump();

    if (selSet.GetNumEntries() == 0) {
        MessageBox(L"Nothing viewable found.",
            L"No match", MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    ViewFilesDialog vfd(this);
    vfd.SetSelectionSet(&selSet);
    vfd.SetTextTypeFace(fPreferences.GetPrefString(kPrViewTextTypeFace));
    vfd.SetTextPointSize(fPreferences.GetPrefLong(kPrViewTextPointSize));
    vfd.SetNoWrapText(fPreferences.GetPrefBool(kPrNoWrapText));
    vfd.DoModal();

    // remember which font they used (sticky pref, not in registry)
    fPreferences.SetPrefString(kPrViewTextTypeFace, vfd.GetTextTypeFace());
    fPreferences.SetPrefLong(kPrViewTextPointSize, vfd.GetTextPointSize());
    LOGI("Preferences: saving view font %d-point '%ls'",
        fPreferences.GetPrefLong(kPrViewTextPointSize),
        fPreferences.GetPrefString(kPrViewTextTypeFace));
}


/*
 * ==========================================================================
 *      Open as disk image
 * ==========================================================================
 */

void MainWindow::OnActionsOpenAsDisk(void)
{
    ASSERT(fpContentList != NULL);
    ASSERT(fpContentList->GetSelectedCount() == 1);

    GenericEntry* pEntry = GetSelectedItem(fpContentList);
    if (pEntry->GetHasDiskImage())
        TmpExtractAndOpen(pEntry, GenericEntry::kDiskImageThread, kModeDiskImage);
    else
        TmpExtractAndOpen(pEntry, GenericEntry::kDataThread, kModeDiskImage);
}
void MainWindow::OnUpdateActionsOpenAsDisk(CCmdUI* pCmdUI)
{
    const int kMinLen = 512 * 7;
    bool allow = false;

    if (fpContentList != NULL && fpContentList->GetSelectedCount() == 1) {
        GenericEntry* pEntry = GetSelectedItem(fpContentList);
        if (pEntry != NULL) {
            if ((pEntry->GetHasDataFork() || pEntry->GetHasDiskImage()) &&
                pEntry->GetUncompressedLen() > kMinLen)
            {
                allow = true;
            }
        }
    }
    pCmdUI->Enable(allow);
}


/*
 * ==========================================================================
 *      Add Files
 * ==========================================================================
 */

void MainWindow::OnActionsAddFiles(void)
{
    LOGI("Add files!");
    AddFilesDialog addFiles(this);
    DiskImgLib::A2File* pTargetSubdir = NULL;

    /*
     * Special handling for adding files to disk images.
     */
    if (fpOpenArchive->GetArchiveKind() == GenericArchive::kArchiveDiskImage)
    {
        if (!ChooseAddTarget(&pTargetSubdir, &addFiles.fpTargetDiskFS))
            return;
    }

    addFiles.fStoragePrefix = "";
    addFiles.fIncludeSubfolders =
        fPreferences.GetPrefBool(kPrAddIncludeSubFolders);
    addFiles.fStripFolderNames =
        fPreferences.GetPrefBool(kPrAddStripFolderNames);
    addFiles.fOverwriteExisting =
        fPreferences.GetPrefBool(kPrAddOverwriteExisting);
    addFiles.fTypePreservation =
        fPreferences.GetPrefLong(kPrAddTypePreservation);
    addFiles.fConvEOL =
        fPreferences.GetPrefLong(kPrAddConvEOL);

    /* if they can't convert EOL when adding files, disable the option */
    if (!fpOpenArchive->GetCapability(GenericArchive::kCapCanConvEOLOnAdd)) {
        addFiles.fConvEOL = AddFilesDialog::kConvEOLNone;
        addFiles.fConvEOLEnable = false;
    }

    /*
     * Disable editing of the storage prefix field.  Force pathname
     * stripping to be on for non-hierarchical filesystems (i.e. everything
     * but ProDOS and HFS).
     */
    if (addFiles.fpTargetDiskFS != NULL) {
        DiskImg::FSFormat format;
        format = addFiles.fpTargetDiskFS->GetDiskImg()->GetFSFormat();

        if (pTargetSubdir != NULL) {
            ASSERT(!pTargetSubdir->IsVolumeDirectory());
            addFiles.fStoragePrefix = pTargetSubdir->GetPathName();
        }

        addFiles.fStripFolderNamesEnable = false;
        addFiles.fStoragePrefixEnable = false;
        switch (format) {
        case DiskImg::kFormatProDOS:
        case DiskImg::kFormatMacHFS:
            addFiles.fStripFolderNamesEnable = true;
            break;
        default:
            break;
        }
    }
    if (!addFiles.fStripFolderNamesEnable) {
        /* if we disabled it, we did so because it's mandatory */
        addFiles.fStripFolderNames = true;
    }

    addFiles.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrAddFileFolder);

    LRESULT addResult = addFiles.DoModal();
    if (addResult == IDOK) {
        fPreferences.SetPrefBool(kPrAddIncludeSubFolders,
            addFiles.fIncludeSubfolders != 0);
        if (addFiles.fStripFolderNamesEnable) {
            // only update the pref if they had the ability to change it
            fPreferences.SetPrefBool(kPrAddStripFolderNames,
                addFiles.fStripFolderNames != 0);
        }
        fPreferences.SetPrefBool(kPrAddOverwriteExisting,
            addFiles.fOverwriteExisting != 0);
        fPreferences.SetPrefLong(kPrAddTypePreservation,
            addFiles.fTypePreservation);
        if (addFiles.fConvEOLEnable)
            fPreferences.SetPrefLong(kPrAddConvEOL,
                addFiles.fConvEOL);

        CString saveFolder = addFiles.GetDirectory();
        fPreferences.SetPrefString(kPrAddFileFolder, saveFolder);

        /*
         * Set up a progress dialog and kick things off.
         */
        bool result;
        fpActionProgress = new ActionProgressDialog;
        fpActionProgress->Create(ActionProgressDialog::kActionAdd, this);

        //fpContentList->Invalidate();      // don't allow redraws until done
        result = fpOpenArchive->BulkAdd(fpActionProgress, &addFiles);
        fpContentList->Reload();

        fpActionProgress->Cleanup(this);
        fpActionProgress = NULL;
        if (result)
            SuccessBeep();
    } else {
        LOGI("SFD bailed with Cancel");
    }
}
void MainWindow::OnUpdateActionsAddFiles(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && !fpOpenArchive->IsReadOnly());
}

bool MainWindow::ChooseAddTarget(DiskImgLib::A2File** ppTargetSubdir,
    DiskImgLib::DiskFS** ppTargetDiskFS)
{
    ASSERT(ppTargetSubdir != NULL);
    ASSERT(ppTargetDiskFS != NULL);

    *ppTargetSubdir = NULL;
    *ppTargetDiskFS = NULL;

    GenericEntry* pEntry = GetSelectedItem(fpContentList);
    if (pEntry != NULL &&
        (pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory ||
         pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir))
    {
        /*
         * They've selected a single subdirectory.  Add the files there.
         */
        DiskEntry* pDiskEntry = (DiskEntry*) pEntry;
        *ppTargetSubdir = pDiskEntry->GetA2File();
        *ppTargetDiskFS = (*ppTargetSubdir)->GetDiskFS();
    } else {
        /*
         * Nothing selected, non-subdir selected, or multiple files
         * selected.  Whatever the case, pop up the choose target dialog.
         *
         * This works on DOS 3.3 and Pascal disks, because the absence
         * of subdirectories means there's only one possible place to
         * put the files.  We could short-circuit this code for anything
         * but ProDOS and HFS, but we have to be careful about embedded
         * sub-volumes.
         */
        DiskArchive* pDiskArchive = (DiskArchive*) fpOpenArchive;

        LOGD("Trying ChooseAddTarget");

        ChooseAddTargetDialog targetDialog(this);
        targetDialog.fpDiskFS = pDiskArchive->GetDiskFS();
        if (targetDialog.DoModal() != IDOK)
            return false;

        *ppTargetSubdir = targetDialog.fpChosenSubdir;
        *ppTargetDiskFS = targetDialog.fpChosenDiskFS;

        /* make sure the subdir is part of the diskfs */
        ASSERT(*ppTargetSubdir == NULL ||
            (*ppTargetSubdir)->GetDiskFS() == *ppTargetDiskFS);
    }

    if (*ppTargetSubdir != NULL && (*ppTargetSubdir)->IsVolumeDirectory())
        *ppTargetSubdir = NULL;

    return true;
}


/*
 * ==========================================================================
 *      Add Disks
 * ==========================================================================
 */

void MainWindow::OnActionsAddDisks(void)
{
    DIError dierr;
    DiskImg img;
    CString failed, errMsg;
    CString openFilters, saveFolder;
    CStringA pathNameA;
    AddFilesDialog addOpts;

    LOGI("Add disks!");

    CheckedLoadString(&failed, IDS_FAILED);

    openFilters = kOpenDiskImage;
    openFilters += kOpenAll;
    openFilters += kOpenEnd;
    CFileDialog dlg(TRUE, L"dsk", NULL, OFN_FILEMUSTEXIST, openFilters, this);
    dlg.m_ofn.lpstrTitle = L"Add Disk Image";

    /* file is always opened read-only */
    dlg.m_ofn.Flags |= OFN_HIDEREADONLY;
    dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrAddFileFolder);

    if (dlg.DoModal() != IDOK)
        goto bail;

    saveFolder = dlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrAddFileFolder, saveFolder);

    /* open the image file and analyze it */
    pathNameA = dlg.GetPathName();
    dierr = img.OpenImage(pathNameA, PathProposal::kLocalFssep, true);
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Unable to open disk image: %hs.",
            DiskImgLib::DIStrError(dierr));
        MessageBox(errMsg, failed, MB_OK|MB_ICONSTOP);
        goto bail;
    }

    if (img.AnalyzeImage() != kDIErrNone) {
        errMsg.Format(L"The file '%ls' doesn't seem to hold a valid disk image.",
            (LPCWSTR) dlg.GetPathName());
        MessageBox(errMsg, failed, MB_OK|MB_ICONSTOP);
        goto bail;
    }


    /* if requested (or necessary), verify the format */
    if (/*img.GetFSFormat() == DiskImg::kFormatUnknown ||*/
        img.GetSectorOrder() == DiskImg::kSectorOrderUnknown ||
        fPreferences.GetPrefBool(kPrQueryImageFormat))
    {
        ImageFormatDialog imf;

        imf.InitializeValues(&img);
        imf.fFileSource = dlg.GetPathName();
        imf.SetQueryDisplayFormat(false);

        LOGI(" On entry, sectord=%d format=%d",
            imf.fSectorOrder, imf.fFSFormat);
        if (imf.fFSFormat == DiskImg::kFormatUnknown)
            imf.fFSFormat = DiskImg::kFormatGenericProDOSOrd;

        if (imf.DoModal() != IDOK) {
            LOGI("User bailed on IMF dialog");
            goto bail;
        }

        LOGI(" On exit, sectord=%d format=%d",
            imf.fSectorOrder, imf.fFSFormat);

        if (imf.fSectorOrder != img.GetSectorOrder() ||
            imf.fFSFormat != img.GetFSFormat())
        {
            LOGI("Initial values overridden, forcing img format");
            dierr = img.OverrideFormat(img.GetPhysicalFormat(), imf.fFSFormat,
                        imf.fSectorOrder);
            if (dierr != kDIErrNone) {
                errMsg.Format(L"Unable to access disk image using selected"
                           L" parameters.  Error: %hs.",
                    DiskImgLib::DIStrError(dierr));
                MessageBox(errMsg, failed, MB_OK | MB_ICONSTOP);
                goto bail;
            }
        }
    }

    /*
     * We want to read from the image the way that a ProDOS application
     * would, which means forcing the FSFormat to generic ProDOS ordering.
     * This way, ProDOS disks load serially, and DOS 3.3 disks load with
     * their sectors swapped around.
     */
    dierr = img.OverrideFormat(img.GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, img.GetSectorOrder());
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Internal error: couldn't switch to generic ProDOS: %hs.",
                DiskImgLib::DIStrError(dierr));
        MessageBox(errMsg, failed, MB_OK | MB_ICONSTOP);
        goto bail;
    }

    /*
     * Set up an AddFilesDialog, but don't actually use it as a dialog.
     * Instead, we just configure the various options appropriately.
     */
    ASSERT(dlg.m_ofn.nFileOffset > 0);
    {
        CString directory(dlg.m_ofn.lpstrFile, dlg.m_ofn.nFileOffset - 1);
        CString file(dlg.m_ofn.lpstrFile + dlg.m_ofn.nFileOffset);
        LOGD("Stuffing '%ls' '%ls'", (LPCWSTR) directory, (LPCWSTR) file);
        addOpts.StuffSingleFilename(directory, file);
    }
    addOpts.fStoragePrefix = "";
    addOpts.fIncludeSubfolders = false;
    addOpts.fStripFolderNames = false;
    addOpts.fOverwriteExisting = false;
    addOpts.fTypePreservation = AddFilesDialog::kPreserveTypes;
    addOpts.fConvEOL = AddFilesDialog::kConvEOLNone;
    addOpts.fConvEOLEnable = false;     // no EOL conversion on disk images!
    addOpts.fpDiskImg = &img;

    bool result;

    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionAddDisk, this);

    //fpContentList->Invalidate();      // don't allow updates until done
    result = fpOpenArchive->AddDisk(fpActionProgress, &addOpts);
    fpContentList->Reload();

    fpActionProgress->Cleanup(this);
    fpActionProgress = NULL;

    if (result)
        SuccessBeep();

bail:
    return;
}
void MainWindow::OnUpdateActionsAddDisks(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && !fpOpenArchive->IsReadOnly() &&
        fpOpenArchive->GetCapability(GenericArchive::kCapCanAddDisk));
}


/*
 * ==========================================================================
 *      Create Subdirectory
 * ==========================================================================
 */

void MainWindow::OnActionsCreateSubdir(void)
{
    CreateSubdirDialog csDialog;

    ASSERT(fpContentList != NULL);
    ASSERT(fpOpenArchive != NULL);
    ASSERT(!fpOpenArchive->IsReadOnly());

    GenericEntry* pEntry = GetSelectedItem(fpContentList);
    if (pEntry == NULL) {
        // can happen for no selection or multi-selection; should not be here
        ASSERT(false);
        return;
    }
    if (pEntry->GetRecordKind() != GenericEntry::kRecordKindDirectory &&
        pEntry->GetRecordKind() != GenericEntry::kRecordKindVolumeDir)
    {
        CString errMsg;
        errMsg = "Please select a subdirectory.";
        ShowFailureMsg(this, errMsg, IDS_MB_APP_NAME);
        return;
    }

    LOGI("Creating subdir in '%ls'", (LPCWSTR) pEntry->GetPathNameUNI());

    csDialog.fBasePath = pEntry->GetPathNameUNI();
    csDialog.fpArchive = fpOpenArchive;
    csDialog.fpParentEntry = pEntry;
    csDialog.fNewName = "New.Subdir";
    if (csDialog.DoModal() != IDOK)
        return;

    LOGI("Creating '%ls'", (LPCWSTR) csDialog.fNewName);

    fpOpenArchive->CreateSubdir(this, pEntry, csDialog.fNewName);
    fpContentList->Reload();
}
void MainWindow::OnUpdateActionsCreateSubdir(CCmdUI* pCmdUI)
{
    bool enable = fpContentList != NULL && !fpOpenArchive->IsReadOnly() &&
        fpContentList->GetSelectedCount() == 1 &&
        fpOpenArchive->GetCapability(GenericArchive::kCapCanCreateSubdir);

    if (enable) {
        /* second-level check: make sure it's a subdir */
        GenericEntry* pEntry = GetSelectedItem(fpContentList);
        if (pEntry == NULL) {
            ASSERT(false);
            return;
        }
        if (pEntry->GetRecordKind() != GenericEntry::kRecordKindDirectory &&
            pEntry->GetRecordKind() != GenericEntry::kRecordKindVolumeDir)
        {
            enable = false;
        }
    }

    pCmdUI->Enable(enable);
}


/*
 * ==========================================================================
 *      Extract
 * ==========================================================================
 */

void MainWindow::OnActionsExtract(void)
{
    ASSERT(fpContentList != NULL);

    /*
     * Ask the user about various options.
     */
    ExtractOptionsDialog extOpts(fpContentList->GetSelectedCount(), this);
    extOpts.fExtractPath = fPreferences.GetPrefString(kPrExtractFileFolder);
    extOpts.fConvEOL = fPreferences.GetPrefLong(kPrExtractConvEOL);
    extOpts.fConvHighASCII = fPreferences.GetPrefBool(kPrExtractConvHighASCII);
    extOpts.fIncludeDataForks = fPreferences.GetPrefBool(kPrExtractIncludeData);
    extOpts.fIncludeRsrcForks = fPreferences.GetPrefBool(kPrExtractIncludeRsrc);
    extOpts.fIncludeDiskImages = fPreferences.GetPrefBool(kPrExtractIncludeDisk);
    extOpts.fEnableReformat = fPreferences.GetPrefBool(kPrExtractEnableReformat);
    extOpts.fDiskTo2MG = fPreferences.GetPrefBool(kPrExtractDiskTo2MG);
    extOpts.fAddTypePreservation = fPreferences.GetPrefBool(kPrExtractAddTypePreservation);
    extOpts.fAddExtension = fPreferences.GetPrefBool(kPrExtractAddExtension);
    extOpts.fStripFolderNames = fPreferences.GetPrefBool(kPrExtractStripFolderNames);
    extOpts.fOverwriteExisting = fPreferences.GetPrefBool(kPrExtractOverwriteExisting);

    if (fpContentList->GetSelectedCount() > 0)
        extOpts.fFilesToExtract = ExtractOptionsDialog::kExtractSelection;
    else
        extOpts.fFilesToExtract = ExtractOptionsDialog::kExtractAll;

    if (extOpts.DoModal() != IDOK)
        return;

    if (extOpts.fExtractPath.Right(1) != "\\")
        extOpts.fExtractPath += "\\";

    /* push preferences back out */
    fPreferences.SetPrefString(kPrExtractFileFolder, extOpts.fExtractPath);
    fPreferences.SetPrefLong(kPrExtractConvEOL, extOpts.fConvEOL);
    fPreferences.SetPrefBool(kPrExtractConvHighASCII, extOpts.fConvHighASCII != 0);
    fPreferences.SetPrefBool(kPrExtractIncludeData, extOpts.fIncludeDataForks != 0);
    fPreferences.SetPrefBool(kPrExtractIncludeRsrc, extOpts.fIncludeRsrcForks != 0);
    fPreferences.SetPrefBool(kPrExtractIncludeDisk, extOpts.fIncludeDiskImages != 0);
    fPreferences.SetPrefBool(kPrExtractEnableReformat, extOpts.fEnableReformat != 0);
    fPreferences.SetPrefBool(kPrExtractDiskTo2MG, extOpts.fDiskTo2MG != 0);
    fPreferences.SetPrefBool(kPrExtractAddTypePreservation, extOpts.fAddTypePreservation != 0);
    fPreferences.SetPrefBool(kPrExtractAddExtension, extOpts.fAddExtension != 0);
    fPreferences.SetPrefBool(kPrExtractStripFolderNames, extOpts.fStripFolderNames != 0);
    fPreferences.SetPrefBool(kPrExtractOverwriteExisting, extOpts.fOverwriteExisting != 0);

    LOGI("Requested extract path is '%ls'", (LPCWSTR) extOpts.fExtractPath);

    /*
     * Create a "selection set" of things to display.
     */
    SelectionSet selSet;
    int threadMask = 0;
    if (extOpts.fIncludeDataForks)
        threadMask |= GenericEntry::kDataThread;
    if (extOpts.fIncludeRsrcForks)
        threadMask |= GenericEntry::kRsrcThread;
    if (extOpts.fIncludeDiskImages)
        threadMask |= GenericEntry::kDiskImageThread;

    if (extOpts.fFilesToExtract == ExtractOptionsDialog::kExtractSelection) {
        selSet.CreateFromSelection(fpContentList, threadMask);
    } else {
        selSet.CreateFromAll(fpContentList, threadMask);
    }
    //selSet.Dump();

    if (selSet.GetNumEntries() == 0) {
        MessageBox(L"No files matched the selection criteria.",
            L"No match", MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    /*
     * Set up the progress dialog then do the extraction.
     */
    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionExtract, this);
    DoBulkExtract(&selSet, &extOpts);
    fpActionProgress->Cleanup(this);
    fpActionProgress = NULL;
}
void MainWindow::OnUpdateActionsExtract(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL);
}

void MainWindow::DoBulkExtract(SelectionSet* pSelSet,
    const ExtractOptionsDialog* pExtOpts)
{
    /*
     * IMPORTANT: since the pActionProgress dialog has the foreground, it's
     * vital that any MessageBox calls go through that.  Otherwise the
     * progress dialog message handler won't get disabled by MessageBox and
     * we can end up permanently hiding the dialog.  (Could also use
     * ::MessageBox or ::AfxMessageBox instead.)
     */
    ReformatHolder* pHolder = NULL;
    bool overwriteExisting, ovwrForAll;

    ASSERT(pSelSet != NULL);
    ASSERT(fpActionProgress != NULL);

    pSelSet->IterReset();

    /* set up our "overwrite existing files" logic */
    overwriteExisting = ovwrForAll = (pExtOpts->fOverwriteExisting != FALSE);

    while (true) {
        SelectionEntry* pSelEntry;
        bool result;

        /* make sure we service events */
        PeekAndPump();

        pSelEntry = pSelSet->IterNext();
        if (pSelEntry == NULL) {
            SuccessBeep();
            break;      // out of while (all done!)
        }

        GenericEntry* pEntry = pSelEntry->GetEntry();
        if (pEntry->GetDamaged()) {
            LOGI("Skipping '%ls' due to damage", (LPCWSTR) pEntry->GetPathNameUNI());
            continue;
        }

        /*
         * Extract all parts of the file -- including those we don't actually
         * intend to extract to disk -- and hold them in the ReformatHolder.
         * Some formats, e.g. GWP, need the resource fork to do formatting,
         * so it's important to have the resource fork available even if
         * we're just going to throw it away.
         *
         * The selection set should have screened out anything totally
         * inappropriate, e.g. files with nothing but a resource fork don't
         * make it into the set if we're not extracting resource forks.
         *
         * We only want to reformat files, not disk images, directories,
         * volume dirs, etc.  We have a reformatter for ProDOS directories,
         * but (a) we don't explicitly extract subdirs, and (b) we'd really
         * like directories to be directories so we can extract files into
         * them.
         */
        if (pExtOpts->ShouldTryReformat() &&
            (pEntry->GetRecordKind() == GenericEntry::kRecordKindFile ||
             pEntry->GetRecordKind() == GenericEntry::kRecordKindForkedFile))
        {
            fpActionProgress->SetArcName(pEntry->GetDisplayName());
            fpActionProgress->SetFileName(L"-");
            SET_PROGRESS_BEGIN();

            if (GetFileParts(pEntry, &pHolder) == 0) {
                /*
                 * Use the prefs, but disable generic text conversion, so
                 * that we default to "raw".  That way we will use the text
                 * conversion that the user has specified in the "extract"
                 * dialog.
                 *
                 * We might want to just disable any "always"-level
                 * reformatter, but that would require tweaking the reformat
                 * code to return "raw" when nothing applies.
                 */
                ConfigureReformatFromPreferences(pHolder);
                pHolder->SetReformatAllowed(ReformatHolder::kReformatTextEOL_HA,
                    false);

                pHolder->SetSourceAttributes(
                    pEntry->GetFileType(),
                    pEntry->GetAuxType(),
                    ReformatterSourceFormat(pEntry->GetSourceFS()),
                    pEntry->GetFileNameExtensionMOR());
                pHolder->TestApplicability();
            }
        }

        if (pExtOpts->fIncludeDataForks && pEntry->GetHasDataFork()) {
            result = ExtractEntry(pEntry, GenericEntry::kDataThread,
                        pHolder, pExtOpts, &overwriteExisting, &ovwrForAll);
            if (!result)
                break;
        }
        if (pExtOpts->fIncludeRsrcForks && pEntry->GetHasRsrcFork()) {
            result = ExtractEntry(pEntry, GenericEntry::kRsrcThread,
                        pHolder, pExtOpts, &overwriteExisting, &ovwrForAll);
            if (!result)
                break;
        }
        if (pExtOpts->fIncludeDiskImages && pEntry->GetHasDiskImage()) {
            result = ExtractEntry(pEntry, GenericEntry::kDiskImageThread,
                        pHolder, pExtOpts, &overwriteExisting, &ovwrForAll);
            if (!result)
                break;
        }
        delete pHolder;
        pHolder = NULL;
    }

    // if they cancelled, delete the "stray"
    delete pHolder;
}

bool MainWindow::ExtractEntry(GenericEntry* pEntry, int thread,
    ReformatHolder* pHolder, const ExtractOptionsDialog* pExtOpts,
    bool* pOverwriteExisting, bool* pOvwrForAll)
{
    /*
     * This first bit of setup is the same for every file.  However, it's
     * pretty quick, and it's easier to pass "pExtOpts" in than all of
     * this stuff, so we just do it every time.
     */
    GenericEntry::ConvertEOL convEOL;
    GenericEntry::ConvertHighASCII convHA;
    bool convTextByType = false;


    /* translate the EOL conversion mode into GenericEntry terms */
    switch (pExtOpts->fConvEOL) {
    case ExtractOptionsDialog::kConvEOLNone:
        convEOL = GenericEntry::kConvertEOLOff;
        break;
    case ExtractOptionsDialog::kConvEOLType:
        convEOL = GenericEntry::kConvertEOLOff;
        convTextByType = true;
        break;
    case ExtractOptionsDialog::kConvEOLAuto:
        convEOL = GenericEntry::kConvertEOLAuto;
        break;
    case ExtractOptionsDialog::kConvEOLAll:
        convEOL = GenericEntry::kConvertEOLOn;
        break;
    default:
        ASSERT(false);
        convEOL = GenericEntry::kConvertEOLOff;
        break;
    }
    if (pExtOpts->fConvHighASCII)
        convHA = GenericEntry::kConvertHAAuto;
    else
        convHA = GenericEntry::kConvertHAOff;

    //LOGI("  DBE initial text conversion: eol=%d ha=%d",
    //  convEOL, convHA);


    ReformatHolder holder;
    CString outputPath;
    CString failed, errMsg;
    CheckedLoadString(&failed, IDS_FAILED);
    bool writeFailed = false;
    bool extractAs2MG = false;
    char* reformatText = NULL;
    MyDIBitmap* reformatDib = NULL;

    ASSERT(pEntry != NULL);

    /*
     * If we're interested in extracting disk images as 2MG files,
     * see if we want to handle this one that way.
     *
     * If they said "don't extract disk images", the images should
     * have been culled from the selection set earlier.
     */
    if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDisk) {
        ASSERT(pExtOpts->fIncludeDiskImages);
        if (pExtOpts->fDiskTo2MG) {
            if (pEntry->GetUncompressedLen() > 0 &&
                (pEntry->GetUncompressedLen() % TwoImgHeader::kBlockSize) == 0)
            {
                extractAs2MG = true;
            } else {
                LOGI("Not extracting funky image '%ls' as 2MG (len=%I64d)",
                    (LPCWSTR) pEntry->GetPathNameUNI(),
                    pEntry->GetUncompressedLen());
            }
        }
    }

    /*
     * Convert the archived pathname to something suitable for the
     * local machine (i.e. Win32).
     */
    PathProposal pathProp;
    CString convName, convNameExtd, convNameExtdPlus;
    pathProp.Init(pEntry);
    pathProp.fThreadKind = thread;
    if (pExtOpts->fStripFolderNames)
        pathProp.fJunkPaths = true;

    pathProp.ArchiveToLocal();
    convName = pathProp.fLocalPathName;

    /* run it through again, this time with optional type preservation */
    if (pExtOpts->fAddTypePreservation)
        pathProp.fPreservation = true;
    pathProp.ArchiveToLocal();
    convNameExtd = pathProp.fLocalPathName;

    /* and a 3rd time, also taking additional extensions into account */
    if (pExtOpts->fAddExtension)
        pathProp.fAddExtension = true;
    pathProp.ArchiveToLocal();
    convNameExtdPlus = pathProp.fLocalPathName;

    /*
     * Prepend the extraction dir to the local pathname.  We also add
     * the sub-volume name (if any), which should already be a valid
     * Win32 directory name.  We can't add it earlier because the fssep
     * char might be '\0'.
     */
    ASSERT(pExtOpts->fExtractPath.Right(1) == "\\");
    CString adjustedExtractPath(pExtOpts->fExtractPath);
    if (!pExtOpts->fStripFolderNames && !pEntry->GetSubVolName().IsEmpty()) {
        adjustedExtractPath += pEntry->GetSubVolName();
        adjustedExtractPath += "\\";
    }
    outputPath = adjustedExtractPath;
    outputPath += convNameExtdPlus;


    ReformatOutput* pOutput = NULL;

    /*
     * If requested, try to reformat this file.
     */
    if (pHolder != NULL) {
        ReformatHolder::ReformatPart part = ReformatHolder::kPartUnknown;
        ReformatHolder::ReformatID id;
        CString title;
        //int result;

        switch (thread) {
        case GenericEntry::kDataThread:
            part = ReformatHolder::kPartData;
            break;
        case GenericEntry::kRsrcThread:
            part = ReformatHolder::kPartRsrc;
            break;
        case GenericEntry::kDiskImageThread:
            part = ReformatHolder::kPartData;
            break;
        case GenericEntry::kCommentThread:
        default:
            assert(false);
            return false;
        }

        fpActionProgress->SetFileName(_T("(reformatting)"));
        id = pHolder->FindBest(part);

        {
            CWaitCursor waitc;
            pOutput = pHolder->Apply(part, id);
        }

        if (pOutput != NULL) {
            /* use output pathname without preservation */
            CString tmpPath;
            bool goodReformat = true;
            bool noChangePath = false;

            tmpPath = adjustedExtractPath;
            tmpPath += convName;

            CString lastFour = tmpPath.Right(4);

            /*
             * Tack on a file extension identifying the reformatted
             * contents.  If the filename already has the correct
             * extension, don't tack it on again.
             */
            switch (pOutput->GetOutputKind()) {
            case ReformatOutput::kOutputText:
                if (lastFour.CompareNoCase(L".txt") != 0)
                    tmpPath += L".txt";
                break;
            case ReformatOutput::kOutputRTF:
                if (lastFour.CompareNoCase(L".rtf") != 0)
                    tmpPath += L".rtf";
                break;
            case ReformatOutput::kOutputCSV:
                if (lastFour.CompareNoCase(L".csv") != 0)
                    tmpPath += L".csv";
                break;
            case ReformatOutput::kOutputBitmap:
                if (lastFour.CompareNoCase(L".bmp") != 0)
                    tmpPath += L".bmp";
                break;
            case ReformatOutput::kOutputRaw:
                noChangePath = true;
                break;
            default:
                // kOutputErrorMsg, kOutputUnknown
                goodReformat = false;
                break;
            }

            /*
             * Issue #26: if "add type extension" is not checked, don't add
             * the format converter extension.  This is useful for Merlin
             * source files where you want to do the text conversion but still
             * want the file to end with ".S".
             */
            if (!pExtOpts->fAddExtension) {
                noChangePath = true;
            }

            if (goodReformat) {
                if (!noChangePath)
                    outputPath = tmpPath;
            } else {
                delete pOutput;
                pOutput = NULL;
            }
        }
    }
    if (extractAs2MG) {
        /*
         * Reduce to base name and add 2IMG suffix.  Would be nice to keep
         * the non-extended file type preservation stuff, but right now we
         * only expect unadorned sectors for that (and so does NuLib2).
         */
        outputPath = adjustedExtractPath;
        outputPath += convName;
        outputPath += L".2mg";
    }

    /* update the display in case we renamed it */
    if (outputPath != fpActionProgress->GetFileName()) {
        LOGI(" Renamed our output, from '%ls' to '%ls'",
            (LPCWSTR) fpActionProgress->GetFileName(), (LPCWSTR) outputPath);
        fpActionProgress->SetFileName(outputPath);
    }

    /*
     * Update the progress meter output filename, and reset the thermometer.
     */
    fpActionProgress->SetArcName(pathProp.fStoredPathName);
    fpActionProgress->SetFileName(outputPath);
    LOGI("Extracting from '%ls' to '%ls'",
        (LPCWSTR) pathProp.fStoredPathName, (LPCWSTR) outputPath);
    SET_PROGRESS_BEGIN();

    /*
     * Open the output file.
     *
     * Returns IDCANCEL on failures as well as user cancellation.
     */
    FILE* fp = NULL;
    int result;
    result = OpenOutputFile(&outputPath, pathProp, pEntry->GetModWhen(),
                pOverwriteExisting, pOvwrForAll, &fp);
    if (result == IDCANCEL) {
        // no messagebox for this one
        delete pOutput;
        return false;
    }


    /* update the display in case they renamed the file */
    if (outputPath != fpActionProgress->GetFileName()) {
        LOGI(" Detected rename, from '%ls' to '%ls'",
            (LPCWSTR) fpActionProgress->GetFileName(), (LPCWSTR) outputPath);
        fpActionProgress->SetFileName(outputPath);
    }

    if (fp == NULL) {
        /* looks like they elected to skip extraction of this file */
        delete pOutput;
        return true;
    }

    //EventPause(500);  // DEBUG DEBUG


    /*
     * Handle "extract as 2MG" by writing a 2MG header to the start
     * of the file before we hand off to the extraction function.
     *
     * NOTE: we're currently assuming that we're extracting an image
     * in ProDOS sector order.  This is a valid assumption so long as
     * we're only pulling disk images out of ShrinkIt archives.
     *
     * We don't currently use the WriteFooter call here, because we're
     * not adding comments.
     */
    if (extractAs2MG) {
        TwoImgHeader header;
        header.InitHeader(TwoImgHeader::kImageFormatProDOS,
            (long) pEntry->GetUncompressedLen(),
            (long) (pEntry->GetUncompressedLen() / TwoImgHeader::kBlockSize));
        int err;
        ASSERT(ftell(fp) == 0);
        err = header.WriteHeader(fp);
        if (err != 0) {
            errMsg.Format(L"Unable to save 2MG file '%ls': %hs\n",
                (LPCWSTR) outputPath, strerror(err));
            fpActionProgress->MessageBox(errMsg, failed,
                MB_OK | MB_ICONERROR);
            goto open_file_fail;
        }
        ASSERT(ftell(fp) == 64);    // size of 2MG header
    }


    /*
     * In some cases we want to override the automatic text detection.
     *
     * If we're in "auto" mode, force conversion on for DOS/RDOS text files.
     * This is important when "convHA" is off, because in high-ASCII mode
     * we might not recognize text files for what they are.  We also
     * consider 0x00 to be binary, which screws up random-access text.
     *
     * We don't want to do text conversion on disk images or resource
     * forks, ever.  Turn them off here.
     */
    GenericEntry::ConvertEOL thisConv;
    thisConv = convEOL;
    if (thisConv == GenericEntry::kConvertEOLAuto) {
        if (DiskImg::UsesDOSFileStructure(pEntry->GetSourceFS()) &&
            pEntry->GetFileType() == kFileTypeTXT)
        {
            LOGI("Switching EOLAuto to EOLOn for DOS text file");
            thisConv = GenericEntry::kConvertEOLOn;
        }
    } else if (convTextByType) {
        /* force it on or off when in conv-by-type mode */
        if (pEntry->GetFileType() == kFileTypeTXT ||
            pEntry->GetFileType() == kFileTypeSRC)
        {
            LOGI("Enabling EOL conv for text file");
            thisConv = GenericEntry::kConvertEOLOn;
        } else {
            ASSERT(thisConv == GenericEntry::kConvertEOLOff);
        }
    }
    if (thisConv != GenericEntry::kConvertEOLOff &&
        (thread == GenericEntry::kRsrcThread ||
         thread == GenericEntry::kDiskImageThread))
    {
        LOGI("Disabling EOL conv for resource fork or disk image");
        thisConv = GenericEntry::kConvertEOLOff;
    }


    /*
     * Extract the contents to the file.
     *
     * In some cases, notably when the file size exceeds the limit of
     * the reformatter, we will be trying to reformat but won't have
     * loaded the original data.  In such cases we fall through to the
     * normal extraction mode, because we threw out pOutput above when
     * the result was kOutputErrorMsg.
     *
     * (Could also be due to extraction failure, e.g. bad CRC.)
     */
    if (pOutput != NULL) {
        /*
         * We have the data in our buffer.  Write it out.  No need
         * to tweak the progress updater, which already shows 100%.
         *
         * There are four possibilities:
         *  - Valid text/rtf/csv converted text.  Write reformatted.
         *  - Valid bitmap converted.  Write bitmap.
         *  - No reformatter found, type is "raw".  Write raw.  (Note
         *    this may be zero bytes long for an empty file.)
         *  - Error message encoded in result.  Should not be here!
         */
        if (pOutput->GetOutputKind() == ReformatOutput::kOutputText ||
            pOutput->GetOutputKind() == ReformatOutput::kOutputRTF ||
            pOutput->GetOutputKind() == ReformatOutput::kOutputCSV)
        {
            LOGI("  Writing text, RTF, CSV, or raw");
            ASSERT(pOutput->GetTextBuf() != NULL);
            int err = 0;
            if (fwrite(pOutput->GetTextBuf(),
                       pOutput->GetTextLen(), 1, fp) != 1)
                err = errno;
            if (err != 0) {
                errMsg.Format(L"Unable to save reformatted file '%ls': %hs\n",
                    (LPCWSTR) outputPath, strerror(err));
                fpActionProgress->MessageBox(errMsg, failed,
                    MB_OK | MB_ICONERROR);
                writeFailed = true;
            } else {
                SET_PROGRESS_UPDATE(100);
            }
        } else if (pOutput->GetOutputKind() == ReformatOutput::kOutputBitmap) {
            LOGI("  Writing bitmap");
            ASSERT(pOutput->GetDIB() != NULL);
            int err = pOutput->GetDIB()->WriteToFile(fp);
            if (err != 0) {
                errMsg.Format(L"Unable to save bitmap '%ls': %hs\n",
                    (LPCWSTR) outputPath, strerror(err));
                fpActionProgress->MessageBox(errMsg, failed,
                    MB_OK | MB_ICONERROR);
                writeFailed = true;
            } else {
                SET_PROGRESS_UPDATE(100);
            }
        } else if (pOutput->GetOutputKind() == ReformatOutput::kOutputRaw) {
            /*
             * Send raw data through the text conversion configured in the
             * extract files dialog.  Any file for which no dedicated
             * reformatter could be found ends up here.
             *
             * We could just send it through to the generic non-reformatter
             * case, but that would require reading the file twice.
             */
            LOGI("  Writing un-reformatted data (%ld bytes)",
                pOutput->GetTextLen());
            ASSERT(pOutput->GetTextBuf() != NULL);
            bool lastCR = false;
            GenericEntry::ConvertHighASCII thisConvHA = convHA;
            int err;
            err = GenericEntry::WriteConvert(fp, pOutput->GetTextBuf(),
                    pOutput->GetTextLen(), &thisConv, &thisConvHA, &lastCR);
            if (err != 0) {
                errMsg.Format(L"Unable to write file '%ls': %hs\n",
                    (LPCWSTR) outputPath, strerror(err));
                fpActionProgress->MessageBox(errMsg, failed,
                    MB_OK | MB_ICONERROR);
                writeFailed = true;
            } else {
                SET_PROGRESS_UPDATE(100);
            }

        } else {
            /* something failed, and we don't have the file */
            LOGI("How'd we get here?");
            ASSERT(false);
        }
    } else {
        /*
         * We don't have the data, probably because we aren't using file
         * reformatters.  Use the GenericEntry extraction routine to copy
         * the data directly into the file.
         *
         * We also get here if the file has a length of zero.
         */
        CString msg;
        int result;
        ASSERT(fpActionProgress != NULL);
        LOGI("Extracting '%ls', requesting thisConv=%d, convHA=%d",
            (LPCWSTR) outputPath, thisConv, convHA);
        result = pEntry->ExtractThreadToFile(thread, fp,
                    thisConv, convHA, &msg);
        if (result != IDOK) {
            if (result == IDCANCEL) {
                CString msg;
                CheckedLoadString(&msg, IDS_OPERATION_CANCELLED);
                fpActionProgress->MessageBox(msg, 
                    L"CiderPress", MB_OK | MB_ICONEXCLAMATION);
            } else {
                LOGI("  FAILED on '%ls': %ls",
                    (LPCWSTR) outputPath, (LPCWSTR) msg);
                errMsg.Format(L"Unable to extract file '%ls': %ls\n",
                    (LPCWSTR) outputPath, (LPCWSTR) msg);
                fpActionProgress->MessageBox(errMsg, failed,
                    MB_OK | MB_ICONERROR);
            }
            writeFailed = true;
        }
    }

open_file_fail:
    delete pOutput;

    fclose(fp);
    if (writeFailed) {
        // clean up
        ::DeleteFile(outputPath);
        return false;
    }

    /*
     * Fix the modification date.
     */
    PathName datePath(outputPath);
    datePath.SetModWhen(pEntry->GetModWhen());
//  datePath.SetAccess(pEntry->GetAccess());

    return true;
}

int MainWindow::OpenOutputFile(CString* pOutputPath, const PathProposal& pathProp,
    time_t arcFileModWhen, bool* pOverwriteExisting, bool* pOvwrForAll,
    FILE** pFp)
{
    const int kUserCancel = -2;     // must not conflict with errno values
    CString failed;
    CString msg;
    int err = 0;

    CheckedLoadString(&failed, IDS_FAILED);

    *pFp = NULL;

did_rename:
    PathName path(*pOutputPath);
    if (path.Exists()) {
        if (*pOverwriteExisting) {
do_overwrite:
            /* delete existing */
            LOGI("  Deleting existing '%ls'", (LPCWSTR) *pOutputPath);
            if (::_wunlink(*pOutputPath) != 0) {
                err = errno;
                LOGI("  Failed deleting '%ls', err=%d",
                    (LPCWSTR)*pOutputPath, err);
                if (err == ENOENT) {
                    /* user might have removed it while dialog was up */
                    err = 0;
                } else {
                    /* unable to delete, we'd better bail out */
                    goto bail;
                }
            }
        } else if (*pOvwrForAll) {
            /* never overwrite */
            LOGI("  Skipping '%ls'", (LPCWSTR) *pOutputPath);
            goto bail;
        } else {
            /* no firm policy, ask the user */
            ConfirmOverwriteDialog confOvwr;
            PathName path(*pOutputPath);
            
            confOvwr.fExistingFile = *pOutputPath;
            confOvwr.fExistingFileModWhen = path.GetModWhen();
            confOvwr.fNewFileSource = pathProp.fStoredPathName;
            confOvwr.fNewFileModWhen = arcFileModWhen;
            if (confOvwr.DoModal() == IDCANCEL) {
                err = kUserCancel;
                goto bail;
            }
            if (confOvwr.fResultRename) {
                *pOutputPath = confOvwr.fExistingFile;
                goto did_rename;
            }
            if (confOvwr.fResultApplyToAll) {
                *pOvwrForAll = confOvwr.fResultApplyToAll;
                *pOverwriteExisting = confOvwr.fResultOverwrite;
            }
            if (confOvwr.fResultOverwrite)
                goto do_overwrite;
            else
                goto bail;
        }

    }

    /* create the subdirectories, if necessary */
    err = path.CreatePathIFN();
    if (err != 0)
        goto bail;

    *pFp = _wfopen(*pOutputPath, L"wb");
    if (*pFp == NULL)
        err = errno ? errno : -1;
    /* fall through with error */

bail:
    /* if we failed, tell the user why */
    if (err == ENOTDIR) {
        /* part of the output path exists, but isn't a directory */
        msg.Format(L"Unable to create folders for '%ls': part of the path "
                   L"already exists but is not a folder.\n",
            (LPCWSTR) *pOutputPath);
        fpActionProgress->MessageBox(msg, failed, MB_OK | MB_ICONERROR);
        return IDCANCEL;
    } else if (err == EINVAL) {
        /* invalid argument; assume it's an invalid filename */
        msg.Format(L"Unable to create file '%ls': invalid filename.\n",
            (LPCWSTR) *pOutputPath);
        fpActionProgress->MessageBox(msg, failed, MB_OK | MB_ICONERROR);
        return IDCANCEL;
    } else if (err == kUserCancel) {
        /* user elected to cancel */
        LOGI("Cancelling due to user request");
        return IDCANCEL;
    } else if (err != 0) {
        msg.Format(L"Unable to create file '%ls': %hs\n",
            (LPCWSTR) *pOutputPath, strerror(err));
        fpActionProgress->MessageBox(msg, failed, MB_OK | MB_ICONERROR);
        return IDCANCEL;
    }

    return IDOK;
}


/*
 * ==========================================================================
 *      Test
 * ==========================================================================
 */

void MainWindow::OnActionsTest(void)
{
    ASSERT(fpContentList != NULL);
    ASSERT(fpOpenArchive != NULL);

    /*
     * Ask the user about various options.
     */
    UseSelectionDialog selOpts(fpContentList->GetSelectedCount(), this);
    selOpts.Setup(IDS_TEST_TITLE, IDS_TEST_OK, IDS_TEST_SELECTED_COUNT,
        IDS_TEST_SELECTED_COUNTS_FMT, IDS_TEST_ALL_FILES);
    if (fpContentList->GetSelectedCount() > 0)
        selOpts.fFilesToAction = UseSelectionDialog::kActionSelection;
    else
        selOpts.fFilesToAction = UseSelectionDialog::kActionAll;

    if (selOpts.DoModal() != IDOK) {
        LOGI("Test cancelled");
        return;
    }

    /*
     * Create a "selection set" of things to test.
     *
     * We don't currently test directories, because we don't currently
     * allow testing anything that has a directory (NuFX doesn't store
     * them explicitly).  We could probably add them to the threadMask.
     */
    SelectionSet selSet;
    int threadMask = GenericEntry::kAnyThread;

    if (selOpts.fFilesToAction == UseSelectionDialog::kActionSelection) {
        selSet.CreateFromSelection(fpContentList, threadMask);
    } else {
        selSet.CreateFromAll(fpContentList, threadMask);
    }
    //selSet.Dump();

    if (selSet.GetNumEntries() == 0) {
        /* should be impossible */
        MessageBox(L"No files matched the selection criteria.",
            L"No match", MB_OK|MB_ICONEXCLAMATION);
        return;
    }

    /*
     * Set up the progress window.
     */
    bool result;

    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionTest, this);

    result = fpOpenArchive->TestSelection(fpActionProgress, &selSet);

    fpActionProgress->Cleanup(this);
    fpActionProgress = NULL;
    //if (result)
    //  SuccessBeep();
}
void
MainWindow::OnUpdateActionsTest(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && fpContentList->GetItemCount() > 0
        && fpOpenArchive->GetCapability(GenericArchive::kCapCanTest));
}


/*
 * ==========================================================================
 *      Delete
 * ==========================================================================
 */

void MainWindow::OnActionsDelete(void)
{
    ASSERT(fpContentList != NULL);
    ASSERT(fpOpenArchive != NULL);
    ASSERT(!fpOpenArchive->IsReadOnly());

    /*
     * We handle deletions specially.  If they have selected any
     * subdirectories, we recursively select the files in those subdirs
     * as well.  We want to do it early so that the "#of files to delete"
     * display accurately reflects what we're about to do.
     */
    fpContentList->SelectSubdirContents();

#if 0
    /*
     * Ask the user about various options.
     */
    UseSelectionDialog delOpts(fpContentList->GetSelectedCount(), this);
    delOpts.Setup(IDS_DEL_TITLE, IDS_DEL_OK, IDS_DEL_SELECTED_COUNT,
        IDS_DEL_SELECTED_COUNTS_FMT, IDS_DEL_ALL_FILES);
    if (fpContentList->GetSelectedCount() > 0)
        delOpts.fFilesToAction = UseSelectionDialog::kActionSelection;
    else
        delOpts.fFilesToAction = UseSelectionDialog::kActionAll;

    if (delOpts.DoModal() != IDOK) {
        LOGI("Delete cancelled");
        return;
    }
#endif

    /*
     * Create a "selection set" of things to delete.
     *
     * We can't delete volume directories, so they're not included.
     */
    SelectionSet selSet;
    int threadMask = GenericEntry::kAnyThread |
        GenericEntry::kAllowDirectory /*| GenericEntry::kAllowVolumeDir*/;

#if 0
    if (delOpts.fFilesToAction == UseSelectionDialog::kActionSelection) {
        selSet.CreateFromSelection(fpContentList, threadMask);
    } else {
        CString appName;
        UINT response;
        appName.LoadString(IDS_MB_APP_NAME);
        response = MessageBox("Are you sure you want to delete everything?",
            appName, MB_OKCANCEL | MB_ICONEXCLAMATION);
        if (response == IDCANCEL)
            return;
        selSet.CreateFromAll(fpContentList, threadMask);
    }
    //selSet.Dump();
#endif

    selSet.CreateFromSelection(fpContentList, threadMask);
    if (selSet.GetNumEntries() == 0) {
        /* can happen if they selected volume dir only */
        MessageBox(L"Nothing to delete.",
            L"No match", MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    CString appName, msg;

    CheckedLoadString(&appName, IDS_MB_APP_NAME);
    msg.Format(L"Delete %d file%ls?", selSet.GetNumEntries(),
        selSet.GetNumEntries() == 1 ? L"" : L"s");
    if (MessageBox(msg, appName, MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
        return;

    bool result;
    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionDelete, this);

    //fpContentList->Invalidate();      // don't allow updates until done
    result = fpOpenArchive->DeleteSelection(fpActionProgress, &selSet);
    fpContentList->Reload();

    fpActionProgress->Cleanup(this);
    fpActionProgress = NULL;

    if (result)
        SuccessBeep();
}
void MainWindow::OnUpdateActionsDelete(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && !fpOpenArchive->IsReadOnly()
        && fpContentList->GetSelectedCount() > 0);
}


/*
 * ==========================================================================
 *      Rename
 * ==========================================================================
 */

void MainWindow::OnActionsRename(void)
{
    ASSERT(fpContentList != NULL);
    ASSERT(fpOpenArchive != NULL);
    ASSERT(!fpOpenArchive->IsReadOnly());

    /*
     * Create a "selection set" of entries to rename.  We always go by
     * the selection, so there's no need to present a "all or some?" dialog.
     *
     * Renaming the volume dir is not done from here, so we don't include
     * it in the set.  We could theoretically allow renaming of "damaged"
     * files, since most of the time the damage is in the file structure
     * not the directory, but the disk will be read-only anyway so there's
     * no point.
     */
    SelectionSet selSet;
    int threadMask = GenericEntry::kAnyThread | GenericEntry::kAllowDirectory;

    selSet.CreateFromSelection(fpContentList, threadMask);
    //selSet.Dump();

    if (selSet.GetNumEntries() == 0) {
        /* should be impossible */
        MessageBox(L"No files matched the selection criteria.",
            L"No match", MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    //fpContentList->Invalidate();  // this might be unnecessary
    fpOpenArchive->RenameSelection(this, &selSet);
    fpContentList->Reload();

    // user interaction on each step, so skip the SuccessBeep
}
void MainWindow::OnUpdateActionsRename(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && !fpOpenArchive->IsReadOnly()
        && fpContentList->GetSelectedCount() > 0);
}


/*
 * ==========================================================================
 *      Edit Comment
 * ==========================================================================
 */

void MainWindow::OnActionsEditComment(void)
{
    ASSERT(fpContentList != NULL);
    ASSERT(fpOpenArchive != NULL);
    ASSERT(!fpOpenArchive->IsReadOnly());

    EditCommentDialog editDlg(this);
    CString oldComment;

    GenericEntry* pEntry = GetSelectedItem(fpContentList);
    if (pEntry == NULL) {
        ASSERT(false);
        return;
    }

    if (!pEntry->GetHasComment()) {
        CString question, title;
        int result;

        CheckedLoadString(&question, IDS_NO_COMMENT_ADD);
        CheckedLoadString(&title, IDS_EDIT_COMMENT);
        result = MessageBox(question, title, MB_OKCANCEL | MB_ICONQUESTION);
        if (result == IDCANCEL)
            return;

        editDlg.fComment = "";
        editDlg.fNewComment = true;
    } else {
        fpOpenArchive->GetComment(this, pEntry, &editDlg.fComment);
    }


    int result;
    result = editDlg.DoModal();
    if (result == IDOK) {
        //fpContentList->Invalidate();  // probably unnecessary
        fpOpenArchive->SetComment(this, pEntry, editDlg.fComment);
        fpContentList->Reload();
    } else if (result == EditCommentDialog::kDeleteCommentID) {
        //fpContentList->Invalidate();  // possibly unnecessary
        fpOpenArchive->DeleteComment(this, pEntry);
        fpContentList->Reload();
    }
}
void MainWindow::OnUpdateActionsEditComment(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && !fpOpenArchive->IsReadOnly() &&
        fpContentList->GetSelectedCount() == 1 &&
        fpOpenArchive->GetCapability(GenericArchive::kCapCanEditComment));
}


/*
 * ==========================================================================
 *      Edit Properties
 * ==========================================================================
 */

void MainWindow::OnActionsEditProps(void)
{
    ASSERT(fpContentList != NULL);
    ASSERT(fpOpenArchive != NULL);

    EditPropsDialog propsDlg(this);
    CString oldComment;

    GenericEntry* pEntry = GetSelectedItem(fpContentList);
    if (pEntry == NULL) {
        ASSERT(false);
        return;
    }

    propsDlg.InitProps(pEntry);
    if (fpOpenArchive->IsReadOnly())
        propsDlg.fReadOnly = true;
    else if (pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir)
        propsDlg.fReadOnly = true;

    int result;
    result = propsDlg.DoModal();
    if (result == IDOK && !propsDlg.fReadOnly) {
        (void) fpOpenArchive->SetProps(this, pEntry, &propsDlg.fProps);

        // only needed if underlying archive reloads
        fpContentList->Reload(true);
    }
}
void MainWindow::OnUpdateActionsEditProps(CCmdUI* pCmdUI)
{
    // allow it in read-only mode, so we can view the props
    pCmdUI->Enable(fpContentList != NULL &&
        fpContentList->GetSelectedCount() == 1);
}


/*
 * ==========================================================================
 *      Rename Volume
 * ==========================================================================
 */

void MainWindow::OnActionsRenameVolume(void)
{
    RenameVolumeDialog rvDialog;

    ASSERT(fpContentList != NULL);
    ASSERT(fpOpenArchive != NULL);
    ASSERT(!fpOpenArchive->IsReadOnly());

    /* only know how to deal with disk images */
    if (fpOpenArchive->GetArchiveKind() != GenericArchive::kArchiveDiskImage) {
        ASSERT(false);
        return;
    }

    DiskImgLib::DiskFS* pDiskFS;

    pDiskFS = ((DiskArchive*) fpOpenArchive)->GetDiskFS();
    ASSERT(pDiskFS != NULL);

    rvDialog.fpArchive = (DiskArchive*) fpOpenArchive;
    if (rvDialog.DoModal() != IDOK)
        return;

    //LOGI("Creating '%s'", rvDialog.fNewName);

    /* rename the chosen disk to the specified name */
    bool result;
    result = fpOpenArchive->RenameVolume(this, rvDialog.fpChosenDiskFS,
                rvDialog.fNewName);
    if (!result) {
        LOGW("RenameVolume FAILED");
        /* keep going -- reload just in case something partially happened */
    }

    /*
     * We need to do two things: reload the content list, because the
     * underlying DiskArchive got reloaded, and update the title bar.  We
     * put the "volume ID" in the title, and we most likely just changed it.
     *
     * SetCPTitle invokes fpOpenArchive->GetDescription(), which pulls the
     * volume ID out of the primary DiskFS.
     */
    fpContentList->Reload();
    SetCPTitle(fOpenArchivePathName, fpOpenArchive);
}
void MainWindow::OnUpdateActionsRenameVolume(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && !fpOpenArchive->IsReadOnly() &&
        fpOpenArchive->GetCapability(GenericArchive::kCapCanRenameVolume));
}


/*
 * ==========================================================================
 *      Recompress
 * ==========================================================================
 */

void MainWindow::OnActionsRecompress(void)
{
    ASSERT(fpContentList != NULL);
    ASSERT(fpOpenArchive != NULL);

    /*
     * Ask the user about various options.
     */
    RecompressOptionsDialog selOpts(fpContentList->GetSelectedCount(), this);
    selOpts.Setup(IDS_RECOMP_TITLE, IDS_RECOMP_OK, IDS_RECOMP_SELECTED_COUNT,
        IDS_RECOMP_SELECTED_COUNTS_FMT, IDS_RECOMP_ALL_FILES);
    if (fpContentList->GetSelectedCount() > 0)
        selOpts.fFilesToAction = UseSelectionDialog::kActionSelection;
    else
        selOpts.fFilesToAction = UseSelectionDialog::kActionAll;

    selOpts.fCompressionType = fPreferences.GetPrefLong(kPrCompressionType);

    if (selOpts.DoModal() != IDOK) {
        LOGI("Recompress cancelled");
        return;
    }

    /*
     * Create a "selection set" of data forks, resource forks, and disk
     * images.  If an entry has nothing but a comment, ignore it.
     */
    SelectionSet selSet;
    int threadMask = GenericEntry::kDataThread | GenericEntry::kRsrcThread |
        GenericEntry::kDiskImageThread;

    if (selOpts.fFilesToAction == UseSelectionDialog::kActionSelection) {
        selSet.CreateFromSelection(fpContentList, threadMask);
    } else {
        selSet.CreateFromAll(fpContentList, threadMask);
    }
    //selSet.Dump();

    if (selSet.GetNumEntries() == 0) {
        /* should be impossible */
        MessageBox(L"No files matched the selection criteria.",
            L"No match", MB_OK|MB_ICONEXCLAMATION);
        return;
    }

    LONGLONG beforeUncomp, beforeComp;
    LONGLONG afterUncomp, afterComp;
    CalcTotalSize(&beforeUncomp, &beforeComp);

    /*
     * Set up the progress window.
     */
    int result;

    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionRecompress, this);

    //fpContentList->Invalidate();      // possibly unnecessary
    result = fpOpenArchive->RecompressSelection(fpActionProgress, &selSet,
                &selOpts);
    fpContentList->Reload();

    fpActionProgress->Cleanup(this);
    fpActionProgress = NULL;

    
    if (result) {
        CString msg, appName;

        CalcTotalSize(&afterUncomp, &afterComp);
        ASSERT(beforeUncomp == afterUncomp);

        CheckedLoadString(&appName, IDS_MB_APP_NAME);
        msg.Format(L"Total uncompressed size of all files:\t%.1fK\r\n"
                   L"Total size before recompress:\t\t%.1fK\r\n"
                   L"Total size after recompress:\t\t%.1fK\r\n"
                   L"Overall reduction:\t\t\t%.1fK",
            beforeUncomp / 1024.0, beforeComp / 1024.0, afterComp / 1024.0,
            (beforeComp - afterComp) / 1024.0);
        MessageBox(msg, appName, MB_OK|MB_ICONINFORMATION);
    }
}
void MainWindow::OnUpdateActionsRecompress(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && !fpOpenArchive->IsReadOnly() &&
        fpContentList->GetItemCount() > 0 &&
        fpOpenArchive->GetCapability(GenericArchive::kCapCanRecompress));
}

void MainWindow::CalcTotalSize(LONGLONG* pUncomp, LONGLONG* pComp) const
{
    GenericEntry* pEntry = fpOpenArchive->GetEntries();
    LONGLONG uncomp = 0, comp = 0;

    while (pEntry != NULL) {
        uncomp += pEntry->GetUncompressedLen();
        comp += pEntry->GetCompressedLen();
        pEntry = pEntry->GetNext();
    }

    *pUncomp = uncomp;
    *pComp = comp;
}


/*
 * ==========================================================================
 *      Convert to disk archive
 * ==========================================================================
 */

void MainWindow::OnActionsConvDisk(void)
{
    ASSERT(fpContentList != NULL);
    ASSERT(fpOpenArchive != NULL);

    /*
     * Ask the user about various options.
     */
    ConvDiskOptionsDialog selOpts(fpContentList->GetSelectedCount(), this);
    selOpts.Setup(IDS_CONVDISK_TITLE, IDS_CONVDISK_OK, IDS_CONVDISK_SELECTED_COUNT,
        IDS_CONVDISK_SELECTED_COUNTS_FMT, IDS_CONVDISK_ALL_FILES);
    if (fpContentList->GetSelectedCount() > 0)
        selOpts.fFilesToAction = UseSelectionDialog::kActionSelection;
    else
        selOpts.fFilesToAction = UseSelectionDialog::kActionAll;

    //selOpts.fAllowLower =
    //  fPreferences.GetPrefBool(kPrConvDiskAllowLower);
    //selOpts.fSparseAlloc =
    //  fPreferences.GetPrefBool(kPrConvDiskAllocSparse);

    if (selOpts.DoModal() != IDOK) {
        LOGI("ConvDisk cancelled");
        return;
    }

    ASSERT(selOpts.fNumBlocks > 0);

    //fPreferences.SetPrefBool(kPrConvDiskAllowLower,
    //  selOpts.fAllowLower != 0);
    //fPreferences.SetPrefBool(kPrConvDiskAllocSparse,
    //  selOpts.fSparseAlloc != 0);

    /*
     * Create a "selection set" of data forks, resource forks, and
     * disk images.  We don't want comment threads, but we can ignore
     * them later.
     */
    SelectionSet selSet;
    int threadMask = GenericEntry::kAnyThread;

    if (selOpts.fFilesToAction == UseSelectionDialog::kActionSelection) {
        selSet.CreateFromSelection(fpContentList, threadMask);
    } else {
        selSet.CreateFromAll(fpContentList, threadMask);
    }
    //selSet.Dump();

    if (selSet.GetNumEntries() == 0) {
        /* should be impossible */
        MessageBox(L"No files matched the selection criteria.",
            L"No match", MB_OK|MB_ICONEXCLAMATION);
        return;
    }

    XferFileOptions xferOpts;
    //xferOpts.fAllowLowerCase =
    //  fPreferences.GetPrefBool(kPrProDOSAllowLower) != 0;
    //xferOpts.fUseSparseBlocks =
    //  fPreferences.GetPrefBool(kPrProDOSUseSparse) != 0;

    LOGI("New volume name will be '%ls'", (LPCWSTR) selOpts.fVolName);

    /*
     * Create a new disk image.
     */
    CString filename, saveFolder, errStr;

    CFileDialog dlg(FALSE, L"po", NULL,
        OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN|OFN_HIDEREADONLY,
        L"Disk Images (*.po)|*.po||", this);

    dlg.m_ofn.lpstrTitle = L"New Disk Image (.PO)";
    dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

    if (dlg.DoModal() != IDOK) {
        LOGI(" User cancelled xfer from image create dialog");
        return;
    }

    saveFolder = dlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    filename = dlg.GetPathName();
    LOGI(" Will xfer to file '%ls'", (LPCWSTR) filename);

    /* remove file if it already exists */
    CString errMsg;
    errMsg = RemoveFile(filename);
    if (!errMsg.IsEmpty()) {
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        return;
    }

    DiskArchive::NewOptions options;
    memset(&options, 0, sizeof(options));
    options.base.format = DiskImg::kFormatProDOS;
    options.base.sectorOrder = DiskImg::kSectorOrderProDOS;
    options.prodos.volName = selOpts.fVolName;
    options.prodos.numBlocks = selOpts.fNumBlocks;

    xferOpts.fTarget = new DiskArchive;
    errStr = xferOpts.fTarget->New(filename, &options);
    if (!errStr.IsEmpty()) {
        ShowFailureMsg(this, errStr, IDS_FAILED);
        delete xferOpts.fTarget;
        return;
    }

    /*
     * Set up the progress window.
     */
    GenericArchive::XferStatus result;

    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionConvDisk, this);

    result = fpOpenArchive->XferSelection(fpActionProgress, &selSet,
                fpActionProgress, &xferOpts);

    fpActionProgress->Cleanup(this);
    fpActionProgress = NULL;

    if (result == GenericArchive::kXferOK)
        SuccessBeep();

    /* clean up */
    delete xferOpts.fTarget;
}
void MainWindow::OnUpdateActionsConvDisk(CCmdUI* pCmdUI)
{
    /* right now, only NufxArchive has the Xfer stuff implemented */
    pCmdUI->Enable(fpContentList != NULL &&
        fpContentList->GetItemCount() > 0 &&
        fpOpenArchive->GetArchiveKind() == GenericArchive::kArchiveNuFX);
}


/*
 * ==========================================================================
 *      Convert disk image to NuFX file archive
 * ==========================================================================
 */

void MainWindow::OnActionsConvFile(void)
{
    ASSERT(fpContentList != NULL);
    ASSERT(fpOpenArchive != NULL);

    /*
     * Ask the user about various options.
     */
    ConvFileOptionsDialog selOpts(fpContentList->GetSelectedCount(), this);
    selOpts.Setup(IDS_CONVFILE_TITLE, IDS_CONVFILE_OK, IDS_CONVFILE_SELECTED_COUNT,
        IDS_CONVFILE_SELECTED_COUNTS_FMT, IDS_CONVFILE_ALL_FILES);
    if (fpContentList->GetSelectedCount() > 0)
        selOpts.fFilesToAction = UseSelectionDialog::kActionSelection;
    else
        selOpts.fFilesToAction = UseSelectionDialog::kActionAll;

    //selOpts.fConvDOSText =
    //  fPreferences.GetPrefBool(kPrConvFileConvDOSText);
    //selOpts.fConvPascalText =
    //  fPreferences.GetPrefBool(kPrConvFileConvPascalText);
    selOpts.fPreserveEmptyFolders =
        fPreferences.GetPrefBool(kPrConvFileEmptyFolders);

    if (selOpts.DoModal() != IDOK) {
        LOGI("ConvFile cancelled");
        return;
    }

    //fPreferences.SetPrefBool(kPrConvFileConvDOSText,
    //  selOpts.fConvDOSText != 0);
    //fPreferences.SetPrefBool(kPrConvFileConvPascalText,
    //  selOpts.fConvPascalText != 0);
    fPreferences.SetPrefBool(kPrConvFileEmptyFolders,
        selOpts.fPreserveEmptyFolders != 0);

    /*
     * Create a "selection set" of data forks, resource forks, and
     * directories.  There are no comments or disk images on a disk image,
     * so we just request "any" thread.
     *
     * We only need to explicitly include directories if "preserve
     * empty folders" is set.
     */
    SelectionSet selSet;
    int threadMask = GenericEntry::kAnyThread;
    if (selOpts.fPreserveEmptyFolders)
        threadMask |= GenericEntry::kAllowDirectory;

    if (selOpts.fFilesToAction == UseSelectionDialog::kActionSelection) {
        selSet.CreateFromSelection(fpContentList, threadMask);
    } else {
        selSet.CreateFromAll(fpContentList, threadMask);
    }
    //selSet.Dump();

    if (selSet.GetNumEntries() == 0) {
        MessageBox(L"No files matched the selection criteria.",
            L"No match", MB_OK|MB_ICONEXCLAMATION);
        return;
    }

    XferFileOptions xferOpts;
    //xferOpts.fConvDOSText = (selOpts.fConvDOSText != 0);
    //xferOpts.fConvPascalText = (selOpts.fConvPascalText != 0);
    xferOpts.fPreserveEmptyFolders = (selOpts.fPreserveEmptyFolders != 0);

    /*
     * Create a new NuFX archive.
     */
    CString filename, saveFolder, errStr;

    CFileDialog dlg(FALSE, L"shk", NULL,
        OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN|OFN_HIDEREADONLY,
        L"ShrinkIt Archives (*.shk)|*.shk||", this);

    dlg.m_ofn.lpstrTitle = L"New Archive";
    dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

    if (dlg.DoModal() != IDOK) {
        LOGI(" User cancelled xfer from archive create dialog");
        return;
    }

    saveFolder = dlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    filename = dlg.GetPathName();
    LOGD(" Will xfer to file '%ls'", (LPCWSTR) filename);

    /* remove file if it already exists */
    CString errMsg;
    errMsg = RemoveFile(filename);
    if (!errMsg.IsEmpty()) {
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        return;
    }

    xferOpts.fTarget = new NufxArchive;
    errStr = xferOpts.fTarget->New(filename, NULL);
    if (!errStr.IsEmpty()) {
        ShowFailureMsg(this, errStr, IDS_FAILED);
        delete xferOpts.fTarget;
        return;
    }

    /*
     * Set up the progress window.
     */
    GenericArchive::XferStatus result;

    fpActionProgress = new ActionProgressDialog;
    fpActionProgress->Create(ActionProgressDialog::kActionConvFile, this);

    result = fpOpenArchive->XferSelection(fpActionProgress, &selSet,
                fpActionProgress, &xferOpts);

    fpActionProgress->Cleanup(this);
    fpActionProgress = NULL;
    if (result == GenericArchive::kXferOK)
        SuccessBeep();

    /* clean up */
    delete xferOpts.fTarget;
}
void MainWindow::OnUpdateActionsConvFile(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL &&
        fpContentList->GetItemCount() > 0 &&
        fpOpenArchive->GetArchiveKind() == GenericArchive::kArchiveDiskImage);
}


/*
 * ==========================================================================
 *      Cassette WAV conversions
 * ==========================================================================
 */

void MainWindow::OnActionsConvToWav(void)
{
    // do this someday
    LOGI("Convert TO wav");
}
void MainWindow::OnUpdateActionsConvToWav(CCmdUI* pCmdUI)
{
    BOOL enable = false;

    if (fpContentList != NULL && fpContentList->GetSelectedCount() == 1) {
        /* only BAS, INT, and BIN shorter than 64K */
        GenericEntry* pEntry = GetSelectedItem(fpContentList);

        if ((pEntry->GetFileType() == kFileTypeBAS ||
             pEntry->GetFileType() == kFileTypeINT ||
             pEntry->GetFileType() == kFileTypeBIN) &&
            pEntry->GetDataForkLen() < 65536 &&
            pEntry->GetRecordKind() == GenericEntry::kRecordKindFile)
        {
            enable = true;
        }
    }
    pCmdUI->Enable(enable);
}

void MainWindow::OnActionsConvFromWav(void)
{
    CassetteDialog dlg;
    CString fileName, saveFolder;

    CFileDialog fileDlg(TRUE, L"wav", NULL, OFN_FILEMUSTEXIST|OFN_HIDEREADONLY,
        L"Sound Files (*.wav)|*.wav||", this);
    fileDlg.m_ofn.lpstrTitle = L"Open Sound File";
    fileDlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenWAVFolder);

    if (fileDlg.DoModal() != IDOK)
        goto bail;

    saveFolder = fileDlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(fileDlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenWAVFolder, saveFolder);

    fileName = fileDlg.GetPathName();
    LOGI("Opening WAV file '%ls'", (LPCWSTR) fileName);

    dlg.fFileName = fileName;
    // pass in fpOpenArchive?

    dlg.DoModal();
    if (dlg.IsDirty()) {
        assert(fpContentList != NULL);
        fpContentList->Reload();
    }

bail:
    return;
}
void MainWindow::OnUpdateActionsConvFromWav(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && !fpOpenArchive->IsReadOnly());
}

/*static*/ bool MainWindow::SaveToArchive(GenericArchive::LocalFileDetails* pDetails,
    const uint8_t* dataBufIn, long dataLen,
    const uint8_t* rsrcBufIn, long rsrcLen,
    CString* pErrMsg, CWnd* pDialog)
{
    MainWindow* pMain = GET_MAIN_WINDOW();
    GenericArchive* pArchive = pMain->GetOpenArchive();
    DiskImgLib::A2File* pTargetSubdir = NULL;
    XferFileOptions xferOpts;
    CString storagePrefix;
    uint8_t* dataBuf = NULL;
    uint8_t* rsrcBuf = NULL;

    ASSERT(pArchive != NULL);
    ASSERT(pErrMsg->IsEmpty());

    /*
     * Make a copy of the data for XferFile.
     */
    if (dataLen >= 0) {
        if (dataLen == 0)
            dataBuf = new unsigned char[1];
        else
            dataBuf = new unsigned char[dataLen];
        if (dataBuf == NULL) {
            pErrMsg->Format(L"Unable to allocate %ld bytes", dataLen);
            goto bail;
        }
        memcpy(dataBuf, dataBufIn, dataLen);
    }
    if (rsrcLen >= 0) {
        assert(false);
    }


    /*
     * Figure out where we want to put the files.  For a disk archive
     * this can be complicated.
     *
     * The target DiskFS (which could be a sub-volume) gets tucked into
     * the xferOpts.
     */
    if (pArchive->GetArchiveKind() == GenericArchive::kArchiveDiskImage) {
        if (!pMain->ChooseAddTarget(&pTargetSubdir, &xferOpts.fpTargetFS))
            goto bail;
    } else if (pArchive->GetArchiveKind() == GenericArchive::kArchiveNuFX) {
        // Always use ':' separator for SHK; this is a matter of
        // convenience, so they can specify a full path.
        //details.storageName.Replace(':', '_');
        pDetails->SetFssep(':');
    }
    if (pTargetSubdir != NULL) {
        storagePrefix = pTargetSubdir->GetPathName();
        LOGD(" using storagePrefix '%ls'", (LPCWSTR) storagePrefix);
    }
    if (!storagePrefix.IsEmpty()) {
        CString tmpStr, tmpFileName;
        tmpFileName = pDetails->GetStrippedLocalPathName();
        tmpFileName.Replace(':', '_');  // strip any ':'s in the name
        pDetails->SetFssep(':');
        tmpStr = storagePrefix;
        tmpStr += ':';
        tmpStr += tmpFileName;
        pDetails->SetStrippedLocalPathName(tmpStr);
    }

    /*
     * Handle the transfer.
     *
     * On success, XferFile will null out our dataBuf and rsrcBuf pointers.
     */
    pArchive->XferPrepare(&xferOpts);

    *pErrMsg = pArchive->XferFile(pDetails, &dataBuf, dataLen,
                &rsrcBuf, rsrcLen);
    delete[] dataBuf;
    delete[] rsrcBuf;

    if (pErrMsg->IsEmpty())
        pArchive->XferFinish(pDialog);
    else
        pArchive->XferAbort(pDialog);

bail:
    return (pErrMsg->IsEmpty() != 0);
}


/*
 * ==========================================================================
 *      Import BASIC programs from a text file
 * ==========================================================================
 */

void MainWindow::OnActionsImportBAS(void)
{
    ImportBASDialog dlg;
    CString fileName, saveFolder;

    CFileDialog fileDlg(TRUE, L"txt", NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
        L"Text files (*.txt)|*.txt||", this);
    fileDlg.m_ofn.lpstrTitle = L"Open Text File";
    fileDlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrAddFileFolder);

    if (fileDlg.DoModal() != IDOK)
        goto bail;

    saveFolder = fileDlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(fileDlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrAddFileFolder, saveFolder);

    fileName = fileDlg.GetPathName();
    LOGI("Opening TXT file '%ls'", (LPCWSTR) fileName);

    dlg.SetFileName(fileName);
    // pass in fpOpenArchive?

    dlg.DoModal();
    if (dlg.IsDirty()) {
        assert(fpContentList != NULL);
        fpContentList->Reload();
    }

bail:
    return;
}
void MainWindow::OnUpdateActionsImportBAS(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(fpContentList != NULL && !fpOpenArchive->IsReadOnly());
}


/*
 * ==========================================================================
 *      Multiple file handling
 * ==========================================================================
 */

int MainWindow::GetFileParts(const GenericEntry* pEntry,
    ReformatHolder** ppHolder) const
{
    ReformatHolder* pHolder = new ReformatHolder;
    CString errMsg;

    if (pHolder == NULL)
        return -1;

    if (pEntry->GetHasDataFork())
        GetFilePart(pEntry, GenericEntry::kDataThread, pHolder);
    if (pEntry->GetHasRsrcFork())
        GetFilePart(pEntry, GenericEntry::kRsrcThread, pHolder);
    if (pEntry->GetHasComment())
        GetFilePart(pEntry, GenericEntry::kCommentThread, pHolder);
    if (pEntry->GetHasDiskImage())
        GetFilePart(pEntry, GenericEntry::kDiskImageThread, pHolder);

    *ppHolder = pHolder;

    return 0;
}

void MainWindow::GetFilePart(const GenericEntry* pEntry, int whichThread,
    ReformatHolder* pHolder) const
{
    CString errMsg;
    ReformatHolder::ReformatPart part;
    char* buf = NULL;
    long len = 0;
    di_off_t threadLen;
    int result;

    switch (whichThread) {
    case GenericEntry::kDataThread:
        part = ReformatHolder::kPartData;
        threadLen = pEntry->GetDataForkLen();
        break;
    case GenericEntry::kRsrcThread:
        part = ReformatHolder::kPartRsrc;
        threadLen = pEntry->GetRsrcForkLen();
        break;
    case GenericEntry::kCommentThread:
        part = ReformatHolder::kPartCmmt;
        threadLen = -1;     // no comment len getter; assume it's small
        break;
    case GenericEntry::kDiskImageThread:
        part = ReformatHolder::kPartData;       // put disks into data thread
        threadLen = pEntry->GetDataForkLen();
        break;
    default:
        ASSERT(false);
        return;
    }

    if (threadLen > fPreferences.GetPrefLong(kPrMaxViewFileSize)) {
        errMsg.Format(
            L"[File size (%I64d KBytes) exceeds file viewer maximum (%ld KBytes).  "
            L"The limit can be adjusted in the file viewer preferences.]\n",
            ((LONGLONG) threadLen + 1023) / 1024,
            (fPreferences.GetPrefLong(kPrMaxViewFileSize) + 1023) / 1024);
        pHolder->SetErrorMsg(part, errMsg);
        goto bail;
    }
    

    result = pEntry->ExtractThreadToBuffer(whichThread, &buf, &len, &errMsg);

    if (result == IDOK) {
        /* on success, ETTB guarantees a buffer, even for zero-len file */
        ASSERT(buf != NULL);
        pHolder->SetSourceBuf(part, (unsigned char*) buf, len);
    } else if (result == IDCANCEL) {
        /* not expected */
        errMsg = L"Cancelled!";
        pHolder->SetErrorMsg(part, errMsg);
        ASSERT(buf == NULL);
    } else {
        /* transfer error message to ReformatHolder buffer */
        LOGI("Got error message from ExtractThread: '%ls'",
            (LPCWSTR) errMsg);
        pHolder->SetErrorMsg(part, errMsg);
        ASSERT(buf == NULL);
    }

bail:
    return;
}
