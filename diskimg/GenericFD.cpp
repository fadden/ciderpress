/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Generic file descriptor class.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"

/*
 * ===========================================================================
 *      GenericFD utility functions
 * ===========================================================================
 */

/*
 * Copy "length" bytes from "pSrc" to "pDst".  Both GenericFDs should be
 * seeked to their initial positions.
 *
 * If "pCRC" is non-NULL, this computes a CRC32 as it goes, using the zlib
 * library function.
 */
/*static*/ DIError GenericFD::CopyFile(GenericFD* pDst, GenericFD* pSrc,
    di_off_t length, uint32_t* pCRC)
{
    DIError dierr = kDIErrNone;
    const int kCopyBufSize = 32768;
    uint8_t* copyBuf = NULL;
    int copySize;

    LOGD("+++ CopyFile: %ld bytes", (long) length);

    if (pDst == NULL || pSrc == NULL || length < 0)
        return kDIErrInvalidArg;
    if (length == 0)
        return kDIErrNone;

    copyBuf = new uint8_t[kCopyBufSize];
    if (copyBuf == NULL)
        return kDIErrMalloc;

    if (pCRC != NULL)
        *pCRC = crc32(0L, Z_NULL, 0);

    while (length != 0) {
        copySize = kCopyBufSize;
        if (copySize > length)
            copySize = (int) length;

        dierr = pSrc->Read(copyBuf, copySize);
        if (dierr != kDIErrNone)
            goto bail;

        if (pCRC != NULL)
            *pCRC = crc32(*pCRC, copyBuf, copySize);

        dierr = pDst->Write(copyBuf, copySize);
        if (dierr != kDIErrNone)
            goto bail;

        length -= copySize;
    }

bail:
    delete[] copyBuf;
    return dierr;
}


/*
 * ===========================================================================
 *      GFDFile
 * ===========================================================================
 */

/*
 * The stdio functions (fopen/fread/fwrite/fseek/ftell) are buffered and,
 * therefore, faster for small operations.  Unfortunately we need 64-bit
 * file offsets, and it doesn't look like the Windows stdio stuff will
 * support it cleanly (e.g. even the _FPOSOFF macro returns a "long").
 *
 * Recent versions of Linux have "fseeko", which is like fseek but takes
 * an off_t, so we can continue to use the FILE* functions there.  Under
 * Windows "lseek" takes a long, so we have to use their specific 64-bit
 * variant.
 *
 * TODO: Visual Studio 2005 added _fseeki64.  We should be able to merge
 * the bulk of the implementation now.
 */
#ifdef HAVE_FSEEKO

DIError GFDFile::Open(const char* filename, bool readOnly)
{
    DIError dierr = kDIErrNone;

    if (fFp != NULL)
        return kDIErrAlreadyOpen;
    if (filename == NULL)
        return kDIErrInvalidArg;
    if (filename[0] == '\0')
        return kDIErrInvalidArg;

    delete[] fPathName;
    fPathName = new char[strlen(filename) +1];
    strcpy(fPathName, filename);

    fFp = fopen(filename, readOnly ? "rb" : "r+b");
    if (fFp == NULL) {
        if (errno == EACCES)
            dierr = kDIErrAccessDenied;
        else
            dierr = ErrnoOrGeneric();
        LOGI("  GDFile Open failed opening '%s', ro=%d (err=%d)",
            filename, readOnly, dierr);
        return dierr;
    }
    fReadOnly = readOnly;
    return dierr;
}

DIError GFDFile::Read(void* buf, size_t length, size_t* pActual)
{
    DIError dierr = kDIErrNone;
    size_t actual;

    if (fFp == NULL)
        return kDIErrNotReady;
    actual = ::fread(buf, 1, length, fFp);
    if (actual == 0) {
        if (feof(fFp))
            return kDIErrEOF;
        if (ferror(fFp)) {
            dierr = ErrnoOrGeneric();
            return dierr;
        }
        LOGI("MYSTERY FREAD RESULT");
        return kDIErrInternal;
    }

    if (pActual == NULL) {
        if (actual != length) {
            dierr = ErrnoOrGeneric();
            LOGW("  GDFile Read failed on %lu bytes (actual=%lu, err=%d)",
                (unsigned long) length, (unsigned long) actual, dierr);
            return dierr;
        }
    } else {
        *pActual = actual;
    }
    return dierr;
}

DIError GFDFile::Write(const void* buf, size_t length, size_t* pActual)
{
    DIError dierr = kDIErrNone;

    if (fFp == NULL)
        return kDIErrNotReady;
    if (fReadOnly)
        return kDIErrAccessDenied;
    assert(pActual == NULL);     // not handling this yet
    if (::fwrite(buf, length, 1, fFp) != 1) {
        dierr = ErrnoOrGeneric();
        LOGW("  GDFile Write failed on %lu bytes (err=%d)",
            (unsigned long) length, dierr);
        return dierr;
    }
    return dierr;
}

DIError GFDFile::Seek(di_off_t offset, DIWhence whence)
{
    DIError dierr = kDIErrNone;
    //static const long kOneGB = 1024*1024*1024;
    //static const long kAlmostTwoGB = kOneGB + (kOneGB -1);

    if (fFp == NULL)
        return kDIErrNotReady;
    //assert(offset <= kAlmostTwoGB);
    //if (::fseek(fFp, (long) offset, whence) != 0) {
    if (::fseeko(fFp, offset, whence) != 0) {
        dierr = ErrnoOrGeneric();
        LOGI("  GDFile Seek failed (err=%d)", dierr);
        return dierr;
    }
    return dierr;
}

di_off_t GFDFile::Tell(void)
{
    DIError dierr = kDIErrNone;
    di_off_t result;

    if (fFp == NULL)
        return kDIErrNotReady;
    //result = ::ftell(fFp);
    result = ::ftello(fFp);
    if (result == -1) {
        dierr = ErrnoOrGeneric();
        LOGI("  GDFile Tell failed (err=%d)", dierr);
        return result;
    }
    return result;
}

DIError GFDFile::Truncate(void)
{
#if defined(HAVE_FTRUNCATE)
    int cc;
    cc = ::ftruncate(fileno(fFp), (long) Tell());
    if (cc != 0)
        return kDIErrWriteFailed;
#elif defined(HAVE_CHSIZE)
    assert(false);      // not tested
    int cc;
    cc = ::chsize(fFd, (long) Tell());
    if (cc != 0)
        return kDIErrWriteFailed;
#else
# error "missing truncate"
#endif
    return kDIErrNone;
}

DIError GFDFile::Close(void)
{
    if (fFp == NULL)
        return kDIErrNotReady;

    LOGI("  GFDFile closing '%s'", fPathName);
    fclose(fFp);
    fFp = NULL;
    return kDIErrNone;
}

#else /*HAVE_FSEEKO*/

DIError GFDFile::Open(const char* filename, bool readOnly)
{
    DIError dierr = kDIErrNone;

    if (fFd >= 0)
        return kDIErrAlreadyOpen;
    if (filename == NULL)
        return kDIErrInvalidArg;
    if (filename[0] == '\0')
        return kDIErrInvalidArg;

    delete[] fPathName;
    fPathName = new char[strlen(filename) +1];
    strcpy(fPathName, filename);

    fFd = open(filename, readOnly ? O_RDONLY|O_BINARY : O_RDWR|O_BINARY, 0);
    if (fFd < 0) {
        if (errno == EACCES) {
            dierr = kDIErrAccessDenied;
        } else if (errno == EINVAL) {
            // Happens on Win32 if a Unicode filename conversion failed,
            // because non-converting chars turn into '?', which is illegal.
            dierr = kDIErrInvalidArg;
        } else {
            dierr = ErrnoOrGeneric();
        }
        LOGW("  GDFile Open failed opening '%s', ro=%d (err=%d)",
            filename, readOnly, dierr);
        return dierr;
    }
    fReadOnly = readOnly;
    return dierr;
}

DIError GFDFile::Read(void* buf, size_t length, size_t* pActual)
{
    DIError dierr;
    ssize_t actual;

    if (fFd < 0)
        return kDIErrNotReady;
    actual = ::read(fFd, buf, length);
    if (actual == 0)
        return kDIErrEOF;
    if (actual < 0) {
        dierr = ErrnoOrGeneric();
        LOGW("  GDFile Read failed on %d bytes (actual=%d, err=%d)",
            length, actual, dierr);
        return dierr;
    }

    if (pActual == NULL) {
        if (actual != (ssize_t) length) {
            LOGI("  GDFile Read partial (wanted=%d actual=%d)",
                length, actual);
            return kDIErrReadFailed;
        }
    } else {
        *pActual = actual;
    }
    return kDIErrNone;
}

DIError GFDFile::Write(const void* buf, size_t length, size_t* pActual)
{
    DIError dierr;
    ssize_t actual;

    if (fFd < 0)
        return kDIErrNotReady;
    if (fReadOnly)
        return kDIErrAccessDenied;
    assert(pActual == NULL);     // not handling partial writes yet
    actual = ::write(fFd, buf, length);
    if (actual != (ssize_t) length) {
        dierr = ErrnoOrGeneric();
        LOGI("  GDFile Write failed on %d bytes (actual=%d err=%d)",
            length, actual, dierr);
        return dierr;
    }
    return kDIErrNone;
}

DIError GFDFile::Seek(di_off_t offset, DIWhence whence)
{
    DIError dierr = kDIErrNone;
    if (fFd < 0)
        return kDIErrNotReady;

#ifdef WIN32
    __int64 newPosn;
    const __int64 kFailure = (__int64) -1;
    newPosn = ::_lseeki64(fFd, (__int64) offset, whence);
#else
    di_off_t newPosn;
    const di_off_t kFailure = (di_off_t) -1;
    newPosn = lseek(fFd, offset, whence);
#endif

    if (newPosn == kFailure) {
        assert((uint32_t) offset != 0xccccccccUL); // uninitialized data!
        dierr = ErrnoOrGeneric();
        LOGI("  GDFile Seek %ld-%lu failed (err=%d)",
            (long) (offset >> 32), (uint32_t) offset, dierr);
    }
    return dierr;
}

di_off_t GFDFile::Tell(void)
{
    DIError dierr = kDIErrNone;
    di_off_t result;

    if (fFd < 0)
        return kDIErrNotReady;

#ifdef WIN32
    result = ::_lseeki64(fFd, 0, SEEK_CUR);
#else
    result = lseek(fFd, 0, SEEK_CUR);
#endif

    if (result == -1) {
        dierr = ErrnoOrGeneric();
        LOGI("  GDFile Tell failed (err=%d)", dierr);
        return result;
    }
    return result;
}

DIError GFDFile::Truncate(void)
{
#if defined(HAVE_FTRUNCATE)
    int cc;
    cc = ::ftruncate(fFd, (long) Tell());
    if (cc != 0)
        return kDIErrWriteFailed;
#elif defined(HAVE_CHSIZE)
    int cc;
    cc = ::chsize(fFd, (long) Tell());
    if (cc != 0)
        return kDIErrWriteFailed;
#else
# error "missing truncate"
#endif
    return kDIErrNone;
}

DIError GFDFile::Close(void)
{
    if (fFd < 0)
        return kDIErrNotReady;

    LOGI("  GFDFile closing '%s'", fPathName);
    ::close(fFd);
    fFd = -1;
    return kDIErrNone;
}
#endif /*HAVE_FSEEKO else*/


/*
 * ===========================================================================
 *      GFDBuffer
 * ===========================================================================
 */

DIError GFDBuffer::Open(void* buffer, di_off_t length, bool doDelete,
    bool doExpand, bool readOnly)
{
    if (fBuffer != NULL)
        return kDIErrAlreadyOpen;
    if (length <= 0)
        return kDIErrInvalidArg;
    if (length > kMaxReasonableSize) {
        // be reasonable
        LOGI(" GFDBuffer refusing to allocate buffer size(long)=%ld bytes",
            (long) length);
        return kDIErrInvalidArg;
    }

    /* if buffer is NULL, allocate it ourselves */
    if (buffer == NULL) {
        fBuffer = (void*) new uint8_t[(int) length];
        if (fBuffer == NULL)
            return kDIErrMalloc;
    } else
        fBuffer = buffer;

    fLength = (long) length;
    fAllocLength = (long) length;
    fDoDelete = doDelete;
    fDoExpand = doExpand;
    fReadOnly = readOnly;

    fCurrentOffset = 0;

    return kDIErrNone;
}

DIError GFDBuffer::Read(void* buf, size_t length, size_t* pActual)
{
    if (fBuffer == NULL)
        return kDIErrNotReady;
    if (length == 0)
        return kDIErrInvalidArg;

    if (fCurrentOffset + (long)length > fLength) {
        if (pActual == NULL) {
            LOGW("  GFDBuffer underrrun off=%ld len=%lu flen=%ld",
                (long) fCurrentOffset, (unsigned long) length, (long) fLength);
            return kDIErrDataUnderrun;
        } else {
            /* set *pActual and adjust "length" */
            assert(fLength >= fCurrentOffset);
            length = (size_t) (fLength - fCurrentOffset);
            *pActual = length;

            if (length == 0)
                return kDIErrEOF;
        }
    }
    if (pActual != NULL)
        *pActual = length;

    memcpy(buf, (const char*)fBuffer + fCurrentOffset, length);
    fCurrentOffset += length;

    return kDIErrNone;
}

DIError GFDBuffer::Write(const void* buf, size_t length, size_t* pActual)
{
    if (fBuffer == NULL)
        return kDIErrNotReady;
    assert(pActual == NULL);     // not handling this yet
    if (fCurrentOffset + (long)length > fLength) {
        if (!fDoExpand) {
            LOGI("  GFDBuffer overrun off=%ld len=%lu flen=%ld",
                (long) fCurrentOffset, (unsigned long) length, (long) fLength);
            return kDIErrDataOverrun;
        }

        /*
         * Expand the buffer as needed.
         *
         * We delete the old buffer unless "doDelete" is not set, in
         * which case we just drop the pointer.  Anything we allocate
         * here can and will be deleted; "doDelete" only applies to the
         * pointer initially passed in.
         */
        if (fCurrentOffset + (long)length <= fAllocLength) {
            /* fits inside allocated space, so just extend length */
            fLength = (long) fCurrentOffset + (long)length;
        } else {
            /* does not fit, realloc buffer */
            fAllocLength = (long) fCurrentOffset + (long)length + 8*1024;
            LOGI("Reallocating buffer (new size = %ld)", fAllocLength);
            assert(fAllocLength < kMaxReasonableSize);
            char* newBuf = new char[(int) fAllocLength];
            if (newBuf == NULL)
                return kDIErrMalloc;

            memcpy(newBuf, fBuffer, fLength);

            if (fDoDelete)
                delete[] (char*)fBuffer;
            else
                fDoDelete = true;       // future deletions are okay

            fBuffer = newBuf;
            fLength = (long) fCurrentOffset + (long)length;
        }
    }

    memcpy((char*)fBuffer + fCurrentOffset, buf, length);
    fCurrentOffset += length;

    return kDIErrNone;
}

DIError GFDBuffer::Seek(di_off_t offset, DIWhence whence)
{
    if (fBuffer == NULL)
        return kDIErrNotReady;

    switch (whence) {
    case kSeekSet:
        if (offset < 0 || offset >= fLength)
            return kDIErrInvalidArg;
        fCurrentOffset = offset;
        break;
    case kSeekEnd:
        if (offset > 0 || offset < -fLength)
            return kDIErrInvalidArg;
        fCurrentOffset = fLength + offset;
        break;
    case kSeekCur:
        if (offset < -fCurrentOffset ||
            offset >= (fLength - fCurrentOffset))
        {
            return kDIErrInvalidArg;
        }
        fCurrentOffset += offset;
        break;
    default:
        assert(false);
        return kDIErrInvalidArg;
    }

    assert(fCurrentOffset >= 0 && fCurrentOffset <= fLength);
    return kDIErrNone;
}

di_off_t GFDBuffer::Tell(void)
{
    if (fBuffer == NULL)
        return (di_off_t) -1;
    return fCurrentOffset;
}

DIError GFDBuffer::Close(void)
{
    if (fBuffer == NULL)
        return kDIErrNone;

    if (fDoDelete) {
        LOGI("  GFDBuffer closing and deleting");
        delete[] (char*) fBuffer;
    } else {
        LOGI("  GFDBuffer closing");
    }
    fBuffer = NULL;

    return kDIErrNone;
}


#ifdef _WIN32
/*
 * ===========================================================================
 *      GFDWinVolume
 * ===========================================================================
 */

/*
 * This class is intended for use with logical volumes under Win32.  Such
 * devices must be accessed on 512-byte boundaries, which means no arbitrary
 * seeks or reads.  The device driver doesn't seem too adept at figuring
 * out how large the device is, either, so we need to work that out for
 * ourselves.  (The standard approach appears to involve examining the
 * partition map for the logical or physical volume, but we don't have a
 * partition map to look at.)
 */

/*
 * Prepare a logical volume device for reading or writing.  "deviceName"
 * must have the form "N:\" for a logical volume or "80:\" for a physical
 * volume.
 */
DIError GFDWinVolume::Open(const char* deviceName, bool readOnly)
{
    DIError dierr = kDIErrNone;
    HANDLE handle = NULL;
    //uint32_t kTwoGBBlocks;
    
    if (fVolAccess.Ready())
        return kDIErrAlreadyOpen;
    if (deviceName == NULL)
        return kDIErrInvalidArg;
    if (deviceName[0] == '\0')
        return kDIErrInvalidArg;

    delete[] fPathName;
    fPathName = new char[strlen(deviceName) +1];
    strcpy(fPathName, deviceName);

    // Create a UNICODE representation of the device name.  We may want
    // to make the argument UNICODE instead, but most of diskimg is 8-bit
    // character oriented.
    size_t srcLen = strlen(deviceName) + 1;
    WCHAR* wdeviceName = new WCHAR[srcLen];
    size_t convertedChars;
    mbstowcs_s(&convertedChars, wdeviceName, srcLen, deviceName, _TRUNCATE);
    dierr = fVolAccess.Open(wdeviceName, readOnly);
    delete[] wdeviceName;
    if (dierr != kDIErrNone)
        goto bail;

    fBlockSize = fVolAccess.GetBlockSize(); // must be power of 2
    assert(fBlockSize > 0);
    //kTwoGBBlocks = kTwoGB / fBlockSize;

    long totalBlocks;
    totalBlocks = fVolAccess.GetTotalBlocks();
    fVolumeEOF = (di_off_t)totalBlocks * fBlockSize;

    assert(fVolumeEOF > 0);

    fReadOnly = readOnly;

bail:
    return dierr;
}

DIError GFDWinVolume::Read(void* buf, size_t length, size_t* pActual)
{
    DIError dierr = kDIErrNone;
    uint8_t* blkBuf = NULL;

    //LOGI(" GFDWinVolume: reading %ld bytes from offset %ld", length,
    //  fCurrentOffset);

    if (!fVolAccess.Ready())
        return kDIErrNotReady;

    // don't allow reading past the end of file
    if (fCurrentOffset + (long) length > fVolumeEOF) {
        if (pActual == NULL)
            return kDIErrDataUnderrun;
        length = (size_t) (fVolumeEOF - fCurrentOffset);
    }
    if (pActual != NULL)
        *pActual = length;
    if (length == 0)
        return kDIErrNone;

    long advanceLen = length;

    blkBuf = new uint8_t[fBlockSize];     // get this off the heap??
    long blockIndex = (long) (fCurrentOffset / fBlockSize);
    int bufOffset = (int) (fCurrentOffset % fBlockSize);    // req power of 2
    assert(blockIndex >= 0);

    /*
     * When possible, do multi-block reads directly into "buf".  The first
     * and last block may require special handling.
     */
    while (length) {
        assert(length > 0);

        if (bufOffset != 0 || length < (size_t) fBlockSize) {
            assert(bufOffset >= 0 && bufOffset < fBlockSize);

            size_t thisCount;

            dierr = fVolAccess.ReadBlocks(blockIndex, 1, blkBuf);
            if (dierr != kDIErrNone)
                goto bail;

            thisCount = fBlockSize - bufOffset;
            if (thisCount > length)
                thisCount = length;

            //LOGI("    Copying %d bytes from block %d",
            //  thisCount, blockIndex);

            memcpy(buf, blkBuf + bufOffset, thisCount);
            length -= thisCount;
            buf = (char*) buf + thisCount;

            bufOffset = 0;
            blockIndex++;
        } else {
            assert(bufOffset == 0);

            long blockCount = length / fBlockSize;
            assert(blockCount < 32768);

            dierr = fVolAccess.ReadBlocks(blockIndex, (short) blockCount, buf);
            if (dierr != kDIErrNone)
                goto bail;

            length -= blockCount * fBlockSize;
            buf = (char*) buf + blockCount * fBlockSize;

            blockIndex += blockCount;
        }

    }

    fCurrentOffset += advanceLen;

bail:
    delete[] blkBuf;
    return dierr;
}

DIError GFDWinVolume::Write(const void* buf, size_t length, size_t* pActual)
{
    DIError dierr = kDIErrNone;
    uint8_t* blkBuf = NULL;

    //LOGI(" GFDWinVolume: writing %ld bytes at offset %ld", length,
    //  fCurrentOffset);

    if (!fVolAccess.Ready())
        return kDIErrNotReady;
    if (fReadOnly)
        return kDIErrAccessDenied;

    // don't allow writing past the end of the volume
    if (fCurrentOffset + (long) length > fVolumeEOF) {
        if (pActual == NULL)
            return kDIErrDataOverrun;
        length = (size_t) (fVolumeEOF - fCurrentOffset);
    }
    if (pActual != NULL)
        *pActual = length;
    if (length == 0)
        return kDIErrNone;

    long advanceLen = length;

    blkBuf = new uint8_t[fBlockSize];     // get this out of the heap??
    long blockIndex = (long) (fCurrentOffset / fBlockSize);
    int bufOffset = (int) (fCurrentOffset % fBlockSize);    // req power of 2
    assert(blockIndex >= 0);

    /*
     * When possible, do multi-block writes directly from "buf".  The first
     * and last block may require special handling.
     */
    while (length) {
        assert(length > 0);

        if (bufOffset != 0 || length < (size_t) fBlockSize) {
            assert(bufOffset >= 0 && bufOffset < fBlockSize);

            size_t thisCount;

            dierr = fVolAccess.ReadBlocks(blockIndex, 1, blkBuf);
            if (dierr != kDIErrNone)
                goto bail;

            thisCount = fBlockSize - bufOffset;
            if (thisCount > length)
                thisCount = length;

            //LOGI("    Copying %d bytes into block %d (off=%d)",
            //  thisCount, blockIndex, bufOffset);

            memcpy(blkBuf + bufOffset, buf, thisCount);
            length -= thisCount;
            buf = (char*) buf + thisCount;

            dierr = fVolAccess.WriteBlocks(blockIndex, 1, blkBuf);
            if (dierr != kDIErrNone)
                goto bail;

            bufOffset = 0;
            blockIndex++;
        } else {
            assert(bufOffset == 0);

            long blockCount = length / fBlockSize;
            assert(blockCount < 32768);

            dierr = fVolAccess.WriteBlocks(blockIndex, (short) blockCount, buf);
            if (dierr != kDIErrNone)
                goto bail;

            length -= blockCount * fBlockSize;
            buf = (char*) buf + blockCount * fBlockSize;

            blockIndex += blockCount;
        }

    }

    fCurrentOffset += advanceLen;

bail:
    delete[] blkBuf;
    return dierr;
}

DIError GFDWinVolume::Seek(di_off_t offset, DIWhence whence)
{
    if (!fVolAccess.Ready())
        return kDIErrNotReady;

    switch (whence) {
    case kSeekSet:
        if (offset < 0 || offset >= fVolumeEOF)
            return kDIErrInvalidArg;
        fCurrentOffset = offset;
        break;
    case kSeekEnd:
        if (offset > 0 || offset < -fVolumeEOF)
            return kDIErrInvalidArg;
        fCurrentOffset = fVolumeEOF + offset;
        break;
    case kSeekCur:
        if (offset < -fCurrentOffset ||
            offset >= (fVolumeEOF - fCurrentOffset))
        {
            return kDIErrInvalidArg;
        }
        fCurrentOffset += offset;
        break;
    default:
        assert(false);
        return kDIErrInvalidArg;
    }

    assert(fCurrentOffset >= 0 && fCurrentOffset <= fVolumeEOF);
    return kDIErrNone;
}

di_off_t GFDWinVolume::Tell(void)
{
    if (!fVolAccess.Ready())
        return (di_off_t) -1;
    return fCurrentOffset;
}

DIError GFDWinVolume::Close(void)
{
    if (!fVolAccess.Ready())
        return kDIErrNotReady;

    LOGI("  GFDWinVolume closing");
    fVolAccess.Close();
    return kDIErrNone;
}
#endif /*_WIN32*/
