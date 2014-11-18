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
#include "FileNameConv.h"
#include "ContentList.h"
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

/*
 * Initialize all data members.
 */
GenericEntry::GenericEntry(void)
{
    fPathName = NULL;
    fFileName = NULL;
    fFssep = '\0';
    fSubVolName = NULL;
    fDisplayName = NULL;
    fFileType = 0;
    fAuxType = 0;
    fAccess = 0;
    fModWhen = kDateNone;
    fCreateWhen = kDateNone;
    fRecordKind = kRecordKindUnknown;
    fFormatStr = L"Unknown";
    fCompressedLen = 0;
    //fUncompressedLen = 0;
    fDataForkLen = fRsrcForkLen = 0;

    fSourceFS = DiskImg::kFormatUnknown;

    fHasDataFork = false;
    fHasRsrcFork = false;
    fHasDiskImage = false;
    fHasComment = false;
    fHasNonEmptyComment = false;

    fIndex = -1;
    fpPrev = NULL;
    fpNext = NULL;

    fDamaged = fSuspicious = false;
}

/*
 * Throw out anything we allocated.
 */
GenericEntry::~GenericEntry(void)
{
    delete[] fPathName;
    delete[] fSubVolName;
    delete[] fDisplayName;
}

/*
 * Pathname getters and setters.
 */
void
GenericEntry::SetPathName(const WCHAR* path)
{
    ASSERT(path != NULL && wcslen(path) > 0);
    if (fPathName != NULL)
        delete fPathName;
    fPathName = wcsdup(path);
    // nuke the derived fields
    fFileName = NULL;
    fFileNameExtension = NULL;
    delete[] fDisplayName;
    fDisplayName = NULL;

    /*
     * Warning: to be 100% pedantically correct here, we should NOT do this
     * if the fssep char is '_'.  However, that may not have been set by
     * the time we got here, so to do this "correctly" we'd need to delay
     * the underscorage until the first GetPathName call.
     */
    const Preferences* pPreferences = GET_PREFERENCES();
    if (pPreferences->GetPrefBool(kPrSpacesToUnder))
        SpacesToUnderscores(fPathName);
}
const WCHAR*
GenericEntry::GetFileName(void)
{
    ASSERT(fPathName != NULL);
    if (fFileName == NULL)
        fFileName = PathName::FilenameOnly(fPathName, fFssep);
    return fFileName;
}
const WCHAR*
GenericEntry::GetFileNameExtension(void)
{
    ASSERT(fPathName != NULL);
    if (fFileNameExtension == NULL)
        fFileNameExtension = PathName::FindExtension(fPathName, fFssep);
    return fFileNameExtension;
}
CStringA
GenericEntry::GetFileNameExtensionA(void)
{
    return GetFileNameExtension();
}
void
GenericEntry::SetSubVolName(const WCHAR* name)
{
    delete[] fSubVolName;
    fSubVolName = NULL;
    if (name != NULL) {
        fSubVolName = wcsdup(name);
    }
}
const WCHAR*
GenericEntry::GetDisplayName(void) const
{
    ASSERT(fPathName != NULL);
    if (fDisplayName != NULL)
        return fDisplayName;

    // TODO: hmm...
    GenericEntry* pThis = const_cast<GenericEntry*>(this);

    int len = wcslen(fPathName) +1;
    if (fSubVolName != NULL)
        len += wcslen(fSubVolName) +1;
    pThis->fDisplayName = new WCHAR[len];
    if (fSubVolName != NULL) {
        WCHAR xtra[2] = { DiskFS::kDIFssep, '\0' };
        wcscpy(pThis->fDisplayName, fSubVolName);
        wcscat(pThis->fDisplayName, xtra);
    } else {
        pThis->fDisplayName[0] = '\0';
    }
    wcscat(pThis->fDisplayName, fPathName);
    return pThis->fDisplayName;
}

/*
 * Get a string for this entry's filetype.
 */
const WCHAR*
GenericEntry::GetFileTypeString(void) const
{
    return PathProposal::FileTypeString(fFileType);
}

/*
 * Convert spaces to underscores.
 */
/*static*/ void
GenericEntry::SpacesToUnderscores(WCHAR* buf)
{
    while (*buf != '\0') {
        if (*buf == ' ')
            *buf = '_';
        buf++;
    }
}


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
/*static*/ bool
GenericEntry::CheckHighASCII(const unsigned char* buffer,
    unsigned long count)
{
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

#define kNuMaxUpperASCII    1       /* max #of binary chars per 100 bytes */
#define kMinConvThreshold   40      /* min of 40 chars for auto-detect */
#define kCharLF             '\n'
#define kCharCR             '\r'
/*
 * Decide, based on the contents of the buffer, whether we should do an
 * EOL conversion on the data.
 *
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
 *
 * Returns kConvEOLOff or kConvEOLOn.
 */
/*static*/ GenericEntry::ConvertEOL
GenericEntry::DetermineConversion(const unsigned char* buffer, long count,
    EOLType* pSourceType, ConvertHighASCII* pConvHA)
{
    ConvertHighASCII wantConvHA = *pConvHA;
    long bufCount, numBinary, numLF, numCR;
    bool isHighASCII;
    unsigned char val;

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
static inline void
PutEOL(FILE* fp)
{
    putc(kCharCR, fp);
    putc(kCharLF, fp);
}

/*
 * Write data to a file, possibly converting EOL markers to Windows CRLF
 * and stripping high ASCII.
 *
 * If "*pConv" is kConvertEOLAuto, this will try to auto-detect whether
 * the input is a text file or not by scanning the input buffer.
 *
 * Ditto for "*pConvHA".
 *
 * "fp" is the output file, "buf" is the input, "len" is the buffer length.
 * "*pLastCR" should initially be "false", and carried across invocations.
 *
 * Returns 0 on success, or an errno value on error.
 */
/*static*/ int
GenericEntry::WriteConvert(FILE* fp, const char* buf, size_t len,
    ConvertEOL* pConv, ConvertHighASCII* pConvHA, bool* pLastCR)
{
    int err = 0;

    LOGI("+++ WriteConvert conv=%d convHA=%d", *pConv, *pConvHA);

    if (len == 0) {
        LOGI("WriteConvert asked to write 0 bytes; returning");
        return err;
    }

    /* if we're in "auto" mode, scan the input for EOL and high ASCII */
    if (*pConv == kConvertEOLAuto) {
        EOLType sourceType;
        *pConv = DetermineConversion((unsigned char*)buf, len, &sourceType,
                    pConvHA);
        if (*pConv == kConvertEOLOn && sourceType == kEOLCRLF) {
            LOGI(" Auto-detected text conversion from CRLF; disabling");
            *pConv = kConvertEOLOff;
        }
        LOGI(" Auto-detected EOL conv=%d ha=%d", *pConv, *pConvHA);
    } else if (*pConvHA == kConvertHAAuto) {
        if (*pConv == kConvertEOLOn) {
            /* definitely converting EOL, test for high ASCII */
            if (CheckHighASCII((unsigned char*)buf, len))
                *pConvHA = kConvertHAOn;
            else
                *pConvHA = kConvertHAOff;
        } else {
            /* not converting EOL, don't convert high ASCII */
            *pConvHA = kConvertHAOff;
        }
    }
    LOGI("+++  After auto, conv=%d convHA=%d", *pConv, *pConvHA);
    ASSERT(*pConv == kConvertEOLOn || *pConv == kConvertEOLOff);
    ASSERT(*pConvHA == kConvertHAOn || *pConvHA == kConvertHAOff);

    /* write the output */
    if (*pConv == kConvertEOLOff) {
        if (fwrite(buf, len, 1, fp) != 1) {
            err = errno;
            LOGI("WriteConvert failed, err=%d", errno);
        }
    } else {
        ASSERT(*pConv == kConvertEOLOn);
        bool lastCR = *pLastCR;
        unsigned char uch;
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

/*
 * Add a new entry to the end of the list.
 */
void
GenericArchive::AddEntry(GenericEntry* pEntry)
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

/*
 * Delete the "entries" list.
 */
void
GenericArchive::DeleteEntries(void)
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

/*
 * Generate a temp name from a file name.
 *
 * The key is to come up with the name of a temp file in the same directory
 * (or at least on the same disk volume) so that the temp file can be
 * renamed on top of the original.
 *
 * Windows _mktemp does appear to test for the existence of the file, which
 * is good.  It doesn't actually open the file, which creates a small window
 * in which bad things could happen, but it should be okay.
 */
/*static*/ CString
GenericArchive::GenDerivedTempName(const WCHAR* filename)
{
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
    LOGI("GenDerived: passed '%ls' returned '%ls'", filename, (LPCWSTR) mangle);

    return mangle;
}


/*
 * Do a strcasecmp-like comparison, taking equivalent fssep chars into
 * account.
 *
 * The tricky part is with files like "foo:bar" ':' -- "foo:bar" '/'.  The
 * names appear to match, but the fssep chars are different, so they don't.
 * If we just return (char1 - char2), though, we'll be returning 0 because
 * the ASCII values match even if the character *meanings* don't.
 *
 * This assumes that the fssep char is not affected by tolower().
 *
 * [This may not sort correctly...haven't verified that I'm returning the
 * right thing for ascending ASCII sort.]
 */
/*static*/ int
GenericArchive::ComparePaths(const CString& name1, char fssep1,
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
 * This comes straight out of NuLib2, and uses NufxLib data structures.  While
 * it may seem strange to use these structures for non-NuFX archives, they are
 * convenient and hold at least as much information as any other format needs.
 */

typedef bool Boolean;

/*
 * Convert from time in seconds to Apple IIgs DateTime format.
 */
/*static*/ void
GenericArchive::UNIXTimeToDateTime(const time_t* pWhen, NuDateTime* pDateTime)
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
 * Set the contents of a NuFileDetails structure, based on the pathname
 * and characteristics of the file.
 *
 * For efficiency and simplicity, the pathname fields are set to CStrings in
 * the GenericArchive object instead of newly-allocated storage.
 */
NuError
GenericArchive::GetFileDetails(const AddFilesDialog* pAddOpts,
    const WCHAR* pathname, struct _stat* psb, FileDetails* pDetails)
{
    //char* livePathStr;
    time_t now;

    ASSERT(pAddOpts != NULL);
    ASSERT(pathname != NULL);
    ASSERT(pDetails != NULL);

    /* init to defaults */
    //pDetails->threadID = kNuThreadIDDataFork;
    pDetails->entryKind = FileDetails::kFileKindDataFork;
    //pDetails->fileSysID = kNuFileSysUnknown;
    pDetails->fileSysFmt = DiskImg::kFormatUnknown;
    pDetails->fileSysInfo = PathProposal::kDefaultStoredFssep;
    pDetails->fileType = 0;
    pDetails->extraType = 0;
    pDetails->storageType = kNuStorageUnknown;  /* let NufxLib worry about it */
    if (psb->st_mode & S_IWUSR)
        pDetails->access = kNuAccessUnlocked;
    else
        pDetails->access = kNuAccessLocked;

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
    UNIXTimeToDateTime(&now, &pDetails->archiveWhen);
    UNIXTimeToDateTime(&psb->st_mtime, &pDetails->modWhen);
    UNIXTimeToDateTime(&psb->st_ctime, &pDetails->createWhen);

    /* get adjusted filename, along with any preserved type info */
    PathProposal pathProp;
    pathProp.Init(pathname);
    pathProp.LocalToArchive(pAddOpts);

    /* set up the local and archived pathnames */
    pDetails->storageName = "";
    if (!pAddOpts->fStoragePrefix.IsEmpty()) {
        pDetails->storageName += pAddOpts->fStoragePrefix;
        pDetails->storageName += pathProp.fStoredFssep;
    }
    pDetails->storageName += pathProp.fStoredPathName;

    /*
     * Fill in the NuFileDetails struct.
     *
     * We use GetBuffer to get the string to ensure that the CString object
     * doesn't do anything while we're not looking.  The string won't be
     * modified, and it won't be around for very long, so it's not strictly
     * necessary to do this.  It is, however, the correct approach.
     */
    pDetails->origName = pathname;
    pDetails->fileSysInfo = pathProp.fStoredFssep;
    pDetails->fileType = pathProp.fFileType;
    pDetails->extraType = pathProp.fAuxType;
    switch (pathProp.fThreadKind) {
    case GenericEntry::kDataThread:
        //pDetails->threadID = kNuThreadIDDataFork;
        pDetails->entryKind = FileDetails::kFileKindDataFork;
        break;
    case GenericEntry::kRsrcThread:
        //pDetails->threadID = kNuThreadIDRsrcFork;
        pDetails->entryKind = FileDetails::kFileKindRsrcFork;
        break;
    case GenericEntry::kDiskImageThread:
        //pDetails->threadID = kNuThreadIDDiskImage;
        pDetails->entryKind = FileDetails::kFileKindDiskImage;
        break;
    default:
        ASSERT(false);
        // was initialized to default earlier
        break;
    }

/*bail:*/
    return kNuErrNone;
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

/*
 * Prepare a directory for reading.
 *
 * Allocates a Win32dirent struct that must be freed by the caller.
 */
Win32dirent*
GenericArchive::OpenDir(const WCHAR* name)
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

/*
 * Get an entry from an open directory.
 *
 * Returns a NULL pointer after the last entry has been read.
 */
Win32dirent*
GenericArchive::ReadDir(Win32dirent* dir)
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

/*
 * Close a directory.
 */
void
GenericArchive::CloseDir(Win32dirent* dir)
{
    if (dir == NULL)
        return;

    FindClose(dir->d_hFindFile);
    free(dir);
}

/*
 * Win32 recursive directory descent.  Scan the contents of a directory.
 * If a subdirectory is found, follow it; otherwise, call Win32AddFile to
 * add the file.
 */
NuError
GenericArchive::Win32AddDirectory(const AddFilesDialog* pAddOpts,
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

/*
 * Add a file to the list we're adding to the archive.  If it's a directory,
 * and the recursive descent feature is enabled, call Win32AddDirectory to
 * add the contents of the dir.
 *
 * Returns with an error if the file doesn't exist or isn't readable.
 */
NuError
GenericArchive::Win32AddFile(const AddFilesDialog* pAddOpts,
    const WCHAR* pathname, CString* pErrMsg)
{
    NuError err = kNuErrNone;
    Boolean exists, isDir, isReadable;
    FileDetails details;
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
    LOGI("+++ ADD '%ls'", pathname);

    /*
     * Fill out the "details" structure.  The class has an automatic
     * conversion to NuFileDetails, but it relies on the CString storage
     * in the FileDetails, so be careful how you use it.
     */
    err = GetFileDetails(pAddOpts, pathname, &sb, &details);
    if (err != kNuErrNone)
        goto bail;

    assert(wcscmp(pathname, details.origName) == 0);
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

/*
 * External entry point; just calls the system-specific version.
 *
 * [ I figure the GS/OS version will want to pass a copy of the file
 *   info from the GSOSAddDirectory function back into GSOSAddFile, so we'd
 *   want to call it from here with a NULL pointer indicating that we
 *   don't yet have the file info.  That way we can get the file info
 *   from the directory read call and won't have to check it again in
 *   GSOSAddFile. ]
 */
NuError
GenericArchive::AddFile(const AddFilesDialog* pAddOpts, const WCHAR* pathname,
    CString* pErrMsg)
{
    *pErrMsg = "";
    return Win32AddFile(pAddOpts, pathname, pErrMsg);
}

/*
 * ===========================================================================
 *      GenericArchive::FileDetails
 * ===========================================================================
 */

/*
 * Constructor.
 */
GenericArchive::FileDetails::FileDetails(void)
{
    //threadID = 0;
    entryKind = kFileKindUnknown;
    fileSysFmt = DiskImg::kFormatUnknown;
    fileSysInfo = storageType = 0;
    access = fileType = extraType = 0;
    memset(&createWhen, 0, sizeof(createWhen));
    memset(&modWhen, 0, sizeof(modWhen));
    memset(&archiveWhen, 0, sizeof(archiveWhen));
}

/*
 * Automatic cast conversion to NuFileDetails.
 *
 * Note the NuFileDetails will have a string pointing into our storage.
 * This is not a good thing, but it's tough to work around.
 */
GenericArchive::FileDetails::operator const NuFileDetails() const
{
    NuFileDetails details;

    //details.threadID = threadID;
    switch (entryKind) {
    case kFileKindDataFork:
        details.threadID = kNuThreadIDDataFork;
        break;
    case kFileKindBothForks:    // not exactly supported, doesn't really matter
    case kFileKindRsrcFork:
        details.threadID = kNuThreadIDRsrcFork;
        break;
    case kFileKindDiskImage:
        details.threadID = kNuThreadIDDiskImage;
        break;
    case kFileKindDirectory:
    default:
        LOGI("Invalid entryKind (%d) for NuFileDetails conversion",
            entryKind);
        ASSERT(false);
        details.threadID = 0;       // that makes it an old-style comment?!
        break;
    }

    // TODO(xyzzy): need narrow-string versions of origName and storageName
    //  (probably need to re-think this automatic-cast-conversion stuff)
    details.origName = "XYZZY-GenericArchive1"; // origName;
    details.storageName = "XYZZY-GenericArchive2"; // storageName;
    //details.fileSysID = fileSysID;
    details.fileSysInfo = fileSysInfo;
    details.access = access;
    details.fileType = fileType;
    details.extraType = extraType;
    details.storageType = storageType;
    details.createWhen = createWhen;
    details.modWhen = modWhen;
    details.archiveWhen = archiveWhen;

    switch (fileSysFmt) {
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
        details.fileSysID = (enum NuFileSysID) fileSysFmt;
        break;

    case DiskImg::kFormatRDOS33:
    case DiskImg::kFormatRDOS32:
    case DiskImg::kFormatRDOS3:
        /* these look like DOS33, e.g. text is high-ASCII */
        details.fileSysID = kNuFileSysDOS33;
        break;

    default:
        details.fileSysID = kNuFileSysUnknown;
        break;
    }


    // Return stack copy, which copies into compiler temporary with our
    // copy constructor.
    return details;
}

/*
 * Copy the contents of our object to a new object.
 *
 * Useful for operator= and copy construction.
 */
/*static*/ void
GenericArchive::FileDetails::CopyFields(FileDetails* pDst,
    const FileDetails* pSrc)
{
    //pDst->threadID = pSrc->threadID;
    pDst->entryKind = pSrc->entryKind;
    pDst->origName = pSrc->origName;
    pDst->storageName = pSrc->storageName;
    pDst->fileSysFmt = pSrc->fileSysFmt;
    pDst->fileSysInfo = pSrc->fileSysInfo;
    pDst->access = pSrc->access;
    pDst->fileType = pSrc->fileType;
    pDst->extraType = pSrc->extraType;
    pDst->storageType = pSrc->storageType;
    pDst->createWhen = pSrc->createWhen;
    pDst->modWhen = pSrc->modWhen;
    pDst->archiveWhen = pSrc->archiveWhen;
}


/*
 * ===========================================================================
 *      SelectionSet
 * ===========================================================================
 */

/*
 * Create a selection set from the selected items in a ContentList.
 *
 * This grabs the items in the order in which they appear in the display
 * (at least under Win2K), which is a good thing.  It appears that, if you
 * just grab indices 0..N, you will get them in order.
 */
void
SelectionSet::CreateFromSelection(ContentList* pContentList, int threadMask)
{
    LOGI("CreateFromSelection (threadMask=0x%02x)", threadMask);

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

/*
 * Like CreateFromSelection, but includes the entire list.
 */
void
SelectionSet::CreateFromAll(ContentList* pContentList, int threadMask)
{
    LOGI("CreateFromAll (threadMask=0x%02x)", threadMask);

    int count = pContentList->GetItemCount();
    for (int idx = 0; idx < count; idx++) {
        GenericEntry* pEntry = (GenericEntry*) pContentList->GetItemData(idx);

        AddToSet(pEntry, threadMask);
    }
}

/*
 * Add a GenericEntry to the set, but only if we can find a thread that
 * matches the flags in "threadMask".
 */
void
SelectionSet::AddToSet(GenericEntry* pEntry, int threadMask)
{
    SelectionEntry* pSelEntry;

    //LOGI("  Sel '%ls'", pEntry->GetPathName());

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

/*
 * Add a new entry to the end of the list.
 */
void
SelectionSet::AddEntry(SelectionEntry* pEntry)
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

/*
 * Delete the "entries" list.
 */
void
SelectionSet::DeleteEntries(void)
{
    SelectionEntry* pEntry;
    SelectionEntry* pNext;

    LOGI("Deleting selection entries");

    pEntry = GetEntries();
    while (pEntry != NULL) {
        pNext = pEntry->GetNext();
        delete pEntry;
        pEntry = pNext;
    }
}

/*
 * Count the #of entries whose display name matches the prefix string.
 */
int
SelectionSet::CountMatchingPrefix(const WCHAR* prefix)
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

/*
 * Dump the contents of a selection set.
 */
void
SelectionSet::Dump(void)
{
    const SelectionEntry* pEntry;

    LOGI("SelectionSet: %d entries", fNumEntries);

    pEntry = fEntryHead;
    while (pEntry != NULL) {
        LOGI("  : name='%ls'", pEntry->GetEntry()->GetPathName());
        pEntry = pEntry->GetNext();
    }
}
