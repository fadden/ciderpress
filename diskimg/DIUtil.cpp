/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * DiskImgLib global utility functions.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"

#define kFilenameExtDelim   '.'     /* separates extension from filename */

/*
 * Get values from a memory buffer.
 */
uint16_t DiskImgLib::GetShortLE(const uint8_t* ptr)
{
    return *ptr | (uint16_t) *(ptr+1) << 8;
}

uint32_t DiskImgLib::GetLongLE(const uint8_t* ptr)
{
    return *ptr |
            (uint32_t) *(ptr+1) << 8 |
            (uint32_t) *(ptr+2) << 16 |
            (uint32_t) *(ptr+3) << 24;
}

uint16_t DiskImgLib::GetShortBE(const uint8_t* ptr)
{
    return *(ptr+1) | (uint16_t) *ptr << 8;
}

uint32_t DiskImgLib::GetLongBE(const uint8_t* ptr)
{
    return *(ptr+3) |
            (uint32_t) *(ptr+2) << 8 |
            (uint32_t) *(ptr+1) << 16 |
            (uint32_t) *ptr << 24;
}

uint32_t DiskImgLib::Get24BE(const uint8_t* ptr)
{
    return *(ptr+2) |
            (uint32_t) *(ptr+1) << 8 |
            (uint32_t) *ptr << 16;
}

void DiskImgLib::PutShortLE(uint8_t* ptr, uint16_t val)
{
    *ptr++ = (uint8_t) val;
    *ptr = val >> 8;
}

void DiskImgLib::PutLongLE(uint8_t* ptr, uint32_t val)
{
    *ptr++ = (uint8_t) val;
    *ptr++ = (uint8_t) (val >> 8);
    *ptr++ = (uint8_t) (val >> 16);
    *ptr = (uint8_t) (val >> 24);
}

void DiskImgLib::PutShortBE(uint8_t* ptr, uint16_t val)
{
    *ptr++ = val >> 8;
    *ptr = (uint8_t) val;
}

void DiskImgLib::PutLongBE(uint8_t* ptr, uint32_t val)
{
    *ptr++ = (uint8_t) (val >> 24);
    *ptr++ = (uint8_t) (val >> 16);
    *ptr++ = (uint8_t) (val >> 8);
    *ptr = (uint8_t) val;
}


/*
 * Read a two-byte little-endian value.
 */
DIError DiskImgLib::ReadShortLE(GenericFD* pGFD, uint16_t* pBuf)
{
    DIError dierr;
    uint8_t val[2];

    dierr = pGFD->Read(&val[0], 1);
    if (dierr == kDIErrNone)
        dierr = pGFD->Read(&val[1], 1);

    *pBuf = val[0] | (short) val[1] << 8;
    return dierr;
}

/*
 * Read a four-byte little-endian value.
 */
DIError DiskImgLib::ReadLongLE(GenericFD* pGFD, uint32_t* pBuf)
{
    DIError dierr;
    uint8_t val[4];

    dierr = pGFD->Read(&val[0], 1);
    if (dierr == kDIErrNone)
        dierr = pGFD->Read(&val[1], 1);
    if (dierr == kDIErrNone)
        dierr = pGFD->Read(&val[2], 1);
    if (dierr == kDIErrNone)
        dierr = pGFD->Read(&val[3], 1);

    *pBuf = val[0] | (uint32_t)val[1] << 8 |
        (uint32_t)val[2] << 16 | (uint32_t)val[3] << 24;
    return dierr;
}

/*
 * Write a two-byte little-endian value.
 */
DIError DiskImgLib::WriteShortLE(FILE* fp, uint16_t val)
{
    putc(val, fp);
    putc(val >> 8, fp);
    return kDIErrNone;
}

/*
 * Write a four-byte little-endian value.
 */
DIError DiskImgLib::WriteLongLE(FILE* fp, uint32_t val)
{
    putc(val, fp);
    putc(val >> 8, fp);
    putc(val >> 16, fp);
    putc(val >> 24, fp);
    return kDIErrNone;
}

/*
 * Write a two-byte little-endian value.
 */
DIError DiskImgLib::WriteShortLE(GenericFD* pGFD, uint16_t val)
{
    uint8_t buf;

    buf = (uint8_t) val;
    pGFD->Write(&buf, 1);
    buf = val >> 8;
    return pGFD->Write(&buf, 1);
}

/*
 * Write a four-byte little-endian value.
 */
DIError DiskImgLib::WriteLongLE(GenericFD* pGFD, uint32_t val)
{
    uint8_t buf;

    buf = (uint8_t) val;
    pGFD->Write(&buf, 1);
    buf = (uint8_t) (val >> 8);
    pGFD->Write(&buf, 1);
    buf = (uint8_t) (val >> 16);
    pGFD->Write(&buf, 1);
    buf = (uint8_t) (val >> 24);
    return pGFD->Write(&buf, 1);
}

/*
 * Write a two-byte big-endian value.
 */
DIError DiskImgLib::WriteShortBE(GenericFD* pGFD, uint16_t val)
{
    uint8_t buf;

    buf = val >> 8;
    pGFD->Write(&buf, 1);
    buf = (uint8_t) val;
    return pGFD->Write(&buf, 1);
}

/*
 * Write a four-byte big-endian value.
 */
DIError DiskImgLib::WriteLongBE(GenericFD* pGFD, uint32_t val)
{
    uint8_t buf;

    buf = (uint8_t) (val >> 24);
    pGFD->Write(&buf, 1);
    buf = (uint8_t) (val >> 16);
    pGFD->Write(&buf, 1);
    buf = (uint8_t) (val >> 8);
    pGFD->Write(&buf, 1);
    buf = (uint8_t) val;
    return pGFD->Write(&buf, 1);
}


/*
 * Find the filename component of a local pathname.  Uses the fssep passed
 * in.  If the fssep is '\0' (as is the case for DOS 3.3), then the entire
 * pathname is returned.
 *
 * Always returns a pointer to a string; never returns NULL.
 */
const char* DiskImgLib::FilenameOnly(const char* pathname, char fssep)
{
    const char* retstr;
    const char* pSlash;
    char* tmpStr = NULL;

    assert(pathname != NULL);
    if (fssep == '\0') {
        retstr = pathname;
        goto bail;
    }

    pSlash = strrchr(pathname, fssep);
    if (pSlash == NULL) {
        retstr = pathname;      /* whole thing is the filename */
        goto bail;
    }

    pSlash++;
    if (*pSlash == '\0') {
        if (strlen(pathname) < 2) {
            retstr = pathname;  /* the pathname is just "/"?  Whatever */
            goto bail;
        }

        /* some bonehead put an fssep on the very end; back up before it */
        /* (not efficient, but this should be rare, and I'm feeling lazy) */
        tmpStr = strdup(pathname);
        tmpStr[strlen(pathname)-1] = '\0';
        pSlash = strrchr(tmpStr, fssep);

        if (pSlash == NULL) {
            retstr = pathname;  /* just a filename with a '/' after it */
            goto bail;
        }

        pSlash++;
        if (*pSlash == '\0') {
            retstr = pathname;  /* I give up! */
            goto bail;
        }

        retstr = pathname + (pSlash - tmpStr);

    } else {
        retstr = pSlash;
    }

bail:
    free(tmpStr);
    return retstr;
}

/*
 * Return the filename extension found in a full pathname.
 *
 * An extension is the stuff following the last '.' in the filename.  If
 * there is nothing following the last '.', then there is no extension.
 *
 * Returns a pointer to the '.' preceding the extension, or NULL if no
 * extension was found.
 *
 * We guarantee that there is at least one character after the '.'.
 */
const char* DiskImgLib::FindExtension(const char* pathname, char fssep)
{
    const char* pFilename;
    const char* pExt;

    /*
     * We have to isolate the filename so that we don't get excited
     * about "/foo.bar/file".
     */
    pFilename = FilenameOnly(pathname, fssep);
    assert(pFilename != NULL);
    pExt = strrchr(pFilename, kFilenameExtDelim);

    /* also check for "/blah/foo.", which doesn't count */
    if (pExt != NULL && *(pExt+1) != '\0')
        return pExt;

    return NULL;
}

/*
 * Like strcpy(), but allocate with new[] instead.
 *
 * If "str" is NULL, or "new" fails, this returns NULL.
 *
 * TODO: should be "StrdupNew()"
 */
char* DiskImgLib::StrcpyNew(const char* str)
{
    char* newStr;

    if (str == NULL)
        return NULL;
    newStr = new char[strlen(str)+1];
    if (newStr != NULL)
        strcpy(newStr, str);
    return newStr;
}


#ifdef _WIN32
/*
 * Convert the value from GetLastError() to its DIError counterpart.
 */
DIError DiskImgLib::LastErrorToDIError(void)
{
    DWORD lastErr = ::GetLastError();

    switch (lastErr) {
    case ERROR_FILE_NOT_FOUND:      return kDIErrFileNotFound;      // 2
    case ERROR_ACCESS_DENIED:       return kDIErrAccessDenied;      // 5
    case ERROR_WRITE_PROTECT:       return kDIErrWriteProtected;    // 19
    case ERROR_SECTOR_NOT_FOUND:    return kDIErrGeneric;           // 27
    case ERROR_SHARING_VIOLATION:   return kDIErrSharingViolation;  // 32
    case ERROR_HANDLE_EOF:          return kDIErrEOF;               // 38
    case ERROR_INVALID_PARAMETER:   return kDIErrInvalidArg;        // 87
    case ERROR_SEM_TIMEOUT:         return kDIErrGenericIO;         // 121
        // ERROR_SEM_TIMEOUT seen read bad blocks from floptical under Win2K

    case ERROR_INVALID_HANDLE:                                      // 6
        LOGI("HEY: got ERROR_INVALID_HANDLE!");
        return kDIErrInternal;
    case ERROR_NEGATIVE_SEEK:                                       // 131
        LOGI("HEY: got ERROR_NEGATIVE_SEEK!");
        return kDIErrInternal;
    default:
        LOGI("LastErrorToDIError: not converting 0x%08lx (%ld)",
            lastErr, lastErr);
        return kDIErrGeneric;
    }
}

/*
 * Returns "true" if we're running on Win9x (Win95, Win98, WinME), "false"
 * if not (could be WinNT/2K/XP or even Win31 with Win32s).
 */
bool DiskImgLib::IsWin9x(void)
{
    return false;
}
#endif
