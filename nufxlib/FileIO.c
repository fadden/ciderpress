/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Operations on output (i.e. non-archive) files, largely system-specific.
 * Portions taken from NuLib, including some code that Devin Reade worked on.
 *
 * It could be argued that "create file" should be a callback function,
 * since it is so heavily system-specific, and most of the other
 * system dependencies are handled by the application rather than the
 * NuFX library.  It would also provide a cleaner solution for renaming
 * extracted files.  However, the goal of the library is to do the work
 * for the application, not the other way around; and while it might be
 * nice to offload all direct file handling on the application, it
 * complicates rather than simplifies the interface.
 */
#include "NufxLibPriv.h"

#ifdef MAC_LIKE
# include <Carbon/Carbon.h>
#endif

/*
 * For systems (e.g. Visual C++ 6.0) that don't have these standard values.
 */
#ifndef S_IRUSR
# define S_IRUSR    0400
# define S_IWUSR    0200
# define S_IXUSR    0100
# define S_IRWXU    (S_IRUSR|S_IWUSR|S_IXUSR)
# define S_IRGRP    (S_IRUSR >> 3)
# define S_IWGRP    (S_IWUSR >> 3)
# define S_IXGRP    (S_IXUSR >> 3)
# define S_IRWXG    (S_IRWXU >> 3)
# define S_IROTH    (S_IRGRP >> 3)
# define S_IWOTH    (S_IWGRP >> 3)
# define S_IXOTH    (S_IXGRP >> 3)
# define S_IRWXO    (S_IRWXG >> 3)
#endif
#ifndef S_ISREG
# define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
# define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

/*
 * ===========================================================================
 *      DateTime conversions
 * ===========================================================================
 */

/*
 * Dates and times in a NuFX archive are always considered to be in the
 * local time zone.  The use of GMT time stamps would have been more
 * appropriate for an archive, but local time works well enough.
 *
 * Regarding Y2K on the Apple II:
 *
 * Dave says P8 drivers should return year values in the range 0..99, where
 * 40..99 = 1940..1999, and 0..39 = 2000..2039.  Year values 100..127 should
 * never be used.  For ProDOS 8, the year 2000 is "00".
 *
 * The IIgs ReadTimeHex call uses "year minus 1900".  For GS/OS, the year
 * 2000 is "100".
 *
 * The NuFX file type note says the archive format should work like
 * The IIgs ReadTimeHex call, which uses "year minus 1900" as its
 * format.  GS/ShrinkIt v1.1 uses the IIgs date calls, and so stores the
 * year 2000 as "100".  P8 ShrinkIt v3.4 uses the P8 mapping, and stores
 * it as "0".  Neither really quite understands what the other is doing.
 *
 * For our purposes, we will follow the NuFX standard and emit "100"
 * for the year 2000, but will accept and understand "0" as well.
 */


#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
/*
 * Convert from local time in a NuDateTime struct to GMT seconds since 1970.
 *
 * If the conversion is invalid, "*pWhen" is set to zero.
 */
static void
Nu_DateTimeToGMTSeconds(const NuDateTime* pDateTime, time_t* pWhen)
{
    struct tm tmbuf;
    time_t when;

    Assert(pDateTime != nil);
    Assert(pWhen != nil);

    tmbuf.tm_sec = pDateTime->second;
    tmbuf.tm_min = pDateTime->minute;
    tmbuf.tm_hour = pDateTime->hour;
    tmbuf.tm_mday = pDateTime->day +1;
    tmbuf.tm_mon = pDateTime->month;
    tmbuf.tm_year = pDateTime->year;
    if (pDateTime->year < 40)
        tmbuf.tm_year += 100;   /* P8 uses 0-39 for 2000-2039 */
    tmbuf.tm_wday = 0;
    tmbuf.tm_yday = 0;
    tmbuf.tm_isdst = -1;        /* let it figure DST and time zone */

    #if defined(HAVE_MKTIME)
    when = mktime(&tmbuf);
    #elif defined(HAVE_TIMELOCAL)
    when = timelocal(&tmbuf);
    #else
    # error "need time converter"
    #endif

    if (when == (time_t) -1)
        *pWhen = 0;
    else
        *pWhen = when;
}

/*
 * Convert from GMT seconds since 1970 to local time in a NuDateTime struct.
 */
static void
Nu_GMTSecondsToDateTime(const time_t* pWhen, NuDateTime *pDateTime)
{
    struct tm* ptm;

    Assert(pWhen != nil);
    Assert(pDateTime != nil);

    #if defined(HAVE_LOCALTIME_R) && defined(USE_REENTRANT_CALLS)
    struct tm res;
    ptm = localtime_r(pWhen, &res);
    #else
    /* NOTE: not thread-safe */
    ptm = localtime(pWhen);
    #endif
    pDateTime->second = ptm->tm_sec;
    pDateTime->minute = ptm->tm_min;
    pDateTime->hour = ptm->tm_hour;
    pDateTime->day = ptm->tm_mday -1;
    pDateTime->month = ptm->tm_mon;
    pDateTime->year = ptm->tm_year;
    pDateTime->extra = 0;
    pDateTime->weekDay = ptm->tm_wday +1;
}
#endif


/*
 * Fill in the current time.
 */
void
Nu_SetCurrentDateTime(NuDateTime* pDateTime)
{
    Assert(pDateTime != nil);

#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
    {
        time_t now = time(nil);
        Nu_GMTSecondsToDateTime(&now, pDateTime);
    }
#else
    #error "Port this"
#endif
}


/*
 * Returns "true" if "pWhen1" is older than "pWhen2".  Returns false if
 * "pWhen1" is the same age or newer than "pWhen2".
 *
 * On systems with mktime, it would be straightforward to convert the dates
 * to time in seconds, and compare them that way.  However, I don't want
 * to rely on that function too heavily, so we just compare fields.
 */
Boolean
Nu_IsOlder(const NuDateTime* pWhen1, const NuDateTime* pWhen2)
{
    long result, year1, year2;

    /* adjust for P8 ShrinkIt Y2K problem */
    year1 = pWhen1->year;
    if (year1 < 40)
        year1 += 100;
    year2 = pWhen2->year;
    if (year2 < 40)
        year2 += 100;

    result = year1 - year2;
    if (!result)
        result = pWhen1->month - pWhen2->month;
    if (!result)
        result = pWhen1->day - pWhen2->day;
    if (!result)
        result = pWhen1->hour - pWhen2->hour;
    if (!result)
        result = pWhen1->minute - pWhen2->minute;
    if (!result)
        result = pWhen1->second - pWhen2->second;

    if (result < 0)
        return true;
    return false;
}


/*
 * ===========================================================================
 *      Get/set file info
 * ===========================================================================
 */

/*
 * System-independent (mostly) file info struct.
 */
typedef struct NuFileInfo {
    Boolean     isValid;    /* init to "false", set "true" after we get data */

    Boolean     isRegularFile;  /* is this a regular file? */
    Boolean     isDirectory;    /* is this a directory? */

    ulong       dataEof;
    ulong       rsrcEof;

    ulong       fileType;
    ulong       auxType;
    NuDateTime  modWhen;
    mode_t      unixMode;       /* UNIX-style permissions */
} NuFileInfo;

#define kDefaultFileType    0       /* "NON" */
#define kDefaultAuxType     0       /* $0000 */


/*
 * Determine whether the record has both data and resource forks.
 */
static Boolean
Nu_IsForkedFile(NuArchive* pArchive, const NuRecord* pRecord)
{
    const NuThread* pThread;
    NuThreadID threadID;
    Boolean gotData, gotRsrc;
    int i;

    gotData = gotRsrc = false;

    for (i = 0; i < (int)pRecord->recTotalThreads; i++) {
        pThread = Nu_GetThread(pRecord, i);
        Assert(pThread != nil);

        threadID = NuMakeThreadID(pThread->thThreadClass,pThread->thThreadKind);
        if (threadID == kNuThreadIDDataFork)
            gotData = true;
        else if (threadID == kNuThreadIDRsrcFork)
            gotRsrc = true;
    }

    if (gotData && gotRsrc)
        return true;
    else
        return false;
}


/*
 * Get the file info into a NuFileInfo struct.  Fields which are
 * inappropriate for the current system are set to default values.
 */
static NuError
Nu_GetFileInfo(NuArchive* pArchive, const char* pathname,
    NuFileInfo* pFileInfo)
{
    NuError err = kNuErrNone;
    Assert(pArchive != nil);
    Assert(pathname != nil);
    Assert(pFileInfo != nil);

    pFileInfo->isValid = false;

#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
    {
        struct stat sbuf;
        int cc;

        cc = stat(pathname, &sbuf);
        if (cc) {
            if (errno == ENOENT)
                err = kNuErrFileNotFound;
            else
                err = kNuErrFileStat;
            goto bail;
        }

        pFileInfo->isRegularFile = false;
        if (S_ISREG(sbuf.st_mode))
            pFileInfo->isRegularFile = true;
        pFileInfo->isDirectory = false;
        if (S_ISDIR(sbuf.st_mode))
            pFileInfo->isDirectory = true;

        /* BUG: should check for 32-bit overflow from 64-bit off_t */
        pFileInfo->dataEof = sbuf.st_size;
        pFileInfo->rsrcEof = 0;
        pFileInfo->fileType = kDefaultFileType;
        pFileInfo->auxType = kDefaultAuxType;
# if defined(MAC_LIKE)
        if (!pFileInfo->isDirectory) {
            char path[4096];
            struct stat res_sbuf;
            OSErr result;
            OSType fileType, creator;
            FSCatalogInfo catalogInfo;
            FSRef ref;
            unsigned long proType, proAux;
            
            strcpy(path, pathname);
            strcat(path, "/rsrc");
            cc = stat(path, &res_sbuf);
            if (cc) {
                if (!errno) {
                    pFileInfo->rsrcEof = res_sbuf.st_size;
                }
            }
            
            result = FSPathMakeRef(pathname, &ref, NULL);
            if (!result) {
                result = FSGetCatalogInfo(&ref, kFSCatInfoFinderInfo, &catalogInfo,
                                NULL, NULL, NULL);
                if (!result) {
                    fileType = ((FileInfo *) &catalogInfo.finderInfo)->fileType;
                    creator = ((FileInfo *) &catalogInfo.finderInfo)->fileCreator;
                    
                    /* This actually is probably more efficient than a weird table, for
                       so few values */
                    
                    switch(creator) {
                        case 'pdos':
                            if (fileType == 'PSYS') {
                                proType = 0xFF;
                                proAux = 0x0000;
                            } else if (fileType == 'PS16') {
                                proType = 0xB3;
                                proAux = 0x0000;
                            } else {
                                if (((fileType >> 24) & 0xFF) == 'p') {
                                    proType = (fileType >> 16) & 0xFF;
                                    proAux = fileType & 0xFFFF;
                                } else {
                                    proType = 0x00;
                                    proAux = 0x0000;
                                }
                            }
                            break;
                        case 'dCpy':
                            if (fileType == 'dImg') {
                                proType = 0xE0;
                                proAux = 0x0005;
                            } else {
                                proType = 0x00;
                                proAux = 0x0000;
                            }
                            break;
                        default:
                            switch(fileType) {
                                case 'BINA':
                                    proType = 0x06;
                                    proAux = 0x0000;
                                    break;
                                case 'TEXT':
                                    proType = 0x04;
                                    proAux = 0x0000;
                                    break;
                                case 'MIDI':
                                    proType = 0xD7;
                                    proAux = 0x0000;
                                    break;
                                case 'AIFF':
                                    proType = 0xD8;
                                    proAux = 0x0000;
                                    break;
                                case 'AIFC':
                                    proType = 0xD8;
                                    proAux = 0x0001;
                                    break;
                                default:
                                    proType = 0x00;
                                    proAux = 0x0000;
                                    break;
                            }
                            break;
                    }
                    pFileInfo->fileType = proType;
                    pFileInfo->auxType = proAux;
                }
            }
        }
# endif
        Nu_GMTSecondsToDateTime(&sbuf.st_mtime, &pFileInfo->modWhen);
        pFileInfo->unixMode = sbuf.st_mode;
        pFileInfo->isValid = true;
    }
#else
    #error "Port this"
#endif

bail:
    return err;
}


/*
 * Determine whether a specific fork in the file exists.
 *
 * On systems that don't support forked files, the "checkRsrcFork" argument
 * is ignored.  If forked files are supported, and we are extracting a
 * file with data and resource forks, we only claim it exists if it has
 * nonzero length.
 */
static NuError
Nu_FileForkExists(NuArchive* pArchive, const char* pathname,
    Boolean isForkedFile, Boolean checkRsrcFork, Boolean* pExists,
    NuFileInfo* pFileInfo)
{
    NuError err = kNuErrNone;

    Assert(pArchive != nil);
    Assert(pathname != nil);
    Assert(checkRsrcFork == true || checkRsrcFork == false);
    Assert(pExists != nil);
    Assert(pFileInfo != nil);

#if defined(MAC_LIKE)
    /*
     * On Mac OS X, we do much like on Unix, but we do need to look for
     * a resource fork.
     */
    *pExists = true;
    if (!checkRsrcFork) {
        /*
         * Check the data fork.
         */
        Assert(pArchive->lastFileCreated == nil);
        err = Nu_GetFileInfo(pArchive, pathname, pFileInfo);
        if (err == kNuErrFileNotFound) {
            err = kNuErrNone;
            *pExists = false;
        }
/*      DBUG(("Data fork %s: %d (err %d)\n", pathname, *pExists, err));*/
    } else {
        /*
         * Check the resource fork.
         */
        char path[4096];
        strncpy(path, pathname, 4089);
        strcat(path, "/rsrc");
        err = Nu_GetFileInfo(pArchive, path, pFileInfo);
        if (err == kNuErrFileNotFound) {
            err = kNuErrNone;
            *pExists = false;
        } else if (!err && !pFileInfo->rsrcEof) {
            err = kNuErrNone;
            *pExists = false;
        }
/*      DBUG(("Rsrc fork %s: %d (err %d)\n", path, *pExists, err));*/
    }
    
#elif defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
    /*
     * On Unix and Windows we ignore "isForkedFile" and "checkRsrcFork".
     * The file must not exist at all.
     */
    Assert(pArchive->lastFileCreated == nil);

    *pExists = true;
    err = Nu_GetFileInfo(pArchive, pathname, pFileInfo);
    if (err == kNuErrFileNotFound) {
        err = kNuErrNone;
        *pExists = false;
    }

#elif defined(__ORCAC__)
    /*
     * If the file doesn't exist, great.  If it does, and "lastFileCreated"
     * matches up with this one, then we know that it exists because we
     * created it.
     *
     * This is great unless the record has two data forks or something
     * equally dopey, so we check to be sure that the fork we want to
     * put the data into is currently empty.
     *
     * It is possible, though asinine, for a Mac OS or GS/OS extraction
     * program to put the data and resource forks of a record into
     * separate files, so we can't just assume that because we wrote
     * the data fork to file A it is okay for file B to exist.  That's
     * why we compare the pathname instead of just remembering that
     * we already created a file for this record.
     */
    #error "Finish me"

#else
    #error "Port this"
#endif

    return err;
}


/*
 * Set the dates on a file according to what's in the record.
 */
static NuError
Nu_SetFileDates(NuArchive* pArchive, const NuRecord* pRecord,
    const char* pathname)
{
    NuError err = kNuErrNone;

    Assert(pArchive != nil);
    Assert(pRecord != nil);
    Assert(pathname != nil);

#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
    {
        struct utimbuf utbuf;

        /* ignore create time, and set access time equal to mod time */
        Nu_DateTimeToGMTSeconds(&pRecord->recModWhen, &utbuf.modtime);
        utbuf.actime = utbuf.modtime;

        /* only do it if the NuDateTime was valid */
        if (utbuf.modtime) {
            if (utime(pathname, &utbuf) < 0) {
                Nu_ReportError(NU_BLOB, errno,
                    "Unable to set time stamp on '%s'", pathname);
                err = kNuErrFileSetDate;
                goto bail;
            }
        }
    }

#else
    #error "Port this"
#endif

bail:
    return err;
}


/*
 * Returns "true" if the record is locked (in the ProDOS sense).
 *
 *  Bits 31-8    reserved, must be zero
 *  Bit 7 (D)    1 = destroy enabled
 *  Bit 6 (R)    1 = rename enabled
 *  Bit 5 (B)    1 = file needs to be backed up
 *  Bits 4-3     reserved, must be zero
 *  Bit 2 (I)    1 = file is invisible
 *  Bit 1 (W)    1 = write enabled
 *  Bit 0 (R)    1 = read enabled
 *
 * A "locked" file would be 00?00001, "unlocked" 11?00011, with many
 * possible variations.  For our purposes, we treat all files as unlocked
 * unless they match the classic "locked" bit pattern.
 */
static Boolean
Nu_IsRecordLocked(const NuRecord* pRecord)
{
    if (pRecord->recAccess == 0x21L || pRecord->recAccess == 0x01L)
        return true;
    else
        return false;
}

/*
 * Set the file access permissions based on what's in the record.
 *
 * This assumes that the file is currently writable, so we only need
 * to do something if the original file was "locked".
 */
static NuError
Nu_SetFileAccess(NuArchive* pArchive, const NuRecord* pRecord,
    const char* pathname)
{
    NuError err = kNuErrNone;

    Assert(pArchive != nil);
    Assert(pRecord != nil);
    Assert(pathname != nil);

#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
    /* only need to do something if the file was "locked" */
    if (Nu_IsRecordLocked(pRecord)) {
        mode_t mask;

        /* set it to 444, modified by umask */
        mask = umask(0);
        umask(mask);
        //DBUG(("+++ chmod '%s' %03o (mask=%03o)\n", pathname,
        //    (S_IRUSR | S_IRGRP | S_IROTH) & ~mask, mask));
        if (chmod(pathname, (S_IRUSR | S_IRGRP | S_IROTH) & ~mask) < 0) {
            Nu_ReportError(NU_BLOB, errno,
                "unable to set access for '%s' to %03o", pathname, (int) mask);
            err = kNuErrFileSetAccess;
            goto bail;
        }
    }

#else
    #error "Port this"
#endif

bail:
    return err;
}


/*
 * ===========================================================================
 *      Create/open an output file
 * ===========================================================================
 */

/*
 * Prepare an existing file for writing.
 *
 * Generally this just involves ensuring that the file is writable.  If
 * this is a convenient place to truncate it, we should do that too.
 */
static NuError
Nu_PrepareForWriting(NuArchive* pArchive, const char* pathname,
    Boolean prepRsrc, NuFileInfo* pFileInfo)
{
    NuError err = kNuErrNone;
#if defined(MAC_LIKE)
    char path[4096];
#endif

    Assert(pArchive != nil);
    Assert(pathname != nil);
    Assert(prepRsrc == true || prepRsrc == false);
    Assert(pFileInfo != nil);

    Assert(pFileInfo->isValid == true);

    /* don't go playing with directories, pipes, etc */
    if (pFileInfo->isRegularFile != true)
        return kNuErrNotRegularFile;

#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
# if defined(MAC_LIKE)
    if (prepRsrc) {
        strcpy(path, pathname);
        strcat(path, "/rsrc");
        pathname = path;
    }
# endif
    if (!(pFileInfo->unixMode & S_IWUSR)) {
        /* make it writable by owner, plus whatever it was before */
        if (chmod(pathname, S_IWUSR | pFileInfo->unixMode) < 0) {
            Nu_ReportError(NU_BLOB, errno,
                "unable to set access for '%s'", pathname);
            err = kNuErrFileSetAccess;
            goto bail;
        }
    }

    return kNuErrNone;

#else
    #error "Port this"
#endif

bail:
    return err;
}


/*
 * Invoke the system-dependent directory creation function.
 */
static NuError
Nu_Mkdir(NuArchive* pArchive, const char* dir)
{
    NuError err = kNuErrNone;

    Assert(pArchive != nil);
    Assert(dir != nil);

#if defined(UNIX_LIKE)
    if (mkdir(dir, S_IRWXU | S_IRGRP|S_IXGRP | S_IROTH|S_IXOTH) < 0) {
        err = errno ? errno : kNuErrDirCreate;
        Nu_ReportError(NU_BLOB, err, "Unable to create dir '%s'", dir);
        goto bail;
    }

#elif defined(WINDOWS_LIKE)
    if (mkdir(dir) < 0) {
        err = errno ? errno : kNuErrDirCreate;
        Nu_ReportError(NU_BLOB, err, "Unable to create dir '%s'", dir);
        goto bail;
    }

#else
    #error "Port this"
#endif

bail:
    return err;
}


/*
 * Create a single subdirectory if it doesn't exist.  If the next-highest
 * subdirectory level doesn't exist either, cut down the pathname and
 * recurse.
 */
static NuError
Nu_CreateSubdirIFN(NuArchive* pArchive, const char* pathStart,
    const char* pathEnd, char fssep)
{
    NuError err = kNuErrNone;
    NuFileInfo fileInfo;
    char* tmpBuf = nil;

    Assert(pArchive != nil);
    Assert(pathStart != nil);
    Assert(pathEnd != nil);
    Assert(fssep != '\0');

    /* pathStart might have whole path, but we only want up to "pathEnd" */
    tmpBuf = strdup(pathStart);
    tmpBuf[pathEnd - pathStart +1] = '\0';

    err = Nu_GetFileInfo(pArchive, tmpBuf, &fileInfo);
    if (err == kNuErrFileNotFound) {
        /* dir doesn't exist; move up a level and check parent */
        pathEnd = strrchr(tmpBuf, fssep);
        if (pathEnd != nil) {
            pathEnd--;
            Assert(pathEnd >= tmpBuf);
            err = Nu_CreateSubdirIFN(pArchive, tmpBuf, pathEnd, fssep);
            BailError(err);
        }

        /* parent is taken care of; create this one */
        err = Nu_Mkdir(pArchive, tmpBuf);
        BailError(err);
    } else if (err != kNuErrNone) {
        goto bail;
    } else {
        /* file does exist, make sure it's a directory */
        Assert(fileInfo.isValid == true);
        if (!fileInfo.isDirectory) {
            err = kNuErrNotDir;
            Nu_ReportError(NU_BLOB, err, "Unable to create path '%s'", tmpBuf);
            goto bail;
        }
    }

bail:
    Nu_Free(pArchive, tmpBuf);
    return err;
}

/*
 * Create subdirectories, if needed.  The paths leading up to the filename
 * in "pathname" will be created.
 *
 * If "pathname" is just a filename, or the set of directories matches
 * the last directory we created, we don't do anything.
 */
static NuError
Nu_CreatePathIFN(NuArchive* pArchive, const char* pathname, char fssep)
{
    NuError err = kNuErrNone;
    const char* pathStart;
    const char* pathEnd;

    Assert(pArchive != nil);
    Assert(pathname != nil);
    Assert(fssep != '\0');

    pathStart = pathname;
    
#if !defined(MAC_LIKE)          /* On the Mac, if it's a full path, treat it like one */
    if (pathname[0] == fssep)
        pathStart++;
#endif

    /* NOTE: not expecting names like "foo/bar/ack/", with terminating fssep */
    pathEnd = strrchr(pathStart, fssep);
    if (pathEnd == nil) {
        /* no subdirectory components found */
        goto bail;
    }
    pathEnd--;

    Assert(pathEnd >= pathStart);
    if (pathEnd - pathStart < 0)
        goto bail;

    /*
     * On some filesystems, strncasecmp would be appropriate here.  However,
     * this is meant solely as an optimization to avoid extra stat() calls,
     * so we want to use the most restrictive case.
     */
    if (pArchive->lastDirCreated &&
        strncmp(pathStart, pArchive->lastDirCreated, pathEnd - pathStart +1) == 0)
    {
        /* we created this one recently, don't do it again */
        goto bail;
    }

    /*
     * Test to determine which directories exist.  The most likely case
     * is that some or all of the components have already been created,
     * so we start with the last one and work backward.
     */
    err = Nu_CreateSubdirIFN(pArchive, pathStart, pathEnd, fssep);
    BailError(err);

bail:
    return err;
}


/*
 * Open the file for writing, possibly truncating it.
 */
static NuError
Nu_OpenFileForWrite(NuArchive* pArchive, const char* pathname,
    Boolean openRsrc, FILE** pFp)
{
#if defined(MAC_LIKE)
    char path[4096];
    if (openRsrc) {
        strcpy(path, pathname);
        strcat(path, "/rsrc");
        pathname = path;
    }
#endif
    *pFp = fopen(pathname, kNuFileOpenWriteTrunc);
    if (*pFp == nil)
        return errno ? errno : -1;
    return kNuErrNone;
}


/*
 * Open an output file and prepare it for writing.
 *
 * There are a number of things to take into consideration, including
 * deal with "file exists" conditions, handling Mac/IIgs file types,
 * coping with resource forks on extended files, and handling the
 * "freshen" option that requires us to only update files that are
 * older than what we have.
 */
NuError
Nu_OpenOutputFile(NuArchive* pArchive, const NuRecord* pRecord,
    const NuThread* pThread, const char* newPathname, char newFssep,
    FILE** pFp)
{
    NuError err = kNuErrNone;
    Boolean exists, isForkedFile, extractingRsrc = false;
    NuFileInfo fileInfo;
    NuErrorStatus errorStatus;
    NuResult result;

    Assert(pArchive != nil);
    Assert(pRecord != nil);
    Assert(pThread != nil);
    Assert(newPathname != nil);
    Assert(pFp != nil);

    /* set up some defaults, in case something goes wrong */
    errorStatus.operation = kNuOpExtract;
    errorStatus.err = kNuErrInternal;
    errorStatus.sysErr = 0;
    errorStatus.message = nil;
    errorStatus.pRecord = pRecord;
    errorStatus.pathname = newPathname;
    errorStatus.origPathname = nil;
    errorStatus.filenameSeparator = newFssep;
    /*errorStatus.origArchiveTouched = false;*/
    errorStatus.canAbort = true;
    errorStatus.canRetry = true;
    errorStatus.canIgnore = false;
    errorStatus.canSkip = true;
    errorStatus.canRename = true;
    errorStatus.canOverwrite = true;

    /* decide if this is a forked file (i.e. has *both* forks) */
    isForkedFile = Nu_IsForkedFile(pArchive, pRecord);

    /* decide if we're extracting a resource fork */
    if (NuMakeThreadID(pThread->thThreadClass, pThread->thThreadKind) ==
        kNuThreadIDRsrcFork)
    {
        extractingRsrc = true;
    }

    /*
     * Determine whether the file and fork already exists.  If the file
     * is one we just created, and the fork we want to write to is
     * empty, this will *not* set "exists".
     */
    fileInfo.isValid = false;
    err = Nu_FileForkExists(pArchive, newPathname, isForkedFile,
            extractingRsrc, &exists, &fileInfo);
    BailError(err);

    if (exists) {
        Assert(fileInfo.isValid == true);

        /*
         * The file exists when it shouldn't.  Decide what to do, based
         * on the options configured by the application.
         */
         
        /*
         * Start by checking to see if we're willing to overwrite older files.
         * If not, see if the application wants to rename the file, or force
         * the overwrite.  Most likely they'll just want to skip it.
         */
        if ((pArchive->valOnlyUpdateOlder) &&
            !Nu_IsOlder(&fileInfo.modWhen, &pRecord->recModWhen))
        {
            if (pArchive->errorHandlerFunc != nil) {
                errorStatus.err = kNuErrNotNewer;
                result = (*pArchive->errorHandlerFunc)(pArchive, &errorStatus);

                switch (result) {
                case kNuAbort:
                    err = kNuErrAborted;
                    goto bail;
                case kNuRetry:
                case kNuRename:
                    err = kNuErrRename;
                    goto bail;
                case kNuSkip:
                    err = kNuErrSkipped;
                    goto bail;
                case kNuOverwrite:
                    break;  /* fall back into main code */
                case kNuIgnore:
                default:
                    err = kNuErrSyntax;
                    Nu_ReportError(NU_BLOB, err,
                        "Wasn't expecting result %d here", result);
                    goto bail;
                }
            } else {
                err = kNuErrNotNewer;
                goto bail;
            }
        }

        /* If they "might" allow overwrites, and they have an error-handling
         * callback defined, call that to find out what they want to do
         * here.  Options include skipping the file, overwriting the file,
         * and extracting to a different file.
         */
        if (pArchive->valHandleExisting == kNuMaybeOverwrite) {
            if (pArchive->errorHandlerFunc != nil) {
                errorStatus.err = kNuErrFileExists;
                result = (*pArchive->errorHandlerFunc)(pArchive, &errorStatus);

                switch (result) {
                case kNuAbort:
                    err = kNuErrAborted;
                    goto bail;
                case kNuRetry:
                case kNuRename:
                    err = kNuErrRename;
                    goto bail;
                case kNuSkip:
                    err = kNuErrSkipped;
                    goto bail;
                case kNuOverwrite:
                    break;  /* fall back into main code */
                case kNuIgnore:
                default:
                    err = kNuErrSyntax;
                    Nu_ReportError(NU_BLOB, err,
                        "Wasn't expecting result %d here", result);
                    goto bail;
                }
            } else {
                /* no error handler, return an error to the caller */
                err = kNuErrFileExists;
                goto bail;
            }
        } else if (pArchive->valHandleExisting == kNuNeverOverwrite) {
            err = kNuErrSkipped;
            goto bail;
        }
    } else {
        /*
         * The file doesn't exist.  If we're doing a "freshen" from the
         * archive, we don't want to create a new file, so we return an
         * error to the user instead.  However, we give the application
         * a chance to straighten things out.  Most likely they'll just
         * return kNuSkip.
         */
        if (pArchive->valHandleExisting == kNuMustOverwrite) {
            DBUG(("+++ can't freshen nonexistent file '%s'\n", newPathname));
            if (pArchive->errorHandlerFunc != nil) {
                errorStatus.err = kNuErrDuplicateNotFound;

                /* give them a chance to rename */
                result = (*pArchive->errorHandlerFunc)(pArchive, &errorStatus);
                switch (result) {
                case kNuAbort:
                    err = kNuErrAborted;
                    goto bail;
                case kNuRetry:
                case kNuRename:
                    err = kNuErrRename;
                    goto bail;
                case kNuSkip:
                    err = kNuErrSkipped;
                    goto bail;
                case kNuOverwrite:
                    break;  /* fall back into main code */
                case kNuIgnore:
                default:
                    err = kNuErrSyntax;
                    Nu_ReportError(NU_BLOB, err,
                        "Wasn't expecting result %d here", result);
                    goto bail;
                }
            } else {
                /* no error handler, return an error to the caller */
                err = kNuErrDuplicateNotFound;
                goto bail;
            }
        }
    }

    Assert(err == kNuErrNone);

    /*
     * After the above, if the file exists then we need to prepare it for
     * writing.  On some systems -- notably those with forked files -- it
     * may be easiest to delete the entire file and start over.  On
     * simpler systems, an (optional) chmod followed by an open that
     * truncates the file should be sufficient.
     *
     * If the file didn't exist, we need to be sure that the path leading
     * up to its eventual location exists.  This might require creating
     * several directories.  We distinguish the case of "file isn't there"
     * from "file is there but fork isn't" by seeing if we were able to
     * get valid file info.
     */
    if (exists) {
        Assert(fileInfo.isValid == true);
        err = Nu_PrepareForWriting(pArchive, newPathname, extractingRsrc,
                &fileInfo);
        BailError(err);
    } else if (!fileInfo.isValid) {
        err = Nu_CreatePathIFN(pArchive, newPathname, newFssep);
        BailError(err);
    }

    /*
     * Open sesame.
     */
    err = Nu_OpenFileForWrite(pArchive, newPathname, extractingRsrc, pFp);
    BailError(err);


#if defined(HAS_RESOURCE_FORKS)
    pArchive->lastFileCreated = newPathname;
#endif

bail:
    if (err != kNuErrNone) {
        if (err != kNuErrSkipped && err != kNuErrRename &&
            err != kNuErrFileExists)
        {
            Nu_ReportError(NU_BLOB, err, "Unable to open '%s'%s",
                newPathname, extractingRsrc ? " (rsrc fork)" : "");
        }
    }
    return err;
}


/*
 * Close the output file, adjusting the modification date and access
 * permissions as needed.
 *
 * On GS/OS and Mac OS, we may need to set the file type here, depending on
 * how much we managed to do when the file was first created.  IIRC,
 * the GS/OS Open call should allow setting the file type.
 *
 * BUG: on GS/OS, if we set the file access after writing the data fork,
 * we may not be able to open the same file for writing the rsrc fork.
 * We can't suppress setting the access permissions, because we don't know
 * if the application will want to write both forks to the same file, or
 * for that matter will want to write the resource fork at all.  Looks
 * like we will have to be smart enough to reset the access permissions
 * when writing a rsrc fork to a file with just a data fork.  This isn't
 * quite right, but it's close enough.
 */
NuError
Nu_CloseOutputFile(NuArchive* pArchive, const NuRecord* pRecord, FILE* fp,
    const char* pathname)
{
    NuError err;

    Assert(pArchive != nil);
    Assert(pRecord != nil);
    Assert(fp != nil);

    fclose(fp);

    err = Nu_SetFileDates(pArchive, pRecord, pathname);
    BailError(err);

    err = Nu_SetFileAccess(pArchive, pRecord, pathname);
    BailError(err);

#ifdef MAC_LIKE
    OSErr result;
    OSType fileType;
    FSCatalogInfo catalogInfo;
    FSRef ref;
    
    result = FSPathMakeRef(pathname, &ref, NULL);
    BailError(result);
    
    result = FSGetCatalogInfo(&ref, kFSCatInfoNodeFlags|kFSCatInfoFinderInfo, &catalogInfo,
                    NULL, NULL, NULL);
    if (result) {
        BailError(kNuErrFileStat);
    }
    
    /* Build the type and creator */
    
    fileType = 0x70000000;
    fileType |= (pRecord->recFileType & 0xFF) << 16;
    fileType |= (pRecord->recExtraType & 0xFFFF);
    
    /* Set the type and creator */
        
    ((FileInfo *) &catalogInfo.finderInfo)->fileType = fileType;
    ((FileInfo *) &catalogInfo.finderInfo)->fileCreator = 'pdos';
    result = FSSetCatalogInfo(&ref, kFSCatInfoFinderInfo, &catalogInfo);
    BailError(result);
#endif

bail:
    return kNuErrNone;
}

/*
 * ===========================================================================
 *      Open an input file
 * ===========================================================================
 */

/*
 * Open the file for reading, in "binary" mode when necessary.
 */
static NuError
Nu_OpenFileForRead(NuArchive* pArchive, const char* pathname,
    Boolean openRsrc, FILE** pFp)
{
    *pFp = fopen(pathname, kNuFileOpenReadOnly);
    if (*pFp == nil)
        return errno ? errno : -1;
    return kNuErrNone;
}


/*
 * Open an input file and prepare it for reading.
 *
 * If the file can't be found, we give the application an opportunity to
 * skip the absent file, retry, or abort the whole thing.
 */
NuError
Nu_OpenInputFile(NuArchive* pArchive, const char* pathname,
    Boolean openRsrc, FILE** pFp)
{
    NuError err = kNuErrNone;
    NuError openErr = kNuErrNone;
    NuErrorStatus errorStatus;
    NuResult result;

    Assert(pArchive != nil);
    Assert(pathname != nil);
    Assert(pFp != nil);

#if defined(MAC_LIKE)
    char path[4096];
    if (openRsrc) {
        strcpy(path, pathname);
        strcat(path, "/rsrc");
        pathname = path;
    }
#endif

retry:
    /*
     * Open sesame.
     */
    err = Nu_OpenFileForRead(pArchive, pathname, openRsrc, pFp);
    if (err == kNuErrNone)  /* success! */
        goto bail;

    if (err == ENOENT)
        openErr = kNuErrFileNotFound;

    if (pArchive->errorHandlerFunc != nil) {
        errorStatus.operation = kNuOpAdd;
        errorStatus.err = openErr;
        errorStatus.sysErr = 0;
        errorStatus.message = nil;
        errorStatus.pRecord = nil;
        errorStatus.pathname = pathname;
        errorStatus.origPathname = nil;
        errorStatus.filenameSeparator = '\0';
        /*errorStatus.origArchiveTouched = false;*/
        errorStatus.canAbort = true;
        errorStatus.canRetry = true;
        errorStatus.canIgnore = false;
        errorStatus.canSkip = true;
        errorStatus.canRename = false;
        errorStatus.canOverwrite = false;

        DBUG(("--- invoking error handler function\n"));
        result = (*pArchive->errorHandlerFunc)(pArchive, &errorStatus);

        switch (result) {
        case kNuAbort:
            err = kNuErrAborted;
            goto bail;
        case kNuRetry:
            goto retry;
        case kNuSkip:
            err = kNuErrSkipped;
            goto bail;
        case kNuRename:
        case kNuOverwrite:
        case kNuIgnore:
        default:
            err = kNuErrSyntax;
            Nu_ReportError(NU_BLOB, err,
                "Wasn't expecting result %d here", result);
            goto bail;
        }
    } else {
        DBUG(("+++ no error callback in OpenInputFile\n"));
    }

bail:
    if (err == kNuErrNone) {
        Assert(*pFp != nil);
    } else {
        if (err != kNuErrSkipped && err != kNuErrRename &&
            err != kNuErrFileExists)
        {
            Nu_ReportError(NU_BLOB, err, "Unable to open '%s'%s",
                pathname, openRsrc ? " (rsrc fork)" : "");
        }
    }
    return err;
}


/*
 * ===========================================================================
 *      Delete and rename files
 * ===========================================================================
 */

/*
 * Delete a file.
 */
NuError
Nu_DeleteFile(const char* pathname)
{
#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
    int cc;

    DBUG(("--- Deleting '%s'\n", pathname));

    cc = unlink(pathname);
    if (cc < 0)
        return errno ? errno : -1;
    else
        return kNuErrNone;
#else
    #error "Port this"
#endif
}

/*
 * Rename a file from "fromPath" to "toPath".
 */
NuError
Nu_RenameFile(const char* fromPath, const char* toPath)
{
#if defined(UNIX_LIKE) || defined(WINDOWS_LIKE)
    int cc;

    DBUG(("--- Renaming '%s' to '%s'\n", fromPath, toPath));

    cc = rename(fromPath, toPath);
    if (cc < 0)
        return errno ? errno : -1;
    else
        return kNuErrNone;
#else
    #error "Port this"
#endif
}


/*
 * ===========================================================================
 *      NuError wrappers for std functions
 * ===========================================================================
 */

/*
 * Wrapper for ftell().
 */
NuError
Nu_FTell(FILE* fp, long* pOffset)
{
    Assert(fp != nil);
    Assert(pOffset != nil);

    errno = 0;
    *pOffset = ftell(fp);
    if (*pOffset < 0) {
        Nu_ReportError(NU_NILBLOB, errno, "file ftell failed");
        return errno ? errno : kNuErrFileSeek;
    }
    return kNuErrNone;
}

/*
 * Wrapper for fseek().
 */
NuError
Nu_FSeek(FILE* fp, long offset, int ptrname)
{
    Assert(fp != nil);
    Assert(ptrname == SEEK_SET || ptrname == SEEK_CUR || ptrname == SEEK_END);

    errno = 0;
    if (fseek(fp, offset, ptrname) < 0) {
        Nu_ReportError(NU_NILBLOB, errno,
            "file fseek(%ld, %d) failed", offset, ptrname);
        return errno ? errno : kNuErrFileSeek;
    }
    return kNuErrNone;
}

/*
 * Wrapper for fread().  Note the arguments resemble read(2) rather than the
 * slightly silly ones used by fread(3S).
 */
NuError
Nu_FRead(FILE* fp, void* buf, size_t nbyte)
{
    size_t result;

    errno = 0;
    result = fread(buf, nbyte, 1, fp);
    if (result != 1)
        return errno ? errno : kNuErrFileRead;
    return kNuErrNone;
}

/*
 * Wrapper for fwrite().  Note the arguments resemble write(2) rather than the
 * slightly silly ones used by fwrite(3S).
 */
NuError
Nu_FWrite(FILE* fp, const void* buf, size_t nbyte)
{
    size_t result;

    errno = 0;
    result = fwrite(buf, nbyte, 1, fp);
    if (result != 1)
        return errno ? errno : kNuErrFileWrite;
    return kNuErrNone;
}

/*
 * ===========================================================================
 *      Misc functions
 * ===========================================================================
 */

/*
 * Copy a section from one file to another.
 */
NuError
Nu_CopyFileSection(NuArchive* pArchive, FILE* dstFp, FILE* srcFp, long length)
{
    NuError err;
    long readLen;

    Assert(pArchive != nil);
    Assert(dstFp != nil);
    Assert(srcFp != nil);
    Assert(length >= 0);    /* can be == 0, e.g. empty data fork from HFS */

    /* nice big buffer, for speed... could use getc/putc for simplicity */
    err = Nu_AllocCompressionBufferIFN(pArchive);
    BailError(err);

    DBUG(("+++ Copying %ld bytes\n", length));

    while (length) {
        readLen = length > kNuGenCompBufSize ?  kNuGenCompBufSize : length;

        err = Nu_FRead(srcFp, pArchive->compBuf, readLen);
        if (err != kNuErrNone) {
            Nu_ReportError(NU_BLOB, err,
                    "Nu_FRead failed while copying file section "
                    "(fp=0x%08lx, readLen=%ld, length=%ld, err=%d)\n",
                (long) srcFp, readLen, length, err);
            goto bail;
        }
        err = Nu_FWrite(dstFp, pArchive->compBuf, readLen);
        BailError(err);

        length -= readLen;
    }

bail:
    return err;
}


/*
 * Find the length of an open file.
 *
 * On UNIX it would be easier to just call fstat(), but fseek is portable.
 *
 * Only useful for files < 2GB in size.
 *
 * (pArchive is only used for BailError message reporting, so it's okay
 * to call here with a nil pointer if the archive isn't open yet.)
 */
NuError
Nu_GetFileLength(NuArchive* pArchive, FILE* fp, long* pLength)
{
    NuError err;
    long oldpos;

    Assert(fp != nil);
    Assert(pLength != nil);

    err = Nu_FTell(fp, &oldpos);
    BailError(err);

    err = Nu_FSeek(fp, 0, SEEK_END);
    BailError(err);

    err = Nu_FTell(fp, pLength);
    BailError(err);

    err = Nu_FSeek(fp, oldpos, SEEK_SET);
    BailError(err);

bail:
    return err;
}


/*
 * Truncate an open file.  This differs from ftruncate() in that it takes
 * a FILE* instead of an fd, and the length is a long instead of off_t.
 */
NuError
Nu_TruncateOpenFile(FILE* fp, long length)
{
    #if defined(HAVE_FTRUNCATE)
    if (ftruncate(fileno(fp), length) < 0)
        return errno ? errno : -1;
    return kNuErrNone;
    #elif defined(HAVE_CHSIZE)
    if (chsize(fileno(fp), length) < 0)
        return errno ? errno : -1;
    return kNuErrNone;
    #else
    /* not fatal; return this to indicate that it's an unsupported operation */
    return kNuErrInternal;
    #endif
}

