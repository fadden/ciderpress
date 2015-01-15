/*
 * CiderPress
 * Copyright (C) 2009 by CiderPress authors.  All Rights Reserved.
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Bridge between DiskImg and GenericArchive.
 */
#include "stdafx.h"
#include "DiskArchive.h"
#include "NufxArchive.h"
#include "Preferences.h"
#include "Main.h"
#include "ImageFormatDialog.h"
#include "RenameEntryDialog.h"
#include "ConfirmOverwriteDialog.h"
#include "../diskimg/DiskImgDetail.h"
#include "../reformat/Charset.h"

static const char kEmptyFolderMarker[] = ".$$EmptyFolder";


/*
 * ===========================================================================
 *      DiskEntry
 * ===========================================================================
 */

int DiskEntry::ExtractThreadToBuffer(int which, char** ppText, long* pLength,
    CString* pErrMsg) const
{
    DIError dierr;
    A2FileDescr* pOpenFile = NULL;
    char* dataBuf = NULL;
    bool rsrcFork;
    bool needAlloc = true;
    int result = -1;

    ASSERT(fpFile != NULL);
    ASSERT(pErrMsg != NULL);
    *pErrMsg = "";

    if (*ppText != NULL)
        needAlloc = false;

    if (GetDamaged()) {
        *pErrMsg = "File is damaged";
        goto bail;
    }

    if (which == kDataThread)
        rsrcFork = false;
    else if (which == kRsrcThread)
        rsrcFork = true;
    else {
        *pErrMsg = "No such fork";
        goto bail;
    }

    LONGLONG len;
    if (rsrcFork)
        len = fpFile->GetRsrcLength();
    else
        len = fpFile->GetDataLength();

    if (len == 0) {
        if (needAlloc) {
            *ppText = new char[1];
            **ppText = '\0';
        }
        *pLength = 0;
        result = IDOK;
        goto bail;
    } else if (len < 0) {
        assert(rsrcFork);   // forked files always have a data fork
        *pErrMsg = L"That fork doesn't exist";
        goto bail;
    }

    dierr = fpFile->Open(&pOpenFile, true, rsrcFork);
    if (dierr != kDIErrNone) {
        *pErrMsg = L"File open failed";
        goto bail;
    }

    SET_PROGRESS_BEGIN();
    pOpenFile->SetProgressUpdater(DiskArchive::ProgressCallback, len, NULL);

    if (needAlloc) {
        dataBuf = new char[(int) len];
        if (dataBuf == NULL) {
            pErrMsg->Format(L"ERROR: allocation of %I64d bytes failed", len);
            goto bail;
        }
    } else {
        if (*pLength < (long) len) {
            pErrMsg->Format(L"ERROR: buf size %ld too short (%ld)",
                *pLength, (long) len);
            goto bail;
        }
        dataBuf = *ppText;
    }

    dierr = pOpenFile->Read(dataBuf, (size_t) len);
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrCancelled) {
            result = IDCANCEL;
        } else {
            pErrMsg->Format(L"File read failed: %hs",
                DiskImgLib::DIStrError(dierr));
        }
        goto bail;
    }

    if (needAlloc)
        *ppText = dataBuf;
    *pLength = (long) len;
    result = IDOK;

bail:
    if (pOpenFile != NULL)
        pOpenFile->Close();
    if (result == IDOK) {
        SET_PROGRESS_END();
        ASSERT(pErrMsg->IsEmpty());
    } else {
        ASSERT(result == IDCANCEL || !pErrMsg->IsEmpty());
        if (needAlloc) {
            delete[] dataBuf;
            ASSERT(*ppText == NULL);
        }
    }
    return result;
}

int DiskEntry::ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
    ConvertHighASCII convHA, CString* pErrMsg) const
{
    A2FileDescr* pOpenFile = NULL;
    bool rsrcFork;
    int result = -1;

    ASSERT(IDOK != -1 && IDCANCEL != -1);
    ASSERT(fpFile != NULL);

    if (which == kDataThread)
        rsrcFork = false;
    else if (which == kRsrcThread)
        rsrcFork = true;
    else {
        /* if we handle disk images, make sure we disable "conv" */
        *pErrMsg = L"No such fork";
        goto bail;
    }

    LONGLONG len;
    if (rsrcFork)
        len = fpFile->GetRsrcLength();
    else
        len = fpFile->GetDataLength();

    if (len == 0) {
        LOGI("Empty fork");
        result = IDOK;
        goto bail;
    } else if (len < 0) {
        assert(rsrcFork);   // forked files always have a data fork
        *pErrMsg = L"That fork doesn't exist";
        goto bail;
    }

    DIError dierr;
    dierr = fpFile->Open(&pOpenFile, true, rsrcFork);
    if (dierr != kDIErrNone) {
        *pErrMsg = L"Unable to open file on disk image";
        goto bail;
    }

    dierr = CopyData(pOpenFile, outfp, conv, convHA, pErrMsg);
    if (dierr != kDIErrNone) {
        if (pErrMsg->IsEmpty()) {
            pErrMsg->Format(L"Failed while copying data: %hs\n",
                DiskImgLib::DIStrError(dierr));
        }
        goto bail;
    }

    result = IDOK;

bail:
    if (pOpenFile != NULL)
        pOpenFile->Close();
    return result;
}

DIError DiskEntry::CopyData(A2FileDescr* pOpenFile, FILE* outfp, ConvertEOL conv,
    ConvertHighASCII convHA, CString* pMsg) const
{
    DIError dierr = kDIErrNone;
    const int kChunkSize = 16384;
    char buf[kChunkSize];
    //bool firstChunk = true;
    //EOLType sourceType;
    bool lastCR = false;
    LONGLONG srcLen, dataRem;

    /* get the length of the open file */
    dierr = pOpenFile->Seek(0, DiskImgLib::kSeekEnd);
    if (dierr != kDIErrNone)
        goto bail;
    srcLen = pOpenFile->Tell();
    dierr = pOpenFile->Rewind();
    if (dierr != kDIErrNone)
        goto bail;
    ASSERT(srcLen > 0); // empty files should've been caught earlier

    SET_PROGRESS_BEGIN();
    pOpenFile->SetProgressUpdater(DiskArchive::ProgressCallback, srcLen, NULL);

    /*
     * Loop until all data copied.
     */
    dataRem = srcLen;
    while (dataRem) {
        int chunkLen;

        if (dataRem > kChunkSize)
            chunkLen = kChunkSize;
        else
            chunkLen = (int) dataRem;

        /* read a chunk from the source file */
        dierr = pOpenFile->Read(buf, chunkLen);
        if (dierr != kDIErrNone) {
            pMsg->Format(L"File read failed: %hs",
                DiskImgLib::DIStrError(dierr));
            goto bail;
        }

        /* write chunk to destination file */
        int err = GenericEntry::WriteConvert(outfp, buf, chunkLen, &conv,
                    &convHA, &lastCR);
        if (err != 0) {
            pMsg->Format(L"File write failed: %hs", strerror(err));
            dierr = kDIErrGeneric;
            goto bail;
        }

        dataRem -= chunkLen;
        //SET_PROGRESS_UPDATE(ComputePercent(srcLen - dataRem, srcLen));
    }

bail:
    pOpenFile->ClearProgressUpdater();
    SET_PROGRESS_END();
    return dierr;
}

bool DiskEntry::GetFeatureFlag(Feature feature) const
{
    DiskImg::FSFormat format;

    format = fpFile->GetDiskFS()->GetDiskImg()->GetFSFormat();

    switch (feature) {
    case kFeatureCanChangeType:
    {
        //if (GetRecordKind() == kRecordKindVolumeDir)
        //  return false;

        switch (format) {
        case DiskImg::kFormatProDOS:
        case DiskImg::kFormatPascal:
        case DiskImg::kFormatMacHFS:
        case DiskImg::kFormatDOS32:
        case DiskImg::kFormatDOS33:
            return true;
        default:
            return false;
        }
    }
    case kFeaturePascalTypes:
    {
        switch (format) {
        case DiskImg::kFormatPascal:
            return true;
        default:
            return false;
        }
    }
    case kFeatureDOSTypes:
    {
        switch (format) {
        case DiskImg::kFormatDOS32:
        case DiskImg::kFormatDOS33:
            return true;
        default:
            return false;
        }
    }
    case kFeatureHFSTypes:
    {
        switch (format) {
        case DiskImg::kFormatMacHFS:
            return true;
        default:
            return false;
        }
    }
    case kFeatureHasFullAccess:
    {
        switch (format) {
        case DiskImg::kFormatProDOS:
            return true;
        default:
            return false;
        }
    }
    case kFeatureHasSimpleAccess:
    {
        switch (format) {
        case DiskImg::kFormatDOS33:
        case DiskImg::kFormatDOS32:
        case DiskImg::kFormatCPM:
        case DiskImg::kFormatMacHFS:
            return true;
        default:
            return false;
        }
    }
    case kFeatureHasInvisibleFlag:
    {
        switch(format) {
        case DiskImg::kFormatProDOS:
        case DiskImg::kFormatMacHFS:
            return true;
        default:
            return false;
        }
    }
    default:
        LOGI("Unexpected feature flag %d", feature);
        assert(false);
        return false;
    }

    assert(false);
    return false;
}


/*
 * ===========================================================================
 *      DiskArchive
 * ===========================================================================
 */

/*static*/ CString DiskArchive::AppInit(void)
{
    CString result("");
    DIError dierr;
    int32_t major, minor, bug;

    LOGI("Initializing DiskImg library");

    // set this before initializing, so we can see init debug msgs
    DiskImgLib::Global::SetDebugMsgHandler(DebugMsgHandler);

    dierr = DiskImgLib::Global::AppInit();
    if (dierr != kDIErrNone) {
        result.Format(L"DiskImg DLL failed to initialize: %hs\n",
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    DiskImgLib::Global::GetVersion(&major, &minor, &bug);
    if (major != kDiskImgVersionMajor || minor < kDiskImgVersionMinor) {
        result.Format(L"Older or incompatible version of DiskImg DLL found.\r\r"
                      L"Wanted v%d.%d.x, found %ld.%ld.%ld.",
                kDiskImgVersionMajor, kDiskImgVersionMinor,
                major, minor, bug);
        goto bail;
    }

bail:
    return result;
}

/*static*/ void DiskArchive::AppCleanup(void)
{
    DiskImgLib::Global::AppCleanup();
}

/*static*/ void DiskArchive::DebugMsgHandler(const char* file, int line,
    const char* msg)
{
    ASSERT(file != NULL);
    ASSERT(msg != NULL);

    LOG_BASE(DebugLog::LOG_INFO, file, line, "<diskimg> %hs", msg);
}

/*static*/ bool DiskArchive::ProgressCallback(DiskImgLib::A2FileDescr* pFile,
    DiskImgLib::di_off_t max, DiskImgLib::di_off_t current, void* state)
{
    int status;

    //::Sleep(10);
    status = SET_PROGRESS_UPDATE(ComputePercent(current, max));
    if (status == IDCANCEL) {
        LOGI("IDCANCEL returned from Main progress updater");
        return false;
    }

    return true;        // tell DiskImgLib to continue what it's doing
}

/*static*/ bool DiskArchive::ScanProgressCallback(void* cookie, const char* str,
    int count)
{
    CString fmt;
    bool cont;

    if (count == 0)
        fmt = str;
    else
        fmt.Format(L"%hs (%%d)", str);
    cont = SET_PROGRESS_COUNTER_2(fmt, count);

    if (!cont) {
        LOGI("cancelled");
    }

    return cont;
}

GenericArchive::OpenResult DiskArchive::Open(const WCHAR* filename,
    bool readOnly, CString* pErrMsg)
{
    DIError dierr;
    CString errMsg;
    OpenResult result = kResultUnknown;
    const Preferences* pPreferences = GET_PREFERENCES();

    ASSERT(fpPrimaryDiskFS == NULL);
    ASSERT(filename != NULL);
    //ASSERT(ext != NULL);

    ASSERT(pPreferences != NULL);

    fIsReadOnly = readOnly;

    // special case for volume open
    bool isVolume = false;
    if (filename[0] >= 'A' && filename[0] <= 'Z' &&
        filename[1] == ':' && filename[2] == '\\' &&
        filename[3] == '\0')
    {
        isVolume = true;
    }

    /*
     * Open the image.  This can be very slow for compressed images,
     * especially 3.5" FDI images.
     */
    {
        CWaitCursor waitc;

        // TODO(Unicode): modify DiskImg lib to accept wide paths
        CStringA fileNameA(filename);
        if (!PathName::TestNarrowConversion(filename, fileNameA, &errMsg)) {
            result = kResultFailure;
            goto bail;
        }
        dierr = fDiskImg.OpenImage(fileNameA, PathProposal::kLocalFssep,
            readOnly);
        if (dierr == kDIErrAccessDenied && !readOnly && !isVolume) {
            // retry file open with read-only set
            // don't do that for volumes -- assume they know what they want
            LOGD("  Retrying open with read-only set");
            fIsReadOnly = readOnly = true;
            dierr = fDiskImg.OpenImage(fileNameA, PathProposal::kLocalFssep,
                readOnly);
        }
        if (dierr != kDIErrNone) {
            if (dierr == kDIErrFileArchive) {
                result = kResultFileArchive;
            } else {
                result = kResultFailure;
                errMsg.Format(L"Unable to open '%ls': %hs.", filename,
                    DiskImgLib::DIStrError(dierr));
            }
            goto bail;
        }
    }

    dierr = fDiskImg.AnalyzeImage();
    if (dierr != kDIErrNone) {
        result = kResultFailure;
        errMsg.Format(L"Analysis of '%ls' failed: %hs", filename,
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    /* allow them to override sector order and filesystem, if requested */
    if (pPreferences->GetPrefBool(kPrQueryImageFormat)) {
        ImageFormatDialog imf;

        imf.InitializeValues(&fDiskImg);
        imf.fFileSource = filename;
        imf.SetQueryDisplayFormat(false);
        imf.SetAllowGenericFormats(false);

        if (imf.DoModal() != IDOK) {
            LOGD("User bailed on IMF dialog");
            result = kResultCancel;
            goto bail;
        }

        if (imf.fSectorOrder != fDiskImg.GetSectorOrder() ||
            imf.fFSFormat != fDiskImg.GetFSFormat())
        {
            LOGI("Initial values overridden, forcing img format");
            dierr = fDiskImg.OverrideFormat(fDiskImg.GetPhysicalFormat(),
                        imf.fFSFormat, imf.fSectorOrder);
            if (dierr != kDIErrNone) {
                result = kResultFailure;
                errMsg.Format(L"Unable to access disk image using selected"
                              L" parameters.  Error: %hs.",
                           DiskImgLib::DIStrError(dierr));
                goto bail;
            }
        }
    }

    if (fDiskImg.GetFSFormat() == DiskImg::kFormatUnknown ||
        fDiskImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        result = kResultFailure;
        errMsg.Format(L"Unable to identify filesystem on '%ls'", filename);
        goto bail;
    }

    /* create an appropriate DiskFS object */
    fpPrimaryDiskFS = fDiskImg.OpenAppropriateDiskFS();
    if (fpPrimaryDiskFS == NULL) {
        /* unknown FS should've been caught above! */
        ASSERT(false);
        result = kResultFailure;
        errMsg.Format(L"Format of '%ls' not recognized.", filename);
        goto bail;
    }

    fpPrimaryDiskFS->SetScanForSubVolumes(DiskFS::kScanSubEnabled);

    /*
     * Scan all files and on the disk image, and recursively descend into
     * sub-volumes.  Can be slow on physical volumes.
     *
     * This is really only useful for ProDOS and HFS disks.  Nothing else
     * can be large enough to really get slow, and nothing else is likely
     * to show up in a large multi-partition image.
     *
     * THOUGHT: only show the dialog if the volume is over a certain size.
     */
    {
        MainWindow* pMain = GET_MAIN_WINDOW();
        ProgressCounterDialog* pProgress;

        pProgress = new ProgressCounterDialog;
        pProgress->Create(L"Examining contents, please wait...", pMain);
        pProgress->SetCounterFormat(L"Scanning...");
        pProgress->CenterWindow();
        //pMain->PeekAndPump(); // redraw
        CWaitCursor waitc;

        /* set up progress dialog and scan all files */
        pMain->SetProgressCounterDialog(pProgress);
        fDiskImg.SetScanProgressCallback(ScanProgressCallback, this);

        dierr = fpPrimaryDiskFS->Initialize(&fDiskImg, DiskFS::kInitFull);

        fDiskImg.SetScanProgressCallback(NULL, NULL);
        pMain->SetProgressCounterDialog(NULL);
        pProgress->DestroyWindow();

        if (dierr != kDIErrNone) {
            if (dierr == kDIErrCancelled) {
                result = kResultCancel;
            } else {
                result = kResultFailure;
                errMsg.Format(L"Error reading list of files from disk: %hs",
                    DiskImgLib::DIStrError(dierr));
            }
            goto bail;
        }
    }

    if (LoadContents() != 0) {
        result = kResultFailure;
        errMsg = L"Failed while loading contents of disk image.";
        goto bail;
    }

    /*
     * Force read-only flag if underlying FS doesn't allow RW.  We need to
     * consider embedded filesystems, so we only set RO if none of the
     * filesystems are writable.
     *
     * BUG: this only checks the first level.  Should be fully recursive.
     */
    if (!fpPrimaryDiskFS->GetReadWriteSupported()) {
        const DiskFS::SubVolume* pSubVol;

        fIsReadOnly = true;
        pSubVol = fpPrimaryDiskFS->GetNextSubVolume(NULL);
        while (pSubVol != NULL) {
            if (pSubVol->GetDiskFS()->GetReadWriteSupported()) {
                fIsReadOnly = false;
                break;
            }

            pSubVol = fpPrimaryDiskFS->GetNextSubVolume(pSubVol);
        }
    }

    /* force read-only if the primary is damaged */
    if (fpPrimaryDiskFS->GetFSDamaged())
        fIsReadOnly = true;
    /* force read-only if the DiskImg thinks a wrapper is damaged */
    if (fpPrimaryDiskFS->GetDiskImg()->GetReadOnly())
        fIsReadOnly = true;

//  /* force read-only on .gz/.zip unless pref allows */
//  if (fDiskImg.GetOuterFormat() != DiskImg::kOuterFormatNone) {
//      if (pPreferences->GetPrefBool(kPrWriteZips) == 0)
//          fIsReadOnly = true;
//  }

    SetPathName(filename);
    result = kResultSuccess;

    /* set any preferences-based settings */
    PreferencesChanged();

bail:
    *pErrMsg = errMsg;
    if (!errMsg.IsEmpty()) {
        assert(result == kResultFailure);
        delete fpPrimaryDiskFS;
        fpPrimaryDiskFS = NULL;
    } else {
        assert(result != kResultFailure);
    }
    return result;
}

CString DiskArchive::New(const WCHAR* fileName, const void* vOptions)
{
    const Preferences* pPreferences = GET_PREFERENCES();
    NewOptions* pOptions = (NewOptions*) vOptions;
    CString volName;
    CStringA volNameA, fileNameA;
    long numBlocks = -1;
    long numTracks = -1;
    int numSectors;
    CString retmsg;
    DIError dierr;
    bool allowLowerCase;

    ASSERT(fileName != NULL);
    ASSERT(pOptions != NULL);

    allowLowerCase = pPreferences->GetPrefBool(kPrProDOSAllowLower) != 0;

    switch (pOptions->base.format) {
    case DiskImg::kFormatUnknown:
        numBlocks = pOptions->blank.numBlocks;
        break;
    case DiskImg::kFormatProDOS:
        volName = pOptions->prodos.volName;
        numBlocks = pOptions->prodos.numBlocks;
        break;
    case DiskImg::kFormatPascal:
        volName = pOptions->pascalfs.volName;
        numBlocks = pOptions->pascalfs.numBlocks;
        break;
    case DiskImg::kFormatMacHFS:
        volName = pOptions->hfs.volName;
        numBlocks = pOptions->hfs.numBlocks;
        break;
    case DiskImg::kFormatDOS32:
        numTracks = pOptions->dos.numTracks;
        numSectors = pOptions->dos.numSectors;

        if (numTracks < DiskFSDOS33::kMinTracks ||
            numTracks > DiskFSDOS33::kMaxTracks)
        {
            retmsg = L"Invalid DOS32 track count";
            goto bail;
        }
        if (numSectors != 13) {
            retmsg = L"Invalid DOS32 sector count";
            goto bail;
        }
        if (pOptions->dos.allocDOSTracks)
            volName = L"DOS";
        break;
    case DiskImg::kFormatDOS33:
        numTracks = pOptions->dos.numTracks;
        numSectors = pOptions->dos.numSectors;

        if (numTracks < DiskFSDOS33::kMinTracks ||
            numTracks > DiskFSDOS33::kMaxTracks)
        {
            retmsg = L"Invalid DOS33 track count";
            goto bail;
        }
        if (numSectors != 16 && numSectors != 32) {     // no 13-sector (yet)
            retmsg = L"Invalid DOS33 sector count";
            goto bail;
        }
        if (pOptions->dos.allocDOSTracks)
            volName = L"DOS";
        break;
    default:
        retmsg = L"Unsupported disk format";
        goto bail;
    }

    LOGI("DiskArchive: new '%ls' %ld %hs in '%ls'",
        (LPCWSTR) volName, numBlocks,
        DiskImg::ToString(pOptions->base.format), fileName);

    bool canSkipFormat;
    if (IsWin9x())
        canSkipFormat = false;
    else
        canSkipFormat = true;

    /*
     * Create an image with the appropriate characteristics.  We set
     * "skipFormat" because we know this will be a brand-new file, and
     * we're not currently creating nibble images.
     *
     * GLITCH: under Win98/ME, brand-new files contain the previous contents
     * of the hard drive.  We need to explicitly zero them out.  We don't
     * want to do it under Win2K/XP because it can be slow for larger
     * volumes.
     */
    fileNameA = fileName;   // TODO(Unicode)
    if (numBlocks > 0) {
        dierr = fDiskImg.CreateImage(fileNameA, NULL,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatSectors,
                    NULL,
                    pOptions->base.sectorOrder,
                    DiskImg::kFormatGenericProDOSOrd,   // arg must be generic
                    numBlocks,
                    canSkipFormat);
    } else {
        ASSERT(numTracks > 0);
        dierr = fDiskImg.CreateImage(fileNameA, NULL,
                    DiskImg::kOuterFormatNone,
                    DiskImg::kFileFormatUnadorned,
                    DiskImg::kPhysicalFormatSectors,
                    NULL,
                    pOptions->base.sectorOrder,
                    DiskImg::kFormatGenericProDOSOrd,   // arg must be generic
                    numTracks, numSectors,
                    canSkipFormat);
    }
    if (dierr != kDIErrNone) {
        retmsg.Format(L"Unable to create disk image: %hs.",
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    if (pOptions->base.format == DiskImg::kFormatUnknown)
        goto skip_format;

    if (pOptions->base.format == DiskImg::kFormatDOS33 ||
        pOptions->base.format == DiskImg::kFormatDOS32)
        fDiskImg.SetDOSVolumeNum(pOptions->dos.volumeNum);

    /*
     * If we don't allow lower case in ProDOS filenames, don't allow them
     * in volume names either.  This works because we don't allow ' ' in
     * volume names; otherwise we'd need to invoke a ProDOS-specific call
     * to convert the ' ' to '.'.  (Or we could just do it ourselves.)
     *
     * We can't ask the ProDOS DiskFS to force upper case for us because
     * the ProDOS DiskFS object doesn't yet exist.
     */
    if (pOptions->base.format == DiskImg::kFormatProDOS && !allowLowerCase)
        volName.MakeUpper();

    /* format it */
    // TODO(Unicode): for HFS, we need to convert Unicode to MOR
    volNameA = volName;
    dierr = fDiskImg.FormatImage(pOptions->base.format, volNameA);
    if (dierr != kDIErrNone) {
        retmsg.Format(L"Unable to format disk image: %hs.",
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }
    fpPrimaryDiskFS = fDiskImg.OpenAppropriateDiskFS(false);
    if (fpPrimaryDiskFS == NULL) {
        retmsg = L"Unable to create DiskFS.";
        goto bail;
    }

    /* prep it */
    dierr = fpPrimaryDiskFS->Initialize(&fDiskImg, DiskFS::kInitFull);
    if (dierr != kDIErrNone) {
        retmsg.Format(L"Error reading list of files from disk: %hs",
            DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    /* this is pretty meaningless, but do it to ensure we're initialized */
    if (LoadContents() != 0) {
        retmsg = L"Failed while loading contents of disk image.";
        goto bail;
    }

skip_format:
    SetPathName(fileName);

    /* set any preferences-based settings */
    PreferencesChanged();

bail:
    return retmsg;
}

CString DiskArchive::Close(void)
{
    if (fpPrimaryDiskFS != NULL) {
        LOGI("DiskArchive shutdown closing disk image");
        delete fpPrimaryDiskFS;
        fpPrimaryDiskFS = NULL;
    }

    DIError dierr;
    dierr = fDiskImg.CloseImage();
    if (dierr != kDIErrNone) {
        MainWindow* pMainWin = (MainWindow*)::AfxGetMainWnd();
        CString msg, failed;

        msg.Format(L"Failed while closing disk image: %hs.",
            DiskImgLib::DIStrError(dierr));
        CheckedLoadString(&failed, IDS_FAILED);
        LOGE("During close: %ls", (LPCWSTR) msg);

        pMainWin->MessageBox(msg, failed, MB_OK);
    }

    return L"";
}

CString DiskArchive::Flush(void)
{
    DIError dierr;
    CWaitCursor waitc;

    assert(fpPrimaryDiskFS != NULL);

    dierr = fpPrimaryDiskFS->Flush(DiskImg::kFlushAll);
    if (dierr != kDIErrNone) {
        CString errMsg;

        errMsg.Format(L"Attempt to flush the current archive failed: %hs.",
            DiskImgLib::DIStrError(dierr));
        return errMsg;
    }

    return L"";
}

bool DiskArchive::IsModified(void) const
{
    assert(fpPrimaryDiskFS != NULL);

    return fpPrimaryDiskFS->GetDiskImg()->GetDirtyFlag();
}

CString DiskArchive::GetDescription() const
{
    CString str = L"Disk Image";

    if (fpPrimaryDiskFS != NULL && fpPrimaryDiskFS->GetVolumeID() != NULL) {
        CString volumeId(Charset::ConvertMORToUNI(fpPrimaryDiskFS->GetVolumeID()));
        str.Format(L"Disk Image - %ls", (LPCWSTR) volumeId);
    }
    return str;
}

int DiskArchive::LoadContents(void)
{
    int result;

    LOGI("DiskArchive LoadContents");
    ASSERT(fpPrimaryDiskFS != NULL);

    {
        MainWindow* pMain = GET_MAIN_WINDOW();
        ExclusiveModelessDialog* pWaitDlg = new ExclusiveModelessDialog;
        pWaitDlg->Create(IDD_LOADING, pMain);
        pWaitDlg->CenterWindow();
        pMain->PeekAndPump();   // redraw
        CWaitCursor waitc;

        result = LoadDiskFSContents(fpPrimaryDiskFS, L"");

        SET_PROGRESS_COUNTER(-1);

        pWaitDlg->DestroyWindow();
        //pMain->PeekAndPump(); // redraw
    }

    return result;
}

CString DiskArchive::Reload()
{
    fReloadFlag = true;     // tell everybody that cached data is invalid

    (void) fpPrimaryDiskFS->Flush(DiskImg::kFlushFastOnly);

    DeleteEntries();        // a GenericArchive operation

    if (LoadContents() != 0)
        return "Disk image reload failed.";

    return "";
}

int DiskArchive::InternalReload(CWnd* pMsgWnd)
{
    CString errMsg;

    errMsg = Reload();

    if (!errMsg.IsEmpty()) {
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        return -1;
    }

    return 0;
}

int DiskArchive::LoadDiskFSContents(DiskFS* pDiskFS, const WCHAR* volName)
{
    static const char* kBlankFileNameMOR = "<blank filename>";
    A2File* pFile;
    DiskEntry* pNewEntry;
    DiskFS::SubVolume* pSubVol;
    const Preferences* pPreferences = GET_PREFERENCES();
    bool wantCoerceDOSFilenames = false;
    CString ourSubVolName;

    wantCoerceDOSFilenames = pPreferences->GetPrefBool(kPrCoerceDOSFilenames);

    LOGI("Notes for disk image '%ls':", volName);
    LOGI("%hs", pDiskFS->GetDiskImg()->GetNotes());

    ASSERT(pDiskFS != NULL);
    pFile = pDiskFS->GetNextFile(NULL);
    while (pFile != NULL) {
        pNewEntry = new DiskEntry(pFile);
        if (pNewEntry == NULL)
            return -1;

        CStringA path(pFile->GetPathName());
        if (path.IsEmpty())
            path = kBlankFileNameMOR;
        if (DiskImg::UsesDOSFileStructure(pFile->GetFSFormat()) &&
            wantCoerceDOSFilenames)
        {
            InjectLowercase(&path);
        }
        pNewEntry->SetPathNameMOR(path);
        if (volName[0] != '\0')
            pNewEntry->SetSubVolName(volName);
        pNewEntry->SetFssep(pFile->GetFssep());
        pNewEntry->SetFileType(pFile->GetFileType());
        pNewEntry->SetAuxType(pFile->GetAuxType());
        pNewEntry->SetAccess(pFile->GetAccess());
        if (pFile->GetCreateWhen() == 0)
            pNewEntry->SetCreateWhen(kDateNone);
        else
            pNewEntry->SetCreateWhen(pFile->GetCreateWhen());
        if (pFile->GetModWhen() == 0)
            pNewEntry->SetModWhen(kDateNone);
        else
            pNewEntry->SetModWhen(pFile->GetModWhen());
        pNewEntry->SetSourceFS(pFile->GetFSFormat());
        pNewEntry->SetHasDataFork(true);
        if (pFile->IsVolumeDirectory()) {
            /* volume directory entry; only on ProDOS/HFS */
            ASSERT(pFile->GetRsrcLength() < 0);
            pNewEntry->SetRecordKind(GenericEntry::kRecordKindVolumeDir);
            //pNewEntry->SetUncompressedLen(pFile->GetDataLength());
            pNewEntry->SetDataForkLen(pFile->GetDataLength());
            pNewEntry->SetCompressedLen(pFile->GetDataLength());
        } else if (pFile->IsDirectory()) {
            /* directory entry */
            ASSERT(pFile->GetRsrcLength() < 0);
            pNewEntry->SetRecordKind(GenericEntry::kRecordKindDirectory);
            //pNewEntry->SetUncompressedLen(pFile->GetDataLength());
            pNewEntry->SetDataForkLen(pFile->GetDataLength());
            pNewEntry->SetCompressedLen(pFile->GetDataLength());
        } else if (pFile->GetRsrcLength() >= 0) {
            /* has resource fork */
            pNewEntry->SetRecordKind(GenericEntry::kRecordKindForkedFile);
            pNewEntry->SetDataForkLen(pFile->GetDataLength());
            pNewEntry->SetRsrcForkLen(pFile->GetRsrcLength());
            //pNewEntry->SetUncompressedLen(
            //      pFile->GetDataLength() + pFile->GetRsrcLength() );
            pNewEntry->SetCompressedLen(
                    pFile->GetDataSparseLength() + pFile->GetRsrcSparseLength() );
            pNewEntry->SetHasRsrcFork(true);
        } else {
            /* just data fork */
            pNewEntry->SetRecordKind(GenericEntry::kRecordKindFile);
            //pNewEntry->SetUncompressedLen(pFile->GetDataLength());
            pNewEntry->SetDataForkLen(pFile->GetDataLength());
            pNewEntry->SetCompressedLen(pFile->GetDataSparseLength());
        }

        switch (pNewEntry->GetSourceFS()) {
        case DiskImg::kFormatDOS33:
        case DiskImg::kFormatDOS32:
        case DiskImg::kFormatUNIDOS:
            pNewEntry->SetFormatStr(L"DOS");
            break;
        case DiskImg::kFormatGutenberg:
            pNewEntry->SetFormatStr(L"Gutenb");
            break;
        case DiskImg::kFormatProDOS:
            pNewEntry->SetFormatStr(L"ProDOS");
            break;
        case DiskImg::kFormatPascal:
            pNewEntry->SetFormatStr(L"Pascal");
            break;
        case DiskImg::kFormatCPM:
            pNewEntry->SetFormatStr(L"CP/M");
            break;
        case DiskImg::kFormatMSDOS:
            pNewEntry->SetFormatStr(L"MS-DOS");
            break;
        case DiskImg::kFormatRDOS33:
        case DiskImg::kFormatRDOS32:
        case DiskImg::kFormatRDOS3:
            pNewEntry->SetFormatStr(L"RDOS");
            break;
        case DiskImg::kFormatMacHFS:
            pNewEntry->SetFormatStr(L"HFS");
            break;
        default:
            pNewEntry->SetFormatStr(L"???");
            break;
        }

        pNewEntry->SetDamaged(pFile->GetQuality() == A2File::kQualityDamaged);
        pNewEntry->SetSuspicious(pFile->GetQuality() == A2File::kQualitySuspicious);

        AddEntry(pNewEntry);

        /* this is not very useful -- all the heavy lifting was done earlier */
        if ((GetNumEntries() % 100) == 0)
            SET_PROGRESS_COUNTER(GetNumEntries());

        pFile = pDiskFS->GetNextFile(pFile);
    }

    /*
     * Load all sub-volumes.
     *
     * We define the sub-volume name to use for the next layer down.  We
     * prepend an underscore to the unmodified name.  So long as the volume
     * name is a valid Windows path -- which should hold true for most disks,
     * though possibly not for Pascal -- it can be extracted directly with
     * its full path with no risk of conflict.  (The extraction code relies
     * on this, so don't put a ':' in the subvol name or Windows will choke.)
     */
    pSubVol = pDiskFS->GetNextSubVolume(NULL);
    while (pSubVol != NULL) {
        CString concatSubVolName;
        int ret;

        const char* subVolNameMOR = pSubVol->GetDiskFS()->GetVolumeName();
        if (subVolNameMOR == NULL) {
            subVolNameMOR = "+++";      // call it *something*
        }
        CString subVolName(Charset::ConvertMORToUNI(subVolNameMOR));

        if (volName[0] == '\0') {
            concatSubVolName.Format(L"_%ls", (LPCWSTR) subVolName);
        } else {
            concatSubVolName.Format(L"%ls_%ls", volName, (LPCWSTR) subVolName);
        }
        ret = LoadDiskFSContents(pSubVol->GetDiskFS(), concatSubVolName);
        if (ret != 0)
            return ret;
        pSubVol = pDiskFS->GetNextSubVolume(pSubVol);
    }

    return 0;
}

void DiskArchive::PreferencesChanged(void)
{
    const Preferences* pPreferences = GET_PREFERENCES();

    if (fpPrimaryDiskFS != NULL) {
        fpPrimaryDiskFS->SetParameter(DiskFS::kParmProDOS_AllowLowerCase,
            pPreferences->GetPrefBool(kPrProDOSAllowLower) != 0);
        fpPrimaryDiskFS->SetParameter(DiskFS::kParmProDOS_AllocSparse,
            pPreferences->GetPrefBool(kPrProDOSUseSparse) != 0);
    }
}

long DiskArchive::GetCapability(Capability cap)
{
    switch (cap) {
    case kCapCanTest:
        return false;
        break;
    case kCapCanRenameFullPath:
        return false;
        break;
    case kCapCanRecompress:
        return false;
        break;
    case kCapCanEditComment:
        return false;
        break;
    case kCapCanAddDisk:
        return false;
        break;
    case kCapCanConvEOLOnAdd:
        return true;
        break;
    case kCapCanCreateSubdir:
        return true;
        break;
    case kCapCanRenameVolume:
        return true;
        break;
    default:
        ASSERT(false);
        return -1;
        break;
    }
}


/*
 * ===========================================================================
 *      DiskArchive -- add files
 * ===========================================================================
 */

bool DiskArchive::BulkAdd(ActionProgressDialog* pActionProgress,
    const AddFilesDialog* pAddOpts)
{
    NuError nerr;
    CString errMsg;
    WCHAR curDir[MAX_PATH] = L"";
    bool retVal = false;

    LOGD("Opts: '%ls' typePres=%d", (LPCWSTR) pAddOpts->fStoragePrefix,
        pAddOpts->fTypePreservation);
    LOGD("      sub=%d strip=%d ovwr=%d",
        pAddOpts->fIncludeSubfolders, pAddOpts->fStripFolderNames,
        pAddOpts->fOverwriteExisting);

    ASSERT(fpAddDataHead == NULL);

    /* these reset on every new add */
    fOverwriteExisting = false;
    fOverwriteNoAsk = false;

    /* we screen for clashes with existing files later; this just ensures
       "batch uniqueness" */
    fpPrimaryDiskFS->SetParameter(DiskFS::kParm_CreateUnique, true);

    /*
     * Save the current directory and change to the one from the file dialog.
     */
    const CString& directory = pAddOpts->GetDirectory();
    LOGI("Selected path = '%ls'", (LPCWSTR) directory);

    if (GetCurrentDirectory(NELEM(curDir), curDir) == 0) {
        errMsg = L"Unable to get current directory.\n";
        ShowFailureMsg(pActionProgress, errMsg, IDS_FAILED);
        goto bail;
    }
    if (SetCurrentDirectory(directory) == false) {
        errMsg.Format(L"Unable to set current directory to '%ls'.\n",
            (LPCWSTR) directory);
        ShowFailureMsg(pActionProgress, errMsg, IDS_FAILED);
        goto bail;
    }

    const CStringArray& fileNames = pAddOpts->GetFileNames();
    for (int i = 0; i < fileNames.GetCount(); i++) {
        const CString& name = fileNames.GetAt(i);
        LOGI("  file '%ls'", (LPCWSTR) name);

        /* add the file, calling DoAddFile via the generic AddFile */
        nerr = AddFile(pAddOpts, name, &errMsg);
        if (nerr != kNuErrNone) {
            if (errMsg.IsEmpty())
                errMsg.Format(L"Failed while adding file '%ls': %hs.",
                    (LPCWSTR) name, NuStrError(nerr));
            if (nerr != kNuErrAborted) {
                ShowFailureMsg(pActionProgress, errMsg, IDS_FAILED);
            }
            goto bail;
        }
    }

    if (fpAddDataHead == NULL) {
        CString title;
        CheckedLoadString(&title, IDS_MB_APP_NAME);
        errMsg = L"No files added.\n";
        pActionProgress->MessageBox(errMsg, title, MB_OK | MB_ICONWARNING);
    } else {
        /* add all pending files */
        retVal = true;
        errMsg = ProcessFileAddData(pAddOpts->fpTargetDiskFS,
                    pAddOpts->fConvEOL);
        if (!errMsg.IsEmpty()) {
            CString title;
            ShowFailureMsg(pActionProgress, errMsg, IDS_FAILED);
            retVal = false;
        }

        /* success or failure, reload the contents */
        errMsg = Reload();
        if (!errMsg.IsEmpty())
            retVal = false;
    }

bail:
    FreeAddDataList();
    if (SetCurrentDirectory(curDir) == false) {
        errMsg.Format(L"Unable to reset current directory to '%ls'.\n", curDir);
        ShowFailureMsg(pActionProgress, errMsg, IDS_FAILED);
        // bummer, but don't signal failure
    }
    return retVal;
}

NuError DiskArchive::DoAddFile(const AddFilesDialog* pAddOpts,
    LocalFileDetails* pDetails)
{
    /*
     * Add a file to a disk image  Unfortunately we can't just add the files
     * here.  We need to figure  which pairs of files should be combined into
     * a single "extended" file (yes, the cursed forked files strike again).
     *
     * The way you tell if two files should be one is by comparing their
     * filenames and type info.  If they match, and one is a data fork and
     * one is a resource fork, we have a single split file.
     *
     * We have to be careful here because we don't know which will be seen
     * first and whether they'll be adjacent.  We have to dig through the
     * list of previously-added files for a match (O(n^2) behavior currently).
     *
     * We also have to compare the right filename.  Comparing the Windows
     * filename is a bad idea, because by definition one of them has a resource
     * fork tag on it.  We need to compare the normalized filename before
     * the ProDOS normalizer/uniqifier gets a chance to mangle it.  As luck
     * would have it, that's exactly what we have in "storageName".
     *
     * For a NuFX archive, NufxLib does all this nonsense for us, but we have
     * to manage it ourselves here.  The good news is that, since we have to
     * wade through all the filenames, we have an opportunity to make the names
     * unique.  So long as we ensure that the names we have don't clash with
     * anything currently on the disk, we know that anything we add that does
     * clash is running into something we just added, which means we can turn
     * on CreateFile's "make unique" feature and let the filesystem-specific
     * code handle uniqueness.
     *
     * Any fields we want to keep from the FileDetails struct need to be
     * copied out.  It's a "hairy" struct, so we need to duplicate the strings.
     */
    NuError nuerr = kNuErrNone;
    DiskFS* pDiskFS = pAddOpts->fpTargetDiskFS;

    DIError dierr;
    int neededLen = 64;     // reasonable guess
    char* fsNormalBuf = NULL;    // name as it will appear on disk image

    LOGI("  +++ ADD file: orig='%ls' strip='%ls'",
        (LPCWSTR) pDetails->GetLocalPathName(),
        (LPCWSTR) pDetails->GetStrippedLocalPathName());

retry:
    /*
     * Convert "storageName" to a filesystem-normalized path.
     */
    delete[] fsNormalBuf;
    fsNormalBuf = new char[neededLen];
    dierr = pDiskFS->NormalizePath(pDetails->GetStoragePathNameMOR(),
                PathProposal::kDefaultStoredFssep, fsNormalBuf, &neededLen);
    if (dierr == kDIErrDataOverrun) {
        /* not long enough, try again *once* */
        delete[] fsNormalBuf;
        fsNormalBuf = new char[neededLen];
        dierr = pDiskFS->NormalizePath(pDetails->GetStoragePathNameMOR(),
                    PathProposal::kDefaultStoredFssep, fsNormalBuf, &neededLen);
    }
    if (dierr != kDIErrNone) {
        nuerr = kNuErrInternal;
        goto bail;
    }

    /*
     * Test to see if the file already exists.  If it does, give the user
     * the opportunity to rename it, overwrite the original, or skip
     * adding it.
     *
     * The FS-normalized path may not reflect the actual storage name,
     * because some features (like ProDOS "allow lower case") aren't
     * factored in until later.  However, it should be close enough -- it
     * has to be, or we'd be in trouble for saying it's going to overwrite
     * the file in the archive.
     */
    A2File* pExisting;
    pExisting = pDiskFS->GetFileByName(fsNormalBuf);
    if (pExisting != NULL) {
        NuResult result;

        result = HandleReplaceExisting(pExisting, pDetails);
        if (result == kNuAbort) {
            nuerr = kNuErrAborted;
            goto bail;
        } else if (result == kNuSkip) {
            nuerr = kNuErrSkipped;
            goto bail;
        } else if (result == kNuRename) {
            goto retry;
        } else if (result == kNuOverwrite) {
            /* delete the existing file immediately */
            LOGD(" Deleting existing file '%hs'", fsNormalBuf);
            dierr = pDiskFS->DeleteFile(pExisting);
            if (dierr != kDIErrNone) {
                // Would be nice to show a dialog and explain *why*, but
                // I'm not sure we have a window here.
                LOGE("  Deletion failed (err=%d)", dierr);
                goto bail;
            }
        } else {
            LOGE("GLITCH: bad return %d from HandleReplaceExisting",result);
            assert(false);
            nuerr = kNuErrInternal;
            goto bail;
        }
    }

    /*
     * Put all the goodies into a new FileAddData object, and add it to
     * the end of the list.
     */
    FileAddData* pAddData;
    pAddData = new FileAddData(pDetails, fsNormalBuf);
    if (pAddData == NULL) {
        nuerr = kNuErrMalloc;
        goto bail;
    }

    LOGD("FSNormalized is '%hs'", pAddData->GetFSNormalPath());

    AddToAddDataList(pAddData);

bail:
    delete[] fsNormalBuf;
    return nuerr;
}

NuResult DiskArchive::HandleReplaceExisting(const A2File* pExisting,
    LocalFileDetails* pDetails)
{
    NuResult result;

    if (fOverwriteNoAsk) {
        if (fOverwriteExisting)
            return kNuOverwrite;
        else
            return kNuSkip;
    }

    ConfirmOverwriteDialog confOvwr;

    confOvwr.fExistingFile = Charset::ConvertMORToUNI(pExisting->GetPathName());
    confOvwr.fExistingFileModWhen = pExisting->GetModWhen();

    PathName srcPath(pDetails->GetLocalPathName());
    confOvwr.fNewFileSource = pDetails->GetLocalPathName();
    confOvwr.fNewFileModWhen = srcPath.GetModWhen();

    if (confOvwr.DoModal() == IDCANCEL) {
        LOGI("User cancelled out of add-to-diskimg replace-existing");
        return kNuAbort;
    }

    // TODO: if they rename one fork, we need to track that fact and
    // carry it over to the other fork -- otherwise they can rename
    // the data and resource forks into separate files.
    if (confOvwr.fResultRename) {
        /*
         * Replace the name in FileDetails.  They were asked to modify
         * the already-normalized version of the filename.  We will run
         * it back through the FS-specific normalizer, which will handle
         * any oddities they type in.
         *
         * We don't want to run it through PathProposal.LocalToArchive
         * because that'll strip out ':' in the pathnames.
         *
         * Ideally the rename dialog would have a way to validate the
         * full path and reject "OK" if it's not valid.  Instead, we just
         * allow the FS normalizer to force the filename to be valid.
         */
        pDetails->SetStrippedLocalPathName(confOvwr.fExistingFile);
        LOGI("Trying rename to '%ls'",
            (LPCWSTR) pDetails->GetStrippedLocalPathName());
        return kNuRename;
    }

    if (confOvwr.fResultApplyToAll) {
            fOverwriteNoAsk = true;
        if (confOvwr.fResultOverwrite)
            fOverwriteExisting = true;
        else
            fOverwriteExisting = false;
    }
    if (confOvwr.fResultOverwrite)
        result = kNuOverwrite;
    else
        result = kNuSkip;

    return result;
}

CString DiskArchive::ProcessFileAddData(DiskFS* pDiskFS, int addOptsConvEOL)
{
    CString errMsg;
    FileAddData* pData;
    unsigned char* dataBuf = NULL;
    unsigned char* rsrcBuf = NULL;
    long dataLen, rsrcLen;
    MainWindow* pMainWin = (MainWindow*)::AfxGetMainWnd();

    LOGI("--- ProcessFileAddData");

    /* map the EOL conversion to something we can use */
    GenericEntry::ConvertEOL convEOL;

    switch (addOptsConvEOL) {
    case AddFilesDialog::kConvEOLNone:
        convEOL = GenericEntry::kConvertEOLOff;
        break;
    case AddFilesDialog::kConvEOLType:
        // will be adjusted each time through the loop
        convEOL = GenericEntry::kConvertEOLOff;
        break;
    case AddFilesDialog::kConvEOLAuto:
        convEOL = GenericEntry::kConvertEOLAuto;
        break;
    case AddFilesDialog::kConvEOLAll:
        convEOL = GenericEntry::kConvertEOLOn;
        break;
    default:
        assert(false);
        convEOL = GenericEntry::kConvertEOLOff;
        break;
    }


    pData = fpAddDataHead;
    while (pData != NULL) {
        const LocalFileDetails* pDataDetails = NULL;
        const LocalFileDetails* pRsrcDetails = NULL;
        const LocalFileDetails* pDetails = pData->GetDetails();
        const char* typeStr = "????";   // for debug msg only

        switch (pDetails->GetEntryKind()) {
        case LocalFileDetails::kFileKindDataFork:
            pDataDetails = pDetails;
            typeStr = "data";
            break;
        case LocalFileDetails::kFileKindRsrcFork:
            pRsrcDetails = pDetails;
            typeStr = "rsrc";
            break;
        case LocalFileDetails::kFileKindDiskImage:
            pDataDetails = pDetails;
            typeStr = "disk";
            break;
        case LocalFileDetails::kFileKindBothForks:
        case LocalFileDetails::kFileKindDirectory:
        default:
            assert(false);
            return L"internal error";
        }

        if (pData->GetOtherFork() != NULL) {
            pDetails = pData->GetOtherFork()->GetDetails();
            typeStr = "both";

            switch (pDetails->GetEntryKind()) {
            case LocalFileDetails::kFileKindDataFork:
                assert(pDataDetails == NULL);
                pDataDetails = pDetails;
                break;
            case LocalFileDetails::kFileKindRsrcFork:
                assert(pRsrcDetails == NULL);
                pRsrcDetails = pDetails;
                break;
            case LocalFileDetails::kFileKindDiskImage:
                assert(false);
                return L"(internal) add other disk error";
            case LocalFileDetails::kFileKindBothForks:
            case LocalFileDetails::kFileKindDirectory:
            default:
                assert(false);
                return L"internal error";
            }
        }

        LOGI("Adding file '%ls' (%hs)",
            (LPCWSTR) pDetails->GetStrippedLocalPathName(), typeStr);
        ASSERT(pDataDetails != NULL || pRsrcDetails != NULL);

        /*
         * The current implementation of DiskImg/DiskFS requires writing each
         * fork in one shot.  This means loading the entire thing into
         * memory.  Not so bad for ProDOS, with its 16MB maximum file size,
         * but it could be awkward for HFS (not to mention HFS Plus!).
         */
        DiskFS::CreateParms parms;
        /* use the FS-normalized path here */
        /* (do we have to? do we want to?) */
        parms.pathName = pData->GetFSNormalPath();
        if (pRsrcDetails != NULL)
            parms.storageType = kNuStorageExtended;
        else
            parms.storageType = kNuStorageSeedling;
        /* copy the rest out of the LocalFileDetails */
        parms.fssep = pDetails->GetFssep();
        parms.fileType = pDetails->GetFileType();
        parms.auxType = pDetails->GetExtraType();
        parms.access = pDetails->GetAccess();
        parms.createWhen = NufxArchive::DateTimeToSeconds(&pDetails->GetCreateWhen());
        parms.modWhen = NufxArchive::DateTimeToSeconds(&pDetails->GetModWhen());

        dataLen = rsrcLen = -1;
        if (pDataDetails != NULL) {
            /* figure out text conversion, including high ASCII for DOS */
            /* (HA conversion only happens if text conversion happens)  */
            GenericEntry::ConvertHighASCII convHA;
            if (addOptsConvEOL == AddFilesDialog::kConvEOLType) {
                if (pDataDetails->GetFileType() == kFileTypeTXT ||
                    pDataDetails->GetFileType() == kFileTypeSRC)
                {
                    LOGI("Enabling text conversion by type");
                    convEOL = GenericEntry::kConvertEOLOn;
                } else {
                    convEOL = GenericEntry::kConvertEOLOff;
                }
            }
            if (DiskImg::UsesDOSFileStructure(pDiskFS->GetDiskImg()->GetFSFormat()))
                convHA = GenericEntry::kConvertHAOn;
            else
                convHA = GenericEntry::kConvertHAOff;

            errMsg = LoadFile(pDataDetails->GetLocalPathName(), &dataBuf, &dataLen,
                convEOL, convHA);
            if (!errMsg.IsEmpty())
                goto bail;
        }
        if (pRsrcDetails != NULL) {
            /* no text conversion on resource forks */
            errMsg = LoadFile(pRsrcDetails->GetLocalPathName(), &rsrcBuf, &rsrcLen,
                GenericEntry::kConvertEOLOff, GenericEntry::kConvertHAOff);
            if (!errMsg.IsEmpty())
                goto bail;
        }

        /* really ought to do this separately for each thread */
        SET_PROGRESS_BEGIN();
        CString pathNameW(Charset::ConvertMORToUNI(parms.pathName));
        SET_PROGRESS_UPDATE2(0, pDetails->GetLocalPathName(), pathNameW);

        DIError dierr;
        dierr = AddForksToDisk(pDiskFS, &parms, dataBuf, dataLen,
                    rsrcBuf, rsrcLen);
        SET_PROGRESS_END();
        if (dierr != kDIErrNone) {
            errMsg.Format(L"Unable to add '%ls' to image: %hs.",
                (LPCWSTR) pathNameW, DiskImgLib::DIStrError(dierr));
            goto bail;
        }
        delete[] dataBuf;
        delete[] rsrcBuf;
        dataBuf = rsrcBuf = NULL;

        pData = pData->GetNext();
    }

bail:
    delete[] dataBuf;
    delete[] rsrcBuf;
    return errMsg;
}


// TODO: really ought to update the progress counter, especially when reading
// really large files.
CString DiskArchive::LoadFile(const WCHAR* pathName, uint8_t** pBuf, long* pLen,
    GenericEntry::ConvertEOL conv, GenericEntry::ConvertHighASCII convHA) const
{
    const char kCharLF = '\n';
    const char kCharCR = '\r';
    CString errMsg;
    FILE* fp;
    long fileLen;

    ASSERT(convHA == GenericEntry::kConvertHAOn ||
           convHA == GenericEntry::kConvertHAOff);
    ASSERT(conv == GenericEntry::kConvertEOLOn ||
           conv == GenericEntry::kConvertEOLOff ||
           conv == GenericEntry::kConvertEOLAuto);
    ASSERT(pathName != NULL);
    ASSERT(pBuf != NULL);
    ASSERT(pLen != NULL);

    fp = _wfopen(pathName, L"rb");
    if (fp == NULL) {
        errMsg.Format(L"Unable to open '%ls': %hs.", pathName,
            strerror(errno));
        goto bail;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        errMsg.Format(L"Unable to seek to end of '%ls': %hs", pathName,
            strerror(errno));
        goto bail;
    }
    fileLen = ftell(fp);
    if (fileLen < 0) {
        errMsg.Format(L"Unable to determine length of '%ls': %hs", pathName,
            strerror(errno));
        goto bail;
    }
    rewind(fp);

    if (fileLen == 0) {     // handle zero-length files
        *pBuf = NULL;
        *pLen = 0;
        goto bail;
    } else if (fileLen > 0x00ffffff) {
        errMsg = L"Cannot add files larger than 16MB to a disk image.";
        goto bail;
    }

    *pBuf = new uint8_t[fileLen];
    if (*pBuf == NULL) {
        errMsg.Format(L"Unable to allocate %ld bytes for '%ls'.",
            fileLen, pathName);
        goto bail;
    }

    /*
     * We're ready to load the file.  We need to sort out EOL conversion.
     * Since we always convert to CR, we know the file will stay the same
     * size or get smaller, which means the buffer we've allocated is
     * guaranteed to hold the file even if we convert it.
     *
     * If the text mode is "auto", we need to load a piece of the file and
     * analyze it.
     */
    if (conv == GenericEntry::kConvertEOLAuto) {
        GenericEntry::EOLType eolType;
        GenericEntry::ConvertHighASCII dummy;

        int chunkLen = 16384;       // nice big chunk
        if (chunkLen > fileLen)
            chunkLen = fileLen;

        if (fread(*pBuf, chunkLen, 1, fp) != 1) {
            errMsg.Format(L"Unable to read initial chunk of '%ls': %hs.",
                pathName, strerror(errno));
            delete[] *pBuf;
            *pBuf = NULL;
            goto bail;
        }
        rewind(fp);

        conv = GenericEntry::DetermineConversion(*pBuf, chunkLen,
                    &eolType, &dummy);
        LOGI("LoadFile DetermineConv returned conv=%d eolType=%d",
            conv, eolType);
        if (conv == GenericEntry::kConvertEOLOn &&
            eolType == GenericEntry::kEOLCR)
        {
            LOGI("  (skipping conversion due to matching eolType)");
            conv = GenericEntry::kConvertEOLOff;
        }
    }
    ASSERT(conv != GenericEntry::kConvertEOLAuto);

    /*
     * The "high ASCII" conversion is either on or off.  In this context,
     * "on" means "convert all text files", and "off" means "don't convert
     * text files".  We never convert non-text files.  Conversion should
     * always be "on" for DOS 3.2/3.3, and "off" for everything else (except
     * RDOS, should we choose to make that writeable).
     */
    if (conv == GenericEntry::kConvertEOLOff) {
        /* fast path */
        LOGI("  +++ NOT converting text '%ls'", pathName);
        if (fread(*pBuf, fileLen, 1, fp) != 1) {
            errMsg.Format(L"Unable to read '%ls': %hs.", pathName, strerror(errno));
            delete[] *pBuf;
            *pBuf = NULL;
            goto bail;
        }
    } else {
        /*
         * Convert as we go.
         *
         * Observation: if we copy a binary file to a DOS disk, and force
         * the text conversion, we will convert 0x0a to 0x0d, and thence
         * to 0x8d.  However, we may still have some 0x8a bytes lying around,
         * because we don't convert 0x8a in the original file to anything.
         * This means that a CR->CRLF or LF->CRLF conversion can't be
         * "undone" on a DOS disk.  (Not that anyone cares.)
         */
        long count = fileLen;
        int mask, ic;
        bool lastCR = false;
        unsigned char* buf = *pBuf;

        if (convHA == GenericEntry::kConvertHAOn)
            mask = 0x80;
        else
            mask = 0x00;

        LOGI("  +++ Converting text '%ls', mask=0x%02x", pathName, mask);

        while (count--) {
            ic = getc(fp);

            if (ic == kCharCR) {
                *buf++ = (unsigned char) (kCharCR | mask);
                lastCR = true;
            } else if (ic == kCharLF) {
                if (!lastCR)
                    *buf++ = (unsigned char) (kCharCR | mask);
                lastCR = false;
            } else {
                if (ic == '\0')
                    *buf++ = (unsigned char) ic;    // don't conv 0x00
                else
                    *buf++ = (unsigned char) (ic | mask);
                lastCR = false;
            }
        }
        fileLen = buf - *pBuf;
    }

    (void) fclose(fp);

    *pLen = fileLen;

bail:
    return errMsg;
}

DIError DiskArchive::AddForksToDisk(DiskFS* pDiskFS,
    const DiskFS::CreateParms* pParms,
    const unsigned char* dataBuf, long dataLen,
    const unsigned char* rsrcBuf, long rsrcLen) const
{
    DIError dierr = kDIErrNone;
    const int kFileTypeBIN = 0x06;
    const int kFileTypeINT = 0xfa;
    const int kFileTypeBAS = 0xfc;
    A2File* pNewFile = NULL;
    A2FileDescr* pOpenFile = NULL;
    DiskFS::CreateParms parmCopy;

    /*
     * Make a temporary copy, pointers and all, so we can rewrite some of
     * the fields.  This is sort of bad, because we're making copies of a
     * const char* filename pointer whose underlying storage we're not
     * really familiar with.  However, so long as we don't try to retain
     * it after this function returns we should be fine.
     *
     * Might be better to make CreateParms a class instead of a struct,
     * make the pathName field new[] storage, and write a copy constructor
     * for the operation below.  This will be fine for now.
     */
    memcpy(&parmCopy, pParms, sizeof(parmCopy));

    if (rsrcLen >= 0) {
        ASSERT(parmCopy.storageType == kNuStorageExtended);
    }

    /*
     * Look for "empty directory holders" that we put into NuFX archives
     * when doing disk-to-archive conversions.  These make no sense if
     * there's no fssep (because it's coming from DOS), or if there's no
     * base path, so we can ignore those cases.  We can also ignore it if
     * the file is forked or is already a directory.
     */
    if (parmCopy.fssep != '\0' && parmCopy.storageType == kNuStorageSeedling) {
        const char* cp;
        cp = strrchr(parmCopy.pathName, parmCopy.fssep);
        if (cp != NULL) {
            if (strcmp(cp+1, kEmptyFolderMarker) == 0 && dataLen == 0) {
                /* drop the junk on the end */
                parmCopy.storageType = kNuStorageDirectory;
                CStringA replacementFileName(parmCopy.pathName);;
                replacementFileName =
                    replacementFileName.Left(cp - parmCopy.pathName -1);
                parmCopy.pathName = replacementFileName;
                parmCopy.fileType = 0x0f;       // DIR
                parmCopy.access &= ~(A2FileProDOS::kAccessInvisible);
                dataLen = -1;
            }
        }
    }

    /*
     * If this is a subdir create request (from the clipboard or an "empty
     * directory placeholder" in a NuFX archive), handle it here.  If we're
     * on a filesystem that doesn't have subdirectories, just skip it.
     */
    if (parmCopy.storageType == kNuStorageDirectory) {
        A2File* pDummyFile;
        ASSERT(dataLen < 0 && rsrcLen < 0);

        if (DiskImg::IsHierarchical(pDiskFS->GetDiskImg()->GetFSFormat())) {
            dierr = pDiskFS->CreateFile(&parmCopy, &pDummyFile);
            if (dierr == kDIErrDirectoryExists)
                dierr = kDIErrNone;     // dirs are not made unique
            goto bail;
        } else {
            LOGI(" Ignoring subdir create req on non-hierarchic FS");
            goto bail;
        }
    }

    /* don't try to put resource forks onto a DOS disk */
    if (!DiskImg::HasResourceForks(pDiskFS->GetDiskImg()->GetFSFormat())) {
        if (rsrcLen >= 0) {
            rsrcLen = -1;
            parmCopy.storageType = kNuStorageSeedling;

            if (dataLen < 0) {
                /* this was a resource-fork-only file */
                LOGI("--- nothing left to write for '%hs'",
                    parmCopy.pathName);
                goto bail;
            }
        } else {
            ASSERT(parmCopy.storageType == kNuStorageSeedling);
        }
    }

    /* quick kluge to get the right file type on large DOS files */
    if (DiskImg::UsesDOSFileStructure(pDiskFS->GetDiskImg()->GetFSFormat()) &&
        dataLen >= 65536)
    {
        if (parmCopy.fileType == kFileTypeBIN ||
            parmCopy.fileType == kFileTypeINT ||
            parmCopy.fileType == kFileTypeBAS)
        {
            LOGI("+++ switching DOS file type to $f2");
            parmCopy.fileType = 0xf2;       // DOS 'S' file
        }
    }

    /*
     * Create the file on the disk.  The storage type determines whether
     * it has data+rsrc forks or just data (there's no such thing in
     * ProDOS as "just a resource fork").  There's no need to open the
     * fork if we're not going to write to it.
     *
     * This holds for resource forks as well, because the storage type
     * determines whether or not the file is forked, and we've asserted
     * that a file with a non-(-1) rsrcLen is forked.
     */
    dierr = pDiskFS->CreateFile(&parmCopy, &pNewFile);
    if (dierr != kDIErrNone) {
        LOGI("  CreateFile failed: %hs", DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    /*
     * Note: if this was an empty directory holder, pNewFile will be set
     * to NULL.  We used to avoid handling this by just not opening the file
     * if it had a length of zero.  However, DOS 3.3 needs to write some
     * kinds of zero-length files, because e.g. a zero-length 'B' file
     * actually has 4 bytes of data in it.
     *
     * Of course, if dataLen is zero then dataBuf is NULL, so we need to
     * supply a dummy write buffer.  None of this is an issue for resource
     * forks, because DOS 3.3 doesn't have those.
     */

    if (dataLen > 0 ||
        (dataLen == 0 && pNewFile != NULL))
    {
        ASSERT(pNewFile != NULL);
        unsigned char dummyBuf[1] = { '\0' };

        dierr = pNewFile->Open(&pOpenFile, false, false);
        if (dierr != kDIErrNone)
            goto bail;

        pOpenFile->SetProgressUpdater(DiskArchive::ProgressCallback,
            dataLen, NULL);

        dierr = pOpenFile->Write(dataBuf != NULL ? dataBuf : dummyBuf, dataLen);
        if (dierr != kDIErrNone)
            goto bail;

        dierr = pOpenFile->Close();
        if (dierr != kDIErrNone)
            goto bail;
        pOpenFile = NULL;
    }

    if (rsrcLen > 0) {
        ASSERT(pNewFile != NULL);

        dierr = pNewFile->Open(&pOpenFile, false, true);
        if (dierr != kDIErrNone)
            goto bail;

        pOpenFile->SetProgressUpdater(DiskArchive::ProgressCallback,
            rsrcLen, NULL);

        dierr = pOpenFile->Write(rsrcBuf, rsrcLen);
        if (dierr != kDIErrNone)
            goto bail;

        dierr = pOpenFile->Close();
        if (dierr != kDIErrNone)
            goto bail;
        pOpenFile = NULL;
    }

bail:
    if (pOpenFile != NULL)
        pOpenFile->Close();
    if (dierr != kDIErrNone && pNewFile != NULL) {
        /*
         * Clean up the partially-written file.  This does not, of course,
         * erase any subdirectories that were created to contain this file.
         * Not worth worrying about.
         */
        LOGI(" Deleting newly-created file '%hs'", parmCopy.pathName);
        (void) pDiskFS->DeleteFile(pNewFile);
    }
    return dierr;
}

void DiskArchive::AddToAddDataList(FileAddData* pData)
{
    ASSERT(pData != NULL);
    ASSERT(pData->GetNext() == NULL);

    /*
     * Run through the entire existing list, looking for a match.  This is
     * O(n^2) behavior, but I'm expecting N to be relatively small (under
     * 1000 in almost all cases).
     */
    FileAddData* pSearch = fpAddDataHead;
    LocalFileDetails::FileKind dataKind, listKind;

    dataKind = pData->GetDetails()->GetEntryKind();
    while (pSearch != NULL) {
        if (pSearch->GetOtherFork() == NULL &&
            wcscmp(pSearch->GetDetails()->GetStrippedLocalPathName(),
                   pData->GetDetails()->GetStrippedLocalPathName()) == 0)
        {
            //NuThreadID dataID = pData->GetDetails()->threadID;
            //NuThreadID listID = pSearch->GetDetails()->threadID;

            listKind = pSearch->GetDetails()->GetEntryKind();

            /* got a name match */
            if (dataKind != listKind &&
                (dataKind == LocalFileDetails::kFileKindDataFork ||
                 dataKind == LocalFileDetails::kFileKindRsrcFork) &&
                (listKind == LocalFileDetails::kFileKindDataFork ||
                 listKind == LocalFileDetails::kFileKindRsrcFork))
            {
                /* looks good, hook it in here instead of the list */
                LOGD("--- connecting forks of '%ls' and '%ls'",
                    (LPCWSTR) pData->GetDetails()->GetLocalPathName(),
                    (LPCWSTR) pSearch->GetDetails()->GetLocalPathName());
                pSearch->SetOtherFork(pData);
                return;
            }
        }

        pSearch = pSearch->GetNext();
    }

    if (fpAddDataHead == NULL) {
        assert(fpAddDataTail == NULL);
        fpAddDataHead = fpAddDataTail = pData;
    } else {
        fpAddDataTail->SetNext(pData);
        fpAddDataTail = pData;
    }
}

void DiskArchive::FreeAddDataList(void)
{
    FileAddData* pData;
    FileAddData* pNext;

    pData = fpAddDataHead;
    while (pData != NULL) {
        pNext = pData->GetNext();
        delete pData->GetOtherFork();
        delete pData;
        pData = pNext;
    }

    fpAddDataHead = fpAddDataTail = NULL;
}


/*
 * ===========================================================================
 *      DiskArchive -- create subdir
 * ===========================================================================
 */

bool DiskArchive::CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
    const WCHAR* newName)
{
    ASSERT(newName != NULL && wcslen(newName) > 0);
    DiskEntry* pEntry = (DiskEntry*) pParentEntry;
    ASSERT(pEntry != NULL);
    A2File* pFile = pEntry->GetA2File();
    ASSERT(pFile != NULL);
    DiskFS* pDiskFS = pFile->GetDiskFS();
    ASSERT(pDiskFS != NULL);

    if (!pFile->IsDirectory()) {
        ASSERT(false);
        return false;
    }

    DIError dierr;
    A2File* pNewFile = NULL;
    DiskFS::CreateParms parms;
    CStringA pathNameMOR;
    time_t now = time(NULL);

    /*
     * Create the full path.
     */
    if (pFile->IsVolumeDirectory()) {
        pathNameMOR = newName;
    } else {
        pathNameMOR = pParentEntry->GetPathNameMOR();
        pathNameMOR += pParentEntry->GetFssep();
        pathNameMOR += newName;
    }
    ASSERT(wcschr(newName, pParentEntry->GetFssep()) == NULL);

    /* using NufxLib constants; they match with ProDOS */
    memset(&parms, 0, sizeof(parms));
    parms.pathName = pathNameMOR;
    parms.fssep = pParentEntry->GetFssep();
    parms.storageType = kNuStorageDirectory;
    parms.fileType = 0x0f;      // ProDOS DIR
    parms.auxType = 0;
    parms.access = kNuAccessUnlocked;
    parms.createWhen = now;
    parms.modWhen = now;

    dierr = pDiskFS->CreateFile(&parms, &pNewFile);
    if (dierr != kDIErrNone) {
        CString errMsg;
        errMsg.Format(L"Unable to create subdirectory: %hs.\n",
            DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        return false;
    }

    if (InternalReload(pMsgWnd) != 0)
        return false;

    return true;
}


/*
 * ===========================================================================
 *      DiskArchive -- delete selection
 * ===========================================================================
 */

/*static*/ int DiskArchive::CompareDisplayNamesDesc(const void* ventry1,
    const void* ventry2)
{
    const DiskEntry* pEntry1 = *((const DiskEntry**) ventry1);
    const DiskEntry* pEntry2 = *((const DiskEntry**) ventry2);

    return wcsicmp(pEntry2->GetDisplayName(), pEntry1->GetDisplayName());
}

bool DiskArchive::DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
    /*
     * The DiskFS DeleteFile() function will not delete a subdirectory unless
     * it is empty.  This complicates matters somewhat for us, because the
     * selection set isn't in any particular order.  We need to sort on the
     * pathname and then delete bottom-up.
     *
     * CiderPress does work to ensure that, if a subdir is selected, everything
     * in that subdir is also selected.  So if we just delete everything in the
     * right order, we should be okay.
     */
    CString errMsg;
    SelectionEntry* pSelEntry;
    DiskEntry* pEntry;
    DIError dierr;
    bool retVal = false;

    SET_PROGRESS_BEGIN();

    /*
     * Start by copying the DiskEntry pointers out of the selection set and
     * into an array.  The selection set was created such that there is one
     * entry in the set for each file.  (The file viewer likes to have one
     * entry for each thread.)
     */
    int numEntries = pSelSet->GetNumEntries();
    ASSERT(numEntries > 0);
    DiskEntry** entryArray = new DiskEntry*[numEntries];
    int idx = 0;

    pSelEntry = pSelSet->IterNext();
    while (pSelEntry != NULL) {
        pEntry = (DiskEntry*) pSelEntry->GetEntry();
        ASSERT(pEntry != NULL);

        entryArray[idx++] = pEntry;
        LOGI("Added 0x%08lx '%ls'", (long) entryArray[idx-1],
            (LPCWSTR) entryArray[idx-1]->GetDisplayName());

        pSelEntry = pSelSet->IterNext();
    }
    ASSERT(idx == numEntries);

    /*
     * Sort the file array by descending filename.
     */
    ::qsort(entryArray, numEntries, sizeof(DiskEntry*), CompareDisplayNamesDesc);

    /*
     * Run through the sorted list, deleting each entry.
     */
    for (idx = 0; idx < numEntries; idx++) {
        A2File* pFile;

        pEntry = entryArray[idx];
        pFile = pEntry->GetA2File();

        /*
         * We shouldn't be here at all if the main volume were opened
         * read-only.  However, it's possible that the main is read-write
         * and our sub-volumes are read-only (probably because we don't
         * support write access to the filesystem).
         */
        if (!pFile->GetDiskFS()->GetReadWriteSupported()) {
            errMsg.Format(L"Unable to delete '%ls' on '%hs': operation not supported.",
                (LPCWSTR) pEntry->GetDisplayName(),
                (LPCSTR) pFile->GetDiskFS()->GetVolumeName());
            ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }

        LOGI("  Deleting '%ls' from '%hs'", (LPCWSTR) pEntry->GetPathNameUNI(),
            (LPCSTR) pFile->GetDiskFS()->GetVolumeName());
        // TODO: should be using display name for progress updater?
        SET_PROGRESS_UPDATE2(0, pEntry->GetPathNameUNI(), NULL);

        /*
         * Ask the DiskFS to delete the file.  As soon as this completes,
         * "pFile" is invalid and must not be dereferenced.
         */
        dierr = pFile->GetDiskFS()->DeleteFile(pFile);
        if (dierr != kDIErrNone) {
            errMsg.Format(L"Unable to delete '%ls' on '%hs': %hs.",
                (LPCWSTR) pEntry->GetDisplayName(),
                (LPCSTR) pFile->GetDiskFS()->GetVolumeName(),
                DiskImgLib::DIStrError(dierr));
            ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }
        SET_PROGRESS_UPDATE(100);

        /*
         * Be paranoid and zap the pointer, on the off chance somebody
         * tries to redraw the content list from the deleted data.
         *
         * In practice we don't work this way -- the stuff that gets drawn
         * on the screen comes out of GenericEntry, not A2File.  If this
         * changes we'll need to raise the "reload" flag here, before the
         * reload, to prevent the ContentList from chasing a bad pointer.
         */
        pEntry->SetA2File(NULL);
    }

    retVal = true;

bail:
    SET_PROGRESS_END();
    delete[] entryArray;
    if (InternalReload(pMsgWnd) != 0)
        retVal = false;

    return retVal;
}

/*
 * ===========================================================================
 *      DiskArchive -- rename files
 * ===========================================================================
 */

bool DiskArchive::RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
    /*
     * If we rename a subdirectory, it could affect the next thing we try to
     * rename (because we show the full path).  We have to reload our file
     * list from the DiskFS after each renamed subdir.  The trouble is that
     * this invalidates the data displayed in the ContentList, and we won't
     * redraw the screen correctly.  We can work around the problem by getting
     * the pathname directly from the DiskFS instead of from DiskEntry, though
     * it's not immediately obvious which is less confusing.
     */
    CString errMsg;
    bool retVal = false;

    LOGI("Renaming %d entries", pSelSet->GetNumEntries());

    /*
     * For each item in the selection set, bring up the "rename" dialog,
     * and ask the GenericEntry to process it.
     *
     * If they hit "cancel" or there's an error, we still flush the
     * previous changes.  This is so that we don't have to create the
     * same sort of deferred-write feature when renaming things in other
     * sorts of archives (e.g. disk archives).
     */
    SelectionEntry* pSelEntry = pSelSet->IterNext();
    while (pSelEntry != NULL) {
        RenameEntryDialog renameDlg(pMsgWnd);
        DiskEntry* pEntry = (DiskEntry*) pSelEntry->GetEntry();

        LOGI("  Renaming '%ls'", (LPCWSTR) pEntry->GetPathNameUNI());
        if (!SetRenameFields(pMsgWnd, pEntry, &renameDlg))
            break;

        int result;
        if (pEntry->GetA2File()->IsVolumeDirectory())
            result = IDIGNORE;  // don't allow rename of volume dir
        else
            result = renameDlg.DoModal();
        if (result == IDOK) {
            DIError dierr;
            DiskFS* pDiskFS;
            A2File* pFile;

            pFile = pEntry->GetA2File();
            pDiskFS = pFile->GetDiskFS();
            CStringA newNameA(renameDlg.fNewName);
            dierr = pDiskFS->RenameFile(pFile, newNameA);
            if (dierr != kDIErrNone) {
                errMsg.Format(L"Unable to rename '%ls' to '%ls': %hs.",
                    (LPCWSTR) pEntry->GetPathNameUNI(),
                    (LPCWSTR) renameDlg.fNewName,
                    DiskImgLib::DIStrError(dierr));
                ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
                goto bail;
            }
            LOGD("Rename of '%ls' to '%ls' succeeded",
                (LPCWSTR) pEntry->GetDisplayName(),
                (LPCWSTR) renameDlg.fNewName);
        } else if (result == IDCANCEL) {
            LOGI("Canceling out of remaining renames");
            break;
        } else {
            /* 3rd possibility is IDIGNORE, i.e. skip this entry */
            LOGI("Skipping rename of '%ls'", (LPCWSTR) pEntry->GetDisplayName());
        }

        pSelEntry = pSelSet->IterNext();
    }

    /* reload GenericArchive from disk image */
    if (InternalReload(pMsgWnd) == kNuErrNone)
        retVal = true;

bail:
    return retVal;
}

bool DiskArchive::SetRenameFields(CWnd* pMsgWnd, DiskEntry* pEntry,
    RenameEntryDialog* pDialog)
{
    DiskFS* pDiskFS;

    ASSERT(pEntry != NULL);

    /*
     * Figure out if we're allowed to change the entire path.  (This is
     * doing it the hard way, but what the hell.)
     */
    long cap = GetCapability(GenericArchive::kCapCanRenameFullPath);
    bool renameFullPath = (cap != 0);

    // a bit round-about, but it works
    pDiskFS = pEntry->GetA2File()->GetDiskFS();

    /*
     * Make sure rename is allowed.  It's nice to do these *before* putting
     * up the rename dialog, so that the user doesn't do a bunch of typing
     * before being told that it's pointless.
     */
    if (!pDiskFS->GetReadWriteSupported()) {
        CString errMsg;
        errMsg.Format(L"Unable to rename '%ls': operation not supported.",
            (LPCWSTR) pEntry->GetPathNameUNI());
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        return false;
    }
    if (pDiskFS->GetFSDamaged()) {
        CString errMsg;
        errMsg.Format(L"Unable to rename '%ls': the disk it's on appears to be damaged.",
            (LPCWSTR) pEntry->GetPathNameUNI());
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        return false;
    }

    pDialog->SetCanRenameFullPath(renameFullPath);
    pDialog->fOldName = pEntry->GetPathNameUNI();
    pDialog->fFssep = pEntry->GetFssep();
    pDialog->fpArchive = this;
    pDialog->fpEntry = pEntry;

    return true;
}

CString DiskArchive::TestPathName(const GenericEntry* pGenericEntry,
    const CString& basePath, const CString& newName, char newFssep) const
{
    const DiskEntry* pEntry = (DiskEntry*) pGenericEntry;
    DiskImg::FSFormat format;
    CString errMsg, pathName;
    CStringA newNameA;
    DiskFS* pDiskFS;

    if (basePath.IsEmpty()) {
        pathName = newName;
    } else {
        pathName = basePath;
        pathName += newFssep;
        pathName += newName;
    }

    pDiskFS = pEntry->GetA2File()->GetDiskFS();
    format = pDiskFS->GetDiskImg()->GetFSFormat();

    /* look for an existing file, but don't compare against self */
    A2File* existingFile;
    CStringA pathNameA(pathName);
    existingFile = pDiskFS->GetFileByName(pathNameA);
    if (existingFile != NULL && existingFile != pEntry->GetA2File()) {
        errMsg = L"A file with that name already exists.";
        goto bail;
    }

    newNameA = newName;
    switch (format) {
    case DiskImg::kFormatProDOS:
        if (!DiskFSProDOS::IsValidFileName(newNameA))
            CheckedLoadString(&errMsg, IDS_VALID_FILENAME_PRODOS);
        break;
    case DiskImg::kFormatDOS33:
    case DiskImg::kFormatDOS32:
        if (!DiskFSDOS33::IsValidFileName(newNameA))
            CheckedLoadString(&errMsg, IDS_VALID_FILENAME_DOS);
        break;
    case DiskImg::kFormatPascal:
        if (!DiskFSPascal::IsValidFileName(newNameA))
            CheckedLoadString(&errMsg, IDS_VALID_FILENAME_PASCAL);
        break;
    case DiskImg::kFormatMacHFS:
        if (!DiskFSHFS::IsValidFileName(newNameA))
            CheckedLoadString(&errMsg, IDS_VALID_FILENAME_HFS);
        break;
    default:
        errMsg = L"Not supported by TestPathName!";
    }

bail:
    return errMsg;
}


/*
 * ===========================================================================
 *      DiskArchive -- rename a volume
 * ===========================================================================
 */

bool DiskArchive::RenameVolume(CWnd* pMsgWnd, DiskFS* pDiskFS,
    const WCHAR* newName)
{
    DIError dierr;
    CString errMsg;
    bool retVal = true;

    CStringA newNameA(newName);
    dierr = pDiskFS->RenameVolume(newNameA);
    if (dierr != kDIErrNone) {
        errMsg.Format(L"Unable to rename volume: %hs.\n",
            DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        retVal = false;
        /* fall through to reload anyway */
    }

    /* reload GenericArchive from disk image */
    if (InternalReload(pMsgWnd) != 0)
        retVal = false;

    return retVal;
}

CString DiskArchive::TestVolumeName(const DiskFS* pDiskFS,
    const WCHAR* newName) const
{
    DiskImg::FSFormat format;
    CString errMsg;

    ASSERT(pDiskFS != NULL);
    ASSERT(newName != NULL);

    format = pDiskFS->GetDiskImg()->GetFSFormat();

    CStringA newNameA(newName);
    switch (format) {
    case DiskImg::kFormatProDOS:
        if (!DiskFSProDOS::IsValidVolumeName(newNameA))
            CheckedLoadString(&errMsg, IDS_VALID_VOLNAME_PRODOS);
        break;
    case DiskImg::kFormatDOS33:
    case DiskImg::kFormatDOS32:
        if (!DiskFSDOS33::IsValidVolumeName(newNameA))
            CheckedLoadString(&errMsg, IDS_VALID_VOLNAME_DOS);
        break;
    case DiskImg::kFormatPascal:
        if (!DiskFSPascal::IsValidVolumeName(newNameA))
            CheckedLoadString(&errMsg, IDS_VALID_VOLNAME_PASCAL);
        break;
    case DiskImg::kFormatMacHFS:
        if (!DiskFSHFS::IsValidVolumeName(newNameA))
            CheckedLoadString(&errMsg, IDS_VALID_VOLNAME_HFS);
        break;
    default:
        errMsg = L"Not supported by TestVolumeName!";
        break;
    }

    return errMsg;
}


/*
 * ===========================================================================
 *      DiskArchive -- set file properties
 * ===========================================================================
 */

bool DiskArchive::SetProps(CWnd* pMsgWnd, GenericEntry* pGenericEntry,
    const FileProps* pProps)
{
    /*
     * Technically we should reload the GenericArchive from the disk image,
     * but the set of changes is pretty small, so we just make them here.
     */
    DIError dierr;
    DiskEntry* pEntry = (DiskEntry*) pGenericEntry;
    A2File* pFile = pEntry->GetA2File();

    dierr = pFile->GetDiskFS()->SetFileInfo(pFile, pProps->fileType,
                pProps->auxType, pProps->access);
    if (dierr != kDIErrNone) {
        CString errMsg;
        errMsg.Format(L"Unable to set file info: %hs.\n",
            DiskImgLib::DIStrError(dierr));
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        return false;
    }

    /* do this in lieu of reloading GenericArchive */
    pEntry->SetFileType(pFile->GetFileType());
    pEntry->SetAuxType(pFile->GetAuxType());
    pEntry->SetAccess(pFile->GetAccess());

    /* DOS 3.2/3.3 may change these as well */
    DiskImg::FSFormat fsFormat;
    fsFormat = pFile->GetDiskFS()->GetDiskImg()->GetFSFormat();
    if (fsFormat == DiskImg::kFormatDOS32 || fsFormat == DiskImg::kFormatDOS33) {
        LOGD(" (reloading additional fields after DOS SFI)");
        pEntry->SetDataForkLen(pFile->GetDataLength());
        pEntry->SetCompressedLen(pFile->GetDataSparseLength());
        pEntry->SetSuspicious(pFile->GetQuality() == A2File::kQualitySuspicious);
    }

    /* clear the dirty flag in trivial cases */
    (void) fpPrimaryDiskFS->Flush(DiskImg::kFlushFastOnly);

    return true;
}


/*
 * ===========================================================================
 *      DiskArchive -- transfer files to another archive
 * ===========================================================================
 */

GenericArchive::XferStatus DiskArchive::XferSelection(CWnd* pMsgWnd,
    SelectionSet* pSelSet, ActionProgressDialog* pActionProgress, 
    const XferFileOptions* pXferOpts)
{
    /*
     * We get the open archive pointer and some options from "pXferOpts".
     *
     * The selection set was created with the "any" selection criteria, which
     * means there's only one entry for each file regardless of whether it's
     * forked or not.
     */
    LOGI("DiskArchive XferSelection!");
    uint8_t* dataBuf = NULL;
    uint8_t* rsrcBuf = NULL;
    LocalFileDetails fileDetails;
    CString errMsg, extractErrMsg, cmpStr;
    CString fixedPathName;
    XferStatus retval = kXferFailed;

    pXferOpts->fTarget->XferPrepare(pXferOpts);

    SelectionEntry* pSelEntry = pSelSet->IterNext();
    for ( ; pSelEntry != NULL; pSelEntry = pSelSet->IterNext()) {
        long dataLen=-1, rsrcLen=-1;
        DiskEntry* pEntry = (DiskEntry*) pSelEntry->GetEntry();
        int typeOverride = -1;
        int result;

        ASSERT(dataBuf == NULL);
        ASSERT(rsrcBuf == NULL);

        if (pEntry->GetDamaged()) {
            LOGI("  XFER skipping damaged entry '%ls'",
                (LPCWSTR) pEntry->GetDisplayName());
            continue;
        }

        /*
         * Do a quick de-colonizing pass for non-ProDOS volumes, then prepend
         * the subvolume name (if any).
         */
        fixedPathName = pEntry->GetPathNameUNI();
        if (fixedPathName.IsEmpty())
            fixedPathName = L"(no filename)";
        if (pEntry->GetFSFormat() != DiskImg::kFormatProDOS)
            fixedPathName.Replace(PathProposal::kDefaultStoredFssep, '.');
        if (!pEntry->GetSubVolName().IsEmpty()) {
            CString tmpStr = pEntry->GetSubVolName();
            tmpStr += (char) PathProposal::kDefaultStoredFssep;
            tmpStr += fixedPathName;
            fixedPathName = tmpStr;
        }

        if (pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir) {
            /* this is the volume dir */
            LOGD("  XFER not transferring volume dir '%ls'",
                (LPCWSTR) fixedPathName);
            continue;
        } else if (pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory) {
            if (pXferOpts->fPreserveEmptyFolders) {
                /* if this is an empty directory, create a fake entry */
                cmpStr = fixedPathName;
                cmpStr += (char)PathProposal::kDefaultStoredFssep;

                if (pSelSet->CountMatchingPrefix(cmpStr) == 0) {
                    LOGD("FOUND empty dir '%ls'", (LPCWSTR) fixedPathName);
                    cmpStr += kEmptyFolderMarker;
                    dataBuf = new unsigned char[1];
                    dataLen = 0;
                    fileDetails.SetEntryKind(LocalFileDetails::kFileKindDataFork);
                    fileDetails.SetStrippedLocalPathName(cmpStr);
                    fileDetails.SetFileType(0);         // NON
                    fileDetails.SetAccess(
                        pEntry->GetAccess() | GenericEntry::kAccessInvisible);
                    goto have_stuff2;
                } else {
                    LOGD("NOT empty dir '%ls'", (LPCWSTR) fixedPathName);
                }
            }

            LOGD("  XFER not transferring directory '%ls'",
                (LPCWSTR) fixedPathName);
            continue;
        }

        LOGI("  Xfer '%ls' (data=%d rsrc=%d)",
            (LPCWSTR) fixedPathName, pEntry->GetHasDataFork(),
            pEntry->GetHasRsrcFork());

        dataBuf = NULL;
        dataLen = 0;
        result = pEntry->ExtractThreadToBuffer(GenericEntry::kDataThread,
                    (char**) &dataBuf, &dataLen, &extractErrMsg);
        if (result == IDCANCEL) {
            LOGI("Cancelled during data extract!");
            goto bail;  /* abort anything that was pending */
        } else if (result != IDOK) {
            errMsg.Format(L"Failed while extracting '%ls': %ls.",
                (LPCWSTR) fixedPathName, (LPCWSTR) extractErrMsg);
            ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }
        ASSERT(dataBuf != NULL);
        ASSERT(dataLen >= 0);

#if 0
        if (pXferOpts->fConvDOSText &&
            DiskImg::UsesDOSFileStructure(pEntry->GetFSFormat()) &&
            pEntry->GetFileType() == kFileTypeTXT)
        {
            /* don't need to convert EOL, so just strip in place */
            long len;
            unsigned char* ucp;

            LOGI("  Converting DOS text in '%ls'", fixedPathName);
            for (ucp = dataBuf, len = dataLen; len > 0; len--, ucp++)
                *ucp = *ucp & 0x7f;
        }
#endif

#if 0   // annoying to invoke PTX reformatter from here... ReformatHolder, etc.
        if (pXferOpts->fConvPascalText &&
            pEntry->GetFSFormat() == DiskImg::kFormatPascal &&
            pEntry->GetFileType() == kFileTypePTX)
        {
            LOGI("WOULD CONVERT ptx '%ls'", fixedPathName);
        }
#endif

        if (pEntry->GetHasRsrcFork()) {
            rsrcBuf = NULL;
            rsrcLen = 0;
            result = pEntry->ExtractThreadToBuffer(GenericEntry::kRsrcThread,
                        (char**) &rsrcBuf, &rsrcLen, &extractErrMsg);
            if (result == IDCANCEL) {
                LOGI("Cancelled during rsrc extract!");
                goto bail;  /* abort anything that was pending */
            } else if (result != IDOK) {
                errMsg.Format(L"Failed while extracting '%ls': %ls.",
                    (LPCWSTR) fixedPathName, (LPCWSTR) extractErrMsg);
                ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
                goto bail;
            }
        } else {
            ASSERT(rsrcBuf == NULL);
        }

        if (pEntry->GetHasDataFork() && pEntry->GetHasRsrcFork()) {
            fileDetails.SetEntryKind(LocalFileDetails::kFileKindBothForks);
        } else if (pEntry->GetHasDataFork()) {
            fileDetails.SetEntryKind(LocalFileDetails::kFileKindDataFork);
        } else if (pEntry->GetHasRsrcFork()) {
            fileDetails.SetEntryKind(LocalFileDetails::kFileKindRsrcFork);
        }  else {
            ASSERT(false);
            fileDetails.SetEntryKind(LocalFileDetails::kFileKindUnknown);
        }

        /*
         * Set up the rest of the LocalFileDetails fields.
         */
        fileDetails.SetStrippedLocalPathName(fixedPathName);
        fileDetails.SetFileType(pEntry->GetFileType());
        fileDetails.SetAccess(pEntry->GetAccess());
have_stuff2:
        fileDetails.SetFileSysFmt(pEntry->GetSourceFS());
        fileDetails.SetFssep(PathProposal::kDefaultStoredFssep);
        fileDetails.SetExtraType(pEntry->GetAuxType());
        fileDetails.SetStorageType(kNuStorageUnknown);  // let NufxLib deal

        NuDateTime ndt;
        time_t when;
        when = time(NULL);
        UNIXTimeToDateTime(&when, &ndt);
        fileDetails.SetArchiveWhen(ndt);
        when = pEntry->GetModWhen();
        UNIXTimeToDateTime(&when, &ndt);
        fileDetails.SetModWhen(ndt);
        when = pEntry->GetCreateWhen();
        UNIXTimeToDateTime(&when, &ndt);
        fileDetails.SetCreateWhen(ndt);

        pActionProgress->SetArcName(fileDetails.GetStrippedLocalPathName());
        if (pActionProgress->SetProgress(0) == IDCANCEL) {
            retval = kXferCancelled;
            goto bail;
        }

        errMsg = pXferOpts->fTarget->XferFile(&fileDetails, &dataBuf, dataLen,
            &rsrcBuf, rsrcLen);
        if (!errMsg.IsEmpty()) {
            LOGI("XferFile failed!");
            errMsg.Format(L"Failed while transferring '%ls': %ls.",
                (LPCWSTR) pEntry->GetDisplayName(), (LPCWSTR) errMsg);
            ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }
        ASSERT(dataBuf == NULL);
        ASSERT(rsrcBuf == NULL);

        if (pActionProgress->SetProgress(100) == IDCANCEL) {
            retval = kXferCancelled;
            goto bail;
        }
    }

    //MainWindow* pMainWin;
    //pMainWin = (MainWindow*)::AfxGetMainWnd();
    //pMainWin->EventPause(1000);

    retval = kXferOK;

bail:
    if (retval != kXferOK)
        pXferOpts->fTarget->XferAbort(pMsgWnd);
    else
        pXferOpts->fTarget->XferFinish(pMsgWnd);
    delete[] dataBuf;
    delete[] rsrcBuf;
    return retval;
}

void DiskArchive::XferPrepare(const XferFileOptions* pXferOpts)
{
    LOGI("DiskArchive::XferPrepare");

    //fpPrimaryDiskFS->SetParameter(DiskFS::kParmProDOS_AllowLowerCase,
    //  pXferOpts->fAllowLowerCase);
    //fpPrimaryDiskFS->SetParameter(DiskFS::kParmProDOS_AllocSparse,
    //  pXferOpts->fUseSparseBlocks);
    fpPrimaryDiskFS->SetParameter(DiskFS::kParm_CreateUnique, true);

    //fXferStoragePrefix = pXferOpts->fStoragePrefix;
    fpXferTargetFS = pXferOpts->fpTargetFS;
}

CString DiskArchive::XferFile(LocalFileDetails* pDetails, uint8_t** pDataBuf,
    long dataLen, uint8_t** pRsrcBuf, long rsrcLen)
{
    DiskFS* pDiskFS;
    CString errMsg;
    DIError dierr = kDIErrNone;

    LOGI(" XFER: transfer '%ls' (dataLen=%ld rsrcLen=%ld)",
        (LPCWSTR) pDetails->GetStrippedLocalPathName(), dataLen, rsrcLen);

    ASSERT(pDataBuf != NULL);
    ASSERT(pRsrcBuf != NULL);

    const DiskFS::CreateParms& createParms = pDetails->GetCreateParms();

    if (fpXferTargetFS == NULL)
        pDiskFS = fpPrimaryDiskFS;
    else
        pDiskFS = fpXferTargetFS;

    /*
     * Strip the high ASCII from DOS and RDOS text files, unless we're adding
     * them to a DOS disk.  Likewise, if we're adding non-DOS text files to
     * a DOS disk, we need to add the high bit.
     *
     * DOS converts both TXT and SRC to 'T', so we have to handle both here.
     * Ideally we'd just ask DOS, "do you think this is a text file?", but it's
     * not worth adding a new interface just for that.
     */
    bool srcIsDOS, dstIsDOS;
    srcIsDOS = DiskImg::UsesDOSFileStructure(pDetails->GetFileSysFmt());
    dstIsDOS = DiskImg::UsesDOSFileStructure(pDiskFS->GetDiskImg()->GetFSFormat());
    if (dataLen > 0 &&
        (pDetails->GetFileType() == kFileTypeTXT ||
         pDetails->GetFileType() == kFileTypeSRC))
    {
        unsigned char* ucp = *pDataBuf;
        long len = dataLen;

        if (srcIsDOS && !dstIsDOS) {
            LOGD(" Stripping high ASCII from '%ls'",
                (LPCWSTR) pDetails->GetStrippedLocalPathName());

            while (len--)
                *ucp++ &= 0x7f;
        } else if (!srcIsDOS && dstIsDOS) {
            LOGD(" Adding high ASCII to '%ls'",
                (LPCWSTR) pDetails->GetStrippedLocalPathName());

            while (len--) {
                if (*ucp != '\0')
                    *ucp |= 0x80;
                ucp++;
            }
        } else if (srcIsDOS && dstIsDOS) {
            LOGD(" --- not altering DOS-to-DOS text '%ls'",
                (LPCWSTR) pDetails->GetStrippedLocalPathName());
        } else {
            LOGD(" --- non-DOS transfer '%ls'",
                (LPCWSTR) pDetails->GetStrippedLocalPathName());
        }
    }

    /* add a file with one or two forks */
    if (createParms.storageType == kNuStorageDirectory) {
        ASSERT(dataLen < 0 && rsrcLen < 0);
    } else {
        ASSERT(dataLen >= 0 || rsrcLen >= 0);   // at least one fork
    }

    /* if we still have something to write, write it */
    dierr = AddForksToDisk(pDiskFS, &createParms, *pDataBuf, dataLen,
                *pRsrcBuf, rsrcLen);
    if (dierr != kDIErrNone) {
        errMsg.Format(L"%hs", DiskImgLib::DIStrError(dierr));
        goto bail;
    }

    /* clean up */
    delete[] *pDataBuf;
    *pDataBuf = NULL;
    delete[] *pRsrcBuf;
    *pRsrcBuf = NULL;

bail:
    return errMsg;
}

void DiskArchive::XferAbort(CWnd* pMsgWnd)
{
    // Can't undo previous actions.
    LOGI("DiskArchive::XferAbort");
    InternalReload(pMsgWnd);
}

void DiskArchive::XferFinish(CWnd* pMsgWnd)
{
    LOGI("DiskArchive::XferFinish");
    InternalReload(pMsgWnd);
}
