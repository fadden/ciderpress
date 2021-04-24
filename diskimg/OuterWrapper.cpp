/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Code for handling "outer wrappers" like ZIP and gzip.
 *
 * TODO: for safety, these should compress into a temp file and then rename
 * the temp file over the original.  The current implementation just
 * truncates the open file descriptor or reopens the original file.  Both
 * risk data loss if the program or system crashes while the data is being
 * written to disk.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"
#define DEF_MEM_LEVEL 8     // normally in zutil.h


/*
 * ===========================================================================
 *      OuterGzip
 * ===========================================================================
 */

/*
 * Test to see if this is a gzip file.
 *
 * This test is pretty weak, so we shouldn't even be looking at this
 * unless the file ends in ".gz".  A better test would scan the entire
 * header.
 *
 * Would be nice to just gzopen the file, but unfortunately that tries
 * to be "helpful" and reads the file whether it's in gz format or not.
 * Some days I could do with a little less "help".
 */
/*static*/ DIError OuterGzip::Test(GenericFD* pGFD, di_off_t outerLength)
{
    const int kGzipMagic = 0x8b1f;  // 0x1f 0x8b
    uint16_t magic, magicBuf;
    const char* imagePath;

    LOGI("Testing for gzip");

    /* don't need this here, but we will later on */
    imagePath = pGFD->GetPathName();
    if (imagePath == NULL) {
        LOGI("Can't test gzip on non-file");
        return kDIErrNotSupported;
    }

    pGFD->Rewind();

    if (pGFD->Read(&magicBuf, 2) != kDIErrNone)
        return kDIErrGeneric;
    magic = GetShortLE((uint8_t*) &magicBuf);

    if (magic == kGzipMagic)
        return kDIErrNone;
    else
        return kDIErrGeneric;
}

/*
 * The gzip file format has a length embedded in the footer, but
 * unfortunately there is no interface to access it.  So, we have
 * to keep reading until we run out of data, extending the buffer
 * to accommodate the new data each time.  (We could also just read
 * the footer directly, but that requires that there are no garbage
 * bytes at the end of the file, which is a real concern on some FTP sites.)
 *
 * Start out by trying sizes that we think will work (140K, 800K),
 * then grow quickly.
 *
 * The largest possible ProDOS image is 32MB, but it's possible to
 * have an HFS volume or a partitioned image larger than that.  We currently
 * cap the limit to avoid nasty behavior when encountering really
 * large .gz files.  This isn't great -- we ought to support extracting
 * to a temp file, or allowing the caller to specify what the largest
 * size they can handle is.
 */
DIError OuterGzip::ExtractGzipImage(gzFile gzfp, char** pBuf, di_off_t* pLength)
{
    DIError dierr = kDIErrNone;
    const int kMinEmpty = 256 * 1024;
    const int kStartSize = 141 * 1024;
    const int kNextSize1 = 801 * 1024;
    const int kNextSize2 = 1024 * 1024;
    const int kMaxIncr = 4096 * 1024;
    const int kAbsoluteMax = kMaxUncompressedSize;
    char* buf = NULL;
    char* newBuf = NULL;
    long curSize, maxSize;

    assert(gzfp != NULL);
    assert(pBuf != NULL);
    assert(pLength != NULL);

    curSize = 0;
    maxSize = kStartSize;

    buf = new char[maxSize];
    if (buf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    while (1) {
        long len;

        /*
         * Try to fill the buffer.
         *
         * It appears that zlib v1.1.4 was more tolerant of certain kinds
         * of broken archives than v1.2.1.  Both give you a pile of data
         * on the first read, with no error reported, but the next read
         * attempt returns with z_err=-3 (Z_DATA_ERROR) and z_eof set.  I'm
         * not sure exactly what the flaw is, but I'm guessing something
         * got lopped off the end of the archives.  gzip v1.3.3 won't touch
         * them either.
         *
         * It would be easy enough to access them, if they were accessible.
         * Unfortunately the implementation is buried.  Instead, we do
         * a quick test against known unadorned floppy disk sizes to see
         * if we can salvage the contents.  (Our read attempts are all
         * slightly *over* the standard disk sizes, so if it comes back right
         * on one we're *probably* okay.)
         */
        len = gzread(gzfp, buf + curSize, maxSize - curSize);
        if (len < 0) {
            LOGI("  ExGZ Call to gzread failed, errno=%d", errno);
            if (curSize == 140*1024 || curSize == 800*1024) {
                LOGI("WARNING: accepting damaged gzip file");
                fWrapperDamaged = true;
                break;      // sleazy, but currently necessary
            }
            dierr = kDIErrReadFailed;
            goto bail;
        } else if (len == 0) {
            /* EOF reached */
            break;
        } else if (len < (maxSize - curSize)) {
            /* we've probably reached the end, but we can't be sure,
               so let's go around again */
            LOGI("  ExGZ gzread(%ld) returned %ld, letting it ride",
                maxSize - curSize, len);
            curSize += len;
        } else {
            /* update buffer, and grow it if it's not big enough */
            curSize += len;
            LOGI("  max=%ld cur=%ld", maxSize, curSize);
            if (maxSize - curSize < kMinEmpty) {
                /* not enough room, grow it */

                if (maxSize == kStartSize)
                    maxSize = kNextSize1;
                else if (maxSize == kNextSize1)
                    maxSize = kNextSize2;
                else {
                    if (maxSize < kMaxIncr)
                        maxSize = maxSize * 2;
                    else
                        maxSize += kMaxIncr;
                }

                newBuf = new char[maxSize];
                if (newBuf == NULL) {
                    LOGI("  ExGZ failed buffer alloc (%ld)",
                        maxSize);
                    dierr = kDIErrMalloc;
                    goto bail;
                }

                memcpy(newBuf, buf, curSize);
                delete[] buf;
                buf = newBuf;
                newBuf = NULL;

                LOGI("  ExGZ grew buffer to %ld", maxSize);
            } else {
                /* don't need to grow buffer yet */
                LOGI("  ExGZ read %ld bytes, cur=%ld max=%ld",
                    len, curSize, maxSize);
            }
        }
        assert(curSize < maxSize);

        if (curSize > kAbsoluteMax) {
            LOGI("  ExGZ excessive size, probably not a disk image");
            dierr = kDIErrTooBig;   // close enough
            goto bail;
        }
    }

    if (curSize + (1024*1024) < maxSize) {
        /* shrink it down so it fits */
        LOGI("  Down-sizing buffer from %ld to %ld", maxSize, curSize);
        newBuf = new char[curSize];
        if (newBuf == NULL)
            goto bail;
        memcpy(newBuf, buf, curSize);
        delete[] buf;
        buf = newBuf;
        newBuf = NULL;
    }

    *pBuf = buf;
    *pLength = curSize;
    LOGI("  ExGZ final size = %ld", curSize);

    buf = NULL;

bail:
    delete[] buf;
    delete[] newBuf;
    return dierr;
}

/*
 * Open the archive, and extract the disk image into a memory buffer.
 */
DIError OuterGzip::Load(GenericFD* pOuterGFD, di_off_t outerLength, bool readOnly,
    di_off_t* pWrapperLength, GenericFD** ppWrapperGFD)
{
    DIError dierr = kDIErrNone;
    GFDBuffer* pNewGFD = NULL;
    char* buf = NULL;
    di_off_t length = -1;
    const char* imagePath;
    gzFile gzfp = NULL;

    imagePath = pOuterGFD->GetPathName();
    if (imagePath == NULL) {
        assert(false);      // should've been caught in Test
        return kDIErrNotSupported;
    }

    gzfp = gzopen(imagePath, "rb");        // use "readOnly" here
    if (gzfp == NULL) { // DON'T retry RO -- should be done at higher level?
        LOGI("gzopen failed, errno=%d", errno);
        dierr = kDIErrGeneric;
        goto bail;
    }

    dierr = ExtractGzipImage(gzfp, &buf, &length);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Everything is going well.  Now we substitute a memory-based GenericFD
     * for the existing GenericFD.
     */
    pNewGFD = new GFDBuffer;
    dierr = pNewGFD->Open(buf, length, true, false, readOnly);
    if (dierr != kDIErrNone)
        goto bail;
    buf = NULL;      // now owned by pNewGFD;

    /*
     * Success!
     */
    assert(dierr == kDIErrNone);
    *ppWrapperGFD = pNewGFD;
    pNewGFD = NULL;

    *pWrapperLength = length;

bail:
    if (dierr != kDIErrNone) {
        delete pNewGFD;
    }
    if (gzfp != NULL)
        gzclose(gzfp);
    return dierr;
}

/*
 * Save the contents of "pWrapperGFD" to the file pointed to by
 * "pOuterGFD".
 *
 * "pOuterGFD" isn't disturbed (same as Load).  All we want is to get the
 * filename and then do everything through gzio.
 */
DIError OuterGzip::Save(GenericFD* pOuterGFD, GenericFD* pWrapperGFD,
    di_off_t wrapperLength)
{
    DIError dierr = kDIErrNone;
    const char* imagePath;
    gzFile gzfp = NULL;

    LOGI(" GZ save (wrapperLen=%ld)", (long) wrapperLength);
    assert(wrapperLength > 0);

    /*
     * Reopen the file.
     */
    imagePath = pOuterGFD->GetPathName();
    if (imagePath == NULL) {
        assert(false);      // should've been caught long ago
        return kDIErrNotSupported;
    }

    gzfp = gzopen(imagePath, "wb");
    if (gzfp == NULL) {
        LOGI("gzopen for write failed, errno=%d", errno);
        dierr = kDIErrGeneric;
        goto bail;
    }

    char buf[16384];
    size_t actual;
    long written, totalWritten;

    pWrapperGFD->Rewind();

    totalWritten = 0;
    while (wrapperLength > 0) {
        dierr = pWrapperGFD->Read(buf, sizeof(buf), &actual);
        if (dierr == kDIErrEOF) {
            dierr = kDIErrNone;
            break;
        }
        if (dierr != kDIErrNone) {
            LOGI("Error reading source GFD during gzip save (err=%d)",dierr);
            goto bail;
        }
        assert(actual > 0);

        written = gzwrite(gzfp, buf, actual);
        if (written == 0) {
            LOGE("Failed writing %lu bytes to gzio", (unsigned long) actual);
            dierr = kDIErrGeneric;
            goto bail;
        }

        totalWritten += written;
        wrapperLength -= actual;
    }
    assert(wrapperLength == 0);     // not expecting any slop

    LOGD(" GZ wrote %ld bytes", totalWritten);

    /*
     * Success!
     */
    assert(dierr == kDIErrNone);

bail:
    if (gzfp != NULL)
        gzclose(gzfp);
    return dierr;
}


/*
 * ===========================================================================
 *      OuterZip
 * ===========================================================================
 */

/*
 * Test to see if this is a ZIP archive.
 */
/*static*/ DIError OuterZip::Test(GenericFD* pGFD, di_off_t outerLength)
{
    DIError dierr = kDIErrNone;
    CentralDirEntry cde;

    LOGI("Testing for zip");
    dierr = ReadCentralDir(pGFD, outerLength, &cde);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Make sure it's a compression method we support.
     */
    if (cde.fCompressionMethod != kCompressStored &&
        cde.fCompressionMethod != kCompressDeflated)
    {
        LOGI(" ZIP compression method %d not supported",
            cde.fCompressionMethod);
        dierr = kDIErrGeneric;
        goto bail;
    }

    /*
     * Limit the size to something reasonable.
     */
    if (cde.fUncompressedSize < 512 ||
        cde.fUncompressedSize > kMaxUncompressedSize)
    {
        LOGI(" ZIP uncompressed size %u is outside range",
            cde.fUncompressedSize);
        dierr = kDIErrGeneric;
        goto bail;
    }

    assert(dierr == kDIErrNone);

bail:
    return dierr;
}

/*
 * Open the archive, and extract the disk image into a memory buffer.
 */
DIError OuterZip::Load(GenericFD* pOuterGFD, di_off_t outerLength, bool readOnly,
    di_off_t* pWrapperLength, GenericFD** ppWrapperGFD)
{
    DIError dierr = kDIErrNone;
    GFDBuffer* pNewGFD = NULL;
    CentralDirEntry cde;
    uint8_t* buf = NULL;
    di_off_t length = -1;
    const char* pExt;

    dierr = ReadCentralDir(pOuterGFD, outerLength, &cde);
    if (dierr != kDIErrNone)
        goto bail;

    if (cde.fFileNameLength > 0) {
        pExt = FindExtension((const char*) cde.fFileName, kZipFssep);
        if (pExt != NULL) {
            assert(*pExt == '.');
            SetExtension(pExt+1);

            LOGI("OuterZip using extension '%s'", GetExtension());
        }

        SetStoredFileName((const char*) cde.fFileName);
    }

    dierr = ExtractZipEntry(pOuterGFD, &cde, &buf, &length);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Everything is going well.  Now we substitute a memory-based GenericFD
     * for the existing GenericFD.
     */
    pNewGFD = new GFDBuffer;
    dierr = pNewGFD->Open(buf, length, true, false, readOnly);
    if (dierr != kDIErrNone)
        goto bail;
    buf = NULL;      // now owned by pNewGFD;

    /*
     * Success!
     */
    assert(dierr == kDIErrNone);
    *ppWrapperGFD = pNewGFD;
    pNewGFD = NULL;

    *pWrapperLength = length;

bail:
    if (dierr != kDIErrNone) {
        delete pNewGFD;
    }
    return dierr;
}

/*
 * Save the contents of "pWrapperGFD" to the file pointed to by
 * "pOuterGFD".
 */
DIError OuterZip::Save(GenericFD* pOuterGFD, GenericFD* pWrapperGFD,
    di_off_t wrapperLength)
{
    DIError dierr = kDIErrNone;
    LocalFileHeader lfh;
    CentralDirEntry cde;
    EndOfCentralDir eocd;
    di_off_t lfhOffset;

    LOGI(" ZIP save (wrapperLen=%ld)", (long) wrapperLength);
    assert(wrapperLength > 0);

    dierr = pOuterGFD->Rewind();
    if (dierr != kDIErrNone)
        goto bail;
    dierr = pOuterGFD->Truncate();
    if (dierr != kDIErrNone)
        goto bail;

    dierr = pWrapperGFD->Rewind();
    if (dierr != kDIErrNone)
        goto bail;

    lfhOffset = pOuterGFD->Tell();      // always 0 with only one file

    /*
     * Don't store an empty filename.  Some applications, e.g. Info-ZIP's
     * "unzip", get confused.  Ideally the DiskImg image creation code
     * will have set the actual filename, with an extension that matches
     * the file contents.
     */
    if (fStoredFileName == NULL || fStoredFileName[0] == '\0')
        SetStoredFileName("disk");

    /*
     * Write the ZIP local file header.  We don't have file lengths or
     * CRCs yet, so we have to go back and fill those in later.
     */
    lfh.fVersionToExtract = kDefaultVersion;
#if NO_ZIP_COMPRESS
    lfh.fGPBitFlag = 0;
    lfh.fCompressionMethod = 0;
#else
    lfh.fGPBitFlag = 0x0002;        // indicates maximum compression used
    lfh.fCompressionMethod = 8;     //  when compressionMethod == deflate
#endif
    GetMSDOSTime(&lfh.fLastModFileDate, &lfh.fLastModFileTime);
    lfh.SetFileName(fStoredFileName);
    dierr = lfh.Write(pOuterGFD);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Write the compressed data.
     */
    uint32_t crc;
    di_off_t compressedLen;
    if (lfh.fCompressionMethod == kCompressDeflated) {
        dierr = DeflateGFDToGFD(pOuterGFD, pWrapperGFD, wrapperLength,
                    &compressedLen, &crc);
        if (dierr != kDIErrNone)
            goto bail;
    } else if (lfh.fCompressionMethod == kCompressStored) {
        dierr = GenericFD::CopyFile(pOuterGFD, pWrapperGFD, wrapperLength,
                    &crc);
        if (dierr != kDIErrNone)
            goto bail;
        compressedLen = wrapperLength;
    } else {
        assert(false);
        dierr = kDIErrInternal;
        goto bail;
    }

    /*
     * Go back and take care of the local file header stuff.
     *
     * It's not supposed to be necessary, but some utilities (WinZip,
     * Info-ZIP) get bent out of shape if these aren't set and the data
     * is compressed.  They seem okay with it when the file isn't
     * compressed.  I don't understand this behavior, but writing the
     * local file header is easy enough.
     */
    lfh.fCRC32 = crc;
    lfh.fCompressedSize = (uint32_t) compressedLen;
    lfh.fUncompressedSize = (uint32_t) wrapperLength;

    di_off_t curPos;
    curPos = pOuterGFD->Tell();
    dierr = pOuterGFD->Seek(lfhOffset, kSeekSet);
    if (dierr != kDIErrNone)
        goto bail;
    dierr = lfh.Write(pOuterGFD);
    if (dierr != kDIErrNone)
        goto bail;
    dierr = pOuterGFD->Seek(curPos, kSeekSet);
    if (dierr != kDIErrNone)
        goto bail;

    di_off_t cdeStart, cdeFinish;
    cdeStart = pOuterGFD->Tell();

    /*
     * Write the central dir entry.  This is largely just a copy of the
     * data in the local file header (and in fact some utilities will
     * get rather bent out of shape if the two don't match exactly).
     */
    cde.fVersionMadeBy = kDefaultVersion;
    cde.fVersionToExtract = lfh.fVersionToExtract;
    cde.fGPBitFlag = lfh.fGPBitFlag;
    cde.fCompressionMethod = lfh.fCompressionMethod;
    cde.fLastModFileDate = lfh.fLastModFileDate;
    cde.fLastModFileTime = lfh.fLastModFileTime;
    cde.fCRC32 = lfh.fCRC32;
    cde.fCompressedSize = lfh.fCompressedSize;
    cde.fUncompressedSize = lfh.fUncompressedSize;
    assert(lfh.fExtraFieldLength == 0 && cde.fExtraFieldLength == 0);
    cde.fExternalAttrs = 0x81b60020;    // matches what WinZip does
    cde.fLocalHeaderRelOffset = (uint32_t) lfhOffset;
    cde.SetFileName(fStoredFileName);
    dierr = cde.Write(pOuterGFD);
    if (dierr != kDIErrNone)
        goto bail;

    cdeFinish = pOuterGFD->Tell();

    /*
     * Write the end-of-central-dir stuff.
     */
    eocd.fNumEntries = 1;
    eocd.fTotalNumEntries = 1;
    eocd.fCentralDirSize = (uint32_t) (cdeFinish - cdeStart);
    eocd.fCentralDirOffset = (uint32_t) cdeStart;
    assert(eocd.fCentralDirSize >= EndOfCentralDir::kEOCDLen);
    dierr = eocd.Write(pOuterGFD);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Success!
     */
    assert(dierr == kDIErrNone);

bail:
    return dierr;
}

/*
 * Track the name of the file stored in the ZIP archive.
 */
void OuterZip::SetStoredFileName(const char* name)
{
    delete[] fStoredFileName;
    fStoredFileName = StrcpyNew(name);
}

/*
 * Find the central directory and read the contents.
 *
 * We currently only support archives with a single entry.
 *
 * The fun thing about ZIP archives is that they may or may not be
 * readable from start to end.  In some cases, notably for archives
 * that were written to stdout, the only length information is in the
 * central directory at the end of the file.
 *
 * Of course, the central directory can be followed by a variable-length
 * comment field, so we have to scan through it backwards.  The comment
 * is at most 64K, plus we have 18 bytes for the end-of-central-dir stuff
 * itself, plus apparently sometimes people throw random junk on the end
 * just for the fun of it.
 *
 * This is all a little wobbly.  If the wrong value ends up in the EOCD
 * area, we're hosed.  This appears to be the way that the Info-ZIP guys
 * do it though, so we're in pretty good company if this fails.
 */
/*static*/ DIError OuterZip::ReadCentralDir(GenericFD* pGFD, di_off_t outerLength,
    CentralDirEntry* pDirEntry)
{
    DIError dierr = kDIErrNone;
    EndOfCentralDir eocd;
    uint8_t* buf = NULL;
    di_off_t seekStart;
    long readAmount;
    int i;

    /* too small to be a ZIP archive? */
    if (outerLength < EndOfCentralDir::kEOCDLen + 4)
        return kDIErrGeneric;

    buf = new uint8_t[kMaxEOCDSearch];
    if (buf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    if (outerLength > kMaxEOCDSearch) {
        seekStart = outerLength - kMaxEOCDSearch;
        readAmount = kMaxEOCDSearch;
    } else {
        seekStart = 0;
        readAmount = (long) outerLength;
    }
    dierr = pGFD->Seek(seekStart, kSeekSet);
    if (dierr != kDIErrNone)
        goto bail;

    /* read the last part of the file into the buffer */
    dierr = pGFD->Read(buf, readAmount);
    if (dierr != kDIErrNone)
        goto bail;

    /* find the end-of-central-dir magic */
    for (i = readAmount - 4; i >= 0; i--) {
        if (buf[i] == 0x50 &&
            GetLongLE(&buf[i]) == EndOfCentralDir::kSignature)
        {
            LOGI("+++ Found EOCD at buf+%d", i);
            break;
        }
    }
    if (i < 0) {
        LOGI("+++ EOCD not found, not ZIP");
        dierr = kDIErrGeneric;
        goto bail;
    }

    /* extract eocd values */
    dierr = eocd.ReadBuf(buf + i, readAmount - i);
    if (dierr != kDIErrNone)
        goto bail;
    eocd.Dump();

    if (eocd.fDiskNumber != 0 || eocd.fDiskWithCentralDir != 0 ||
        eocd.fNumEntries != 1 || eocd.fTotalNumEntries != 1)
    {
        LOGI(" Probable ZIP archive has more than one member");
        dierr = kDIErrFileArchive;
        goto bail;
    }

    /*
     * So far so good.  "fCentralDirSize" is the size in bytes of the
     * central directory, so we can just seek back that far to find it.
     * We can also seek forward fCentralDirOffset bytes from the
     * start of the file.
     *
     * We're not guaranteed to have the rest of the central dir in the
     * buffer, nor are we guaranteed that the central dir will have any
     * sort of convenient size.  We need to skip to the start of it and
     * read the header, then the other goodies.
     *
     * The only thing we really need right now is the file comment, which
     * we're hoping to preserve.
     */
    dierr = pGFD->Seek(eocd.fCentralDirOffset, kSeekSet);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Read the central dir entry.
     */
    dierr = pDirEntry->Read(pGFD);
    if (dierr != kDIErrNone)
        goto bail;

    pDirEntry->Dump();

    {
        uint8_t checkBuf[4];
        dierr = pGFD->Read(checkBuf, 4);
        if (dierr != kDIErrNone)
            goto bail;
        if (GetLongLE(checkBuf) != EndOfCentralDir::kSignature) {
            LOGI("CDE read check failed");
            assert(false);
            dierr = kDIErrGeneric;
            goto bail;
        }
        LOGI("+++ CDE read check passed");
    }

bail:
    delete[] buf;
    return dierr;
}

/*
 * The central directory tells us where to find the local header.  We
 * have to skip over that to get to the start of the data.
 */
DIError OuterZip::ExtractZipEntry(GenericFD* pOuterGFD, CentralDirEntry* pCDE,
    uint8_t** pBuf, di_off_t* pLength)
{
    DIError dierr = kDIErrNone;
    LocalFileHeader lfh;
    uint8_t* buf = NULL;

    /* seek to the start of the local header */
    dierr = pOuterGFD->Seek(pCDE->fLocalHeaderRelOffset, kSeekSet);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Read the local file header, mainly as a way to get past it.  There
     * are legitimate reasons why the size fields and filename might be
     * empty, so we really don't want to depend on any data in the LFH.
     * We just need to find where the data starts.
     */
    dierr = lfh.Read(pOuterGFD);
    if (dierr != kDIErrNone)
        goto bail;
    lfh.Dump();

    /* we should now be pointing at the data */
    LOGI("File offset is 0x%08lx", (long) pOuterGFD->Tell());

    buf = new uint8_t[pCDE->fUncompressedSize];
    if (buf == NULL) {
        /* a very real possibility */
        LOGI(" ZIP unable to allocate buffer of %u bytes",
            pCDE->fUncompressedSize);
        dierr = kDIErrMalloc;
        goto bail;
    }

    /* unpack or copy the data */
    if (pCDE->fCompressionMethod == kCompressDeflated) {
        dierr = InflateGFDToBuffer(pOuterGFD, pCDE->fCompressedSize,
                    pCDE->fUncompressedSize, buf);
        if (dierr != kDIErrNone)
            goto bail;
    } else if (pCDE->fCompressionMethod == kCompressStored) {
        dierr = pOuterGFD->Read(buf, pCDE->fUncompressedSize);
        if (dierr != kDIErrNone)
            goto bail;
    } else {
        assert(false);
        dierr = kDIErrInternal;
        goto bail;
    }

    /* check the CRC32 */
    uint32_t crc;
    crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, buf, pCDE->fUncompressedSize);

    if (crc == pCDE->fCRC32) {
        LOGI("+++ ZIP CRCs match");
    } else {
        LOGI("ZIP CRC mismatch: inflated crc32=0x%08x, stored=0x%08x",
            crc, pCDE->fCRC32);
        dierr = kDIErrBadChecksum;
        goto bail;
    }

    *pBuf = buf;
    *pLength = pCDE->fUncompressedSize;

    buf = NULL;

bail:
    delete[] buf;
    return dierr;
}

/*
 * Uncompress data from "pOuterGFD" to "buf".
 *
 * "buf" must be able to hold "uncompSize" bytes.
 */
DIError OuterZip::InflateGFDToBuffer(GenericFD* pGFD, unsigned long compSize,
    unsigned long uncompSize, uint8_t* buf)
{
    DIError dierr = kDIErrNone;
    const size_t kReadBufSize = 65536;
    uint8_t* readBuf = NULL;
    z_stream zstream;
    int zerr;
    unsigned long compRemaining;

    readBuf = new uint8_t[kReadBufSize];
    if (readBuf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }
    compRemaining = compSize;

    /*
     * Initialize the zlib stream.
     */
    memset(&zstream, 0, sizeof(zstream));
    zstream.zalloc = Z_NULL;
    zstream.zfree = Z_NULL;
    zstream.opaque = Z_NULL;
    zstream.next_in = NULL;
    zstream.avail_in = 0;
    zstream.next_out = buf;
    zstream.avail_out = uncompSize;
    zstream.data_type = Z_UNKNOWN;

    /*
     * Use the undocumented "negative window bits" feature to tell zlib
     * that there's no zlib header waiting for it.
     */
    zerr = inflateInit2(&zstream, -MAX_WBITS);
    if (zerr != Z_OK) {
        dierr = kDIErrInternal;
        if (zerr == Z_VERSION_ERROR) {
            LOGI("Installed zlib is not compatible with linked version (%s)",
                ZLIB_VERSION);
        } else {
            LOGI("Call to inflateInit2 failed (zerr=%d)", zerr);
        }
        goto bail;
    }

    /*
     * Loop while we have data.
     */
    do {
        unsigned long getSize;

        /* read as much as we can */
        if (zstream.avail_in == 0) {
            getSize = (compRemaining > kReadBufSize) ?
                        kReadBufSize : compRemaining;
            LOGI("+++ reading %ld bytes (%ld left)", getSize,
                compRemaining);

            dierr = pGFD->Read(readBuf, getSize);
            if (dierr != kDIErrNone) {
                LOGI("inflate read failed");
                goto z_bail;
            }

            compRemaining -= getSize;

            zstream.next_in = readBuf;
            zstream.avail_in = getSize;
        }

        /* uncompress the data */
        zerr = inflate(&zstream, Z_NO_FLUSH);
        if (zerr != Z_OK && zerr != Z_STREAM_END) {
            dierr = kDIErrInternal;
            LOGI("zlib inflate call failed (zerr=%d)", zerr);
            goto z_bail;
        }

        /* output buffer holds all, so no need to write the output */
    } while (zerr == Z_OK);

    assert(zerr == Z_STREAM_END);       /* other errors should've been caught */

    if (zstream.total_out != uncompSize) {
        dierr = kDIErrBadCompressedData;
        LOGI("Size mismatch on inflated file (%ld vs %ld)",
            zstream.total_out, uncompSize);
        goto z_bail;
    }

z_bail:
    inflateEnd(&zstream);        /* free up any allocated structures */

bail:
    delete[] readBuf;
    return dierr;
}

/*
 * Get the current date/time, in MS-DOS format.
 */
void OuterZip::GetMSDOSTime(uint16_t* pDate, uint16_t* pTime)
{
#if 0
    /* this gets gmtime; we want localtime */
    SYSTEMTIME sysTime;
    FILETIME fileTime;
    ::GetSystemTime(&sysTime);
    ::SystemTimeToFileTime(&sysTime, &fileTime);
    ::FileTimeToDosDateTime(&fileTime, pDate, pTime);
    //LOGI("+++ Windows date: %04x %04x %d", *pDate, *pTime,
    //  (*pTime >> 11) & 0x1f);
#endif

    time_t now = time(NULL);
    DOSTime(now, pDate, pTime);
    //LOGI("+++ Our date    : %04x %04x %d", *pDate, *pTime,
    //  (*pTime >> 11) & 0x1f);
}

/*
 * Convert a time_t to MS-DOS date and time values.
 */
void OuterZip::DOSTime(time_t when, uint16_t* pDate, uint16_t* pTime)
{
    time_t even;

    *pDate = *pTime = 0;

    struct tm* ptm;

    /* round up to an even number of seconds */
    even = (time_t)(((unsigned long)(when) + 1) & (~1));

    /* expand */
    ptm = localtime(&even);

    int year;
    year = ptm->tm_year;
    if (year < 80)
        year = 80;

    *pDate = (year - 80) << 9 | (ptm->tm_mon+1) << 5 | ptm->tm_mday;
    *pTime = ptm->tm_hour << 11 | ptm->tm_min << 5 | ptm->tm_sec >> 1;
}

/*
 * Compress "length" bytes of data from "pSrc" to "pDst".
 */
DIError OuterZip::DeflateGFDToGFD(GenericFD* pDst, GenericFD* pSrc,
    di_off_t srcLen, di_off_t* pCompLength, uint32_t* pCRC)
{
    DIError dierr = kDIErrNone;
    const size_t kBufSize = 32768;
    uint8_t* inBuf = NULL;
    uint8_t* outBuf = NULL;
    z_stream zstream;
    uint32_t crc;
    int zerr;

    /*
     * Create an input buffer and an output buffer.
     */
    inBuf = new uint8_t[kBufSize];
    outBuf = new uint8_t[kBufSize];
    if (inBuf == NULL || outBuf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    /*
     * Initialize the zlib stream.
     */
    memset(&zstream, 0, sizeof(zstream));
    zstream.zalloc = Z_NULL;
    zstream.zfree = Z_NULL;
    zstream.opaque = Z_NULL;
    zstream.next_in = NULL;
    zstream.avail_in = 0;
    zstream.next_out = outBuf;
    zstream.avail_out = kBufSize;
    zstream.data_type = Z_UNKNOWN;

    zerr = deflateInit2(&zstream, Z_BEST_COMPRESSION,
        Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (zerr != Z_OK) {
        dierr = kDIErrInternal;
        if (zerr == Z_VERSION_ERROR) {
            LOGI("Installed zlib is not compatible with linked version (%s)",
                ZLIB_VERSION);
        } else {
            LOGI("Call to deflateInit2 failed (zerr=%d)", zerr);
        }
        goto bail;
    }

    crc = crc32(0L, Z_NULL, 0);

    /*
     * Loop while we have data.
     */
    do {
        long getSize;
        int flush;

        /* only read if the input is empty */
        if (zstream.avail_in == 0 && srcLen) {
            getSize = (srcLen > (long) kBufSize) ? kBufSize : (long) srcLen;
            LOGI("+++ reading %ld bytes", getSize);

            dierr = pSrc->Read(inBuf, getSize);
            if (dierr != kDIErrNone) {
                LOGI("deflate read failed");
                goto z_bail;
            }

            srcLen -= getSize;

            crc = crc32(crc, inBuf, getSize);

            zstream.next_in = inBuf;
            zstream.avail_in = getSize;
        }

        if (srcLen == 0)
            flush = Z_FINISH;       /* tell zlib that we're done */
        else
            flush = Z_NO_FLUSH;     /* more to come! */

        zerr = deflate(&zstream, flush);
        if (zerr != Z_OK && zerr != Z_STREAM_END) {
            LOGI("zlib deflate call failed (zerr=%d)", zerr);
            dierr = kDIErrInternal;
            goto z_bail;
        }

        /* write when we're full or when we're done */
        if (zstream.avail_out == 0 ||
            (zerr == Z_STREAM_END && zstream.avail_out != kBufSize))
        {
            LOGI("+++ writing %ld bytes", zstream.next_out - outBuf);
            dierr = pDst->Write(outBuf, zstream.next_out - outBuf);
            if (dierr != kDIErrNone) {
                LOGI("write failed in deflate");
                goto z_bail;
            }

            zstream.next_out = outBuf;
            zstream.avail_out = kBufSize;
        }
    } while (zerr == Z_OK);

    assert(zerr == Z_STREAM_END);       /* other errors should've been caught */

    *pCompLength = zstream.total_out;
    *pCRC = crc;

z_bail:
    deflateEnd(&zstream);        /* free up any allocated structures */

bail:
    delete[] inBuf;
    delete[] outBuf;

    return dierr;
}

/*
 * Set the "fExtension" field.
 */
void OuterZip::SetExtension(const char* ext)
{
    delete[] fExtension;
    fExtension = StrcpyNew(ext);
}


/*
 * ===================================
 *      OuterZip::LocalFileHeader
 * ===================================
 */

/*
 * Read a local file header.
 *
 * On entry, "pGFD" points to the signature at the start of the header.
 * On exit, "pGFD" points to the start of data.
 */
DIError OuterZip::LocalFileHeader::Read(GenericFD* pGFD)
{
    DIError dierr = kDIErrNone;
    uint8_t buf[kLFHLen];

    dierr = pGFD->Read(buf, kLFHLen);
    if (dierr != kDIErrNone)
        goto bail;

    if (GetLongLE(&buf[0x00]) != kSignature) {
        LOGI(" ZIP: whoops: didn't find expected signature");
        assert(false);
        return kDIErrGeneric;
    }

    fVersionToExtract = GetShortLE(&buf[0x04]);
    fGPBitFlag = GetShortLE(&buf[0x06]);
    fCompressionMethod = GetShortLE(&buf[0x08]);
    fLastModFileTime = GetShortLE(&buf[0x0a]);
    fLastModFileDate = GetShortLE(&buf[0x0c]);
    fCRC32 = GetLongLE(&buf[0x0e]);
    fCompressedSize = GetLongLE(&buf[0x12]);
    fUncompressedSize = GetLongLE(&buf[0x16]);
    fFileNameLength = GetShortLE(&buf[0x1a]);
    fExtraFieldLength = GetShortLE(&buf[0x1c]);

    /* grab filename */
    if (fFileNameLength != 0) {
        assert(fFileName == NULL);
        fFileName = new uint8_t[fFileNameLength+1];
        if (fFileName == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        } else {
            dierr = pGFD->Read(fFileName, fFileNameLength);
            fFileName[fFileNameLength] = '\0';
        }
        if (dierr != kDIErrNone)
            goto bail;
    }

    dierr = pGFD->Seek(fExtraFieldLength, kSeekCur);
    if (dierr != kDIErrNone)
        goto bail;

bail:
    return dierr;
}

/*
 * Write a local file header.
 */
DIError OuterZip::LocalFileHeader::Write(GenericFD* pGFD)
{
    DIError dierr = kDIErrNone;
    uint8_t buf[kLFHLen];

    PutLongLE(&buf[0x00], kSignature);
    PutShortLE(&buf[0x04], fVersionToExtract);
    PutShortLE(&buf[0x06], fGPBitFlag);
    PutShortLE(&buf[0x08], fCompressionMethod);
    PutShortLE(&buf[0x0a], fLastModFileTime);
    PutShortLE(&buf[0x0c], fLastModFileDate);
    PutLongLE(&buf[0x0e], fCRC32);
    PutLongLE(&buf[0x12], fCompressedSize);
    PutLongLE(&buf[0x16], fUncompressedSize);
    PutShortLE(&buf[0x1a], fFileNameLength);
    PutShortLE(&buf[0x1c], fExtraFieldLength);

    dierr = pGFD->Write(buf, kLFHLen);
    if (dierr != kDIErrNone)
        goto bail;

    /* write filename */
    if (fFileNameLength != 0) {
        dierr = pGFD->Write(fFileName, fFileNameLength);
        if (dierr != kDIErrNone)
            goto bail;
    }
    assert(fExtraFieldLength == 0);

bail:
    return dierr;
}

/*
 * Change the filename field.
 */
void OuterZip::LocalFileHeader::SetFileName(const char* name)
{
    delete[] fFileName;
    fFileName = NULL;
    fFileNameLength = 0;

    if (name != NULL) {
        fFileNameLength = (uint16_t)strlen(name);
        fFileName = new uint8_t[fFileNameLength+1];
        if (fFileName == NULL) {
            LOGW("Malloc failure in SetFileName %u", fFileNameLength);
            fFileName = NULL;
            fFileNameLength = 0;
        } else {
            memcpy(fFileName, name, fFileNameLength);
            fFileName[fFileNameLength] = '\0';
            LOGD("+++ OuterZip LFH filename set to '%s'", fFileName);
        }
    }
}

/*
 * Dump the contents of a LocalFileHeader object.
 */
void OuterZip::LocalFileHeader::Dump(void) const
{
    LOGI(" LocalFileHeader contents:");
    LOGI("  versToExt=%u gpBits=0x%04x compression=%u",
        fVersionToExtract, fGPBitFlag, fCompressionMethod);
    LOGI("  modTime=0x%04x modDate=0x%04x crc32=0x%08x",
        fLastModFileTime, fLastModFileDate, fCRC32);
    LOGI("  compressedSize=%u uncompressedSize=%u",
        fCompressedSize, fUncompressedSize);
    LOGI("  filenameLen=%u extraLen=%u",
        fFileNameLength, fExtraFieldLength);
}


/*
 * ===================================
 *      OuterZip::CentralDirEntry
 * ===================================
 */

/*
 * Read the central dir entry that appears next in the file.
 *
 * On entry, "pGFD" should be positioned on the signature bytes for the
 * entry.  On exit, "pGFD" will point at the signature word for the next
 * entry or for the EOCD.
 */
DIError OuterZip::CentralDirEntry::Read(GenericFD* pGFD)
{
    DIError dierr = kDIErrNone;
    uint8_t buf[kCDELen];

    dierr = pGFD->Read(buf, kCDELen);
    if (dierr != kDIErrNone)
        goto bail;

    if (GetLongLE(&buf[0x00]) != kSignature) {
        LOGI(" ZIP: whoops: didn't find expected signature");
        assert(false);
        return kDIErrGeneric;
    }

    fVersionMadeBy = GetShortLE(&buf[0x04]);
    fVersionToExtract = GetShortLE(&buf[0x06]);
    fGPBitFlag = GetShortLE(&buf[0x08]);
    fCompressionMethod = GetShortLE(&buf[0x0a]);
    fLastModFileTime = GetShortLE(&buf[0x0c]);
    fLastModFileDate = GetShortLE(&buf[0x0e]);
    fCRC32 = GetLongLE(&buf[0x10]);
    fCompressedSize = GetLongLE(&buf[0x14]);
    fUncompressedSize = GetLongLE(&buf[0x18]);
    fFileNameLength = GetShortLE(&buf[0x1c]);
    fExtraFieldLength = GetShortLE(&buf[0x1e]);
    fFileCommentLength = GetShortLE(&buf[0x20]);
    fDiskNumberStart = GetShortLE(&buf[0x22]);
    fInternalAttrs = GetShortLE(&buf[0x24]);
    fExternalAttrs = GetLongLE(&buf[0x26]);
    fLocalHeaderRelOffset = GetLongLE(&buf[0x2a]);

    /* grab filename */
    if (fFileNameLength != 0) {
        assert(fFileName == NULL);
        fFileName = new uint8_t[fFileNameLength+1];
        if (fFileName == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        } else {
            dierr = pGFD->Read(fFileName, fFileNameLength);
            fFileName[fFileNameLength] = '\0';
        }
        if (dierr != kDIErrNone)
            goto bail;
    }

    /* skip over "extra field" */
    dierr = pGFD->Seek(fExtraFieldLength, kSeekCur);
    if (dierr != kDIErrNone)
        goto bail;

    /* grab comment, if any */
    if (fFileCommentLength != 0) {
        assert(fFileComment == NULL);
        fFileComment = new uint8_t[fFileCommentLength+1];
        if (fFileComment == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        } else {
            dierr = pGFD->Read(fFileComment, fFileCommentLength);
            fFileComment[fFileCommentLength] = '\0';
        }
        if (dierr != kDIErrNone)
            goto bail;
    }

bail:
    return dierr;
}

/*
 * Write a central dir entry.
 */
DIError OuterZip::CentralDirEntry::Write(GenericFD* pGFD)
{
    DIError dierr = kDIErrNone;
    uint8_t buf[kCDELen];

    PutLongLE(&buf[0x00], kSignature);
    PutShortLE(&buf[0x04], fVersionMadeBy);
    PutShortLE(&buf[0x06], fVersionToExtract);
    PutShortLE(&buf[0x08], fGPBitFlag);
    PutShortLE(&buf[0x0a], fCompressionMethod);
    PutShortLE(&buf[0x0c], fLastModFileTime);
    PutShortLE(&buf[0x0e], fLastModFileDate);
    PutLongLE(&buf[0x10], fCRC32);
    PutLongLE(&buf[0x14], fCompressedSize);
    PutLongLE(&buf[0x18], fUncompressedSize);
    PutShortLE(&buf[0x1c], fFileNameLength);
    PutShortLE(&buf[0x1e], fExtraFieldLength);
    PutShortLE(&buf[0x20], fFileCommentLength);
    PutShortLE(&buf[0x22], fDiskNumberStart);
    PutShortLE(&buf[0x24], fInternalAttrs);
    PutLongLE(&buf[0x26], fExternalAttrs);
    PutLongLE(&buf[0x2a], fLocalHeaderRelOffset);

    dierr = pGFD->Write(buf, kCDELen);
    if (dierr != kDIErrNone)
        goto bail;

    /* write filename */
    if (fFileNameLength != 0) {
        dierr = pGFD->Write(fFileName, fFileNameLength);
        if (dierr != kDIErrNone)
            goto bail;
    }
    assert(fExtraFieldLength == 0);
    assert(fFileCommentLength == 0);

bail:
    return dierr;
}

/*
 * Change the filename field.
 */
void OuterZip::CentralDirEntry::SetFileName(const char* name)
{
    delete[] fFileName;
    fFileName = NULL;
    fFileNameLength = 0;

    if (name != NULL) {
        fFileNameLength = (uint16_t)strlen(name);
        fFileName = new uint8_t[fFileNameLength+1];
        if (fFileName == NULL) {
            LOGI("Malloc failure in SetFileName %u", fFileNameLength);
            fFileName = NULL;
            fFileNameLength = 0;
        } else {
            memcpy(fFileName, name, fFileNameLength);
            fFileName[fFileNameLength] = '\0';
            LOGI("+++ OuterZip CDE filename set to '%s'", fFileName);
        }
    }
}

/*
 * Dump the contents of a CentralDirEntry object.
 */
void OuterZip::CentralDirEntry::Dump(void) const
{
    LOGI(" CentralDirEntry contents:");
    LOGI("  versMadeBy=%u versToExt=%u gpBits=0x%04x compression=%u",
        fVersionMadeBy, fVersionToExtract, fGPBitFlag, fCompressionMethod);
    LOGI("  modTime=0x%04x modDate=0x%04x crc32=0x%08x",
        fLastModFileTime, fLastModFileDate, fCRC32);
    LOGI("  compressedSize=%u uncompressedSize=%u",
        fCompressedSize, fUncompressedSize);
    LOGI("  filenameLen=%u extraLen=%u commentLen=%u",
        fFileNameLength, fExtraFieldLength, fFileCommentLength);
    LOGI("  diskNumStart=%u intAttr=0x%04x extAttr=0x%08x relOffset=%u",
        fDiskNumberStart, fInternalAttrs, fExternalAttrs,
        fLocalHeaderRelOffset);

    if (fFileName != NULL) {
        LOGI("  filename: '%s'", fFileName);
    }
    if (fFileComment != NULL) {
        LOGI("  comment: '%s'", fFileComment);
    }
}


/*
 * ===================================
 *      OuterZip::EndOfCentralDir
 * ===================================
 */

/*
 * Read the end-of-central-dir fields.
 *
 * "buf" should be positioned at the EOCD signature.
 */
DIError OuterZip::EndOfCentralDir::ReadBuf(const uint8_t* buf, int len)
{
    if (len < kEOCDLen) {
        /* looks like ZIP file got truncated */
        LOGI(" Zip EOCD: expected >= %d bytes, found %d",
            kEOCDLen, len);
        return kDIErrBadArchiveStruct;
    }

    if (GetLongLE(&buf[0x00]) != kSignature)
        return kDIErrInternal;

    fDiskNumber = GetShortLE(&buf[0x04]);
    fDiskWithCentralDir = GetShortLE(&buf[0x06]);
    fNumEntries = GetShortLE(&buf[0x08]);
    fTotalNumEntries = GetShortLE(&buf[0x0a]);
    fCentralDirSize = GetLongLE(&buf[0x0c]);
    fCentralDirOffset = GetLongLE(&buf[0x10]);
    fCommentLen = GetShortLE(&buf[0x14]);

    return kDIErrNone;
}

/*
 * Write an end-of-central-directory section.
 */
DIError OuterZip::EndOfCentralDir::Write(GenericFD* pGFD)
{
    DIError dierr = kDIErrNone;
    uint8_t buf[kEOCDLen];

    PutLongLE(&buf[0x00], kSignature);
    PutShortLE(&buf[0x04], fDiskNumber);
    PutShortLE(&buf[0x06], fDiskWithCentralDir);
    PutShortLE(&buf[0x08], fNumEntries);
    PutShortLE(&buf[0x0a], fTotalNumEntries);
    PutLongLE(&buf[0x0c], fCentralDirSize);
    PutLongLE(&buf[0x10], fCentralDirOffset);
    PutShortLE(&buf[0x14], fCommentLen);

    dierr = pGFD->Write(buf, kEOCDLen);
    if (dierr != kDIErrNone)
        goto bail;

    assert(fCommentLen == 0);

bail:
    return dierr;
}

/*
 * Dump the contents of an EndOfCentralDir object.
 */
void
OuterZip::EndOfCentralDir::Dump(void) const
{
    LOGI(" EndOfCentralDir contents:");
    LOGI("  diskNum=%u diskWCD=%u numEnt=%u totalNumEnt=%u",
        fDiskNumber, fDiskWithCentralDir, fNumEntries, fTotalNumEntries);
    LOGI("  centDirSize=%u centDirOff=%u commentLen=%u",
        fCentralDirSize, fCentralDirOffset, fCommentLen);
}
