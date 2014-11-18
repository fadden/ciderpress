/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Bridge between NufxLib and GenericArchive.
 */
#include "stdafx.h"
#include "NufxArchive.h"
#include "ConfirmOverwriteDialog.h"
#include "RenameEntryDialog.h"
#include "RecompressOptionsDialog.h"
#include "AddClashDialog.h"
#include "Main.h"
#include "../nufxlib/NufxLib.h"

/*
 * NufxLib doesn't currently allow an fssep of '\0', so we use this instead
 * to indicate the absence of an fssep char.  Not quite right, but it'll do
 * until NufxLib gets fixed.
 */
const unsigned char kNufxNoFssep = 0xff;


/*
 * ===========================================================================
 *      NufxEntry
 * ===========================================================================
 */

/*
 * Extract data from a thread into a buffer.
 *
 * If "*ppText" is non-NULL and "*pLength" is > 0, the data will be read into
 * the pointed-to buffer so long as it's shorter than *pLength bytes.  The
 * value in "*pLength" will be set to the actual length used.
 *
 * If "*ppText" is NULL or the length is <= 0, the uncompressed data will be
 * placed into a buffer allocated with "new[]".
 *
 * Returns IDOK on success, IDCANCEL if the operation was cancelled by the
 * user, and -1 value on failure.  On failure, "*ppText" and "*pLength" will
 * be valid but point at an error message.
 *
 * "which" is an anonymous GenericArchive enum.
 */
int
NufxEntry::ExtractThreadToBuffer(int which, char** ppText, long* pLength,
    CString* pErrMsg) const
{
    NuError nerr;
    char* dataBuf = NULL;
    NuDataSink* pDataSink = NULL;
    NuThread thread;
    unsigned long actualThreadEOF;
    NuThreadIdx threadIdx;
    bool needAlloc = true;
    int result = -1;

    ASSERT(IDOK != -1 && IDCANCEL != -1);   // make sure return vals don't clash

    if (*ppText != NULL)
        needAlloc = false;

    FindThreadInfo(which, &thread, pErrMsg);
    if (!pErrMsg->IsEmpty())
        goto bail;
    threadIdx = thread.threadIdx;
    actualThreadEOF = thread.actualThreadEOF;

    /*
     * We've got the right thread.  Create an appropriately-sized buffer
     * and extract the data into it (WITHOUT doing EOL conversion).
     *
     * First check for a length of zero.
     */
    if (actualThreadEOF == 0) {
        LOGI("Empty thread");
        if (needAlloc) {
            *ppText = new char[1];
            **ppText = '\0';
        }
        *pLength = 0;
        result = IDOK;
        goto bail;
    }

    if (needAlloc) {
        dataBuf = new char[actualThreadEOF];
        if (dataBuf == NULL) {
            pErrMsg->Format(L"allocation of %ld bytes failed",
                actualThreadEOF);
            goto bail;
        }
    } else {
        if (*pLength < (long) actualThreadEOF) {
            pErrMsg->Format(L"buf size %ld too short (%ld)",
                *pLength, actualThreadEOF);
            goto bail;
        }
        dataBuf = *ppText;
    }
    nerr = NuCreateDataSinkForBuffer(true, kNuConvertOff,
            (unsigned char*)dataBuf, actualThreadEOF, &pDataSink);
    if (nerr != kNuErrNone) {
        pErrMsg->Format(L"unable to create buffer data sink: %hs",
            NuStrError(nerr));
        goto bail;
    }

    SET_PROGRESS_BEGIN();
    nerr = NuExtractThread(fpArchive, threadIdx, pDataSink);
    if (nerr != kNuErrNone) {
        if (nerr == kNuErrAborted) {
            result = IDCANCEL;
            //::sprintf(errorBuf, "Cancelled.\n");
        } else if (nerr == kNuErrBadFormat) {
            pErrMsg->Format(L"The compression method used on this file is not supported "
                            L"by your copy of \"nufxlib.dll\".  For more information, "
                            L"please visit us on the web at "
                            L"http://www.faddensoft.com/ciderpress/");
        } else {
            pErrMsg->Format(L"unable to extract thread %ld: %hs",
                threadIdx, NuStrError(nerr));
        }
        goto bail;
    }

    if (needAlloc)
        *ppText = dataBuf;
    *pLength = actualThreadEOF;
    result = IDOK;

bail:
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
    if (pDataSink != NULL)
        NuFreeDataSink(pDataSink);
    return result;
}

/*
 * Extract data from a thread to a file.  Since we're not copying to memory,
 * we can't assume that we're able to hold the entire file all at once.
 *
 * Returns IDOK on success, IDCANCEL if the operation was cancelled by the
 * user, and -1 value on failure.  On failure, "*pMsg" holds an
 * error message.
 */
int
NufxEntry::ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
    ConvertHighASCII convHA, CString* pErrMsg) const
{
    NuDataSink* pDataSink = NULL;
    NuError nerr;
    NuThread thread;
    unsigned long actualThreadEOF;
    NuThreadIdx threadIdx;
    int result = -1;

    ASSERT(outfp != NULL);

    //CString errMsg;
    FindThreadInfo(which, &thread, pErrMsg);
    if (!pErrMsg->IsEmpty())
        goto bail;
    threadIdx = thread.threadIdx;
    actualThreadEOF = thread.actualThreadEOF;

    /* we've got the right thread, see if it's empty */
    if (actualThreadEOF == 0) {
        LOGI("Empty thread");
        result = IDOK;
        goto bail;
    }

    /* set EOL conversion flags */
    NuValue nuConv;
    switch (conv) {
    case kConvertEOLOff:    nuConv = kNuConvertOff;     break;
    case kConvertEOLOn:     nuConv = kNuConvertOn;      break;
    case kConvertEOLAuto:   nuConv = kNuConvertAuto;    break;
    default:
        ASSERT(false);
        pErrMsg->Format(L"internal error: bad conv flag %d", conv);
        goto bail;
    }
    if (which == kDiskImageThread) {
        /* override the above; never EOL-convert a disk image */
        nuConv = kNuConvertOff;
    }

    switch (convHA) {
    case kConvertHAOff:
        nerr = NuSetValue(fpArchive, kNuValueStripHighASCII, false);
        break;
    case kConvertHAOn:
    case kConvertHAAuto:
        nerr = NuSetValue(fpArchive, kNuValueStripHighASCII, true);
        break;
    default:
        ASSERT(false);
        pErrMsg->Format(L"internal error: bad convHA flag %d", convHA);
        goto bail;
    }

    /* make sure we convert to CRLF */
    nerr = NuSetValue(fpArchive, kNuValueEOL, kNuEOLCRLF);  // for Win32
    if (nerr != kNuErrNone) {
        pErrMsg->Format(L"failed setting EOL value: %hs", NuStrError(nerr));
        goto bail;
    }

    /* create a data sink for "outfp" */
    nerr = NuCreateDataSinkForFP(true, nuConv, outfp, &pDataSink);
    if (nerr != kNuErrNone) {
        pErrMsg->Format(L"unable to create FP data sink: %hs",
            NuStrError(nerr));
        goto bail;
    }
    
    /* extract the thread to the file */
    SET_PROGRESS_BEGIN();
    nerr = NuExtractThread(fpArchive, threadIdx, pDataSink);
    if (nerr != kNuErrNone) {
        if (nerr == kNuErrAborted) {
            /* user hit the "cancel" button */
            *pErrMsg = L"cancelled";
            result = IDCANCEL;
        } else if (nerr == kNuErrBadFormat) {
            pErrMsg->Format(L"The compression method used on this file is not supported "
                            L"by your copy of \"nufxlib.dll\".  For more information, "
                            L"please visit us on the web at "
                            L"http://www.faddensoft.com/ciderpress/");
        } else {
            pErrMsg->Format(L"unable to extract thread %ld: %hs",
                threadIdx, NuStrError(nerr));
        }
        goto bail;
    }

    result = IDOK;

bail:
    if (result == IDOK) {
        SET_PROGRESS_END();
    }
    if (pDataSink != NULL)
        NuFreeDataSink(pDataSink);
    return result;
}

/*
 * Find info for the thread we're about to extract.
 *
 * Given the NuRecordIdx stored in the object, find the thread whose
 * ThreadID matches "which".  Copies the NuThread structure into
 * "*pThread".
 *
 * On failure, "pErrMsg" will have a nonzero length, and contain an error
 * message describing the problem.
 */
void
NufxEntry::FindThreadInfo(int which, NuThread* pRetThread,
    CString* pErrMsg) const
{
    NuError nerr;

    ASSERT(pErrMsg->IsEmpty());

    /*
     * Retrieve the record from the archive.
     */
    const NuRecord* pRecord;
    nerr = NuGetRecord(fpArchive, fRecordIdx, &pRecord);
    if (nerr != kNuErrNone) {
        pErrMsg->Format(L"NufxLib unable to locate record %ld: %hs",
            fRecordIdx, NuStrError(nerr));
        goto bail;
    }

    /*
     * Find the right thread.
     */
    const NuThread* pThread;
    unsigned long wantedThreadID;
    switch (which) {
    case kDataThread:       wantedThreadID = kNuThreadIDDataFork;       break;
    case kRsrcThread:       wantedThreadID = kNuThreadIDRsrcFork;       break;
    case kDiskImageThread:  wantedThreadID = kNuThreadIDDiskImage;      break;
    case kCommentThread:    wantedThreadID = kNuThreadIDComment;        break;
    default:
        pErrMsg->Format(L"looking for bogus thread 0x%02x", which);
        goto bail;
    }

    int i;
    pThread = NULL;
    for (i = 0; i < (int)NuRecordGetNumThreads(pRecord); i++) {
        pThread = NuGetThread(pRecord, i);
        if (NuGetThreadID(pThread) == wantedThreadID)
            break;
    }
    if (i == (int)NuRecordGetNumThreads(pRecord)) {
        /* didn't find the thread we wanted */
        pErrMsg->Format(L"searched %d threads but couldn't find 0x%02x",
            NuRecordGetNumThreads(pRecord), which);
        goto bail;
    }

    memcpy(pRetThread, pThread, sizeof(*pRetThread));

bail:
    return;
}


//static const char* gShortFormatNames[] = {
//    "unc", "squ", "lz1", "lz2", "u12", "u16", "dfl", "bzp"
//};
static const WCHAR* gFormatNames[] = {
    L"Uncompr", L"Squeeze", L"LZW/1", L"LZW/2", L"LZC-12",
    L"LZC-16", L"Deflate", L"Bzip2"
};

/*
 * Analyze the contents of a record to determine if it's a disk, file,
 * or "other".  Compute the total compressed and uncompressed lengths
 * of all data threads.  Return the "best" format.
 *
 * The "best format" and "record type" stuff assume that the entire
 * record contains only a disk thread or a file thread, and that any
 * format is interesting so long as it isn't "no compression".  In
 * general these will be true, because ShrinkIt and NuLib create files
 * this way.
 *
 * You could, of course, create a single record with a data thread and
 * a disk image thread, but it's a fair bet ShrinkIt would ignore one
 * or the other.
 *
 * NOTE: we don't currently work around the GSHK zero-length file bug.
 * Such records, which have a filename thread but no data threads at all,
 * will be categorized as "unknown".  We could detect the situation and
 * correct it, but we might as well flag it in a user-visible way.
 */
void
NufxEntry::AnalyzeRecord(const NuRecord* pRecord)
{
    const NuThread* pThread;
    NuThreadID threadID;
    unsigned long idx;
    RecordKind recordKind;
    unsigned long uncompressedLen;
    unsigned long compressedLen;
    unsigned short format;

    recordKind = kRecordKindUnknown;
    uncompressedLen = compressedLen = 0;
    format = kNuThreadFormatUncompressed;

    for (idx = 0; idx < pRecord->recTotalThreads; idx++) {
        pThread = NuGetThread(pRecord, idx);
        ASSERT(pThread != NULL);

        threadID = NuMakeThreadID(pThread->thThreadClass,
                    pThread->thThreadKind);

        if (pThread->thThreadClass == kNuThreadClassData) {
            /* replace what's there if this might be more interesting */
            if (format == kNuThreadFormatUncompressed)
                format = (unsigned short) pThread->thThreadFormat;

            if (threadID == kNuThreadIDRsrcFork)
                recordKind = kRecordKindForkedFile;
            else if (threadID == kNuThreadIDDiskImage)
                recordKind = kRecordKindDisk;
            else if (threadID == kNuThreadIDDataFork &&
                    recordKind == kRecordKindUnknown)
                recordKind = kRecordKindFile;

            /* sum up, so we get both forks of forked files */
            //uncompressedLen += pThread->actualThreadEOF;
            compressedLen += pThread->thCompThreadEOF;
        }

        if (threadID == kNuThreadIDDataFork) {
            if (!GetHasDataFork() && !GetHasDiskImage()) {
                SetHasDataFork(true);
                SetDataForkLen(pThread->actualThreadEOF);
            } else {
                LOGI("WARNING: ignoring second disk image / data fork");
            }
        }
        if (threadID == kNuThreadIDRsrcFork) {
            if (!GetHasRsrcFork()) {
                SetHasRsrcFork(true);
                SetRsrcForkLen(pThread->actualThreadEOF);
            } else {
                LOGI("WARNING: ignoring second data fork");
            }
        }
        if (threadID == kNuThreadIDDiskImage) {
            if (!GetHasDiskImage() && !GetHasDataFork()) {
                SetHasDiskImage(true);
                SetDataForkLen(pThread->actualThreadEOF);
            } else {
                LOGI("WARNING: ignoring second disk image / data fork");
            }
        }
        if (threadID == kNuThreadIDComment) {
            SetHasComment(true);
            if (pThread->actualThreadEOF != 0)
                SetHasNonEmptyComment(true);
        }
    }

    SetRecordKind(recordKind);
    //SetUncompressedLen(uncompressedLen);
    SetCompressedLen(compressedLen);

    if (format >= 0 && format < NELEM(gFormatNames))
        SetFormatStr(gFormatNames[format]);
    else
        SetFormatStr(L"Unknown");
}


/*
 * ===========================================================================
 *      NufxArchive
 * ===========================================================================
 */

/*
 * Perform one-time initialization of the NufxLib library.
 *
 * Returns with an error if the NufxLib version is off.  Major version must
 * match (since it indicates an interface change), minor version must be
 * >= what we expect (in case we're relying on recent behavior changes).
 *
 * Returns 0 on success, nonzero on error.
 */
/*static*/ CString
NufxArchive::AppInit(void)
{
    NuError nerr;
    CString result("");
    long major, minor, bug;

    nerr = NuGetVersion(&major, &minor, &bug, NULL, NULL);
    if (nerr != kNuErrNone) {
        result = "Unable to get version number from NufxLib.";
        goto bail;
    }

    if (major != kNuVersionMajor || minor < kNuVersionMinor) {
        result.Format(L"Older or incompatible version of NufxLib DLL found.\r\r"
                      L"Wanted v%d.%d.x, found %ld.%ld.%ld.",
                kNuVersionMajor, kNuVersionMinor,
                major, minor, bug);
        goto bail;
    }
    if (bug != kNuVersionBug) {
        LOGI("Different 'bug' version (built vX.X.%d, dll vX.X.%d)",
            kNuVersionBug, bug);
    }

    /* set NufxLib's global error message handler */
    NuSetGlobalErrorMessageHandler(NufxErrorMsgHandler);

bail:
    return result;
}


/*
 * Determine whether a particular kind of compression is supported by
 * NufxLib.
 *
 * Returns "true" if supported, "false" if not.
 */
/*static*/ bool
NufxArchive::IsCompressionSupported(NuThreadFormat format)
{
    NuFeature feature;

    switch (format) {
    case kNuThreadFormatUncompressed:
        return true;

    case kNuThreadFormatHuffmanSQ:
        feature = kNuFeatureCompressSQ;
        break;
    case kNuThreadFormatLZW1:
    case kNuThreadFormatLZW2:
        feature = kNuFeatureCompressLZW;
        break;
    case kNuThreadFormatLZC12:
    case kNuThreadFormatLZC16:
        feature = kNuFeatureCompressLZC;
        break;
    case kNuThreadFormatDeflate:
        feature = kNuFeatureCompressDeflate;
        break;
    case kNuThreadFormatBzip2:
        feature = kNuFeatureCompressBzip2;
        break;

    default:
        ASSERT(false);
        return false;
    }

    NuError nerr;
    nerr = NuTestFeature(feature);
    if (nerr == kNuErrNone)
        return true;
    return false;
}

/*
 * Display error messages... or not.
 */
NuResult
NufxArchive::NufxErrorMsgHandler(NuArchive* /*pArchive*/, void* vErrorMessage)
{
    const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;

    LOG_BASE(pErrorMessage->isDebug ? DebugLog::LOG_DEBUG : DebugLog::LOG_WARNING,
        pErrorMessage->file, pErrorMessage->line, "<nufxlib> %hs",
        pErrorMessage->message);

    return kNuOK;
}

/*
 * Display our progress.
 *
 * "oldName" ends up on top, "newName" on bottom.
 */
/*static*/NuResult
NufxArchive::ProgressUpdater(NuArchive* pArchive, void* vpProgress)
{
    const NuProgressData* pProgress = (const NuProgressData*) vpProgress;
    NufxArchive* pThis;
    MainWindow* pMainWin = (MainWindow*)::AfxGetMainWnd();
    int status;
    const char* oldName;
    const char* newName;
    int perc;

    ASSERT(pProgress != NULL);
    ASSERT(pMainWin != NULL);

    ASSERT(pArchive != NULL);
    (void) NuGetExtraData(pArchive, (void**) &pThis);
    ASSERT(pThis != NULL);

    oldName = newName = NULL;
    if (pProgress->operation == kNuOpAdd) {
        oldName = pProgress->origPathname;
        newName = pProgress->pathname;
        if (pThis->fProgressAsRecompress)
            oldName = "-";
    } else if (pProgress->operation == kNuOpTest) {
        oldName = pProgress->pathname;
    } else if (pProgress->operation == kNuOpExtract) {
        if (pThis->fProgressAsRecompress) {
            oldName = pProgress->origPathname;
            newName = "-";
        }
    }

    perc = pProgress->percentComplete;
    if (pProgress->state == kNuProgressDone)
        perc = 100;

    //LOGI("Progress: %d%% '%hs' '%hs'", perc,
    //  oldName == NULL ? "(null)" : oldName,
    //  newName == NULL ? "(null)" : newName);

    //status = pMainWin->SetProgressUpdate(perc, oldName, newName);
    CString oldNameW(oldName);
    CString newNameW(newName);
    status = SET_PROGRESS_UPDATE2(perc, oldNameW, newNameW);

    /* check to see if user hit the "cancel" button on the progress dialog */
    if (pProgress->state == kNuProgressAborted) {
        LOGI("(looks like we're aborting)");
        ASSERT(status == IDCANCEL);
    }
    
    if (status == IDCANCEL) {
        LOGI("Signaling NufxLib to abort");
        return kNuAbort;
    } else
        return kNuOK;
}


/*
 * Finish instantiating a NufxArchive object by opening an existing file.
 *
 * Returns an error string on failure, or NULL on success.
 */
GenericArchive::OpenResult
NufxArchive::Open(const WCHAR* filename, bool readOnly, CString* pErrMsg)
{
    NuError nerr;
    CString errMsg;

    ASSERT(fpArchive == NULL);

    CStringA filenameA(filename);
    if (!readOnly) {
        CString tmpname = GenDerivedTempName(filename);
        LOGI("Opening file '%ls' rw (tmp='%ls')", filename, (LPCWSTR) tmpname);
        fIsReadOnly = false;
        CStringA tmpnameA(tmpname);
        nerr = NuOpenRW(filenameA, tmpnameA, 0, &fpArchive);
    }
    if (nerr == kNuErrFileAccessDenied || nerr == EACCES) {
        LOGI("Read-write failed with access denied, trying read-only");
        readOnly = true;
    }
    if (readOnly) {
        LOGI("Opening file '%ls' ro", (LPCWSTR) filename);
        fIsReadOnly = true;
        nerr = NuOpenRO(filenameA, &fpArchive);
    }
    if (nerr != kNuErrNone) {
        errMsg.Format(L"Unable to open '%ls': %hs", filename, NuStrError(nerr));
        goto bail;
    } else {
        //LOGI("FILE OPEN SUCCESS");
    }

    nerr = SetCallbacks();
    if (nerr != kNuErrNone) {
        errMsg = L"Callback init failed";
        goto bail;
    }

    nerr = LoadContents();
    if (nerr != kNuErrNone) {
        errMsg = L"Failed reading archive contents: ";
        errMsg += NuStrError(nerr);
    }

    SetPathName(filename);

bail:
    *pErrMsg = errMsg;
    if (!errMsg.IsEmpty())
        return kResultFailure;
    else
        return kResultSuccess;
}


/*
 * Finish instantiating a NufxArchive object by creating a new archive.
 *
 * Returns an error string on failure, or "" on success.
 */
CString
NufxArchive::New(const WCHAR* filename, const void* options)
{
    NuError nerr;
    CString retmsg;

    ASSERT(fpArchive == NULL);
    ASSERT(options == NULL);

    CString tmpname = GenDerivedTempName(filename);
    LOGI("Creating file '%ls' (tmp='%ls')", filename, (LPCWSTR) tmpname);
    fIsReadOnly = false;
    CStringA filenameA(filename);
    CStringA tmpnameA(tmpname);
    nerr = NuOpenRW(filenameA, tmpnameA, kNuOpenCreat | kNuOpenExcl, &fpArchive);
    if (nerr != kNuErrNone) {
        retmsg.Format(L"Unable to open '%ls': %hs", filename, NuStrError(nerr));
        goto bail;
    } else {
        LOGI("NEW FILE SUCCESS");
    }


    nerr = SetCallbacks();
    if (nerr != kNuErrNone) {
        retmsg = L"Callback init failed";
        goto bail;
    }

    SetPathName(filename);

bail:
    return retmsg;
}

/*
 * Set some standard callbacks and feature flags.
 */
NuError
NufxArchive::SetCallbacks(void)
{
    NuError nerr;

    nerr = NuSetExtraData(fpArchive, this);
    if (nerr != kNuErrNone)
        goto bail;
//    nerr = NuSetSelectionFilter(fpArchive, SelectionFilter);
//  if (nerr != kNuErrNone)
//      goto bail;
//    nerr = NuSetOutputPathnameFilter(fpArchive, OutputPathnameFilter);
//  if (nerr != kNuErrNone)
//      goto bail;
    NuSetProgressUpdater(fpArchive, ProgressUpdater);
//    nerr = NuSetErrorHandler(fpArchive, ErrorHandler);
//  if (nerr != kNuErrNone)
//      goto bail;
    NuSetErrorMessageHandler(fpArchive, NufxErrorMsgHandler);

    /* let NufxLib worry about buggy records without data threads */
    nerr = NuSetValue(fpArchive, kNuValueMaskDataless, kNuValueTrue);
    if (nerr != kNuErrNone)
        goto bail;

    /* set any values based on Preferences values */
    PreferencesChanged();

bail:
    return nerr;
}

/*
 * User has updated their preferences.  Take note.
 *
 * (This is also called the first time through.)
 */
void
NufxArchive::PreferencesChanged(void)
{
    NuError nerr;
    const Preferences* pPreferences = GET_PREFERENCES();
    bool val;

    val = pPreferences->GetPrefBool(kPrMimicShrinkIt);
    nerr = NuSetValue(fpArchive, kNuValueMimicSHK, val);
    if (nerr != kNuErrNone) {
        LOGI("NuSetValue(kNuValueMimicSHK, %d) failed, err=%d", val, nerr);
        ASSERT(false);
    } else {
        LOGI("Set MimicShrinkIt to %d", val);
    }

    val = pPreferences->GetPrefBool(kPrReduceSHKErrorChecks);
    NuSetValue(fpArchive, kNuValueIgnoreLZW2Len, val);
    NuSetValue(fpArchive, kNuValueIgnoreCRC, val);

    val = pPreferences->GetPrefBool(kPrBadMacSHK);
    NuSetValue(fpArchive, kNuValueHandleBadMac, val);
}

/*
 * Report on what NuFX is capable of.
 */
long
NufxArchive::GetCapability(Capability cap)
{
    switch (cap) {
    case kCapCanTest:
        return true;
        break;
    case kCapCanRenameFullPath:
        return true;
        break;
    case kCapCanRecompress:
        return true;
        break;
    case kCapCanEditComment:
        return true;
        break;
    case kCapCanAddDisk:
        return true;
        break;
    case kCapCanConvEOLOnAdd:
        return false;
        break;
    case kCapCanCreateSubdir:
        return false;
        break;
    case kCapCanRenameVolume:
        return false;
        break;
    default:
        ASSERT(false);
        return -1;
        break;
    }
}

/*
 * Load the contents of an archive into the GenericEntry/NufxEntry list.
 *
 * We will need to set an error handler if we want to be able to do things
 * like "I found a bad CRC, did you want me to keep trying anyway?".
 */
NuError
NufxArchive::LoadContents(void)
{
    long counter = 0;
    NuError result;

    LOGI("NufxArchive LoadContents");
    ASSERT(fpArchive != NULL);

    {
        MainWindow* pMain = GET_MAIN_WINDOW();
        ExclusiveModelessDialog* pWaitDlg = new ExclusiveModelessDialog;
        pWaitDlg->Create(IDD_LOADING, pMain);
        pWaitDlg->CenterWindow();
        pMain->PeekAndPump();   // redraw
        CWaitCursor waitc;

        result = NuContents(fpArchive, ContentFunc);

        SET_PROGRESS_COUNTER(-1);

        pWaitDlg->DestroyWindow();
        //pMain->PeekAndPump(); // redraw
    }

    return result;
}

/*
 * Reload the contents.
 */
CString
NufxArchive::Reload(void)
{
    NuError nerr;
    CString errMsg;

    fReloadFlag = true;     // tell everybody that cached data is invalid

    DeleteEntries();        // a GenericArchive operation

    nerr = LoadContents();
    if (nerr != kNuErrNone) {
        errMsg.Format(L"ERROR: unable to reload archive contents: %hs.",
            NuStrError(nerr));

        DeleteEntries();
        fIsReadOnly = true;
    }

    return errMsg;
}

/*
 * Reload the contents of the archive, showing an error message if the
 * reload fails.
 */
NuError
NufxArchive::InternalReload(CWnd* pMsgWnd)
{
    CString errMsg;

    errMsg = Reload();

    if (!errMsg.IsEmpty()) {
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        return kNuErrGeneric;
    }

    return kNuErrNone;
}

/*
 * Static callback function.  Used for scanning the contents of an archive.
 */
NuResult
NufxArchive::ContentFunc(NuArchive* pArchive, void* vpRecord)
{
    const NuRecord* pRecord = (const NuRecord*) vpRecord;
    NufxArchive* pThis;
    NufxEntry* pNewEntry;

    ASSERT(pArchive != NULL);
    ASSERT(vpRecord != NULL);

    NuGetExtraData(pArchive, (void**) &pThis);

    pNewEntry = new NufxEntry(pArchive);

    CStringW filenameW(pRecord->filename);
    pNewEntry->SetPathName(filenameW);
    pNewEntry->SetFssep(NuGetSepFromSysInfo(pRecord->recFileSysInfo));
    pNewEntry->SetFileType(pRecord->recFileType);
    pNewEntry->SetAuxType(pRecord->recExtraType);
    pNewEntry->SetAccess(pRecord->recAccess);
    pNewEntry->SetCreateWhen(DateTimeToSeconds(&pRecord->recCreateWhen));
    pNewEntry->SetModWhen(DateTimeToSeconds(&pRecord->recModWhen));

    /*
     * Our files are always ProDOS format.  This is especially important
     * when cutting & pasting, so that the DOS high ASCII converter gets
     * invoked at appropriate times.
     */
    pNewEntry->SetSourceFS(DiskImg::kFormatProDOS);

    pNewEntry->AnalyzeRecord(pRecord);
    pNewEntry->SetRecordIdx(pRecord->recordIdx);

    pThis->AddEntry(pNewEntry);
    if ((pThis->GetNumEntries() % 10) == 0)
        SET_PROGRESS_COUNTER(pThis->GetNumEntries());

    return kNuOK;
}

/*
 * Convert a NuDateTime structure to a time_t.
 */
/*static*/ time_t
NufxArchive::DateTimeToSeconds(const NuDateTime* pDateTime)
{
    if (pDateTime->second == 0 &&
        pDateTime->minute == 0 &&
        pDateTime->hour == 0 &&
        pDateTime->year == 0 &&
        pDateTime->day == 0 &&
        pDateTime->month == 0 &&
        pDateTime->extra == 0 &&
        pDateTime->weekDay == 0)
    {
        return kDateNone;
    }

    int year;
    if (pDateTime->year < 40)
        year = pDateTime->year + 2000;
    else
        year = pDateTime->year + 1900;

    if (year < 1969) {
        /*
         * Years like 1963 are valid on an Apple II but cannot be represented
         * as a time_t, which starts in 1970.  (Depending on GMT offsets,
         * it actually starts a few hours earlier at the end of 1969.)
         *
         * I'm catching this here because of an assert in the CTime
         * constructor.  The constructor seems to do the right thing, and the
         * assert won't be present in the shipping version, but it's annoying
         * during debugging.
         */
        //LOGI(" Ignoring funky year %ld", year);
        return kDateInvalid;
    }
    if (pDateTime->month > 11)
        return kDateInvalid;

    CTime modTime(year,
                  pDateTime->month+1,
                  pDateTime->day+1,
                  pDateTime->hour,
                  pDateTime->minute,
                  pDateTime->second);
    return (time_t) modTime.GetTime();
}

/*
 * Callback from a DataSource that is done with a buffer.  Use for memory
 * allocated with new[].
 */
/*static*/ NuResult
NufxArchive::ArrayDeleteHandler(NuArchive* pArchive, void* ptr)
{
    delete[] ptr;
    return kNuOK;
}


/*
 * ===========================================================================
 *      NufxArchive -- add files (or disks)
 * ===========================================================================
 */

/*
 * Process a bulk "add" request.
 *
 * This calls into the GenericArchive "AddFile" function, which does
 * Win32-specific processing.  That function calls our DoAddFile function,
 * which does the NuFX stuff.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
NufxArchive::BulkAdd(ActionProgressDialog* pActionProgress,
    const AddFilesDialog* pAddOpts)
{
    NuError nerr;
    CString errMsg;
    WCHAR curDir[MAX_PATH] = L"";
    bool retVal = false;

    LOGI("Opts: '%ls' typePres=%d", (LPCWSTR) pAddOpts->fStoragePrefix,
        pAddOpts->fTypePreservation);
    LOGI("      sub=%d strip=%d ovwr=%d",
        pAddOpts->fIncludeSubfolders, pAddOpts->fStripFolderNames,
        pAddOpts->fOverwriteExisting);

    AddPrep(pActionProgress, pAddOpts);

    pActionProgress->SetArcName(L"(Scanning files to be added...)");
    pActionProgress->SetFileName(L"");

    /* initialize count */
    fNumAdded = 0;

    const WCHAR* buf = pAddOpts->GetFileNames();
    LOGI("Selected path = '%ls' (offset=%d)", buf,
        pAddOpts->GetFileNameOffset());

    if (GetCurrentDirectory(NELEM(curDir), curDir) == 0) {
        errMsg = L"Unable to get current directory.\n";
        ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        goto bail;
    }
    if (SetCurrentDirectory(buf) == false) {
        errMsg.Format(L"Unable to set current directory to '%ls'.\n", buf);
        ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        goto bail;
    }

    buf += pAddOpts->GetFileNameOffset();
    while (*buf != '\0') {
        LOGI("  file '%ls'", buf);

        /* this just provides the list of files to NufxLib */
        nerr = AddFile(pAddOpts, buf, &errMsg);
        if (nerr != kNuErrNone) {
            if (errMsg.IsEmpty())
                errMsg.Format(L"Failed while adding file '%ls': %hs.",
                    buf, NuStrError(nerr));
            if (nerr != kNuErrAborted) {
                ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
            }
            goto bail;
        }

        buf += wcslen(buf)+1;
    }

    /* actually do the work */
    long statusFlags;
    nerr = NuFlush(fpArchive, &statusFlags);
    if (nerr != kNuErrNone) {
        if (nerr != kNuErrAborted) {
            errMsg.Format(L"Unable to add files: %hs.", NuStrError(nerr));
            ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        }

        /* see if it got converted to read-only status */
        if (statusFlags & kNuFlushReadOnly)
            fIsReadOnly = true;
        goto bail;
    }

    if (!fNumAdded) {
        errMsg = L"No files added.\n";
        fpMsgWnd->MessageBox(errMsg, L"CiderPress", MB_OK | MB_ICONWARNING);
    } else {
        if (InternalReload(fpMsgWnd) == kNuErrNone)
            retVal = true;
        else
            errMsg = L"Reload failed.";
    }

bail:
    NuAbort(fpArchive);     // abort anything that didn't get flushed
    if (SetCurrentDirectory(curDir) == false) {
        errMsg.Format(L"Unable to reset current directory to '%ls'.\n", buf);
        ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        // bummer, but don't signal failure
    }
    AddFinish();
    return retVal;
}

/*
 * Add a single disk to the archive.
 */
bool
NufxArchive::AddDisk(ActionProgressDialog* pActionProgress,
    const AddFilesDialog* pAddOpts)
{
    NuError nerr;
    CString errMsg;
    const int kBlockSize = 512;
    PathProposal pathProp;
    PathName pathName;
    DiskImg* pDiskImg;
    NuDataSource* pSource = NULL;
    unsigned char* diskData = NULL;
    WCHAR curDir[MAX_PATH] = L"\\";
    bool retVal = false;
    CStringA storageNameA, origNameA;

    LOGI("AddDisk: '%ls' %d", pAddOpts->GetFileNames(),
        pAddOpts->GetFileNameOffset());
    LOGI("Opts: '%ls' type=%d", (LPCWSTR) pAddOpts->fStoragePrefix,
        pAddOpts->fTypePreservation);
    LOGI("      sub=%d strip=%d ovwr=%d",
        pAddOpts->fIncludeSubfolders, pAddOpts->fStripFolderNames,
        pAddOpts->fOverwriteExisting);

    pDiskImg = pAddOpts->fpDiskImg;
    ASSERT(pDiskImg != NULL);

    /* allocate storage for the entire disk */
    diskData = new BYTE[pDiskImg->GetNumBlocks() * kBlockSize];
    if (diskData == NULL) {
        errMsg.Format(L"Unable to allocate %d bytes.",
            pDiskImg->GetNumBlocks() * kBlockSize);
        ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        goto bail;
    }

    /* prepare to add */
    AddPrep(pActionProgress, pAddOpts);

    const WCHAR* buf;
    buf = pAddOpts->GetFileNames();
    LOGI("Selected path = '%ls' (offset=%d)", buf,
        pAddOpts->GetFileNameOffset());

    if (GetCurrentDirectory(NELEM(curDir), curDir) == 0) {
        errMsg = L"Unable to get current directory.\n";
        ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        goto bail;
    }
    if (SetCurrentDirectory(buf) == false) {
        errMsg.Format(L"Unable to set current directory to '%ls'.\n", buf);
        ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        goto bail;
    }

    buf += pAddOpts->GetFileNameOffset();
    LOGI("  file '%ls'", buf);

    /* strip off preservation stuff, and ignore it */
    pathProp.Init(buf);
    pathProp.fStripDiskImageSuffix = true;
    pathProp.LocalToArchive(pAddOpts);

    /* fill in the necessary file details */
    NuFileDetails details;
    memset(&details, 0, sizeof(details));
    details.threadID = kNuThreadIDDiskImage;
    details.storageType = kBlockSize;
    details.access = kNuAccessUnlocked;
    details.extraType = pAddOpts->fpDiskImg->GetNumBlocks();
    origNameA = buf;
    storageNameA = pathProp.fStoredPathName;
    details.origName = origNameA;
    details.storageName = storageNameA;
    details.fileSysID = kNuFileSysUnknown;
    details.fileSysInfo = PathProposal::kDefaultStoredFssep;

    time_t now, then;

    pathName = buf;
    now = time(NULL);
    then = pathName.GetModWhen();
    UNIXTimeToDateTime(&now, &details.archiveWhen);
    UNIXTimeToDateTime(&then, &details.modWhen);
    UNIXTimeToDateTime(&then, &details.createWhen);

    /* set up the progress updater */
    pActionProgress->SetArcName(pathProp.fStoredPathName);
    pActionProgress->SetFileName(buf);

    /* read the disk now that we have progress update titles in place */
    int block, numBadBlocks;
    unsigned char* bufPtr;
    numBadBlocks = 0;
    for (block = 0, bufPtr = diskData; block < pDiskImg->GetNumBlocks(); block++)
    {
        DIError dierr;
        dierr = pDiskImg->ReadBlock(block, bufPtr);
        if (dierr != kDIErrNone)
            numBadBlocks++;
        bufPtr += kBlockSize;
    }
    if (numBadBlocks > 0) {
        CString appName, msg;
        appName.LoadString(IDS_MB_APP_NAME);
        msg.Format(L"Skipped %ld unreadable block%ls.", numBadBlocks,
            numBadBlocks == 1 ? L"" : L"s");
        fpMsgWnd->MessageBox(msg, appName, MB_OK | MB_ICONWARNING);
        // keep going -- just a warning
    }

    /* create a data source for the disk */
    nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed, 0,
            diskData, 0, pAddOpts->fpDiskImg->GetNumBlocks() * kBlockSize,
            NULL, &pSource);
    if (nerr != kNuErrNone) {
        errMsg = "Unable to create NufxLib data source.";
        ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        goto bail;
    }

    /* add the record; name conflicts cause the error handler to fire */
    NuRecordIdx recordIdx;
    nerr = NuAddRecord(fpArchive, &details, &recordIdx);
    if (nerr != kNuErrNone) {
        if (nerr != kNuErrAborted) {
            errMsg.Format(L"Failed adding record: %hs.", NuStrError(nerr));
            ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        }
        goto bail;
    }

    /* do the compression */
    nerr = NuAddThread(fpArchive, recordIdx, kNuThreadIDDiskImage,
            pSource, NULL);
    if (nerr != kNuErrNone) {
        errMsg.Format(L"Failed adding thread: %hs.", NuStrError(nerr));
        ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        goto bail;
    }
    pSource = NULL;      /* NufxLib owns it now */

    /* actually do the work */
    long statusFlags;
    nerr = NuFlush(fpArchive, &statusFlags);
    if (nerr != kNuErrNone) {
        if (nerr != kNuErrAborted) {
            errMsg.Format(L"Unable to add disk: %hs.", NuStrError(nerr));
            ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        }

        /* see if it got converted to read-only status */
        if (statusFlags & kNuFlushReadOnly)
            fIsReadOnly = true;
        goto bail;
    }

    if (InternalReload(fpMsgWnd) == kNuErrNone)
        retVal = true;

bail:
    delete[] diskData;
    NuAbort(fpArchive);     // abort anything that didn't get flushed
    NuFreeDataSource(pSource);
    if (SetCurrentDirectory(curDir) == false) {
        errMsg.Format(L"Unable to reset current directory to '%hs'.\n", buf);
        ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        // bummer
    }
    AddFinish();
    return retVal;
}

/*
 * Do the archive-dependent part of the file add, including things like
 * adding comments.  This is eventually called by AddFile() during bulk
 * adds.
 */
NuError
NufxArchive::DoAddFile(const AddFilesDialog* pAddOpts,
    FileDetails* pDetails)
{
    NuError err;
    NuRecordIdx recordIdx = 0;
    NuFileDetails nuFileDetails;
    
retry:
    nuFileDetails = *pDetails;  // stuff class contents into struct
    CStringA origNameA(pDetails->origName);
    err = NuAddFile(fpArchive, origNameA /*pathname*/,
            &nuFileDetails, false, &recordIdx);

    if (err == kNuErrNone) {
        fNumAdded++;
    } else if (err == kNuErrSkipped) {
        /* "maybe overwrite" UI causes this if user declines */
        // fall through with the error
        LOGI("DoAddFile: skipped '%ls'", (LPCWSTR) pDetails->origName);
    } else if (err == kNuErrRecordExists) {
        AddClashDialog dlg;

        dlg.fWindowsName = pDetails->origName;
        dlg.fStorageName = pDetails->storageName;
        if (dlg.DoModal() != IDOK) {
            err = kNuErrAborted;
            goto bail_quiet;
        }
        if (dlg.fDoRename) {
            LOGD("add clash: rename to '%ls'", (LPCWSTR) dlg.fNewName);
            pDetails->storageName = dlg.fNewName;
            goto retry;
        } else {
            LOGD("add clash: skip");
            err = kNuErrSkipped;
            // fall through with error
        }
    }
    //if (err != kNuErrNone)
    //  goto bail;

//bail:
    if (err != kNuErrNone && err != kNuErrAborted && err != kNuErrSkipped) {
        CString msg;
        msg.Format(L"Unable to add file '%ls': %hs.",
            (LPCWSTR) pDetails->origName, NuStrError(err));
        ShowFailureMsg(fpMsgWnd, msg, IDS_FAILED);
    }
bail_quiet:
    return err;
}

/*
 * Prepare to add files.
 */
void
NufxArchive::AddPrep(CWnd* pMsgWnd, const AddFilesDialog* pAddOpts)
{
    NuError nerr;
    const Preferences* pPreferences = GET_PREFERENCES();
    int defaultCompression;

    ASSERT(fpArchive != NULL);

    fpMsgWnd = pMsgWnd;
    ASSERT(fpMsgWnd != NULL);

    fpAddOpts = pAddOpts;
    ASSERT(fpAddOpts != NULL);

    //fBulkProgress = true;

    defaultCompression = pPreferences->GetPrefLong(kPrCompressionType);
    nerr = NuSetValue(fpArchive, kNuValueDataCompression,
            defaultCompression + kNuCompressNone);
    if (nerr != kNuErrNone) {
        LOGI("GLITCH: unable to set compression type to %d",
            defaultCompression);
        /* keep going */
    }

    if (pAddOpts->fOverwriteExisting)
        NuSetValue(fpArchive, kNuValueHandleExisting, kNuAlwaysOverwrite);
    else
        NuSetValue(fpArchive, kNuValueHandleExisting, kNuMaybeOverwrite);

    NuSetErrorHandler(fpArchive, BulkAddErrorHandler);
    NuSetExtraData(fpArchive, this);
}

/*
 * Reset some things after we finish adding files.  We don't necessarily
 * want these to stay in effect for other operations, e.g. extracting
 * (though that is currently handled within CiderPress).
 */
void
NufxArchive::AddFinish(void)
{
    NuSetErrorHandler(fpArchive, NULL);
    NuSetValue(fpArchive, kNuValueHandleExisting, kNuMaybeOverwrite);
    fpMsgWnd = NULL;
    fpAddOpts = NULL;
    //fBulkProgress = false;
}


/*
 * Error handler callback for "bulk" adds.
 */
/*static*/ NuResult
NufxArchive::BulkAddErrorHandler(NuArchive* pArchive, void* vErrorStatus)
{
    const NuErrorStatus* pErrorStatus = (const NuErrorStatus*)vErrorStatus;
    NufxArchive* pThis;
    NuResult result;

    ASSERT(pArchive != NULL);
    (void) NuGetExtraData(pArchive, (void**) &pThis);
    ASSERT(pThis != NULL);
    ASSERT(pArchive == pThis->fpArchive);

    /* default action is to abort the current operation */
    result = kNuAbort;

    /*
     * When adding files, the NuAddFile and NuAddRecord calls can return
     * immediate, specific results for a single add.  The only reasons for
     * calling here are to decide if an existing record should be replaced
     * or not (without even an option to rename), or to decide what to do
     * when the NuFlush call runs into a problem while adding a file.
     */
    if (pErrorStatus->operation != kNuOpAdd) {
        ASSERT(false);
        return kNuAbort;
    }

    if (pErrorStatus->err == kNuErrRecordExists) {
        /* if they want to update or freshen, don't hassle them */
        //if (NState_GetModFreshen(pState) || NState_GetModUpdate(pState))
        if (pThis->fpAddOpts->fOverwriteExisting) {
            ASSERT(false);  // should be handled by AddPrep()/NufxLib
            result = kNuOverwrite;
        } else
            result = pThis->HandleReplaceExisting(pErrorStatus);
    } else if (pErrorStatus->err == kNuErrFileNotFound) {
        /* file was specified with NuAdd but removed during NuFlush */
        result = pThis->HandleAddNotFound(pErrorStatus);
    }

    return result;
}

/*
 * Decide whether or not to replace an existing file (during extract)
 * or record (during add).
 */
NuResult
NufxArchive::HandleReplaceExisting(const NuErrorStatus* pErrorStatus)
{
    NuResult result = kNuOK;

    ASSERT(pErrorStatus != NULL);
    ASSERT(pErrorStatus->pathname != NULL);

    ASSERT(pErrorStatus->canOverwrite);
    ASSERT(pErrorStatus->canSkip);
    ASSERT(pErrorStatus->canAbort);
    ASSERT(!pErrorStatus->canRename);

    /* no firm policy, ask the user */
    ConfirmOverwriteDialog confOvwr;
    PathName path(pErrorStatus->pathname);
    
    confOvwr.fExistingFile = pErrorStatus->pRecord->filename;
    confOvwr.fExistingFileModWhen =
        DateTimeToSeconds(&pErrorStatus->pRecord->recModWhen);
    if (pErrorStatus->origPathname != NULL) {
        confOvwr.fNewFileSource = pErrorStatus->origPathname;
        PathName checkPath(confOvwr.fNewFileSource);
        confOvwr.fNewFileModWhen = checkPath.GetModWhen();
    } else {
        confOvwr.fNewFileSource = "???";
        confOvwr.fNewFileModWhen = kDateNone;
    }

    confOvwr.fAllowRename = false;
    if (confOvwr.DoModal() == IDCANCEL) {
        result = kNuAbort;
        goto bail;
    }
    if (confOvwr.fResultRename) {
        ASSERT(false);
        result = kNuAbort;
        goto bail;
    }
    if (confOvwr.fResultApplyToAll) {
        if (confOvwr.fResultOverwrite) {
            (void) NuSetValue(fpArchive, kNuValueHandleExisting,
                    kNuAlwaysOverwrite);
        } else {
            (void) NuSetValue(fpArchive, kNuValueHandleExisting,
                    kNuNeverOverwrite);
        }
    }
    if (confOvwr.fResultOverwrite)
        result = kNuOverwrite;
    else
        result = kNuSkip;

bail:
    return result;
}

/*
 * A file that used to be there isn't anymore.
 *
 * This should be exceedingly rare.
 */
NuResult
NufxArchive::HandleAddNotFound(const NuErrorStatus* pErrorStatus)
{
    CString errMsg;

    errMsg.Format(L"Failed while adding '%hs': file no longer exists.",
        pErrorStatus->pathname);
    ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);

    return kNuAbort;
}


/*
 * ===========================================================================
 *      NufxArchive -- test files
 * ===========================================================================
 */

/*
 * Test the records represented in the selection set.
 */
bool
NufxArchive::TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
    NuError nerr;
    NufxEntry* pEntry;
    CString errMsg;
    bool retVal = false;

    ASSERT(fpArchive != NULL);

    LOGI("Testing %d entries", pSelSet->GetNumEntries());

    SelectionEntry* pSelEntry = pSelSet->IterNext();
    while (pSelEntry != NULL) {
        pEntry = (NufxEntry*) pSelEntry->GetEntry();

        LOGI("  Testing %ld '%ls'", pEntry->GetRecordIdx(),
            pEntry->GetPathName());
        nerr = NuTestRecord(fpArchive, pEntry->GetRecordIdx());
        if (nerr != kNuErrNone) {
            if (nerr == kNuErrAborted) {
                CString title;
                title.LoadString(IDS_MB_APP_NAME);
                errMsg = "Cancelled.";
                pMsgWnd->MessageBox(errMsg, title, MB_OK);
            } else {
                errMsg.Format(L"Failed while testing '%ls': %hs.",
                    pEntry->GetPathName(), NuStrError(nerr));
                ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
            }
            goto bail;
        }

        pSelEntry = pSelSet->IterNext();
    }

    /* show success message */
    errMsg.Format(L"Tested %d file%ls, no errors found.",
        pSelSet->GetNumEntries(),
        pSelSet->GetNumEntries() == 1 ? L"" : L"s");
    pMsgWnd->MessageBox(errMsg);
    retVal = true;

bail:
    return retVal;
}


/*
 * ===========================================================================
 *      NufxArchive -- delete files
 * ===========================================================================
 */

/*
 * Delete the records represented in the selection set.
 */
bool
NufxArchive::DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
    NuError nerr;
    NufxEntry* pEntry;
    CString errMsg;
    bool retVal = false;

    ASSERT(fpArchive != NULL);

    LOGI("Deleting %d entries", pSelSet->GetNumEntries());

    /* mark entries for deletion */
    SelectionEntry* pSelEntry = pSelSet->IterNext();
    while (pSelEntry != NULL) {
        pEntry = (NufxEntry*) pSelEntry->GetEntry();

        LOGI("  Deleting %ld '%ls'", pEntry->GetRecordIdx(),
            pEntry->GetPathName());
        nerr = NuDeleteRecord(fpArchive, pEntry->GetRecordIdx());
        if (nerr != kNuErrNone) {
            errMsg.Format(L"Unable to delete record %d: %hs.",
                pEntry->GetRecordIdx(), NuStrError(nerr));
            ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }

        pSelEntry = pSelSet->IterNext();
    }

    /* actually do the delete */
    long statusFlags;
    nerr = NuFlush(fpArchive, &statusFlags);
    if (nerr != kNuErrNone) {
        errMsg.Format(L"Unable to delete all files: %hs.", NuStrError(nerr));
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);

        /* see if it got converted to read-only status */
        if (statusFlags & kNuFlushReadOnly)
            fIsReadOnly = true;
    }

    if (InternalReload(fpMsgWnd) == kNuErrNone)
        retVal = true;

bail:
    return retVal;
}


/*
 * ===========================================================================
 *      NufxArchive -- rename files
 * ===========================================================================
 */

/*
 * Rename the records represented in the selection set.
 */
bool
NufxArchive::RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
    CString errMsg;
    NuError nerr;
    bool retVal = false;

    ASSERT(fpArchive != NULL);

    LOGI("Renaming %d entries", pSelSet->GetNumEntries());

    /*
     * Figure out if we're allowed to change the entire path.  (This is
     * doing it the hard way, but what the hell.)
     */
    long cap = GetCapability(GenericArchive::kCapCanRenameFullPath);
    bool renameFullPath = (cap != 0);

    LOGI("Rename, fullpath=%d", renameFullPath);

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
        NufxEntry* pEntry = (NufxEntry*) pSelEntry->GetEntry();
        LOGI("  Renaming '%ls'", pEntry->GetPathName());

        RenameEntryDialog renameDlg(pMsgWnd);
        renameDlg.SetCanRenameFullPath(renameFullPath);
        renameDlg.SetCanChangeFssep(true);
        renameDlg.fOldName = pEntry->GetPathName();
        renameDlg.fFssep = pEntry->GetFssep();
        renameDlg.fpArchive = this;
        renameDlg.fpEntry = pEntry;

        int result = renameDlg.DoModal();
        if (result == IDOK) {
            if (renameDlg.fFssep == '\0')
                renameDlg.fFssep = kNufxNoFssep;
            CStringA newNameA(renameDlg.fNewName);
            nerr = NuRename(fpArchive, pEntry->GetRecordIdx(),
                newNameA, renameDlg.fFssep);
            if (nerr != kNuErrNone) {
                errMsg.Format(L"Unable to rename '%ls': %hs.", pEntry->GetPathName(),
                    NuStrError(nerr));
                ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
                break;
            }
            LOGI("Rename of '%ls' to '%ls' succeeded",
                pEntry->GetDisplayName(), (LPCWSTR) renameDlg.fNewName);
        } else if (result == IDCANCEL) {
            LOGI("Canceling out of remaining renames");
            break;
        } else {
            /* 3rd possibility is IDIGNORE, i.e. skip this entry */
            LOGI("Skipping rename of '%ls'", pEntry->GetDisplayName());
        }

        pSelEntry = pSelSet->IterNext();
    }

    /* flush pending rename calls */
    {
        CWaitCursor waitc;

        long statusFlags;
        nerr = NuFlush(fpArchive, &statusFlags);
        if (nerr != kNuErrNone) {
            errMsg.Format(L"Unable to rename all files: %hs.",
                NuStrError(nerr));
            ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);

            /* see if it got converted to read-only status */
            if (statusFlags & kNuFlushReadOnly)
                fIsReadOnly = true;
        }
    }

    /* reload GenericArchive from NufxLib */
    if (InternalReload(fpMsgWnd) == kNuErrNone)
        retVal = true;

    return retVal;
}


/*
 * Verify that the a name is suitable.  Called by RenameEntryDialog.
 *
 * Tests for context-specific syntax and checks for duplicates.
 *
 * Returns an empty string on success, or an error message on failure.
 */
CString
NufxArchive::TestPathName(const GenericEntry* pGenericEntry,
    const CString& basePath, const CString& newName, char newFssep) const
{
    CString errMsg;
    ASSERT(pGenericEntry != NULL);

    ASSERT(basePath.IsEmpty());

    /* can't start or end with fssep */
    if (newName.Left(1) == newFssep || newName.Right(1) == newFssep) {
        errMsg.Format(L"Names in NuFX archives may not start or end with a "
                      L"path separator character (%c).",
            newFssep);
        goto bail;
    }

    /* if it's a disk image, don't allow complex paths */
    if (pGenericEntry->GetRecordKind() == GenericEntry::kRecordKindDisk) {
        if (newName.Find(newFssep) != -1) {
            errMsg.Format(L"Disk image names may not contain a path separator "
                          L"character (%c).",
                newFssep);
            goto bail;
        }
    }

    /*
     * Test for case-sensitive name collisions.  Each individual path
     * component must be compared
     */
    GenericEntry* pEntry;
    pEntry = GetEntries();
    while (pEntry != NULL) {
        if (pEntry != pGenericEntry &&
            ComparePaths(pEntry->GetPathName(), pEntry->GetFssep(),
                         newName, newFssep) == 0)
        {
            errMsg = L"An entry with that name already exists.";
        }

        pEntry = pEntry->GetNext();
    }

bail:
    return errMsg;
}


/*
 * ===========================================================================
 *      NufxArchive -- recompress
 * ===========================================================================
 */

/*
 * Recompress the files in the selection set.
 *
 * We have to uncompress the files into memory and then recompress them.
 * We don't want to flush after every file (too slow), but we can't wait
 * until they're expanded (unbounded memory requirements).  So we have
 * to keep expanding until we reach a certain limit, then call flush to
 * push the changes out.
 *
 * Since we're essentially making the changes in place (it's actually
 * all getting routed through the temp file), we need to delete the thread
 * and re-add it.  This isn't quite as thorough as "launder", which
 * actually reconstructs the entire record.
 */
bool
NufxArchive::RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
    const RecompressOptionsDialog* pRecompOpts)
{
    const int kMaxSizeInMemory = 2 * 1024 * 1024;   // 2MB
    CString errMsg;
    NuError nerr;
    bool retVal = false;

    /* set the compression type */
    nerr = NuSetValue(fpArchive, kNuValueDataCompression,
            pRecompOpts->fCompressionType + kNuCompressNone);
    if (nerr != kNuErrNone) {
        LOGI("GLITCH: unable to set compression type to %d",
            pRecompOpts->fCompressionType);
        /* keep going */
    }

    fProgressAsRecompress = true;

    /*
     * Loop over all items in the selection set.  Because the selection
     * set has one entry for each interesting thread, we don't need to
     * pry the NuRecord open and play with it.
     *
     * We should only be here for data forks, resource forks, and disk
     * images.  Comments and filenames are not compressed, and so cannot
     * be recompressed.
     */
    SelectionEntry* pSelEntry = pSelSet->IterNext();
    long sizeInMemory = 0;
    bool result = true;
    NufxEntry* pEntry = NULL;
    for ( ; pSelEntry != NULL; pSelEntry = pSelSet->IterNext()) {
        pEntry = (NufxEntry*) pSelEntry->GetEntry();

        /*
         * Compress each thread in turn.
         */
        if (pEntry->GetHasDataFork()) {
            result = RecompressThread(pEntry, GenericEntry::kDataThread,
                        pRecompOpts, &sizeInMemory, &errMsg);
            if (!result)
                break;
        }
        if (pEntry->GetHasRsrcFork()) {
            result = RecompressThread(pEntry, GenericEntry::kRsrcThread,
                        pRecompOpts, &sizeInMemory, &errMsg);
            if (!result)
                break;
        }
        if (pEntry->GetHasDiskImage()) {
            result = RecompressThread(pEntry, GenericEntry::kDiskImageThread,
                        pRecompOpts, &sizeInMemory, &errMsg);
            if (!result)
                break;
        }
        /* don't do anything with comments */

        /* if we're sitting on too much, push it out */
        if (sizeInMemory > kMaxSizeInMemory) {
            /* flush anything pending */
            long statusFlags;
            nerr = NuFlush(fpArchive, &statusFlags);
            if (nerr != kNuErrNone) {
                if (nerr != kNuErrAborted) {
                    errMsg.Format(L"Unable to recompress all files: %hs.",
                        NuStrError(nerr));
                    ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
                } else {
                    LOGI("Cancelled out of sub-flush/compress");
                }

                /* see if it got converted to read-only status */
                if (statusFlags & kNuFlushReadOnly)
                    fIsReadOnly = true;

                goto bail;
            }

            sizeInMemory = 0;
        }
    }

    /* handle errors that threw us out of the while loop */
    if (!result) {
        ASSERT(pEntry != NULL);
        CString dispStr;
        dispStr.Format(L"Failed while recompressing '%ls': %ls.",
            pEntry->GetDisplayName(), (LPCWSTR) errMsg);
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        goto bail;
    }


    /* flush anything pending */
    long statusFlags;
    nerr = NuFlush(fpArchive, &statusFlags);
    if (nerr != kNuErrNone) {
        if (nerr != kNuErrAborted) {
            errMsg.Format(L"Unable to recompress all files: %hs.",
                NuStrError(nerr));
            ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        } else {
            LOGI("Cancelled out of flush/compress");
        }

        /* see if it got converted to read-only status */
        if (statusFlags & kNuFlushReadOnly)
            fIsReadOnly = true;
    } else {
        retVal = true;
    }

bail:
    /* abort anything that didn't get flushed */
    NuAbort(fpArchive);
    /* reload to pick up changes */
    (void) InternalReload(pMsgWnd);

    fProgressAsRecompress = false;
    return retVal;
}

/*
 * Recompress one thread.
 *
 * Returns "true" if things went okay, "false" on a fatal failure.
 */
bool
NufxArchive::RecompressThread(NufxEntry* pEntry, int threadKind,
    const RecompressOptionsDialog* pRecompOpts, long* pSizeInMemory,
    CString* pErrMsg)
{
    NuThread thread;
    NuThreadID threadID;
    NuError nerr;
    NuDataSource* pSource = NULL;
    CString subErrMsg;
    bool retVal = false;
    char* buf = NULL;
    long len = 0;

    LOGI("  Recompressing %ld '%ls'", pEntry->GetRecordIdx(),
        pEntry->GetDisplayName());

    /* get a copy of the thread header */
    pEntry->FindThreadInfo(threadKind, &thread, pErrMsg);
    if (!pErrMsg->IsEmpty()) {
        pErrMsg->Format(L"Unable to locate thread for %ls (type %d)",
            pEntry->GetDisplayName(), threadKind);
        goto bail;
    }
    threadID = NuGetThreadID(&thread);

    /* if it's already in the target format, skip it */
    if (thread.thThreadFormat == pRecompOpts->fCompressionType) {
        LOGI("Skipping (fmt=%d) '%ls'",
            pRecompOpts->fCompressionType, pEntry->GetDisplayName());
        return true;
    }

    /* extract the thread */
    int result;
    result = pEntry->ExtractThreadToBuffer(threadKind, &buf, &len, &subErrMsg);
    if (result == IDCANCEL) {
        LOGI("Cancelled during extract!");
        ASSERT(buf == NULL);
        goto bail;  /* abort anything that was pending */
    } else if (result != IDOK) {
        pErrMsg->Format(L"Failed while extracting '%ls': %ls",
            pEntry->GetDisplayName(), (LPCWSTR) subErrMsg);
        goto bail;
    }
    *pSizeInMemory += len;

    /* create a data source for it */
    nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
            0, (const BYTE*)buf, 0, len, ArrayDeleteHandler,
            &pSource);
    if (nerr != kNuErrNone) {
        pErrMsg->Format(L"Unable to create NufxLib data source (len=%d).",
            len);
        goto bail;
    }
    buf = NULL;      // data source owns it now

    /* delete the existing thread */
    //LOGI("+++ DELETE threadIdx=%d", thread.threadIdx);
    nerr = NuDeleteThread(fpArchive, thread.threadIdx);
    if (nerr != kNuErrNone) {
        pErrMsg->Format(L"Unable to delete thread %d: %hs",
            pEntry->GetRecordIdx(), NuStrError(nerr));
        goto bail;
    }

    /* mark the new thread for addition */
    //LOGI("+++ ADD threadID=0x%08lx", threadID);
    nerr = NuAddThread(fpArchive, pEntry->GetRecordIdx(), threadID,
                pSource, NULL);
    if (nerr != kNuErrNone) {
        pErrMsg->Format(L"Unable to add thread type %d: %hs",
            threadID, NuStrError(nerr));
        goto bail;
    }
    pSource = NULL;      // now owned by nufxlib

    /* at this point, we just wait for the flush in the outer loop */
    retVal = true;

bail:
    NuFreeDataSource(pSource);
    return retVal;
}


/*
 * ===========================================================================
 *      NufxArchive -- transfer files to another archive
 * ===========================================================================
 */

/*
 * Transfer the selected files out of this archive and into another.
 *
 * We get one entry in the selection set per record.
 *
 * I think this now throws kXferCancelled whenever it's supposed to.  Not
 * 100% sure, but it looks good.
 */
GenericArchive::XferStatus
NufxArchive::XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
    ActionProgressDialog* pActionProgress, const XferFileOptions* pXferOpts)
{
    LOGI("NufxArchive XferSelection!");
    XferStatus retval = kXferFailed;
    unsigned char* dataBuf = NULL;
    unsigned char* rsrcBuf = NULL;
    CString errMsg, dispMsg;

    pXferOpts->fTarget->XferPrepare(pXferOpts);

    SelectionEntry* pSelEntry = pSelSet->IterNext();
    for ( ; pSelEntry != NULL; pSelEntry = pSelSet->IterNext()) {
        long dataLen=-1, rsrcLen=-1;
        NufxEntry* pEntry = (NufxEntry*) pSelEntry->GetEntry();
        FileDetails fileDetails;
        CString errMsg;

        ASSERT(dataBuf == NULL);
        ASSERT(rsrcBuf == NULL);

        /* in case we start handling CRC errors better */
        if (pEntry->GetDamaged()) {
            LOGI("  XFER skipping damaged entry '%ls'",
                pEntry->GetDisplayName());
            continue;
        }

        LOGI(" XFER converting '%ls'", pEntry->GetDisplayName());

        fileDetails.storageName = pEntry->GetDisplayName();
        fileDetails.fileType = pEntry->GetFileType();
        fileDetails.fileSysFmt = DiskImg::kFormatUnknown;
        fileDetails.fileSysInfo = PathProposal::kDefaultStoredFssep;
        fileDetails.access = pEntry->GetAccess();
        fileDetails.extraType = pEntry->GetAuxType();
        fileDetails.storageType = kNuStorageSeedling;

        time_t when;
        when = time(NULL);
        UNIXTimeToDateTime(&when, &fileDetails.archiveWhen);
        when = pEntry->GetModWhen();
        UNIXTimeToDateTime(&when, &fileDetails.modWhen);
        when = pEntry->GetCreateWhen();
        UNIXTimeToDateTime(&when, &fileDetails.createWhen);

        pActionProgress->SetArcName(fileDetails.storageName);
        if (pActionProgress->SetProgress(0) == IDCANCEL) {
            retval = kXferCancelled;
            goto bail;
        }

        /*
         * Handle all relevant threads in this record.  We assume it's either
         * a data/rsrc pair or a disk image.
         */
        if (pEntry->GetHasDataFork()) {
            /*
             * Found a data thread.
             */
            int result;
            dataBuf = NULL;
            dataLen = 0;
            result = pEntry->ExtractThreadToBuffer(GenericEntry::kDataThread,
                        (char**) &dataBuf, &dataLen, &errMsg);
            if (result == IDCANCEL) {
                LOGI("Cancelled during data extract!");
                retval = kXferCancelled;
                goto bail;  /* abort anything that was pending */
            } else if (result != IDOK) {
                dispMsg.Format(L"Failed while extracting '%ls': %ls.",
                    pEntry->GetDisplayName(), (LPCWSTR) errMsg);
                ShowFailureMsg(pMsgWnd, dispMsg, IDS_FAILED);
                goto bail;
            }
            ASSERT(dataBuf != NULL);
            ASSERT(dataLen >= 0);

        } else if (pEntry->GetHasDiskImage()) {
            /*
             * No data thread found.  Look for a disk image.
             */
            int result;
            dataBuf = NULL;
            dataLen = 0;
            result = pEntry->ExtractThreadToBuffer(GenericEntry::kDiskImageThread,
                        (char**) &dataBuf, &dataLen, &errMsg);
            if (result == IDCANCEL) {
                LOGI("Cancelled during data extract!");
                goto bail;  /* abort anything that was pending */
            } else if (result != IDOK) {
                dispMsg.Format(L"Failed while extracting '%ls': %ls.",
                    pEntry->GetDisplayName(), (LPCWSTR) errMsg);
                ShowFailureMsg(pMsgWnd, dispMsg, IDS_FAILED);
                goto bail;
            }
            ASSERT(dataBuf != NULL);
            ASSERT(dataLen >= 0);
        }

        /*
         * See if there's a resource fork in here (either by itself or
         * with a data fork).
         */
        if (pEntry->GetHasRsrcFork()) {
            int result;
            rsrcBuf = NULL;
            rsrcLen = 0;
            result = pEntry->ExtractThreadToBuffer(GenericEntry::kRsrcThread,
                        (char**) &rsrcBuf, &rsrcLen, &errMsg);
            if (result == IDCANCEL) {
                LOGI("Cancelled during rsrc extract!");
                goto bail;  /* abort anything that was pending */
            } else if (result != IDOK) {
                dispMsg.Format(L"Failed while extracting '%ls': %ls.",
                    pEntry->GetDisplayName(), (LPCWSTR) errMsg);
                ShowFailureMsg(pMsgWnd, dispMsg, IDS_FAILED);
                goto bail;
            }

            fileDetails.storageType = kNuStorageExtended;
        } else {
            ASSERT(rsrcBuf == NULL);
        }

        if (dataLen < 0 && rsrcLen < 0) {
            LOGI(" XFER: WARNING: nothing worth transferring in '%ls'",
                pEntry->GetDisplayName());
            continue;
        }

        errMsg = pXferOpts->fTarget->XferFile(&fileDetails, &dataBuf, dataLen,
                    &rsrcBuf, rsrcLen);
        if (!errMsg.IsEmpty()) {
            LOGI("XferFile failed!");
            errMsg.Format(L"Failed while transferring '%ls': %ls.",
                pEntry->GetDisplayName(), (LPCWSTR) errMsg);
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

/*
 * Prepare to transfer files into a NuFX archive.
 *
 * We set the "allow duplicates" flag because DOS 3.3 volumes can have
 * files with duplicate names.
 */
void
NufxArchive::XferPrepare(const XferFileOptions* pXferOpts)
{
    LOGI("  NufxArchive::XferPrepare");
    (void) NuSetValue(fpArchive, kNuValueAllowDuplicates, true);
}

/*
 * Transfer the data and optional resource fork of a single file into the
 * NuFX archive.
 *
 * "dataLen" and "rsrcLen" will be -1 if the corresponding fork doesn't
 * exist.
 *
 * Returns 0 on success, -1 on failure.  On success, "*pDataBuf" and
 * "*pRsrcBuf" are set to NULL (ownership transfers to NufxLib).
 */
CString
NufxArchive::XferFile(FileDetails* pDetails, unsigned char** pDataBuf,
    long dataLen, unsigned char** pRsrcBuf, long rsrcLen)
{
    NuError nerr;
    const int kFileTypeTXT = 0x04;
    NuDataSource* pSource = NULL;
    CString errMsg;

    LOGI("  NufxArchive::XferFile '%ls'", (LPCWSTR) pDetails->storageName);
    LOGI("  dataBuf=0x%08lx dataLen=%ld rsrcBuf=0x%08lx rsrcLen=%ld",
        *pDataBuf, dataLen, *pRsrcBuf, rsrcLen);
    ASSERT(pDataBuf != NULL);
    ASSERT(pRsrcBuf != NULL);

    /* NuFX doesn't explicitly store directories */
    if (pDetails->entryKind == FileDetails::kFileKindDirectory) {
        delete[] *pDataBuf;
        delete[] *pRsrcBuf;
        *pDataBuf = *pRsrcBuf = NULL;
        goto bail;
    }

    ASSERT(dataLen >= 0 || rsrcLen >= 0);
    ASSERT(*pDataBuf != NULL || *pRsrcBuf != NULL);

    /* add the record; we have "allow duplicates" enabled for clashes */
    NuRecordIdx recordIdx;
    NuFileDetails nuFileDetails;
    nuFileDetails = *pDetails;

    /*
     * Odd bit of trivia: NufxLib refuses to accept an fssep of '\0'.  It
     * really wants to have one.  Which is annoying, since files coming
     * from DOS or Pascal don't have one.  We therefore need to supply
     * one, so we provide 0xff on the theory that nobody in their right
     * mind would have it in an Apple II filename.
     *
     * Since we don't strip Pascal and ProDOS names down when we load
     * the disks, it's possible the for 0xff to occur if the disk got
     * damaged.  For ProDOS we don't care, since it has an fssep, but
     * Pascal could be at risk.  DOS and RDOS are sanitized and so should
     * be okay.
     *
     * One issue: we don't currently allow changing the fssep when renaming
     * a file.  We need to fix this, or else there's no way to rename a
     * file into a subdirectory once it has been pasted in this way.
     */
    if (NuGetSepFromSysInfo(nuFileDetails.fileSysInfo) == 0) {
        nuFileDetails.fileSysInfo =
            NuSetSepInSysInfo(nuFileDetails.fileSysInfo, kNufxNoFssep);
    }

    nerr = NuAddRecord(fpArchive, &nuFileDetails, &recordIdx);
    if (nerr != kNuErrNone) {
        if (nerr != kNuErrAborted) {
            errMsg.Format(L"Failed adding record: %hs", NuStrError(nerr));
            //ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        }
        // else the add was cancelled
        goto bail;
    }

    if (dataLen >= 0) {
        ASSERT(*pDataBuf != NULL);

        /* strip the high ASCII from DOS and RDOS text files */
        if (pDetails->entryKind != FileDetails::kFileKindDiskImage &&
            pDetails->fileType == kFileTypeTXT &&
            DiskImg::UsesDOSFileStructure(pDetails->fileSysFmt))
        {
            LOGI(" Stripping high ASCII from '%ls'",
                (LPCWSTR) pDetails->storageName);
            unsigned char* ucp = *pDataBuf;
            long len = dataLen;

            while (len--)
                *ucp++ &= 0x7f;
        }

        /* create a data source for the data fork; might be zero len */
        nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed, 0,
                *pDataBuf, 0, dataLen, ArrayDeleteHandler, &pSource);
        if (nerr != kNuErrNone) {
            errMsg = "Unable to create NufxLib data source.";
            //ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }
        *pDataBuf = NULL;        /* owned by data source */

        /* add the data fork, as a disk image if appropriate */
        NuThreadID targetID;
        if (pDetails->entryKind == FileDetails::kFileKindDiskImage)
            targetID = kNuThreadIDDiskImage;
        else
            targetID = kNuThreadIDDataFork;

        nerr = NuAddThread(fpArchive, recordIdx, targetID, pSource, NULL);
        if (nerr != kNuErrNone) {
            errMsg.Format(L"Failed adding thread: %hs.", NuStrError(nerr));
            //ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }
        pSource = NULL;      /* NufxLib owns it now */
    }

    /* add the resource fork, if one was provided */
    if (rsrcLen >= 0) {
        ASSERT(*pRsrcBuf != NULL);

        nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed, 0,
                *pRsrcBuf, 0, rsrcLen, ArrayDeleteHandler, &pSource);
        if (nerr != kNuErrNone) {
            errMsg = L"Unable to create NufxLib data source.";
            //ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }
        *pRsrcBuf = NULL;        /* owned by data source */

        /* add the data fork */
        nerr = NuAddThread(fpArchive, recordIdx, kNuThreadIDRsrcFork,
                pSource, NULL);
        if (nerr != kNuErrNone) {
            errMsg.Format(L"Failed adding thread: %hs.", NuStrError(nerr));
            //ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }
        pSource = NULL;      /* NufxLib owns it now */
    }

bail:
    NuFreeDataSource(pSource);
    return errMsg;
}

/*
 * Abort the transfer.
 *
 * Since we don't do any interim flushes, we can just call NuAbort.  If that
 * weren't the case, we would need to delete all records and flush.
 */
void
NufxArchive::XferAbort(CWnd* pMsgWnd)
{
    NuError nerr;
    CString errMsg;

    LOGI("  NufxArchive::XferAbort");

    nerr = NuAbort(fpArchive);
    if (nerr != kNuErrNone) {
        errMsg.Format(L"Failed while aborting procedure: %hs.", NuStrError(nerr));
        ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
    }
}

/*
 * Flush all changes to the archive.
 */
void
NufxArchive::XferFinish(CWnd* pMsgWnd)
{
    NuError nerr;
    CString errMsg;

    LOGI("  NufxArchive::XferFinish");

    /* actually do the work */
    long statusFlags;
    nerr = NuFlush(fpArchive, &statusFlags);
    if (nerr != kNuErrNone) {
        if (nerr != kNuErrAborted) {
            errMsg.Format(L"Unable to add file: %hs.", NuStrError(nerr));
            ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
        }

        /* see if it got converted to read-only status */
        if (statusFlags & kNuFlushReadOnly)
            fIsReadOnly = true;
        goto bail;
    }

    (void) InternalReload(fpMsgWnd);

bail:
    return;
}


/*
 * ===========================================================================
 *      NufxArchive -- add/update/delete comments
 * ===========================================================================
 */

/*
 * Extract a comment from the archive, converting line terminators to CRLF.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
NufxArchive::GetComment(CWnd* pMsgWnd, const GenericEntry* pGenericEntry,
    CString* pStr)
{
    NufxEntry* pEntry = (NufxEntry*) pGenericEntry;
    CString errMsg;
    const char* kNewEOL = "\r\n";
    int result;
    char* buf;
    long len;

    ASSERT(pGenericEntry->GetHasComment());

    /* use standard extract function to pull comment out */
    buf = NULL;
    len = 0;
    result = pEntry->ExtractThreadToBuffer(GenericEntry::kCommentThread,
                &buf, &len, &errMsg);
    if (result != IDOK) {
        LOGI("Failed getting comment: %hs", buf);
        ASSERT(buf == NULL);
        return false;
    }

    /* convert EOL and add '\0' */
    CString convStr;
    const char* ccp;

    ccp = buf;
    while (len-- && *ccp != '\0') {
        if (len > 1 && *ccp == '\r' && *(ccp+1) == '\n') {
            ccp++;
            len--;
            convStr += kNewEOL;
        } else if (*ccp == '\r' || *ccp == '\n') {
            convStr += kNewEOL;
        } else {
            convStr += *ccp;
        }
        ccp++;
    }

    *pStr = convStr;
    delete[] buf;
    return true;
}

/*
 * Set the comment.  This requires either adding a new comment or updating
 * an existing one.  The latter is constrained by the maximum size of the
 * comment buffer.
 *
 * We want to update in place whenever possible because it's faster (don't
 * have to rewrite the entire archive), but that really only holds for new
 * archives or if we foolishly set the kNuValueModifyOrig flag.
 *
 * Cleanest approach is to delete the existing thread and add a new one.
 * If somebody complains we can try to be smarter about it.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
NufxArchive::SetComment(CWnd* pMsgWnd, GenericEntry* pGenericEntry,
    const CString& str)
{
    NuDataSource* pSource = NULL;
    NufxEntry* pEntry = (NufxEntry*) pGenericEntry;
    NuError nerr;
    bool retVal = false;

    /* convert CRLF to CR */
    CStringA newStr(str);
    char* srcp;
    char* dstp;
    srcp = dstp = newStr.GetBuffer(0);
    while (*srcp != '\0') {
        if (*srcp == '\r' && *(srcp+1) == '\n') {
            srcp++;
            *dstp = '\r';
        } else {
            *dstp = *srcp;
        }
        srcp++;
        dstp++;
    }
    *dstp = '\0';
    newStr.ReleaseBuffer();

    /* get the thread info */
    CString errMsg;
    NuThread thread;
    NuThreadIdx threadIdx;

    pEntry->FindThreadInfo(GenericEntry::kCommentThread, &thread, &errMsg);
    threadIdx = thread.threadIdx;
    if (errMsg.IsEmpty()) {
        /* delete existing thread */
        nerr = NuDeleteThread(fpArchive, threadIdx);
        if (nerr != kNuErrNone) {
            errMsg.Format(L"Unable to delete thread: %hs.", NuStrError(nerr));
            goto bail;
        }
    }

    /* set a maximum pre-size value for the thread */
    long maxLen;
    maxLen = ((newStr.GetLength() + 99) / 100) * 100;
    if (maxLen < 200)
        maxLen = 200;


    /* create a data source to write from */
    nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
            maxLen, (const BYTE*)(LPCSTR)newStr, 0,
            newStr.GetLength(), NULL, &pSource);
    if (nerr != kNuErrNone) {
        errMsg.Format(L"Unable to create NufxLib data source (len=%d, maxLen=%d).",
            newStr.GetLength(), maxLen);
        goto bail;
    }

    /* add the new thread */
    nerr = NuAddThread(fpArchive, pEntry->GetRecordIdx(),
            kNuThreadIDComment, pSource, NULL);
    if (nerr != kNuErrNone) {
        errMsg.Format(L"Unable to add comment thread: %hs.",
            NuStrError(nerr));
        goto bail;
    }
    pSource = NULL;  // nufxlib owns it now

    /* flush changes */
    long statusFlags;
    nerr = NuFlush(fpArchive, &statusFlags);
    if (nerr != kNuErrNone) {
        errMsg.Format(L"Unable to flush comment changes: %hs.",
            NuStrError(nerr));
        goto bail;
    }

    /* reload GenericArchive from NufxLib */
    if (InternalReload(fpMsgWnd) == kNuErrNone)
        retVal = true;

bail:
    NuFreeDataSource(pSource);
    if (!retVal) {
        LOGI("FAILED: %ls", (LPCWSTR) errMsg);
        NuAbort(fpArchive);
    }
    return retVal;
}

/*
 * Remove a comment.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
NufxArchive::DeleteComment(CWnd* pMsgWnd, GenericEntry* pGenericEntry)
{
    CString errMsg;
    NuError nerr;
    NufxEntry* pEntry = (NufxEntry*) pGenericEntry;
    NuThread thread;
    NuThreadIdx threadIdx;
    bool retVal = false;

    pEntry->FindThreadInfo(GenericEntry::kCommentThread, &thread, &errMsg);
    if (!errMsg.IsEmpty())
        goto bail;
    threadIdx = thread.threadIdx;

    nerr = NuDeleteThread(fpArchive, threadIdx);
    if (nerr != kNuErrNone) {
        errMsg.Format(L"Unable to delete thread: %hs.", NuStrError(nerr));
        goto bail;
    }

    /* flush changes */
    long statusFlags;
    nerr = NuFlush(fpArchive, &statusFlags);
    if (nerr != kNuErrNone) {
        errMsg.Format(L"Unable to flush comment deletion: %hs.",
            NuStrError(nerr));
        goto bail;
    }

    /* reload GenericArchive from NufxLib */
    if (InternalReload(pMsgWnd) == kNuErrNone)
        retVal = true;

bail:
    if (retVal != 0) {
        LOGI("FAILED: %ls", (LPCWSTR) errMsg);
        NuAbort(fpArchive);
    }
    return retVal;
}


/*
 * Set file properties via the NuSetRecordAttr call.
 *
 * Get the existing properties, copy the fields from FileProps over, and
 * set them.
 *
 * [currently only supports file type, aux type, and access flags]
 *
 * Technically we should reload the GenericArchive from the NufxArchive,
 * but the set of changes is pretty small, so we just make them here.
 */
bool
NufxArchive::SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
    const FileProps* pProps)
{
    NuError nerr;
    NufxEntry* pNufxEntry = (NufxEntry*) pEntry;
    const NuRecord* pRecord;
    NuRecordAttr recordAttr;

    LOGI(" SET fileType=0x%02x auxType=0x%04x access=0x%02x",
        pProps->fileType, pProps->auxType, pProps->access);

    nerr = NuGetRecord(fpArchive, pNufxEntry->GetRecordIdx(), &pRecord);
    if (nerr != kNuErrNone) {
        LOGI("ERROR: couldn't find recordIdx %ld: %hs",
            pNufxEntry->GetRecordIdx(), NuStrError(nerr));
        return false;
    }

    NuRecordCopyAttr(&recordAttr, pRecord);
    recordAttr.fileType = pProps->fileType;
    recordAttr.extraType = pProps->auxType;
    recordAttr.access = pProps->access;

    nerr = NuSetRecordAttr(fpArchive, pNufxEntry->GetRecordIdx(), &recordAttr);
    if (nerr != kNuErrNone) {
        LOGI("ERROR: couldn't set recordAttr %ld: %hs",
            pNufxEntry->GetRecordIdx(), NuStrError(nerr));
        return false;
    }

    long statusFlags;
    nerr = NuFlush(fpArchive, &statusFlags);
    if (nerr != kNuErrNone) {
        LOGI("ERROR: NuFlush failed: %hs", NuStrError(nerr));

        /* see if it got converted to read-only status */
        if (statusFlags & kNuFlushReadOnly)
            fIsReadOnly = true;
        return false;
    }

    LOGI("Props set");

    /* do this in lieu of reloading GenericArchive */
    pEntry->SetFileType(pProps->fileType);
    pEntry->SetAuxType(pProps->auxType);
    pEntry->SetAccess(pProps->access);

    return true;
}
