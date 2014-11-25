/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Pathname utilities.
 */
#include "stdafx.h"
#include "PathName.h"
#include "Util.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <errno.h>

#ifndef S_ISREG
# define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

# ifndef F_OK
#  define F_OK  00
# endif
# ifndef R_OK
#  define R_OK  04
# endif


/*
 * ===========================================================================
 *      Filename utils
 * ===========================================================================
 */

#define kFilenameExtDelim   '.'     /* separates extension from filename */

const WCHAR* PathName::FilenameOnly(const WCHAR* pathname, WCHAR fssep)
{
    const WCHAR* retstr;
    const WCHAR* pSlash;
    WCHAR* tmpStr = NULL;

    ASSERT(pathname != NULL);
    if (fssep == '\0') {
        retstr = pathname;
        goto bail;
    }

    pSlash = wcsrchr(pathname, fssep);  // strrchr
    if (pSlash == NULL) {
        retstr = pathname;      /* whole thing is the filename */
        goto bail;
    }

    pSlash++;
    if (*pSlash == '\0') {
        if (wcslen(pathname) < 2) {
            retstr = pathname;  /* the pathname is just "/"?  Whatever */
            goto bail;
        }

        /* some bonehead put an fssep on the very end; back up before it */
        /* (not efficient, but this should be rare, and I'm feeling lazy) */
        tmpStr = wcsdup(pathname);
        tmpStr[wcslen(pathname)-1] = '\0';
        pSlash = wcsrchr(tmpStr, fssep);

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

const WCHAR* PathName::FindExtension(const WCHAR* pathname, WCHAR fssep)
{
    const WCHAR* pFilename;
    const WCHAR* pExt;

    /*
     * We have to isolate the filename so that we don't get excited
     * about "/foo.bar/file".
     */
    pFilename = FilenameOnly(pathname, fssep);
    ASSERT(pFilename != NULL);
    pExt = wcsrchr(pFilename, kFilenameExtDelim);

    /* also check for "/blah/foo.", which doesn't count */
    if (pExt != NULL && *(pExt+1) != '\0')
        return pExt;

    return NULL;
}

CString PathName::GetFileName(void)
{
    CString str;

    SplitIFN();
    str = fFileName;
    str += fExt;

    {
        const WCHAR* ccp;
        ccp = FilenameOnly(fPathName, '\\');
        if (wcscmp(ccp, str) != 0) {
            LOGI("NOTE: got different filenames '%ls' vs '%ls'",
                ccp, (LPCTSTR) str);
        }
    }

    return str;
}

CString PathName::GetDriveOnly(void)
{
    SplitIFN();

    return fDrive;
}

CString PathName::GetDriveAndPath(void)
{
    CString str;

    SplitIFN();
    str = fDrive;
    str += fDir;

    return str;
}

CString PathName::GetPathOnly(void)
{
    SplitIFN();

    return fDir;
}

CString PathName::GetExtension(void)
{
    SplitIFN();

    {
        const WCHAR* ccp;
        ccp = FindExtension(fPathName, '\\');
        if ((ccp == NULL && wcslen(fExt) > 0) ||
            (ccp != NULL && wcscmp(ccp, fExt) != 0))
        {
            LOGI("NOTE: got different extensions '%ls' vs '%ls'",
                ccp, (LPCTSTR) fExt);
        }
    }

    return fExt;
}

int PathName::SFNToLFN(void)
{
    WCHAR buf[MAX_PATH];
    WIN32_FIND_DATA findFileData;
    HANDLE hFind;
    WCHAR* cp;
    DWORD len;
    CString lfn;
    bool hadEndingSlash = false;

    lfn = "";
    if (fPathName.IsEmpty())
        return 0;

    /* fully expand it */
    len = GetFullPathName(fPathName, NELEM(buf), buf, &cp);
    if (len == 0 || len >= sizeof(buf))
        return -1;
    //LOGI("  FullPathName='%ls'", buf);

    if (buf[len-1] == '\\') {
        hadEndingSlash = true;
        buf[len-1] = '\0';
        len--;
    }

    /*
     * Walk forward in the buffer, passing increasingly-long filenames into
     * FindFirstFile.
     */
    cp = buf;
    while (cp != buf + len) {
        if (*cp == '\\') {
            if (cp == buf) {
                /* just the leading '\'; shouldn't happen after GetFPN? */
                lfn += "\\";
            } else if (cp == buf+2 && *buf != '\\') {
                /* this is probably "C:\", which FindFF doesn't handle */
                *cp = '\0';
                lfn += buf;
                lfn += "\\";
                *cp = '\\';
            } else {
                *cp = '\0';
                hFind = ::FindFirstFile(buf, &findFileData);
                if (hFind == INVALID_HANDLE_VALUE) {
                    DWORD err = ::GetLastError();
                    LOGI("FindFirstFile '%ls' failed, err=%d", buf, err);
                    return -1;
                } else {
                    FindClose(hFind);
                }
                //LOGI("  COMPONENT '%ls' [%ls]", findFileData.cFileName,
                //  findFileData.cAlternateFileName);
                lfn += findFileData.cFileName;
                lfn += "\\";
                *cp = '\\';
            }
        }

        cp++;
    }
    //LOGI("  Interim name = '%ls'", (LPCTSTR) lfn);

    if (*(cp-1) != '\\') {
        /* there was some stuff after the last '\\'; handle it */
        hFind = ::FindFirstFile(buf, &findFileData);
        if (hFind == INVALID_HANDLE_VALUE) {
            DWORD err = ::GetLastError();
            LOGI("FindFirstFile '%ls' failed, err=%d", buf, err);
            return -1;
        } else {
            FindClose(hFind);
        }
        //LOGI("  COMPONENT2 '%ls' [%ls]", findFileData.cFileName,
        //  findFileData.cAlternateFileName);
        lfn += findFileData.cFileName;
    }

    //LOGI("  Almost done = '%ls'", (LPCTSTR) lfn);
    if (hadEndingSlash)
        lfn += "\\";

    fPathName = lfn;

    return 0;
}

CString PathName::GetDescription()
{
    CString     szTypeName;
    SHFILEINFO  sfi = { 0 };

    SHGetFileInfo(fPathName, 0, &sfi, sizeof(SHFILEINFO), SHGFI_TYPENAME);

    szTypeName = sfi.szTypeName;

    return szTypeName;
}

bool PathName::Exists(void)
{
    /*
     * If we use something simple like access(), we will catch all files including
     * the ones in Network Neighborhood.  Using the FindFirstFile stuff avoids
     * the problem, but raises the difficulty of being unable to find simple
     * things like "D:\".
     */

    return (::_waccess(fPathName, 0) != -1);

#if 0
    WIN32_FIND_DATA fd;
    bool result;

    CString szFindPath = fPathName;
    int nSlash = szFindPath.ReverseFind('\\');

    if( nSlash == szFindPath.GetLength()-1)
    {
        szFindPath = szFindPath.Left(nSlash);
    }

    HANDLE hFind = FindFirstFile( szFindPath, &fd );

    if ( hFind != INVALID_HANDLE_VALUE )
    {
        FindClose( hFind );
    }

    result = (hFind != INVALID_HANDLE_VALUE);
#endif

#if 0
    if (::access(fPathName, 0) != -1) {
        /* exists */
        if (!result) {
            ASSERT(false);
        }
    } else {
        /* doesn't exist */
        if (result) {
            ASSERT(false);
        }
    }

    return result;
#endif
}

/*
Problem:
FindFirstFile returns INVALID_HANDLE_VALUE while seeking "C:"

my workaround:
instead of removing the backshash at the end of the path, i add a * to find any file in this folder.

FileName.cpp line 135:
replace
----------------------
    if( nSlash == szFindPath.GetLength()-1)
    {
        szFindPath = szFindPath.Left(nSlash);
    }
----------------------
with
----------------------
    if( nSlash == szFindPath.GetLength()-1)
    {
        szFindPath += "*";
    }
    else
        szFindPath += "\\*";
----------------------
*/

int PathName::Mkdir(const WCHAR* dir)
{
    int err = 0;

    ASSERT(dir != NULL);

    if (_wmkdir(dir) < 0)
        err = errno ? errno : -1;

    return err;
}

int PathName::GetFileInfo(const WCHAR* pathname, struct _stat* psb,
    time_t* pModWhen, bool* pExists, bool* pIsReadable, bool* pIsDirectory)
{
    struct _stat sbuf;
    int cc;

    /*
     * On base network path, e.g. \\webby\fadden, the stat() call fails
     * with ENOENT, but the access() call succeeds.  Not sure if this
     * can happen in other circumstances, so I'm not messing with it
     * for now.
     */
    //{
    //  int cc2 = access(pathname, 0);
    //}

    if (pModWhen != NULL)
        *pModWhen = (time_t) -1;
    if (pExists != NULL)
        *pExists = false;
    if (pIsReadable != NULL)
        *pIsReadable = false;
    if (pIsDirectory != NULL)
        *pIsDirectory = false;

    cc = _wstat(pathname, &sbuf);
    if (psb != NULL)
        *psb = sbuf;
    if (cc != 0) {
        if (errno == ENOENT) {
            if (pExists != NULL)
                *pExists = false;
            return 0;
        } else
            return errno;
    }

    if (pExists != NULL)
        *pExists = true;

    if (pIsDirectory != NULL && S_ISDIR(sbuf.st_mode))
        *pIsDirectory = true;
    if (pModWhen != NULL)
        *pModWhen = sbuf.st_mtime;

    if (pIsReadable != NULL) {
        /*
         * Test if we can read this file.  How do we do that?  The easy but
         * slow way is to call access(2), the harder way is to figure out
         * what user/group we are and compare the appropriate file mode.
         */
        if (_waccess(pathname, R_OK) < 0)
            *pIsReadable = false;
        else
            *pIsReadable = true;
    }

    return 0;
}

int PathName::CheckFileStatus(struct _stat* psb, bool* pExists, bool* pIsReadable,
    bool* pIsDir)
{
    return GetFileInfo(fPathName, psb, NULL, pExists, pIsReadable, pIsDir);
}

time_t PathName::GetModWhen(void)
{
    time_t when;

    if (GetFileInfo(fPathName, NULL, &when, NULL, NULL, NULL) != 0)
        return (time_t) -1;

    return when;
}

int PathName::SetModWhen(time_t when)
{
    struct _utimbuf utbuf;

    if (when == (time_t) -1 || when == kDateNone || when == kDateInvalid) {
        LOGI("NOTE: not setting invalid date (%I64d)", (long long) when);
        return 0;
    }

    utbuf.actime = utbuf.modtime = when;

    if (_wutime(fPathName, &utbuf) < 0)
        return errno;

    return 0;
}

int PathName::CreateSubdirIFN(const WCHAR* pathStart, const WCHAR* pathEnd,
    WCHAR fssep)
{
    int err = 0;
    WCHAR* tmpBuf = NULL;
    bool isDirectory;
    bool exists;

    ASSERT(pathStart != NULL);
    ASSERT(pathEnd != NULL);
    ASSERT(fssep != '\0');

    /* pathStart might have whole path, but we only want up to "pathEnd" */
    tmpBuf = wcsdup(pathStart);
    tmpBuf[pathEnd - pathStart +1] = '\0';

    err = GetFileInfo(tmpBuf, NULL, NULL, &exists, NULL, &isDirectory);
    if (err != 0) {
        LOGI("  Could not get file info for '%ls'", tmpBuf);
        goto bail;
    } else if (!exists) {
        /* dir doesn't exist; move up a level and check parent */
        pathEnd = wcsrchr(tmpBuf, fssep);
        if (pathEnd != NULL) {
            pathEnd--;
            ASSERT(pathEnd >= tmpBuf);
            err = CreateSubdirIFN(tmpBuf, pathEnd, fssep);
            if (err != 0)
                goto bail;
        }

        /* parent is taken care of; create this one */
        err = Mkdir(tmpBuf);
        if (err != 0)
            goto bail;
    } else {
        /* file does exist, make sure it's a directory */
        if (!isDirectory) {
            LOGI("Existing file '%ls' is not a directory", tmpBuf);
            err = ENOTDIR;
            goto bail;
        }
    }

bail:
    free(tmpBuf);
    return err;
}

int PathName::CreatePathIFN(void)
{
    int err = 0;
    CString pathName(fPathName);
    WCHAR* pathStart;
    const WCHAR* pathEnd;

    ASSERT(fFssep != '\0');

    pathStart = pathName.GetBuffer(1);
    /* BAD: network paths begin with "\\", not a drive letter */
//  if (pathStart[0] == fFssep)
//      pathStart++;

    /* remove trailing fssep */
    if (pathStart[wcslen(pathStart)-1] == fFssep)
        pathStart[wcslen(pathStart)-1] = '\0';

    /* work around bug in Win32 strrchr */
    if (pathStart[0] == '\0' || pathStart[1] == '\0') {
        err = EINVAL;
        goto bail;
    }

    pathEnd = wcsrchr(pathStart, fFssep);
    if (pathEnd == NULL) {
        /* no subdirectory components found */
        goto bail;
    }
    pathEnd--;  // back up past the fssep

    ASSERT(pathEnd >= pathStart);
    if (pathEnd - pathStart < 0) {
        err = EINVAL;
        goto bail;
    }

    /*
     * Special-case the root directory, e.g. "C:\", which needs the final
     * slash to be recognized by Windows file calls.
     */
    if (pathEnd - pathStart == 1 && pathStart[1] == ':' &&
        (toupper(pathStart[0]) >= 'A' && toupper(pathStart[0]) <= 'Z'))
    {
        pathEnd++;  // put it back on
    }

    /*
     * Test to determine which directories exist.  The most likely case
     * is that some or all of the components have already been created,
     * so we start with the last one and work backward.
     */
    err = CreateSubdirIFN(pathStart, pathEnd, fFssep);
    /* fall through with err */

bail:
    pathName.ReleaseBuffer();
    return err;
}
