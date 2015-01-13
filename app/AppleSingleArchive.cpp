/*
 * CiderPress
 * Copyright (C) 2015 by faddenSoft.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "stdafx.h"
#include "AppleSingleArchive.h"
#include "NufxArchive.h"            // using date/time function
#include "Preferences.h"
#include "Main.h"
#include <errno.h>


/*
 * ===========================================================================
 *      AppleSingleEntry
 * ===========================================================================
 */

int AppleSingleEntry::ExtractThreadToBuffer(int which, char** ppText,
    long* pLength, CString* pErrMsg) const
{
    ExpandBuffer expBuf;
    char* dataBuf = NULL;
    bool needAlloc = true;
    int result = -1;

    ASSERT(fpArchive != NULL);
    ASSERT(fpArchive->fFp != NULL);

    if (*ppText != NULL)
        needAlloc = false;

    long offset, length;
    if (which == kDataThread && fDataOffset >= 0) {
        offset = fDataOffset;
        length = (long) GetDataForkLen();
    } else if (which == kRsrcThread && fRsrcOffset >= 0) {
        offset = fRsrcOffset;
        length = (long) GetRsrcForkLen();
    } else {
        *pErrMsg = "No such fork";
        goto bail;
    }

    SET_PROGRESS_BEGIN();

    errno = 0;
    if (fseek(fpArchive->fFp, offset, SEEK_SET) < 0) {
        pErrMsg->Format(L"Unable to seek to offset %ld: %hs",
            fDataOffset, strerror(errno));
        goto bail;
    }

    if (needAlloc) {
        dataBuf = new char[length];
        if (dataBuf == NULL) {
            pErrMsg->Format(L"allocation of %ld bytes failed", length);
            goto bail;
        }
    } else {
        if (*pLength < length) {
            pErrMsg->Format(L"buf size %ld too short (%ld)",
                *pLength, length);
            goto bail;
        }
        dataBuf = *ppText;
    }
    if (length > 0) {
        if (fread(dataBuf, length, 1, fpArchive->fFp) != 1) {
            pErrMsg->Format(L"File read failed: %hs", strerror(errno));
            goto bail;
        }
    }

    if (needAlloc)
        *ppText = dataBuf;
    *pLength = length;

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

int AppleSingleEntry::ExtractThreadToFile(int which, FILE* outfp,
    ConvertEOL conv, ConvertHighASCII convHA, CString* pErrMsg) const
{
    int result = -1;

    ASSERT(IDOK != -1 && IDCANCEL != -1);
    long offset, length;
    if (which == kDataThread && fDataOffset >= 0) {
        offset = fDataOffset;
        length = (long) GetDataForkLen();
    } else if (which == kRsrcThread && fRsrcOffset >= 0) {
        offset = fRsrcOffset;
        length = (long) GetRsrcForkLen();
    } else {
        *pErrMsg = "No such fork";
        goto bail;
    }

    if (length == 0) {
        LOGD("Empty fork");
        result = IDOK;
        goto bail;
    }

    errno = 0;
    if (fseek(fpArchive->fFp, offset, SEEK_SET) < 0) {
        pErrMsg->Format(L"Unable to seek to offset %ld: %hs",
            fDataOffset, strerror(errno));
        goto bail;
    }

    SET_PROGRESS_BEGIN();

    if (CopyData(length, outfp, conv, convHA, pErrMsg) != 0) {
        if (pErrMsg->IsEmpty()) {
            *pErrMsg = L"Failed while copying data.";
        }
        goto bail;
    }

    result = IDOK;

bail:
    SET_PROGRESS_END();
    return result;
}

int AppleSingleEntry::CopyData(long srcLen, FILE* outfp, ConvertEOL conv,
    ConvertHighASCII convHA, CString* pMsg) const
{
    int err = 0;
    const int kChunkSize = 65536;
    char* buf = new char[kChunkSize];
    bool lastCR = false;
    long dataRem;

    ASSERT(srcLen > 0); // empty files should've been caught earlier

    /*
     * Loop until all data copied.
     */
    dataRem = srcLen;
    while (dataRem) {
        int chunkLen;

        if (dataRem > kChunkSize) {
            chunkLen = kChunkSize;
        } else {
            chunkLen = dataRem;
        }

        /* read a chunk from the source file */
        size_t result = fread(buf, 1, chunkLen, fpArchive->fFp);
        if (result != chunkLen) {
            pMsg->Format(L"File read failed: %hs.", strerror(errno));
            err = -1;
            goto bail;
        }

        /* write chunk to destination file */
        int err = GenericEntry::WriteConvert(outfp, buf, chunkLen, &conv,
                    &convHA, &lastCR);
        if (err != 0) {
            pMsg->Format(L"File write failed: %hs.", strerror(err));
            err = -1;
            goto bail;
        }

        dataRem -= chunkLen;
        SET_PROGRESS_UPDATE(ComputePercent(srcLen - dataRem, srcLen));
    }

bail:
    delete[] buf;
    return err;
}


/*
 * ===========================================================================
 *      AppleSingleArchive
 * ===========================================================================
 */

/*static*/ CString AppleSingleArchive::AppInit(void)
{
    return L"";
}

GenericArchive::OpenResult AppleSingleArchive::Open(const WCHAR* filename,
    bool readOnly, CString* pErrMsg)
{
    CString errMsg;

    errno = 0;
    fFp = _wfopen(filename, L"rb");
    if (fFp == NULL) {
        errMsg.Format(L"Unable to open %ls: %hs.", filename, strerror(errno));
        goto bail;
    }

    // Set this before calling LoadContents() -- we may need to use it as
    // the name of the archived file.
    SetPathName(filename);

    {
        CWaitCursor waitc;
        int result;

        result = LoadContents();
        if (result < 0) {
            errMsg.Format(L"The file is not an AppleSingle archive.");
            goto bail;
        } else if (result > 0) {
            errMsg.Format(L"Failed while reading data from AppleSingle file.");
            goto bail;
        }
    }

bail:
    *pErrMsg = errMsg;
    if (!errMsg.IsEmpty())
        return kResultFailure;
    else
        return kResultSuccess;
}

CString AppleSingleArchive::New(const WCHAR* /*filename*/, const void* /*options*/)
{
    return L"Sorry, AppleSingle files can't be created.";
}

long AppleSingleArchive::GetCapability(Capability cap)
{
    switch (cap) {
    case kCapCanTest:               return false; break;
    case kCapCanRenameFullPath:     return false; break;
    case kCapCanRecompress:         return false; break;
    case kCapCanEditComment:        return true;  break;
    case kCapCanAddDisk:            return false; break;
    case kCapCanConvEOLOnAdd:       return false; break;
    case kCapCanCreateSubdir:       return false; break;
    case kCapCanRenameVolume:       return false; break;
    default:
        ASSERT(false);
        return -1;
        break;
    }
}

int AppleSingleArchive::LoadContents(void)
{
    ASSERT(fFp != NULL);
    rewind(fFp);

    /*
     * Read the file header.
     */
    uint8_t headerBuf[kHeaderLen];
    if (fread(headerBuf, 1, kHeaderLen, fFp) != kHeaderLen) {
        return -1;      // probably not AppleSingle
    }
    if (headerBuf[1] == 0x05) {
        // big-endian (spec-compliant)
        fIsBigEndian = true;
        fHeader.magic = Get32BE(&headerBuf[0]);
        fHeader.version = Get32BE(&headerBuf[4]);
        fHeader.numEntries = Get16BE(&headerBuf[8 + kHomeFileSystemLen]);
    } else {
        // little-endian (Mac OS X generated)
        fIsBigEndian = false;
        fHeader.magic = Get32LE(&headerBuf[0]);
        fHeader.version = Get32LE(&headerBuf[4]);
        fHeader.numEntries = Get16LE(&headerBuf[8 + kHomeFileSystemLen]);
    }
    memcpy(fHeader.homeFileSystem, &headerBuf[8], kHomeFileSystemLen);
    fHeader.homeFileSystem[kHomeFileSystemLen] = '\0';

    if (fHeader.magic != kMagicNumber) {
        LOGD("File does not have AppleSingle magic number");
        return -1;
    }
    if (fHeader.version != kVersion1 && fHeader.version != kVersion2) {
        LOGI("AS file has unrecognized version number 0x%08x", fHeader.version);
        return -1;
    }

    /*
     * Read the entries (a table of contents).  There are at most 65535
     * entries, so we don't need to worry about capping it at a "reasonable"
     * size.
     */
    size_t totalEntryLen = fHeader.numEntries * kEntryLen;
    uint8_t* entryBuf = new uint8_t[totalEntryLen];
    if (fread(entryBuf, 1, totalEntryLen, fFp) != totalEntryLen) {
        LOGW("Unable to read entry list from AS file (err=%d)", errno);
        delete[] entryBuf;
        return 1;
    }
    fEntries = new TOCEntry[fHeader.numEntries];
    const uint8_t* ptr = entryBuf;
    for (size_t i = 0; i < fHeader.numEntries; i++, ptr += kEntryLen) {
        if (fIsBigEndian) {
            fEntries[i].entryId = Get32BE(ptr);
            fEntries[i].offset = Get32BE(ptr + 4);
            fEntries[i].length = Get32BE(ptr + 8);
        } else {
            fEntries[i].entryId = Get32LE(ptr);
            fEntries[i].offset = Get32LE(ptr + 4);
            fEntries[i].length = Get32LE(ptr + 8);
        }
    }

    delete[] entryBuf;

    /*
     * Make sure the file actually has everything.
     */
    if (!CheckFileLength()) {
        return 1;
    }

    /*
     * Walk through the TOC entries, using them to fill out the fields in an
     * AppleSingleEntry class.
     */
    if (!CreateEntry()) {
        return 1;
    }

    DumpArchive();

    return 0;
}

bool AppleSingleArchive::CheckFileLength()
{
    // Find the biggest offset+length.
    uint64_t maxPosn = 0;

    for (size_t i = 0; i < fHeader.numEntries; i++) {
        uint64_t end = (uint64_t) fEntries[i].offset + fEntries[i].length;
        if (maxPosn < end) {
            maxPosn = end;
        }
    }

    fseek(fFp, 0, SEEK_END);
    long fileLen = ftell(fFp);
    if (fileLen < 0) {
        LOGW("Unable to determine file length");
        return false;
    }
    if (maxPosn > (uint64_t) fileLen) {
        LOGW("AS max=%llu, file len is only %ld", maxPosn, fileLen);
        return false;
    }

    return true;
}

bool AppleSingleArchive::CreateEntry()
{
    AppleSingleEntry* pNewEntry = new AppleSingleEntry(this);
    uint32_t dataLen = 0, rsrcLen = 0;
    bool haveInfo = false;
    bool hasFileName = false;

    for (size_t i = 0; i < fHeader.numEntries; i++) {
        const TOCEntry* pToc = &fEntries[i];
        switch (pToc->entryId) {
        case kIdDataFork:
            if (pNewEntry->GetHasDataFork()) {
                LOGW("Found two data forks in AppleSingle");
                return false;
            }
            dataLen = pToc->length;
            pNewEntry->SetHasDataFork(true);
            pNewEntry->SetDataOffset(pToc->offset);
            pNewEntry->SetDataForkLen(pToc->length);
            break;
        case kIdResourceFork:
            if (pNewEntry->GetHasRsrcFork()) {
                LOGW("Found two rsrc forks in AppleSingle");
                return false;
            }
            rsrcLen = pToc->length;
            pNewEntry->SetHasRsrcFork(true);
            pNewEntry->SetRsrcOffset(pToc->offset);
            pNewEntry->SetRsrcForkLen(pToc->length);
            break;
        case kIdRealName:
            hasFileName = HandleRealName(pToc, pNewEntry);
            break;
        case kIdComment:
            // We could handle this, but I don't think this is widely used.
            break;
        case kIdFileInfo:
            HandleFileInfo(pToc, pNewEntry);
            break;
        case kIdFileDatesInfo:
            HandleFileDatesInfo(pToc, pNewEntry);
            break;
        case kIdFinderInfo:
            if (!haveInfo) {
                HandleFinderInfo(pToc, pNewEntry);
            }
            break;
        case kIdProDOSFileInfo:
            // this take precedence over Finder info
            haveInfo = HandleProDOSFileInfo(pToc, pNewEntry);
            break;
        case kIdBWIcon:
        case kIdColorIcon:
        case kIdMacintoshFileInfo:
        case kIdMSDOSFileInfo:
        case kIdShortName:
        case kIdAFPFileInfo:
        case kIdDirectoryId:
            // We're not interested in these.
            break;
        default:
            LOGD("Ignoring entry with type=%u", pToc->entryId);
            break;
        }
    }

    pNewEntry->SetCompressedLen(dataLen + rsrcLen);
    if (rsrcLen > 0) {  // could do ">=" to preserve empty resource forks
        pNewEntry->SetRecordKind(GenericEntry::kRecordKindForkedFile);
    } else {
        pNewEntry->SetRecordKind(GenericEntry::kRecordKindFile);
    }
    pNewEntry->SetFormatStr(L"Uncompr");

    // If there wasn't a file name, use the AppleSingle file's name, minus
    // any ".as" extension.
    if (!hasFileName) {
        CString fileName(PathName::FilenameOnly(GetPathName(), '\\'));
        if (fileName.GetLength() > 3 &&
                fileName.Right(3).CompareNoCase(L".as") == 0) {
            fileName = fileName.Left(fileName.GetLength() - 3);
        }
        // TODO: convert UTF-16 Unicode to MOR
        CStringA fileNameA(fileName);
        pNewEntry->SetPathNameMOR(fileNameA);
    }

    // This doesn't matter, since we only have the file name, but it keeps
    // the entry from getting a weird default.
    pNewEntry->SetFssep(':');
    
    AddEntry(pNewEntry);
    return true;
}

bool AppleSingleArchive::HandleRealName(const TOCEntry* tocEntry,
    AppleSingleEntry* pEntry)
{
    if (tocEntry->length > 1024) {
        // this is a single file name, not a full path
        LOGW("Ignoring excessively long filename (%u)", tocEntry->length);
        return false;
    }

    (void) fseek(fFp, tocEntry->offset, SEEK_SET);

    char* buf = new char[tocEntry->length + 1];
    if (fread(buf, 1, tocEntry->length, fFp) != tocEntry->length) {
        LOGW("failed reading file name");
        delete[] buf;
        return false;
    }
    buf[tocEntry->length] = '\0';

    if (fHeader.version == kVersion1) {
        // filename is in Mac OS Roman format already
        pEntry->SetPathNameMOR(buf);
    } else {
        // filename is in UTF-8-encoded Unicode
        // TODO: convert UTF-8 to MOR, dropping invalid characters
        pEntry->SetPathNameMOR(buf);
    }

    delete[] buf;
    return true;
}

bool AppleSingleArchive::HandleFileInfo(const TOCEntry* tocEntry,
    AppleSingleEntry* pEntry)
{
    if (strcmp(fHeader.homeFileSystem, "ProDOS          ") != 0) {
        LOGD("Ignoring file info for filesystem '%s'", fHeader.homeFileSystem);
        return false;
    }

    const int kEntrySize = 16;

    if (tocEntry->length != kEntrySize) {
        LOGW("Bad length on ProDOS File Info (%d)", tocEntry->length);
        return false;
    }
    (void) fseek(fFp, tocEntry->offset, SEEK_SET);

    uint8_t buf[kEntrySize];
    if (fread(buf, 1, kEntrySize, fFp) != kEntrySize) {
        LOGW("failed reading ProDOS File Info");
        return false;
    }

    uint16_t createDate, createTime, modDate, modTime, access, fileType;
    uint32_t auxType;

    if (fIsBigEndian) {
        createDate = Get16BE(buf);
        createTime = Get16BE(buf + 2);
        modDate = Get16BE(buf + 4);
        modTime = Get16BE(buf + 6);
        access = Get16BE(buf + 8);
        fileType = Get16BE(buf + 10);
        auxType = Get32BE(buf + 12);
    } else {
        createDate = Get16LE(buf);
        createTime = Get16LE(buf + 2);
        modDate = Get16LE(buf + 4);
        modTime = Get16LE(buf + 6);
        access = Get16LE(buf + 8);
        fileType = Get16LE(buf + 10);
        auxType = Get32LE(buf + 12);
    }

    pEntry->SetAccess(access);
    pEntry->SetFileType(fileType);
    pEntry->SetAuxType(auxType);
    pEntry->SetCreateWhen(ConvertProDOSDateTime(createDate, createTime));
    pEntry->SetModWhen(ConvertProDOSDateTime(modDate, modTime));
    return true;
}

bool AppleSingleArchive::HandleFileDatesInfo(const TOCEntry* tocEntry,
    AppleSingleEntry* pEntry)
{
    const int kEntrySize = 16;

    if (tocEntry->length != kEntrySize) {
        LOGW("Bad length on File Dates info (%d)", tocEntry->length);
        return false;
    }
    (void) fseek(fFp, tocEntry->offset, SEEK_SET);

    uint8_t buf[kEntrySize];
    if (fread(buf, 1, kEntrySize, fFp) != kEntrySize) {
        LOGW("failed reading File Dates info");
        return false;
    }

    int32_t createDate, modDate;
    if (fIsBigEndian) {
        createDate = Get32BE(buf);
        modDate = Get32BE(buf + 4);
        // ignore backup date and access date
    } else {
        createDate = Get32LE(buf);
        modDate = Get32LE(buf + 4);
    }

    // Number of seconds between Jan 1 1970 and Jan 1 2000, computed with
    // Linux mktime().  Does not include leap-seconds.
    //
    const int32_t kTimeOffset = 946684800;

    // The Mac OS X applesingle tool is creating entries with some pretty
    // wild values, so we have to range-check them here or the Windows
    // time conversion method gets bent out of shape.
    //
    // TODO: these are screwy enough that I'm just going to ignore them.
    //   If it turns out I'm holding it wrong we can re-enable it.
    time_t tmpTime = (time_t) createDate + kTimeOffset;
    if (tmpTime >= 0 && tmpTime <= 0xffffffffLL) {
        //pEntry->SetCreateWhen(tmpTime);
    }
    tmpTime = (time_t) modDate + kTimeOffset;
    if (tmpTime >= 0 && tmpTime <= 0xffffffffLL) {
        //pEntry->SetModWhen(tmpTime);
    }

    return false;
}

bool AppleSingleArchive::HandleProDOSFileInfo(const TOCEntry* tocEntry,
    AppleSingleEntry* pEntry)
{
    const int kEntrySize = 8;
    uint16_t access, fileType;
    uint32_t auxType;

    if (tocEntry->length != kEntrySize) {
        LOGW("Bad length on ProDOS file info (%d)", tocEntry->length);
        return false;
    }
    (void) fseek(fFp, tocEntry->offset, SEEK_SET);

    uint8_t buf[kEntrySize];
    if (fread(buf, 1, kEntrySize, fFp) != kEntrySize) {
        LOGW("failed reading ProDOS info");
        return false;
    }

    if (fIsBigEndian) {
        access = Get16BE(buf);
        fileType = Get16BE(buf + 2);
        auxType = Get32BE(buf + 4);
    } else {
        access = Get16LE(buf);
        fileType = Get16LE(buf + 2);
        auxType = Get32LE(buf + 4);
    }

    pEntry->SetAccess(access);
    pEntry->SetFileType(fileType);
    pEntry->SetAuxType(auxType);
    return true;
}

bool AppleSingleArchive::HandleFinderInfo(const TOCEntry* tocEntry,
    AppleSingleEntry* pEntry)
{
    const int kEntrySize = 32;
    const int kPdosType = 0x70646f73;   // 'pdos'
    uint32_t creator, macType;

    if (tocEntry->length != kEntrySize) {
        LOGW("Bad length on Finder info (%d)", tocEntry->length);
        return false;
    }
    (void) fseek(fFp, tocEntry->offset, SEEK_SET);

    uint8_t buf[kEntrySize];
    if (fread(buf, 1, kEntrySize, fFp) != kEntrySize) {
        LOGW("failed reading Finder info");
        return false;
    }

    // These values are stored big-endian even on Mac OS X.
    macType = Get32BE(buf);
    creator = Get32BE(buf + 4);

    if (creator == kPdosType && (macType >> 24) == 'p') {
        pEntry->SetFileType((macType >> 16) & 0xff);
        pEntry->SetAuxType(macType & 0xffff);
    } else {
        pEntry->SetFileType(macType);
        pEntry->SetAuxType(creator);
    }
    return true;
}

CString AppleSingleArchive::Reload(void)
{
    fReloadFlag = true;     // tell everybody that cached data is invalid

    DeleteEntries();
    if (LoadContents() != 0) {
        return L"Reload failed.";
    }
    
    return "";
}

CString AppleSingleArchive::GetInfoString()
{
    CString str;

    if (fHeader.version == kVersion1) {
        str += "Version 1, ";
    } else {
        str += "Version 2, ";
    }
    if (fIsBigEndian) {
        str += "big endian";
    } else {
        str += "little endian";
    }

    return str;
}


/*
 * ===========================================================================
 *      Utility functions
 * ===========================================================================
 */

time_t AppleSingleArchive::ConvertProDOSDateTime(uint16_t prodosDate,
    uint16_t prodosTime)
{
    NuDateTime ndt;

    ndt.second = 0;
    ndt.minute = prodosTime & 0x3f;
    ndt.hour = (prodosTime >> 8) & 0x1f;
    ndt.day = (prodosDate & 0x1f) -1;
    ndt.month = ((prodosDate >> 5) & 0x0f) -1;
    ndt.year = (prodosDate >> 9) & 0x7f;
    if (ndt.year < 40)
        ndt.year += 100;     /* P8 uses 0-39 for 2000-2039 */
    ndt.extra = 0;
    ndt.weekDay = 0;

    return NufxArchive::DateTimeToSeconds(&ndt);
}

void AppleSingleArchive::DumpArchive()
{
    LOGI("AppleSingleArchive: %hs magic=0x%08x, version=%08x, entries=%u",
        fIsBigEndian ? "BE" : "LE", fHeader.magic, fHeader.version,
        fHeader.numEntries);
    LOGI(" homeFileSystem='%hs'", fHeader.homeFileSystem);
    for (size_t i = 0; i < fHeader.numEntries; i++) {
        LOGI(" %2u: id=%u off=%u len=%u", i,
            fEntries[i].entryId, fEntries[i].offset, fEntries[i].length);
    }
}
