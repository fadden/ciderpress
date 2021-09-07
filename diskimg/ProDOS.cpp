/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of DiskFSProDOS class.
 *
 * We currently only allow one fork to be open at a time, and each file may
 * only be opened once.
 *
 * BUG: does not keep VolumeUsage up to date.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"

// disable Y2K+ dates when testing w/ProSel-16 vol rep (newer ProSel is OK)
//#define OLD_PRODOS_DATES

#if defined(OLD_PRODOS_DATES) && !(defined(_DEBUG))
# error "don't set OLD_PRODOS_DATES for production"
#endif


/*
 * ===========================================================================
 *      DiskFSProDOS
 * ===========================================================================
 */

const int kBlkSize = 512;
const int kVolHeaderBlock = 2;          // block where Volume Header resides
const int kFormatVolDirNumBlocks = 4;   // #of volume header blocks for new volumes
const int kMinReasonableBlocks = 16;    // min size for ProDOS volume
const int kExpectedBitmapStart = 6;     // block# where vol bitmap should start
const int kMaxCatalogIterations = 1024; // theoretical max is 32768?
const int kMaxDirectoryDepth = 64;      // not sure what ProDOS limit is
const int kEntriesPerBlock = 0x0d;      // expected value for entries per blk
const int kEntryLength = 0x27;          // expected value for dir entry len
const int kTypeDIR = 0x0f;


/*
 * Directory header.  All fields not marked as "only for subdirs" also apply
 * to the volume directory header.
 */
typedef struct DiskFSProDOS::DirHeader {
    uint8_t     storageType;
    char        dirName[A2FileProDOS::kMaxFileName+1];
    DiskFSProDOS::ProDate createWhen;
    uint8_t     version;
    uint8_t     minVersion;
    uint8_t     access;
    uint8_t     entryLength;
    uint8_t     entriesPerBlock;
    uint16_t    fileCount;
    /* the rest are only for subdirs */
    uint16_t    parentPointer;
    uint8_t     parentEntry;
    uint8_t     parentEntryLength;
} DirHeader;


/*
 * See if this looks like a ProDOS volume.
 *
 * We test a few fields in the volume directory header for validity.
 */
static DIError TestImage(DiskImg* pImg, DiskImg::SectorOrder imageOrder)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    int volDirEntryLength;
    int volDirEntriesPerBlock;

    dierr = pImg->ReadBlockSwapped(kVolHeaderBlock, blkBuf, imageOrder,
                DiskImg::kSectorOrderProDOS);
    if (dierr != kDIErrNone)
        goto bail;

    volDirEntryLength = blkBuf[0x23];
    volDirEntriesPerBlock = blkBuf[0x24];


    if (!(blkBuf[0x00] == 0 && blkBuf[0x01] == 0) ||
        !((blkBuf[0x04] & 0xf0) == 0xf0) ||
        !((blkBuf[0x04] & 0x0f) != 0) ||
        !(volDirEntryLength * volDirEntriesPerBlock <= kBlkSize) ||
        !(blkBuf[0x05] >= 'A' && blkBuf[0x05] <= 'Z') ||
        0)
    {
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

bail:
    return dierr;
}

/*
 * Test to see if the image is a ProDOS disk.
 */
/*static*/ DIError DiskFSProDOS::TestFS(DiskImg* pImg, DiskImg::SectorOrder* pOrder,
    DiskImg::FSFormat* pFormat, FSLeniency leniency)
{
    DiskImg::SectorOrder ordering[DiskImg::kSectorOrderMax];
    
    DiskImg::GetSectorOrderArray(ordering, *pOrder);

    for (int i = 0; i < DiskImg::kSectorOrderMax; i++) {
        if (ordering[i] == DiskImg::kSectorOrderUnknown)
            continue;
        if (TestImage(pImg, ordering[i]) == kDIErrNone) {
            *pOrder = ordering[i];
            *pFormat = DiskImg::kFormatProDOS;
            return kDIErrNone;
        }
    }

    LOGI(" ProDOS didn't find valid FS");
    return kDIErrFilesystemNotFound;
}

/*
 * Get things rolling.
 *
 * Since we're assured that this is a valid disk, errors encountered from here
 * on out must be handled somehow, possibly by claiming that the disk has
 * no files on it.
 */
DIError DiskFSProDOS::Initialize(InitMode initMode)
{
    DIError dierr = kDIErrNone;
    char msg[kMaxVolumeName + 32];

    fDiskIsGood = false;        // hosed until proven innocent
    fEarlyDamage = false;

    /*
     * NOTE: we'd probably be better off with fTotalBlocks, since that's how
     * big the disk *thinks* it is, especially on a CFFA or MacPart subvol.
     * However, we know that the image block count is the absolute maximum,
     * so while it may not be a tight bound it is an upper bound.
     */
    fVolumeUsage.Create(fpImg->GetNumBlocks());

    dierr = LoadVolHeader();
    if (dierr != kDIErrNone)
        goto bail;
    DumpVolHeader();

    dierr = ScanVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    if (initMode == kInitHeaderOnly) {
        LOGI(" ProDOS - headerOnly set, skipping file load");
        goto bail;
    }

    sprintf(msg, "Scanning %s", fVolumeName);
    if (!fpImg->UpdateScanProgress(msg)) {
        LOGI(" ProDOS cancelled by user");
        dierr = kDIErrCancelled;
        goto bail;
    }

    /* volume dir is guaranteed to come first; if not, we need a lookup func */
    A2FileProDOS* pVolumeDir;
    pVolumeDir = (A2FileProDOS*) GetNextFile(NULL);

    dierr = RecursiveDirAdd(pVolumeDir, kVolHeaderBlock, "", 0);
    if (dierr != kDIErrNone) {
        LOGI(" ProDOS RecursiveDirAdd failed");
        goto bail;
    }

    sprintf(msg, "Processing %s", fVolumeName);
    if (!fpImg->UpdateScanProgress(msg)) {
        LOGI(" ProDOS cancelled by user");
        dierr = kDIErrCancelled;
        goto bail;
    }

    dierr = ScanFileUsage();
    if (dierr != kDIErrNone) {
        if (dierr == kDIErrCancelled)
            goto bail;

        /* this might not be fatal; just means that *some* files are bad */
        LOGI("WARNING: ScanFileUsage returned err=%d", dierr);
        dierr = kDIErrNone;
        fpImg->AddNote(DiskImg::kNoteWarning,
            "Some errors were encountered while scanning files.");
        fEarlyDamage = true;    // make sure we know it's damaged
    }

    fDiskIsGood = CheckDiskIsGood();

    if (fScanForSubVolumes != kScanSubDisabled)
        (void) ScanForSubVolumes();

    if (fpImg->GetNumBlocks() <= 1600)
        fVolumeUsage.Dump();

//  A2File* pFile;
//  pFile = GetNextFile(NULL);
//  while (pFile != NULL) {
//      pFile->Dump();
//      pFile = GetNextFile(pFile);
//  }

bail:
    return dierr;
}

/*
 * Read some interesting fields from the volume header.
 *
 * The "test" function verified certain things, e.g. the storage type
 * is $f and the volume name length is nonzero.
 */
DIError DiskFSProDOS::LoadVolHeader(void)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    int nameLen;

    dierr = fpImg->ReadBlock(kVolHeaderBlock, blkBuf);
    if (dierr != kDIErrNone)
        goto bail;

    //fPrevBlock = GetShortLE(&blkBuf[0x00]);
    //fNextBlock = GetShortLE(&blkBuf[0x02]);
    nameLen = blkBuf[0x04] & 0x0f;
    memcpy(fVolumeName, &blkBuf[0x05], nameLen);
    fVolumeName[nameLen] = '\0';
    // 0x14-15 reserved
    // undocumented: GS/OS writes the modification date to 0x16-19
    fModWhen = GetLongLE(&blkBuf[0x16]);
    // undocumented: GS/OS uses 0x1a-1b for lower-case handling (see below)
    fCreateWhen = GetLongLE(&blkBuf[0x1c]);
    //fVersion = blkBuf[0x20];
    if (blkBuf[0x21] != 0) {
        /*
         * We don't care about the MIN_VERSION field, but it looks like GS/OS
         * rejects anything with a nonzero value here.  We want to add a note
         * about it.
         */
        fpImg->AddNote(DiskImg::kNoteInfo,
            "Volume header has nonzero min_version; could confuse GS/OS.");
    }
    fAccess = blkBuf[0x22];
    //fEntryLength = blkBuf[0x23];
    //fEntriesPerBlock = blkBuf[0x24];
    fVolDirFileCount = GetShortLE(&blkBuf[0x25]);
    fBitMapPointer = GetShortLE(&blkBuf[0x27]);
    fTotalBlocks = GetShortLE(&blkBuf[0x29]);

    if (blkBuf[0x1b] & 0x80) {
        /*
         * Handle lower-case conversion; see GS/OS tech note #8.  Unlike
         * filenames, volume names are not allowed to contain spaces.  If
         * they try it we just ignore them.
         *
         * Technote 8 doesn't actually talk about volume names.  By
         * experimentation the field was discovered at offset 0x1a from
         * the start of the block, which is marked as "reserved" in Beneath
         * Apple ProDOS.
         */
        uint16_t lcFlags = GetShortLE(&blkBuf[0x1a]);

        GenerateLowerCaseName(fVolumeName, fVolumeName, lcFlags, false);
    }

    if (fTotalBlocks <= kVolHeaderBlock) {
        /* incr to min; don't use max, or bitmap count may be too large */
        LOGI(" ProDOS found tiny fTotalBlocks (%d), increasing to minimum",
            fTotalBlocks);
        fpImg->AddNote(DiskImg::kNoteWarning,
            "ProDOS filesystem blockcount (%d) too small, setting to %d.",
            fTotalBlocks, kMinReasonableBlocks);
        fTotalBlocks = kMinReasonableBlocks;
        fEarlyDamage = true;
    } else if (fTotalBlocks != fpImg->GetNumBlocks()) {
        if (fTotalBlocks != 65535 || fpImg->GetNumBlocks() != 65536) {
            LOGI(" ProDOS WARNING: total (%u) != img (%ld)",
                fTotalBlocks, fpImg->GetNumBlocks());
            // could AddNote here, but not really necessary
        }

        /*
         * For safety (esp. vol bitmap read), constrain fTotalBlocks.  We might
         * consider not doing this for ".hdv", which can start small and then
         * expand as files are added.  (Check "fExpanded".)
         */
        if (fTotalBlocks > fpImg->GetNumBlocks()) {
            fpImg->AddNote(DiskImg::kNoteWarning,
                "ProDOS filesystem blockcount (%d) exceeds disk image blocks (%ld).",
                fTotalBlocks, fpImg->GetNumBlocks());
            fTotalBlocks = (uint16_t) fpImg->GetNumBlocks();
            fEarlyDamage = true;
        }
    }

    /*
     * Test for funky volume bitmap pointer.  Some disks (e.g. /RAM and
     * ProSel-16) truncate the volume directory to eke a little more storage
     * out of a disk.  There's nothing wrong with that, but we don't want to
     * try to use a volume bitmap pointer of zero or 0xffff, because it's
     * probably garbage.
     */
    if (fBitMapPointer != kExpectedBitmapStart) {
        if (fBitMapPointer <= kVolHeaderBlock ||
            fBitMapPointer > kExpectedBitmapStart)
        {
            fpImg->AddNote(DiskImg::kNoteWarning,
                "Volume bitmap pointer (%d) is probably invalid.",
                fBitMapPointer);
            fBitMapPointer = 6;     // just fix it and hope for the best
            fEarlyDamage = true;
        } else {
            fpImg->AddNote(DiskImg::kNoteInfo,
                "Unusual volume bitmap start (%d).", fBitMapPointer);
            // try it and see
        }
    }

    SetVolumeID();

    /*
     * Create a "magic" directory entry for the volume directory.
     *
     * Normally these values are pulled out of the file entry in the parent
     * directory.  Here, we synthesize them from the volume dir header.
     */
    A2FileProDOS* pFile;
    pFile = new A2FileProDOS(this);
    if (pFile == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    A2FileProDOS::DirEntry* pEntry;
    pEntry = &pFile->fDirEntry;

    int foundStorage;
    foundStorage = (blkBuf[0x04] & 0xf0) >> 4;
    if (foundStorage != A2FileProDOS::kStorageVolumeDirHeader) {
        LOGI(" ProDOS WARNING: unexpected vol dir file type %d",
            pEntry->storageType);
        /* keep going */
    }
    pEntry->storageType = A2FileProDOS::kStorageVolumeDirHeader;
    strcpy(pEntry->fileName, fVolumeName);
    //nameLen = blkBuf[0x04] & 0x0f;
    //memcpy(pEntry->fileName, &blkBuf[0x05], nameLen);
    //pEntry->fileName[nameLen] = '\0';
    pFile->SetPathName(":", pEntry->fileName);
    pEntry->fileName[nameLen] = '\0';
    pEntry->fileType = kTypeDIR;
    pEntry->keyPointer = kVolHeaderBlock;
    dierr = DetermineVolDirLen(GetShortLE(&blkBuf[0x02]), &pEntry->blocksUsed);
    if (dierr != kDIErrNone) {
        goto bail;
    }
    pEntry->eof = pEntry->blocksUsed * 512;
    pEntry->createWhen = GetLongLE(&blkBuf[0x1c]);
    pEntry->version = blkBuf[0x20];
    pEntry->minVersion = blkBuf[0x21];
    pEntry->access = blkBuf[0x22];
    pEntry->auxType = 0;
//  if (blkBuf[0x20] >= 5)
        pEntry->modWhen = GetLongLE(&blkBuf[0x16]);
    pEntry->headerPointer = 0;

    pFile->fSparseDataEof = pEntry->eof;
    pFile->fSparseRsrcEof = -1;

    AddFileToList(pFile);
    pFile = NULL;

bail:
    delete pFile;
    return dierr;
}

DIError DiskFSProDOS::DetermineVolDirLen(uint16_t nextBlock, uint16_t* pBlocksUsed) {
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    uint16_t blocksUsed = 1;
    int iterCount = 0;

    // Traverse the volume directory chain, counting blocks.  Normally this will have 4, but
    // variations are possible.
    while (nextBlock != 0) {
        blocksUsed++;

        if (nextBlock < 2 || nextBlock >= fpImg->GetNumBlocks()) {
            LOGI(" ProDOS ERROR: invalid volume dir link block %u", nextBlock);
            dierr = kDIErrInvalidBlock;
            goto bail;
        }
        dierr = fpImg->ReadBlock(nextBlock, blkBuf);
        if (dierr != kDIErrNone) {
            goto bail;
        }

        nextBlock = GetShortLE(&blkBuf[0x02]);

        // Watch for infinite loop.
        iterCount++;
        if (iterCount > fpImg->GetNumBlocks()) {
            LOGI(" ProDOS ERROR: infinite vol directory loop found");
            dierr = kDIErrDirectoryLoop;
            goto bail;
        }
    }

bail:
    *pBlocksUsed = blocksUsed;
    return dierr;
}

/*
 * Set the volume ID field.
 */
void DiskFSProDOS::SetVolumeID(void)
{
    sprintf(fVolumeID, "ProDOS /%s", fVolumeName);
}

/*
 * Dump what we pulled out of the volume header.
 */
void DiskFSProDOS::DumpVolHeader(void)
{
    LOGI(" ProDOS volume header for '%s'", fVolumeName);
    LOGI("  CreateWhen=0x%08x access=0x%02x bitmap=%d totalbl=%d",
        fCreateWhen, fAccess, fBitMapPointer, fTotalBlocks);

    time_t when;
    when = A2FileProDOS::ConvertProDate(fCreateWhen);
    LOGI("  CreateWhen is %.24s", ctime(&when));

    //LOGI("  prev=%d next=%d bitmap=%d total=%d",
    //  fPrevBlock, fNextBlock, fBitMapPointer, fTotalBlocks);
    //LOGI("  create date=0x%08lx access=0x%02x", fCreateWhen, fAccess);
    //LOGI("  version=%d minVersion=%d entryLen=%d epb=%d",
    //  fVersion, fMinVersion, fEntryLength, fEntriesPerBlock);
    //LOGI("  volume dir fileCount=%d", fFileCount);
}

/*
 * Load the disk's volume bitmap into the object's "fBlockUseMap" pointer.
 *
 * Does not attempt to analyze the data.
 */
DIError DiskFSProDOS::LoadVolBitmap(void)
{
    DIError dierr = kDIErrNone;
    int bitBlock, numBlocks;

    if (fBitMapPointer <= kVolHeaderBlock)
        return kDIErrBadDiskImage;
    if (fTotalBlocks <= kVolHeaderBlock)
        return kDIErrBadDiskImage;

    /* should not already be allocated */
    assert(fBlockUseMap == NULL);
    delete[] fBlockUseMap;      // just in case

    bitBlock = fBitMapPointer;

    numBlocks = GetNumBitmapBlocks();   // based on fTotalBlocks
    assert(numBlocks > 0);

    fBlockUseMap = new uint8_t[kBlkSize * numBlocks];
    if (fBlockUseMap == NULL)
        return kDIErrMalloc;

    while (numBlocks--) {
        dierr = fpImg->ReadBlock(bitBlock + numBlocks,
                    fBlockUseMap + kBlkSize * numBlocks);
        if (dierr != kDIErrNone) {
            delete[] fBlockUseMap;
            fBlockUseMap = NULL;
            return dierr;
        }
    }

    return kDIErrNone;
}

/*
 * Save our copy of the volume bitmap.
 */
DIError DiskFSProDOS::SaveVolBitmap(void)
{
    DIError dierr = kDIErrNone;
    int bitBlock, numBlocks;

    if (fBlockUseMap == NULL) {
        assert(false);
        return kDIErrNotReady;
    }
    assert(fBitMapPointer > kVolHeaderBlock);
    assert(fTotalBlocks > kVolHeaderBlock);

    bitBlock = fBitMapPointer;

    numBlocks = GetNumBitmapBlocks();
    assert(numBlocks > 0);

    while (numBlocks--) {
        dierr = fpImg->WriteBlock(bitBlock + numBlocks,
                    fBlockUseMap + kBlkSize * numBlocks);
        if (dierr != kDIErrNone)
            return dierr;
    }

    return kDIErrNone;
}

/*
 * Throw away the volume bitmap, discarding any unsaved changes.
 *
 * It's okay to call this if the bitmap isn't loaded.
 */
void DiskFSProDOS::FreeVolBitmap(void)
{
    delete[] fBlockUseMap;
    fBlockUseMap = NULL;
}

/*
 * Examine the volume bitmap, setting fields in the VolumeUsage map
 * as appropriate.
 */
DIError DiskFSProDOS::ScanVolBitmap(void)
{
    DIError dierr;

    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone) {
        LOGI(" ProDOS failed to load volume bitmap (err=%d)", dierr);
        return dierr;
    }

    assert(fBlockUseMap != NULL);

    /* mark the boot blocks as system */
    SetBlockUsage(0, VolumeUsage::kChunkPurposeSystem);
    SetBlockUsage(1, VolumeUsage::kChunkPurposeSystem);

    /* mark the bitmap blocks as system */
    int i;
    for (i = GetNumBitmapBlocks(); i > 0; i--)
        SetBlockUsage(fBitMapPointer + i -1, VolumeUsage::kChunkPurposeSystem);

    /*
     * Set the "isMarkedUsed" flag in VolumeUsage for all used blocks.
     */
    VolumeUsage::ChunkState cstate;

    long block = 0;
    long numBytes = (fTotalBlocks + 7) / 8;
    for (i = 0; i < numBytes; i++) {
        uint8_t val = fBlockUseMap[i];

        for (int j = 0; j < 8; j++) {
            if (!(val & 0x80)) {
                /* block is in use, mark it */
                if (fVolumeUsage.GetChunkState(block, &cstate) != kDIErrNone)
                {
                    assert(false);
                    // keep going, I guess
                }
                cstate.isMarkedUsed = true;
                fVolumeUsage.SetChunkState(block, &cstate);
            }
            val <<= 1;
            block++;

            if (block >= fTotalBlocks)
                break;
        }
        if (block >= fTotalBlocks)
            break;
    }

    FreeVolBitmap();
    return dierr;
}

/*
 * Generate an empty block use map.  Used by disk formatter.
 */
DIError DiskFSProDOS::CreateEmptyBlockMap(void)
{
    DIError dierr;

    /* load from disk; this is just to allocate the data structures */
    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        return dierr;

    /*
     * Set the bits, block by block.  Not the most efficient way, but it's
     * fast enough, and it exercises the standard set of functions.
     */
    long block;
    long firstEmpty =
        kVolHeaderBlock + kFormatVolDirNumBlocks + GetNumBitmapBlocks();
    for (block = 0; block < firstEmpty; block++)
        SetBlockUseEntry(block, true);
    for ( ; block < fTotalBlocks; block++)
        SetBlockUseEntry(block, false);

    dierr = SaveVolBitmap();
    FreeVolBitmap();
    if (dierr != kDIErrNone)
        return dierr;

    return kDIErrNone;
}

/*
 * Get the state of an entry in the block use map.
 *
 * Returns "true" if it's in use, "false" otherwise.
 */
bool DiskFSProDOS::GetBlockUseEntry(long block) const
{
    assert(block >= 0 && block < fTotalBlocks);
    assert(fBlockUseMap != NULL);

    int offset;
    uint8_t mask;

    offset = block / 8;
    mask = 0x80 >> (block & 0x07);
    if (fBlockUseMap[offset] & mask)
        return false;
    else
        return true;
}

/*
 * Change the state of an entry in the block use map.
 */
void DiskFSProDOS::SetBlockUseEntry(long block, bool inUse)
{
    assert(block >= 0 && block < fTotalBlocks);
    assert(fBlockUseMap != NULL);

    if (block == 0 && !inUse) {
        // shouldn't happen
        assert(false);
    }

    int offset;
    uint8_t mask;

    offset = block / 8;
    mask = 0x80 >> (block & 0x07);
    if (!inUse)
        fBlockUseMap[offset] |= mask;
    else
        fBlockUseMap[offset] &= ~mask;
}

/*
 * Check for entries in the block use map past the point where they should be.
 *
 * Returns "true" if bogus entries were found, "false" if all is well.
 */
bool DiskFSProDOS::ScanForExtraEntries(void) const
{
    assert(fBlockUseMap != NULL);

    int offset, endOffset;

    /* sloppy: we're not checking for excess bits within last byte */
    offset = (fTotalBlocks / 8) +1;
    endOffset = GetNumBitmapBlocks() * kBlkSize;

    while (offset < endOffset) {
        if (fBlockUseMap[offset] != 0) {
            LOGI(" ProDOS found bogus bitmap junk 0x%02x at offset=%d",
                fBlockUseMap[offset], offset);
            return true;
        }
        offset++;
    }
    return false;
}

/*
 * Allocate a new block on a ProDOS volume.
 *
 * Only touches the in-memory copy.
 *
 * Returns the block number (0-65535) on success or -1 on failure.
 */
long DiskFSProDOS::AllocBlock(void)
{
    assert(fBlockUseMap != NULL);

#if 0   // whoa... this is REALLY slow
    /*
     * Run through the entire set of blocks until we find one that's not
     * allocated.  We could probably make this faster by scanning bytes and
     * then shifting bits, but this is easier and fast enough.
     *
     * We don't scan block 0 because (a) it should never be available and
     * (b) it has a special meaning in some circumstances.  We could probably
     * start at kVolHeaderBlock+kVolHeaderNumBlocks.
     */
    long block;
    for (block = kVolHeaderBlock; block < fTotalBlocks; block++) {
        if (!GetBlockUseEntry(block)) {
            SetBlockUseEntry(block, true);
            return block;
        }
    }
#endif

    int offset;
    int maxOffset = (fTotalBlocks + 7) / 8;

    for (offset = 0; offset < maxOffset; offset++) {
        if (fBlockUseMap[offset] != 0) {
            /* got one, figure out which */
            int subBlock = 0;
            uint8_t uch = fBlockUseMap[offset];
            while ((uch & 0x80) == 0) {
                subBlock++;
                uch <<= 1;
            }

            long block = offset * 8 + subBlock;
            assert(!GetBlockUseEntry(block));
            SetBlockUseEntry(block, true);
            if (block == 0 || block == 1) {
                LOGI("PRODOS: GLITCH: rejecting alloc of block 0");
                continue;
            }
            return block;
        }
    }

    LOGI("ProDOS: NOTE: AllocBlock just failed!");
    return -1;
}

/*
 * Tally up the number of free blocks.
 */
DIError DiskFSProDOS::GetFreeSpaceCount(long* pTotalUnits, long* pFreeUnits,
    int* pUnitSize) const
{
    DIError dierr;
    long block, freeBlocks;
    freeBlocks = 0;

    dierr = const_cast<DiskFSProDOS*>(this)->LoadVolBitmap();
    if (dierr != kDIErrNone)
        return dierr;

    for (block = 0; block < fTotalBlocks; block++) {
        if (!GetBlockUseEntry(block))
            freeBlocks++;
    }

    *pTotalUnits = fTotalBlocks;
    *pFreeUnits = freeBlocks;
    *pUnitSize = kBlockSize;

    const_cast<DiskFSProDOS*>(this)->FreeVolBitmap();
    return kDIErrNone;
}

/*
 * Update an entry in the VolumeUsage map.
 *
 * The VolumeUsage map spans the range of blocks 
 */
void DiskFSProDOS::SetBlockUsage(long block, VolumeUsage::ChunkPurpose purpose)
{
    VolumeUsage::ChunkState cstate;

    fVolumeUsage.GetChunkState(block, &cstate);
    if (cstate.isUsed) {
        cstate.purpose = VolumeUsage::kChunkPurposeConflict;
        LOGI(" ProDOS conflicting uses for bl=%ld", block);
    } else {
        cstate.isUsed = true;
        cstate.purpose = purpose;
    }
    fVolumeUsage.SetChunkState(block, &cstate);
}

/*
 * Pass in the number of the first block of the directory.
 *
 * Start with "pParent" set to the magic entry for the volume dir.
 */
DIError DiskFSProDOS::RecursiveDirAdd(A2File* pParent, uint16_t dirBlock,
    const char* basePath, int depth)
{
    DIError dierr = kDIErrNone;
    DirHeader header;
    uint8_t blkBuf[kBlkSize];
    int numEntries, iterations, foundCount;
    bool first;

    /* if we get too deep, assume it's a loop */
    if (depth > kMaxDirectoryDepth) {
        dierr = kDIErrDirectoryLoop;
        goto bail;
    }


    if (dirBlock < kVolHeaderBlock || dirBlock >= fpImg->GetNumBlocks()) {
        LOGI(" ProDOS ERROR: directory block %u out of range", dirBlock);
        dierr = kDIErrInvalidBlock;
        goto bail;
    }

    numEntries = 1;
    iterations = 0;
    foundCount = 0;
    first = true;

    while (dirBlock && iterations < kMaxCatalogIterations) {
        dierr = fpImg->ReadBlock(dirBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;
        if (pParent->IsVolumeDirectory())
            SetBlockUsage(dirBlock, VolumeUsage::kChunkPurposeVolumeDir);
        else
            SetBlockUsage(dirBlock, VolumeUsage::kChunkPurposeSubdir);

        if (first) {
            /* this is the directory header entry */
            dierr = GetDirHeader(blkBuf, &header);
            if (dierr != kDIErrNone)
                goto bail;
            numEntries = header.fileCount;
            //LOGI("  ProDOS got dir header numEntries = %d", numEntries);
        }

        /* slurp the entries out of this block */
        dierr = SlurpEntries(pParent, &header, blkBuf, first, &foundCount,
                    basePath, dirBlock, depth);
        if (dierr != kDIErrNone)
            goto bail;

        dirBlock = GetShortLE(&blkBuf[0x02]);
        if (dirBlock != 0 &&
            (dirBlock < 2 || dirBlock >= fpImg->GetNumBlocks()))
        {
            LOGI(" ProDOS ERROR: invalid dir link block %u in base='%s'",
                dirBlock, basePath);
            dierr = kDIErrInvalidBlock;
            goto bail;
        }
        first = false;
        iterations++;
    }
    if (iterations == kMaxCatalogIterations) {
        LOGI(" ProDOS subdir iteration count exceeded");
        dierr = kDIErrDirectoryLoop;
        goto bail;
    }
    if (foundCount != numEntries) {
        /* not significant; just means somebody isn't updating correctly */
        LOGI(" ProDOS WARNING: numEntries=%d foundCount=%d in base='%s'",
            numEntries, foundCount, basePath);
    }

bail:
    return dierr;
}

/*
 * Slurp the entries out of a single ProDOS directory block.
 *
 * Recursively calls RecursiveDirAdd for directories.
 *
 * "*pFound" is increased by the number of valid entries found in this block.
 */
DIError DiskFSProDOS::SlurpEntries(A2File* pParent, const DirHeader* pHeader,
    const uint8_t* blkBuf, bool skipFirst, int* pCount,
    const char* basePath, uint16_t thisBlock, int depth)
{
    DIError dierr = kDIErrNone;
    int entriesThisBlock = pHeader->entriesPerBlock;
    const uint8_t* entryBuf;
    A2FileProDOS* pFile;

    int idx = 0;
    entryBuf = &blkBuf[0x04];
    if (skipFirst) {
        entriesThisBlock--;
        entryBuf += pHeader->entryLength;
        idx++;
    }

    for ( ; entriesThisBlock > 0 ;
        entriesThisBlock--, idx++, entryBuf += pHeader->entryLength)
    {
        if (entryBuf >= blkBuf + kBlkSize) {
            LOGI("  ProDOS whoops, just walked out of dirent buffer");
            return kDIErrBadDirectory;
        }

        if ((entryBuf[0x00] & 0xf0) == A2FileProDOS::kStorageDeleted) {
            /* skip deleted entries */
            continue;
        }

        pFile = new A2FileProDOS(this);
        if (pFile == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        }

        A2FileProDOS::DirEntry* pEntry;
        pEntry = &pFile->fDirEntry;
        A2FileProDOS::InitDirEntry(pEntry, entryBuf);

        pFile->SetParent(pParent);
        pFile->fParentDirBlock = thisBlock;
        pFile->fParentDirIdx = idx;

        pFile->SetPathName(basePath, pEntry->fileName);

        if (pEntry->keyPointer <= kVolHeaderBlock) {
            LOGI("ProDOS invalid key pointer %d on '%s'",
                pEntry->keyPointer, pFile->GetPathName());
            pFile->SetQuality(A2File::kQualityDamaged);
        } else
        if (pEntry->storageType == A2FileProDOS::kStorageExtended) {
            dierr = ReadExtendedInfo(pFile);
            if (dierr != kDIErrNone) {
                pFile->SetQuality(A2File::kQualityDamaged);
                dierr = kDIErrNone;
            }
        }

        //pFile->Dump();
        AddFileToList(pFile);
        (*pCount)++;

        if (!fpImg->UpdateScanProgress(NULL)) {
            LOGI(" ProDOS cancelled by user");
            dierr = kDIErrCancelled;
            goto bail;
        }

        if (pEntry->storageType == A2FileProDOS::kStorageDirectory) {
            // don't need to check for kStorageVolumeDirHeader here
            dierr = RecursiveDirAdd(pFile, pEntry->keyPointer,
                        pFile->GetPathName(), depth+1);
            if (dierr != kDIErrNone) {
                if (dierr == kDIErrCancelled)
                    goto bail;

                /* mark subdir as damaged and keep going */
                pFile->SetQuality(A2File::kQualityDamaged);
                dierr = kDIErrNone;
            }
        }
    }

bail:
    return dierr;
}

/*
 * Pull the directory header out of the first block of a directory.
 */
DIError DiskFSProDOS::GetDirHeader(const uint8_t* blkBuf, DirHeader* pHeader)
{
    int nameLen;

    pHeader->storageType = (blkBuf[0x04] & 0xf0) >> 4;
    if (pHeader->storageType != A2FileProDOS::kStorageSubdirHeader &&
        pHeader->storageType != A2FileProDOS::kStorageVolumeDirHeader)
    {
        LOGI(" ProDOS WARNING: subdir header has wrong storage type (%d)",
            pHeader->storageType);
        /* keep going... might be bad idea */
    }
    nameLen = blkBuf[0x04] & 0x0f;
    memcpy(pHeader->dirName, &blkBuf[0x05], nameLen);
    pHeader->dirName[nameLen] = '\0';
    pHeader->createWhen = GetLongLE(&blkBuf[0x1c]);
    pHeader->version = blkBuf[0x20];
    pHeader->minVersion = blkBuf[0x21];
    pHeader->access = blkBuf[0x22];
    pHeader->entryLength = blkBuf[0x23];
    pHeader->entriesPerBlock = blkBuf[0x24];
    pHeader->fileCount = GetShortLE(&blkBuf[0x25]);
    pHeader->parentPointer = GetShortLE(&blkBuf[0x27]);
    pHeader->parentEntry = blkBuf[0x29];
    pHeader->parentEntryLength = blkBuf[0x2a];

    if (pHeader->entryLength * pHeader->entriesPerBlock > kBlkSize ||
        pHeader->entryLength * pHeader->entriesPerBlock == 0)
    {
        LOGI(" ProDOS invalid subdir header: entryLen=%d, entriesPerBlock=%d",
            pHeader->entryLength, pHeader->entriesPerBlock);
        return kDIErrBadDirectory;
    }

    return kDIErrNone;
}

/*
 * Read the information from the key block of an extended file.
 *
 * There's some "HFS Finder information" stuffed into the key block
 * right after the data fork info, but I'm planning to ignore that.
 */
DIError DiskFSProDOS::ReadExtendedInfo(A2FileProDOS* pFile)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];

    dierr = fpImg->ReadBlock(pFile->fDirEntry.keyPointer, blkBuf);
    if (dierr != kDIErrNone) {
        LOGI(" ProDOS ReadExtendedInfo: unable to read key block %d",
            pFile->fDirEntry.keyPointer);
        goto bail;
    }

    pFile->fExtData.storageType = blkBuf[0x0000] & 0x0f;
    pFile->fExtData.keyBlock = GetShortLE(&blkBuf[0x0001]);
    pFile->fExtData.blocksUsed = GetShortLE(&blkBuf[0x0003]);
    pFile->fExtData.eof = GetLongLE(&blkBuf[0x0005]);
    pFile->fExtData.eof &= 0x00ffffff;

    pFile->fExtRsrc.storageType = blkBuf[0x0100] & 0x0f;
    pFile->fExtRsrc.keyBlock = GetShortLE(&blkBuf[0x0101]);
    pFile->fExtRsrc.blocksUsed = GetShortLE(&blkBuf[0x0103]);
    pFile->fExtRsrc.eof = GetLongLE(&blkBuf[0x0105]);
    pFile->fExtRsrc.eof &= 0x00ffffff;

    if (pFile->fExtData.keyBlock <= kVolHeaderBlock ||
        pFile->fExtRsrc.keyBlock <= kVolHeaderBlock)
    {
        LOGI(" ProDOS ReadExtendedInfo: found bad extended key blocks %d/%d",
            pFile->fExtData.keyBlock, pFile->fExtRsrc.keyBlock);
        return kDIErrBadFile;
    }

bail:
    return dierr;
}

/*
 * Scan all of the files on the disk, reading their block usage into the
 * volume usage map.  This is important for detecting damage, and makes
 * later accesses easier.
 *
 * As a side-effect, we set the "sparse" length for the file.
 */
DIError DiskFSProDOS::ScanFileUsage(void)
{
    DIError dierr = kDIErrNone;
    A2FileProDOS* pFile;
    long blockCount, indexCount, sparseCount;
    uint16_t* blockList = NULL;
    uint16_t* indexList = NULL;

    pFile = (A2FileProDOS*) GetNextFile(NULL);
    while (pFile != NULL) {
        if (!fpImg->UpdateScanProgress(NULL)) {
            LOGI(" ProDOS cancelled by user");
            dierr = kDIErrCancelled;
            goto bail;
        }

        //pFile->Dump();
        if (pFile->GetQuality() == A2File::kQualityDamaged)
            goto skip;

        if (pFile->fDirEntry.storageType == A2FileProDOS::kStorageExtended) {
            /* resource fork */
            if (!A2FileProDOS::IsRegularFile(pFile->fExtRsrc.storageType)) {
                /* not expecting to find a directory here, but it happens */
                dierr = kDIErrBadFile;
            } else {
                dierr = pFile->LoadBlockList(pFile->fExtRsrc.storageType,
                            pFile->fExtRsrc.keyBlock, pFile->fExtRsrc.eof,
                            &blockCount, &blockList, &indexCount, &indexList);
            }
            if (dierr != kDIErrNone) {
                LOGI(" ProDOS skipping scan rsrc '%s'",
                    pFile->fDirEntry.fileName);
                pFile->SetQuality(A2File::kQualityDamaged);
                goto skip;
            }
            ScanBlockList(blockCount, blockList, indexCount, indexList,
                &sparseCount);
            pFile->fSparseRsrcEof =
                        (di_off_t) pFile->fExtRsrc.eof - sparseCount * kBlkSize;
            //LOGI(" SparseCount %d rsrcEof %d '%s'",
            //  sparseCount, pFile->fSparseRsrcEof, pFile->fDirEntry.fileName);
            delete[] blockList;
            blockList = NULL;
            delete[] indexList;
            indexList = NULL;

            /* data fork */
            if (!A2FileProDOS::IsRegularFile(pFile->fExtRsrc.storageType)) {
                dierr = kDIErrBadFile;
            } else {
                dierr = pFile->LoadBlockList(pFile->fExtData.storageType,
                            pFile->fExtData.keyBlock, pFile->fExtData.eof,
                            &blockCount, &blockList, &indexCount, &indexList);
            }
            if (dierr != kDIErrNone) {
                LOGI(" ProDOS skipping scan data '%s'",
                    pFile->fDirEntry.fileName);
                pFile->SetQuality(A2File::kQualityDamaged);
                goto skip;
            }
            ScanBlockList(blockCount, blockList, indexCount, indexList,
                &sparseCount);
            pFile->fSparseDataEof =
                        (di_off_t) pFile->fExtData.eof - sparseCount * kBlkSize;
            //LOGI(" SparseCount %ld dataEof %ld -> %lld '%s'",
            //    sparseCount, pFile->fExtData.eof, pFile->fSparseDataEof,
            //    pFile->fDirEntry.fileName);
            delete[] blockList;
            blockList = NULL;
            delete[] indexList;
            indexList = NULL;

            /* mark the extended key block as in-use */
            SetBlockUsage(pFile->fDirEntry.keyPointer,
                VolumeUsage::kChunkPurposeFileStruct);
        } else if (pFile->fDirEntry.storageType == A2FileProDOS::kStorageDirectory ||
                   pFile->fDirEntry.storageType == A2FileProDOS::kStorageVolumeDirHeader)
        {
            /* we already got these during the recursive descent */
            /* (could do them here if we used "fake" directory entry
                for volume dir to lead off the recursion) */
            goto skip;
        } else if (pFile->fDirEntry.storageType == A2FileProDOS::kStorageSeedling ||
                   pFile->fDirEntry.storageType == A2FileProDOS::kStorageSapling ||
                   pFile->fDirEntry.storageType == A2FileProDOS::kStorageTree)
        {
            /* standard file */
            dierr = pFile->LoadBlockList(pFile->fDirEntry.storageType,
                        pFile->fDirEntry.keyPointer, pFile->fDirEntry.eof,
                        &blockCount, &blockList, &indexCount, &indexList);
            if (dierr != kDIErrNone) {
                LOGI(" ProDOS skipping scan '%s'",
                    pFile->fDirEntry.fileName);
                pFile->SetQuality(A2File::kQualityDamaged);
                goto skip;
            }
            ScanBlockList(blockCount, blockList, indexCount, indexList,
                &sparseCount);
            pFile->fSparseDataEof =
                        (di_off_t) pFile->fDirEntry.eof - sparseCount * kBlkSize;
            //LOGI(" +++ sparseCount=%ld blockCount=%ld sparseDataEof=%lld '%s'",
            //    sparseCount, blockCount, pFile->fSparseDataEof,
            //    pFile->fDirEntry.fileName);

            delete[] blockList;
            blockList = NULL;
            delete[] indexList;
            indexList = NULL;
        } else {
            LOGI(" ProDOS found weird storage type %d on '%s', ignoring",
                pFile->fDirEntry.storageType, pFile->fDirEntry.fileName);
            pFile->SetQuality(A2File::kQualityDamaged);
        }

        /*
         * A completely empty file written as zero blocks (as opposed to simply
         * having its EOF extended, e.g. "sparse seedlings") will have zero data
         * blocks but possibly an EOF that doesn't land on 512 bytes.  This can
         * result in a slightly negative "sparse length", which we trim to zero
         * here.
         */
        //if (stricmp(pFile->fDirEntry.fileName, "EMPTY.SPARSE.R") == 0)
        //  LOGI("wahoo");
        if (pFile->fSparseDataEof < 0)
            pFile->fSparseDataEof = 0;
        if (pFile->fSparseRsrcEof < 0)
            pFile->fSparseRsrcEof = 0;

skip:
        pFile = (A2FileProDOS*) GetNextFile(pFile);
    }

    dierr = kDIErrNone;

bail:
    return dierr;
}

/*
 * Scan a block list into the volume usage map.
 */
void DiskFSProDOS::ScanBlockList(long blockCount, uint16_t* blockList,
    long indexCount, uint16_t* indexList, long* pSparseCount)
{
    assert(blockList != NULL);
    assert(indexCount == 0 || indexList != NULL);
    assert(pSparseCount != NULL);

    *pSparseCount = 0;

    int i;
    for (i = 0; i < blockCount; i++) {
        if (blockList[i] != 0) {
            SetBlockUsage(blockList[i], VolumeUsage::kChunkPurposeUserData);
        } else {
            (*pSparseCount)++;  // sparse data block
        }
    }

    for (i = 0; i < indexCount; i++) {
        if (indexList[i] != 0) {
            SetBlockUsage(indexList[i], VolumeUsage::kChunkPurposeFileStruct);
        }   // else sparse index block
    }
}

/*
 * ProDOS disks may contain other filesystems.  The typical DOS-in-ProDOS
 * strategy involves marking a bunch of blocks at the end of the disc as
 * "in use" without creating a file to go along with them.
 *
 * We look for certain types of embedded volume by looking for disk
 * usage patterns and then testing those with the standard disk testing
 * facilities.
 */
DIError DiskFSProDOS::ScanForSubVolumes(void)
{
    DIError dierr = kDIErrNone;
    VolumeUsage::ChunkState cstate;
    int firstBlock, matchCount;
    int block;

    /* this is guaranteed by constraint in volume header read */
    assert(fTotalBlocks <= fpImg->GetNumBlocks());

    if (fTotalBlocks != 1600) {
        LOGI(" ProDOS ScanForSub: not 800K disk (%ld)",
            fpImg->GetNumBlocks());
        return kDIErrNone;      // only scan 800K disks
    }

    matchCount = 0;
    for (block = fTotalBlocks-1; block >= 0; block--) {
        if (fVolumeUsage.GetChunkState(block, &cstate) != kDIErrNone) {
            assert(false);
            return kDIErrGeneric;
        }

        if (!cstate.isMarkedUsed || cstate.isUsed)
            break;

        matchCount++;
    }
    firstBlock = block+1;

    LOGI("MATCH COUNT %d", matchCount);
    if (matchCount < 35*8)      // 280 blocks on 35-track floppy
        return kDIErrNone;
    //if (matchCount % 8 != 0) {    // must have 4K tracks
    //  LOGI(" ProDOS ScanForSub: matchCount %d odd number",
    //      matchCount);
    //  return kDIErrNone;
    //}

    /*
     * Try #1: this is a single DOS 3.3 volume (200K or less).
     */
    if ((matchCount % 8) == 0 && matchCount <= (50*8)) {    // max 50 tracks
        DiskFS* pNewFS = NULL;
        DiskImg* pNewImg = NULL;
        LOGI(" Sub #1: looking for single DOS volume");
        dierr = FindSubVolume(firstBlock, matchCount, &pNewImg, &pNewFS);
        if (dierr == kDIErrNone) {
            AddSubVolumeToList(pNewImg, pNewFS);
            MarkSubVolumeBlocks(firstBlock, matchCount);
            return kDIErrNone;
        }
    }


    /*
     * Try #2: there are multiple 140K DOS 3.3 volumes here.
     *
     * We may want to override their volume numbers, but it looks like
     * DOS Master disks have distinct volume numbers anyway.
     */
    const int kBlkCount140 = 140*2;
    if ((matchCount % (kBlkCount140)) == 0) {
        int i, count;
        bool found = false;

        count = matchCount / kBlkCount140;
        LOGI(" Sub #2: looking for %d 140K volumes",
            matchCount / kBlkCount140);

        for (i = 0; i < count; i++) {
            DiskFS* pNewFS = NULL;
            DiskImg* pNewImg = NULL;
            LOGI(" Sub #2: looking for DOS volume at (%d)",
                firstBlock + i * kBlkCount140);
            dierr = FindSubVolume(firstBlock + i * kBlkCount140,
                        kBlkCount140, &pNewImg, &pNewFS);
            if (dierr == kDIErrNone) {
                AddSubVolumeToList(pNewImg, pNewFS);
                MarkSubVolumeBlocks(firstBlock + i * kBlkCount140,
                        kBlkCount140);
                found = true;
            }
        }
        if (found)
            return kDIErrNone;
    }

    /*
     * Try #3: there are five 160K DOS 3.3 volumes here (which works out
     * to exactly 800K).  The first DOS volume loses early tracks as
     * needed to accommodate the ProDOS directory and up to 28K of
     * boot files.
     *
     * Because the first 160K volume starts at the front of the disk,
     * we need to restrict this to non-ProDOS sub-volumes, or we'll see
     * a "ghost" volume in the first position.  This stuff is going to
     * fail if we test for ProDOS before we check for DOS 3.3.
     */
    const int kBlkCount160 = 160*2;
    if (matchCount == 1537 || matchCount == 1593) {
        int i, count;
        bool found = false;

        count = 1600 / kBlkCount160;
        LOGI(" Sub #3: looking for %d 160K volumes",
            matchCount / kBlkCount160);

        for (i = 0; i < count; i++) {
            DiskFS* pNewFS = NULL;
            DiskImg* pNewImg = NULL;
            LOGI(" Sub #3: looking for DOS volume at (%d)",
                i * kBlkCount160);
            dierr = FindSubVolume(i * kBlkCount160,
                        kBlkCount160, &pNewImg, &pNewFS);
            if (dierr == kDIErrNone) {
                if (pNewImg->GetFSFormat() == DiskImg::kFormatDOS33) {
                    AddSubVolumeToList(pNewImg, pNewFS);
                    if (i == 0)
                        MarkSubVolumeBlocks(firstBlock, kBlkCount160 - firstBlock);
                    else
                        MarkSubVolumeBlocks(i * kBlkCount160, kBlkCount160);
                } else {
                    delete pNewFS;
                    delete pNewImg;
                    pNewFS = NULL;
                    pNewImg = NULL;
                }
            }
        }
        if (found)
            return kDIErrNone;
    }

    return kDIErrNone;
}

/*
 * Look for a sub-volume at the specified location.
 *
 * On success, "*ppDiskImg" and "*ppDiskFS" are newly-allocated objects
 * of the appropriate kind.
 */
DIError DiskFSProDOS::FindSubVolume(long blockStart, long blockCount,
    DiskImg** ppDiskImg, DiskFS** ppDiskFS)
{
    DIError dierr = kDIErrNone;
    DiskFS* pNewFS = NULL;
    DiskImg* pNewImg = NULL;

    pNewImg = new DiskImg;
    if (pNewImg == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    dierr = pNewImg->OpenImage(fpImg, blockStart, blockCount);
    if (dierr != kDIErrNone) {
        LOGI(" Sub: OpenImage(%ld,%ld) failed (err=%d)",
            blockStart, blockCount, dierr);
        goto bail;
    }

    dierr = pNewImg->AnalyzeImage();
    if (dierr != kDIErrNone) {
        LOGI(" Sub: analysis failed (err=%d)", dierr);
        goto bail;
    }

    if (pNewImg->GetFSFormat() == DiskImg::kFormatUnknown ||
        pNewImg->GetSectorOrder() == DiskImg::kSectorOrderUnknown)
    {
        LOGI(" Sub: unable to identify filesystem");
        dierr = kDIErrFilesystemNotFound;
        goto bail;
    }

    /* open a DiskFS for the sub-image */
    LOGI(" Sub DiskImg succeeded, opening DiskFS");
    pNewFS = pNewImg->OpenAppropriateDiskFS();
    if (pNewFS == NULL) {
        LOGI(" Sub: OpenAppropriateDiskFS failed");
        dierr = kDIErrUnsupportedFSFmt;
        goto bail;
    }

    /* load the files from the sub-image */
    dierr = pNewFS->Initialize(pNewImg, kInitFull);
    if (dierr != kDIErrNone) {
        LOGE(" Sub: error %d reading list of files from disk", dierr);
        goto bail;
    }

bail:
    if (dierr != kDIErrNone) {
        delete pNewFS;
        delete pNewImg;
    } else {
        assert(pNewImg != NULL && pNewFS != NULL);
        *ppDiskImg = pNewImg;
        *ppDiskFS = pNewFS;
    }
    return dierr;
}

/*
 * Mark the blocks used by a sub-volume as in-use.
 */
void DiskFSProDOS::MarkSubVolumeBlocks(long block, long count)
{
    VolumeUsage::ChunkState cstate;

    while (count--) {
        if (fVolumeUsage.GetChunkState(block, &cstate) != kDIErrNone) {
            assert(false);
            return;
        }

        assert(cstate.isMarkedUsed && !cstate.isUsed);
        cstate.isUsed = true;
        cstate.purpose = VolumeUsage::kChunkPurposeEmbedded;
        if (fVolumeUsage.SetChunkState(block, &cstate) != kDIErrNone) {
            assert(false);
            return;
        }

        block++;
    }
}

/*
 * Put a ProDOS filesystem image on the specified DiskImg.
 */
DIError DiskFSProDOS::Format(DiskImg* pDiskImg, const char* volName)
{
    DIError dierr = kDIErrNone;
    const bool allowLowerCase = (GetParameter(kParmProDOS_AllowLowerCase) != 0);
    uint8_t blkBuf[kBlkSize];
    long formatBlocks;

    if (!IsValidVolumeName(volName))
        return kDIErrInvalidArg;

    /* set fpImg so calls that rely on it will work; we un-set it later */
    assert(fpImg == NULL);
    SetDiskImg(pDiskImg);

    LOGI(" ProDOS formatting disk image");

    /* write ProDOS blocks */
    dierr = fpImg->OverrideFormat(fpImg->GetPhysicalFormat(),
                DiskImg::kFormatGenericProDOSOrd, fpImg->GetSectorOrder());
    if (dierr != kDIErrNone)
        goto bail;

    formatBlocks = pDiskImg->GetNumBlocks();
    if (formatBlocks > 65536) {
        LOGI(" ProDOS: rejecting format req blocks=%ld", formatBlocks);
        assert(false);
        return kDIErrInvalidArg;
    }
    if (formatBlocks == 65536) {
        LOGI(" ProDOS: trimming FS size from 65536 to 65535");
        formatBlocks = 65535;
    }

    /*
     * We should now zero out the disk blocks, but on a 32MB volume that can
     * take a little while.  The blocks are zeroed for us when a disk is
     * created, so this is really only needed if we're re-formatting an
     * existing disk.  CiderPress currently doesn't do that, so we're going
     * to skip it here.
     */
//  dierr = fpImg->ZeroImage();
    LOGI(" ProDOS  (not zeroing blocks)");

    /*
     * Start by writing blocks 0 and 1 (the boot blocks).  This is done from
     * a standard boot block image that happens to be essentially the same
     * for all types of disks.  (Apparently these blocks are only used when
     * booting 5.25" disks?)
     */
    dierr = WriteBootBlocks();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Write the four-block disk volume entry.  Start by writing the three
     * empty ones (which only have the prev/next pointers), and finish by
     * writing the first block, which has the volume directory header.
     */
    int i;
    memset(blkBuf, 0, sizeof(blkBuf));
    for (i = kVolHeaderBlock+1; i < kVolHeaderBlock+kFormatVolDirNumBlocks; i++)
    {
        PutShortLE(&blkBuf[0x00], i-1);
        if (i == kVolHeaderBlock+kFormatVolDirNumBlocks-1)
            PutShortLE(&blkBuf[0x02], 0);
        else
            PutShortLE(&blkBuf[0x02], i+1);

        dierr = fpImg->WriteBlock(i, blkBuf);
        if (dierr != kDIErrNone) {
            LOGI(" Format: block %d write failed (err=%d)", i, dierr);
            goto bail;
        }
    }

    char upperName[A2FileProDOS::kMaxFileName+1];
    uint16_t lcFlags;
    time_t now;

    now = time(NULL);

    /*
     * Compute the lower-case flags, if desired.  The test for "allowLowerCase"
     * is probably bogus, because in most cases we just got created by the
     * DiskImg and the app hasn't had time to set the "allow lower" flag.
     * So it defaults to "enabled", which means the app needs to manually
     * change the volume name to lower case.
     */
    UpperCaseName(upperName, volName);
    lcFlags = 0;
    if (allowLowerCase)
        lcFlags = GenerateLowerCaseBits(upperName, volName, false);

    PutShortLE(&blkBuf[0x00], 0);
    PutShortLE(&blkBuf[0x02], kVolHeaderBlock+1);
    blkBuf[0x04] = (uint8_t)(strlen(upperName) | (A2FileProDOS::kStorageVolumeDirHeader << 4));
    strncpy((char*) &blkBuf[0x05], upperName, A2FileProDOS::kMaxFileName);
    PutLongLE(&blkBuf[0x16], A2FileProDOS::ConvertProDate(now));
    PutShortLE(&blkBuf[0x1a], lcFlags);
    PutLongLE(&blkBuf[0x1c], A2FileProDOS::ConvertProDate(now));
    blkBuf[0x20] = 0;           // GS/OS uses 5?
    /* min_version is zero */
    blkBuf[0x22] = 0xe3;        // access (format/rename/backup/write/read)
    blkBuf[0x23] = 0x27;        // entry_length: always $27
    blkBuf[0x24] = 0x0d;        // entries_per_block: always $0d
    /* file_count is zero - does not include volume dir */
    PutShortLE(&blkBuf[0x27], kVolHeaderBlock + kFormatVolDirNumBlocks); // bit_map_pointer
    PutShortLE(&blkBuf[0x29], (uint16_t) formatBlocks); // total_blocks
    dierr = fpImg->WriteBlock(kVolHeaderBlock, blkBuf);
    if (dierr != kDIErrNone) {
        LOGI(" Format: block %d write failed (err=%d)",
            kVolHeaderBlock, dierr);
        goto bail;
    }

    /* check our work, and set some object fields, by reading what we wrote */
    dierr = LoadVolHeader();
    if (dierr != kDIErrNone) {
        LOGI(" GLITCH: couldn't read header we just wrote (err=%d)", dierr);
        goto bail;
    }

    /*
     * Generate the initial block usage map.  The only entries in use are
     * right at the start of the disk.
     */
    CreateEmptyBlockMap();

    /* don't do this -- assume they're going to call Initialize() later */
    //ScanVolBitmap();

bail:
    SetDiskImg(NULL);        // shouldn't really be set by us
    return dierr;
}


/*
 * The standard boot block found on ProDOS disks.  The same thing appears
 * to be written to both 5.25" and 3.5" disks, with some modifications
 * made for HD images.
 *
 * This is block 0; block 1 is either zeroed out or filled with a repeating
 * pattern.
 */
const uint8_t gFloppyBlock0[512] = {
    0x01, 0x38, 0xb0, 0x03, 0x4c, 0x32, 0xa1, 0x86, 0x43, 0xc9, 0x03, 0x08,
    0x8a, 0x29, 0x70, 0x4a, 0x4a, 0x4a, 0x4a, 0x09, 0xc0, 0x85, 0x49, 0xa0,
    0xff, 0x84, 0x48, 0x28, 0xc8, 0xb1, 0x48, 0xd0, 0x3a, 0xb0, 0x0e, 0xa9,
    0x03, 0x8d, 0x00, 0x08, 0xe6, 0x3d, 0xa5, 0x49, 0x48, 0xa9, 0x5b, 0x48,
    0x60, 0x85, 0x40, 0x85, 0x48, 0xa0, 0x63, 0xb1, 0x48, 0x99, 0x94, 0x09,
    0xc8, 0xc0, 0xeb, 0xd0, 0xf6, 0xa2, 0x06, 0xbc, 0x1d, 0x09, 0xbd, 0x24,
    0x09, 0x99, 0xf2, 0x09, 0xbd, 0x2b, 0x09, 0x9d, 0x7f, 0x0a, 0xca, 0x10,
    0xee, 0xa9, 0x09, 0x85, 0x49, 0xa9, 0x86, 0xa0, 0x00, 0xc9, 0xf9, 0xb0,
    0x2f, 0x85, 0x48, 0x84, 0x60, 0x84, 0x4a, 0x84, 0x4c, 0x84, 0x4e, 0x84,
    0x47, 0xc8, 0x84, 0x42, 0xc8, 0x84, 0x46, 0xa9, 0x0c, 0x85, 0x61, 0x85,
    0x4b, 0x20, 0x12, 0x09, 0xb0, 0x68, 0xe6, 0x61, 0xe6, 0x61, 0xe6, 0x46,
    0xa5, 0x46, 0xc9, 0x06, 0x90, 0xef, 0xad, 0x00, 0x0c, 0x0d, 0x01, 0x0c,
    0xd0, 0x6d, 0xa9, 0x04, 0xd0, 0x02, 0xa5, 0x4a, 0x18, 0x6d, 0x23, 0x0c,
    0xa8, 0x90, 0x0d, 0xe6, 0x4b, 0xa5, 0x4b, 0x4a, 0xb0, 0x06, 0xc9, 0x0a,
    0xf0, 0x55, 0xa0, 0x04, 0x84, 0x4a, 0xad, 0x02, 0x09, 0x29, 0x0f, 0xa8,
    0xb1, 0x4a, 0xd9, 0x02, 0x09, 0xd0, 0xdb, 0x88, 0x10, 0xf6, 0x29, 0xf0,
    0xc9, 0x20, 0xd0, 0x3b, 0xa0, 0x10, 0xb1, 0x4a, 0xc9, 0xff, 0xd0, 0x33,
    0xc8, 0xb1, 0x4a, 0x85, 0x46, 0xc8, 0xb1, 0x4a, 0x85, 0x47, 0xa9, 0x00,
    0x85, 0x4a, 0xa0, 0x1e, 0x84, 0x4b, 0x84, 0x61, 0xc8, 0x84, 0x4d, 0x20,
    0x12, 0x09, 0xb0, 0x17, 0xe6, 0x61, 0xe6, 0x61, 0xa4, 0x4e, 0xe6, 0x4e,
    0xb1, 0x4a, 0x85, 0x46, 0xb1, 0x4c, 0x85, 0x47, 0x11, 0x4a, 0xd0, 0xe7,
    0x4c, 0x00, 0x20, 0x4c, 0x3f, 0x09, 0x26, 0x50, 0x52, 0x4f, 0x44, 0x4f,
    0x53, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xa5, 0x60,
    0x85, 0x44, 0xa5, 0x61, 0x85, 0x45, 0x6c, 0x48, 0x00, 0x08, 0x1e, 0x24,
    0x3f, 0x45, 0x47, 0x76, 0xf4, 0xd7, 0xd1, 0xb6, 0x4b, 0xb4, 0xac, 0xa6,
    0x2b, 0x18, 0x60, 0x4c, 0xbc, 0x09, 0xa9, 0x9f, 0x48, 0xa9, 0xff, 0x48,
    0xa9, 0x01, 0xa2, 0x00, 0x4c, 0x79, 0xf4, 0x20, 0x58, 0xfc, 0xa0, 0x1c,
    0xb9, 0x50, 0x09, 0x99, 0xae, 0x05, 0x88, 0x10, 0xf7, 0x4c, 0x4d, 0x09,
    0xaa, 0xaa, 0xaa, 0xa0, 0xd5, 0xce, 0xc1, 0xc2, 0xcc, 0xc5, 0xa0, 0xd4,
    0xcf, 0xa0, 0xcc, 0xcf, 0xc1, 0xc4, 0xa0, 0xd0, 0xd2, 0xcf, 0xc4, 0xcf,
    0xd3, 0xa0, 0xaa, 0xaa, 0xaa, 0xa5, 0x53, 0x29, 0x03, 0x2a, 0x05, 0x2b,
    0xaa, 0xbd, 0x80, 0xc0, 0xa9, 0x2c, 0xa2, 0x11, 0xca, 0xd0, 0xfd, 0xe9,
    0x01, 0xd0, 0xf7, 0xa6, 0x2b, 0x60, 0xa5, 0x46, 0x29, 0x07, 0xc9, 0x04,
    0x29, 0x03, 0x08, 0x0a, 0x28, 0x2a, 0x85, 0x3d, 0xa5, 0x47, 0x4a, 0xa5,
    0x46, 0x6a, 0x4a, 0x4a, 0x85, 0x41, 0x0a, 0x85, 0x51, 0xa5, 0x45, 0x85,
    0x27, 0xa6, 0x2b, 0xbd, 0x89, 0xc0, 0x20, 0xbc, 0x09, 0xe6, 0x27, 0xe6,
    0x3d, 0xe6, 0x3d, 0xb0, 0x03, 0x20, 0xbc, 0x09, 0xbc, 0x88, 0xc0, 0x60,
    0xa5, 0x40, 0x0a, 0x85, 0x53, 0xa9, 0x00, 0x85, 0x54, 0xa5, 0x53, 0x85,
    0x50, 0x38, 0xe5, 0x51, 0xf0, 0x14, 0xb0, 0x04, 0xe6, 0x53, 0x90, 0x02,
    0xc6, 0x53, 0x38, 0x20, 0x6d, 0x09, 0xa5, 0x50, 0x18, 0x20, 0x6f, 0x09,
    0xd0, 0xe3, 0xa0, 0x7f, 0x84, 0x52, 0x08, 0x28, 0x38, 0xc6, 0x52, 0xf0,
    0xce, 0x18, 0x08, 0x88, 0xf0, 0xf5, 0xbd, 0x8c, 0xc0, 0x10, 0xfb, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t gHDBlock0[] = {
    0x01, 0x38, 0xb0, 0x03, 0x4c, 0x1c, 0x09, 0x78, 0x86, 0x43, 0xc9, 0x03,
    0x08, 0x8a, 0x29, 0x70, 0x4a, 0x4a, 0x4a, 0x4a, 0x09, 0xc0, 0x85, 0x49,
    0xa0, 0xff, 0x84, 0x48, 0x28, 0xc8, 0xb1, 0x48, 0xd0, 0x3a, 0xb0, 0x0e,
    0xa9, 0x03, 0x8d, 0x00, 0x08, 0xe6, 0x3d, 0xa5, 0x49, 0x48, 0xa9, 0x5b,
    0x48, 0x60, 0x85, 0x40, 0x85, 0x48, 0xa0, 0x5e, 0xb1, 0x48, 0x99, 0x94,
    0x09, 0xc8, 0xc0, 0xeb, 0xd0, 0xf6, 0xa2, 0x06, 0xbc, 0x32, 0x09, 0xbd,
    0x39, 0x09, 0x99, 0xf2, 0x09, 0xbd, 0x40, 0x09, 0x9d, 0x7f, 0x0a, 0xca,
    0x10, 0xee, 0xa9, 0x09, 0x85, 0x49, 0xa9, 0x86, 0xa0, 0x00, 0xc9, 0xf9,
    0xb0, 0x2f, 0x85, 0x48, 0x84, 0x60, 0x84, 0x4a, 0x84, 0x4c, 0x84, 0x4e,
    0x84, 0x47, 0xc8, 0x84, 0x42, 0xc8, 0x84, 0x46, 0xa9, 0x0c, 0x85, 0x61,
    0x85, 0x4b, 0x20, 0x27, 0x09, 0xb0, 0x66, 0xe6, 0x61, 0xe6, 0x61, 0xe6,
    0x46, 0xa5, 0x46, 0xc9, 0x06, 0x90, 0xef, 0xad, 0x00, 0x0c, 0x0d, 0x01,
    0x0c, 0xd0, 0x52, 0xa9, 0x04, 0xd0, 0x02, 0xa5, 0x4a, 0x18, 0x6d, 0x23,
    0x0c, 0xa8, 0x90, 0x0d, 0xe6, 0x4b, 0xa5, 0x4b, 0x4a, 0xb0, 0x06, 0xc9,
    0x0a, 0xf0, 0x71, 0xa0, 0x04, 0x84, 0x4a, 0xad, 0x20, 0x09, 0x29, 0x0f,
    0xa8, 0xb1, 0x4a, 0xd9, 0x20, 0x09, 0xd0, 0xdb, 0x88, 0x10, 0xf6, 0xa0,
    0x16, 0xb1, 0x4a, 0x4a, 0x6d, 0x1f, 0x09, 0x8d, 0x1f, 0x09, 0xa0, 0x11,
    0xb1, 0x4a, 0x85, 0x46, 0xc8, 0xb1, 0x4a, 0x85, 0x47, 0xa9, 0x00, 0x85,
    0x4a, 0xa0, 0x1e, 0x84, 0x4b, 0x84, 0x61, 0xc8, 0x84, 0x4d, 0x20, 0x27,
    0x09, 0xb0, 0x35, 0xe6, 0x61, 0xe6, 0x61, 0xa4, 0x4e, 0xe6, 0x4e, 0xb1,
    0x4a, 0x85, 0x46, 0xb1, 0x4c, 0x85, 0x47, 0x11, 0x4a, 0xd0, 0x18, 0xa2,
    0x01, 0xa9, 0x00, 0xa8, 0x91, 0x60, 0xc8, 0xd0, 0xfb, 0xe6, 0x61, 0xea,
    0xea, 0xca, 0x10, 0xf4, 0xce, 0x1f, 0x09, 0xf0, 0x07, 0xd0, 0xd8, 0xce,
    0x1f, 0x09, 0xd0, 0xca, 0x58, 0x4c, 0x00, 0x20, 0x4c, 0x47, 0x09, 0x02,
    0x26, 0x50, 0x52, 0x4f, 0x44, 0x4f, 0x53, 0xa5, 0x60, 0x85, 0x44, 0xa5,
    0x61, 0x85, 0x45, 0x6c, 0x48, 0x00, 0x08, 0x1e, 0x24, 0x3f, 0x45, 0x47,
    0x76, 0xf4, 0xd7, 0xd1, 0xb6, 0x4b, 0xb4, 0xac, 0xa6, 0x2b, 0x18, 0x60,
    0x4c, 0xbc, 0x09, 0x20, 0x58, 0xfc, 0xa0, 0x14, 0xb9, 0x58, 0x09, 0x99,
    0xb1, 0x05, 0x88, 0x10, 0xf7, 0x4c, 0x55, 0x09, 0xd5, 0xce, 0xc1, 0xc2,
    0xcc, 0xc5, 0xa0, 0xd4, 0xcf, 0xa0, 0xcc, 0xcf, 0xc1, 0xc4, 0xa0, 0xd0,
    0xd2, 0xcf, 0xc4, 0xcf, 0xd3, 0xa5, 0x53, 0x29, 0x03, 0x2a, 0x05, 0x2b,
    0xaa, 0xbd, 0x80, 0xc0, 0xa9, 0x2c, 0xa2, 0x11, 0xca, 0xd0, 0xfd, 0xe9,
    0x01, 0xd0, 0xf7, 0xa6, 0x2b, 0x60, 0xa5, 0x46, 0x29, 0x07, 0xc9, 0x04,
    0x29, 0x03, 0x08, 0x0a, 0x28, 0x2a, 0x85, 0x3d, 0xa5, 0x47, 0x4a, 0xa5,
    0x46, 0x6a, 0x4a, 0x4a, 0x85, 0x41, 0x0a, 0x85, 0x51, 0xa5, 0x45, 0x85,
    0x27, 0xa6, 0x2b, 0xbd, 0x89, 0xc0, 0x20, 0xbc, 0x09, 0xe6, 0x27, 0xe6,
    0x3d, 0xe6, 0x3d, 0xb0, 0x03, 0x20, 0xbc, 0x09, 0xbc, 0x88, 0xc0, 0x60,
    0xa5, 0x40, 0x0a, 0x85, 0x53, 0xa9, 0x00, 0x85, 0x54, 0xa5, 0x53, 0x85,
    0x50, 0x38, 0xe5, 0x51, 0xf0, 0x14, 0xb0, 0x04, 0xe6, 0x53, 0x90, 0x02,
    0xc6, 0x53, 0x38, 0x20, 0x6d, 0x09, 0xa5, 0x50, 0x18, 0x20, 0x6f, 0x09,
    0xd0, 0xe3, 0xa0, 0x7f, 0x84, 0x52, 0x08, 0x28, 0x38, 0xc6, 0x52, 0xf0,
    0xce, 0x18, 0x08, 0x88, 0xf0, 0xf5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * Write the ProDOS boot blocks onto the disk image.
 */
DIError DiskFSProDOS::WriteBootBlocks(void)
{
    DIError dierr;
    uint8_t block0[512];
    uint8_t block1[512];
    bool isHD;

    assert(fpImg->GetHasBlocks());

    if (fpImg->GetNumBlocks() == 280 || fpImg->GetNumBlocks() == 1600)
        isHD = false;
    else
        isHD = true;

    if (isHD) {
        memcpy(block0, gHDBlock0, sizeof(block0));
        // repeating 0x42 0x48 pattern
        int i;
        uint8_t* ucp;
        for (i = 0, ucp = block1; i < (int)sizeof(block1); i++)
            *ucp++ = 0x42 + 6 * (i & 0x01);
    } else {
        memcpy(block0, gFloppyBlock0, sizeof(block0));
        memset(block1, 0, sizeof(block1));
    }

    dierr = fpImg->WriteBlock(0, block0);
    if (dierr != kDIErrNone) {
        LOGI(" WriteBootBlocks: block0 write failed (err=%d)", dierr);
        return dierr;
    }
    dierr = fpImg->WriteBlock(1, block1);
    if (dierr != kDIErrNone) {
        LOGI(" WriteBootBlocks: block1 write failed (err=%d)", dierr);
        return dierr;
    }

    return kDIErrNone;
}

/*
 * Create a new, empty file.  There are three different kinds of files we
 * need to be able to handle:
 *  (1) Standard file.  Create the directory entry and an empty "seedling"
 *    file with one block allocated.  It does not appear that "sparse"
 *    allocation applies to seedlings.
 *  (2) Extended file.  Create the directory entry, the extended key block,
 *    and allocate one seedling block for each fork.
 *  (3) Subdirectory.  Allocate a block for the subdir and fill in the
 *    details in the subdir header.
 *
 * In all cases we need to add a new directory entry as well.
 *
 * By not flushing the updated block usage map and the updated directory
 * block(s) until we're done, we can abort our changes at any time if we
 * encounter a damaged sector or run out of disk space.  We do need to be
 * careful when updating our internal copies of things like file storage
 * types and lengths, updating them only after everything else has
 * succeeded.
 *
 * NOTE: if we detect an empty directory holder, "*ppNewFile" does NOT
 * end up pointing at a file.
 *
 * NOTE: kParm_CreateUnique does *not* apply to creating subdirectories.
 */
DIError DiskFSProDOS::CreateFile(const CreateParms* pParms, A2File** ppNewFile)
{
    DIError dierr = kDIErrNone;
    char* normalizedPath = NULL;
    char* basePath = NULL;
    char* fileName = NULL;
    A2FileProDOS* pSubdir = NULL;
    A2FileDescr* pOpenSubdir = NULL;
    A2FileProDOS* pNewFile = NULL;
    uint8_t* subdirBuf = NULL;
    const bool allowLowerCase = (GetParameter(kParmProDOS_AllowLowerCase) != 0);
    const bool createUnique = (GetParameter(kParm_CreateUnique) != 0);
    char upperName[A2FileProDOS::kMaxFileName+1];
    char lowerName[A2FileProDOS::kMaxFileName+1];

    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;
    if (!fDiskIsGood)
        return kDIErrBadDiskImage;

    assert(pParms != NULL);
    assert(pParms->pathName != NULL);
    assert(pParms->storageType == A2FileProDOS::kStorageSeedling ||
           pParms->storageType == A2FileProDOS::kStorageExtended ||
           pParms->storageType == A2FileProDOS::kStorageDirectory);
    // kStorageVolumeDirHeader not allowed -- that's created by Format
    LOGI(" ProDOS ---v--- CreateFile '%s'", pParms->pathName);
    *ppNewFile = NULL;

    /*
     * Normalize the pathname so that all components are ProDOS-safe
     * and separated by ':'.
     */
    assert(pParms->pathName != NULL);
    dierr = DoNormalizePath(pParms->pathName, pParms->fssep,
                &normalizedPath);
    if (dierr != kDIErrNone)
        goto bail;
    assert(normalizedPath != NULL);

    /*
     * Split the base path and filename apart.
     */
    char* cp;
    cp = strrchr(normalizedPath, A2FileProDOS::kFssep);
    if (cp == NULL) {
        assert(basePath == NULL);
        fileName = normalizedPath;
    } else {
        fileName = new char[strlen(cp+1) +1];
        strcpy(fileName, cp+1);
        *cp = '\0';
        basePath = normalizedPath;
    }
    normalizedPath = NULL;   // either fileName or basePath points here now

    assert(fileName != NULL);
    //LOGI(" ProDOS normalized to '%s':'%s'",
    //  basePath == NULL ? "" : basePath, fileName);

    /*
     * Open the base path.  If it doesn't exist, create it recursively.
     */
    if (basePath != NULL) {
        LOGI(" ProDOS  Creating '%s' in '%s'", fileName, basePath);
        /* open the named subdir, creating it if it doesn't exist */
        pSubdir = (A2FileProDOS*)GetFileByName(basePath);
        if (pSubdir == NULL) {
            LOGI("  ProDOS  Creating subdir '%s'", basePath);
            A2File* pNewSub;
            CreateParms newDirParms;
            newDirParms.pathName = basePath;
            newDirParms.fssep = A2FileProDOS::kFssep;
            newDirParms.storageType = A2FileProDOS::kStorageDirectory;
            newDirParms.fileType = kTypeDIR;    // 0x0f
            newDirParms.auxType = 0;
            newDirParms.access = 0xe3;  // unlocked, backup bit set
            newDirParms.createWhen = newDirParms.modWhen = time(NULL);
            dierr = this->CreateFile(&newDirParms, &pNewSub);
            if (dierr != kDIErrNone)
                goto bail;
            assert(pNewSub != NULL);

            pSubdir = (A2FileProDOS*) pNewSub;
        }

        /*
         * And now the annoying part.  We need to reconstruct basePath out
         * of the filenames actually present, rather than relying on the
         * argument passed in.  That's because some directories might have
         * lower-case flags and some might not, and we do case-insensitive
         * comparisons.  It's not crucial for our inner workings, but the
         * linear file list in the DiskFS should have accurate strings.
         * (It'll work just fine, but the display might show the wrong values
         * for parent directories until they reload the disk.)
         *
         * On the bright side, we know exactly how long the string needs
         * to be, so we can just stomp on it in place.  Assuming, of course,
         * that the filename created matches up with what the filename
         * normalizer came up with, which we can guarantee since (a) everybody
         * uses the same normalizer and (b) the "uniqueify" stuff doesn't
         * kick in for subdirs because we wouldn't be creating a new subdir
         * if it didn't already exist.
         *
         * This is essentially the same as RegeneratePathName(), but that's
         * meant for a situation where the filename already exists.
         */
        A2FileProDOS* pBaseDir = pSubdir;
        int basePathLen = strlen(basePath);
        while (!pBaseDir->IsVolumeDirectory()) {
            const char* fixedName = pBaseDir->GetFileName();
            int fixedLen = strlen(fixedName);
            if (fixedLen > basePathLen) {
                assert(false);
                break;
            }
            assert(basePathLen == fixedLen ||
                   *(basePath + (basePathLen-fixedLen-1)) == kDIFssep);
            memcpy(basePath + (basePathLen-fixedLen), fixedName, fixedLen);
            basePathLen -= fixedLen+1;

            pBaseDir = (A2FileProDOS*) pBaseDir->GetParent();
            assert(pBaseDir != NULL);
        }
        // check the math
        if (pSubdir->IsVolumeDirectory())
            assert(basePathLen == 0);
        else
            assert(basePathLen == -1);
    } else {
        /* open the volume directory */
        LOGI(" ProDOS  Creating '%s' in volume dir", fileName);
        /* volume dir must be first in the list */
        pSubdir = (A2FileProDOS*) GetNextFile(NULL);
        assert(pSubdir != NULL);
        assert(pSubdir->IsVolumeDirectory());
    }
    if (pSubdir == NULL) {
        LOGI(" ProDOS Unable to open subdir '%s'", basePath);
        dierr = kDIErrFileNotFound;
        goto bail;
    }

    /*
     * Load the block usage map into memory.  All changes, to the end of this
     * function, are made to the in-memory copy and can be "undone" by simply
     * throwing the temporary map away.
     */
    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        return dierr;

    /*
     * Load the subdir or volume dir into memory, and alloc a new directory
     * entry.
     */
    dierr = pSubdir->Open(&pOpenSubdir, false);
    if (dierr != kDIErrNone)
        goto bail;

    uint8_t* dirEntryPtr;
    long dirLen;
    uint16_t dirBlock, dirKeyBlock;
    int dirEntrySlot;
    dierr = AllocDirEntry(pOpenSubdir, &subdirBuf, &dirLen, &dirEntryPtr,
                &dirKeyBlock, &dirEntrySlot, &dirBlock);
    if (dierr != kDIErrNone)
        goto bail;

    assert(subdirBuf != NULL);
    assert(dirLen > 0);
    assert(dirKeyBlock > 0);
    assert(dirEntrySlot >= 0);
    assert(dirBlock > 0);

    /*
     * Create a copy of the filename with everything in upper case and spaces
     * changed to periods.
     */
    UpperCaseName(upperName, fileName);

    /*
     * Make the name unique within the current directory.  This requires
     * appending digits until the name doesn't match any others.
     *
     * The filename buffer ("upperName") must be able to hold kMaxFileName+1
     * chars.  It will be modified in place.
     */
    if (createUnique &&
        pParms->storageType != A2FileProDOS::kStorageDirectory)
    {
        MakeFileNameUnique(subdirBuf, dirLen, upperName);
    } else {
        /* check to see if it already exists */
        if (NameExistsInDir(subdirBuf, dirLen, upperName)) {
            if (pParms->storageType == A2FileProDOS::kStorageDirectory)
                dierr = kDIErrDirectoryExists;
            else
                dierr = kDIErrFileExists;
            goto bail;
        }
    }

    /*
     * Allocate file storage and initialize:
     *  - For directory, a single block with the directory header.
     *  - For seedling, an empty block.
     *  - For extended, an extended key block entry and two empty blocks.
     */
    long keyBlock;
    int blocksUsed;
    int newEOF;
    keyBlock = -1;
    blocksUsed = newEOF = -1;

    dierr = AllocInitialFileStorage(pParms, upperName, dirBlock,
                dirEntrySlot, &keyBlock, &blocksUsed, &newEOF);
    if (dierr != kDIErrNone)
        goto bail;

    assert(blocksUsed > 0);
    assert(keyBlock > 0);
    assert(newEOF >= 0);

    /*
     * Fill out the newly-created directory entry pointed to by "dirEntryPtr".
     *
     * ProDOS filenames are always stored in upper case.  ProDOS 8 v1.8 and
     * later allow lower-case names with '.' converting to ' '.  We optionally
     * set the flags here, using the original file name to decide which parts
     * are lower case.  (Some parts of the original may have been stomped
     * when the name was made unique, so we need to watch for that.)
     */
    dirEntryPtr[0x00] = (uint8_t)((pParms->storageType << 4) | strlen(upperName));
    strncpy((char*) &dirEntryPtr[0x01], upperName, A2FileProDOS::kMaxFileName);
    if (pParms->fileType >= 0 && pParms->fileType <= 0xff)
        dirEntryPtr[0x10] = (uint8_t) pParms->fileType;
    else
        dirEntryPtr[0x10] = 0;      // HFS long type?
    PutShortLE(&dirEntryPtr[0x11], (uint16_t) keyBlock);
    PutShortLE(&dirEntryPtr[0x13], blocksUsed);
    PutShortLE(&dirEntryPtr[0x15], newEOF);
    dirEntryPtr[0x17] = 0;      // high byte of EOF
    PutLongLE(&dirEntryPtr[0x18], A2FileProDOS::ConvertProDate(pParms->createWhen));
    if (allowLowerCase) {
        uint16_t lcBits;
        lcBits = GenerateLowerCaseBits(upperName, fileName, false);
        GenerateLowerCaseName(upperName, lowerName, lcBits, false);
        lowerName[strlen(upperName)] = '\0';

        PutShortLE(&dirEntryPtr[0x1c], lcBits);
    } else {
        strcpy(lowerName, upperName);
        PutShortLE(&dirEntryPtr[0x1c], 0);  // version, min_version
    }
    dirEntryPtr[0x1e] = pParms->access;
    if (pParms->auxType >= 0 && pParms->auxType <= 0xffff)
        PutShortLE(&dirEntryPtr[0x1f], (uint16_t) pParms->auxType);
    else
        PutShortLE(&dirEntryPtr[0x1f], 0);
    PutLongLE(&dirEntryPtr[0x21], A2FileProDOS::ConvertProDate(pParms->modWhen));
    PutShortLE(&dirEntryPtr[0x25], dirKeyBlock);

    /*
     * Write updated directory.  If this succeeds, we can no longer undo
     * what we have done by simply bailing.  If this fails partway through,
     * we might have a corrupted disk, so it's best to ensure that it's not
     * going to fail before we call.
     *
     * Assuming this isn't a nibble image with I/O errors, the only way we
     * can really fail is by running out of disk space.  The block has been
     * pre-allocated, so this should always work.
     */
    dierr = pOpenSubdir->Write(subdirBuf, dirLen);
    if (dierr != kDIErrNone) {
        LOGI(" ProDOS directory write failed (dirLen=%ld)", dirLen);
        goto bail;
    }

    /*
     * Flush updated block usage map.
     */
    dierr = SaveVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Success!
     *
     * Create an A2File entry for this, and add it to the list.  The calls
     * below will re-process some of what we just created, which is slightly
     * inefficient but helps guarantee that we aren't creating bogus data
     * structures that won't match what we see when the disk is reloaded.
     *
     * - Regen or update internal VolumeUsage map??  Throw it away or mark
     * it as invalid?
     */
    pNewFile = new A2FileProDOS(this);

    A2FileProDOS::DirEntry* pEntry;
    pEntry = &pNewFile->fDirEntry;

    A2FileProDOS::InitDirEntry(pEntry, dirEntryPtr);

    pNewFile->fParentDirBlock = dirBlock;
    pNewFile->fParentDirIdx = (dirEntrySlot-1) % kEntriesPerBlock;
    pNewFile->fSparseDataEof = 0;
    pNewFile->fSparseRsrcEof = 0;

    /*
     * Get the properly-cased filename for the file list.  We already have
     * a name in "lowerName", but it doesn't take AppleWorks aux type
     * case stuff into account.  If necessary, deal with it now.
     */
    if (A2FileProDOS::UsesAppleWorksAuxType(pNewFile->fDirEntry.fileType)) {
        DiskFSProDOS::GenerateLowerCaseName(pNewFile->fDirEntry.fileName,
            lowerName, pNewFile->fDirEntry.auxType, true);
    }
    pNewFile->SetPathName(basePath == NULL ? "" : basePath, lowerName);

    if (pEntry->storageType == A2FileProDOS::kStorageExtended) {
        dierr = ReadExtendedInfo(pNewFile);
        if (dierr != kDIErrNone) {
            LOGI(" ProDOS GLITCH: readback of extended block failed!");
            delete pNewFile;
            goto bail;
        }
    }

    pNewFile->SetParent(pSubdir);
    //pNewFile->Dump();

    /*
     * Because we're hierarchical, and we guarantee that the contents of
     * subdirectories are grouped together, we must insert the file into an
     * appropriate place in the list rather than just throwing it onto the
     * end.
     *
     * The proper location for the new file in the linear list is after the
     * previous file in our subdir.  If we're the first item in the subdir,
     * we get added right after the parent.  If not, we need to scan, starting
     * from the parent, for an entry in the file list whose key block pointer
     * matches that of the previous item in the list.
     *
     * We wouldn't be this far if the disk were damaged, so we don't have to
     * worry too much about weirdness.  The directory entry allocator always
     * returns the first available, so we know the previous entry is valid.
     */
    uint8_t* prevDirEntryPtr;
    prevDirEntryPtr = GetPrevDirEntry(subdirBuf, dirEntryPtr);
    if (prevDirEntryPtr == NULL) {
        /* previous entry is volume or subdir header */
        InsertFileInList(pNewFile, pNewFile->GetParent());
        LOGI("Inserted '%s' after '%s'",
            pNewFile->GetPathName(), pNewFile->GetParent()->GetPathName());
    } else {
        /* dig out the key block pointer and find the matching file */
        uint16_t prevKeyBlock;
        assert((prevDirEntryPtr[0x00] & 0xf0) != 0);        // verify storage type
        prevKeyBlock = GetShortLE(&prevDirEntryPtr[0x11]);
        A2File* pPrev;
        pPrev = FindFileByKeyBlock(pNewFile->GetParent(), prevKeyBlock);
        if (pPrev == NULL) {
            /* should be impossible! */
            assert(false);
            AddFileToList(pNewFile);
        } else {
            /* insert the new file in the list after the previous file */
            InsertFileInList(pNewFile, pPrev);
        }
    }
//  LOGI("LIST NOW:");
//  DumpFileList();

    *ppNewFile = pNewFile;
    pNewFile = NULL;

bail:
    delete pNewFile;
    if (pOpenSubdir != NULL)
        pOpenSubdir->Close();   // writes updated dir entry in parent dir
    FreeVolBitmap();
    delete[] normalizedPath;
    delete[] subdirBuf;
    delete[] fileName;
    delete[] basePath;
    LOGI(" ProDOS ---^--- CreateFile '%s' DONE", pParms->pathName);
    return dierr;
}

/*
 * Run through the DiskFS file list, looking for an entry with a matching
 * key block.
 */
A2File* DiskFSProDOS::FindFileByKeyBlock(A2File* pStart, uint16_t keyBlock)
{
    while (pStart != NULL) {
        A2FileProDOS* pPro = (A2FileProDOS*) pStart;

        if (pPro->fDirEntry.keyPointer == keyBlock)
            return pStart;

        pStart = GetNextFile(pStart);
    }

    return NULL;
}

/*
 * Allocate the initial storage (key blocks, directory header) for a new file.
 *
 * Output values are the key block for the new file, the number of blocks
 * used, and an EOF value.
 *
 * "upperName" is the upper-case name for the file.  "dirBlock" and
 * "dirEntrySlot" refer to the entry in the higher-level directory for this
 * file, and are only needed when creating a new subdir (because the first
 * entry in a subdir points to its entry in the parent dir).
 */
DIError DiskFSProDOS::AllocInitialFileStorage(const CreateParms* pParms,
    const char* upperName, uint16_t dirBlock, int dirEntrySlot,
    long* pKeyBlock, int* pBlocksUsed, int* pNewEOF)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    long keyBlock;
    int blocksUsed;
    int newEOF;

    blocksUsed = -1;
    keyBlock = -1;
    newEOF = 0;
    memset(blkBuf, 0, sizeof(blkBuf));

    if (pParms->storageType == A2FileProDOS::kStorageSeedling) {
        keyBlock = AllocBlock();
        if (keyBlock == -1) {
            dierr = kDIErrDiskFull;
            goto bail;
        }
        blocksUsed = 1;

        /* write zeroed block */
        dierr = fpImg->WriteBlock(keyBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;
    } else if (pParms->storageType == A2FileProDOS::kStorageExtended) {
        long dataBlock, rsrcBlock;

        dataBlock = AllocBlock();
        rsrcBlock = AllocBlock();
        keyBlock = AllocBlock();
        if (dataBlock < 0 || rsrcBlock < 0 || keyBlock < 0) {
            dierr = kDIErrDiskFull;
            goto bail;
        }
        blocksUsed = 3;
        newEOF = kBlkSize;

        /* write zeroed block */
        dierr = fpImg->WriteBlock(dataBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;
        dierr = fpImg->WriteBlock(rsrcBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;

        /* fill in extended key block details */
        blkBuf[0x00] = blkBuf[0x100] = A2FileProDOS::kStorageSeedling;
        PutShortLE(&blkBuf[0x01], (uint16_t) dataBlock);
        PutShortLE(&blkBuf[0x101], (uint16_t) rsrcBlock);
        blkBuf[0x03] = blkBuf[0x103] = 1;       // blocks used (lo byte)
        /* 3 bytes at 0x05 hold EOF, currently 0 */

        dierr = fpImg->WriteBlock(keyBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;
    } else if (pParms->storageType == A2FileProDOS::kStorageDirectory) {
        keyBlock = AllocBlock();
        if (keyBlock == -1) {
            dierr = kDIErrDiskFull;
            goto bail;
        }
        blocksUsed = 1;
        newEOF = kBlkSize;

        /* fill in directory header fields */
        // 0x00: prev, set to zero
        // 0x02: next, set to zero
        blkBuf[0x04] = (uint8_t)((A2FileProDOS::kStorageSubdirHeader << 4) | strlen(upperName));
        strncpy((char*) &blkBuf[0x05], upperName, A2FileProDOS::kMaxFileName);
        blkBuf[0x14] = 0x76;    // 0x75 under old P8, 0x76 under GS/OS
        PutLongLE(&blkBuf[0x1c], A2FileProDOS::ConvertProDate(pParms->createWhen));
        blkBuf[0x20] = 5;       // 0 under 1.0, 3 under v1.4?, 5 under GS/OS
        blkBuf[0x21] = 0;
        blkBuf[0x22] = pParms->access;
        blkBuf[0x23] = kEntryLength;
        blkBuf[0x24] = kEntriesPerBlock;
        PutShortLE(&blkBuf[0x25], 0);       // file count
        PutShortLE(&blkBuf[0x27], dirBlock);
        blkBuf[0x29] = (uint8_t) dirEntrySlot;
        blkBuf[0x2a] = kEntryLength;    // the parent dir's entry length

        dierr = fpImg->WriteBlock(keyBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;
    } else {
        assert(false);
        dierr = kDIErrInternal;
        goto bail;
    }

    *pKeyBlock = keyBlock;
    *pBlocksUsed = blocksUsed;
    *pNewEOF = newEOF;

bail:
    return dierr;
}

/*
 * Scan for damaged files and mysterious or conflicting block usage map
 * entries.
 *
 * Appends some entries to the DiskImg notes, so this should only be run
 * once per DiskFS.
 *
 * This function doesn't set anything; it's effectively "const" except
 * that LoadVolBitmap is inherently non-const.
 *
 * Returns "true" if disk appears to be perfect, "false" otherwise.
 */
bool DiskFSProDOS::CheckDiskIsGood(void)
{
    DIError dierr;
    bool result = true;
    int i;

    if (fEarlyDamage)
        result = false;

    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Check the system blocks to see if any of them are marked as free.
     * If so, refuse to write to this disk.
     */
    if (!GetBlockUseEntry(0) || !GetBlockUseEntry(1)) {
        fpImg->AddNote(DiskImg::kNoteWarning, "Block 0/1 marked as free.");
        result = false;
    }
    for (i = GetNumBitmapBlocks(); i > 0; i--) {
        if (!GetBlockUseEntry(fBitMapPointer + i -1)) {
            fpImg->AddNote(DiskImg::kNoteWarning,
                "One or more bitmap blocks are marked as free.");
            result = false;
            break;
        }
    }

    /*
     * Check for used blocks that aren't marked in-use.
     *
     * This requires that VolumeUsage be accurate.  Since this function is
     * only run during initial startup, any later deviation between VU and
     * the block use map is irrelevant.
     */
    VolumeUsage::ChunkState cstate;
    long blk, notMarked, extraUsed, conflicts;
    notMarked = extraUsed = conflicts = 0;
    for (blk = 0; blk < fVolumeUsage.GetNumChunks(); blk++) {
        dierr = fVolumeUsage.GetChunkState(blk, &cstate);
        if (dierr != kDIErrNone) {
            fpImg->AddNote(DiskImg::kNoteWarning,
                "Internal volume usage error on blk=%ld.", blk);
            result = false;
            goto bail;
        }

        if (cstate.isUsed && !cstate.isMarkedUsed)
            notMarked++;
        if (!cstate.isUsed && cstate.isMarkedUsed)
            extraUsed++;
        if (cstate.purpose == VolumeUsage::kChunkPurposeConflict)
            conflicts++;
    }
    if (extraUsed > 0) {
        fpImg->AddNote(DiskImg::kNoteInfo,
            "%ld block%s marked used but not part of any file.",
            extraUsed, extraUsed == 1 ? " is" : "s are");
        // not a problem, really
    }
    if (notMarked > 0) {
        fpImg->AddNote(DiskImg::kNoteWarning,
            "%ld block%s used by files but not marked used.",
            notMarked, notMarked == 1 ? " is" : "s are");
        result = false;     // very bad -- any change could trash files
    }
    if (conflicts > 0) {
        fpImg->AddNote(DiskImg::kNoteWarning,
            "%ld block%s used by more than one file.",
            conflicts, conflicts == 1 ? " is" : "s are");
        result = false;     // kinda bad -- file deletion leads to trouble
    }

    /*
     * Check for bits set past the end of the actually-needed bits.  For
     * some reason P8 and GS/OS both examine these bits, and GS/OS will
     * freak out completely and claim the disk is unrecognizeable ("would
     * you like to format?") if they're set.
     */
    if (ScanForExtraEntries()) {
        fpImg->AddNote(DiskImg::kNoteWarning,
            "Blocks past the end of the disk are marked 'in use' in the"
            " volume bitmap.");
        /* don't flunk the disk just for this */
    }

    /*
     * Scan for "damaged" or "suspicious" files diagnosed earlier.
     */
    bool damaged, suspicious;
    ScanForDamagedFiles(&damaged, &suspicious);

    if (damaged) {
        fpImg->AddNote(DiskImg::kNoteWarning,
            "One or more files are damaged.");
        result = false;
    } else if (suspicious) {
        fpImg->AddNote(DiskImg::kNoteWarning,
            "One or more files look suspicious.");
        result = false;
    }

bail:
    FreeVolBitmap();
    return result;
}

/*
 * Test a string for validity as a ProDOS volume name.  Syntax is the same as
 * ProDOS file names, but we also disallow spaces.
 */
/*static*/ bool DiskFSProDOS::IsValidVolumeName(const char* name)
{
    assert((int) A2FileProDOS::kMaxFileName == (int) kMaxVolumeName);
    if (!IsValidFileName(name))
        return false;
    while (*name != '\0') {
        if (*name++ == ' ')
            return false;
    }
    return true;
}

/*
 * Test a string for validity as a ProDOS file name.  Names may be 1-15
 * characters long, must start with a letter, and may contain letters and
 * digits.
 *
 * Lower case and spaces (a/k/a lower-case '.') are accepted.  Trailing
 * spaces are not allowed.
 */
/*static*/ bool DiskFSProDOS::IsValidFileName(const char* name)
{
    if (name == NULL) {
        assert(false);
        return false;
    }

    /* must be 1-15 characters long */
    if (name[0] == '\0')
        return false;
    if (strlen(name) > A2FileProDOS::kMaxFileName)
        return false;

    /* must begin with letter; this also catches zero-length filenames */
    if (toupper(name[0]) < 'A' || toupper(name[0]) > 'Z')
        return false;

    /* no trailing spaces */
    if (name[strlen(name)-1] == ' ')
        return false;

    /* must be A-Za-z 0-9 '.' ' ' */
    name++;
    while (*name != '\0') {
        if (!(  (toupper(*name) >= 'A' && toupper(*name) <= 'Z') ||
                (*name >= '0' && *name <= '9') ||
                (*name == '.') ||
                (*name == ' ')
             ))
        {
            return false;
        }

        name++;
    }

    return true;
}

/*
 * Generate lower case flags by comparing "upperName" to "lowerName".
 *
 * It's okay for "lowerName" to be longer than "upperName".  The extra chars
 * are just ignored.  Similarly, "lowerName" does not need to be
 * null-terminated.  "lowerName" does need to point to storage with at least
 * as many valid bytes as "upperName", though, or we could crash.
 *
 * Returns the mask to use in a ProDOS dir.  If "forAppleWorks" is set to
 * "true", the mask is modified for use with an AppleWorks aux type.
 */
/*static*/ uint16_t DiskFSProDOS::GenerateLowerCaseBits(const char* upperName,
    const char* lowerName, bool forAppleWorks)
{
    uint16_t caseMask = 0x8000;
    uint16_t caseBit = 0x8000;
    int len, i;
    char lowch;
    
    len = strlen(upperName);
    assert(len <= A2FileProDOS::kMaxFileName);

    for (i = 0; i < len; i++) {
        caseBit >>= 1;
        lowch = A2FileProDOS::NameToLower(upperName[i]);
        if (lowch == lowerName[i])
            caseMask |= caseBit;
    }

    if (forAppleWorks) {
        uint16_t adjusted;
        caseMask <<= 1;
        adjusted = caseMask << 8 | caseMask >> 8;
        return adjusted;
    } else {
        if (caseMask == 0x8000)
            return 0;   // all upper case, don't freak out pre-v1.8
        else
            return caseMask;
    }
}

/*
 * Generate the lower-case version of a ProDOS filename, using the supplied
 * lower case flags.  "lowerName" must be able to hold 15 chars (enough for
 * a filename or volname).
 *
 * The string will NOT be null-terminated, but the output buffer will be padded
 * with NULs out to the maximum filename len.  This makes it suitable for
 * copying directly into directory block buffers.
 *
 * It's okay to pass the same buffer for "upperName" and "lowerName".
 *
 * "lcFlags" is either ProDOS directory flags or AppleWorks aux type flags,
 * depending on the value of "fromAppleWorks".
 */
/*static*/ void DiskFSProDOS::GenerateLowerCaseName(const char* upperName,
    char* lowerName, uint16_t lcFlags, bool fromAppleWorks)
{
    int nameLen = strlen(upperName);
    int bit;
    assert(nameLen <= A2FileProDOS::kMaxFileName);

    if (fromAppleWorks) {
        /* handle AppleWorks lower-case-in-auxtype */
        uint16_t caseMask =   // swap bytes
            (lcFlags << 8) | (lcFlags >> 8);
        for (bit = 0; bit < nameLen ; bit++) {
            if ((caseMask & 0x8000) != 0)
                lowerName[bit] = A2FileProDOS::NameToLower(upperName[bit]);
            else
                lowerName[bit] = upperName[bit];
            caseMask <<= 1;
        }
        for ( ; bit < A2FileProDOS::kMaxFileName; bit++)
            lowerName[bit] = '\0';
    } else {
        /* handle lower-case conversion; see GS/OS tech note #8 */
        if (lcFlags != 0 && !(lcFlags & 0x8000)) {
            // Should be zero or 0x8000 plus other bits; shouldn't be
            //  bunch of bits without 0x8000 or 0x8000 by itself.  Not
            //  really a problem, just unexpected.
            assert(false);
            memcpy(lowerName, upperName, A2FileProDOS::kMaxFileName);
            return;
        }
        for (bit = 0; bit < nameLen; bit++) {
            lcFlags <<= 1;
            if ((lcFlags & 0x8000) != 0)
                lowerName[bit] = A2FileProDOS::NameToLower(upperName[bit]);
            else
                lowerName[bit] = upperName[bit];
        }
    }
    for ( ; bit < A2FileProDOS::kMaxFileName; bit++)
        lowerName[bit] = '\0';
}

/*
 * Normalize a ProDOS path.  Invokes DoNormalizePath and handles the buffer
 * management (if the normalized path doesn't fit in "*pNormalizedBufLen"
 * bytes, we set "*pNormalizedBufLen to the required length).
 *
 * This is invoked from the generalized "add" function in CiderPress, which
 * doesn't want to understand the ins and outs of ProDOS pathnames.
 */
DIError DiskFSProDOS::NormalizePath(const char* path, char fssep,
    char* normalizedBuf, int* pNormalizedBufLen)
{
    DIError dierr = kDIErrNone;
    char* normalizedPath = NULL;
    int len;

    assert(pNormalizedBufLen != NULL);
    assert(normalizedBuf != NULL || *pNormalizedBufLen == 0);

    dierr = DoNormalizePath(path, fssep, &normalizedPath);
    if (dierr != kDIErrNone)
        goto bail;

    assert(normalizedPath != NULL);
    len = strlen(normalizedPath);
    if (normalizedBuf == NULL || *pNormalizedBufLen <= len) {
        /* too short */
        dierr = kDIErrDataOverrun;
    } else {
        /* fits */
        strcpy(normalizedBuf, normalizedPath);
    }

    *pNormalizedBufLen = len+1;     // alloc room for the '\0'

bail:
    delete[] normalizedPath;
    return dierr;
}

/*
 * Normalize a ProDOS path.  This requires separating each path component
 * out, making it ProDOS-compliant, and then putting it back in.
 * The fssep could be anything, so we need to change it to kFssep.
 *
 * We don't try to identify duplicates here.  If more than one subdir maps
 * to the same thing, then you're just going to end up with lots of files
 * in the same subdir.  If this is unacceptable then it will have to be
 * fixed at a higher level.
 *
 * Lower-case letters and spaces are left in place.  They're expected to
 * be removed later.
 *
 * The caller must delete[] "*pNormalizedPath".
 */
DIError DiskFSProDOS::DoNormalizePath(const char* path, char fssep,
    char** pNormalizedPath)
{
    DIError dierr = kDIErrNone;
    char* workBuf = NULL;
    char* partBuf = NULL;
    char* outputBuf = NULL;
    char* start;
    char* end;
    char* outPtr;

    assert(path != NULL);
    workBuf = new char[strlen(path)+1];
    partBuf = new char[strlen(path)+1 +1];  // need +1 for prepending letter
    outputBuf = new char[strlen(path) * 2];
    if (workBuf == NULL || partBuf == NULL || outputBuf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    strcpy(workBuf, path);
    outputBuf[0] = '\0';

    outPtr = outputBuf;
    start = workBuf;
    while (*start != '\0') {
        //char* origStart = start;  // need for debug msg
        int partIdx;

        if (fssep == '\0') {
            end = NULL;
        } else {
            end = strchr(start, fssep);
            if (end != NULL)
                *end = '\0';
        }
        partIdx = 0;

        /*
         * Skip over everything up to the first letter.  If we encounter a
         * number or a '\0' first, insert a leading letter.
         */
        while (*start != '\0') {
            if (toupper(*start) >= 'A' && toupper(*start) <= 'Z') {
                partBuf[partIdx++] = *start++;
                break;
            }
            if (*start >= '0' && *start <= '9') {
                partBuf[partIdx++] = 'A';
                break;
            }

            start++;
        }
        if (partIdx == 0)
            partBuf[partIdx++] = 'Z';

        /*
         * Continue copying, dropping all illegal chars.
         */
        while (*start != '\0') {
            if ((toupper(*start) >= 'A' && toupper(*start) <= 'Z') ||
                (*start >= '0' && *start <= '9') ||
                (*start == '.') ||
                (*start == ' ') )
            {
                partBuf[partIdx++] = *start++;
            } else {
                start++;
            }
        }

        /*
         * Truncate at 15 chars, preserving anything that looks like a
         * filename extension.  "partIdx" represents the length of the
         * string at this point.  "partBuf" holds the string, which we
         * want to null-terminate before proceeding.
         */
        partBuf[partIdx] = '\0';
        if (partIdx > A2FileProDOS::kMaxFileName) {
            const char* pDot = strrchr(partBuf, '.');
            //int DEBUGDOTLEN = pDot - partBuf;
            if (pDot != NULL && partIdx - (pDot-partBuf) <= kMaxExtensionLen) {
                int dotLen = partIdx - (pDot-partBuf);
                memmove(partBuf + (A2FileProDOS::kMaxFileName - dotLen),
                    pDot, dotLen);      // don't use memcpy, move might overlap
            }
            partIdx = A2FileProDOS::kMaxFileName;
        }
        partBuf[partIdx] = '\0';

        //LOGI(" ProDOS   Converted component '%s' to '%s'",
        //  origStart, partBuf);

        if (outPtr != outputBuf)
            *outPtr++ = A2FileProDOS::kFssep;
        strcpy(outPtr, partBuf);
        outPtr += partIdx;

        /*
         * Continue with next segment.
         */
        if (end == NULL)
            break;
        start = end+1;
    }

    *outPtr = '\0';

    LOGI(" ProDOS  Converted path '%s' to '%s' (fssep='%c')",
        path, outputBuf, fssep);
    assert(*outputBuf != '\0');

    *pNormalizedPath = outputBuf;
    outputBuf = NULL;

bail:
    delete[] workBuf;
    delete[] partBuf;
    delete[] outputBuf;
    return dierr;
}

/*
 * Create a copy of the filename with everything in upper case and spaces
 * changed to periods.
 *
 * "upperName" must be a buffer that holds at least kMaxFileName+1 characters.
 * If "name" is longer than kMaxFileName, it will be truncated.
 */
void DiskFSProDOS::UpperCaseName(char* upperName, const char* name)
{
    int i;

    for (i = 0; i < A2FileProDOS::kMaxFileName; i++) {
        char ch = name[i];
        if (ch == '\0')
            break;
        else if (ch == ' ')
            upperName[i] = '.';
        else
            upperName[i] = toupper(ch);
    }

    /* null terminate with prejudice -- we memcpy this buffer into subdirs */
    for ( ; i <= A2FileProDOS::kMaxFileName; i++)
        upperName[i] = '\0';
}

/*
 * Allocate a new directory entry.  We start by reading the entire thing
 * into memory.  If the current set of allocated directory blocks is full,
 * and we're not operating on the volume dir, we extend the directory.
 *
 * This just allocates the space; it does not fill in any details, except
 * for the prev/next block pointers and the file count in the header.  (One
 * small exception: if we have to extend the directory, the "prev/next" fields
 * of the new block will be filled in.)
 *
 * The volume in-use block map must be loaded before this is called.  If
 * this needs to extend the directory, a new block will be allocated.
 *
 * Returns a pointer to the new entry, and a whole bunch of other stuff:
 *  "ppDir" gets a pointer to newly-allocated memory with the whole directory
 *  "pDirLen" is the size of the *ppDir buffer
 *  "ppDirEntry" gets a memory pointer to the start of the created entry
 *  "pDirKeyBlock" gets the key block of the directory as a whole
 *  "pDirEntrySlot" gets the slot number within the directory block (first is 1)
 *  "pDirBlock" gets the actual block in which the created entry resides
 *
 * The caller should Write the entire thing to "pOpenSubdir" after filling
 * in the new details for the entry.
 *
 * Possible reasons for failure: disk is out of space, volume dir is out
 * of space, pOpenSubdir is screwy.
 *
 * We guarantee that we will return the first available entry in the current
 * directory.
 */
DIError DiskFSProDOS::AllocDirEntry(A2FileDescr* pOpenSubdir, uint8_t** ppDir,
    long* pDirLen, uint8_t** ppDirEntry, uint16_t* pDirKeyBlock,
    int* pDirEntrySlot, uint16_t* pDirBlock)
{
    assert(pOpenSubdir != NULL);
    *ppDirEntry = NULL;
    *pDirLen = -1;
    *pDirKeyBlock = 0;
    *pDirEntrySlot = -1;
    *pDirBlock = 0;

    DIError dierr = kDIErrNone;
    uint8_t* dirBuf = NULL;
    long dirLen;
    A2FileProDOS* pFile;
    long newBlock = -1;

    /*
     * Load the subdir into memory.
     */
    pFile = (A2FileProDOS*) pOpenSubdir->GetFile();
    dirLen = (long) pFile->GetDataLength();
    if (dirLen < 512 || (dirLen % 512) != 0) {
        LOGI(" ProDOS GLITCH: funky dir EOF %ld (quality=%d)",
            dirLen, pFile->GetQuality());
        dierr = kDIErrBadFile;
        goto bail;
    }
    dirBuf = new uint8_t[dirLen];
    if (dirBuf == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    dierr = pOpenSubdir->Read(dirBuf, dirLen);
    if (dierr != kDIErrNone)
        goto bail;

    if (dirBuf[0x23] != kEntryLength ||
        dirBuf[0x24] != kEntriesPerBlock)
    {
        LOGI(" ProDOS GLITCH: funky entries per block %d", dirBuf[0x24]);
        dierr = kDIErrBadDirectory;
        goto bail;
    }

    /*
     * Find the first available entry (storage_type is zero).  We need to
     * step through this by blocks, because the data is block-oriented.
     * If we run off the end of the last block, (re)alloc a new one.
     */
    uint8_t* pDirEntry;
    int blockIdx;
    int entryIdx;

    pDirEntry = NULL;    // make the compiler happy
    entryIdx = -1;      // make the compiler happy

    for (blockIdx = 0; blockIdx < dirLen / 512; blockIdx++) {
        pDirEntry = dirBuf + 512*blockIdx + 4;  // skip 4 bytes of prev/next

        for (entryIdx = 0; entryIdx < kEntriesPerBlock;
                                        entryIdx++, pDirEntry += kEntryLength)
        {
            if ((pDirEntry[0x00] & 0xf0) == 0) {
                LOGI(" ProDOS  Found empty dir entry in slot %d", entryIdx);
                break;      // found one; break out of inner loop
            }
        }
        if (entryIdx < kEntriesPerBlock)
            break;      // out of outer loop
    }
    if (blockIdx == dirLen / 512) {
        if (((dirBuf[0x04] & 0xf0) >> 4) == A2FileProDOS::kStorageVolumeDirHeader)
        {
            /* can't extend the volume dir */
            dierr = kDIErrVolumeDirFull;
            goto bail;
        }

        LOGI(" ProDOS ran out of directory space, adding another block");

        /*
         * Request an unused block from the system.  Point the "next" pointer
         * in the last block at it, so that when we go to write this dir
         * we will know where to put it.
         */
        uint8_t* pBlock;
        pBlock = dirBuf + 512 * (blockIdx-1);
        if (pBlock[0x02] != 0) {
            LOGI(" ProDOS GLITCH: adding to block with nonzero next ptr!");
            dierr = kDIErrBadDirectory;
            goto bail;
        }

        newBlock = AllocBlock();
        if (newBlock < 0) {
            dierr = kDIErrDiskFull;
            goto bail;
        }

        PutShortLE(&pBlock[0x02], (uint16_t) newBlock);   // set "next"

        /*
         * Extend our memory buffer to hold the new entry.
         */
        uint8_t* newSpace = new uint8_t[dirLen + 512];
        if (newSpace == NULL) {
            dierr = kDIErrMalloc;
            goto bail;
        }
        memcpy(newSpace, dirBuf, dirLen);
        memset(newSpace + dirLen, 0, 512);
        delete[] dirBuf;
        dirBuf = newSpace;
        dirLen += 512;

        /*
         * Set the "prev" pointer in the new block to point at the last
         * block of the existing directory structure.
         */
        long lastBlock;
        dierr = pOpenSubdir->GetStorage(blockIdx-1, &lastBlock);
        if (dierr != kDIErrNone)
            goto bail;
        pBlock = dirBuf + 512 * blockIdx;
        PutShortLE(&pBlock[0x00], (uint16_t) lastBlock);    // set "prev"
        assert(GetShortLE(&pBlock[0x02]) == 0);             // "next" pointer

        /*
         * Finally, point pDirEntry at the first entry in the new area.
         */
        pDirEntry = pBlock + 4;
        entryIdx = 0;
        assert(pDirEntry[0x00] == 0x00);
    }

    /*
     * Success.  Update the file count in the header.
     */
    uint16_t count;
    count = GetShortLE(&dirBuf[0x25]);
    count++;
    PutShortLE(&dirBuf[0x25], count);

    long whichBlock;

    *ppDir = dirBuf;
    *pDirLen = dirLen;
    *ppDirEntry = pDirEntry;
    *pDirKeyBlock = pFile->fDirEntry.keyPointer;
    *pDirEntrySlot = entryIdx +1;
    if (blockIdx == ((A2FDProDOS*)pOpenSubdir)->GetBlockCount()) {
        /* not yet added to block list, so can't use GetStorage */
        assert(newBlock > 0);
        *pDirBlock = (uint16_t) newBlock;
    } else {
        assert(newBlock < 0);
        dierr = pOpenSubdir->GetStorage(blockIdx, &whichBlock);
        assert(dierr == kDIErrNone);
        *pDirBlock = (uint16_t) whichBlock;
    }
    dirBuf = NULL;

bail:
    delete[] dirBuf;
    return dierr;
}

/*
 * Given a pointer to a directory buffer and a pointer to an entry, find the
 * previous entry.  (This is handy when trying to figure out where to insert
 * a new entry into the DiskFS linear file list.)
 *
 * If the previous entry is the first in the list (i.e. it's a volume or
 * subdir header), this returns NULL.
 *
 * This is a little awkward because the directories are chopped up into
 * 512-byte blocks, with 13 entries per block (which doesn't completely fill
 * the block, leaving gaps we have to skip around).  If the previous entry is
 * in the same block we can just return (ptr-0x27), but if it's in a previous
 * block we need to return the last entry in the previous.
 */
uint8_t* DiskFSProDOS::GetPrevDirEntry(uint8_t* buf, uint8_t* ptr)
{
    assert(buf != NULL);
    assert(ptr != NULL);

    const int kStartOffset = 4;

    if (ptr == buf + kStartOffset || ptr == buf + kStartOffset + kEntryLength)
        return NULL;

    while (ptr - buf > 512)
        buf += 512;

    assert((ptr - buf - kStartOffset) % kEntryLength == 0);

    if (ptr == buf + kStartOffset) {
        /* whoops, went too far */
        buf -= 512;
        return buf + kStartOffset + kEntryLength * (kEntriesPerBlock-1);
    } else {
        return ptr - kEntryLength;
    }
}

/*
 * Make the name pointed to by "fileName" unique within the directory
 * loaded in "subdirBuf".  The name should already be trimmed to 15 chars
 * or less and converted to upper-case only, and be in a buffer that can
 * hold at least kMaxFileName+1 bytes.
 *
 * Returns an error on failure, which should only happen if there are a
 * large number of files with similar names.
 */
DIError DiskFSProDOS::MakeFileNameUnique(const uint8_t* dirBuf, long dirLen,
    char* fileName)
{
    assert(dirBuf != NULL);
    assert(dirLen > 0);
    assert((dirLen % 512) == 0);
    assert(fileName != NULL);
    assert(strlen(fileName) <= A2FileProDOS::kMaxFileName);

    if (!NameExistsInDir(dirBuf, dirLen, fileName))
        return kDIErrNone;

    LOGI(" ProDOS   found duplicate of '%s', making unique", fileName);

    int nameLen = strlen(fileName);
    int dotOffset=0, dotLen=0;
    char dotBuf[kMaxExtensionLen+1];

    /* ensure the result will be null-terminated */
    memset(fileName + nameLen, 0, (A2FileProDOS::kMaxFileName - nameLen) +1);

    /*
     * If this has what looks like a filename extension, grab it.  We want
     * to preserve ".gif", ".c", etc., since the filetypes don't necessarily
     * do everything we need.
     *
     * This will tend to screw up the upper/lower case stuff, especially
     * since what we think is a '.' might actually be a ' '.  We could work
     * around this, but it's probably not necessary.
     */
    const char* cp = strrchr(fileName, '.');
    if (cp != NULL) {
        int tmpOffset = cp - fileName;
        if (tmpOffset > 0 && nameLen - tmpOffset <= kMaxExtensionLen) {
            LOGI("  ProDOS   (keeping extension '%s')", cp);
            assert(strlen(cp) <= kMaxExtensionLen);
            strcpy(dotBuf, cp);
            dotOffset = tmpOffset;
            dotLen = nameLen - dotOffset;
        }
    }

    const int kMaxDigits = 999;
    int digits = 0;
    int digitLen;
    int copyOffset;
    char digitBuf[4];
    do {
        if (digits == kMaxDigits)
            return kDIErrFileExists;
        digits++;

        /* not the most efficient way to do this, but it'll do */
        sprintf(digitBuf, "%d", digits);
        digitLen = strlen(digitBuf);
        if (nameLen + digitLen > A2FileProDOS::kMaxFileName)
            copyOffset = A2FileProDOS::kMaxFileName - dotLen - digitLen;
        else
            copyOffset = nameLen - dotLen;
        memcpy(fileName + copyOffset, digitBuf, digitLen);
        if (dotLen != 0)
            memcpy(fileName + copyOffset + digitLen, dotBuf, dotLen);
    } while (NameExistsInDir(dirBuf, dirLen, fileName));

    LOGI(" ProDOS  converted to unique name: %s", fileName);

    return kDIErrNone;
}

/*
 * Determine whether the specified file name exists in the raw directory
 * buffer.
 *
 * This should be called with the upper-case-only version of the filename.
 */
bool DiskFSProDOS::NameExistsInDir(const uint8_t* dirBuf, long dirLen,
    const char* fileName)
{
    const uint8_t* pDirEntry;
    int blockIdx;
    int entryIdx;
    int nameLen = strlen(fileName);

    assert(nameLen <= A2FileProDOS::kMaxFileName);

    for (blockIdx = 0; blockIdx < dirLen / 512; blockIdx++) {
        pDirEntry = dirBuf + 512*blockIdx + 4;      // skip 4 bytes of prev/next

        for (entryIdx = 0; entryIdx < kEntriesPerBlock;
                                        entryIdx++, pDirEntry += kEntryLength)
        {
            /* skip directory header */
            if (blockIdx == 0 && entryIdx == 0)
                continue;

            if ((pDirEntry[0x00] & 0xf0) != 0 &&
                (pDirEntry[0x00] & 0x0f) == nameLen &&
                strncmp((char*) &pDirEntry[0x01], fileName, nameLen) == 0)
            {
                return true;
            }
        }
    }

    return false;
}

/*
 * Delete a file.
 *
 * There are three fairly simple steps: (1) mark all blocks used by the file as
 * free, (2) set the storage type in the directory entry to 0, and (3)
 * decrement the file count in the directory header.  We then remove it from
 * the DiskFS file list.
 *
 * We only allow deletion of a subdirectory when the subdir is empty.
 */
DIError DiskFSProDOS::DeleteFile(A2File* pGenericFile)
{
    DIError dierr = kDIErrNone;
    long blockCount = -1;
    long indexCount = -1;
    uint16_t* blockList = NULL;
    uint16_t* indexList = NULL;

    if (pGenericFile == NULL) {
        assert(false);
        return kDIErrInvalidArg;
    }

    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;
    if (!fDiskIsGood)
        return kDIErrBadDiskImage;
    if (pGenericFile->IsFileOpen())
        return kDIErrFileOpen;

    /*
     * If they try to delete all entries, we don't want to spit back a
     * failure message over our "fake" volume dir entry.  So we just silently
     * ignore the request.
     */
    if (pGenericFile->IsVolumeDirectory()) {
        LOGI("ProDOS not deleting volume directory");
        return kDIErrNone;
    }

    A2FileProDOS* pFile = (A2FileProDOS*) pGenericFile;

    LOGI("    Deleting '%s'", pFile->GetPathName());

    dierr = LoadVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;
    switch (pFile->fDirEntry.storageType) {
    case A2FileProDOS::kStorageExtended:
        // handle rsrc fork here, fall out for data fork
        dierr = pFile->LoadBlockList(
                    pFile->fExtRsrc.storageType,
                    pFile->fExtRsrc.keyBlock,
                    pFile->fExtRsrc.eof,
                    &blockCount, &blockList,
                    &indexCount, &indexList);
        if (dierr != kDIErrNone)
            goto bail;
        FreeBlocks(blockCount, blockList);
        if (indexList != NULL)   // no indices for seedling
            FreeBlocks(indexCount, indexList);
        delete[] blockList;
        delete[] indexList;
        indexList = NULL;

        // handle the key block "manually"
        blockCount = 1;
        blockList = new uint16_t[blockCount];
        blockList[0] = pFile->fDirEntry.keyPointer;
        FreeBlocks(blockCount, blockList);
        delete[] blockList;
        blockList = NULL;

        dierr = pFile->LoadBlockList(
                    pFile->fExtData.storageType,
                    pFile->fExtData.keyBlock,
                    pFile->fExtData.eof,
                    &blockCount, &blockList,
                    &indexCount, &indexList);
        break;  // fall out

    case A2FileProDOS::kStorageDirectory:
        dierr = pFile->LoadDirectoryBlockList(
                    pFile->fDirEntry.keyPointer,
                    pFile->fDirEntry.eof,
                    &blockCount, &blockList);
        break;  // fall out

    case A2FileProDOS::kStorageSeedling:
    case A2FileProDOS::kStorageSapling:
    case A2FileProDOS::kStorageTree:
        dierr = pFile->LoadBlockList(
                    pFile->fDirEntry.storageType,
                    pFile->fDirEntry.keyPointer,
                    pFile->fDirEntry.eof,
                    &blockCount, &blockList,
                    &indexCount, &indexList);
        break;  // fall out

    default:
        LOGI("ProDOS can't delete unknown storage type %d",
            pFile->fDirEntry.storageType);
        dierr = kDIErrBadDirectory;
        break;  // fall out
    }

    if (dierr != kDIErrNone)
        goto bail;

    FreeBlocks(blockCount, blockList);
    if (indexList != NULL)
        FreeBlocks(indexCount, indexList);

    /*
     * Update the directory entry.  After this point, failure gets ugly.
     *
     * It might be "proper" to open the subdir file, find the correct entry,
     * and write it back, but the A2FileProDOS structure has the directory
     * block and entry index stored in it.  Makes it a little easier.
     */
    uint8_t blkBuf[kBlkSize];
    uint8_t* ptr;
    assert(pFile->fParentDirBlock > 0);
    assert(pFile->fParentDirIdx >= 0 &&
           pFile->fParentDirIdx < kEntriesPerBlock);
    dierr = fpImg->ReadBlock(pFile->fParentDirBlock, blkBuf);
    if (dierr != kDIErrNone) {
        LOGI("ProDOS unable to read directory block %u",
            pFile->fParentDirBlock);
        goto bail;
    }

    ptr = blkBuf + 4 + pFile->fParentDirIdx * kEntryLength;
    if ((*ptr) >> 4 != pFile->fDirEntry.storageType) {
        LOGI("ProDOS GLITCH: mismatched storage types (%d vs %d)",
            (*ptr) >> 4, pFile->fDirEntry.storageType);
        assert(false);
        dierr = kDIErrBadDirectory;
        goto bail;
    }
    ptr[0x00] = 0;      // zap both storage type and name length
    dierr = fpImg->WriteBlock(pFile->fParentDirBlock, blkBuf);
    if (dierr != kDIErrNone) {
        LOGI("ProDOS unable to write directory block %u",
            pFile->fParentDirBlock);
        goto bail;
    }

    /*
     * Save our updated copy of the volume bitmap to disk.
     */
    dierr = SaveVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * One last little thing: decrement the file count in the directory
     * header.  We can find the appropriate place pretty easily because
     * we know it's the first block in pFile->fpParent, which for a dir is
     * always the block pointed to by the key pointer.
     *
     * Strictly speaking, failure to update this correctly isn't fatal.  I
     * doubt most utilities pay any attention to this.  Still, it's important
     * to keep the filesystem in a consistent state, so we at least must
     * report the error.  They'll need to run the ProSel volume repair util
     * to fix it.
     */
    A2FileProDOS* pParent;
    uint16_t fileCount;
    int storageType;
    pParent = (A2FileProDOS*) pFile->GetParent();
    assert(pParent != NULL);
    assert(pParent->fDirEntry.keyPointer >= kVolHeaderBlock);
    dierr = fpImg->ReadBlock(pParent->fDirEntry.keyPointer, blkBuf);
    if (dierr != kDIErrNone) {
        LOGI("ProDOS unable to read parent dir block %u",
            pParent->fDirEntry.keyPointer);
        goto bail;
    }
    ptr = NULL;

    storageType = (blkBuf[0x04] & 0xf0) >> 4;
    if (storageType != A2FileProDOS::kStorageSubdirHeader &&
        storageType != A2FileProDOS::kStorageVolumeDirHeader)
    {
        LOGI("ProDOS invalid storage type %d in dir header block",
            storageType);
        DebugBreak();
        dierr = kDIErrBadDirectory;
        goto bail;
    }
    fileCount = GetShortLE(&blkBuf[0x25]);
    if (fileCount > 0)
        fileCount--;
    PutShortLE(&blkBuf[0x25], fileCount);
    dierr = fpImg->WriteBlock(pParent->fDirEntry.keyPointer, blkBuf);
    if (dierr != kDIErrNone) {
        LOGI("ProDOS unable to write parent dir block %u",
            pParent->fDirEntry.keyPointer);
        goto bail;
    }

    /*
     * Remove the A2File* from the list.
     */
    DeleteFileFromList(pFile);

bail:
    FreeVolBitmap();
    delete[] blockList;
    delete[] indexList;
    return kDIErrNone;
}

/*
 * Mark all of the blocks in the blockList as free.
 *
 * The in-use map must already be loaded.
 */
DIError DiskFSProDOS::FreeBlocks(long blockCount, uint16_t* blockList)
{
    VolumeUsage::ChunkState cstate;
    int i;

    //LOGI(" +++ FreeBlocks (blockCount=%d blockList=0x%08lx)",
    //  blockCount, blockList);
    assert(blockCount >= 0 && blockCount < 65536);
    assert(blockList != NULL);

    cstate.isUsed = false;
    cstate.isMarkedUsed = false;
    cstate.purpose = VolumeUsage::kChunkPurposeUnknown;

    for (i = 0; i < blockCount; i++) {
        if (blockList[i] == 0)  // expected for "sparse" files
            continue;

        if (!GetBlockUseEntry(blockList[i])) {
            LOGI("WARNING: freeing unallocated block %u", blockList[i]);
            assert(false);  // impossible unless disk is "damaged"
        }
        SetBlockUseEntry(blockList[i], false);

        fVolumeUsage.SetChunkState(blockList[i], &cstate);
    }

    return kDIErrNone;
}

/*
 * Rename a file.
 *
 * Pass in a pointer to the file and a string with the new filename (just
 * the filename, not a pathname -- this function doesn't move files
 * between directories).  The new name must already be normalized.
 *
 * Renaming the magic volume directory "file" is not allowed.
 *
 * Things to note:
 *  - Renaming subdirs is annoying.  The name has to be changed in two
 *    places, and the "pathname" value cached in A2FileProDOS must be
 *    updated for all children of the subdir.
 *  - Must check for duplicates.
 *  - If it's an AppleWorks file type, we need to change the aux type
 *    according to the upper/lower case flags.  This holds even if the
 *    "allow lower case" flag is disabled.
 */
DIError DiskFSProDOS::RenameFile(A2File* pGenericFile, const char* newName)
{
    DIError dierr = kDIErrNone;
    A2FileProDOS* pFile = (A2FileProDOS*) pGenericFile;
    char upperName[A2FileProDOS::kMaxFileName+1];
    char upperComp[A2FileProDOS::kMaxFileName+1];

    if (pFile == NULL || newName == NULL)
        return kDIErrInvalidArg;
    if (!IsValidFileName(newName))
        return kDIErrInvalidArg;
    if (pFile->IsVolumeDirectory())
        return kDIErrInvalidArg;
    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;
    if (!fDiskIsGood)
        return kDIErrBadDiskImage;

    LOGI(" ProDOS renaming '%s' to '%s'", pFile->GetPathName(), newName);

    /*
     * Check for duplicates.  We do this by getting the parent subdir and
     * running through it looking for an upper-case-converted match.
     *
     * We start in the list at our parent node, knowing that the kids are
     * grouped together after it.  However, we can't stop right away,
     * because some of the kids might be subdirectories themselves.  So we
     * will probably run through a significant chunk of the list.
     */
    A2File* pParent = pFile->GetParent();
    A2File* pCur;

    UpperCaseName(upperName, newName);
    pCur = GetNextFile(pParent);
    assert(pCur != NULL);    // at the very least, pFile is in this dir
    while (pCur != NULL) {
        if (pCur != pFile && pCur->GetParent() == pParent) {
            /* one of our siblings; see if the name matches */
            UpperCaseName(upperComp, pCur->GetFileName());
            if (strcmp(upperName, upperComp) == 0) {
                LOGI(" ProDOS rename dup found");
                return kDIErrFileExists;
            }
        }

        pCur = GetNextFile(pCur);
    }

    /*
     * Grab the directory block and update the filename in the entry.  If this
     * was a subdir we also need to update its directory header entry.  To
     * minimize the chances of a partial update, we load both blocks up
     * front, modify both, then write them both back.
     */
    uint8_t parentDirBuf[kBlkSize];
    uint8_t thisDirBuf[kBlkSize];

    dierr = fpImg->ReadBlock(pFile->fParentDirBlock, parentDirBuf);
    if (dierr != kDIErrNone)
        goto bail;
    if (pFile->IsDirectory()) {
        dierr = fpImg->ReadBlock(pFile->fDirEntry.keyPointer, thisDirBuf);
        if (dierr != kDIErrNone)
            goto bail;
    }

    /* compute lower case flags as needed */
    uint16_t lcFlags, lcAuxType;
    bool allowLowerCase, isAW;

    allowLowerCase = GetParameter(kParmProDOS_AllowLowerCase) != 0;
    isAW = A2FileProDOS::UsesAppleWorksAuxType((uint8_t)pFile->GetFileType());

    if (allowLowerCase)
        lcFlags = GenerateLowerCaseBits(upperName, newName, false);
    else
        lcFlags = 0;
    if (isAW)
        lcAuxType = GenerateLowerCaseBits(upperName, newName, true);
    else
        lcAuxType = 0;

    /*
     * Possible optimization: if "upperName" matches what's in the block on
     * disk and the "lcFlags"/"lcAuxType" values match as well, we don't
     * need to write the blocks back.
     *
     * It's difficult to test for this earlier, because we need to do the
     * update if (a) they're just changing the capitalization or (b) we're
     * changing the capitalization for them because the "allow lower case"
     * flag got turned off.
     */

    /* find the right entry, and copy our filename in */
    uint8_t* ptr;
    assert(pFile->fParentDirIdx >= 0 &&
           pFile->fParentDirIdx < kEntriesPerBlock);
    ptr = parentDirBuf + 4 + pFile->fParentDirIdx * kEntryLength;
    if ((*ptr) >> 4 != pFile->fDirEntry.storageType) {
        LOGI("ProDOS GLITCH: mismatched storage types (%d vs %d)",
            (*ptr) >> 4, pFile->fDirEntry.storageType);
        assert(false);
        dierr = kDIErrBadDirectory;
        goto bail;
    }
    ptr[0x00] = (uint8_t)((ptr[0x00] & 0xf0) | strlen(upperName));
    memcpy(&ptr[0x01], upperName, A2FileProDOS::kMaxFileName);
    PutShortLE(&ptr[0x1c], lcFlags);        // version/min_version
    if (isAW)
        PutShortLE(&ptr[0x1f], lcAuxType);

    if (pFile->IsDirectory()) {
        ptr = thisDirBuf + 4;
        if ((*ptr) >> 4 != A2FileProDOS::kStorageSubdirHeader) {
            LOGI("ProDOS GLITCH: bad storage type in subdir header (%d)",
                (*ptr) >> 4);
            assert(false);
            dierr = kDIErrBadDirectory;
            goto bail;
        }
        ptr[0x00] = (uint8_t)((ptr[0x00] & 0xf0) | strlen(upperName));
        memcpy(&ptr[0x01], upperName, A2FileProDOS::kMaxFileName);
        PutShortLE(&ptr[0x1c], lcFlags);        // version/min_version
    }

    /* write the updated data back to the disk */
    dierr = fpImg->WriteBlock(pFile->fParentDirBlock, parentDirBuf);
    if (dierr != kDIErrNone)
        goto bail;
    if (pFile->IsDirectory()) {
        dierr = fpImg->WriteBlock(pFile->fDirEntry.keyPointer, thisDirBuf);
        if (dierr != kDIErrNone)
            goto bail;
    }

    /*
     * At this point the ProDOS filesystem is back in a consistent state.
     * Everything we do from here on is self-inflicted.
     *
     * We need to update this entry's A2FileProDOS::fDirEntry.fileName,
     * as well as the A2FileProDOS::fPathName.  If this was a subdir, then
     * we need to update A2FileProDOS::fPathName for all files inside the
     * directory (including children of children).
     *
     * The latter is somewhat awkward, so we just re-acquire the pathname
     * for every file on the disk.  Less efficient but easier to code.
     */
    if (isAW)
        GenerateLowerCaseName(upperName, pFile->fDirEntry.fileName,
            lcAuxType, true);
    else
        GenerateLowerCaseName(upperName, pFile->fDirEntry.fileName,
            lcFlags, false);
    assert(pFile->fDirEntry.fileName[A2FileProDOS::kMaxFileName] == '\0');

    if (pFile->IsDirectory()) {
        /* do all files that come after us */
        pCur = pFile;
        while (pCur != NULL) {
            RegeneratePathName((A2FileProDOS*) pCur);
            pCur = GetNextFile(pCur);
        }
    } else {
        RegeneratePathName(pFile);
    }

    LOGI("Okay!");

bail:
    return dierr;
}

/*
 * Regenerate fPathName for the specified file.
 *
 * Has no effect on the magic volume dir entry.
 *
 * This could be implemented more efficiently, but it's only used when
 * renaming files, so there's not much point.
 */
DIError DiskFSProDOS::RegeneratePathName(A2FileProDOS* pFile)
{
    A2FileProDOS* pParent;
    char* buf = NULL;
    int len;

    /* nothing to do here */
    if (pFile->IsVolumeDirectory())
        return kDIErrNone;

    /* compute the length of the path name */
    len = strlen(pFile->GetFileName());
    pParent = (A2FileProDOS*) pFile->GetParent();
    while (!pParent->IsVolumeDirectory()) {
        len++;      // leave space for the ':'
        len += strlen(pParent->GetFileName());

        pParent = (A2FileProDOS*) pParent->GetParent();
    }

    buf = new char[len+1];
    if (buf == NULL)
        return kDIErrMalloc;

    /* generate the new path name */
    int partLen;
    partLen = strlen(pFile->GetFileName());
    strcpy(buf + len - partLen, pFile->GetFileName());
    len -= partLen;

    pParent = (A2FileProDOS*) pFile->GetParent();
    while (!pParent->IsVolumeDirectory()) {
        assert(len > 0);
        buf[--len] = kDIFssep;

        partLen = strlen(pParent->GetFileName());
        strncpy(buf + len - partLen, pParent->GetFileName(), partLen);
        len -= partLen;
        assert(len >= 0);

        pParent = (A2FileProDOS*) pParent->GetParent();
    }

    LOGI("Replacing '%s' with '%s'", pFile->GetPathName(), buf);
    pFile->SetPathName("", buf);
    delete[] buf;

    return kDIErrNone;
}

/*
 * Change the attributes of the specified file.
 *
 * Subdirectories have access bits in the subdir header as well as their
 * file entry.  The BASIC.SYSTEM "lock" command only changes the access
 * bits of the file; the permissions inside the subdir remain 0xe3.  (Which
 * might explain why you can still add files to a locked subdir.)  I'm going
 * to mimic this behavior.
 *
 * This does, of course, mean that there's no meaning in attempts to change
 * the file access permissions of the volume directory.
 */
DIError DiskFSProDOS::SetFileInfo(A2File* pGenericFile, uint32_t fileType,
    uint32_t auxType, uint32_t accessFlags)
{
    DIError dierr = kDIErrNone;
    A2FileProDOS* pFile = (A2FileProDOS*) pGenericFile;

    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;
    if (pFile == NULL) {
        assert(false);
        return kDIErrInvalidArg;
    }
    if ((fileType & ~(0xff)) != 0 ||
        (auxType & ~(0xffff)) != 0 ||
        (accessFlags & ~(0xff)) != 0)
    {
        return kDIErrInvalidArg;
    }
    if (pFile->IsVolumeDirectory()) {
        LOGI(" ProDOS refusing to change file info for volume dir");
        return kDIErrAccessDenied;      // not quite right
    }

    LOGI("ProDOS changing values for '%s' to 0x%02x 0x%04x 0x%02x",
        pFile->GetPathName(), fileType, auxType, accessFlags);

    /* load the directory block for this file */
    uint8_t thisDirBuf[kBlkSize];
    dierr = fpImg->ReadBlock(pFile->fParentDirBlock, thisDirBuf);
    if (dierr != kDIErrNone)
        goto bail;

    /* find the right entry, and set the fields */
    uint8_t* ptr;
    assert(pFile->fParentDirIdx >= 0 &&
           pFile->fParentDirIdx < kEntriesPerBlock);
    ptr = thisDirBuf + 4 + pFile->fParentDirIdx * kEntryLength;
    if ((*ptr) >> 4 != pFile->fDirEntry.storageType) {
        LOGI("ProDOS GLITCH: mismatched storage types (%d vs %d)",
            (*ptr) >> 4, pFile->fDirEntry.storageType);
        assert(false);
        dierr = kDIErrBadDirectory;
        goto bail;
    }
    if ((size_t) (*ptr & 0x0f) != strlen(pFile->fDirEntry.fileName)) {
        LOGW("ProDOS GLITCH: wrong file?  (len=%d vs %u)",
            *ptr & 0x0f, (unsigned int) strlen(pFile->fDirEntry.fileName));
        assert(false);
        dierr = kDIErrBadDirectory;
        goto bail;
    }

    ptr[0x10] = (uint8_t) fileType;
    ptr[0x1e] = (uint8_t) accessFlags;
    PutShortLE(&ptr[0x1f], (uint16_t) auxType);

    dierr = fpImg->WriteBlock(pFile->fParentDirBlock, thisDirBuf);
    if (dierr != kDIErrNone)
        goto bail;

    /* update our local copy */
    pFile->fDirEntry.fileType = (uint8_t) fileType;
    pFile->fDirEntry.auxType = (uint16_t) auxType;
    pFile->fDirEntry.access = (uint8_t) accessFlags;

bail:
    return dierr;
}

/*
 * Change the disk volume name.
 *
 * This is a lot like renaming a subdirectory, except that there's no parent
 * directory to update, and the name of the volume dir doesn't affect the
 * pathname of anything else.  There's also no risk of a duplicate.
 *
 * Internally we need to update the "fake" entry and the cached copies in
 * fVolumeName and fVolumeID.
 */
DIError DiskFSProDOS::RenameVolume(const char* newName)
{
    DIError dierr = kDIErrNone;
    char upperName[A2FileProDOS::kMaxFileName+1];
    A2FileProDOS* pFile;

    if (!IsValidVolumeName(newName))
        return kDIErrInvalidArg;
    if (fpImg->GetReadOnly())
        return kDIErrAccessDenied;

    pFile = (A2FileProDOS*) GetNextFile(NULL);
    assert(pFile != NULL);
    assert(strcmp(pFile->GetFileName(), fVolumeName) == 0);

    LOGI(" ProDOS renaming volume '%s' to '%s'",
        pFile->GetPathName(), newName);

    /*
     * Figure out the lower-case flags.
     */
    uint16_t lcFlags;
    bool allowLowerCase;

    UpperCaseName(upperName, newName);
    allowLowerCase = GetParameter(kParmProDOS_AllowLowerCase) != 0;
    if (allowLowerCase)
        lcFlags = GenerateLowerCaseBits(upperName, newName, false);
    else
        lcFlags = 0;

    /*
     * Update the volume dir header.
     */
    uint8_t thisDirBuf[kBlkSize];
    uint8_t* ptr;
    assert(pFile->fDirEntry.keyPointer == kVolHeaderBlock);

    dierr = fpImg->ReadBlock(pFile->fDirEntry.keyPointer, thisDirBuf);
    if (dierr != kDIErrNone)
        goto bail;

    ptr = thisDirBuf + 4;
    if ((*ptr) >> 4 != A2FileProDOS::kStorageVolumeDirHeader) {
        LOGI("ProDOS GLITCH: bad storage type in voldir header (%d)",
            (*ptr) >> 4);
        assert(false);
        dierr = kDIErrBadDirectory;
        goto bail;
    }
    ptr[0x00] = (uint8_t)((ptr[0x00] & 0xf0) | strlen(upperName));
    memcpy(&ptr[0x01], upperName, A2FileProDOS::kMaxFileName);
    PutShortLE(&ptr[0x16], lcFlags);        // reserved fields

    dierr = fpImg->WriteBlock(pFile->fDirEntry.keyPointer, thisDirBuf);
    if (dierr != kDIErrNone)
        goto bail;

    /*
     * Set the volume name, based on the upper-case name and lower-case flags
     * we just wrote.  If "allowLowerCase" was set to false, it may not be
     * the same as what's in "newName".
     */
    char lowerName[A2FileProDOS::kMaxFileName+1];
    memset(lowerName, 0, sizeof(lowerName));    // lowerName won't be term'ed
    GenerateLowerCaseName(upperName, lowerName, lcFlags, false);

    strcpy(fVolumeName, lowerName);
    SetVolumeID();
    strcpy(pFile->fDirEntry.fileName, lowerName);

    /* update the entry in the linear file list */
    pFile->SetPathName(":", fVolumeName);

bail:
    return dierr;
}


/*
 * ===========================================================================
 *      A2FileProDOS
 * ===========================================================================
 */

/*
 * Convert from ProDOS compact date format to a time_t.
 *
 *  Byte 0 and 1: yyyyyyymmmmddddd
 *  Byte 2 and 3: 000hhhhh00mmmmmm
 *
 * The field is set entirely to zero if no date was assigned (which cannot
 * be a valid date since "day" ranges from 1 to 31).  If this is found then
 * ((time_t) 0) is returned.
 */
/*static*/ time_t A2FileProDOS::ConvertProDate(ProDate proDate)
{
    uint16_t prodosDate, prodosTime;
    int year, month, day, hour, minute, second;

    if (proDate == 0)
        return 0;

    prodosDate = (uint16_t) (proDate & 0x0000ffff);
    prodosTime = (uint16_t) ((proDate >> 16) & 0x0000ffff);

    second = 0;
    minute = prodosTime & 0x3f;
    hour = (prodosTime >> 8) & 0x1f;
    day = prodosDate & 0x1f;
    month = (prodosDate >> 5) & 0x0f;
    year = (prodosDate >> 9) & 0x7f;
    if (year < 40)
        year += 100;     /* P8 uses 0-39 for 2000-2039 */

    struct tm tmbuf;
    time_t when;

    tmbuf.tm_sec = second;
    tmbuf.tm_min = minute;
    tmbuf.tm_hour = hour;
    tmbuf.tm_mday = day;
    tmbuf.tm_mon = month-1;     // ProDOS uses 1-12
    tmbuf.tm_year = year;
    tmbuf.tm_wday = 0;
    tmbuf.tm_yday = 0;
    tmbuf.tm_isdst = -1;        // let it figure DST and time zone
    when = mktime(&tmbuf);

    if (when == (time_t) -1)
        when = 0;

    return when;
}

/*
 * Convert a time_t to a ProDOS-format date.
 *
 * CiderPress uses kDateInvalid==-1 and kDateNone==-2.
 */
/*static*/ A2FileProDOS::ProDate A2FileProDOS::ConvertProDate(time_t unixDate)
{
    ProDate proDate;
    uint32_t prodosDate, prodosTime;
    struct tm* ptm;
    int year;

    if (unixDate == 0 || unixDate == -1 || unixDate == -2)
        return 0;

    ptm = localtime(&unixDate);
    if (ptm == NULL)
        return 0;       // must've been invalid or unspecified

    year = ptm->tm_year;
#ifdef OLD_PRODOS_DATES
    /* ProSel-16 volume repair complaints about dates < 1980 and >= Y2K */
    if (year > 100)
        year -= 20;
#endif

    if (year >= 100)
        year -= 100;
    if (year < 0 || year >= 128) {
        LOGI("WHOOPS: got year %d from %d", year, ptm->tm_year);
        year = 70;
    }

    prodosDate = year << 9 | (ptm->tm_mon+1) << 5 | ptm->tm_mday;
    prodosTime = ptm->tm_hour << 8 | ptm->tm_min;

    proDate = prodosTime << 16 | prodosDate;
    return proDate;
}

/*
 * Return the file creation time as a time_t.
 */
time_t A2FileProDOS::GetCreateWhen(void) const
{
    return ConvertProDate(fDirEntry.createWhen);
}

/*
 * Return the file modification time as a time_t.
 */
time_t A2FileProDOS::GetModWhen(void) const
{
    return ConvertProDate(fDirEntry.modWhen);
}

/*
 * Set the full pathname to a combination of the base path and the
 * current file's name.
 *
 * If we're in the volume directory, pass in "" for the base path (not NULL).
 */
void A2FileProDOS::SetPathName(const char* basePath, const char* fileName)
{
    assert(basePath != NULL && fileName != NULL);
    if (fPathName != NULL)
        delete[] fPathName;

    int baseLen = strlen(basePath);
    fPathName = new char[baseLen + 1 + strlen(fileName)+1];
    strcpy(fPathName, basePath);
    if (baseLen != 0 &&
        !(baseLen == 1 && basePath[0] == ':'))
    {
        *(fPathName + baseLen) = kFssep;
        baseLen++;
    }
    strcpy(fPathName + baseLen, fileName);
}

/*
 * Convert a character in a ProDOS name to lower case.
 *
 * This is special in that '.' is considered upper case, with ' ' as its
 * lower-case counterpart.
 */
/*static*/ char A2FileProDOS::NameToLower(char ch)
{
    if (ch == '.')
        return ' ';
    else
        return tolower(ch);
}

/*
 * Init the fields in the DirEntry struct from the values in the ProDOS
 * directory entry pointed to by "entryBuf".
 *
 * Deals with lower case conversions on the filename.
 */
/*static*/ void A2FileProDOS::InitDirEntry(A2FileProDOS::DirEntry* pEntry,
    const uint8_t* entryBuf)
{
    int nameLen;

    pEntry->storageType = (entryBuf[0x00] & 0xf0) >> 4;
    nameLen = entryBuf[0x00] & 0x0f;
    memcpy(pEntry->fileName, &entryBuf[0x01], nameLen);
    pEntry->fileName[nameLen] = '\0';
    pEntry->fileType = entryBuf[0x10];
    pEntry->keyPointer = GetShortLE(&entryBuf[0x11]);
    pEntry->blocksUsed = GetShortLE(&entryBuf[0x13]);
    pEntry->eof = GetLongLE(&entryBuf[0x15]);
    pEntry->eof &= 0x00ffffff;
    pEntry->createWhen = GetLongLE(&entryBuf[0x18]);
    pEntry->version = entryBuf[0x1c];
    pEntry->minVersion = entryBuf[0x1d];
    pEntry->access = entryBuf[0x1e];
    pEntry->auxType = GetShortLE(&entryBuf[0x1f]);
    pEntry->modWhen = GetLongLE(&entryBuf[0x21]);
    pEntry->headerPointer = GetShortLE(&entryBuf[0x25]);

    /* generate the name into the buffer; does not null-terminate */
    if (UsesAppleWorksAuxType(pEntry->fileType)) {
        DiskFSProDOS::GenerateLowerCaseName(pEntry->fileName, pEntry->fileName,
            pEntry->auxType, true);
    } else if (pEntry->minVersion & 0x80) {
        DiskFSProDOS::GenerateLowerCaseName(pEntry->fileName, pEntry->fileName,
            GetShortLE(&entryBuf[0x1c]), false);
    }
    pEntry->fileName[sizeof(pEntry->fileName)-1] = '\0';
}

/*
 * Open one fork of this file.
 *
 * I really, really dislike forked files.
 */
DIError A2FileProDOS::Open(A2FileDescr** ppOpenFile, bool readOnly,
    bool rsrcFork /*= false*/)
{
    DIError dierr = kDIErrNone;
    A2FDProDOS* pOpenFile = NULL;

    LOGI(" ProDOS Open(ro=%d, rsrc=%d) on '%s'",
        readOnly, rsrcFork, fPathName);
    //Dump();

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
    if (rsrcFork && fDirEntry.storageType != kStorageExtended) {
        dierr = kDIErrForkNotFound;
        goto bail;
    }

    pOpenFile = new A2FDProDOS(this);
    if (pOpenFile == NULL)
        return kDIErrMalloc;

    pOpenFile->fOpenRsrcFork = false;

    if (fDirEntry.storageType == kStorageExtended) {
        if (rsrcFork) {
            dierr = LoadBlockList(fExtRsrc.storageType, fExtRsrc.keyBlock,
                        fExtRsrc.eof, &pOpenFile->fBlockCount,
                        &pOpenFile->fBlockList);
            pOpenFile->fOpenEOF = fExtRsrc.eof;
            pOpenFile->fOpenBlocksUsed = fExtRsrc.blocksUsed;
            pOpenFile->fOpenStorageType = fExtRsrc.storageType;
            pOpenFile->fOpenRsrcFork = true;
        } else {
            dierr = LoadBlockList(fExtData.storageType, fExtData.keyBlock,
                        fExtData.eof, &pOpenFile->fBlockCount,
                        &pOpenFile->fBlockList);
            pOpenFile->fOpenEOF = fExtData.eof;
            pOpenFile->fOpenBlocksUsed = fExtData.blocksUsed;
            pOpenFile->fOpenStorageType = fExtData.storageType;
        }
    } else if (fDirEntry.storageType == kStorageDirectory ||
               fDirEntry.storageType == kStorageVolumeDirHeader)
    {
        dierr = LoadDirectoryBlockList(fDirEntry.keyPointer,
                    fDirEntry.eof, &pOpenFile->fBlockCount,
                    &pOpenFile->fBlockList);
        pOpenFile->fOpenEOF = fDirEntry.eof;
        pOpenFile->fOpenBlocksUsed = fDirEntry.blocksUsed;
        pOpenFile->fOpenStorageType = fDirEntry.storageType;
    } else if (fDirEntry.storageType == kStorageSeedling ||
               fDirEntry.storageType == kStorageSapling ||
               fDirEntry.storageType == kStorageTree)
    {
        dierr = LoadBlockList(fDirEntry.storageType, fDirEntry.keyPointer,
                    fDirEntry.eof, &pOpenFile->fBlockCount,
                    &pOpenFile->fBlockList);
        pOpenFile->fOpenEOF = fDirEntry.eof;
        pOpenFile->fOpenBlocksUsed = fDirEntry.blocksUsed;
        pOpenFile->fOpenStorageType = fDirEntry.storageType;
    } else {
        LOGI("PrODOS can't open unknown storage type %d",
            fDirEntry.storageType);
        dierr = kDIErrBadDirectory;
        goto bail;
    }
    if (dierr != kDIErrNone) {
        LOGI(" ProDOS open failed");
        goto bail;
    }

    pOpenFile->fOffset = 0;
    //pOpenFile->DumpBlockList();

    fpOpenFile = pOpenFile;     // add it to our single-member "open file set"
    *ppOpenFile = pOpenFile;
    pOpenFile = NULL;

bail:
    delete pOpenFile;
    return dierr;
}

/*
 * Gather a linear, non-sparse list of file blocks into an array.
 *
 * Pass in the storage type and top-level key block.  Separation of
 * extended files should have been handled by the caller.  This loads the
 * list for only one fork.
 *
 * There are two kinds of sparse: sparse *inside* data, and sparse
 * *past* data.  The latter is interesting, because there is no need
 * to create space in index blocks to hold it.  Thus, a sapling could
 * hold a file with an EOF of 16MB.
 *
 * If "pIndexBlockCount" and "pIndexBlockList" are non-NULL, then we
 * also accumulate the list of index blocks and return those as well.
 * For a Tree-structured file, the first entry in the index list is
 * the master index block.
 *
 * The caller must delete[] "*pBlockList" and "*pIndexBlockList".
 */
DIError A2FileProDOS::LoadBlockList(int storageType, uint16_t keyBlock,
    long eof, long* pBlockCount, uint16_t** pBlockList,
    long* pIndexBlockCount, uint16_t** pIndexBlockList)
{
    if (storageType == kStorageDirectory ||
        storageType == kStorageVolumeDirHeader)
    {
        assert(pIndexBlockList == NULL && pIndexBlockCount == NULL);
        return LoadDirectoryBlockList(keyBlock, eof, pBlockCount, pBlockList);
    }
    
    assert(keyBlock != 0);
    assert(pBlockCount != NULL);
    assert(pBlockList != NULL);
    assert(*pBlockList == NULL);
    if (storageType != kStorageSeedling &&
        storageType != kStorageSapling &&
        storageType != kStorageTree)
    {
        /*
         * We can get here if somebody puts a bad storage type inside the
         * extended key block of a forked file.  Bad storage types on other
         * kinds of files are caught earlier.
         */
        LOGI(" ProDOS unexpected storageType %d in '%s'",
            storageType, GetPathName());
        return kDIErrNotSupported;
    }

    DIError dierr = kDIErrNone;
    uint16_t* list = NULL;
    long count;

    assert(eof < 1024*1024*16);
    count = (eof + kBlkSize -1) / kBlkSize;
    if (count == 0)
        count = 1;
    list = new uint16_t[count+1];
    if (list == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    if (pIndexBlockList != NULL) {
        assert(pIndexBlockCount != NULL);
        assert(*pIndexBlockList == NULL);
    }

    /* this should take care of trailing sparse entries */
    memset(list, 0, sizeof(uint16_t) * count);
    list[count] = kInvalidBlockNum;     // overrun check

    if (storageType == kStorageSeedling) {
        list[0] = keyBlock;

        if (pIndexBlockList != NULL) {
            *pIndexBlockCount = 0;
            *pIndexBlockList = NULL;
        }
    } else if (storageType == kStorageSapling) {
        dierr = LoadIndexBlock(keyBlock, list, count);
        if (dierr != kDIErrNone)
            goto bail;

        if (pIndexBlockList != NULL) {
            *pIndexBlockCount = 1;
            *pIndexBlockList = new uint16_t[1];
            **pIndexBlockList = keyBlock;
        }
    } else if (storageType == kStorageTree) {
        uint8_t blkBuf[kBlkSize];
        uint16_t* listPtr = list;
        uint16_t* outIndexPtr = NULL;
        long countDown = count;
        int idx = 0;

        dierr = fpDiskFS->GetDiskImg()->ReadBlock(keyBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;

        if (pIndexBlockList != NULL) {
            int numIndices = (count + kMaxBlocksPerIndex-1) / kMaxBlocksPerIndex;
            numIndices++;   // add one for the master index block
            *pIndexBlockList = new uint16_t[numIndices];
            outIndexPtr = *pIndexBlockList;
            *outIndexPtr++ = keyBlock;
            *pIndexBlockCount = 1;
        }

        while (countDown) {
            long blockCount = countDown;
            if (blockCount > kMaxBlocksPerIndex)
                blockCount = kMaxBlocksPerIndex;
            uint16_t idxBlock;

            idxBlock = blkBuf[idx] | (uint16_t) blkBuf[idx+256] << 8;
            if (idxBlock == 0) {
                /* fully sparse index block */
                //LOGI(" ProDOS that's seriously sparse (%d)!", idx);
                memset(listPtr, 0, blockCount * sizeof(uint16_t));
                if (pIndexBlockList != NULL) {
                    *outIndexPtr++ = idxBlock;
                    (*pIndexBlockCount)++;
                }
            } else {
                dierr = LoadIndexBlock(idxBlock, listPtr, blockCount);
                if (dierr != kDIErrNone)
                    goto bail;

                if (pIndexBlockList != NULL) {
                    *outIndexPtr++ = idxBlock;
                    (*pIndexBlockCount)++;
                }
            }

            idx++;
            listPtr += blockCount;
            countDown -= blockCount;
        }
    } else {
        assert(false);
    }

    assert(list[count] == kInvalidBlockNum);

    dierr = ValidateBlockList(list, count);
    if (dierr != kDIErrNone)
        goto bail;

    *pBlockCount = count;
    *pBlockList = list;

bail:
    if (dierr != kDIErrNone) {
        delete[] list;
        assert(*pBlockList == NULL);

        if (pIndexBlockList != NULL && *pIndexBlockList != NULL) {
            delete[] *pIndexBlockList;
            *pIndexBlockList = NULL;
        }
    }
    return dierr;
}

/*
 * Make sure all values in the block list fall in accepted ranges.
 *
 * We allow zero (used for sparse blocks), but disallow values in the "system"
 * area (block 1 through the end of the usage map).
 *
 * It's hard to say whether we should compare against the DiskImg block count
 * (representing blocks we can physically read but aren't necessarily part
 * of the filesystem) or the filesystem "total blocks" value from the volume
 * header.  Using the one in the volume header is correct, but sometimes the
 * value is off on an otherwise reasonable disk.
 *
 * I'm falling on the side of generosity, allowing files that reference
 * potentially bad data to appear okay.  My main reason is that, except for
 * CFFA volumes that have been tweaked by CiderPress users, very few ProDOS
 * disks will have a large disparity between the two numbers unless somebody
 * has trashed the volume dir header.
 *
 * What we really need is three states for each file: good, suspect, damaged.
 */
DIError A2FileProDOS::ValidateBlockList(const uint16_t* list, long count)
{
    DiskImg* pImg = fpDiskFS->GetDiskImg();
    bool foundBad = false;

    while (count--) {
        if (*list > pImg->GetNumBlocks() ||
            (*list > 0 && *list <= 2))      // not enough, but it'll do
        {
            LOGI("Invalid block %d in '%s'", *list, fDirEntry.fileName);
            SetQuality(kQualityDamaged);
            return kDIErrBadFile;
        }
        if (*list > fpDiskFS->GetFSNumBlocks())
            foundBad = true;
        list++;
    }

    if (foundBad) {
        LOGI("  --- found out-of-range block in '%s'", GetPathName());
        SetQuality(kQualitySuspicious);
    }

    return kDIErrNone;
}

/*
 * Copy the entries from the index block in "block" to "list", copying
 * at most "maxCount" entries.
 */
DIError A2FileProDOS::LoadIndexBlock(uint16_t block, uint16_t* list,
    int maxCount)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    int i;

    if (maxCount > kMaxBlocksPerIndex)
        maxCount = kMaxBlocksPerIndex;

    dierr = fpDiskFS->GetDiskImg()->ReadBlock(block, blkBuf);
    if (dierr != kDIErrNone)
        goto bail;

    //LOGI("LOADING 0x%04x", block);
    for (i = 0; i < maxCount; i++) {
        *list++ = blkBuf[i] | (uint16_t) blkBuf[i+256] << 8;
    }

bail:
    return dierr;
}

/*
 * Load the block list from a directory, which is essentially a linear
 * linked list.
 */
DIError A2FileProDOS::LoadDirectoryBlockList(uint16_t keyBlock,
    long eof, long* pBlockCount, uint16_t** pBlockList)
{
    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    uint16_t* list = NULL;
    uint16_t* listPtr;
    int iterations;
    long count;

    assert(eof < 1024*1024*16);
    count = (eof + kBlkSize -1) / kBlkSize;
    if (count == 0)
        count = 1;
    list = new uint16_t[count+1];
    if (list == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    /* this should take care of trailing sparse entries */
    memset(list, 0, sizeof(uint16_t) * count);
    list[count] = kInvalidBlockNum;     // overrun check

    iterations = 0;
    listPtr = list;

    while (keyBlock && iterations < kMaxCatalogIterations) {
        if (keyBlock < 2 ||
            keyBlock >= fpDiskFS->GetDiskImg()->GetNumBlocks())
        {
            LOGI(" ProDOS ERROR: directory block %u out of range", keyBlock);
            dierr = kDIErrInvalidBlock;
            goto bail;
        }

        *listPtr++ = keyBlock;

        dierr = fpDiskFS->GetDiskImg()->ReadBlock(keyBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;

        keyBlock = GetShortLE(&blkBuf[0x02]);
        iterations++;
    }
    if (iterations == kMaxCatalogIterations) {
        LOGI(" ProDOS subdir iteration count exceeded");
        dierr = kDIErrDirectoryLoop;
        goto bail;
    }

    assert(list[count] == kInvalidBlockNum);

    *pBlockCount = count;
    *pBlockList = list;

bail:
    if (dierr != kDIErrNone)
        delete[] list;
    return dierr;
}

/*
 * Dump the contents.
 */
void A2FileProDOS::Dump(void) const
{
    LOGI(" ProDOS file '%s' (path='%s')",
        fDirEntry.fileName, fPathName);
    LOGI("   fileType=0x%02x auxType=0x%04x storage=%d",
        fDirEntry.fileType, fDirEntry.auxType, fDirEntry.storageType);
    LOGI("   keyPointer=%d blocksUsed=%d eof=%d",
        fDirEntry.keyPointer, fDirEntry.blocksUsed, fDirEntry.eof);
    LOGI("   access=0x%02x create=0x%08x mod=0x%08x",
        fDirEntry.access, fDirEntry.createWhen, fDirEntry.modWhen);
    LOGI("   version=%d minVersion=%d headerPtr=%d",
        fDirEntry.version, fDirEntry.minVersion, fDirEntry.headerPointer);
    if (fDirEntry.storageType == kStorageExtended) {
        LOGI("   DATA storage=%d keyBlk=%d blkUsed=%d eof=%d",
            fExtData.storageType, fExtData.keyBlock, fExtData.blocksUsed,
            fExtData.eof);
        LOGI("   RSRC storage=%d keyBlk=%d blkUsed=%d eof=%d",
            fExtRsrc.storageType, fExtRsrc.keyBlock, fExtRsrc.blocksUsed,
            fExtRsrc.eof);
    }
    LOGI("   * sparseData=%ld  sparseRsrc=%ld",
        (long) fSparseDataEof, (long) fSparseRsrcEof);
}


/*
 * ===========================================================================
 *      A2FDProDOS
 * ===========================================================================
 */

/*
 * Read a chunk of data from whichever fork is open.
 */
DIError A2FDProDOS::Read(void* buf, size_t len, size_t* pActual)
{
    LOGD(" ProDOS reading %lu bytes from '%s' (offset=%ld)",
        (unsigned long) len, fpFile->GetPathName(), (long) fOffset);
    //if (fBlockList == NULL)
    //  return kDIErrNotReady;

    if (fOffset + (long)len > fOpenEOF) {
        if (pActual == NULL)
            return kDIErrDataUnderrun;
        len = (long) (fOpenEOF - fOffset);
    }
    if (pActual != NULL)
        *pActual = len;
//
    long incrLen = len;

    DIError dierr = kDIErrNone;
    uint8_t blkBuf[kBlkSize];
    long blockIndex = (long) (fOffset / kBlkSize);
    int bufOffset = (int) (fOffset % kBlkSize);     // (& 0x01ff)
    size_t thisCount;
    long progressCounter = 0;

    if (len == 0) {
        ///* one block allocated for empty file */
        //SetLastBlock(fBlockList[0], true);
        return kDIErrNone;
    }
    assert(fOpenEOF != 0);

    assert(blockIndex >= 0 && blockIndex < fBlockCount);

    while (len) {
        if (fBlockList[blockIndex] == 0) {
            //LOGI(" ProDOS sparse index %d", blockIndex);
            memset(blkBuf, 0, sizeof(blkBuf));
        } else {
            //LOGI(" ProDOS non-sparse index %d", blockIndex);
            dierr = fpFile->GetDiskFS()->GetDiskImg()->ReadBlock(fBlockList[blockIndex],
                        blkBuf);
            if (dierr != kDIErrNone) {
                LOGI(" ProDOS error reading block [%ld]=%d of '%s'",
                    blockIndex, fBlockList[blockIndex], fpFile->GetPathName());
                return dierr;
            }
        }
        thisCount = kBlkSize - bufOffset;
        if (thisCount > len)
            thisCount = len;

        memcpy(buf, blkBuf + bufOffset, thisCount);
        len -= thisCount;
        buf = (char*)buf + thisCount;

        bufOffset = 0;
        blockIndex++;

        progressCounter++;
        if (progressCounter > 100 && len) {
            progressCounter = 0;
            /*
             * Show progress within the current read request.  This only
             * kicks in for large reads, e.g. reformatting the entire file.
             * For smaller reads, used when we're extracting w/o reformatting,
             * "progressCounter" never gets large enough.
             */
            if (!UpdateProgress(fOffset + incrLen - len)) {
                dierr = kDIErrCancelled;
                return dierr;
            }
            //::Sleep(100);     // DEBUG DEBUG
        }
    }

    fOffset += incrLen;

    if (!UpdateProgress(fOffset))
        dierr = kDIErrCancelled;

    return dierr;
}

/*
 * Write data at the current offset.
 *
 * For simplicity, we assume that there can only be one of two situations:
 *  (1) We're writing a directory, which might expand by one block; or
 *  (2) We're writing all of a brand-new file in one shot.
 *
 * Modifies fOpenEOF, fOpenBlocksUsed, fStorageType, and sets fModified.
 *
 * HEY: ProSel-16 describes these as fragmented, and it's probably right.
 * The correct way to do this is to allocate index blocks before allocating
 * the blocks they refer to, so that we don't have to jump all over the disk
 * to read the indexes (which, at the moment, appear at the end of the file).
 * A bit tricky, but doable.
 */
DIError A2FDProDOS::Write(const void* buf, size_t len, size_t* pActual)
{
    DIError dierr = kDIErrNone;
    A2FileProDOS* pFile = (A2FileProDOS*) fpFile;
    DiskFSProDOS* pDiskFS = (DiskFSProDOS*) fpFile->GetDiskFS();
    bool allocSparse = (pDiskFS->GetParameter(DiskFS::kParmProDOS_AllocSparse) != 0);
    uint8_t blkBuf[kBlkSize];
    uint16_t keyBlock;
    bool allZero = true;
    const uint8_t* scanPtr = (const uint8_t*)buf;

    if (len >= 0x01000000) {    // 16MB
        assert(false);
        return kDIErrInvalidArg;
    }

    /* use separate function for directories */
    if (pFile->fDirEntry.storageType == A2FileProDOS::kStorageDirectory ||
        pFile->fDirEntry.storageType == A2FileProDOS::kStorageVolumeDirHeader)
    {
        return WriteDirectory(buf, len, pActual);
    }

    dierr = pDiskFS->LoadVolBitmap();
    if (dierr != kDIErrNone)
        goto bail;

    assert(fOffset == 0);       // big simplifying assumption
    assert(fOpenEOF == 0);      // another one
    assert(fOpenBlocksUsed == 1);
    assert(buf != NULL);

    /* nothing to do for zero-length write; don't even set fModified */
    if (len == 0)
        goto bail;

    if (pFile->fDirEntry.storageType != A2FileProDOS::kStorageExtended)
        keyBlock = pFile->fDirEntry.keyPointer;
    else {
        if (fOpenRsrcFork)
            keyBlock = pFile->fExtRsrc.keyBlock;
        else
            keyBlock = pFile->fExtData.keyBlock;
    }

    /*
     * See if the file is completely empty.  This lets us do an optimization
     * where we store it as a seedling.  (GS/OS seems to do this, ProDOS 8
     * v2.0.3 tends to allocate the first block.)
     */
    for (unsigned int i = 0; i < len; ++i, ++scanPtr) {
        if (*scanPtr != 0x00) {
            allZero = false;
            break;
        }
    }
    if (allZero) {
        LOGI("+++ found file filled with %zd zeroes", len);
    }

    /*
     * Special-case seedling files.  Just write the data into the key block
     * and we're done.
     */
    if (allZero || len <= (size_t)kBlkSize) {
        memset(blkBuf, 0, sizeof(blkBuf));
        if (!allZero) {
            memcpy(blkBuf, buf, len);
        } else {
            LOGI("+++ ProDOS storing large but empty file as seedling");
        }
        dierr = pDiskFS->GetDiskImg()->WriteBlock(keyBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;

        fOpenEOF = len;
        fOpenBlocksUsed = 1;
        assert(fOpenStorageType == A2FileProDOS::kStorageSeedling);
        fOffset += len;
        fModified = true;
        goto bail;
    }

    /*
     * Start by allocating space for the block list.  The list is always the
     * same size, regardless of sparse allocations.
     *
     * We over-alloc by one so we can have an overrun detection entry.
     */
    fBlockCount = (len + kBlkSize-1) / kBlkSize;
    assert(fBlockCount > 0);
    delete[] fBlockList;
    fBlockList = new uint16_t[fBlockCount+1];
    if (fBlockList == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }
    fBlockList[fBlockCount] = A2FileProDOS::kInvalidBlockNum;

    /*
     * Write the data blocks to disk, allocating as we go.  We have to treat
     * the last entry specially because it might not fill an entire block.
     */
    const uint8_t* blkPtr;
    long blockIdx;
    long progressCounter;

    progressCounter = 0;
    blkPtr = (const uint8_t*) buf;
    for (blockIdx = 0; blockIdx < fBlockCount; blockIdx++) {
        long newBlock;

        if (blockIdx == fBlockCount-1) {
            /* for last block, copy partial and move blkPtr */
            int copyLen = len - (blockIdx * kBlkSize);
            assert(copyLen > 0 && copyLen <= kBlkSize);
            memset(blkBuf, 0, sizeof(blkBuf));
            memcpy(blkBuf, blkPtr, copyLen);
            blkPtr = blkBuf;
        }

        if (allocSparse && IsEmptyBlock(blkPtr)) {
            if (blockIdx == 0) {
                // Fix for issues #18 and #49.  GS/OS appears to get confused
                // if the first entry in the master index block for a "tree"
                // file is zero.  We can avoid the problem by always allocating
                // the first data block, which causes allocation of the first
                // index block.  (The "all zeroes" case was handled earlier,
                // so if we got here we know this won't be an empty seedling.)
                LOGI("+++ allocating storage for empty first block");
                newBlock = pDiskFS->AllocBlock();
                fOpenBlocksUsed++;
            } else {
                // Sparse.
                newBlock = 0;
            }
        } else {
            newBlock = pDiskFS->AllocBlock();
            fOpenBlocksUsed++;
        }

        if (newBlock < 0) {
            LOGI(" ProDOS disk full during write!");
            dierr = kDIErrDiskFull;
            goto bail;
        }

        fBlockList[blockIdx] = (uint16_t) newBlock;

        if (newBlock != 0) {
            dierr = pDiskFS->GetDiskImg()->WriteBlock(newBlock, blkPtr);
            if (dierr != kDIErrNone)
                goto bail;
        }

        blkPtr += kBlkSize;

        /*
         * Update the progress counter and check to see if the "cancel" button
         * has been hit.  We don't call UpdateProgress on the last block
         * because we could be passing an offset value larger than "len".
         * Also, we don't want the progress bar to hit 100% until we've
         * actually finished.
         *
         * We do NOT want to check this after we start writing index blocks.
         * If we do, we need to make sure that whatever index blocks the file
         * has match up with what we've allocated in the disk block map.
         *
         * We don't want to save the disk block map if the user cancels here,
         * because then the blocks will be marked as "used" even though the
         * index blocks for this file haven't been written yet.
         *
         * It's tricky to get this right, which is why we allocate space
         * for the index blocks now -- running out of disk space and
         * user cancellation are handled the same way.  Once we get to the
         * point where we're updating the file structure, we can neither be
         * cancelled nor run out of space.  (We can still hit a bad block,
         * though, which we currently don't handle.)
         */
        progressCounter++;  // update every N blocks
        if (progressCounter > 100 && blockIdx != fBlockCount) {
            progressCounter = 0;
            if (!UpdateProgress(blockIdx * kBlkSize)) {
                dierr = kDIErrCancelled;
                goto bail;
            }
        }
    }

    assert(fBlockList[fBlockCount] == A2FileProDOS::kInvalidBlockNum);

    /*
     * Now we have a full block map.  Allocate any needed index blocks and
     * write them.
     */
#if 0  // now done earlier
     /*
     * If our block map is empty, i.e. the entire file is sparse, then
     * there's no need to create a sapling.  We just leave the file in
     * seedling form.  This can only happen for a completely empty file.
     */
    if (allZero) {
        LOGI("+++ ProDOS storing large but empty file as seedling");
        /* make sure key block is empty */
        memset(blkBuf, 0, sizeof(blkBuf));
        dierr = pDiskFS->GetDiskImg()->WriteBlock(keyBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;
        fOpenStorageType = A2FileProDOS::kStorageSeedling;
        fBlockList[0] = keyBlock;
    } else
#endif
    if (fBlockCount <= 256) {
        /* sapling file, write an index block into the key block */
        //bool allzero = true;  <-- should this be getting used?
        assert(fBlockCount > 1);
        memset(blkBuf, 0, sizeof(blkBuf));
        int i;
        for (i = 0; i < fBlockCount; i++) {
            //if (fBlockList[i] != 0)
            //    allzero = false;
            blkBuf[i] = fBlockList[i] & 0xff;
            blkBuf[256 + i] = (fBlockList[i] >> 8) & 0xff;
        }

        dierr = pDiskFS->GetDiskImg()->WriteBlock(keyBlock, blkBuf);
        if (dierr != kDIErrNone)
            goto bail;
        fOpenStorageType = A2FileProDOS::kStorageSapling;
    } else {
        /* tree file, write two or more indexes and write master into key */
        uint8_t masterBlk[kBlkSize];
        int idx;

        memset(masterBlk, 0, sizeof(masterBlk));

        for (idx = 0; idx < fBlockCount; ) {
            long newBlock;
            int i;

            memset(blkBuf, 0, sizeof(blkBuf));
            for (i = 0; i < 256 && idx < fBlockCount; i++, idx++) {
                blkBuf[i] = fBlockList[idx] & 0xff;
                blkBuf[256+i] = (fBlockList[idx] >> 8) & 0xff;
            }

            /* allocate a new index block, if needed */
            if (allocSparse && IsEmptyBlock(blkBuf))
                newBlock = 0;
            else {
                newBlock = pDiskFS->AllocBlock();
                fOpenBlocksUsed++;
            }
            if (newBlock != 0) {
                dierr = pDiskFS->GetDiskImg()->WriteBlock(newBlock, blkBuf);
                if (dierr != kDIErrNone)
                    goto bail;
            }

            masterBlk[(idx-1) / 256] = (uint8_t) newBlock;
            masterBlk[256 + (idx-1)/256] = (uint8_t) (newBlock >> 8);
        }

        dierr = pDiskFS->GetDiskImg()->WriteBlock(keyBlock, masterBlk);
        if (dierr != kDIErrNone)
            goto bail;
        fOpenStorageType = A2FileProDOS::kStorageTree;
    }

    fOpenEOF = len;
    fOffset += len;
    fModified = true;

bail:
    if (dierr == kDIErrNone)
        dierr = pDiskFS->SaveVolBitmap();

    /*
     * We need to check UpdateProgress *after* the volume bitmap has been
     * saved.  Otherwise we'll have blocks allocated in the file's structure
     * but not marked in-use in the map when the "dierr" check above fails.
     */
    if (dierr == kDIErrNone) {
        if (!UpdateProgress(fOffset))
            dierr = kDIErrCancelled;
    }

    pDiskFS->FreeVolBitmap();
    return dierr;
}

/*
 * Determine whether a block is filled entirely with zeroes.
 */
bool A2FDProDOS::IsEmptyBlock(const uint8_t* blk)
{
    int i;

    for (i = 0; i < kBlkSize; i++) {
        if (*blk++ != 0)
            return false;
    }

    return true;
}

/*
 * Write a directory, possibly extending it by one block.
 *
 * If we're growing, the extra block will already have been allocated, and is
 * pointed to by the "next" pointer in the next-to-last block.  (This
 * pre-allocation makes our lives easier, and avoids a situation where we
 * would have to update the volume bitmap when another function is already
 * making lots of changes to it.)
 */
DIError A2FDProDOS::WriteDirectory(const void* buf, size_t len, size_t* pActual)
{
    DIError dierr = kDIErrNone;

    LOGD("ProDOS  writing %lu bytes to directory '%s'",
        (unsigned long) len, fpFile->GetPathName());

    assert(len >= (size_t)kBlkSize);
    assert((len % kBlkSize) == 0);
    assert(len == (size_t)fOpenEOF || len == (size_t)fOpenEOF + kBlkSize);

    if (len > (size_t)fOpenEOF) {
        /*
         * Extend the block list, remembering that we add an extra item
         * on the end to check for overruns.
         */
        uint16_t* newBlockList;

        fBlockCount++;
        newBlockList = new uint16_t[fBlockCount+1];
        memcpy(newBlockList, fBlockList,
            sizeof(uint16_t) * fBlockCount);
        newBlockList[fBlockCount] = A2FileProDOS::kInvalidBlockNum;

        uint8_t* blkPtr;
        blkPtr = (uint8_t*)buf + fOpenEOF - kBlkSize;
        assert(blkPtr >= buf);
        assert(GetShortLE(&blkPtr[0x02]) != 0);
        newBlockList[fBlockCount-1] = GetShortLE(&blkPtr[0x02]);

        delete[] fBlockList;
        fBlockList = newBlockList;

        LOGI(" ProDOS updated block list for subdir:");
        DumpBlockList();
    }

    /*
     * Now just run down the block list writing the directory.
     */
    assert(len == (size_t)fBlockCount * kBlkSize);
    int idx;
    for (idx = 0; idx < fBlockCount; idx++) {
        assert(fBlockList[idx] >= kVolHeaderBlock);
        dierr = fpFile->GetDiskFS()->GetDiskImg()->WriteBlock(fBlockList[idx],
                    (uint8_t*)buf + idx * kBlkSize);
        if (dierr != kDIErrNone) {
            LOGI(" ProDOS failed writing dir, block=%d", fBlockList[idx]);
            goto bail;
        }
    }

    fOpenEOF = len;
    fOpenBlocksUsed = (uint16_t) fBlockCount; // very simple for subdirs
    //fOpenStorageType
    fModified = true;

bail:
    return dierr;
}

/*
 * Seek to a new position within the file.
 */
DIError A2FDProDOS::Seek(di_off_t offset, DIWhence whence)
{
    DIError dierr = kDIErrNone;
    switch (whence) {
    case kSeekSet:
        if (offset < 0 || offset > fOpenEOF)
            return kDIErrInvalidArg;
        fOffset = offset;
        break;
    case kSeekEnd:
        if (offset > 0 || offset < -fOpenEOF)
            return kDIErrInvalidArg;
        fOffset = fOpenEOF + offset;
        break;
    case kSeekCur:
        if (offset < -fOffset ||
            offset >= (fOpenEOF - fOffset))
        {
            return kDIErrInvalidArg;
        }
        fOffset += offset;
        break;
    default:
        assert(false);
        return kDIErrInvalidArg;
    }

    assert(fOffset >= 0 && fOffset <= fOpenEOF);

    return dierr;
}

/*
 * Return current offset.
 */
di_off_t A2FDProDOS::Tell(void)
{
    //if (fBlockList == NULL)
    //  return kDIErrNotReady;

    return fOffset;
}

/*
 * Release file state.
 *
 * Most applications don't check the value of "Close", or call it from a
 * destructor, so we call CloseDescr whether we succeed or not.
 */
DIError A2FDProDOS::Close(void)
{
    DIError dierr = kDIErrNone;

    if (fModified) {
        A2FileProDOS* pFile = (A2FileProDOS*) fpFile;
        uint8_t blkBuf[kBlkSize];
        uint8_t newStorageType = fOpenStorageType;
        uint16_t newBlocksUsed = fOpenBlocksUsed;
        uint32_t newEOF = (uint32_t) fOpenEOF;  // TODO: assert range
        uint16_t combinedBlocksUsed;
        uint32_t combinedEOF;

        /*
         * If this is an extended file, fix the entries in the extended
         * key block, and adjust the values to be stored in the directory.
         */
        if (pFile->fDirEntry.storageType == A2FileProDOS::kStorageExtended) {
            /* these two don't change */
            newStorageType = pFile->fDirEntry.storageType;

            dierr = fpFile->GetDiskFS()->GetDiskImg()->ReadBlock(
                        pFile->fDirEntry.keyPointer, blkBuf);
            if (dierr != kDIErrNone)
                goto bail;

            int offset = 0;
            if (fOpenRsrcFork)
                offset = 256;

            blkBuf[0x00 + offset] = fOpenStorageType;
            // key block doesn't change
            PutShortLE(&blkBuf[0x03 + offset], newBlocksUsed);
            blkBuf[0x05 + offset] = (uint8_t) newEOF;
            blkBuf[0x06 + offset] = (uint8_t) (newEOF >> 8);
            blkBuf[0x07 + offset] = (uint8_t) (newEOF >> 16);

            dierr = fpFile->GetDiskFS()->GetDiskImg()->WriteBlock(
                        pFile->fDirEntry.keyPointer, blkBuf);
            if (dierr != kDIErrNone)
                goto bail;

            // file blocks used is sum of data and rsrc block counts +1 for key
            combinedBlocksUsed =
                GetShortLE(&blkBuf[0x03]) + GetShortLE(&blkBuf[0x103]) +1;
            combinedEOF = 512;  // for some reason this gets stuffed in
        } else {
            combinedBlocksUsed = newBlocksUsed;
            combinedEOF = newEOF;
        }

        /*
         * Update fields in the file's directory entry.  Unless, of course,
         * this is the volume directory itself.
         */
        if (pFile->fParentDirBlock != 0) {
            dierr = fpFile->GetDiskFS()->GetDiskImg()->ReadBlock(
                        pFile->fParentDirBlock, blkBuf);
            if (dierr != kDIErrNone)
                goto bail;

            uint8_t* pParentPtr;
            pParentPtr = blkBuf + 0x04 + pFile->fParentDirIdx * kEntryLength;
            assert(pParentPtr + kEntryLength < blkBuf + kBlkSize);
            if (toupper(pParentPtr[0x01]) != toupper(pFile->fDirEntry.fileName[0]))
            {
                LOGW("ProDOS ERROR: parent pointer has wrong entry??");
                assert(false);
                dierr = kDIErrInternal;
                goto bail;
            }

            /* update the fields from the open file */
            pParentPtr[0x00] =
                (pParentPtr[0x00] & 0x0f) | (newStorageType << 4);
            PutShortLE(&pParentPtr[0x13], combinedBlocksUsed);
            if (pFile->fDirEntry.storageType != A2FileProDOS::kStorageExtended)
            {
                PutShortLE(&pParentPtr[0x15], (uint16_t) newEOF);
                pParentPtr[0x17] = (uint8_t) (newEOF >> 16);
            }
            /* don't update the mod date for now */
            //PutLongLE(&pParentPtr[0x21], A2FileProDOS::ConvertProDate(time(NULL)));

            dierr = fpFile->GetDiskFS()->GetDiskImg()->WriteBlock(
                        pFile->fParentDirBlock, blkBuf);
            if (dierr != kDIErrNone)
                goto bail;
        }

        /*
         * Find the #of sparse blocks.  We do this to update the "sparse EOF",
         * which determines the "compressed" size shown in the file list.  We
         * have two cases: normal file with sparse contents, and seedling file
         * that is entirely sparse except for the first block.
         *
         * In the normal case, we walk through the list of data blocks,
         * looking for gaps.  In the seedling case, we just use the EOF.
         *
         * This is just for display.  The value seen after adding a file should
         * not change if you reload the disk image.
         */
        int sparseBlocks = 0;
        if (fBlockCount == 1 && fOpenEOF > kBlkSize) {
            // 1023/1024 = 2 blocks = 1 sparse
            // 1025 = 3 blocks = 2 sparse
            sparseBlocks = (int)((fOpenEOF-1) / kBlkSize);
        } else {
            for (int i = 0; i < fBlockCount; i++) {
                if (fBlockList[i] == 0)
                    sparseBlocks++;
            }
        }

        /*
         * Update our internal copies of stuff.  The EOFs have changed, and
         * in theory we'd want to update the modification date.  In practice
         * we're usually shuffling data from one archive to another and want
         * to preserve the mod date.  (Could be a DiskFS global pref?)
         */
        pFile->fDirEntry.storageType = newStorageType;
        pFile->fDirEntry.blocksUsed = combinedBlocksUsed;
        pFile->fDirEntry.eof = combinedEOF;

        if (newStorageType == A2FileProDOS::kStorageExtended) {
            if (!fOpenRsrcFork) {
                pFile->fExtData.storageType = fOpenStorageType;
                pFile->fExtData.blocksUsed = newBlocksUsed;
                pFile->fExtData.eof = newEOF;
                pFile->fSparseDataEof = (di_off_t) newEOF - (sparseBlocks * kBlkSize);
                if (pFile->fSparseDataEof < 0)
                    pFile->fSparseDataEof = 0;
            } else {
                pFile->fExtRsrc.storageType = fOpenStorageType;
                pFile->fExtRsrc.blocksUsed = newBlocksUsed;
                pFile->fExtRsrc.eof = newEOF;
                pFile->fSparseRsrcEof = (di_off_t) newEOF - (sparseBlocks * kBlkSize);
                if (pFile->fSparseRsrcEof < 0)
                    pFile->fSparseRsrcEof = 0;
            }
        } else {
            pFile->fSparseDataEof = (di_off_t) newEOF - (sparseBlocks * kBlkSize);
            if (pFile->fSparseDataEof < 0)
                pFile->fSparseDataEof = 0;
        }
        // update mod date?
        
        //LOGI("File '%s' closed", pFile->GetPathName());
        //pFile->Dump();
    }

bail:
    fpFile->CloseDescr(this);
    return dierr;
}


/*
 * Return the #of sectors/blocks in the file.
 */
long A2FDProDOS::GetSectorCount(void) const
{
    //if (fBlockList == NULL)
    //  return kDIErrNotReady;
    return fBlockCount * 2;
}

long A2FDProDOS::GetBlockCount(void) const
{
    //if (fBlockList == NULL)
    //  return kDIErrNotReady;
    return fBlockCount;
}

/*
 * Return the Nth track/sector in this file.
 */
DIError A2FDProDOS::GetStorage(long sectorIdx, long* pTrack, long* pSector) const
{
    //if (fBlockList == NULL)
    //  return kDIErrNotReady;
    long prodosIdx = sectorIdx / 2;
    if (prodosIdx < 0 || prodosIdx >= fBlockCount)
        return kDIErrInvalidIndex;
    long prodosBlock = fBlockList[prodosIdx];

    if (prodosBlock == 0)
        *pTrack = *pSector = 0;     // special-case to avoid returning (0,1)
    else
        BlockToTrackSector(prodosBlock, (sectorIdx & 0x01) != 0, pTrack, pSector);
    return kDIErrNone;
}
/*
 * Return the Nth 512-byte block in this file.
 */
DIError A2FDProDOS::GetStorage(long blockIdx, long* pBlock) const
{
    //if (fBlockList == NULL)
    //  return kDIErrNotReady;
    if (blockIdx < 0 || blockIdx >= fBlockCount)
        return kDIErrInvalidIndex;
    long prodosBlock = fBlockList[blockIdx];

    *pBlock = prodosBlock;
    assert(*pBlock < fpFile->GetDiskFS()->GetDiskImg()->GetNumBlocks());
    return kDIErrNone;
}

/*
 * Dump the list of blocks from an open file, skipping over
 * "sparsed-out" entries.
 */
void A2FDProDOS::DumpBlockList(void) const
{
    long ll;

    LOGI(" ProDOS file block list (count=%ld)", fBlockCount);
    for (ll = 0; ll <= fBlockCount; ll++) {
        if (fBlockList[ll] != 0) {
            LOGI(" %5ld: 0x%04x", ll, fBlockList[ll]);
        }
    }
}
