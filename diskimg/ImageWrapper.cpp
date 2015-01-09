/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Code for handling disk image "wrappers", things like DiskCopy, 2MG, and
 * SHK that surround a disk image.
 *
 * Returning with kDIErrBadChecksum from Test or Prep is taken as a sign
 * that, while we have correctly identified the wrapper format, the contents
 * of the file are corrupt, and the user needs to be told.
 *
 * Some formats, such as 2MG, include a DOS volume number.  This is useful
 * because DOS actually embeds the volume number in sector headers; the value
 * stored in the VTOC is ignored by certain things (notably some games with
 * trivial copy-protection).  This value needs to be preserved.  It's
 * unclear how useful this will actually be; mostly we just want to preserve
 * it when translating from one format to another.
 *
 * If a library (such as NufxLib) needs to read an actual file, it can
 * (usually) pry the name out of the GFD.
 *
 * In general, it should be possible to write to any "wrapped" file that we
 * can read from.  For things like NuFX and DDD, this means we need to be
 * able to re-compress the image file when we're done with it.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"
#include "TwoImg.h"


/*
 * ===========================================================================
 *      2MG (a/k/a 2IMG)
 * ===========================================================================
 */

/*
 * Test to see if this is a 2MG file.
 *
 * The easiest way to do that is to open up the header and see if
 * it looks valid.
 */
/*static*/ DIError Wrapper2MG::Test(GenericFD* pGFD, di_off_t wrappedLength)
{
    TwoImgHeader header;

    LOGI("Testing for 2MG");

    // HEY: should test for wrappedLength > 2GB; if so, skip

    pGFD->Rewind();
    if (header.ReadHeader(pGFD, (long) wrappedLength) != 0)
        return kDIErrGeneric;

    LOGI("Looks like valid 2MG");
    return kDIErrNone;
}

/*
 * Get the header (again) and use it to locate the data.
 */
DIError Wrapper2MG::Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
    di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
    DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
    LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD)
{
    TwoImgHeader header;
    long offset;

    LOGI("Prepping for 2MG");
    pGFD->Rewind();
    if (header.ReadHeader(pGFD, (long) wrappedLength) != 0)
        return kDIErrGeneric;

    offset = header.fDataOffset;

    if (header.fFlags & TwoImgHeader::kDOSVolumeSet)
        *pDiskVolNum = header.GetDOSVolumeNum();

    *pLength = header.fDataLen;
    *pPhysical = DiskImg::kPhysicalFormatSectors;
    if (header.fImageFormat == TwoImgHeader::kImageFormatDOS)
        *pOrder = DiskImg::kSectorOrderDOS;
    else if (header.fImageFormat == TwoImgHeader::kImageFormatProDOS)
        *pOrder = DiskImg::kSectorOrderProDOS;
    else if (header.fImageFormat == TwoImgHeader::kImageFormatNibble) {
        *pOrder = DiskImg::kSectorOrderPhysical;
        if (*pLength == kTrackCount525 * kTrackLenNib525) {
            LOGI("  Prepping for 6656-byte 2MG-NIB");
            *pPhysical = DiskImg::kPhysicalFormatNib525_6656;
        } else if (*pLength == kTrackCount525 * kTrackLenNb2525) {
            LOGI("  Prepping for 6384-byte 2MG-NB2");
            *pPhysical = DiskImg::kPhysicalFormatNib525_6384;
        } else {
            LOGI("  NIB 2MG with length=%ld rejected", (long) *pLength);
            return kDIErrOddLength;
        }
    }

    *ppNewGFD = new GFDGFD;
    return ((GFDGFD*)*ppNewGFD)->Open(pGFD, offset, readOnly);
}

/*
 * Initialize fields for a new file.
 */
DIError Wrapper2MG::Create(di_off_t length, DiskImg::PhysicalFormat physical,
    DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
    di_off_t* pWrappedLength, GenericFD** pDataFD)
{
    TwoImgHeader header;
    int cc;

    switch (physical) {
    case DiskImg::kPhysicalFormatNib525_6656:
        if (length != kTrackLenNib525 * kTrackCount525) {
            LOGI("Invalid 2MG nibble length %ld", (long) length);
            return kDIErrInvalidArg;
        }
        header.InitHeader(TwoImgHeader::kImageFormatNibble, (long) length,
            8 * kTrackCount525);    // 8 blocks per track
        break;
    case DiskImg::kPhysicalFormatSectors:
        if ((length % 512) != 0) {
            LOGI("Invalid 2MG length %ld", (long) length);
            return kDIErrInvalidArg;
        }
        if (order == DiskImg::kSectorOrderProDOS)
            cc = header.InitHeader(TwoImgHeader::kImageFormatProDOS,
                (long) length, (long) length / 512);
        else if (order == DiskImg::kSectorOrderDOS)
            cc = header.InitHeader(TwoImgHeader::kImageFormatDOS,
                (long) length, (long) length / 512);
        else {
            LOGI("Invalid 2MG sector order %d", order);
            return kDIErrInvalidArg;
        }
        if (cc != 0) {
            LOGI("TwoImg InitHeader failed (len=%ld)", (long) length);
            return kDIErrInvalidArg;
        }
        break;
    default:
        LOGI("Invalid 2MG physical %d", physical);
        return kDIErrInvalidArg;
    }

    if (dosVolumeNum != DiskImg::kVolumeNumNotSet)
        header.SetDOSVolumeNum(dosVolumeNum);

    cc = header.WriteHeader(pWrapperGFD);
    if (cc != 0) {
        LOGI("ERROR: 2MG header write failed (cc=%d)", cc);
        return kDIErrGeneric;
    }

    long footerLen = header.fCmtLen + header.fCreatorLen;
    if (footerLen > 0) {
        // This is currently impossible, which is good because the Seek call
        // will fail if pWrapperGFD is a buffer.
        assert(false);
        pWrapperGFD->Seek(header.fDataOffset + length, kSeekSet);
        header.WriteFooter(pWrapperGFD);
    }

    long offset = header.fDataOffset;


    *pWrappedLength = length + offset + footerLen;
    *pDataFD = new GFDGFD;
    return ((GFDGFD*)*pDataFD)->Open(pWrapperGFD, offset, false);
}

/*
 * We only use GFDGFD, so there's nothing to do here.
 *
 * If we want to support changing the comment field in an open image, we'd
 * need to handle making the file longer or shorter here.  Right now we
 * just ignore everything that comes before or after the start of the data.
 * Since there's no checksum, none of the header fields change, so we
 * don't even deal with that.
 */
DIError Wrapper2MG::Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
    di_off_t dataLen, di_off_t* pWrappedLen)
{
    return kDIErrNone;
}


/*
 * ===========================================================================
 *      SHK (ShrinkIt NuFX), also .SDK and .BXY
 * ===========================================================================
 */

/*
 * NOTE: this doesn't override the global error message callback because
 * we expect it to be set by the application.
 */

/*
 * Display error messages... or not.
 */
/*static*/ NuResult WrapperNuFX::ErrMsgHandler(NuArchive* /*pArchive*/,
    void* vErrorMessage)
{
    const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;

    if (pErrorMessage->isDebug) {
        Global::PrintDebugMsg(pErrorMessage->file, pErrorMessage->line,
            "[D] %s\n", pErrorMessage->message);
    } else {
        Global::PrintDebugMsg(pErrorMessage->file, pErrorMessage->line,
            "%s\n", pErrorMessage->message);
    }

    return kNuOK;
}

/*
 * Open a NuFX archive, and verify that it holds exactly one disk archive.
 *
 * On success, the NuArchive pointer and thread idx are set, and 0 is
 * returned.  Returns -1 on failure.
 */
/*static*/ DIError WrapperNuFX::OpenNuFX(const char* pathName, NuArchive** ppArchive,
    NuThreadIdx* pThreadIdx, long* pLength, bool readOnly)
{
    NuError nerr = kNuErrNone;
    NuArchive* pArchive = NULL;
    NuRecordIdx recordIdx;
    NuAttr attr;
    const NuRecord* pRecord;
    const NuThread* pThread = NULL;
    int idx;

    LOGI("Opening file '%s' to test for NuFX", pathName);

    /*
     * Open the archive.
     */
    if (readOnly) {
        nerr = NuOpenRO(pathName, &pArchive);
        if (nerr != kNuErrNone) {
            LOGI(" NuFX unable to open archive (err=%d)", nerr);
            goto bail;
        }
    } else {
        char* tmpPath;

        tmpPath = GenTempPath(pathName);
        if (tmpPath == NULL) {
            nerr = kNuErrInternal;
            goto bail;
        }

        nerr = NuOpenRW(pathName, tmpPath, 0, &pArchive);
        if (nerr != kNuErrNone) {
            LOGI(" NuFX OpenRW failed (nerr=%d)", nerr);
            nerr = kNuErrGeneric;
            delete[] tmpPath;
            goto bail;
        }
        delete[] tmpPath;
    }

    NuSetErrorMessageHandler(pArchive, ErrMsgHandler);

    nerr = NuGetAttr(pArchive, kNuAttrNumRecords, &attr);
    if (nerr != kNuErrNone) {
        LOGI(" NuFX unable to get record count (err=%d)", nerr);
        goto bail;
    }
    if (attr != 1) {
        LOGI(" NuFX archive has %d entries, not disk-only", attr);
        nerr = kNuErrGeneric;
        if (attr > 1)
            goto file_archive;
        else
            goto bail;      // shouldn't get zero-count archives, but...
    }

    /* get the first record */
    nerr = NuGetRecordIdxByPosition(pArchive, 0, &recordIdx);
    if (nerr != kNuErrNone) {
        LOGI(" NuFX unable to get first recordIdx (err=%d)", nerr);
        goto bail;
    }
    nerr = NuGetRecord(pArchive, recordIdx, &pRecord);
    if (nerr != kNuErrNone) {
        LOGI(" NuFX unable to get first record (err=%d)", nerr);
        goto bail;
    }

    /* find a disk image thread */
    for (idx = 0; idx < (int)NuRecordGetNumThreads(pRecord); idx++) {
        pThread = NuGetThread(pRecord, idx);

        if (NuGetThreadID(pThread) == kNuThreadIDDiskImage)
            break;
    }
    if (idx == (int)NuRecordGetNumThreads(pRecord)) {
        LOGI(" NuFX no disk image found in first record");
        nerr = kNuErrGeneric;
        goto file_archive;
    }
    assert(pThread != NULL);
    *pThreadIdx = pThread->threadIdx;

    /*
     * Don't allow zero-length disks.
     */
    *pLength = pThread->actualThreadEOF;
    if (!*pLength) {
        LOGI(" NuFX length of disk image is bad (%ld)", *pLength);
        nerr = kNuErrGeneric;
        goto bail;
    }

    /*
     * Success!
     */
    assert(nerr == kNuErrNone);
    *ppArchive = pArchive;
    pArchive = NULL;

bail:
    if (pArchive != NULL)
        NuClose(pArchive);
    if (nerr == kNuErrNone)
        return kDIErrNone;
    else if (nerr == kNuErrBadMHCRC || nerr == kNuErrBadRHCRC)
        return kDIErrBadChecksum;
    else
        return kDIErrGeneric;

file_archive:
    if (pArchive != NULL)
        NuClose(pArchive);
    return kDIErrFileArchive;
}

/*
 * Load a disk image into memory.
 *
 * Allocates a buffer with the specified length and loads the desired
 * thread into it.
 *
 * In an LZW-I compressed thread, the third byte of the compressed thread
 * data is the disk volume number that P8 ShrinkIt would use when formatting
 * the disk.  In an LZW-II compressed thread, it's the first byte of the
 * compressed data.  Uncompressed disk images simply don't have the disk
 * volume number in them.  Until NufxLib provides a simple way to access
 * this bit of loveliness, we're going to pretend it's not there.
 *
 * Returns 0 on success, -1 on error.
 */
DIError WrapperNuFX::GetNuFXDiskImage(NuArchive* pArchive, NuThreadIdx threadIdx,
    long length, char** ppData)
{
    NuError err;
    NuDataSink* pDataSink = NULL;
    uint8_t* buf = NULL;

    assert(length > 0);
    buf = new uint8_t[length];
    if (buf == NULL)
        return kDIErrMalloc;

    /*
     * Create a buffer and expand the disk image into it.
     */
    err = NuCreateDataSinkForBuffer(true, kNuConvertOff, buf, length,
            &pDataSink);
    if (err != kNuErrNone) {
        LOGI(" NuFX: unable to create data sink (err=%d)", err);
        goto bail;
    }

    err = NuExtractThread(pArchive, threadIdx, pDataSink);
    if (err != kNuErrNone) {
        LOGI(" NuFX: unable to extract thread (err=%d)", err);
        goto bail;
    }

    //err = kNuErrBadThreadCRC; goto bail;  // debug test only

    *ppData = (char*)buf;

bail:
    NuFreeDataSink(pDataSink);
    if (err != kNuErrNone) {
        LOGI(" NuFX GetNuFXDiskImage returning after nuerr=%d", err);
        delete[] buf;
    }
    if (err == kNuErrNone)
        return kDIErrNone;
    else if (err == kNuErrBadDataCRC || err == kNuErrBadThreadCRC)
        return kDIErrBadChecksum;
    else if (err == kNuErrBadData)
        return kDIErrBadCompressedData;
    else if (err == kNuErrBadFormat)
        return kDIErrUnsupportedCompression;
    else
        return kDIErrGeneric;
}

/*
 * Test to see if this is a single-record NuFX archive with a disk archive
 * in it.
 */
/*static*/ DIError WrapperNuFX::Test(GenericFD* pGFD, di_off_t wrappedLength)
{
    DIError dierr;
    NuArchive* pArchive = NULL;
    NuThreadIdx threadIdx;
    long length;
    const char* imagePath;

    imagePath = pGFD->GetPathName();
    if (imagePath == NULL) {
        LOGI("Can't test NuFX on non-file");
        return kDIErrNotSupported;
    }
    LOGI("Testing for NuFX");
    dierr = OpenNuFX(imagePath, &pArchive, &threadIdx, &length, true);
    if (dierr != kDIErrNone)
        return dierr;

    /* success; throw away state in case they don't like us anyway */
    assert(pArchive != NULL);
    NuClose(pArchive);
    
    return kDIErrNone;
}

/*
 * Open the archive, extract the disk image into a memory buffer.
 */
DIError WrapperNuFX::Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
    di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
    DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
    LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD)
{
    DIError dierr = kDIErrNone;
    NuThreadIdx threadIdx;
    GFDBuffer* pNewGFD = NULL;
    char* buf = NULL;
    long length = -1;
    const char* imagePath;

    imagePath = pGFD->GetPathName();
    if (imagePath == NULL) {
        assert(false);      // should've been caught in Test
        return kDIErrNotSupported;
    }
    pGFD->Close();      // don't hold the file open
    dierr = OpenNuFX(imagePath, &fpArchive, &threadIdx, &length, readOnly);
    if (dierr != kDIErrNone)
        goto bail;

    dierr = GetNuFXDiskImage(fpArchive, threadIdx, length, &buf);
    if (dierr != kDIErrNone)
        goto bail;

    pNewGFD = new GFDBuffer;
    dierr = pNewGFD->Open(buf, length, true, false, readOnly);
    if (dierr != kDIErrNone)
        goto bail;
    buf = NULL;      // now owned by pNewGFD;

    /*
     * Success!
     */
    assert(dierr == kDIErrNone);
    *ppNewGFD = pNewGFD;
    *pLength = length;
    *pPhysical = DiskImg::kPhysicalFormatSectors;
    *pOrder = DiskImg::kSectorOrderProDOS;

    LOGI(" NuFX is ready, threadIdx=%d", threadIdx);
    fThreadIdx = threadIdx;

bail:
    if (dierr != kDIErrNone) {
        NuClose(fpArchive);
        fpArchive = NULL;
        delete pNewGFD;
        delete buf;
    }
    return dierr;
}

/*
 * Given a filename, create a suitable temp pathname.
 *
 * This is really the wrong place to be doing this -- the application
 * should get to deal with this -- but it's not the end of the world
 * if we handle it here.  Add to wish list: fix NufxLib so that the
 * temp file can be a memory buffer.
 *
 * Returns a string allocated with new[].
 */
/*static*/ char* WrapperNuFX::GenTempPath(const char* path)
{
    static const char* kTmpTemplate = "DItmp_XXXXXX";
    char* tmpPath;

    assert(path != NULL);
    assert(strlen(path) > 0);

    tmpPath = new char[strlen(path) + 32];
    if (tmpPath == NULL)
        return NULL;

    strcpy(tmpPath, path);

    /* back up to the first thing that looks like it's an fssep */
    char* cp;
    cp = tmpPath + strlen(tmpPath);
    while (--cp >= tmpPath) {
        if (*cp == '/' || *cp == '\\' || *cp == ':')
            break;
    }

    /* we either fell off the back end or found an fssep; advance */
    cp++;

    strcpy(cp, kTmpTemplate);

    LOGI("  NuFX GenTempPath '%s' -> '%s'", path, tmpPath);

    return tmpPath;
}

/*
 * Initialize fields for a new file.
 *
 * "pWrapperGFD" will be fairly useless after this, because we're
 * recreating the underlying file.  (If it doesn't have an underlying
 * file, then we're hosed.)
 */
DIError WrapperNuFX::Create(di_off_t length, DiskImg::PhysicalFormat physical,
    DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
    di_off_t* pWrappedLength, GenericFD** pDataFD)
{
    assert(physical == DiskImg::kPhysicalFormatSectors);
    assert(order == DiskImg::kSectorOrderProDOS);

    DIError dierr = kDIErrNone;
    NuArchive* pArchive;
    const char* imagePath;
    char* tmpPath = NULL;
    uint8_t* buf = NULL;
    NuError nerr;

    /*
     * Create the NuFX archive, stomping on the existing file.  (This
     * makes pWrapperGFD invalid, but such is life with NufxLib.)
     */
    imagePath = pWrapperGFD->GetPathName();
    if (imagePath == NULL) {
        assert(false);      // must not have an outer wrapper
        dierr = kDIErrNotSupported;
        goto bail;
    }
    pWrapperGFD->Close();       // don't hold the file open
    tmpPath = GenTempPath(imagePath);
    if (tmpPath == NULL) {
        dierr = kDIErrInternal;
        goto bail;
    }

    nerr = NuOpenRW(imagePath, tmpPath, kNuOpenCreat, &pArchive);
    if (nerr != kNuErrNone) {
        LOGI(" NuFX OpenRW failed (nerr=%d)", nerr);
        dierr = kDIErrGeneric;
        goto bail;
    }

    /*
     * Create a blank chunk of memory for the image.
     */
    assert(length > 0);
    buf = new uint8_t[(int) length];
    if (buf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    GFDBuffer* pNewGFD;
    pNewGFD = new GFDBuffer;
    dierr = pNewGFD->Open(buf, length, true, false, false);
    if (dierr != kDIErrNone) {
        delete pNewGFD;
        goto bail;
    }
    *pDataFD = pNewGFD;
    buf = NULL;          // now owned by pNewGFD;

    /*
     * Success!  Set misc stuff.
     */
    fThreadIdx = 0;     // don't have one to overwrite
    fpArchive = pArchive;

bail:
    delete[] tmpPath;
    delete[] buf;
    return dierr;
}

/*
 * Close the NuFX archive.
 */
DIError WrapperNuFX::CloseNuFX(void)
{
    NuError nerr;

    /* throw away any un-flushed changes so that "close" can't fail */
    (void) NuAbort(fpArchive);

    nerr = NuClose(fpArchive);
    if (nerr != kNuErrNone) {
        LOGI("WARNING: NuClose failed");
        return kDIErrGeneric;
    }
    return kDIErrNone;
}

/*
 * Write the data using the default compression method.
 *
 * Doesn't touch "pWrapperGFD" or "pWrappedLen".  Could probably update
 * "pWrappedLen", but that's really only useful if we have a gzip Outer
 * that wants to know how much data we have.  Because we don't write to
 * pWrapperGFD, we can't have a gzip wrapper, so there's no point in
 * updating it.
 */
DIError WrapperNuFX::Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
    di_off_t dataLen, di_off_t* pWrappedLen)
{
    NuError nerr = kNuErrNone;
    NuFileDetails fileDetails;
    NuRecordIdx recordIdx;
    NuThreadIdx threadIdx;
    NuDataSource* pDataSource = NULL;

    if (fThreadIdx != 0) {
        /*
         * Mark the old record for deletion.
         */
        nerr = NuGetRecordIdxByPosition(fpArchive, 0, &recordIdx);
        if (nerr != kNuErrNone) {
            LOGI(" NuFX unable to get first recordIdx (err=%d)", nerr);
            goto bail;
        }
        nerr = NuDeleteRecord(fpArchive, recordIdx);
        if (nerr != kNuErrNone) {
            LOGI(" NuFX unable to delete first record (err=%d)", nerr);
            goto bail;
        }
    }

    assert((dataLen % 512) == 0);

    nerr = NuSetValue(fpArchive, kNuValueDataCompression,
                fCompressType + kNuCompressNone);
    if (nerr != kNuErrNone) {
        LOGI("WARNING: unable to set compression to format %d",
            fCompressType);
        nerr = kNuErrNone;
    } else {
        LOGI(" NuFX set compression to %d/%d", fCompressType,
            fCompressType + kNuCompressNone);
    }

    /*
     * Fill out the fileDetails record appropriately.
     */
    memset(&fileDetails, 0, sizeof(fileDetails));
    fileDetails.threadID = kNuThreadIDDiskImage;
    if (fStorageName != NULL)
        fileDetails.storageNameMOR = fStorageName;  // TODO
    else
        fileDetails.storageNameMOR = "NEW.DISK";
    fileDetails.fileSysID = kNuFileSysUnknown;
    fileDetails.fileSysInfo = kDefaultStorageFssep;
    fileDetails.storageType = 512;
    fileDetails.extraType = (long) (dataLen / 512);
    fileDetails.access = kNuAccessUnlocked;

    time_t now;
    now = time(NULL);
    UNIXTimeToDateTime(&now, &fileDetails.archiveWhen);
    UNIXTimeToDateTime(&now, &fileDetails.modWhen);
    UNIXTimeToDateTime(&now, &fileDetails.createWhen);

    /*
     * Create the new record.
     */
    nerr = NuAddRecord(fpArchive, &fileDetails, &recordIdx);
    if (nerr != kNuErrNone) {
        LOGI(" NuFX AddRecord failed (nerr=%d)", nerr);
        goto bail;
    }

    /*
     * Create a data source for the thread.
     *
     * We need to get the memory buffer from pDataGFD, which we do in
     * a somewhat unwholesome manner.  However, there's no other way to
     * feed the data into NufxLib.
     */
    nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed, 0,
            (const uint8_t*) ((GFDBuffer*) pDataGFD)->GetBuffer(),
            0, (long) dataLen, NULL, &pDataSource);
    if (nerr != kNuErrNone) {
        LOGI(" NuFX unable to create NufxLib data source (nerr=%d)", nerr);
        goto bail;
    }

    /*
     * Add the thread.
     */
    nerr = NuAddThread(fpArchive, recordIdx, kNuThreadIDDiskImage,
            pDataSource, &threadIdx);
    if (nerr != kNuErrNone) {
        LOGI(" NuFX AddThread failed (nerr=%d)", nerr);
        goto bail;
    }
    pDataSource = NULL;      // now owned by NufxLib
    LOGI(" NuFX added thread %d in record %d, flushing changes",
        threadIdx, recordIdx);

    /*
     * Flush changes (does the actual compression).
     */
    uint32_t status;
    nerr = NuFlush(fpArchive, &status);
    if (nerr != kNuErrNone) {
        LOGI(" NuFX flush failed (nerr=%d, status=%u)", nerr, status);
        goto bail;
    }

    /* update the threadID */
    fThreadIdx = threadIdx;

bail:
    NuFreeDataSource(pDataSource);
    if (nerr != kNuErrNone)
        return kDIErrGeneric;
    return kDIErrNone;
}

/*
 * Common NuFX utility function.  This ought to be in NufxLib.
 */
void WrapperNuFX::UNIXTimeToDateTime(const time_t* pWhen, NuDateTime *pDateTime)
{
    struct tm* ptm;

    assert(pWhen != NULL);
    assert(pDateTime != NULL);

    ptm = localtime(pWhen);
    pDateTime->second = ptm->tm_sec;
    pDateTime->minute = ptm->tm_min;
    pDateTime->hour = ptm->tm_hour;
    pDateTime->day = ptm->tm_mday -1;
    pDateTime->month = ptm->tm_mon;
    pDateTime->year = ptm->tm_year;
    pDateTime->extra = 0;
    pDateTime->weekDay = ptm->tm_wday +1;
}


/*
 * ===========================================================================
 *      DDD (DDD 2.1, DDD Pro)
 * ===========================================================================
 */

/*
 * There really isn't a way to test if the file is a DDD archive, except
 * to try to unpack it.  One thing we can do fairly quickly is look for
 * runs of repeated bytes, which will be impossible in a DDD file because
 * we compress runs of repeated bytes with RLE.
 */
/*static*/ DIError WrapperDDD::Test(GenericFD* pGFD, di_off_t wrappedLength)
{
    DIError dierr;
    GenericFD* pNewGFD = NULL;
    LOGI("Testing for DDD");

    pGFD->Rewind();

    dierr = CheckForRuns(pGFD);
    if (dierr != kDIErrNone)
        return dierr;

    dierr = Unpack(pGFD, &pNewGFD, NULL);
    delete pNewGFD;
    return dierr;
}

/*
 * Load a bunch of data and check it for repeated byte sequences that
 * would be removed by RLE.  Runs of 4 bytes or longer should have been
 * stripped out.  DDD adds a couple of zeroes onto the end, so to avoid
 * special cases we assume that a run of 5 is okay, and only flunk the
 * data when it gets to 6.
 *
 * One big exception: the "favorites" table isn't run-length encoded,
 * and if the track is nothing but zeroes the entire thing will be
 * filled with 0xff.  So we allow runs of 0xff bytes.
 *
 * PROBLEM: some sequences, such as repeated d5aa, can turn into what looks
 * like a run of bytes in the output.  We can't assume that arbitrary
 * sequences of bytes won't be repeated.  It does appear that we can assume
 * that 00 bytes won't be repeated, so we can still scan for a series of
 * zeroes and reject the image if found (which should clear us for all
 * uncompressed formats and any compressed format with a padded file header).
 *
 * The goal is to detect uncompressed data sources.  The test for DDD
 * should come after other compressed data formats.
 *
 * For speed we crank the data in 8K at a time and don't correctly handle
 * the boundaries.  We do, however, need to avoid scanning the last 256
 * bytes of the file, because DOS DDD just fills it with junk, and it's
 * possible that junk might have runs in it.
 */
/*static*/ DIError WrapperDDD::CheckForRuns(GenericFD* pGFD)
{
    DIError dierr = kDIErrNone;
    int kRunThreshold = 5;
    uint8_t buf[8192];
    size_t bufCount;
    int runLen;
    di_off_t fileLen;
    int i;

    dierr = pGFD->Seek(0, kSeekEnd);
    if (dierr != kDIErrNone)
        goto bail;
    fileLen = pGFD->Tell();
    pGFD->Rewind();

    fileLen -= 256;     // could be extra data from DOS DDD

    while (fileLen) {
        bufCount = (size_t) fileLen;
        if (bufCount > sizeof(buf))
            bufCount = sizeof(buf);
        fileLen -= bufCount;

        dierr = pGFD->Read(buf, bufCount);
        if (dierr != kDIErrNone)
            goto bail;
        //LOGI(" DDD READ %d bytes", bufCount);
        if (dierr != kDIErrNone) {
            LOGI(" DDD CheckForRuns read failed (err=%d)", dierr);
            return dierr;
        }

        runLen = 0;
        for (i = 1; i < (int) bufCount; i++) {
            if (buf[i] == 0 && buf[i] == buf[i-1]) {
                runLen++;
                if (runLen == kRunThreshold && buf[i] != 0xff) {
                    LOGI(" DDD found run of >= %d of 0x%02x, bailing",
                        runLen+1, buf[i]);
                    return kDIErrGeneric;
                }
            } else {
                runLen = 0;
            }
        }
    }

    LOGI(" DDD CheckForRuns scan complete, no long runs found");

bail:
    return dierr;
}

/*
 * Prepping is much the same as testing, but we fill in a few more details.
 */
DIError WrapperDDD::Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
    di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
    DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
    LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD)
{
    DIError dierr;
    LOGI("Prepping for DDD");

    assert(*ppNewGFD == NULL);

    dierr = Unpack(pGFD, ppNewGFD, pDiskVolNum);
    if (dierr != kDIErrNone)
        return dierr;

    *pLength = kNumTracks * kTrackLen;
    *pPhysical = DiskImg::kPhysicalFormatSectors;
    *pOrder = DiskImg::kSectorOrderDOS;

    return dierr;
}

/*
 * Unpack a compressed disk image from "pGFD" to a new memory buffer
 * created in "*ppNewGFD".
 */
/*static*/ DIError WrapperDDD::Unpack(GenericFD* pGFD, GenericFD** ppNewGFD,
    short* pDiskVolNum)
{
    DIError dierr;
    GFDBuffer* pNewGFD = NULL;
    uint8_t* buf = NULL;
    short diskVolNum;

    pGFD->Rewind();

    buf = new uint8_t[kNumTracks * kTrackLen];
    if (buf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    pNewGFD = new GFDBuffer;
    if (pNewGFD == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }
    dierr = pNewGFD->Open(buf, kNumTracks * kTrackLen, true, false, false);
    if (dierr != kDIErrNone)
        goto bail;
    buf = NULL;      // now owned by pNewGFD;

    dierr = UnpackDisk(pGFD, pNewGFD, &diskVolNum);
    if (dierr != kDIErrNone)
        goto bail;

    if (pDiskVolNum != NULL)
        *pDiskVolNum = diskVolNum;
    *ppNewGFD = pNewGFD;
    pNewGFD = NULL;  // now owned by caller

bail:
    delete[] buf;
    delete pNewGFD;
    return dierr;
}

/*
 * Initialize stuff for a new file.  There's no file header or other
 * goodies, so we leave "pWrapperGFD" and "pWrappedLength" alone.
 */
DIError WrapperDDD::Create(di_off_t length, DiskImg::PhysicalFormat physical,
    DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
    di_off_t* pWrappedLength, GenericFD** pDataFD)
{
    assert(length == kNumTracks * kTrackLen);
    assert(physical == DiskImg::kPhysicalFormatSectors);
    assert(order == DiskImg::kSectorOrderDOS);

    DIError dierr;
    uint8_t* buf = NULL;

    /*
     * Create a blank chunk of memory for the image.
     */
    buf = new uint8_t[(int) length];
    if (buf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    GFDBuffer* pNewGFD;
    pNewGFD = new GFDBuffer;
    dierr = pNewGFD->Open(buf, length, true, false, false);
    if (dierr != kDIErrNone) {
        delete pNewGFD;
        goto bail;
    }
    *pDataFD = pNewGFD;
    buf = NULL;          // now owned by pNewGFD;

    // can't set *pWrappedLength yet

    if (dosVolumeNum != DiskImg::kVolumeNumNotSet)
        fDiskVolumeNum = dosVolumeNum;
    else
        fDiskVolumeNum = kDefaultNibbleVolumeNum;

bail:
    delete[] buf;
    return dierr;
}

/*
 * Compress the disk image.
 */
DIError WrapperDDD::Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
    di_off_t dataLen, di_off_t* pWrappedLen)
{
    DIError dierr;

    assert(dataLen == kNumTracks * kTrackLen);

    pDataGFD->Rewind();

    dierr = PackDisk(pDataGFD, pWrapperGFD, fDiskVolumeNum);
    if (dierr != kDIErrNone)
        return dierr;

    *pWrappedLen = pWrapperGFD->Tell();
    LOGI("  DDD compressed from %d to %ld",
        kNumTracks * kTrackLen, (long) *pWrappedLen);

    return kDIErrNone;
}


/*
 * ===========================================================================
 *      DiskCopy (primarily a Mac format)
 * ===========================================================================
 */

/*
 * DiskCopy 4.2 header, from FTN $e0/0005.
 *
 * All values are BIG-endian.
 */
const int kDC42NameLen = 64;
const int kDC42ChecksumOffset = 72;     // where the checksum lives
const int kDC42DataOffset = 84;         // header is always this long
const int kDC42PrivateMagic = 0x100;
const int kDC42FakeTagLen = 19200;      // add a "fake" tag to match Mac

typedef struct DiskImgLib::DC42Header {
    char        diskName[kDC42NameLen+1];   // from pascal string
    uint32_t    dataSize;           // in bytes
    uint32_t    tagSize;
    uint32_t    dataChecksum;
    uint32_t    tagChecksum;
    uint8_t     diskFormat;         // should be 1 for 800K
    uint8_t     formatByte;         // should be $24, sometimes $22
    uint16_t    privateWord;        // must be 0x0100
    // userData begins at +84
    // tagData follows user data
} DC42Header;

/*
 * Dump the contents of a DC42Header.
 */
/*static*/ void WrapperDiskCopy42::DumpHeader(const DC42Header* pHeader)
{
    LOGI("--- header contents:");
    LOGI("\tdiskName      = '%s'", pHeader->diskName);
    LOGI("\tdataSize      = %d (%dK)", pHeader->dataSize,
        pHeader->dataSize / 1024);
    LOGI("\ttagSize       = %d", pHeader->tagSize);
    LOGI("\tdataChecksum  = 0x%08x", pHeader->dataChecksum);
    LOGI("\ttagChecksum   = 0x%08x", pHeader->tagChecksum);
    LOGI("\tdiskFormat    = %d", pHeader->diskFormat);
    LOGI("\tformatByte    = 0x%02x", pHeader->formatByte);
    LOGI("\tprivateWord   = 0x%04x", pHeader->privateWord);
}

/*
 * Init a DC42 header for an 800K ProDOS disk.
 */
void WrapperDiskCopy42::InitHeader(DC42Header* pHeader)
{
    memset(pHeader, 0, sizeof(*pHeader));
    if (fStorageName == NULL || strlen(fStorageName) == 0)
        strcpy(pHeader->diskName, "-not a Macintosh disk");
    else
        strcpy(pHeader->diskName, fStorageName);
    pHeader->dataSize = 819200;
    pHeader->tagSize = kDC42FakeTagLen;     // emulate Mac behavior
    pHeader->dataChecksum = 0xffffffff;     // fixed during Flush
    pHeader->tagChecksum = 0x00000000;      // 19200 zeroes
    pHeader->diskFormat = 1;
    pHeader->formatByte = 0x24;
    pHeader->privateWord = kDC42PrivateMagic;
}

/*
 * Read the header from a DC42 file and verify it.
 *
 * Returns 0 on success, -1 on error or invalid header.
 */
/*static*/ int WrapperDiskCopy42::ReadHeader(GenericFD* pGFD, DC42Header* pHeader)
{
    uint8_t hdrBuf[kDC42DataOffset];

    if (pGFD->Read(hdrBuf, kDC42DataOffset) != kDIErrNone)
        return -1;

    // test the Pascal length byte
    if (hdrBuf[0] >= kDC42NameLen)
        return -1;

    memcpy(pHeader->diskName, &hdrBuf[1], hdrBuf[0]);
    pHeader->diskName[hdrBuf[0]] = '\0';

    pHeader->dataSize = GetLongBE(&hdrBuf[64]);
    pHeader->tagSize = GetLongBE(&hdrBuf[68]);
    pHeader->dataChecksum = GetLongBE(&hdrBuf[72]);
    pHeader->tagChecksum = GetLongBE(&hdrBuf[76]);
    pHeader->diskFormat = hdrBuf[80];
    pHeader->formatByte = hdrBuf[81];
    pHeader->privateWord = GetShortBE(&hdrBuf[82]);

    if (pHeader->dataSize != 800 * 1024 ||
        pHeader->diskFormat != 1 ||
        (pHeader->formatByte != 0x22 && pHeader->formatByte != 0x24) ||
        pHeader->privateWord != kDC42PrivateMagic)
    {
        return -1;
    }

    return 0;
}

/*
 * Write the header for a DC42 file.
 */
DIError WrapperDiskCopy42::WriteHeader(GenericFD* pGFD, const DC42Header* pHeader)
{
    uint8_t hdrBuf[kDC42DataOffset];

    pGFD->Rewind();

    memset(hdrBuf, 0, sizeof(hdrBuf));
    /*
     * Disks created on a Mac include the null byte in the count; not sure
     * if this applies to volume labels or just the "not a Macintosh disk"
     * magic string.  To be safe, we only increment it if it starts with '-'.
     * (Need access to a Macintosh to test this.)
     */
    hdrBuf[0] = strlen(pHeader->diskName);
    if (pHeader->diskName[0] == '-' && hdrBuf[0] < (kDC42NameLen-1))
        hdrBuf[0]++;
    memcpy(&hdrBuf[1], pHeader->diskName, hdrBuf[0]);

    PutLongBE(&hdrBuf[64], pHeader->dataSize);
    PutLongBE(&hdrBuf[68], pHeader->tagSize);
    PutLongBE(&hdrBuf[72], pHeader->dataChecksum);
    PutLongBE(&hdrBuf[76], pHeader->tagChecksum);
    hdrBuf[80] = pHeader->diskFormat;
    hdrBuf[81] = pHeader->formatByte;
    PutShortBE(&hdrBuf[82], pHeader->privateWord);

    return pGFD->Write(hdrBuf, kDC42DataOffset);
}

/*
 * Check to see if this is a DiskCopy 4.2 image.
 *
 * The format doesn't really have a magic number, but if we're stringent
 * about our interpretation of some of the header fields (e.g. we only
 * recognize 800K disks) we should be okay.
 */
/*static*/ DIError WrapperDiskCopy42::Test(GenericFD* pGFD, di_off_t wrappedLength)
{
    DC42Header header;

    LOGI("Testing for DiskCopy");

    if (wrappedLength < 800 * 1024 + kDC42DataOffset)
        return kDIErrGeneric;

    pGFD->Rewind();
    if (ReadHeader(pGFD, &header) != 0)
        return kDIErrGeneric;

    DumpHeader(&header);

    return kDIErrNone;
}

/*
 * Compute the funky DiskCopy checksum.
 *
 * Position "pGFD" at the start of data.
 */
/*static*/ DIError WrapperDiskCopy42::ComputeChecksum(GenericFD* pGFD, uint32_t* pChecksum)
{
    DIError dierr = kDIErrNone;
    uint8_t buf[512];
    long dataRem = 800 * 1024 /*pHeader->dataSize*/;
    uint32_t checksum;

    assert(dataRem % sizeof(buf) == 0);
    assert((sizeof(buf) & 0x01) == 0);  // we take it two bytes at a time

    checksum = 0;
    while (dataRem) {
        int i;

        dierr = pGFD->Read(buf, sizeof(buf));
        if (dierr != kDIErrNone) {
            LOGI(" DC42 read failed, dataRem=%ld (err=%d)", dataRem, dierr);
            return dierr;
        }

        for (i = 0; i < (int) sizeof(buf); i += 2) {
            uint16_t val = GetShortBE(buf+i);

            checksum += val;
            if (checksum & 0x01)
                checksum = checksum >> 1 | 0x80000000;
            else
                checksum = checksum >> 1;
        }

        dataRem -= sizeof(buf);
    }

    *pChecksum = checksum;

    return dierr;
}

/*
 * Prepare a DiskCopy image for use.
 */
DIError WrapperDiskCopy42::Prep(GenericFD* pGFD, di_off_t wrappedLength,
    bool readOnly, di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
    DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
    LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD)
{
    DIError dierr;
    DC42Header header;

    LOGI("Prepping for DiskCopy 4.2");
    pGFD->Rewind();
    if (ReadHeader(pGFD, &header) != 0)
        return kDIErrGeneric;

    /*
     * Verify checksum.  File should already be seeked to appropriate place.
     */
    uint32_t checksum;
    dierr = ComputeChecksum(pGFD, &checksum);
    if (dierr != kDIErrNone)
        return dierr;

    if (checksum != header.dataChecksum) {
        LOGW(" DC42 checksum mismatch (got 0x%08x, expected 0x%08x)",
            checksum, header.dataChecksum);
        fBadChecksum = true;
        //return kDIErrBadChecksum;
    } else {
        LOGD(" DC42 checksum matches!");
    }


    /* looks good! */
    *pLength = header.dataSize;
    *pPhysical = DiskImg::kPhysicalFormatSectors;
    *pOrder = DiskImg::kSectorOrderProDOS;

    *ppNewGFD = new GFDGFD;
    return ((GFDGFD*)*ppNewGFD)->Open(pGFD, kDC42DataOffset, readOnly);

}

/*
 * Initialize fields for a new file.
 */
DIError WrapperDiskCopy42::Create(di_off_t length, DiskImg::PhysicalFormat physical,
    DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
    di_off_t* pWrappedLength, GenericFD** pDataFD)
{
    DIError dierr;
    DC42Header header;

    assert(length == 800 * 1024);
    assert(physical == DiskImg::kPhysicalFormatSectors);
    //assert(order == DiskImg::kSectorOrderProDOS);

    InitHeader(&header);        // set all but checksum

    dierr = WriteHeader(pWrapperGFD, &header);
    if (dierr != kDIErrNone) {
        LOGI("ERROR: 2MG header write failed (err=%d)", dierr);
        return dierr;
    }

    *pWrappedLength = length + kDC42DataOffset;
    *pDataFD = new GFDGFD;
    return ((GFDGFD*)*pDataFD)->Open(pWrapperGFD, kDC42DataOffset, false);
}

/*
 * We only use GFDGFD, so there's no data to write.  However, we do need
 * to update the checksum, and append our "fake" tag section.
 */
DIError WrapperDiskCopy42::Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
    di_off_t dataLen, di_off_t* pWrappedLen)
{
    DIError dierr;
    uint32_t checksum;

    /* compute the data checksum */
    dierr = pWrapperGFD->Seek(kDC42DataOffset, kSeekSet);
    if (dierr != kDIErrNone)
        goto bail;

    dierr = ComputeChecksum(pWrapperGFD, &checksum);
    if (dierr != kDIErrNone) {
        LOGI(" DC42 failed while computing checksum (err=%d)", dierr);
        goto bail;
    }

    /* write it into the wrapper */
    dierr = pWrapperGFD->Seek(kDC42ChecksumOffset, kSeekSet);
    if (dierr != kDIErrNone)
        goto bail;

    dierr = WriteLongBE(pWrapperGFD, checksum);
    if (dierr != kDIErrNone)
        goto bail;

    /* add the tag bytes */
    dierr = pWrapperGFD->Seek(kDC42DataOffset + 800*1024, kSeekSet);
    char* tmpBuf;
    tmpBuf = new char[kDC42FakeTagLen];
    if (tmpBuf == NULL)
        return kDIErrMalloc;
    memset(tmpBuf, 0, kDC42FakeTagLen);
    dierr = pWrapperGFD->Write(tmpBuf, kDC42FakeTagLen, NULL);
    delete[] tmpBuf;
    if (dierr != kDIErrNone)
        goto bail;

bail:
    return dierr;
}


/*
 * ===========================================================================
 *      Sim2eHDV (Sim2e virtual hard-drive images)
 * ===========================================================================
 */

/*
//  mkhdv.c
//
//  Create a Hard Disk Volume File (.HDV) for simIIe
static  int mkhdv(FILE *op, uint blocks)
{
    byte    sector[512];
    byte    data[15];
    uint    i;

    memset(data, 0, sizeof(data));
    memcpy(data, "SIMSYSTEM_HDV", 13);
    data[13] = (blocks & 0xff);
    data[14] = (blocks & 0xff00) >> 8;
    fwrite(data, 1, sizeof(data), op);

    memset(sector, 0, sizeof(sector));
    for (i = 0; i < blocks; i++)
        fwrite(sector, 1, sizeof(sector), op);
    return 0;
}
*/

const int kSim2eHeaderLen = 15;
static const char* kSim2eID = "SIMSYSTEM_HDV";

/*
 * Test for a virtual hard-drive image.  This is either a "raw" unadorned
 * image, or one with a 15-byte "SimIIe" header on it.
 */
DIError WrapperSim2eHDV::Test(GenericFD* pGFD, di_off_t wrappedLength)
{
    char buf[kSim2eHeaderLen];

    LOGI("Testing for Sim2e HDV");

    if (wrappedLength < 512 ||
        ((wrappedLength - kSim2eHeaderLen) % 4096) != 0)
    {
        return kDIErrGeneric;
    }

    pGFD->Rewind();

    if (pGFD->Read(buf, sizeof(buf)) != kDIErrNone)
        return kDIErrGeneric;

    if (strncmp(buf, kSim2eID, strlen(kSim2eID)) == 0)
        return kDIErrNone;
    else
        return kDIErrGeneric;
}

/*
 * These are always ProDOS volumes.
 */
DIError WrapperSim2eHDV::Prep(GenericFD* pGFD, di_off_t wrappedLength,
    bool readOnly, di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
    DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
    LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD)
{
    *pLength = wrappedLength - kSim2eHeaderLen;
    *pPhysical = DiskImg::kPhysicalFormatSectors;
    *pOrder = DiskImg::kSectorOrderProDOS;

    *ppNewGFD = new GFDGFD;
    return ((GFDGFD*)*ppNewGFD)->Open(pGFD, kSim2eHeaderLen, readOnly);
}

/*
 * Initialize fields for a new file.
 */
DIError WrapperSim2eHDV::Create(di_off_t length, DiskImg::PhysicalFormat physical,
    DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
    di_off_t* pWrappedLength, GenericFD** pDataFD)
{
    uint8_t header[kSim2eHeaderLen];
    long blocks = (long) (length / 512);

    assert(physical == DiskImg::kPhysicalFormatSectors);
    assert(order == DiskImg::kSectorOrderProDOS);

    if (blocks < 4 || blocks > 65536) {
        LOGI("  Sim2e invalid blocks %ld", blocks);
        return kDIErrInvalidArg;
    }
    if (blocks == 65536)        // 32MB volumes are actually 31.9
        blocks = 65535;

    memcpy(header, kSim2eID, strlen(kSim2eID));
    header[13] = (uint8_t) blocks;
    header[14] = (uint8_t) ((blocks & 0xff00) >> 8);
    DIError dierr = pWrapperGFD->Write(header, kSim2eHeaderLen);
    if (dierr != kDIErrNone) {
        LOGI(" Sim2eHDV header write failed (err=%d)", dierr);
        return dierr;
    }

    *pWrappedLength = length + kSim2eHeaderLen;

    *pDataFD = new GFDGFD;
    return ((GFDGFD*)*pDataFD)->Open(pWrapperGFD, kSim2eHeaderLen, false);
}

/*
 * We only use GFDGFD, so there's nothing to do here.
 */
DIError WrapperSim2eHDV::Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
    di_off_t dataLen, di_off_t* pWrappedLen)
{
    return kDIErrNone;
}


/*
 * ===========================================================================
 *      TrackStar .app images
 * ===========================================================================
 */

/*
 * File format:
 *  $0000  track 0 data
 *  $1a00  track 1 data
 *  $3400  track 2 data
 *   ...
 *  $3f600 track 39 data
 *
 * Each track consists of:
 *  $0000  Text description of disk contents (same on every track), in low
 *         ASCII, padded out with spaces ($20)
 *  $002e  Start of zeroed-out header field
 *  $0080  $00 (indicates end of data when reading from end??)
 *  $0081  Raw nibble data (hi bit set), written backwards
 *  $19fe  Start offset of track data
 *
 * Take the start offset, add 128, and walk backward until you find a
 * value with the high bit clear.  If the start offset is zero, start
 * scanning from $19fd backward.  (This approach courtesty Gerald Ryckman.)
 *
 * My take: the "offset" actually indicates the length of data, and the
 * $00 is there to simplify somebody's algorithm.  If the offset is zero
 * it means the track couldn't be analyzed successfully, so a raw dump has
 * been provided.  Tracks 35-39 on most Apple II disks have zero length,
 * but occasionally one analyzes "successfully" with some horribly truncated
 * length.
 *
 * I'm going to assert that byte $81 be zero and that nothing else has the
 * high bit clear until you hit the end of valid data.
 *
 * Because the nibbles are stored in reverse order, it's easiest to unpack
 * the tracks to local buffers, then re-pack them when saving the file.
 */

/*
 * Test to see if this is a TrackStar 5.25" disk image.
 *
 * While the image format supports variable-length nibble tracks, it uses
 * fixed-length fields to store them.  Each track is stored in 6656 bytes,
 * but has a 129-byte header and a 2-byte footer (max of 6525).
 *
 * Images may be 40-track (5.25") or 80-track (5.25" disk with half-track
 * stepping).  The latter is useful in some circumstances for handling
 * copy-protected disks.  We don't have a half-track interface, so we just
 * ignore the odd-numbered tracks.
 *
 * There is currently no way for the API to set the number of tracks.
 */
/*static*/ DIError WrapperTrackStar::Test(GenericFD* pGFD, di_off_t wrappedLength)
{
    DIError dierr = kDIErrNone;
    LOGI("Testing for TrackStar");
    int numTracks;

    /* check the length */
    if (wrappedLength == 6656*40)
        numTracks = 40;
    else if (wrappedLength == 6656*80)
        numTracks = 80;
    else
        return kDIErrGeneric;

    LOGI("  Checking for %d-track image", numTracks);

    /* verify each track */
    uint8_t trackBuf[kFileTrackStorageLen];
    pGFD->Rewind();
    for (int trk = 0; trk < numTracks; trk++) {
        dierr = pGFD->Read(trackBuf, sizeof(trackBuf));
        if (dierr != kDIErrNone)
            goto bail;
        dierr = VerifyTrack(trk, trackBuf);
        if (dierr != kDIErrNone)
            goto bail;
    }
    LOGI("  TrackStar tracks verified");

bail:
    return dierr;
}

/*
 * Check the format.
 */
/*static*/ DIError WrapperTrackStar::VerifyTrack(int track, const uint8_t* trackBuf)
{
    unsigned int dataLen;

    if (trackBuf[0x80] != 0) {
        LOGI("   TrackStar track=%d found nonzero at 129", track);
        return kDIErrGeneric;
    }

    dataLen = GetShortLE(trackBuf + 0x19fe);
    if (dataLen > kMaxTrackLen) {
        LOGI("   TrackStar track=%d len=%d exceeds max (%d)",
            track, dataLen, kMaxTrackLen);
        return kDIErrGeneric;
    }
    if (dataLen == 0)
        dataLen = kMaxTrackLen;

    unsigned int i;
    for (i = 0; i < dataLen; i++) {
        if ((trackBuf[0x81 + i] & 0x80) == 0) {
            LOGI("   TrackStar track=%d found invalid data 0x%02x at %d",
                track, trackBuf[0x81+i], i);
            return kDIErrGeneric;
        }
    }

    if (track == 0) {
        LOGI("   TrackStar msg='%s'", trackBuf);
    }

    return kDIErrNone;
}

/*
 * Fill in some details.
 */
DIError WrapperTrackStar::Prep(GenericFD* pGFD, di_off_t wrappedLength,
    bool readOnly, di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
    DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
    LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD)
{
    LOGI("Prepping for TrackStar");
    DIError dierr = kDIErrNone;

    if (wrappedLength == kFileTrackStorageLen * 40)
        fImageTracks = 40;
    else if (wrappedLength == kFileTrackStorageLen * 80)
        fImageTracks = 80;
    else
        return kDIErrInternal;

    dierr = Unpack(pGFD, ppNewGFD);
    if (dierr != kDIErrNone)
        return dierr;

    *pLength = kTrackStarNumTracks * kTrackAllocSize;
    *pPhysical = DiskImg::kPhysicalFormatNib525_Var;
    *pOrder = DiskImg::kSectorOrderPhysical;

    return dierr;
}

/*
 * Unpack reverse-order nibbles from "pGFD" to a new memory buffer
 * created in "*ppNewGFD".
 */
DIError WrapperTrackStar::Unpack(GenericFD* pGFD, GenericFD** ppNewGFD)
{
    DIError dierr;
    GFDBuffer* pNewGFD = NULL;
    uint8_t* buf = NULL;

    pGFD->Rewind();

    buf = new uint8_t[kTrackStarNumTracks * kTrackAllocSize];
    if (buf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    pNewGFD = new GFDBuffer;
    if (pNewGFD == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }
    dierr = pNewGFD->Open(buf, kTrackStarNumTracks * kTrackAllocSize,
        true, false, false);
    if (dierr != kDIErrNone)
        goto bail;
    buf = NULL;      // now owned by pNewGFD;

    dierr = UnpackDisk(pGFD, pNewGFD);
    if (dierr != kDIErrNone)
        goto bail;

    *ppNewGFD = pNewGFD;
    pNewGFD = NULL;  // now owned by caller

bail:
    delete[] buf;
    delete pNewGFD;
    return dierr;
}

/*
 * Unpack a TrackStar image.  This is mainly just copying bytes around.  The
 * nibble code is perfectly happy with odd-sized tracks.  However, we want
 * to be able to find a particular track without having to do a lookup.  So,
 * we just block out 40 sets of 6656-byte tracks.
 *
 * The resultant image will always have 40 tracks.  On an 80-track image
 * we skip the odd ones.
 *
 * The bytes are stored in reverse order, so we need to unpack them to a
 * separate buffer.
 *
 * This fills out "fNibbleTrackInfo".
 */
DIError WrapperTrackStar::UnpackDisk(GenericFD* pGFD, GenericFD* pNewGFD)
{
    DIError dierr = kDIErrNone;
    uint8_t inBuf[kFileTrackStorageLen];
    uint8_t outBuf[kTrackAllocSize];
    int i, trk;

    assert(kTrackStarNumTracks <= kMaxNibbleTracks525);

    pGFD->Rewind();
    pNewGFD->Rewind();

    /* we don't currently support half-tracks */
    fNibbleTrackInfo.numTracks = kTrackStarNumTracks;
    for (trk = 0; trk < kTrackStarNumTracks; trk++) {
        unsigned int dataLen;

        fNibbleTrackInfo.offset[trk] = trk * kTrackAllocSize;

        /* these were verified earlier, so assume data is okay */
        dierr = pGFD->Read(inBuf, sizeof(inBuf));
        if (dierr != kDIErrNone)
            goto bail;

        dataLen = GetShortLE(inBuf + 0x19fe);
        if (dataLen == 0)
            dataLen = kMaxTrackLen;
        assert(dataLen <= kMaxTrackLen);
        assert(dataLen <= sizeof(outBuf));

        fNibbleTrackInfo.length[trk] = dataLen;

        memset(outBuf, 0x11, sizeof(outBuf));
        for (i = 0; i < (int) dataLen; i++)
            outBuf[i] = inBuf[128+dataLen-i];

        pNewGFD->Write(outBuf, sizeof(outBuf));

        if (fImageTracks == 2*kTrackStarNumTracks) {
            /* skip the odd-numbered tracks */
            dierr = pGFD->Read(inBuf, sizeof(inBuf));
            if (dierr != kDIErrNone)
                goto bail;
        }
    }

bail:
    return dierr;
}


/*
 * Initialize stuff for a new file.  There's no file header or other
 * goodies, so we leave "pWrapperGFD" and "pWrappedLength" alone.
 */
DIError WrapperTrackStar::Create(di_off_t length, DiskImg::PhysicalFormat physical,
    DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
    di_off_t* pWrappedLength, GenericFD** pDataFD)
{
    assert(length == kTrackLenTrackStar525 * kTrackCount525 ||
           length == kTrackLenTrackStar525 * kTrackStarNumTracks);
    assert(physical == DiskImg::kPhysicalFormatNib525_Var);
    assert(order == DiskImg::kSectorOrderPhysical);

    DIError dierr;
    uint8_t* buf = NULL;
    int numTracks = (int) (length / kTrackLenTrackStar525);
    int i;

    /*
     * Set up the track offset and length table.  We use the maximum
     * data length (kTrackLenTrackStar525) for each.  The nibble write
     * routine will alter the length field as appropriate.
     */
    fNibbleTrackInfo.numTracks = numTracks;
    assert(fNibbleTrackInfo.numTracks <= kMaxNibbleTracks525);
    for (i = 0; i < numTracks; i++) {
        fNibbleTrackInfo.offset[i] = kTrackLenTrackStar525 * i;
        fNibbleTrackInfo.length[i] = kTrackLenTrackStar525;
    }

    /*
     * Create a blank chunk of memory for the image.
     */
    buf = new uint8_t[(int) length];
    if (buf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    GFDBuffer* pNewGFD;
    pNewGFD = new GFDBuffer;
    dierr = pNewGFD->Open(buf, length, true, false, false);
    if (dierr != kDIErrNone) {
        delete pNewGFD;
        goto bail;
    }
    *pDataFD = pNewGFD;
    buf = NULL;          // now owned by pNewGFD;

    // can't set *pWrappedLength yet

bail:
    delete[] buf;
    return dierr;
}

/*
 * Write the stored data into TrackStar format.
 *
 * The source data is in "pDataGFD" in a layout described by fNibbleTrackInfo.
 * We need to create the new file in "pWrapperGFD".
 */
DIError WrapperTrackStar::Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
    di_off_t dataLen, di_off_t* pWrappedLen)
{
    DIError dierr = kDIErrNone;

    assert(dataLen == kTrackLenTrackStar525 * kTrackCount525 ||
           dataLen == kTrackLenTrackStar525 * kTrackStarNumTracks);
    assert(kTrackLenTrackStar525 <= kMaxTrackLen);

    pDataGFD->Rewind();

    uint8_t writeBuf[kFileTrackStorageLen];
    uint8_t dataBuf[kTrackLenTrackStar525];
    int track, trackLen;

    for (track = 0; track < kTrackStarNumTracks; track++) {
        if (track < fNibbleTrackInfo.numTracks) {
            dierr = pDataGFD->Read(dataBuf, kTrackLenTrackStar525);
            if (dierr != kDIErrNone)
                goto bail;
            trackLen = fNibbleTrackInfo.length[track];
            assert(fNibbleTrackInfo.offset[track] == kTrackLenTrackStar525 * track);
        } else {
            LOGI("   TrackStar faking track %d", track);
            memset(dataBuf, 0xff, sizeof(dataBuf));
            trackLen = kMaxTrackLen;
        }

        memset(writeBuf, 0x80, sizeof(writeBuf));   // not strictly necessary
        memset(writeBuf, 0x20, kCommentFieldLen);
        memset(writeBuf+kCommentFieldLen, 0x00, 0x81-kCommentFieldLen);

        const char* comment;
        if (fStorageName != NULL && *fStorageName != '\0')
            comment = fStorageName;
        else
            comment = "(created by CiderPress)";
        if (strlen(comment) > kCommentFieldLen)
            memcpy(writeBuf, comment, kCommentFieldLen);
        else
            memcpy(writeBuf, comment, strlen(comment));

        int i;
        for (i = 0; i < trackLen; i++) {
            // If we write a value here with the high bit clear, we will
            // reject the file when we try to open it.  So, we force the
            // high bit on here, on the assumption that the nibble data
            // we've been handled is otherwise good.
            //writeBuf[0x81+i] = dataBuf[trackLen - i -1];
            writeBuf[0x81+i] = dataBuf[trackLen - i -1] | 0x80;
        }

        if (trackLen == kMaxTrackLen)
            PutShortLE(writeBuf + 0x19fe, 0);
        else
            PutShortLE(writeBuf + 0x19fe, (uint16_t) trackLen);

        dierr = pWrapperGFD->Write(writeBuf, sizeof(writeBuf));
        if (dierr != kDIErrNone)
            goto bail;
    }

    *pWrappedLen = pWrapperGFD->Tell();
    assert(*pWrappedLen == kFileTrackStorageLen * kTrackStarNumTracks);

bail:
    return dierr;
}

void WrapperTrackStar::SetNibbleTrackLength(int track, int length)
{
    assert(track >= 0);
    assert(length > 0 && length <= kMaxTrackLen);
    assert(track < fNibbleTrackInfo.numTracks);

    LOGI("  TrackStar: set length of track %d to %d", track, length);
    fNibbleTrackInfo.length[track] = length;
}


/*
 * ===========================================================================
 *      FDI (Formatted Disk Image) format
 * ===========================================================================
 */

/*
 * The format is described in detail in documents on the "disk2fdi" web site.
 *
 * FDI is currently unique in that it can (and often does) store nibble
 * images of 3.5" disks.  Rather than add an understanding of nibblized
 * 3.5" disks to DiskImg, I've chosen to present it as a simple 800K
 * ProDOS disk image.  The only flaw in the scheme is that we have to
 * keep track of the bad blocks in a parallel data structure.
 */

/*static*/ const char* WrapperFDI::kFDIMagic = "Formatted Disk Image file\r\n";

/*
 * Test to see if this is an FDI disk image.
 */
/*static*/ DIError WrapperFDI::Test(GenericFD* pGFD, di_off_t wrappedLength)
{
    DIError dierr = kDIErrNone;
    uint8_t headerBuf[kMinHeaderLen];
    FDIHeader hdr;

    LOGI("Testing for FDI");

    pGFD->Rewind();
    dierr = pGFD->Read(headerBuf, sizeof(headerBuf));
    if (dierr != kDIErrNone)
        goto bail;

    UnpackHeader(headerBuf, &hdr);
    if (strcmp(hdr.signature, kFDIMagic) != 0) {
        LOGI("FDI: FDI signature not found");
        return kDIErrGeneric;
    }
    if (hdr.version < kMinVersion) {
        LOGI("FDI: bad version 0x%.04x", hdr.version);
        return kDIErrGeneric;
    }

bail:
    return dierr;
}

/*
 * Unpack a 512-byte buffer with the FDI header into its components.
 */
/*static*/ void WrapperFDI::UnpackHeader(const uint8_t* headerBuf, FDIHeader* pHdr)
{
    memcpy(pHdr->signature, &headerBuf[0], kSignatureLen);
    pHdr->signature[kSignatureLen] = '\0';
    memcpy(pHdr->creator, &headerBuf[27], kCreatorLen);
    pHdr->creator[kCreatorLen] = '\0';
    memcpy(pHdr->comment, &headerBuf[59], kCommentLen);
    pHdr->comment[kCommentLen] = '\0';

    pHdr->version = GetShortBE(&headerBuf[140]);
    pHdr->lastTrack = GetShortBE(&headerBuf[142]);
    pHdr->lastHead = headerBuf[144];
    pHdr->type = headerBuf[145];
    pHdr->rotSpeed = headerBuf[146];
    pHdr->flags = headerBuf[147];
    pHdr->tpi = headerBuf[148];
    pHdr->headWidth = headerBuf[149];
    pHdr->reserved = GetShortBE(&headerBuf[150]);
}

/*
 * Dump the contents of an FDI header.
 */
/*static*/ void WrapperFDI::DumpHeader(const FDIHeader* pHdr)
{
    static const char* kTypes[] = {
        "8\"", "5.25\"", "3.5\"", "3\""
    };
    static const char* kTPI[] = {
        "48", "67", "96", "100", "135", "192"
    };

    LOGI(" FDI header contents:");
    LOGI("  signature: '%s'", pHdr->signature);
    LOGI("  creator  : '%s'", pHdr->creator);
    LOGI("  comment  : '%s'", pHdr->comment);
    LOGI("  version  : %d.%d", pHdr->version >> 8, pHdr->version & 0xff);
    LOGI("  lastTrack: %d", pHdr->lastTrack);
    LOGI("  lastHead : %d", pHdr->lastHead);
    LOGI("  type     : %d (%s)", pHdr->type,
        (/*pHdr->type >= 0 &&*/ pHdr->type < NELEM(kTypes)) ?
        kTypes[pHdr->type] : "???");
    LOGI("  rotSpeed : %drpm", pHdr->rotSpeed + 128);
    LOGI("  flags    : 0x%02x", pHdr->flags);
    LOGI("  tpi      : %d (%s)", pHdr->tpi,
        (/*pHdr->tpi >= 0 &&*/ pHdr->tpi < NELEM(kTPI)) ?
        kTPI[pHdr->tpi] : "???");
    LOGI("  headWidth: %d (%s)", pHdr->headWidth,
        (/*pHdr->headWidth >= 0 &&*/ pHdr->headWidth < NELEM(kTPI)) ?
        kTPI[pHdr->headWidth] : "???");
    LOGI("  reserved : %d", pHdr->reserved);
}

/*
 * Unpack the disk to heap storage.
 */
DIError WrapperFDI::Prep(GenericFD* pGFD, di_off_t wrappedLength, bool readOnly,
    di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
    DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
    LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD)
{
    LOGI("Prepping for FDI");
    DIError dierr = kDIErrNone;
    FDIHeader hdr;

    pGFD->Rewind();
    dierr = pGFD->Read(fHeaderBuf, sizeof(fHeaderBuf));
    if (dierr != kDIErrNone)
        goto bail;

    UnpackHeader(fHeaderBuf, &hdr);
    if (strcmp(hdr.signature, kFDIMagic) != 0)
        return kDIErrGeneric;
    DumpHeader(&hdr);

    /*
     * There are two formats that we're interested in, 3.5" and 5.25".  They
     * are handled differently within CiderPress, so we split here.
     *
     * Sometimes disk2fdi finds extra tracks.  No Apple II hardware ever
     * went past 40 on 5.25" disks, but we'll humor the software and allow
     * images with up to 50.  Ditto for 3.5" disks, which should always
     * have 80 double-sided tracks.
     */
    if (hdr.type == kDiskType525) {
        LOGI("FDI: decoding 5.25\" disk");
        if (hdr.lastHead != 0 || hdr.lastTrack >= kMaxNibbleTracks525 + 10) {
            LOGI("FDI: bad params head=%d ltrack=%d",
                hdr.lastHead, hdr.lastTrack);
            dierr = kDIErrUnsupportedImageFeature;
            goto bail;
        }
        if (hdr.lastTrack >= kMaxNibbleTracks525) {
            LOGI("FDI: reducing hdr.lastTrack from %d to %d",
                hdr.lastTrack, kMaxNibbleTracks525-1);
            hdr.lastTrack = kMaxNibbleTracks525-1;
        }

        /*
         * Unpack to a series of variable-length nibble tracks.  The data
         * goes into ppNewGFD, and a table of track info goes into
         * fNibbleTrackInfo.
         */
        dierr = Unpack525(pGFD, ppNewGFD, hdr.lastTrack+1, hdr.lastHead+1);
        if (dierr != kDIErrNone)
            return dierr;

        *pLength = kMaxNibbleTracks525 * kTrackAllocSize;
        *pPhysical = DiskImg::kPhysicalFormatNib525_Var;
        *pOrder = DiskImg::kSectorOrderPhysical;
    } else if (hdr.type == kDiskType35) {
        LOGI("FDI: decoding 3.5\" disk");
        if (hdr.lastHead != 1 || hdr.lastTrack >= kMaxNibbleTracks35 + 10) {
            LOGI("FDI: bad params head=%d ltrack=%d",
                hdr.lastHead, hdr.lastTrack);
            dierr = kDIErrUnsupportedImageFeature;
            goto bail;
        }
        if (hdr.lastTrack >= kMaxNibbleTracks35) {
            LOGI("FDI: reducing hdr.lastTrack from %d to %d",
                hdr.lastTrack, kMaxNibbleTracks35-1);
            hdr.lastTrack = kMaxNibbleTracks35-1;
        }

        /*
         * Unpack to 800K of 512-byte ProDOS-order blocks, with a
         * "bad block" map.
         */
        dierr = Unpack35(pGFD, ppNewGFD, hdr.lastTrack+1, hdr.lastHead+1,
                    ppBadBlockMap);
        if (dierr != kDIErrNone)
            return dierr;

        *pLength = 800 * 1024;
        *pPhysical = DiskImg::kPhysicalFormatSectors;
        *pOrder = DiskImg::kSectorOrderProDOS;
    } else {
        LOGI("FDI: unsupported disk type");
        dierr = kDIErrUnsupportedImageFeature;
        goto bail;
    }

bail:
    return dierr;
}

/*
 * Unpack pulse timing values to nibbles.
 */
DIError WrapperFDI::Unpack525(GenericFD* pGFD, GenericFD** ppNewGFD, int numCyls,
    int numHeads)
{
    DIError dierr = kDIErrNone;
    GFDBuffer* pNewGFD = NULL;
    uint8_t* buf = NULL;
    int numTracks;

    numTracks = numCyls * numHeads;
    if (numTracks < kMaxNibbleTracks525)
        numTracks = kMaxNibbleTracks525;

    pGFD->Rewind();

    buf = new uint8_t[numTracks * kTrackAllocSize];
    if (buf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    pNewGFD = new GFDBuffer;
    if (pNewGFD == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }
    dierr = pNewGFD->Open(buf, numTracks * kTrackAllocSize,
        true, false, false);
    if (dierr != kDIErrNone)
        goto bail;
    buf = NULL;      // now owned by pNewGFD;

    dierr = UnpackDisk525(pGFD, pNewGFD, numCyls, numHeads);
    if (dierr != kDIErrNone)
        goto bail;

    *ppNewGFD = pNewGFD;
    pNewGFD = NULL;  // now owned by caller

bail:
    delete[] buf;
    delete pNewGFD;
    return dierr;
}

/*
 * Unpack pulse timing values to fully-decoded blocks.
 */
DIError WrapperFDI::Unpack35(GenericFD* pGFD, GenericFD** ppNewGFD, int numCyls,
    int numHeads, LinearBitmap** ppBadBlockMap)
{
    DIError dierr = kDIErrNone;
    GFDBuffer* pNewGFD = NULL;
    uint8_t* buf = NULL;

    pGFD->Rewind();

    buf = new uint8_t[800 * 1024];
    if (buf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    pNewGFD = new GFDBuffer;
    if (pNewGFD == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }
    dierr = pNewGFD->Open(buf, 800 * 1024, true, false, false);
    if (dierr != kDIErrNone)
        goto bail;
    buf = NULL;      // now owned by pNewGFD;

    *ppBadBlockMap = new LinearBitmap(1600);
    if (*ppBadBlockMap == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    dierr = UnpackDisk35(pGFD, pNewGFD, numCyls, numHeads, *ppBadBlockMap);
    if (dierr != kDIErrNone)
        goto bail;

    *ppNewGFD = pNewGFD;
    pNewGFD = NULL;  // now owned by caller

bail:
    delete[] buf;
    delete pNewGFD;
    return dierr;
}

/*
 * Initialize stuff for a new file.  There's no file header or other
 * goodies, so we leave "pWrapperGFD" and "pWrappedLength" alone.
 */
DIError WrapperFDI::Create(di_off_t length, DiskImg::PhysicalFormat physical,
    DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
    di_off_t* pWrappedLength, GenericFD** pDataFD)
{
    DIError dierr = kDIErrGeneric;      // not yet
#if 0
    uint8_t* buf = NULL;
    int numTracks = (int) (length / kTrackLenTrackStar525);
    int i;

    assert(length == kTrackLenTrackStar525 * kTrackCount525 ||
           length == kTrackLenTrackStar525 * kTrackStarNumTracks);
    assert(physical == DiskImg::kPhysicalFormatNib525_Var);
    assert(order == DiskImg::kSectorOrderPhysical);

    /*
     * Set up the track offset and length table.  We use the maximum
     * data length (kTrackLenTrackStar525) for each.  The nibble write
     * routine will alter the length field as appropriate.
     */
    fNibbleTrackInfo.numTracks = numTracks;
    assert(fNibbleTrackInfo.numTracks <= kMaxNibbleTracks);
    for (i = 0; i < numTracks; i++) {
        fNibbleTrackInfo.offset[i] = kTrackLenTrackStar525 * i;
        fNibbleTrackInfo.length[i] = kTrackLenTrackStar525;
    }

    /*
     * Create a blank chunk of memory for the image.
     */
    buf = new uint8_t[(int) length];
    if (buf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    GFDBuffer* pNewGFD;
    pNewGFD = new GFDBuffer;
    dierr = pNewGFD->Open(buf, length, true, false, false);
    if (dierr != kDIErrNone) {
        delete pNewGFD;
        goto bail;
    }
    *pDataFD = pNewGFD;
    buf = NULL;          // now owned by pNewGFD;

    // can't set *pWrappedLength yet

bail:
    delete[] buf;
#endif
    return dierr;
}

/*
 * Write the stored data into FDI format.
 *
 * The source data is in "pDataGFD" in a layout described by fNibbleTrackInfo.
 * We need to create the new file in "pWrapperGFD".
 */
DIError WrapperFDI::Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
    di_off_t dataLen, di_off_t* pWrappedLen)
{
    DIError dierr = kDIErrGeneric;      // not yet

#if 0
    assert(dataLen == kTrackLenTrackStar525 * kTrackCount525 ||
           dataLen == kTrackLenTrackStar525 * kTrackStarNumTracks);
    assert(kTrackLenTrackStar525 <= kMaxTrackLen);

    pDataGFD->Rewind();

    uint8_t writeBuf[kFileTrackStorageLen];
    uint8_t dataBuf[kTrackLenTrackStar525];
    int track, trackLen;

    for (track = 0; track < kTrackStarNumTracks; track++) {
        if (track < fNibbleTrackInfo.numTracks) {
            dierr = pDataGFD->Read(dataBuf, kTrackLenTrackStar525);
            if (dierr != kDIErrNone)
                goto bail;
            trackLen = fNibbleTrackInfo.length[track];
            assert(fNibbleTrackInfo.offset[track] == kTrackLenTrackStar525 * track);
        } else {
            LOGI("   TrackStar faking track %d", track);
            memset(dataBuf, 0xff, sizeof(dataBuf));
            trackLen = kMaxTrackLen;
        }

        memset(writeBuf, 0x80, sizeof(writeBuf));   // not strictly necessary
        memset(writeBuf, 0x20, kCommentFieldLen);
        memset(writeBuf+kCommentFieldLen, 0x00, 0x81-kCommentFieldLen);

        const char* comment;
        if (fStorageName != NULL && *fStorageName != '\0')
            comment = fStorageName;
        else
            comment = "(created by CiderPress)";
        if (strlen(comment) > kCommentFieldLen)
            memcpy(writeBuf, comment, kCommentFieldLen);
        else
            memcpy(writeBuf, comment, strlen(comment));

        int i;
        for (i = 0; i < trackLen; i++)
            writeBuf[0x81+i] = dataBuf[trackLen - i -1];

        if (trackLen == kMaxTrackLen)
            PutShortLE(writeBuf + 0x19fe, 0);
        else
            PutShortLE(writeBuf + 0x19fe, (uint16_t) trackLen);

        dierr = pWrapperGFD->Write(writeBuf, sizeof(writeBuf));
        if (dierr != kDIErrNone)
            goto bail;
    }

    *pWrappedLen = pWrapperGFD->Tell();
    assert(*pWrappedLen == kFileTrackStorageLen * kTrackStarNumTracks);

bail:
#endif
    return dierr;
}

void WrapperFDI::SetNibbleTrackLength(int track, int length)
{
    assert(false);      // not yet
#if 0
    assert(track >= 0);
    assert(length > 0 && length <= kMaxTrackLen);
    assert(track < fNibbleTrackInfo.numTracks);

    LOGI("  FDI: set length of track %d to %d", track, length);
    fNibbleTrackInfo.length[track] = length;
#endif
}


/*
 * ===========================================================================
 *      Unadorned nibble format
 * ===========================================================================
 */

/*
 * See if this is unadorned nibble format.
 */
/*static*/ DIError WrapperUnadornedNibble::Test(GenericFD* pGFD,
    di_off_t wrappedLength)
{
    LOGI("Testing for unadorned nibble");

    /* test length */
    if (wrappedLength != kTrackCount525 * kTrackLenNib525 &&
        wrappedLength != kTrackCount525 * kTrackLenNb2525)
    {
        return kDIErrGeneric;
    }

    /* quick scan for invalid data */
    const int kScanSize = 512;
    uint8_t buf[kScanSize];

    pGFD->Rewind();
    if (pGFD->Read(buf, sizeof(buf)) != kDIErrNone)
        return kDIErrGeneric;

    /*
     * Make sure this is a nibble image and not just a ProDOS volume that
     * happened to get the right number of blocks.  The primary test is
     * for < 0x80 since there's no way that can be valid, even on a track
     * full of junk.
     */
    for (int i = 0; i < kScanSize; i++) {
        if (buf[i] < 0x80) {
            LOGD("  Disqualifying len=%ld from nibble, byte=0x%02x",
                (long) wrappedLength, buf[i]);
            return kDIErrGeneric;
        } else if (buf[i] < 0x96) {
            LOGD("  Warning: funky byte 0x%02x in file", buf[i]);
        }
    }

    return kDIErrNone;
}

/*
 * Prepare unadorned nibble for use.  Not much to do here.
 */
DIError WrapperUnadornedNibble::Prep(GenericFD* pGFD, di_off_t wrappedLength,
    bool readOnly, di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
    DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
    LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD)
{
    LOGI("Prep for unadorned nibble");

    if (wrappedLength == kTrackCount525 * kTrackLenNib525) {
        LOGI("  Prepping for 6656-byte NIB");
        *pPhysical = DiskImg::kPhysicalFormatNib525_6656;
    } else if (wrappedLength == kTrackCount525 * kTrackLenNb2525) {
        LOGI("  Prepping for 6384-byte NB2");
        *pPhysical = DiskImg::kPhysicalFormatNib525_6384;
    } else {
        LOGI("  Unexpected wrappedLength %ld for unadorned nibble",
            (long) wrappedLength);
        assert(false);
    }

    *pLength = wrappedLength;
    *pOrder = DiskImg::kSectorOrderPhysical;

    *ppNewGFD = new GFDGFD;
    return ((GFDGFD*)*ppNewGFD)->Open(pGFD, 0, readOnly);
}

/*
 * Initialize fields for a new file.
 */
DIError WrapperUnadornedNibble::Create(di_off_t length,
    DiskImg::PhysicalFormat physical, DiskImg::SectorOrder order,
    short dosVolumeNum, GenericFD* pWrapperGFD,
    di_off_t* pWrappedLength, GenericFD** pDataFD)
{
    LOGI("Create unadorned nibble");

    *pWrappedLength = length;
    *pDataFD = new GFDGFD;
    return ((GFDGFD*)*pDataFD)->Open(pWrapperGFD, 0, false);
}

/*
 * We only use GFDGFD, so there's nothing to do here.
 */
DIError WrapperUnadornedNibble::Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
    di_off_t dataLen, di_off_t* pWrappedLen)
{
    return kDIErrNone;
}


/*
 * ===========================================================================
 *      Unadorned sectors
 * ===========================================================================
 */

/*
 * See if this is unadorned sector format.  The only way we can really tell
 * is by looking at the file size.
 *
 * The only requirement is that it be a multiple of 512 bytes.  This holds
 * for all ProDOS volumes and all floppy disk images.  We also need to test
 * for 13-sector ".d13" images.
 *
 * It also holds for 35-track 6656-byte unadorned nibble images, so we need
 * to test for them first.
 */
/*static*/ DIError WrapperUnadornedSector::Test(GenericFD* pGFD, di_off_t wrappedLength)
{
    LOGI("Testing for unadorned sector (wrappedLength=%ld/%u)",
        (long) (wrappedLength >> 32), (uint32_t) wrappedLength);
    if (wrappedLength >= 1536 && (wrappedLength % 512) == 0)
        return kDIErrNone;
    if (wrappedLength == kD13Length)        // 13-sector image?
        return kDIErrNone;

    return kDIErrGeneric;
}

/*
 * Prepare unadorned sector for use.  Not much to do here.
 */
DIError WrapperUnadornedSector::Prep(GenericFD* pGFD, di_off_t wrappedLength,
    bool readOnly, di_off_t* pLength, DiskImg::PhysicalFormat* pPhysical,
    DiskImg::SectorOrder* pOrder, short* pDiskVolNum,
    LinearBitmap** ppBadBlockMap, GenericFD** ppNewGFD)
{
    LOGI("Prepping for unadorned sector");
    assert(wrappedLength > 0);
    *pLength = wrappedLength;
    *pPhysical = DiskImg::kPhysicalFormatSectors;
    //*pOrder = undetermined

    *ppNewGFD = new GFDGFD;
    return ((GFDGFD*)*ppNewGFD)->Open(pGFD, 0, readOnly);
}

/*
 * Initialize fields for a new file.
 */
DIError WrapperUnadornedSector::Create(di_off_t length, DiskImg::PhysicalFormat physical,
    DiskImg::SectorOrder order, short dosVolumeNum, GenericFD* pWrapperGFD,
    di_off_t* pWrappedLength, GenericFD** pDataFD)
{
    LOGI("Create unadorned sector");

    *pWrappedLength = length;
    *pDataFD = new GFDGFD;
    return ((GFDGFD*)*pDataFD)->Open(pWrapperGFD, 0, false);
}

/*
 * We only use GFDGFD, so there's nothing to do here.
 */
DIError WrapperUnadornedSector::Flush(GenericFD* pWrapperGFD, GenericFD* pDataGFD,
    di_off_t dataLen, di_off_t* pWrappedLen)
{
    return kDIErrNone;
}
