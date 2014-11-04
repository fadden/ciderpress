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
unsigned short
DiskImgLib::GetShortLE(const unsigned char* ptr)
{
    return *ptr | (unsigned short) *(ptr+1) << 8;
}

unsigned long
DiskImgLib::GetLongLE(const unsigned char* ptr)
{
    return *ptr |
            (unsigned long) *(ptr+1) << 8 |
            (unsigned long) *(ptr+2) << 16 |
            (unsigned long) *(ptr+3) << 24;
}

unsigned short
DiskImgLib::GetShortBE(const unsigned char* ptr)
{
    return *(ptr+1) | (unsigned short) *ptr << 8;
}

unsigned long
DiskImgLib::GetLongBE(const unsigned char* ptr)
{
    return *(ptr+3) |
            (unsigned long) *(ptr+2) << 8 |
            (unsigned long) *(ptr+1) << 16 |
            (unsigned long) *ptr << 24;
}

unsigned long
DiskImgLib::Get24BE(const unsigned char* ptr)
{
    return *(ptr+2) |
            (unsigned long) *(ptr+1) << 8 |
            (unsigned long) *ptr << 16;
}

void
DiskImgLib::PutShortLE(unsigned char* ptr, unsigned short val)
{
    *ptr++ = (unsigned char) val;
    *ptr = val >> 8;
}

void
DiskImgLib::PutLongLE(unsigned char* ptr, unsigned long val)
{
    *ptr++ = (unsigned char) val;
    *ptr++ = (unsigned char) (val >> 8);
    *ptr++ = (unsigned char) (val >> 16);
    *ptr = (unsigned char) (val >> 24);
}

void
DiskImgLib::PutShortBE(unsigned char* ptr, unsigned short val)
{
    *ptr++ = val >> 8;
    *ptr = (unsigned char) val;
}

void
DiskImgLib::PutLongBE(unsigned char* ptr, unsigned long val)
{
    *ptr++ = (unsigned char) (val >> 24);
    *ptr++ = (unsigned char) (val >> 16);
    *ptr++ = (unsigned char) (val >> 8);
    *ptr = (unsigned char) val;
}


/*
 * Read a two-byte little-endian value.
 */
DIError
DiskImgLib::ReadShortLE(GenericFD* pGFD, short* pBuf)
{
    DIError dierr;
    unsigned char val[2];

    dierr = pGFD->Read(&val[0], 1);
    if (dierr == kDIErrNone)
        dierr = pGFD->Read(&val[1], 1);

    *pBuf = val[0] | (short) val[1] << 8;
    return dierr;
}

/*
 * Read a four-byte little-endian value.
 */
DIError
DiskImgLib::ReadLongLE(GenericFD* pGFD, long* pBuf)
{
    DIError dierr;
    unsigned char val[4];

    dierr = pGFD->Read(&val[0], 1);
    if (dierr == kDIErrNone)
        dierr = pGFD->Read(&val[1], 1);
    if (dierr == kDIErrNone)
        dierr = pGFD->Read(&val[2], 1);
    if (dierr == kDIErrNone)
        dierr = pGFD->Read(&val[3], 1);

    *pBuf = val[0] | (long)val[1] << 8 | (long)val[2] << 16 | (long)val[3] << 24;
    return dierr;
}

/*
 * Write a two-byte little-endian value.
 */
DIError
DiskImgLib::WriteShortLE(FILE* fp, unsigned short val)
{
    putc(val, fp);
    putc(val >> 8, fp);
    return kDIErrNone;
}

/*
 * Write a four-byte little-endian value.
 */
DIError
DiskImgLib::WriteLongLE(FILE* fp, unsigned long val)
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
DIError
DiskImgLib::WriteShortLE(GenericFD* pGFD, unsigned short val)
{
    unsigned char buf;

    buf = (unsigned char) val;
    pGFD->Write(&buf, 1);
    buf = val >> 8;
    return pGFD->Write(&buf, 1);
}

/*
 * Write a four-byte little-endian value.
 */
DIError
DiskImgLib::WriteLongLE(GenericFD* pGFD, unsigned long val)
{
    unsigned char buf;

    buf = (unsigned char) val;
    pGFD->Write(&buf, 1);
    buf = (unsigned char) (val >> 8);
    pGFD->Write(&buf, 1);
    buf = (unsigned char) (val >> 16);
    pGFD->Write(&buf, 1);
    buf = (unsigned char) (val >> 24);
    return pGFD->Write(&buf, 1);
}

/*
 * Write a two-byte big-endian value.
 */
DIError
DiskImgLib::WriteShortBE(GenericFD* pGFD, unsigned short val)
{
    unsigned char buf;

    buf = val >> 8;
    pGFD->Write(&buf, 1);
    buf = (unsigned char) val;
    return pGFD->Write(&buf, 1);
}

/*
 * Write a four-byte big-endian value.
 */
DIError
DiskImgLib::WriteLongBE(GenericFD* pGFD, unsigned long val)
{
    unsigned char buf;

    buf = (unsigned char) (val >> 24);
    pGFD->Write(&buf, 1);
    buf = (unsigned char) (val >> 16);
    pGFD->Write(&buf, 1);
    buf = (unsigned char) (val >> 8);
    pGFD->Write(&buf, 1);
    buf = (unsigned char) val;
    return pGFD->Write(&buf, 1);
}


/*
 * Find the filename component of a local pathname.  Uses the fssep passed
 * in.  If the fssep is '\0' (as is the case for DOS 3.3), then the entire
 * pathname is returned.
 *
 * Always returns a pointer to a string; never returns nil.
 */
const char*
DiskImgLib::FilenameOnly(const char* pathname, char fssep)
{
    const char* retstr;
    const char* pSlash;
    char* tmpStr = nil;

    assert(pathname != nil);
    if (fssep == '\0') {
        retstr = pathname;
        goto bail;
    }

    pSlash = strrchr(pathname, fssep);
    if (pSlash == nil) {
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

        if (pSlash == nil) {
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
 * Returns a pointer to the '.' preceding the extension, or nil if no
 * extension was found.
 *
 * We guarantee that there is at least one character after the '.'.
 */
const char*
DiskImgLib::FindExtension(const char* pathname, char fssep)
{
    const char* pFilename;
    const char* pExt;

    /*
     * We have to isolate the filename so that we don't get excited
     * about "/foo.bar/file".
     */
    pFilename = FilenameOnly(pathname, fssep);
    assert(pFilename != nil);
    pExt = strrchr(pFilename, kFilenameExtDelim);

    /* also check for "/blah/foo.", which doesn't count */
    if (pExt != nil && *(pExt+1) != '\0')
        return pExt;

    return nil;
}

/*
 * Like strcpy(), but allocate with new[] instead.
 *
 * If "str" is nil, or "new" fails, this returns nil.
 */
char*
DiskImgLib::StrcpyNew(const char* str)
{
    char* newStr;

    if (str == nil)
        return nil;
    newStr = new char[strlen(str)+1];
    if (newStr != nil)
        strcpy(newStr, str);
    return newStr;
}


#ifdef _WIN32
/*
 * Convert the value from GetLastError() to its DIError counterpart.
 */
DIError
DiskImgLib::LastErrorToDIError(void)
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
        WMSG0("HEY: got ERROR_INVALID_HANDLE!\n");
        return kDIErrInternal;
    case ERROR_NEGATIVE_SEEK:                                       // 131
        WMSG0("HEY: got ERROR_NEGATIVE_SEEK!\n");
        return kDIErrInternal;
    default:
        WMSG2("LastErrorToDIError: not converting 0x%08lx (%ld)\n",
            lastErr, lastErr);
        return kDIErrGeneric;
    }
}

/*
 * Returns "true" if we're running on Win9x (Win95, Win98, WinME), "false"
 * if not (could be WinNT/2K/XP or even Win31 with Win32s).
 */
bool
DiskImgLib::IsWin9x(void)
{
    OSVERSIONINFO osvers;
    BOOL result;

    osvers.dwOSVersionInfoSize = sizeof(osvers);
    result = ::GetVersionEx(&osvers);
    assert(result != FALSE);

    if (osvers.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
        return true;
    else
        return false;
}
#endif
