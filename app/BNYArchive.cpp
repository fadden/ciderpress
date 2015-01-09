/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Binary II file support.
 */
#include "stdafx.h"
#include "BNYArchive.h"
#include "NufxArchive.h"
#include "Preferences.h"
#include "Main.h"
#include "Squeeze.h"
#include <errno.h>


/*
 * ===========================================================================
 *      BnyEntry
 * ===========================================================================
 */

int BnyEntry::ExtractThreadToBuffer(int which, char** ppText, long* pLength,
    CString* pErrMsg) const
{
    NuError nerr;
    ExpandBuffer expBuf;
    char* dataBuf = NULL;
    long len;
    bool needAlloc = true;
    int result = -1;

    ASSERT(fpArchive != NULL);
    ASSERT(fpArchive->fFp != NULL);

    if (*ppText != NULL)
        needAlloc = false;

    if (which != kDataThread) {
        *pErrMsg = "No such fork";
        goto bail;
    }

    len = (long) GetUncompressedLen();
    if (len == 0) {
        if (needAlloc) {
            *ppText = new char[1];
            **ppText = '\0';
        }
        *pLength = 0;
        result = IDOK;
        goto bail;
    }

    SET_PROGRESS_BEGIN();

    errno = 0;
    if (fseek(fpArchive->fFp, fOffset, SEEK_SET) < 0) {
        pErrMsg->Format(L"Unable to seek to offset %ld: %hs",
            fOffset, strerror(errno));
        goto bail;
    }

    if (GetSqueezed()) {
        nerr = UnSqueeze(fpArchive->fFp, (unsigned long) GetUncompressedLen(),
                    &expBuf, true, kBNYBlockSize);
        if (nerr != kNuErrNone) {
            pErrMsg->Format(L"File read failed: %hs", NuStrError(nerr));
            goto bail;
        }

        char* unsqBuf = NULL;
        long unsqLen = 0;
        expBuf.SeizeBuffer(&unsqBuf, &unsqLen);
        LOGI("Unsqueezed %ld bytes to %d", len, unsqLen);
        if (unsqLen == 0) {
            // some bonehead squeezed a zero-length file
            delete[] unsqBuf;
            ASSERT(*ppText == NULL);
            LOGI("Handling zero-length squeezed file!");
            if (needAlloc) {
                *ppText = new char[1];
                **ppText = '\0';
            }
            *pLength = 0;
        } else {
            if (needAlloc) {
                /* just use the seized buffer */
                *ppText = unsqBuf;
                *pLength = unsqLen;
            } else {
                if (*pLength < unsqLen) {
                    pErrMsg->Format(L"buf size %ld too short (%ld)",
                        *pLength, unsqLen);
                    delete[] unsqBuf;
                    goto bail;
                }

                memcpy(*ppText, unsqBuf, unsqLen);
                delete[] unsqBuf;
                *pLength = unsqLen;
            }
        }

    } else {
        if (needAlloc) {
            dataBuf = new char[len];
            if (dataBuf == NULL) {
                pErrMsg->Format(L"allocation of %ld bytes failed", len);
                goto bail;
            }
        } else {
            if (*pLength < (long) len) {
                pErrMsg->Format(L"buf size %ld too short (%ld)",
                    *pLength, len);
                goto bail;
            }
            dataBuf = *ppText;
        }
        if (fread(dataBuf, len, 1, fpArchive->fFp) != 1) {
            pErrMsg->Format(L"File read failed: %hs", strerror(errno));
            goto bail;
        }

        if (needAlloc)
            *ppText = dataBuf;
        *pLength = len;
    }

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
    return result;
}

int BnyEntry::ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
    ConvertHighASCII convHA, CString* pErrMsg) const
{
    NuError nerr;
    long len;
    int result = -1;

    ASSERT(IDOK != -1 && IDCANCEL != -1);
    if (which != kDataThread) {
        *pErrMsg = L"No such fork";
        goto bail;
    }

    len = (long) GetUncompressedLen();
    if (len == 0) {
        LOGI("Empty fork");
        result = IDOK;
        goto bail;
    }

    errno = 0;
    if (fseek(fpArchive->fFp, fOffset, SEEK_SET) < 0) {
        pErrMsg->Format(L"Unable to seek to offset %ld: %hs",
            fOffset, strerror(errno));
        goto bail;
    }

    SET_PROGRESS_BEGIN();

    /*
     * Generally speaking, anything in a BNY file is going to be small.  The
     * major exception is a BXY file, which could be huge.  However, the
     * SHK embedded in a BXY is never squeezed.
     *
     * To make life easy, we either unsqueeze the entire thing into a buffer
     * and then write that, or we do a file-to-file copy of the specified
     * number of bytes.
     */
    if (GetSqueezed()) {
        ExpandBuffer expBuf;
        bool lastCR = false;
        char* buf;
        long uncLen;

        nerr = UnSqueeze(fpArchive->fFp, (unsigned long) GetUncompressedLen(),
                    &expBuf, true, kBNYBlockSize);
        if (nerr != kNuErrNone) {
            pErrMsg->Format(L"File read failed: %hs", NuStrError(nerr));
            goto bail;
        }

        expBuf.SeizeBuffer(&buf, &uncLen);
        LOGI("Unsqueezed %ld bytes to %d", len, uncLen);

        // some bonehead squeezed a zero-length file
        if (uncLen == 0) {
            ASSERT(buf == NULL);
            LOGI("Handling zero-length squeezed file!");
            result = IDOK;
            goto bail;
        }

        int err = GenericEntry::WriteConvert(outfp, buf, uncLen, &conv,
                        &convHA, &lastCR);
        if (err != 0) {
            pErrMsg->Format(L"File write failed: %hs", strerror(err));
            delete[] buf;
            goto bail;
        }

        delete[] buf;
    } else {
        nerr = CopyData(outfp, conv, convHA, pErrMsg);
        if (nerr != kNuErrNone) {
            if (pErrMsg->IsEmpty()) {
                pErrMsg->Format(L"Failed while copying data: %hs\n",
                    NuStrError(nerr));
            }
            goto bail;
        }
    }

    result = IDOK;

bail:
    SET_PROGRESS_END();
    return result;
}

NuError BnyEntry::CopyData(FILE* outfp, ConvertEOL conv, ConvertHighASCII convHA,
        CString* pMsg) const
{
    NuError nerr = kNuErrNone;
    const int kChunkSize = 8192;
    char buf[kChunkSize];
    bool lastCR = false;
    long srcLen, dataRem;

    srcLen = (long) GetUncompressedLen();
    ASSERT(srcLen > 0); // empty files should've been caught earlier

    /*
     * Loop until all data copied.
     */
    dataRem = srcLen;
    while (dataRem) {
        int chunkLen;

        if (dataRem > kChunkSize)
            chunkLen = kChunkSize;
        else
            chunkLen = dataRem;

        /* read a chunk from the source file */
        nerr = fpArchive->BNYRead(buf, chunkLen);
        if (nerr != kNuErrNone) {
            pMsg->Format(L"File read failed: %hs.", NuStrError(nerr));
            goto bail;
        }

        /* write chunk to destination file */
        int err = GenericEntry::WriteConvert(outfp, buf, chunkLen, &conv,
                    &convHA, &lastCR);
        if (err != 0) {
            pMsg->Format(L"File write failed: %hs.", strerror(err));
            nerr = kNuErrGeneric;
            goto bail;
        }

        dataRem -= chunkLen;
        SET_PROGRESS_UPDATE(ComputePercent(srcLen - dataRem, srcLen));
    }

bail:
    return nerr;
}

NuError BnyEntry::TestEntry(CWnd* pMsgWnd)
{
    NuError nerr = kNuErrNone;
    CString errMsg;
    long len;
    int result = -1;

    len = (long) GetUncompressedLen();
    if (len == 0)
        goto bail;

    errno = 0;
    if (fseek(fpArchive->fFp, fOffset, SEEK_SET) < 0) {
        nerr = kNuErrGeneric;
        errMsg.Format(L"Unable to seek to offset %ld: %hs\n",
            fOffset, strerror(errno));
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        goto bail;
    }

    if (GetSqueezed()) {
        nerr = UnSqueeze(fpArchive->fFp, (unsigned long) GetUncompressedLen(),
                    NULL, true, kBNYBlockSize);
        if (nerr != kNuErrNone) {
            errMsg.Format(L"Unsqueeze failed: %hs.", NuStrError(nerr));
            ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }
    } else {
        errno = 0;
        if (fseek(fpArchive->fFp, fOffset + len, SEEK_SET) < 0) {
            nerr = kNuErrGeneric;
            errMsg.Format(L"Unable to seek to offset %ld (file truncated?): %hs\n",
                fOffset, strerror(errno));
            ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }
    }

    if (SET_PROGRESS_UPDATE(100) == IDCANCEL)
        nerr = kNuErrAborted;

bail:
    return nerr;
}


/*
 * ===========================================================================
 *      BnyArchive
 * ===========================================================================
 */

/*static*/ CString BnyArchive::AppInit(void)
{
    // We don't really have anything to initialize.  Having this method
    // is kind of silly, but we include it for consistency.
    return L"";
}

GenericArchive::OpenResult BnyArchive::Open(const WCHAR* filename,
    bool readOnly, CString* pErrMsg)
{
    CString errMsg;

    fIsReadOnly = true;     // ignore "readOnly"

    errno = 0;
    fFp = _wfopen(filename, L"rb");
    if (fFp == NULL) {
        errMsg.Format(L"Unable to open %ls: %hs.", filename, strerror(errno));
        goto bail;
    }

    {
        CWaitCursor waitc;

        if (LoadContents() != 0) {
            errMsg.Format(L"Failed while loading contents of Binary II file.");
            goto bail;
        }
    }

    SetPathName(filename);

bail:
    *pErrMsg = errMsg;
    if (!errMsg.IsEmpty())
        return kResultFailure;
    else
        return kResultSuccess;
}

CString BnyArchive::New(const WCHAR* /*filename*/, const void* /*options*/)
{
    CString retmsg(L"Sorry, Binary II files can't be created.");
    return retmsg;
}


long BnyArchive::GetCapability(Capability cap)
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
        return false;
        break;
    case kCapCanAddDisk:
        return false;
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

int BnyArchive::LoadContents(void)
{
    NuError nerr;

    ASSERT(fFp != NULL);
    rewind(fFp);

    nerr = BNYIterate();
    LOGI("BNYIterate returned %d", nerr);
    return (nerr != kNuErrNone);
}

CString BnyArchive::Reload(void)
{
    fReloadFlag = true;     // tell everybody that cached data is invalid

    DeleteEntries();
    if (LoadContents() != 0) {
        return L"Reload failed.";
    }
    
    return "";
}

NuError BnyArchive::LoadContentsCallback(BnyFileEntry* pEntry)
{
    const int kBNYFssep = '/';
    NuError err = kNuErrNone;
    BnyEntry* pNewEntry;
    char* fileName;


    /* make sure filename doesn't start with '/' (not allowed by BNY spec) */
    fileName = pEntry->fileName;
    while (*fileName == kBNYFssep)
        fileName++;
    if (*fileName == '\0')
        return kNuErrBadData;

    /* remove '.QQ' from end of squeezed files */
    bool isSqueezed = false;
    if (pEntry->realEOF && IsSqueezed(pEntry->blockBuf[0], pEntry->blockBuf[1]))
        isSqueezed = true;

    if (isSqueezed && strlen(fileName) > 3) {
        char* ext;
        ext = fileName + strlen(fileName) -3;
        if (stricmp(ext, ".qq") == 0)
            *ext = '\0';
    }

    /*
     * Create the new entry.
     */
    pNewEntry = new BnyEntry(this);
    pNewEntry->SetPathNameMOR(fileName);
    pNewEntry->SetFssep(kBNYFssep);
    pNewEntry->SetFileType(pEntry->fileType);
    pNewEntry->SetAuxType(pEntry->auxType);
    pNewEntry->SetAccess(pEntry->access);
    pNewEntry->SetCreateWhen(NufxArchive::DateTimeToSeconds(&pEntry->createWhen));
    pNewEntry->SetModWhen(NufxArchive::DateTimeToSeconds(&pEntry->modWhen));

    /* always ProDOS */
    pNewEntry->SetSourceFS(DiskImg::kFormatProDOS);

    pNewEntry->SetHasDataFork(true);
    pNewEntry->SetHasRsrcFork(false);
    if (IsDir(pEntry)) {
        pNewEntry->SetRecordKind(GenericEntry::kRecordKindDirectory);
    } else {
        pNewEntry->SetRecordKind(GenericEntry::kRecordKindFile);
    }

    /* there's no way to get the uncompressed EOF from a squeezed file */
    pNewEntry->SetCompressedLen(pEntry->realEOF);
    pNewEntry->SetDataForkLen(pEntry->realEOF);

    if (isSqueezed)
        pNewEntry->SetFormatStr(L"Squeeze");
    else
        pNewEntry->SetFormatStr(L"Uncompr");

    pNewEntry->SetSqueezed(isSqueezed);
    if (pEntry->realEOF != 0)
        pNewEntry->SetOffset(ftell(fFp) - kBNYBlockSize);
    else
        pNewEntry->SetOffset(ftell(fFp));

    AddEntry(pNewEntry);

    return err;
}


/*
 * ===========================================================================
 *      Binary II functions
 * ===========================================================================
 */

/*
 * Most of what follows was adapted directly from NuLib2 v2.0.  There's no
 * such thing as BnyLib, so all of the code for manipulating the file is
 * included here.
 */

bool BnyArchive::IsSqueezed(uint8_t one, uint8_t two)
{
    return (one == 0x76 && two == 0xff);
}

bool BnyArchive::IsDir(BnyFileEntry* pEntry)
{
    /*
     * NuLib and "unblu.c" compared against file type 15 (DIR), so I'm
     * going to do that too, but it would probably be better to compare
     * against storageType 0x0d.
     */
    return (pEntry->fileType == 15);
}

NuError BnyArchive::BNYRead(void* buf, size_t nbyte)
{
    size_t result;

    ASSERT(buf != NULL);
    ASSERT(nbyte > 0);
    ASSERT(fFp != NULL);

    errno = 0;
    result = fread(buf, 1, nbyte, fFp);
    if (result != nbyte)
        return errno ? (NuError)errno : kNuErrFileRead;
    return kNuErrNone;
}

NuError BnyArchive::BNYSeek(long offset)
{
    ASSERT(fFp != NULL);
    ASSERT(offset > 0);

    LOGV("--- seeking forward %ld bytes\n", offset);

    if (fseek(fFp, offset, SEEK_CUR) < 0)
        return kNuErrFileSeek;
    
    return kNuErrNone;
}


void BnyArchive::BNYConvertDateTime(unsigned short prodosDate,
    unsigned short prodosTime, NuDateTime* pWhen)
{
    pWhen->second = 0;
    pWhen->minute = prodosTime & 0x3f;
    pWhen->hour = (prodosTime >> 8) & 0x1f;
    pWhen->day = (prodosDate & 0x1f) -1;
    pWhen->month = ((prodosDate >> 5) & 0x0f) -1;
    pWhen->year = (prodosDate >> 9) & 0x7f;
    if (pWhen->year < 40)
        pWhen->year += 100;     /* P8 uses 0-39 for 2000-2039 */
    pWhen->extra = 0;
    pWhen->weekDay = 0;
}

NuError BnyArchive::BNYDecodeHeader(BnyFileEntry* pEntry)
{
    /*
     * See the File Type Note for $e0/8000 to decipher the buffer offsets
     * and meanings.
     */
    NuError err = kNuErrNone;
    uint8_t* raw;
    int len;

    ASSERT(pEntry != NULL);

    raw = pEntry->blockBuf;

    if (raw[0] != 0x0a || raw[1] != 0x47 || raw[2] != 0x4c || raw[18] != 0x02) {
        err = kNuErrBadData;
        LOGI("this doesn't look like a Binary II header");
        goto bail;
    }

    pEntry->access = raw[3] | raw[111] << 8;
    pEntry->fileType = raw[4] | raw[112] << 8;
    pEntry->auxType = raw[5] | raw[6] << 8 | raw[109] << 16 | raw[110] << 24;
    pEntry->storageType = raw[7];
    pEntry->fileSize = raw[8] | raw[9] << 8;
    pEntry->prodosModDate = raw[10] | raw[11] << 8;
    pEntry->prodosModTime = raw[12] | raw[13] << 8;
    BNYConvertDateTime(pEntry->prodosModDate, pEntry->prodosModTime,
        &pEntry->modWhen);
    pEntry->prodosCreateDate = raw[14] | raw[15] << 8;
    pEntry->prodosCreateTime = raw[16] | raw[17] << 8;
    BNYConvertDateTime(pEntry->prodosCreateDate, pEntry->prodosCreateTime,
        &pEntry->createWhen);
    pEntry->eof = raw[20] | raw[21] << 8 | raw[22] << 16 | raw[116] << 24;
    len = raw[23];
    if (len > kBNYMaxFileName) {
        err = kNuErrBadData;
        LOGI("invalid filename length %d", len);
        goto bail;
    }
    memcpy(pEntry->fileName, &raw[24], len);
    pEntry->fileName[len] = '\0';

    pEntry->nativeName[0] = '\0';
    if (len <= 15 && raw[39] != 0) {
        len = raw[39];
        if (len > kBNYMaxNativeName) {
            err = kNuErrBadData;
            LOGI("invalid filename length %d", len);
            goto bail;
        }
        memcpy(pEntry->nativeName, &raw[40], len);
        pEntry->nativeName[len] = '\0';
    }

    pEntry->diskSpace = raw[117] | raw[118] << 8 | raw[119] << 16 |
                        raw[120] << 24;

    pEntry->osType = raw[121];
    pEntry->nativeFileType = raw[122] | raw[123] << 8;
    pEntry->phantomFlag = raw[124];
    pEntry->dataFlags = raw[125];
    pEntry->version = raw[126];
    pEntry->filesToFollow = raw[127];

    /* directories are given an EOF but don't actually have any content */
    if (IsDir(pEntry))
        pEntry->realEOF = 0;
    else
        pEntry->realEOF = pEntry->eof;

bail:
    return err;
}

#if 0
/*
 * Normalize the pathname by running it through the usual NuLib2
 * function.  The trick here is that the function usually takes a
 * NuPathnameProposal, which we don't happen to have handy.  Rather
 * than generalize the NuLib2 code, we just create a fake proposal,
 * which is a bit dicey but shouldn't break too easily.
 *
 * This takes care of -e, -ee, and -j.
 *
 * We return the new path, which is stored in NulibState's temporary
 * filename buffer.
 */
const char*
BNYNormalizePath(BnyFileEntry* pEntry)
{
    NuPathnameProposal pathProposal;
    NuRecord fakeRecord;
    NuThread fakeThread;

    /* make uninitialized data obvious */
    memset(&fakeRecord, 0xa1, sizeof(fakeRecord));
    memset(&fakeThread, 0xa5, sizeof(fakeThread));

    pathProposal.pathname = pEntry->fileName;
    pathProposal.filenameSeparator = '/';   /* BNY always uses ProDOS conv */
    pathProposal.pRecord = &fakeRecord;
    pathProposal.pThread = &fakeThread;

    pathProposal.newPathname = NULL;
    pathProposal.newFilenameSeparator = '\0';
    pathProposal.newDataSink = NULL;

    /* need the filetype and auxtype for -e/-ee */
    fakeRecord.recFileType = pEntry->fileType;
    fakeRecord.recExtraType = pEntry->auxType;

    /* need the components of a ThreadID */
    fakeThread.thThreadClass = kNuThreadClassData;
    fakeThread.thThreadKind = 0x0000;       /* data fork */

    return NormalizePath(pBny->pState, &pathProposal);
}
#endif

#if 0
/*
 * Copy all data from the Binary II file to "outfp", reading in 128-byte
 * blocks.
 *
 * Uses pEntry->blockBuf, which already has the first 128 bytes in it.
 */
NuError
BnyArchive::BNYCopyBlocks(BnyFileEntry* pEntry, FILE* outfp)
{
    NuError err = kNuErrNone;
    long bytesLeft;

    ASSERT(pEntry->realEOF > 0);

    bytesLeft = pEntry->realEOF;
    while (bytesLeft > 0) {
        long toWrite;

        toWrite = bytesLeft;
        if (toWrite > kBNYBlockSize)
            toWrite = kBNYBlockSize;

        if (outfp != NULL) {
            if (fwrite(pEntry->blockBuf, toWrite, 1, outfp) != 1) {
                err = errno ? (NuError) errno : kNuErrFileWrite;
                LOGI("BNY write failed");
                goto bail;
            }
        }

        bytesLeft -= toWrite;

        if (bytesLeft) {
            err = BNYRead(pEntry->blockBuf, kBNYBlockSize);
            if (err != kNuErrNone) {
                LOGI("BNY read failed");
                goto bail;
            }
        }
    }

bail:
    return err;
}
#endif


NuError BnyArchive::BNYIterate(void)
{
    NuError err = kNuErrNone;
    BnyFileEntry entry;
    //bool consumed;
    int first = true;
    int toFollow;

    toFollow = 1;       /* assume 1 file in archive */
    while (toFollow) {
        err = BNYRead(entry.blockBuf, sizeof(entry.blockBuf));
        if (err != kNuErrNone) {
            LOGI("failed while reading header");
            goto bail;
        }

        err = BNYDecodeHeader(&entry);
        if (err != kNuErrNone) {
            if (first) {
                LOGI("not a Binary II archive?");
            }
            goto bail;
        }

        /*
         * If the file has one or more blocks, read the first block now.
         * This will allow the various functions to evaluate the file
         * contents for SQueeze compression.
         */
        if (entry.realEOF != 0) {
            err = BNYRead(entry.blockBuf, sizeof(entry.blockBuf));
            if (err != kNuErrNone) {
                LOGI("failed while reading");
                goto bail;
            }
        }

        /*
         * Invoke the load function.
         */
        //consumed = false;

        err = LoadContentsCallback(&entry);
        if (err != kNuErrNone)
            goto bail;

        /*
         * If they didn't "consume" the entire BNY entry, we need to
         * do it for them.  We've already read the first block (if it
         * existed), so we don't need to eat that one again.
         */
        if (true /*!consumed*/) {
            int nblocks = (entry.realEOF + kBNYBlockSize-1) / kBNYBlockSize;

            if (nblocks > 1) {
                err = BNYSeek((nblocks-1) * kBNYBlockSize);
                if (err != kNuErrNone) {
                    LOGI("failed while seeking forward");
                    goto bail;
                }
            }
        }

        if (!first) {
            if (entry.filesToFollow != toFollow -1) {
                LOGI("WARNING: filesToFollow %d, expected %d",
                    entry.filesToFollow, toFollow -1);
            }
        }
        toFollow = entry.filesToFollow;

        first = false;
    }

bail:
    if (err != kNuErrNone) {
        LOGI("--- Iterator returning failure %d", err);
    }
    return err;
}


/*
 * ===========================================================================
 *      BnyArchive -- test files
 * ===========================================================================
 */

bool BnyArchive::TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
    NuError nerr;
    BnyEntry* pEntry;
    CString errMsg;
    bool retVal = false;

    ASSERT(fFp != NULL);

    LOGI("Testing %d entries", pSelSet->GetNumEntries());

    SelectionEntry* pSelEntry = pSelSet->IterNext();
    while (pSelEntry != NULL) {
        pEntry = (BnyEntry*) pSelEntry->GetEntry();

        LOGI("  Testing '%ls' (offset=%ld)", (LPCWSTR) pEntry->GetDisplayName(),
            pEntry->GetOffset());

        SET_PROGRESS_UPDATE2(0, pEntry->GetDisplayName(), NULL);

        nerr = pEntry->TestEntry(pMsgWnd);
        if (nerr != kNuErrNone) {
            if (nerr == kNuErrAborted) {
                CString title;
                CheckedLoadString(&title, IDS_MB_APP_NAME);
                errMsg = "Cancelled.";
                pMsgWnd->MessageBox(errMsg, title, MB_OK);
            } else {
                errMsg.Format(L"Failed while testing '%ls': %hs.",
                    (LPCWSTR) pEntry->GetPathNameUNI(), NuStrError(nerr));
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
    SET_PROGRESS_END();
    return retVal;
}
