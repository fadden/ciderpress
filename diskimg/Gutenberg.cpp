/*
 * CiderPress
 * Copyright (C) 2009 by CiderPress authors.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of Gutenberg disk format (used by the Gutenberg and
 * Gutenberg Jr. word processors).
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * ===========================================================================
 *      DiskFSGutenberg
 * ===========================================================================
 */

/*
The Gutenberg disk format embeds the file structure meta-data into the disk
sectors themselves, rather than having a separate track/sector list.  The
first six bytes in every sector are:

+00 previous track in this file (+$80 indicates link not valid?)
+01 previous sector
+02 current track (+$80 indicates start of file)
+03 current sector
+04 next track (+$80 indicates end of file)
+05 next sector

The files are circular -- the "next" and "previous" links will wrap around --
so you have to test the high bit to see if you've reached an end.

The disk catalog works the same way, and is present as the first file on
the disk (called "DIR").  (It's not quite the same -- the "previous" pointer
in the first sector just points to the first sector.)  The catalog begins
at track 17 sector 7, and skips around the disk.

The boot area is not represented by a file, and does not include the
embedded T/S links.

Each directory entry is 16 bytes, and lays out rather nicely in the sector
editor:

 00: 11 07 11 87 15 0e c7 c2 af cd c1 d3 d4 c5 d2 8d  ......GB/MASTER.
 10: c4 c9 d2 a0 a0 a0 a0 a0 a0 a0 a0 a0 11 07 cc 8d  DIR         ..L.
 20: c3 cf d0 d9 a0 a0 a0 a0 a0 a0 a0 a0 10 40 d0 8d  COPY        .@P.
 30: c3 cf d0 d9 c1 cc cc a0 a0 a0 a0 a0 10 04 d0 8d  COPYALL     ..P.

This shows the six T/S bytes, followed by a nine-character volume name.
The fact that each entry ends in 0x8d is likely a deliberate attempt to
make the file readable as high-ASCII text, with one entry per line.

The regular directory entries start at 0x10.  The file name is 12 bytes,
followed by the track and sector of the start of the file.  Note "DIR"
starts at track 17 sector 7, as expected. The next entry, COPY, has 0x40 for
its sector number, indicating that it has been deleted.  (Some entries on
Gutenberg Jr. disks use 0x40 for the track number instead?)

The next value is one of 'L', 'P', 'M', or ' ' (0xcc, 0xd0, 0xcd, 0xa0).
Some files have text, some have fonts, some have executable code (a short
header followed by 6502 instructions).  There's no apparent link between
the value and the type of data in the file.
*/

const int kMaxSectors = 32;
const int kMaxVolNameLen = 9;
const int kSctSize = 256;
const int kVTOCTrack = 17;
const int kVTOCSector = 7;
const int kCatalogEntryOffset = 0x10;   // first entry in cat sect starts here
const int kCatalogEntrySize = 16;       // length in bytes of catalog entries
const int kCatalogEntriesPerSect = 15;  // #of entries per catalog sector
const int kEntryDeleted = 0x40;     // this is used to designate deleted files
const int kEntryUnused = 0x00;      // this is track# in never-used entries
const int kMaxTSPairs = 0x7a;           // 122 entries for 256-byte sectors
const int kTSOffset = 0x0c;             // first T/S entry in a T/S list

const int kMaxTSIterations = 32;

/*
 * Get a pointer to the Nth entry in a catalog sector.
 */
static inline uint8_t* GetCatalogEntryPtr(uint8_t* basePtr, int entryNum)
{
    assert(entryNum >= 0 && entryNum < kCatalogEntriesPerSect);
    return basePtr + kCatalogEntryOffset + entryNum * kCatalogEntrySize;
}


/*
 * Test this image for Gutenberg-ness.
 *
 */
static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder,
    int* pGoodCount)
{
    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];
    int catTrack = kVTOCTrack;
    int catSect = kVTOCSector;
    int foundGood = 0;
    int iterations = 0;

    *pGoodCount = 0;

    /*
     * Walk through the catalog track to try to figure out ordering.
     */
    while (iterations < DiskFSGutenberg::kMaxCatalogSectors)
    {
        dierr = pImg->ReadTrackSectorSwapped(catTrack, catSect, sctBuf,
                    imageOrder, DiskImg::kSectorOrderDOS);
        if (dierr != kDIErrNone) {
            dierr = kDIErrNone;
            break;      /* allow it if earlier stuff was okay */
        }
        if (catTrack == (sctBuf[2] & 0x7f) && catSect == (sctBuf[3] & 0x7f)) {
            // current-sector values matched, check for the end-of-entry bits
            foundGood++;
            if (sctBuf[0x0f] == 0x8d && sctBuf[0x1f] == 0x8d &&
                sctBuf[0x2f] == 0x8d && sctBuf[0x3f] == 0x8d &&
                sctBuf[0x4f] == 0x8d && sctBuf[0x5f] == 0x8d &&
                sctBuf[0x6f] == 0x8d && sctBuf[0x7f] == 0x8d &&
                sctBuf[0x8f] == 0x8d && sctBuf[0x9f] == 0x8d)
            {
                foundGood++;
            }
        }
        catTrack = sctBuf[0x04];
        catSect = sctBuf[0x05];
        if ((catTrack & 0x80) != 0) {
            // full circle
            break;
        }
        iterations++;       // watch for infinite loops
    }
    if (iterations >= DiskFSGutenberg::kMaxCatalogSectors) {
        /* possible cause: LF->CR conversion screws up link to sector $0a */
        dierr = kDIErrDirectoryLoop;
        LOGI("  Gutenberg directory links cause a loop (order=%d)", imageOrder);
        goto bail;
    }

    LOGI(" Gutenberg foundGood=%d order=%d", foundGood, imageOrder);
    *pGoodCount = foundGood;

bail:
    return dierr;
}

/*
 * Test to see if the image is a Gutenberg word processor data disk.
 */
/*static*/ DIError DiskFSGutenberg::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    if (pImg->GetNumTracks() > kMaxInterestingTracks)
        return kDIErrFilesystemNotFound;

    DiskImg::SectorOrder ordering[DiskImg::kSectorOrderMax];
    
    DiskImg::GetSectorOrderArray(ordering, *pOrder);

    DiskImg::SectorOrder bestOrder = DiskImg::kSectorOrderUnknown;
    int bestCount = 0;

    for (int i = 0; i < DiskImg::kSectorOrderMax; i++) {
        int goodCount = 0;

        if (ordering[i] == DiskImg::kSectorOrderUnknown)
            continue;
        if (TestImage(pImg, ordering[i], &goodCount) == kDIErrNone) {
            if (goodCount > bestCount) {
                bestCount = goodCount;
                bestOrder = ordering[i];
            }
        }
    }

    if (bestCount >= 2 ||
        (leniency == kLeniencyVery && bestCount >= 1))
    {
        LOGI(" Gutenberg test: bestCount=%d for order=%d", bestCount, bestOrder);
        assert(bestOrder != DiskImg::kSectorOrderUnknown);
        *pOrder = bestOrder;
        *pFormat = DiskImg::kFormatGutenberg;
        return kDIErrNone;
    }

    LOGI(" Gutenberg didn't find a valid filesystem.");
    return kDIErrFilesystemNotFound;
}


/*
 * Get things rolling.
 *
 * Since we're assured that this is a valid disk, errors encountered from here
 * on out must be handled somehow, possibly by claiming that the disk is
 * completely full and has no files on it.
 */
DIError DiskFSGutenberg::Initialize(InitMode initMode)
{
    DIError dierr = kDIErrNone;

    fVolumeUsage.Create(fpImg->GetNumTracks(), fpImg->GetNumSectPerTrack());

    /* read the contents of the catalog, creating our A2File list */
    dierr = ReadCatalog();
    if (dierr != kDIErrNone)
        goto bail;

    /* run through and get file lengths and data offsets */
    dierr = GetFileLengths();
    if (dierr != kDIErrNone)
        goto bail;

    sprintf(fDiskVolumeID, "Gutenberg: %s", fDiskVolumeName);

    fDiskIsGood = CheckDiskIsGood();

    fVolumeUsage.Dump();

bail:
    return dierr;
}

/*
 * Get the amount of free space remaining.
 */
DIError DiskFSGutenberg::GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
    int* pUnitSize) const
{
    *pTotalUnits = fpImg->GetNumTracks() * fpImg->GetNumSectPerTrack();
    *pFreeUnits = 0;
    *pUnitSize = kSectorSize;
    return kDIErrNone;
}

/*
 * Read the disk's catalog.
 */
DIError DiskFSGutenberg::ReadCatalog(void)
{
    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];
    int catTrack, catSect;
    int iterations;

    catTrack = kVTOCTrack;
    catSect = kVTOCSector;
    iterations = 0;

    memset(fCatalogSectors, 0, sizeof(fCatalogSectors));

    while (catTrack < 35 && catSect < 16 && iterations < kMaxCatalogSectors)
    {
        LOGD(" Gutenberg reading catalog sector T=%d S=%d", catTrack, catSect);
        dierr = fpImg->ReadTrackSector(catTrack, catSect, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;
        memcpy(fDiskVolumeName, &sctBuf[6], kMaxVolNameLen);    // Copy out the volume name; it should be the same on all catalog sectors.
        fDiskVolumeName[kMaxVolNameLen] = 0x00;
        DiskFSGutenberg::LowerASCII((uint8_t*)fDiskVolumeName, kMaxVolNameLen);
        A2FileGutenberg::TrimTrailingSpaces(fDiskVolumeName);

        dierr = ProcessCatalogSector(catTrack, catSect, sctBuf);
        if (dierr != kDIErrNone)
            goto bail;

        fCatalogSectors[iterations].track = catTrack;
        fCatalogSectors[iterations].sector = catSect;

        catTrack = sctBuf[0x04];
        catSect = sctBuf[0x05];

        iterations++;       // watch for infinite loops

    }
    if (iterations >= kMaxCatalogSectors) {
        dierr = kDIErrDirectoryLoop;
        goto bail;
    }

bail:
    return dierr;
}

/*
 * Process the list of files in one sector of the catalog.
 *
 * Pass in the track, sector, and the contents of that track and sector.
 * (We only use "catTrack" and "catSect" to fill out some fields.)
 */
DIError DiskFSGutenberg::ProcessCatalogSector(int catTrack, int catSect,
    const uint8_t* sctBuf)
{
    A2FileGutenberg* pFile;
    const uint8_t* pEntry;
    int i;

    pEntry = &sctBuf[kCatalogEntryOffset];

    for (i = 0; i < kCatalogEntriesPerSect; i++) {
        if (pEntry[0x0c] != kEntryDeleted && pEntry[0x0d] != kEntryDeleted &&
            pEntry[0x00] != 0xa0 && pEntry[0x00] != 0x00)
        {
            pFile = new A2FileGutenberg(this);

            pFile->SetQuality(A2File::kQualityGood);

            pFile->fTrack = pEntry[0x0c];
            pFile->fSector = pEntry[0x0d];

            memcpy(pFile->fFileName, &pEntry[0x00], A2FileGutenberg::kMaxFileName);
            pFile->fFileName[A2FileGutenberg::kMaxFileName] = '\0';
            pFile->FixFilename();

            //pFile->fCatTS.track = catTrack;
            //pFile->fCatTS.sector = catSect;
            pFile->fCatEntryNum = i;

            /* can't do these yet, so just set to defaults */
            pFile->fLength = 0;
            pFile->fSparseLength = 0;
            pFile->fDataOffset = 0;
            pFile->fLengthInSectors = 0;
            pFile->fLengthInSectors = 0;

            AddFileToList(pFile);
        }
        //if (pEntry[0x00] == 0xa0)
        //  break;
        pEntry += kCatalogEntrySize;
    }

    return kDIErrNone;
}

/*
 * Perform consistency checks on the filesystem.
 *
 * Returns "true" if disk appears to be perfect, "false" otherwise.
 */
bool DiskFSGutenberg::CheckDiskIsGood(void)
{
    bool result = true;
    return result;
}

/*
 * Run through our list of files, computing the lengths and marking file
 * usage in the VolumeUsage object.
 */
DIError DiskFSGutenberg::GetFileLengths(void)
{
    A2FileGutenberg* pFile;
    uint8_t sctBuf[kSctSize];
    int tsCount = 0;
    uint16_t currentTrack, currentSector;

    pFile = (A2FileGutenberg*) GetNextFile(NULL);
    while (pFile != NULL) {
        DIError dierr;
        tsCount = 0;
        currentTrack = pFile->fTrack;
        currentSector = pFile->fSector;

        while (currentTrack < 0x80) {
            tsCount ++;
            dierr = fpImg->ReadTrackSector(currentTrack, currentSector, sctBuf);
            if (dierr != kDIErrNone) {
                LOGI("Gutenberg failed loading track/sector for '%s'",
                    pFile->GetPathName());
                goto bail;
            }
            currentTrack = sctBuf[0x04];
            currentSector = sctBuf[0x05];
        }
        pFile->fLengthInSectors = tsCount;
        pFile->fLength = tsCount * 250; // First six bytes of sector are t/s pointers

        pFile = (A2FileGutenberg*) GetNextFile(pFile);
    }

bail:
    return kDIErrNone;
}

/*
 * Convert high ASCII to low ASCII.
 *
 * Some people put inverse and flashing text into filenames, not to mention
 * control characters, so we have to cope with those too.
 *
 * We modify the first "len" bytes of "buf" in place.
 */
/*static*/ void DiskFSGutenberg::LowerASCII(uint8_t* buf, long len)
{
    while (len--) {
        if (*buf & 0x80) {
            if (*buf >= 0xa0)
                *buf &= 0x7f;
            else
                *buf = (*buf & 0x7f) + 0x20;
        } else
            *buf = ((*buf & 0x3f) ^ 0x20) + 0x20;

        buf++;
    }
}


/*
 * ===========================================================================
 *      A2FileGutenberg
 * ===========================================================================
 */

/*
 * Constructor.
 */
A2FileGutenberg::A2FileGutenberg(DiskFS* pDiskFS) : A2File(pDiskFS)
{
    fTrack = -1;
    fSector = -1;
    fLengthInSectors = 0;
    fLocked = true;
    fFileName[0] = '\0';
    fFileType = kTypeText;

    fCatTS.track = fCatTS.sector = 0;
    fCatEntryNum = -1;

    fAuxType = 0;
    fDataOffset = 0;
    fLength = -1;
    fSparseLength = -1;

    fpOpenFile = NULL;
}

/*
 * Destructor.  Make sure an "open" file gets "closed".
 */
A2FileGutenberg::~A2FileGutenberg(void)
{
    delete fpOpenFile;
}

/*
 * Convert the filetype enum to a ProDOS type.
 *
 */
uint32_t A2FileGutenberg::GetFileType(void) const
{
    return 0x04;    // TXT;
}

/*
 * "Fix" a filename.  Convert DOS-ASCII to normal ASCII, and strip
 * trailing spaces.
 */
void A2FileGutenberg::FixFilename(void)
{
    DiskFSGutenberg::LowerASCII((uint8_t*)fFileName, kMaxFileName);
    TrimTrailingSpaces(fFileName);
}

/*
 * Trim the spaces off the end of a filename.
 *
 * Assumes the filename has already been converted to low ASCII.
 */
/*static*/ void A2FileGutenberg::TrimTrailingSpaces(char* filename)
{
    char* lastspc = filename + strlen(filename);

    assert(*lastspc == '\0');

    while (--lastspc) {
        if (*lastspc != ' ')
            break;
    }

    *(lastspc+1) = '\0';
}

/*
 * Encode a filename into high ASCII, padded out with spaces to
 * kMaxFileName chars.  Lower case is converted to upper case.  This
 * does not filter out control characters or other chunk.
 *
 * "buf" must be able to hold kMaxFileName+1 chars.
 */
/*static*/ void A2FileGutenberg::MakeDOSName(char* buf, const char* name)
{
    for (int i = 0; i < kMaxFileName; i++) {
        if (*name == '\0')
            *buf++ = (char) 0xa0;
        else
            *buf++ = toupper(*name++) | 0x80;
    }
    *buf = '\0';
}


/*
 * Set up state for this file.
 */
DIError A2FileGutenberg::Open(A2FileDescr** ppOpenFile, bool readOnly,
    bool rsrcFork /*=false*/)
{
    DIError dierr = kDIErrNone;
    A2FDGutenberg* pOpenFile = NULL;

    if (!readOnly) {
        if (fpDiskFS->GetDiskImg()->GetReadOnly())
            return kDIErrAccessDenied;
        if (fpDiskFS->GetFSDamaged())
            return kDIErrBadDiskImage;
    }

    if (fpOpenFile != NULL) {
        dierr = kDIErrAlreadyOpen;
        goto bail;
    }

    if (rsrcFork)
        return kDIErrForkNotFound;

    pOpenFile = new A2FDGutenberg(this);

    pOpenFile->fOffset = 0;
    pOpenFile->fOpenEOF = fLength;
    pOpenFile->fOpenSectorsUsed = fLengthInSectors;

    fpOpenFile = pOpenFile;     // add it to our single-member "open file set"
    *ppOpenFile = pOpenFile;
    pOpenFile = NULL;

bail:
    delete pOpenFile;
    return dierr;
}

/*
 * Dump the contents of an A2FileGutenberg.
 */
void A2FileGutenberg::Dump(void) const
{
    LOGI("A2FileGutenberg '%s'", fFileName);
    LOGI("  TS T=%-2d S=%-2d", fTrack, fSector);
    LOGI("  Cat T=%-2d S=%-2d", fCatTS.track, fCatTS.sector);
    LOGI("  type=%d lck=%d slen=%d", fFileType, fLocked, fLengthInSectors);
    LOGI("  auxtype=0x%04x length=%ld",
        fAuxType, (long) fLength);
}


/*
 * ===========================================================================
 *      A2FDGutenberg
 * ===========================================================================
 */

/*
 * Read data from the current offset.
 *
 */
DIError A2FDGutenberg::Read(void* buf, size_t len, size_t* pActual)
{
    LOGD(" Gutenberg reading %lu bytes from '%s' (offset=%ld)",
        (unsigned long) len, fpFile->GetPathName(), (long) fOffset);

    A2FileGutenberg* pFile = (A2FileGutenberg*) fpFile;

    DIError dierr = kDIErrNone;
    uint8_t sctBuf[kSctSize];
    short currentTrack, currentSector;
    //di_off_t actualOffset = fOffset + pFile->fDataOffset;   // adjust for embedded len
    int bufOffset = 6;
    size_t thisCount;

    if (len == 0)
        return kDIErrNone;
    assert(fOpenEOF != 0);
    currentTrack = pFile->fTrack;
    currentSector = pFile->fSector;
    /* could be more clever in here and avoid double-buffering */
    while (len) {
        dierr = pFile->GetDiskFS()->GetDiskImg()->ReadTrackSector(
                currentTrack,
                currentSector,
                sctBuf);
        if (dierr != kDIErrNone) {
            LOGI(" Gutenberg error reading file '%s'", pFile->GetPathName());
            return dierr;
        }
        thisCount = kSctSize - bufOffset;
        if (thisCount > len)
            thisCount = len;
        memcpy(buf, sctBuf + bufOffset, thisCount);
        len -= thisCount;
        buf = (char*)buf + thisCount;
        currentTrack = sctBuf[0x04];
        currentSector = sctBuf[0x05];
    }

    return dierr;
}

/*
 * Writing Gutenberg files isn't supported.
 */
DIError A2FDGutenberg::Write(const void* buf, size_t len, size_t* pActual)
{
    return kDIErrNotSupported;
}

/*
 * Seek to the specified offset.
 */
DIError A2FDGutenberg::Seek(di_off_t offset, DIWhence whence)
{
    return kDIErrNotSupported;
}

/*
 * Return current offset.
 */
di_off_t A2FDGutenberg::Tell(void)
{
    return kDIErrNotSupported;
}

/*
 * Release file state.
 *
 * If the file was modified, we need to update the sector usage count in
 * the catalog track, and possibly a length word in the first sector of
 * the file (for A/I/B).
 *
 * Given the current "write all at once" implementation of Write, we could
 * have handled the length word back when initially writing the data, but
 * someday we may fix that and I don't want to have to rewrite this part.
 *
 * Most applications don't check the value of "Close", or call it from a
 * destructor, so we call CloseDescr whether we succeed or not.
 */
DIError A2FDGutenberg::Close(void)
{
    DIError dierr = kDIErrNone;

    fpFile->CloseDescr(this);
    return dierr;
}

/*
 * Return the #of sectors/blocks in the file.
 */
long A2FDGutenberg::GetSectorCount(void) const
{
    return fTSCount;
}

long A2FDGutenberg::GetBlockCount(void) const
{
    return (fTSCount+1)/2;
}

/*
 * Return the Nth track/sector in this file.
 *
 * Returns (0,0) for a sparse sector.
 */
DIError A2FDGutenberg::GetStorage(long sectorIdx, long* pTrack, long* pSector) const
{
        return kDIErrInvalidIndex;
}

/*
 * Unimplemented
 */
DIError A2FDGutenberg::GetStorage(long blockIdx, long* pBlock) const
{
        return kDIErrInvalidIndex;
}
