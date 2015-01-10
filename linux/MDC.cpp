/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Drive diskimglib.  Similar to MDC.
 */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#include "zlib.h"
#include "../diskimg/DiskImg.h"
#include "../nufxlib/NufxLib.h"
#include "StringArray.h"

using namespace DiskImgLib;

#define nil NULL
#define ASSERT assert
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))
typedef const char* LPCTSTR;

#define UNIX_LIKE
#define HAVE_DIRENT_H   // linux
#define MAX_PATH_LEN 1024

/* get a grip on this opendir/readdir stuff */
#if defined(UNIX_LIKE)
#  if defined(HAVE_DIRENT_H)
#    include <dirent.h>
#    define DIR_NAME_LEN(dirent)    ((int)strlen((dirent)->d_name))
     typedef struct dirent DIR_TYPE;
#  elif defined(HAVE_SYS_DIR_H)
#    include <sys/dir.h>
#    define DIR_NAME_LEN(direct)    ((direct)->d_namlen)
     typedef struct direct DIR_TYPE;
#  elif defined(HAVE_NDIR_H)
#    include <sys/ndir.h>
#    define DIR_NAME_LEN(direct)    ((direct)->d_namlen)
     typedef struct direct DIR_TYPE;
#  else
#    error "Port this?"
#  endif
#endif

/*
 * Globals.
 */
FILE* gLog = nil;
pid_t gPid = getpid();

struct Stats {
    long    numFiles;
    long    numDirectories;
    long    goodDiskImages;
} gStats = { 0 };

typedef struct ScanOpts {
    FILE*   outfp;
} ScanOpts;

typedef enum RecordKind {
    kRecordKindUnknown = 0,
    kRecordKindDisk,
    kRecordKindFile,
    kRecordKindForkedFile,
    kRecordKindDirectory,
    kRecordKindVolumeDirectory,
} RecordKind;


//#define kFilenameExtDelim '.'     /* separates extension from filename */

/* time_t values for bad dates */
#define kDateNone       ((time_t) -2)
#define kDateInvalid    ((time_t) -1)       // should match return from mktime()

/*
 * "buf" must hold at least 64 chars.
 */
void
FormatDate(time_t when, char* buf)
{
    if (when == kDateNone) {
        strcpy(buf, "[No Date]");
    } else if (when == kDateInvalid) {
        strcpy(buf, "<invalid>");
    } else {
        struct tm* ptm;

        ptm = localtime(&when);
        strftime(buf, 64, "%d-%b-%y %H:%M", ptm);
    }
}

#if 0
/*
 * Find the filename component of a local pathname.  Uses the fssep passed
 * in.  If the fssep is '\0' (as is the case for DOS 3.3), then the entire
 * pathname is returned.
 *
 * Always returns a pointer to a string; never returns nil.
 */
const char*
FilenameOnly(const char* pathname, char fssep)
{
    const char* retstr;
    const char* pSlash;
    char* tmpStr = nil;

    ASSERT(pathname != nil);
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
FindExtension(const char* pathname, char fssep)
{
    const char* pFilename;
    const char* pExt;

    /*
     * We have to isolate the filename so that we don't get excited
     * about "/foo.bar/file".
     */
    pFilename = FilenameOnly(pathname, fssep);
    ASSERT(pFilename != nil);
    pExt = strrchr(pFilename, kFilenameExtDelim);

    /* also check for "/blah/foo.", which doesn't count */
    if (pExt != nil && *(pExt+1) != '\0')
        return pExt;

    return nil;
}
#endif


/*
 * Analyze a file's characteristics.
 */
void
AnalyzeFile(const A2File* pFile, RecordKind* pRecordKind,
    unsigned long* pTotalLen, unsigned long* pTotalCompLen)
{
    if (pFile->IsVolumeDirectory()) {
        /* volume directory entry */
        ASSERT(pFile->GetRsrcLength() < 0);
        *pRecordKind = kRecordKindVolumeDirectory;
        *pTotalLen = pFile->GetDataLength();
        *pTotalCompLen = pFile->GetDataLength();
    } else if (pFile->IsDirectory()) {
        /* directory entry */
        ASSERT(pFile->GetRsrcLength() < 0);
        *pRecordKind = kRecordKindDirectory;
        *pTotalLen = pFile->GetDataLength();
        *pTotalCompLen = pFile->GetDataLength();
    } else if (pFile->GetRsrcLength() >= 0) {
        /* has resource fork */
        *pRecordKind = kRecordKindForkedFile;
        *pTotalLen = pFile->GetDataLength() + pFile->GetRsrcLength();
        *pTotalCompLen =
            pFile->GetDataSparseLength() + pFile->GetRsrcSparseLength();
    } else {
        /* just data fork */
        *pRecordKind = kRecordKindFile;
        *pTotalLen = pFile->GetDataLength();
        *pTotalCompLen = pFile->GetDataSparseLength();
    }
}

/*
 * Determine whether the access bits on the record make it a read-only
 * file or not.
 *
 * Uses a simplified view of the access flags.
 */
bool
IsRecordReadOnly(int accessBits)
{
    if (accessBits == 0x21L || accessBits == 0x01L)
        return true;
    else
        return false;
}

/* ProDOS file type names; must be entirely in upper case */
static const char gFileTypeNames[256][4] = {
    "NON", "BAD", "PCD", "PTX", "TXT", "PDA", "BIN", "FNT",
    "FOT", "BA3", "DA3", "WPF", "SOS", "$0D", "$0E", "DIR",
    "RPD", "RPI", "AFD", "AFM", "AFR", "SCL", "PFS", "$17",
    "$18", "ADB", "AWP", "ASP", "$1C", "$1D", "$1E", "$1F",
    "TDM", "$21", "$22", "$23", "$24", "$25", "$26", "$27",
    "$28", "$29", "8SC", "8OB", "8IC", "8LD", "P8C", "$2F",
    "$30", "$31", "$32", "$33", "$34", "$35", "$36", "$37",
    "$38", "$39", "$3A", "$3B", "$3C", "$3D", "$3E", "$3F",
    "DIC", "OCR", "FTD", "$43", "$44", "$45", "$46", "$47",
    "$48", "$49", "$4A", "$4B", "$4C", "$4D", "$4E", "$4F",
    "GWP", "GSS", "GDB", "DRW", "GDP", "HMD", "EDU", "STN",
    "HLP", "COM", "CFG", "ANM", "MUM", "ENT", "DVU", "FIN",
    "$60", "$61", "$62", "$63", "$64", "$65", "$66", "$67",
    "$68", "$69", "$6A", "BIO", "$6C", "TDR", "PRE", "HDV",
    "$70", "$71", "$72", "$73", "$74", "$75", "$76", "$77",
    "$78", "$79", "$7A", "$7B", "$7C", "$7D", "$7E", "$7F",
    "$80", "$81", "$82", "$83", "$84", "$85", "$86", "$87",
    "$88", "$89", "$8A", "$8B", "$8C", "$8D", "$8E", "$8F",
    "$90", "$91", "$92", "$93", "$94", "$95", "$96", "$97",
    "$98", "$99", "$9A", "$9B", "$9C", "$9D", "$9E", "$9F",
    "WP ", "$A1", "$A2", "$A3", "$A4", "$A5", "$A6", "$A7",
    "$A8", "$A9", "$AA", "GSB", "TDF", "BDF", "$AE", "$AF",
    "SRC", "OBJ", "LIB", "S16", "RTL", "EXE", "PIF", "TIF",
    "NDA", "CDA", "TOL", "DVR", "LDF", "FST", "$BE", "DOC",
    "PNT", "PIC", "ANI", "PAL", "$C4", "OOG", "SCR", "CDV",
    "FON", "FND", "ICN", "$CB", "$CC", "$CD", "$CE", "$CF",
    "$D0", "$D1", "$D2", "$D3", "$D4", "MUS", "INS", "MDI",
    "SND", "$D9", "$DA", "DBM", "$DC", "DDD", "$DE", "$DF",
    "LBR", "$E1", "ATK", "$E3", "$E4", "$E5", "$E6", "$E7",
    "$E8", "$E9", "$EA", "$EB", "$EC", "$ED", "R16", "PAS",
    "CMD", "$F1", "$F2", "$F3", "$F4", "$F5", "$F6", "$F7",
    "$F8", "OS ", "INT", "IVR", "BAS", "VAR", "REL", "SYS"
};

/*
 * Return a pointer to the three-letter representation of the file type name.
 *
 * Note to self: code down below tests first char for '?'.
 */
/*static*/ const char*
GetFileTypeString(unsigned long fileType)
{
    if (fileType < NELEM(gFileTypeNames))
        return gFileTypeNames[fileType];
    else
        return "???";
}

/*  
 * Sanitize a string.  The Mac likes to stick control characters into
 * things, e.g. ^C and ^M.
 */
static void
MacSanitize(char* str)
{       
    while (*str != '\0') {
        if (*str < 0x20 || *str >= 0x7f)
            *str = '?';
        str++;
    }
}   


/*
 * Load the contents of a DiskFS.
 *
 * Recursively handle sub-volumes.
 */
int
LoadDiskFSContents(DiskFS* pDiskFS, const char* volName,
    ScanOpts* pScanOpts)
{
    static const char* kBlankFileName = "<blank filename>";
    DiskFS::SubVolume* pSubVol = nil;
    A2File* pFile;

    ASSERT(pDiskFS != nil);
    pFile = pDiskFS->GetNextFile(nil);
    while (pFile != nil) {
        char subVolName[128] = "";
        char dispName[128] = "";
        //CString subVolName, dispName;
        RecordKind recordKind;
        unsigned long totalLen, totalCompLen;
        char tmpbuf[16];

        AnalyzeFile(pFile, &recordKind, &totalLen, &totalCompLen);
        if (recordKind == kRecordKindVolumeDirectory) {
            /* skip these */
            pFile = pDiskFS->GetNextFile(pFile);
            continue;
        }

        /* prepend volName for sub-volumes; must be valid Win32 dirname */
        if (volName[0] != '\0')
            snprintf(subVolName, sizeof(subVolName), "_%s", volName);

        const char* ccp = pFile->GetPathName();
        ASSERT(ccp != nil);
        if (strlen(ccp) == 0)
            ccp = kBlankFileName;

        if (subVolName[0] == '\0') {
            strncpy(dispName, ccp, sizeof(dispName));
            dispName[sizeof(dispName) - 1] = '\0';
        } else {
            snprintf(dispName, sizeof(dispName), "%s:%s", subVolName, ccp);
            //dispName = subVolName;
            //dispName += ':';
            //dispName += ccp;
        }
        ccp = dispName;

        int len = strlen(ccp);
        if (len <= 32) {
            fprintf(pScanOpts->outfp, "%c%-32.32s ",
                IsRecordReadOnly(pFile->GetAccess()) ? '*' : ' ',
                ccp);
        } else {
            fprintf(pScanOpts->outfp, "%c..%-30.30s ",
                IsRecordReadOnly(pFile->GetAccess()) ? '*' : ' ',
                ccp + len - 30);
        }
        switch (recordKind) {
        case kRecordKindUnknown:
            fprintf(pScanOpts->outfp, "%s- $%04X  ",
                GetFileTypeString(pFile->GetFileType()),
                pFile->GetAuxType());
            break;
        case kRecordKindDisk:
            snprintf(tmpbuf, sizeof(tmpbuf), "%ldk", totalLen / 1024);
            fprintf(pScanOpts->outfp, "Disk %-6s ", tmpbuf);
            break;
        case kRecordKindFile:
        case kRecordKindForkedFile:
        case kRecordKindDirectory:
            if (pDiskFS->GetDiskImg()->GetFSFormat() == DiskImg::kFormatMacHFS)
            {
                if (recordKind != kRecordKindDirectory &&
                    pFile->GetFileType() >= 0 && pFile->GetFileType() <= 0xff &&
                    pFile->GetAuxType() >= 0 && pFile->GetAuxType() <= 0xffff)
                {
                    /* ProDOS type embedded in HFS */
                    fprintf(pScanOpts->outfp, "%s%c $%04X  ",
                        GetFileTypeString(pFile->GetFileType()),
                        recordKind == kRecordKindForkedFile ? '+' : ' ',
                        pFile->GetAuxType());
                } else {
                    char typeStr[5];
                    char creatorStr[5];
                    unsigned long val;

                    val = pFile->GetAuxType();
                    creatorStr[0] = (unsigned char) (val >> 24);
                    creatorStr[1] = (unsigned char) (val >> 16);
                    creatorStr[2] = (unsigned char) (val >> 8);
                    creatorStr[3] = (unsigned char) val;
                    creatorStr[4] = '\0';

                    val = pFile->GetFileType();
                    typeStr[0] = (unsigned char) (val >> 24);
                    typeStr[1] = (unsigned char) (val >> 16);
                    typeStr[2] = (unsigned char) (val >> 8);
                    typeStr[3] = (unsigned char) val;
                    typeStr[4] = '\0';

                    MacSanitize(creatorStr);
                    MacSanitize(typeStr);

                    if (recordKind == kRecordKindDirectory) {
                        fprintf(pScanOpts->outfp, "DIR   %-4s  ", creatorStr);
                    } else {
                        fprintf(pScanOpts->outfp, "%-4s%c %-4s  ",
                            typeStr,
                            pFile->GetRsrcLength() > 0 ? '+' : ' ',
                            creatorStr);
                    }
                }
            } else {
                fprintf(pScanOpts->outfp, "%s%c $%04X  ",
                    GetFileTypeString(pFile->GetFileType()),
                    recordKind == kRecordKindForkedFile ? '+' : ' ',
                    pFile->GetAuxType());
            }
            break;
        default:
            ASSERT(0);
            fprintf(pScanOpts->outfp, "ERROR  ");
            break;
        }

        char date[64];
        if (pFile->GetModWhen() == 0)
            FormatDate(kDateNone, date);
        else
            FormatDate(pFile->GetModWhen(), date);
        fprintf(pScanOpts->outfp, "%-15s  ", (LPCTSTR) date);

        const char* fmtStr;
        switch (pFile->GetFSFormat()) {
        case DiskImg::kFormatDOS33:
        case DiskImg::kFormatDOS32:
        case DiskImg::kFormatUNIDOS:
            fmtStr = "DOS   ";
            break;
        case DiskImg::kFormatProDOS:
            fmtStr = "ProDOS";
            break;
        case DiskImg::kFormatPascal:
            fmtStr = "Pascal";
            break;
        case DiskImg::kFormatMacHFS:
            fmtStr = "HFS   ";
            break;
        case DiskImg::kFormatCPM:
            fmtStr = "CP/M  ";
            break;
        case DiskImg::kFormatMSDOS:
            fmtStr = "MS-DOS";
            break;
        case DiskImg::kFormatRDOS33:
        case DiskImg::kFormatRDOS32:
        case DiskImg::kFormatRDOS3:
            fmtStr = "RDOS  ";
            break;
        case DiskImg::kFormatGutenberg:
            fmtStr = "Gutenb";
            break;
        default:
            fmtStr = "???   ";
            break;
        }
        if (pFile->GetQuality() == A2File::kQualityDamaged)
            fmtStr = "BROKEN";

        fprintf(pScanOpts->outfp, "%s ", fmtStr);


#if 0
        /* compute the percent size */
        if ((!totalLen && totalCompLen) || (totalLen && !totalCompLen))
            fprintf(pScanOpts->outfp, "---   ");       /* weird */
        else if (totalLen < totalCompLen)
            fprintf(pScanOpts->outfp, ">100%% ");      /* compression failed? */
        else {
            sprintf(tmpbuf, "%02d%%", ComputePercent(totalCompLen, totalLen));
            fprintf(pScanOpts->outfp, "%4s  ", tmpbuf);
        }
#endif

        if (!totalLen && totalCompLen)
            fprintf(pScanOpts->outfp, "   ????");      /* weird */
        else
            fprintf(pScanOpts->outfp, "%8ld", totalLen);

        fprintf(pScanOpts->outfp, "\n");

        pFile = pDiskFS->GetNextFile(pFile);
    }

    /*
     * Load all sub-volumes.
     */
    pSubVol = pDiskFS->GetNextSubVolume(nil);
    while (pSubVol != nil) {
        const char* subVolName;
        int ret;

        subVolName = pSubVol->GetDiskFS()->GetVolumeName();
        if (subVolName == nil)
            subVolName = "+++";     // could probably do better than this

        ret = LoadDiskFSContents(pSubVol->GetDiskFS(), subVolName, pScanOpts);
        if (ret != 0)
            return ret;
        pSubVol = pDiskFS->GetNextSubVolume(pSubVol);
    }

    return 0;
}

/*
 * Open a disk image and dump the contents.
 *
 * Returns 0 on success, nonzero on failure.
 */
int
ScanDiskImage(const char* pathName, ScanOpts* pScanOpts)
{
    ASSERT(pathName != nil);
    ASSERT(pScanOpts != nil);
    ASSERT(pScanOpts->outfp != nil);

    DIError dierr;
    char errMsg[256] = "";
    DiskImg diskImg;
    DiskFS* pDiskFS = nil;

    dierr = diskImg.OpenImage(pathName, '/', true);
    if (dierr != kDIErrNone) {
        snprintf(errMsg, sizeof(errMsg), "Unable to open '%s': %s",
            pathName, DIStrError(dierr));
        goto bail;
    }

    dierr = diskImg.AnalyzeImage();
    if (dierr != kDIErrNone) {
        snprintf(errMsg, sizeof(errMsg), "Analysis of '%s' failed: %s",
            pathName, DIStrError(dierr));
        goto bail;
    }

    if (diskImg.GetFSFormat() == DiskImg::kFormatUnknown ||
        diskImg.GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        snprintf(errMsg, sizeof(errMsg), "Unable to identify filesystem on '%s'",
            pathName);
        goto bail;
    }

    /* create an appropriate DiskFS object */
    pDiskFS = diskImg.OpenAppropriateDiskFS();
    if (pDiskFS == nil) {
        /* unknown FS should've been caught above! */
        ASSERT(false);
        snprintf(errMsg, sizeof(errMsg), "Format of '%s' not recognized.",
            pathName);
        goto bail;
    }

    pDiskFS->SetScanForSubVolumes(DiskFS::kScanSubEnabled);

    /* object created; prep it */
    dierr = pDiskFS->Initialize(&diskImg, DiskFS::kInitFull);
    if (dierr != kDIErrNone) {
        snprintf(errMsg, sizeof(errMsg),
            "Error reading list of files from disk: %s", DIStrError(dierr));
        goto bail;
    }

    fprintf(pScanOpts->outfp, "File: %s\n", pathName);

    int kbytes;
    if (pDiskFS->GetDiskImg()->GetHasBlocks())
        kbytes = pDiskFS->GetDiskImg()->GetNumBlocks() / 2;
    else if (pDiskFS->GetDiskImg()->GetHasSectors())
        kbytes = (pDiskFS->GetDiskImg()->GetNumTracks() *
                pDiskFS->GetDiskImg()->GetNumSectPerTrack()) / 4;
    else
        kbytes = 0;
    fprintf(pScanOpts->outfp, "Disk: %s%s (%dKB)\n", pDiskFS->GetVolumeID(),
        pDiskFS->GetFSDamaged() ? " [*]" : "", kbytes);

    fprintf(pScanOpts->outfp,
        " Name                             Type Auxtyp Modified"
        "         Format   Length\n");
    fprintf(pScanOpts->outfp,
        "------------------------------------------------------"
        "------------------------\n");
    if (LoadDiskFSContents(pDiskFS, "", pScanOpts) != 0) {
        snprintf(errMsg, sizeof(errMsg),
            "Failed while loading contents of '%s'.", pathName);
        goto bail;
    }
    fprintf(pScanOpts->outfp,
        "------------------------------------------------------"
        "------------------------\n\n");

    gStats.goodDiskImages++;

bail:
    delete pDiskFS;

    if (errMsg[0] != '\0') {
        fprintf(pScanOpts->outfp, "Unable to process '%s'\n", pathName);
        fprintf(pScanOpts->outfp, "  %s\n\n", (LPCTSTR) errMsg);
        return -1;
    } else {
        return 0;
    }
}


/*
 * Check a file's status.
 *
 * [ Someday we may want to modify this to handle symbolic links. ]
 */
int
CheckFileStatus(const char* pathname, struct stat* psb, bool* pExists,
    bool* pIsReadable, bool* pIsDir)
{
    int result = 0;
    int cc;

    assert(pathname != nil);
    assert(psb != nil);
    assert(pExists != nil);
    assert(pIsReadable != nil);
    assert(pIsDir != nil);

    *pExists = true;
    *pIsReadable = true;
    *pIsDir = false;

    cc = stat(pathname, psb);
    if (cc) {
        if (errno == ENOENT)
            *pExists = false;
        else
            result = -1;        // stat failed
        goto bail;
    }

    if (S_ISDIR(psb->st_mode))
        *pIsDir = true;

    /*
     * Test if we can read this file.  How do we do that?  The easy but slow
     * way is to call access(2), the harder way is to figure out
     * what user/group we are and compare the appropriate file mode.
     */
    if (access(pathname, R_OK) < 0)
        *pIsReadable = false;

bail:
    return result;
}


/* forward decl */
int ProcessFile(const char* pathname, ScanOpts* pScanOpts);

/*
 * UNIX-style recursive directory descent.  Scan the contents of a directory.
 * If a subdirectory is found, follow it; otherwise, call ProcessFile to
 * handle the file.
 */
int
ProcessDirectory(const char* dirName, ScanOpts* pScanOpts)
{
    StringArray strArray;
    int result = -1;
    DIR* dirp = nil;
    DIR_TYPE* entry;
    char nbuf[MAX_PATH_LEN];    /* malloc might be better; this soaks stack */
    char fssep;
    int len;

    assert(pScanOpts != nil);
    assert(dirName != nil);

#ifdef _DEBUG
    fprintf(gLog, "+++ Processing directory '%s'\n", dirName);
#endif

    dirp = opendir(dirName);
    if (dirp == nil) {
        //err = errno ? errno : -1;
        goto bail;
    }

    fssep = '/';

    /* could use readdir_r, but we don't care about reentrancy here */
    while ((entry = readdir(dirp)) != nil) {
        /* skip the dotsies */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        len = strlen(dirName);
        if (len + DIR_NAME_LEN(entry) +2 > MAX_PATH_LEN) {
            fprintf(stderr, "ERROR: Filename exceeds %d bytes: %s%c%s\n",
                MAX_PATH_LEN, dirName, fssep, entry->d_name);
            goto bail;
        }

        /* form the new name, inserting an fssep if needed */
        strcpy(nbuf, dirName);
        if (dirName[len-1] != fssep)
            nbuf[len++] = fssep;
        strcpy(nbuf+len, entry->d_name);

        strArray.Add(nbuf);
    }

    /* sort the list, then process the files */
    strArray.Sort(StringArray::CmpAscendingAlpha);
    for (int i = 0; i < strArray.GetCount(); i++)
        (void) ProcessFile(strArray.GetEntry(i), pScanOpts);

    result = 0;

bail:
    if (dirp != nil)
        (void)closedir(dirp);
    return result;
}

/*
 * Process a file.
 *
 * Returns with an error if the file doesn't exist or isn't readable.
 */
int
ProcessFile(const char* pathname, ScanOpts* pScanOpts)
{
    int result = -1;
    bool exists, isDir, isReadable;
    struct stat sb;

    assert(pathname != nil);
    assert(pScanOpts != nil);

#ifdef _DEBUG
    fprintf(gLog, "+++ Processing file or dir '%s'\n", pathname);
#endif

    if (CheckFileStatus(pathname, &sb, &exists, &isReadable, &isDir) != 0) {
        fprintf(stderr, "ERROR: unexpected error while examining '%s'\n",
            pathname);
        goto bail;
    }

    if (!exists) {
        fprintf(stderr, "ERROR: couldn't find '%s'\n", pathname);
        goto bail;
    }
    if (!isReadable) {
        fprintf(stderr, "ERROR: file '%s' isn't readable\n", pathname);
        goto bail;
    }

    if (isDir) {
        result = ProcessDirectory(pathname, pScanOpts);
        gStats.numDirectories++;
    } else {
        result = ScanDiskImage(pathname, pScanOpts);
        gStats.numFiles++;
    }

bail:
    return result;
}


/*
 * Handle a debug message from the DiskImg library.
 */
/*static*/ void
MsgHandler(const char* file, int line, const char* msg)
{
    ASSERT(file != nil);
    ASSERT(msg != nil);

#ifdef _DEBUG
    fprintf(gLog, "%05u %s", gPid, msg);
#endif
}
/*
 * Handle a global error message from the NufxLib library by shoving it
 * through the DiskImgLib message function.
 */
NuResult
NufxErrorMsgHandler(NuArchive* /*pArchive*/, void* vErrorMessage)
{
    const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;

    if (pErrorMessage->isDebug) {
        Global::PrintDebugMsg(pErrorMessage->file, pErrorMessage->line,
            "<nufxlib> [D] %s\n", pErrorMessage->message);
    } else {
        Global::PrintDebugMsg(pErrorMessage->file, pErrorMessage->line,
            "<nufxlib> %s\n", pErrorMessage->message);
    }

    return kNuOK;
}

/*
 * Process every argument.
 */
int
main(int argc, char** argv)
{
    ScanOpts scanOpts;
    scanOpts.outfp = stdout;

#ifdef _DEBUG
    const char* kLogFile = "mdc-log.txt";
    gLog = fopen(kLogFile, "w");
    if (gLog == nil) {
        fprintf(stderr, "ERROR: unable to open log file\n");
        exit(1);
    }
#endif

    int32_t major, minor, bug;
    Global::GetVersion(&major, &minor, &bug);

    printf("MDC for Linux v3.0.0 (DiskImg library v%d.%d.%d)\n",
        major, minor, bug);
    printf("Copyright (C) 2006 by faddenSoft, LLC.  All rights reserved.\n");
    printf("MDC is part of CiderPress, available from http://www.faddensoft.com/.\n");
    NuGetVersion(&major, &minor, &bug, nil, nil);
    printf("Linked against NufxLib v%d.%d.%d and zlib version %s.\n",
        major, minor, bug, zlibVersion());

    if (argc == 1) {
        fprintf(stderr, "\nUsage: mdc file ...\n");
        goto done;
    }

#ifdef _DEBUG
    printf("Log file is '%s'\n", kLogFile);
#endif
    printf("\n");

    Global::SetDebugMsgHandler(MsgHandler);
    Global::AppInit();

    NuSetGlobalErrorMessageHandler(NufxErrorMsgHandler);

    time_t start;
    start = time(NULL);
    printf("Run started at %.24s\n\n", ctime(&start));

    while (--argc) {
        ProcessFile(*++argv, &scanOpts);
    }

    printf("Scan completed in %ld seconds:\n", time(NULL) - start);
    printf("  Directories : %ld\n", gStats.numDirectories);
    printf("  Files       : %ld (%ld good disk images)\n", gStats.numFiles,
        gStats.goodDiskImages);

    Global::AppCleanup();

done:
#ifdef _DEBUG
    fclose(gLog);
#endif

    return 0;
}

