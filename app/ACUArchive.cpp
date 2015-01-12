/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * AppleLink Compression Utility file support.
 *
 * This was adapted from the Binary II support, which has a mixed history,
 * so this is a little scrambled in spots.
 */
#include "stdafx.h"
#include "ACUArchive.h"
#include "NufxArchive.h"        // uses NuError
#include "Preferences.h"
#include "Main.h"
#include "Squeeze.h"
#include <errno.h>

/*
+00 2b      Number of items in archive
+02 2b      0100
+04 5b      "fZink"
+09 11b     0136 0000 0000 0000 0000 dd

+14

+00 1b      ?? 00
+01 1b      Compression type, 00=none, 03=sq
+02 2b      ?? 0000   0000         0000    0000
+04 2b      ?? 0a74   961f   7d85  af2c    2775    <- 0000 for dir (checksum?)
+06 2b      ?? 0000   0000         0000    0000
+08 2b      ?? 0000   0000         0000    0000
+0a 2b      Storage size (in 512-byte blocks)
+0c 6b      ?? 000000 000000       000000  000000
+12 4b      Length of file in this archive (compressed or uncompressed)
+16 2b      ProDOS file permissions
+18 2b      ProDOS file type
+1a 4b      ProDOS aux type
+1e         ?? 0000
+20 1b      ProDOS storage type (usually 02, 0d for dirs)
+21         ?? 00
+22         ?? 0000 0000
+26 4b      Uncompressed file len
+2a 2b      ProDOS date (create?)
+2c 2b      ProDOS time
+2e 2b      ProDOS date (mod?)
+30 2b      ProDOS time
+32 2b      Filename len
+34 2b      ?? ac4a  2d02 for dir  <- header checksum?
+36 FL      Filename
+xx data start  (dir has no data)
*/

/*
 * ===========================================================================
 *      AcuEntry
 * ===========================================================================
 */

int AcuEntry::ExtractThreadToBuffer(int which, char** ppText, long* pLength,
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
        nerr = UnSqueeze(fpArchive->fFp, (unsigned long) GetCompressedLen(),
                    &expBuf, false, 0);
        if (nerr != kNuErrNone) {
            pErrMsg->Format(L"File read failed: %hs", NuStrError(nerr));
            goto bail;
        }

        char* unsqBuf = NULL;
        long unsqLen = 0;
        expBuf.SeizeBuffer(&unsqBuf, &unsqLen);
        LOGI("Unsqueezed %ld bytes to %d",
            (unsigned long) GetCompressedLen(), unsqLen);
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

int AcuEntry::ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
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
     * Generally speaking, anything in an ACU file is going to be small.
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

        nerr = UnSqueeze(fpArchive->fFp, (unsigned long) GetCompressedLen(),
                    &expBuf, false, 0);
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

NuError AcuEntry::CopyData(FILE* outfp, ConvertEOL conv, ConvertHighASCII convHA,
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
        nerr = fpArchive->AcuRead(buf, chunkLen);
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


NuError AcuEntry::TestEntry(CWnd* pMsgWnd)
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
        nerr = UnSqueeze(fpArchive->fFp, (unsigned long) GetCompressedLen(),
                    NULL, false, 0);
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
 *      AcuArchive
 * ===========================================================================
 */

/*static*/ CString AcuArchive::AppInit(void)
{
    return L"";
}

GenericArchive::OpenResult AcuArchive::Open(const WCHAR* filename,
    bool readOnly, CString* pErrMsg)
{
    CString errMsg;

    //fIsReadOnly = true;     // ignore "readOnly"

    errno = 0;
    fFp = _wfopen(filename, L"rb");
    if (fFp == NULL) {
        errMsg.Format(L"Unable to open %ls: %hs.", filename, strerror(errno));
        goto bail;
    }

    {
        CWaitCursor waitc;
        int result;

        result = LoadContents();
        if (result < 0) {
            errMsg.Format(L"The file is not an ACU archive.");
            goto bail;
        } else if (result > 0) {
            errMsg.Format(L"Failed while reading data from ACU archive.");
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

CString AcuArchive::New(const WCHAR* /*filename*/, const void* /*options*/)
{
    return L"Sorry, AppleLink Compression Utility files can't be created.";
}


long AcuArchive::GetCapability(Capability cap)
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

int AcuArchive::LoadContents(void)
{
    NuError nerr;
    int numEntries;

    ASSERT(fFp != NULL);
    rewind(fFp);

    /*
     * Read the master header.  In an ACU file this holds the number of
     * files and a bunch of stuff that doesn't seem to change.
     */
    if (ReadMasterHeader(&numEntries) != 0)
        return -1;

    while (numEntries) {
        AcuFileEntry fileEntry;

        nerr = ReadFileHeader(&fileEntry);
        if (nerr != kNuErrNone)
            return 1;

        if (CreateEntry(&fileEntry) != 0)
            return 1;

        /* if file isn't empty, seek past it */
        if (fileEntry.dataStorageLen) {
            nerr = AcuSeek(fileEntry.dataStorageLen);
            if (nerr != kNuErrNone)
                return 1;
        }

        numEntries--;
    }

    return 0;
}

CString AcuArchive::Reload(void)
{
    fReloadFlag = true;     // tell everybody that cached data is invalid

    DeleteEntries();
    if (LoadContents() != 0) {
        return L"Reload failed.";
    }
    
    return "";
}

int AcuArchive::ReadMasterHeader(int* pNumEntries)
{
    AcuMasterHeader header;
    unsigned char buf[kAcuMasterHeaderLen];
    NuError nerr;

    nerr = AcuRead(buf, kAcuMasterHeaderLen);
    if (nerr != kNuErrNone)
        return -1;

    header.fileCount = buf[0x00] | buf[0x01] << 8;
    header.unknown1 = buf[0x02] | buf[0x03] << 8;
    memcpy(header.fZink, &buf[0x04], 5);
    header.fZink[5] = '\0';
    memcpy(header.unknown2, &buf[0x09], 11);

    if (header.fileCount == 0 ||
        header.unknown1 != 1 ||
        strcmp((char*) header.fZink, "fZink") != 0)
    {
        LOGW("Not an ACU archive");
        return -1;
    }

    LOGD("Looks like an ACU archive with %d entries", header.fileCount);

    *pNumEntries = header.fileCount;
    return 0;
}

NuError AcuArchive::ReadFileHeader(AcuFileEntry* pEntry)
{
    NuError err = kNuErrNone;
    unsigned char buf[kAcuEntryHeaderLen];

    ASSERT(pEntry != NULL);

    err = AcuRead(buf, kAcuEntryHeaderLen);
    if (err != kNuErrNone)
        goto bail;

    // unknown at 00
    pEntry->compressionType = buf[0x01];
    // unknown at 02-03
    pEntry->dataChecksum = buf[0x04] | buf[0x05] << 8;      // not sure
    // unknown at 06-09
    pEntry->blockCount = buf[0x0a] | buf[0x0b] << 8;
    // unknown at 0c-11
    pEntry->dataStorageLen = buf[0x12] | buf [0x13] << 8 | buf[0x14] << 16 |
        buf[0x15] << 24;
    pEntry->access = buf[0x16] | buf[0x17] << 8;
    pEntry->fileType = buf[0x18] | buf[0x19] << 8;
    pEntry->auxType = buf[0x1a] | buf[0x1b] << 8;
    // unknown at 1e-1f
    pEntry->storageType = buf[0x20];
    // unknown at 21-25
    pEntry->dataEof = buf[0x26] | buf[0x27] << 8 | buf[0x28] << 16 |
        buf[0x29] << 24;
    pEntry->prodosModDate = buf[0x2a] | buf[0x2b] << 8;
    pEntry->prodosModTime = buf[0x2c] | buf[0x2d] << 8;
    AcuConvertDateTime(pEntry->prodosModDate, pEntry->prodosModTime,
        &pEntry->modWhen);
    pEntry->prodosCreateDate = buf[0x2e] | buf[0x2f] << 8;
    pEntry->prodosCreateTime = buf[0x30] | buf[0x31] << 8;
    AcuConvertDateTime(pEntry->prodosCreateDate, pEntry->prodosCreateTime,
        &pEntry->createWhen);
    pEntry->fileNameLen = buf[0x32] | buf[0x33] << 8;
    pEntry->headerChecksum = buf[0x34] | buf[0x35] << 8;    // not sure

    /* read the filename */
    if (pEntry->fileNameLen > kAcuMaxFileName) {
        LOGI("GLITCH: filename is too long (%d bytes)",
            pEntry->fileNameLen);
        err = kNuErrGeneric;
        goto bail;
    }
    if (!pEntry->fileNameLen) {
        LOGI("GLITCH: filename missing");
        err = kNuErrGeneric;
        goto bail;
    }

    /* don't know if this is possible or not */
    if (pEntry->storageType == 5) {
        LOGI("HEY: EXTENDED FILE");
    }

    err = AcuRead(pEntry->fileName, pEntry->fileNameLen);
    if (err != kNuErrNone)
        goto bail;
    pEntry->fileName[pEntry->fileNameLen] = '\0';

    //DumpFileHeader(pEntry);

bail:
    return err;
}

void AcuArchive::DumpFileHeader(const AcuFileEntry* pEntry)
{
    time_t createWhen, modWhen;
    CString createStr, modStr;

    createWhen = NufxArchive::DateTimeToSeconds(&pEntry->createWhen);
    modWhen = NufxArchive::DateTimeToSeconds(&pEntry->modWhen);
    FormatDate(createWhen, &createStr);
    FormatDate(modWhen, &modStr);

    LOGI("  Header for file '%hs':", pEntry->fileName);
    LOGI("    dataStorageLen=%d eof=%d blockCount=%d checksum=0x%04x",
        pEntry->dataStorageLen, pEntry->dataEof, pEntry->blockCount,
        pEntry->dataChecksum);
    LOGI("    fileType=0x%02x auxType=0x%04x storageType=0x%02x access=0x%04x",
        pEntry->fileType, pEntry->auxType, pEntry->storageType, pEntry->access);
    LOGI("    created %ls, modified %ls",
        (LPCWSTR) createStr, (LPCWSTR) modStr);
    LOGI("    fileNameLen=%d headerChecksum=0x%04x",
        pEntry->fileNameLen, pEntry->headerChecksum);
}

int AcuArchive::CreateEntry(const AcuFileEntry* pEntry)
{
    const int kAcuFssep = '/';
    NuError err = kNuErrNone;
    AcuEntry* pNewEntry;

    /*
     * Create the new entry.
     */
    pNewEntry = new AcuEntry(this);
    pNewEntry->SetPathNameMOR(pEntry->fileName);
    pNewEntry->SetFssep(kAcuFssep);
    pNewEntry->SetFileType(pEntry->fileType);
    pNewEntry->SetAuxType(pEntry->auxType);
    pNewEntry->SetAccess(pEntry->access);
    pNewEntry->SetCreateWhen(NufxArchive::DateTimeToSeconds(&pEntry->createWhen));
    pNewEntry->SetModWhen(NufxArchive::DateTimeToSeconds(&pEntry->modWhen));

    /* always ProDOS? */
    pNewEntry->SetSourceFS(DiskImg::kFormatProDOS);

    pNewEntry->SetHasDataFork(true);
    pNewEntry->SetHasRsrcFork(false);   // ?
    if (IsDir(pEntry)) {
        pNewEntry->SetRecordKind(GenericEntry::kRecordKindDirectory);
    } else {
        pNewEntry->SetRecordKind(GenericEntry::kRecordKindFile);
    }

    pNewEntry->SetCompressedLen(pEntry->dataStorageLen);
    pNewEntry->SetDataForkLen(pEntry->dataEof);

    if (pEntry->compressionType == kAcuCompNone) {
        pNewEntry->SetFormatStr(L"Uncompr");
    } else if (pEntry->compressionType == kAcuCompSqueeze) {
        pNewEntry->SetFormatStr(L"Squeeze");
        pNewEntry->SetSqueezed(true);
    } else {
        pNewEntry->SetFormatStr(L"(unknown)");
        pNewEntry->SetSqueezed(false);
    }

    pNewEntry->SetOffset(ftell(fFp));

    AddEntry(pNewEntry);

    return err;
}


/*
 * ===========================================================================
 *      ACU functions
 * ===========================================================================
 */

bool AcuArchive::IsDir(const AcuFileEntry* pEntry)
{
    return (pEntry->storageType == 0x0d);
}

NuError AcuArchive::AcuRead(void* buf, size_t nbyte)
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

NuError AcuArchive::AcuSeek(long offset)
{
    ASSERT(fFp != NULL);
    ASSERT(offset > 0);

    /*DBUG(("--- seeking forward %ld bytes\n", offset));*/

    if (fseek(fFp, offset, SEEK_CUR) < 0)
        return kNuErrFileSeek;
    
    return kNuErrNone;
}


void AcuArchive::AcuConvertDateTime(uint16_t prodosDate,
    uint16_t prodosTime, NuDateTime* pWhen)
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


/*
 * ===========================================================================
 *      AcuArchive -- test files
 * ===========================================================================
 */

bool AcuArchive::TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
    // TODO: this is essentially copy & paste from NufxArchive::TestSelection().
    // We can move the implementation to GenericArchive and just have an
    // archive-specific TestEntry() function.
    NuError nerr;
    AcuEntry* pEntry;
    CString errMsg;
    bool retVal = false;

    ASSERT(fFp != NULL);

    LOGI("Testing %d entries", pSelSet->GetNumEntries());

    SelectionEntry* pSelEntry = pSelSet->IterNext();
    while (pSelEntry != NULL) {
        pEntry = (AcuEntry*) pSelEntry->GetEntry();

        LOGD("  Testing '%ls' (offset=%ld)", (LPCWSTR) pEntry->GetDisplayName(),
            pEntry->GetOffset());

        SET_PROGRESS_UPDATE2(0, pEntry->GetDisplayName(), NULL);

        nerr = pEntry->TestEntry(pMsgWnd);
        if (nerr != kNuErrNone) {
            if (nerr == kNuErrAborted) {
                CString title;
                CheckedLoadString(&title, IDS_MB_APP_NAME);
                errMsg = L"Cancelled.";
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
