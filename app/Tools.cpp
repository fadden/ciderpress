/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Some items from the "tools" menu.
 *
 * [ There's a lot of cutting and pasting going on in here.  Some of this
 *   stuff needs to get refactored.  ++ATM 20040729 ]
 */
#include "StdAfx.h"
#include "Main.h"
#include "DiskEditDialog.h"
#include "ImageFormatDialog.h"
#include "DiskConvertDialog.h"
#include "ChooseDirDialog.h"
#include "DoneOpenDialog.h"
#include "OpenVolumeDialog.h"
#include "DiskEditOpenDialog.h"
#include "VolumeCopyDialog.h"
#include "CreateImageDialog.h"
#include "DiskArchive.h"
#include "EOLScanDialog.h"
#include "TwoImgPropsDialog.h"
#include <io.h>     // need chsize() for TwoImgProps


int MainWindow::TryDiskImgOverride(DiskImg* pImg, const WCHAR* fileSource,
    DiskImg::FSFormat defaultFormat, int* pDisplayFormat, bool allowUnknown,
    CString* pErrMsg)
{
    ImageFormatDialog imf;

    *pErrMsg = "";
    imf.InitializeValues(pImg);
    imf.fFileSource = fileSource;
    imf.fAllowUnknown = allowUnknown;
    if (pDisplayFormat == NULL)
        imf.SetQueryDisplayFormat(false);

    /* don't show "unknown format" if we have a default value */
    if (defaultFormat != DiskImg::kFormatUnknown &&
        imf.fFSFormat == DiskImg::kFormatUnknown)
    {
        imf.fFSFormat = defaultFormat;
    }

    LOGI(" On entry, sectord=%d format=%d",
        imf.fSectorOrder, imf.fFSFormat);

    if (imf.DoModal() != IDOK) {
        LOGI(" User bailed on IMF dialog");
        return IDCANCEL;
    }

    LOGI(" On exit, sectord=%d format=%d",
        imf.fSectorOrder, imf.fFSFormat);

    if (pDisplayFormat != NULL)
        *pDisplayFormat = imf.fDisplayFormat;
    if (imf.fSectorOrder != pImg->GetSectorOrder() ||
        imf.fFSFormat != pImg->GetFSFormat())
    {
        LOGI("Initial values overridden, forcing img format");
        DIError dierr;
        dierr = pImg->OverrideFormat(pImg->GetPhysicalFormat(), imf.fFSFormat,
                    imf.fSectorOrder);
        if (dierr != kDIErrNone) {
            pErrMsg->Format(L"Unable to access disk image using selected"
                            L" parameters.  Error: %hs.",
                DiskImgLib::DIStrError(dierr));
            // fall through to "return IDOK"
        }
    }

    return IDOK;
}


/*
 * ==========================================================================
 *      Disk Editor
 * ==========================================================================
 */

void MainWindow::OnToolsDiskEdit(void)
{
    DIError dierr;
    DiskImg img;
    CString loadName, saveFolder;
    CString failed, errMsg;
    DiskEditOpenDialog diskEditOpen(this);
    /* create three, show one */
    BlockEditDialog blockEdit(this);
    SectorEditDialog sectorEdit(this);
    NibbleEditDialog nibbleEdit(this);
    DiskEditDialog* pEditDialog;
    int displayFormat;
    bool readOnly = true;

    /* flush current archive in case that's what we're planning to edit */
    OnFileSave();

    CheckedLoadString(&failed, IDS_FAILED);

    diskEditOpen.fArchiveOpen = false;
    if (fpOpenArchive != NULL &&
        fpOpenArchive->GetArchiveKind() == GenericArchive::kArchiveDiskImage)
    {
        diskEditOpen.fArchiveOpen = true;
    }

    if (diskEditOpen.DoModal() != IDOK)
        goto bail;

    /*
     * Choose something to open, based on "fOpenWhat".
     */
    if (diskEditOpen.fOpenWhat == DiskEditOpenDialog::kOpenFile) {
        CString openFilters, saveFolder;

        openFilters = kOpenDiskImage;
        openFilters += kOpenAll;
        openFilters += kOpenEnd;
        CFileDialog dlg(TRUE, L"dsk", NULL, OFN_FILEMUSTEXIST, openFilters, this);

        /* for now, everything is read-only */
        dlg.m_ofn.Flags |= OFN_HIDEREADONLY;
        dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

        if (dlg.DoModal() != IDOK)
            goto bail;

        loadName = dlg.GetPathName();
        readOnly = true;        // add to file dialog

        saveFolder = dlg.m_ofn.lpstrFile;
        saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
        fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);
    } else if (diskEditOpen.fOpenWhat == DiskEditOpenDialog::kOpenVolume) {
        OpenVolumeDialog dlg(this);
        int result;

        result = dlg.DoModal();
        if (result != IDOK)
            goto bail;

        loadName = dlg.fChosenDrive;
        readOnly = (dlg.fReadOnly != 0);
    } else if (diskEditOpen.fOpenWhat == DiskEditOpenDialog::kOpenCurrent) {
        // get values from currently open archive
        assert(fpOpenArchive != NULL);
        loadName = fpOpenArchive->GetPathName();
        readOnly = fpOpenArchive->IsReadOnly();
    } else {
        LOGI("GLITCH: unexpected fOpenWhat %d", diskEditOpen.fOpenWhat);
        ASSERT(false);
        goto bail;
    }

    LOGI("Disk editor what=%d name='%ls' ro=%d",
        diskEditOpen.fOpenWhat, (LPCWSTR) loadName, readOnly);


#if 1
    {
        CWaitCursor waitc;
        /* open the image file and analyze it */
        CStringA loadNameA(loadName);
        dierr = img.OpenImage(loadNameA, PathProposal::kLocalFssep, true);
    }
#else
    /* quick test of memory-buffer-based interface */
    FILE* tmpfp;
    char* phatbuf;
    long length;

    tmpfp = fopen(loadName, "rb");
    ASSERT(tmpfp != NULL);
    fseek(tmpfp, 0, SEEK_END);
    length = ftell(tmpfp);
    rewind(tmpfp);
    LOGI(" PHATBUF %d", length);
    phatbuf = new char[length];
    if (fread(phatbuf, length, 1, tmpfp) != 1)
        LOGI("FREAD FAILED %d", errno);
    fclose(tmpfp);
    dierr = img.OpenImage(phatbuf, length, true);
#endif

    if (dierr != kDIErrNone) {
        errMsg.Format(L"Unable to open disk image: %hs.",
            DiskImgLib::DIStrError(dierr));
        MessageBox(errMsg, failed, MB_OK|MB_ICONSTOP);
        goto bail;
    }

#if 0
    {
        /*
         * TEST - set custom entry to match Sheila NIB image.  We have to
         * do this here so that the disk routines can analyze the disk
         * correctly.  We need a way to enter these parameters in the
         * disk editor and then re-analyze the image.  (Not to mention a way
         * to flip in and out of block/sector/nibble mode.)
         */
        DiskImg::NibbleDescr sheilaDescr =
        {
            "H.A.L. Labs (Sheila)",
            16,
            { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
            0x00,   // checksum seed
            true,   // verify checksum
            true,   // verify track
            2,      // epilog verify count
            { 0xd5, 0xaa, 0xda }, { 0xde, 0xaa, 0xeb },
            0x00,   // checksum seed
            true,   // verify checksum
            2,      // epilog verify count
            DiskImg::kNibbleEnc62,
            DiskImg::kNibbleSpecialNone,
        };
        /* same thing, but for original 13-sector Zork */
        DiskImg::NibbleDescr zork13Descr =
        {
            "Zork 13-sector",
            13,
            { 0xd5, 0xaa, 0xb5 }, { 0xde, 0xaa, 0xeb },
            0x00,
            false,
            false,
            0,
            { 0xd5, 0xaa, 0xad }, { 0xde, 0xaa, 0xeb },
            0x1f,
            true,
            0,
            DiskImg::kNibbleEnc53,
            DiskImg::kNibbleSpecialNone,
        };

        img.SetCustomNibbleDescr(&zork13Descr);
    }
#endif


    if (img.AnalyzeImage() != kDIErrNone) {
        errMsg.Format(L"The file '%ls' doesn't seem to hold a valid disk image.",
            (LPCWSTR) loadName);
        MessageBox(errMsg, failed, MB_OK|MB_ICONSTOP);
        goto bail;
    }

    if (img.ShowAsBlocks())
        displayFormat = ImageFormatDialog::kShowAsBlocks;
    else
        displayFormat = ImageFormatDialog::kShowAsSectors;

    /* if they can't do anything but view nibbles, don't demand an fs format */
    bool allowUnknown;
    allowUnknown = false;
    if (!img.GetHasSectors() && !img.GetHasBlocks() && img.GetHasNibbles())
        allowUnknown = true;

    /*
     * If requested (or necessary), verify the format.
     */
    if (img.GetFSFormat() == DiskImg::kFormatUnknown ||
        img.GetSectorOrder() == DiskImg::kSectorOrderUnknown ||
        fPreferences.GetPrefBool(kPrQueryImageFormat))
    {
        if (TryDiskImgOverride(&img, loadName, DiskImg::kFormatUnknown,
            &displayFormat, allowUnknown, &errMsg) != IDOK)
        {
            goto bail;
        }
        if (!errMsg.IsEmpty()) {
            ShowFailureMsg(this, errMsg, IDS_FAILED);
            goto bail;
        }
    }


    /* select edit dialog type, based on blocks vs. sectors */
    if (displayFormat == ImageFormatDialog::kShowAsSectors)
        pEditDialog = &sectorEdit;
    else if (displayFormat == ImageFormatDialog::kShowAsBlocks)
        pEditDialog = &blockEdit;
    else
        pEditDialog = &nibbleEdit;

    /*
     * Create an appropriate DiskFS object and hand it to the edit dialog.
     */
    DiskFS* pDiskFS;
    pDiskFS = img.OpenAppropriateDiskFS(true);
    if (pDiskFS == NULL) {
        LOGI("HEY: OpenAppropriateDiskFS failed!");
        goto bail;
    }

    pDiskFS->SetScanForSubVolumes(DiskFS::kScanSubEnabled);

    {
        CWaitCursor wait;       // big ProDOS volumes can be slow
        dierr = pDiskFS->Initialize(&img, DiskFS::kInitFull);
    }
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Warning: error during disk scan: %hs.",
            DiskImgLib::DIStrError(dierr));
        MessageBox(errMsg, failed, MB_OK | MB_ICONEXCLAMATION);
        /* keep going */
    }
    pEditDialog->Setup(pDiskFS, loadName);
    (void) pEditDialog->DoModal();

    delete pDiskFS;

    /*
     * FUTURE: if we edited the file we have open in the contentlist,
     * we need to post a warning and/or close it in the contentlist.
     * Or maybe just re-open it?  Allow that as an option.
     */

bail:
#if 0
    delete phatbuf;
#endif
    return;
}


/*
 * ==========================================================================
 *      Disk Converter
 * ==========================================================================
 */

void MainWindow::OnToolsDiskConv(void)
{
    DIError dierr;
    CString openFilters, errMsg;
    CString loadName, saveName, saveFolder;
    DiskImg srcImg, dstImg;
    DiskConvertDialog convDlg(this);
    CString storageName;
    CStringA saveNameA, storageNameA, loadNameA;

    /* flush current archive in case that's what we're planning to convert */
    OnFileSave();

    dstImg.SetNuFXCompressionType(fPreferences.GetPrefLong(kPrCompressionType));

    /*
     * Select the image to convert.
     */
    openFilters = kOpenDiskImage;
    openFilters += kOpenAll;
    openFilters += kOpenEnd;
    CFileDialog dlg(TRUE, L"dsk", NULL, OFN_FILEMUSTEXIST, openFilters, this);

    /* for now, everything is read-only */
    dlg.m_ofn.Flags |= OFN_HIDEREADONLY;
    dlg.m_ofn.lpstrTitle = L"Select image to convert";
    dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

    if (dlg.DoModal() != IDOK)
        goto bail;
    loadName = dlg.GetPathName();

    saveFolder = dlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    /* open the image file and analyze it */
    loadNameA = loadName;
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

    /*
     * If confirm image format is set, or we can't figure out the sector
     * ordering, prompt the user.
     */
    if (srcImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown ||
        fPreferences.GetPrefBool(kPrQueryImageFormat))
    {
        if (TryDiskImgOverride(&srcImg, loadName, DiskImg::kFormatGenericProDOSOrd,
            NULL, false, &errMsg) != IDOK)
        {
            goto bail;
        }
        if (!errMsg.IsEmpty()) {
            ShowFailureMsg(this, errMsg, IDS_FAILED);
            goto bail;
        }
    }

    /*
     * If this is a ProDOS volume, use the disk volume name as the default
     * value for "storageName" (which is used for NuFX archives and DC42).
     */
    if (srcImg.GetFSFormat() == DiskImg::kFormatProDOS) {
        CWaitCursor waitc;
        DiskFS* pDiskFS = srcImg.OpenAppropriateDiskFS();
        // use "headerOnly", which gets the volume name
        dierr = pDiskFS->Initialize(&srcImg, DiskFS::kInitHeaderOnly);
        if (dierr == kDIErrNone) {
            storageName = pDiskFS->GetVolumeName();     // note: ASCII only
        }
        delete pDiskFS;
    } else {
        /* use filename as storageName (exception for DiskCopy42 later) */
        storageName = PathName::FilenameOnly(loadName, '\\');
    }
    LOGI("  Using '%ls' as storageName", (LPCWSTR) storageName);

    /* transfer the DOS volume num, if one was set */
    dstImg.SetDOSVolumeNum(srcImg.GetDOSVolumeNum());
    LOGI("DOS volume number set to %d", dstImg.GetDOSVolumeNum());

    DiskImg::FSFormat origFSFormat;
    origFSFormat = srcImg.GetFSFormat();

    /*
     * The converter always tries to read and write images as if they were
     * ProDOS blocks.  This way the only sector ordering changes are caused by
     * differences in the sector ordering, rather than differences in the
     * assumed filesystem types (which may not be knowable).
     */
    dierr = srcImg.OverrideFormat(srcImg.GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, srcImg.GetSectorOrder());
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Internal error: couldn't switch to generic ProDOS: %hs.",
                DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /*
     * Put up a dialog to figure out what we want to do with this image.
     *
     * We have a fair amount of faith that this will not pick impossible
     * combinations.  If we do, we will fail later on in CreateImage.
     */
    convDlg.Init(&srcImg);
    if (convDlg.DoModal() != IDOK) {
        LOGI(" User bailed out of convert dialog");
        goto bail;
    }

    /*
     * Examine their choices.
     */
    DiskImg::OuterFormat outerFormat;
    DiskImg::FileFormat fileFormat;
    DiskImg::PhysicalFormat physicalFormat;
    DiskImg::SectorOrder sectorOrder;

    if (DetermineImageSettings(convDlg.fConvertIdx, (convDlg.fAddGzip != 0),
        &outerFormat, &fileFormat, &physicalFormat, &sectorOrder) != 0)
    {
        goto bail;
    }

    const DiskImg::NibbleDescr* pNibbleDescr;
    pNibbleDescr = srcImg.GetNibbleDescr();
    if (pNibbleDescr == NULL && DiskImg::IsNibbleFormat(physicalFormat)) {
        /*
         * We're writing to a nibble format, so we have to decide how the
         * disk should be formatted.  The source doesn't specify it, so we
         * use generic 13- or 16-sector, defaulting to the latter when in
         * doubt.
         */
        if (srcImg.GetHasSectors() && srcImg.GetNumSectPerTrack() == 13) {
            pNibbleDescr = DiskImg::GetStdNibbleDescr(
                                DiskImg::kNibbleDescrDOS32Std);
        } else {
            pNibbleDescr = DiskImg::GetStdNibbleDescr(
                                DiskImg::kNibbleDescrDOS33Std);
        }
    }
    LOGI(" NibbleDescr is 0x%08lx (%hs)", (long) pNibbleDescr,
        pNibbleDescr != NULL ? pNibbleDescr->description : "---");

    if (srcImg.GetFileFormat() == DiskImg::kFileFormatTrackStar &&
        fileFormat != DiskImg::kFileFormatTrackStar)
    {
        /* converting from TrackStar to anything else */
        CString msg, appName;
        CheckedLoadString(&msg, IDS_TRACKSTAR_TO_OTHER_WARNING);
        CheckedLoadString(&appName, IDS_MB_APP_NAME);
        if (MessageBox(msg, appName, MB_OKCANCEL | MB_ICONWARNING) != IDOK) {
            LOGI(" User bailed after trackstar-to-other warning");
            goto bail;
        }
    } else if (srcImg.GetFileFormat() == DiskImg::kFileFormatFDI &&
               fileFormat != DiskImg::kFileFormatTrackStar &&
               srcImg.GetNumBlocks() != 1600)
    {
        /* converting from 5.25" FDI to anything but TrackStar */
        CString msg, appName;
        CheckedLoadString(&msg, IDS_FDI_TO_OTHER_WARNING);
        CheckedLoadString(&appName, IDS_MB_APP_NAME);
        if (MessageBox(msg, appName, MB_OKCANCEL | MB_ICONWARNING) != IDOK) {
            LOGI(" User bailed after fdi-to-other warning");
            goto bail;
        }
    } else if (srcImg.GetHasNibbles() && DiskImg::IsSectorFormat(physicalFormat))
    {
        /* converting from nibble to non-nibble format */
        CString msg, appName;
        CheckedLoadString(&msg, IDS_NIBBLE_TO_SECTOR_WARNING);
        CheckedLoadString(&appName, IDS_MB_APP_NAME);
        if (MessageBox(msg, appName, MB_OKCANCEL | MB_ICONWARNING) != IDOK) {
            LOGI(" User bailed after nibble-to-sector warning");
            goto bail;
        }
    } else if (srcImg.GetHasNibbles() &&
        DiskImg::IsNibbleFormat(physicalFormat) &&
        srcImg.GetPhysicalFormat() != physicalFormat)
    {
        /* converting between differing nibble formats */
        CString msg, appName;
        CheckedLoadString(&msg, IDS_DIFFERENT_NIBBLE_WARNING);
        CheckedLoadString(&appName, IDS_MB_APP_NAME);
        if (MessageBox(msg, appName, MB_OKCANCEL | MB_ICONWARNING) != IDOK) {
            LOGI(" User bailed after differing-nibbles warning");
            goto bail;
        }
    }

    /*
     * If the source is a UNIDOS volume and the target format is DiskCopy 4.2,
     * use DOS sector ordering instead of ProDOS block ordering.  For some
     * reason the disks come out that way.
     */
    if (origFSFormat == DiskImg::kFormatUNIDOS &&
        fileFormat == DiskImg::kFileFormatDiskCopy42)
    {
        LOGI("  Switching to DOS sector ordering for UNIDOS/DiskCopy42");
        sectorOrder = DiskImg::kSectorOrderDOS;
    }
    if (origFSFormat != DiskImg::kFormatProDOS &&
        fileFormat == DiskImg::kFileFormatDiskCopy42)
    {
        LOGI("  Nuking storage name for non-ProDOS DiskCopy42 image");
        storageName = L"";  // want to use "-not a mac disk" for non-ProDOS
    }

    /*
     * Pick file to save into.
     */
    {
        CFileDialog saveDlg(FALSE, convDlg.fExtension, NULL,
            OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN|OFN_HIDEREADONLY,
            L"All Files (*.*)|*.*||", this);

        CString saveFolder;
        CString title = L"New disk image (.";
        title += convDlg.fExtension;
        title += L")";

        saveDlg.m_ofn.lpstrTitle = title;
        saveDlg.m_ofn.lpstrInitialDir =
            fPreferences.GetPrefString(kPrConvertArchiveFolder);
    
        if (saveDlg.DoModal() != IDOK) {
            LOGI(" User bailed out of image save dialog");
            goto bail;
        }

        saveFolder = saveDlg.m_ofn.lpstrFile;
        saveFolder = saveFolder.Left(saveDlg.m_ofn.nFileOffset);
        fPreferences.SetPrefString(kPrConvertArchiveFolder, saveFolder);

        saveName = saveDlg.GetPathName();
    }
    LOGI("File will be saved to '%ls'", (LPCWSTR) saveName);

    /* DiskImgLib does not like it if file already exists */
    errMsg = RemoveFile(saveName);
    if (!errMsg.IsEmpty()) {
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /*
     * Create the image file.  Adjust the number of tracks if we're
     * copying to or from TrackStar or FDI images.
     */
    int dstNumTracks;
    int dstNumBlocks;
    bool isPartial;
    dstNumTracks = srcImg.GetNumTracks();
    dstNumBlocks = srcImg.GetNumBlocks();
    isPartial = false;

    if (srcImg.GetFileFormat() == DiskImg::kFileFormatTrackStar &&
        fileFormat != DiskImg::kFileFormatTrackStar &&
        srcImg.GetNumTracks() == 40)
    {
        /* from TrackStar to other */
        dstNumTracks = 35;
        dstNumBlocks = 280;
        isPartial = true;
    }
    if (srcImg.GetFileFormat() == DiskImg::kFileFormatFDI &&
        fileFormat != DiskImg::kFileFormatFDI &&
        srcImg.GetNumTracks() != 35 && srcImg.GetNumBlocks() != 1600)
    {
        /* from 5.25" FDI to other */
        dstNumTracks = 35;
        dstNumBlocks = 280;
        isPartial = true;
    }

    if (srcImg.GetFileFormat() != DiskImg::kFileFormatTrackStar &&
        fileFormat == DiskImg::kFileFormatTrackStar &&
        dstNumTracks == 35)
    {
        /* from other to TrackStar */
        isPartial = true;
    }

    saveNameA = saveName;
    storageNameA = storageName;

    if (srcImg.GetHasNibbles() &&
        DiskImg::IsNibbleFormat(physicalFormat) &&
        physicalFormat == srcImg.GetPhysicalFormat())
    {
        /*
         * For nibble-to-nibble with the same track format, copy it as
         * a collection of tracks.
         */
        dierr = dstImg.CreateImage((LPCSTR) saveNameA,
                    (LPCSTR) storageNameA,
                    outerFormat,
                    fileFormat,
                    physicalFormat,
                    pNibbleDescr,
                    sectorOrder,
                    DiskImg::kFormatGenericProDOSOrd,
                    dstNumTracks, srcImg.GetNumSectPerTrack(),
                    false   /* must format */);
    } else if (srcImg.GetHasBlocks()) {
        /*
         * For general case, copy as a block image, converting in and out of
         * nibbles as needed.
         */
        dierr = dstImg.CreateImage((LPCSTR) saveNameA,
                    (LPCSTR) storageNameA,
                    outerFormat,
                    fileFormat,
                    physicalFormat,
                    pNibbleDescr,
                    sectorOrder,
                    DiskImg::kFormatGenericProDOSOrd,
                    dstNumBlocks,
                    false   /* only needed for nibble?? */);
    } else if (srcImg.GetHasSectors()) {
        /*
         * We should only get here when converting to/from D13.  We have to
         * special-case this because this was originally written to support
         * block copying as the lowest common denominator.  D13 screwed
         * everything up. :-)
         */
        dierr = dstImg.CreateImage((LPCSTR) saveNameA,
                    (LPCSTR) storageNameA,
                    outerFormat,
                    fileFormat,
                    physicalFormat,
                    pNibbleDescr,
                    sectorOrder,
                    DiskImg::kFormatGenericProDOSOrd,   // needs to match above
                    dstNumTracks, srcImg.GetNumSectPerTrack(),
                    false   /* only need for dest=nibble? */);
    } else {
        /*
         * Generally speaking, we don't allow the user to make choices that
         * would get us here.  In particular, the UI should not allow the
         * user to convert directly between nibble formats when the source
         * image doesn't have a recognizeable block format.
         */
        ASSERT(false);
        dierr = kDIErrInternal;
    }
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Couldn't create disk image: %hs.",
                DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /*
     * Do the actual copy, either as blocks or tracks.
     */
    dierr = CopyDiskImage(&dstImg, &srcImg, false, isPartial, NULL);
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Copy failed: %hs.", DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    dierr = srcImg.CloseImage();
    if (dierr != kDIErrNone) {
        errMsg.Format(L"ERROR: srcImg close failed (err=%d)\n", dierr);
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    dierr = dstImg.CloseImage();
    if (dierr != kDIErrNone) {
        errMsg.Format(L"ERROR: dstImg close failed (err=%d)\n", dierr);
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    SuccessBeep();

    /*
     * We're done.  Give them the opportunity to open the disk image they
     * just created.
     */
    {
        DoneOpenDialog doneOpen(this);

        if (doneOpen.DoModal() == IDOK) {
            LOGI(" At user request, opening '%ls'", (LPCWSTR) saveName);

            DoOpenArchive(saveName, convDlg.fExtension,
                kFilterIndexDiskImage, false);
        }
    }

bail:
    return;
}

int MainWindow::DetermineImageSettings(int convertIdx, bool addGzip,
    DiskImg::OuterFormat* pOuterFormat, DiskImg::FileFormat* pFileFormat,
    DiskImg::PhysicalFormat* pPhysicalFormat,
    DiskImg::SectorOrder* pSectorOrder)
{
    if (addGzip)
        *pOuterFormat = DiskImg::kOuterFormatGzip;
    else
        *pOuterFormat = DiskImg::kOuterFormatNone;

    switch (convertIdx) {
    case DiskConvertDialog::kConvDOSRaw:
        *pFileFormat = DiskImg::kFileFormatUnadorned;
        *pPhysicalFormat = DiskImg::kPhysicalFormatSectors;
        *pSectorOrder = DiskImg::kSectorOrderDOS;
        break;
    case DiskConvertDialog::kConvDOS2MG:
        *pFileFormat = DiskImg::kFileFormat2MG;
        *pPhysicalFormat = DiskImg::kPhysicalFormatSectors;
        *pSectorOrder = DiskImg::kSectorOrderDOS;
        break;
    case DiskConvertDialog::kConvProDOSRaw:
        *pFileFormat = DiskImg::kFileFormatUnadorned;
        *pPhysicalFormat = DiskImg::kPhysicalFormatSectors;
        *pSectorOrder = DiskImg::kSectorOrderProDOS;
        break;
    case DiskConvertDialog::kConvProDOS2MG:
        *pFileFormat = DiskImg::kFileFormat2MG;
        *pPhysicalFormat = DiskImg::kPhysicalFormatSectors;
        *pSectorOrder = DiskImg::kSectorOrderProDOS;
        break;
    case DiskConvertDialog::kConvNibbleRaw:
        *pFileFormat = DiskImg::kFileFormatUnadorned;
        *pPhysicalFormat = DiskImg::kPhysicalFormatNib525_6656;
        *pSectorOrder = DiskImg::kSectorOrderPhysical;
        break;
    case DiskConvertDialog::kConvNibble2MG:
        *pFileFormat = DiskImg::kFileFormat2MG;
        *pPhysicalFormat = DiskImg::kPhysicalFormatNib525_6656;
        *pSectorOrder = DiskImg::kSectorOrderPhysical;
        break;
    case DiskConvertDialog::kConvD13:
        *pFileFormat = DiskImg::kFileFormatUnadorned;
        *pPhysicalFormat = DiskImg::kPhysicalFormatSectors;
        *pSectorOrder = DiskImg::kSectorOrderDOS;
        break;
    case DiskConvertDialog::kConvDiskCopy42:
        *pFileFormat = DiskImg::kFileFormatDiskCopy42;
        *pPhysicalFormat = DiskImg::kPhysicalFormatSectors;
        *pSectorOrder = DiskImg::kSectorOrderProDOS;
        break;
    case DiskConvertDialog::kConvTrackStar:
        *pFileFormat = DiskImg::kFileFormatTrackStar;
        *pPhysicalFormat = DiskImg::kPhysicalFormatNib525_Var;
        *pSectorOrder = DiskImg::kSectorOrderPhysical;
        break;
    case DiskConvertDialog::kConvNuFX:
        *pFileFormat = DiskImg::kFileFormatNuFX;
        *pPhysicalFormat = DiskImg::kPhysicalFormatSectors;
        *pSectorOrder = DiskImg::kSectorOrderProDOS;
        break;
    case DiskConvertDialog::kConvSim2eHDV:
        *pFileFormat = DiskImg::kFileFormatSim2eHDV;
        *pPhysicalFormat = DiskImg::kPhysicalFormatSectors;
        *pSectorOrder = DiskImg::kSectorOrderProDOS;
        break;
    case DiskConvertDialog::kConvDDD:
        *pFileFormat = DiskImg::kFileFormatDDD;
        *pPhysicalFormat = DiskImg::kPhysicalFormatSectors;
        *pSectorOrder = DiskImg::kSectorOrderDOS;
        break;
    default:
        ASSERT(false);
        LOGI(" WHOA: invalid conv type %d", convertIdx);
        return -1;
    }

    return 0;
}

static inline int MIN(int val1, int val2)
{
    return (val1 < val2) ? val1 : val2;
}

DIError MainWindow::CopyDiskImage(DiskImg* pDstImg, DiskImg* pSrcImg, bool bulk,
    bool partial, ProgressCancelDialog* pPCDialog)
{
    /*
     * This originally just did a block copy.  Nibble track copies were added
     * later, and sector copies were added even later.
     */

    DIError dierr = kDIErrNone;
    CString errMsg;
    unsigned char* dataBuf = NULL;

    if (pSrcImg->GetHasNibbles() && pDstImg->GetHasNibbles() &&
        pSrcImg->GetPhysicalFormat() == pDstImg->GetPhysicalFormat())
    {
        /*
         * Copy as a series of nibble tracks.
         *
         * NOTE: we could do better here for 6384 to ".app", but in
         * practice nobody cares anyway.
         */
        if (!partial) {
            ASSERT(pSrcImg->GetNumTracks() == pDstImg->GetNumTracks());
        }

        //unsigned char trackBuf[kTrackAllocSize];
        long trackLen;
        int numTracks;

        dataBuf = new unsigned char[kTrackAllocSize];
        if (dataBuf == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        }
        
        numTracks = MIN(pSrcImg->GetNumTracks(), pDstImg->GetNumTracks());
        LOGI("Nibble track copy (%d tracks)", numTracks);
        for (int track = 0; track < numTracks; track++) {
            dierr = pSrcImg->ReadNibbleTrack(track, dataBuf, &trackLen);
            if (dierr != kDIErrNone) {
                errMsg.Format(L"ERROR: read on track %d failed (err=%d)\n",
                    track, dierr);
                ShowFailureMsg(this, errMsg, IDS_FAILED);
                goto bail;
            }
            dierr = pDstImg->WriteNibbleTrack(track, dataBuf, trackLen);
            if (dierr != kDIErrNone) {
                errMsg.Format(L"ERROR: write on track %d failed (err=%d)\n",
                    track, dierr);
                ShowFailureMsg(this, errMsg, IDS_FAILED);
                goto bail;
            }

            /* these aren't slow enough that we need progress updating */
        }
    } else if (!pSrcImg->GetHasBlocks() || !pDstImg->GetHasBlocks()) {
        /*
         * Do a sector copy, for D13 images (which can't be accessed as blocks).
         */
        if (!partial) {
            ASSERT(pSrcImg->GetNumTracks() == pDstImg->GetNumTracks());
            ASSERT(pSrcImg->GetNumSectPerTrack() == pDstImg->GetNumSectPerTrack());
        }

        long numTracks, numSectPerTrack;
        int numBadSectors = 0;

        dataBuf = new unsigned char[256];   // one sector
        if (dataBuf == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        }
        
        numTracks = MIN(pSrcImg->GetNumTracks(), pDstImg->GetNumTracks());
        numSectPerTrack = MIN(pSrcImg->GetNumSectPerTrack(),
                              pDstImg->GetNumSectPerTrack());
        LOGI("Sector copy (%d tracks / %d sectors)",
            numTracks, numSectPerTrack);
        for (int track = 0; track < numTracks; track++) {
            for (int sector = 0; sector < numSectPerTrack; sector++) {
                dierr = pSrcImg->ReadTrackSector(track, sector, dataBuf);
                if (dierr != kDIErrNone) {
                    LOGI("Bad sector T=%d S=%d", track, sector);
                    numBadSectors++;
                    dierr = kDIErrNone;
                    memset(dataBuf, 0, 256);
                }
                dierr = pDstImg->WriteTrackSector(track, sector, dataBuf);
                if (dierr != kDIErrNone) {
                    errMsg.Format(L"ERROR: write of T=%d S=%d failed (err=%d)\n",
                        track, sector, dierr);
                    ShowFailureMsg(this, errMsg, IDS_FAILED);
                    goto bail;
                }
            }

            /* these aren't slow enough that we need progress updating */
        }

        if (!bulk && numBadSectors != 0) {
            CString appName;
            CheckedLoadString(&appName, IDS_MB_APP_NAME);
            errMsg.Format(L"Skipped %ld unreadable sector%ls.", numBadSectors,
                numBadSectors == 1 ? L"" : L"s");
            MessageBox(errMsg, appName, MB_OK | MB_ICONWARNING);
        }
    } else {
        /*
         * Do a block copy, copying multiple blocks at a time for performance.
         */
        if (!partial) {
            ASSERT(pSrcImg->GetNumBlocks() == pDstImg->GetNumBlocks());
        }

        //unsigned char blkBuf[512];
        long numBadBlocks = 0;
        long numBlocks;
        int blocksPerRead;

        numBlocks = MIN(pSrcImg->GetNumBlocks(), pDstImg->GetNumBlocks());
        if (numBlocks <= 2880)
            blocksPerRead = 9;      // better granularity (one floppy track)
        else
            blocksPerRead = 64;     // 32K per read; max seems to be 64K?

        dataBuf = new unsigned char[blocksPerRead * 512];
        if (dataBuf == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        }

        LOGD("--- BLOCK COPY (%ld blocks, %d per)",
            numBlocks, blocksPerRead);
        for (long block = 0; block < numBlocks; ) {
            long blocksThisTime = blocksPerRead;
            if (block + blocksThisTime > numBlocks)
                blocksThisTime = numBlocks - block;

            dierr = pSrcImg->ReadBlocks(block, blocksThisTime, dataBuf);
            if (dierr != kDIErrNone) {
                if (blocksThisTime != 1) {
                    /*
                     * Media with errors.  Drop to one block per read.
                     */
                    LOGW(" Bad sector encountered at %ld(%ld), slowing",
                        block, blocksThisTime);
                    blocksThisTime = blocksPerRead = 1;
                    continue;   // retry this block
                }
                numBadBlocks++;
                dierr = kDIErrNone;
                memset(dataBuf, 0, 512);
            }
            dierr = pDstImg->WriteBlocks(block, blocksThisTime, dataBuf);
            if (dierr != kDIErrNone) {
                if (dierr != kDIErrWriteProtected) {
                    errMsg.Format(L"ERROR: write of block %ld failed: %hs\n",
                        block, DiskImgLib::DIStrError(dierr));
                    ShowFailureMsg(this, errMsg, IDS_FAILED);
                }
                goto bail;
            }

            /* if we have a cancel dialog, keep it lively */
            if (pPCDialog != NULL && (block % 18) == 0) {
                int status;
                PeekAndPump();
                LONGLONG bigBlock = block;
                bigBlock = bigBlock * ProgressCancelDialog::kProgressResolution;
                status = pPCDialog->SetProgress((int)(bigBlock / numBlocks));
                if (status == IDCANCEL) {
                    dierr = kDIErrCancelled;    // pretend it came from DiskImg
                    goto bail;
                }
            } else if (bulk && (block % 512) == 0) {
                PeekAndPump();
            }

            block += blocksThisTime;
        }

        if (!bulk && numBadBlocks != 0) {
            CString appName;
            CheckedLoadString(&appName, IDS_MB_APP_NAME);
            errMsg.Format(L"Skipped %ld unreadable block%ls.", numBadBlocks,
                numBadBlocks == 1 ? L"" : L"s");
            MessageBox(errMsg, appName, MB_OK | MB_ICONWARNING);
        }
    }

bail:
    delete[] dataBuf;
    return dierr;
}


/*
 * ==========================================================================
 *      Bulk disk convert
 * ==========================================================================
 */

/*
 * Sub-class the generic libutil CancelDialog class.
 */
class BulkConvCancelDialog : public CancelDialog {
public:
    BOOL Create(CWnd* pParentWnd = NULL) {
        fAbortOperation = false;
        return CancelDialog::Create(&fAbortOperation,
                IDD_BULKCONV, pParentWnd);
    }

    void SetCurrentFile(const WCHAR* fileName) {
        CWnd* pWnd = GetDlgItem(IDC_BULKCONV_PATHNAME);
        ASSERT(pWnd != NULL);
        pWnd->SetWindowText(fileName);
    }

    bool fAbortOperation;

private:
    void OnOK(void) {
        LOGD("Ignoring BulkConvCancelDialog OnOK");
    }

    MainWindow* GetMainWindow(void) const {
        return (MainWindow*)::AfxGetMainWnd();
    }
};

void MainWindow::OnToolsBulkDiskConv(void)
{
    const int kFileNameBufSize = 32768;
    DiskConvertDialog convDlg(this);
    ChooseDirDialog chooseDirDlg(this);
    BulkConvCancelDialog* pCancelDialog = new BulkConvCancelDialog; // on heap
    CString openFilters, errMsg;
    CString saveFolder, targetDir;
    int nameCount;

    /* flush current archive in case that's what we're planning to convert */
    OnFileSave();

    /*
     * Select the set of images to convert.
     */
    openFilters = kOpenDiskImage;
    openFilters += kOpenAll;
    openFilters += kOpenEnd;
    CFileDialog dlg(TRUE, L"dsk", NULL, OFN_FILEMUSTEXIST, openFilters, this);

    dlg.m_ofn.lpstrFile = new WCHAR[kFileNameBufSize];
    dlg.m_ofn.lpstrFile[0] = dlg.m_ofn.lpstrFile[1] = '\0';
    dlg.m_ofn.nMaxFile = kFileNameBufSize;
    dlg.m_ofn.Flags |= OFN_HIDEREADONLY;    // open all images as read-only
    dlg.m_ofn.Flags |= OFN_ALLOWMULTISELECT;
    dlg.m_ofn.lpstrTitle = L"Select images to convert";
    dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

    if (dlg.DoModal() != IDOK)
        goto bail;

    saveFolder = dlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    /* count up the number of entries */
    POSITION posn;
    posn = dlg.GetStartPosition();
    nameCount = 0;
    while (posn != NULL) {
        CString pathName;
        pathName = dlg.GetNextPathName(posn);
        nameCount++;
    }
    LOGD("BulkConv got nameCount=%d", nameCount);

    /*
     * Choose the target directory.
     *
     * We use the "convert archive" folder by default.
     */
    chooseDirDlg.SetPathName(fPreferences.GetPrefString(kPrConvertArchiveFolder));
    if (chooseDirDlg.DoModal() != IDOK)
        goto bail;

    targetDir = chooseDirDlg.GetPathName();
    fPreferences.SetPrefString(kPrConvertArchiveFolder, targetDir);

    /*
     * Put up a dialog to select the target conversion format.
     *
     * It is up to the user to select a format that matches the selected
     * files.  If it doesn't (e.g. converting an 800K floppy to DDD format),
     * the process will fail later on.
     */
    convDlg.Init(nameCount);
    if (convDlg.DoModal() != IDOK) {
        LOGI(" User bailed out of convert dialog");
        goto bail;
    }

    /* initialize cancel dialog, and disable main window */
    EnableWindow(FALSE);
    if (pCancelDialog->Create(this) == FALSE) {
        LOGE("Cancel dialog init failed?!");
        ASSERT(false);
        goto bail;
    }

    /*
     * Loop through all selected files and convert them one at a time.
     */
    posn = dlg.GetStartPosition();
    while (posn != NULL) {
        CString pathName;
        pathName = dlg.GetNextPathName(posn);
        LOGI(" BulkConv: source path='%ls'", (LPCWSTR) pathName);

        pCancelDialog->SetCurrentFile(PathName::FilenameOnly(pathName, '\\'));
        PeekAndPump();
        if (pCancelDialog->fAbortOperation)
            break;
        BulkConvertImage(pathName, targetDir, convDlg, &errMsg);

        if (!errMsg.IsEmpty()) {
            /* show error message, do OK/Cancel */
            /* do we need to delete the output file on failure?  In general
               we can't, because we could have failed because the file
               already existed. */
            CString failed;
            int res;

            CheckedLoadString(&failed, IDS_FAILED);
            errMsg += "\n\nSource file: ";
            errMsg += pathName;
            errMsg += "\n\nClick OK to skip this and continue, or Cancel to "
                      "stop now.";
            res = pCancelDialog->MessageBox(errMsg,
                        failed, MB_OKCANCEL | MB_ICONERROR);
            if (res != IDOK)
                goto bail;
        }
    }

    if (!pCancelDialog->fAbortOperation)
        SuccessBeep();

bail:
    // restore the main window to prominence
    EnableWindow(TRUE);
    //SetActiveWindow();
    if (pCancelDialog != NULL)
        pCancelDialog->DestroyWindow();

    delete[] dlg.m_ofn.lpstrFile;
    return;
}

void MainWindow::BulkConvertImage(const WCHAR* pathName, const WCHAR* targetDir,
    const DiskConvertDialog& convDlg, CString* pErrMsg)
{
    /*
     * [Much of this is copy & pasted from OnToolsDiskConv().  This needs to get
     * refactored.]
     */

    DIError dierr;
    CString saveName;
    DiskImg srcImg, dstImg;
    CString storageName;
    PathName srcPath(pathName);
    CString fileName, ext;
    CStringA saveNameA, storageNameA;

    *pErrMsg = L"";

    dstImg.SetNuFXCompressionType(
                            fPreferences.GetPrefLong(kPrCompressionType));

    /* open the image file and analyze it */
    CStringA pathNameA(pathName);
    dierr = srcImg.OpenImage(pathNameA, PathProposal::kLocalFssep, true);
    if (dierr != kDIErrNone) {
        pErrMsg->Format(L"Unable to open disk image: %hs.",
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    if (srcImg.AnalyzeImage() != kDIErrNone) {
        pErrMsg->Format(L"The file doesn't seem to hold a valid disk image.");
        goto bail;
    }

#if 0       // don't feel like posting this UI
    /*
     * If we can't figure out the sector ordering, prompt the user.  Don't
     * go into it if they have "confirm format" selected, since that would be
     * annoying.  If they need to confirm it, they can use the one-at-a-time
     * interface.
     */
    if (srcImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown) {
        if (TryDiskImgOverride(&srcImg, pathName, DiskImg::kFormatGenericProDOSOrd,
            NULL, pErrMsg) != IDOK)
        {
            *pErrMsg = "Image conversion cancelled.";
        }
        if (!pErrMsg->IsEmpty())
            goto bail;
    }
#else
    if (srcImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown) {
        *pErrMsg = L"Could not determine the disk image sector ordering.  You "
                   L"may need to change the file extension.";
        goto bail;
    }
#endif

    /* transfer the DOS volume num, if one was set */
    dstImg.SetDOSVolumeNum(srcImg.GetDOSVolumeNum());
    LOGI("DOS volume number set to %d", dstImg.GetDOSVolumeNum());

    DiskImg::FSFormat origFSFormat;
    origFSFormat = srcImg.GetFSFormat();

    /*
     * The converter always tries to read and write images as if they were
     * ProDOS blocks.  This way the only sector ordering changes are caused by
     * differences in the sector ordering, rather than differences in the
     * assumed filesystem types (which may not be knowable).
     */
    dierr = srcImg.OverrideFormat(srcImg.GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, srcImg.GetSectorOrder());
    if (dierr != kDIErrNone) {
        pErrMsg->Format(L"Internal error: couldn't switch to generic ProDOS: %hs.",
                DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    /*
     * Examine their choices.
     */
    DiskImg::OuterFormat outerFormat;
    DiskImg::FileFormat fileFormat;
    DiskImg::PhysicalFormat physicalFormat;
    DiskImg::SectorOrder sectorOrder;

    if (DetermineImageSettings(convDlg.fConvertIdx, (convDlg.fAddGzip != 0),
        &outerFormat, &fileFormat, &physicalFormat, &sectorOrder) != 0)
    {
        *pErrMsg = L"Odd: couldn't configure image settings";
        goto bail;
    }

    const DiskImg::NibbleDescr* pNibbleDescr;
    pNibbleDescr = srcImg.GetNibbleDescr();
    if (pNibbleDescr == NULL && DiskImg::IsNibbleFormat(physicalFormat)) {
        /*
         * We're writing to a nibble format, so we have to decide how the
         * disk should be formatted.  The source doesn't specify it, so we
         * use generic 13- or 16-sector, defaulting to the latter when in
         * doubt.
         */
        if (srcImg.GetHasSectors() && srcImg.GetNumSectPerTrack() == 13) {
            pNibbleDescr = DiskImg::GetStdNibbleDescr(
                                DiskImg::kNibbleDescrDOS32Std);
        } else {
            pNibbleDescr = DiskImg::GetStdNibbleDescr(
                                DiskImg::kNibbleDescrDOS33Std);
        }
    }
    LOGI(" NibbleDescr is 0x%08lx (%hs)", (long) pNibbleDescr,
        pNibbleDescr != NULL ? pNibbleDescr->description : "---");

    /*
     * Create the new filename based on the old filename.
     */
    saveName = targetDir;
    if (saveName.Right(1) != '\\')
        saveName += '\\';
    fileName = srcPath.GetFileName();
    ext = srcPath.GetExtension();           // extension, including '.'
    if (ext.CompareNoCase(L".gz") == 0) {
        /* got a .gz, see if there's anything else in front of it */
        CString tmpName, ext2;
        tmpName = srcPath.GetPathName();
        tmpName = tmpName.Left(tmpName.GetLength() - ext.GetLength());
        PathName tmpPath(tmpName);
        ext2 = tmpPath.GetExtension();
        if (ext2.GetLength() >= 2 && ext2.GetLength() <= 4)
            ext = ext2 + ext;

        saveName += fileName.Left(fileName.GetLength() - ext.GetLength());
    } else {
        if (ext.GetLength() < 2 || ext.GetLength() > 4) {
            /* no meaningful extension */
            saveName += fileName;
        } else {
            saveName += fileName.Left(fileName.GetLength() - ext.GetLength());
        }
    }
    storageName = PathName::FilenameOnly(saveName, '\\'); // grab this for SHK name
    saveName += '.';
    saveName += convDlg.fExtension;
    LOGI(" Bulk converting '%ls' to '%ls'", pathName, (LPCWSTR) saveName);

    /*
     * If this is a ProDOS volume, use the disk volume name as the default
     * value for "storageName" (which is only used for NuFX archives).
     */
    if (srcImg.GetFSFormat() == DiskImg::kFormatProDOS) {
        CWaitCursor waitc;
        DiskFS* pDiskFS = srcImg.OpenAppropriateDiskFS();
        // set "headerOnly" since we only need the volume name
        dierr = pDiskFS->Initialize(&srcImg, DiskFS::kInitHeaderOnly);
        if (dierr == kDIErrNone) {
            storageName = pDiskFS->GetVolumeName();     // note: ASCII only
        }
        delete pDiskFS;
    } else {
        /* just use storageName as set earlier, unless target is DiskCopy42 */
        if (fileFormat == DiskImg::kFileFormatDiskCopy42)
            storageName = L"";  // want to use "not a mac disk" for non-ProDOS
    }
    LOGI("  Using '%ls' as storageName", (LPCWSTR) storageName);

    /*
     * If the source is a UNIDOS volume and the target format is DiskCopy 4.2,
     * use DOS sector ordering instead of ProDOS block ordering.  For some
     * reason the disks come out that way.
     */
    if (origFSFormat == DiskImg::kFormatUNIDOS &&
        fileFormat == DiskImg::kFileFormatDiskCopy42)
    {
        LOGI("  Switching to DOS sector ordering for UNIDOS/DiskCopy42");
        sectorOrder = DiskImg::kSectorOrderDOS;
    }

    /*
     * Create the image file.  Adjust the number of tracks if we're
     * copying to or from a TrackStar image.
     */
    int dstNumTracks;
    int dstNumBlocks;
    bool isPartial;
    dstNumTracks = srcImg.GetNumTracks();
    dstNumBlocks = srcImg.GetNumBlocks();
    isPartial = false;

    if (srcImg.GetFileFormat() == DiskImg::kFileFormatTrackStar &&
        fileFormat != DiskImg::kFileFormatTrackStar &&
        srcImg.GetNumTracks() == 40)
    {
        /* from TrackStar to other */
        dstNumTracks = 35;
        dstNumBlocks = 280;
        isPartial = true;
    }
    if (srcImg.GetFileFormat() == DiskImg::kFileFormatFDI &&
        fileFormat != DiskImg::kFileFormatTrackStar &&
        srcImg.GetNumTracks() != 35 && srcImg.GetNumBlocks() != 1600)
    {
        /* from 5.25" FDI to other */
        dstNumTracks = 35;
        dstNumBlocks = 280;
        isPartial = true;
    }

    if (srcImg.GetFileFormat() != DiskImg::kFileFormatTrackStar &&
        fileFormat == DiskImg::kFileFormatTrackStar &&
        dstNumTracks == 35)
    {
        /* other to TrackStar */
        isPartial = true;
    }

    saveNameA = saveName;
    storageNameA = storageName;
    if (srcImg.GetHasNibbles() &&
        DiskImg::IsNibbleFormat(physicalFormat) &&
        physicalFormat == srcImg.GetPhysicalFormat())
    {
        /* for nibble-to-nibble with the same track format, copy it
           as collection of tracks */
        dierr = dstImg.CreateImage((LPCSTR) saveNameA,
                    (LPCSTR) storageNameA,
                    outerFormat,
                    fileFormat,
                    physicalFormat,
                    pNibbleDescr,
                    sectorOrder,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumTracks(), srcImg.GetNumSectPerTrack(),
                    false   /* must format */);
    } else if (srcImg.GetHasBlocks()) {
        /* for general case, create as a block image */
        ASSERT(srcImg.GetHasBlocks());
        dierr = dstImg.CreateImage((LPCSTR) saveNameA,
                    (LPCSTR) storageNameA,
                    outerFormat,
                    fileFormat,
                    physicalFormat,
                    pNibbleDescr,
                    sectorOrder,
                    DiskImg::kFormatGenericProDOSOrd,
                    srcImg.GetNumBlocks(),
                    false   /* only need for nibble? */);
    } else if (srcImg.GetHasSectors()) {
        /*
         * We should only get here when converting to/from D13.  We have to
         * special-case this because this was originally written to support
         * block copying as the lowest common denominator.  D13 screwed
         * everything up. :-)
         */
        dierr = dstImg.CreateImage((LPCSTR) saveNameA,
                    (LPCSTR) storageNameA,
                    outerFormat,
                    fileFormat,
                    physicalFormat,
                    pNibbleDescr,
                    sectorOrder,
                    DiskImg::kFormatGenericProDOSOrd,   // needs to match above
                    dstNumTracks, srcImg.GetNumSectPerTrack(),
                    false   /* only need for dest=nibble? */);
    } else {
        /* e.g. unrecognizeable nibble to blocks */
        *pErrMsg = L"Could not convert to requested format.";
        goto bail;
    }
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrInvalidCreateReq)
            *pErrMsg = L"Could not convert to requested format.";
        else
            pErrMsg->Format(L"Couldn't construct disk image: %hs.",
                    DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    /*
     * Do the actual copy, either as blocks or tracks.
     */
    dierr = CopyDiskImage(&dstImg, &srcImg, true, isPartial, NULL);
    if (dierr != kDIErrNone)
        goto bail;

    dierr = dstImg.CloseImage();
    if (dierr != kDIErrNone) {
        pErrMsg->Format(L"ERROR: dstImg close failed (err=%d)\n", dierr);
        goto bail;
    }

    dierr = srcImg.CloseImage();
    if (dierr != kDIErrNone) {
        pErrMsg->Format(L"ERROR: srcImg close failed (err=%d)\n", dierr);
        goto bail;
    }

bail:
    return;
}


/*
 * ==========================================================================
 *      SST Merge
 * ==========================================================================
 */

const int kSSTNumTracks = 35;
const int kSSTNumSectPerTrack = 16;
const int kSSTTrackLen = 6656;

void MainWindow::OnToolsSSTMerge(void)
{
    const int kBadCountThreshold = 3072;
    DiskImg srcImg0, srcImg1;
    CString appName, saveName, saveFolder, errMsg;
    uint8_t* trackBuf = NULL;
    long badCount;

    // no need to flush -- can't really open raw SST images

    CFileDialog saveDlg(FALSE, L"nib", NULL,
        OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN|OFN_HIDEREADONLY,
        L"All Files (*.*)|*.*||", this);

    CheckedLoadString(&appName, IDS_MB_APP_NAME);

    trackBuf = new uint8_t[kSSTNumTracks * kSSTTrackLen];
    if (trackBuf == NULL)
        goto bail;

    /*
     * Open the two images and verify that they are what they seem.
     */
    badCount = 0;
    if (SSTOpenImage(0, &srcImg0) != 0)
        goto bail;
    if (SSTLoadData(0, &srcImg0, trackBuf, &badCount) != 0)
        goto bail;
    LOGI("FOUND %ld bad bytes in part 0", badCount);
    if (badCount > kBadCountThreshold) {
        CheckedLoadString(&errMsg, IDS_BAD_SST_IMAGE);
        if (MessageBox(errMsg, appName, MB_OKCANCEL | MB_ICONWARNING) != IDOK)
            goto bail;
    }

    badCount = 0;
    if (SSTOpenImage(1, &srcImg1) != 0)
        goto bail;
    if (SSTLoadData(1, &srcImg1, trackBuf, &badCount) != 0)
        goto bail;
    LOGI("FOUND %ld bad bytes in part 1", badCount);
    if (badCount > kBadCountThreshold) {
        CheckedLoadString(&errMsg, IDS_BAD_SST_IMAGE);
        if (MessageBox(errMsg, appName, MB_OKCANCEL | MB_ICONWARNING) != IDOK)
            goto bail;
    }

    /*
     * Realign the tracks and OR 0x80 to everything.
     */
    SSTProcessTrackData(trackBuf);

    /*
     * Pick the output file and write the buffer to it.
     */
    saveDlg.m_ofn.lpstrTitle = L"Save .NIB disk image as...";
    saveDlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);
    if (saveDlg.DoModal() != IDOK) {
        LOGI(" User bailed out of image save dialog");
        goto bail;
    }
    saveFolder = saveDlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(saveDlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    saveName = saveDlg.GetPathName();
    LOGI("File will be saved to '%ls'", (LPCWSTR) saveName);

    /* remove the file if it exists */
    errMsg = RemoveFile(saveName);
    if (!errMsg.IsEmpty()) {
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    FILE* fp;
    fp = _wfopen(saveName, L"wb");
    if (fp == NULL) {
        errMsg.Format(L"Unable to create '%ls': %hs.",
            (LPCWSTR) saveName, strerror(errno));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    if (fwrite(trackBuf, kSSTNumTracks * kSSTTrackLen, 1, fp) != 1) {
        errMsg.Format(L"Failed while writing to new image file: %hs.",
            strerror(errno));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        fclose(fp);
        goto bail;
    }

    fclose(fp);

    SuccessBeep();

    /*
     * We're done.  Give them the opportunity to open the disk image they
     * just created.
     */
    {
        DoneOpenDialog doneOpen(this);

        if (doneOpen.DoModal() == IDOK) {
            LOGI(" At user request, opening '%ls'", (LPCWSTR) saveName);

            DoOpenArchive(saveName, L"nib", kFilterIndexDiskImage, false);
        }
    }

bail:
    delete[] trackBuf;
    return;
}

int MainWindow::SSTOpenImage(int seqNum, DiskImg* pDiskImg)
{
    DIError dierr;
    int result = -1;
    CString openFilters, errMsg;
    CString loadName, saveFolder;
    CStringA loadNameA;

    /*
     * Select the image to convert.
     */
    openFilters = kOpenDiskImage;
    openFilters += kOpenAll;
    openFilters += kOpenEnd;
    CFileDialog dlg(TRUE, L"dsk", NULL, OFN_FILEMUSTEXIST, openFilters, this);

    dlg.m_ofn.Flags |= OFN_HIDEREADONLY;
    if (seqNum == 0)
        dlg.m_ofn.lpstrTitle = L"Select first SST image";
    else
        dlg.m_ofn.lpstrTitle = L"Select second SST image";
    dlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

    if (dlg.DoModal() != IDOK)
        goto bail;
    loadName = dlg.GetPathName();

    saveFolder = dlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(dlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    /* open the image file and analyze it */
    loadNameA = loadName;
    dierr = pDiskImg->OpenImage(loadNameA, PathProposal::kLocalFssep, true);
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Unable to open disk image: %hs.",
            DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    if (pDiskImg->AnalyzeImage() != kDIErrNone) {
        errMsg.Format(L"The file '%ls' doesn't seem to hold a valid disk image.",
            (LPCWSTR) loadName);
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /*
     * If confirm image format is set, or we can't figure out the sector
     * ordering, prompt the user.
     */
    if (pDiskImg->GetSectorOrder() == DiskImg::kSectorOrderUnknown ||
        fPreferences.GetPrefBool(kPrQueryImageFormat))
    {
        if (TryDiskImgOverride(pDiskImg, loadName,
            DiskImg::kFormatGenericDOSOrd, NULL, false, &errMsg) != IDOK)
        {
            goto bail;
        }
        if (!errMsg.IsEmpty()) {
            ShowFailureMsg(this, errMsg, IDS_FAILED);
            goto bail;
        }
    }

    if (pDiskImg->GetFSFormat() != DiskImg::kFormatUnknown &&
        !DiskImg::IsGenericFormat(pDiskImg->GetFSFormat()))
    {
        errMsg = L"This disk image appears to have a valid filesystem.  SST"
                 L" images are just raw track dumps.";
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }
    if (pDiskImg->GetNumTracks() != kSSTNumTracks ||
        pDiskImg->GetNumSectPerTrack() != kSSTNumSectPerTrack)
    {
        errMsg = L"ERROR: only 5.25\" floppy disk images can be SST inputs.";
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    /* use DOS filesystem sector ordering */
    dierr = pDiskImg->OverrideFormat(pDiskImg->GetPhysicalFormat(),
                DiskImg::kFormatGenericDOSOrd, pDiskImg->GetSectorOrder());
    if (dierr != kDIErrNone) {
        errMsg = L"ERROR: internal failure: format override failed.";
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        goto bail;
    }

    result = 0;

bail:
    return result;
}

int MainWindow::SSTLoadData(int seqNum, DiskImg* pDiskImg, uint8_t* trackBuf,
    long* pBadCount)
{
    DIError dierr;
    uint8_t sctBuf[256];
    int track, sector;
    long bufOffset;

    for (track = 0; track < kSSTNumTracks; track++) {
        int virtualTrack = track + (seqNum * kSSTNumTracks);
        bufOffset = SSTGetBufOffset(virtualTrack);
        //LOGI("USING offset=%ld (track=%d / %d)",
        //    bufOffset, track, virtualTrack);

        if (virtualTrack & 0x01) {
            /* odd-numbered track, sectors 15-4 */
            for (sector = 15; sector >= 4; sector--) {
                dierr = pDiskImg->ReadTrackSector(track, sector, sctBuf);
                if (dierr != kDIErrNone) {
                    LOGI("ERROR: on track=%d sector=%d",
                        track, sector);
                    return -1;
                }

                *pBadCount += SSTCountBadBytes(sctBuf, 256);

                memcpy(trackBuf + bufOffset, sctBuf, 256);
                bufOffset += 256;
            }
        } else {
            for (sector = 13; sector >= 0; sector--) {
                dierr = pDiskImg->ReadTrackSector(track, sector, sctBuf);
                if (dierr != kDIErrNone) {
                    LOGI("ERROR: on track=%d sector=%d",
                        track, sector);
                    return -1;
                }

                *pBadCount += SSTCountBadBytes(sctBuf, 256);

                memcpy(trackBuf + bufOffset, sctBuf, 256);
                bufOffset += 256;
            }
        }
    }

    return 0;
}

long MainWindow::SSTGetBufOffset(int track)
{
    assert(track >= 0 && track < kSSTNumTracks*2);

    long offset;

    if (track & 0x01) {
        /* odd, use start of data */
        offset = (track / 2) * kSSTTrackLen;
    } else {
        /* even, start of data plus 12 sectors */
        offset = (track / 2) * kSSTTrackLen + 12 * 256;
    }

    assert(offset >= 0 && offset < kSSTTrackLen * kSSTNumTracks);

    return offset;
}

long MainWindow::SSTCountBadBytes(const unsigned char* sctBuf, int count)
{
    long badCount = 0;
    unsigned char uch;

    while (count--) {
        uch = (*sctBuf) | 0x80;
        if (uch >= 0x80 && uch <= 0x92)
            badCount++;
        sctBuf++;
    }

    return badCount;
}

void MainWindow::SSTProcessTrackData(unsigned char* trackBuf)
{
    unsigned char* trackPtr;
    int track;

    for (track = 0, trackPtr = trackBuf;  track < kSSTNumTracks;
        track++, trackPtr += kSSTTrackLen)
    {
        bool inRun;
        int start, longestStart;
        int count7f, longest = -1;
        int i;

        inRun = false;
        for (i = 0; i < kSSTTrackLen; i++) {
            if (trackPtr[i] == 0x7f) {
                if (inRun) {
                    count7f++;
                } else {
                    count7f = 1;
                    start = i;
                    inRun = true;
                }
            } else {
                if (inRun) {
                    if (count7f > longest) {
                        longest = count7f;
                        longestStart = start;
                    }
                    inRun = false;
                } else {
                    /* do nothing */
                }
            }

            trackPtr[i] |= 0x80;
        }


        if (longest == -1) {
            LOGI("HEY: couldn't find any 0x7f in track %d",
                track);
        } else {
            LOGI("Found run of %d at %d in track %d",
                longest, longestStart, track);

            int bkpt = longestStart + longest;
            assert(bkpt < kSSTTrackLen);

            char oneTrack[kSSTTrackLen];
            memcpy(oneTrack, trackPtr, kSSTTrackLen);

            /* copy it back so sync bytes are at end of track */
            memcpy(trackPtr, oneTrack + bkpt, kSSTTrackLen - bkpt);
            memcpy(trackPtr + (kSSTTrackLen - bkpt), oneTrack, bkpt);
        }
    }
}


/*
 * ==========================================================================
 *      Volume Copier
 * ==========================================================================
 */

void MainWindow::OnToolsVolumeCopierVolume(void)
{
    VolumeCopier(false);
}

void MainWindow::OnToolsVolumeCopierFile(void)
{
    VolumeCopier(true);
}

void MainWindow::VolumeCopier(bool openFile)
{
    VolumeCopyDialog copyDlg(this);
    DiskImg srcImg;
    //DiskFS* pDiskFS = NULL;
    DIError dierr;
    CString failed, errMsg, msg;
    CString deviceName;
    bool readOnly = false;
    int result;

    /* flush current archive in case that's what we're planning to edit */
    OnFileSave();

    CheckedLoadString(&failed, IDS_FAILED);

    if (!openFile) {
        /*
         * Select the volume to manipulate.
         */
        OpenVolumeDialog openVolDlg(this);
        //openVolDlg.fReadOnly = false;
        //openVolDlg.fAllowROChange = true;
        result = openVolDlg.DoModal();
        if (result != IDOK)
            goto bail;
        deviceName = openVolDlg.fChosenDrive;
        readOnly = (openVolDlg.fReadOnly != 0);
    } else {
        /*
         * Open a disk image file instead.
         */
        CString openFilters;
        openFilters = kOpenDiskImage;
        openFilters += kOpenAll;
        openFilters += kOpenEnd;
        CFileDialog fileDlg(TRUE, L"dsk", NULL, OFN_FILEMUSTEXIST, openFilters, this);

        //dlg.m_ofn.Flags |= OFN_HIDEREADONLY;
        fileDlg.m_ofn.Flags &= ~(OFN_READONLY);
        fileDlg.m_ofn.lpstrTitle = L"Select disk image file";
        fileDlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

        if (fileDlg.DoModal() != IDOK)
            goto bail;
        deviceName = fileDlg.GetPathName();
        readOnly = (fileDlg.GetReadOnlyPref() != 0);
    }

    /*
     * Open the disk image and figure out what it is.
     */
    {
        CWaitCursor waitc;

        DiskImg::SetAllowWritePhys0(false);

        CStringA deviceNameA(deviceName);
        dierr = srcImg.OpenImage(deviceNameA, '\0', readOnly);
        if (dierr == kDIErrAccessDenied) {
            if (openFile) {
                errMsg.Format(L"Unable to open '%ls': %hs (try opening the file"
                              L" with 'Read Only' checked).",
                    (LPCWSTR) deviceName, DiskImgLib::DIStrError(dierr));
            } else if (!IsWin9x() && !openFile) {
                errMsg.Format(L"Unable to open '%ls': %hs (make sure you have"
                              L" administrator privileges).",
                    (LPCWSTR) deviceName, DiskImgLib::DIStrError(dierr));
            } else {
                errMsg.Format(L"Unable to open '%ls': %hs.",
                    (LPCWSTR) deviceName, DiskImgLib::DIStrError(dierr));
            }
            ShowFailureMsg(this, errMsg, IDS_FAILED);
            goto bail;
        } else if (dierr != kDIErrNone) {
            errMsg.Format(L"Unable to open '%ls': %hs.", (LPCWSTR) deviceName,
                DiskImgLib::DIStrError(dierr));
            ShowFailureMsg(this, errMsg, IDS_FAILED);
            goto bail;
        }

        /* analyze it to get #of blocks and determine the FS */
        if (srcImg.AnalyzeImage() != kDIErrNone) {
            errMsg.Format(L"There isn't a valid disk image here?!?");
            MessageBox(errMsg, failed, MB_OK|MB_ICONSTOP);
            goto bail;
        }
    }

    /*
     * If requested (or necessary), verify the format.
     */
    if (srcImg.GetFSFormat() == DiskImg::kFormatUnknown ||
        srcImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown ||
        fPreferences.GetPrefBool(kPrQueryImageFormat))
    {
        if (TryDiskImgOverride(&srcImg, deviceName, DiskImg::kFormatUnknown,
            NULL, true, &errMsg) != IDOK)
        {
            goto bail;
        }
        if (!errMsg.IsEmpty()) {
            ShowFailureMsg(this, errMsg, IDS_FAILED);
            goto bail;
        }
    }

    /*
     * Hand the DiskImg object off to the volume copier dialog.
     */
    copyDlg.fpDiskImg = &srcImg;
    copyDlg.fPathName = deviceName;
    (void) copyDlg.DoModal();

    /*
     * The volume copier could have modified our open file.  If it has,
     * we need to close and reopen the archive.
     */
    srcImg.CloseImage();    // could interfere with volume reopen
    if (fNeedReopen) {
        PeekAndPump();      // clear out dialog
        ReopenArchive();
    }

bail:
    return;
}


/*
 * ==========================================================================
 *      Disk image creator
 * ==========================================================================
 */

void MainWindow::OnToolsDiskImageCreator(void)
{
    CreateImageDialog createDlg(this);
    DiskArchive* pNewArchive = NULL;

    createDlg.fDiskFormatIdx =
        fPreferences.GetPrefLong(kPrDiskImageCreateFormat);
    if (createDlg.fDiskFormatIdx < 0)
        createDlg.fDiskFormatIdx = CreateImageDialog::kFmtProDOS;

    /*
     * Ask the user what sort of disk they'd like to create.
     */
    if (createDlg.DoModal() != IDOK)
        return;

    fPreferences.SetPrefLong(kPrDiskImageCreateFormat, createDlg.fDiskFormatIdx);

    /*
     * Set up the options struct.  We set base.sectorOrder later.
     */
    assert(createDlg.fNumBlocks > 0);

    DiskArchive::NewOptions options;
    memset(&options, 0, sizeof(options));
    switch (createDlg.fDiskFormatIdx) {
    case CreateImageDialog::kFmtBlank:
        options.base.format = DiskImg::kFormatUnknown;
        options.blank.numBlocks = createDlg.fNumBlocks;
        break;
    case CreateImageDialog::kFmtProDOS:
        options.base.format = DiskImg::kFormatProDOS;
        options.prodos.numBlocks = createDlg.fNumBlocks;
        options.prodos.volName = createDlg.fVolName_ProDOS;
        break;
    case CreateImageDialog::kFmtPascal:
        options.base.format = DiskImg::kFormatPascal;
        options.pascalfs.numBlocks = createDlg.fNumBlocks;
        options.pascalfs.volName = createDlg.fVolName_Pascal;
        break;
    case CreateImageDialog::kFmtHFS:
        options.base.format = DiskImg::kFormatMacHFS;
        options.hfs.numBlocks = createDlg.fNumBlocks;
        options.hfs.volName = createDlg.fVolName_HFS;
        break;
    case CreateImageDialog::kFmtDOS32:
        options.base.format = DiskImg::kFormatDOS32;
        options.dos.volumeNum = createDlg.fDOSVolumeNum;
        options.dos.allocDOSTracks = (createDlg.fAllocTracks_DOS != 0);
        options.dos.numTracks = 35;
        options.dos.numSectors = 13;
        break;
    case CreateImageDialog::kFmtDOS33:
        options.base.format = DiskImg::kFormatDOS33;
        options.dos.volumeNum = createDlg.fDOSVolumeNum;
        options.dos.allocDOSTracks = (createDlg.fAllocTracks_DOS != 0);
        if (createDlg.fNumBlocks <= 400) {
            ASSERT(createDlg.fNumBlocks % 8 == 0);
            options.dos.numTracks = createDlg.fNumBlocks / 8;
            options.dos.numSectors = 16;
        } else if (createDlg.fNumBlocks <= 800) {
            ASSERT(createDlg.fNumBlocks % 16 == 0);
            options.dos.numTracks = createDlg.fNumBlocks / 16;
            options.dos.numSectors = 32;
            options.dos.allocDOSTracks = false;
        } else {
            ASSERT(false);
            return;
        }
        break;
    default:
        LOGI("Invalid fDiskFormatIdx %d from CreateImageDialog",
            createDlg.fDiskFormatIdx);
        ASSERT(false);
        return;
    }

    /*
     * Select the file to store it in.
     */
    CString filename, saveFolder, errStr;
    int filterIndex = 1;
    CString formats;

    if (createDlg.fDiskFormatIdx == CreateImageDialog::kFmtDOS32) {
        formats = L"13-sector disk (*.d13)|*.d13|";
    } else {
        formats = L"ProDOS-ordered image (*.po)|*.po|";
        if (createDlg.fNumBlocks == 280) {
            formats += L"DOS-ordered image (*.do)|*.do|";
            filterIndex = 2;
        }
    }
    formats += L"|";

    CFileDialog saveDlg(FALSE, L"po", NULL,
        OFN_OVERWRITEPROMPT|OFN_NOREADONLYRETURN|OFN_HIDEREADONLY,
        formats, this);
    saveDlg.m_ofn.lpstrTitle = L"New Disk Image";
    saveDlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);
    saveDlg.m_ofn.nFilterIndex = filterIndex;

    if (saveDlg.DoModal() != IDOK) {
        LOGI(" User cancelled xfer from image create dialog");
        return;
    }

    saveFolder = saveDlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(saveDlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    filename = saveDlg.GetPathName();
    LOGI(" Will xfer to file '%ls' (filterIndex=%d)",
        (LPCWSTR) filename, saveDlg.m_ofn.nFilterIndex);

    if (createDlg.fDiskFormatIdx == CreateImageDialog::kFmtDOS32) {
        options.base.sectorOrder = DiskImg::kSectorOrderDOS;
    } else {
        if (saveDlg.m_ofn.nFilterIndex == 2)
            options.base.sectorOrder = DiskImg::kSectorOrderDOS;
        else
            options.base.sectorOrder = DiskImg::kSectorOrderProDOS;
    }

    /* remove file if it already exists */
    CString errMsg;
    errMsg = RemoveFile(filename);
    if (!errMsg.IsEmpty()) {
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        return;
    }

    pNewArchive = new DiskArchive;

    /* create the new archive, showing a "busy" message */
    {
        ExclusiveModelessDialog* pWaitDlg = new ExclusiveModelessDialog;
        pWaitDlg->Create(IDD_FORMATTING, this);
        pWaitDlg->CenterWindow();
        PeekAndPump();  // redraw
        CWaitCursor waitc;

        errStr = pNewArchive->New(filename, &options);

        pWaitDlg->DestroyWindow();
        //PeekAndPump();    // redraw
    }

    delete pNewArchive;     // close it, either way
    if (!errStr.IsEmpty()) {
        ShowFailureMsg(this, errStr, IDS_FAILED);
        (void) _wunlink(filename);
    } else {
        LOGI("Disk image created successfully");
#if 0
        SuccessBeep();

        /* give them the opportunity to open the new disk image */
        DoneOpenDialog doneOpen(this);

        if (doneOpen.DoModal() == IDOK) {
            LOGI(" At user request, opening '%ls'", filename);

            DoOpenArchive(filename, "dsk", kFilterIndexDiskImage, false);
        }
#else
        if (createDlg.fDiskFormatIdx != CreateImageDialog::kFmtBlank)
            DoOpenArchive(filename, L"dsk", kFilterIndexDiskImage, false);
#endif
    }
}


/*
 * ==========================================================================
 *      EOL scanner
 * ==========================================================================
 */

void MainWindow::OnToolsEOLScanner(void)
{
    CString fileName, saveFolder, errMsg;

    CString openFilters;
    openFilters = kOpenAll;
    openFilters += kOpenEnd;
    CFileDialog fileDlg(TRUE, L"dsk", NULL, OFN_FILEMUSTEXIST, openFilters, this);

    fileDlg.m_ofn.Flags |= OFN_HIDEREADONLY;
    //fileDlg.m_ofn.Flags &= ~(OFN_READONLY);
    fileDlg.m_ofn.lpstrTitle = L"Select file to scan";
    fileDlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

    if (fileDlg.DoModal() != IDOK)
        return;
    fileName = fileDlg.GetPathName();

    saveFolder = fileDlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(fileDlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    LOGI("Scanning '%ls'", (LPCWSTR) fileName);

    FILE* fp = _wfopen(fileName, L"rb");
    if (fp == NULL) {
        errMsg.Format(L"Unable to open '%ls': %hs.",
            (LPCWSTR) fileName, strerror(errno));
        ShowFailureMsg(this, errMsg, IDS_FAILED);
        return;
    }

    long numCR, numLF, numCRLF, numHAChars, numChars;
    bool lastCR;
    int ic;

    /*
     * Plow through the file, counting up characters.
     */
    numCR = numLF = numCRLF = numChars = numHAChars = 0;
    lastCR = false;
    while (true) {
        ic = getc(fp);
        if (ic == EOF)
            break;

        if ((ic & 0x80) != 0)
            numHAChars++;

        if (ic == '\r') {
            lastCR = true;
            numCR++;
        } else if (ic == '\n') {
            if (lastCR) {
                numCR--;
                numCRLF++;
                lastCR = false;
            } else {
                numLF++;
            }
        } else {
            lastCR = false;
        }
        numChars++;
    }
    fclose(fp);

    LOGD("Got CR=%ld LF=%ld CRLF=%ld (numChars=%ld)",
        numCR, numLF, numCRLF, numChars);

    EOLScanDialog output;
    output.fCountCR = numCR;
    output.fCountLF = numLF;
    output.fCountCRLF = numCRLF;
    output.fCountChars = numChars;
    output.fCountHighASCII = numHAChars;
    (void) output.DoModal();
}


/*
 * ==========================================================================
 *      2MG disk image properties editor
 * ==========================================================================
 */

void MainWindow::OnToolsTwoImgProps(void)
{
    CString fileName, saveFolder, errMsg;
    CString openFilters;

    /* flush current archive in case that's what we're planning to edit */
    OnFileSave();

    /*
     * Select the file to open.
     */
    openFilters = L"2MG Disk Images (.2mg .2img)|*.2mg;*.2img|";
    openFilters += kOpenAll;
    openFilters += kOpenEnd;
    CFileDialog fileDlg(TRUE, L"2mg", NULL, OFN_FILEMUSTEXIST, openFilters, this);

    fileDlg.m_ofn.Flags |= OFN_HIDEREADONLY;
    //fileDlg.m_ofn.Flags &= ~(OFN_READONLY);
    fileDlg.m_ofn.lpstrTitle = L"Select file to edit";
    fileDlg.m_ofn.lpstrInitialDir = fPreferences.GetPrefString(kPrOpenArchiveFolder);

    if (fileDlg.DoModal() != IDOK)
        return;
    fileName = fileDlg.GetPathName();

    saveFolder = fileDlg.m_ofn.lpstrFile;
    saveFolder = saveFolder.Left(fileDlg.m_ofn.nFileOffset);
    fPreferences.SetPrefString(kPrOpenArchiveFolder, saveFolder);

    /*
     * Open it up.
     */
    bool changed;
    changed = EditTwoImgProps(fileName);

    if (changed && IsOpenPathName(fileName)) {
        PeekAndPump();      // clear out dialog
        ReopenArchive();
    }
}

bool MainWindow::EditTwoImgProps(const WCHAR* fileName)
{
    TwoImgPropsDialog dialog;
    TwoImgHeader header;
    FILE* fp = NULL;
    bool dirty = false;
    CString errMsg;
    long totalLength;
    bool readOnly = false;

    LOGI("EditTwoImgProps '%ls'", fileName);
    fp = _wfopen(fileName, L"r+b");
    if (fp == NULL) {
        int firstError = errno;
        fp = _wfopen(fileName, L"rb");
        if (fp == NULL) {
            errMsg.Format(L"Unable to open '%ls': %hs.",
                fileName, strerror(firstError));
            goto bail;
        } else {
            readOnly = true;
        }
    }

    fseek(fp, 0, SEEK_END);
    totalLength = ftell(fp);
    rewind(fp);

    if (header.ReadHeader(fp, totalLength) != 0) {
        errMsg.Format(L"Unable to process 2MG header in '%ls'"
                      L" (are you sure this is in 2MG format?).",
            (LPCWSTR) fileName);
        goto bail;
    }

    dialog.Setup(&header, readOnly);
    if (dialog.DoModal() == IDOK) {
        long result;
        //header.SetCreatorChunk("fubar", 5);
        header.DumpHeader();

        rewind(fp);
        if (header.WriteHeader(fp) != 0) {
            errMsg = L"Unable to write 2MG header";
            goto bail;
        }

        /*
         * Clip off the footer.  They might have had one before but don't
         * have one now.  If they do have one now we'll add it back in a
         * second.
         */
        result = fseek(fp, header.fDataOffset + header.fDataLen, SEEK_SET);
        if (result < 0) {
            errMsg = L"Unable to seek to end of 2MG file";
            goto bail;
        }
        dirty = true;

        if (::chsize(fileno(fp), ftell(fp)) != 0) {
            errMsg = L"Unable to truncate 2MG file before writing footer";
            goto bail;
        }

        if (header.fCmtLen || header.fCreatorLen) {
            if (header.WriteFooter(fp) != 0) {
                errMsg = L"Unable to write 2MG footer";
                goto bail;
            }
        }

        LOGI("2MG success!");
    }

bail:
    if (fp != NULL)
        fclose(fp);
    if (!errMsg.IsEmpty()) {
        ShowFailureMsg(this, errMsg, IDS_FAILED);
    }

    return dirty;
}
