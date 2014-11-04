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

/*
 * Extract data from an entry.
 *
 * If "*ppText" is non-nil, the data will be read into the pointed-to buffer
 * so long as it's shorter than *pLength bytes.  The value in "*pLength"
 * will be set to the actual length used.
 *
 * If "*ppText" is nil, the uncompressed data will be placed into a buffer
 * allocated with "new[]".
 *
 * Returns IDOK on success, IDCANCEL if the operation was cancelled by the
 * user, and -1 value on failure.  On failure, "*pErrMsg" holds an error
 * message.
 *
 * "which" is an anonymous GenericArchive enum (e.g. "kDataThread").
 */
int
AcuEntry::ExtractThreadToBuffer(int which, char** ppText, long* pLength,
    CString* pErrMsg) const
{
    NuError nerr;
    ExpandBuffer expBuf;
    char* dataBuf = nil;
    long len;
    bool needAlloc = true;
    int result = -1;

    ASSERT(fpArchive != nil);
    ASSERT(fpArchive->fFp != nil);

    if (*ppText != nil)
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
        pErrMsg->Format("Unable to seek to offset %ld: %s",
            fOffset, strerror(errno));
        goto bail;
    }

    if (GetSqueezed()) {
        nerr = UnSqueeze(fpArchive->fFp, (unsigned long) GetCompressedLen(),
                    &expBuf, false, 0);
        if (nerr != kNuErrNone) {
            pErrMsg->Format("File read failed: %s", NuStrError(nerr));
            goto bail;
        }

        char* unsqBuf = nil;
        long unsqLen = 0;
        expBuf.SeizeBuffer(&unsqBuf, &unsqLen);
        WMSG2("Unsqueezed %ld bytes to %d\n",
            (unsigned long) GetCompressedLen(), unsqLen);
        if (unsqLen == 0) {
            // some bonehead squeezed a zero-length file
            delete[] unsqBuf;
            ASSERT(*ppText == nil);
            WMSG0("Handling zero-length squeezed file!\n");
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
                    pErrMsg->Format("buf size %ld too short (%ld)",
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
            if (dataBuf == nil) {
                pErrMsg->Format("allocation of %ld bytes failed", len);
                goto bail;
            }
        } else {
            if (*pLength < (long) len) {
                pErrMsg->Format("buf size %ld too short (%ld)",
                    *pLength, len);
                goto bail;
            }
            dataBuf = *ppText;
        }
        if (fread(dataBuf, len, 1, fpArchive->fFp) != 1) {
            pErrMsg->Format("File read failed: %s", strerror(errno));
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
            ASSERT(*ppText == nil);
        }
    }
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
AcuEntry::ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
    ConvertHighASCII convHA, CString* pErrMsg) const
{
    NuError nerr;
    long len;
    int result = -1;

    ASSERT(IDOK != -1 && IDCANCEL != -1);
    if (which != kDataThread) {
        *pErrMsg = "No such fork";
        goto bail;
    }

    len = (long) GetUncompressedLen();
    if (len == 0) {
        WMSG0("Empty fork\n");
        result = IDOK;
        goto bail;
    }

    errno = 0;
    if (fseek(fpArchive->fFp, fOffset, SEEK_SET) < 0) {
        pErrMsg->Format("Unable to seek to offset %ld: %s",
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

        nerr = UnSqueeze(fpArchive->fFp, (unsigned long) GetCompressedLen(),
                    &expBuf, false, 0);
        if (nerr != kNuErrNone) {
            pErrMsg->Format("File read failed: %s", NuStrError(nerr));
            goto bail;
        }

        expBuf.SeizeBuffer(&buf, &uncLen);
        WMSG2("Unsqueezed %ld bytes to %d\n", len, uncLen);

        // some bonehead squeezed a zero-length file
        if (uncLen == 0) {
            ASSERT(buf == nil);
            WMSG0("Handling zero-length squeezed file!\n");
            result = IDOK;
            goto bail;
        }

        int err = GenericEntry::WriteConvert(outfp, buf, uncLen, &conv,
                        &convHA, &lastCR);
        if (err != 0) {
            pErrMsg->Format("File write failed: %s", strerror(err));
            delete[] buf;
            goto bail;
        }

        delete[] buf;
    } else {
        nerr = CopyData(outfp, conv, convHA, pErrMsg);
        if (nerr != kNuErrNone) {
            if (pErrMsg->IsEmpty()) {
                pErrMsg->Format("Failed while copying data: %s\n",
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

/*
 * Copy data from the seeked archive to outfp, possibly converting EOL along
 * the way.
 */
NuError
AcuEntry::CopyData(FILE* outfp, ConvertEOL conv, ConvertHighASCII convHA,
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
            pMsg->Format("File read failed: %s.", NuStrError(nerr));
            goto bail;
        }

        /* write chunk to destination file */
        int err = GenericEntry::WriteConvert(outfp, buf, chunkLen, &conv,
                    &convHA, &lastCR);
        if (err != 0) {
            pMsg->Format("File write failed: %s.", strerror(err));
            nerr = kNuErrGeneric;
            goto bail;
        }

        dataRem -= chunkLen;
        SET_PROGRESS_UPDATE(ComputePercent(srcLen - dataRem, srcLen));
    }

bail:
    return nerr;
}


/*
 * Test this entry by extracting it.
 *
 * If the file isn't compressed, just make sure the file is big enough.  If
 * it's squeezed, invoke the un-squeeze function with a "nil" buffer pointer.
 */
NuError
AcuEntry::TestEntry(CWnd* pMsgWnd)
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
        errMsg.Format("Unable to seek to offset %ld: %s\n",
            fOffset, strerror(errno));
        ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
        goto bail;
    }

    if (GetSqueezed()) {
        nerr = UnSqueeze(fpArchive->fFp, (unsigned long) GetCompressedLen(),
                    nil, false, 0);
        if (nerr != kNuErrNone) {
            errMsg.Format("Unsqueeze failed: %s.", NuStrError(nerr));
            ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
            goto bail;
        }
    } else {
        errno = 0;
        if (fseek(fpArchive->fFp, fOffset + len, SEEK_SET) < 0) {
            nerr = kNuErrGeneric;
            errMsg.Format("Unable to seek to offset %ld (file truncated?): %s\n",
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

/*
 * Perform one-time initialization.  There really isn't any for us.
 *
 * Returns 0 on success, nonzero on error.
 */
/*static*/ CString
AcuArchive::AppInit(void)
{
    return "";
}

/*
 * Open an ACU archive.
 *
 * Returns an error string on failure, or "" on success.
 */
GenericArchive::OpenResult
AcuArchive::Open(const char* filename, bool readOnly, CString* pErrMsg)
{
    CString errMsg;

    fIsReadOnly = true;     // ignore "readOnly"

    errno = 0;
    fFp = fopen(filename, "rb");
    if (fFp == nil) {
        errMsg.Format("Unable to open %s: %s.", filename, strerror(errno));
        goto bail;
    }

    {
        CWaitCursor waitc;
        int result;

        result = LoadContents();
        if (result < 0) {
            errMsg.Format("The file is not an ACU archive.");
            goto bail;
        } else if (result > 0) {
            errMsg.Format("Failed while reading data from ACU archive.");
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

/*
 * Finish instantiating an AcuArchive object by creating a new archive.
 *
 * Returns an error string on failure, or "" on success.
 */
CString
AcuArchive::New(const char* /*filename*/, const void* /*options*/)
{
    CString retmsg("Sorry, AppleLink Compression Utility files can't be created.");
    return retmsg;
}


/*
 * Our capabilities.
 */
long
AcuArchive::GetCapability(Capability cap)
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


/*
 * Load the contents of the archive.
 *
 * Returns 0 on success, < 0 if this is not an ACU archive > 0 if this appears
 * to be an ACU archive but it's damaged.
 */
int
AcuArchive::LoadContents(void)
{
    NuError nerr;
    int numEntries;

    ASSERT(fFp != nil);
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

/*
 * Reload the contents of the archive.
 */
CString
AcuArchive::Reload(void)
{
    fReloadFlag = true;     // tell everybody that cached data is invalid

    DeleteEntries();
    if (LoadContents() != 0) {
        return "Reload failed.";
    }
    
    return "";
}

/*
 * Read the archive header.  The archive file is left seeked to the point
 * at the end of the header.
 *
 * Returns 0 on success, -1 on failure.  Sets *pNumEntries to the number of
 * entries in the archive.
 */
int
AcuArchive::ReadMasterHeader(int* pNumEntries)
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
        WMSG0("Not an ACU archive\n");
        return -1;
    }

    WMSG1("Looks like an ACU archive with %d entries\n", header.fileCount);

    *pNumEntries = header.fileCount;
    return 0;
}

/*
 * Read and decode an AppleLink Compression Utility file entry header.
 * This leaves the file seeked to the point immediately past the filename.
 */
NuError
AcuArchive::ReadFileHeader(AcuFileEntry* pEntry)
{
    NuError err = kNuErrNone;
    unsigned char buf[kAcuEntryHeaderLen];

    ASSERT(pEntry != nil);

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
        WMSG1("GLITCH: filename is too long (%d bytes)\n",
            pEntry->fileNameLen);
        err = kNuErrGeneric;
        goto bail;
    }
    if (!pEntry->fileNameLen) {
        WMSG0("GLITCH: filename missing\n");
        err = kNuErrGeneric;
        goto bail;
    }

    /* don't know if this is possible or not */
    if (pEntry->storageType == 5) {
        WMSG0("HEY: EXTENDED FILE\n");
    }

    err = AcuRead(pEntry->fileName, pEntry->fileNameLen);
    if (err != kNuErrNone)
        goto bail;
    pEntry->fileName[pEntry->fileNameLen] = '\0';

    //DumpFileHeader(pEntry);

bail:
    return err;
}

/*
 * Dump the contents of an AcuFileEntry struct.
 */
void
AcuArchive::DumpFileHeader(const AcuFileEntry* pEntry)
{
    time_t createWhen, modWhen;
    CString createStr, modStr;

    createWhen = NufxArchive::DateTimeToSeconds(&pEntry->createWhen);
    modWhen = NufxArchive::DateTimeToSeconds(&pEntry->modWhen);
    FormatDate(createWhen, &createStr);
    FormatDate(modWhen, &modStr);

    WMSG1("  Header for file '%s':\n", pEntry->fileName);
    WMSG4("    dataStorageLen=%d eof=%d blockCount=%d checksum=0x%04x\n",
        pEntry->dataStorageLen, pEntry->dataEof, pEntry->blockCount,
        pEntry->dataChecksum);
    WMSG4("    fileType=0x%02x auxType=0x%04x storageType=0x%02x access=0x%04x\n",
        pEntry->fileType, pEntry->auxType, pEntry->storageType, pEntry->access);
    WMSG2("    created %s, modified %s\n", (const char*) createStr,
        (const char*) modStr);
    WMSG2("    fileNameLen=%d headerChecksum=0x%04x\n",
        pEntry->fileNameLen, pEntry->headerChecksum);
}


/*
 * Given an AcuFileEntry structure, add an appropriate entry to the list.
 */
int
AcuArchive::CreateEntry(const AcuFileEntry* pEntry)
{
    const int kAcuFssep = '/';
    NuError err = kNuErrNone;
    AcuEntry* pNewEntry;

    /*
     * Create the new entry.
     */
    pNewEntry = new AcuEntry(this);
    pNewEntry->SetPathName(pEntry->fileName);
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
        pNewEntry->SetFormatStr("Uncompr");
    } else if (pEntry->compressionType == kAcuCompSqueeze) {
        pNewEntry->SetFormatStr("Squeeze");
        pNewEntry->SetSqueezed(true);
    } else {
        pNewEntry->SetFormatStr("(unknown)");
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

/*
 * Test if this entry is a directory.
 */
bool
AcuArchive::IsDir(const AcuFileEntry* pEntry)
{
    return (pEntry->storageType == 0x0d);
}

/*
 * Wrapper for fread().  Note the arguments resemble read(2) rather
 * than fread(3S).
 */
NuError
AcuArchive::AcuRead(void* buf, size_t nbyte)
{
    size_t result;

    ASSERT(buf != nil);
    ASSERT(nbyte > 0);
    ASSERT(fFp != nil);

    errno = 0;
    result = fread(buf, 1, nbyte, fFp);
    if (result != nbyte)
        return errno ? (NuError)errno : kNuErrFileRead;
    return kNuErrNone;
}

/*
 * Seek within an archive.  Because we need to handle streaming archives,
 * and don't need to special-case anything, we only allow relative
 * forward seeks.
 */
NuError
AcuArchive::AcuSeek(long offset)
{
    ASSERT(fFp != nil);
    ASSERT(offset > 0);

    /*DBUG(("--- seeking forward %ld bytes\n", offset));*/

    if (fseek(fFp, offset, SEEK_CUR) < 0)
        return kNuErrFileSeek;
    
    return kNuErrNone;
}


/*
 * Convert from ProDOS compact date format to the expanded DateTime format.
 */
void
AcuArchive::AcuConvertDateTime(unsigned short prodosDate,
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


/*
 * ===========================================================================
 *      AcuArchive -- test files
 * ===========================================================================
 */

/*
 * Test the records represented in the selection set.
 */
bool
AcuArchive::TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
    NuError nerr;
    AcuEntry* pEntry;
    CString errMsg;
    bool retVal = false;

    ASSERT(fFp != nil);

    WMSG1("Testing %d entries\n", pSelSet->GetNumEntries());

    SelectionEntry* pSelEntry = pSelSet->IterNext();
    while (pSelEntry != nil) {
        pEntry = (AcuEntry*) pSelEntry->GetEntry();

        WMSG2("  Testing '%s' (offset=%ld)\n", pEntry->GetDisplayName(),
            pEntry->GetOffset());

        SET_PROGRESS_UPDATE2(0, pEntry->GetDisplayName(), nil);

        nerr = pEntry->TestEntry(pMsgWnd);
        if (nerr != kNuErrNone) {
            if (nerr == kNuErrAborted) {
                CString title;
                title.LoadString(IDS_MB_APP_NAME);
                errMsg = "Cancelled.";
                pMsgWnd->MessageBox(errMsg, title, MB_OK);
            } else {
                errMsg.Format("Failed while testing '%s': %s.",
                    pEntry->GetPathName(), NuStrError(nerr));
                ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
            }
            goto bail;
        }

        pSelEntry = pSelSet->IterNext();
    }

    /* show success message */
    errMsg.Format("Tested %d file%s, no errors found.",
        pSelSet->GetNumEntries(),
        pSelSet->GetNumEntries() == 1 ? "" : "s");
    pMsgWnd->MessageBox(errMsg);
    retVal = true;

bail:
    SET_PROGRESS_END();
    return retVal;
}
