/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of GenericArchive and GenericEntry.
 *
 * These serve as abstract base classes for archive-specific classes.
 */
#include "stdafx.h"
#include "GenericArchive.h"
#include "NufxArchive.h"
#include "FileNameConv.h"
#include "ContentList.h"
#include "../reformat/ReformatBase.h"
#include "Main.h"
#include <sys/stat.h>
#include <errno.h>

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
 *      GenericEntry
 * ===========================================================================
 */

GenericEntry::GenericEntry(void)
  : fFssep('\0'),
    fFileType(0),
    fAuxType(0),
    fAccess(0),
    fCreateWhen(kDateNone),
    fModWhen(kDateNone),
    fRecordKind(kRecordKindUnknown),
    fFormatStr(L"Unknown"),
    fDataForkLen(0),
    fRsrcForkLen(0),
    fCompressedLen(0),
    fSourceFS(DiskImg::kFormatUnknown),
    fHasDataFork(false),
    fHasRsrcFork(false),
    fHasDiskImage(false),
    fHasComment(false),
    fHasNonEmptyComment(false),
    fDamaged(false),
    fSuspicious(false),
    fIndex(-1),
    fpPrev(NULL),
    fpNext(NULL)
{
}

GenericEntry::~GenericEntry(void) {}

void GenericEntry::SetPathNameMOR(const char* path)
{
    ASSERT(path != NULL && strlen(path) > 0);
    fPathNameMOR = path;
    // nuke the derived fields
    fFileName = L"";
    fFileNameExtension = L"";
    fDisplayName = L"";

    /*
     * Generate the Unicode representation from the Mac OS Roman source.
     * For now, we just treat the input as CP-1252.
     *
     * TODO(Unicode)
     */
    fPathNameUNI = fPathNameMOR;

    /*
     * Warning: to be 100% pedantically correct here, we should NOT do this
     * if the fssep char is '_'.  However, that may not have been set by
     * the time we got here, so to do this "correctly" we'd need to delay
     * the underscorage until the first GetPathName call.
     */
    const Preferences* pPreferences = GET_PREFERENCES();
    if (pPreferences->GetPrefBool(kPrSpacesToUnder)) {
        SpacesToUnderscores(&fPathNameMOR);
    }
}

const CString& GenericEntry::GetFileName(void)
{
    ASSERT(!fPathNameMOR.IsEmpty());
    if (fFileName.IsEmpty()) {
        fFileName = PathName::FilenameOnly(fPathNameUNI, fFssep);
    }
    return fFileName;
}

const CString& GenericEntry::GetFileNameExtension(void)
{
    ASSERT(!fPathNameMOR.IsEmpty());
    if (fFileNameExtension.IsEmpty()) {
        fFileNameExtension = PathName::FindExtension(fPathNameUNI, fFssep);
    }
    return fFileNameExtension;
}

const CStringA& GenericEntry::GetFileNameExtensionMOR(void)
{
    ASSERT(!fPathNameMOR.IsEmpty());
    if (fFileNameExtensionMOR.IsEmpty()) {
        CString str = PathName::FindExtension(fPathNameUNI, fFssep);
        // TODO(Unicode): either get the extension from the MOR filename,
        //  or convert this properly from Unicode to MOR (not CP-1252).
        fFileNameExtensionMOR = str;
    }
    return fFileNameExtensionMOR;
}

void GenericEntry::SetSubVolName(const WCHAR* name)
{
    fSubVolName = name;
}

const CString& GenericEntry::GetDisplayName(void) const
{
    ASSERT(!fPathNameMOR.IsEmpty());
    if (!fDisplayName.IsEmpty()) {
        return fDisplayName;
    }

    if (!fSubVolName.IsEmpty()) {
        fDisplayName = fSubVolName + (WCHAR) DiskFS::kDIFssep;
    }
    fDisplayName += Charset::ConvertMORToUNI(fPathNameMOR);
    return fDisplayName;
}

const WCHAR* GenericEntry::GetFileTypeString(void) const
{
    return PathProposal::FileTypeString(fFileType);
}

/*static*/ void GenericEntry::SpacesToUnderscores(CStringA* pStr)
{
    pStr->Replace(' ', '_');
}

/*static*/ bool GenericEntry::CheckHighASCII(const uint8_t* buffer,
    size_t count)
{
    /*
     * (Pulled from NufxLib Funnel.c.)
     *
     * Check to see if this is a high-ASCII file.  To qualify, EVERY
     * character must have its high bit set, except for spaces (0x20,
     * courtesy Glen Bredon's "Merlin") and nulls (0x00, because of random-
     * access text files).
     *
     * The test for 0x00 is actually useless in many circumstances because the
     * NULLs will cause the text file auto-detector to flunk the file.  It will,
     * however, allow the user to select "convert ALL files" and still have the
     * stripping enabled.
     */
    bool isHighASCII;

    ASSERT(buffer != NULL);
    ASSERT(count != 0);

    isHighASCII = true;
    while (count--) {
        if ((*buffer & 0x80) == 0 && *buffer != 0x20 && *buffer != 0x00) {
            LOGI("Flunking CheckHighASCII on 0x%02x", *buffer);
            isHighASCII = false;
            break;
        }
        
        buffer++;
    }

    return isHighASCII;
}

/*
 * (Pulled from NufxLib Funnel.c.)
 *
 * Table determining what's a binary character and what isn't.  It would
 * possibly be more compact to generate this from a simple description,
 * but I'm hoping static/const data will end up in the code segment and
 * save space on the heap.
 *
 * This corresponds to less-316's ISO-latin1 "8bcccbcc18b95.33b.".  This
 * may be too loose by itself; we may want to require that the lower-ASCII
 * values appear in higher proportions than the upper-ASCII values.
 * Otherwise we run the risk of converting a binary file with specific
 * properties.  (Note that "upper-ASCII" refers to umlauts and other
 * accented characters, not DOS 3.3 "high ASCII".)
 *
 * The auto-detect mechanism will never be perfect though, so there's not
 * much point in tweaking it to death.
 */
static const char gIsBinary[256] = {
    1, 1, 1, 1, 1, 1, 1, 1,  0, 0, 0, 1, 0, 0, 1, 1,    /* ^@-^O */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,    /* ^P-^_ */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /*   - / */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0 - ? */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* @ - O */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* P - _ */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* ` - o */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 1,    /* p - DEL */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,    /* 0x80 */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,    /* 0x90 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xa0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xb0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xc0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xd0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xe0 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,    /* 0xf0 */
};

static const int  kNuMaxUpperASCII = 1;     /* max #of binary chars per 100 bytes */
static const int  kMinConvThreshold = 40;   /* min of 40 chars for auto-detect */
static const char kCharLF = '\n';
static const char kCharCR = '\r';

/*static*/ GenericEntry::ConvertEOL GenericEntry::DetermineConversion(
    const uint8_t* buffer, size_t count,
    EOLType* pSourceType, ConvertHighASCII* pConvHA)
{
    /*
     * We need to decide if we are looking at text data, and if so, what kind
     * of line terminator is in use.
     *
     * If we don't have enough data to make a determination, don't mess with it.
     * (Thought for the day: add a "bias" flag, based on the NuRecord fileType,
     * that causes us to handle borderline or sub-min-threshold cases more
     * reasonably.  If it's of type TXT, it's probably text.)
     *
     * We try to figure out whether it's CR, LF, or CRLF, so that we can
     * skip the CPU-intensive conversion process if it isn't necessary.
     *
     * We will also investigate enabling a "high-ASCII" stripper if requested.
     * This is only enabled when EOL conversions are enabled.  Set "*pConvHA"
     * to on/off/auto before calling.  If it's initially set to "off", no
     * attempt to evaluate high ASCII will be made.  If "on" or "auto", the
     * buffer will be scanned, and if the input appears to be high ASCII then
     * it will be stripped *before* the EOL determination is made.
     */
    ConvertHighASCII wantConvHA = *pConvHA;
    size_t bufCount, numBinary, numLF, numCR;
    bool isHighASCII;
    uint8_t val;

    *pSourceType = kEOLUnknown;
    *pConvHA = kConvertHAOff;

    if (count < kMinConvThreshold)
        return kConvertEOLOff;

    /*
     * Check to see if the buffer is all high-ASCII characters.  If it is,
     * we want to strip characters before we test them below.
     *
     * If high ASCII conversion is disabled, assume that any high-ASCII
     * characters are not meant to be line terminators, i.e. 0x8d != 0x0d.
     */
    if (wantConvHA == kConvertHAOn || wantConvHA == kConvertHAAuto) {
        isHighASCII = CheckHighASCII(buffer, count);
        LOGI(" +++ Determined isHighASCII=%d", isHighASCII);
    } else {
        isHighASCII = false;
        LOGI(" +++ Not even checking isHighASCII");
    }

    bufCount = count;
    numBinary = numLF = numCR = 0;
    while (bufCount--) {
        val = *buffer++;
        if (isHighASCII)
            val &= 0x7f;
        if (gIsBinary[val])
            numBinary++;
        if (val == kCharLF)
            numLF++;
        if (val == kCharCR)
            numCR++;
    }

    /* if #found is > #allowed, it's a binary file */
    if (count < 100) {
        /* use simplified check on files between kNuMinConvThreshold and 100 */
        if (numBinary > kNuMaxUpperASCII)
            return kConvertEOLOff;
    } else if (numBinary > (count / 100) * kNuMaxUpperASCII)
        return kConvertEOLOff;

    /*
     * If our "convert to" setting is the same as what we're converting
     * from, we can turn off the converter and speed things up.
     *
     * These are simplistic, but this is intended as an optimization.  We
     * will blow it if the input has lots of CRs and LFs scattered about,
     * and they just happen to be in equal amounts, but it's not clear
     * to me that an automatic EOL conversion makes sense on that sort
     * of file anyway.
     *
     * None of this applies if we also need to do a high-ASCII conversion,
     * because we can't bypass the processing.
     */
    if (isHighASCII) {
        *pConvHA = kConvertHAOn;
    } else {
        if (numLF && !numCR)
            *pSourceType = kEOLLF;
        else if (!numLF && numCR)
            *pSourceType = kEOLCR;
        else if (numLF && numLF == numCR)
            *pSourceType = kEOLCRLF;
        else
            *pSourceType = kEOLUnknown;
    }

    return kConvertEOLOn;
}

/*
 * Output CRLF.
 */
static inline void PutEOL(FILE* fp)
{
    putc(kCharCR, fp);
    putc(kCharLF, fp);
}

/*static*/ int GenericEntry::WriteConvert(FILE* fp, const char* buf, size_t len,
    ConvertEOL* pConv, ConvertHighASCII* pConvHA, bool* pLastCR)
{
    int err = 0;

    LOGD("+++ WriteConvert conv=%d convHA=%d", *pConv, *pConvHA);

    if (len == 0) {
        LOGI("WriteConvert asked to write 0 bytes; returning");
        return err;
    }

    /* if we're in "auto" mode, scan the input for EOL and high ASCII */
    if (*pConv == kConvertEOLAuto) {
        EOLType sourceType;
        *pConv = DetermineConversion((uint8_t*)buf, len, &sourceType,
                    pConvHA);
        if (*pConv == kConvertEOLOn && sourceType == kEOLCRLF) {
            LOGI(" Auto-detected text conversion from CRLF; disabling");
            *pConv = kConvertEOLOff;
        }
        LOGI(" Auto-detected EOL conv=%d ha=%d", *pConv, *pConvHA);
    } else if (*pConvHA == kConvertHAAuto) {
        if (*pConv == kConvertEOLOn) {
            /* definitely converting EOL, test for high ASCII */
            if (CheckHighASCII((uint8_t*)buf, len))
                *pConvHA = kConvertHAOn;
            else
                *pConvHA = kConvertHAOff;
        } else {
            /* not converting EOL, don't convert high ASCII */
            *pConvHA = kConvertHAOff;
        }
    }
    LOGD("+++  After auto, conv=%d convHA=%d", *pConv, *pConvHA);
    ASSERT(*pConv == kConvertEOLOn || *pConv == kConvertEOLOff);
    ASSERT(*pConvHA == kConvertHAOn || *pConvHA == kConvertHAOff);

    /* write the output */
    if (*pConv == kConvertEOLOff) {
        if (fwrite(buf, len, 1, fp) != 1) {
            err = errno;
            LOGE("WriteConvert failed, err=%d", errno);
        }
    } else {
        ASSERT(*pConv == kConvertEOLOn);
        bool lastCR = *pLastCR;
        uint8_t uch;
        int mask;

        if (*pConvHA == kConvertHAOn)
            mask = 0x7f;
        else
            mask = 0xff;

        while (len--) {
            uch = (*buf) & mask;

            if (uch == kCharCR) {
                PutEOL(fp);
                lastCR = true;
            } else if (uch == kCharLF) {
                if (!lastCR)
                    PutEOL(fp);
                lastCR = false;
            } else {
                putc(uch, fp);
                lastCR = false;
            }
            buf++;
        }
        *pLastCR = lastCR;
    }

    return err;
}


/*
 * ===========================================================================
 *      GenericArchive
 * ===========================================================================
 */

void GenericArchive::AddEntry(GenericEntry* pEntry)
{
    if (fEntryHead == NULL) {
        ASSERT(fEntryTail == NULL);
        fEntryHead = pEntry;
        fEntryTail = pEntry;
        ASSERT(pEntry->GetPrev() == NULL);
        ASSERT(pEntry->GetNext() == NULL);
    } else {
        ASSERT(fEntryTail != NULL);
        ASSERT(pEntry->GetPrev() == NULL);
        pEntry->SetPrev(fEntryTail);
        ASSERT(fEntryTail->GetNext() == NULL);
        fEntryTail->SetNext(pEntry);
        fEntryTail = pEntry;
    }

    fNumEntries++;

    //if (fEntryIndex != NULL) {
    //  LOGI("Resetting fEntryIndex");
    //  delete [] fEntryIndex;
    //  fEntryIndex = NULL;
    //}
}

void GenericArchive::DeleteEntries(void)
{
    GenericEntry* pEntry;
    GenericEntry* pNext;

    LOGI("Deleting %d archive entries", fNumEntries);

    pEntry = GetEntries();
    while (pEntry != NULL) {
        pNext = pEntry->GetNext();
        delete pEntry;
        pEntry = pNext;
    }

    //delete [] fEntryIndex;
    fNumEntries = 0;
    fEntryHead = fEntryTail = NULL;
}

#if 0
/*
 * Create an index for fast access.
 */
void
GenericArchive::CreateIndex(void)
{
    GenericEntry* pEntry;
    int num;

    LOGI("Creating entry index (%d entries)", fNumEntries);

    ASSERT(fNumEntries != 0);

    fEntryIndex = new GenericEntry*[fNumEntries];
    if (fEntryIndex == NULL)
        return;

    pEntry = GetEntries();
    num = 0;
    while (pEntry != NULL) {
        fEntryIndex[num] = pEntry;
        pEntry = pEntry->GetNext();
        num++;
    }
}
#endif

/*static*/ CString GenericArchive::GenDerivedTempName(const WCHAR* filename)
{
    /*
     * The key is to come up with the name of a temp file in the same directory
     * (or at least on the same disk volume) so that the temp file can be
     * renamed on top of the original.
     *
     * Windows _mktemp does appear to test for the existence of the file, which
     * is good.  It doesn't actually open the file, which creates a small window
     * in which bad things could happen, but it should be okay.
     */
    static const WCHAR kTmpTemplate[] = L"CPtmp_XXXXXX";
    CString mangle(filename);
    int idx, len;

    ASSERT(filename != NULL);

    len = mangle.GetLength();
    ASSERT(len > 0);
    idx = mangle.ReverseFind('\\');
    if (idx < 0) {
        /* generally shouldn't happen -- we're using full paths */
        return kTmpTemplate;
    } else {
        mangle.Delete(idx+1, len-(idx+1));  /* delete out to the end */
        mangle += kTmpTemplate;
    }
    LOGD("GenDerived: passed '%ls' returned '%ls'", filename, (LPCWSTR) mangle);

    return mangle;
}

/*static*/ int GenericArchive::ComparePaths(const CString& name1, char fssep1,
    const CString& name2, char fssep2)
{
    const WCHAR* cp1 = name1;
    const WCHAR* cp2 = name2;

    while (*cp1 != '\0' && *cp2 != '\0') {
        if (*cp1 == fssep1) {
            if (*cp2 != fssep2) {
                /* one fssep, one not, no match */
                if (*cp1 == *cp2)
                    return 1;
                else
                    return *cp1 - *cp2;
            } else {
                /* both are fssep, it's a match even if ASCII is different */
            }
        } else if (*cp2 == fssep2) {
            /* one fssep, one not */
            if (*cp1 == *cp2)
                return -1;
            else
                return *cp1 - *cp2;
        } else if (tolower(*cp1) != tolower(*cp2)) {
            /* mismatch */
            return tolower(*cp1) - tolower(*cp2);
        }

        cp1++;
        cp2++;
    }

    return *cp1 - *cp2;
}


/*
 * ===========================================================================
 *      GenericArchive -- "add files" stuff
 * ===========================================================================
 */

/*
 * Much of this was adapted from NuLib2.
 */

/*static*/ void GenericArchive::UNIXTimeToDateTime(const time_t* pWhen,
    NuDateTime* pDateTime)
{
    struct tm* ptm;

    ASSERT(pWhen != NULL);
    ASSERT(pDateTime != NULL);

    ptm = localtime(pWhen);
    if (ptm == NULL) {
        ASSERT(*pWhen == kDateNone || *pWhen == kDateInvalid);
        memset(pDateTime, 0, sizeof(*pDateTime));
        return;
    }
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
 * Directory structure and functions, based on zDIR in Info-Zip sources.
 */
typedef struct Win32dirent {
    char    d_attr;
    WCHAR   d_name[MAX_PATH];
    int     d_first;
    HANDLE  d_hFindFile;
} Win32dirent;

static const WCHAR kWildMatchAll[] = L"*.*";

Win32dirent* GenericArchive::OpenDir(const WCHAR* name)
{
    Win32dirent* dir = NULL;
    WCHAR* tmpStr = NULL;
    WCHAR* cp;
    WIN32_FIND_DATA fnd;

    dir = (Win32dirent*) malloc(sizeof(*dir));
    tmpStr = (WCHAR*) malloc((wcslen(name) + 2 + wcslen(kWildMatchAll)) * sizeof(WCHAR));
    if (dir == NULL || tmpStr == NULL)
        goto failed;

    wcscpy(tmpStr, name);
    cp = tmpStr + wcslen(tmpStr);

    /* don't end in a colon (e.g. "C:") */
    if ((cp - tmpStr) > 0 && wcsrchr(tmpStr, ':') == (cp - 1))
        *cp++ = '.';
    /* must end in a slash */
    if ((cp - tmpStr) > 0 &&
            wcsrchr(tmpStr, PathProposal::kLocalFssep) != (cp - 1))
        *cp++ = PathProposal::kLocalFssep;

    wcscpy(cp, kWildMatchAll);

    dir->d_hFindFile = FindFirstFile(tmpStr, &fnd);
    if (dir->d_hFindFile == INVALID_HANDLE_VALUE)
        goto failed;

    wcscpy(dir->d_name, fnd.cFileName);
    dir->d_attr = (unsigned char) fnd.dwFileAttributes;
    dir->d_first = 1;

bail:
    free(tmpStr);
    return dir;

failed:
    free(dir);
    dir = NULL;
    goto bail;
}

Win32dirent* GenericArchive::ReadDir(Win32dirent* dir)
{
    if (dir->d_first)
        dir->d_first = 0;
    else {
        WIN32_FIND_DATA fnd;

        if (!FindNextFile(dir->d_hFindFile, &fnd))
            return NULL;
        wcscpy(dir->d_name, fnd.cFileName);
        dir->d_attr = (unsigned char) fnd.dwFileAttributes;
    }

    return dir;
}

void GenericArchive::CloseDir(Win32dirent* dir)
{
    if (dir == NULL)
        return;

    FindClose(dir->d_hFindFile);
    free(dir);
}

NuError GenericArchive::Win32AddDirectory(const AddFilesDialog* pAddOpts,
    const WCHAR* dirName, CString* pErrMsg)
{
    NuError err = kNuErrNone;
    Win32dirent* dirp = NULL;
    Win32dirent* entry;
    WCHAR nbuf[MAX_PATH];   /* malloc might be better; this soaks stack */
    char fssep;
    int len;

    ASSERT(pAddOpts != NULL);
    ASSERT(dirName != NULL);

    LOGI("+++ DESCEND: '%ls'", dirName);

    dirp = OpenDir(dirName);
    if (dirp == NULL) {
        if (errno == ENOTDIR)
            err = kNuErrNotDir;
        else
            err = errno ? (NuError)errno : kNuErrOpenDir;
        
        pErrMsg->Format(L"Failed on '%ls': %hs.", dirName, NuStrError(err));
        goto bail;
    }

    fssep = PathProposal::kLocalFssep;

    /* could use readdir_r, but we don't care about reentrancy here */
    while ((entry = ReadDir(dirp)) != NULL) {
        /* skip the dotsies */
        if (wcscmp(entry->d_name, L".") == 0 ||
            wcscmp(entry->d_name, L"..") == 0)
        {
            continue;
        }

        len = wcslen(dirName);
        if (len + wcslen(entry->d_name) +2 > MAX_PATH) {
            err = kNuErrInternal;
            LOGE("ERROR: Filename exceeds %d bytes: %ls%c%ls",
                MAX_PATH, dirName, fssep, entry->d_name);
            goto bail;
        }

        /* form the new name, inserting an fssep if needed */
        wcscpy(nbuf, dirName);
        if (dirName[len-1] != fssep)
            nbuf[len++] = fssep;
        wcscpy(nbuf+len, entry->d_name);

        err = Win32AddFile(pAddOpts, nbuf, pErrMsg);
        if (err != kNuErrNone)
            goto bail;
    }

bail:
    if (dirp != NULL)
        (void)CloseDir(dirp);
    return err;
}

NuError GenericArchive::Win32AddFile(const AddFilesDialog* pAddOpts,
    const WCHAR* pathname, CString* pErrMsg)
{
    NuError err = kNuErrNone;
    bool exists, isDir, isReadable;
    LocalFileDetails details;
    struct _stat sb;

    ASSERT(pAddOpts != NULL);
    ASSERT(pathname != NULL);

    PathName checkPath(pathname);
    int ierr = checkPath.CheckFileStatus(&sb, &exists, &isReadable, &isDir);
    if (ierr != 0) {
        err = kNuErrGeneric;
        pErrMsg->Format(L"Unexpected error while examining '%ls': %hs.",
            pathname, NuStrError((NuError) ierr));
        goto bail;
    }

    if (!exists) {
        err = kNuErrFileNotFound;
        pErrMsg->Format(L"Couldn't find '%ls'", pathname);
        goto bail;
    }
    if (!isReadable) {
        err = kNuErrFileNotReadable;
        pErrMsg->Format(L"File '%ls' isn't readable.", pathname);
        goto bail;
    }
    if (isDir) {
        if (pAddOpts->fIncludeSubfolders)
            err = Win32AddDirectory(pAddOpts, pathname, pErrMsg);
        goto bail;
    }

    /*
     * We've found a file that we want to add.  We need to decide what
     * filetype and auxtype it has, and whether or not it's actually the
     * resource fork of another file.
     */
    LOGD("+++ ADD '%ls'", pathname);

    /*
     * Fill out the "details" structure.
     */
    err = details.SetFields(pAddOpts, pathname, &sb);
    if (err != kNuErrNone)
        goto bail;

    assert(wcscmp(pathname, details.GetLocalPathName()) == 0);
    err = DoAddFile(pAddOpts, &details);
    if (err == kNuErrSkipped)   // ignore "skipped" result
        err = kNuErrNone;
    if (err != kNuErrNone)
        goto bail;

bail:
    if (err != kNuErrNone && pErrMsg->IsEmpty()) {
        pErrMsg->Format(L"Unable to add file '%ls': %hs.",
            pathname, NuStrError(err));
    }
    return err;
}

NuError GenericArchive::AddFile(const AddFilesDialog* pAddOpts,
    const WCHAR* pathname, CString* pErrMsg)
{
    *pErrMsg = L"";
    return Win32AddFile(pAddOpts, pathname, pErrMsg);
}


/*
 * ===========================================================================
 *      GenericArchive::FileDetails
 * ===========================================================================
 */

GenericArchive::LocalFileDetails::LocalFileDetails(void)
  : fEntryKind(kFileKindUnknown),
    fFileSysFmt(DiskImg::kFormatUnknown),
    fFssep('\0'),
    fFileType(0),
    fExtraType(0),
    fAccess(0),
    fStorageType(0)
{
    memset(&fCreateWhen, 0, sizeof(fCreateWhen));
    memset(&fModWhen, 0, sizeof(fModWhen));
    memset(&fArchiveWhen, 0, sizeof(fArchiveWhen));

    // set these for debugging
    memset(&fNuFileDetails, 0xcc, sizeof(fNuFileDetails));
    memset(&fCreateParms, 0xcc, sizeof(&fCreateParms));
    fStoragePathNameMOR = "!INIT!";
}

NuError GenericArchive::LocalFileDetails::SetFields(const AddFilesDialog* pAddOpts,
    const WCHAR* pathname, struct _stat* psb)
{
    time_t now;

    ASSERT(pAddOpts != NULL);
    ASSERT(pathname != NULL);

    /* get adjusted filename, along with any preserved type info */
    PathProposal pathProp;
    pathProp.Init(pathname);
    pathProp.LocalToArchive(pAddOpts);

    /* set up the local and archived pathnames */
    fLocalPathName = pathname;
    fStrippedLocalPathName = L"";
    if (!pAddOpts->fStoragePrefix.IsEmpty()) {
        fStrippedLocalPathName += pAddOpts->fStoragePrefix;
        fStrippedLocalPathName += pathProp.fStoredFssep;
    }
    fStrippedLocalPathName += pathProp.fStoredPathName;
    GenerateStoragePathName();

    fFileSysFmt = DiskImg::kFormatUnknown;
    fStorageType = kNuStorageUnknown;  /* let NufxLib et.al. worry about it */
    if (psb->st_mode & S_IWUSR)
        fAccess = kNuAccessUnlocked;
    else
        fAccess = kNuAccessLocked;
    fEntryKind = LocalFileDetails::kFileKindDataFork;
    fFssep = pathProp.fStoredFssep;
    fFileType = pathProp.fFileType;
    fExtraType = pathProp.fAuxType;

#if 0
    /* if this is a disk image, fill in disk-specific fields */
    if (NState_GetModAddAsDisk(pState)) {
        if ((psb->st_size & 0x1ff) != 0) {
            /* reject anything whose size isn't a multiple of 512 bytes */
            printf("NOT storing odd-sized (%ld) file as disk image: %ls\n",
                (long)psb->st_size, livePathStr);
        } else {
            /* set fields; note the "preserve" stuff can override this */
            pDetails->threadID = kNuThreadIDDiskImage;
            pDetails->storageType = 512;
            pDetails->extraType = psb->st_size / 512;
        }
    }
#endif

    now = time(NULL);
    UNIXTimeToDateTime(&now, &fArchiveWhen);
    UNIXTimeToDateTime(&psb->st_mtime, &fModWhen);
    UNIXTimeToDateTime(&psb->st_ctime, &fCreateWhen);

    switch (pathProp.fThreadKind) {
    case GenericEntry::kDataThread:
        //pDetails->threadID = kNuThreadIDDataFork;
        fEntryKind = LocalFileDetails::kFileKindDataFork;
        break;
    case GenericEntry::kRsrcThread:
        //pDetails->threadID = kNuThreadIDRsrcFork;
        fEntryKind = LocalFileDetails::kFileKindRsrcFork;
        break;
    case GenericEntry::kDiskImageThread:
        //pDetails->threadID = kNuThreadIDDiskImage;
        fEntryKind = LocalFileDetails::kFileKindDiskImage;
        break;
    default:
        ASSERT(false);
        // was initialized to default earlier
        break;
    }

/*bail:*/
    return kNuErrNone;
}

const NuFileDetails& GenericArchive::LocalFileDetails::GetNuFileDetails()
{
    //details.threadID = threadID;
    switch (fEntryKind) {
    case kFileKindDataFork:
        fNuFileDetails.threadID = kNuThreadIDDataFork;
        break;
    case kFileKindBothForks:    // not exactly supported, doesn't really matter
    case kFileKindRsrcFork:
        fNuFileDetails.threadID = kNuThreadIDRsrcFork;
        break;
    case kFileKindDiskImage:
        fNuFileDetails.threadID = kNuThreadIDDiskImage;
        break;
    case kFileKindDirectory:
    default:
        LOGW("Invalid entryKind (%d) for NuFileDetails conversion", fEntryKind);
        ASSERT(false);
        fNuFileDetails.threadID = 0;    // that makes it an old-style comment?!
        break;
    }

    fNuFileDetails.origName = (LPCWSTR) fLocalPathName;
    fNuFileDetails.storageNameMOR = (LPCSTR) fStoragePathNameMOR;
    fNuFileDetails.fileSysInfo = fFssep;
    fNuFileDetails.access = fAccess;
    fNuFileDetails.fileType = fFileType;
    fNuFileDetails.extraType = fExtraType;
    fNuFileDetails.storageType = fStorageType;
    fNuFileDetails.createWhen = fCreateWhen;
    fNuFileDetails.modWhen = fModWhen;
    fNuFileDetails.archiveWhen = fArchiveWhen;

    switch (fFileSysFmt) {
    case DiskImg::kFormatProDOS:
    case DiskImg::kFormatDOS33:
    case DiskImg::kFormatDOS32:
    case DiskImg::kFormatPascal:
    case DiskImg::kFormatMacHFS:
    case DiskImg::kFormatMacMFS:
    case DiskImg::kFormatLisa:
    case DiskImg::kFormatCPM:
    //kFormatCharFST
    case DiskImg::kFormatMSDOS:
    //kFormatHighSierra
    case DiskImg::kFormatISO9660:
        /* these map directly */
        fNuFileDetails.fileSysID = (enum NuFileSysID) fFileSysFmt;
        break;

    case DiskImg::kFormatRDOS33:
    case DiskImg::kFormatRDOS32:
    case DiskImg::kFormatRDOS3:
        /* these look like DOS33, e.g. text is high-ASCII */
        fNuFileDetails.fileSysID = kNuFileSysDOS33;
        break;

    default:
        fNuFileDetails.fileSysID = kNuFileSysUnknown;
        break;
    }

    return fNuFileDetails;
}

const DiskFS::CreateParms& GenericArchive::LocalFileDetails::GetCreateParms()
{
    fCreateParms.pathName = (LPCSTR) fStoragePathNameMOR;
    fCreateParms.fssep = fFssep;
    fCreateParms.storageType = fStorageType;
    fCreateParms.fileType = fFileType;
    fCreateParms.auxType = fExtraType;
    fCreateParms.access = fAccess;
    fCreateParms.createWhen = NufxArchive::DateTimeToSeconds(&fCreateWhen);
    fCreateParms.modWhen = NufxArchive::DateTimeToSeconds(&fModWhen);
    return fCreateParms;
}

void GenericArchive::LocalFileDetails::GenerateStoragePathName()
{
    // TODO(Unicode): generate MOR name from Unicode, instead of just
    //  doing a generic CP-1252 conversion.  We need to do this on both
    //  sides though, so until we can extract MOR->Unicode we don't
    //  want to add Unicode->MOR.  For this all to work well we need NufxLib
    //  and DiskImgLib to be able to handle UTF-16 filenames.
    fStoragePathNameMOR = fStrippedLocalPathName;
}


/*static*/ void GenericArchive::LocalFileDetails::CopyFields(LocalFileDetails* pDst,
    const LocalFileDetails* pSrc)
{
    // don't copy fNuFileDetails, fCreateParms
    pDst->fEntryKind = pSrc->fEntryKind;
    pDst->fLocalPathName = pSrc->fLocalPathName;
    pDst->fStrippedLocalPathName = pSrc->fStrippedLocalPathName;
    pDst->fStoragePathNameMOR = pSrc->fStoragePathNameMOR;
    pDst->fFileSysFmt = pSrc->fFileSysFmt;
    pDst->fFssep = pSrc->fFssep;
    pDst->fAccess = pSrc->fAccess;
    pDst->fFileType = pSrc->fFileType;
    pDst->fExtraType = pSrc->fExtraType;
    pDst->fStorageType = pSrc->fStorageType;
    pDst->fCreateWhen = pSrc->fCreateWhen;
    pDst->fModWhen = pSrc->fModWhen;
    pDst->fArchiveWhen = pSrc->fArchiveWhen;
}


/*
 * ===========================================================================
 *      SelectionSet
 * ===========================================================================
 */

void SelectionSet::CreateFromSelection(ContentList* pContentList, int threadMask)
{
    /*
     * This grabs the items in the order in which they appear in the display
     * (at least under Win2K), which is a good thing.  It appears that, if you
     * just grab indices 0..N, you will get them in order.
     */
    LOGD("CreateFromSelection (threadMask=0x%02x)", threadMask);

    POSITION posn;
    posn = pContentList->GetFirstSelectedItemPosition();
    ASSERT(posn != NULL);
    if (posn == NULL)
        return;
    while (posn != NULL) {
        int num = pContentList->GetNextSelectedItem(/*ref*/ posn);
        GenericEntry* pEntry = (GenericEntry*) pContentList->GetItemData(num);

        AddToSet(pEntry, threadMask);
    }
}

void SelectionSet::CreateFromAll(ContentList* pContentList, int threadMask)
{
    LOGD("CreateFromAll (threadMask=0x%02x)", threadMask);

    int count = pContentList->GetItemCount();
    for (int idx = 0; idx < count; idx++) {
        GenericEntry* pEntry = (GenericEntry*) pContentList->GetItemData(idx);

        AddToSet(pEntry, threadMask);
    }
}

void SelectionSet::AddToSet(GenericEntry* pEntry, int threadMask)
{
    SelectionEntry* pSelEntry;

    LOGV("  Sel '%ls'", (LPCWSTR) pEntry->GetPathNameUNI());

    if (!(threadMask & GenericEntry::kAllowVolumeDir) &&
         pEntry->GetRecordKind() == GenericEntry::kRecordKindVolumeDir)
    {
        /* only include volume dir if specifically requested */
        //LOGI(" Excluding volume dir '%ls' from set", pEntry->GetPathName());
        return;
    }

    if (!(threadMask & GenericEntry::kAllowDirectory) &&
        pEntry->GetRecordKind() == GenericEntry::kRecordKindDirectory)
    {
        /* only include directories if specifically requested */
        //LOGI(" Excluding folder '%ls' from set", pEntry->GetPathName());
        return;
    }

    if (!(threadMask & GenericEntry::kAllowDamaged) && pEntry->GetDamaged())
    {
        /* only include "damaged" files if specifically requested */
        return;
    }

    bool doAdd = false;

    if (threadMask & GenericEntry::kAnyThread)
        doAdd = true;

    if ((threadMask & GenericEntry::kCommentThread) && pEntry->GetHasComment())
        doAdd = true;
    if ((threadMask & GenericEntry::kDataThread) && pEntry->GetHasDataFork())
        doAdd = true;
    if ((threadMask & GenericEntry::kRsrcThread) && pEntry->GetHasRsrcFork())
        doAdd = true;
    if ((threadMask & GenericEntry::kDiskImageThread) && pEntry->GetHasDiskImage())
        doAdd = true;

    if (doAdd) {
        pSelEntry = new SelectionEntry(pEntry);
        AddEntry(pSelEntry);
    }
}

void SelectionSet::AddEntry(SelectionEntry* pEntry)
{
    if (fEntryHead == NULL) {
        ASSERT(fEntryTail == NULL);
        fEntryHead = pEntry;
        fEntryTail = pEntry;
        ASSERT(pEntry->GetPrev() == NULL);
        ASSERT(pEntry->GetNext() == NULL);
    } else {
        ASSERT(fEntryTail != NULL);
        ASSERT(pEntry->GetPrev() == NULL);
        pEntry->SetPrev(fEntryTail);
        ASSERT(fEntryTail->GetNext() == NULL);
        fEntryTail->SetNext(pEntry);
        fEntryTail = pEntry;
    }

    fNumEntries++;
}

void SelectionSet::DeleteEntries(void)
{
    SelectionEntry* pEntry;
    SelectionEntry* pNext;

    LOGD("Deleting selection entries");

    pEntry = GetEntries();
    while (pEntry != NULL) {
        pNext = pEntry->GetNext();
        delete pEntry;
        pEntry = pNext;
    }
}

int SelectionSet::CountMatchingPrefix(const WCHAR* prefix)
{
    SelectionEntry* pEntry;
    int count = 0;
    int len = wcslen(prefix);
    ASSERT(len > 0);

    pEntry = GetEntries();
    while (pEntry != NULL) {
        GenericEntry* pGeneric = pEntry->GetEntry();

        if (wcsnicmp(prefix, pGeneric->GetDisplayName(), len) == 0)
            count++;
        pEntry = pEntry->GetNext();
    }

    return count;
}

void SelectionSet::Dump(void)
{
    const SelectionEntry* pEntry;

    LOGI("SelectionSet: %d entries", fNumEntries);

    pEntry = fEntryHead;
    while (pEntry != NULL) {
        LOGI("  : name='%ls'", (LPCWSTR) pEntry->GetEntry()->GetPathNameUNI());
        pEntry = pEntry->GetNext();
    }
}
